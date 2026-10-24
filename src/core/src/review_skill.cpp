#include "bpr/core/review_skill.hpp"

#include "bpr/core/review_preparation.hpp"

#include <charconv>
#include <system_error>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto trim(std::string_view aText) noexcept -> std::string_view
		{
			while (!aText.empty() && (aText.front() == ' ' || aText.front() == '\t')) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && (aText.back() == ' ' || aText.back() == '\t' || aText.back() == '\r')) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		/// One line's scalar value: bare, or quoted whole on the line. A
		/// double-quoted value reads `\"` and `\\`; any other escape, or an
		/// unterminated quote, reads as absent.
		[[nodiscard]] auto scalar(std::string_view aValue) -> std::optional<std::string>
		{
			aValue = trim(aValue);
			if (aValue.empty() || (aValue.front() != '"' && aValue.front() != '\'')) {
				return std::string{ aValue };
			}
			const auto quote = aValue.front();
			auto       text  = std::string{ };
			for (auto index = std::size_t{ 1U }; index < aValue.size(); ++index) {
				const auto character = aValue[index];
				if (character == quote) {
					if (quote == '\'' && index + 1U < aValue.size() && aValue[index + 1U] == '\'') {
						text += '\'';
						++index;
						continue;
					}
					return trim(aValue.substr(index + 1U)).empty() ? std::optional{ std::move(text) } : std::nullopt;
				}
				if (quote == '"' && character == '\\') {
					if (index + 1U >= aValue.size() || (aValue[index + 1U] != '"' && aValue[index + 1U] != '\\')) {
						return std::nullopt;
					}
					text += aValue[++index];
					continue;
				}
				text += character;
			}
			return std::nullopt;
		}

		/// `KEY: VALUE` at the given indent, or nothing.
		[[nodiscard]] auto key_value(std::string_view aLine, const std::size_t aIndent, std::string_view& aKey) -> std::optional<std::string_view>
		{
			if (aLine.size() <= aIndent || aLine.substr(0U, aIndent).find_first_not_of(' ') != std::string_view::npos || aLine[aIndent] == ' '
				|| aLine[aIndent] == '\t' || aLine[aIndent] == '#') {
				return std::nullopt;
			}
			const auto rest  = aLine.substr(aIndent);
			const auto colon = rest.find(':');
			if (colon == std::string_view::npos || colon == 0U || (colon + 1U < rest.size() && rest[colon + 1U] != ' ' && rest[colon + 1U] != '\t' && rest[colon + 1U] != '\r')) {
				return std::nullopt;
			}
			aKey = rest.substr(0U, colon);
			return rest.substr(colon + 1U);
		}
	} // namespace

	auto stable_name(const review_skill_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case review_skill_refusal::too_large:
			return "too_large";
		case review_skill_refusal::not_text:
			return "not_text";
		case review_skill_refusal::no_front_matter:
			return "no_front_matter";
		case review_skill_refusal::name_changed:
			return "name_changed";
		case review_skill_refusal::no_description:
			return "no_description";
		}
		return { };
	}

	auto stable_name(const review_skill_source aSource) noexcept -> std::string_view
	{
		switch (aSource) {
		case review_skill_source::built_in:
			return "built_in";
		case review_skill_source::profile_copy:
			return "profile_copy";
		}
		return { };
	}

	auto inspect_review_skill(const std::string_view aText) -> std::expected<review_skill_facts, review_skill_refusal>
	{
		if (aText.size() > maximum_review_skill_bytes) {
			return std::unexpected(review_skill_refusal::too_large);
		}
		if (!valid_writing_utf8(aText)) {
			return std::unexpected(review_skill_refusal::not_text);
		}
		auto rest = aText;
		auto line = [&rest]() -> std::optional<std::string_view> {
			if (rest.empty()) {
				return std::nullopt;
			}
			const auto end  = rest.find('\n');
			const auto text = rest.substr(0U, end);
			rest            = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
			return text.ends_with('\r') ? text.substr(0U, text.size() - 1U) : text;
		};
		if (const auto first = line(); !first || trim(*first) != "---") {
			return std::unexpected(review_skill_refusal::no_front_matter);
		}
		auto name       = std::optional<std::string>{ };
		auto facts      = review_skill_facts{ };
		auto inMetadata = false;
		auto closed     = false;
		while (const auto next = line()) {
			if (trim(*next) == "---" && !next->starts_with(' ')) {
				closed = true;
				break;
			}
			auto key = std::string_view{ };
			if (const auto value = key_value(*next, 0U, key)) {
				inMetadata = key == "metadata" && trim(*value).empty();
				if (key == "name") {
					name = scalar(*value);
				} else if (key == "description") {
					facts.Description = scalar(*value).value_or(std::string{ });
				}
				continue;
			}
			if (!inMetadata) {
				continue;
			}
			const auto indent = next->find_first_not_of(' ');
			if (indent == std::string_view::npos || indent == 0U) {
				continue;
			}
			if (const auto value = key_value(*next, indent, key); value && key == "bpr-version") {
				const auto text    = scalar(*value).value_or(std::string{ });
				auto       version = std::uint32_t{ 0U };
				const auto parsed  = std::from_chars(text.data(), text.data() + text.size(), version);
				if (parsed.ec == std::errc{ } && parsed.ptr == text.data() + text.size() && !text.empty()) {
					facts.Version = version;
				}
			}
		}
		if (!closed) {
			return std::unexpected(review_skill_refusal::no_front_matter);
		}
		if (!name || *name != review_skill_name) {
			return std::unexpected(review_skill_refusal::name_changed);
		}
		if (trim(facts.Description).empty()) {
			return std::unexpected(review_skill_refusal::no_description);
		}
		return facts;
	}

	auto newer_review_skill(const std::uint32_t aBuiltInVersion, const std::optional<review_skill_copy_record>& aCopy) noexcept -> std::optional<std::uint32_t>
	{
		if (!aCopy || aBuiltInVersion <= aCopy->BaseVersion || (aCopy->KeptVersion && *aCopy->KeptVersion >= aBuiltInVersion)) {
			return std::nullopt;
		}
		return aBuiltInVersion;
	}
} // namespace bpr::core
