#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/core/git_objects.hpp"

#include <algorithm>
#include <deque>
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
			std::uint64_t              Generation = 0U;
			std::uint64_t              Cycle      = 0U;
			/// [[BPR-1679]]. Cancelled to give way; asked again if that is
			/// what answers it.
			bool Preempted = false;
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
			LastEnded.reset();
			BackoffUntil.reset();
			MetadataConsidered.clear();
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
			// [[BPR-PENDING-B2]]. Freshness is measured from the last refresh
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

		[[nodiscard]] auto still_enrolled(const job& aJob) const -> bool
		{
			const auto ref = queue_ref(aJob.Repository, *aJob.Request.Number);
			return std::ranges::any_of(Queue.enrollments(), [&ref](const enrollment_record& aRecord) {
				return aRecord.Ref == ref && aRecord.present();
			});
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

		void enqueue_searches()
		{
			Searching = true;
			for (const auto& repository : Searched) {
				for (const auto reason : { personal_discovery_reason::requested, personal_discovery_reason::authored }) {
					enqueue(repository, reason);
				}
			}
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
			CycleRunning   = true;
			CycleFailed    = false;
			CycleStopped   = false;
			MetadataFailed = false;
			Searching      = false;
			ClosedThisCycle.clear();
			CycleProblem = queue_discovery_problem::none;
			for (std::size_t index = 0U; index < items.size(); ++index) {
				MetadataConsidered.insert(items[index].Ref);
				if (index < maximum_personal_discovery_retained) {
					enqueue(items[index].Ref.Repository, personal_discovery_reason::retained,
							core::pull_request_number{ static_cast<std::uint32_t>(items[index].Ref.Number) });
				} else {
					CycleProblem = queue_discovery_problem::detail_limit;
				}
			}
			Status.State        = queue_discovery_state::refreshing;
			Status.Problem      = queue_discovery_problem::none;
			Status.Repositories = static_cast<std::uint32_t>(repositories.size());
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
			const auto saved = Session ? partition() : std::vector<std::string>{ };
			if (!current(finished) || !Session || (finished.Request.Reason == personal_discovery_reason::retained ? !still_enrolled(finished) : std::ranges::find(saved, finished.Repository) == saved.end())) {
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
						auto answer = queue_discovery_answer{
							.Session      = Session->Queue,
							.Repository   = finished.Repository,
							.Requested    = finished.Request.Reason == personal_discovery_reason::requested,
							.Complete     = aResult.Completeness == personal_discovery_completeness::complete && !MetadataFailed,
							.PullRequests = { },
						};
						answer.FactRevision = finished.Request.FactRevision;
						answer.PullRequests.reserve(aResult.Rows.size());
						for (auto& row : aResult.Rows) {
							if (ClosedThisCycle.contains(queue_ref(finished.Repository, row.Number))) {
								continue;
							}
							answer.PullRequests.push_back({
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

		void apply_metadata(const job& aJob, const personal_discovery_success& aResult)
		{
			if (aResult.Completeness != personal_discovery_completeness::complete || aResult.Rows.size() != 1U
				|| aResult.Rows.front().Number != aJob.Request.Number || !aResult.Rows.front().Detailed
				|| !aResult.Rows.front().HeadOid || aResult.Rows.front().Reviews.size() > maximum_personal_discovery_reviews) {
				fail(queue_discovery_problem::failed, false);
				return;
			}
			const auto& row = aResult.Rows.front();
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
			for (const auto& review : row.Reviews) {
				const auto commit = review.Commit ? core::parse_git_commit_oid(review.Commit->value()) : std::nullopt;
				if (review.DatabaseId != 0U && review.Author && !Session->Viewer.Node.value().empty()
					&& *review.Author == Session->Viewer.Node && review.State != github_review_state::pending
					&& review.State != github_review_state::uninitialized && commit && commit->algorithm() == head->algorithm()) {
					reviewedHead = commit;
				}
			}
			Queue.apply(queue_metadata_answer{
				.Session  = Session->Queue,
				.Ref      = ref,
				.Facts    = { .Title = row.Title, .Author = row.Author, .Draft = row.Draft, .Change = row.Change },
				.Activity = { .ReviewedByMe = reviewedHead.has_value(), .HeadMovedSinceMyReview = reviewedHead && *reviewedHead != *head, .ReviewersAwaited = row.ReviewersAwaited, .Known = true, .Waiting = row.Waiting, .FactRevision = aJob.Request.FactRevision },
				.Closed   = row.Closed,
				.Authored = row.AuthorIdentity ? std::optional<bool>{ *row.AuthorIdentity == Session->Viewer.Node } : std::nullopt,
			});
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
		std::set<core::pull_request_ref>          MetadataConsidered;
		std::set<core::pull_request_ref>          ClosedThisCycle;
		std::vector<job>                          Active;
		std::optional<std::int64_t>               LastSuccess;
		std::optional<std::int64_t>               LastEnded;
		std::optional<std::int64_t>               BackoffUntil;
		queue_discovery_status                    Status;
		personal_discovery_statistics             Statistics;
		queue_discovery_problem                   CycleProblem   = queue_discovery_problem::none;
		std::uint64_t                             Generation     = 0U;
		std::uint64_t                             Cycle          = 0U;
		std::uint64_t                             NextRequest    = 1U;
		bool                                      CycleRunning   = false;
		bool                                      CycleFailed    = false;
		bool                                      CycleStopped   = false;
		bool                                      Searching      = false;
		bool                                      MetadataFailed = false;
		bool                                      Closing        = false;
		bool                                      Yielding       = false;
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
