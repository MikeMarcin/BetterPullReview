// RFC-BPR-0089 M4: where a comment on a commit-filtered diff goes (owner
// decision Q1, AC07) and which threads are marked on it, under core-only
// (AC08). The lines are BPR-Test #116's, where GitHub itself was measured: a
// 30-line file; C1 changes lines 5, 15 and 25; C2, the head, inserts two
// lines at the top and changes C1's line 5 again.
#include "bpr/core/filtered_comment.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>

namespace {
	using namespace bpr;
	using core::diff_side;
	using placement = core::filtered_comment_placement;
	using refusal   = core::filtered_comment_refusal;

	void expect(const bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(1);
		}
	}

	constexpr auto oid(const std::uint8_t aSeed) noexcept -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[19]  = 0x5AU;
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	constexpr auto base = oid(0x10);
	constexpr auto c1   = oid(0x01);
	constexpr auto c2   = oid(0x02);

	/// The merge base against C1: lines 5, 15 and 25 (zero-based 4, 14, 24) replaced.
	auto base_to_c1() -> core::line_correspondence
	{
		return { .LeftLines = 30U, .RightLines = 30U, .Unchanged = { { 0U, 0U, 4U }, { 5U, 5U, 9U }, { 15U, 15U, 9U }, { 25U, 25U, 5U } } };
	}

	/// C1 against the head: two lines inserted at the top, C1's line 5 replaced.
	auto c1_to_head() -> core::line_correspondence
	{
		return { .LeftLines = 30U, .RightLines = 32U, .Unchanged = { { 0U, 2U, 4U }, { 5U, 7U, 25U } } };
	}

	/// The merge base against the head: two lines inserted at the top, lines
	/// 5, 15 and 25 replaced. Its hunks are what GitHub resolves at the head.
	auto base_to_head() -> core::line_correspondence
	{
		return { .LeftLines = 30U, .RightLines = 32U, .Unchanged = { { 0U, 2U, 4U }, { 5U, 7U, 9U }, { 15U, 17U, 9U }, { 25U, 27U, 5U } } };
	}

	auto source(const core::filtered_diff_endpoints aDiff) -> core::filtered_comment_source
	{
		return { .Head = c2, .MergeBase = base, .Diff = aDiff, .Chain = { c1, c2 }, .Fork = base, .Lines = std::nullopt };
	}

	void test_correspondence()
	{
		const auto lines = c1_to_head();
		expect(core::valid(lines), "the measured correspondence is valid");
		expect(core::corresponding_right_line(lines, 14U) == std::optional{ 16U }, "C1's line 15 is the head's line 17");
		expect(!core::corresponding_right_line(lines, 4U), "C1's line 5 is not at the head");
		expect(core::corresponding_left_line(lines, 0U) == std::nullopt, "an inserted head line has no C1 line");
		expect(core::corresponding_left_line(lines, 16U) == std::optional{ 14U }, "and back");
		expect(!core::corresponding_right_line(lines, 30U), "a line past the document has none");

		// GitHub took C1's line 8 (a context line of the line-5 hunk) and
		// refused line 10 with 422: context is three lines.
		const auto commit = base_to_c1();
		expect(core::line_in_change_context(commit, diff_side::right, 4U), "a changed line is in its hunk");
		expect(core::line_in_change_context(commit, diff_side::right, 7U), "line 8 is in the line-5 hunk's context");
		expect(core::line_in_change_context(commit, diff_side::right, 1U), "line 2 is too");
		expect(!core::line_in_change_context(commit, diff_side::right, 0U), "line 1 is not");
		expect(!core::line_in_change_context(commit, diff_side::right, 9U), "line 10 is outside every hunk");
		expect(core::line_in_change_context(commit, diff_side::right, 11U), "line 12 is in the line-15 hunk");
		expect(!core::line_in_change_context(commit, diff_side::right, 30U), "a line past the document is in none");

		// A pure deletion still has a place on the side that keeps no line.
		const auto deleted = core::line_correspondence{ .LeftLines = 10U, .RightLines = 9U, .Unchanged = { { 0U, 0U, 5U }, { 6U, 5U, 4U } } };
		expect(core::valid(deleted), "a deletion's correspondence is valid");
		expect(core::line_in_change_context(deleted, diff_side::right, 2U) && core::line_in_change_context(deleted, diff_side::right, 7U),
			   "the right lines three either side of a deletion are in its hunk");
		expect(!core::line_in_change_context(deleted, diff_side::right, 1U) && !core::line_in_change_context(deleted, diff_side::right, 8U),
			   "and the fourth are not");
		// A new file: every line is a change.
		const auto added = core::line_correspondence{ .LeftLines = 0U, .RightLines = 40U, .Unchanged = { } };
		expect(core::line_in_change_context(added, diff_side::right, 39U), "every line of an added file is in its hunk");

		expect(!core::valid(core::line_correspondence{ .LeftLines = 3U, .RightLines = 3U, .Unchanged = { { 0U, 0U, 4U } } }), "a run past the document is invalid");
		expect(!core::valid(core::line_correspondence{ .LeftLines = 9U, .RightLines = 9U, .Unchanged = { { 2U, 2U, 2U }, { 1U, 5U, 1U } } }), "runs out of order are invalid");
		expect(!core::valid(core::line_correspondence{ .LeftLines = 9U, .RightLines = 9U, .Unchanged = { { 0U, 0U, 2U }, { 2U, 2U, 1U } } }), "runs touching on both sides are invalid");
		expect(!core::valid(core::line_correspondence{ .LeftLines = 9U, .RightLines = 9U, .Unchanged = { { 0U, 0U, 0U } } }), "an empty run is invalid");
	}

	void test_alignment()
	{
		auto alignment           = core::file_alignment{ };
		alignment.LeftLineCount  = 6U;
		alignment.RightLineCount = 7U;
		alignment.Segments       = {
			{ .Kind = core::alignment_kind::equal, .Left = { 0U, 2U }, .Right = { 0U, 2U } },
			{ .Kind = core::alignment_kind::replacement, .Left = { 2U, 3U }, .Right = { 2U, 4U } },
			{ .Kind = core::alignment_kind::equal, .Left = { 3U, 6U }, .Right = { 4U, 7U } },
		};
		const auto lines = core::correspondence_of(alignment);
		expect(lines && lines->Unchanged == std::vector<core::unchanged_run>{ { 0U, 0U, 2U }, { 3U, 4U, 3U } }, "equal segments are the unchanged runs");
		alignment.Content = core::diff_content_state::binary;
		expect(!core::correspondence_of(alignment), "a binary alignment has no lines");
	}

	void test_right_side()
	{
		// Hide C2: the file's diff is the merge base to C1.
		auto hidden = source({ .Left = base, .Right = c1 });
		expect(core::filtered_comment_needs_lines(hidden), "a diff ending before the head needs its lines read");
		expect(core::place_filtered_comment(hidden, diff_side::right, 14U, std::nullopt).Refusal == refusal::lines_pending, "nothing is placed before the lines are read");
		hidden.Lines = core::filtered_comment_lines{ };
		expect(core::place_filtered_comment(hidden, diff_side::right, 14U, std::nullopt).Refusal == refusal::lines_unavailable, "lines that could not be read place nothing");
		hidden.Lines = core::filtered_comment_lines{ .RightToHead = c1_to_head(), .BaseToRight = base_to_c1(), .LeftToBase = std::nullopt, .BaseToHead = std::nullopt };
		expect(core::place_filtered_comment(hidden, diff_side::right, 14U, std::nullopt).Refusal == refusal::lines_unavailable, "without the head's hunks nothing moves there");
		hidden.Lines->BaseToHead = base_to_head();

		const auto unchanged = core::place_filtered_comment(hidden, diff_side::right, 14U, std::nullopt);
		expect(unchanged.Placement == placement::head && unchanged.Commit == c2 && unchanged.Side == diff_side::right && unchanged.Line == 16U,
			   "a line unchanged at the head goes to the head at the head's line");
		// GitHub refused head line 12 of this shape with 422 (BPR-Test #117):
		// unchanged, but outside every hunk of the merge base to the head.
		expect(core::place_filtered_comment(hidden, diff_side::right, 9U, std::nullopt).Refusal == refusal::outside_github_diff,
			   "a line no change shows at the head is refused before Publish");

		const auto changed = core::place_filtered_comment(hidden, diff_side::right, 4U, std::nullopt);
		expect(changed.Placement == placement::earlier_commit && changed.Commit == c1 && changed.Side == diff_side::right && changed.Line == 4U,
			   "a line the head changed goes to C1 at C1's line, outdated");

		// A line the head changed that no hunk of the merge base to C1 shows.
		hidden.Lines->RightToHead = core::line_correspondence{ .LeftLines = 30U, .RightLines = 30U, .Unchanged = { { 0U, 0U, 9U }, { 10U, 10U, 20U } } };
		expect(core::place_filtered_comment(hidden, diff_side::right, 9U, std::nullopt).Refusal == refusal::not_on_commit,
			   "a changed line GitHub cannot resolve on C1 is refused before Publish");
		expect(core::place_filtered_comment(hidden, diff_side::right, 8U, std::nullopt).Placement == placement::head, "its unchanged neighbour still goes to the head");
		hidden.Lines->BaseToRight.reset();
		expect(core::place_filtered_comment(hidden, diff_side::right, 9U, std::nullopt).Refusal == refusal::lines_unavailable, "without the commit's hunks nothing goes to C1");

		expect(core::place_filtered_comment(hidden, diff_side::right, 14U, 12U).Refusal == refusal::range, "a range moves nowhere off the head");
		expect(core::place_filtered_comment(hidden, diff_side::right, 12U, 14U).Refusal == refusal::outside_pull_request, "a range ending before it starts is refused");

		// A diff whose right side is the head is the head's own document.
		const auto head   = source({ .Left = c1, .Right = c2 });
		const auto onHead = core::place_filtered_comment(head, diff_side::right, 20U, 18U);
		expect(onHead.Placement == placement::head && onHead.Commit == c2 && onHead.Line == 20U, "a right side that is the head keeps its lines, ranges too");
	}

	void test_left_side()
	{
		// The merge base on the left: GitHub's LEFT lines.
		const auto fromBase = core::place_filtered_comment(source({ .Left = base, .Right = c1 }), diff_side::left, 6U, 4U);
		expect(fromBase.Placement == placement::head && fromBase.Side == diff_side::left && fromBase.Line == 6U && fromBase.Commit == c2,
			   "a left side that is the merge base is LEFT at the head, ranges too");

		// Show only C2: the left side is C1.
		auto onlyHead = source({ .Left = c1, .Right = c2 });
		expect(core::filtered_comment_needs_lines(onlyHead), "a left side after the merge base needs its lines read");
		expect(core::place_filtered_comment(onlyHead, diff_side::left, 6U, std::nullopt).Refusal == refusal::lines_pending, "pending until read");
		onlyHead.Lines  = core::filtered_comment_lines{ .RightToHead = std::nullopt, .BaseToRight = std::nullopt, .LeftToBase = core::line_correspondence{ .LeftLines = 30U, .RightLines = 30U, .Unchanged = base_to_c1().Unchanged }, .BaseToHead = base_to_head() };
		const auto kept = core::place_filtered_comment(onlyHead, diff_side::left, 6U, std::nullopt);
		expect(kept.Placement == placement::head && kept.Side == diff_side::left && kept.Line == 6U, "a C1 line the merge base has unchanged is LEFT at its base line");
		expect(core::place_filtered_comment(onlyHead, diff_side::left, 4U, std::nullopt).Refusal == refusal::base_line_changed, "a line C1 changed has no LEFT line");
		expect(core::place_filtered_comment(onlyHead, diff_side::left, 9U, std::nullopt).Refusal == refusal::outside_github_diff,
			   "a base line no change shows at the head has no LEFT line GitHub resolves (BPR-Test #117)");
		expect(core::place_filtered_comment(onlyHead, diff_side::left, 6U, 5U).Refusal == refusal::range, "nor does a range on it");
	}

	void test_outside_pull_request()
	{
		const auto stranger = oid(0x77);
		auto       outside  = source({ .Left = base, .Right = stranger });
		outside.Lines       = core::filtered_comment_lines{ .RightToHead = c1_to_head(), .BaseToRight = base_to_c1(), .LeftToBase = std::nullopt, .BaseToHead = base_to_head() };
		expect(core::place_filtered_comment(outside, diff_side::right, 4U, std::nullopt).Refusal == refusal::outside_pull_request,
			   "a right commit outside the pull request is refused before anything is written");
		expect(core::place_filtered_comment(source({ .Left = stranger, .Right = c1 }), diff_side::right, 4U, std::nullopt).Refusal == refusal::outside_pull_request,
			   "so is a left commit outside it");
		auto moved  = source({ .Left = base, .Right = c1 });
		moved.Chain = { c1 };
		expect(core::place_filtered_comment(moved, diff_side::right, 4U, std::nullopt).Refusal == refusal::outside_pull_request, "a chain that does not end at the head is refused");
		expect(core::place_filtered_comment(source({ .Left = c1, .Right = c1 }), diff_side::right, 4U, std::nullopt).Refusal == refusal::outside_pull_request,
			   "a diff of one commit with itself is refused");
	}

	void test_thread_lines()
	{
		const auto hidden = core::filtered_diff_endpoints{ .Left = base, .Right = c1 };
		// Written on C1 at line 5 and outdated: GitHub kept commit_id at C1.
		const auto outdated = core::filtered_thread_anchor{ .Side = diff_side::right, .Line = std::nullopt, .OriginalLine = 5U, .Commit = c1, .OriginalCommit = c1 };
		expect(core::filtered_thread_line(outdated, hidden, base) == std::optional{ 4U }, "a thread written on C1 is marked on C1's diff");
		// Written on C1 at line 15 and moved to the head by GitHub.
		const auto moved = core::filtered_thread_anchor{ .Side = diff_side::right, .Line = 17U, .OriginalLine = 15U, .Commit = c2, .OriginalCommit = c1 };
		expect(core::filtered_thread_line(moved, hidden, base) == std::optional{ 14U }, "a thread GitHub moved on is marked where it was written");
		expect(core::filtered_thread_line(moved, { .Left = c1, .Right = c2 }, base) == std::optional{ 16U }, "and at its current line on a diff ending at the head");
		// Written at the head.
		const auto head = core::filtered_thread_anchor{ .Side = diff_side::right, .Line = 17U, .OriginalLine = 17U, .Commit = c2, .OriginalCommit = c2 };
		expect(!core::filtered_thread_line(head, hidden, base), "a head thread stays in the drawer on C1's diff");
		// LEFT lines count in the merge base.
		const auto left = core::filtered_thread_anchor{ .Side = diff_side::left, .Line = 5U, .OriginalLine = 5U, .Commit = c2, .OriginalCommit = c1 };
		expect(core::filtered_thread_line(left, hidden, base) == std::optional{ 4U }, "a LEFT thread is marked when the left side is the merge base");
		expect(!core::filtered_thread_line(left, { .Left = c1, .Right = c2 }, base), "and not when it is a later commit");
		expect(!core::filtered_thread_line(core::filtered_thread_anchor{ .Side = diff_side::right, .Line = 0U, .OriginalLine = std::nullopt, .Commit = c1, .OriginalCommit = std::nullopt }, hidden, base),
			   "a line GitHub never numbers marks nothing");
	}
} // namespace

auto main() -> int
{
	test_correspondence();
	test_alignment();
	test_right_side();
	test_left_side();
	test_outside_pull_request();
	test_thread_lines();
	std::cout << "filtered comment decisions pass\n";
	return 0;
}
