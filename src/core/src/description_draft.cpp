#include "bpr/core/description_draft.hpp"

#include "bpr/core/description_edit.hpp"

namespace bpr::core {
	namespace {
		[[nodiscard]] auto trimmed(const std::string_view aText) -> std::string_view
		{
			constexpr auto blank = std::string_view{ " \t\r\n" };
			const auto     first = aText.find_first_not_of(blank);
			if (first == std::string_view::npos) {
				return { };
			}
			return aText.substr(first, aText.find_last_not_of(blank) - first + 1U);
		}

		/// The longest prefix of at most `aLimit` bytes that ends on a whole
		/// UTF-8 sequence.
		[[nodiscard]] auto utf8_prefix(const std::string_view aText, std::size_t aLimit) -> std::string_view
		{
			if (aText.size() <= aLimit) {
				return aText;
			}
			while (aLimit > 0U && (static_cast<unsigned char>(aText[aLimit]) & 0xC0U) == 0x80U) {
				--aLimit;
			}
			return aText.substr(0U, aLimit);
		}
	} // namespace

	auto compose_description_draft(const description_draft_material& aMaterial) -> std::string
	{
		auto       text = std::string{ description_draft_marker } + "\n\nTitle: " + std::string{ trimmed(aMaterial.Title) } + "\n\n";
		const auto body = trimmed(aMaterial.Body);
		if (body.empty()) {
			text += "The pull request has no description yet.\n";
		} else {
			const auto kept = utf8_prefix(body, maximum_description_draft_body_bytes);
			text += "Its description now (rewrite it, keeping the author's own structure and words where they still hold):\n\n```markdown\n" + std::string{ kept } + "\n```\n";
			if (kept.size() < body.size()) {
				text += "(" + std::to_string(body.size() - kept.size()) + " more bytes of the description were not sent.)\n";
			}
		}
		text += "\nThe change, file by file (GitHub's unified diff at the head):\n";
		auto spent = std::size_t{ 0U };
		auto left  = std::vector<std::string_view>{ };
		for (const auto& file : aMaterial.Files) {
			const auto patch = file.Patch ? std::string_view{ *file.Patch } : std::string_view{ };
			if (!left.empty() || spent + patch.size() > maximum_description_draft_diff_bytes) {
				left.emplace_back(file.Path);
				continue;
			}
			spent += patch.size();
			text += "\n" + file.Path + " (" + file.Change + ")\n";
			if (patch.empty()) {
				text += "(no patch: a binary or very large file)\n";
				continue;
			}
			text += "```diff\n" + std::string{ patch };
			if (patch.back() != '\n') {
				text += "\n";
			}
			text += "```\n";
		}
		if (!left.empty()) {
			text += "\nLeft out, past what one request carries:";
			for (const auto path : left) {
				text += " " + std::string{ path };
			}
			text += "\n";
		}
		return text;
	}

	auto read_description_draft(const std::string_view aAnswer) -> std::optional<std::string>
	{
		auto text = trimmed(aAnswer);
		// One fenced block around the whole answer is a wrapper, not content.
		if (text.starts_with("```") && text.ends_with("```") && text.size() > 6U) {
			const auto open  = text.find('\n');
			const auto inner = open == std::string_view::npos ? std::string_view{ } : text.substr(open + 1U, text.size() - 3U - open - 1U);
			if (inner.find("```") == std::string_view::npos) {
				text = trimmed(inner);
			}
		}
		if (text.empty() || description_characters(text) > maximum_description_characters) {
			return std::nullopt;
		}
		return std::string{ text };
	}
} // namespace bpr::core
