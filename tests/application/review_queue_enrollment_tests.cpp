#include "bpr/application/manual_executor.hpp"
#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/review_queue_facts.hpp"
#include "bpr/core/discovery_diagnosis.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto ref(const std::int64_t aNumber) -> core::pull_request_ref
	{
		return core::pull_request_ref{
			.Host       = "github.com",
			.Account    = "reviewer",
			.Repository = "octo/app",
			.Number     = aNumber,
		};
	}

	[[nodiscard]] auto path_rule(const std::string_view aPattern)
		-> core::observer_rule
	{
		return core::observer_rule{
			.Kind      = core::observer_rule_kind::path,
			.Pattern   = std::string{ aPattern },
			.Excluding = false,
		};
	}

	[[nodiscard]] auto by_rule(const std::string_view aPattern)
		-> core::queue_membership
	{
		return core::queue_membership{
			.Role        = core::github_role::none,
			.MatchedRule = true,
			.Rule        = path_rule(aPattern),
		};
	}

	[[nodiscard]] auto by_role(const core::github_role aRole)
		-> core::queue_membership
	{
		return core::queue_membership{
			.Role        = aRole,
			.MatchedRule = false,
			.Rule        = { },
		};
	}

	[[nodiscard]] auto withdrawal(
		const core::pull_request_ref&           aRef,
		const application::queue_refresh_status aStatus)
		-> application::queue_refresh_completion
	{
		return application::queue_refresh_completion{
			.Session = { },
			.Ref     = aRef,
			.Status  = aStatus,
		};
	}

	class memory_queue_store final
		: public application::queue_enrollment_store_port
	{
	public:
		[[nodiscard]] auto load(std::stop_token) -> std::expected<
			application::durable_queue_state,
			application::queue_store_failure> override
		{
			++Loads;
			if (FailLoad) {
				return std::unexpected(application::queue_store_failure::unavailable);
			}
			return State;
		}

		[[nodiscard]] auto apply_rule_change(
			const application::queue_rule_intent&                 aIntent,
			const std::span<const application::enrollment_record> aEnrollments,
			std::stop_token) -> std::expected<void, application::queue_store_failure> override
		{
			auto next = State;
			if (aIntent.Action == application::queue_rule_action::add) {
				next.Rules.push_back(aIntent.Rule);
			} else {
				std::erase(next.Rules, aIntent.Rule);
			}
			if (FailEnrollmentStore && !aEnrollments.empty()) {
				return std::unexpected(
					application::queue_store_failure::unavailable);
			}
			for (const auto& enrollment : aEnrollments) {
				const auto found = std::ranges::find(
					next.Enrollments,
					enrollment.Ref,
					&application::enrollment_record::Ref);
				if (found == next.Enrollments.end()) {
					next.Enrollments.push_back(enrollment);
				} else {
					*found = enrollment;
				}
			}
			State = std::move(next);
			return { };
		}

		[[nodiscard]] auto store_enrollment(
			const application::enrollment_record& aRecord,
			std::stop_token) -> std::expected<void, application::queue_store_failure> override
		{
			if (FailEnrollmentStore) {
				return std::unexpected(
					application::queue_store_failure::unavailable);
			}
			const auto found = std::ranges::find(
				State.Enrollments,
				aRecord.Ref,
				&application::enrollment_record::Ref);
			if (found == State.Enrollments.end()) {
				State.Enrollments.push_back(aRecord);
			} else {
				*found = aRecord;
			}
			return { };
		}

		[[nodiscard]] auto store_preferences(
			const application::queue_presentation_preferences& aPreferences,
			std::stop_token) -> std::expected<void, application::queue_store_failure> override
		{
			if (FailEnrollmentStore) {
				return std::unexpected(
					application::queue_store_failure::unavailable);
			}
			State.Preferences = aPreferences;
			return { };
		}

		[[nodiscard]] auto apply_saved_server_change(
			const application::saved_server_intent& aIntent,
			std::stop_token) -> std::expected<void, application::queue_store_failure> override
		{
			++SavedServerChanges;
			if (FailEnrollmentStore) {
				return std::unexpected(application::queue_store_failure::unavailable);
			}
			std::erase_if(State.SavedServers, [&aIntent](const application::saved_server& aServer) {
				return aServer.Hostname == aIntent.Server.Hostname;
			});
			if (aIntent.Action == application::saved_server_action::add) {
				State.SavedServers.push_back(aIntent.Server);
			}
			return { };
		}
		[[nodiscard]] auto apply_saved_repository_change(
			const application::saved_repository_intent& aIntent,
			std::stop_token) -> std::expected<void, application::queue_store_failure> override
		{
			++SavedChanges;
			if (ThrowSavedStore) {
				throw std::runtime_error{ "synthetic store failure" };
			}
			if (FailEnrollmentStore) {
				return std::unexpected(
					application::queue_store_failure::unavailable);
			}
			if (aIntent.Action == application::saved_repository_action::add) {
				if (std::ranges::find(State.SavedRepositories, aIntent.Repository)
					== State.SavedRepositories.end()) {
					State.SavedRepositories.push_back(aIntent.Repository);
				}
			} else {
				std::erase(State.SavedRepositories, aIntent.Repository);
			}
			return { };
		}

		[[nodiscard]] auto apply_home_ignores(
			const std::span<const core::home_ignore_delta> aDeltas,
			std::stop_token) -> std::expected<void, application::queue_store_failure> override
		{
			IgnoreWrites.emplace_back(aDeltas.begin(), aDeltas.end());
			if (FailIgnoreStore) {
				return std::unexpected(application::queue_store_failure::unavailable);
			}
			for (const auto& delta : aDeltas) {
				std::erase(State.Ignored, delta.Ref);
				if (delta.Change == core::home_ignore_change::ignored) {
					State.Ignored.push_back(delta.Ref);
				}
			}
			return { };
		}

		std::size_t                                       SavedChanges       = 0U;
		std::size_t                                       SavedServerChanges = 0U;
		std::vector<std::vector<core::home_ignore_delta>> IgnoreWrites;
		bool                                              FailIgnoreStore = false;

		application::durable_queue_state State;
		std::size_t                      Loads               = 0U;
		bool                             FailEnrollmentStore = false;
		bool                             ThrowSavedStore     = false;
		bool                             FailLoad            = false;
	};

	class observed_refresh final : public application::queue_refresh_effect_port
	{
	public:
		[[nodiscard]] auto refresh(
			const application::queue_refresh_request& aRequest,
			std::stop_token                           aStop) -> application::queue_refresh_completion override
		{
			Requests.push_back(aRequest);
			if (aStop.stop_requested() && !IgnoreStop) {
				return {
					.Session = aRequest.Session,
					.Ref     = aRequest.Ref,
					.Status  = application::queue_refresh_status::cancelled,
				};
			}
			if (ReturnUnchanged) {
				return {
					.Session   = aRequest.Session,
					.Ref       = aRequest.Ref,
					.Status    = application::queue_refresh_status::unchanged,
					.Validator = aRequest.Validator,
				};
			}
			auto completion = application::queue_refresh_completion{
				.Session     = aRequest.Session,
				.Ref         = WrongRef ? ref(99) : aRequest.Ref,
				.Status      = application::queue_refresh_status::changed,
				.Validator   = "etag-1",
				.Observation = Observation,
			};
			if (WrongRef && completion.Observation.has_value()) {
				completion.Observation->Ref = completion.Ref;
			}
			return completion;
		}

		core::pull_request_observation                  Observation;
		std::vector<application::queue_refresh_request> Requests;
		bool                                            ReturnUnchanged = false;
		bool                                            IgnoreStop      = false;
		bool                                            WrongRef        = false;
	};

	class landing_log final : public application::queue_landing_projection_port
	{
	public:
		void publish(
			const application::queue_landing_projection& aProjection) override
		{
			Published.push_back(aProjection);
		}

		std::vector<application::queue_landing_projection> Published;
	};

	void run_one(application::manual_executor& aEffects,
				 application::manual_executor& aLane)
	{
		expect(aEffects.pending() == 1U, "one effect is pending");
		aEffects.run_next();
		expect(aLane.pending() == 1U, "one typed completion is pending");
		aLane.run_next();
	}

	void test_single_refresh_projection_work()
	{
		for (const auto count : { 32, 4096 }) {
			auto store = memory_queue_store{ };
			for (int number = 1; number <= count; ++number) {
				store.State.Enrollments.push_back({ .Ref = ref(number), .Membership = by_role(core::github_role::reviewer) });
			}
			auto refresh     = observed_refresh{ };
			auto effects     = application::manual_executor{ };
			auto decisions   = application::manual_executor{ };
			auto projections = landing_log{ };
			auto host        = application::review_queue_application_host{ store, refresh, effects, decisions, projections, { .Host = "github.com", .Account = "reviewer", .Generation = 4U } };
			host.restore();
			run_one(effects, decisions);
			refresh.Observation = core::pull_request_observation{ .Ref = ref(1), .Author = "fixture", .ChangedPaths = { "one.cpp" } };
			const auto before   = host.statistics();
			host.refresh(ref(1));
			run_one(effects, decisions);
			run_one(effects, decisions);
			const auto after = host.statistics();
			expect(after.AppliedRefreshes == before.AppliedRefreshes + 1U, "single exact refresh accepted on decision lane");
			expect(host.queue().size() == static_cast<std::size_t>(count), "single refresh retains unrelated queue items");
			expect(after.ItemPublications == before.ItemPublications + 1U
					   && after.Projection.RecordsVisited == before.Projection.RecordsVisited,
				   "retained membership with no newly observed role publishes one item without walking records");
			std::cout << "queue records=" << count << " changed=1 publications=" << after.Publications - before.Publications
					  << " records_visited=" << after.Projection.RecordsVisited - before.Projection.RecordsVisited
					  << " activity_comparisons=" << after.Projection.ActivityComparisons - before.Projection.ActivityComparisons
					  << " capacity_growths=" << after.Projection.CapacityGrowths - before.Projection.CapacityGrowths << '\n';
			host.shutdown();
		}
	}

	// [[BPR-0972]]. Every record finds its report by binary search, so the
	// comparisons grow as n log n rather than records times reports.
	void test_projection_finds_reports_without_walking_them()
	{
		for (const auto count : { 32, 4096 }) {
			auto records = application::queue_enrollment_register{ };
			auto reports = std::vector<application::queue_activity_report>{ };
			for (int number = 1; number <= count; ++number) {
				records.enroll(ref(number), by_role(core::github_role::reviewer));
			}
			for (int number = count; number >= 1; --number) {
				reports.push_back({ .Ref = ref(number), .Observation = { .UnseenComments = static_cast<std::uint32_t>(number), .Known = true } });
			}
			auto       work  = application::queue_projection_work{ };
			const auto queue = records.project(reports, &work);
			auto       depth = 0U;
			while ((1U << depth) < static_cast<unsigned>(count)) {
				++depth;
			}
			std::cout << "queue-projection records=" << count << " reports=" << count << " activity_comparisons=" << work.ActivityComparisons << '\n';
			expect(queue.size() == static_cast<std::size_t>(count)
					   && std::ranges::all_of(queue, [](const core::queue_item& aItem) { return aItem.Facts.UnseenComments == static_cast<std::uint32_t>(aItem.Ref.Number); }),
				   "every record joins its own report");
			expect(work.RecordsVisited == static_cast<std::uint64_t>(count)
					   && work.ActivityComparisons <= static_cast<std::uint64_t>(count) * (depth + 2U),
				   "the join is logarithmic per record");
		}
	}

	void test_composed_host_persists_matches_and_removal()
	{
		auto       store       = memory_queue_store{ };
		auto       refresh     = observed_refresh{ };
		auto       effects     = application::manual_executor{ };
		auto       decisions   = application::manual_executor{ };
		auto       projections = landing_log{ };
		const auto session     = application::queue_session_id{
			.Host = "github.com", .Account = "reviewer", .Generation = 4U
		};
		const auto rule = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		refresh.Observation = core::pull_request_observation{
			.Ref = ref(7), .Author = "octocat", .ChangedPaths = { "src/parser.cpp" }, .Title = "BPR_DIAGNOSTIC_PRIVATE_CANARY queue title"
		};

		auto host = application::review_queue_application_host{
			store, refresh, effects, decisions, projections, session
		};
		host.restore();
		expect(store.Loads == 0U, "restore does not access the store on the caller lane");
		run_one(effects, decisions);
		expect(store.Loads == 1U && host.rules().empty(), "restore adopts durable state");

		host.apply({ .Action = application::queue_rule_action::add, .Rule = rule });
		host.apply({ .Action = application::queue_rule_action::add, .Rule = rule });
		expect(store.State.Rules.empty(), "the rule is not stored on the caller lane");
		expect(effects.pending() == 1U,
			   "an identical add cannot overtake the first durable mutation");
		run_one(effects, decisions);
		expect(store.State.Rules == std::vector{ rule }, "the exact rule is durable");
		expect(host.rules().size() == 1U, "the in-memory rule is not duplicated");

		host.refresh(ref(7), core::queue_activity_observation{ .UnseenComments = 2U });
		run_one(effects, decisions);
		expect(
			projections.Published.back().Items.empty(),
			"a match is not published before persistence");
		run_one(effects, decisions);
		expect(host.queue().size() == 1U, "the persisted match reaches the queue");
		// [[BPR-0659]]. The refresh's observation names the title and author;
		// the activity names what I have not seen; the item carries all three.
		expect(host.queue().front().Facts
				   == core::queue_item_facts{ .Title = "BPR_DIAGNOSTIC_PRIVATE_CANARY queue title", .Author = "octocat", .UnseenComments = 2U },
			   "the queue item carries the title, author and unseen count the refresh read");
		expect(
			store.State.Enrollments.front().Membership.Rule == rule,
			"the durable enrollment keeps its exact matched reason");

		refresh.ReturnUnchanged = true;
		host.refresh(ref(7));
		run_one(effects, decisions);
		expect(
			refresh.Requests.back().Validator == std::optional<std::string>{ "etag-1" },
			"the next refresh carries the accepted ETag");

		refresh.ReturnUnchanged = false;
		host.apply({ .Action = application::queue_rule_action::remove, .Rule = rule });
		expect(effects.pending() == 1U,
			   "one transaction owns rule removal and enrollment reconciliation");
		run_one(effects, decisions);
		expect(host.queue().empty(), "cutting the only match empties the projection");
		expect(
			store.State.Enrollments.front().Departed
				== application::enrollment_departure::removed_by_reviewer,
			"the durable row explains why it left");
		host.shutdown();
	}

	void test_rule_removal_reconciles_every_matching_enrollment_before_publish()
	{
		auto       store       = memory_queue_store{ };
		auto       refresh     = observed_refresh{ };
		auto       effects     = application::manual_executor{ };
		auto       decisions   = application::manual_executor{ };
		auto       projections = landing_log{ };
		const auto rule        = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		auto host = application::review_queue_application_host{
			store,
			refresh,
			effects,
			decisions,
			projections,
			{ .Host = "github.com", .Account = "reviewer", .Generation = 4U }
		};
		host.restore();
		run_one(effects, decisions);
		host.apply({ .Action = application::queue_rule_action::add, .Rule = rule });
		run_one(effects, decisions);

		for (const auto number : { 7, 8 }) {
			refresh.Observation = core::pull_request_observation{
				.Ref = ref(number), .Author = "octocat", .ChangedPaths = { "src/parser.cpp" }
			};
			host.refresh(ref(number));
			run_one(effects, decisions);
			run_one(effects, decisions);
		}
		expect(host.queue().size() == 2U, "one rule enrolls two observed pull requests");

		host.apply({ .Action = application::queue_rule_action::remove, .Rule = rule });
		expect(
			projections.Published.back().Items.size() == 2U,
			"rule removal is not published before every changed row is durable");
		expect(effects.pending() == 1U,
			   "one atomic effect owns both changed enrollment rows");
		run_one(effects, decisions);
		expect(host.queue().empty(), "removing the shared rule empties both items");
		expect(
			std::ranges::all_of(
				store.State.Enrollments,
				[](const application::enrollment_record& aRecord) {
					return aRecord.Departed
						== application::enrollment_departure::removed_by_reviewer;
				}),
			"both durable rows retain the exact local-removal reason");
		host.shutdown();
	}

	void test_refresh_identity_and_cancellation_are_authoritative()
	{
		auto store       = memory_queue_store{ };
		auto refresh     = observed_refresh{ };
		auto effects     = application::manual_executor{ };
		auto decisions   = application::manual_executor{ };
		auto projections = landing_log{ };
		auto host        = application::review_queue_application_host{
			store,
			refresh,
			effects,
			decisions,
			projections,
			{ .Host = "github.com", .Account = "reviewer", .Generation = 4U }
		};
		host.restore();
		run_one(effects, decisions);
		const auto rule = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		host.apply({ .Action = application::queue_rule_action::add, .Rule = rule });
		run_one(effects, decisions);
		refresh.Observation = core::pull_request_observation{ .Ref = ref(7) };

		refresh.WrongRef = true;
		host.refresh(ref(7));
		run_one(effects, decisions);
		expect(host.queue().empty(), "a wrong-reference completion changes no queue item");
		expect(host.statistics().StaleCompletions == 1U,
			   "the wrong-reference completion is reported stale");

		refresh.WrongRef   = false;
		refresh.IgnoreStop = true;
		host.refresh(ref(7));
		host.cancel();
		run_one(effects, decisions);
		expect(host.queue().empty(), "a cancelled completion changes no queue item");
		expect(host.statistics().CancelledCompletions == 1U,
			   "cancellation remains authoritative when the effect ignores its token");
		host.shutdown();
	}

	void test_failed_rule_reconciliation_rolls_back_before_unavailable_projection()
	{
		auto       store       = memory_queue_store{ };
		auto       refresh     = observed_refresh{ };
		auto       effects     = application::manual_executor{ };
		auto       decisions   = application::manual_executor{ };
		auto       projections = landing_log{ };
		const auto rule        = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		auto host = application::review_queue_application_host{
			store,
			refresh,
			effects,
			decisions,
			projections,
			{ .Host = "github.com", .Account = "reviewer", .Generation = 4U }
		};
		host.restore();
		run_one(effects, decisions);
		host.apply({ .Action = application::queue_rule_action::add, .Rule = rule });
		run_one(effects, decisions);
		refresh.Observation = core::pull_request_observation{ .Ref = ref(7) };
		host.refresh(ref(7));
		run_one(effects, decisions);
		run_one(effects, decisions);
		expect(host.queue().size() == 1U, "the baseline enrollment is visible");

		store.FailEnrollmentStore = true;
		host.apply({ .Action = application::queue_rule_action::remove, .Rule = rule });
		run_one(effects, decisions);
		expect(
			projections.Published.back().Status
					== application::queue_landing_status::unavailable
				&& projections.Published.back().Rules == std::vector{ rule }
				&& projections.Published.back().Items.size() == 1U,
			"a failed enrollment write rolls back before the unavailable projection");
		host.shutdown();

		auto restartEffects     = application::manual_executor{ };
		auto restartDecisions   = application::manual_executor{ };
		auto restartProjections = landing_log{ };
		auto restarted          = application::review_queue_application_host{
			store,
			refresh,
			restartEffects,
			restartDecisions,
			restartProjections,
			{ .Host = "github.com", .Account = "reviewer", .Generation = 5U }
		};
		restarted.restore();
		run_one(restartEffects, restartDecisions);
		expect(
			restarted.rules().size() == 1U && restarted.rules().front() == rule
				&& restarted.queue().size() == 1U,
			"reopen sees the same durable rule and enrollment after atomic failure");
		restarted.shutdown();
	}

	void test_rule_reconciliation_never_suppresses_a_github_role()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), core::queue_membership{
								   .Role        = core::github_role::requested_reviewer,
								   .MatchedRule = true,
								   .Rule        = path_rule("src/parser"),
							   });
		records.reconcile_rule_change(
			ref(7), core::queue_membership{ .Role = core::github_role::requested_reviewer });
		const auto record = records.record_for(ref(7));
		expect(record.has_value() && record->present(),
			   "removing a local reason keeps the assigned review present");
		expect(record->Membership.Role == core::github_role::requested_reviewer
				   && !record->Membership.MatchedRule,
			   "the GitHub role remains while the deleted local reason is cleared");
	}

	// The reason a reviewer is watching does not stop being true when the path
	// that matched leaves the pull request. It is why they wanted to see the
	// change in the first place, and a queue that recomputed membership on
	// every refresh would drop exactly the pull requests a path rule exists to
	// catch.
	void test_enrollment_survives_the_path_that_matched()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_rule("src/parser"));

		// A later refresh sees a pull request that no longer touches the path.
		// Nothing re-enrolls it, and nothing needs to.
		const auto retained = records.record_for(ref(7));
		expect(retained.has_value(), "the enrollment is retained");
		expect(retained->present(), "and is still in the queue");
		expect(
			retained->Membership.MatchedRule
				&& retained->Membership.Rule.Pattern == "src/parser",
			"with the rule that brought it in, so the reviewer can still see "
			"why");

		const auto queue = records.project({ });
		expect(queue.size() == 1U, "and it is projected");
	}

	// Both reasons are kept. A rule match must not overwrite a role, or
	// removing the rule would silently drop a pull request GitHub assigned; a
	// role must not overwrite the rule, or the reviewer loses the answer to why
	// an unfamiliar item is in their queue.
	void test_re_enrolling_keeps_both_reasons()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_rule("src/parser"));
		records.enroll(ref(7), by_role(core::github_role::requested_reviewer));

		const auto both = records.record_for(ref(7));
		expect(both.has_value(), "one record, not two");
		expect(records.records().size() == 1U, "the item was not duplicated");
		expect(
			both->Membership.Role == core::github_role::requested_reviewer,
			"the role that arrived is kept");
		expect(
			both->Membership.MatchedRule
				&& both->Membership.Rule.Pattern == "src/parser",
			"and so is the rule that was already there");

		records.enroll(ref(7), by_rule("src/other"));
		const auto after = records.record_for(ref(7));
		expect(
			after->Membership.Role == core::github_role::requested_reviewer,
			"a later rule match does not clear the role");
		expect(
			after->Membership.Rule.Pattern == "src/parser",
			"and does not replace the rule that first brought it in");
	}

	// Forbidden and gone are different facts with different next steps: one is
	// a permission the reviewer may be able to ask for, the other is a pull
	// request that is not there any more. Collapsing them would send somebody
	// to the wrong person.
	void test_losing_access_and_being_gone_are_distinguished()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));
		records.enroll(ref(8), by_role(core::github_role::reviewer));

		records.observe(
			withdrawal(ref(7), application::queue_refresh_status::forbidden),
			application::refresh_disposition::withdrawn);
		records.observe(
			withdrawal(ref(8), application::queue_refresh_status::gone),
			application::refresh_disposition::withdrawn);

		expect(
			records.record_for(ref(7))->Departed
				== application::enrollment_departure::access_lost,
			"a forbidden pull request is recorded as access lost");
		expect(
			records.record_for(ref(8))->Departed
				== application::enrollment_departure::gone,
			"and one that is gone is recorded as gone");
	}

	// A departure is retained so the reviewer can be told what happened, and
	// dropped from the queue because a closed pull request is not asking for
	// attention.
	void test_a_departed_item_is_retained_but_not_projected()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));
		records.enroll(ref(8), by_role(core::github_role::reviewer));
		records.depart(ref(7), application::enrollment_departure::closed);

		expect(
			records.records().size() == 2U,
			"the closed item is still retained");
		expect(
			records.record_for(ref(7))->Departed
				== application::enrollment_departure::closed,
			"with the reason it left");

		const auto queue = records.project({ });
		expect(queue.size() == 1U, "but the queue shows only what is present");
		expect(queue.front().Ref == ref(8), "and it is the right one");
	}

	// Reopening is enrolling again. The reviewer gets the pull request back
	// with the reason they had it, rather than a second record beside the first.
	void test_reopening_restores_the_same_record()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_rule("src/parser"));
		records.depart(ref(7), application::enrollment_departure::closed);
		records.enroll(ref(7), by_rule("src/parser"));

		expect(records.records().size() == 1U, "one record, not two");
		expect(records.record_for(ref(7))->present(), "and it is back");
		expect(
			records.record_for(ref(7))->Membership.Rule.Pattern == "src/parser",
			"with the reason it had");
	}

	// Only a withdrawal moves an item out. A deferred or refused refresh says
	// nothing about whether the pull request is still there, and treating it as
	// a departure would empty the queue during an outage.
	void test_only_a_withdrawal_moves_an_item_out()
	{
		const auto dispositions = std::array{
			application::refresh_disposition::applied,
			application::refresh_disposition::unchanged,
			application::refresh_disposition::stale_session,
			application::refresh_disposition::deferred,
		};

		for (const auto disposition : dispositions) {
			auto records = application::queue_enrollment_register{ };
			records.enroll(ref(7), by_role(core::github_role::reviewer));
			records.observe(
				withdrawal(ref(7), application::queue_refresh_status::gone),
				disposition);
			expect(
				records.record_for(ref(7))->present(),
				"a disposition other than withdrawn leaves the item alone");
		}
	}

	// Every departure has a distinct name and a sentence the reviewer reads. A
	// pull request that vanished with no reason would leave them wondering
	// whether they imagined it.
	void test_every_departure_names_and_explains_itself()
	{
		const auto departures = std::array{
			application::enrollment_departure::none,
			application::enrollment_departure::closed,
			application::enrollment_departure::access_lost,
			application::enrollment_departure::gone,
			application::enrollment_departure::removed_by_reviewer,
		};

		auto names        = std::vector<std::string_view>{ };
		auto explanations = std::vector<std::string_view>{ };
		for (const auto departure : departures) {
			const auto text  = application::name(departure);
			const auto sense = application::explanation(departure);
			expect(!text.empty(), "every departure has a name");
			expect(!sense.empty(), "every departure has an explanation");
			for (const auto& other : names) {
				expect(other != text, "two departures never share a name");
			}
			for (const auto& other : explanations) {
				expect(
					other != sense, "two departures never share an explanation");
			}
			names.push_back(text);
			explanations.push_back(sense);
		}
	}

	// The projection is the core's, ordered by the core's rule. The host
	// decides what is retained; it does not decide what is urgent.
	void test_the_projection_is_ordered_by_attention()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::participant));
		records.enroll(ref(8), by_role(core::github_role::requested_reviewer));

		const auto activity = std::array{
			application::queue_activity_report{ .Ref = ref(7) },
			application::queue_activity_report{ .Ref = ref(8) },
		};
		const auto queue = records.project(activity);

		expect(queue.size() == 2U, "both are projected");
		expect(
			queue.front().Bucket
				== core::attention_bucket::initial_review_requested,
			"and the unanswered request comes first");
		expect(queue.front().Ref == ref(8), "which is the one that was asked");
	}

	// An item the host has not read yet is not guessed at: inventing an
	// observation would put a pull request in a bucket on no evidence, and
	// calling it quiet would be one such guess ([[BPR-0989]], RFC-BPR-0026
	// AC-02). It is in the queue with its detail unknown.
	void test_an_unobserved_item_is_not_known_to_be_quiet()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));

		const auto queue = records.project({ });
		expect(queue.size() == 1U, "it is still in the queue");
		expect(
			queue.front().Bucket == core::attention_bucket::detail_unknown,
			"and is not known to be quiet until something has been read about it");
	}

	// Activity pairs to enrollments by ref, so the caller owes no particular
	// order. Under the old positional contract this could not even be stated:
	// the same reports in a different order described different pull requests.
	void test_activity_pairs_by_ref_in_any_order()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));
		records.enroll(ref(8), by_role(core::github_role::reviewer));
		records.enroll(ref(9), by_role(core::github_role::reviewer));

		const auto moved = core::queue_activity_observation{
			.ReviewedByMe           = true,
			.HeadMovedSinceMyReview = true,
		};
		const auto forward = std::array{
			application::queue_activity_report{ .Ref = ref(7) },
			application::queue_activity_report{
				.Ref         = ref(8),
				.Observation = moved,
				.Facts       = core::queue_item_facts{ .Title = "Moved", .Author = "octocat" } },
			application::queue_activity_report{ .Ref = ref(9) },
		};
		const auto shuffled = std::array{ forward[2], forward[0], forward[1] };

		const auto expected = records.project(forward);
		expect(
			records.project(shuffled) == expected,
			"the same reports in any order project the same queue");
		const auto item
			= std::ranges::find(expected, ref(8), &core::queue_item::Ref);
		expect(
			item != expected.end()
				&& item->Bucket != core::attention_bucket::quiet,
			"and the moved head lands on the pull request it was read about");
		// [[BPR-0659]]. The facts the report carries reach the item, and the
		// unseen count is the observation's, never a second copy.
		expect(
			item != expected.end() && item->Facts.Title == "Moved"
				&& item->Facts.Author == "octocat" && item->Facts.UnseenComments == 0U,
			"the title and author travel with the activity onto the item");
		const auto unseen = std::array{
			application::queue_activity_report{
				.Ref         = ref(7),
				.Observation = core::queue_activity_observation{ .UnseenComments = 4U },
				.Facts       = core::queue_item_facts{ .UnseenComments = 1U } },
		};
		const auto counted = records.project(unseen);
		const auto seven   = std::ranges::find(counted, ref(7), &core::queue_item::Ref);
		expect(
			seven != counted.end() && seven->Facts.UnseenComments == 4U,
			"the item's unseen count is the observation's");

		const auto unenrolled = std::array{
			application::queue_activity_report{ .Ref = ref(99), .Observation = moved },
		};
		const auto ignored = records.project(unenrolled);
		expect(ignored == records.project({ }), "a report about an unenrolled ref moves nothing");
	}

	// [[BPR-0659]]. A saved repository is durable before it is projected, a
	// refused write leaves the list as it was, and a save of what is saved or
	// a forget of what is not is no write at all.
	// [[BPR-0660]]. Remembered servers: restored and published with the
	// queue, remembered durably before they are shown, forgotten the same
	// way; remembering a known hostname again replaces its account.
	void test_saved_servers_are_durable_before_projection()
	{
		auto       store       = memory_queue_store{ };
		auto       refresh     = observed_refresh{ };
		auto       effects     = application::manual_executor{ };
		auto       decisions   = application::manual_executor{ };
		auto       projections = landing_log{ };
		const auto session     = application::queue_session_id{
			.Host = "github.com", .Account = "reviewer", .Generation = 4U
		};
		const auto ghe           = application::saved_server{ .Hostname = "ghe.example.com", .Account = "reviewer" };
		store.State.SavedServers = { ghe };
		auto host                = application::review_queue_application_host{
			store, refresh, effects, decisions, projections, session
		};
		host.restore();
		run_one(effects, decisions);
		expect(host.saved_servers().size() == 1U && host.saved_servers().front() == ghe
				   && projections.Published.back().SavedServers == std::vector{ ghe },
			   "restore adopts the saved servers and publishes them");
		const auto lab = application::saved_server{ .Hostname = "code.example.org", .Account = "" };
		host.apply(application::saved_server_intent{ .Action = application::saved_server_action::add, .Server = lab });
		expect(store.SavedServerChanges == 0U && host.saved_servers().size() == 1U,
			   "a remember is not stored on the caller lane and is not shown before the store answers");
		run_one(effects, decisions);
		expect(store.SavedServerChanges == 1U && host.saved_servers().size() == 2U
				   && host.saved_servers().front() == lab
				   && projections.Published.back().SavedServers == std::vector{ lab, ghe },
			   "once stored, the server is shown, ordered by hostname");
		host.apply(application::saved_server_intent{
			.Action = application::saved_server_action::add,
			.Server = { .Hostname = "ghe.example.com", .Account = "other" } });
		run_one(effects, decisions);
		expect(store.SavedServerChanges == 2U && host.saved_servers().size() == 2U
				   && host.saved_servers().back().Account == "other",
			   "remembering a known hostname again replaces its account");
		host.apply(application::saved_server_intent{ .Action = application::saved_server_action::remove, .Server = lab });
		run_one(effects, decisions);
		expect(store.SavedServerChanges == 3U && host.saved_servers().size() == 1U
				   && projections.Published.back().SavedServers.size() == 1U,
			   "forgetting removes the server once the store answers");
		host.apply(application::saved_server_intent{
			.Action = application::saved_server_action::add, .Server = { .Hostname = "https://bad" } });
		expect(store.SavedServerChanges == 3U && host.statistics().Failures == 1U,
			   "a hostname outside the vocabulary is refused before the store");
	}

	void test_saved_repositories_are_durable_before_projection()
	{
		auto       store       = memory_queue_store{ };
		auto       refresh     = observed_refresh{ };
		auto       effects     = application::manual_executor{ };
		auto       decisions   = application::manual_executor{ };
		auto       projections = landing_log{ };
		const auto session     = application::queue_session_id{
			.Host = "github.com", .Account = "reviewer", .Generation = 4U
		};
		const auto kept = application::saved_repository{
			.Host = "github.com", .Account = "reviewer", .Repository = "octo/app"
		};
		store.State.SavedRepositories = { kept };
		auto host                     = application::review_queue_application_host{
			store, refresh, effects, decisions, projections, session
		};
		host.restore();
		run_one(effects, decisions);
		expect(host.saved_repositories().size() == 1U && host.saved_repositories().front() == kept
				   && projections.Published.back().SavedRepositories == std::vector{ kept },
			   "restore adopts the saved repositories and publishes them");

		const auto other = application::saved_repository{
			.Host = "github.com", .Account = "reviewer", .Repository = "octo/tools"
		};
		host.apply(application::saved_repository_intent{
			.Action = application::saved_repository_action::add, .Repository = other });
		expect(store.SavedChanges == 0U && host.saved_repositories().size() == 1U,
			   "a save is not stored on the caller lane and is not shown before the store answers");
		host.apply(application::saved_repository_intent{
			.Action = application::saved_repository_action::add, .Repository = other });
		expect(effects.pending() == 1U, "an identical save cannot overtake the pending one");
		run_one(effects, decisions);
		expect(store.SavedChanges == 1U && store.State.SavedRepositories == std::vector{ kept, other }
				   && host.saved_repositories().size() == 2U
				   && projections.Published.back().SavedRepositories == std::vector{ kept, other },
			   "the saved repository is durable, then projected");

		host.apply(application::saved_repository_intent{
			.Action = application::saved_repository_action::add, .Repository = other });
		host.apply(application::saved_repository_intent{
			.Action     = application::saved_repository_action::remove,
			.Repository = application::saved_repository{ .Host = "github.com", .Account = "reviewer", .Repository = "octo/never" } });
		expect(effects.pending() == 0U, "saving what is saved and forgetting what is not are no writes");
		host.apply(application::saved_repository_intent{
			.Action     = application::saved_repository_action::add,
			.Repository = application::saved_repository{ .Host = "github.com", .Account = "reviewer", .Repository = "no-slash" } });
		expect(effects.pending() == 0U && host.statistics().Failures == 1U,
			   "a repository that is not owner/name is refused before the store");

		store.FailEnrollmentStore = true;
		host.apply(application::saved_repository_intent{
			.Action = application::saved_repository_action::remove, .Repository = kept });
		run_one(effects, decisions);
		expect(host.saved_repositories().size() == 2U
				   && projections.Published.back().Status == application::queue_landing_status::unavailable,
			   "a refused forget leaves the list as it was and reports the store unavailable");
		store.FailEnrollmentStore = false;
		host.apply(application::saved_repository_intent{
			.Action = application::saved_repository_action::remove, .Repository = kept });
		run_one(effects, decisions);
		expect(host.saved_repositories().size() == 1U && host.saved_repositories().front() == other
				   && store.State.SavedRepositories == std::vector{ other }
				   && projections.Published.back().SavedRepositories == std::vector{ other },
			   "a forget is durable, then projected");
		host.shutdown();
	}

	void test_rapid_independent_preference_gestures_merge_in_the_host()
	{
		auto store       = memory_queue_store{ };
		auto refresh     = observed_refresh{ };
		auto effects     = application::manual_executor{ };
		auto decisions   = application::manual_executor{ };
		auto projections = landing_log{ };
		auto host        = application::review_queue_application_host{ store, refresh, effects, decisions, projections, { } };
		host.restore();
		run_one(effects, decisions);
		auto first                  = application::queue_presentation_preferences{ };
		first.Notifications.Enabled = false;
		host.apply({ first, application::queue_preference_field::notifications_enabled });
		auto second                 = application::queue_presentation_preferences{ };
		second.Notifications.Paused = true;
		host.apply({ second, application::queue_preference_field::notifications_paused });
		auto third = application::queue_presentation_preferences{ };
		third.Role = core::github_role::author;
		host.apply({ third, application::queue_preference_field::role });
		while (!host.is_quiescent()) {
			run_one(effects, decisions);
		}
		expect(!host.preferences().Notifications.Enabled && host.preferences().Notifications.Paused
				   && host.preferences().Role == core::github_role::author
				   && store.State.Preferences == host.preferences(),
			   "rapid independent gestures retain every edited field through persistence");
		host.shutdown();
	}

	void test_presentation_preferences_are_durable_before_projection()
	{
		auto store       = memory_queue_store{ };
		auto refresh     = observed_refresh{ };
		auto effects     = application::manual_executor{ };
		auto decisions   = application::manual_executor{ };
		auto projections = landing_log{ };
		auto host        = application::review_queue_application_host{
			store, refresh, effects, decisions, projections, { }
		};
		host.restore();
		run_one(effects, decisions);
		const auto preferences = application::queue_presentation_preferences{
			.Role           = core::github_role::requested_reviewer,
			.ObserverReason = core::observer_rule_kind::path,
			.Bucket         = core::attention_bucket::initial_review_requested,
			.Repository     = "octo/app",
			.Quiet          = application::queue_quiet_filter::attention,
			.Sort           = application::queue_sort_order::repository,
			.Notifications  = { .Enabled = false, .Paused = true },
		};
		host.apply(application::queue_preferences_intent{
			.Preferences = preferences,
		});
		expect(
			store.State.Preferences
				== application::queue_presentation_preferences{ },
			"the decision lane performs no preference write");
		expect(
			projections.Published.back().Preferences
				== application::queue_presentation_preferences{ },
			"an uncommitted preference is not projected as durable");
		run_one(effects, decisions);
		expect(
			store.State.Preferences == preferences
				&& projections.Published.back().Preferences == preferences,
			"the accepted preference is stored and projected together");
		auto refused              = preferences;
		refused.Notifications     = { };
		store.FailEnrollmentStore = true;
		host.apply(application::queue_preferences_intent{ refused });
		run_one(effects, decisions);
		expect(host.preferences() == preferences && projections.Published.back().Preferences == preferences,
			   "a failed notification preference store leaves the accepted choices intact");
		store.FailEnrollmentStore = false;
		host.shutdown();

		auto restartEffects     = application::manual_executor{ };
		auto restartDecisions   = application::manual_executor{ };
		auto restartProjections = landing_log{ };
		auto restarted          = application::review_queue_application_host{
			store,
			refresh,
			restartEffects,
			restartDecisions,
			restartProjections,
			{ }
		};
		restarted.restore();
		run_one(restartEffects, restartDecisions);
		expect(
			restartProjections.Published.back().Preferences == preferences,
			"a separate host lifetime restores the exact private preferences");
		restarted.shutdown();
	}
	// BPR-0344. A rule added while a refresh is already in flight used to lose
	// the enrollment entirely. The refresh completion is evaluated against the
	// rules as they stood when it was dispatched — none — and the rule
	// mutation's own reconciliation was captured before that observation
	// existed, so neither side ever matched the two together.
	//
	// Nothing in the queue depends on a clock or a filesystem, but which of the
	// two lands first does, which is why this reached a contract only on a host
	// whose pull-request open path was slower than another's.
	void test_a_rule_added_during_a_refresh_still_enrolls()
	{
		auto       store       = memory_queue_store{ };
		auto       refresh     = observed_refresh{ };
		auto       effects     = application::manual_executor{ };
		auto       decisions   = application::manual_executor{ };
		auto       projections = landing_log{ };
		const auto rule        = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		refresh.Observation = core::pull_request_observation{
			.Ref = ref(7), .Author = "octocat", .ChangedPaths = { "src/parser.cpp" }
		};

		auto host = application::review_queue_application_host{
			store,
			refresh,
			effects,
			decisions,
			projections,
			{ .Host = "github.com", .Account = "reviewer", .Generation = 4U }
		};
		host.restore();
		run_one(effects, decisions);
		expect(host.rules().empty(), "the host starts with no rules");

		// Both in flight at once, refresh first.
		host.refresh(ref(7));
		host.apply({ .Action = application::queue_rule_action::add, .Rule = rule });
		expect(effects.pending() == 2U, "the refresh and the rule mutation overlap");

		// The refresh completes while the rule is still uncommitted, so it is
		// judged against an empty rule set and matches nothing. That is
		// correct on its own terms and must stay that way.
		effects.run_next();
		decisions.run_next();
		expect(host.queue().empty(),
			   "a refresh judged before the rule exists matches nothing");

		// Committing the rule has to re-judge that observation. Before
		// BPR-0344 it re-judged nothing, because it only looked for a *missing*
		// observation and this one was present.
		effects.run_next();
		decisions.run_next();
		expect(host.rules().size() == 1U && host.rules().front() == rule,
			   "the rule is committed");
		expect(effects.pending() == 1U,
			   "committing the rule enrolls the observation it now matches");
		effects.run_next();
		decisions.run_next();

		expect(host.queue().size() == 1U,
			   "the pull request the rule matches reaches the queue");
		expect(host.queue().front().Membership.MatchedRule
				   && host.queue().front().Membership.Rule == rule,
			   "the queued item names the rule that matched it");
		expect(store.State.Enrollments.size() == 1U
				   && store.State.Enrollments.front().Membership.Rule == rule,
			   "the enrollment is durable, not only projected");
		host.shutdown();
	}

	[[nodiscard]] auto full_queue_state() -> application::durable_queue_state
	{
		auto state = application::durable_queue_state{ };
		for (std::size_t index = 0; index < application::maximum_queue_rules; ++index) {
			state.Rules.push_back(path_rule("path/" + std::to_string(index)));
		}
		for (std::size_t index = 0; index < application::maximum_saved_repositories; ++index) {
			state.SavedRepositories.push_back({ "github.com", "reviewer", "octo/repo" + std::to_string(index) });
		}
		for (std::size_t index = 0; index < application::maximum_saved_servers; ++index) {
			state.SavedServers.push_back({ "server" + std::to_string(index) + ".example", "reviewer" });
		}
		for (std::size_t index = 0; index < application::maximum_queue_enrollments; ++index) {
			state.Enrollments.push_back({ .Ref = ref(static_cast<std::int64_t>(index) + 1), .Membership = by_role(core::github_role::reviewer) });
		}
		state.Preferences.Notifications.Paused = true;
		return state;
	}

	void test_collection_boundaries_preserve_accepted_state()
	{
		auto store       = memory_queue_store{ };
		store.State      = full_queue_state();
		auto refresh     = observed_refresh{ };
		auto effects     = application::manual_executor{ };
		auto decisions   = application::manual_executor{ };
		auto projections = landing_log{ };
		auto host        = application::review_queue_application_host{ store, refresh, effects, decisions, projections, { } };
		host.restore();
		run_one(effects, decisions);
		const auto accepted = projections.Published.back();
		expect(accepted.Status == application::queue_landing_status::ready && accepted.Items.size() == application::maximum_queue_enrollments, "maximum collections are admitted");
		for (unsigned index = 0U; index < 256U; ++index) {
			projections.Published.clear();
			host.apply(application::queue_rule_intent{ .Rule = path_rule("BPR_DIAGNOSTIC_PRIVATE_CANARY/overflow") });
		}
		host.apply(application::saved_repository_intent{ .Repository = { "github.com", "reviewer", "octo/overflow" } });
		host.apply(application::saved_server_intent{ .Server = { "overflow.example", "reviewer" } });
		expect(effects.pending() == 0 && host.statistics().PersistenceRequests == 0, "overflow edits never reach persistence");
		expect(projections.Published.back().Failure == application::queue_store_failure::collection_limit && projections.Published.back().Status == application::queue_landing_status::ready, "overflow is a typed refusal that leaves accepted controls usable");
		expect(projections.Published.back().Rules == accepted.Rules && projections.Published.back().SavedRepositories == accepted.SavedRepositories && projections.Published.back().SavedServers == accepted.SavedServers && projections.Published.back().Items == accepted.Items && host.preferences() == accepted.Preferences, "overflow preserves accepted collections and unrelated preferences");
		refresh.Observation = { .Ref = ref(20000), .Role = core::github_role::author };
		host.adopt({ .Host = "github.com", .Account = "reviewer", .Generation = 1 });
		host.refresh(ref(20000), { });
		run_one(effects, decisions);
		expect(effects.pending() == 0 && store.State.Enrollments.size() == application::maximum_queue_enrollments && projections.Published.back().Failure == application::queue_store_failure::collection_limit, "new enrollment at capacity is refused before write");
		host.apply(application::saved_server_intent{ .Server = { "server0.example", "other" } });
		run_one(effects, decisions);
		expect(host.saved_servers().size() == application::maximum_saved_servers && !projections.Published.back().Failure.has_value(), "existing server remains editable at capacity");
	}

	void test_oversized_restores_are_atomic()
	{
		for (int domain = 0; domain < 4; ++domain) {
			auto store  = memory_queue_store{ };
			store.State = full_queue_state();
			switch (domain) {
			case 0:  store.State.Rules.push_back(path_rule("overflow")); break;
			case 1:  store.State.SavedRepositories.push_back({ "github.com", "reviewer", "octo/overflow" }); break;
			case 2:  store.State.SavedServers.push_back({ "overflow.example", "reviewer" }); break;
			default: store.State.Enrollments.push_back({ .Ref = ref(20000) }); break;
			}
			auto refresh     = observed_refresh{ };
			auto effects     = application::manual_executor{ };
			auto decisions   = application::manual_executor{ };
			auto projections = landing_log{ };
			auto host        = application::review_queue_application_host{ store, refresh, effects, decisions, projections, { } };
			host.restore();
			run_one(effects, decisions);
			const auto& rejected = projections.Published.back();
			expect(rejected.Failure == application::queue_store_failure::collection_limit && rejected.Rules.empty() && rejected.Items.empty() && rejected.SavedRepositories.empty() && rejected.SavedServers.empty() && !rejected.Preferences.Notifications.Paused, "each limit plus one restore is rejected atomically before projection");
		}
	}

	struct admitted_queue_windows final : application::queue_window_context_port
	{
		[[nodiscard]] auto current(const core::bpr_window_context& aContext) const noexcept -> bool override
		{
			return std::ranges::find(Live, aContext) != Live.end();
		}
		std::vector<core::bpr_window_context> Live;
	};

	[[nodiscard]] auto queue_window(const std::uint64_t aWindow, const std::uint64_t aGeneration = 1U) -> core::bpr_window_context
	{
		return { core::bpr_profile_id{ 2U }, core::bpr_window_id{ aWindow }, core::bpr_window_generation{ aGeneration } };
	}

	struct window_queue_log final : application::queue_window_projection_port
	{
		void publish(const core::bpr_window_context& aContext, const application::queue_landing_projection& aProjection) override
		{
			expect(!Delivering, "window callbacks are never recursive");
			Delivering = true;
			Contexts.push_back(aContext);
			Published.push_back(aProjection);
			const auto callback = Callback;
			if (callback) {
				callback();
			}
			Delivering = false;
		}
		std::vector<core::bpr_window_context>              Contexts;
		std::vector<application::queue_landing_projection> Published;
		std::function<void()>                              Callback;
		bool                                               Delivering = false;
	};

	void test_known_refresh_delta_and_complete_attachment_replay()
	{
		for (const auto count : { 32, 4096 }) {
			auto store = memory_queue_store{ };
			for (auto number = 1; number <= count; ++number) {
				store.State.Enrollments.push_back({ .Ref = ref(number), .Membership = by_role(core::github_role::reviewer) });
			}
			auto refresh   = observed_refresh{ };
			auto effects   = application::manual_executor{ };
			auto decisions = application::manual_executor{ };
			auto windows   = admitted_queue_windows{ };
			windows.Live   = { queue_window(1U), queue_window(2U) };
			auto host      = application::review_queue_application_host{ store, refresh, effects, decisions, core::bpr_profile_id{ 2U }, windows, { .Host = "github.com", .Account = "reviewer", .Generation = 4U } };
			host.restore();
			run_one(effects, decisions);
			auto first = std::make_shared<window_queue_log>();
			expect(host.attach(queue_window(1U), first).has_value(), "first consumer receives full baseline");
			const auto before   = host.statistics();
			refresh.Observation = { .Ref = ref(count), .Author = "fixture", .Role = core::github_role::reviewer, .Title = "one changed item" };
			host.refresh(ref(count), { .UnseenComments = 2U });
			run_one(effects, decisions);
			run_one(effects, decisions);
			const auto after = host.statistics();
			expect(after.ItemPublications == before.ItemPublications + 1U && after.Projection == before.Projection
					   && after.PersistenceRequests == before.PersistenceRequests + 1U && effects.pending() == 0U,
				   "accepted metadata persists and updates one item without a catalogue projection");
			const auto& delta = first->Published.back();
			expect(delta.Items.empty() && delta.UpdatedItem && delta.UpdatedItem->Ref == ref(count) && delta.UpdatedItem->Facts.UnseenComments == 2U, "caught-up consumer receives exact changed identity");
			auto second = std::make_shared<window_queue_log>();
			expect(host.attach(queue_window(2U), second).has_value(), "new consumer attaches after a delta");
			expect(!second->Published.back().UpdatedItem && second->Published.back().Items == host.queue(), "new consumer receives complete current state, never a delta without its base");
			first->Callback = [&] {
				first->Callback         = { };
				refresh.Observation.Ref = ref(count - 1);
				host.refresh(ref(count - 1), { .UnseenComments = 4U });
				run_one(effects, decisions);
				run_one(effects, decisions);
			};
			host.refresh(ref(count), { .UnseenComments = 3U });
			run_one(effects, decisions);
			expect(!second->Published.back().UpdatedItem && second->Published.back().Items == host.queue(), "a reentrant publication cannot give a lagging consumer a delta with a missing base");
			std::cout << "queue-delta records=" << count << " changed=1 projected_catalogue_rows=" << after.Projection.RecordsVisited - before.Projection.RecordsVisited << " item_publications=" << after.ItemPublications - before.ItemPublications << '\n';
			host.shutdown();
		}
	}

	struct profile_queue_log final : application::queue_landing_projection_port
	{
		void publish(const application::queue_landing_projection&) override
		{
			expect(!Delivering, "profile observer is never recursive");
			Delivering = true;
			++Publications;
			const auto callback = Callback;
			if (callback) {
				callback();
			}
			Delivering = false;
		}
		std::function<void()> Callback;
		std::size_t           Publications = 0U;
		bool                  Delivering   = false;
	};

	struct legacy_queue_log final : application::queue_landing_projection_port
	{
		void publish(const application::queue_landing_projection& aProjection) override
		{
			expect(!Delivering, "legacy combined sink is never called recursively");
			Delivering = true;
			Accounts.push_back(aProjection.Session.Account);
			const auto callback = Callback;
			if (callback) {
				callback();
			}
			Delivering = false;
		}
		std::vector<std::string> Accounts;
		std::function<void()>    Callback;
		bool                     Delivering = false;
	};

	void test_legacy_combined_sink_keeps_every_reentrant_observation()
	{
		auto store     = memory_queue_store{ };
		auto refresh   = observed_refresh{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto sink      = legacy_queue_log{ };
		auto host      = application::review_queue_application_host{ store, refresh, effects, decisions, sink, { } };
		sink.Callback  = [&] {
			sink.Callback = { };
			host.adopt({ .Host = "github.com", .Account = "B", .Generation = 2U });
			host.adopt({ .Host = "github.com", .Account = "C", .Generation = 3U });
		};
		host.adopt({ .Host = "github.com", .Account = "A", .Generation = 1U });
		expect(sink.Accounts == std::vector<std::string>{ "A", "B", "C" }, "legacy notification/projection sink receives every fresh state exactly once in order");
		expect(host.statistics().Publications == 3U && effects.pending() == 0U, "fanout introduces neither duplicate publication nor effects");
	}

	void test_rule_publication_shutdown_never_posts_reconciliation()
	{
		auto store     = memory_queue_store{ };
		auto refresh   = observed_refresh{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto sink      = legacy_queue_log{ };
		auto host      = application::review_queue_application_host{ store, refresh, effects, decisions, sink, { } };
		host.restore();
		run_one(effects, decisions);
		refresh.Observation = { .Ref = ref(7), .Role = core::github_role::reviewer };
		host.refresh(ref(7));
		run_one(effects, decisions);
		run_one(effects, decisions);
		sink.Callback = [&] { host.shutdown(); };
		host.apply(application::queue_rule_intent{ .Rule = path_rule("unrelated") });
		run_one(effects, decisions);
		expect(host.is_quiescent() && effects.pending() == 0U && decisions.pending() == 0U, "shutdown during accepted rule publication cannot launch a later enrollment reconciliation");
	}

	void test_restore_publication_shutdown_never_posts_a_later_load()
	{
		auto store     = memory_queue_store{ };
		auto refresh   = observed_refresh{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto windows   = admitted_queue_windows{ };
		windows.Live   = { queue_window(1U) };
		auto host      = application::review_queue_application_host{ store, refresh, effects, decisions, core::bpr_profile_id{ 2U }, windows, { } };
		auto sink      = std::make_shared<window_queue_log>();
		expect(host.attach(queue_window(1U), sink).has_value(), "window attaches before initial restore");
		sink->Callback = [&] { host.shutdown(); expect(host.is_quiescent(), "shutdown is safe before any restore effect is reserved"); };
		host.restore();
		expect(effects.pending() == 0U && decisions.pending() == 0U && store.Loads == 0U && host.is_quiescent(), "restore cannot schedule work after shutdown reported quiescence");
		host.restore();
		expect(effects.pending() == 0U && store.Loads == 0U, "closed restore remains effect-free");
	}

	void test_profile_queue_fanout_replays_without_repeating_effects()
	{
		auto store     = memory_queue_store{ };
		auto refresh   = observed_refresh{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto windows   = admitted_queue_windows{ };
		windows.Live   = { queue_window(1U), queue_window(2U) };
		auto observer  = std::make_shared<profile_queue_log>();
		auto host      = application::review_queue_application_host{ store, refresh, effects, decisions, core::bpr_profile_id{ 2U }, windows, { }, observer };
		host.restore();
		run_one(effects, decisions);
		expect(store.Loads == 1U && observer->Publications == 2U, "owner restores once without any initial window");
		auto       first  = std::make_shared<window_queue_log>();
		auto       second = std::make_shared<window_queue_log>();
		const auto one    = host.attach(queue_window(1U), first);
		const auto two    = host.attach(queue_window(2U), second);
		expect(one.has_value() && two.has_value() && first->Published.size() == 1U && second->Published == first->Published && observer->Publications == 2U, "two initial replays do not repeat notification observations");
		auto preferences                 = host.preferences();
		preferences.Notifications.Paused = true;
		host.apply(application::queue_preferences_intent{ preferences });
		run_one(effects, decisions);
		expect(host.statistics().PersistenceRequests == 1U && observer->Publications == 3U && first->Published.size() == 2U && second->Published == first->Published, "one durable mutation reaches both windows and one profile observer");
		refresh.Observation = { .Ref = ref(7), .Role = core::github_role::reviewer };
		host.refresh(ref(7));
		run_one(effects, decisions);
		run_one(effects, decisions);
		expect(refresh.Requests.size() == 1U && store.State.Enrollments.size() == 1U && observer->Publications == 4U && first->Published.back().Items.size() == 1U && second->Published == first->Published, "one refresh and enrollment observation feed both consumers");
		const auto weakFirst = std::weak_ptr{ first };
		host.detach(*one);
		first.reset();
		expect(weakFirst.expired(), "profile owner releases the first window entirely on detach");
		host.detach(*two);
		host.refresh(ref(8));
		host.shutdown();
		run_one(effects, decisions);
		expect(host.is_quiescent() && host.attached_consumers() == 0U && observer->Publications == 4U, "last-consumer shutdown rejects late refresh publication");
		const auto closed = host.attach(queue_window(1U), second);
		expect(!closed && closed.error() == application::queue_attachment_failure::closing, "shutdown cannot acquire new windows");
	}

	void test_queue_consumers_recheck_exact_generation_and_reentrant_detach()
	{
		auto store     = memory_queue_store{ };
		auto refresh   = observed_refresh{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto windows   = admitted_queue_windows{ };
		windows.Live   = { queue_window(1U), queue_window(2U) };
		auto host      = application::review_queue_application_host{ store, refresh, effects, decisions, core::bpr_profile_id{ 2U }, windows, { } };
		host.restore();
		run_one(effects, decisions);
		auto       first  = std::make_shared<window_queue_log>();
		auto       second = std::make_shared<window_queue_log>();
		const auto one    = host.attach(queue_window(1U), first);
		const auto two    = host.attach(queue_window(2U), second);
		expect(one.has_value() && two.has_value(), "both exact windows attach");
		first->Callback = [&] { first->Callback = {}; host.detach(*two); host.adopt({.Host = "github.com", .Account = "new", .Generation = 2U}); };
		host.adopt({ .Host = "github.com", .Account = "reviewer", .Generation = 1U });
		expect(second->Published.size() == 1U && first->Published.back().Session.Account == "new", "reentrant detach prevents stale sibling callbacks and queues newer delivery");
		windows.Live[0] = queue_window(1U, 2U);
		host.adopt({ .Host = "github.com", .Account = "later", .Generation = 3U });
		expect(first->Published.back().Session.Account == "new", "retired generation receives no later callback even without explicit detach");
		const auto stale          = host.attach(queue_window(1U), first);
		auto       foreignContext = queue_window(3U);
		foreignContext.Profile    = core::default_bpr_profile_id;
		windows.Live.push_back(foreignContext);
		const auto foreign = host.attach(foreignContext, first);
		expect(!stale && stale.error() == application::queue_attachment_failure::stale_window && !foreign, "stale and foreign attachment authority are refused");
		const auto replacement = host.attach(queue_window(1U, 2U), first);
		expect(replacement.has_value() && first->Contexts.back() == queue_window(1U, 2U), "fresh generation receives exact-context initial replay");
		host.detach(*one);
		expect(host.attached_consumers() == 1U, "stale token cannot detach replacement generation");
		const auto duplicate = host.attach(queue_window(1U, 2U), second);
		expect(!duplicate && duplicate.error() == application::queue_attachment_failure::invalid_consumer, "same live window cannot duplicate a consumer");
	}

	void test_queue_consumer_capacity_counts_only_live_attachments()
	{
		auto store     = memory_queue_store{ };
		auto refresh   = observed_refresh{ };
		auto effects   = application::manual_executor{ };
		auto decisions = application::manual_executor{ };
		auto windows   = admitted_queue_windows{ };
		auto host      = application::review_queue_application_host{ store, refresh, effects, decisions, core::bpr_profile_id{ 2U }, windows, { } };
		auto sink      = std::make_shared<window_queue_log>();
		for (std::uint64_t index = 1U; index <= 100U; ++index) {
			windows.Live        = { queue_window(index) };
			const auto attached = host.attach(queue_window(index), sink);
			expect(attached.has_value(), "repeated lifetime churn does not exhaust concurrent capacity");
			host.detach(*attached);
		}
		for (std::uint64_t index = 1U; index <= core::maximum_bpr_windows_per_profile + 1U; ++index) {
			windows.Live.push_back(queue_window(index));
			const auto attached = host.attach(queue_window(index), sink);
			if (index <= core::maximum_bpr_windows_per_profile) {
				expect(attached.has_value(), "all32 live consumer slots are usable");
			} else {
				expect(!attached && attached.error() == application::queue_attachment_failure::capacity, "concurrent consumer33 is explicitly refused");
			}
		}
	}

	void test_queue_callback_failure_and_observer_backlog_are_isolated()
	{
		auto store        = memory_queue_store{ };
		auto refresh      = observed_refresh{ };
		auto effects      = application::manual_executor{ };
		auto decisions    = application::manual_executor{ };
		auto windows      = admitted_queue_windows{ };
		windows.Live      = { queue_window(1U), queue_window(2U) };
		auto observer     = std::make_shared<profile_queue_log>();
		auto host         = application::review_queue_application_host{ store, refresh, effects, decisions, core::bpr_profile_id{ 2U }, windows, { }, observer };
		auto failing      = std::make_shared<window_queue_log>();
		auto healthy      = std::make_shared<window_queue_log>();
		failing->Callback = [] { throw std::runtime_error{ "fixture callback" }; };
		expect(host.attach(queue_window(1U), failing).has_value() && host.attach(queue_window(2U), healthy).has_value(), "consumers attach before first publication");
		host.restore();
		run_one(effects, decisions);
		expect(host.projection_delivery_failed() && healthy->Published.size() == 2U && host.attached_consumers() == 1U, "throwing window is retired without skipping healthy consumers");
		observer->Callback = [&] {
			observer->Callback = { };
			for (std::uint64_t generation = 1U; generation <= core::maximum_bpr_windows_per_profile + 1U; ++generation) {
				host.adopt({ .Host = "github.com", .Account = "feedback", .Generation = generation });
			}
		};
		host.adopt({ .Host = "github.com", .Account = "begin", .Generation = 100U });
		expect(healthy->Published.back().Session.Generation == core::maximum_bpr_windows_per_profile + 1U && !healthy->Delivering, "bounded observer feedback still delivers latest state without recursive callbacks");
		const auto retired = observer->Publications;
		host.adopt({ .Host = "github.com", .Account = "after", .Generation = 101U });
		expect(observer->Publications == retired && healthy->Published.back().Session.Account == "after", "overflow retires only the observer while healthy windows continue");
	}

	// ---- [[BPR-0989]] personal discovery ------------------------------------

	struct recording_discovery final : application::personal_discovery_port
	{
		void discover(application::personal_discovery_request aRequest, application::personal_discovery_completion_handler aCompletion) override
		{
			Requests.push_back(aRequest);
			Handlers.push_back(std::move(aCompletion));
		}
		void cancel_discovery(const application::personal_discovery_request& aRequest) override
		{
			Cancelled.push_back(aRequest.Context.Request);
		}
		/// Answers the oldest unanswered request.
		void answer(application::personal_discovery_completion aCompletion)
		{
			expect(Answered < Handlers.size(), "a request is waiting to be answered");
			Handlers[Answered++](std::move(aCompletion));
		}
		[[nodiscard]] auto next() const -> const application::personal_discovery_request&
		{
			return Requests[Answered];
		}
		[[nodiscard]] auto rows(std::initializer_list<application::personal_discovery_row> aRows,
								application::personal_discovery_completeness               aCompleteness = application::personal_discovery_completeness::complete) const
			-> application::personal_discovery_completion
		{
			return application::personal_discovery_success{ .Request = next(), .Rows = aRows, .Completeness = aCompleteness };
		}
		[[nodiscard]] auto failure(const application::github_provider_failure aFailure, std::optional<std::uint32_t> aRetry = std::nullopt, const bool aSecondary = false) const
			-> application::personal_discovery_completion
		{
			auto error = application::github_provider_error{ .Failure = aFailure, .RetryAfterSeconds = aRetry };
			if (aSecondary) {
				error.RateLimitCause = application::github_rate_limit_record{ .SecondaryLimited = true };
			}
			return application::personal_discovery_error{ .Request = next(), .Error = error };
		}
		std::vector<application::personal_discovery_request>            Requests;
		std::vector<application::personal_discovery_completion_handler> Handlers;
		std::vector<core::request_id>                                   Cancelled;
		std::size_t                                                     Answered = 0U;
	};

	struct manual_clock final : application::personal_discovery_clock_port
	{
		[[nodiscard]] auto now_seconds() const -> std::int64_t override { return Now; }
		std::int64_t       Now = 1'000;
	};

	/// [[BPR-1680]]. Keeps every event discovery records, in order.
	struct recording_diagnostics final : application::diagnostic_log_port
	{
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			try {
				Events.push_back(aEvent);
			} catch (...) {
				return application::diagnostic_log_status::dropped;
			}
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
		/// The discovery lines recorded, read back.
		[[nodiscard]] auto lines() const -> std::vector<core::discovery_diagnosis>
		{
			auto result = std::vector<core::discovery_diagnosis>{ };
			for (const auto& event : Events) {
				const auto parsed = core::parse_discovery_diagnosis(event.Summary.text());
				expect(event.Operation == application::diagnostic_operation::discovery && parsed.has_value() && event.Request == 0U && event.WindowSession == 0U,
					   "every discovery event is a discovery line with no request or window identity");
				result.push_back(*parsed);
			}
			return result;
		}
		std::vector<application::diagnostic_event> Events;
	};

	[[nodiscard]] auto discovery_session(const std::string_view aAccount = "reviewer", const std::uint64_t aGeneration = 4U)
		-> application::personal_discovery_session
	{
		const auto account = core::github_account_id{ aAccount == "reviewer" ? 1001U : 1002U };
		return {
			.Queue   = { .Host = "github.com", .Account = std::string{ aAccount }, .Generation = aGeneration, .ViewerAccount = account },
			.Context = {
				.ApplicationSession = core::application_session_id{ 7U },
				.Profile            = core::host_profile_id{ 11U },
				.Host               = core::git_host_id{ 22U },
				.Account            = account,
				.Capabilities       = core::capability_revision{ 44U },
				.IdentityGeneration = core::github_identity_generation{ aGeneration },
			},
			.Viewer = { .Account = account, .Node = application::github_actor_id{ "U_" + std::string{ aAccount } }, .Login = std::string{ aAccount } },
		};
	}

	[[nodiscard]] auto row(const std::uint32_t aNumber, const bool aDetailed = true, const std::uint32_t aAwaited = 0U) -> application::personal_discovery_row
	{
		return { .Number = core::pull_request_number{ aNumber }, .Title = "Pull request " + std::to_string(aNumber), .Author = "author", .Detailed = aDetailed, .ReviewersAwaited = aAwaited };
	}

	struct discovery_fixture
	{
		explicit discovery_fixture(std::vector<application::saved_repository> aSaved = {
									   { .Host = "github.com", .Account = "reviewer", .Repository = "octo/tools" },
									   { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" },
									   { .Host = "github.com", .Account = "someone-else", .Repository = "octo/other" },
									   { .Host = "ghe.example", .Account = "reviewer", .Repository = "octo/app" },
								   },
								   std::vector<application::enrollment_record> aRetained = { }, const bool aRestore = true,
								   // [[BPR-1680]]. The scheduler's own tests search each repository
								   // on its own, the shape they were written for; the batching
								   // tests pass the production options.
								   const application::personal_discovery_options aOptions = { .SearchRepositories = 1U, .RetainedBatch = 1U })
			: Options{ aOptions }
		{
			Windows.Live                  = { queue_window(1U), queue_window(2U) };
			Store.State.SavedRepositories = std::move(aSaved);
			Store.State.Enrollments       = std::move(aRetained);
			if (aRestore) {
				Queue.restore();
				run_one(Effects, Decisions);
			}
			expect(Queue.attach(queue_window(1U), First).has_value() && Queue.attach(queue_window(2U), Second).has_value(), "two windows attach");
		}
		/// Runs every posted decision and every posted store effect.
		void settle()
		{
			while (Decisions.pending() != 0U || Effects.pending() != 0U) {
				Decisions.drain();
				Effects.drain();
			}
		}
		[[nodiscard]] auto item(const std::int64_t aNumber) const -> std::optional<core::queue_item>
		{
			const auto items = Queue.queue();
			const auto found = std::ranges::find_if(items, [aNumber](const core::queue_item& aItem) { return aItem.Ref.Number == aNumber; });
			return found == items.end() ? std::nullopt : std::optional{ *found };
		}
		[[nodiscard]] auto record(const std::int64_t aNumber) const -> std::optional<application::enrollment_record>
		{
			const auto records = Queue.enrollments();
			const auto found   = std::ranges::find_if(records, [aNumber](const application::enrollment_record& aRecord) { return aRecord.Ref.Number == aNumber; });
			return found == records.end() ? std::nullopt : std::optional{ *found };
		}

		memory_queue_store                         Store;
		observed_refresh                           Refresh;
		application::manual_executor               Effects;
		application::manual_executor               Decisions;
		admitted_queue_windows                     Windows;
		std::shared_ptr<window_queue_log>          First  = std::make_shared<window_queue_log>();
		std::shared_ptr<window_queue_log>          Second = std::make_shared<window_queue_log>();
		application::review_queue_application_host Queue{ Store, Refresh, Effects, Decisions, core::bpr_profile_id{ 2U }, Windows, { } };
		recording_discovery                        Provider;
		manual_clock                               Clock;
		recording_diagnostics                      Diagnostics;
		application::personal_discovery_options    Options;
		application::personal_discovery_host       Discovery{ Provider, Decisions, Queue, Clock, &Diagnostics, Options };
	};

	[[nodiscard]] auto metadata_row(const std::uint32_t aNumber, const bool aReviewed = false, const bool aMoved = false)
		-> application::personal_discovery_row
	{
		auto result    = row(aNumber);
		result.HeadOid = application::commit_oid{ std::string(40U, aMoved ? 'b' : 'a') };
		if (aReviewed) {
			result.Reviews.push_back({ .DatabaseId = 1U, .Author = application::github_actor_id{ "U_reviewer" }, .State = application::github_review_state::approved, .Commit = application::commit_oid{ std::string(40U, 'a') } });
		}
		result.Waiting.Requests = result.ReviewersAwaited;
		return result;
	}

	void answer_retained(discovery_fixture& aFixture)
	{
		while (aFixture.Provider.Answered < aFixture.Provider.Requests.size()
			   && aFixture.Provider.next().Reason == application::personal_discovery_reason::retained) {
			aFixture.Provider.answer(aFixture.Provider.rows({ metadata_row(aFixture.Provider.next().Number->value()) }));
			aFixture.settle();
		}
	}

	void test_retained_titles_and_reviews_before_open_and_on_refresh()
	{
		auto other    = ref(18);
		other.Account = "someone-else";
		auto fixture  = discovery_fixture{ { }, { { .Ref = ref(17), .Membership = { .Requested = true } }, { .Ref = other, .Membership = { .Followed = true } } } };
		fixture.Discovery.adopt(discovery_session());
		expect(fixture.Provider.Requests.size() == 1U && fixture.Provider.next().Reason == application::personal_discovery_reason::retained
				   && fixture.Provider.next().Number == core::pull_request_number{ 17U },
			   "restored rows read exact metadata even with no saved repository; another account is excluded");
		expect(fixture.item(17)->Facts.Title.empty() && fixture.item(17)->Bucket == core::attention_bucket::initial_review_requested, "restored queue initially has no title or review facts");
		fixture.Provider.answer(fixture.Provider.rows({ metadata_row(17U, true) }));
		fixture.settle();
		expect(fixture.item(17)->Facts.Title == "Pull request 17" && fixture.item(17)->Bucket == core::attention_bucket::quiet
				   && fixture.record(17)->Membership.Reviewed && fixture.Refresh.Requests.empty(),
			   "Home fills title and moves an already submitted review to Following without opening a PR");
		fixture.Discovery.refresh();
		fixture.Provider.answer(fixture.Provider.rows({ metadata_row(17U, true, true) }));
		fixture.settle();
		expect(fixture.item(17)->Bucket == core::attention_bucket::changed_since_my_review, "explicit Refresh notices a head changed since the submitted review");
		fixture.Discovery.refresh();
		auto changed  = metadata_row(17U, true);
		changed.Title = "Renamed after review";
		changed.Reviews.push_back({ .DatabaseId = 2U, .Author = application::github_actor_id{ "U_reviewer" }, .State = application::github_review_state::pending, .Commit = application::commit_oid{ std::string(40U, 'b') } });
		fixture.Provider.answer(fixture.Provider.rows({ changed }));
		fixture.settle();
		expect(fixture.item(17)->Bucket == core::attention_bucket::quiet && fixture.item(17)->Facts.Title == changed.Title, "Refresh updates an existing title and ignores a later pending review");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_retained_summary_limit_keeps_complete_metadata()
	{
		auto fixture = discovery_fixture{ { }, { { .Ref = ref(17), .Membership = { .Followed = true } } } };
		fixture.Discovery.adopt(discovery_session());
		auto row           = metadata_row(17, true);
		row.WaitingLimited = true;
		fixture.Provider.answer(fixture.Provider.rows({ row }));
		fixture.settle();
		expect(fixture.item(17)->Facts.Title == row.Title && fixture.item(17)->Bucket == core::attention_bucket::quiet && fixture.Discovery.status().State == application::queue_discovery_state::partial && fixture.Discovery.status().Problem == application::queue_discovery_problem::detail_limit,
			   "retained summary limit publishes complete title/review facts and explicit partial status");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	// [[BPR-1407]]. A row's age, size and comment count come with the read
	// that reported them, outlive one that did not, and survive the store.
	void test_change_facts_merge_and_persist()
	{
		auto fixture = discovery_fixture{ { }, { { .Ref = ref(17), .Membership = { .Followed = true } } } };
		fixture.Discovery.adopt(discovery_session());
		auto read   = metadata_row(17U, true);
		read.Change = { .OpenedAt = 1'760'000'000, .Additions = 1204U, .Deletions = 318U, .Comments = 14U };
		fixture.Provider.answer(fixture.Provider.rows({ read }));
		fixture.settle();
		expect(fixture.item(17) && fixture.item(17)->Facts.Change == read.Change, "a retained read carries age, size and comments onto the row");
		const auto session = discovery_session().Queue;
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Read without change" }, .Activity = { .Known = true, .FactRevision = fixture.Queue.begin_observation() } });
		fixture.settle();
		expect(fixture.item(17)->Facts.Change == read.Change, "a read that did not report them keeps the last known");
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Read with a comment", .Change = { .Comments = 15U } }, .Activity = { .Known = true, .FactRevision = fixture.Queue.begin_observation() } });
		fixture.settle();
		const auto change = fixture.item(17)->Facts.Change;
		expect(change.Comments == 15U && change.Additions == 1204U && change.OpenedAt == read.Change.OpenedAt, "a read replaces only what it reported");
		const auto durable = fixture.record(17);
		expect(durable && durable->LastFacts.Change == change, "the private record keeps them");
		const auto encoded = application::encode_queue_facts(durable->LastFacts);
		const auto decoded = encoded ? application::decode_queue_facts(*encoded) : std::nullopt;
		expect(decoded && decoded->Change == change, "the store's codec carries them across a restart");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_change_facts_codec_reads_version_one()
	{
		auto       facts   = core::queue_item_facts{ .Title = "Old", .Author = "octocat" };
		const auto encoded = application::encode_queue_facts(facts).value_or(std::string{ });
		const auto unknown = std::string{ "0:0:0:0:" };
		expect(encoded.starts_with("2;") && encoded.ends_with(unknown), "unknown change facts are four empty fields of version 2");
		const auto body    = encoded.substr(2U, encoded.size() - 2U - unknown.size());
		const auto version = application::decode_queue_facts("1;" + body);
		expect(version && version->Title == "Old" && version->Author == "octocat" && version->Change == core::queue_change_facts{ }, "a version 1 record still reads, with its change unknown");
		expect(!application::decode_queue_facts("1;" + body + unknown), "version 1 never carries change facts");
		expect(!application::decode_queue_facts("2;" + body), "version 2 without its change fields is refused");
		facts.Change    = { .OpenedAt = 0, .Additions = 0U, .Deletions = 0U, .Comments = 0U };
		const auto zero = application::decode_queue_facts(application::encode_queue_facts(facts).value_or(std::string{ }));
		expect(zero && zero->Change == facts.Change, "zero is a known value, never read back as unknown");
		facts.Change.OpenedAt = -1;
		expect(!application::encode_queue_facts(facts), "an opening before the epoch is refused");
	}

	void test_retained_review_survives_request_withdrawal()
	{
		auto fixture = discovery_fixture{ { { "github.com", "reviewer", "octo/app" } }, { { .Ref = ref(17), .Membership = { .Requested = true } } } };
		fixture.Discovery.adopt(discovery_session());
		fixture.Provider.answer(fixture.Provider.rows({ metadata_row(17U) }));
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ row(17U) }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		fixture.Discovery.refresh();
		expect(fixture.Provider.next().Reason == application::personal_discovery_reason::retained, "Refresh reads submitted-review facts before withdrawing old discovery membership");
		fixture.Provider.answer(fixture.Provider.rows({ metadata_row(17U, true) }));
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.item(17) && fixture.item(17)->Bucket == core::attention_bucket::quiet
				   && fixture.record(17)->Membership.Reviewed && !fixture.record(17)->Membership.Requested,
			   "a review submitted during this session keeps the PR in Following after GitHub drops its request");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_retained_metadata_preserves_local_attention_and_rejects_late_results()
	{
		auto fixture = discovery_fixture{ { }, { { .Ref = ref(17), .Membership = { .Followed = true } } } };
		fixture.Discovery.adopt(discovery_session());
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = discovery_session().Queue, .Ref = ref(17), .Facts = { .Title = "Previously read", .UnseenComments = 3U }, .Activity = { .UnseenComments = 3U, .DefectsAwaitingVerification = 2U } });
		fixture.Provider.answer(fixture.Provider.rows({ metadata_row(17U, true) }));
		fixture.settle();
		expect(fixture.item(17)->Facts.UnseenComments == 3U && fixture.item(17)->Bucket == core::attention_bucket::new_comments_or_author_replies, "metadata refresh preserves local unread attention");
		fixture.Discovery.refresh();
		auto late = fixture.Provider.rows({ metadata_row(17U) });
		fixture.Queue.apply(application::queue_follow_intent{ .Ref = ref(17), .Follow = false });
		fixture.settle();
		fixture.Provider.answer(std::move(late));
		fixture.settle();
		expect(!fixture.item(17) && fixture.Discovery.statistics().StaleCompletions == 1U, "late metadata cannot revive an unfollowed PR");
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = discovery_session().Queue, .Ref = ref(17), .Facts = { .Title = "Late title" } });
		expect(!fixture.item(17), "queue metadata cannot enroll or revive a PR");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_draft_metadata_retains_only_accepted_exact_account_facts()
	{
		auto       fixture = discovery_fixture{ { }, { { .Ref = ref(17), .Membership = { .Followed = true } } } };
		const auto session = discovery_session();
		fixture.Discovery.adopt(session);
		auto draft  = metadata_row(17U);
		draft.Draft = true;
		fixture.Provider.answer(fixture.Provider.rows({ draft }));
		fixture.settle();
		expect(fixture.item(17)->Facts.Draft == true && fixture.Store.State.Enrollments.front().Draft == true && fixture.record(17)->DraftAccount == session.Queue.ViewerAccount, "accepted unopened draft persists under its numeric account");
		fixture.Discovery.refresh();
		fixture.Provider.answer(fixture.Provider.failure(application::github_provider_failure::network));
		fixture.settle();
		expect(fixture.item(17)->Facts.Draft == true, "failed refresh retains the accepted draft");
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session.Queue, .Ref = ref(17), .Facts = { .Title = "ready", .Draft = false } });
		fixture.settle();
		expect(fixture.item(17)->Facts.Draft == false && fixture.record(17)->Draft == false, "accepted ready removes the draft without changing membership");
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session.Queue, .Ref = ref(17), .Facts = { .Title = "unknown" } });
		expect(fixture.item(17)->Facts.Draft == false, "omitted fact never fabricates ready or erases accepted status");
		auto foreign = session.Queue;
		++foreign.Generation;
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = foreign, .Ref = ref(17), .Facts = { .Draft = true } });
		expect(fixture.item(17)->Facts.Draft == false, "late session cannot replace draft status");
		foreign               = session.Queue;
		foreign.ViewerAccount = core::github_account_id{ 9999U };
		fixture.Queue.adopt(foreign);
		fixture.settle();
		expect(!fixture.item(17)->Facts.Draft, "same login under another numeric account cannot reuse retained metadata");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_retained_metadata_budget_and_restore_after_session()
	{
		auto records = std::vector<application::enrollment_record>{ };
		for (std::int64_t number = 1; number <= 31; ++number) {
			records.push_back({ .Ref = ref(number), .Membership = { .Followed = true } });
		}
		auto fixture = discovery_fixture{ { }, records, false };
		fixture.Discovery.adopt(discovery_session());
		expect(fixture.Provider.Requests.empty(), "session adopted before queue restoration has no metadata target yet");
		fixture.Queue.restore();
		fixture.settle();
		fixture.Discovery.reconcile();
		answer_retained(fixture);
		expect(fixture.Provider.Requests.size() == application::maximum_personal_discovery_retained
				   && fixture.Discovery.statistics().MaximumInFlight <= application::maximum_personal_discovery_in_flight,
			   "restoration triggers at most thirty retained reads and two in flight");
		const auto submitted = fixture.Provider.Requests.size();
		fixture.Discovery.reconcile();
		expect(fixture.Provider.Requests.size() == submitted, "unread rows beyond the budget cause no projection-driven retry loop");
		fixture.Discovery.refresh();
		expect(fixture.Provider.next().Number == core::pull_request_number{ 31U }, "a later explicit refresh gives the remaining missing title priority");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_retained_failure_and_foreign_envelopes_preserve_accepted_state()
	{
		auto fixture = discovery_fixture{ { { "github.com", "reviewer", "octo/app" } }, { { .Ref = ref(17), .Membership = { .Requested = true } } } };
		fixture.Discovery.adopt(discovery_session());
		fixture.Provider.answer(fixture.Provider.failure(application::github_provider_failure::permission));
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.item(17) && fixture.item(17)->Facts.Title.empty() && fixture.Discovery.status().State == application::queue_discovery_state::stale,
			   "metadata failure preserves the old request when a search cannot distinguish approval from withdrawal");
		const auto before = fixture.Provider.Requests.size();
		fixture.Discovery.reconcile();
		expect(fixture.Provider.Requests.size() == before, "failed missing-title read does not restart from its projection");
		fixture.Discovery.refresh();
		auto foreign                = std::get<application::personal_discovery_success>(fixture.Provider.rows({ metadata_row(17U, true) }));
		foreign.Request.Viewer.Node = application::github_actor_id{ "U_foreign" };
		fixture.Provider.answer(std::move(foreign));
		fixture.settle();
		expect(fixture.item(17)->Facts.Title.empty() && !fixture.record(17)->Membership.Reviewed, "completion with a different authority cannot supply title or review state");
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		fixture.Discovery.refresh();
		auto late = fixture.Provider.rows({ metadata_row(17U, true) });
		fixture.Discovery.adopt(discovery_session("reviewer", 5U));
		fixture.Provider.answer(std::move(late));
		fixture.settle();
		expect(fixture.item(17)->Facts.Title.empty() && !fixture.record(17)->Membership.Reviewed, "retained metadata from an earlier generation is refused");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_closed_metadata_cannot_be_revived_by_the_same_search_cycle()
	{
		auto fixture = discovery_fixture{ { { "github.com", "reviewer", "octo/app" } }, { { .Ref = ref(17), .Membership = { .Requested = true } } } };
		fixture.Discovery.adopt(discovery_session());
		auto closed   = metadata_row(17U);
		closed.Closed = true;
		fixture.Provider.answer(fixture.Provider.rows({ closed }));
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ row(17U) }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(!fixture.item(17) && fixture.record(17)->Departed == application::enrollment_departure::closed,
			   "a search index that still lists the request cannot revive the PR this cycle observed closed");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	[[nodiscard]] auto home_ignore(const std::int64_t aNumber, const core::home_ignore_action aAction = core::home_ignore_action::ignore)
		-> core::home_ignore_intent
	{
		return { .Ref = ref(aNumber), .Action = aAction };
	}

	// [[BPR-1683]]. Ignoring is decided by the core from the retained
	// record, durable before it is kept, and moves only the row's section.
	void test_ignore_is_durable_and_moves_only_the_section()
	{
		auto fixture = discovery_fixture{ { }, { { .Ref = ref(17), .Membership = { .Requested = true } }, { .Ref = ref(18), .Membership = { .Requested = true } } } };
		fixture.Queue.apply(home_ignore(17));
		fixture.settle();
		expect(fixture.Store.State.Ignored == std::vector{ ref(17) }, "the ignore is written to the store");
		expect(fixture.item(17)->Ignored && core::home_section_for(*fixture.item(17)) == core::home_queue_section::ignored,
			   "the ignored row is in Ignored");
		expect(fixture.item(17)->Bucket == core::attention_bucket::initial_review_requested, "its attention is unchanged");
		expect(!fixture.item(18)->Ignored, "the other row is not ignored");
		const auto queue = fixture.Queue.queue();
		expect(queue.size() == 2U && queue.front().Ref == ref(18) && queue.back().Ref == ref(17), "the ignored row sorts last");
		expect(!fixture.First->Published.empty() && fixture.First->Published.back().Items.back().Ignored,
			   "the window is told, with the ignored row last");

		const auto writes = fixture.Store.IgnoreWrites.size();
		fixture.Queue.apply(home_ignore(17));
		fixture.Queue.apply(home_ignore(99));
		fixture.settle();
		expect(fixture.Store.IgnoreWrites.size() == writes, "ignoring it again, or a row Home does not have, writes nothing");

		fixture.Queue.apply(home_ignore(17, core::home_ignore_action::unignore));
		fixture.settle();
		expect(fixture.Store.State.Ignored.empty() && !fixture.item(17)->Ignored, "unignoring is written and the row returns");
		expect(fixture.Queue.statistics().IgnoreIntents == 4U, "every gesture is counted");

		fixture.Store.FailIgnoreStore = true;
		fixture.Queue.apply(home_ignore(18));
		fixture.settle();
		expect(fixture.Queue.ignored().size() == 0U && !fixture.item(18)->Ignored, "a refused write is not kept");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_ignores_survive_restart_and_closed_ones_are_pruned()
	{
		auto closedRecord           = application::enrollment_record{ .Ref = ref(19), .Membership = { .Requested = true } };
		closedRecord.Departed       = application::enrollment_departure::closed;
		auto fixture                = discovery_fixture{ { { "github.com", "reviewer", "octo/app" } }, { { .Ref = ref(17), .Membership = { .Requested = true } }, closedRecord }, false };
		auto foreign                = ref(17);
		foreign.Account             = "someone-else";
		fixture.Store.State.Ignored = { ref(17), ref(19), foreign };
		fixture.Queue.restore();
		fixture.settle();
		expect(fixture.item(17) && fixture.item(17)->Ignored, "a restored ignore marks its row");
		expect(!fixture.Queue.ignored().contains(ref(19)) && fixture.Queue.ignored().contains(foreign),
			   "a restored ignore of a closed record is pruned; one with no record is kept");
		expect(!fixture.Store.IgnoreWrites.empty()
				   && fixture.Store.IgnoreWrites.back() == std::vector<core::home_ignore_delta>{ { .Ref = ref(19), .Change = core::home_ignore_change::pruned_closed } },
			   "the prune is written as a closed prune");

		fixture.Discovery.adopt(discovery_session());
		auto closed   = metadata_row(17U);
		closed.Closed = true;
		fixture.Provider.answer(fixture.Provider.rows({ closed }));
		fixture.settle();
		expect(fixture.record(17)->Departed == application::enrollment_departure::closed && !fixture.item(17),
			   "the closed pull request leaves Home");
		expect(!fixture.Queue.ignored().contains(ref(17)) && std::ranges::find(fixture.Store.State.Ignored, ref(17)) == fixture.Store.State.Ignored.end(),
			   "and its ignore is pruned from the store");
		expect(fixture.Queue.statistics().IgnoresPruned == 2U, "both prunes are counted");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_one_scheduler_discovers_the_saved_repositories_of_its_account()
	{
		auto fixture = discovery_fixture{ };
		fixture.Discovery.adopt(discovery_session());
		const auto& provider = fixture.Provider;
		expect(provider.Requests.size() == application::maximum_personal_discovery_in_flight && fixture.Discovery.in_flight() == 2U,
			   "adoption starts one bounded refresh: two reads in flight");
		expect(fixture.Discovery.status().State == application::queue_discovery_state::refreshing && fixture.Discovery.status().Repositories == 2U
				   && fixture.First->Published.back().Discovery == fixture.Discovery.status() && fixture.Second->Published.back().Discovery == fixture.Discovery.status(),
			   "both windows are told discovery is running over the account's two saved repositories");
		expect(provider.Requests[0].Context.Repository == core::repository_locator{ "octo", "app" }
				   && provider.Requests[0].Reason == application::personal_discovery_reason::requested
				   && provider.Requests[1].Reason == application::personal_discovery_reason::authored
				   && provider.Requests[0].Context.Request != provider.Requests[1].Context.Request
				   && provider.Requests[0].Viewer.Login == "reviewer",
			   "each read names one saved repository, one reason and the viewer");
		fixture.Provider.answer(fixture.Provider.rows({ row(5U) }));
		fixture.Provider.answer(fixture.Provider.rows({ row(6U, true, 2U), row(8U, false) }));
		fixture.settle();
		expect(provider.Requests.size() == 4U, "answers release the remaining reads, still two at a time");
		expect(std::ranges::all_of(provider.Requests, [](const auto& aRequest) { return aRequest.Context.Repository.Owner == "octo" && (aRequest.Context.Repository.Name == "app" || aRequest.Context.Repository.Name == "tools"); }),
			   "another account's and another host's saved repositories are never searched");
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		const auto requested = fixture.item(5);
		const auto authored  = fixture.item(6);
		const auto unknown   = fixture.item(8);
		expect(requested && requested->Bucket == core::attention_bucket::initial_review_requested && requested->Membership.Requested
				   && requested->Facts.Title == "Pull request 5",
			   "a direct request is discovered without opening it, and needs me");
		expect(authored && authored->Bucket == core::attention_bucket::authored_awaiting_reviewer_action && authored->Membership.Authored,
			   "my pull request awaiting two reviewers waits on others and adds nothing to needs me");
		expect(unknown && unknown->Bucket == core::attention_bucket::detail_unknown, "an authored pull request whose detail was not read is unknown, not quiet");
		const auto& status = fixture.Discovery.status();
		expect(status.State == application::queue_discovery_state::current && status.LastSuccess == std::optional<std::int64_t>{ 1'000 }
				   && fixture.First->Published.back().Discovery == status && fixture.Second->Published.back().Discovery == status,
			   "a complete refresh is current, with its time, in both windows");
		expect(fixture.Store.State.Enrollments.size() == 3U, "every discovered pull request is durable");
		const auto before           = provider.Requests.size();
		fixture.Refresh.Observation = { .Ref = ref(9), .Role = core::github_role::reviewer };
		fixture.Queue.refresh(ref(9));
		fixture.settle();
		fixture.Clock.Now += 60;
		fixture.Discovery.activate();
		expect(provider.Requests.size() == before, "opening a pull request and a fresh activation start no discovery");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	/// [[BPR-1680]]. Each refresh and each of its reads is in the diagnostics
	/// log: a refresh's start and end, a read's start (verbose) and its end
	/// with the requests, lane wait and run time its provider reports, and a
	/// refresh's totals. A refresh a session change stops says it was cancelled.
	void test_discovery_records_each_refresh_and_read()
	{
		using scope  = core::discovery_diagnosis_scope;
		using reason = core::discovery_diagnosis_reason;
		using state  = core::discovery_diagnosis_state;
		auto fixture = discovery_fixture{ };
		fixture.Discovery.adopt(discovery_session());
		auto&      provider = fixture.Provider;
		const auto answer   = [&](application::personal_discovery_completion aCompletion, const application::personal_discovery_cost aCost) {
			std::visit([&aCost](auto& aResult) { aResult.Cost = aCost; }, aCompletion);
			provider.answer(std::move(aCompletion));
			fixture.settle();
		};
		fixture.Clock.Now += 2;
		answer(provider.rows({ row(5U) }), { .Requests = 4U, .WaitMilliseconds = 3U, .RunMilliseconds = 900U });
		answer(provider.failure(application::github_provider_failure::permission), { .Requests = 1U, .WaitMilliseconds = 7U, .RunMilliseconds = 400U });
		answer(provider.rows({ }), { .Requests = 1U, .RunMilliseconds = 300U });
		answer(provider.rows({ }), { .Requests = 1U, .RunMilliseconds = 200U });
		const auto& events = fixture.Diagnostics.Events;
		const auto  lines  = fixture.Diagnostics.lines();
		expect(lines.size() == 10U, "one refresh of four reads is ten lines");
		expect(lines[0] == core::discovery_diagnosis{ .Scope = scope::refresh, .Refresh = 1U, .Repositories = 2U } && events[0].Outcome == application::diagnostic_outcome::started
				   && events[0].Detail == application::diagnostic_detail::standard,
			   "the refresh's start counts its saved repositories");
		expect(lines[1] == core::discovery_diagnosis{ .Scope = scope::read, .Refresh = 1U, .Read = 1U, .Reason = reason::requested, .Repositories = 1U }
				   && lines[2] == core::discovery_diagnosis{ .Scope = scope::read, .Refresh = 1U, .Read = 2U, .Reason = reason::authored, .Repositories = 1U }
				   && events[1].Outcome == application::diagnostic_outcome::started && events[1].Detail == application::diagnostic_detail::verbose,
			   "each submitted read starts, numbered within its refresh, at verbose detail");
		expect(lines[3] == core::discovery_diagnosis{ .Scope = scope::read, .Refresh = 1U, .Read = 1U, .Reason = reason::requested, .Repositories = 1U, .Requests = 4U, .WaitMilliseconds = 3U, .RunMilliseconds = 900U }
				   && events[3].Outcome == application::diagnostic_outcome::succeeded && events[3].Detail == application::diagnostic_detail::standard,
			   "an answered read ends with its requests, lane wait and run time");
		expect(lines[4].Read == 3U && lines[5].Read == 2U && events[5].Outcome == application::diagnostic_outcome::failed
				   && events[5].GithubFailure == application::diagnostic_github_failure{ core::github_failure::permission },
			   "a failed read ends failed with its failure");
		expect(lines[6].Read == 4U && lines[7].Read == 3U && lines[8].Read == 4U, "the later reads start as earlier ones end");
		expect(lines[9] == core::discovery_diagnosis{ .Scope = scope::refresh, .Refresh = 1U, .State = state::stale, .Repositories = 2U, .Reads = 4U, .Failed = 1U, .Requests = 7U, .WaitMilliseconds = 10U, .RunMilliseconds = 2'000U }
				   && events[9].Outcome == application::diagnostic_outcome::succeeded,
			   "the refresh's end says how it ended, its reads, failures, requests, lane wait and duration");
		expect(fixture.Discovery.statistics().Requests == 7U, "the statistics count the requests the reads reported");

		fixture.Discovery.refresh();
		expect(fixture.Discovery.status().State == application::queue_discovery_state::refreshing, "a second refresh runs");
		const auto second = fixture.Diagnostics.lines()[10];
		expect(second.Scope == scope::refresh && second.Refresh == 2U && second.PullRequests == 1U && second.State == state::none,
			   "the next refresh is numbered after it and counts its retained pull request");
		fixture.Discovery.adopt(discovery_session("someone-else"));
		expect(std::ranges::any_of(fixture.Diagnostics.lines(), [](const core::discovery_diagnosis& aLine) {
				   return aLine.Scope == scope::refresh && aLine.Refresh == 2U && aLine.State == state::cancelled;
			   }),
			   "a refresh a session change stops ends cancelled");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	/// [[BPR-1680]] option 2. A pull request read in full by the refresh's
	/// retained read is known to that refresh's searches: a row the provider
	/// returns for it, not read again, takes the retained read's detail; a
	/// requested row stays only when that read named the viewer. One the
	/// retained read found closed, or read only in part, is read again.
	void test_discovery_searches_reuse_the_refresh_retained_reads()
	{
		auto fixture = discovery_fixture{ { { "github.com", "reviewer", "octo/app" } },
										  {
											  { .Ref = ref(5), .Membership = { .Requested = true } },
											  { .Ref = ref(6), .Membership = { .Authored = true } },
											  { .Ref = ref(7), .Membership = { .Requested = true } },
											  { .Ref = ref(9), .Membership = { .Requested = true } },
										  } };
		fixture.Discovery.adopt(discovery_session());
		auto& provider = fixture.Provider;
		while (provider.Answered < provider.Requests.size() && provider.next().Reason == application::personal_discovery_reason::retained) {
			const auto number     = provider.next().Number->value();
			auto       read       = metadata_row(number);
			read.ReviewersAwaited = number == 6U ? 2U : 1U;
			read.Waiting.Requests = read.ReviewersAwaited;
			read.Change.Comments  = 40U + number;
			read.RequestsViewer   = number == 5U;
			read.WaitingLimited   = number == 9U;
			provider.answer(provider.rows({ read }));
			fixture.settle();
		}
		expect(provider.Requests.size() >= 5U && provider.next().Reason == application::personal_discovery_reason::requested, "the retained reads come first, then the searches");
		const auto known = std::vector<application::personal_discovery_known>{
			{ .Repository = { "octo", "app" }, .Number = core::pull_request_number{ 5U } },
			{ .Repository = { "octo", "app" }, .Number = core::pull_request_number{ 6U } },
			{ .Repository = { "octo", "app" }, .Number = core::pull_request_number{ 7U } },
		};
		expect(provider.next().Known == known && provider.Requests[provider.Answered + 1U].Known == known,
			   "both searches know the three pull requests read in full, not the one whose summary hit a bound");
		const auto reused = [](const std::uint32_t aNumber) {
			auto result   = row(aNumber, false);
			result.Reused = true;
			return result;
		};
		provider.answer(provider.rows({ reused(5U), reused(7U), row(8U) }));
		provider.answer(provider.rows({ reused(6U) }));
		fixture.settle();
		expect(fixture.Discovery.statistics().Reused == 2U, "two found rows are answered from the retained reads");
		expect(fixture.record(5) && fixture.record(5)->Membership.Requested && fixture.item(5)->Facts.Waiting.Requests == std::optional<std::uint32_t>{ 1U },
			   "a requested pull request that names the viewer stays requested, with the retained read's detail");
		expect(!fixture.record(7) || !fixture.record(7)->Membership.Requested, "one asked only of a team is not the viewer's, exactly as its own read would decide");
		expect(fixture.item(6) && fixture.item(6)->Membership.Authored && fixture.item(6)->Facts.Waiting.Requests == std::optional<std::uint32_t>{ 2U }
				   && fixture.item(6)->Facts.Change.Comments == std::optional<std::uint64_t>{ 46U },
			   "an authored row takes the retained read's awaited reviewers and change facts");
		expect(fixture.item(8) && fixture.record(8)->Membership.Requested, "a pull request only the search found is read as before");
		expect(fixture.Discovery.status().State == application::queue_discovery_state::partial, "the refresh ends, partial for the bounded summary");

		// A reused row the request did not know is not trusted.
		fixture.Discovery.refresh();
		while (provider.Answered < provider.Requests.size() && provider.next().Reason == application::personal_discovery_reason::retained) {
			provider.answer(provider.rows({ metadata_row(provider.next().Number->value()) }));
			fixture.settle();
		}
		provider.answer(provider.rows({ reused(11U) }));
		provider.answer(provider.rows({ }));
		fixture.settle();
		expect(!fixture.item(11), "a row the provider calls reused for an unknown pull request is dropped");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	/// [[BPR-1680]] option 3. With the production options one search per
	/// reason names every saved repository of the account; each row is
	/// applied to the repository it belongs to, each repository's answer is as
	/// complete as the search. A search GitHub refuses, or whose page or
	/// detail bound cannot say whose rows it left out, is asked again one
	/// repository at a time, and a repository refused alone is searched alone
	/// for the rest of the session.
	void test_discovery_searches_several_repositories_at_once()
	{
		auto fixture = discovery_fixture{ {
											  { .Host = "github.com", .Account = "reviewer", .Repository = "octo/tools" },
											  { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" },
											  { .Host = "github.com", .Account = "reviewer", .Repository = "octo/web" },
										  },
										  { },
										  true,
										  { } };
		fixture.Discovery.adopt(discovery_session());
		auto&      provider = fixture.Provider;
		const auto app      = core::repository_locator{ "octo", "app" };
		const auto tools    = core::repository_locator{ "octo", "tools" };
		const auto web      = core::repository_locator{ "octo", "web" };
		expect(provider.Requests.size() == 2U && provider.Requests[0].Reason == application::personal_discovery_reason::requested
				   && provider.Requests[1].Reason == application::personal_discovery_reason::authored
				   && provider.Requests[0].Repositories == std::vector{ app, tools, web } && provider.Requests[0].Context.Repository == app
				   && provider.Requests[1].Repositories == provider.Requests[0].Repositories,
			   "two searches, one per reason, each naming the three saved repositories in order");
		const auto in = [](application::personal_discovery_row aRow, const core::repository_locator& aRepository) {
			aRow.Repository = aRepository;
			return aRow;
		};
		provider.answer(provider.rows({ in(row(5U), tools), in(row(6U), web) }));
		provider.answer(provider.rows({ in(row(7U, true, 1U), app) }));
		fixture.settle();
		expect(provider.Requests.size() == 2U && fixture.Discovery.status().State == application::queue_discovery_state::current,
			   "the two searches are the whole refresh");
		expect(fixture.item(5) && fixture.item(5)->Ref.Repository == "octo/tools" && fixture.item(5)->Membership.Requested
				   && fixture.item(6) && fixture.item(6)->Ref.Repository == "octo/web" && fixture.item(7) && fixture.item(7)->Ref.Repository == "octo/app"
				   && fixture.item(7)->Membership.Authored,
			   "each row is applied to the repository it belongs to");
		expect(fixture.Discovery.status().Repositories == 3U && fixture.Discovery.statistics().Split == 0U, "Home says three repositories were searched");

		// The next refresh's requested search finds nothing in octo/tools: a
		// complete answer withdraws #5 there, as one per repository would.
		fixture.Discovery.refresh();
		while (provider.Answered < provider.Requests.size() && provider.next().Reason == application::personal_discovery_reason::retained) {
			provider.answer(provider.rows({ metadata_row(provider.next().Number->value()) }));
			fixture.settle();
		}
		provider.answer(provider.rows({ in(row(6U), web) }));
		provider.answer(provider.rows({ }, application::personal_discovery_completeness::result_limit));
		fixture.settle();
		expect(!fixture.record(5) || !fixture.record(5)->Membership.Requested, "a complete shared answer withdraws what it no longer finds");
		expect(fixture.Discovery.statistics().Split == 1U && provider.Requests.size() == provider.Answered + 2U
				   && provider.next().Repositories.empty() && provider.next().Context.Repository == app
				   && provider.Requests[provider.Answered + 1U].Context.Repository == tools,
			   "a shared search past its page is asked again one repository at a time, ahead of the rest");
		provider.answer(provider.rows({ row(7U, true, 1U) }));
		fixture.settle();
		provider.answer(provider.rows({ }));
		fixture.settle();
		provider.answer(provider.rows({ }));
		fixture.settle();
		expect(fixture.record(7) && fixture.record(7)->Membership.Authored && fixture.Discovery.status().State == application::queue_discovery_state::current,
			   "each repository gets its own answer and the refresh ends current");

		// GitHub refuses a shared search when one repository cannot be read.
		fixture.Discovery.refresh();
		while (provider.Answered < provider.Requests.size() && provider.next().Reason == application::personal_discovery_reason::retained) {
			provider.answer(provider.rows({ metadata_row(provider.next().Number->value()) }));
			fixture.settle();
		}
		provider.answer(provider.failure(application::github_provider_failure::permission));
		fixture.settle();
		expect(fixture.Discovery.statistics().Split == 2U && provider.Requests.back().Context.Repository == app && provider.Requests.back().Repositories.empty(),
			   "a refused shared search is asked again one repository at a time");
		provider.answer(provider.rows({ }));
		fixture.settle();
		provider.answer(provider.rows({ }));
		fixture.settle();
		expect(provider.next().Context.Repository == tools, "octo/tools is asked on its own");
		provider.answer(provider.failure(application::github_provider_failure::permission));
		fixture.settle();
		while (provider.Answered < provider.Requests.size()) {
			provider.answer(provider.rows({ }));
			fixture.settle();
		}
		expect(fixture.Discovery.status().State == application::queue_discovery_state::stale
				   && fixture.Discovery.status().Problem == application::queue_discovery_problem::repository_unavailable,
			   "only the repository refused alone is unreadable");
		fixture.Discovery.refresh();
		while (provider.Answered < provider.Requests.size() && provider.next().Reason == application::personal_discovery_reason::retained) {
			provider.answer(provider.rows({ metadata_row(provider.next().Number->value()) }));
			fixture.settle();
		}
		expect(provider.next().Repositories == std::vector{ app, web } && provider.Requests[provider.Answered + 1U].Repositories == std::vector{ app, web },
			   "the others share one search per reason");
		provider.answer(provider.rows({ }));
		provider.answer(provider.rows({ }));
		fixture.settle();
		expect(provider.next().Repositories.empty() && provider.next().Context.Repository == tools
				   && provider.Requests[provider.Answered + 1U].Repositories.empty() && provider.Requests[provider.Answered + 1U].Context.Repository == tools,
			   "the repository refused alone is searched alone, for both reasons");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	/// [[BPR-1680]] option 4. With the production options the retained pull
	/// requests of one repository are one read, as the core batches them;
	/// each row it answers is applied as its own read's would be, a pull
	/// request it leaves out fails alone, and a row for one it did not ask
	/// for fails the read's answer.
	void test_discovery_batches_retained_reads()
	{
		auto fixture = discovery_fixture{ { { "github.com", "reviewer", "octo/app" } },
										  {
											  { .Ref = ref(9), .Membership = { .Followed = true } },
											  { .Ref = ref(5), .Membership = { .Followed = true } },
											  { .Ref = ref(7), .Membership = { .Followed = true } },
										  },
										  true,
										  { } };
		fixture.Discovery.adopt(discovery_session());
		auto& provider = fixture.Provider;
		expect(provider.Requests.size() == 1U && provider.next().Reason == application::personal_discovery_reason::retained
				   && provider.next().Numbers == std::vector{ core::pull_request_number{ 5U }, core::pull_request_number{ 7U }, core::pull_request_number{ 9U } }
				   && provider.next().Number == core::pull_request_number{ 5U },
			   "the three retained pull requests of one repository are one read, ascending");
		const auto diagnostics = fixture.Diagnostics.lines();
		expect(diagnostics.size() == 2U && diagnostics[1].Scope == core::discovery_diagnosis_scope::read && diagnostics[1].PullRequests == 3U,
			   "the read is logged as one read of three pull requests");
		provider.answer(provider.rows({ metadata_row(5U), metadata_row(9U) }));
		fixture.settle();
		expect(fixture.item(5) && fixture.item(5)->Facts.Title == "Pull request 5" && fixture.item(9) && fixture.item(9)->Facts.Title == "Pull request 9" && fixture.item(7) && fixture.item(7)->Facts.Title.empty(),
			   "each answered row is applied; the one left out keeps what it had");
		while (provider.Answered < provider.Requests.size()) {
			provider.answer(provider.rows({ }));
			fixture.settle();
		}
		expect(fixture.Discovery.status().State == application::queue_discovery_state::stale, "the pull request left out fails the refresh alone");

		fixture.Discovery.refresh();
		provider.answer(provider.rows({ metadata_row(5U), metadata_row(6U) }));
		fixture.settle();
		while (provider.Answered < provider.Requests.size()) {
			provider.answer(provider.rows({ }));
			fixture.settle();
		}
		expect(!fixture.item(6) && fixture.Discovery.status().State == application::queue_discovery_state::stale,
			   "a row the read did not ask for fails its answer and enrolls nothing");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_discovery_membership_is_honest_about_absence()
	{
		auto fixture = discovery_fixture{ { { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" } } };
		fixture.Discovery.adopt(discovery_session());
		fixture.Provider.answer(fixture.Provider.rows({ row(5U), row(7U) }));
		fixture.Provider.answer(fixture.Provider.rows({ row(6U, true, 1U), row(9U, true, 1U) }));
		fixture.settle();
		fixture.Queue.apply(application::queue_follow_intent{ .Ref = { "github.com", "reviewer", "octo/app", 9 }, .Follow = true });
		fixture.settle();
		expect(fixture.record(9) && fixture.record(9)->Membership.Followed && fixture.record(9)->Membership.Authored, "a follow is kept beside the discovery reason");
		// A partial answer finds less, and removes nothing.
		fixture.Clock.Now += 400;
		fixture.Discovery.refresh();
		answer_retained(fixture);
		fixture.Provider.answer(fixture.Provider.rows({ }, application::personal_discovery_completeness::incomplete_results));
		fixture.Provider.answer(fixture.Provider.rows({ }, application::personal_discovery_completeness::result_limit));
		fixture.settle();
		expect(fixture.item(5) && fixture.item(6) && fixture.item(7) && fixture.item(9), "an incomplete or limited answer establishes no removal");
		expect(fixture.Discovery.status().State == application::queue_discovery_state::partial
				   && fixture.Discovery.status().Problem == application::queue_discovery_problem::incomplete_results,
			   "and says the list is partial, with why");
		// A complete answer that no longer finds them withdraws their reason.
		fixture.Clock.Now += 400;
		fixture.Discovery.refresh();
		answer_retained(fixture);
		fixture.Provider.answer(fixture.Provider.rows({ row(7U) }));
		fixture.Provider.answer(fixture.Provider.rows({ row(6U, true, 1U) }));
		fixture.settle();
		expect(!fixture.item(5) && fixture.record(5) && fixture.record(5)->Departed == application::enrollment_departure::undiscovered,
			   "a request no complete answer finds any more departs, with a reason that claims nothing it did not read");
		expect(fixture.item(9) && fixture.record(9)->Membership.Followed && !fixture.record(9)->Membership.Authored,
			   "a followed pull request stays when discovery no longer finds it");
		expect(fixture.item(6) && fixture.item(7), "what is still found stays");
		fixture.Queue.apply(application::queue_follow_intent{ .Ref = { "github.com", "reviewer", "octo/app", 9 }, .Follow = false });
		fixture.settle();
		expect(!fixture.item(9) && fixture.record(9)->Departed == application::enrollment_departure::removed_by_reviewer, "unfollowing what nothing else holds removes it");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_discovery_failures_keep_rows_and_back_off()
	{
		auto fixture = discovery_fixture{ };
		fixture.Discovery.adopt(discovery_session());
		fixture.Provider.answer(fixture.Provider.rows({ row(5U) }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.item(5).has_value(), "a first refresh leaves a cached row");
		fixture.Clock.Now += 400;
		fixture.Discovery.refresh();
		answer_retained(fixture);
		const auto cycleStart = fixture.Provider.Requests.size() - 2U;
		fixture.Provider.answer(fixture.Provider.failure(application::github_provider_failure::rate_limit, 120U));
		fixture.settle();
		const auto& limited = fixture.Discovery.status();
		expect(limited.State == application::queue_discovery_state::stale && limited.Problem == application::queue_discovery_problem::rate_limited
				   && limited.RetryAfterSeconds == std::optional<std::uint32_t>{ 120U } && limited.LastSuccess == std::optional<std::int64_t>{ 1'000 },
			   "a rate limit leaves the list stale with its last success and how long to wait");
		expect(fixture.Provider.Requests.size() == cycleStart + 2U && fixture.Provider.Cancelled.size() == 1U, "the rest of that refresh is cancelled, not sent");
		fixture.Provider.answer(application::personal_discovery_cancelled{ .Request = fixture.Provider.next() });
		fixture.settle();
		expect(fixture.item(5).has_value(), "the cached row survives the failure");
		fixture.Discovery.refresh();
		answer_retained(fixture);
		fixture.Discovery.activate();
		expect(fixture.Provider.Requests.size() == cycleStart + 2U && fixture.Discovery.statistics().RefusedRefreshes == 1U
				   && fixture.Discovery.status().RetryAfterSeconds == std::optional<std::uint32_t>{ 120U },
			   "a refresh inside the backoff is refused and says so; activation waits too");
		const auto beforeRetry = fixture.Provider.Requests.size();
		fixture.Clock.Now += 121;
		fixture.Discovery.refresh();
		answer_retained(fixture);
		expect(fixture.Provider.Requests.size() == beforeRetry + 3U, "after the retry-after a refresh runs again");
		fixture.Provider.answer(fixture.Provider.failure(application::github_provider_failure::rate_limit, std::nullopt, true));
		fixture.settle();
		expect(fixture.Discovery.status().Problem == application::queue_discovery_problem::secondary_rate_limited
				   && fixture.Discovery.status().RetryAfterSeconds == std::optional<std::uint32_t>{ application::personal_discovery_default_retry_seconds },
			   "a secondary limit is its own problem, with the default wait");
		fixture.Provider.answer(application::personal_discovery_cancelled{ .Request = fixture.Provider.next() });
		fixture.settle();
		for (const auto& [failure, problem] : std::array{
				 std::pair{ application::github_provider_failure::authentication, application::queue_discovery_problem::authentication },
				 std::pair{ application::github_provider_failure::network, application::queue_discovery_problem::offline },
			 }) {
			fixture.Clock.Now += 1'000;
			fixture.Discovery.refresh();
			answer_retained(fixture);
			fixture.Provider.answer(fixture.Provider.failure(failure));
			fixture.settle();
			expect(fixture.Discovery.status().State == application::queue_discovery_state::stale && fixture.Discovery.status().Problem == problem && fixture.item(5),
				   "an authentication or network failure is stale, named, and keeps the cached rows");
			fixture.Provider.answer(application::personal_discovery_cancelled{ .Request = fixture.Provider.next() });
			fixture.settle();
		}
		fixture.Clock.Now += 1'000;
		fixture.Discovery.refresh();
		answer_retained(fixture);
		const auto permission = fixture.Provider.Requests.size();
		fixture.Provider.answer(fixture.Provider.failure(application::github_provider_failure::permission));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.Provider.Requests.size() == permission + 2U, "a repository that cannot be read does not stop the others");
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.Discovery.status().State == application::queue_discovery_state::stale
				   && fixture.Discovery.status().Problem == application::queue_discovery_problem::repository_unavailable && fixture.item(5),
			   "and is named, with nothing removed");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_discovery_refuses_stale_and_foreign_answers()
	{
		auto fixture = discovery_fixture{ {
			{ .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" },
			{ .Host = "github.com", .Account = "someone-else", .Repository = "octo/other" },
		} };
		fixture.Discovery.adopt(discovery_session());
		auto early = fixture.Provider.rows({ row(5U) });
		fixture.Discovery.adopt(discovery_session("someone-else", 5U));
		expect(fixture.Provider.Cancelled.size() == 2U, "an account switch cancels what was in flight");
		fixture.Provider.answer(std::move(early));
		fixture.settle();
		expect(!fixture.item(5) && fixture.Discovery.statistics().StaleCompletions == 1U, "an answer read as the earlier account is refused");
		expect(fixture.Discovery.status().Account == "someone-else" && fixture.Discovery.status().State == application::queue_discovery_state::refreshing,
			   "the new account's refresh runs over its own saved repository");
		fixture.Queue.apply(application::queue_discovery_answer{ .Session = { .Host = "github.com", .Account = "reviewer", .Generation = 4U }, .Repository = "octo/app", .Complete = true, .PullRequests = { { .Number = 6 } } });
		fixture.settle();
		expect(!fixture.item(6) && fixture.Queue.statistics().StaleDiscoveryAnswers == 1U, "the queue refuses an answer about another session's queue");
		fixture.Discovery.adopt(std::nullopt);
		expect(fixture.Discovery.status().State == application::queue_discovery_state::unavailable
				   && fixture.Discovery.status().Problem == application::queue_discovery_problem::no_account,
			   "with no account session discovery is unavailable, and says why");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	// The same account signs in again: a new identity generation over the same
	// saved repository. An answer read under the earlier one is refused, and
	// the new generation's reads go out once the cancelled ones return.
	void test_discovery_refuses_an_earlier_generation_of_the_same_account()
	{
		auto fixture = discovery_fixture{ { { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" } } };
		fixture.Discovery.adopt(discovery_session("reviewer", 6U));
		expect(fixture.Provider.Requests.size() == 2U, "two reads under generation six");
		fixture.Discovery.adopt(discovery_session("reviewer", 7U));
		expect(fixture.Provider.Cancelled.size() == 2U && fixture.Provider.Requests.size() == 2U, "they are cancelled, and hold their slots until they return");
		fixture.Provider.answer(fixture.Provider.rows({ row(11U) }));
		fixture.settle();
		expect(!fixture.item(11) && fixture.Discovery.statistics().StaleCompletions == 1U,
			   "an answer read under an earlier generation of the same account is refused");
		expect(fixture.Provider.Requests.size() == 3U && fixture.Provider.Requests.back().Context.IdentityGeneration == core::github_identity_generation{ 7U },
			   "the freed slot carries a read of the new generation");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	// [[BPR-1679]]. A pull request asked for while Home is still loading goes
	// first. The owner: "Right now it seems like it queues behind the home load
	// before it goes to open the PR". Discovery gives way at once: what it has
	// in flight is cancelled and nothing new is asked. A read its cancellation
	// answers is asked again, under a new identity, once it resumes; one that
	// answered first is kept. The refresh ends current, not failed.
	void test_discovery_gives_way_to_a_window_read_and_resumes()
	{
		auto  fixture  = discovery_fixture{ };
		auto& provider = fixture.Provider;
		fixture.Discovery.adopt(discovery_session());
		expect(provider.Requests.size() == 2U && !fixture.Discovery.yielding(), "Home's refresh has two searches in flight");
		const auto preempted = provider.Requests[0];
		const auto raced     = provider.Requests[1];

		fixture.Discovery.yield(true);
		expect(fixture.Discovery.yielding() && provider.Cancelled == std::vector{ preempted.Context.Request, raced.Context.Request },
			   "giving way cancels both reads in flight, in the same turn");
		fixture.Discovery.yield(true);
		expect(provider.Cancelled.size() == 2U && fixture.Discovery.statistics().Preempted == 2U, "giving way again cancels nothing more");
		expect(fixture.Discovery.status().State == application::queue_discovery_state::refreshing, "Home still says it is loading");

		provider.answer(application::personal_discovery_cancelled{ .Request = provider.next() });
		fixture.settle();
		expect(provider.Requests.size() == 2U, "a read stopped to give way is not asked again while the window reads");
		provider.answer(provider.rows({ row(6U, true, 1U) }));
		fixture.settle();
		expect(provider.Requests.size() == 2U && fixture.item(6).has_value(), "a read that answered before its cancellation is kept, and nothing new is asked");
		expect(fixture.Discovery.statistics().Failed == 0U && fixture.Discovery.statistics().CancelledRequests == 0U, "nothing was failed or given up");

		fixture.Discovery.yield(false);
		expect(!fixture.Discovery.yielding() && provider.Requests.size() == 4U, "once the window's read ends discovery resumes, two at a time");
		const auto& again = provider.Requests[2];
		expect(again.Context.Repository == preempted.Context.Repository && again.Reason == preempted.Reason
				   && again.Context.Request != preempted.Context.Request && again.Context.Request != raced.Context.Request,
			   "the stopped read goes first, under a new identity");
		expect(fixture.Discovery.statistics().Resubmitted == 1U, "only the read its cancellation answered is asked again");
		provider.answer(provider.rows({ row(5U) }));
		provider.answer(provider.rows({ }));
		fixture.settle();
		expect(provider.Requests.size() == 5U, "the last search follows");
		provider.answer(provider.rows({ }));
		fixture.settle();
		expect(fixture.item(5).has_value() && fixture.item(6).has_value(), "both found pull requests are listed");
		expect(fixture.Discovery.status().State == application::queue_discovery_state::current, "the refresh that gave way ends current");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	// Giving way holds a refresh started meanwhile too, and a session change
	// while it gives way refuses the stopped read's late answer.
	void test_discovery_gives_way_across_a_session_change()
	{
		auto  fixture  = discovery_fixture{ { { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" } } };
		auto& provider = fixture.Provider;
		fixture.Discovery.yield(true);
		fixture.Discovery.adopt(discovery_session("reviewer", 6U));
		expect(provider.Requests.empty() && fixture.Discovery.status().State == application::queue_discovery_state::refreshing,
			   "a refresh that starts while a window reads waits, and says it is loading");
		fixture.Discovery.yield(false);
		expect(provider.Requests.size() == 2U, "it starts once the read ends");
		fixture.Discovery.yield(true);
		fixture.Discovery.adopt(discovery_session("reviewer", 7U));
		provider.answer(application::personal_discovery_cancelled{ .Request = provider.next() });
		provider.answer(application::personal_discovery_cancelled{ .Request = provider.next() });
		fixture.settle();
		expect(fixture.Discovery.statistics().Resubmitted == 0U && fixture.Discovery.statistics().StaleCompletions == 2U,
			   "a stopped read of an earlier generation is refused, not asked again");
		fixture.Discovery.yield(false);
		expect(provider.Requests.size() == 4U && provider.Requests[2].Context.IdentityGeneration == core::github_identity_generation{ 7U },
			   "the new generation's reads go out once discovery resumes");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	// A discovery answer the profile's store refuses is not shown as if it were
	// kept: the update is rolled back and the queue reports its store
	// unavailable, while the rows already stored stay.
	void test_a_refused_discovery_write_is_reported_not_kept()
	{
		auto fixture = discovery_fixture{ { { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" } } };
		fixture.Discovery.adopt(discovery_session());
		fixture.Provider.answer(fixture.Provider.rows({ row(5U) }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.item(5) && fixture.First->Published.back().Status == application::queue_landing_status::ready, "a stored row");
		fixture.Store.FailEnrollmentStore = true;
		fixture.Clock.Now += 400;
		fixture.Discovery.refresh();
		answer_retained(fixture);
		fixture.Provider.answer(fixture.Provider.rows({ row(5U), row(6U) }));
		fixture.settle();
		const auto& published = fixture.First->Published.back();
		expect(!fixture.item(6) && fixture.item(5) && published.Status == application::queue_landing_status::unavailable
				   && published.Failure == std::optional{ application::queue_store_failure::unavailable } && fixture.Second->Published.back().Status == published.Status,
			   "the refused update is rolled back, the kept row stays, and every window is told the store refused it");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	// [[BPR-0990]]. The session composition hands discovery is the accepted
	// session's own authority, named as the queue names a pull request opened
	// under it; a signed-out session has no discovery session at all.
	void test_the_discovery_session_of_an_accepted_session()
	{
		auto session = application::github_profile_session{
			.BprProfile         = core::bpr_profile_id{ 2U },
			.ApplicationSession = core::application_session_id{ 7U },
			.ProfileAuthority   = { .Profile = core::host_profile_id{ 11U }, .Host = core::git_host_id{ 22U }, .Hostname = application::github_canonical_hostname{ "ghe.example" } },
			.Session            = { .Viewer = { .Account = core::github_account_id{ 1001U }, .Login = "reviewer" }, .Mode = core::github_access_mode::authenticated },
			.Generation         = core::github_identity_generation{ 9U },
		};
		session.CapabilityEvidence.Revision = core::capability_revision{ 44U };
		const auto discovery                = application::personal_discovery_session_of(session);
		expect(discovery && discovery->Queue == application::queue_session_id{ .Host = "ghe.example", .Account = "reviewer", .Generation = 9U, .ViewerAccount = core::github_account_id{ 1001U } }
				   && discovery->Context.ApplicationSession == session.ApplicationSession && discovery->Context.Profile == core::host_profile_id{ 11U }
				   && discovery->Context.Host == core::git_host_id{ 22U } && discovery->Context.Account == core::github_account_id{ 1001U }
				   && discovery->Context.Capabilities == core::capability_revision{ 44U } && discovery->Context.IdentityGeneration == session.Generation
				   && discovery->AuthenticationSession == std::optional{ session.ApplicationSession } && discovery->Viewer.Login == "reviewer",
			   "an accepted account session names its server, login and generation, and carries its exact authority");
		auto signedOut         = session;
		signedOut.Session.Mode = core::github_access_mode::public_preview;
		auto noLogin           = session;
		noLogin.Session.Viewer.Login.clear();
		expect(!application::personal_discovery_session_of(signedOut) && !application::personal_discovery_session_of(noLogin),
			   "a signed-out session, or one without a viewer login, has nothing to search for");
	}

	void test_observer_authorship_does_not_change_enrollment()
	{
		auto membership        = core::queue_membership{ };
		membership.MatchedRule = true;
		membership.Rule        = { .Kind = core::observer_rule_kind::repository, .Pattern = "octo/app" };
		auto       fixture     = discovery_fixture{ { }, { { .Ref = ref(17), .Membership = membership } } };
		const auto session     = discovery_session().Queue;
		fixture.Queue.adopt(session);
		auto waiting           = core::queue_waiting_facts{ };
		waiting.RequiredReview = true;
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Observer author" }, .Activity = { .Known = true, .Waiting = waiting, .FactRevision = fixture.Queue.begin_observation() }, .Authored = true });
		fixture.settle();
		expect(fixture.item(17)->Membership.authored() && fixture.item(17)->Bucket == core::attention_bucket::authored_awaiting_reviewer_action && fixture.record(17)->Membership == membership && fixture.Queue.saved_repositories().empty(), "observer-only authored waiting is a projection fact; the original enrollment reason stays unchanged");
		fixture.Queue.shutdown();
		fixture.Discovery.shutdown();
		fixture.settle();
	}

	void test_failed_metadata_storage_preserves_accepted_observations()
	{
		auto       fixture = discovery_fixture{ { } };
		const auto session = discovery_session().Queue;
		fixture.Queue.adopt(session);
		fixture.Queue.apply(application::queue_follow_intent{ .Ref = ref(17), .Follow = true });
		fixture.settle();
		fixture.Store.FailEnrollmentStore = true;
		auto waiting                      = core::queue_waiting_facts{ };
		waiting.RequiredReview            = true;
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Read despite storage failure" }, .Activity = { .Known = true, .Waiting = waiting, .FactRevision = fixture.Queue.begin_observation() }, .Authored = true });
		fixture.settle();
		const auto item = fixture.item(17);
		expect(item && item->Membership.authored() && item->Bucket == core::attention_bucket::authored_awaiting_reviewer_action, "failed optional metadata persistence cannot erase accepted authorship or aggregate review waiting");
		expect(fixture.First->Published.back().Status == application::queue_landing_status::unavailable, "storage failure remains explicit without changing server facts");
		fixture.Store.FailEnrollmentStore = false;
		fixture.Queue.shutdown();
		fixture.Discovery.shutdown();
		fixture.settle();
	}

	void test_home_waiting_observations_survive_local_activity_and_restart()
	{
		auto       fixture = discovery_fixture{ { } };
		const auto session = discovery_session().Queue;
		fixture.Queue.adopt(session);
		fixture.Queue.apply(application::queue_follow_intent{ .Ref = ref(17), .Follow = true });
		fixture.settle();
		auto waiting           = core::queue_waiting_facts{ };
		waiting.Requests       = 3;
		waiting.Teams          = 3;
		waiting.NamesKnown     = true;
		waiting.CodeOwners     = { "@org/team-a", "@org/team-b", "@org/team-c" };
		waiting.RequiredReview = true;
		waiting.MergeBlocked   = true;
		waiting.ChecksKnown    = true;
		waiting.FailedChecks   = { "Branch Freshness" };
		waiting.Head           = std::string(40, 'a');
		waiting.Base           = std::string(40, 'b');
		const auto newer       = fixture.Queue.begin_observation();
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Authored follow", .Author = "reviewer", .Waiting = waiting }, .Activity = { .Known = true, .Waiting = waiting, .FactRevision = newer }, .Authored = true });
		fixture.settle();
		expect(fixture.item(17)->Membership.authored() && fixture.item(17)->Membership.Followed && fixture.item(17)->Bucket == core::attention_bucket::authored_awaiting_reviewer_action, "Follow-only identity becomes authored and waits without saving its repository");
		fixture.Refresh.Observation = { .Ref = ref(17), .Author = "reviewer", .Role = core::github_role::none, .Title = "Authored follow" };
		fixture.Queue.refresh(ref(17), { .UnseenComments = 2, .Known = true });
		fixture.settle();
		expect(fixture.item(17)->Bucket == core::attention_bucket::new_comments_or_author_replies && fixture.item(17)->Facts.Waiting.CodeOwners.size() == 3, "local unread observation preserves roster and concurrent failure");
		fixture.Queue.refresh(ref(17), { .Known = true });
		fixture.settle();
		expect(fixture.item(17)->Bucket == core::attention_bucket::authored_awaiting_reviewer_action && fixture.item(17)->Facts.Waiting.FailedChecks == std::vector<std::string>{ "Branch Freshness" }, "marking read restores waiting and never erases a check failure");
		auto empty     = waiting;
		empty.Requests = 0;
		empty.People   = 0;
		empty.Teams    = 0;
		empty.CodeOwners.clear();
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Authored follow" }, .Activity = { .Known = true, .Waiting = empty, .FactRevision = fixture.Queue.begin_observation() } });
		fixture.settle();
		expect(fixture.item(17)->Bucket == core::attention_bucket::authored_awaiting_reviewer_action, "complete last-request removal cannot settle a separate unmet requirement");
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Authored follow" }, .Activity = { .Known = true, .Waiting = waiting, .FactRevision = newer } });
		fixture.settle();
		expect(fixture.item(17)->Facts.Waiting.Requests == std::uint32_t{ 0 }, "older completion cannot restore requests removed by a newer dispatch");
		empty.RequiredReview = false;
		fixture.Queue.apply(application::queue_metadata_answer{ .Session = session, .Ref = ref(17), .Facts = { .Title = "Authored follow" }, .Activity = { .Known = true, .Waiting = empty, .FactRevision = fixture.Queue.begin_observation() } });
		fixture.settle();
		expect(fixture.item(17)->Bucket == core::attention_bucket::quiet && core::merge_blocked(fixture.item(17)->Facts.Waiting), "settled review leaves independent blocker without inventing a notification category");
		auto       restored = application::queue_enrollment_register{ };
		const auto durable  = fixture.Store.State.Enrollments.front();
		restored.enroll(durable.Ref, durable.Membership);
		const auto encoded = application::encode_queue_facts(durable.LastFacts);
		expect(encoded.has_value(), "summary fits the bounded codec");
		const auto decoded = application::decode_queue_facts(*encoded);
		expect(decoded && decoded->Waiting.ChecksStale, "stored facts restart explicitly last-known");
		(void)restored.observe_facts(durable.Ref, *decoded, durable.FactsAccount);
		const auto items = restored.project({ }, nullptr, &session);
		expect(items.size() == 1 && items.front().Facts.Waiting.FailedChecks == waiting.FailedChecks, "same-account restart preserves remaining blockers");
		auto other          = session;
		other.ViewerAccount = core::github_account_id{ 9999 };
		const auto hidden   = restored.project({ }, nullptr, &other);
		expect(!hidden.front().Membership.authored() && hidden.front().Facts.Waiting.FailedChecks.empty(), "same login with another numeric account cannot borrow author or waiting facts");
		expect(!application::decode_queue_facts("1;99999:x"), "malformed private metadata fails closed");
		fixture.Queue.apply(application::queue_follow_intent{ .Ref = ref(17), .Follow = false });
		fixture.settle();
		expect(!fixture.item(17), "accepted ownership cannot turn a Follow-only PR into a new enrollment reason after Unfollow");
		fixture.Queue.shutdown();
		fixture.Discovery.shutdown();
		fixture.settle();
	}

	void test_partial_failed_check_restart_is_last_known()
	{
		auto record                           = application::enrollment_record{ };
		record.Ref                            = ref(17);
		record.Membership.Followed            = true;
		record.LastFacts.Waiting.Head         = std::string(40, 'a');
		record.LastFacts.Waiting.FailedChecks = { "Branch Freshness" };
		record.LastFacts.Waiting.ChecksStale  = true;
		record.FactsAccount                   = discovery_session().Queue.ViewerAccount;
		const auto encoded                    = application::encode_queue_facts(record.LastFacts);
		expect(encoded.has_value(), "partial failure fits the bounded metadata codec");
		const auto decoded = application::decode_queue_facts(*encoded);
		expect(decoded && !decoded->Waiting.ChecksKnown && decoded->Waiting.ChecksStale && !decoded->Waiting.RequestsStale && !decoded->Waiting.AssessmentStale && decoded->Waiting.FailedChecks == record.LastFacts.Waiting.FailedChecks, "codec retains partial failed check as last known without manufacturing other known facts");
		record.LastFacts = *decoded;
		auto fixture     = discovery_fixture{ { }, { record } };
		fixture.Queue.adopt(discovery_session().Queue);
		const auto item = fixture.item(17);
		expect(item && !item->Facts.Waiting.ChecksKnown && item->Facts.Waiting.ChecksStale && item->Facts.Waiting.FailedChecks == record.LastFacts.Waiting.FailedChecks, "host restoration does not erase partial-failure freshness from decoded metadata");
		fixture.Queue.shutdown();
		fixture.Discovery.shutdown();
		fixture.settle();
	}

	// [[BPR-0990]]. Saving a repository is searched without waiting for the
	// freshness window, a repository saved during a refresh is searched when it
	// ends, and forgetting one withdraws only what was found there.
	void test_discovery_follows_the_saved_repositories()
	{
		auto fixture = discovery_fixture{ { } };
		fixture.Discovery.adopt(discovery_session());
		expect(fixture.Provider.Requests.empty() && fixture.Discovery.status().State == application::queue_discovery_state::unavailable
				   && fixture.Discovery.status().Problem == application::queue_discovery_problem::no_saved_repository,
			   "with nothing saved nothing is read, and the status says what to do");
		const auto save = [&fixture](const std::string& aRepository, const application::saved_repository_action aAction) {
			fixture.Queue.apply(application::saved_repository_intent{ .Action = aAction, .Repository = { .Host = "github.com", .Account = "reviewer", .Repository = aRepository } });
			fixture.settle();
			fixture.Discovery.reconcile();
		};
		save("octo/app", application::saved_repository_action::add);
		expect(fixture.Provider.Requests.size() == 2U && fixture.Provider.Requests[0].Context.Repository == core::repository_locator{ "octo", "app" },
			   "saving a repository searches it at once");
		// Saved while that refresh runs: searched when it ends.
		save("octo/tools", application::saved_repository_action::add);
		expect(fixture.Provider.Requests.size() == 2U, "nothing more while a refresh runs");
		fixture.Provider.answer(fixture.Provider.rows({ row(5U) }));
		fixture.Provider.answer(fixture.Provider.rows({ row(6U, true, 1U) }));
		fixture.settle();
		expect(fixture.Provider.Requests.size() == 4U && fixture.Discovery.statistics().Refreshes == 2U, "the refresh that ends catches up with the saved list");
		answer_retained(fixture);
		fixture.Provider.answer(fixture.Provider.rows({ row(5U) }));
		fixture.Provider.answer(fixture.Provider.rows({ row(6U, true, 1U) }));
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.item(5) && fixture.item(6) && fixture.Discovery.status().State == application::queue_discovery_state::current,
			   "both repositories are searched and octo/app's rows are still found");
		fixture.Queue.apply(application::queue_follow_intent{ .Ref = { "github.com", "reviewer", "octo/app", 6 }, .Follow = true });
		fixture.settle();
		const auto before = fixture.Provider.Requests.size();
		save("octo/app", application::saved_repository_action::remove);
		expect(fixture.Provider.Requests.size() == before && fixture.Discovery.status().Repositories == 1U,
			   "forgetting a repository reads nothing, and the status counts what is still saved");
		expect(!fixture.item(5) && fixture.record(5)->Departed == application::enrollment_departure::undiscovered,
			   "what was found only there is no longer found");
		expect(fixture.item(6) && fixture.record(6)->Membership.Followed && !fixture.record(6)->Membership.Authored,
			   "a followed pull request stays, without the reason it was found for");
		fixture.Discovery.reconcile();
		expect(fixture.Provider.Requests.size() == before, "an unchanged saved list reads nothing");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	void test_discovery_freshness_and_forgotten_repositories()
	{
		auto fixture = discovery_fixture{ };
		fixture.Discovery.adopt(discovery_session());
		fixture.Discovery.activate();
		fixture.Discovery.refresh();
		expect(fixture.Provider.Requests.size() == 2U && fixture.Discovery.statistics().Refreshes == 1U, "activation and Refresh during a refresh add nothing");
		// octo/app is forgotten while its reads are in flight.
		fixture.Queue.apply(application::saved_repository_intent{ .Action = application::saved_repository_action::remove, .Repository = { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app" } });
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ row(5U) }));
		fixture.Provider.answer(fixture.Provider.rows({ row(6U) }));
		fixture.settle();
		expect(!fixture.item(5) && !fixture.item(6), "a repository forgotten mid-flight publishes nothing");
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.Discovery.status().State == application::queue_discovery_state::current, "the refresh completes over what is still saved");
		const auto settled = fixture.Provider.Requests.size();
		fixture.Clock.Now += application::personal_discovery_freshness_seconds - 1;
		fixture.Discovery.activate();
		expect(fixture.Provider.Requests.size() == settled, "activation inside the freshness window reads nothing");
		fixture.Clock.Now += 1;
		fixture.Discovery.activate();
		expect(fixture.Provider.Requests.size() == settled + 2U && fixture.Discovery.statistics().MaximumInFlight == application::maximum_personal_discovery_in_flight,
			   "activation after it refreshes, never more than two reads at once");
		fixture.Discovery.shutdown();
		fixture.Queue.shutdown();
	}

	// [[BPR-1681]]. A refresh that ran to its end is fresh for activation
	// even when one repository or pull request failed: coming back to Home (or
	// to the window after the diagnostic export's dialog) does not run the whole
	// refresh again. A refresh a session-wide failure stopped is retried.
	void test_activation_after_a_refresh_that_ended_with_a_failure()
	{
		auto fixture = discovery_fixture{ };
		fixture.Discovery.adopt(discovery_session());
		fixture.Provider.answer(fixture.Provider.failure(application::github_provider_failure::permission));
		fixture.Provider.answer(fixture.Provider.rows({ row(5U) }));
		fixture.settle();
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.Provider.answer(fixture.Provider.rows({ }));
		fixture.settle();
		expect(fixture.Discovery.status().State == application::queue_discovery_state::stale
				   && fixture.Discovery.status().Problem == application::queue_discovery_problem::repository_unavailable
				   && !fixture.Discovery.status().LastSuccess && fixture.item(5),
			   "a refresh with one unreadable repository ends stale, says why and keeps what it found");
		const auto ended = fixture.Provider.Requests.size();
		expect(ended == 4U && fixture.Discovery.statistics().Refreshes == 1U, "the refresh read each repository once");
		fixture.Clock.Now += 30;
		fixture.Discovery.activate();
		fixture.Clock.Now += application::personal_discovery_freshness_seconds - 31;
		fixture.Discovery.activate();
		expect(fixture.Provider.Requests.size() == ended && fixture.Discovery.statistics().Refreshes == 1U,
			   "activation inside the freshness window of that ended refresh reads nothing");
		fixture.Clock.Now += 1;
		fixture.Discovery.activate();
		expect(fixture.Provider.Requests.size() == ended + 1U && fixture.Provider.next().Reason == application::personal_discovery_reason::retained
				   && fixture.Discovery.statistics().Refreshes == 2U,
			   "activation after the freshness window refreshes again, retained metadata first");
		fixture.Provider.answer(fixture.Provider.failure(application::github_provider_failure::network));
		fixture.settle();
		expect(fixture.Discovery.status().State == application::queue_discovery_state::stale
				   && fixture.Discovery.status().Problem == application::queue_discovery_problem::offline,
			   "a network failure stops the refresh");
		const auto stopped = fixture.Provider.Requests.size();
		fixture.Clock.Now += 30;
		fixture.Discovery.activate();
		expect(fixture.Provider.Requests.size() == stopped + 1U && fixture.Discovery.statistics().Refreshes == 3U,
			   "a refresh a session-wide failure stopped is retried on the next activation");
		fixture.Discovery.shutdown();
		fixture.settle();
		fixture.Queue.shutdown();
	}

	void test_repository_addition_refuses_unknown_durable_state()
	{
		auto store     = memory_queue_store{ };
		store.FailLoad = true;
		store.State.SavedRepositories.push_back({ "github.com", "reviewer", "octo/retained" });
		auto       refresh    = observed_refresh{ };
		auto       effects    = application::manual_executor{ };
		auto       decisions  = application::manual_executor{ };
		auto       projection = landing_log{ };
		const auto session    = application::queue_session_id{ "github.com", "reviewer", 4U };
		auto       host       = application::review_queue_application_host{ store, refresh, effects, decisions, projection, session };
		host.restore();
		run_one(effects, decisions);
		host.apply(application::repository_addition_intent{
			.Source      = { core::default_bpr_profile_id, core::bpr_window_id{ 1U }, core::bpr_window_generation{ 1U } },
			.Gesture     = 1U,
			.Destination = session,
			.Text        = "octo/app" });
		expect(effects.pending() == 0U && store.SavedChanges == 0U && projection.Published.back().Addition->Status == core::repository_addition_status::unavailable
				   && store.State.SavedRepositories.size() == 1U,
			   "failed initial durable load cannot overwrite unseen saved state");
		host.shutdown();
		expect(host.is_quiescent(), "refused initial load leaves no outstanding addition");
	}

	void test_repository_addition_results_without_a_pull_request()
	{
		using status           = core::repository_addition_status;
		auto       store       = memory_queue_store{ };
		auto       refresh     = observed_refresh{ };
		auto       effects     = application::manual_executor{ };
		auto       decisions   = application::manual_executor{ };
		auto       projections = landing_log{ };
		const auto source      = core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ 1U }, core::bpr_window_generation{ 1U } };
		const auto session     = application::queue_session_id{ "github.com", "reviewer", 4U };
		auto       host        = application::review_queue_application_host{ store, refresh, effects, decisions, projections, session };
		auto       intent      = application::repository_addition_intent{ .Source = source, .Gesture = 1U, .Destination = session, .Text = "octo/app" };
		host.apply(intent);
		expect(projections.Published.back().Addition->Status == status::unavailable && effects.pending() == 0U, "addition refuses before durable state is known");
		host.restore();
		run_one(effects, decisions);
		host.apply(intent);
		expect(projections.Published.back().Addition->Status == status::saving && store.SavedChanges == 0U && host.saved_repositories().empty(), "saving projection precedes effect/store without a loaded PR");
		host.apply(intent);
		expect(effects.pending() == 1U && projections.Published.back().Addition->Status == status::saving, "repeat of exact active gesture leaves one save/progress");
		intent.Gesture = 2U;
		host.apply(intent);
		expect(effects.pending() == 1U && projections.Published.back().Addition->Status == status::busy && projections.Published.back().Addition->Gesture == 2U, "another gesture cannot overtake active save");
		run_one(effects, decisions);
		expect(store.SavedChanges == 1U && host.saved_repositories().size() == 1U && projections.Published.back().Addition->Status == status::saved && projections.Published.back().Addition->Gesture == 1U, "durable terminal belongs to originating gesture even after busy refusal");
		intent.Gesture = 3U;
		host.apply(intent);
		expect(effects.pending() == 0U && store.SavedChanges == 1U && projections.Published.back().Addition->Status == status::already_added, "exact duplicate reports already added with no second write");
		intent.Text = "https://enterprise.example/octo/app";
		host.apply(intent);
		expect(effects.pending() == 0U && projections.Published.back().Addition->Status == status::destination_changed && projections.Published.back().Addition->Target->Hostname == "enterprise.example", "another URL host is explicit and has no save/configuration authority");
		intent.Text = "octo/app/pull/1";
		host.apply(intent);
		expect(effects.pending() == 0U && projections.Published.back().Addition->Status == status::invalid_input && projections.Published.back().Addition->InputProblem.has_value(), "non-repository input reports its typed correction");
		intent.Text                   = "octo/tools";
		intent.Destination.Generation = 3U;
		host.apply(intent);
		expect(effects.pending() == 0U && projections.Published.back().Addition->Status == status::destination_changed, "old same-account authentication generation cannot save");
		intent.Destination       = session;
		intent.Source.Generation = core::bpr_window_generation{ 2U };
		host.apply(intent);
		expect(effects.pending() == 0U && projections.Published.back().Addition->Status == status::destination_changed, "wrong window generation cannot save");
		intent.Source             = source;
		store.FailEnrollmentStore = true;
		host.apply(intent);
		run_one(effects, decisions);
		expect(host.saved_repositories().size() == 1U && projections.Published.back().Addition->Status == status::unavailable && projections.Published.back().Addition->StoreFailure == application::queue_store_failure::unavailable && projections.Published.back().Addition->Target->Repository.Name == "tools", "refused storage retains target and reports no save");
		store.FailEnrollmentStore = false;
		++intent.Gesture;
		host.apply(intent);
		run_one(effects, decisions);
		expect(host.saved_repositories().size() == 2U && projections.Published.back().Addition->Status == status::saved, "fresh retry after local refusal succeeds without restoring/resetting profile");
		store.ThrowSavedStore = true;
		intent.Text           = "octo/throw";
		++intent.Gesture;
		host.apply(intent);
		run_one(effects, decisions);
		expect(host.is_quiescent() && host.saved_repositories().size() == 2U && projections.Published.back().Addition->Status == status::unavailable, "adapter exception drains as typed refusal");
		expect(refresh.Requests.empty() && store.State.Enrollments.empty() && store.State.Rules.empty(), "local addition opens no PR, performs no network read/mutation and enrolls no observer rule");
		host.adopt({ });
		expect(!projections.Published.back().Addition, "sign-out clears prior account result");
		host.apply(intent);
		expect(effects.pending() == 0U && projections.Published.back().Addition->Status == status::sign_in_required, "no account/public preview cannot create saved repository");
		host.shutdown();
	}

	void test_repository_addition_fanout_and_stale_terminals()
	{
		using status       = core::repository_addition_status;
		auto store         = memory_queue_store{ };
		auto refresh       = observed_refresh{ };
		auto effects       = application::manual_executor{ };
		auto decisions     = application::manual_executor{ };
		auto windows       = admitted_queue_windows{ };
		windows.Live       = { queue_window(1U), queue_window(2U) };
		const auto session = application::queue_session_id{ "github.com", "reviewer", 4U };
		auto       host    = application::review_queue_application_host{ store, refresh, effects, decisions, core::bpr_profile_id{ 2U }, windows, session };
		auto       first   = std::make_shared<window_queue_log>();
		auto       second  = std::make_shared<window_queue_log>();
		expect(host.attach(queue_window(1U), first).has_value() && host.attach(queue_window(2U), second).has_value(), "addition consumers attach under exact live profile contexts");
		host.restore();
		run_one(effects, decisions);
		auto intent = application::repository_addition_intent{ .Source = queue_window(1U), .Gesture = 1U, .Destination = session, .Text = "octo/app" };
		host.apply(intent);
		expect(!second->Published.back().Addition, "a sibling cannot receive another window's numeric Add gesture");
		auto siblingIntent   = intent;
		siblingIntent.Source = queue_window(2U);
		siblingIntent.Text   = "octo/sibling";
		host.apply(siblingIntent);
		expect(second->Published.back().Addition && second->Published.back().Addition->Status == status::busy && !first->Published.back().Addition,
			   "simultaneous equal-number gestures keep busy feedback on its source only");
		effects.run_next();
		expect(store.State.SavedRepositories.size() == 1U, "durable effect is not yet a decision-lane confirmation");
		auto replacement = session;
		++replacement.Generation;
		host.adopt(replacement);
		decisions.run_next();
		expect(first->Published.back().SavedRepositories.size() == 1U && second->Published.back().SavedRepositories == first->Published.back().SavedRepositories && !first->Published.back().Addition && !second->Published.back().Addition, "committed old-session save stays under original authority without confirming into replacement; siblings share record");
		intent.Destination = replacement;
		++intent.Gesture;
		host.apply(intent);
		expect(effects.pending() == 0U && first->Published.back().Addition->Status == status::already_added, "new displayed destination needs fresh deliberate gesture; duplicate is local");
		intent.Text = "octo/cancelled";
		++intent.Gesture;
		host.apply(intent);
		++replacement.Generation;
		host.adopt(replacement);
		run_one(effects, decisions);
		expect(store.SavedChanges == 1U && host.saved_repositories().size() == 1U && !first->Published.back().Addition, "authority replacement before effect cancels save without a store call");
		intent.Destination = replacement;
		intent.Text        = "octo/reentrant";
		++intent.Gesture;
		first->Callback = [&] {
			if (first->Published.back().Addition && first->Published.back().Addition->Status == status::saving) {
				first->Callback = { };
				++replacement.Generation;
				host.adopt(replacement);
			}
		};
		host.apply(intent);
		expect(effects.pending() == 0U && host.is_quiescent() && store.SavedChanges == 1U, "reentrant progress retirement cannot enqueue a stale write");
		intent.Destination = std::move(replacement);
		intent.Text        = "octo/window";
		++intent.Gesture;
		host.apply(intent);
		effects.run_next();
		windows.Live[0] = queue_window(1U, 2U);
		decisions.run_next();
		expect(second->Published.back().SavedRepositories.size() == 2U && !second->Published.back().Addition && host.is_quiescent(), "closed/replaced originating window cannot receive a saved terminal; live sibling gets durable record");
		intent.Source = queue_window(2U);
		intent.Text   = "octo/shutdown";
		++intent.Gesture;
		host.apply(intent);
		const auto count = second->Published.size();
		host.shutdown();
		run_one(effects, decisions);
		expect(host.is_quiescent() && store.SavedChanges == 2U && second->Published.size() == count, "shutdown drains typed cancellation without another write/publication");
	}

} // namespace

auto main() -> int
{
	test_repository_addition_refuses_unknown_durable_state();
	test_repository_addition_results_without_a_pull_request();
	test_repository_addition_fanout_and_stale_terminals();
	test_known_refresh_delta_and_complete_attachment_replay();
	test_legacy_combined_sink_keeps_every_reentrant_observation();
	test_restore_publication_shutdown_never_posts_a_later_load();
	test_rule_publication_shutdown_never_posts_reconciliation();
	test_profile_queue_fanout_replays_without_repeating_effects();
	test_queue_consumers_recheck_exact_generation_and_reentrant_detach();
	test_queue_consumer_capacity_counts_only_live_attachments();
	test_queue_callback_failure_and_observer_backlog_are_isolated();
	test_collection_boundaries_preserve_accepted_state();
	test_oversized_restores_are_atomic();
	test_enrollment_survives_the_path_that_matched();
	test_re_enrolling_keeps_both_reasons();
	test_losing_access_and_being_gone_are_distinguished();
	test_a_departed_item_is_retained_but_not_projected();
	test_reopening_restores_the_same_record();
	test_only_a_withdrawal_moves_an_item_out();
	test_every_departure_names_and_explains_itself();
	test_the_projection_is_ordered_by_attention();
	test_an_unobserved_item_is_not_known_to_be_quiet();
	test_activity_pairs_by_ref_in_any_order();
	test_single_refresh_projection_work();
	test_projection_finds_reports_without_walking_them();
	test_composed_host_persists_matches_and_removal();
	test_rule_removal_reconciles_every_matching_enrollment_before_publish();
	test_refresh_identity_and_cancellation_are_authoritative();
	test_failed_rule_reconciliation_rolls_back_before_unavailable_projection();
	test_rule_reconciliation_never_suppresses_a_github_role();
	test_rapid_independent_preference_gestures_merge_in_the_host();
	test_presentation_preferences_are_durable_before_projection();
	test_a_rule_added_during_a_refresh_still_enrolls();
	test_saved_repositories_are_durable_before_projection();
	test_saved_servers_are_durable_before_projection();
	test_retained_titles_and_reviews_before_open_and_on_refresh();
	test_retained_summary_limit_keeps_complete_metadata();
	test_change_facts_merge_and_persist();
	test_change_facts_codec_reads_version_one();
	test_retained_review_survives_request_withdrawal();
	test_retained_metadata_preserves_local_attention_and_rejects_late_results();
	test_draft_metadata_retains_only_accepted_exact_account_facts();
	test_retained_metadata_budget_and_restore_after_session();
	test_retained_failure_and_foreign_envelopes_preserve_accepted_state();
	test_closed_metadata_cannot_be_revived_by_the_same_search_cycle();
	test_ignore_is_durable_and_moves_only_the_section();
	test_ignores_survive_restart_and_closed_ones_are_pruned();
	test_one_scheduler_discovers_the_saved_repositories_of_its_account();
	test_discovery_records_each_refresh_and_read();
	test_discovery_searches_reuse_the_refresh_retained_reads();
	test_discovery_searches_several_repositories_at_once();
	test_discovery_batches_retained_reads();
	test_discovery_membership_is_honest_about_absence();
	test_discovery_failures_keep_rows_and_back_off();
	test_discovery_refuses_stale_and_foreign_answers();
	test_discovery_refuses_an_earlier_generation_of_the_same_account();
	test_discovery_gives_way_to_a_window_read_and_resumes();
	test_discovery_gives_way_across_a_session_change();
	test_a_refused_discovery_write_is_reported_not_kept();
	test_the_discovery_session_of_an_accepted_session();
	test_observer_authorship_does_not_change_enrollment();
	test_failed_metadata_storage_preserves_accepted_observations();
	test_home_waiting_observations_survive_local_activity_and_restart();
	test_partial_failed_check_restart_is_last_known();
	test_discovery_follows_the_saved_repositories();
	test_discovery_freshness_and_forgotten_repositories();
	test_activation_after_a_refresh_that_ended_with_a_failure();

	std::cout << "review-queue-enrollment-tests:pass\n";
	return EXIT_SUCCESS;
}
