#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PERSONAL_DISCOVERY_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PERSONAL_DISCOVERY_PORT_HPP

#include "bpr/application/github_provider_port.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-0988]]. Why a saved repository is searched for the viewer.
	enum class personal_discovery_reason : std::uint8_t {
		/// Open pull requests the viewer authored.
		authored,
		/// Open pull requests that ask the viewer, by name, for a review.
		requested,
		/// Metadata and submitted reviews of one already enrolled pull request.
		retained,
	};

	/// \brief The largest page one discovery search reads: a longer answer is
	/// partial, never exhausted.
	inline constexpr std::size_t maximum_personal_discovery_rows = 100U;
	/// \brief The requested-reviewers reads one discovery answer may issue.
	inline constexpr std::size_t maximum_personal_discovery_details = 30U;
	/// \brief Retained pull requests refreshed per cycle, with at most four review pages each.
	inline constexpr std::size_t maximum_personal_discovery_retained = 30U;
	inline constexpr std::size_t maximum_personal_discovery_reviews  = 400U;

	/// \brief One bounded personal search or retained pull request metadata read.
	///
	/// \details The exact accepted authority a pull request read carries, the
	/// viewer whose login is searched for, and the repository context that
	/// scopes every request. A retained read names one already enrolled pull
	/// request; no snapshot or diff is requested.
	struct personal_discovery_request
	{
		core::repository_request_context            Context;
		github_profile_authority                    ProfileAuthority;
		github_viewer_record                        Viewer;
		github_capability_cache_entry               CapabilityEvidence;
		github_account_transport                    AccountTransport;
		personal_discovery_reason                   Reason = personal_discovery_reason::authored;
		std::optional<core::application_session_id> AuthenticationSession;
		/// Present only for a retained read; never a discovery search qualifier.
		std::optional<core::pull_request_number> Number;
		std::uint64_t                            FactRevision = 0;

		auto operator<=>(const personal_discovery_request&) const = default;
	};

	/// \brief Submitted-review evidence without a review's prose or conversation.
	struct personal_discovery_review
	{
		std::uint64_t                  DatabaseId = 0U;
		std::optional<github_actor_id> Author;
		github_review_state            State = github_review_state::uninitialized;
		std::optional<commit_oid>      Commit;

		auto operator<=>(const personal_discovery_review&) const = default;
	};

	/// \brief One pull request a discovery read found, with what it established.
	///
	/// \details `Detailed` says the pull request's requested reviewers were
	/// read. For an authored row it gives `ReviewersAwaited`; a requested row is
	/// returned only when that read named the viewer as a user, so a team-only
	/// request never appears and `Detailed` is always true for it.
	struct personal_discovery_row
	{
		core::pull_request_number Number;
		std::string               Title;
		std::string               Author;
		bool                      Detailed         = false;
		std::uint32_t             ReviewersAwaited = 0U;
		/// Retained reads carry only the head and submitted-review evidence needed by Home.
		std::optional<commit_oid>              HeadOid;
		std::vector<personal_discovery_review> Reviews;
		bool                                   Closed = false;
		std::optional<bool>                    Draft  = std::nullopt;
		std::optional<github_actor_id>         AuthorIdentity;
		core::queue_waiting_facts              Waiting;
		/// Required title/reviews are complete; only the added summary hit a bound.
		bool WaitingLimited = false;
		/// [[BPR-1407]]. Age, size and comment count from the pull request read.
		core::queue_change_facts Change;

		auto operator<=>(const personal_discovery_row&) const = default;
	};

	/// \brief Whether a discovery answer may be trusted to say what is absent.
	enum class personal_discovery_completeness : std::uint8_t {
		/// Every open pull request matching the search was read.
		complete,
		/// GitHub reported its search incomplete.
		incomplete_results,
		/// More matched than one bounded page holds.
		result_limit,
		/// More requested rows than the detail reads could confirm.
		detail_limit,
	};

	/// \brief [[BPR-1680]]. What one discovery read cost, for the diagnostics
	/// log: the wire requests it sent, how long it waited for its turn on the
	/// provider's lane after it was submitted, and how long it then ran.
	struct personal_discovery_cost
	{
		std::uint32_t Requests         = 0U;
		std::uint32_t WaitMilliseconds = 0U;
		std::uint32_t RunMilliseconds  = 0U;

		auto operator<=>(const personal_discovery_cost&) const = default;
	};

	struct personal_discovery_success
	{
		personal_discovery_request          Request;
		std::vector<personal_discovery_row> Rows;
		personal_discovery_completeness     Completeness = personal_discovery_completeness::complete;
		github_rate_limit_state             RateLimits;
		personal_discovery_cost             Cost;

		auto operator<=>(const personal_discovery_success&) const = default;
	};

	struct personal_discovery_error
	{
		personal_discovery_request Request;
		github_provider_error      Error;
		personal_discovery_cost    Cost;

		auto operator<=>(const personal_discovery_error&) const = default;
	};

	struct personal_discovery_cancelled
	{
		personal_discovery_request Request;
		personal_discovery_cost    Cost;

		auto operator<=>(const personal_discovery_cancelled&) const = default;
	};

	using personal_discovery_completion         = std::variant<personal_discovery_success,
															   personal_discovery_error,
															   personal_discovery_cancelled>;
	using personal_discovery_completion_handler = std::function<void(personal_discovery_completion)>;

	/// \brief Reads the viewer's pull requests of one saved repository.
	///
	/// \details The same submission contract as the pull request port: a
	/// submission that returns normally invokes its completion exactly once,
	/// possibly before returning; cancellation is advisory.
	class personal_discovery_port
	{
	public:
		virtual ~personal_discovery_port()                                        = default;
		virtual void discover(personal_discovery_request            aRequest,
							  personal_discovery_completion_handler aCompletion)  = 0;
		virtual void cancel_discovery(const personal_discovery_request& aRequest) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PERSONAL_DISCOVERY_PORT_HPP
