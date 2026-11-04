#include "bpr/core/pull_request_activity.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto thread_identity(const std::string_view aId) noexcept -> bool
		{
			return !aId.empty() && aId.size() <= 32U && std::ranges::all_of(aId, [](const char aByte) { return aByte >= '0' && aByte <= '9'; });
		}

		[[nodiscard]] auto ordered_seen(std::vector<activity_seen> aSeen) -> std::vector<activity_seen>
		{
			std::erase_if(aSeen, [](const activity_seen& aEntry) { return !thread_identity(aEntry.Thread); });
			std::ranges::sort(aSeen, [](const activity_seen& aLeft, const activity_seen& aRight) {
				return aLeft.Thread != aRight.Thread ? aLeft.Thread < aRight.Thread : aLeft.UpdatedAt > aRight.UpdatedAt;
			});
			const auto duplicates = std::ranges::unique(aSeen, [](const activity_seen& aLeft, const activity_seen& aRight) { return aLeft.Thread == aRight.Thread; });
			aSeen.erase(duplicates.begin(), duplicates.end());
			if (aSeen.size() > activity_seen_limit) {
				// The least recently updated go first.
				std::ranges::sort(aSeen, [](const activity_seen& aLeft, const activity_seen& aRight) {
					return aLeft.UpdatedAt != aRight.UpdatedAt ? aLeft.UpdatedAt > aRight.UpdatedAt : aLeft.Thread < aRight.Thread;
				});
				aSeen.resize(activity_seen_limit);
				std::ranges::sort(aSeen, { }, &activity_seen::Thread);
			}
			return aSeen;
		}

		[[nodiscard]] auto seen_at(const std::vector<activity_seen>& aSeen, const std::string& aThread) -> std::optional<std::int64_t>
		{
			const auto found = std::ranges::lower_bound(aSeen, aThread, { }, &activity_seen::Thread);
			if (found == aSeen.end() || found->Thread != aThread) {
				return std::nullopt;
			}
			return found->UpdatedAt;
		}

		[[nodiscard]] auto heard(const activity_reason aReason) noexcept -> bool
		{
			switch (aReason) {
			case activity_reason::author:
			case activity_reason::comment:
			case activity_reason::mention:
			case activity_reason::team_mention:
				return true;
			case activity_reason::review_requested:
			case activity_reason::other:
				return false;
			}
			return false;
		}

		[[nodiscard]] auto mentions(const activity_reason aReason) noexcept -> bool
		{
			return aReason == activity_reason::mention || aReason == activity_reason::team_mention;
		}

		void fold(activity_held& aInto, const activity_held& aMore)
		{
			const auto first = aInto.Updates == 0U;
			aInto.Updates += aMore.Updates;
			aInto.Mentioned = aInto.Mentioned || aMore.Mentioned;
			aInto.Kind      = first ? aMore.Kind : activity_anchor_kind::none;
			if (aMore.Latest.Kind != activity_anchor_kind::none) {
				aInto.Latest = aMore.Latest;
			}
		}

		[[nodiscard]] auto notification_of(const activity_held& aHeld) -> activity_notification
		{
			return activity_notification{
				.Target       = activity_target{ .PullRequest = aHeld.Ref, .Anchor = aHeld.Latest },
				.Updates      = aHeld.Updates,
				.PullRequests = 1U,
				.Mentioned    = aHeld.Mentioned,
				.Kind         = aHeld.Updates == 1U ? aHeld.Kind : activity_anchor_kind::none,
			};
		}

		[[nodiscard]] auto plural(const std::uint32_t aCount, const std::string_view aOne, const std::string_view aMany) -> std::string
		{
			return std::to_string(aCount) + " " + std::string{ aCount == 1U ? aOne : aMany };
		}
	} // namespace

	auto read_activity_memory(const std::optional<stored_activity_memory>& aStored) -> activity_memory_read
	{
		if (!aStored) {
			return { };
		}
		if (aStored->Version > activity_memory_version) {
			return { .Memory = { }, .Writable = false, .Recovered = true };
		}
		if (aStored->Version < activity_memory_version || (aStored->Enabled != 0 && aStored->Enabled != 1)
			|| (aStored->Baselined != 0 && aStored->Baselined != 1)) {
			auto fresh = activity_memory{ };
			// A known setting survives an otherwise damaged row.
			fresh.Enabled = aStored->Enabled != 0;
			return { .Memory = std::move(fresh), .Writable = true, .Recovered = true };
		}
		auto memory      = activity_memory{ };
		memory.Enabled   = aStored->Enabled == 1;
		memory.Baselined = aStored->Baselined == 1;
		memory.Account   = aStored->Account;
		if (aStored->LastModified.size() <= activity_last_modified_limit) {
			memory.LastModified = aStored->LastModified;
		}
		memory.Seen        = ordered_seen(aStored->Seen);
		const auto damaged = memory.Seen.size() != aStored->Seen.size() || memory.LastModified != aStored->LastModified;
		return { .Memory = std::move(memory), .Writable = true, .Recovered = damaged };
	}

	auto stored_activity_rows(const activity_memory& aMemory) -> stored_activity_memory
	{
		return stored_activity_memory{
			.Version      = activity_memory_version,
			.Enabled      = aMemory.Enabled ? 1 : 0,
			.Baselined    = aMemory.Baselined ? 1 : 0,
			.Account      = aMemory.Account,
			.LastModified = aMemory.LastModified,
			.Seen         = aMemory.Seen,
		};
	}

	auto set_activity_enabled(activity_memory aMemory, const bool aEnabled) -> activity_memory
	{
		if (aMemory.Enabled == aEnabled) {
			return aMemory;
		}
		aMemory.Enabled = aEnabled;
		if (aEnabled) {
			aMemory.Baselined = false;
			aMemory.LastModified.clear();
			aMemory.Seen.clear();
		}
		return aMemory;
	}

	auto activity_memory_for(activity_memory aMemory, const std::string_view aAccount) -> activity_memory
	{
		if (aMemory.Account == aAccount) {
			return aMemory;
		}
		auto fresh    = activity_memory{ };
		fresh.Enabled = aMemory.Enabled;
		fresh.Account = std::string{ aAccount };
		return fresh;
	}

	auto activity_notification_title(const activity_notification& aNotification) -> std::string
	{
		if (!aNotification.Target.PullRequest) {
			return "BetterPullReview";
		}
		return aNotification.Target.PullRequest->Repository + " #" + std::to_string(aNotification.Target.PullRequest->Number);
	}

	auto activity_notification_body(const activity_notification& aNotification) -> std::string
	{
		if (!aNotification.Target.PullRequest) {
			return "New activity on " + plural(aNotification.PullRequests, "pull request.", "pull requests.");
		}
		if (aNotification.Updates > 1U) {
			return std::to_string(aNotification.Updates) + " new updates" + (aNotification.Mentioned ? ", including a mention." : ".");
		}
		if (aNotification.Mentioned) {
			return "You were mentioned.";
		}
		switch (aNotification.Kind) {
		case activity_anchor_kind::review:         return "New review.";
		case activity_anchor_kind::review_comment: return "New review comment.";
		case activity_anchor_kind::issue_comment:  return "New comment.";
		case activity_anchor_kind::none:           break;
		}
		return "New activity.";
	}

	auto decide_activity(activity_memory                     aMemory,
						 activity_session                    aSession,
						 const std::vector<activity_thread>& aThreads,
						 const bool                          aComplete,
						 const activity_context&             aContext) -> activity_decision
	{
		auto decision = activity_decision{ };
		// Forget coalescing windows that have ended; what they held is posted below.
		auto due = std::vector<activity_held>{ };
		std::erase_if(aSession.Notified, [&](const activity_notified& aEntry) {
			if (aContext.Now - aEntry.At < activity_coalesce_seconds) {
				return false;
			}
			const auto held = std::ranges::find(aSession.Held, aEntry.Ref, &activity_held::Ref);
			if (held != aSession.Held.end()) {
				due.push_back(*held);
				aSession.Held.erase(held);
			}
			return true;
		});

		auto       seen     = std::vector<activity_seen>{ };
		auto       fresh    = std::vector<activity_held>{ };
		const auto baseline = !aMemory.Baselined;
		for (const auto& thread : aThreads) {
			if (!thread_identity(thread.Id)) {
				continue;
			}
			seen.push_back({ .Thread = thread.Id, .UpdatedAt = thread.UpdatedAt });
			const auto before = seen_at(aMemory.Seen, thread.Id);
			if (baseline || (before && *before >= thread.UpdatedAt)) {
				continue;
			}
			const auto ref  = pull_request_ref{ .Host = aContext.Host, .Account = aContext.Login, .Repository = thread.Repository, .Number = thread.Number };
			const auto read = !thread.Unread || (thread.LastReadAt && *thread.LastReadAt >= thread.UpdatedAt);
			if (read || !heard(thread.Reason) || thread.Number <= 0 || thread.Repository.empty()
				|| std::ranges::find(aContext.Focused, ref) != aContext.Focused.end()) {
				++decision.Suppressed;
				continue;
			}
			const auto more = activity_held{ .Ref = ref, .Updates = 1U, .Mentioned = mentions(thread.Reason), .Kind = thread.Anchor.Kind, .Latest = thread.Anchor };
			if (auto found = std::ranges::find(fresh, ref, &activity_held::Ref); found != fresh.end()) {
				fold(*found, more);
			} else {
				fresh.push_back(more);
			}
		}

		// Seen: what this answer lists, and, when it may not list everything,
		// what was seen before.
		if (!aThreads.empty() || aComplete) {
			if (!aComplete) {
				for (const auto& entry : aMemory.Seen) {
					seen.push_back(entry);
				}
			}
			aMemory.Seen = ordered_seen(std::move(seen));
		}
		aMemory.Baselined = aMemory.Baselined || !aThreads.empty() || aComplete;

		auto posts = std::move(due);
		for (auto& activity : fresh) {
			// A pull request notified within the window holds its activity.
			if (std::ranges::find(aSession.Notified, activity.Ref, &activity_notified::Ref) != aSession.Notified.end()) {
				if (auto held = std::ranges::find(aSession.Held, activity.Ref, &activity_held::Ref); held != aSession.Held.end()) {
					fold(*held, activity);
				} else {
					aSession.Held.push_back(activity);
				}
				continue;
			}
			if (auto post = std::ranges::find(posts, activity.Ref, &activity_held::Ref); post != posts.end()) {
				fold(*post, activity);
			} else {
				posts.push_back(activity);
			}
		}
		// A pull request focused now does not need what was held for it.
		std::erase_if(posts, [&](const activity_held& aHeld) {
			return std::ranges::find(aContext.Focused, aHeld.Ref) != aContext.Focused.end();
		});
		std::ranges::sort(posts, { }, &activity_held::Ref);

		for (const auto& post : posts) {
			aSession.Notified.push_back({ .Ref = post.Ref, .At = aContext.Now });
		}
		if (posts.size() > activity_individual_limit) {
			auto summary         = activity_notification{ };
			summary.PullRequests = static_cast<std::uint32_t>(posts.size());
			for (const auto& post : posts) {
				summary.Updates += post.Updates;
				summary.Mentioned = summary.Mentioned || post.Mentioned;
			}
			decision.Notifications.push_back(summary);
		} else {
			for (const auto& post : posts) {
				decision.Notifications.push_back(notification_of(post));
			}
		}
		for (const auto& notification : decision.Notifications) {
			++aSession.Outstanding;
			aSession.Last = notification.Target;
		}
		decision.Memory  = std::move(aMemory);
		decision.Session = std::move(aSession);
		return decision;
	}

	auto activate_activity(activity_session aSession) -> activity_activation
	{
		auto target          = aSession.Outstanding == 1U && aSession.Last ? *aSession.Last : activity_target{ };
		aSession.Outstanding = 0U;
		aSession.Last.reset();
		return { .Session = std::move(aSession), .Target = std::move(target) };
	}

	auto next_activity_poll(activity_schedule aSchedule, const activity_poll_answer& aAnswer, const std::int64_t aNow) -> activity_next_poll
	{
		if (aAnswer.PollInterval) {
			// Never faster than GitHub asks, nor than the floor.
			aSchedule.Interval = std::clamp(*aAnswer.PollInterval, activity_minimum_interval_seconds, activity_maximum_rate_wait_seconds);
		}
		const auto untilReset = [&]() -> std::optional<std::uint32_t> {
			if (!aAnswer.RateReset || *aAnswer.RateReset <= aNow) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(std::min<std::int64_t>(*aAnswer.RateReset - aNow, activity_maximum_rate_wait_seconds));
		};
		switch (aAnswer.Outcome) {
		case activity_poll_outcome::changed:
		case activity_poll_outcome::unchanged: {
			aSchedule.Failures = 0U;
			auto delay         = aSchedule.Interval;
			// A changed answer spends the rate limit; with little left, wait for the reset.
			if (aAnswer.Outcome == activity_poll_outcome::changed && aAnswer.RateRemaining && *aAnswer.RateRemaining < activity_rate_reserve) {
				delay = std::max(delay, untilReset().value_or(delay));
			}
			return { .Schedule = aSchedule, .Delay = delay };
		}
		case activity_poll_outcome::unsupported:
			aSchedule.Stopped = true;
			return { .Schedule = aSchedule, .Delay = std::nullopt };
		case activity_poll_outcome::rate_limited: {
			aSchedule.Failures = std::min(aSchedule.Failures + 1U, 16U);
			auto wait          = aAnswer.RetryAfter ? std::min(*aAnswer.RetryAfter, activity_maximum_rate_wait_seconds) : untilReset().value_or(0U);
			if (wait == 0U) {
				wait = std::min(aSchedule.Interval << std::min(aSchedule.Failures, 6U), activity_maximum_rate_wait_seconds);
			}
			return { .Schedule = aSchedule, .Delay = std::max(wait, aSchedule.Interval) };
		}
		case activity_poll_outcome::failed:
			break;
		}
		aSchedule.Failures = std::min(aSchedule.Failures + 1U, 16U);
		const auto backoff = std::min(aSchedule.Interval << std::min(aSchedule.Failures, 6U), activity_maximum_backoff_seconds);
		return { .Schedule = aSchedule, .Delay = std::max(backoff, aSchedule.Interval) };
	}
} // namespace bpr::core
