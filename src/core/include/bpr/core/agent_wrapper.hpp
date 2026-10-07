#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_WRAPPER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_WRAPPER_HPP

#include "bpr/core/agent_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-1301]]. Why a Claude process wrapper path is refused.
	///
	/// \details A wrapper is one absolute path to a program a person chose to
	/// run in place of `claude`. It is a path, never a command line: nothing
	/// splits, quotes or expands it, and no shell runs it. The first five
	/// reasons are read from the text alone; the rest need the file, and only
	/// the agent adapter, which owns agent launches, looks at one.
	enum class agent_wrapper_refusal : std::uint8_t {
		/// A NUL, line break or other control character.
		control_character,
		/// Longer than `maximum_agent_wrapper_bytes`.
		too_long,
		/// Begins with `~`, which only a shell would expand.
		home_relative,
		/// Not an absolute path on this platform.
		relative,
		/// [[BPR-1301]], owner 2026-10-06 ("Accept (.exe only)"). A Windows
		/// `.bat` or `.cmd` file, which `CreateProcessW` runs through
		/// `cmd.exe`, a shell.
		windows_script,
		/// Any other Windows file that is not an `.exe`.
		windows_not_program,
		/// Nothing exists at the path, or the path cannot be reached.
		missing,
		/// The path names a folder.
		directory,
		/// The path names something other than a regular file.
		not_regular_file,
		/// A regular file this user may not execute.
		not_executable,
	};

	/// \brief Which path rules the text is read under. The core knows no
	/// platform; the adapter that checks a file names its own.
	enum class agent_wrapper_platform : std::uint8_t {
		posix,
		windows,
	};

	/// \brief The longest wrapper path stored or launched, in UTF-8 bytes.
	inline constexpr std::size_t maximum_agent_wrapper_bytes = maximum_agent_text_bytes;

	/// \brief Refuses what the text alone shows is not a wrapper path.
	///
	/// \details Empty text is "no wrapper", which callers handle before
	/// asking; asked anyway, it is `relative`.
	[[nodiscard]] auto inspect_agent_wrapper_text(std::string_view aPath, agent_wrapper_platform aPlatform) noexcept
		-> std::optional<agent_wrapper_refusal>;

	/// \brief Whether the text may be stored: empty, or bounded and free of
	/// control characters. A stored path is checked again before each launch.
	[[nodiscard]] auto storable_agent_wrapper(std::string_view aPath) noexcept -> bool;

	/// \brief One line saying why, without the path.
	[[nodiscard]] auto describe(agent_wrapper_refusal aRefusal) noexcept -> std::string_view;

	/// \brief [[BPR-1301]]. Where a `claude` used without a wrapper was found.
	enum class claude_found_in : std::uint8_t {
		/// An absolute entry of the application's own `PATH`.
		path,
		/// One of the fixed usual install folders, searched after `PATH`.
		usual_folder,
	};

	/// \brief [[BPR-1301]]. One place to look for `claude`, in search order.
	struct claude_search_candidate
	{
		std::string     Path;
		claude_found_in Origin = claude_found_in::path;

		auto operator<=>(const claude_search_candidate&) const = default;
	};

	/// \brief [[BPR-1301]]. The fixed usual install folders searched after
	/// `PATH`, in order.
	///
	/// \details A macOS application opened from the Finder or the Dock gets
	/// launchd's `PATH` (`/usr/bin:/bin:/usr/sbin:/sbin`), which holds none
	/// of the places `claude` is installed, and no shell is ever started to
	/// ask for a login `PATH`. So, on POSIX: `HOME/.local/bin` (the native
	/// installer), `HOME/.claude/local` (the older local install),
	/// `/opt/homebrew/bin` and `/usr/local/bin` (Homebrew, and npm's global
	/// folder under a Homebrew or installer Node) and `HOME/.npm-global/bin`.
	/// On Windows, where an application gets the user's whole `PATH`,
	/// `HOME\.local\bin`, the native installer's folder. A `HOME` folder is
	/// left out when `aHome` is not absolute.
	[[nodiscard]] auto usual_claude_folders(std::string_view aHome, agent_wrapper_platform aPlatform) -> std::vector<std::string>;

	/// \brief [[BPR-1301]], owner 2026-10-07 ("Yes, search PATH
	/// (Recommended)"). The ordered places a Claude launch with no saved
	/// wrapper looks for `claude`.
	///
	/// \details First each entry of `aPathVariable` (split on `:`, or `;` on
	/// Windows), then each of `aUsualFolders`, joined with `claude`
	/// (`claude.exe` on Windows; no `PATHEXT` script, as a wrapper may not be
	/// one either). An empty, relative or `~` folder, the current directory
	/// included, is skipped, as is one with a control character or a result
	/// over the wrapper bound. A path already listed is not listed again.
	/// Nothing here reads a file: the caller checks each in turn and takes
	/// the first that is an executable regular file.
	[[nodiscard]] auto claude_search_candidates(std::string_view aPathVariable, std::span<const std::string> aUsualFolders, agent_wrapper_platform aPlatform)
		-> std::vector<claude_search_candidate>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_WRAPPER_HPP
