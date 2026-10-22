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

		/// [[BPR-1706]]. GitHub resolves the line nowhere as a line comment:
		/// a file comment whose permalink names it at the diff's own commit.
		[[nodiscard]] auto file_comment(const filtered_comment_refusal aReason, const git_commit_oid& aCommit, const diff_side aSide,
										const std::uint32_t aLine) noexcept -> filtered_comment_target
		{
			return { .Placement = filtered_comment_placement::file_comment, .Refusal = aReason, .Commit = aCommit, .Side = aSide, .Line = aLine };
		}

		/// \brief [[BPR-1735]]. The hunk holding a line on one side, as the
		/// half-open span of its lines with their context; none when no hunk
		/// holds it.
		///
		/// \details Each change (a gap between unchanged runs, before the
		/// first or after the last) is widened by the context on both ends and
		/// clipped to the document. Spans that touch or overlap are one hunk:
		/// two changes at most twice the context apart share their context, as
		/// Git's hunks do.
		[[nodiscard]] auto hunk_holding(const line_correspondence& aLines, const diff_side aSide, const std::uint32_t aLine) noexcept
			-> std::optional<std::pair<std::uint64_t, std::uint64_t>>
		{
			using span               = std::pair<std::uint64_t, std::uint64_t>;
			const auto lines         = std::uint64_t{ aSide == diff_side::left ? aLines.LeftLines : aLines.RightLines };
			auto       previousLeft  = std::uint64_t{ 0U };
			auto       previousRight = std::uint64_t{ 0U };
			auto       current       = std::optional<span>{ };
			const auto holds         = [aLine](const std::optional<span>& aSpan) { return aSpan && aLine >= aSpan->first && aLine < aSpan->second; };
			for (std::size_t index = 0U; index <= aLines.Unchanged.size(); ++index) {
				const auto nextLeft  = index < aLines.Unchanged.size() ? std::uint64_t{ aLines.Unchanged[index].Left } : std::uint64_t{ aLines.LeftLines };
				const auto nextRight = index < aLines.Unchanged.size() ? std::uint64_t{ aLines.Unchanged[index].Right } : std::uint64_t{ aLines.RightLines };
				if (nextLeft > previousLeft || nextRight > previousRight) {
					const auto begin = aSide == diff_side::left ? previousLeft : previousRight;
					const auto end   = aSide == diff_side::left ? nextLeft : nextRight;
					const auto from  = begin > github_comment_context_lines ? begin - github_comment_context_lines : 0U;
					const auto to    = std::min(end + github_comment_context_lines, lines);
					if (current && from <= current->second) {
						current->second = std::max(current->second, to);
					} else {
						if (holds(current)) {
							return current;
						}
						current = span{ from, to };
					}
				}
				if (index < aLines.Unchanged.size()) {
					previousLeft  = nextLeft + aLines.Unchanged[index].Length;
					previousRight = nextRight + aLines.Unchanged[index].Length;
				}
			}
			return holds(current) ? current : std::nullopt;
		}

		/// \brief [[BPR-1735]]. A line of a side that is GitHub's own: at the
		/// head, unless the merge base to the head shows GitHub resolves it
		/// nowhere. The permalink names it at that side's commit.
		[[nodiscard]] auto own_side(const std::optional<line_correspondence>& aBaseToHead, const git_commit_oid& aHead, const git_commit_oid& aSideCommit,
									const diff_side aSide, const std::uint32_t aLine, const std::optional<std::uint32_t> aStartLine) noexcept
			-> filtered_comment_target
		{
			switch (place_on_github_diff(aBaseToHead, aSide, aLine, aStartLine)) {
			case github_diff_placement::outside_diff:
				return file_comment(filtered_comment_refusal::outside_github_diff, aSideCommit, aSide, aLine);
			case github_diff_placement::range_outside_diff:
				return refused(filtered_comment_refusal::range_outside_github_diff);
			case github_diff_placement::unknown:
			case github_diff_placement::in_diff:
				break;
			}
			return placed(filtered_comment_placement::head, aHead, aSide, aLine);
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

	auto place_on_github_diff(const std::optional<line_correspondence>& aBaseToHead, const diff_side aSide, const std::uint32_t aLine,
							  const std::optional<std::uint32_t> aStartLine) noexcept -> github_diff_placement
	{
		if (!aBaseToHead || (aSide != diff_side::left && aSide != diff_side::right) || (aStartLine && *aStartLine >= aLine) || !valid(*aBaseToHead)
			|| aLine >= (aSide == diff_side::left ? aBaseToHead->LeftLines : aBaseToHead->RightLines)) {
			return github_diff_placement::unknown;
		}
		const auto hunk = hunk_holding(*aBaseToHead, aSide, aLine);
		if (!aStartLine) {
			return hunk ? github_diff_placement::in_diff : github_diff_placement::outside_diff;
		}
		return hunk && *aStartLine >= hunk->first ? github_diff_placement::in_diff : github_diff_placement::range_outside_diff;
	}

	auto filtered_comment_needs_lines(const filtered_comment_source& aSource) noexcept -> bool
	{
		return valid(aSource.Diff);
	}

	auto place_whole_diff_comment(const whole_diff_comment_source& aSource, const diff_side aSide, const std::uint32_t aLine,
								  const std::optional<std::uint32_t> aStartLine) -> std::optional<filtered_comment_target>
	{
		if (!aSource.Head.valid() || !aSource.MergeBase.valid()
			|| place_on_github_diff(aSource.BaseToHead, aSide, aLine, aStartLine) == github_diff_placement::unknown) {
			return std::nullopt;
		}
		return own_side(aSource.BaseToHead, aSource.Head, aSide == diff_side::left ? aSource.MergeBase : aSource.Head, aSide, aLine, aStartLine);
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
			// The head's own document: every line is where it is at the head,
			// and GitHub resolves it there only inside a hunk ([[BPR-1735]]).
			if (diff.Right == aSource.Head) {
				return own_side(aSource.Lines ? aSource.Lines->BaseToHead : std::nullopt, aSource.Head, aSource.Head, diff_side::right, aLine, aStartLine);
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
					: file_comment(filtered_comment_refusal::outside_github_diff, diff.Right, diff_side::right, aLine);
			}
			if (!aSource.Lines->BaseToRight) {
				return refused(filtered_comment_refusal::lines_unavailable);
			}
			return line_in_change_context(*aSource.Lines->BaseToRight, diff_side::right, aLine)
				? placed(filtered_comment_placement::earlier_commit, diff.Right, diff_side::right, aLine)
				: file_comment(filtered_comment_refusal::not_on_commit, diff.Right, diff_side::right, aLine);
		}
		if (aSide != diff_side::left) {
			return refused(filtered_comment_refusal::outside_pull_request);
		}
		// GitHub counts a `LEFT` line in the merge base's file.
		if (diff.Left == aSource.MergeBase) {
			return own_side(aSource.Lines ? aSource.Lines->BaseToHead : std::nullopt, aSource.Head, aSource.MergeBase, diff_side::left, aLine, aStartLine);
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
			return file_comment(filtered_comment_refusal::base_line_changed, diff.Left, diff_side::left, aLine);
		}
		if (!aSource.Lines->BaseToHead) {
			return refused(filtered_comment_refusal::lines_unavailable);
		}
		return line_in_change_context(*aSource.Lines->BaseToHead, diff_side::left, *base)
			? placed(filtered_comment_placement::head, aSource.Head, diff_side::left, *base)
			: file_comment(filtered_comment_refusal::outside_github_diff, diff.Left, diff_side::left, aLine);
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
