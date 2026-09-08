#include "bpr/application/review_queue_enrollment.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <memory>
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
			|| *aPreferences.Bucket <= core::attention_bucket::quiet;
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
		}
		return "This is in your queue.";
	}

	auto queue_enrollment_register::find(const core::pull_request_ref& aRef)
		-> enrollment_record*
	{
		const auto found
			= std::ranges::find(Records, aRef, &enrollment_record::Ref);
		return found == Records.end() ? nullptr : &*found;
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
			existing->Departed = enrollment_departure::none;
			return;
		}
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
			existing->Membership = aMembership;
			existing->Departed   = aMembership.enrolled()
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

	auto queue_enrollment_register::records() const noexcept
		-> std::span<const enrollment_record>
	{
		return Records;
	}

	auto queue_enrollment_register::record_for(
		const core::pull_request_ref& aRef) const
		-> std::optional<enrollment_record>
	{
		const auto found
			= std::ranges::find(Records, aRef, &enrollment_record::Ref);
		if (found == Records.end()) {
			return std::nullopt;
		}
		return *found;
	}

	auto queue_enrollment_register::project(
		const std::span<const queue_activity_report> aActivity) const
		-> std::vector<core::queue_item>
	{
		auto items = std::vector<core::queue_item>{ };
		items.reserve(Records.size());
		for (const auto& record : Records) {
			if (!record.present()) {
				continue;
			}
			// A missing report is not an error and must not be guessed at. The
			// host supplies what it read; an item it has not read yet is
			// quiet, which is what `bucket_for` says about an empty one.
			const auto found = std::ranges::find(
				aActivity, record.Ref, &queue_activity_report::Ref);
			const auto activity  = found == aActivity.end()
				? core::queue_activity_observation{ }
				: found->Observation;
			auto       facts     = found == aActivity.end() ? core::queue_item_facts{ } : found->Facts;
			facts.UnseenComments = activity.UnseenComments;
			items.push_back(core::queue_item{
				.Ref        = record.Ref,
				.Membership = record.Membership,
				.Bucket     = core::bucket_for(record.Membership, activity),
				.Facts      = std::move(facts),
			});
		}
		return core::ordered_queue(items);
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

		void report_unavailable()
		{
			++Statistics.Failures;
			Status = queue_landing_status::unavailable;
			publish();
		}

		void publish()
		{
			Projection.publish(queue_landing_projection{
				.Status            = Status,
				.Rules             = Rules,
				.Items             = Enrollments.project(Activity),
				.Preferences       = Preferences,
				.Session           = Scheduler.session(),
				.Online            = Scheduler.online(),
				.BackoffSeconds    = Scheduler.backoff_seconds(),
				.SavedRepositories = SavedRepositories,
				.SavedServers      = SavedServers,
			});
			++Statistics.Publications;
		}

		void restore()
		{
			if (Closing || Restored) {
				return;
			}
			Status = queue_landing_status::restoring;
			publish();
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
				report_unavailable();
				return;
			}
			Rules             = std::move(aResult->Rules);
			Preferences       = std::move(aResult->Preferences);
			SavedRepositories = std::move(aResult->SavedRepositories);
			SavedServers      = std::move(aResult->SavedServers);
			for (const auto& record : aResult->Enrollments) {
				Enrollments.enroll(record.Ref, record.Membership);
				if (!record.present()) {
					Enrollments.depart(record.Ref, record.Departed);
				}
			}
			Status = queue_landing_status::ready;
			publish();
			reconcile_last();
		}

		// [[BPR-0659]]. A save or forget is durable before it is shown: the
		// store answers on the effect lane and the decision lane adopts the
		// list it was asked for, so a refused write leaves the list as it was.
		void apply(saved_repository_intent aIntent)
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
			try {
				Effects.post([this, gate, id, stop, intent = std::move(aIntent), pending = std::move(next)]() mutable {
					auto result = Store.apply_saved_repository_change(intent, stop);
					DecisionLane.post(
						[gate, id, published = std::move(pending), applyResult = std::move(result)]() mutable {
							if (auto* const host = locked_host(gate); host != nullptr) {
								host->finish_saved_repository(id, std::move(published), std::move(applyResult));
							}
						});
				});
			} catch (...) {
				SavedMutationPending = false;
				(void)retire(id, request_kind::saved_repository);
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
				report_unavailable();
				return;
			}
			SavedServers = std::move(aSaved);
			Status       = queue_landing_status::ready;
			publish();
		}
		void finish_saved_repository(
			const std::uint64_t                             aId,
			std::vector<saved_repository>                   aSaved,
			const std::expected<void, queue_store_failure>& aResult)
		{
			if (!retire(aId, request_kind::saved_repository) || Closing) {
				return;
			}
			SavedMutationPending = false;
			if (!aResult) {
				report_unavailable();
				return;
			}
			SavedRepositories = std::move(aSaved);
			Status            = queue_landing_status::ready;
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
			Status      = queue_landing_status::ready;
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
				report_unavailable();
				return;
			}
			Rules       = std::move(aRules);
			Enrollments = std::move(aEnrollments);
			Status      = queue_landing_status::ready;
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

		void reconcile_last()
		{
			if (LastObservation.has_value()) {
				auto       prior      = Enrollments;
				const auto membership = core::enrollment_for(*LastObservation, Rules);
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
			const auto found = std::ranges::find(
				Activity, aRef, &queue_activity_report::Ref);
			if (found == Activity.end()) {
				Activity.push_back(queue_activity_report{
					.Ref         = aRef,
					.Observation = aObservation,
					.Facts       = std::move(facts),
				});
			} else {
				found->Observation = aObservation;
				found->Facts       = std::move(facts);
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
			const auto membership = core::enrollment_for(
				*aCompletion.Observation, Rules);
			if (membership.enrolled()) {
				auto prior = Enrollments;
				Enrollments.enroll(aCompletion.Ref, membership);
				persist_record(aCompletion.Ref, std::move(prior));
				return;
			}
			const auto existing = Enrollments.record_for(aCompletion.Ref);
			if (existing.has_value() && existing->Membership.Role == core::github_role::none) {
				auto prior = Enrollments;
				Enrollments.depart(
					aCompletion.Ref,
					enrollment_departure::removed_by_reviewer);
				persist_record(aCompletion.Ref, std::move(prior));
				return;
			}
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
				report_unavailable();
				return;
			}
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
			for (auto& request : Outstanding) {
				request.Stop.request_stop();
			}
		}

		queue_enrollment_store_port&                  Store;
		queue_refresh_effect_port&                    Refresh;
		effect_executor&                              Effects;
		decision_executor&                            DecisionLane;
		queue_landing_projection_port&                Projection;
		queue_refresh_scheduler                       Scheduler;
		queue_enrollment_register                     Enrollments;
		std::vector<core::observer_rule>              Rules;
		queue_presentation_preferences                Preferences;
		std::vector<saved_repository>                 SavedRepositories;
		std::vector<saved_server>                     SavedServers;
		std::optional<queue_presentation_preferences> QueuedPreferences;
		std::optional<queue_presentation_preferences> PendingPreferences;
		std::vector<queue_activity_report>            Activity;
		std::vector<outstanding_request>              Outstanding;
		std::optional<core::pull_request_ref>         LastRef;
		std::optional<core::pull_request_observation> LastObservation;
		core::queue_activity_observation              LastActivity;
		queue_landing_status                          Status = queue_landing_status::restoring;
		review_queue_host_statistics                  Statistics;
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

	auto review_queue_application_host::saved_repositories() const noexcept
		-> std::span<const saved_repository>
	{
		return Implementation->SavedRepositories;
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
