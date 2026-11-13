#include "bpr/core/suggest_change.hpp"

#include "bpr/core/pull_request_suggestion.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_preparation.hpp"

#include <algorithm>
#include <limits>
#include <ranges>
#include <utility>
#include <variant>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto ending_text(const text_line_ending aEnding) noexcept -> std::string_view
		{
			switch (aEnding) {
			case text_line_ending::lf:   return "\n";
			case text_line_ending::crlf: return "\r\n";
			case text_line_ending::none: return "";
			}
			return "";
		}

		[[nodiscard]] auto whole_text(std::span<const text_line> aLines) -> std::string
		{
			auto text = std::string{ };
			for (const auto& line : aLines) {
				text += line.Content;
				text += ending_text(line.Ending);
			}
			return text;
		}

		/// The longest prefix within `aLimit` that ends at a character boundary.
		[[nodiscard]] auto cut_at_character(std::string aText, const std::size_t aLimit) -> std::string
		{
			if (aText.size() <= aLimit) {
				return aText;
			}
			auto size = aLimit;
			while (size > 0U && (static_cast<unsigned char>(aText[size]) & 0xC0U) == 0x80U) {
				--size;
			}
			aText.resize(size);
			return aText;
		}

		/// Strips one fenced block's fences, or nothing when there is none.
		[[nodiscard]] auto unfenced(std::string_view aText) -> std::string_view
		{
			const auto blank = [](const char aChar) { return aChar == ' ' || aChar == '\t' || aChar == '\r' || aChar == '\n'; };
			while (!aText.empty() && blank(aText.front())) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && blank(aText.back())) {
				aText.remove_suffix(1U);
			}
			if (!aText.starts_with("```")) {
				return aText;
			}
			const auto open = aText.find('\n');
			if (open == std::string_view::npos || !aText.ends_with("```")) {
				return { };
			}
			// The opening line names a language at most: a word, no blanks.
			auto language = aText.substr(3U, open - 3U);
			if (language.ends_with('\r')) {
				language.remove_suffix(1U);
			}
			if (std::ranges::any_of(language, [](const char aChar) { return aChar == ' ' || aChar == '\t' || aChar == '\r' || aChar == '`'; })) {
				return { };
			}
			auto inner = aText.substr(open + 1U, aText.size() - open - 1U - 3U);
			// The closing fence stands on a line of its own.
			if (!inner.empty() && inner.back() != '\n') {
				return { };
			}
			return inner;
		}

		/// The replacement's lines, without their endings.
		[[nodiscard]] auto replacement_lines(std::string_view aReplacement, const bool aCrlf) -> std::vector<std::string>
		{
			if (aReplacement.ends_with(aCrlf ? std::string_view{ "\r\n" } : std::string_view{ "\n" })) {
				aReplacement.remove_suffix(aCrlf ? 2U : 1U);
			} else if (aReplacement.ends_with('\n')) {
				aReplacement.remove_suffix(1U);
			}
			auto lines = std::vector<std::string>{ };
			if (aReplacement.empty()) {
				return lines;
			}
			for (auto&& piece : std::views::split(aReplacement, '\n')) {
				auto line = std::string{ piece.begin(), piece.end() };
				if (aCrlf && line.ends_with('\r')) {
					line.pop_back();
				}
				lines.push_back(std::move(line));
			}
			return lines;
		}

		[[nodiscard]] auto line_number(const cycle_json_value& aValue) -> std::optional<std::uint32_t>
		{
			const auto* const number = std::get_if<std::int64_t>(&aValue.Value);
			if (number == nullptr || *number < 1 || *number > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(*number);
		}
	} // namespace

	auto stable_name(const suggest_change_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case suggest_change_refusal::none:            return "none";
		case suggest_change_refusal::unreadable:      return "unreadable";
		case suggest_change_refusal::head_moved:      return "head_moved";
		case suggest_change_refusal::range_differs:   return "range_differs";
		case suggest_change_refusal::current_differs: return "current_differs";
		case suggest_change_refusal::identical:       return "identical";
		case suggest_change_refusal::invalid_text:    return "invalid_text";
		case suggest_change_refusal::too_large:       return "too_large";
		case suggest_change_refusal::not_suggestible: return "not_suggestible";
		}
		return { };
	}

	auto suggest_lines_text(std::span<const text_line> aLines) -> std::string
	{
		auto text = whole_text(aLines);
		if (!aLines.empty()) {
			text.resize(text.size() - ending_text(aLines.back().Ending).size());
		}
		return text;
	}

	auto valid(const supplement_suggest_change& aMaterial) -> bool
	{
		return !aMaterial.Path.empty() && aMaterial.Path.size() <= maximum_suggest_path_bytes && !aMaterial.HeadCommit.empty() && aMaterial.HeadCommit.size() <= 64U && aMaterial.FirstLine != 0U
			&& aMaterial.FirstLine <= aMaterial.LastLine && aMaterial.Lines.size() <= maximum_suggest_lines_bytes && aMaterial.Comment.size() <= maximum_suggest_comment_bytes
			&& aMaterial.Before.size() <= maximum_suggest_context_bytes && aMaterial.After.size() <= maximum_suggest_context_bytes && valid_writing_utf8(aMaterial.Path)
			&& valid_writing_utf8(aMaterial.Comment) && (aMaterial.Lines.empty() || valid_writing_utf8(aMaterial.Lines)) && (aMaterial.Before.empty() || valid_writing_utf8(aMaterial.Before))
			&& (aMaterial.After.empty() || valid_writing_utf8(aMaterial.After));
	}

	auto bound_suggest_change_material(const suggest_change_material_input& aInput) -> std::optional<supplement_suggest_change>
	{
		if (aInput.Path.empty() || aInput.HeadCommit.empty() || aInput.First > aInput.Last || aInput.Last >= aInput.Head.size()) {
			return std::nullopt;
		}
		auto material = supplement_suggest_change{
			.Path       = aInput.Path,
			.HeadCommit = aInput.HeadCommit,
			.FirstLine  = aInput.First + 1U,
			.LastLine   = aInput.Last + 1U,
			.Comment    = cut_at_character(aInput.Comment, maximum_suggest_comment_bytes),
			.Lines      = suggest_lines_text(aInput.Head.subspan(aInput.First, aInput.Last - aInput.First + 1U)),
		};
		// Surroundings: whole lines, nearest first, within the bound.
		auto before = std::vector<text_line>{ };
		auto size   = std::size_t{ 0U };
		for (auto line = aInput.First; line > 0U && before.size() < suggest_context_lines; --line) {
			const auto& held = aInput.Head[line - 1U];
			size += held.Content.size() + ending_text(held.Ending).size();
			if (size > maximum_suggest_context_bytes) {
				break;
			}
			before.insert(before.begin(), held);
		}
		material.Before = whole_text(before);
		auto after      = std::vector<text_line>{ };
		size            = 0U;
		for (auto line = static_cast<std::size_t>(aInput.Last) + 1U; line < aInput.Head.size() && after.size() < suggest_context_lines; ++line) {
			const auto& held = aInput.Head[line];
			size += held.Content.size() + ending_text(held.Ending).size();
			if (size > maximum_suggest_context_bytes) {
				break;
			}
			after.push_back(held);
		}
		// The lines' own last ending is not in `Lines`; it leads the text after.
		if (!after.empty()) {
			material.After = std::string{ ending_text(aInput.Head[aInput.Last].Ending) } + whole_text(after);
			if (material.After.size() > maximum_suggest_context_bytes) {
				material.After.clear();
			}
		}
		if (!valid(material)) {
			return std::nullopt;
		}
		return material;
	}

	auto read_suggested_replacement(const std::string_view aAnswer) -> std::optional<suggested_replacement>
	{
		if (aAnswer.size() > maximum_suggest_answer_bytes) {
			return std::nullopt;
		}
		const auto text = unfenced(aAnswer);
		if (text.empty()) {
			return std::nullopt;
		}
		const auto parsed = parse_cycle_json(text, maximum_suggest_answer_bytes);
		if (!parsed) {
			return std::nullopt;
		}
		const auto* const object = std::get_if<cycle_json_value::object>(&parsed->Value);
		if (object == nullptr || object->size() != 4U) {
			return std::nullopt;
		}
		auto reply = suggested_replacement{ };
		auto seen  = 0U;
		for (const auto& member : *object) {
			if (member.Key == "first_line" || member.Key == "last_line") {
				const auto number = line_number(member.Value);
				if (!number) {
					return std::nullopt;
				}
				(member.Key == "first_line" ? reply.FirstLine : reply.LastLine) = *number;
				++seen;
			} else if (member.Key == "current" || member.Key == "replacement") {
				const auto* const string = std::get_if<std::string>(&member.Value.Value);
				if (string == nullptr) {
					return std::nullopt;
				}
				(member.Key == "current" ? reply.Current : reply.Replacement) = *string;
				++seen;
			} else {
				return std::nullopt;
			}
		}
		if (seen != 4U) {
			return std::nullopt;
		}
		return reply;
	}

	auto decide_suggested_change(const suggest_change_input& aInput) -> suggest_change_decision
	{
		auto       decision = suggest_change_decision{ };
		const auto refuse   = [&decision](const suggest_change_refusal aRefusal) {
			decision.Refusal = aRefusal;
			decision.Span.clear();
			decision.Block.clear();
			return decision;
		};
		const auto reply = read_suggested_replacement(aInput.Answer);
		if (!reply) {
			return refuse(suggest_change_refusal::unreadable);
		}
		// The head now must hold the lines the press read, where it read them.
		const auto count = static_cast<std::size_t>(aInput.Last) - aInput.First + 1U;
		if (!aInput.SameHead || aInput.First > aInput.Last || aInput.Last >= aInput.Head.size() || aInput.Asked.size() != count
			|| !std::ranges::equal(aInput.Asked, aInput.Head.subspan(aInput.First, count))) {
			return refuse(suggest_change_refusal::head_moved);
		}
		if (reply->FirstLine != aInput.First + 1U || reply->LastLine != aInput.Last + 1U) {
			return refuse(suggest_change_refusal::range_differs);
		}
		if (reply->Current != suggest_lines_text(aInput.Asked)) {
			return refuse(suggest_change_refusal::current_differs);
		}
		if (!reply->Replacement.empty() && !valid_writing_utf8(reply->Replacement)) {
			return refuse(suggest_change_refusal::invalid_text);
		}
		if (reply->Replacement.size() > maximum_suggested_replacement_bytes) {
			return refuse(suggest_change_refusal::too_large);
		}
		const auto crlf  = aInput.Asked.back().Ending == text_line_ending::crlf;
		auto       block = replacement_lines(reply->Replacement, crlf);
		if (std::ranges::equal(block, aInput.Asked, {}, {}, &text_line::Content)) {
			return refuse(suggest_change_refusal::identical);
		}
		// The span the in-pane edit opens with, as a kept draft opens again.
		const auto rebuilt = rebuild_suggestion_span(compose_suggestion_body({ }, block), aInput.Asked, true);
		if (rebuilt.Refusal != suggestion_rebuild_refusal::none) {
			return refuse(suggest_change_refusal::not_suggestible);
		}
		const auto plan = plan_pull_request_suggestion(pull_request_suggestion_input{
			.First    = aInput.First,
			.Last     = aInput.Last,
			.Original = aInput.Asked,
			.Edited   = rebuilt.Lines,
			.Prose    = aInput.Prose,
			.Filtered = aInput.Filtered,
		});
		if (!plan.Changed) {
			return refuse(suggest_change_refusal::identical);
		}
		const auto split = split_suggestion_body(plan.Body);
		if (!plan.placeable() || plan.Block != block || !split || split->Block != block) {
			return refuse(suggest_change_refusal::not_suggestible);
		}
		decision.Refusal = suggest_change_refusal::none;
		decision.Span    = rebuilt.Lines;
		decision.Block   = std::move(block);
		return decision;
	}
} // namespace bpr::core
