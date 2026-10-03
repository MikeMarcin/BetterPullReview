#ifndef BPR_TESTS_END_TO_END_NATIVE_MAIN_MENU_PROBE_HPP
#define BPR_TESTS_END_TO_END_NATIVE_MAIN_MENU_PROBE_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/// \brief Reads and drives the macOS application menu bar through AppKit.
///
/// \details RFC-BPR-0033 B1 and B2 are claims about `[NSApp mainMenu]`, the
/// Dock and application activation - native facts no widget tree can show. This
/// probe answers them from AppKit itself, in the process under test, so a course
/// running on the cocoa platform can assert the menu bar the owner sees rather
/// than the `QMenuBar` the composition holds. Every entry point is process-scoped
/// and performs no effect outside the test process.
namespace bpr::tests::native_menu {
	/// \brief One line for `[NSApp mainMenu]`: `Title[item;item]|Title[...]`.
	///
	/// \details A top-level item without a submenu prints `Title[-]`; a hidden
	/// top-level item prints `~Title[...]`. The application menu AppKit owns is
	/// printed under its own title so a course can tell it from the menus Qt
	/// installs. An empty string means the application has no main menu.
	[[nodiscard]] auto describe_main_menu() -> std::string;
	/// \brief Whether `[NSApp mainMenu]` is exactly the given `NSMenu`.
	[[nodiscard]] auto main_menu_is(const void* aNsMenu) -> bool;
	/// \brief Whether the main menu carries a visible top-level menu with the
	/// given title whose submenu holds a visible item with the given title.
	///
	/// \details An empty menu title searches every top-level menu, which is how
	/// a course finds the Quit item AppKit merges into the application menu. An
	/// item title matches exactly or as the first word, because that merged item
	/// carries the application name after it.
	[[nodiscard]] auto main_menu_offers(std::string_view aMenuTitle, std::string_view aItemTitle) -> bool;
	/// \brief The visible top-level menu titles of `[NSApp mainMenu]`, in order.
	///
	/// \details These are the words a reviewer reads across the top of the
	/// screen. A menu AppKit hides - an emptied `File` whose only item merged
	/// into the application menu - is not one of them.
	[[nodiscard]] auto main_menu_visible_titles() -> std::vector<std::string>;
	/// \brief The visible, non-separator item titles of the named top-level menu.
	///
	/// \details Comparing this list before and after a close says whether a
	/// command left the bar, without the course having to spell out a title Qt
	/// or AppKit owns, such as the merged `Preferences` item.
	[[nodiscard]] auto main_menu_items(std::string_view aMenuTitle) -> std::vector<std::string>;
	/// \brief Whether the named item of the named top-level menu is actionable.
	[[nodiscard]] auto main_menu_item_is_enabled(std::string_view aMenuTitle, std::string_view aItemTitle) -> bool;
	/// \brief Whether the process is the active application.
	///
	/// \details This is the difference between what `[NSApp mainMenu]` holds and
	/// what a person sees: only the active application's menu bar is drawn. An
	/// inactive application with no window is present in the Dock and nowhere
	/// else, which is exactly how [[BPR-0796]]'s rejection described it.
	[[nodiscard]] auto application_is_active() -> bool;
	/// \brief Whether the application is hidden, as `-[NSApplication hide:]` leaves it.
	[[nodiscard]] auto application_is_hidden() -> bool;
	/// \brief Whether the console session's screen is locked, which is when no
	/// application can be activated and the login window is frontmost.
	[[nodiscard]] auto session_screen_is_locked() -> bool;
	/// \brief The localized name of the frontmost application.
	[[nodiscard]] auto frontmost_application_name() -> std::string;
	/// \brief Whether the process asked AppKit for a Dock tile and a menu bar.
	[[nodiscard]] auto activation_policy_is_regular() -> bool;
	/// \brief Count of AppKit windows currently on screen, not counting the
	/// windows AppKit draws this process's menu bar status items in.
	[[nodiscard]] auto visible_window_count() -> std::size_t;
	/// \brief Count of this process's status items AppKit has on the menu bar:
	/// each is drawn in a visible `NSStatusBarWindow` of its own ([[BPR-1250]]).
	[[nodiscard]] auto visible_status_item_count() -> std::size_t;
	/// \brief Records every later `-[NSApplication setMainMenu:]` with a timestamp.
	void begin_main_menu_trace();
	/// \brief The recorded installations since the trace began, oldest first.
	[[nodiscard]] auto main_menu_trace() -> std::vector<std::string>;
	/// \brief Asks AppKit to make this process the active application.
	///
	/// \details An ask, not a guarantee. Current macOS reserves activation for
	/// the person: a process that is not already active is routinely refused,
	/// which a caller must treat as "not run" rather than as a failure of the
	/// application under test.
	void activate_application();
	/// \brief Hides the application, which resigns activation.
	void hide_application();
	/// \brief Unhides the application.
	///
	/// \details `-[NSApplication unhide:]` restores an application's windows; it
	/// does not take activation, and an application with no window has none to
	/// restore. Measured on a plain AppKit application on macOS 26: after
	/// `hide:` then `unhide:`, and again after `activateIgnoringOtherApps:`,
	/// `isActive` stays `NO`.
	void unhide_application();
	/// \brief Delivers the delegate message AppKit sends for a Dock icon click.
	///
	/// \details `applicationShouldHandleReopen:hasVisibleWindows:` is what the
	/// Dock sends to a running application. Sending it here exercises everything
	/// the process does with that message; the click itself stays with the Dock.
	[[nodiscard]] auto send_dock_reopen() -> bool;
	/// \brief Sends a Command-key key-down through `-[NSApplication sendEvent:]`.
	///
	/// \details With no key window AppKit resolves the key equivalent against the
	/// main menu, which is how a menu bar shortcut reaches a resident application
	/// with no window. Returns false when there is no main menu to resolve it.
	[[nodiscard]] auto send_command_key(char aKey) -> bool;
	/// \brief Performs the named item of the named main-menu menu as a click would.
	[[nodiscard]] auto perform_main_menu_item(std::string_view aMenuTitle, std::string_view aItemTitle) -> bool;
}

#endif
