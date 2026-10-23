#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/core/discovery_diagnosis.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/home_discovery_plan.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto locator_of(const std::string_view aRepository) -> std::optional<core::repository_locator>
		{
			const auto slash = aRepository.find('/');
			if (slash == std::string_view::npos || slash == 0U || slash + 1U >= aRepository.size()) {
				return std::nullopt;
			}
			auto locator = core::repository_locator{ .Owner = std::string{ aRepository.substr(0U, slash) }, .Name = std::string{ aRepository.substr(slash + 1U) } };
			return core::valid(locator) ? std::optional{ std::move(locator) } : std::nullopt;
		}

		[[nodiscard]] auto problem_of(const personal_discovery_completeness aCompleteness) noexcept -> queue_discovery_problem
		{
			switch (aCompleteness) {
			case personal_discovery_completeness::complete:           return queue_discovery_problem::none;
			case personal_discovery_completeness::incomplete_results: return queue_discovery_problem::incomplete_results;
			case personal_discovery_completeness::result_limit:       return queue_discovery_problem::result_limit;
			case personal_discovery_completeness::detail_limit:       return queue_discovery_problem::detail_limit;
			}
			return queue_discovery_problem::failed;
		}

		/// [[BPR-1718]]. A submitted review's state as the core's verdict.
		[[nodiscard]] auto my_review_verdict_of(const github_review_state aState) noexcept -> core::my_review_verdict
		{
			switch (aState) {
			case github_review_state::approved:          return core::my_review_verdict::approved;
			case github_review_state::changes_requested: return core::my_review_verdict::changes_requested;
			case github_review_state::dismissed:         return core::my_review_verdict::dismissed;
			case github_review_state::commented:
			case github_review_state::pending:
			case github_review_state::uninitialized:     break;
			}
			return core::my_review_verdict::commented;
		}

		/// Whether a failure is about the whole session rather than one
		/// repository, so the rest of the refresh would fail the same way.
		[[nodiscard]] auto stops_refresh(const github_provider_failure aFailure) noexcept -> bool
		{
			switch (aFailure) {
			case github_provider_failure::rate_limit:
			case github_provider_failure::authentication:
			case github_provider_failure::network:
			case github_provider_failure::timeout:
			case github_provider_failure::certificate:
			case github_provider_failure::redirect:
			case github_provider_failure::process_startup:
			case github_provider_failure::cli_unavailable:
			case github_provider_failure::vault:
				return true;
			case github_provider_failure::permission:
			case github_provider_failure::api_version:
			case github_provider_failure::unsupported_capability:
			case github_provider_failure::malformed_response:
			case github_provider_failure::response_limit:
			case github_provider_failure::unavailable:
				return false;
			}
			return true;
		}

		/// [[BPR-1680]]. The diagnostics log's name for a read's failure.
		[[nodiscard]] auto diagnostic_failure(const github_provider_failure aFailure) noexcept -> core::github_failure
		{
			switch (aFailure) {
			case github_provider_failure::authentication:         return core::github_failure::authentication;
			case github_provider_failure::permission:             return core::github_failure::permission;
			case github_provider_failure::network:
			case github_provider_failure::process_startup:
			case github_provider_failure::unavailable:            return core::github_failure::network;
			case github_provider_failure::api_version:            return core::github_failure::api_version;
			case github_provider_failure::unsupported_capability: return core::github_failure::capability;
			case github_provider_failure::rate_limit:             return core::github_failure::rate_limit;
			case github_provider_failure::certificate:            return core::github_failure::certificate;
			case github_provider_failure::redirect:               return core::github_failure::redirect;
			case github_provider_failure::timeout:                return core::github_failure::timeout;
			case github_provider_failure::cli_unavailable:        return core::github_failure::cli_unavailable;
			case github_provider_failure::vault:                  return core::github_failure::vault;
			case github_provider_failure::malformed_response:     return core::github_failure::malformed_response;
			case github_provider_failure::response_limit:         return core::github_failure::response_too_large;
			}
			return core::github_failure::malformed_response;
		}

		[[nodiscard]] auto diagnosis_reason(const personal_discovery_reason aReason) noexcept -> core::discovery_diagnosis_reason
		{
			switch (aReason) {
			case personal_discovery_reason::authored:  return core::discovery_diagnosis_reason::authored;
			case personal_discovery_reason::requested: return core::discovery_diagnosis_reason::requested;
			case personal_discovery_reason::retained:  return core::discovery_diagnosis_reason::retained;
			}
			return core::discovery_diagnosis_reason::none;
		}

		[[nodiscard]] auto saturated(const std::uint64_t aValue) noexcept -> std::uint32_t
		{
			return static_cast<std::uint32_t>(std::min<std::uint64_t>(aValue, std::numeric_limits<std::uint32_t>::max()));
		}

		[[nodiscard]] auto cost_of(const personal_discovery_completion& aCompletion) noexcept -> personal_discovery_cost
		{
			return std::visit([](const auto& aResult) { return aResult.Cost; }, aCompletion);
		}

		[[nodiscard]] auto problem_of(const github_provider_error& aError) noexcept -> queue_discovery_problem
		{
			switch (aError.Failure) {
			case github_provider_failure::rate_limit:
				return aError.RateLimitCause.has_value() && aError.RateLimitCause->SecondaryLimited
					? queue_discovery_problem::secondary_rate_limited
					: queue_discovery_problem::rate_limited;
			case github_provider_failure::authentication:
			case github_provider_failure::vault:
			case github_provider_failure::cli_unavailable:
				return queue_discovery_problem::authentication;
			case github_provider_failure::network:
			case github_provider_failure::timeout:
			case github_provider_failure::certificate:
			case github_provider_failure::redirect:
			case github_provider_failure::process_startup:
				return queue_discovery_problem::offline;
			case github_provider_failure::permission:
				return queue_discovery_problem::repository_unavailable;
			case github_provider_failure::api_version:
			case github_provider_failure::unsupported_capability:
			case github_provider_failure::malformed_response:
			case github_provider_failure::response_limit:
			case github_provider_failure::unavailable:
				return queue_discovery_problem::failed;
			}
			return queue_discovery_problem::failed;
		}
	} // namespace

	auto personal_discovery_session_of(const github_profile_session& aSession)
		-> std::optional<personal_discovery_session>
	{
		const auto& viewer   = aSession.Session.Viewer;
		const auto  hostname = std::string_view{ aSession.ProfileAuthority.Hostname.value() };
		if (aSession.Session.Mode != core::github_access_mode::authenticated || hostname.empty() || viewer.Login.empty()
			|| viewer.Account.value() == 0U || aSession.Generation.value() == 0U) {
			return std::nullopt;
		}
		return personal_discovery_session{
			.Queue   = queue_session_id{ .Host = std::string{ hostname }, .Account = viewer.Login, .Generation = aSession.Generation.value(), .ViewerAccount = viewer.Account },
			.Context = core::repository_request_context{
				.ApplicationSession = aSession.ApplicationSession,
				.Profile            = aSession.ProfileAuthority.Profile,
				.Host               = aSession.ProfileAuthority.Host,
				.Account            = viewer.Account,
				.Repository         = core::repository_locator{ },
				.Capabilities       = aSession.CapabilityEvidence.Revision,
				.IdentityGeneration = aSession.Generation,
			},
			.ProfileAuthority      = aSession.ProfileAuthority,
			.Viewer                = viewer,
			.CapabilityEvidence    = aSession.CapabilityEvidence,
			.AccountTransport      = aSession.AccountTransport,
			.AuthenticationSession = aSession.ApplicationSession,
		};
	}

	struct personal_discovery_host::implementation final : std::enable_shared_from_this<implementation>
	{
		struct job
		{
			personal_discovery_request Request;
			std::string                Repository;
			/// [[BPR-1680]]. A search: every saved repository it names, in the
			/// request's order, the first being `Repository`.
			std::vector<std::string> Repositories;
			std::uint64_t            Generation = 0U;
			std::uint64_t            Cycle      = 0U;
			/// [[BPR-1679]]. Cancelled to give way; asked again if that is
			/// what answers it.
			bool Preempted = false;
			/// [[BPR-1680]]. The read's number within its refresh, from 1.
			std::uint32_t Read = 0U;
		};

		implementation(personal_discovery_port& aProvider, decision_executor& aDecisionLane,
					   review_queue_application_host& aQueue, const personal_discovery_clock_port& aClock,
					   diagnostic_log_port* aLog, const personal_discovery_options aOptions)
			: Provider{ aProvider }
			, DecisionLane{ aDecisionLane }
			, Queue{ aQueue }
			, Clock{ aClock }
			, Log{ aLog }
			, Options{ aOptions }
		{
			Status.State   = queue_discovery_state::unavailable;
			Status.Problem = queue_discovery_problem::no_account;
		}

		void publish()
		{
			Queue.present_discovery(Status);
		}

		/// [[BPR-1680]]. One `github.discovery` event. Logging never changes
		/// what discovery does.
		void record(const diagnostic_outcome aOutcome, const core::discovery_diagnosis& aDiagnosis,
					const diagnostic_detail aDetail = diagnostic_detail::standard, const std::optional<github_provider_failure> aFailure = std::nullopt) noexcept
		{
			if (Log == nullptr) {
				return;
			}
			try {
				const auto text  = core::describe(aDiagnosis);
				auto       event = diagnostic_event{ };
				if (text.empty() || text.size() > event.Summary.Text.size()) {
					return;
				}
				event.Operation = diagnostic_operation::discovery;
				event.Outcome   = aOutcome;
				event.Detail    = aDetail;
				std::ranges::copy(text, event.Summary.Text.begin());
				event.Summary.Size  = static_cast<std::uint16_t>(text.size());
				event.Summary.State = diagnostic_summary_state::sanitized;
				if (aFailure) {
					event.GithubFailure = diagnostic_failure(*aFailure);
				}
				static_cast<void>(Log->record(event));
			} catch (...) {
				// A diagnostic that cannot be formed is dropped.
			}
		}

		[[nodiscard]] auto refresh_diagnosis(const core::discovery_diagnosis_state aState) const -> core::discovery_diagnosis
		{
			return core::discovery_diagnosis{
				.Scope                  = core::discovery_diagnosis_scope::refresh,
				.Refresh                = saturated(Cycle),
				.State                  = aState,
				.Repositories           = saturated(Searched.size()),
				.PullRequests           = CycleRetained,
				.Reads                  = CycleReads,
				.Failed                 = CycleFailedReads,
				.Requests               = CycleRequests,
				.WaitMilliseconds       = CycleWait,
				.RunMilliseconds        = saturated(static_cast<std::uint64_t>(std::max<std::int64_t>(Clock.elapsed_milliseconds() - CycleStarted, 0))),
				.CredentialMilliseconds = CycleCost.CredentialMilliseconds,
				.WireMilliseconds       = CycleCost.WireMilliseconds,
				.DecisionMilliseconds   = CycleDecision,
				.Forbidden              = CycleCost.Forbidden,
				.Throttled              = CycleCost.Throttled,
				.ServerErrors           = CycleCost.ServerErrors,
				.Timeouts               = CycleCost.Timeouts,
			};
		}

		[[nodiscard]] static auto read_diagnosis(const job& aJob) -> core::discovery_diagnosis
		{
			return core::discovery_diagnosis{
				.Scope        = core::discovery_diagnosis_scope::read,
				.Refresh      = saturated(aJob.Cycle),
				.Read         = aJob.Read,
				.Reason       = diagnosis_reason(aJob.Request.Reason),
				.Repositories = saturated(std::max<std::size_t>(aJob.Repositories.size(), 1U)),
				.PullRequests = saturated(aJob.Request.Numbers.empty() ? (aJob.Request.Number ? 1U : 0U) : aJob.Request.Numbers.size()),
			};
		}

		/// [[BPR-1680]]. A read's end, with what it says it cost and
		/// ([[BPR-1744]]) how long its answer waited for this lane.
		void record_read(const job& aJob, const personal_discovery_completion& aCompletion, const std::uint32_t aDecisionMilliseconds)
		{
			const auto cost                  = cost_of(aCompletion);
			auto       diagnosis             = read_diagnosis(aJob);
			diagnosis.Requests               = cost.Requests;
			diagnosis.WaitMilliseconds       = cost.WaitMilliseconds;
			diagnosis.RunMilliseconds        = cost.RunMilliseconds;
			diagnosis.CredentialMilliseconds = cost.CredentialMilliseconds;
			diagnosis.WireMilliseconds       = cost.WireMilliseconds;
			diagnosis.DecisionMilliseconds   = aDecisionMilliseconds;
			diagnosis.Forbidden              = cost.Forbidden;
			diagnosis.Throttled              = cost.Throttled;
			diagnosis.ServerErrors           = cost.ServerErrors;
			diagnosis.Timeouts               = cost.Timeouts;
			Statistics.Requests += cost.Requests;
			const auto* const error = std::get_if<personal_discovery_error>(&aCompletion);
			if (current(aJob)) {
				const auto add = [](std::uint32_t& aTotal, const std::uint32_t aValue) { aTotal = saturated(std::uint64_t{ aTotal } + aValue); };
				add(CycleRequests, cost.Requests);
				add(CycleWait, cost.WaitMilliseconds);
				add(CycleCost.CredentialMilliseconds, cost.CredentialMilliseconds);
				add(CycleCost.WireMilliseconds, cost.WireMilliseconds);
				add(CycleCost.Forbidden, cost.Forbidden);
				add(CycleCost.Throttled, cost.Throttled);
				add(CycleCost.ServerErrors, cost.ServerErrors);
				add(CycleCost.Timeouts, cost.Timeouts);
				add(CycleDecision, aDecisionMilliseconds);
				CycleFailedReads += error != nullptr ? 1U : 0U;
			}
			const auto outcome = error != nullptr ? diagnostic_outcome::failed
				: std::holds_alternative<personal_discovery_cancelled>(aCompletion)
				? diagnostic_outcome::cancelled
				: diagnostic_outcome::succeeded;
			record(outcome, diagnosis, diagnostic_detail::standard, error != nullptr ? std::optional{ error->Error.Failure } : std::nullopt);
		}

		[[nodiscard]] auto ended_state() const noexcept -> core::discovery_diagnosis_state
		{
			if (CycleStopped) {
				return core::discovery_diagnosis_state::stopped;
			}
			if (CycleFailed) {
				return core::discovery_diagnosis_state::stale;
			}
			return CycleProblem == queue_discovery_problem::none ? core::discovery_diagnosis_state::current : core::discovery_diagnosis_state::partial;
		}

		void cancel_all()
		{
			if (CycleRunning) {
				record(diagnostic_outcome::cancelled, refresh_diagnosis(core::discovery_diagnosis_state::cancelled));
			}
			Queued.clear();
			CycleRunning = false;
			for (const auto& active : Active) {
				Provider.cancel_discovery(active.Request);
				++Statistics.CancelledRequests;
			}
		}

		void adopt(std::optional<personal_discovery_session> aSession)
		{
			if (Closing || Session == aSession) {
				return;
			}
			cancel_all();
			++Generation;
			Session = std::move(aSession);
			LastSuccess.reset();
			LastEnded.reset();
			BackoffUntil.reset();
			MetadataConsidered.clear();
			RefusedAlone.clear();
			if (!Session) {
				Status = queue_discovery_status{ .State = queue_discovery_state::unavailable, .Problem = queue_discovery_problem::no_account };
				publish();
				return;
			}
			Queue.adopt(Session->Queue);
			Status = queue_discovery_status{ .Host = Session->Queue.Host, .Account = Session->Queue.Account };
			start_cycle();
		}

		[[nodiscard]] auto backing_off(const std::int64_t aNow) const noexcept -> bool
		{
			return BackoffUntil.has_value() && aNow < *BackoffUntil;
		}

		void activate()
		{
			if (Closing || !Session || CycleRunning) {
				return;
			}
			// [[BPR-1681]]. Freshness is measured from the last refresh
			// that ran to its end, not only the last success: one that ended
			// with an unreadable repository or pull request would fail the
			// same way again, so every return to the window (closing the
			// diagnostic export's dialog, say) re-ran the whole refresh. A
			// refresh a session-wide failure stopped is still retried.
			const auto now = Clock.now_seconds();
			if (backing_off(now) || (LastEnded.has_value() && now - *LastEnded < personal_discovery_freshness_seconds)) {
				return;
			}
			start_cycle();
		}

		void refresh()
		{
			if (Closing || CycleRunning) {
				return;
			}
			const auto now = Clock.now_seconds();
			if (!Session || backing_off(now)) {
				++Statistics.RefusedRefreshes;
				if (Session) {
					// Said where the status is shown, with how long remains.
					Status.RetryAfterSeconds = static_cast<std::uint32_t>(*BackoffUntil - now);
				}
				publish();
				return;
			}
			start_cycle();
		}

		/// Whether the account now saves a repository the last refresh did
		/// not search.
		[[nodiscard]] auto unsearched(const std::vector<std::string>& aRepositories) const -> bool
		{
			return std::ranges::any_of(aRepositories, [this](const std::string& aRepository) {
				return std::ranges::find(Searched, aRepository) == Searched.end();
			});
		}

		void reconcile()
		{
			if (Closing || !Session || CycleRunning || backing_off(Clock.now_seconds())) {
				return;
			}
			auto repositories = partition();
			if (repositories == Searched && !unread_retained()) {
				return;
			}
			if (unsearched(repositories) || unread_retained()) {
				start_cycle();
				return;
			}
			// Only forgotten: nothing new to read, and what was found in them
			// is the queue's to withdraw.
			Searched            = std::move(repositories);
			Status.Repositories = static_cast<std::uint32_t>(Searched.size());
			if (Searched.empty()) {
				Status.State   = queue_discovery_state::unavailable;
				Status.Problem = queue_discovery_problem::no_saved_repository;
				Status.RetryAfterSeconds.reset();
			}
			publish();
		}

		[[nodiscard]] auto partition() const -> std::vector<std::string>
		{
			auto repositories = std::vector<std::string>{ };
			for (const auto& saved : Queue.saved_repositories()) {
				if (saved.Host == Session->Queue.Host && saved.Account == Session->Queue.Account && locator_of(saved.Repository)) {
					repositories.push_back(saved.Repository);
				}
			}
			std::ranges::sort(repositories);
			const auto [first, last] = std::ranges::unique(repositories);
			repositories.erase(first, last);
			return repositories;
		}

		[[nodiscard]] auto queue_ref(const std::string& aRepository, const core::pull_request_number aNumber) const -> core::pull_request_ref
		{
			return { .Host = Session->Queue.Host, .Account = Session->Queue.Account, .Repository = aRepository, .Number = static_cast<std::int64_t>(aNumber.value()) };
		}

		[[nodiscard]] auto still_enrolled(const std::string& aRepository, const core::pull_request_number aNumber) const -> bool
		{
			const auto ref = queue_ref(aRepository, aNumber);
			return std::ranges::any_of(Queue.enrollments(), [&ref](const enrollment_record& aRecord) {
				return aRecord.Ref == ref && aRecord.present();
			});
		}

		/// [[BPR-1680]]. The pull requests one retained read asks for.
		[[nodiscard]] static auto numbers_of(const job& aJob) -> std::vector<core::pull_request_number>
		{
			return aJob.Request.Numbers.empty() ? std::vector<core::pull_request_number>{ *aJob.Request.Number } : aJob.Request.Numbers;
		}

		/// A retained read is still wanted while any pull request it asks for
		/// is still enrolled.
		[[nodiscard]] auto still_enrolled(const job& aJob) const -> bool
		{
			return std::ranges::any_of(numbers_of(aJob), [&](const core::pull_request_number aNumber) { return still_enrolled(aJob.Repository, aNumber); });
		}

		[[nodiscard]] auto unread_retained() const -> bool
		{
			return std::ranges::any_of(Queue.queue(), [this](const core::queue_item& aItem) {
				return aItem.Ref.Host == Session->Queue.Host && aItem.Ref.Account == Session->Queue.Account
					&& aItem.Facts.Title.empty() && !MetadataConsidered.contains(aItem.Ref);
			});
		}

		void enqueue(const std::string& aRepository, const personal_discovery_reason aReason,
					 const std::optional<core::pull_request_number> aNumber = std::nullopt)
		{
			auto context       = Session->Context;
			context.Request    = core::request_id{ NextRequest++ };
			context.Repository = *locator_of(aRepository);
			Queued.push_back(job{
				.Request = {
					.Context               = std::move(context),
					.ProfileAuthority      = Session->ProfileAuthority,
					.Viewer                = Session->Viewer,
					.CapabilityEvidence    = Session->CapabilityEvidence,
					.AccountTransport      = Session->AccountTransport,
					.Reason                = aReason,
					.AuthenticationSession = Session->AuthenticationSession,
					.Number                = aNumber,
				},
				.Repository = aRepository,
				.Generation = Generation,
				.Cycle      = Cycle,
			});
		}

		/// [[BPR-1680]]. The pull requests of one repository the searches
		/// need not read again: the ones this refresh's retained reads read
		/// in full, as the core decides.
		[[nodiscard]] auto known_in(const std::string& aRepository) const -> std::vector<personal_discovery_known>
		{
			auto       known   = std::vector<personal_discovery_known>{ };
			const auto locator = locator_of(aRepository);
			for (const auto& read : core::discovery_known_pull_requests(CycleRetainedReads)) {
				if (locator && read.Repository == *locator) {
					known.push_back({ .Repository = read.Repository, .Number = read.Number });
				}
			}
			return known;
		}

		/// [[BPR-1680]]. One search of the saved repositories \p aRepositories,
		/// in locator order, knowing what this refresh's retained reads read.
		[[nodiscard]] auto search_job(const std::vector<std::string>& aRepositories, const personal_discovery_reason aReason) -> job
		{
			auto context       = Session->Context;
			context.Request    = core::request_id{ NextRequest++ };
			context.Repository = *locator_of(aRepositories.front());
			auto result        = job{
				.Request = {
					.Context               = std::move(context),
					.ProfileAuthority      = Session->ProfileAuthority,
					.Viewer                = Session->Viewer,
					.CapabilityEvidence    = Session->CapabilityEvidence,
					.AccountTransport      = Session->AccountTransport,
					.Reason                = aReason,
					.AuthenticationSession = Session->AuthenticationSession,
				},
				.Repository   = aRepositories.front(),
				.Repositories = aRepositories,
				.Generation   = Generation,
				.Cycle        = Cycle,
			};
			for (const auto& repository : aRepositories) {
				auto known = known_in(repository);
				result.Request.Known.insert(result.Request.Known.end(), known.begin(), known.end());
				if (aRepositories.size() > 1U) {
					result.Request.Repositories.push_back(*locator_of(repository));
				}
			}
			std::ranges::sort(result.Request.Known);
			return result;
		}

		/// [[BPR-1680]]. The saved repositories by locator, in locator order,
		/// which is the order a search names them in.
		[[nodiscard]] auto searched_by_locator() const -> std::vector<std::pair<core::repository_locator, std::string>>
		{
			auto result = std::vector<std::pair<core::repository_locator, std::string>>{ };
			for (const auto& repository : Searched) {
				result.emplace_back(*locator_of(repository), repository);
			}
			std::ranges::sort(result);
			return result;
		}

		/// [[BPR-1680]]. One search per reason for as many saved repositories
		/// as the core's query bound lets one search name, instead of one per
		/// repository; a repository refused on its own before is searched on
		/// its own.
		void enqueue_searches()
		{
			Searching          = true;
			const auto byName  = searched_by_locator();
			auto       names   = std::vector<core::repository_locator>{ };
			auto       refused = std::vector<core::repository_locator>{ };
			for (const auto& [locator, repository] : byName) {
				names.push_back(locator);
				if (RefusedAlone.contains(repository)) {
					refused.push_back(locator);
				}
			}
			const auto requested = core::plan_discovery_searches(core::discovery_search_reason::requested, Session->Viewer.Login, names, refused, Options.SearchRepositories);
			const auto authored  = core::plan_discovery_searches(core::discovery_search_reason::authored, Session->Viewer.Login, names, refused, Options.SearchRepositories);
			const auto enqueue   = [&](const std::vector<core::repository_locator>& aSearch, const personal_discovery_reason aReason) {
				auto repositories = std::vector<std::string>{ };
				for (const auto& locator : aSearch) {
					repositories.push_back(std::ranges::lower_bound(byName, locator, { }, &std::pair<core::repository_locator, std::string>::first)->second);
				}
				Queued.push_back(search_job(repositories, aReason));
			};
			// Both reasons of the same repositories go out together, as one
			// search per repository and reason always did.
			for (std::size_t index = 0U; index < std::max(requested.size(), authored.size()); ++index) {
				if (index < requested.size()) {
					enqueue(requested[index], personal_discovery_reason::requested);
				}
				if (index < authored.size()) {
					enqueue(authored[index], personal_discovery_reason::authored);
				}
			}
		}

		/// [[BPR-1680]]. A search of several repositories that cannot speak
		/// for each of them is asked again, one repository at a time, ahead of
		/// the rest; a repository forgotten meanwhile is not.
		void split(const job& aJob)
		{
			const auto saved = partition();
			for (auto repository = aJob.Repositories.rbegin(); repository != aJob.Repositories.rend(); ++repository) {
				if (std::ranges::find(saved, *repository) != saved.end()) {
					Queued.push_front(search_job({ *repository }, aJob.Request.Reason));
				}
			}
			++Statistics.Split;
		}

		/// [[BPR-1680]]. The saved repository a search row belongs to: the one
		/// of the search the adapter attributed it to, or, for a search of one
		/// repository, that one.
		[[nodiscard]] static auto repository_of(const job& aJob, const personal_discovery_row& aRow) -> std::optional<std::string>
		{
			if (aJob.Repositories.size() <= 1U) {
				return aJob.Repository;
			}
			for (const auto& repository : aJob.Repositories) {
				if (locator_of(repository) == aRow.Repository) {
					return repository;
				}
			}
			return std::nullopt;
		}

		/// [[BPR-1680]]. A search row the adapter did not read again, with
		/// the detail this refresh's retained read established, or none when
		/// the core says the row is not the viewer's or nothing stands for it.
		[[nodiscard]] auto reused_row(const job& aJob, const std::string& aRepository, const personal_discovery_row& aRow) const -> std::optional<queue_discovered_pull_request>
		{
			const auto locator  = *locator_of(aRepository);
			const auto ref      = queue_ref(aRepository, aRow.Number);
			const auto retained = CycleRetainedRows.find(ref);
			const auto read     = std::ranges::find_if(CycleRetainedReads, [&](const core::discovery_retained_read& aRead) {
				return aRead.Repository == locator && aRead.Number == aRow.Number;
			});
			const auto known    = std::ranges::binary_search(aJob.Request.Known, personal_discovery_known{ .Repository = locator, .Number = aRow.Number });
			const auto reason   = aJob.Request.Reason == personal_discovery_reason::requested ? core::discovery_search_reason::requested : core::discovery_search_reason::authored;
			if (!known || retained == CycleRetainedRows.end() || read == CycleRetainedReads.end() || !core::keeps_reused_row(reason, *read)) {
				return std::nullopt;
			}
			return queue_discovered_pull_request{
				.Number           = static_cast<std::int64_t>(aRow.Number.value()),
				.Title            = aRow.Title,
				.Author           = aRow.Author,
				.Detailed         = true,
				.ReviewersAwaited = retained->second.ReviewersAwaited,
				.Draft            = aRow.Draft,
				.Waiting          = retained->second.Waiting,
				.Change           = retained->second.Change,
			};
		}

		void start_cycle()
		{
			const auto repositories = partition();
			auto       items        = Queue.queue();
			std::erase_if(items, [this](const core::queue_item& aItem) {
				return aItem.Ref.Host != Session->Queue.Host || aItem.Ref.Account != Session->Queue.Account
					|| !locator_of(aItem.Ref.Repository) || aItem.Ref.Number <= 0 || aItem.Ref.Number > core::maximum_pull_request_number;
			});
			// Fill missing titles first; later explicit refreshes can reach rows
			// beyond the per-cycle limit. Within each group keep the queue order.
			std::stable_sort(items.begin(), items.end(), [](const core::queue_item& aLeft, const core::queue_item& aRight) {
				return aLeft.Facts.Title.empty() && !aRight.Facts.Title.empty();
			});
			Searched           = repositories;
			Status.Host        = Session->Queue.Host;
			Status.Account     = Session->Queue.Account;
			Status.LastSuccess = LastSuccess;
			Status.RetryAfterSeconds.reset();
			if (repositories.empty() && items.empty()) {
				Status.State        = queue_discovery_state::unavailable;
				Status.Problem      = queue_discovery_problem::no_saved_repository;
				Status.Repositories = 0U;
				publish();
				return;
			}
			++Cycle;
			++Statistics.Refreshes;
			CycleRunning     = true;
			CycleFailed      = false;
			CycleStopped     = false;
			MetadataFailed   = false;
			Searching        = false;
			CycleStarted     = Clock.elapsed_milliseconds();
			CycleRetained    = saturated(std::min(items.size(), maximum_personal_discovery_retained));
			CycleReads       = 0U;
			CycleFailedReads = 0U;
			CycleRequests    = 0U;
			CycleWait        = 0U;
			CycleCost        = { };
			CycleDecision    = 0U;
			ClosedThisCycle.clear();
			CycleRetainedRows.clear();
			CycleRetainedReads.clear();
			CycleProblem = queue_discovery_problem::none;
			// [[BPR-1680]]. Several retained pull requests of one repository go
			// in one read, as the core batches them.
			auto targets = std::vector<core::discovery_retained_target>{ };
			auto names   = std::vector<std::pair<core::repository_locator, std::string>>{ };
			for (std::size_t index = 0U; index < items.size(); ++index) {
				MetadataConsidered.insert(items[index].Ref);
				if (index < maximum_personal_discovery_retained) {
					const auto locator = *locator_of(items[index].Ref.Repository);
					targets.push_back({ .Repository = locator, .Number = core::pull_request_number{ static_cast<std::uint32_t>(items[index].Ref.Number) } });
					if (std::ranges::find(names, locator, &std::pair<core::repository_locator, std::string>::first) == names.end()) {
						names.emplace_back(locator, items[index].Ref.Repository);
					}
				} else {
					CycleProblem = queue_discovery_problem::detail_limit;
				}
			}
			for (auto& batch : core::plan_retained_batches(targets, Options.RetainedBatch)) {
				const auto& repository = std::ranges::find(names, batch.Repository, &std::pair<core::repository_locator, std::string>::first)->second;
				enqueue(repository, personal_discovery_reason::retained, batch.Numbers.front());
				if (batch.Numbers.size() > 1U) {
					Queued.back().Request.Numbers = std::move(batch.Numbers);
				}
			}
			Status.State        = queue_discovery_state::refreshing;
			Status.Problem      = queue_discovery_problem::none;
			Status.Repositories = static_cast<std::uint32_t>(repositories.size());
			record(diagnostic_outcome::started, refresh_diagnosis(core::discovery_diagnosis_state::none));
			publish();
			pump();
		}

		void yield(const bool aYield)
		{
			if (Closing || Yielding == aYield) {
				return;
			}
			Yielding = aYield;
			if (!Yielding) {
				pump();
				return;
			}
			for (auto& active : Active) {
				if (current(active) && !active.Preempted) {
					active.Preempted = true;
					++Statistics.Preempted;
					Provider.cancel_discovery(active.Request);
				}
			}
		}

		/// [[BPR-1679]]. Puts a read cancelled to give way back at the front,
		/// under a new identity, so its late terminal can never be taken for
		/// the read asked again.
		void resubmit(job aJob)
		{
			aJob.Preempted               = false;
			aJob.Request.Context.Request = core::request_id{ NextRequest++ };
			aJob.Request.FactRevision    = 0U;
			++Statistics.Resubmitted;
			Queued.push_front(std::move(aJob));
		}

		void pump()
		{
			while (!Closing && !Yielding && Active.size() < maximum_personal_discovery_in_flight && !Queued.empty()) {
				auto next = std::move(Queued.front());
				Queued.pop_front();
				if (next.Request.Reason == personal_discovery_reason::retained && !still_enrolled(next)) {
					continue;
				}
				next.Request.FactRevision = Queue.begin_observation();
				next.Read                 = ++CycleReads;
				record(diagnostic_outcome::started, read_diagnosis(next), diagnostic_detail::verbose);
				Active.push_back(next);
				++Statistics.Submitted;
				Statistics.MaximumInFlight = std::max<std::uint64_t>(Statistics.MaximumInFlight, Active.size());
				const auto request         = next.Request.Context.Request;
				try {
					// The lane outlives the host, so a completion is always
					// posted; only the lane's turn reads the host, and only if
					// it still exists.
					// [[BPR-1744]]. The answer's wait for the lane is timed
					// on the steady clock, which needs nothing the host owns.
					Provider.discover(std::move(next.Request), [&lane = DecisionLane, weak = weak_from_this(), request](personal_discovery_completion aCompletion) {
						lane.post([weak, request, answered = std::chrono::steady_clock::now(), completion = std::move(aCompletion)]() mutable {
							if (const auto host = weak.lock(); host != nullptr) {
								const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - answered).count();
								host->finish(request, std::move(completion), saturated(static_cast<std::uint64_t>(std::max<std::int64_t>(waited, 0))));
							}
						});
					});
				} catch (...) {
					std::erase_if(Active, [request](const job& aJob) { return aJob.Request.Context.Request == request; });
					fail(queue_discovery_problem::failed, true);
				}
			}
			// A refresh stopped by a session-wide failure is decided at once:
			// what it cancelled only has its terminal still to come.
			if (CycleRunning && Queued.empty() && (CycleStopped || std::ranges::none_of(Active, [this](const job& aJob) { return current(aJob); }))) {
				if (!CycleStopped && !Searching) {
					enqueue_searches();
					pump();
				} else {
					finish_cycle();
				}
			}
		}

		[[nodiscard]] auto current(const job& aJob) const noexcept -> bool
		{
			return aJob.Generation == Generation && aJob.Cycle == Cycle;
		}

		void fail(const queue_discovery_problem aProblem, const bool aStop)
		{
			CycleFailed    = true;
			MetadataFailed = MetadataFailed || !Searching;
			if (CycleProblem == queue_discovery_problem::none || aStop) {
				CycleProblem = aProblem;
			}
			if (aStop) {
				CycleStopped = true;
				Queued.clear();
				for (const auto& active : Active) {
					if (current(active)) {
						Provider.cancel_discovery(active.Request);
						++Statistics.CancelledRequests;
					}
				}
			}
		}

		void finish(const core::request_id aRequest, personal_discovery_completion aCompletion, const std::uint32_t aDecisionMilliseconds)
		{
			const auto found = std::ranges::find_if(Active, [aRequest](const job& aJob) { return aJob.Request.Context.Request == aRequest; });
			if (found == Active.end()) {
				++Statistics.StaleCompletions;
				return;
			}
			const auto finished = std::move(*found);
			Active.erase(found);
			if (Closing) {
				return;
			}
			record_read(finished, aCompletion, aDecisionMilliseconds);
			// A repository forgotten while its read was in flight publishes
			// nothing: the reviewer has said it is not theirs to watch.
			const auto saved      = Session ? partition() : std::vector<std::string>{ };
			const auto stillSaved = [&saved](const std::string& aRepository) { return std::ranges::find(saved, aRepository) != saved.end(); };
			if (!current(finished) || !Session
				|| (finished.Request.Reason == personal_discovery_reason::retained ? !still_enrolled(finished) : std::ranges::none_of(finished.Repositories, stillSaved))) {
				++Statistics.StaleCompletions;
				pump();
				return;
			}
			// [[BPR-1679]]. Stopped to give way: not a failure of the refresh.
			if (const auto* const cancelled = std::get_if<personal_discovery_cancelled>(&aCompletion);
				finished.Preempted && !CycleStopped && cancelled != nullptr && cancelled->Request == finished.Request) {
				resubmit(finished);
				pump();
				return;
			}
			std::visit(
				[this, &finished](auto& aResult) {
					using result_type = std::remove_cvref_t<decltype(aResult)>;
					if (aResult.Request != finished.Request) {
						++Statistics.StaleCompletions;
						fail(queue_discovery_problem::failed, false);
						return;
					}
					if constexpr (std::is_same_v<result_type, personal_discovery_success>) {
						++Statistics.Answered;
						if (finished.Request.Reason == personal_discovery_reason::retained) {
							apply_metadata(finished, aResult);
							return;
						}
						// [[BPR-1680]]. A shared search that cannot speak for
						// each of its repositories is asked again alone.
						if (core::splits_discovery_search(finished.Repositories.size(), search_outcome(aResult.Completeness))) {
							split(finished);
							return;
						}
						apply_search(finished, aResult);
					} else if constexpr (std::is_same_v<result_type, personal_discovery_error>) {
						const auto refused = aResult.Error.Failure == github_provider_failure::permission;
						if (finished.Request.Reason != personal_discovery_reason::retained
							&& core::splits_discovery_search(finished.Repositories.size(), refused ? core::discovery_search_outcome::refused : core::discovery_search_outcome::failed)) {
							split(finished);
							return;
						}
						// [[BPR-1680]]. Refused on its own: searched on its own
						// while this session lasts, so it costs no shared search.
						if (refused && finished.Request.Reason != personal_discovery_reason::retained && finished.Repositories.size() == 1U) {
							RefusedAlone.insert(finished.Repository);
						}
						++Statistics.Failed;
						const auto problem = problem_of(aResult.Error);
						if (aResult.Error.Failure == github_provider_failure::rate_limit) {
							const auto wait = aResult.Error.RetryAfterSeconds.value_or(personal_discovery_default_retry_seconds);
							BackoffUntil    = Clock.now_seconds() + static_cast<std::int64_t>(wait);
						}
						fail(problem, stops_refresh(aResult.Error.Failure));
					} else {
						++Statistics.CancelledRequests;
						fail(queue_discovery_problem::failed, false);
					}
				},
				aCompletion);
			pump();
		}

		[[nodiscard]] static auto search_outcome(const personal_discovery_completeness aCompleteness) noexcept -> core::discovery_search_outcome
		{
			switch (aCompleteness) {
			case personal_discovery_completeness::complete:           return core::discovery_search_outcome::complete;
			case personal_discovery_completeness::incomplete_results: return core::discovery_search_outcome::incomplete_results;
			case personal_discovery_completeness::result_limit:       return core::discovery_search_outcome::result_limit;
			case personal_discovery_completeness::detail_limit:       return core::discovery_search_outcome::detail_limit;
			}
			return core::discovery_search_outcome::failed;
		}

		/// One answer per saved repository the search names, each with the
		/// rows the adapter attributed to it; a repository forgotten while the
		/// search ran gets none.
		void apply_search(const job& aJob, personal_discovery_success& aResult)
		{
			const auto saved = partition();
			auto       rows  = std::map<std::string, std::vector<queue_discovered_pull_request>>{ };
			for (auto& row : aResult.Rows) {
				const auto repository = repository_of(aJob, row);
				if (!repository) {
					fail(queue_discovery_problem::failed, false);
					continue;
				}
				if (ClosedThisCycle.contains(queue_ref(*repository, row.Number))) {
					continue;
				}
				// [[BPR-1680]]. Not read again: the retained read stands for it.
				if (row.Reused) {
					if (auto reused = reused_row(aJob, *repository, row)) {
						++Statistics.Reused;
						rows[*repository].push_back(std::move(*reused));
					}
					continue;
				}
				rows[*repository].push_back({
					.Number           = static_cast<std::int64_t>(row.Number.value()),
					.Title            = std::move(row.Title),
					.Author           = std::move(row.Author),
					.Detailed         = row.Detailed,
					.ReviewersAwaited = row.ReviewersAwaited,
					.Draft            = row.Draft,
					.Waiting          = row.Waiting,
					.Change           = row.Change,
				});
			}
			if (aResult.Completeness != personal_discovery_completeness::complete && CycleProblem == queue_discovery_problem::none) {
				CycleProblem = problem_of(aResult.Completeness);
			}
			RefusedAlone.erase(aJob.Repository);
			for (const auto& repository : aJob.Repositories) {
				if (std::ranges::find(saved, repository) == saved.end()) {
					continue;
				}
				auto answer = queue_discovery_answer{
					.Session      = Session->Queue,
					.Repository   = repository,
					.Requested    = aJob.Request.Reason == personal_discovery_reason::requested,
					.Complete     = aResult.Completeness == personal_discovery_completeness::complete && !MetadataFailed,
					.PullRequests = std::move(rows[repository]),
				};
				answer.FactRevision = aJob.Request.FactRevision;
				Queue.apply(std::move(answer));
			}
		}

		/// One row for each pull request the retained read asked for; a pull
		/// request it could not read fails, a row for one it did not ask for
		/// fails the read's answer, and one no longer enrolled is not applied.
		void apply_metadata(const job& aJob, const personal_discovery_success& aResult)
		{
			const auto numbers = numbers_of(aJob);
			if (aResult.Completeness != personal_discovery_completeness::complete || aResult.Rows.size() > numbers.size()
				|| std::ranges::any_of(aResult.Rows, [&numbers](const personal_discovery_row& aRow) { return std::ranges::find(numbers, aRow.Number) == numbers.end(); })) {
				fail(queue_discovery_problem::failed, false);
				return;
			}
			for (const auto number : numbers) {
				const auto row = std::ranges::find(aResult.Rows, number, &personal_discovery_row::Number);
				if (row == aResult.Rows.end() || std::ranges::count(aResult.Rows, number, &personal_discovery_row::Number) != 1) {
					fail(queue_discovery_problem::failed, false);
					continue;
				}
				if (numbers.size() == 1U || still_enrolled(aJob.Repository, number)) {
					apply_metadata_row(aJob, *row);
				}
			}
		}

		void apply_metadata_row(const job& aJob, const personal_discovery_row& aRow)
		{
			const auto& row = aRow;
			if (!row.Detailed || !row.HeadOid || row.Reviews.size() > maximum_personal_discovery_reviews) {
				fail(queue_discovery_problem::failed, false);
				return;
			}
			if (row.WaitingLimited && CycleProblem == queue_discovery_problem::none) {
				CycleProblem = queue_discovery_problem::detail_limit;
			}
			const auto head = core::parse_git_commit_oid(row.HeadOid->value());
			if (!head) {
				fail(queue_discovery_problem::failed, false);
				return;
			}
			const auto ref = queue_ref(aJob.Repository, row.Number);
			if (row.Closed) {
				ClosedThisCycle.insert(ref);
			}
			auto reviewedHead = std::optional<core::git_commit_oid>{ };
			// [[BPR-1718]]. The same submitted reviews, for the core to say
			// which verdict of mine stands.
			auto mine = std::vector<core::my_submitted_review>{ };
			for (const auto& review : row.Reviews) {
				const auto commit = review.Commit ? core::parse_git_commit_oid(review.Commit->value()) : std::nullopt;
				if (review.DatabaseId == 0U || !review.Author || Session->Viewer.Node.value().empty() || *review.Author != Session->Viewer.Node
					|| review.State == github_review_state::pending || review.State == github_review_state::uninitialized) {
					continue;
				}
				const auto comparable = commit && commit->algorithm() == head->algorithm();
				if (comparable) {
					reviewedHead = commit;
				}
				mine.push_back({ .State = my_review_verdict_of(review.State), .SubmittedAt = review.SubmittedAt, .OnHead = comparable ? std::optional{ *commit == *head } : std::nullopt });
			}
			Queue.apply(queue_metadata_answer{
				.Session  = Session->Queue,
				.Ref      = ref,
				.Facts    = { .Title = row.Title, .Author = row.Author, .Draft = row.Draft, .Change = row.Change, .MyReview = core::my_review_for(mine) },
				.Activity = { .ReviewedByMe = reviewedHead.has_value(), .HeadMovedSinceMyReview = reviewedHead && *reviewedHead != *head, .ReviewersAwaited = row.ReviewersAwaited, .Known = true, .Waiting = row.Waiting, .FactRevision = aJob.Request.FactRevision },
				.Closed   = row.Closed,
				.Authored = row.AuthorIdentity ? std::optional<bool>{ *row.AuthorIdentity == Session->Viewer.Node } : std::nullopt,
			});
			// [[BPR-1680]]. What this read established, for the searches that
			// follow it in this refresh.
			CycleRetainedRows.insert_or_assign(ref, row);
			CycleRetainedReads.push_back({ .Repository = aJob.Request.Context.Repository, .Number = row.Number, .Open = !row.Closed, .Complete = !row.WaitingLimited, .RequestsViewer = row.RequestsViewer });
		}

		void finish_cycle()
		{
			CycleRunning   = false;
			const auto now = Clock.now_seconds();
			if (CycleStopped) {
				LastEnded.reset();
			} else {
				LastEnded = now;
			}
			Status.Host    = Session->Queue.Host;
			Status.Account = Session->Queue.Account;
			Status.Problem = CycleProblem;
			Status.RetryAfterSeconds.reset();
			if (!CycleFailed) {
				LastSuccess  = now;
				Status.State = CycleProblem == queue_discovery_problem::none ? queue_discovery_state::current : queue_discovery_state::partial;
			} else {
				Status.State = queue_discovery_state::stale;
				if (backing_off(now)) {
					Status.RetryAfterSeconds = static_cast<std::uint32_t>(*BackoffUntil - now);
				}
			}
			Status.LastSuccess = LastSuccess;
			if (!CycleFailed && Searched.empty() && CycleProblem == queue_discovery_problem::none) {
				Status.State   = queue_discovery_state::unavailable;
				Status.Problem = queue_discovery_problem::no_saved_repository;
			}
			publish();
			auto ended           = refresh_diagnosis(ended_state());
			ended.BackoffSeconds = Status.RetryAfterSeconds.value_or(0U);
			record(CycleStopped ? diagnostic_outcome::failed : diagnostic_outcome::succeeded, ended);
			// A repository saved while this refresh ran is searched now.
			if (!backing_off(now) && unsearched(partition())) {
				start_cycle();
			}
		}

		void shutdown()
		{
			if (Closing) {
				return;
			}
			cancel_all();
			Closing = true;
		}

		personal_discovery_port&                  Provider;
		decision_executor&                        DecisionLane;
		review_queue_application_host&            Queue;
		const personal_discovery_clock_port&      Clock;
		diagnostic_log_port*                      Log = nullptr;
		personal_discovery_options                Options;
		std::optional<personal_discovery_session> Session;
		std::deque<job>                           Queued;
		std::vector<std::string>                  Searched;
		std::set<core::pull_request_ref>          MetadataConsidered;
		std::set<core::pull_request_ref>          ClosedThisCycle;
		/// [[BPR-1680]]. Saved repositories whose own search GitHub refused in
		/// this session: searched alone, so a shared search is not refused.
		std::set<std::string> RefusedAlone;
		/// [[BPR-1680]]. What this refresh's retained reads applied, which its
		/// searches reuse rather than read again.
		std::map<core::pull_request_ref, personal_discovery_row> CycleRetainedRows;
		std::vector<core::discovery_retained_read>               CycleRetainedReads;
		std::vector<job>                                         Active;
		std::optional<std::int64_t>                              LastSuccess;
		std::optional<std::int64_t>                              LastEnded;
		std::optional<std::int64_t>                              BackoffUntil;
		queue_discovery_status                                   Status;
		personal_discovery_statistics                            Statistics;
		queue_discovery_problem                                  CycleProblem = queue_discovery_problem::none;
		std::uint64_t                                            Generation   = 0U;
		std::uint64_t                                            Cycle        = 0U;
		std::uint64_t                                            NextRequest  = 1U;
		/// [[BPR-1680]]. The running refresh's diagnostic counts.
		std::int64_t  CycleStarted     = 0;
		std::uint32_t CycleRetained    = 0U;
		std::uint32_t CycleReads       = 0U;
		std::uint32_t CycleFailedReads = 0U;
		std::uint32_t CycleRequests    = 0U;
		std::uint32_t CycleWait        = 0U;
		/// [[BPR-1744]]. Its reads' summed timing split and answer counts.
		personal_discovery_cost CycleCost;
		std::uint32_t           CycleDecision  = 0U;
		bool                    CycleRunning   = false;
		bool                    CycleFailed    = false;
		bool                    CycleStopped   = false;
		bool                    Searching      = false;
		bool                    MetadataFailed = false;
		bool                    Closing        = false;
		bool                    Yielding       = false;
	};

	personal_discovery_host::personal_discovery_host(personal_discovery_port&             aProvider,
													 decision_executor&                   aDecisionLane,
													 review_queue_application_host&       aQueue,
													 const personal_discovery_clock_port& aClock,
													 diagnostic_log_port*                 aLog,
													 const personal_discovery_options     aOptions)
		: Implementation{ std::make_shared<implementation>(aProvider, aDecisionLane, aQueue, aClock, aLog, aOptions) }
	{ }

	personal_discovery_host::~personal_discovery_host()
	{
		Implementation->shutdown();
	}

	void personal_discovery_host::adopt(std::optional<personal_discovery_session> aSession)
	{
		Implementation->adopt(std::move(aSession));
	}

	void personal_discovery_host::activate()
	{
		Implementation->activate();
	}

	void personal_discovery_host::refresh()
	{
		Implementation->refresh();
	}

	void personal_discovery_host::reconcile()
	{
		Implementation->reconcile();
	}

	void personal_discovery_host::yield(const bool aYield)
	{
		Implementation->yield(aYield);
	}

	void personal_discovery_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto personal_discovery_host::yielding() const noexcept -> bool
	{
		return Implementation->Yielding;
	}

	auto personal_discovery_host::status() const noexcept -> const queue_discovery_status&
	{
		return Implementation->Status;
	}

	auto personal_discovery_host::statistics() const noexcept -> personal_discovery_statistics
	{
		return Implementation->Statistics;
	}

	auto personal_discovery_host::in_flight() const noexcept -> std::size_t
	{
		return Implementation->Active.size();
	}

	auto personal_discovery_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Active.empty();
	}
} // namespace bpr::application
