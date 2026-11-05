#ifndef BPR_CORE_PULL_REQUEST_ACTIVITY_HPP
#define BPR_CORE_PULL_REQUEST_ACTIVITY_HPP

#include "bpr/core/review_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1850]]. Desktop notifications for activity on my pull
/// requests, decided from GitHub's notification threads.
/// \details The owner, 2026-11-03: notify when someone comments, reviews or
/// mentions me on my pull requests, within about a minute, at low overhead.
///
/// The host polls GitHub's notification list (participating threads only)
/// with `If-Modified-Since`; a 304 answer costs nothing against the rate
/// limit. Everything that decides is here: which threads are new since they
/// were last seen (thread identity and `updated_at`), which are my own or
/// already read, which are about a pull request I am looking at, how bursts
/// are folded into one notification per pull request, what the notification
/// says, where its click goes and when the next poll may run.
///
/// A notification names the repository and number and says what kind of
/// activity it was, from a fixed table. It never carries a title, a comment
/// or any other text from the pull request: the operating system's
/// notification store outlives the application and may be read or synced by
/// other software (`application::queue_notification_request` says the same).
namespace bpr::core {
	/// \brief Why GitHub put a thread in my notifications.
	enum class activity_reason : std::uint8_t {
		/// Activity on a thread I opened: my pull request.
		author,
		/// Activity on a thread I commented on.
		comment,
		/// I was @mentioned.
		mention,
		/// A team I am in was @mentioned.
		team_mention,
		/// My review was requested. The review queue's own notification
		/// covers this, so it is seen and never notified here.
		review_requested,
		/// Anything else GitHub names (assign, state change, subscribed...).
		other,
	};

	/// \brief What the thread's latest comment is, as far as GitHub says.
	enum class activity_anchor_kind : std::uint8_t {
		none,
		issue_comment,
		review_comment,
		review,
	};

	/// \brief The latest comment of a thread: its kind and REST identity.
	struct activity_anchor
	{
		activity_anchor_kind Kind = activity_anchor_kind::none;
		std::uint64_t        Id   = 0U;

		auto operator<=>(const activity_anchor&) const = default;
	};

	/// \brief One pull request notification thread, as the host read it.
	struct activity_thread
	{
		/// GitHub's thread identity, decimal digits.
		std::string  Id;
		std::int64_t UpdatedAt = 0;
		bool         Unread    = true;
		/// When I last read the thread; none when never.
		std::optional<std::int64_t> LastReadAt;
		activity_reason             Reason = activity_reason::other;
		/// `owner/name`.
		std::string     Repository;
		std::int64_t    Number = 0;
		activity_anchor Anchor;

		auto operator<=>(const activity_thread&) const = default;
	};

	/// \brief The most thread identities the memory keeps.
	inline constexpr std::size_t activity_seen_limit = 512U;
	/// \brief The longest `Last-Modified` the memory keeps; a longer one is not kept.
	inline constexpr std::size_t activity_last_modified_limit = 64U;
	/// \brief The version this build writes.
	inline constexpr std::int64_t activity_memory_version = 1;

	/// \brief One thread as last seen.
	struct activity_seen
	{
		std::string  Thread;
		std::int64_t UpdatedAt = 0;

		auto operator<=>(const activity_seen&) const = default;
	};

	/// \brief What one profile remembers across launches.
	struct activity_memory
	{
		/// The setting: notify about activity on my pull requests. On by default.
		bool Enabled = true;
		/// Whether a poll has recorded what was already there. Until then
		/// nothing is notified, so turning the setting on, or a first launch,
		/// does not announce old threads.
		bool Baselined = false;
		/// The account the memory belongs to: the canonical host and the numeric
		/// account, `host/123`. Another account's memory starts again.
		std::string Account;
		/// The last `Last-Modified`, sent back as `If-Modified-Since`.
		std::string LastModified;
		/// Ascending by `Thread`, unique, at most `activity_seen_limit`.
		std::vector<activity_seen> Seen;

		auto operator<=>(const activity_memory&) const = default;
	};

	/// \brief The stored rows as the store found them.
	struct stored_activity_memory
	{
		std::int64_t               Version   = 0;
		std::int64_t               Enabled   = 1;
		std::int64_t               Baselined = 0;
		std::string                Account;
		std::string                LastModified;
		std::vector<activity_seen> Seen;

		auto operator<=>(const stored_activity_memory&) const = default;
	};

	struct activity_memory_read
	{
		activity_memory Memory;
		/// False for a newer build's rows: they are read as nothing and never overwritten.
		bool Writable = true;
		/// The rows were damaged, an old version or a newer one.
		bool Recovered = false;

		auto operator<=>(const activity_memory_read&) const = default;
	};

	/// \brief Reads the stored rows; none is a fresh memory.
	[[nodiscard]] auto read_activity_memory(const std::optional<stored_activity_memory>& aStored) -> activity_memory_read;
	/// \brief The rows this build writes for \p aMemory.
	[[nodiscard]] auto stored_activity_rows(const activity_memory& aMemory) -> stored_activity_memory;

	/// \brief Turns the setting on or off. Turning it on starts a new baseline.
	[[nodiscard]] auto set_activity_enabled(activity_memory aMemory, bool aEnabled) -> activity_memory;
	/// \brief The memory for \p aAccount: unchanged when it is the memory's own,
	/// otherwise a new baseline that keeps only the setting.
	[[nodiscard]] auto activity_memory_for(activity_memory aMemory, std::string_view aAccount) -> activity_memory;

	/// \brief Where a notification's click goes: a pull request, or none when
	/// one notification stands for several (the window only comes forward).
	struct activity_target
	{
		std::optional<pull_request_ref> PullRequest;
		activity_anchor                 Anchor;

		auto operator<=>(const activity_target&) const = default;
	};

	/// \brief One notification to post.
	struct activity_notification
	{
		activity_target Target;
		/// How many threads it stands for.
		std::uint32_t Updates = 0U;
		/// How many pull requests it stands for: one, or several for a summary.
		std::uint32_t PullRequests = 1U;
		/// A thread it stands for mentioned me or my team.
		bool Mentioned = false;
		/// The kind of the only update; `none` for several.
		activity_anchor_kind Kind = activity_anchor_kind::none;

		auto operator<=>(const activity_notification&) const = default;
	};

	/// \brief The notification's title: `owner/name #123`, or the product's
	/// name for a summary.
	[[nodiscard]] auto activity_notification_title(const activity_notification& aNotification) -> std::string;
	/// \brief The notification's body, from a fixed table.
	[[nodiscard]] auto activity_notification_body(const activity_notification& aNotification) -> std::string;

	/// \brief How long after a pull request's notification more of its activity
	/// is held, to be posted as one notification when the window ends.
	inline constexpr std::int64_t activity_coalesce_seconds = 120;
	/// \brief More pull requests than this in one decision are posted as one summary.
	inline constexpr std::size_t activity_individual_limit = 3U;

	/// \brief Activity held for a pull request notified less than the coalescing window ago.
	struct activity_held
	{
		pull_request_ref     Ref;
		std::uint32_t        Updates   = 0U;
		bool                 Mentioned = false;
		activity_anchor_kind Kind      = activity_anchor_kind::none;
		activity_anchor      Latest;

		auto operator<=>(const activity_held&) const = default;
	};

	struct activity_notified
	{
		pull_request_ref Ref;
		std::int64_t     At = 0;

		auto operator<=>(const activity_notified&) const = default;
	};

	/// \brief What one launch keeps and never stores.
	struct activity_session
	{
		/// The last notification of each pull request, within the coalescing window.
		std::vector<activity_notified> Notified;
		std::vector<activity_held>     Held;
		/// Notifications posted since the last click, and the last one's target.
		std::uint32_t                  Outstanding = 0U;
		std::optional<activity_target> Last;

		auto operator<=>(const activity_session&) const = default;
	};

	/// \brief What a decision is made against.
	struct activity_context
	{
		/// The canonical host and login a pull request reference names.
		std::string  Host;
		std::string  Login;
		std::int64_t Now = 0;
		/// Pull requests open and focused in a window now.
		std::vector<pull_request_ref> Focused;

		auto operator<=>(const activity_context&) const = default;
	};

	struct activity_decision
	{
		activity_memory                    Memory;
		activity_session                   Session;
		std::vector<activity_notification> Notifications;
		/// Threads that were new and are not notified: mine, read, focused or of another reason.
		std::uint32_t Suppressed = 0U;
		/// [[BPR-1867]]. Every pull request with a new thread, notified or not,
		/// ascending and unique: a window showing one reads it again by itself.
		/// None while a baseline is taken.
		std::vector<pull_request_ref> Active;

		auto operator<=>(const activity_decision&) const = default;
	};

	/// \brief [[BPR-1867]]. Whether \p aShown is among \p aActive. GitHub's host,
	/// owner, repository and login names compare without case; a window may
	/// name the pull request as a pasted link spelt it.
	[[nodiscard]] auto activity_concerns(const std::vector<pull_request_ref>& aActive, const pull_request_ref& aShown) -> bool;

	/// \brief Decides one poll's answer.
	///
	/// \details \p aThreads is what a 200 answer listed. A 304 answer is an
	/// empty list that is not complete: it changes nothing seen and only
	/// releases held activity whose window has ended. \p aComplete says the
	/// list holds every thread GitHub would list; only then are threads it no
	/// longer lists forgotten. A thread
	/// is new when its identity is unseen or its `updated_at` is later than
	/// seen. A new thread is not notified when it is read (mine: GitHub marks a
	/// thread read for the one who acted), when its reason is not one of mine
	/// to hear about, or when its pull request is focused. The first answer
	/// after a baseline was lost only records what is there.
	[[nodiscard]] auto decide_activity(activity_memory                     aMemory,
									   activity_session                    aSession,
									   const std::vector<activity_thread>& aThreads,
									   bool                                aComplete,
									   const activity_context&             aContext) -> activity_decision;

	/// \brief A click on a notification: the target when only one is
	/// outstanding, none otherwise (the platform does not say which one was
	/// clicked). Clears the outstanding count.
	struct activity_activation
	{
		activity_session Session;
		activity_target  Target;

		auto operator<=>(const activity_activation&) const = default;
	};
	[[nodiscard]] auto activate_activity(activity_session aSession) -> activity_activation;

	/// \brief How a poll ended.
	enum class activity_poll_outcome : std::uint8_t {
		/// 200: the list changed.
		changed,
		/// 304: nothing changed since `If-Modified-Since`.
		unchanged,
		/// GitHub refused this sign-in the notification list (a token without
		/// the scope, a GitHub App token): no more polls this session.
		unsupported,
		/// 403 or 429 with a rate limit, or a secondary limit.
		rate_limited,
		/// Anything else: offline, 5xx, malformed.
		failed,
	};

	struct activity_poll_answer
	{
		activity_poll_outcome Outcome = activity_poll_outcome::failed;
		/// `X-Poll-Interval`, seconds.
		std::optional<std::uint32_t> PollInterval;
		/// `Retry-After`, seconds.
		std::optional<std::uint32_t> RetryAfter;
		/// `X-RateLimit-Remaining` and `X-RateLimit-Reset` (seconds since the epoch).
		std::optional<std::uint32_t> RateRemaining;
		std::optional<std::int64_t>  RateReset;

		auto operator<=>(const activity_poll_answer&) const = default;
	};

	/// \brief The shortest wait between polls; `X-Poll-Interval` may only lengthen it.
	inline constexpr std::uint32_t activity_minimum_interval_seconds = 60U;
	/// \brief The longest wait after failures.
	inline constexpr std::uint32_t activity_maximum_backoff_seconds = 600U;
	/// \brief The longest wait for a rate limit.
	inline constexpr std::uint32_t activity_maximum_rate_wait_seconds = 3600U;
	/// \brief Below this many requests left, a changed answer waits for the reset.
	inline constexpr std::uint32_t activity_rate_reserve = 100U;

	struct activity_schedule
	{
		std::uint32_t Interval = activity_minimum_interval_seconds;
		std::uint32_t Failures = 0U;
		bool          Stopped  = false;

		auto operator<=>(const activity_schedule&) const = default;
	};

	struct activity_next_poll
	{
		activity_schedule Schedule;
		/// Seconds until the next poll; none when polling has stopped.
		std::optional<std::uint32_t> Delay;

		auto operator<=>(const activity_next_poll&) const = default;
	};

	/// \brief When the next poll may run after \p aAnswer.
	[[nodiscard]] auto next_activity_poll(activity_schedule aSchedule, const activity_poll_answer& aAnswer, std::int64_t aNow) -> activity_next_poll;
} // namespace bpr::core

#endif
