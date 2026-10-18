// [[BPR-1576]]. The application's one loading-state pattern: which look a
// waiting pane takes from its real facts, when a wait is said, and what is
// announced. A fast load never flashes; a slow one says what it waits on;
// content that arrived is never dimmed; a failure always offers Retry.
#include "bpr/core/pending_presentation.hpp"

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

	[[nodiscard]] auto loading(const bool aContent, const std::uint32_t aElapsed, const bool aPartial = false) -> pending_facts
	{
		return { .Activity = pending_activity::loading, .HasContent = aContent, .Partial = aPartial, .ElapsedMilliseconds = aElapsed };
	}

	void test_a_first_load_is_quiet_then_said_then_slow()
	{
		expect(decide_pending_look(loading(false, 0U)) == pending_look::placeholder, "a first load starts with placeholders only");
		expect(decide_pending_look(loading(false, pending_reveal_milliseconds - 1U)) == pending_look::placeholder, "a load faster than the reveal never shows a spinner");
		expect(decide_pending_look(loading(false, pending_reveal_milliseconds)) == pending_look::loading, "at the reveal the turning mark and words appear");
		expect(decide_pending_look(loading(false, pending_slow_milliseconds)) == pending_look::loading_slow, "a slow first load says what it waits on");
		expect(pending_turning(pending_look::loading) && pending_turning(pending_look::loading_slow), "a revealed first load turns");
		expect(!pending_turning(pending_look::placeholder), "placeholders alone do not turn");
		expect(pending_slow(pending_look::loading_slow) && !pending_slow(pending_look::loading), "only past the slow mark are the words about the wait");
	}

	void test_a_refresh_keeps_content_readable_and_never_blocks()
	{
		expect(decide_pending_look(loading(true, 0U)) == pending_look::quiet, "a refresh shows the retained content as it is before the reveal");
		expect(decide_pending_look(loading(true, pending_reveal_milliseconds)) == pending_look::refreshing, "after the reveal the content is veiled with the rule");
		expect(decide_pending_look(loading(true, pending_slow_milliseconds)) == pending_look::refreshing_slow, "a slow refresh says what it waits on");
		expect(pending_veiled(pending_look::refreshing) && pending_veiled(pending_look::refreshing_slow), "retained content is veiled while it is replaced");
		expect(!pending_veiled(pending_look::quiet) && !pending_veiled(pending_look::partial) && !pending_veiled(pending_look::failed_retained),
			   "content is only veiled while a revealed refresh replaces it");
	}

	void test_partial_content_says_what_is_still_coming()
	{
		expect(decide_pending_look(loading(true, 0U, true)) == pending_look::quiet, "a part that arrived quickly is shown as it is");
		expect(decide_pending_look(loading(true, pending_reveal_milliseconds, true)) == pending_look::partial, "after the reveal a quiet line says what is still coming");
		expect(decide_pending_look(loading(true, pending_slow_milliseconds * 2U, true)) == pending_look::partial, "partial content is never veiled, however long");
		expect(!pending_veiled(pending_look::partial) && pending_turning(pending_look::partial), "what arrived is never dimmed; the line turns");
	}

	void test_a_failure_keeps_what_was_shown_and_offers_retry()
	{
		const auto empty    = decide_pending_look({ .Activity = pending_activity::failed });
		const auto retained = decide_pending_look({ .Activity = pending_activity::failed, .HasContent = true });
		expect(empty == pending_look::failed && retained == pending_look::failed_retained, "a failure keeps retained content at full strength");
		expect(pending_retry(empty) && pending_retry(retained), "every failure offers Retry");
		expect(!pending_busy(empty) && !pending_busy(retained), "a failed pane is not busy");
		expect(decide_pending_look({ .Activity = pending_activity::idle, .HasContent = true }) == pending_look::ready, "idle is ready");
	}

	void test_busy_is_true_from_the_first_moment()
	{
		for (const auto look : { pending_look::placeholder, pending_look::quiet, pending_look::loading, pending_look::partial, pending_look::refreshing }) {
			expect(pending_busy(look), "a loading pane is busy for assistive technology even before anything is drawn");
		}
		expect(!pending_busy(pending_look::ready), "a ready pane is not busy");
	}

	void test_announcements_follow_what_was_seen()
	{
		expect(pending_announcement_between(pending_look::ready, pending_look::placeholder) == pending_announcement::none, "a wait is not announced before it shows");
		expect(pending_announcement_between(pending_look::placeholder, pending_look::ready) == pending_announcement::none, "a fast load is never announced");
		expect(pending_announcement_between(pending_look::quiet, pending_look::ready) == pending_announcement::none, "a fast refresh is never announced by its pane");
		expect(pending_announcement_between(pending_look::placeholder, pending_look::loading) == pending_announcement::started, "a revealed wait is announced");
		expect(pending_announcement_between(pending_look::loading, pending_look::loading_slow) == pending_announcement::slow, "a wait turning slow says what it waits on");
		expect(pending_announcement_between(pending_look::loading_slow, pending_look::ready) == pending_announcement::finished, "a revealed wait announces its end");
		expect(pending_announcement_between(pending_look::refreshing, pending_look::ready) == pending_announcement::finished, "a revealed refresh announces its end");
		expect(pending_announcement_between(pending_look::placeholder, pending_look::failed) == pending_announcement::failed, "a failure is always announced");
		expect(pending_announcement_between(pending_look::quiet, pending_look::failed_retained) == pending_announcement::failed, "a failed refresh is always announced");
		expect(pending_announcement_between(pending_look::loading, pending_look::loading) == pending_announcement::none, "the same look says nothing again");
	}

	void test_the_presenter_wakes_only_when_time_changes_the_look()
	{
		expect(pending_next_change_milliseconds(loading(false, 100U)) == pending_reveal_milliseconds - 100U, "a quiet wait wakes at the reveal");
		expect(pending_next_change_milliseconds(loading(false, pending_reveal_milliseconds)) == pending_slow_milliseconds - pending_reveal_milliseconds,
			   "a revealed wait wakes at the slow mark");
		expect(pending_next_change_milliseconds(loading(false, pending_slow_milliseconds)) == 0U, "a slow wait changes only with its facts");
		expect(pending_next_change_milliseconds(loading(true, pending_reveal_milliseconds, true)) == 0U, "a partial line needs no slow mark");
		expect(pending_next_change_milliseconds({ .Activity = pending_activity::idle }) == 0U, "a ready pane never wakes");
		expect(pending_next_change_milliseconds({ .Activity = pending_activity::failed }) == 0U, "a failed pane never wakes");
	}
} // namespace

auto main() -> int
{
	test_a_first_load_is_quiet_then_said_then_slow();
	test_a_refresh_keeps_content_readable_and_never_blocks();
	test_partial_content_says_what_is_still_coming();
	test_a_failure_keeps_what_was_shown_and_offers_retry();
	test_busy_is_true_from_the_first_moment();
	test_announcements_follow_what_was_seen();
	test_the_presenter_wakes_only_when_time_changes_the_look();
	std::cout << "pending presentation tests passed\n";
	return EXIT_SUCCESS;
}
