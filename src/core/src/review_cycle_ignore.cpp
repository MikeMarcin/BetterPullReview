#include "bpr/core/review_cycle_ignore.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

namespace bpr::core {
	namespace {
		struct bracket_result
		{
			bool        Matched = false;
			std::size_t Next    = 0;
		};

		/// One `[...]` expression at `aStart`, or nothing when it is unterminated.
		auto bracket(const std::string_view aPattern, const std::size_t aStart, const char aCharacter) -> std::optional<bracket_result>
		{
			const auto value   = static_cast<unsigned char>(aCharacter);
			auto       index   = aStart + 1;
			auto       negated = false;
			if (index < aPattern.size() && (aPattern[index] == '!' || aPattern[index] == '^')) {
				negated = true;
				++index;
			}
			auto matched = false;
			auto first   = true;
			while (index < aPattern.size() && (aPattern[index] != ']' || first)) {
				first = false;
				if (aPattern[index] == '[' && index + 1 < aPattern.size() && aPattern[index + 1] == ':') {
					// A POSIX class is not supported: it matches nothing.
					const auto close = aPattern.find(":]", index + 2);
					if (close == std::string_view::npos) {
						return std::nullopt;
					}
					index = close + 2;
					continue;
				}
				auto low = aPattern[index];
				if (low == '\\' && index + 1 < aPattern.size()) {
					low = aPattern[++index];
				}
				if (index + 2 < aPattern.size() && aPattern[index + 1] == '-' && aPattern[index + 2] != ']') {
					auto high = aPattern[index + 2];
					index += 3;
					if (high == '\\' && index < aPattern.size()) {
						high = aPattern[index++];
					}
					matched = matched || (static_cast<unsigned char>(low) <= value && value <= static_cast<unsigned char>(high));
					continue;
				}
				matched = matched || static_cast<unsigned char>(low) == value;
				++index;
			}
			if (index >= aPattern.size()) {
				return std::nullopt;
			}
			return bracket_result{ .Matched = matched != negated, .Next = index + 1 };
		}

		auto match(const std::string_view aPattern, std::size_t aPatternIndex, const std::string_view aText, std::size_t aTextIndex) -> bool
		{
			while (aPatternIndex < aPattern.size()) {
				auto character = aPattern[aPatternIndex];
				if (character == '*') {
					const auto twice        = aPatternIndex + 1 < aPattern.size() && aPattern[aPatternIndex + 1] == '*';
					const auto after        = aPatternIndex + (twice ? 2U : 1U);
					const auto segmentStart = aPatternIndex == 0 || aPattern[aPatternIndex - 1] == '/';
					const auto segmentEnd   = after == aPattern.size() || aPattern[after] == '/';
					if (twice && segmentStart && segmentEnd) {
						if (after == aPattern.size()) {
							return true;
						}
						const auto rest = after + 1;
						if (match(aPattern, rest, aText, aTextIndex)) {
							return true;
						}
						for (auto position = aTextIndex; position < aText.size(); ++position) {
							if (aText[position] == '/' && match(aPattern, rest, aText, position + 1)) {
								return true;
							}
						}
						return false;
					}
					while (aPatternIndex < aPattern.size() && aPattern[aPatternIndex] == '*') {
						++aPatternIndex;
					}
					if (aPatternIndex == aPattern.size()) {
						return aText.find('/', aTextIndex) == std::string_view::npos;
					}
					for (auto position = aTextIndex; position <= aText.size(); ++position) {
						if (match(aPattern, aPatternIndex, aText, position)) {
							return true;
						}
						if (position < aText.size() && aText[position] == '/') {
							return false;
						}
					}
					return false;
				}
				if (aTextIndex >= aText.size()) {
					return false;
				}
				if (character == '?') {
					if (aText[aTextIndex] == '/') {
						return false;
					}
					++aPatternIndex;
					++aTextIndex;
					continue;
				}
				if (character == '[') {
					if (aText[aTextIndex] == '/') {
						return false;
					}
					if (const auto result = bracket(aPattern, aPatternIndex, aText[aTextIndex])) {
						if (!result->Matched) {
							return false;
						}
						aPatternIndex = result->Next;
						++aTextIndex;
						continue;
					}
				}
				if (character == '\\' && aPatternIndex + 1 < aPattern.size()) {
					character = aPattern[++aPatternIndex];
				}
				if (aText[aTextIndex] != character) {
					return false;
				}
				++aPatternIndex;
				++aTextIndex;
			}
			return aTextIndex == aText.size();
		}

		/// Removes unescaped trailing spaces, as Git does; `\ ` keeps one.
		auto trim_trailing_spaces(std::string_view aLine) -> std::string_view
		{
			while (!aLine.empty() && aLine.back() == ' ') {
				if (aLine.size() >= 2 && aLine[aLine.size() - 2] == '\\') {
					break;
				}
				aLine.remove_suffix(1);
			}
			return aLine;
		}

		constexpr auto default_names = std::array<std::string_view, 22>{
			// Version control and tool state.
			".git",
			".hg",
			".svn",
			".bzr",
			"_darcs",
			".jj",
			".DS_Store",
			"Thumbs.db",
			// Caches that are regenerated, never reviewed.
			"__pycache__",
			".pytest_cache",
			".mypy_cache",
			".tox",
			".venv",
			"node_modules",
			// Common credential stores.
			".ssh",
			".gnupg",
			".aws",
			".azure",
			".kube",
			".netrc",
			".git-credentials",
			".pypirc",
		};
	}

	auto wildcard_match(const std::string_view aPattern, const std::string_view aPath) -> bool
	{
		return match(aPattern, 0, aPath, 0);
	}

	auto workspace_ignore_rules::add(const std::string_view aBase, std::string_view aContents) -> bool
	{
		auto parsed = std::vector<rule>{ };
		while (!aContents.empty()) {
			const auto end  = aContents.find('\n');
			auto       line = aContents.substr(0, end);
			aContents       = end == std::string_view::npos ? std::string_view{ } : aContents.substr(end + 1);
			if (!line.empty() && line.back() == '\r') {
				line.remove_suffix(1);
			}
			line = trim_trailing_spaces(line);
			if (line.empty() || line.front() == '#') {
				continue;
			}
			auto current = rule{ .Base = std::string{ aBase } };
			if (line.front() == '!') {
				current.Negated = true;
				line.remove_prefix(1);
			} else if (line.starts_with("\\!") || line.starts_with("\\#")) {
				line.remove_prefix(1);
			}
			if (!line.empty() && line.back() == '/') {
				current.DirectoryOnly = true;
				line.remove_suffix(1);
			}
			current.Anchored = line.find('/') != std::string_view::npos;
			if (!line.empty() && line.front() == '/') {
				line.remove_prefix(1);
			}
			if (line.empty()) {
				continue;
			}
			current.Pattern = std::string{ line };
			parsed.push_back(std::move(current));
		}
		if (Rules.size() + parsed.size() > maximum_ignore_rules) {
			return false;
		}
		std::ranges::move(parsed, std::back_inserter(Rules));
		return true;
	}

	auto workspace_ignore_rules::ignored(const std::string_view aPath, const bool aDirectory) const -> bool
	{
		for (const auto& current : std::views::reverse(Rules)) {
			if (current.DirectoryOnly && !aDirectory) {
				continue;
			}
			auto relative = aPath;
			if (!current.Base.empty()) {
				if (!aPath.starts_with(current.Base) || aPath.size() <= current.Base.size() || aPath[current.Base.size()] != '/') {
					continue;
				}
				relative = aPath.substr(current.Base.size() + 1);
			}
			const auto subject = current.Anchored ? relative : relative.substr(relative.find_last_of('/') + 1);
			if (wildcard_match(current.Pattern, subject)) {
				return !current.Negated;
			}
		}
		return false;
	}

	namespace {
		/// The path below `aBase`, or nothing when it is not beneath it.
		auto below(const std::string_view aBase, const std::string_view aPath) -> std::optional<std::string_view>
		{
			if (aBase.empty()) {
				return aPath;
			}
			if (!aPath.starts_with(aBase) || aPath.size() <= aBase.size() || aPath[aBase.size()] != '/') {
				return std::nullopt;
			}
			return aPath.substr(aBase.size() + 1);
		}
	}

	auto workspace_attribute_rules::add(const std::string_view aBase, std::string_view aContents) -> bool
	{
		constexpr auto space  = std::string_view{ " \t" };
		auto           parsed = std::vector<rule>{ };
		while (!aContents.empty()) {
			const auto end  = aContents.find('\n');
			auto       line = aContents.substr(0, end);
			aContents       = end == std::string_view::npos ? std::string_view{ } : aContents.substr(end + 1);
			if (!line.empty() && line.back() == '\r') {
				line.remove_suffix(1);
			}
			const auto first = line.find_first_not_of(space);
			if (first == std::string_view::npos || line[first] == '#') {
				continue;
			}
			line                  = line.substr(first);
			const auto patternEnd = line.find_first_of(space);
			auto       pattern    = line.substr(0, patternEnd);
			auto       setting    = std::optional<bool>{ };
			for (auto rest = patternEnd == std::string_view::npos ? std::string_view{ } : line.substr(patternEnd); !rest.empty();) {
				const auto start = rest.find_first_not_of(space);
				if (start == std::string_view::npos) {
					break;
				}
				rest             = rest.substr(start);
				const auto token = rest.substr(0, rest.find_first_of(space));
				rest             = rest.substr(token.size());
				if (token.starts_with("filter=")) {
					setting = true;
				} else if (token == "-filter" || token == "!filter") {
					setting = false;
				}
			}
			// Attribute patterns never match a directory's contents.
			if (!setting || pattern.empty() || pattern.back() == '/') {
				continue;
			}
			auto current = rule{ .Base = std::string{ aBase }, .Anchored = pattern.find('/') != std::string_view::npos, .Set = *setting };
			if (pattern.front() == '/') {
				pattern.remove_prefix(1);
			}
			current.Pattern = std::string{ pattern };
			parsed.push_back(std::move(current));
		}
		if (Rules.size() + parsed.size() > maximum_ignore_rules) {
			return false;
		}
		std::ranges::move(parsed, std::back_inserter(Rules));
		return true;
	}

	auto workspace_attribute_rules::filter(const std::string_view aPath) const -> std::optional<bool>
	{
		for (const auto& current : std::views::reverse(Rules)) {
			const auto relative = below(current.Base, aPath);
			if (!relative) {
				continue;
			}
			const auto subject = current.Anchored ? *relative : relative->substr(relative->find_last_of('/') + 1);
			if (wildcard_match(current.Pattern, subject)) {
				return current.Set;
			}
		}
		return std::nullopt;
	}

	auto default_exclusion_names() noexcept -> std::span<const std::string_view>
	{
		return default_names;
	}

	auto excluded_by_default(const std::string_view aName) noexcept -> bool
	{
		return std::ranges::find(default_names, aName) != default_names.end();
	}
}
