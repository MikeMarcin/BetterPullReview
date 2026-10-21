#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PERSONAL_DISCOVERY_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PERSONAL_DISCOVERY_HOST_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_window_read_host.hpp"
#include "bpr/application/personal_discovery_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/core/home_discovery_plan.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace bpr::application {
	/// \brief [[BPR-0989]]. The host clock discovery reads, in whole seconds.
	class personal_discovery_clock_port
	{
	public:
		virtual ~personal_discovery_clock_port()                       = default;
		[[nodiscard]] virtual auto now_seconds() const -> std::int64_t = 0;
		/// \brief [[BPR-1680]]. A monotonic instant in milliseconds, read only
		/// to say in the diagnostics log how long a refresh took.
		[[nodiscard]] virtual auto elapsed_milliseconds() const -> std::int64_t { return now_seconds() * 1000; }
	};

	/// \brief [[BPR-0989]]. The one accepted account session discovery reads under.
	///
	/// \details `Queue` names the server, the viewer's login and the identity
	/// generation exactly as the profile's queue names them, so an answer is
	/// applied to the queue of the session it was read under and no other.
	/// `Context` carries the session's request facts; its request and
	/// repository are filled for each read.
	struct personal_discovery_session
	{
		queue_session_id                            Queue;
		core::repository_request_context            Context;
		github_profile_authority                    ProfileAuthority;
		github_viewer_record                        Viewer;
		github_capability_cache_entry               CapabilityEvidence;
		github_account_transport                    AccountTransport;
		std::optional<core::application_session_id> AuthenticationSession;

		auto operator<=>(const personal_discovery_session&) const = default;
	};

	/// \brief [[BPR-0990]]. The discovery session of an accepted account session.
	///
	/// \details Absent for a public-preview session, which has no viewer to
	/// search for, and for a session without a viewer login. The queue
	/// session is the profile's canonical hostname, the viewer's login and
	/// the identity generation, exactly as a pull request opened under the
	/// same session names it.
	[[nodiscard]] auto personal_discovery_session_of(const github_profile_session& aSession)
		-> std::optional<personal_discovery_session>;

	/// \brief How long a refresh that ran to its end stays fresh for activation.
	inline constexpr std::int64_t personal_discovery_freshness_seconds = 300;
	/// \brief How many discovery reads one profile has in flight at once.
	inline constexpr std::size_t maximum_personal_discovery_in_flight = 2U;
	/// \brief The wait a secondary limit without its own retry-after asks for.
	inline constexpr std::uint32_t personal_discovery_default_retry_seconds = 60U;

	/// \brief [[BPR-1680]]. How discovery shapes its reads.
	struct personal_discovery_options
	{
		/// The most saved repositories one personal search names. One searches
		/// each repository on its own, as discovery did before.
		std::size_t SearchRepositories = core::maximum_discovery_search_repositories;

		auto operator<=>(const personal_discovery_options&) const = default;
	};

	struct personal_discovery_statistics
	{
		std::uint64_t Refreshes         = 0U;
		std::uint64_t Submitted         = 0U;
		std::uint64_t Answered          = 0U;
		std::uint64_t Failed            = 0U;
		std::uint64_t StaleCompletions  = 0U;
		std::uint64_t RefusedRefreshes  = 0U;
		std::uint64_t CancelledRequests = 0U;
		std::uint64_t MaximumInFlight   = 0U;
		/// [[BPR-1679]]. Reads stopped to give way, and reads asked again after.
		std::uint64_t Preempted   = 0U;
		std::uint64_t Resubmitted = 0U;
		/// [[BPR-1680]]. The wire requests the answered, failed and cancelled
		/// reads say they sent.
		std::uint64_t Requests = 0U;
		/// [[BPR-1680]]. Search rows answered from the refresh's retained
		/// read instead of reads of their own.
		std::uint64_t Reused = 0U;
		/// [[BPR-1680]]. Searches of several repositories asked again one
		/// repository at a time.
		std::uint64_t Split = 0U;

		auto operator<=>(const personal_discovery_statistics&) const = default;
	};

	/// \brief [[BPR-0989]]. One personal discovery scheduler per BPR profile.
	///
	/// \details Every window of the profile sees what it finds through the
	/// profile's queue host, so there is one scheduler however many windows
	/// are open. Each cycle first refreshes bounded metadata and submitted
	/// reviews for retained PRs, then searches the saved repositories of the
	/// adopted account's server and account for both reasons, one search per
	/// reason naming as many repositories as `personal_discovery_options`
	/// and the core's query bound allow (a pull request its retained read
	/// already read in full is not read again by a search, [[BPR-1680]]), at most
	/// `maximum_personal_discovery_in_flight` reads at a time, and tags every
	/// read with its session generation and refresh: a completion from an
	/// earlier session or refresh is refused. It runs when a session is
	/// adopted, when activated and the last refresh that ran to its end is
	/// older than the freshness window (one a session-wide failure stopped
	/// does not count; one that only failed for a repository or pull request
	/// does), when a repository it has not searched is saved, and
	/// on an explicit refresh; a rate limit holds it off until its
	/// retry-after has passed. Nothing a window opens or selects
	/// starts it. All calls run on the decision lane; provider completions are
	/// posted there. The port, lane, queue host, clock and diagnostics log
	/// outlive the host until it is quiescent.
	///
	/// [[BPR-1680]]. With a diagnostics log, each refresh records a start and
	/// an end, and each read a start (verbose) and an end with its requests,
	/// lane wait and run time, as `github.discovery` events whose detail is a
	/// `core::discovery_diagnosis`: counts and enums, no host, repository or
	/// account.
	class personal_discovery_host final
	{
	public:
		personal_discovery_host(personal_discovery_port&             aProvider,
								decision_executor&                   aDecisionLane,
								review_queue_application_host&       aQueue,
								const personal_discovery_clock_port& aClock,
								diagnostic_log_port*                 aLog     = nullptr,
								personal_discovery_options           aOptions = { });
		~personal_discovery_host();

		personal_discovery_host(const personal_discovery_host&)                    = delete;
		auto operator=(const personal_discovery_host&) -> personal_discovery_host& = delete;
		personal_discovery_host(personal_discovery_host&&)                         = delete;
		auto operator=(personal_discovery_host&&) -> personal_discovery_host&      = delete;

		/// \brief Adopts the profile's account session, or none.
		///
		/// \details A different session cancels what is in flight, forgets the
		/// last success, and starts a refresh under the new one. The same
		/// session again changes nothing.
		void adopt(std::optional<personal_discovery_session> aSession);
		/// \brief Refreshes when the last refresh that ran to its end is older
		/// than the freshness window and no rate limit holds discovery off.
		///
		/// \details [[BPR-1681]]. A refresh that ended with an unreadable
		/// repository or pull request counts: its failure is the same on the
		/// next activation. A refresh a session-wide failure (offline,
		/// authentication, rate limit) stopped does not, so activation retries it.
		void activate();
		/// \brief The explicit Refresh: refreshes now unless one is running or
		/// a rate limit holds discovery off, which is said instead.
		void refresh();
		/// \brief [[BPR-0990]]. Catches up with the account's saved repositories.
		///
		/// \details Refreshes when a repository is saved that the last refresh
		/// did not search, unless a refresh is running (it catches up when that
		/// one ends) or a rate limit holds discovery off. A forgotten repository
		/// is only no longer searched. The composition calls it after each queue
		/// projection. Newly restored rows with missing titles also trigger a
		/// bounded read; rows already considered, including failures and overflow,
		/// do not restart the cycle until activation or explicit Refresh.
		void reconcile();
		/// \brief [[BPR-1679]]. Gives way to the reads a window is waiting for,
		/// or resumes.
		///
		/// \details Discovery shares the profile's one GitHub lane with every
		/// pull request read, so a read a reviewer asked for waited behind it.
		/// The composition asks `core::home_loading_yields` after each window
		/// read changes. While yielding, nothing new is submitted, and every
		/// read of the current refresh still in flight is cancelled. A read
		/// whose cancellation is what answers it is asked again, under a new
		/// request identity, ahead of the rest once discovery resumes; one that
		/// answered before the cancellation reached it is applied as usual. The
		/// refresh is still running throughout, so Home keeps saying so.
		void yield(bool aYield);
		void shutdown();

		[[nodiscard]] auto yielding() const noexcept -> bool;
		[[nodiscard]] auto status() const noexcept -> const queue_discovery_status&;
		[[nodiscard]] auto statistics() const noexcept -> personal_discovery_statistics;
		[[nodiscard]] auto in_flight() const noexcept -> std::size_t;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PERSONAL_DISCOVERY_HOST_HPP
