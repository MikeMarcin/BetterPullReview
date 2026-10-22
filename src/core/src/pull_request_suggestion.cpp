#include "bpr/core/pull_request_suggestion.hpp"

#include "bpr/core/review_publication_policy.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto contents_of(const std::span<const text_line> aLines) -> std::vector<std::string>
		{
			auto result = std::vector<std::string>{ };
			result.reserve(aLines.size());
			for (const auto& line : aLines) {
				result.push_back(line.Content);
			}
			return result;
		}

		/// The longest run of backticks anywhere in the lines.
		[[nodiscard]] auto longest_backtick_run(const std::span<const std::string> aLines) -> std::size_t
		{
			auto longest = std::size_t{ 0U };
			for (const auto& line : aLines) {
				auto run = std::size_t{ 0U };
				for (const auto character : line) {
					run     = character == '`' ? run + 1U : 0U;
					longest = std::max(longest, run);
				}
			}
			return longest;
		}

		/// "line 12" or "lines 12–15", one-based.
		[[nodiscard]] auto lines_words(const std::uint32_t aFirst, const std::uint32_t aLast) -> std::string
		{
			if (aFirst == aLast) {
				return "line " + std::to_string(aFirst + 1U);
			}
			return "lines " + std::to_string(aFirst + 1U) + "\xE2\x80\x93" + std::to_string(aLast + 1U);
		}

		constexpr auto prose_separator = std::string_view{ "\n\n" };
	} // namespace

	auto stable_name(const suggestion_range_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case suggestion_range_refusal::none:                   return "none";
		case suggestion_range_refusal::comment_range:          return "comment_range";
		case suggestion_range_refusal::base_side:              return "base_side";
		case suggestion_range_refusal::outside_changed_region: return "outside_changed_region";
		case suggestion_range_refusal::filtered_range:         return "filtered_range";
		}
		return "unknown";
	}

	auto stable_name(const suggestion_placement_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case suggestion_placement_reason::none:                 return "none";
		case suggestion_placement_reason::changed_after_commit: return "changed_after_commit";
		case suggestion_placement_reason::unresolvable_line:    return "unresolvable_line";
		case suggestion_placement_reason::not_placed:           return "not_placed";
		}
		return "unknown";
	}

	auto stable_name(const suggestion_rebuild_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case suggestion_rebuild_refusal::none:             return "none";
		case suggestion_rebuild_refusal::not_composed:     return "not_composed";
		case suggestion_rebuild_refusal::second_block:     return "second_block";
		case suggestion_rebuild_refusal::other_comparison: return "other_comparison";
		case suggestion_rebuild_refusal::no_original:      return "no_original";
		}
		return "unknown";
	}

	auto decide_suggestion_range(const file_alignment& aAlignment, const source_line_ref aFirst, const source_line_ref aLast, const bool aRightIsNotHead)
		-> suggestion_range_decision
	{
		auto       result = suggestion_range_decision{ };
		const auto range  = decide_comment_range(aAlignment, aFirst, aLast, false);
		if (range.Refusal == comment_range_refusal::base_side || (range.Refusal == comment_range_refusal::none && range.Anchor.Side != diff_side::right)) {
			result.Refusal = suggestion_range_refusal::base_side;
			return result;
		}
		if (range.Refusal != comment_range_refusal::none) {
			result.Refusal        = suggestion_range_refusal::comment_range;
			result.CommentRefusal = range.Refusal;
			return result;
		}
		if (range.StartLine.has_value() && aRightIsNotHead) {
			result.Refusal = suggestion_range_refusal::filtered_range;
			return result;
		}
		// One line must be in a hunk too. On a filtered diff whose right side
		// is earlier than the head, the filtered placement decides the line.
		if (!range.StartLine.has_value() && !aRightIsNotHead) {
			const auto hunks  = github_review_hunks(aAlignment, diff_side::right);
			const auto inside = std::ranges::any_of(hunks, [&](const line_span& aHunk) { return aHunk.Begin <= range.Anchor.Line && range.Anchor.Line < aHunk.End; });
			if (!inside) {
				result.Refusal = suggestion_range_refusal::outside_changed_region;
				return result;
			}
		}
		result.Range = range;
		return result;
	}

	auto compose_suggestion_body(const std::string_view aProse, const std::span<const std::string> aBlock) -> std::string
	{
		auto body = std::string{ aProse };
		if (!body.empty()) {
			body += prose_separator;
		}
		body += suggestion_fence_body(aBlock);
		return body;
	}

	auto split_suggestion_body(const std::string_view aBody) -> std::optional<split_suggestion>
	{
		// The closing fence is the body's last line: a run of backticks alone.
		const auto lastBreak = aBody.rfind('\n');
		if (lastBreak == std::string_view::npos) {
			return std::nullopt;
		}
		const auto closing = aBody.substr(lastBreak + 1U);
		if (closing.size() < 3U || closing.find_first_not_of('`') != std::string_view::npos) {
			return std::nullopt;
		}
		const auto opening = std::string{ closing } + "suggestion\n";
		// The block's opening line, at the start of the body or after the
		// separator; the latest such line whose composition gives the body back.
		for (auto at = aBody.rfind(opening, lastBreak); at != std::string_view::npos; at = at == 0U ? std::string_view::npos : aBody.rfind(opening, at - 1U)) {
			auto prose = std::string_view{ };
			if (at != 0U) {
				if (at < prose_separator.size() || aBody.substr(at - prose_separator.size(), prose_separator.size()) != prose_separator) {
					continue;
				}
				prose = aBody.substr(0U, at - prose_separator.size());
				if (prose.empty()) {
					continue;
				}
			}
			const auto inner = aBody.substr(at + opening.size(), lastBreak + 1U - (at + opening.size()));
			auto       block = std::vector<std::string>{ };
			for (auto start = std::size_t{ 0U }; start < inner.size();) {
				const auto end = inner.find('\n', start);
				block.emplace_back(inner.substr(start, end - start));
				start = end + 1U;
			}
			if (compose_suggestion_body(prose, block) == aBody) {
				return split_suggestion{ .Prose = std::string{ prose }, .Block = std::move(block) };
			}
		}
		return std::nullopt;
	}

	auto plan_pull_request_suggestion(const pull_request_suggestion_input& aInput) -> pull_request_suggestion_plan
	{
		auto plan = pull_request_suggestion_plan{ };
		if (aInput.First < aInput.Last) {
			plan.StartLine = aInput.First;
		}
		plan.Line          = aInput.Last;
		const auto before  = contents_of(aInput.Original);
		auto       edited  = contents_of(aInput.Edited);
		const auto emptied = edited.size() == 1U && edited.front().empty() && !(before.size() == 1U && before.front().empty());
		if (emptied || edited.empty()) {
			edited.clear();
		}
		plan.Changed  = edited != before;
		plan.Deletion = plan.Changed && edited.empty();
		if (!plan.Changed) {
			plan.Body = std::string{ aInput.Prose };
			return plan;
		}
		plan.Block = std::move(edited);
		plan.Body  = compose_suggestion_body(aInput.Prose, plan.Block);
		// The block's own change: a fence in the prose is not this edit's.
		plan.Change = suggested_change_of(suggestion_fence_body(plan.Block), before);
		if (aInput.Filtered.has_value()) {
			switch (aInput.Filtered->Placement) {
			case filtered_comment_placement::head:           break;
			case filtered_comment_placement::earlier_commit: plan.Reason = suggestion_placement_reason::changed_after_commit; break;
			case filtered_comment_placement::file_comment:   plan.Reason = suggestion_placement_reason::unresolvable_line; break;
			case filtered_comment_placement::refused:        plan.Reason = suggestion_placement_reason::not_placed; break;
			}
		}
		return plan;
	}

	auto rebuild_suggestion_span(const std::string_view aBody, const std::span<const text_line> aOriginal, const bool aSameComparison) -> suggestion_rebuild
	{
		auto result = suggestion_rebuild{ };
		if (!aSameComparison) {
			result.Refusal = suggestion_rebuild_refusal::other_comparison;
			return result;
		}
		if (aOriginal.empty()) {
			result.Refusal = suggestion_rebuild_refusal::no_original;
			return result;
		}
		auto split = split_suggestion_body(aBody);
		if (!split.has_value()) {
			result.Refusal = suggestion_rebuild_refusal::not_composed;
			return result;
		}
		if (classify_review_body(split->Prose) == review_body_compatibility::requires_line_subject) {
			result.Refusal = suggestion_rebuild_refusal::second_block;
			return result;
		}
		if (split->Block.empty()) {
			split->Block.emplace_back();
		}
		const auto last = aOriginal.size() - 1U;
		for (std::size_t index = 0U; index < split->Block.size(); ++index) {
			const auto final  = index + 1U == split->Block.size();
			const auto source = final ? last : std::min(index, last);
			auto       ending = aOriginal[source].Ending;
			// An inner line never ends the file: the last original line's
			// ending, or a line feed, takes the place of none.
			if (!final && ending == text_line_ending::none) {
				const auto found = std::ranges::find_if(aOriginal, [](const text_line& aLine) { return aLine.Ending != text_line_ending::none; });
				ending           = found != aOriginal.end() ? found->Ending : text_line_ending::lf;
			}
			result.Lines.push_back(text_line{ .Content = std::move(split->Block[index]), .Ending = ending });
		}
		result.Prose = std::move(split->Prose);
		return result;
	}

	auto convert_suggestion_to_text(const std::string_view aProse, const std::uint32_t aFirst, const std::uint32_t aLast, const std::span<const std::string> aOriginal,
									const std::span<const std::string> aBlock) -> std::string
	{
		// The fence outruns any run of backticks in the code it holds.
		const auto run   = std::max(longest_backtick_run(aOriginal), longest_backtick_run(aBlock));
		const auto fence = std::string(std::max<std::size_t>(3U, run + 1U), '`');
		auto       body  = std::string{ aProse };
		if (!body.empty()) {
			body += prose_separator;
		}
		body += "Suggested change to " + lines_words(aFirst, aLast) + ":\n\n";
		body += fence + "diff\n";
		for (const auto& line : aOriginal) {
			body += '-';
			body += line;
			body += '\n';
		}
		for (const auto& line : aBlock) {
			body += '+';
			body += line;
			body += '\n';
		}
		body += fence;
		return body;
	}

	auto decide_suggestion_carry(const line_correspondence& aOldToNew, const std::uint32_t aFirst, const std::uint32_t aLast)
		-> std::optional<std::pair<std::uint32_t, std::uint32_t>>
	{
		if (aFirst > aLast || !valid(aOldToNew)) {
			return std::nullopt;
		}
		const auto first = corresponding_right_line(aOldToNew, aFirst);
		if (!first.has_value()) {
			return std::nullopt;
		}
		for (auto line = aFirst + 1U; line <= aLast; ++line) {
			const auto mapped = corresponding_right_line(aOldToNew, line);
			if (!mapped.has_value() || *mapped != *first + (line - aFirst)) {
				return std::nullopt;
			}
		}
		return std::pair{ *first, *first + (aLast - aFirst) };
	}
} // namespace bpr::core
