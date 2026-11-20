#include "bpr/core/agent_program_bundle.hpp"

#include <algorithm>
#include <cctype>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto without_trailing_slashes(std::string_view aPath) -> std::string_view
		{
			while (aPath.size() > 1U && aPath.back() == '/') {
				aPath.remove_suffix(1U);
			}
			return aPath;
		}

		[[nodiscard]] auto ascii_lower(const char aCharacter) -> char { return static_cast<char>(std::tolower(static_cast<unsigned char>(aCharacter))); }

		[[nodiscard]] auto plain_name(const std::string_view aName) -> bool { return !aName.empty() && aName != "." && aName != ".." && aName.find('/') == std::string_view::npos && aName.find('\0') == std::string_view::npos; }

		[[nodiscard]] auto program_path(const std::string_view aBundle, const std::string_view aName) -> std::string
		{
			auto path = std::string{ without_trailing_slashes(aBundle) };
			path += "/Contents/MacOS/";
			path += aName;
			return path;
		}
	} // namespace

	auto names_app_bundle(const std::string_view aPath) -> bool
	{
		const auto     path  = without_trailing_slashes(aPath);
		const auto     slash = path.rfind('/');
		const auto     last  = slash == std::string_view::npos ? path : path.substr(slash + 1U);
		constexpr auto tail  = std::string_view{ ".app" };
		return last.size() > tail.size() && std::ranges::equal(last.substr(last.size() - tail.size()), tail, [](const char aLeft, const char aRight) { return ascii_lower(aLeft) == aRight; });
	}

	auto resolve_app_bundle_program(const app_bundle_listing& aListing) -> std::expected<std::string, app_bundle_refusal>
	{
		if (!aListing.Programs) {
			return std::unexpected{ app_bundle_refusal::no_programs_folder };
		}
		const auto& programs = *aListing.Programs;
		if (aListing.NamedProgram) {
			const auto& named = *aListing.NamedProgram;
			const auto  found = plain_name(named) && std::ranges::any_of(programs, [&named](const app_bundle_file& aFile) { return aFile.Name == named && !aFile.Directory; });
			if (!found) {
				return std::unexpected{ app_bundle_refusal::named_program_missing };
			}
			return program_path(aListing.Bundle, named);
		}
		const auto executable = [](const app_bundle_file& aFile) { return aFile.Executable && !aFile.Directory && plain_name(aFile.Name); };
		const auto count      = std::ranges::count_if(programs, executable);
		if (count == 0) {
			return std::unexpected{ app_bundle_refusal::no_program };
		}
		if (count > 1) {
			return std::unexpected{ app_bundle_refusal::several_programs };
		}
		return program_path(aListing.Bundle, std::ranges::find_if(programs, executable)->Name);
	}
} // namespace bpr::core
