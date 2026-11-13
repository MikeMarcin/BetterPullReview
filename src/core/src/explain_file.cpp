#include "bpr/core/explain_file.hpp"

#include "bpr/core/review_preparation.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		/// Cuts `aText` to at most `aMaximum` bytes at the last line end within
		/// it, or at a character boundary when no line end is. True when cut.
		auto cut_at_line(std::string& aText, const std::size_t aMaximum) -> bool
		{
			if (aText.size() <= aMaximum) {
				return false;
			}
			const auto end = aText.rfind('\n', aMaximum == 0U ? 0U : aMaximum - 1U);
			if (end != std::string::npos && aMaximum != 0U) {
				aText.resize(end + 1U);
				return true;
			}
			aText.resize(aMaximum);
			while (!aText.empty() && (static_cast<unsigned char>(aText.back()) & 0xC0U) == 0x80U) {
				aText.pop_back();
			}
			if (!aText.empty() && static_cast<unsigned char>(aText.back()) >= 0xC0U) {
				aText.pop_back();
			}
			return true;
		}

		[[nodiscard]] auto full_hex(const std::string_view aCommit) -> bool
		{
			return (aCommit.size() == 40U || aCommit.size() == 64U)
				&& std::ranges::all_of(aCommit, [](const char aChar) { return (aChar >= '0' && aChar <= '9') || (aChar >= 'a' && aChar <= 'f'); });
		}
	} // namespace

	auto stable_name(const explain_content_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case explain_content_state::whole:     return "whole";
		case explain_content_state::cut:       return "cut";
		case explain_content_state::absent:    return "absent";
		case explain_content_state::binary:    return "binary";
		case explain_content_state::too_large: return "too_large";
		}
		return { };
	}

	auto bound_explain_material(explain_material_input aInput) -> std::optional<supplement_explain>
	{
		if (aInput.Path.empty() || aInput.Path.size() > maximum_explain_path_bytes || !full_hex(aInput.BaseCommit) || !full_hex(aInput.HeadCommit) || !valid_writing_utf8(aInput.Path)) {
			return std::nullopt;
		}
		auto result = supplement_explain{ .Path = std::move(aInput.Path), .BaseCommit = std::move(aInput.BaseCommit), .HeadCommit = std::move(aInput.HeadCommit), .Change = std::move(aInput.Change) };
		if (!valid_writing_utf8(result.Change)) {
			result.Change.clear();
		}
		result.ChangeCut = cut_at_line(result.Change, maximum_explain_change_bytes);
		if (aInput.TooLarge) {
			result.ContentState = explain_content_state::too_large;
		} else if (!aInput.Head) {
			result.ContentState = explain_content_state::absent;
		} else if (aInput.Head->find('\0') != std::string::npos || !valid_writing_utf8(*aInput.Head)) {
			result.ContentState = explain_content_state::binary;
		} else {
			result.Content      = std::move(*aInput.Head);
			result.ContentState = cut_at_line(result.Content, maximum_explain_content_bytes) ? explain_content_state::cut : explain_content_state::whole;
		}
		return result;
	}

	auto valid(const supplement_explain& aExplain) -> bool
	{
		const auto textual = aExplain.ContentState == explain_content_state::whole || aExplain.ContentState == explain_content_state::cut;
		return !aExplain.Path.empty() && aExplain.Path.size() <= maximum_explain_path_bytes && full_hex(aExplain.BaseCommit) && full_hex(aExplain.HeadCommit)
			&& aExplain.Change.size() <= maximum_explain_change_bytes && aExplain.Content.size() <= maximum_explain_content_bytes && (textual || aExplain.Content.empty())
			&& valid_writing_utf8(aExplain.Path) && valid_writing_utf8(aExplain.Change) && valid_writing_utf8(aExplain.Content);
	}

	auto explain_note_text(const std::string_view aAgent, const std::string_view aAnswer, const std::size_t aMaximumBytes) -> std::string
	{
		auto answer = std::string{ aAnswer };
		while (!answer.empty() && (answer.back() == '\n' || answer.back() == ' ' || answer.back() == '\r' || answer.back() == '\t')) {
			answer.pop_back();
		}
		if (answer.empty()) {
			return { };
		}
		auto       text      = "Agent: " + (aAgent.empty() ? std::string{ "the agent" } : std::string{ aAgent }) + " explained this file.\n\n";
		const auto ellipsis  = std::string_view{ "\n…" };
		if (text.size() + ellipsis.size() >= aMaximumBytes) {
			return { };
		}
		if (text.size() + answer.size() > aMaximumBytes) {
			static_cast<void>(cut_at_line(answer, aMaximumBytes - text.size() - ellipsis.size()));
			while (!answer.empty() && answer.back() == '\n') {
				answer.pop_back();
			}
			answer += ellipsis;
		}
		return text + answer;
	}
} // namespace bpr::core
