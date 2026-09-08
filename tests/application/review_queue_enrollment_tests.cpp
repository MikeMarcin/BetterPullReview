#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_queue_enrollment.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <ranges>
#include <stop_token>
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

		std::size_t SavedChanges       = 0U;
		std::size_t SavedServerChanges = 0U;

		application::durable_queue_state State;
		std::size_t                      Loads               = 0U;
		bool                             FailEnrollmentStore = false;
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
			.Ref = ref(7), .Author = "octocat", .ChangedPaths = { "src/parser.cpp" }, .Title = "Parse faster"
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
				   == core::queue_item_facts{ .Title = "Parse faster", .Author = "octocat", .UnseenComments = 2U },
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

	// An item the host has not read yet is quiet, not guessed at. Inventing an
	// observation would put a pull request in a bucket on no evidence.
	void test_an_unobserved_item_is_quiet()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));

		const auto queue = records.project({ });
		expect(queue.size() == 1U, "it is still in the queue");
		expect(
			queue.front().Bucket == core::attention_bucket::quiet,
			"and asks for nothing until something has been read about it");
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
		expect(
			std::ranges::all_of(
				ignored,
				[](const core::queue_item& aItem) {
					return aItem.Bucket == core::attention_bucket::quiet;
				}),
			"a report about an unenrolled ref moves nothing");
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
		host.apply(application::queue_rule_intent{ .Rule = path_rule("overflow") });
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

} // namespace

auto main() -> int
{
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
	test_an_unobserved_item_is_quiet();
	test_activity_pairs_by_ref_in_any_order();
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

	std::cout << "review-queue-enrollment-tests:pass\n";
	return EXIT_SUCCESS;
}
