#include "bpr/application/personal_discovery_host.hpp"

#include <algorithm>
#include <deque>
#include <optional>
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
			.Queue   = { .Host = std::string{ hostname }, .Account = viewer.Login, .Generation = aSession.Generation.value() },
			.Context = {
				.ApplicationSession = aSession.ApplicationSession,
				.Profile            = aSession.ProfileAuthority.Profile,
				.Host               = aSession.ProfileAuthority.Host,
				.Account            = viewer.Account,
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
			std::uint64_t              Generation = 0U;
			std::uint64_t              Cycle      = 0U;
		};

		implementation(personal_discovery_port& aProvider, decision_executor& aDecisionLane,
					   review_queue_application_host& aQueue, const personal_discovery_clock_port& aClock)
			: Provider{ aProvider }
			, DecisionLane{ aDecisionLane }
			, Queue{ aQueue }
			, Clock{ aClock }
		{
			Status.State   = queue_discovery_state::unavailable;
			Status.Problem = queue_discovery_problem::no_account;
		}

		void publish()
		{
			Queue.present_discovery(Status);
		}

		void cancel_all()
		{
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
			BackoffUntil.reset();
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
			const auto now = Clock.now_seconds();
			if (backing_off(now) || (LastSuccess.has_value() && now - *LastSuccess < personal_discovery_freshness_seconds)) {
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
			if (repositories == Searched) {
				return;
			}
			if (unsearched(repositories)) {
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

		void start_cycle()
		{
			const auto repositories = partition();
			Searched                = repositories;
			Status.Host             = Session->Queue.Host;
			Status.Account          = Session->Queue.Account;
			Status.LastSuccess      = LastSuccess;
			Status.RetryAfterSeconds.reset();
			if (repositories.empty()) {
				Status.State        = queue_discovery_state::unavailable;
				Status.Problem      = queue_discovery_problem::no_saved_repository;
				Status.Repositories = 0U;
				publish();
				return;
			}
			++Cycle;
			++Statistics.Refreshes;
			CycleRunning = true;
			CycleFailed  = false;
			CycleStopped = false;
			CycleProblem = queue_discovery_problem::none;
			for (const auto& repository : repositories) {
				for (const auto reason : { personal_discovery_reason::requested, personal_discovery_reason::authored }) {
					auto context       = Session->Context;
					context.Request    = core::request_id{ NextRequest++ };
					context.Repository = *locator_of(repository);
					Queued.push_back(job{
						.Request = {
							.Context               = std::move(context),
							.ProfileAuthority      = Session->ProfileAuthority,
							.Viewer                = Session->Viewer,
							.CapabilityEvidence    = Session->CapabilityEvidence,
							.AccountTransport      = Session->AccountTransport,
							.Reason                = reason,
							.AuthenticationSession = Session->AuthenticationSession,
						},
						.Repository = repository,
						.Generation = Generation,
						.Cycle      = Cycle,
					});
				}
			}
			Status.State        = queue_discovery_state::refreshing;
			Status.Problem      = queue_discovery_problem::none;
			Status.Repositories = static_cast<std::uint32_t>(repositories.size());
			publish();
			pump();
		}

		void pump()
		{
			while (!Closing && Active.size() < maximum_personal_discovery_in_flight && !Queued.empty()) {
				auto next = std::move(Queued.front());
				Queued.pop_front();
				Active.push_back(next);
				++Statistics.Submitted;
				Statistics.MaximumInFlight = std::max<std::uint64_t>(Statistics.MaximumInFlight, Active.size());
				const auto request         = next.Request.Context.Request;
				try {
					// The lane outlives the host, so a completion is always
					// posted; only the lane's turn reads the host, and only if
					// it still exists.
					Provider.discover(std::move(next.Request), [&lane = DecisionLane, weak = weak_from_this(), request](personal_discovery_completion aCompletion) {
						lane.post([weak, request, completion = std::move(aCompletion)]() mutable {
							if (const auto host = weak.lock(); host != nullptr) {
								host->finish(request, std::move(completion));
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
				finish_cycle();
			}
		}

		[[nodiscard]] auto current(const job& aJob) const noexcept -> bool
		{
			return aJob.Generation == Generation && aJob.Cycle == Cycle;
		}

		void fail(const queue_discovery_problem aProblem, const bool aStop)
		{
			CycleFailed = true;
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

		void finish(const core::request_id aRequest, personal_discovery_completion aCompletion)
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
			// A repository forgotten while its read was in flight publishes
			// nothing: the reviewer has said it is not theirs to watch.
			const auto saved = partition();
			if (!current(finished) || std::ranges::find(saved, finished.Repository) == saved.end()) {
				++Statistics.StaleCompletions;
				pump();
				return;
			}
			std::visit(
				[this, &finished](auto& aResult) {
					using result_type = std::remove_cvref_t<decltype(aResult)>;
					if constexpr (std::is_same_v<result_type, personal_discovery_success>) {
						++Statistics.Answered;
						auto answer = queue_discovery_answer{
							.Session      = Session->Queue,
							.Repository   = finished.Repository,
							.Requested    = finished.Request.Reason == personal_discovery_reason::requested,
							.Complete     = aResult.Completeness == personal_discovery_completeness::complete,
							.PullRequests = { },
						};
						answer.PullRequests.reserve(aResult.Rows.size());
						for (auto& row : aResult.Rows) {
							answer.PullRequests.push_back({
								.Number           = static_cast<std::int64_t>(row.Number.value()),
								.Title            = std::move(row.Title),
								.Author           = std::move(row.Author),
								.Detailed         = row.Detailed,
								.ReviewersAwaited = row.ReviewersAwaited,
							});
						}
						if (aResult.Completeness != personal_discovery_completeness::complete && CycleProblem == queue_discovery_problem::none) {
							CycleProblem = problem_of(aResult.Completeness);
						}
						Queue.apply(std::move(answer));
					} else if constexpr (std::is_same_v<result_type, personal_discovery_error>) {
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

		void finish_cycle()
		{
			CycleRunning   = false;
			const auto now = Clock.now_seconds();
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
			publish();
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
		std::optional<personal_discovery_session> Session;
		std::deque<job>                           Queued;
		std::vector<std::string>                  Searched;
		std::vector<job>                          Active;
		std::optional<std::int64_t>               LastSuccess;
		std::optional<std::int64_t>               BackoffUntil;
		queue_discovery_status                    Status;
		personal_discovery_statistics             Statistics;
		queue_discovery_problem                   CycleProblem = queue_discovery_problem::none;
		std::uint64_t                             Generation   = 0U;
		std::uint64_t                             Cycle        = 0U;
		std::uint64_t                             NextRequest  = 1U;
		bool                                      CycleRunning = false;
		bool                                      CycleFailed  = false;
		bool                                      CycleStopped = false;
		bool                                      Closing      = false;
	};

	personal_discovery_host::personal_discovery_host(personal_discovery_port&             aProvider,
													 decision_executor&                   aDecisionLane,
													 review_queue_application_host&       aQueue,
													 const personal_discovery_clock_port& aClock)
		: Implementation{ std::make_shared<implementation>(aProvider, aDecisionLane, aQueue, aClock) }
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

	void personal_discovery_host::shutdown()
	{
		Implementation->shutdown();
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
