#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_REFRESH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_REFRESH_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/review_queue.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief Identifies the session a refresh belongs to.
	///
	/// \details Host, account, and generation. A queue refreshes many pull
	/// requests at once, so it has more ways than any earlier surface to
	/// deliver a completion into the wrong place: the reviewer switches
	/// account, and answers to questions asked as the old one are still in
	/// flight.
	///
	/// `Generation` advances whenever the active session is replaced. It is the
	/// host's counter and never a clock, because two sessions started in the
	/// same millisecond must still be distinguishable.
	struct queue_session_id
	{
		std::string   Host;
		std::string   Account;
		std::uint64_t Generation = 0U;
		/// Exact accepted numeric account, never inferred from its display login.
		core::github_account_id ViewerAccount = { };

		auto operator<=>(const queue_session_id&) const = default;
	};

	/// \brief What the host asks the provider for.
	///
	/// \details `Validator` carries the entity tag the last answer supplied, so
	/// an unchanged pull request costs a conditional request and nothing else.
	struct queue_refresh_request
	{
		queue_session_id           Session;
		core::pull_request_ref     Ref;
		std::optional<std::string> Validator;

		auto operator<=>(const queue_refresh_request&) const = default;
	};

	/// \brief Names how the provider answered.
	enum class queue_refresh_status : std::uint8_t {
		// The provider supplied a new observation.
		changed,
		// The validator still holds. Nothing was transferred.
		unchanged,
		// The host is offline. Nothing was asked.
		offline,
		// A primary rate limit is in force.
		rate_limited,
		// A secondary rate limit is in force. Distinct from the primary one
		// because GitHub means something different by it: not "you have used
		// your allowance" but "slow down", and the two are not answered the
		// same way.
		secondary_rate_limited,
		// The request was cancelled before it completed.
		cancelled,
		// The pull request is no longer readable by this account.
		forbidden,
		// The pull request no longer exists, or was transferred away.
		gone,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(queue_refresh_status aStatus) noexcept
		-> std::string_view;

	/// \brief One answer, carrying the session it was asked under.
	///
	/// \details The session travels with the completion rather than being
	/// looked up when it arrives. A completion that had to ask "what session am
	/// I in" would get the answer for whatever session is current, which is
	/// exactly the mistake this exists to prevent.
	struct queue_refresh_completion
	{
		queue_session_id       Session;
		core::pull_request_ref Ref;
		queue_refresh_status   Status = queue_refresh_status::unchanged;

		std::optional<std::string>                    Validator;
		std::optional<core::pull_request_observation> Observation;

		// How long the provider says to wait before asking again. Present only
		// for the two rate-limited statuses.
		std::optional<std::uint32_t> RetryAfterSeconds;

		auto operator<=>(const queue_refresh_completion&) const = default;
	};

	/// \brief What the scheduler decided to do with one completion.
	enum class refresh_disposition : std::uint8_t {
		// Applied to the queue.
		applied,
		// Accepted, but nothing changed.
		unchanged,
		// Refused: the completion belongs to a session that is no longer
		// active.
		stale_session,
		// Held back: the host is offline or rate limited.
		deferred,
		// The item left the queue, and the reviewer is told why.
		withdrawn,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(refresh_disposition aDisposition) noexcept
		-> std::string_view;

	/// \brief Directly observable scheduler activity.
	struct queue_refresh_statistics
	{
		std::uint64_t Requested        = 0U;
		std::uint64_t Applied          = 0U;
		std::uint64_t RefusedAsStale   = 0U;
		std::uint64_t Deferred         = 0U;
		std::uint64_t ConditionalAsked = 0U;

		auto operator<=>(const queue_refresh_statistics&) const = default;
	};

	/// \brief Schedules queue refreshes and applies their answers.
	///
	/// \details Holds no provider and no clock. The host drives it: it says
	/// what to ask for, and it is told what came back. Backoff is expressed as
	/// a number of seconds to wait rather than as a moment to wait until,
	/// because a moment would need a clock to compare against and this must
	/// stay replayable.
	class queue_refresh_scheduler final
	{
	public:
		explicit queue_refresh_scheduler(queue_session_id aSession) noexcept;

		/// \brief The session refreshes are currently issued under.
		[[nodiscard]] auto session() const noexcept -> const queue_session_id&;

		/// \brief Replaces the active session.
		///
		/// \details Everything in flight under the old one will be refused when
		/// it arrives. Nothing is cancelled here, because the request is
		/// already gone; what matters is that its answer cannot land.
		void adopt(queue_session_id aSession);

		/// \brief Builds the request for one pull request.
		[[nodiscard]] auto request_for(const core::pull_request_ref& aRef)
			-> queue_refresh_request;

		/// \brief Applies one completion.
		[[nodiscard]] auto apply(const queue_refresh_completion& aCompletion)
			-> refresh_disposition;

		/// \brief How long the scheduler is holding off, in seconds.
		///
		/// \details Zero when it is not. The value doubles on each consecutive
		/// rate-limited answer and is capped, so a host that keeps refusing is
		/// asked less and less often rather than forever more urgently.
		[[nodiscard]] auto backoff_seconds() const noexcept -> std::uint32_t;

		/// \brief Whether the host is believed to be reachable.
		[[nodiscard]] auto online() const noexcept -> bool;

		[[nodiscard]] auto statistics() const noexcept
			-> queue_refresh_statistics;

		/// \brief The validator held for one pull request, if any.
		[[nodiscard]] auto validator_for(const core::pull_request_ref& aRef) const
			-> std::optional<std::string>;

		/// \brief The largest backoff the scheduler will ever ask for.
		static constexpr std::uint32_t MaximumBackoffSeconds = 900U;

	private:
		struct known_pull_request
		{
			core::pull_request_ref Ref;
			std::string            Validator;
		};

		[[nodiscard]] auto find(const core::pull_request_ref& aRef)
			-> known_pull_request*;

		queue_session_id                Session;
		std::vector<known_pull_request> Known;
		queue_refresh_statistics        Statistics;
		std::uint32_t                   Backoff = 0U;
		bool                            Online  = true;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_REFRESH_HPP
