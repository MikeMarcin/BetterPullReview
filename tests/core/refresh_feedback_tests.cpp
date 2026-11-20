// [[BPR-1575]]. A refresh's lifecycle on Home and the pull request: who asked,
// what changed between the two accepted reads, how old the last update is in
// words, and what a screen reader is told. Automatic updates stay quiet unless
// they bring news; the reviewer's own refresh is always answered. [[BPR-2315]]:
// a read the reviewer did not ask for never shows its wait.
#include "bpr/core/refresh_feedback.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto observation() -> pull_request_refresh_observation
	{
		return {
			.Comments = { "IC_1", "PRRC_1", "PRRC_2" },
			.Reviews  = { "PRR_1" },
			.Head     = "aaaa",
			.Commits  = 3U,
			.Status   = refresh_status::open,
		};
	}

	void test_nothing_new_is_nothing()
	{
		const auto changes = compare_refresh(observation(), observation());
		expect(!any(changes), "an identical read brings nothing");
		auto reordered = observation();
		std::swap(reordered.Comments.front(), reordered.Comments.back());
		expect(!any(compare_refresh(observation(), reordered)), "order is not news");
	}

	void test_new_comments_reviews_and_a_push_are_counted()
	{
		auto after = observation();
		after.Comments.emplace_back("IC_2");
		after.Comments.emplace_back("PRRC_3");
		after.Comments.emplace_back("PRRC_3");
		after.Reviews.emplace_back("PRR_2");
		after.Head         = "bbbb";
		after.Commits      = 5U;
		const auto changes = compare_refresh(observation(), after);
		expect(changes.NewComments == 2U, "two new comments, the duplicate counted once");
		expect(changes.NewReviews == 1U, "one new review");
		expect(changes.NewPush && changes.NewCommits == 2U, "the head moved: one push with two commits");
		expect(!changes.Status.has_value(), "the state did not move");
	}

	void test_deletions_and_force_pushes()
	{
		auto after = observation();
		after.Comments.pop_back();
		after.Head         = "cccc";
		after.Commits      = 2U;
		const auto changes = compare_refresh(observation(), after);
		expect(changes.NewComments == 0U, "a deleted comment is not news");
		expect(changes.NewPush && changes.NewCommits == 0U, "a force push is a push, with no commit growth to say");
	}

	void test_a_state_change_is_said()
	{
		auto after         = observation();
		after.Status       = refresh_status::merged;
		const auto changes = compare_refresh(observation(), after);
		expect(changes.Status == refresh_status::merged && any(changes), "a merge is news");
	}

	void test_home_counts_new_pull_requests()
	{
		const auto first  = pull_request_ref{ .Host = "github.com", .Account = "octo", .Repository = "o/r", .Number = 1 };
		const auto second = pull_request_ref{ .Host = "github.com", .Account = "octo", .Repository = "o/r", .Number = 2 };
		const auto third  = pull_request_ref{ .Host = "github.com", .Account = "octo", .Repository = "o/r", .Number = 3 };
		expect(compare_refresh(std::vector{ first }, std::vector{ first, second, third }).NewPullRequests == 2U, "two pull requests found");
		expect(!any(compare_refresh(std::vector{ first, second }, std::vector{ first })), "a pull request leaving the list is not news");
	}

	void test_failures_are_classified_for_what_can_be_done()
	{
		expect(refresh_failure_of(github_reason_code::network_unavailable) == refresh_failure::offline, "no network is offline");
		expect(refresh_failure_of(github_reason_code::timed_out) == refresh_failure::offline, "a timeout reads as offline");
		expect(refresh_failure_of(github_reason_code::rate_limited) == refresh_failure::rate_limited, "rate limited");
		expect(refresh_failure_of(github_reason_code::authentication_failed) == refresh_failure::signed_out, "a refused credential");
		expect(refresh_failure_of(github_reason_code::malformed_response) == refresh_failure::other, "anything else");
	}

	void test_the_lifecycle_follows_the_read()
	{
		auto state = refresh_state{ };
		state      = apply_refresh(state, refresh_started{ .Trigger = refresh_trigger::reviewer });
		expect(state.Phase == refresh_phase::refreshing && state.Trigger == refresh_trigger::reviewer, "the reviewer's refresh is under way");
		state = apply_refresh(state, refresh_succeeded{ .Changes = { .NewComments = 2U }, .At = 1'000 });
		expect(state.Phase == refresh_phase::updated && state.UpdatedAt == 1'000 && state.Changes.NewComments == 2U, "updated, with what it brought");
		state = apply_refresh(state, refresh_started{ .Trigger = refresh_trigger::automatic });
		state = apply_refresh(state, refresh_failed{ .Failure = refresh_failure::rate_limited, .RetryAfterSeconds = 120U, .At = 1'300 });
		expect(state.Phase == refresh_phase::failed && state.Failure == refresh_failure::rate_limited && state.RetryAfterSeconds == 120U, "failed, rate limited, with the wait");
		expect(state.UpdatedAt == 1'000 && !any(state.Changes), "a failure keeps when the shown content was updated, and brings nothing");
		state = apply_refresh(state, refresh_started{ .Trigger = refresh_trigger::reviewer });
		state = apply_refresh(state, refresh_cancelled{ });
		expect(state.Phase == refresh_phase::updated, "a cancelled read returns to what was settled");
		state = apply_refresh(refresh_state{ }, refresh_started{ });
		state = apply_refresh(state, refresh_cancelled{ });
		expect(state.Phase == refresh_phase::never, "a cancelled first refresh leaves nothing settled");
	}

	void test_the_reviewer_outranks_the_application()
	{
		auto state = apply_refresh(refresh_state{ }, refresh_started{ .Trigger = refresh_trigger::automatic });
		state      = apply_refresh(state, refresh_started{ .Trigger = refresh_trigger::reviewer });
		expect(state.Trigger == refresh_trigger::reviewer, "pressing Refresh during an automatic update makes it the reviewer's");
		state = apply_refresh(state, refresh_started{ .Trigger = refresh_trigger::automatic });
		expect(state.Trigger == refresh_trigger::reviewer, "an automatic start never demotes it");
	}

	void test_ages_read_as_words()
	{
		expect(refresh_age_at(100, 100) == refresh_age{ .Unit = refresh_age_unit::just_now }, "just now");
		expect(refresh_age_at(100, 159) == refresh_age{ .Unit = refresh_age_unit::just_now }, "under a minute is just now");
		expect(refresh_age_at(100, 50) == refresh_age{ .Unit = refresh_age_unit::just_now }, "a clock that went back is just now");
		expect(refresh_age_at(100, 220) == refresh_age{ .Unit = refresh_age_unit::minutes, .Count = 2U }, "two minutes");
		expect(refresh_age_at(0, 3 * 3'600 + 59) == refresh_age{ .Unit = refresh_age_unit::hours, .Count = 3U }, "three hours");
		expect(refresh_age_at(0, 2 * 86'400) == refresh_age{ .Unit = refresh_age_unit::days, .Count = 2U }, "two days");
		expect(refresh_age_next_change_seconds(100, 100) == 60, "just now changes at a minute");
		expect(refresh_age_next_change_seconds(100, 220) == 60, "two minutes changes at three");
		expect(refresh_age_next_change_seconds(100, 130) == 30, "the next minute boundary");
		expect(refresh_age_next_change_seconds(0, 3'600) == 3'600, "hours change on the hour");
	}

	void test_announcements()
	{
		const auto idle      = refresh_state{ };
		const auto reviewer  = apply_refresh(idle, refresh_started{ .Trigger = refresh_trigger::reviewer });
		const auto automatic = apply_refresh(idle, refresh_started{ .Trigger = refresh_trigger::automatic });
		expect(refresh_announcement_between(idle, reviewer) == refresh_announcement::started, "the reviewer's refresh is announced when it starts");
		expect(refresh_announcement_between(idle, automatic) == refresh_announcement::none, "an automatic one is not");
		expect(refresh_announcement_between(automatic, apply_refresh(automatic, refresh_started{ .Trigger = refresh_trigger::reviewer })) == refresh_announcement::started,
			   "the reviewer joining an automatic update is told it is under way");
		const auto nothing = refresh_succeeded{ .At = 5 };
		const auto news    = refresh_succeeded{ .Changes = { .NewComments = 1U }, .At = 5 };
		expect(refresh_announcement_between(reviewer, apply_refresh(reviewer, nothing)) == refresh_announcement::updated, "the reviewer hears that nothing is new");
		expect(refresh_announcement_between(automatic, apply_refresh(automatic, nothing)) == refresh_announcement::none, "an automatic update with nothing new is silent");
		expect(refresh_announcement_between(automatic, apply_refresh(automatic, news)) == refresh_announcement::updated, "an automatic update with news is announced");
		const auto failure = refresh_failed{ .Failure = refresh_failure::offline, .At = 5 };
		expect(refresh_announcement_between(automatic, apply_refresh(automatic, failure)) == refresh_announcement::failed, "any failure is announced");
		const auto elsewhere = apply_refresh(idle, refresh_started{ .Trigger = refresh_trigger::elsewhere });
		expect(refresh_announcement_between(elsewhere, apply_refresh(elsewhere, nothing)) == refresh_announcement::none, "a handed-over answer with nothing new is silent");
		const auto updated = apply_refresh(reviewer, nothing);
		expect(refresh_announcement_between(updated, updated) == refresh_announcement::none, "nothing changed, nothing said");
	}

	/// [[BPR-1867]]. Coming back to a pull request reads it again only once
	/// its update has aged, and never into a rate limit's wait.
	void test_a_return_reads_an_aged_pull_request()
	{
		const auto updated = apply_refresh(apply_refresh({ }, refresh_started{ }), refresh_succeeded{ .At = 1000 });
		expect(!refresh_due_on_return({ }, 5000), "nothing read yet, nothing to read again");
		expect(!refresh_due_on_return(updated, 1000 + refresh_return_seconds - 1), "a recent update is current");
		expect(refresh_due_on_return(updated, 1000 + refresh_return_seconds), "an update five minutes old is read again");
		expect(!refresh_due_on_return(updated, 900), "a clock that went back is not an age");
		const auto opened = refresh_state{ .UpdatedAt = 1000 };
		expect(refresh_due_on_return(opened, 1000 + refresh_return_seconds), "the first read ages as a refresh does");
		const auto running = apply_refresh(updated, refresh_started{ .Trigger = refresh_trigger::reviewer });
		expect(!refresh_due_on_return(running, 5000), "a read under way is not doubled");

		const auto limited = apply_refresh(updated, refresh_failed{ .Failure = refresh_failure::rate_limited, .RetryAfterSeconds = 600U, .At = 2000 });
		expect(!refresh_due_on_return(limited, 2599), "a rate limit's wait is waited out");
		expect(refresh_due_on_return(limited, 2600), "and then the aged update is read again");
		const auto unnamed = apply_refresh(updated, refresh_failed{ .Failure = refresh_failure::rate_limited, .At = 2000 });
		expect(!refresh_due_on_return(unnamed, 2000 + refresh_rate_wait_seconds - 1) && refresh_due_on_return(unnamed, 2000 + refresh_rate_wait_seconds),
			   "a rate limit without a wait is given a minute");
		const auto offline = apply_refresh(updated, refresh_failed{ .Failure = refresh_failure::offline, .At = 2000 });
		expect(refresh_due_on_return(offline, 2001), "an offline failure over an aged update tries again on return");
	}

	/// [[BPR-1867]]. An owed reread waits for a running read, and the
	/// application's own news waits for the reviewer to stop writing.
	void test_an_owed_reread_waits_its_turn()
	{
		expect(decide_reread({ }) == reread_step::none, "nothing owed, nothing read");
		expect(decide_reread({ .Owed = true }) == reread_step::now, "an owed reread runs");
		expect(decide_reread({ .Owed = true, .Reading = true }) == reread_step::wait, "after the read already running");
		expect(decide_reread({ .Owed = true, .Quiet = true, .Composing = true }) == reread_step::wait, "news waits while the reviewer writes");
		expect(decide_reread({ .Owed = true, .Quiet = false, .Composing = true }) == reread_step::now, "a publication's reread does not");
		expect(decide_reread({ .Owed = true, .Quiet = true }) == reread_step::now, "news runs once the reviewer stops");
	}
	// [[BPR-2315]]. The decision table: who started the read, what it brought
	// and how it ended, against whether it shows its wait and leaves a cue.
	void test_a_read_the_reviewer_did_not_ask_for_stays_out_of_the_way()
	{
		const auto settled = refresh_state{ .Phase = refresh_phase::updated, .UpdatedAt = 100, .SettledAt = 100 };
		const auto started = [&settled](const refresh_trigger aTrigger) { return apply_refresh(settled, refresh_started{ .Trigger = aTrigger }); };
		const auto news    = refresh_changes{ .NewComments = 1U };
		for (const auto phase : { refresh_phase::never, refresh_phase::updated, refresh_phase::failed }) {
			for (const auto trigger : { refresh_trigger::reviewer, refresh_trigger::automatic, refresh_trigger::elsewhere }) {
				expect(!refresh_quiet({ .Phase = phase, .Trigger = trigger }), "a settled refresh has no wait to keep quiet");
			}
		}
		expect(!refresh_quiet(started(refresh_trigger::reviewer)), "the reviewer's own read shows its wait");
		expect(refresh_quiet(started(refresh_trigger::automatic)), "the application's own read is quiet");
		expect(refresh_quiet(started(refresh_trigger::elsewhere)), "so is one another window made");
		const auto promoted = apply_refresh(started(refresh_trigger::automatic), refresh_started{ .Trigger = refresh_trigger::reviewer });
		expect(!refresh_quiet(promoted), "pressing Refresh during a quiet read makes it show");
		expect(!refresh_quiet(apply_refresh(started(refresh_trigger::reviewer), refresh_started{ .Trigger = refresh_trigger::automatic })),
			   "an automatic start never quietens the reviewer's read");
		for (const auto trigger : { refresh_trigger::reviewer, refresh_trigger::automatic, refresh_trigger::elsewhere }) {
			const auto before  = started(trigger);
			const auto nothing = apply_refresh(before, refresh_succeeded{ .At = 500 });
			const auto brought = apply_refresh(before, refresh_succeeded{ .Changes = news, .At = 500 });
			const auto failed  = apply_refresh(before, refresh_failed{ .Failure = refresh_failure::offline, .At = 500 });
			expect(refresh_settled_cue(before, nothing) == (trigger == refresh_trigger::reviewer), "nothing new is cued for the reviewer's refresh only");
			expect(refresh_settled_cue(before, brought), "news is cued whoever asked");
			expect(!refresh_settled_cue(before, failed), "a failure is said, not cued");
			expect(!refresh_quiet(nothing) && !refresh_quiet(failed), "an ended read is not under way");
			expect(!refresh_settled_cue(nothing, nothing), "the same settled state cues nothing again");
		}
		const auto handed = apply_refresh(settled, refresh_succeeded{ .Changes = news, .At = 600 });
		expect(refresh_settled_cue(settled, handed), "an answer handed over with news, with no read here, is cued once");
	}

	void test_each_cause_is_named_for_the_log()
	{
		for (const auto cause : { refresh_cause::reviewer, refresh_cause::activity, refresh_cause::return_to_window, refresh_cause::publication, refresh_cause::edit }) {
			const auto detail = std::string{ refresh_cause_detail_prefix } + std::string{ name(cause) };
			expect(parse_refresh_cause_detail(detail) == cause, "every cause reads back from its detail");
		}
		expect(name(refresh_cause::return_to_window) == "return" && name(refresh_cause::activity) == "activity", "the log's words");
		expect(!parse_refresh_cause_detail("refresh_cause=").has_value() && !parse_refresh_cause_detail("refresh_cause=timer").has_value()
				   && !parse_refresh_cause_detail("activity").has_value() && !parse_refresh_cause_detail("refresh_cause=activity ").has_value(),
			   "nothing else is a cause");
	}
} // namespace

auto main() -> int
{
	test_nothing_new_is_nothing();
	test_new_comments_reviews_and_a_push_are_counted();
	test_deletions_and_force_pushes();
	test_a_state_change_is_said();
	test_home_counts_new_pull_requests();
	test_failures_are_classified_for_what_can_be_done();
	test_the_lifecycle_follows_the_read();
	test_the_reviewer_outranks_the_application();
	test_ages_read_as_words();
	test_announcements();
	test_a_return_reads_an_aged_pull_request();
	test_an_owed_reread_waits_its_turn();
	test_a_read_the_reviewer_did_not_ask_for_stays_out_of_the_way();
	test_each_cause_is_named_for_the_log();
	std::cout << "refresh feedback tests passed\n";
	return EXIT_SUCCESS;
}
