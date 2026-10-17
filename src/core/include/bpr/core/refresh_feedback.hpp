#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REFRESH_FEEDBACK_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REFRESH_FEEDBACK_HPP

/// \file
/// \brief [[BPR-1575]]. What a refresh says while it runs and once it ends.
///
/// \details The owner, 2026-10-17: "I wish that refresh on both Home and PR
/// was more visible as an action. It's kind of hard to tell it's doing
/// anything."
///
/// Both refreshes, the open pull request's and Home's discovery, follow the
/// same lifecycle: started by the reviewer or by the application itself,
/// under way, then updated (with what changed, or nothing) or failed (offline,
/// rate limited, or another reason, with how long GitHub asked to wait). This
/// decides that lifecycle from the real read's own stages, what the update
/// changed from the two accepted observations either side of it, how old the
/// last update is in the words the reviewer reads, and what is announced. The
/// presenter supplies the clock readings; nothing here reads a clock.

#include "bpr/core/github_model.hpp"
#include "bpr/core/review_queue.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::core {
	/// \brief Who asked for a refresh.
	enum class refresh_trigger : std::uint8_t {
		/// The reviewer's Refresh: the button, its shortcut or its menu item.
		reviewer,
		/// The application, by itself: after publishing, after resolving a
		/// thread, or arriving at Home with a list that is out of date.
		automatic,
		/// Another window of the profile read the same thing; this window was
		/// handed its answer.
		elsewhere,
	};

	/// \brief Why a refresh failed, as the reviewer can act on it.
	enum class refresh_failure : std::uint8_t {
		/// GitHub could not be reached: the network, a timeout.
		offline,
		/// GitHub's rate limit, primary or secondary.
		rate_limited,
		/// The account is no longer signed in or allowed.
		signed_out,
		/// Anything else.
		other,
	};

	/// \brief Classifies a GitHub reason into what the reviewer can do about it.
	[[nodiscard]] auto refresh_failure_of(github_reason_code aReason) noexcept -> refresh_failure;

	/// \brief The pull request's state, as a refresh compares it.
	enum class refresh_status : std::uint8_t {
		open,
		draft,
		closed,
		merged,
	};

	/// \brief What one accepted read of a pull request held, as far as a
	/// refresh reports changes in it.
	struct pull_request_refresh_observation
	{
		/// Identities of the comments and replies, threads' and the pull
		/// request's own, in any order.
		std::vector<std::string> Comments{ };
		/// Identities of the submitted reviews, in any order.
		std::vector<std::string> Reviews{ };
		/// The head commit's identity.
		std::string    Head{ };
		std::uint64_t  Commits = 0U;
		refresh_status Status  = refresh_status::open;

		auto operator<=>(const pull_request_refresh_observation&) const = default;
	};

	/// \brief What one refresh brought that the reviewer had not seen.
	struct refresh_changes
	{
		std::uint32_t NewComments = 0U;
		std::uint32_t NewReviews  = 0U;
		/// The head moved: someone pushed.
		bool          NewPush    = false;
		std::uint32_t NewCommits = 0U;
		/// The state the pull request moved to, if it moved.
		std::optional<refresh_status> Status{ };
		/// [[BPR-0990]]. Home: pull requests discovery listed that it had not.
		std::uint32_t NewPullRequests = 0U;

		auto operator<=>(const refresh_changes&) const = default;
	};

	/// \brief Whether the refresh brought anything at all.
	[[nodiscard]] auto any(const refresh_changes& aChanges) noexcept -> bool;

	/// \brief What changed between two accepted reads of one pull request.
	/// \details Counts only what is new: a comment deleted since is not news,
	/// and an identity that was already there is not new however it changed.
	/// A head that moved is one push whatever it carried; the commit count's
	/// growth is said beside it.
	[[nodiscard]] auto compare_refresh(const pull_request_refresh_observation& aBefore,
									   const pull_request_refresh_observation& aAfter) -> refresh_changes;

	/// \brief [[BPR-0990]]. What Home's discovery found that it had not listed.
	[[nodiscard]] auto compare_refresh(const std::vector<pull_request_ref>& aBefore,
									   const std::vector<pull_request_ref>& aAfter) -> refresh_changes;

	/// \brief Where a refresh stands.
	enum class refresh_phase : std::uint8_t {
		/// Nothing has been refreshed in this window yet.
		never,
		/// A read is under way.
		refreshing,
		/// The last refresh answered.
		updated,
		/// The last refresh failed; what was shown is kept.
		failed,
	};

	/// \brief One surface's refresh, in the facts its presentation needs.
	struct refresh_state
	{
		refresh_phase   Phase   = refresh_phase::never;
		refresh_trigger Trigger = refresh_trigger::reviewer;
		/// What the last update brought; empty after a failure.
		refresh_changes Changes{ };
		refresh_failure Failure = refresh_failure::other;
		/// How long GitHub asked to wait, when it did.
		std::optional<std::uint32_t> RetryAfterSeconds{ };
		/// When the content shown was last brought up to date, in the
		/// application clock's seconds. Kept through later refreshes and failures.
		std::optional<std::int64_t> UpdatedAt{ };
		/// When the last refresh ended, either way.
		std::optional<std::int64_t> SettledAt{ };

		auto operator<=>(const refresh_state&) const = default;
	};

	/// \brief A read began.
	struct refresh_started
	{
		refresh_trigger Trigger = refresh_trigger::reviewer;

		auto operator<=>(const refresh_started&) const = default;
	};

	/// \brief A read was accepted.
	struct refresh_succeeded
	{
		refresh_changes Changes{ };
		std::int64_t    At = 0;

		auto operator<=>(const refresh_succeeded&) const = default;
	};

	/// \brief A read failed.
	struct refresh_failed
	{
		refresh_failure              Failure = refresh_failure::other;
		std::optional<std::uint32_t> RetryAfterSeconds{ };
		std::int64_t                 At = 0;

		auto operator<=>(const refresh_failed&) const = default;
	};

	/// \brief A read stopped before it answered, as by closing what it read.
	struct refresh_cancelled
	{
		auto operator<=>(const refresh_cancelled&) const = default;
	};

	using refresh_event = std::variant<refresh_started, refresh_succeeded, refresh_failed, refresh_cancelled>;

	/// \brief Applies one event of the read to a surface's refresh.
	/// \details A start while a read is already under way keeps it, and the
	/// reviewer's own gesture outranks one the application made, so pressing
	/// Refresh during an automatic update makes it the reviewer's. A success
	/// or failure ends the read. A cancel returns to what was settled before.
	/// An end with no read under way still records its outcome: an answer
	/// handed over by another window arrives that way.
	[[nodiscard]] auto apply_refresh(const refresh_state& aState, const refresh_event& aEvent) -> refresh_state;

	/// \brief How long ago the last update was, in the words' units.
	enum class refresh_age_unit : std::uint8_t {
		just_now,
		minutes,
		hours,
		days,
	};

	struct refresh_age
	{
		refresh_age_unit Unit  = refresh_age_unit::just_now;
		std::uint32_t    Count = 0U;

		auto operator<=>(const refresh_age&) const = default;
	};

	/// \brief The age of an update at `aNow`: just now under a minute (or for a
	/// clock that went back), then whole minutes, hours and days, rounded down.
	[[nodiscard]] auto refresh_age_at(std::int64_t aUpdatedAt, std::int64_t aNow) noexcept -> refresh_age;

	/// \brief Seconds from `aNow` until `refresh_age_at` would give other words,
	/// so a presenter can wake exactly then rather than polling.
	[[nodiscard]] auto refresh_age_next_change_seconds(std::int64_t aUpdatedAt, std::int64_t aNow) noexcept -> std::int64_t;

	/// \brief What a change in the refresh says to a screen reader, at most once.
	enum class refresh_announcement : std::uint8_t {
		none,
		/// "Refreshing": the reviewer's own gesture only. An update the
		/// application makes by itself is not announced until it brings news.
		started,
		/// "Updated: 2 new comments" or "Nothing new".
		updated,
		/// Why it failed and what can be done.
		failed,
	};

	/// \brief The announcement between two states of one surface's refresh.
	/// \details The reviewer's refresh is announced when it starts and when it
	/// ends either way. An automatic or handed-over one is announced only when
	/// it brought something or failed, so the application talking to itself
	/// never interrupts a reader.
	[[nodiscard]] auto refresh_announcement_between(const refresh_state& aBefore, const refresh_state& aAfter) noexcept
		-> refresh_announcement;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REFRESH_FEEDBACK_HPP
