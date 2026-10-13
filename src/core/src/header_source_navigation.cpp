#include "bpr/core/header_source_navigation.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto ascii_lower(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter + ('a' - 'A')) : aCharacter;
		}

		[[nodiscard]] auto extension_is(const std::string_view aExtension, const std::string_view aExpected) noexcept -> bool
		{
			return std::ranges::equal(aExtension, aExpected, { }, ascii_lower);
		}
	}

	auto classify_header_source_path(const std::string_view aPath) noexcept -> std::optional<header_source_path>
	{
		const auto slash = aPath.rfind('/');
		const auto name  = slash == std::string_view::npos ? aPath : aPath.substr(slash + 1U);
		const auto dot   = name.rfind('.');
		if (dot == std::string_view::npos || dot == 0U) {
			return std::nullopt;
		}
		const auto extension = name.substr(dot);
		const auto headers   = std::array{ std::string_view{ ".h" }, std::string_view{ ".hh" }, std::string_view{ ".hpp" }, std::string_view{ ".hxx" } };
		const auto sources   = std::array{ std::string_view{ ".c" }, std::string_view{ ".cc" }, std::string_view{ ".cpp" }, std::string_view{ ".cxx" } };
		const auto matches   = [extension](const auto aExpected) { return extension_is(extension, aExpected); };
		const auto header    = std::ranges::any_of(headers, matches);
		if (!header && !std::ranges::any_of(sources, matches)) {
			return std::nullopt;
		}
		return header_source_path{ slash == std::string_view::npos ? std::string_view{ } : aPath.substr(0U, slash), name.substr(0U, dot), header ? header_source_family::header : header_source_family::source };
	}

	auto take_header_source_directory_component(std::string_view& aDirectory) noexcept -> std::string_view
	{
		const auto slash     = aDirectory.rfind('/');
		const auto component = slash == std::string_view::npos ? aDirectory : aDirectory.substr(slash + 1U);
		aDirectory           = slash == std::string_view::npos ? std::string_view{ } : aDirectory.substr(0U, slash);
		return component;
	}

	void header_source_candidates::include(const file_identity aFile) noexcept
	{
		if (!aFile.valid() || Ambiguous || Unique == aFile) {
			return;
		}
		if (Unique.has_value()) {
			Unique.reset();
			Ambiguous = true;
		} else {
			Unique = aFile;
		}
	}

	auto resolve_header_source_match(const header_source_family      aSelectedFamily,
									 const header_source_candidates& aSameDirectory, const header_source_candidates& aClosestSuffix) noexcept -> header_source_resolution
	{
		const auto& preferred = aSameDirectory.Ambiguous || aSameDirectory.Unique ? aSameDirectory : aClosestSuffix;
		return { preferred.Ambiguous ? header_source_status::ambiguous : preferred.Unique ? header_source_status::matched
																						  : header_source_status::no_match,
				 preferred.Ambiguous ? std::nullopt : preferred.Unique, aSelectedFamily };
	}
}
