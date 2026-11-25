// Previous and Next comment in the comments drawer step through the file
// shown and stop at its ends. The owner, 2026-11-24: "They should just go
// through comments in the current file and stop when it reaches either end."
#include "bpr/core/comment_steps.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto placed(std::string aId, const std::int64_t aShown, const std::uint32_t aLine, const std::size_t aOrder = 0U) -> core::comment_step_stop
	{
		auto stop  = core::comment_step_stop{ };
		stop.Id    = std::move(aId);
		stop.Shown = aShown;
		stop.Line  = aLine;
		stop.Order = aOrder;
		return stop;
	}

	[[nodiscard]] auto unplaced(std::string aId, const std::uint32_t aLine, const std::size_t aOrder = 0U) -> core::comment_step_stop
	{
		auto stop  = core::comment_step_stop{ };
		stop.Id    = std::move(aId);
		stop.Line  = aLine;
		stop.Order = aOrder;
		return stop;
	}

	[[nodiscard]] auto some(const std::string_view aId) -> std::optional<std::string>
	{
		return std::string{ aId };
	}

	void the_file_s_comments_are_stepped_in_line_order()
	{
		// Given out of order, as a refresh may give them; shown lines first,
		// top to bottom, then the rest by line.
		const auto stops = std::vector{ unplaced("whole-file", 0U), placed("line-22", 440, 22U), unplaced("outdated", 9U), placed("line-5", 100, 5U), placed("draft-10", 200, 10U) };
		const auto order = std::vector<std::string>{ "line-5", "draft-10", "line-22", "whole-file", "outdated" };
		expect(core::decide_comment_steps(stops, "line-22").Stops == order, "shown lines top to bottom, then the rest by line");
		const auto middle = core::decide_comment_steps(stops, "line-22");
		expect(middle.Previous == some("draft-10") && middle.Next == some("whole-file"), "from the middle, the comment before and the comment after");
		// The comparison's order, not the line number, decides among shown
		// lines: a left-side line may sit lower than a higher right-side one.
		const auto split = core::decide_comment_steps({ placed("right-30", 300, 30U), placed("left-40", 200, 40U) }, "left-40");
		expect(split.Stops == std::vector<std::string>{ "left-40", "right-30" } && split.Next == some("right-30"), "shown comments follow where the comparison draws them");
		// Two comments at one place keep the projection's order, then the
		// order given.
		const auto tied = core::decide_comment_steps({ placed("second", 100, 5U, 2U), placed("first", 100, 5U, 1U), placed("third", 100, 5U, 2U) }, "");
		expect(tied.Stops == std::vector<std::string>{ "first", "second", "third" }, "ties keep the projection's order, then the order given");
		expect(core::decide_comment_steps(stops, "line-22") == middle, "equal inputs decide equally");
	}

	void the_steps_stop_at_the_file_s_ends()
	{
		const auto stops = std::vector{ placed("a", 100, 5U), placed("b", 200, 10U), placed("c", 300, 22U) };
		const auto first = core::decide_comment_steps(stops, "a");
		expect(!first.Previous.has_value() && first.Next == some("b"), "at the first, no Previous; Next goes on");
		const auto last = core::decide_comment_steps(stops, "c");
		expect(last.Previous == some("b") && !last.Next.has_value(), "at the last, no Next: no wrap to the first and no other file");
		const auto only = core::decide_comment_steps({ placed("only", 100, 5U) }, "only");
		expect(!only.Previous.has_value() && !only.Next.has_value(), "the file's one comment open: neither step");
	}

	void with_none_open_the_steps_enter_at_the_ends()
	{
		const auto stops = std::vector{ placed("a", 100, 5U), placed("b", 200, 10U) };
		const auto none  = core::decide_comment_steps(stops, "");
		expect(none.Previous == some("b") && none.Next == some("a"), "none open: Next opens the first, Previous the last");
		// A comment open on another file, or one the filters took away, is
		// not in the file's order: the steps enter it at its ends.
		const auto elsewhere = core::decide_comment_steps(stops, "another-file-thread");
		expect(elsewhere.Previous == some("b") && elsewhere.Next == some("a"), "an open comment not on the file enters at the ends");
	}

	void a_file_without_comments_has_no_steps()
	{
		const auto empty = core::decide_comment_steps({ }, "");
		expect(empty.Stops.empty() && !empty.Previous.has_value() && !empty.Next.has_value(), "no comments: neither step");
		const auto stale = core::decide_comment_steps({ }, "a");
		expect(!stale.Previous.has_value() && !stale.Next.has_value(), "no comments with a stale open identity: neither step");
	}

	void the_filters_decide_the_stops()
	{
		// The drawer gives only what it lists after its filters: Hide
		// resolved takes the resolved comment at line 10 away, so the
		// steps pass over it, and the last listed is the end.
		const auto all      = std::vector{ placed("a", 100, 5U), placed("resolved", 200, 10U), placed("c", 300, 22U) };
		const auto filtered = std::vector{ placed("a", 100, 5U), placed("c", 300, 22U) };
		expect(core::decide_comment_steps(all, "a").Next == some("resolved"), "unfiltered, Next reaches the resolved comment");
		expect(core::decide_comment_steps(filtered, "a").Next == some("c"), "filtered, Next passes over it");
		expect(!core::decide_comment_steps(filtered, "c").Next.has_value(), "the last listed is the end");
	}
} // namespace

int main()
{
	the_file_s_comments_are_stepped_in_line_order();
	the_steps_stop_at_the_file_s_ends();
	with_none_open_the_steps_enter_at_the_ends();
	a_file_without_comments_has_no_steps();
	the_filters_decide_the_stops();
	std::cout << "COMMENT_STEPS_OK\n";
	return EXIT_SUCCESS;
}
