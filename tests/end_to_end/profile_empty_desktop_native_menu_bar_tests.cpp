#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/settings_window.hpp"
#include "native_main_menu_probe.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QThread>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// RFC-BPR-0033 B1 and B2, the native half. The offscreen management course proves
// the composition keeps a parentless `desktop_menu_bar`; the owner's attended
// review of the shipped build found that closing the last window left the macOS
// menu bar with nothing in it (BPR-0782, `bpr-0779-empty-desktop-dock`). This
// course runs the real desktop composition on the cocoa platform and asserts what
// `[NSApp mainMenu]` carries at every event-loop turn from the last close to the
// settled empty desktop, then reopens through the Dock's reopen message, native
// activation, Command-N and the Profiles menu as AppKit delivers them.
//
// Qt installs a menu bar from `QGuiApplication::focusWindow()`, which the cocoa
// plugin sets when AppKit makes a window key (qcocoawindow.mm, windowDidBecomeKey
// and windowDidResignKey). Only the active application has a key window, and a
// locked screen lets nothing become active. When this process cannot be activated
// the course delivers those two key-window transitions itself through the same
// QPA entry point the plugin uses, says so, and leaves the activation gesture
// recorded as not run; everything downstream of that delivery - Qt's cocoa menu
// bar arbitration and AppKit's main menu - is the real thing either way.

namespace {
	using namespace bpr;
	namespace native = bpr::tests::native_menu;

	QElapsedTimer course_clock;
	/// True when AppKit itself makes windows key; false when the course delivers
	/// the key-window transitions because the session refused activation.
	bool native_key_windows = true;

	void note(const std::string& aLine)
	{
		std::cout << "native-menu-bar: " << course_clock.elapsed() << "ms " << aLine << '\n';
		std::cout.flush();
	}
	void print_trace()
	{
		for (const auto& entry : native::main_menu_trace()) {
			note("trace " + entry);
		}
	}
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			note(std::string{ "main menu now: " } + native::describe_main_menu());
			print_trace();
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void pump()
	{
		QApplication::processEvents();
		QThread::msleep(1);
	}
	auto wait_quietly(const std::function<bool()>& aCondition, const int aMilliseconds) -> bool
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aMilliseconds) {
			pump();
		}
		return aCondition();
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const int aMilliseconds = 15000)
	{
		expect(wait_quietly(aCondition, aMilliseconds), aMessage);
	}
	void settle(const int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			pump();
		}
	}
	/// What the cocoa plugin delivers when AppKit makes a window key or the key
	/// window resigns (qcocoawindow.mm windowDidBecomeKey / windowDidResignKey).
	void deliver_key_window(QWindow* aWindow)
	{
		QWindowSystemInterface::handleFocusWindowChanged(aWindow, Qt::ActiveWindowFocusReason);
		QApplication::processEvents();
	}
	/// Gives the window focus the way the platform would; delivers the key-window
	/// change itself when the session will not activate this process.
	void focus(QWidget& aWindow, const char* aMessage)
	{
		if (native_key_windows) {
			wait_for([&] { return QGuiApplication::focusWindow() == aWindow.windowHandle(); }, aMessage);
			return;
		}
		deliver_key_window(aWindow.windowHandle());
		expect(QGuiApplication::focusWindow() == aWindow.windowHandle(), aMessage);
	}
	/// Closes a window the way the platform would: the key window resigns as it
	/// orders out, which AppKit reports before anything else happens to it.
	void close_window(QWidget& aWindow)
	{
		aWindow.close();
		if (!native_key_windows) {
			deliver_key_window(nullptr);
		}
	}
	/// The menu bar RFC-BPR-0033 promises while no window is open: Profiles with New
	/// Window and management, and Quit, which AppKit merges into the application
	/// menu. Read from AppKit, not from Qt.
	auto resident_menu_bar_present() -> bool
	{
		return native::main_menu_offers("Profiles", "New Window")
			&& native::main_menu_offers("Profiles", "Manage Profiles…")
			&& native::main_menu_offers("", "Quit");
	}
	/// Samples `[NSApp mainMenu]` on every event-loop turn until the condition holds,
	/// recording each change and the longest stretch where the Profiles menu was
	/// missing. That stretch is the defect the owner saw; zero is the contract.
	struct gap_report
	{
		qint64 LongestGapMs = 0;
		qint64 TotalGapMs   = 0;
		bool   GapOpen      = false;
	};
	auto sample(const std::function<bool()>& aCondition, const int aMilliseconds) -> gap_report
	{
		gap_report    report;
		QElapsedTimer elapsed;
		QElapsedTimer gap;
		elapsed.start();
		std::string last;
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			const auto description = native::describe_main_menu();
			if (description != last) {
				note("main menu: " + description);
				last = description;
			}
			const auto present = resident_menu_bar_present();
			if (!present && !report.GapOpen) {
				report.GapOpen = true;
				gap.start();
			} else if (present && report.GapOpen) {
				report.GapOpen      = false;
				report.LongestGapMs = std::max(report.LongestGapMs, gap.elapsed());
				report.TotalGapMs += gap.elapsed();
			}
			if (aCondition()) {
				break;
			}
			QThread::msleep(1);
		}
		if (report.GapOpen) {
			report.LongestGapMs = std::max(report.LongestGapMs, gap.elapsed());
			report.TotalGapMs += gap.elapsed();
		}
		return report;
	}
	auto sample_until(const std::function<bool()>& aCondition, const char* aMessage, const int aMilliseconds = 20000) -> gap_report
	{
		const auto report = sample(aCondition, aMilliseconds);
		expect(aCondition(), aMessage);
		return report;
	}
	/// Samples for the whole duration; the settled desktop must hold its menu bar.
	auto sample_for(const int aMilliseconds) -> gap_report
	{
		return sample([] { return false; }, aMilliseconds);
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	course_clock.start();
	QApplication qtApplication{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	expect(QGuiApplication::platformName() == QStringLiteral("cocoa"),
		   "this course observes the macOS menu bar and needs the cocoa platform");
	expect(native::activation_policy_is_regular(), "the process asked AppKit for a Dock tile and a menu bar");
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private desktop test root");
	const auto root = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };

	app::github_application* initial  = nullptr;
	auto                     terminal = false;
	auto                     options  = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = root;
	// The platform's own policy, exactly as the shipped composition selects it.
	expect(app::native_empty_desktop_policy()() == app::empty_desktop_outcome::stay_resident,
		   "macOS stays resident when its last window closes");
	auto desktop = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "desktop initial startup");
	expect(initial != nullptr, "initial runtime admitted");
	native::begin_main_menu_trace();
	initial->show();
	native::activate_application();
	native_key_windows = wait_quietly([&] { return QGuiApplication::focusWindow() == initial->window().windowHandle(); }, 3000);
	if (native_key_windows) {
		note("key windows: AppKit activated this process and made the window key");
	} else {
		note("key windows: the session refused activation (screen locked: " + std::string{ native::session_screen_is_locked() ? "yes" : "no" }
			 + ", frontmost: " + native::frontmost_application_name()
			 + "); the course delivers key-window changes through the QPA entry point the cocoa plugin uses");
	}
	focus(initial->window(), "the window is the focus window");
	wait_for([&] { return native::main_menu_is(initial->window().menu_bar()->toNSMenu()); },
			 "with a window open, the main menu is that window's own menu bar");
	note("windowed main menu: " + native::describe_main_menu());
	expect(native::main_menu_offers("Profiles", "New Window") && native::main_menu_offers("View", "Line numbers"),
		   "and it carries the workbench menus");

	// A menubar-less window of the workbench, such as Settings, must not swap the
	// workbench's menus out from under the reviewer while a workbench window exists.
	initial->window().open_settings(qt::settings_page::general);
	wait_for([&] { return initial->window().settings() != nullptr && initial->window().settings()->isVisible(); }, "Settings opens");
	wait_for([&] { return initial->window().settings()->windowHandle() != nullptr; }, "Settings has a native window");
	focus(*initial->window().settings(), "Settings takes focus");
	settle(200);
	note("main menu with Settings focused: " + native::describe_main_menu());
	expect(native::main_menu_is(initial->window().menu_bar()->toNSMenu()),
		   "focusing the Settings window keeps the workbench menu bar");
	close_window(*initial->window().settings());
	focus(initial->window(), "focus returns to the workbench");

	// B1. Close the last window and watch the menu bar through the whole close.
	native::begin_main_menu_trace();
	close_window(initial->window());
	const auto closing = sample_until([&] { return desktop->runtime(core::default_bpr_profile_id) == nullptr && desktop->desktop_menu() != nullptr; },
									  "the last window closes, its profile retires and the desktop keeps its own menu bar");
	expect(desktop->management_window() == nullptr || !desktop->management_window()->isVisible(),
		   "closing the last window presents no chooser");
	expect(!desktop->is_quiescent(), "and the application is still running with no window");
	const auto settled = sample_for(1000);
	print_trace();
	expect(settled.TotalGapMs == 0, "the settled empty desktop holds its menu bar for a further second");
	note("longest stretch without the resident menu bar during the close: " + std::to_string(closing.LongestGapMs) + "ms"
		 + ", total " + std::to_string(closing.TotalGapMs) + "ms" + (closing.GapOpen ? ", still open" : ""));
	expect(native::visible_window_count() == 0U, "no AppKit window remains on screen");
	expect(native::activation_policy_is_regular(), "the process still owns a Dock tile");
	expect(native::main_menu_is(desktop->desktop_menu()->toNSMenu()),
		   "with no window, the main menu is the desktop's own menu bar");
	expect(resident_menu_bar_present(), "and it offers Profiles, New Window, management and Quit");
	expect(native::main_menu_offers("Profiles", "Default"), "and the profile by name");
	expect(closing.LongestGapMs == 0 && !closing.GapOpen,
		   "at no moment during the close was the menu bar left without its Profiles menu");

	// B2. Each native reopening gesture opens the most recently used profile at Home,
	// exactly once, and a further close returns to the same resident menu bar.
	const auto reopened_exactly_once = [&](const char* aMessage) {
		wait_for([&] { return desktop->runtime(core::default_bpr_profile_id) != nullptr; }, aMessage);
		auto* const runtime = desktop->runtime(core::default_bpr_profile_id);
		QApplication::processEvents();
		expect(desktop->windows(core::default_bpr_profile_id).size() == 1U, "exactly one window, under one runtime owner");
		expect(runtime->window().showing_home(), "the reopened window arrives at Home");
		wait_for([&] { return runtime->window().windowHandle() != nullptr && runtime->window().isVisible(); }, "the reopened window is shown");
		focus(runtime->window(), "the reopened window is the focus window");
		wait_for([&] { return native::main_menu_is(runtime->window().menu_bar()->toNSMenu()); },
				 "and the reopened window's menu bar takes the menu bar back");
		return runtime;
	};
	const auto closed_again = [&](app::github_application& aRuntime) {
		native::begin_main_menu_trace();
		close_window(aRuntime.window());
		const auto report = sample_until([&] { return desktop->runtime(core::default_bpr_profile_id) == nullptr; },
										 "the window closes again and its profile retires");
		settle(100);
		expect(native::main_menu_is(desktop->desktop_menu()->toNSMenu()) && resident_menu_bar_present(),
			   "the resident menu bar returns after the close");
		expect(report.LongestGapMs == 0 && !report.GapOpen, "with no gap");
	};

	expect(native::send_dock_reopen(), "AppKit's Dock reopen message reaches the application delegate");
	auto* const byDock = reopened_exactly_once("the Dock's reopen message reopens the most recently used profile");
	closed_again(*byDock);

	if (native_key_windows) {
		native::hide_application();
		wait_for([&] { return !native::application_is_active(); }, "hiding the application resigns activation");
		native::unhide_application();
		wait_for([&] { return native::application_is_active(); }, "unhiding activates it again");
		auto* const byActivation = reopened_exactly_once("native application activation reopens the most recently used profile");
		closed_again(*byActivation);
	} else {
		note("not run: native application activation (hide and unhide) needs a session that activates this process");
	}

	expect(native::send_command_key('n'), "Command-N is sent through the application with no key window");
	auto* const byShortcut = reopened_exactly_once("Command-N on the resident menu bar reopens the most recently used profile");
	closed_again(*byShortcut);

	expect(native::perform_main_menu_item("Profiles", "Default"), "the Profiles menu offers the profile by name as an enabled item");
	auto* const byMenu = reopened_exactly_once("choosing the profile from the native Profiles menu reopens it");
	closed_again(*byMenu);

	expect(native::perform_main_menu_item("", "Quit"), "Quit is reachable on the resident menu bar");
	wait_for([&] { return desktop->is_quiescent(); }, "the resident desktop quits from its own menu bar");
	desktop.reset();
	note(native_key_windows ? "key windows were native throughout" : "key windows were delivered by the course; the activation gesture was not run");
	std::cout << "profile-empty-desktop-native-menu-bar:pass\n";
	return EXIT_SUCCESS;
}
