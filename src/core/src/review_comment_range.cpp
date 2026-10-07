#include "bpr/core/review_comment_range.hpp"

#include "bpr/core/review_preparation.hpp"

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
			if (!merged.empty() && hunk.Begin <= merged.back().End) {
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
		source_line_ref       aLast) -> comment_range_decision
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
		if (aLast.Side != diff_side::right) {
			result.Refusal = comment_range_refusal::base_side;
			return result;
		}
		if (aAlignment.Content != diff_content_state::text || aLast.Line >= aAlignment.RightLineCount) {
			result.Refusal = comment_range_refusal::outside_document;
			return result;
		}
		if (aFirst.Line == aLast.Line) {
			return result;
		}
		const auto hunks  = github_review_hunks(aAlignment, diff_side::right);
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
		result.StartLine = aFirst.Line;
		return result;
	}

	auto suggestion_prefill(
		const decoded_text_document& aDocument,
		const std::uint32_t          aFirst,
		const std::uint32_t          aLast) -> std::optional<std::string>
	{
		if (aFirst > aLast || aLast >= aDocument.Lines.size()) {
			return std::nullopt;
		}
		auto lines   = std::string{ };
		auto longest = std::size_t{ 0U };
		for (auto index = aFirst; index <= aLast; ++index) {
			const auto& content = aDocument.Lines[index].Content;
			if (content.Begin > content.End || content.End > aDocument.Bytes.size()) {
				return std::nullopt;
			}
			if (index != aFirst) {
				lines.push_back('\n');
			}
			auto run = std::size_t{ 0U };
			for (auto at = content.Begin; at < content.End; ++at) {
				const auto byte = static_cast<char>(aDocument.Bytes[static_cast<std::size_t>(at)]);
				lines.push_back(byte);
				run     = byte == '`' ? run + 1U : 0U;
				longest = std::max(longest, run);
			}
		}
		if (!valid_writing_utf8(lines)) {
			return std::nullopt;
		}
		const auto fence = std::string(std::max<std::size_t>(3U, longest + 1U), '`');
		return fence + "suggestion\n" + lines + "\n" + fence;
	}
} // namespace bpr::core
