#include "bpr/application/review_queue_enrollment.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <deque>
#include <limits>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <utility>

namespace bpr::application {
	auto valid(const saved_server& aServer) noexcept -> bool
	{
		// A hostname as typed into a browser's address bar: no scheme, no
		// path, no port, no whitespace, bounded like a DNS name; the account
		// name, when present, bounded like a login.
		const auto& host = aServer.Hostname;
		return !host.empty() && host.size() <= 253U && host.front() != '.' && host.back() != '.'
			&& std::ranges::all_of(host, [](const char aCharacter) {
				   const auto byte = static_cast<unsigned char>(aCharacter);
				   return std::isalnum(byte) != 0 || aCharacter == '-' || aCharacter == '.';
			   })
			&& aServer.Account.size() <= 128U && std::ranges::all_of(aServer.Account, [](const char aCharacter) {
				   const auto byte = static_cast<unsigned char>(aCharacter);
				   return byte > 0x20U && byte < 0x7FU;
			   });
	}

	auto valid(const saved_repository& aRepository) noexcept -> bool
	{
		// "owner/name": one slash, both halves present, the same bound and
		// the same refusal of an embedded NUL a rule pattern has.
		const auto slash = aRepository.Repository.find('/');
		return !aRepository.Host.empty() && !aRepository.Account.empty()
			&& slash != std::string::npos && slash > 0U
			&& slash + 1U < aRepository.Repository.size()
			&& aRepository.Repository.find('/', slash + 1U) == std::string::npos
			&& aRepository.Repository.size() <= core::maximum_observer_rule_pattern_bytes
			&& aRepository.Repository.find('\0') == std::string::npos
			&& aRepository.Host.find('\0') == std::string::npos
			&& aRepository.Account.find('\0') == std::string::npos;
	}

	auto valid(const queue_presentation_preferences& aPreferences) noexcept -> bool
	{
		const auto roleValid = !aPreferences.Role.has_value()
			|| *aPreferences.Role <= core::github_role::author;
		const auto observerValid = !aPreferences.ObserverReason.has_value()
			|| *aPreferences.ObserverReason <= core::observer_rule_kind::path;
		const auto bucketValid = !aPreferences.Bucket.has_value()
			|| *aPreferences.Bucket <= core::attention_bucket::detail_unknown;
		return roleValid && observerValid && bucketValid
			&& aPreferences.Repository.size()
			<= core::maximum_observer_rule_pattern_bytes
			&& aPreferences.Repository.find('\0') == std::string::npos
			&& aPreferences.Quiet <= queue_quiet_filter::quiet
			&& aPreferences.Sort <= queue_sort_order::pull_request_number;
	}

	auto name(const enrollment_departure aDeparture) noexcept -> std::string_view
	{
		switch (aDeparture) {
		case enrollment_departure::none:
			return "present";
		case enrollment_departure::closed:
			return "closed";
		case enrollment_departure::access_lost:
			return "access lost";
		case enrollment_departure::gone:
			return "gone";
		case enrollment_departure::removed_by_reviewer:
			return "removed";
		case enrollment_departure::undiscovered:
			return "no longer discovered";
		}
		return "present";
	}

	auto explanation(const enrollment_departure aDeparture) noexcept
		-> std::string_view
	{
		switch (aDeparture) {
		case enrollment_departure::none:
			return "This is in your queue.";
		case enrollment_departure::closed:
			return "This pull request was closed or merged.";
		case enrollment_departure::access_lost:
			return "You can no longer read this pull request.";
		case enrollment_departure::gone:
			return "This pull request no longer exists where it was.";
		case enrollment_departure::removed_by_reviewer:
			return "You removed this from your queue.";
		case enrollment_departure::undiscovered:
			return "It is no longer open with my name on it in this saved repository.";
		}
		return "This is in your queue.";
	}

	auto queue_enrollment_register::find(const core::pull_request_ref& aRef)
		-> enrollment_record*
	{
		const auto found = RecordIndex.find(aRef);
		return found == RecordIndex.end() ? nullptr : &Records[found->second];
	}

	void queue_enrollment_register::enroll(
		const core::pull_request_ref& aRef,
		const core::queue_membership& aMembership)
	{
		if (auto* const existing = find(aRef); existing != nullptr) {
			// Both reasons are kept. A rule match must not overwrite a role, or
			// removing the rule would silently drop a pull request GitHub
			// assigned; a role must not overwrite the rule, or the reviewer
			// loses the answer to why an unfamiliar item is in their queue.
			if (aMembership.Role != core::github_role::none) {
				existing->Membership.Role = aMembership.Role;
			}
			if (aMembership.MatchedRule && !existing->Membership.MatchedRule) {
				existing->Membership.MatchedRule = true;
				existing->Membership.Rule        = aMembership.Rule;
			}
			// [[BPR-0989]]. Discovery reasons and the reviewer's own choices
			// are added, never taken away, by an enrollment.
			existing->Membership.Authored  = existing->Membership.Authored || aMembership.Authored;
			existing->Membership.Requested = existing->Membership.Requested || aMembership.Requested;
			existing->Membership.Followed  = existing->Membership.Followed || aMembership.Followed;
			existing->Membership.Reviewed  = existing->Membership.Reviewed || aMembership.Reviewed;
			existing->Departed             = enrollment_departure::none;
			return;
		}
		RecordIndex.emplace(aRef, Records.size());
		Records.push_back(enrollment_record{
			.Ref        = aRef,
			.Membership = aMembership,
			.Departed   = enrollment_departure::none,
		});
	}

	void queue_enrollment_register::reconcile_rule_change(
		const core::pull_request_ref& aRef,
		const core::queue_membership& aMembership)
	{
		if (auto* const existing = find(aRef); existing != nullptr) {
			// [[BPR-0989]]. A rule edit recomputes the role and the rule only;
			// discovery reasons, a follow and a review are not the rule's to
			// take away.
			existing->Membership.Role        = aMembership.Role;
			existing->Membership.MatchedRule = aMembership.MatchedRule;
			existing->Membership.Rule        = aMembership.Rule;
			existing->Departed               = existing->Membership.enrolled()
				? enrollment_departure::none
				: enrollment_departure::removed_by_reviewer;
			return;
		}
		if (aMembership.enrolled()) {
			enroll(aRef, aMembership);
		}
	}

	void queue_enrollment_register::depart(
		const core::pull_request_ref& aRef,
		const enrollment_departure    aDeparture)
	{
		if (auto* const existing = find(aRef); existing != nullptr) {
			existing->Departed = aDeparture;
		}
	}

	void queue_enrollment_register::observe(
		const queue_refresh_completion& aCompletion,
		const refresh_disposition       aDisposition)
	{
		if (aDisposition != refresh_disposition::withdrawn) {
			return;
		}
		// Forbidden and gone are different facts with different next steps: one
		// is a permission the reviewer may be able to ask for, the other is a
		// pull request that is not there any more. Collapsing them would send
		// somebody to the wrong person.
		depart(
			aCompletion.Ref,
			aCompletion.Status == queue_refresh_status::forbidden
				? enrollment_departure::access_lost
				: enrollment_departure::gone);
	}

	auto queue_enrollment_register::observe_discovery(const queue_discovery_answer& aAnswer) -> std::vector<core::pull_request_ref>
	{
		auto       changed = std::vector<core::pull_request_ref>{ };
		const auto refOf   = [&aAnswer](const std::int64_t aNumber) {
			return core::pull_request_ref{ .Host = aAnswer.Session.Host, .Account = aAnswer.Session.Account, .Repository = aAnswer.Repository, .Number = aNumber };
		};
		const auto flagOf = [&aAnswer](core::queue_membership& aMembership) -> bool& {
			return aAnswer.Requested ? aMembership.Requested : aMembership.Authored;
		};
		for (const auto& found : aAnswer.PullRequests) {
			const auto ref    = refOf(found.Number);
			auto*      record = find(ref);
			if (record == nullptr) {
				auto membership    = core::queue_membership{ };
				flagOf(membership) = true;
				RecordIndex.emplace(ref, Records.size());
				Records.push_back(enrollment_record{ .Ref = ref, .Membership = membership, .Departed = enrollment_departure::none });
				changed.push_back(ref);
				continue;
			}
			if (!flagOf(record->Membership) || !record->present()) {
				flagOf(record->Membership) = true;
				record->Departed           = enrollment_departure::none;
				changed.push_back(ref);
			}
		}
		if (!aAnswer.Complete) {
			return changed;
		}
		for (auto& record : Records) {
			const auto ours = record.Ref.Host == aAnswer.Session.Host && record.Ref.Account == aAnswer.Session.Account
				&& record.Ref.Repository == aAnswer.Repository;
			const auto kept = std::ranges::any_of(aAnswer.PullRequests, [&record](const queue_discovered_pull_request& aFound) {
				return aFound.Number == record.Ref.Number;
			});
			if (!ours || kept || !flagOf(record.Membership)) {
				continue;
			}
			flagOf(record.Membership) = false;
			if (record.present() && !record.Membership.enrolled()) {
				record.Departed = enrollment_departure::undiscovered;
			}
			changed.push_back(record.Ref);
		}
		return changed;
	}

	auto queue_enrollment_register::follow(const core::pull_request_ref& aRef, const bool aFollowed) -> bool
	{
		auto* record = find(aRef);
		if (record == nullptr) {
			if (!aFollowed) {
				return false;
			}
			RecordIndex.emplace(aRef, Records.size());
			Records.push_back(enrollment_record{ .Ref = aRef, .Membership = { .Followed = true }, .Departed = enrollment_departure::none });
			return true;
		}
		if (record->Membership.Followed == aFollowed && (!aFollowed || record->present())) {
			return false;
		}
		record->Membership.Followed = aFollowed;
		if (aFollowed) {
			record->Departed = enrollment_departure::none;
		} else if (record->present() && !record->Membership.enrolled()) {
			record->Departed = enrollment_departure::removed_by_reviewer;
		}
		return true;
	}

	auto queue_enrollment_register::reviewed(const core::pull_request_ref& aRef) -> bool
	{
		auto* record = find(aRef);
		if (record == nullptr || !record->present() || record->Membership.Reviewed) {
			return false;
		}
		record->Membership.Reviewed = true;
		return true;
	}

	auto queue_enrollment_register::records() const noexcept
		-> std::span<const enrollment_record>
	{
		return Records;
	}

	auto queue_enrollment_register::record_for(
		const core::pull_request_ref& aRef) const
		-> std::optional<enrollment_record>
	{
		const auto found = RecordIndex.find(aRef);
		if (found == RecordIndex.end()) {
			return std::nullopt;
		}
		return Records[found->second];
	}

	auto queue_enrollment_register::project(
		const std::span<const queue_activity_report> aActivity,
		queue_projection_work* const                 aWork) const
		-> std::vector<core::queue_item>
	{
		auto items = std::vector<core::queue_item>{ };
		items.reserve(Records.size());
		if (aWork && !Records.empty()) {
			++aWork->CapacityGrowths;
		}
		// [[BPR-0972]]. The reports are ordered once, so each record's report
		// is a binary search instead of a walk of every report. The first
		// report for a ref answers for it, as the walk did.
		auto reports = std::vector<const queue_activity_report*>{ };
		reports.reserve(aActivity.size());
		for (const auto& report : aActivity) {
			reports.push_back(&report);
		}
		std::ranges::stable_sort(reports, [](const queue_activity_report* aLeft, const queue_activity_report* aRight) {
			return aLeft->Ref < aRight->Ref;
		});
		for (const auto& record : Records) {
			if (aWork) {
				++aWork->RecordsVisited;
			}
			if (!record.present()) {
				continue;
			}
			// A missing report is not an error and must not be guessed at. The
			// host supplies what it read; an item it has not read yet is not
			// known to be quiet ([[BPR-0989]]), and `bucket_for` says so.
			const auto        candidate = std::ranges::lower_bound(reports, record.Ref, [&](const core::pull_request_ref& aLeft, const core::pull_request_ref& aRight) {
				if (aWork) {
					++aWork->ActivityComparisons;
				}
				return aLeft < aRight; }, [](const queue_activity_report* aReport) -> const core::pull_request_ref& { return aReport->Ref; });
			const auto* const found     = candidate != reports.end() && (*candidate)->Ref == record.Ref ? *candidate : nullptr;
			const auto        activity  = found == nullptr
				? core::queue_activity_observation{ .Known = false }
				: found->Observation;
			auto              facts     = found == nullptr ? core::queue_item_facts{ } : found->Facts;
			facts.UnseenComments        = activity.UnseenComments;
			items.push_back(core::queue_item{
				.Ref        = record.Ref,
				.Membership = record.Membership,
				.Bucket     = core::bucket_for(record.Membership, activity),
				.Facts      = std::move(facts),
			});
		}
		return core::ordered_queue(items);
	}

	namespace {
		class compatibility_queue_observer final : public queue_landing_projection_port
		{
		public:
			explicit compatibility_queue_observer(queue_landing_projection_port& aPort)
				: Port{ aPort }
			{ }
			void publish(const queue_landing_projection& aProjection) override { Port.publish(aProjection); }

		private:
			queue_landing_projection_port& Port;
		};

		/// \brief Owns consumer lifetimes and serializes callbacks independently of queue effects.
		class queue_projection_hub final
		{
			struct consumer
			{
				queue_consumer_id                             Id;
				core::bpr_window_context                      Context;
				std::shared_ptr<queue_window_projection_port> Port;
				std::uint64_t                                 Delivered = 0U;
			};

		public:
			explicit queue_projection_hub(queue_landing_projection_port& aPort)
				: Profile{ core::default_bpr_profile_id }
				, Observer{ std::make_shared<compatibility_queue_observer>(aPort) }
			{
				// The shipped sink combines notification observation and projection;
				// it must see every fresh state, never latest-only window replay.
			}
			queue_projection_hub(core::bpr_profile_id aProfile, queue_window_context_port& aWindows, std::shared_ptr<queue_landing_projection_port> aObserver)
				: Profile{ aProfile }
				, Windows{ &aWindows }
				, Observer{ std::move(aObserver) }
			{
				if (Profile.value() == 0U) {
					throw std::invalid_argument{ "invalid queue profile" };
				}
			}

			[[nodiscard]] auto attach(core::bpr_window_context aContext, std::shared_ptr<queue_window_projection_port> aPort)
				-> std::expected<queue_consumer_id, queue_attachment_failure>
			{
				if (Closing) {
					return std::unexpected{ queue_attachment_failure::closing };
				}
				if (!aPort) {
					return std::unexpected{ queue_attachment_failure::invalid_consumer };
				}
				if (!current(aContext)) {
					return std::unexpected{ queue_attachment_failure::stale_window };
				}
				prune();
				if (std::ranges::any_of(Consumers, [&aContext](const consumer& aConsumer) { return aConsumer.Context.Window == aContext.Window; })) {
					return std::unexpected{ queue_attachment_failure::invalid_consumer };
				}
				if (Consumers.size() >= core::maximum_bpr_windows_per_profile) {
					return std::unexpected{ queue_attachment_failure::capacity };
				}
				if (NextConsumer == std::numeric_limits<std::uint64_t>::max()) {
					return std::unexpected{ queue_attachment_failure::identity_exhausted };
				}
				const auto id = queue_consumer_id{ NextConsumer++ };
				Consumers.push_back({ id, aContext, std::move(aPort) });
				drain();
				return id;
			}
			void detach(queue_consumer_id aId)
			{
				std::erase_if(Consumers, [aId](const consumer& aConsumer) { return aConsumer.Id == aId; });
			}
			void publish(queue_landing_projection aProjection)
			{
				if (Closing) {
					return;
				}
				if (aProjection.UpdatedItem) {
					Retained.insert_or_assign(aProjection.UpdatedItem->Ref, *aProjection.UpdatedItem);
				} else {
					Retained.clear();
					for (const auto& item : aProjection.Items) {
						Retained.emplace(item.Ref, item);
					}
				}
				Complete.reset();
				Latest = std::make_shared<const queue_landing_projection>(std::move(aProjection));
				++Revision;
				if (Observer) {
					if (Observations.size() >= core::maximum_bpr_windows_per_profile) {
						Failed = true;
						Observer.reset();
						Observations.clear();
					} else {
						Observations.push_back(Windows == nullptr ? snapshot() : Latest);
					}
				}
				drain();
			}
			void shutdown()
			{
				Closing = true;
				Consumers.clear();
				Observations.clear();
				Observer.reset();
			}
			[[nodiscard]] auto admitted(const core::bpr_window_context& aContext) const noexcept -> bool { return !Closing && current(aContext); }
			[[nodiscard]] auto size() const noexcept -> std::size_t { return Consumers.size(); }
			[[nodiscard]] auto failed() const noexcept -> bool { return Failed; }

		private:
			[[nodiscard]] auto snapshot() -> std::shared_ptr<const queue_landing_projection>
			{
				if (!Latest->UpdatedItem) {
					return Latest;
				}
				if (!Complete) {
					auto projection = *Latest;
					projection.UpdatedItem.reset();
					projection.Items.reserve(Retained.size());
					for (const auto& [ref, item] : Retained) {
						static_cast<void>(ref);
						projection.Items.push_back(item);
					}
					projection.Items = core::ordered_queue(projection.Items);
					Complete         = std::make_shared<const queue_landing_projection>(std::move(projection));
				}
				return Complete;
			}
			[[nodiscard]] auto current(const core::bpr_window_context& aContext) const noexcept -> bool
			{
				return aContext.Profile == Profile && aContext.Window.value() != 0U && aContext.Generation.value() != 0U
					&& (Windows ? Windows->current(aContext) : aContext == core::bpr_window_context{ Profile, core::bpr_window_id{ 1U }, core::bpr_window_generation{ 1U } });
			}
			void prune()
			{
				std::erase_if(Consumers, [this](const consumer& aConsumer) { return !current(aConsumer.Context); });
			}
			void observe_one()
			{
				const auto projection = Observations.front();
				Observations.pop_front();
				const auto observer = Observer;
				try {
					if (observer) {
						observer->publish(*projection);
					}
				} catch (...) {
					Failed = true;
					Observer.reset();
					Observations.clear();
				}
			}
			[[nodiscard]] auto deliver_one() -> bool
			{
				prune();
				const auto found = std::ranges::find_if(Consumers, [this](const consumer& aConsumer) { return aConsumer.Delivered != Revision; });
				if (found == Consumers.end() || !Latest) {
					return false;
				}
				// Copy only the exact attachment and shared immutable payload. A callback
				// can erase/reallocate the live list or publish a newer revision safely.
				const auto selected   = *found; // NOLINT(performance-unnecessary-copy-initialization): callbacks may erase the referenced consumer.
				const auto projection = found->Delivered + 1U == Revision && found->Delivered != 0U ? Latest : snapshot();
				found->Delivered      = Revision;
				try {
					if (current(selected.Context)) {
						// The collection is shared; a gesture result belongs only to
						// its exact originating attachment. Sibling gesture counters
						// may have the same numeric value.
						if (projection->Addition && projection->Addition->Source != selected.Context) {
							auto scoped = *projection;
							scoped.Addition.reset();
							selected.Port->publish(selected.Context, scoped);
						} else {
							selected.Port->publish(selected.Context, *projection);
						}
					}
				} catch (...) {
					Failed = true;
					detach(selected.Id);
				}
				return true;
			}
			void drain()
			{
				if (Delivering || Closing) {
					return;
				}
				Delivering = true;
				while (!Closing) {
					if (!Observations.empty()) {
						observe_one();
					} else if (!deliver_one()) {
						break;
					}
				}
				Delivering = false;
			}

			const core::bpr_profile_id                                  Profile;
			queue_window_context_port*                                  Windows = nullptr;
			std::shared_ptr<queue_landing_projection_port>              Observer;
			std::vector<consumer>                                       Consumers;
			std::shared_ptr<const queue_landing_projection>             Latest;
			std::shared_ptr<const queue_landing_projection>             Complete;
			std::map<core::pull_request_ref, core::queue_item>          Retained;
			std::deque<std::shared_ptr<const queue_landing_projection>> Observations;
			std::uint64_t                                               NextConsumer = 1U;
			std::uint64_t                                               Revision     = 0U;
			bool                                                        Closing      = false;
			bool                                                        Delivering   = false;
			bool                                                        Failed       = false;
		};
	}

	struct review_queue_application_host::implementation
	{
		enum class request_kind : std::uint8_t {
			load,
			rule_change,
			preferences,
			refresh,
			store_enrollment,
			saved_repository,
			saved_server,
			/// [[BPR-0989]].
			store_enrollments,
		};

		struct completion_gate
		{
			std::atomic<implementation*> Host = nullptr;
		};

		struct outstanding_request
		{
			std::uint64_t                         Id   = 0U;
			request_kind                          Kind = request_kind::load;
			queue_session_id                      Session;
			std::optional<core::pull_request_ref> Ref;
			std::stop_source                      Stop;
			bool                                  Cancelled = false;
		};

		implementation(
			queue_enrollment_store_port&   aStore,
			queue_refresh_effect_port&     aRefresh,
			effect_executor&               aEffects,
			decision_executor&             aDecisionLane,
			queue_landing_projection_port& aProjection,
			queue_session_id               aSession)
			: Store{ aStore }
			, Refresh{ aRefresh }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projection{ aProjection }
			, Scheduler{ std::move(aSession) }
			, Gate{ std::make_shared<completion_gate>() }
		{
			Gate->Host.store(this, std::memory_order_release);
		}

		implementation(queue_enrollment_store_port& aStore, queue_refresh_effect_port& aRefresh,
					   effect_executor& aEffects, decision_executor& aDecisionLane, core::bpr_profile_id aProfile,
					   queue_window_context_port& aWindows, queue_session_id aSession, std::shared_ptr<queue_landing_projection_port> aObserver)
			: Store{ aStore }
			, Refresh{ aRefresh }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projection{ aProfile, aWindows, std::move(aObserver) }
			, Scheduler{ std::move(aSession) }
			, Gate{ std::make_shared<completion_gate>() }
		{
			Gate->Host.store(this, std::memory_order_release);
		}

		~implementation()
		{
			Gate->Host.store(nullptr, std::memory_order_release);
		}

		[[nodiscard]] static auto locked_host(
			const std::weak_ptr<completion_gate>& aGate) noexcept
			-> implementation*
		{
			const auto gate = aGate.lock();
			return gate
				? gate->Host.load(std::memory_order_acquire)
				: nullptr;
		}

		[[nodiscard]] auto begin(
			const request_kind                    aKind,
			queue_session_id                      aSession = { },
			std::optional<core::pull_request_ref> aRef     = std::nullopt)
			-> outstanding_request&
		{
			Outstanding.push_back(outstanding_request{
				.Id      = NextRequest++,
				.Kind    = aKind,
				.Session = std::move(aSession),
				.Ref     = std::move(aRef),
			});
			return Outstanding.back();
		}

		[[nodiscard]] auto claim_refresh(
			const std::uint64_t             aId,
			const queue_refresh_completion& aCompletion) -> bool
		{
			const auto found = std::ranges::find_if(
				Outstanding,
				[aId](const outstanding_request& aRequest) {
					return aRequest.Id == aId
						&& aRequest.Kind == request_kind::refresh;
				});
			if (found == Outstanding.end()) {
				++Statistics.StaleCompletions;
				return false;
			}
			const auto cancelled = found->Cancelled;
			const auto matches   = found->Session == aCompletion.Session
				&& found->Ref == std::optional{ aCompletion.Ref };
			Outstanding.erase(found);
			if (cancelled) {
				++Statistics.CancelledCompletions;
				return false;
			}
			if (!matches) {
				++Statistics.StaleCompletions;
				return false;
			}
			return true;
		}

		[[nodiscard]] auto retire(
			const std::uint64_t aId,
			const request_kind  aKind) -> bool
		{
			const auto found = std::ranges::find_if(
				Outstanding,
				[aId, aKind](const outstanding_request& aRequest) {
					return aRequest.Id == aId && aRequest.Kind == aKind;
				});
			if (found == Outstanding.end()) {
				++Statistics.StaleCompletions;
				return false;
			}
			Outstanding.erase(found);
			return true;
		}

		void report_unavailable(const queue_store_failure aFailure = queue_store_failure::unavailable)
		{
			if (aFailure == queue_store_failure::collection_limit && Status == queue_landing_status::ready) {
				report_collection_limit();
				return;
			}
			++Statistics.Failures;
			Failure = aFailure;
			Status  = queue_landing_status::unavailable;
			publish();
		}

		void report_collection_limit()
		{
			++Statistics.Failures;
			Failure = queue_store_failure::collection_limit;
			publish();
		}

		void publish(std::optional<core::queue_item> aItem = std::nullopt)
		{
			const auto itemUpdate = aItem.has_value();
			Projection.publish(queue_landing_projection{
				.Status            = Status,
				.Rules             = Rules,
				.Items             = itemUpdate ? std::vector<core::queue_item>{ } : Enrollments.project(Activity, &Statistics.Projection),
				.Preferences       = Preferences,
				.Session           = Scheduler.session(),
				.Online            = Scheduler.online(),
				.BackoffSeconds    = Scheduler.backoff_seconds(),
				.SavedRepositories = SavedRepositories,
				.SavedServers      = SavedServers,
				.Failure           = Failure,
				.Discovery         = Discovery,
				.Addition          = Addition,
				.UpdatedItem       = std::move(aItem),
			});
			Statistics.ItemPublications += itemUpdate ? 1U : 0U;
			++Statistics.Publications;
		}

		void restore()
		{
			if (Closing || Restored) {
				return;
			}
			Status = queue_landing_status::restoring;
			publish();
			if (Closing) {
				return;
			}
			auto&      request = begin(request_kind::load);
			const auto id      = request.Id;
			const auto stop    = request.Stop.get_token();
			const auto gate    = std::weak_ptr<completion_gate>{ Gate };
			try {
				Effects.post([this, gate, id, stop] {
					auto result = Store.load(stop);
					DecisionLane.post(
						[gate, id, loadResult = std::move(result)]() mutable {
							if (auto* const host = locked_host(gate); host != nullptr) {
								host->finish_load(id, std::move(loadResult));
							}
						});
				});
			} catch (...) {
				(void)retire(id, request_kind::load);
				report_unavailable();
			}
		}

		void finish_load(
			const std::uint64_t                                     aId,
			std::expected<durable_queue_state, queue_store_failure> aResult)
		{
			if (!retire(aId, request_kind::load) || Closing) {
				return;
			}
			Restored = true;
			if (!aResult) {
				report_unavailable(aResult.error());
				return;
			}
			if (aResult->Rules.size() > maximum_queue_rules || aResult->SavedRepositories.size() > maximum_saved_repositories
				|| aResult->SavedServers.size() > maximum_saved_servers || aResult->Enrollments.size() > maximum_queue_enrollments) {
				report_unavailable(queue_store_failure::collection_limit);
				return;
			}
			DurableStateLoaded = true;
			Rules              = std::move(aResult->Rules);
			Preferences        = std::move(aResult->Preferences);
			SavedRepositories  = std::move(aResult->SavedRepositories);
			SavedServers       = std::move(aResult->SavedServers);
			for (const auto& record : aResult->Enrollments) {
				Enrollments.enroll(record.Ref, record.Membership);
				if (!record.present()) {
					Enrollments.depart(record.Ref, record.Departed);
				}
			}
			Failure.reset();
			Status = queue_landing_status::ready;
			publish();
			reconcile_last();
		}

		[[nodiscard]] auto addition_current(const repository_addition_result& aResult) const noexcept -> bool
		{
			return core::accept_repository_addition_completion(Closing, aResult.Destination == Scheduler.session(), Projection.admitted(aResult.Source));
		}

		void apply(repository_addition_intent aIntent)
		{
			if (Closing) {
				return;
			}
			if (SavedMutationPending && Addition && Addition->Source == aIntent.Source && Addition->Gesture == aIntent.Gesture
				&& Addition->Status == core::repository_addition_status::saving) {
				return;
			}
			auto result           = repository_addition_result{ };
			result.Source         = aIntent.Source;
			result.Gesture        = aIntent.Gesture;
			result.Destination    = aIntent.Destination;
			const auto parsed     = parse_repository_addition(aIntent.Text, aIntent.Destination.Host);
			auto       repository = saved_repository{ };
			if (parsed) {
				result.Target         = *parsed;
				repository.Host       = parsed->Hostname;
				repository.Account    = aIntent.Destination.Account;
				repository.Repository = parsed->Repository.Owner + "/" + parsed->Repository.Name;
			} else {
				result.InputProblem = parsed.error();
			}
			const auto& session = Scheduler.session();
			result.Status       = core::admit_repository_addition(core::repository_addition_observation{
				.SignedIn           = !session.Host.empty() && !session.Account.empty() && session.Generation != 0U,
				.DestinationCurrent = aIntent.Destination == session && (!parsed || parsed->Hostname == session.Host),
				.WindowCurrent      = aIntent.Gesture != 0U && Projection.admitted(aIntent.Source),
				.StoreReady         = DurableStateLoaded,
				.InputValid         = parsed.has_value(),
				.Pending            = SavedMutationPending,
				.AlreadyAdded       = std::ranges::find(SavedRepositories, repository) != SavedRepositories.end(),
				.HasCapacity        = SavedRepositories.size() < maximum_saved_repositories,
			});
			Addition            = result;
			if (result.Status != core::repository_addition_status::saving) {
				++Statistics.SavedRepositoryIntents;
				publish();
				return;
			}
			apply(saved_repository_intent{ .Repository = std::move(repository) }, std::move(result));
		}

		// [[BPR-0659]]. A save or forget is durable before it is shown: the
		// store answers on the effect lane and the decision lane adopts the
		// list it was asked for, so a refused write leaves the list as it was.
		void apply(saved_repository_intent aIntent, std::optional<repository_addition_result> aAddition = std::nullopt)
		{
			++Statistics.SavedRepositoryIntents;
			if (Closing || !Restored || !valid(aIntent.Repository)) {
				++Statistics.Failures;
				return;
			}
			if (SavedMutationPending) {
				return;
			}
			const auto exists = std::ranges::find(SavedRepositories, aIntent.Repository)
				!= SavedRepositories.end();
			if ((aIntent.Action == saved_repository_action::add && exists)
				|| (aIntent.Action == saved_repository_action::remove && !exists)) {
				return;
			}
			if (aIntent.Action == saved_repository_action::add && SavedRepositories.size() >= maximum_saved_repositories) {
				report_collection_limit();
				return;
			}
			auto next = SavedRepositories;
			if (aIntent.Action == saved_repository_action::add) {
				next.push_back(aIntent.Repository);
			} else {
				std::erase(next, aIntent.Repository);
			}
			++Statistics.PersistenceRequests;
			auto&      request   = begin(request_kind::saved_repository);
			const auto id        = request.Id;
			const auto stop      = request.Stop.get_token();
			const auto gate      = std::weak_ptr<completion_gate>{ Gate };
			SavedMutationPending = true;
			if (aAddition) {
				publish();
				// A progress observer may synchronously retire the source/session.
				if (!addition_current(*aAddition)) {
					SavedMutationPending = false;
					(void)retire(id, request_kind::saved_repository);
					Addition.reset();
					return;
				}
			}
			try {
				Effects.post([this, gate, id, stop, intent = std::move(aIntent), pending = std::move(next), addition = aAddition]() mutable {
					auto result = std::expected<void, queue_store_failure>{ std::unexpected{ queue_store_failure::unavailable } };
					try {
						result = stop.stop_requested() ? std::unexpected{ queue_store_failure::cancelled } : Store.apply_saved_repository_change(intent, stop);
					} catch (...) {
						// An adapter exception still owes this request its typed terminal.
					}
					DecisionLane.post(
						[gate, id, published = std::move(pending), applyResult = std::move(result), addition]() mutable {
							if (auto* const host = locked_host(gate); host != nullptr) {
								host->finish_saved_repository(id, std::move(published), std::move(applyResult), addition);
							}
						});
				});
			} catch (...) {
				SavedMutationPending = false;
				(void)retire(id, request_kind::saved_repository);
				if (aAddition && addition_current(*aAddition)) {
					Addition               = *aAddition;
					Addition->Status       = core::repository_addition_status::unavailable;
					Addition->StoreFailure = queue_store_failure::unavailable;
				}
				report_unavailable();
			}
		}

		// [[BPR-0660]]. Remembering a server is the saved-repository shape
		// over its own list and its own pending flag.
		void apply(saved_server_intent aIntent)
		{
			++Statistics.SavedServerIntents;
			if (Closing || !Restored || !valid(aIntent.Server)) {
				++Statistics.Failures;
				return;
			}
			if (SavedServerMutationPending) {
				return;
			}
			const auto known  = std::ranges::find(SavedServers, aIntent.Server.Hostname, &saved_server::Hostname);
			const auto exists = known != SavedServers.end();
			if ((aIntent.Action == saved_server_action::add && exists && *known == aIntent.Server)
				|| (aIntent.Action == saved_server_action::remove && !exists)) {
				return;
			}
			if (aIntent.Action == saved_server_action::add && !exists && SavedServers.size() >= maximum_saved_servers) {
				report_collection_limit();
				return;
			}
			auto next = SavedServers;
			std::erase_if(next, [&aIntent](const saved_server& aServer) {
				return aServer.Hostname == aIntent.Server.Hostname;
			});
			if (aIntent.Action == saved_server_action::add) {
				next.push_back(aIntent.Server);
				std::ranges::sort(next, { }, &saved_server::Hostname);
			}
			++Statistics.PersistenceRequests;
			auto&      request         = begin(request_kind::saved_server);
			const auto id              = request.Id;
			const auto stop            = request.Stop.get_token();
			const auto gate            = std::weak_ptr<completion_gate>{ Gate };
			SavedServerMutationPending = true;
			try {
				Effects.post([this, gate, id, stop, intent = std::move(aIntent), pending = std::move(next)]() mutable {
					auto result = Store.apply_saved_server_change(intent, stop);
					DecisionLane.post(
						[gate, id, published = std::move(pending), applyResult = std::move(result)]() mutable {
							if (auto* const host = locked_host(gate); host != nullptr) {
								host->finish_saved_server(id, std::move(published), std::move(applyResult));
							}
						});
				});
			} catch (...) {
				SavedServerMutationPending = false;
				(void)retire(id, request_kind::saved_server);
				report_unavailable();
			}
		}
		void finish_saved_server(
			const std::uint64_t                             aId,
			std::vector<saved_server>                       aSaved,
			const std::expected<void, queue_store_failure>& aResult)
		{
			if (!retire(aId, request_kind::saved_server) || Closing) {
				return;
			}
			SavedServerMutationPending = false;
			if (!aResult) {
				report_unavailable(aResult.error());
				return;
			}
			SavedServers = std::move(aSaved);
			Failure.reset();
			Status = queue_landing_status::ready;
			publish();
		}
		void finish_saved_repository(
			const std::uint64_t                              aId,
			std::vector<saved_repository>                    aSaved,
			const std::expected<void, queue_store_failure>&  aResult,
			const std::optional<repository_addition_result>& aAddition)
		{
			if (!retire(aId, request_kind::saved_repository) || Closing) {
				return;
			}
			SavedMutationPending = false;
			if (aAddition && addition_current(*aAddition)) {
				Addition         = *aAddition;
				Addition->Status = aResult ? core::repository_addition_status::saved : core::repository_addition_status::unavailable;
				if (!aResult) {
					Addition->StoreFailure = aResult.error();
				}
			} else if (aAddition) {
				Addition.reset();
			}
			if (!aResult) {
				report_unavailable(aResult.error());
				return;
			}
			// [[BPR-0990]]. A forgotten repository is no longer searched, so
			// what discovery found only there is no longer found: its reasons
			// are withdrawn as a complete answer that finds nothing would
			// withdraw them. A follow, a review, a role or a rule still holds.
			auto prior   = Enrollments;
			auto changed = std::vector<core::pull_request_ref>{ };
			for (const auto& forgotten : SavedRepositories) {
				if (std::ranges::find(aSaved, forgotten) != aSaved.end()) {
					continue;
				}
				for (const auto requested : { false, true }) {
					auto withdrawn = Enrollments.observe_discovery(queue_discovery_answer{
						.Session    = queue_session_id{ .Host = forgotten.Host, .Account = forgotten.Account },
						.Repository = forgotten.Repository,
						.Requested  = requested,
						.Complete   = true,
					});
					changed.insert(changed.end(), withdrawn.begin(), withdrawn.end());
				}
			}
			SavedRepositories = std::move(aSaved);
			Failure.reset();
			Status = queue_landing_status::ready;
			if (!changed.empty()) {
				persist_records(changed, std::move(prior));
				return;
			}
			publish();
		}

		void apply(queue_preferences_intent aIntent)
		{
			++Statistics.PreferenceIntents;
			if (Closing || !Restored || !valid(aIntent.Preferences)) {
				return;
			}
			if (aIntent.Field != queue_preference_field::all) {
				auto        next    = QueuedPreferences.value_or(PendingPreferences.value_or(Preferences));
				const auto& offered = aIntent.Preferences;
				switch (aIntent.Field) {
				case queue_preference_field::role:                  next.Role = offered.Role; break;
				case queue_preference_field::observer:              next.ObserverReason = offered.ObserverReason; break;
				case queue_preference_field::bucket:                next.Bucket = offered.Bucket; break;
				case queue_preference_field::repository:            next.Repository = offered.Repository; break;
				case queue_preference_field::quiet:                 next.Quiet = offered.Quiet; break;
				case queue_preference_field::sort:                  next.Sort = offered.Sort; break;
				case queue_preference_field::notifications_enabled: next.Notifications.Enabled = offered.Notifications.Enabled; break;
				case queue_preference_field::notifications_paused:  next.Notifications.Paused = offered.Notifications.Paused; break;
				case queue_preference_field::notifications:         next.Notifications = offered.Notifications; break;
				case queue_preference_field::queue_filters:         {
					const auto notifications = next.Notifications;
					next                     = offered;
					next.Notifications       = notifications;
					break;
				}
				default: return;
				}
				aIntent.Preferences = std::move(next);
			}
			if (PreferenceMutationPending) {
				QueuedPreferences = std::move(aIntent.Preferences);
				return;
			}
			if (aIntent.Preferences == Preferences) {
				return;
			}
			++Statistics.PersistenceRequests;
			auto&      request        = begin(request_kind::preferences);
			const auto id             = request.Id;
			const auto stop           = request.Stop.get_token();
			const auto gate           = std::weak_ptr<completion_gate>{ Gate };
			PreferenceMutationPending = true;
			PendingPreferences        = aIntent.Preferences;
			try {
				Effects.post([this,
							  gate,
							  id,
							  stop,
							  preferences = std::move(aIntent.Preferences)]() mutable {
					auto result = Store.store_preferences(preferences, stop);
					DecisionLane.post(
						[gate,
						 id,
						 storedPreferences = std::move(preferences),
						 storeResult       = std::move(result)]() mutable {
							if (auto* const host = locked_host(gate); host != nullptr) {
								host->finish_preferences(
									id,
									std::move(storedPreferences),
									std::move(storeResult));
							}
						});
				});
			} catch (...) {
				PreferenceMutationPending = false;
				PendingPreferences.reset();
				(void)retire(id, request_kind::preferences);
				report_unavailable();
			}
		}

		void finish_preferences(
			const std::uint64_t                             aId,
			queue_presentation_preferences                  aPreferences,
			const std::expected<void, queue_store_failure>& aResult)
		{
			if (!retire(aId, request_kind::preferences) || Closing) {
				return;
			}
			PreferenceMutationPending = false;
			PendingPreferences.reset();
			if (!aResult) {
				QueuedPreferences.reset();
				report_unavailable();
				return;
			}
			Preferences = std::move(aPreferences);
			Failure.reset();
			Status = queue_landing_status::ready;
			publish();
			if (QueuedPreferences.has_value()) {
				auto queued = std::exchange(QueuedPreferences, std::nullopt);
				apply(queue_preferences_intent{
					.Preferences = std::move(*queued),
				});
			}
		}

		void apply(queue_rule_intent aIntent)
		{
			++Statistics.RuleIntents;
			if (Closing || !Restored || !core::valid(aIntent.Rule)) {
				++Statistics.Failures;
				return;
			}
			if (RuleMutationPending) {
				return;
			}
			const auto exists = std::ranges::find(Rules, aIntent.Rule) != Rules.end();
			if ((aIntent.Action == queue_rule_action::add && exists)
				|| (aIntent.Action == queue_rule_action::remove && !exists)) {
				return;
			}
			if (aIntent.Action == queue_rule_action::add && Rules.size() >= maximum_queue_rules) {
				report_collection_limit();
				return;
			}
			auto nextRules       = Rules;
			auto nextEnrollments = Enrollments;
			if (aIntent.Action == queue_rule_action::add) {
				nextRules.push_back(aIntent.Rule);
			} else {
				std::erase(nextRules, aIntent.Rule);
				for (const auto& record : Enrollments.records()) {
					if (record.Membership.MatchedRule
						&& record.Membership.Rule == aIntent.Rule) {
						nextEnrollments.reconcile_rule_change(
							record.Ref,
							core::queue_membership{ .Role = record.Membership.Role });
					}
				}
			}
			if (LastObservation.has_value()) {
				if (!can_enroll(LastObservation->Ref, core::enrollment_for(*LastObservation, nextRules))) {
					return;
				}
				nextEnrollments.reconcile_rule_change(
					LastObservation->Ref,
					core::enrollment_for(*LastObservation, nextRules));
			}
			auto changed = std::vector<enrollment_record>{ };
			for (const auto& record : nextEnrollments.records()) {
				const auto prior = Enrollments.record_for(record.Ref);
				if (!prior.has_value() || *prior != record) {
					changed.push_back(record);
				}
			}
			++Statistics.PersistenceRequests;
			auto&      request  = begin(request_kind::rule_change);
			const auto id       = request.Id;
			const auto stop     = request.Stop.get_token();
			const auto gate     = std::weak_ptr<completion_gate>{ Gate };
			RuleMutationPending = true;
			try {
				Effects.post([this,
							  gate,
							  id,
							  stop,
							  intent             = std::move(aIntent),
							  changedRecords     = std::move(changed),
							  pendingRules       = std::move(nextRules),
							  pendingEnrollments = std::move(nextEnrollments)]() mutable {
					auto result = Store.apply_rule_change(intent, changedRecords, stop);
					DecisionLane.post(
						[gate,
						 id,
						 publishedRules       = std::move(pendingRules),
						 publishedEnrollments = std::move(pendingEnrollments),
						 applyResult          = std::move(result)]() mutable {
							if (auto* const host = locked_host(gate); host != nullptr) {
								host->finish_rule(
									id,
									std::move(publishedRules),
									std::move(publishedEnrollments),
									std::move(applyResult));
							}
						});
				});
			} catch (...) {
				RuleMutationPending = false;
				(void)retire(id, request_kind::rule_change);
				report_unavailable();
			}
		}

		void finish_rule(
			const std::uint64_t                             aId,
			std::vector<core::observer_rule>                aRules,
			queue_enrollment_register                       aEnrollments,
			const std::expected<void, queue_store_failure>& aResult)
		{
			if (!retire(aId, request_kind::rule_change) || Closing) {
				return;
			}
			RuleMutationPending = false;
			if (!aResult) {
				report_unavailable(aResult.error());
				return;
			}
			Rules       = std::move(aRules);
			Enrollments = std::move(aEnrollments);
			Failure.reset();
			Status = queue_landing_status::ready;
			publish();
			// `reconcile_last`, not a bare `refresh_last` for the missing case.
			// The enrollments just adopted were computed when the intent was
			// accepted, against whatever observation existed then. A refresh
			// that completed while this mutation was in flight is judged
			// against the rules as they stood when *it* was dispatched, which
			// did not include this one. Neither side sees the pair, and the
			// pull request stays out of the queue for good.
			//
			// `reconcile_last` re-judges the observation against the rules just
			// committed, and still falls through to `refresh_last` when there
			// is no observation to re-judge, which is what this used to do and
			// all it used to do. BPR-0344.
			reconcile_last();
		}

		[[nodiscard]] auto can_enroll(const core::pull_request_ref& aRef, const core::queue_membership& aMembership) -> bool
		{
			if (aMembership.enrolled() && Enrollments.records().size() >= maximum_queue_enrollments && !Enrollments.record_for(aRef).has_value()) {
				report_collection_limit();
				return false;
			}
			return true;
		}

		void reconcile_last()
		{
			if (Closing) {
				return;
			}
			if (LastObservation.has_value()) {
				auto       prior      = Enrollments;
				const auto membership = core::enrollment_for(*LastObservation, Rules);
				if (!can_enroll(LastObservation->Ref, membership)) {
					return;
				}
				Enrollments.reconcile_rule_change(
					LastObservation->Ref, membership);
				persist_record(LastObservation->Ref, std::move(prior));
			} else {
				refresh_last();
			}
		}

		void adopt(queue_session_id aSession)
		{
			if (Closing || aSession == Scheduler.session()) {
				return;
			}
			cancel_refreshes();
			Addition.reset();
			for (auto& request : Outstanding) {
				if (request.Kind == request_kind::saved_repository) {
					request.Stop.request_stop();
				}
			}
			Scheduler.adopt(std::move(aSession));
			publish();
		}

		void refresh(
			const core::pull_request_ref&    aRef,
			core::queue_activity_observation aActivity)
		{
			LastRef      = aRef;
			LastActivity = aActivity;
			if (Closing || !Restored) {
				if (Restored) {
					publish();
				}
				return;
			}

			auto providerRequest = Scheduler.request_for(aRef);
			++Statistics.RefreshRequests;
			if (providerRequest.Validator.has_value()) {
				++Statistics.ConditionalRequests;
			}
			auto& request = begin(
				request_kind::refresh,
				providerRequest.Session,
				providerRequest.Ref);
			const auto id   = request.Id;
			const auto stop = request.Stop.get_token();
			const auto gate = std::weak_ptr<completion_gate>{ Gate };
			try {
				Effects.post(
					[this,
					 gate,
					 id,
					 stop,
					 activity       = aActivity,
					 refreshRequest = std::move(providerRequest)]() mutable {
						auto completion = Refresh.refresh(refreshRequest, stop);
						DecisionLane.post(
							[gate,
							 id,
							 activity,
							 refreshCompletion = std::move(completion)]() mutable {
								if (auto* const host = locked_host(gate); host != nullptr) {
									host->finish_refresh(
										id, activity, std::move(refreshCompletion));
								}
							});
					});
			} catch (...) {
				(void)retire(id, request_kind::refresh);
				report_unavailable();
			}
		}

		void refresh_last()
		{
			if (LastRef.has_value()) {
				refresh(*LastRef, LastActivity);
			}
		}

		void remember_activity(
			const core::pull_request_ref&           aRef,
			const core::queue_activity_observation& aObservation,
			const core::pull_request_observation&   aRead)
		{
			// [[BPR-0659]]. The title and author travel with the activity so
			// the projection can say them beside the number.
			auto facts = core::queue_item_facts{
				.Title          = aRead.Title,
				.Author         = aRead.Author,
				.UnseenComments = aObservation.UnseenComments,
			};
			const auto found = ActivityIndex.find(aRef);
			if (found == ActivityIndex.end()) {
				ActivityIndex.emplace(aRef, Activity.size());
				Activity.push_back(queue_activity_report{
					.Ref         = aRef,
					.Observation = aObservation,
					.Facts       = std::move(facts),
				});
			} else {
				Activity[found->second].Observation = aObservation;
				Activity[found->second].Facts       = std::move(facts);
			}
		}

		void finish_refresh(
			const std::uint64_t              aId,
			core::queue_activity_observation aActivity,
			queue_refresh_completion         aCompletion)
		{
			if (!claim_refresh(aId, aCompletion)) {
				if (!Closing) {
					publish();
				}
				return;
			}
			if (Closing) {
				return;
			}
			const auto disposition = Scheduler.apply(aCompletion);
			if (disposition == refresh_disposition::stale_session) {
				++Statistics.StaleCompletions;
				publish();
				return;
			}
			if (aCompletion.Status == queue_refresh_status::cancelled) {
				++Statistics.CancelledCompletions;
				publish();
				return;
			}
			if (disposition == refresh_disposition::withdrawn) {
				auto prior = Enrollments;
				Enrollments.observe(aCompletion, disposition);
				persist_record(aCompletion.Ref, std::move(prior));
				return;
			}
			if (disposition != refresh_disposition::applied
				|| !aCompletion.Observation.has_value()) {
				publish();
				return;
			}

			++Statistics.AppliedRefreshes;
			LastObservation = *aCompletion.Observation;
			remember_activity(aCompletion.Ref, aActivity, *aCompletion.Observation);
			auto       membership = core::enrollment_for(*aCompletion.Observation, Rules);
			const auto existing   = Enrollments.record_for(aCompletion.Ref);
			// [[BPR-0989]]. A review the reviewer is seen to have given keeps a
			// discovered pull request here after its request goes. A rule's
			// item still leaves with its rule, as RFC-BPR-0010 decided.
			membership.Reviewed = existing.has_value() && existing->present() && aActivity.ReviewedByMe
				&& (existing->Membership.discovered() || existing->Membership.Reviewed);
			if (existing && existing->present()) {
				auto candidate = queue_enrollment_register{ };
				candidate.enroll(existing->Ref, existing->Membership);
				candidate.enroll(existing->Ref, membership);
				if (membership.enrolled() && candidate.record_for(existing->Ref) == existing) {
					const auto& report = Activity[ActivityIndex.at(existing->Ref)];
					publish(core::queue_item{ .Ref = existing->Ref, .Membership = existing->Membership, .Bucket = core::bucket_for(existing->Membership, report.Observation), .Facts = report.Facts });
					return;
				}
			}
			if (membership.enrolled()) {
				if (!can_enroll(aCompletion.Ref, membership)) {
					return;
				}
				auto prior = Enrollments;
				Enrollments.enroll(aCompletion.Ref, membership);
				persist_record(aCompletion.Ref, std::move(prior));
				return;
			}
			// Discovery, a follow or a review hold a record the rules no longer
			// match; only a record the rules alone held leaves.
			if (existing.has_value() && existing->Membership.Role == core::github_role::none
				&& !existing->Membership.discovered() && !existing->Membership.Followed && !existing->Membership.Reviewed) {
				auto prior = Enrollments;
				Enrollments.depart(
					aCompletion.Ref,
					enrollment_departure::removed_by_reviewer);
				persist_record(aCompletion.Ref, std::move(prior));
				return;
			}
			if (existing && existing->present()) {
				const auto& report = Activity[ActivityIndex.at(existing->Ref)];
				publish(core::queue_item{ .Ref = existing->Ref, .Membership = existing->Membership, .Bucket = core::bucket_for(existing->Membership, report.Observation), .Facts = report.Facts });
			} else {
				publish();
			}
		}

		/// [[BPR-0989]]. What a discovery answer read about each pull request,
		/// merged into the activity an open may already have reported: the
		/// title, author and awaited reviewers are discovery's; unseen comments
		/// and review state stay what the open read.
		void remember_discovery(const queue_discovery_answer& aAnswer)
		{
			for (const auto& found : aAnswer.PullRequests) {
				const auto ref      = core::pull_request_ref{ .Host = aAnswer.Session.Host, .Account = aAnswer.Session.Account, .Repository = aAnswer.Repository, .Number = found.Number };
				const auto known    = aAnswer.Requested || found.Detailed;
				const auto position = ActivityIndex.find(ref);
				if (position == ActivityIndex.end()) {
					ActivityIndex.emplace(ref, Activity.size());
					Activity.push_back(queue_activity_report{
						.Ref         = ref,
						.Observation = { .ReviewersAwaited = found.ReviewersAwaited, .Known = known },
						.Facts       = { .Title = found.Title, .Author = found.Author, .UnseenComments = 0U },
					});
					continue;
				}
				auto* existing         = &Activity[position->second];
				existing->Facts.Title  = found.Title;
				existing->Facts.Author = found.Author;
				if (found.Detailed) {
					existing->Observation.ReviewersAwaited = found.ReviewersAwaited;
				}
				existing->Observation.Known = existing->Observation.Known || known;
			}
		}

		void apply(queue_discovery_answer aAnswer)
		{
			++Statistics.DiscoveryAnswers;
			if (Closing || !Restored) {
				return;
			}
			// An answer read under another account or an earlier session is
			// about someone else's queue.
			const auto& session = Scheduler.session();
			if (aAnswer.Session != session || aAnswer.Session.Host.empty() || aAnswer.Session.Account.empty()) {
				++Statistics.StaleDiscoveryAnswers;
				return;
			}
			auto       prior   = Enrollments;
			const auto changed = Enrollments.observe_discovery(aAnswer);
			if (Enrollments.records().size() > maximum_queue_enrollments) {
				Enrollments = std::move(prior);
				report_collection_limit();
				return;
			}
			remember_discovery(aAnswer);
			persist_records(changed, std::move(prior));
		}

		void apply(queue_metadata_answer aAnswer)
		{
			if (Closing || !Restored || aAnswer.Session != Scheduler.session()
				|| aAnswer.Ref.Host != aAnswer.Session.Host || aAnswer.Ref.Account != aAnswer.Session.Account) {
				return;
			}
			const auto existing = Enrollments.record_for(aAnswer.Ref);
			if (!existing || !existing->present()) {
				return;
			}
			auto prior = Enrollments;
			if (aAnswer.Closed) {
				Enrollments.depart(aAnswer.Ref, enrollment_departure::closed);
				persist_record(aAnswer.Ref, std::move(prior));
				return;
			}
			const auto position = ActivityIndex.find(aAnswer.Ref);
			if (position == ActivityIndex.end()) {
				ActivityIndex.emplace(aAnswer.Ref, Activity.size());
				Activity.push_back({ .Ref = aAnswer.Ref, .Observation = aAnswer.Activity, .Facts = std::move(aAnswer.Facts) });
			} else {
				auto& report                              = Activity[position->second];
				report.Facts.Title                        = std::move(aAnswer.Facts.Title);
				report.Facts.Author                       = std::move(aAnswer.Facts.Author);
				report.Observation.ReviewedByMe           = aAnswer.Activity.ReviewedByMe;
				report.Observation.HeadMovedSinceMyReview = aAnswer.Activity.HeadMovedSinceMyReview;
				report.Observation.ReviewersAwaited       = aAnswer.Activity.ReviewersAwaited;
				report.Observation.Known                  = aAnswer.Activity.Known;
			}
			// Read the submitted review before discovery can withdraw the old
			// request: it keeps a discovered PR in Following when that request goes.
			if (aAnswer.Activity.ReviewedByMe && !existing->Membership.Reviewed
				&& (existing->Membership.discovered() || existing->Membership.Role == core::github_role::requested_reviewer)) {
				Enrollments.enroll(aAnswer.Ref, { .Reviewed = true });
				persist_record(aAnswer.Ref, std::move(prior));
				return;
			}
			publish();
		}

		void apply(queue_follow_intent aIntent)
		{
			++Statistics.FollowIntents;
			if (Closing || !Restored || aIntent.Ref.Host.empty() || aIntent.Ref.Account.empty() || aIntent.Ref.Repository.empty() || aIntent.Ref.Number <= 0) {
				++Statistics.Failures;
				return;
			}
			auto prior = Enrollments;
			if (!Enrollments.follow(aIntent.Ref, aIntent.Follow)) {
				return;
			}
			if (Enrollments.records().size() > maximum_queue_enrollments) {
				Enrollments = std::move(prior);
				report_collection_limit();
				return;
			}
			if (aIntent.Follow && !aIntent.Title.empty() && !ActivityIndex.contains(aIntent.Ref)) {
				ActivityIndex.emplace(aIntent.Ref, Activity.size());
				Activity.push_back(queue_activity_report{ .Ref = aIntent.Ref, .Observation = { .Known = false }, .Facts = { .Title = aIntent.Title } });
			}
			persist_record(aIntent.Ref, std::move(prior));
		}

		void present_discovery(queue_discovery_status aStatus)
		{
			if (Closing || Discovery == aStatus) {
				return;
			}
			Discovery = std::move(aStatus);
			publish();
		}

		void persist_records(const std::vector<core::pull_request_ref>& aRefs, queue_enrollment_register aPrior)
		{
			auto records = std::vector<enrollment_record>{ };
			for (const auto& ref : aRefs) {
				if (const auto record = Enrollments.record_for(ref); record.has_value()) {
					records.push_back(*record);
				}
			}
			if (records.empty()) {
				publish();
				return;
			}
			++Statistics.PersistenceRequests;
			auto&      request  = begin(request_kind::store_enrollments);
			const auto id       = request.Id;
			const auto stop     = request.Stop.get_token();
			const auto gate     = std::weak_ptr<completion_gate>{ Gate };
			const auto rollback = std::make_shared<queue_enrollment_register>(std::move(aPrior));
			try {
				Effects.post([this, gate, id, stop, stored = std::move(records), rollback] {
					auto result = Store.store_enrollments(stored, stop);
					DecisionLane.post([gate, id, rollback, storeResult = std::move(result)]() mutable {
						if (auto* const host = locked_host(gate); host != nullptr) {
							host->finish_store_enrollments(id, rollback, std::move(storeResult));
						}
					});
				});
			} catch (...) {
				(void)retire(id, request_kind::store_enrollments);
				Enrollments = *rollback;
				report_unavailable();
			}
		}

		void finish_store_enrollments(
			const std::uint64_t                               aId,
			const std::shared_ptr<queue_enrollment_register>& aRollback,
			const std::expected<void, queue_store_failure>&   aResult)
		{
			if (!retire(aId, request_kind::store_enrollments) || Closing) {
				return;
			}
			if (!aResult) {
				Enrollments = *aRollback;
				report_unavailable(aResult.error());
				return;
			}
			Failure.reset();
			Status = queue_landing_status::ready;
			publish();
		}

		void persist_record(
			const core::pull_request_ref& aRef,
			queue_enrollment_register     aPrior)
		{
			const auto record = Enrollments.record_for(aRef);
			if (!record.has_value()) {
				publish();
				return;
			}
			++Statistics.PersistenceRequests;
			auto&      request  = begin(request_kind::store_enrollment);
			const auto id       = request.Id;
			const auto stop     = request.Stop.get_token();
			const auto gate     = std::weak_ptr<completion_gate>{ Gate };
			const auto rollback = std::make_shared<queue_enrollment_register>(
				std::move(aPrior));
			try {
				Effects.post([this, gate, id, stop, enrollmentRecord = *record, rollback] {
					auto result = Store.store_enrollment(enrollmentRecord, stop);
					DecisionLane.post(
						[gate, id, rollback, storeResult = std::move(result)]() mutable {
							if (auto* const host = locked_host(gate); host != nullptr) {
								host->finish_store_enrollment(
									id, rollback, std::move(storeResult));
							}
						});
				});
			} catch (...) {
				(void)retire(id, request_kind::store_enrollment);
				Enrollments = *rollback;
				report_unavailable();
			}
		}

		void finish_store_enrollment(
			const std::uint64_t                               aId,
			const std::shared_ptr<queue_enrollment_register>& aRollback,
			const std::expected<void, queue_store_failure>&   aResult)
		{
			if (!retire(aId, request_kind::store_enrollment) || Closing) {
				return;
			}
			if (!aResult) {
				Enrollments = *aRollback;
				report_unavailable(aResult.error());
				return;
			}
			Failure.reset();
			Status = queue_landing_status::ready;
			publish();
		}

		void cancel_refreshes()
		{
			for (auto& request : Outstanding) {
				if (request.Kind == request_kind::refresh) {
					request.Cancelled = true;
					request.Stop.request_stop();
				}
			}
		}

		void shutdown()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			Projection.shutdown();
			for (auto& request : Outstanding) {
				request.Stop.request_stop();
			}
		}

		queue_enrollment_store_port&                  Store;
		queue_refresh_effect_port&                    Refresh;
		effect_executor&                              Effects;
		decision_executor&                            DecisionLane;
		queue_projection_hub                          Projection;
		queue_refresh_scheduler                       Scheduler;
		queue_enrollment_register                     Enrollments;
		std::vector<core::observer_rule>              Rules;
		queue_presentation_preferences                Preferences;
		std::vector<saved_repository>                 SavedRepositories;
		std::vector<saved_server>                     SavedServers;
		std::optional<queue_presentation_preferences> QueuedPreferences;
		std::optional<queue_presentation_preferences> PendingPreferences;
		std::vector<queue_activity_report>            Activity;
		std::map<core::pull_request_ref, std::size_t> ActivityIndex;
		std::vector<outstanding_request>              Outstanding;
		std::optional<core::pull_request_ref>         LastRef;
		std::optional<core::pull_request_observation> LastObservation;
		core::queue_activity_observation              LastActivity;
		queue_landing_status                          Status = queue_landing_status::restoring;
		review_queue_host_statistics                  Statistics;
		std::optional<queue_store_failure>            Failure;
		queue_discovery_status                        Discovery;
		std::optional<repository_addition_result>     Addition;
		bool                                          DurableStateLoaded = false;
		std::shared_ptr<completion_gate>              Gate;
		std::uint64_t                                 NextRequest                = 1U;
		bool                                          Restored                   = false;
		bool                                          Closing                    = false;
		bool                                          RuleMutationPending        = false;
		bool                                          PreferenceMutationPending  = false;
		bool                                          SavedMutationPending       = false;
		bool                                          SavedServerMutationPending = false;
	};

	review_queue_application_host::review_queue_application_host(
		queue_enrollment_store_port&   aStore,
		queue_refresh_effect_port&     aRefresh,
		effect_executor&               aEffects,
		decision_executor&             aDecisionLane,
		queue_landing_projection_port& aProjection,
		queue_session_id               aSession)
		: Implementation{ std::make_unique<implementation>(
			  aStore,
			  aRefresh,
			  aEffects,
			  aDecisionLane,
			  aProjection,
			  std::move(aSession)) }
	{
	}

	review_queue_application_host::review_queue_application_host(queue_enrollment_store_port& aStore, queue_refresh_effect_port& aRefresh,
																 effect_executor& aEffects, decision_executor& aDecisionLane, core::bpr_profile_id aProfile,
																 queue_window_context_port& aWindows, queue_session_id aSession, std::shared_ptr<queue_landing_projection_port> aProfileObserver)
		: Implementation{ std::make_unique<implementation>(aStore, aRefresh, aEffects, aDecisionLane, aProfile, aWindows, std::move(aSession), std::move(aProfileObserver)) }
	{ }

	auto review_queue_application_host::attach(core::bpr_window_context aContext, std::shared_ptr<queue_window_projection_port> aProjection)
		-> std::expected<queue_consumer_id, queue_attachment_failure>
	{
		return Implementation->Projection.attach(aContext, std::move(aProjection));
	}
	void review_queue_application_host::detach(queue_consumer_id aConsumer) { Implementation->Projection.detach(aConsumer); }
	auto review_queue_application_host::attached_consumers() const noexcept -> std::size_t { return Implementation->Projection.size(); }
	auto review_queue_application_host::projection_delivery_failed() const noexcept -> bool { return Implementation->Projection.failed(); }

	review_queue_application_host::~review_queue_application_host() = default;

	void review_queue_application_host::restore()
	{
		Implementation->restore();
	}

	void review_queue_application_host::apply(queue_rule_intent aIntent)
	{
		Implementation->apply(std::move(aIntent));
	}

	void review_queue_application_host::apply(queue_preferences_intent aIntent)
	{
		Implementation->apply(std::move(aIntent));
	}

	void review_queue_application_host::apply(saved_server_intent aIntent)
	{
		Implementation->apply(std::move(aIntent));
	}

	auto review_queue_application_host::saved_servers() const noexcept
		-> std::span<const saved_server>
	{
		return Implementation->SavedServers;
	}

	void review_queue_application_host::apply(saved_repository_intent aIntent)
	{
		Implementation->apply(std::move(aIntent));
	}

	void review_queue_application_host::apply(repository_addition_intent aIntent)
	{
		Implementation->apply(std::move(aIntent));
	}

	auto review_queue_application_host::saved_repositories() const noexcept
		-> std::span<const saved_repository>
	{
		return Implementation->SavedRepositories;
	}

	void review_queue_application_host::apply(queue_discovery_answer aAnswer)
	{
		Implementation->apply(std::move(aAnswer));
	}

	void review_queue_application_host::apply(queue_follow_intent aIntent)
	{
		Implementation->apply(std::move(aIntent));
	}

	void review_queue_application_host::apply(queue_metadata_answer aAnswer)
	{
		Implementation->apply(std::move(aAnswer));
	}

	void review_queue_application_host::present_discovery(queue_discovery_status aStatus)
	{
		Implementation->present_discovery(std::move(aStatus));
	}

	auto review_queue_application_host::enrollments() const noexcept -> std::span<const enrollment_record>
	{
		return Implementation->Enrollments.records();
	}

	auto review_queue_application_host::session() const noexcept -> const queue_session_id&
	{
		return Implementation->Scheduler.session();
	}

	void review_queue_application_host::adopt(queue_session_id aSession)
	{
		Implementation->adopt(std::move(aSession));
	}

	void review_queue_application_host::refresh(
		const core::pull_request_ref&    aRef,
		core::queue_activity_observation aActivity)
	{
		Implementation->refresh(aRef, aActivity);
	}

	void review_queue_application_host::cancel()
	{
		Implementation->cancel_refreshes();
	}

	void review_queue_application_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto review_queue_application_host::rules() const noexcept
		-> std::span<const core::observer_rule>
	{
		return Implementation->Rules;
	}

	auto review_queue_application_host::preferences() const noexcept -> const queue_presentation_preferences&
	{
		return Implementation->Preferences;
	}

	auto review_queue_application_host::queue() const
		-> std::vector<core::queue_item>
	{
		return Implementation->Enrollments.project(Implementation->Activity);
	}

	auto review_queue_application_host::statistics() const noexcept
		-> review_queue_host_statistics
	{
		return Implementation->Statistics;
	}

	auto review_queue_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Outstanding.empty();
	}
} // namespace bpr::application
