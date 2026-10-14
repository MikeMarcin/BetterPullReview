#include "bpr/core/review_comment_range.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace bpr::core {
	auto name(const comment_range_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case comment_range_refusal::none:
			return "none";
		case comment_range_refusal::mixed_comparison:
			return "mixed_comparison";
		case comment_range_refusal::mixed_side:
			return "mixed_side";
		case comment_range_refusal::base_side:
			return "base_side";
		case comment_range_refusal::outside_document:
			return "outside_document";
		case comment_range_refusal::outside_hunk:
			return "outside_hunk";
		case comment_range_refusal::crosses_hunk:
			return "crosses_hunk";
		}
		return "none";
	}

	auto github_review_hunks(const file_alignment& aAlignment, const diff_side aSide) -> std::vector<line_span>
	{
		const auto count  = aSide == diff_side::left ? aAlignment.LeftLineCount : aAlignment.RightLineCount;
		auto       result = std::vector<line_span>{ };
		if (aAlignment.Content != diff_content_state::text || count == 0U) {
			return result;
		}
		result.reserve(aAlignment.Groups.size());
		for (const auto& group : aAlignment.Groups) {
			const auto& span  = aSide == diff_side::left ? group.Left : group.Right;
			const auto  begin = span.Begin > github_hunk_context_lines ? span.Begin - github_hunk_context_lines : 0U;
			const auto  end   = static_cast<std::uint32_t>(std::min<std::uint64_t>(count, std::uint64_t{ std::max(span.Begin, span.End) } + github_hunk_context_lines));
			if (begin >= end) {
				continue;
			}
			result.push_back({ .Begin = begin, .End = end });
		}
		std::ranges::sort(result, { }, &line_span::Begin);
		auto merged = std::vector<line_span>{ };
		merged.reserve(result.size());
		for (const auto& hunk : result) {
			if (!merged.empty() && hunk.Begin <= merged.back().End + github_inter_hunk_context_lines) {
				merged.back().End = std::max(merged.back().End, hunk.End);
				continue;
			}
			merged.push_back(hunk);
		}
		return merged;
	}

	auto decide_comment_range(
		const file_alignment& aAlignment,
		source_line_ref       aFirst,
		source_line_ref       aLast,
		const bool            aBaseNumberedAsGitHub) -> comment_range_decision
	{
		auto result = comment_range_decision{ };
		if (aFirst.Line > aLast.Line) {
			std::swap(aFirst, aLast);
		}
		result.Anchor = aLast;
		if (aFirst.Comparison != aLast.Comparison || aLast.Comparison != aAlignment.Digest) {
			result.Refusal = comment_range_refusal::mixed_comparison;
			return result;
		}
		if (aFirst.Side != aLast.Side) {
			result.Refusal = comment_range_refusal::mixed_side;
			return result;
		}
		const auto side = aLast.Side;
		if (side != diff_side::right && (side != diff_side::left || !aBaseNumberedAsGitHub)) {
			result.Refusal = comment_range_refusal::base_side;
			return result;
		}
		const auto count = side == diff_side::left ? aAlignment.LeftLineCount : aAlignment.RightLineCount;
		if (aAlignment.Content != diff_content_state::text || aLast.Line >= count) {
			result.Refusal = comment_range_refusal::outside_document;
			return result;
		}
		// A single right line may lie outside every hunk; Publish decides it.
		if (aFirst.Line == aLast.Line && side == diff_side::right) {
			return result;
		}
		const auto hunks  = github_review_hunks(aAlignment, side);
		const auto within = [&hunks](const std::uint32_t aLine) {
			return std::ranges::find_if(hunks, [aLine](const line_span& aHunk) { return aLine >= aHunk.Begin && aLine < aHunk.End; });
		};
		const auto first = within(aFirst.Line);
		const auto last  = within(aLast.Line);
		if (first == hunks.end() && last == hunks.end()) {
			result.Refusal = comment_range_refusal::outside_hunk;
			return result;
		}
		if (first != last) {
			result.Refusal = comment_range_refusal::crosses_hunk;
			return result;
		}
		if (aFirst.Line != aLast.Line) {
			result.StartLine = aFirst.Line;
		}
		return result;
	}
} // namespace bpr::core
