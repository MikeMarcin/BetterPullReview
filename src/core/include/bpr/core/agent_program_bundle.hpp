#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_PROGRAM_BUNDLE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_PROGRAM_BUNDLE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-2303]]. The most entries of an app's `Contents/MacOS`
	/// folder the host lists; a bundle holding more is read as its first ones.
	inline constexpr std::size_t maximum_app_bundle_programs = 256U;

	/// \brief One entry of an app bundle's `Contents/MacOS` folder, by
	/// metadata the host read without following links.
	struct app_bundle_file
	{
		std::string Name;
		bool        Directory = false;
		/// A regular file, not a link, with an execute permission bit set.
		bool Executable = false;
	};

	/// \brief [[BPR-2303]] (owner, 2026-11-19: "The executable I need to
	/// point at is inside of an app bundle"). What the host read of a macOS
	/// `.app` bundle: the bundle path as chosen, the `CFBundleExecutable` its
	/// `Contents/Info.plist` names, and its `Contents/MacOS` folder.
	struct app_bundle_listing
	{
		std::string Bundle;
		/// None when `Contents/Info.plist` is missing, unreadable or names no
		/// `CFBundleExecutable` string.
		std::optional<std::string> NamedProgram;
		/// None when `Contents/MacOS` is not a folder.
		std::optional<std::vector<app_bundle_file>> Programs;
	};

	/// \brief Why an app bundle gives no program to run.
	enum class app_bundle_refusal : std::uint8_t {
		/// The bundle has no `Contents/MacOS` folder.
		no_programs_folder,
		/// `CFBundleExecutable` names a program `Contents/MacOS` does not hold.
		named_program_missing,
		/// No name, and `Contents/MacOS` holds no executable file.
		no_program,
		/// No name, and `Contents/MacOS` holds more than one executable file.
		several_programs,
	};

	/// \brief Whether a path a reviewer chose or typed names an app bundle: its
	/// last component, past any trailing slashes, ends in `.app` (any case)
	/// after at least one other character.
	[[nodiscard]] auto names_app_bundle(std::string_view aPath) -> bool;

	/// \brief The program an app bundle runs.
	///
	/// \details `CFBundleExecutable` wins when it names an entry of
	/// `Contents/MacOS` that is not a folder; a name holding a `/` or naming
	/// `.` or `..` is never found there. With no name, the one executable file
	/// in `Contents/MacOS` is the program. The result is
	/// `<Bundle>/Contents/MacOS/<name>` with the bundle's trailing slashes
	/// dropped. Whether the program can run is checked where it always was;
	/// this only says which file it is.
	[[nodiscard]] auto resolve_app_bundle_program(const app_bundle_listing& aListing) -> std::expected<std::string, app_bundle_refusal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_PROGRAM_BUNDLE_HPP
