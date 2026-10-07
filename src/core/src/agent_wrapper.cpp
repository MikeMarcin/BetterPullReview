#include "bpr/core/agent_wrapper.hpp"

#include <algorithm>

namespace bpr::core {
	static_assert(maximum_agent_wrapper_bytes == 1024U, "describe(too_long) names the bound");

	namespace {
		[[nodiscard]] auto control(const char aCharacter) noexcept -> bool
		{
			const auto byte = static_cast<unsigned char>(aCharacter);
			return byte < 0x20U || byte == 0x7FU;
		}

		[[nodiscard]] auto letter(const char aCharacter) noexcept -> bool
		{
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z');
		}

		[[nodiscard]] auto separator(const char aCharacter) noexcept -> bool
		{
			return aCharacter == '\\' || aCharacter == '/';
		}

		/// A drive path (`C:\`) or a UNC or device path (`\\server`, `\\?\`).
		[[nodiscard]] auto windows_absolute(const std::string_view aPath) noexcept -> bool
		{
			return (aPath.size() >= 3U && letter(aPath[0]) && aPath[1] == ':' && separator(aPath[2]))
				|| (aPath.size() >= 2U && separator(aPath[0]) && separator(aPath[1]));
		}

		/// The final name's extension, dot included; empty when the name has
		/// none. "x.exe." and "x.exe " have none that matches, so a name
		/// Windows would trim is refused rather than trimmed.
		[[nodiscard]] auto extension(const std::string_view aPath) noexcept -> std::string_view
		{
			const auto name = aPath.substr(aPath.find_last_of("\\/") + 1U);
			const auto dot  = name.rfind('.');
			return dot == std::string_view::npos ? std::string_view{ } : name.substr(dot);
		}

		[[nodiscard]] auto same_letters(const std::string_view aText, const std::string_view aLower) noexcept -> bool
		{
			return std::ranges::equal(aText, aLower, [](const char aLeft, const char aRight) {
				return (aLeft >= 'A' && aLeft <= 'Z' ? static_cast<char>(aLeft - 'A' + 'a') : aLeft) == aRight;
			});
		}
	}

	auto inspect_agent_wrapper_text(const std::string_view aPath, const agent_wrapper_platform aPlatform) noexcept
		-> std::optional<agent_wrapper_refusal>
	{
		if (std::ranges::any_of(aPath, control)) {
			return agent_wrapper_refusal::control_character;
		}
		if (aPath.size() > maximum_agent_wrapper_bytes) {
			return agent_wrapper_refusal::too_long;
		}
		if (aPath.starts_with('~')) {
			return agent_wrapper_refusal::home_relative;
		}
		if (aPlatform == agent_wrapper_platform::posix) {
			return aPath.starts_with('/') ? std::nullopt : std::optional{ agent_wrapper_refusal::relative };
		}
		if (!windows_absolute(aPath)) {
			return agent_wrapper_refusal::relative;
		}
		const auto name = extension(aPath);
		if (same_letters(name, ".bat") || same_letters(name, ".cmd")) {
			return agent_wrapper_refusal::windows_script;
		}
		// A colon in the final name selects an alternate data stream, never
		// the file the name reads as.
		const auto file = aPath.substr(aPath.find_last_of("\\/") + 1U);
		if (!same_letters(name, ".exe") || file.find(':') != std::string_view::npos) {
			return agent_wrapper_refusal::windows_not_program;
		}
		return std::nullopt;
	}

	auto storable_agent_wrapper(const std::string_view aPath) noexcept -> bool
	{
		return aPath.size() <= maximum_agent_wrapper_bytes && std::ranges::none_of(aPath, control);
	}

	auto describe(const agent_wrapper_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case agent_wrapper_refusal::control_character:
			return "The path contains a line break or another control character.";
		case agent_wrapper_refusal::too_long:
			return "The path is longer than 1024 bytes.";
		case agent_wrapper_refusal::home_relative:
			return "The path starts with ~; write the full path, because no shell expands it.";
		case agent_wrapper_refusal::relative:
			return "The path is not absolute; write the full path from the root.";
		case agent_wrapper_refusal::windows_script:
			return "A .bat or .cmd file runs only through cmd.exe, a shell; choose an .exe.";
		case agent_wrapper_refusal::windows_not_program:
			return "The file is not an .exe program.";
		case agent_wrapper_refusal::missing:
			return "Nothing exists at this path.";
		case agent_wrapper_refusal::directory:
			return "The path names a folder, not a program.";
		case agent_wrapper_refusal::not_regular_file:
			return "The path names something other than a regular file.";
		case agent_wrapper_refusal::not_executable:
			return "You do not have permission to run this file.";
		}
		return "The path cannot be used.";
	}
} // namespace bpr::core
