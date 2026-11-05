#ifndef BPR_CORE_WINDOW_TITLE_HPP
#define BPR_CORE_WINDOW_TITLE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-1864]]. The words a workbench window's title says.
///
/// \details The owner, 2026-11-04: "Window title in the dock on Mac id like to
/// see the open PR if there is one rather than the app name and the profile".
/// The title is what the Dock's window list, the Window menu, Mission Control
/// and the Windows and Linux taskbars name the window by; macOS hides the
/// title bar itself. With a pull request shown it names that pull request,
/// its number first, then its title, then where it lives:
/// "#188875 Add the Flow Control… — owner/repo". A long title is shortened at
/// a word, so the number and the repository survive a narrow menu. The
/// profile follows only when more than one profile exists, where it is what
/// tells two windows on the same pull request apart. Home, and a window with
/// no pull request, keep the application's name and the profile.
namespace bpr::core {
	/// \brief The most characters (code points) of a pull request's title a
	/// window title keeps, the ellipsis included.
	inline constexpr std::size_t maximum_window_title_pull_request_characters = 48U;

	/// \brief The application's name, the title of a window with nothing open.
	inline constexpr std::string_view window_title_application = "BetterPullReview";

	/// \brief The pull request a window shows, as its title names it.
	struct window_title_pull_request
	{
		std::string_view Owner;
		std::string_view Repository;
		std::uint64_t    Number = 0U;
		/// GitHub's title, literally; it may be empty or very long.
		std::string_view Title;
	};

	/// \brief What decides a window's title.
	struct window_title_input
	{
		/// The pull request the window shows, accepted or named by GitHub's
		/// first answer while it loads; absent on Home and with nothing open.
		std::optional<window_title_pull_request> PullRequest;
		/// The window's profile label; empty before one was admitted.
		std::string_view Profile;
		/// How many profiles exist; the profile follows a pull request only
		/// when there is more than one.
		std::size_t Profiles = 0U;
		/// Words for a mode the window is in, said in Home's title ("Reading
		/// without signing in"); empty when there is none.
		std::string_view Mode;
	};

	/// \brief A pull request title on one line and at most `aCharacters` code
	/// points: shortened at the last space when one is near enough, ending in
	/// "…". Control characters read as spaces, and runs of spaces as one.
	[[nodiscard]] auto shorten_window_title_text(std::string_view aText, std::size_t aCharacters) -> std::string;

	/// \brief The window's title.
	[[nodiscard]] auto window_title(const window_title_input& aInput) -> std::string;
} // namespace bpr::core

#endif // BPR_CORE_WINDOW_TITLE_HPP
