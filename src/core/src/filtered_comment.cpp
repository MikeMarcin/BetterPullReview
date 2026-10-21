#include "bpr/core/filtered_comment.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

namespace bpr::core {
	namespace {
		/// \brief The run holding `aLine` on one side, by binary search.
		[[nodiscard]] auto run_holding(const line_correspondence& aLines, const diff_side aSide, const std::uint32_t aLine) noexcept
			-> const unchanged_run*
		{
			const auto begin = [aSide](const unchanged_run& aRun) { return aSide == diff_side::left ? aRun.Left : aRun.Right; };
			const auto found = std::ranges::upper_bound(aLines.Unchanged, aLine, { }, begin);
			if (found == aLines.Unchanged.begin()) {
				return nullptr;
			}
			const auto& run = *std::prev(found);
			return aLine - begin(run) < run.Length ? &run : nullptr;
		}

		[[nodiscard]] auto contains(const std::vector<git_commit_oid>& aChain, const git_commit_oid& aCommit) noexcept -> bool
		{
			return std::ranges::find(aChain, aCommit) != aChain.end();
		}

		[[nodiscard]] auto refused(const filtered_comment_refusal aRefusal) noexcept -> filtered_comment_target
		{
			return { .Placement = filtered_comment_placement::refused, .Refusal = aRefusal };
		}

		[[nodiscard]] auto placed(const filtered_comment_placement aPlacement, const git_commit_oid& aCommit, const diff_side aSide,
								  const std::uint32_t aLine) noexcept -> filtered_comment_target
		{
			return { .Placement = aPlacement, .Refusal = filtered_comment_refusal::none, .Commit = aCommit, .Side = aSide, .Line = aLine };
		}
	} // namespace

	auto valid(const line_correspondence& aLines) noexcept -> bool
	{
		if (aLines.Unchanged.size() > maximum_correspondence_runs) {
			return false;
		}
		auto left  = std::uint64_t{ 0U };
		auto right = std::uint64_t{ 0U };
		auto first = true;
		for (const auto& run : aLines.Unchanged) {
			// Apart on both sides: two runs that touch on both would be one.
			if (run.Length == 0U || run.Left < left || run.Right < right || (!first && run.Left == left && run.Right == right)) {
				return false;
			}
			left  = std::uint64_t{ run.Left } + run.Length;
			right = std::uint64_t{ run.Right } + run.Length;
			if (left > aLines.LeftLines || right > aLines.RightLines) {
				return false;
			}
			first = false;
		}
		return true;
	}

	auto correspondence_of(const file_alignment& aAlignment) -> std::optional<line_correspondence>
	{
		if (aAlignment.Content != diff_content_state::text) {
			return std::nullopt;
		}
		auto result       = line_correspondence{ };
		result.LeftLines  = aAlignment.LeftLineCount;
		result.RightLines = aAlignment.RightLineCount;
		for (const auto& segment : aAlignment.Segments) {
			if (segment.Kind != alignment_kind::equal) {
				continue;
			}
			const auto length = segment.Left.extent();
			if (length == 0U || length != segment.Right.extent() || result.Unchanged.size() == maximum_correspondence_runs) {
				return std::nullopt;
			}
			result.Unchanged.push_back({ .Left = segment.Left.Begin, .Right = segment.Right.Begin, .Length = static_cast<std::uint32_t>(length) });
		}
		return valid(result) ? std::optional{ std::move(result) } : std::nullopt;
	}

	auto corresponding_right_line(const line_correspondence& aLines, const std::uint32_t aLeftLine) noexcept
		-> std::optional<std::uint32_t>
	{
		const auto* run = aLeftLine < aLines.LeftLines ? run_holding(aLines, diff_side::left, aLeftLine) : nullptr;
		return run != nullptr ? std::optional{ run->Right + (aLeftLine - run->Left) } : std::nullopt;
	}

	auto corresponding_left_line(const line_correspondence& aLines, const std::uint32_t aRightLine) noexcept
		-> std::optional<std::uint32_t>
	{
		const auto* run = aRightLine < aLines.RightLines ? run_holding(aLines, diff_side::right, aRightLine) : nullptr;
		return run != nullptr ? std::optional{ run->Left + (aRightLine - run->Right) } : std::nullopt;
	}

	auto line_in_change_context(const line_correspondence& aLines, const diff_side aSide, const std::uint32_t aLine) noexcept
		-> bool
	{
		const auto lines = aSide == diff_side::left ? aLines.LeftLines : aLines.RightLines;
		if (aLine >= lines) {
			return false;
		}
		// Each gap between unchanged runs, and before the first and after the
		// last, is a change; its hunk is its span on this side widened by the
		// context on both ends. An empty span (the other side's lines only)
		// still has a place on this side, between two lines.
		auto       previousLeft  = std::uint64_t{ 0U };
		auto       previousRight = std::uint64_t{ 0U };
		const auto inside        = [aLine](const std::uint64_t aBegin, const std::uint64_t aEnd) {
			const auto from = aBegin > github_comment_context_lines ? aBegin - github_comment_context_lines : 0U;
			return aLine >= from && aLine < aEnd + github_comment_context_lines;
		};
		for (std::size_t index = 0U; index <= aLines.Unchanged.size(); ++index) {
			const auto nextLeft  = index < aLines.Unchanged.size() ? std::uint64_t{ aLines.Unchanged[index].Left } : std::uint64_t{ aLines.LeftLines };
			const auto nextRight = index < aLines.Unchanged.size() ? std::uint64_t{ aLines.Unchanged[index].Right } : std::uint64_t{ aLines.RightLines };
			if ((nextLeft > previousLeft || nextRight > previousRight)
				&& (aSide == diff_side::left ? inside(previousLeft, nextLeft) : inside(previousRight, nextRight))) {
				return true;
			}
			if (index < aLines.Unchanged.size()) {
				previousLeft  = nextLeft + aLines.Unchanged[index].Length;
				previousRight = nextRight + aLines.Unchanged[index].Length;
			}
		}
		return false;
	}

	auto filtered_comment_needs_lines(const filtered_comment_source& aSource) noexcept -> bool
	{
		return aSource.Diff.Right != aSource.Head || aSource.Diff.Left != aSource.MergeBase;
	}

	auto place_filtered_comment(const filtered_comment_source& aSource, const diff_side aSide, const std::uint32_t aLine,
								const std::optional<std::uint32_t> aStartLine) -> filtered_comment_target
	{
		const auto& diff = aSource.Diff;
		if (aSource.Chain.empty() || aSource.Chain.back() != aSource.Head || !aSource.Head.valid() || !aSource.MergeBase.valid() || !valid(diff)
			|| !contains(aSource.Chain, diff.Right)
			|| !(contains(aSource.Chain, diff.Left) || diff.Left == aSource.Fork || diff.Left == aSource.MergeBase)
			|| (aStartLine && *aStartLine >= aLine)) {
			return refused(filtered_comment_refusal::outside_pull_request);
		}
		if (aSide == diff_side::right) {
			// The head's own document: every line is where it is at the head.
			if (diff.Right == aSource.Head) {
				return placed(filtered_comment_placement::head, aSource.Head, diff_side::right, aLine);
			}
			if (aStartLine) {
				return refused(filtered_comment_refusal::range);
			}
			if (!aSource.Lines) {
				return refused(filtered_comment_refusal::lines_pending);
			}
			if (!aSource.Lines->RightToHead) {
				return refused(filtered_comment_refusal::lines_unavailable);
			}
			if (const auto head = corresponding_right_line(*aSource.Lines->RightToHead, aLine)) {
				if (!aSource.Lines->BaseToHead) {
					return refused(filtered_comment_refusal::lines_unavailable);
				}
				return line_in_change_context(*aSource.Lines->BaseToHead, diff_side::right, *head)
					? placed(filtered_comment_placement::head, aSource.Head, diff_side::right, *head)
					: refused(filtered_comment_refusal::outside_github_diff);
			}
			if (!aSource.Lines->BaseToRight) {
				return refused(filtered_comment_refusal::lines_unavailable);
			}
			return line_in_change_context(*aSource.Lines->BaseToRight, diff_side::right, aLine)
				? placed(filtered_comment_placement::earlier_commit, diff.Right, diff_side::right, aLine)
				: refused(filtered_comment_refusal::not_on_commit);
		}
		if (aSide != diff_side::left) {
			return refused(filtered_comment_refusal::outside_pull_request);
		}
		// GitHub counts a `LEFT` line in the merge base's file.
		if (diff.Left == aSource.MergeBase) {
			return placed(filtered_comment_placement::head, aSource.Head, diff_side::left, aLine);
		}
		if (aStartLine) {
			return refused(filtered_comment_refusal::range);
		}
		if (!aSource.Lines) {
			return refused(filtered_comment_refusal::lines_pending);
		}
		if (!aSource.Lines->LeftToBase) {
			return refused(filtered_comment_refusal::lines_unavailable);
		}
		const auto base = corresponding_right_line(*aSource.Lines->LeftToBase, aLine);
		if (!base) {
			return refused(filtered_comment_refusal::base_line_changed);
		}
		if (!aSource.Lines->BaseToHead) {
			return refused(filtered_comment_refusal::lines_unavailable);
		}
		return line_in_change_context(*aSource.Lines->BaseToHead, diff_side::left, *base)
			? placed(filtered_comment_placement::head, aSource.Head, diff_side::left, *base)
			: refused(filtered_comment_refusal::outside_github_diff);
	}

	auto filtered_thread_line(const filtered_thread_anchor& aAnchor, const filtered_diff_endpoints& aDiff, const git_commit_oid& aMergeBase) noexcept
		-> std::optional<std::uint32_t>
	{
		if (!valid(aDiff) || (aAnchor.Side == diff_side::left && aDiff.Left != aMergeBase)) {
			return std::nullopt;
		}
		if (aAnchor.Commit == std::optional{ aDiff.Right } && aAnchor.Line && *aAnchor.Line > 0U) {
			return *aAnchor.Line - 1U;
		}
		if (aAnchor.OriginalCommit == std::optional{ aDiff.Right } && aAnchor.OriginalLine && *aAnchor.OriginalLine > 0U) {
			return *aAnchor.OriginalLine - 1U;
		}
		return std::nullopt;
	}
} // namespace bpr::core
