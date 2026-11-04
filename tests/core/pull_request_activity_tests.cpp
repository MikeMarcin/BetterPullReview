// [[BPR-1850]]. Which notification threads are new, which are mine, read or
// focused, how bursts fold into one notification per pull request, where a
// click goes, when the next poll runs, and how the stored rows degrade.
#include "bpr/core/pull_request_activity.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto thread(std::string aId, const std::int64_t aUpdated, const std::int64_t aNumber, const activity_reason aReason = activity_reason::author,
				const activity_anchor_kind aKind = activity_anchor_kind::issue_comment) -> activity_thread
	{
		return activity_thread{
			.Id         = std::move(aId),
			.UpdatedAt  = aUpdated,
			.Unread     = true,
			.LastReadAt = std::nullopt,
			.Reason     = aReason,
			.Repository = "octo/widgets",
			.Number     = aNumber,
			.Anchor     = { .Kind = aKind, .Id = static_cast<std::uint64_t>(aUpdated) },
		};
	}

	auto context(const std::int64_t aNow, std::vector<pull_request_ref> aFocused = { }) -> activity_context
	{
		return { .Host = "github.com", .Login = "me", .Now = aNow, .Focused = std::move(aFocused) };
	}

	auto ref(const std::int64_t aNumber) -> pull_request_ref
	{
		return { .Host = "github.com", .Account = "me", .Repository = "octo/widgets", .Number = aNumber };
	}

	/// A memory with its baseline taken over \p aThreads.
	auto baselined(const std::vector<activity_thread>& aThreads) -> activity_memory
	{
		const auto first = decide_activity({ }, { }, aThreads, true, context(1000));
		expect(first.Notifications.empty(), "the first answer only records a baseline");
		expect(first.Memory.Baselined, "and the baseline is taken");
		return first.Memory;
	}

	void test_new_and_seen()
	{
		const auto memory = baselined({ thread("1", 900, 7) });
		const auto again  = decide_activity(memory, { }, { thread("1", 900, 7) }, true, context(1060));
		expect(again.Notifications.empty(), "a thread seen at the same updated_at is not new");

		const auto later = decide_activity(memory, { }, { thread("1", 1050, 7), thread("2", 1055, 8, activity_reason::mention) }, true, context(1060));
		expect(later.Notifications.size() == 2U, "a later updated_at and an unseen thread are each new");
		expect(later.Notifications[0].Target.PullRequest == ref(7) && later.Notifications[0].Kind == activity_anchor_kind::issue_comment, "first by pull request");
		expect(activity_notification_title(later.Notifications[0]) == "octo/widgets #7", "title names repository and number");
		expect(activity_notification_body(later.Notifications[0]) == "New comment.", "body from the fixed table");
		expect(activity_notification_body(later.Notifications[1]) == "You were mentioned.", "a mention says so");
		expect(later.Memory.Seen.size() == 2U && later.Memory.Seen[0].UpdatedAt == 1050, "seen records the new updated_at");

		const auto replay = decide_activity(later.Memory, later.Session, { thread("1", 1050, 7), thread("2", 1055, 8, activity_reason::mention) }, true, context(1500));
		expect(replay.Notifications.empty(), "the same answer after a relaunch's memory notifies nothing");
	}

	void test_suppression()
	{
		const auto memory = baselined({ });
		auto       mine   = thread("3", 1100, 9);
		mine.Unread       = false;
		auto read         = thread("4", 1100, 10);
		read.LastReadAt   = 1100;
		const auto other  = thread("5", 1100, 11, activity_reason::review_requested);
		const auto issue  = thread("6", 1100, 12, activity_reason::other);
		const auto result = decide_activity(memory, { }, { mine, read, other, issue }, true, context(1200));
		expect(result.Notifications.empty(), "my own (read) threads, review requests and other reasons are not notified");
		expect(result.Suppressed == 4U, "and are counted as suppressed");
		expect(result.Memory.Seen.size() == 4U, "but they are seen");

		const auto focused = decide_activity(memory, { }, { thread("7", 1100, 13) }, true, context(1200, { ref(13) }));
		expect(focused.Notifications.empty() && focused.Suppressed == 1U, "a pull request open and focused is not notified");
		const auto after = decide_activity(focused.Memory, focused.Session, { thread("7", 1100, 13) }, true, context(1300));
		expect(after.Notifications.empty(), "nor later, once seen");
	}

	void test_coalescing()
	{
		const auto memory = baselined({ });
		const auto burst  = decide_activity(memory, { }, { thread("1", 1100, 7, activity_reason::author, activity_anchor_kind::review), thread("2", 1101, 7, activity_reason::mention), thread("3", 1102, 7) }, true, context(1200));
		expect(burst.Notifications.size() == 1U, "a burst on one pull request is one notification");
		expect(burst.Notifications[0].Updates == 3U && burst.Notifications[0].Mentioned, "counting every thread");
		expect(activity_notification_body(burst.Notifications[0]) == "3 new updates, including a mention.", "and saying so");

		// More within the coalescing window is held, then posted once.
		const auto held = decide_activity(burst.Memory, burst.Session, { thread("1", 1250, 7), thread("4", 1255, 7) }, true, context(1260));
		expect(held.Notifications.empty() && held.Session.Held.size() == 1U && held.Session.Held[0].Updates == 2U, "activity within the window is held");
		const auto quiet = decide_activity(held.Memory, held.Session, { }, false, context(1300));
		expect(quiet.Notifications.empty(), "a 304 inside the window keeps holding");
		const auto released = decide_activity(quiet.Memory, quiet.Session, { }, false, context(1200 + activity_coalesce_seconds));
		expect(released.Notifications.size() == 1U && released.Notifications[0].Updates == 2U, "the window's end posts what was held, once");
		expect(released.Memory == quiet.Memory, "a 304 changes nothing seen");

		// Held activity for a pull request focused by then is dropped.
		const auto dropped = decide_activity(quiet.Memory, quiet.Session, { }, false, context(1200 + activity_coalesce_seconds, { ref(7) }));
		expect(dropped.Notifications.empty() && dropped.Session.Held.empty(), "held activity is dropped once the pull request is focused");

		// Many pull requests at once are one summary that opens Home.
		auto many = std::vector<activity_thread>{ };
		for (auto number = 1; number <= 5; ++number) {
			many.push_back(thread(std::to_string(100 + number), 1100, number));
		}
		const auto summary = decide_activity(memory, { }, many, true, context(1200));
		expect(summary.Notifications.size() == 1U && !summary.Notifications[0].Target.PullRequest && summary.Notifications[0].PullRequests == 5U, "more than the limit is one summary");
		expect(activity_notification_title(summary.Notifications[0]) == "BetterPullReview" && activity_notification_body(summary.Notifications[0]) == "New activity on 5 pull requests.", "summary wording");
	}

	void test_activation()
	{
		const auto memory = baselined({ });
		const auto one    = decide_activity(memory, { }, { thread("1", 1100, 7, activity_reason::author, activity_anchor_kind::review_comment) }, true, context(1200));
		const auto click  = activate_activity(one.Session);
		expect(click.Target.PullRequest == ref(7) && click.Target.Anchor.Kind == activity_anchor_kind::review_comment, "one outstanding notification opens its pull request");
		expect(click.Session.Outstanding == 0U, "and is consumed");
		expect(!activate_activity(click.Session).Target.PullRequest, "a second click goes Home");

		const auto two     = decide_activity(memory, { }, { thread("1", 1100, 7), thread("2", 1100, 8) }, true, context(1200));
		const auto unclear = activate_activity(two.Session);
		expect(!unclear.Target.PullRequest, "with two outstanding the click cannot say which: Home");
	}

	void test_schedule()
	{
		auto next = next_activity_poll({ }, { .Outcome = activity_poll_outcome::unchanged, .PollInterval = 60U }, 0);
		expect(next.Delay == 60U, "a 304 waits the poll interval");
		next = next_activity_poll({ }, { .Outcome = activity_poll_outcome::unchanged, .PollInterval = 30U }, 0);
		expect(next.Delay == activity_minimum_interval_seconds, "never faster than the floor");
		next = next_activity_poll({ }, { .Outcome = activity_poll_outcome::changed, .PollInterval = 120U }, 0);
		expect(next.Delay == 120U && next.Schedule.Interval == 120U, "never faster than X-Poll-Interval");
		next = next_activity_poll(next.Schedule, { .Outcome = activity_poll_outcome::unchanged }, 0);
		expect(next.Delay == 120U, "the interval is remembered");

		auto failing = activity_schedule{ };
		auto delays  = std::vector<std::uint32_t>{ };
		for (auto attempt = 0; attempt < 6; ++attempt) {
			const auto failed = next_activity_poll(failing, { .Outcome = activity_poll_outcome::failed }, 0);
			failing           = failed.Schedule;
			delays.push_back(*failed.Delay);
		}
		expect(delays == std::vector<std::uint32_t>{ 120U, 240U, 480U, 600U, 600U, 600U }, "failures back off to the cap");
		expect(next_activity_poll(failing, { .Outcome = activity_poll_outcome::unchanged }, 0).Delay == 60U, "a success resets the backoff");

		expect(next_activity_poll({ }, { .Outcome = activity_poll_outcome::rate_limited, .RetryAfter = 900U }, 0).Delay == 900U, "Retry-After is obeyed");
		expect(next_activity_poll({ }, { .Outcome = activity_poll_outcome::rate_limited, .RateReset = 1500 }, 1000).Delay == 500U, "else the reset");
		expect(next_activity_poll({ }, { .Outcome = activity_poll_outcome::changed, .RateRemaining = 20U, .RateReset = 2000 }, 1000).Delay == 1000U, "a changed answer with little rate limit left waits for the reset");
		expect(next_activity_poll({ }, { .Outcome = activity_poll_outcome::unchanged, .RateRemaining = 20U, .RateReset = 2000 }, 1000).Delay == 60U, "a 304 costs nothing and keeps the interval");

		const auto stopped = next_activity_poll({ }, { .Outcome = activity_poll_outcome::unsupported }, 0);
		expect(!stopped.Delay && stopped.Schedule.Stopped, "a sign-in GitHub refuses stops polling");
	}

	void test_memory()
	{
		expect(read_activity_memory(std::nullopt) == activity_memory_read{ }, "no rows: a fresh memory, enabled");

		auto memory         = activity_memory{ };
		memory.Baselined    = true;
		memory.Account      = "github.com/42";
		memory.LastModified = "Fri, 30 Oct 2026 04:02:16 GMT";
		memory.Seen         = { { .Thread = "1", .UpdatedAt = 5 }, { .Thread = "2", .UpdatedAt = 6 } };
		const auto round    = read_activity_memory(stored_activity_rows(memory));
		expect(round.Memory == memory && round.Writable && !round.Recovered, "the rows round-trip");

		auto newer        = stored_activity_rows(memory);
		newer.Version     = activity_memory_version + 1;
		const auto future = read_activity_memory(newer);
		expect(!future.Writable && future.Recovered && future.Memory == activity_memory{ }, "a newer build's rows read as nothing and are not written");

		auto old            = stored_activity_rows(memory);
		old.Version         = 0;
		old.Enabled         = 0;
		const auto obsolete = read_activity_memory(old);
		expect(obsolete.Writable && obsolete.Recovered && !obsolete.Memory.Enabled && !obsolete.Memory.Baselined, "an old version starts again, keeping the setting");

		auto damaged = stored_activity_rows(memory);
		damaged.Seen.push_back({ .Thread = "not-a-thread", .UpdatedAt = 1 });
		const auto repaired = read_activity_memory(damaged);
		expect(repaired.Recovered && repaired.Memory.Seen.size() == 2U, "a damaged seen row is dropped");

		const auto off = set_activity_enabled(memory, false);
		expect(!off.Enabled && off.Baselined, "turning off keeps the memory");
		const auto on = set_activity_enabled(off, true);
		expect(on.Enabled && !on.Baselined && on.Seen.empty() && on.LastModified.empty(), "turning on starts a new baseline");

		const auto same = activity_memory_for(memory, "github.com/42");
		expect(same == memory, "the same account keeps its memory");
		const auto other = activity_memory_for(off, "ghe.example/7");
		expect(other.Account == "ghe.example/7" && !other.Baselined && !other.Enabled, "another account starts again, keeping the setting");

		auto large      = activity_memory{ };
		large.Baselined = true;
		auto threads    = std::vector<activity_thread>{ };
		for (auto index = 0; index < static_cast<int>(activity_seen_limit) + 10; ++index) {
			threads.push_back(thread(std::to_string(1000 + index), 100 + index, 1));
		}
		const auto bounded = decide_activity(large, { }, threads, false, context(5000));
		expect(bounded.Memory.Seen.size() == activity_seen_limit, "seen is bounded");
		expect(std::ranges::is_sorted(bounded.Memory.Seen, { }, &activity_seen::Thread), "and ordered");
	}
} // namespace

auto main() -> int
{
	test_new_and_seen();
	test_suppression();
	test_coalescing();
	test_activation();
	test_schedule();
	test_memory();
	std::cout << "pull-request-activity:pass\n";
	return EXIT_SUCCESS;
}
