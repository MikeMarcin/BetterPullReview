// [[BPR-1575]]. A refresh's lifecycle on Home and the pull request: who asked,
// what changed between the two accepted reads, how old the last update is in
// words, and what a screen reader is told. Automatic updates stay quiet unless
// they bring news; the reviewer's own refresh is always answered.
#include "bpr/core/refresh_feedback.hpp"

#include <cstdlib>
#include <iostream>
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
	std::cout << "refresh feedback tests passed\n";
	return EXIT_SUCCESS;
}
