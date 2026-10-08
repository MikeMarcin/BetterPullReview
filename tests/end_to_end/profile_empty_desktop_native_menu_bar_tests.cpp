#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/settings_window.hpp"
#include "native_main_menu_probe.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSystemTrayIcon>
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
#include <string_view>
#include <vector>

// RFC-BPR-0033 B1 and B2, the native half. The offscreen management course proves
// the composition keeps a parentless `desktop_menu_bar`; the owner's attended
// review of the shipped build rejected the empty desktop with "Closing last window
// closes menu bar." (BPR-0782, `bpr-0779-empty-desktop-dock`). This course runs the
// real desktop composition on the cocoa platform and asserts what `[NSApp mainMenu]`
// carries at every event-loop turn from the last close to the settled empty desktop,
// then reopens through the Dock's reopen message, native activation, Command-N and
// the Profiles menu as AppKit delivers them.
//
// The bar was never lost and the gap was always 0 ms; what the close took was most
// of the bar's titles. So the contract here is a comparison, not a presence check:
// the windowed bar is snapshotted, and the windowless bar must equal it - the same
// visible top-level menus, the same items in the application menu AppKit owns, the
// same items under View - with the window's own presentation commands disabled.
// [[BPR-0796]] grew the desktop's bar to meet it.
//
// Qt installs a menu bar from `QGuiApplication::focusWindow()`, which the cocoa
// plugin sets when AppKit makes a window key (qcocoawindow.mm, windowDidBecomeKey
// and windowDidResignKey). Only the active application has a key window, and a
// locked screen lets nothing become active. When this process cannot be activated
// the course delivers those two key-window transitions itself through the same
// QPA entry point the plugin uses, says so, and leaves the activation gesture
// recorded as not run; everything downstream of that delivery - Qt's cocoa menu
// bar arbitration and AppKit's main menu - is the real thing either way.
//
// `[NSApp mainMenu]` is not the bar on the screen. It is this process's property
// and it reads the same whoever is active, while the bar drawn across the top
// belongs to the active application alone. The owner rejected the fixed build a
// second time with "When I close the last window it stays running in the bottom
// dock but not in the top menu bar." - a sentence about activation that every
// assertion here was blind to, because a windowless application that is not
// active is in the Dock and nowhere else with its main menu perfectly intact. So
// the close is now also required to leave this application active, whenever the
// session let it be active in the first place.

namespace {
	using namespace bpr;
	namespace native = bpr::tests::native_menu;

	QElapsedTimer course_clock;
	/// True when AppKit itself makes windows key; false when the course delivers
	/// the key-window transitions because the session refused activation.
	bool native_key_windows = true;
	/// Closes that were watched while this process was the active application,
	/// and how many of those left it active. A close that resigned activation
	/// would put the application in the Dock and nowhere else, which is the
	/// rejection [[BPR-0796]] answers; a close that lost it because somebody
	/// clicked another application says nothing, and on a console in use that
	/// happens. So the contract is over every close the course watches: at least
	/// one of them, observed natively, kept the application active.
	int closes_watched_while_active = 0;
	int closes_that_kept_activation = 0;

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
			bpr::tests::fail(aMessage);
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
	///
	/// A console somebody is using is not a locked one and is not a quiet one
	/// either: a person clicking another application takes activation away
	/// mid-course, and a process that is no longer active gets no key window no
	/// matter how long it waits. That is the session's doing, not the
	/// application's, so the course downgrades to delivering the key-window
	/// changes itself - exactly as it does behind a locked screen - and says so.
	/// Still active and still not key stays a failure.
	void focus(QWidget& aWindow, const char* aMessage)
	{
		if (native_key_windows) {
			if (wait_quietly([&] { return QGuiApplication::focusWindow() == aWindow.windowHandle(); }, 5000)) {
				return;
			}
			expect(!native::application_is_active(), aMessage);
			note("activation went to " + native::frontmost_application_name()
				 + " while the course was running; key-window changes are delivered through the QPA entry point from here on");
			native_key_windows = false;
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
	/// Window and management, View, and Quit, which AppKit merges into the
	/// application menu. Read from AppKit, not from Qt. Every bar this course sees -
	/// a window's and the desktop's - satisfies it, so it is also the predicate for
	/// "no menu was lost for a moment" across a close or a reopening.
	auto resident_menu_bar_present() -> bool
	{
		return native::main_menu_offers("Profiles", "New Window")
			&& native::main_menu_offers("Profiles", "Manage Profiles…")
			&& native::main_menu_offers("View", "Line numbers")
			&& native::main_menu_offers("", "Quit");
	}
	auto join(const std::vector<std::string>& aValues) -> std::string
	{
		std::string line;
		for (const auto& value : aValues) {
			if (!line.empty()) {
				line += ", ";
			}
			line += value;
		}
		return line;
	}
	/// The bar a reviewer actually reads: the titles across the top, the items of
	/// the application menu AppKit owns, and the items of View. RFC-BPR-0033 B1
	/// promises all of it survives the close - "the menu bar remains (File,
	/// Profiles, View and the rest), the Dock icon remains" - so the windowed bar
	/// is the specification for the windowless one, and the close must subtract
	/// nothing from it.
	struct menu_bar_snapshot
	{
		std::vector<std::string> Titles;
		std::vector<std::string> ApplicationItems;
		std::vector<std::string> ViewItems;
		std::string              Description;
	};
	/// \brief The description with each "unavailable — reason" title reduced
	/// to its command.
	///
	/// \details [[BPR-1250]]. A window names why Comment is unavailable in the
	/// item's own title (fa194e6bc), and the reason is the window's state: a
	/// window on Home says to select a line. The bar is the same bar when the
	/// same commands are offered, whatever each says about its current state.
	auto commands_only(std::string aDescription) -> std::string
	{
		constexpr auto marker = std::string_view{ " unavailable — " };
		for (auto found = aDescription.find(marker); found != std::string::npos; found = aDescription.find(marker, found)) {
			const auto end = aDescription.find_first_of(";]", found);
			aDescription.erase(found, end == std::string::npos ? std::string::npos : end - found);
		}
		return aDescription;
	}
	auto snapshot_menu_bar() -> menu_bar_snapshot
	{
		return { native::main_menu_visible_titles(), native::main_menu_items("Apple"),
				 native::main_menu_items("View"), commands_only(native::describe_main_menu()) };
	}
	void check_update_menu()
	{
		wait_for([] { return native::main_menu_item_is_enabled("Apple", "Check for Updates…"); }, "the focused bar enables its native update item");
		const auto items = native::main_menu_items("Apple");
		expect(std::ranges::count(items, std::string{ "Check for Updates…" }) == 1,
			   "exactly one Check for Updates item is visible in the native application menu");
		expect(native::perform_main_menu_item("Apple", "Check for Updates…"), "native update action is enabled");
		QWidget* offer = nullptr;
		wait_for([&] {
			for (auto* widget : QApplication::topLevelWidgets()) {
				if (widget->windowTitle() == QStringLiteral("BetterPullReview Updates") && widget->isVisible()) {
					offer              = widget;
					const auto* status = offer->findChild<QLabel*>(QStringLiteral("releaseUpdateStatus"));
					return status && !status->text().isEmpty();
				}
			}
			return false;
		},
				 "native update gesture reaches the public status panel");
		close_window(*offer);
	}
	/// Samples `[NSApp mainMenu]` on every event-loop turn until the condition holds,
	/// recording each change and the longest stretch where the Profiles menu was
	/// missing. That stretch is the defect the owner saw; zero is the contract.
	struct gap_report
	{
		qint64 LongestGapMs = 0;
		qint64 TotalGapMs   = 0;
		bool   GapOpen      = false;
		/// Whether this process was the active application on the first turn.
		/// Only the active application's menu bar is the one on screen, so a
		/// sample taken while some other application is active says nothing
		/// about what a reviewer would have seen.
		bool BeganActive = false;
		/// False once the process was observed to stop being the active
		/// application while the sample was running.
		bool StayedActive = true;
		/// Whoever was frontmost at the moment activation was lost.
		std::string TookTheFront;
	};
	auto sample(const std::function<bool()>& aCondition, const int aMilliseconds) -> gap_report
	{
		gap_report    report;
		QElapsedTimer elapsed;
		QElapsedTimer gap;
		elapsed.start();
		std::string last;
		auto        firstTurn = true;
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			const auto active = native::application_is_active();
			if (firstTurn) {
				report.BeganActive = active;
				firstTurn          = false;
			}
			if (report.BeganActive && !active && report.StayedActive) {
				report.StayedActive = false;
				report.TookTheFront = native::frontmost_application_name();
			}
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
try {
	course_clock.start();
	QApplication qtApplication{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	expect(QGuiApplication::platformName() == QStringLiteral("cocoa"),
		   "this course observes the macOS menu bar and needs the cocoa platform");
	expect(native::activation_policy_is_regular(), "the process asked AppKit for a Dock tile and a menu bar");
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private desktop test root");
	const auto root = app::native_filesystem_path(QFileInfo{ temporary.path() }.canonicalFilePath());

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
	// [[BPR-1250]]. Dif is in the menu bar from startup, as a template image the
	// bar tints, in one status item the desktop owns. The profile's notifications
	// go through that item; a tray of the profile's own would leave the menu bar
	// with its last window, which is what the owner saw in help-v001.
	auto* const statusItem = desktop->status_item();
	expect(statusItem != nullptr && statusItem->isVisible(), "the desktop shows its status item from startup");
	expect(statusItem->icon().isMask(), "and the item's Dif is a template image");
	expect(app::detail::github_application_fixture_access::notification_tray(*initial) == statusItem,
		   "the profile's notifications go through the desktop's item, not a tray of their own");
	wait_for([] { return native::visible_status_item_count() == 1U; }, "AppKit has exactly one status item for this process");
	const auto statusItemPresent = [&] {
		return desktop->status_item() == statusItem && statusItem->isVisible() && native::visible_status_item_count() == 1U;
	};
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
	const auto windowed = snapshot_menu_bar();
	// [[BPR-1305]]. Home hides Hide Drawers in the window's native menu;
	// the resident bar offers every window command, disabled. Bind its exact
	// order to the declared View menu, including commands hidden on Home.
	auto expectedResident = windowed;
	expectedResident.ViewItems.clear();
	for (auto* action : initial->window().view_menu()->actions()) {
		if (!action->isSeparator() && action->menuRole() != QAction::PreferencesRole) {
			expectedResident.ViewItems.push_back(action->text().remove(QLatin1Char{ '&' }).toStdString());
		}
	}
	const auto viewBegin = expectedResident.Description.find("|View[");
	const auto viewEnd   = expectedResident.Description.find(']', viewBegin);
	expect(viewBegin != std::string::npos && viewEnd != std::string::npos, "the native description contains View");
	auto viewCommands = std::string{ };
	for (const auto& command : expectedResident.ViewItems) {
		if (!viewCommands.empty()) {
			viewCommands += ';';
		}
		viewCommands += command;
	}
	expectedResident.Description.replace(viewBegin + 6U, viewEnd - viewBegin - 6U, viewCommands);
	check_update_menu();
	initial->window().activateWindow();
	focus(initial->window(), "focus returns after the update dialog");
	expect(native::perform_main_menu_item("Profiles", "New Window"), "native New Window creates an updater sibling");
	wait_for([&] { return initial->windows().size() == 2U; }, "two native windows are open");
	const auto contexts = initial->windows();
	auto*      sibling  = initial->window(contexts.back());
	wait_for([&] { return sibling->isVisible() && sibling->windowHandle(); }, "the sibling is visible");
	focus(*sibling, "the sibling receives focus");
	check_update_menu();
	initial->window().activateWindow();
	focus(initial->window(), "the original window receives focus again");
	check_update_menu();
	close_window(*sibling);
	wait_for([&] { return initial->windows().size() == 1U; }, "the sibling closes without losing the update action");
	initial->window().activateWindow();
	focus(initial->window(), "the original remains usable");
	check_update_menu();
	initial->window().activateWindow();
	focus(initial->window(), "focus returns for the last-window course");
	note("windowed top-level menus: " + join(windowed.Titles));
	expect(std::ranges::find(windowed.Titles, std::string{ "Profiles" }) != windowed.Titles.end()
			   && std::ranges::find(windowed.Titles, std::string{ "View" }) != windowed.Titles.end(),
		   "the windowed bar carries the menus RFC-BPR-0033 names, so it is the thing the close must preserve");

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
	check_update_menu();
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
	expect(statusItemPresent(), "closing the last window leaves Dif's status item in the menu bar");
	expect(native::activation_policy_is_regular(), "the process still owns a Dock tile");
	expect(native::main_menu_is(desktop->desktop_menu()->toNSMenu()),
		   "with no window, the main menu is the desktop's own menu bar");
	expect(resident_menu_bar_present(), "and it offers Profiles, New Window, management and Quit");
	expect(native::main_menu_offers("Profiles", "Default"), "and the profile by name");
	// The owner's rejection, "Closing last window closes menu bar.", is this
	// comparison. The bar is never lost, but until [[BPR-0796]] the close took
	// View away and the merged Preferences with it, leaving one visible menu
	// where the RFC names four.
	const auto resident = snapshot_menu_bar();
	check_update_menu();
	note("windowless top-level menus: " + join(resident.Titles));
	expect(resident.Titles == windowed.Titles, "closing the last window takes no menu off the bar");
	expect(resident.ApplicationItems == windowed.ApplicationItems,
		   "and no item out of the application menu, the merged Preferences item included");
	expect(resident.ViewItems == expectedResident.ViewItems, "every declared View command is offered in order with no window open");
	expect(!native::main_menu_item_is_enabled("View", "Line numbers")
			   && !native::main_menu_item_is_enabled("View", "One pane")
			   && !native::main_menu_item_is_enabled("View", "Hide Drawers")
			   && !native::main_menu_item_is_enabled("View", "Threads")
			   && !native::main_menu_item_is_enabled("View", "Updates Line")
			   && !native::main_menu_item_is_enabled("View", "Next Tab")
			   && !native::main_menu_item_is_enabled("View", "Previous Tab")
			   && !native::main_menu_item_is_enabled("File", "Close Tab")
			   && !native::main_menu_item_is_enabled("Pull Request", "Compose review")
			   && !native::main_menu_item_is_enabled("Pull Request", "Comment"),
		   "offered and not actionable, because no window's presentation is theirs to change");
	expect(resident.Description == expectedResident.Description,
		   "the windowless bar retains the full workbench command set");
	expect(closing.LongestGapMs == 0 && !closing.GapOpen,
		   "at no moment during the close was the menu bar left without its Profiles menu");

	// The other half of the owner's sentence, and the half `[NSApp mainMenu]`
	// cannot answer. That menu is this process's property and it reads the same
	// whoever is active; the bar drawn across the top of the screen belongs to
	// the active application alone. An application that resigned activation as
	// its last window closed would be "running in the Dock but not in the menu
	// bar" with its main menu still perfectly intact, which is what
	// [[BPR-0796]]'s rejection of 2026-09-16 described. So the close is required
	// to leave this application active, not merely to leave its menu alone.
	if (closing.BeganActive) {
		++closes_watched_while_active;
		if (closing.StayedActive && native::application_is_active()) {
			++closes_that_kept_activation;
			note(std::string{ "the close kept the application active; the resident bar above is the bar a person sees. Through the settled second afterwards: " }
				 + (settled.StayedActive ? "still active" : "activation went to " + settled.TookTheFront));
		} else {
			note("this close did not keep activation: it went to " + (closing.StayedActive ? native::frontmost_application_name() : closing.TookTheFront)
				 + ". On a console in use that is a person clicking elsewhere, so it is the later closes that have to settle it");
		}
	} else {
		note("not run: this process was not the active application across the close (" + native::frontmost_application_name()
			 + " holds the front), so whether the close resigns activation was not observed here");
	}

	// B2. Each native reopening gesture opens the most recently used profile at Home,
	// exactly once, and a further close returns to the same resident menu bar.
	const auto reopenedExactlyOnce = [&](const char* aMessage) {
		const auto  reopening = sample_until([&] { return desktop->runtime(core::default_bpr_profile_id) != nullptr; }, aMessage);
		auto* const runtime   = desktop->runtime(core::default_bpr_profile_id);
		QApplication::processEvents();
		expect(desktop->windows(core::default_bpr_profile_id).size() == 1U, "exactly one window, under one runtime owner");
		expect(runtime->window().showing_home(), "the reopened window arrives at Home");
		wait_for([&] { return runtime->window().windowHandle() != nullptr && runtime->window().isVisible(); }, "the reopened window is shown");
		focus(runtime->window(), "the reopened window is the focus window");
		// The desktop retires its own bar when a window becomes active, so this is the
		// handover; a reviewer must not see the bar lose a menu while it happens.
		const auto handover = sample_until([&] { return native::main_menu_is(runtime->window().menu_bar()->toNSMenu()); },
										   "and the reopened window's menu bar takes the menu bar back");
		check_update_menu();
		runtime->window().activateWindow();
		focus(runtime->window(), "focus returns after the reopened update gesture");
		expect(reopening.LongestGapMs == 0 && !reopening.GapOpen && handover.LongestGapMs == 0 && !handover.GapOpen,
			   "with no moment in either direction where the bar was short a menu");
		expect(statusItemPresent() && app::detail::github_application_fixture_access::notification_tray(*runtime) == statusItem,
			   "the reopened profile submits through the same one status item");
		return runtime;
	};
	const auto closedAgain = [&](app::github_application& aRuntime) {
		native::begin_main_menu_trace();
		close_window(aRuntime.window());
		const auto report = sample_until([&] { return desktop->runtime(core::default_bpr_profile_id) == nullptr; },
										 "the window closes again and its profile retires");
		settle(100);
		expect(native::main_menu_is(desktop->desktop_menu()->toNSMenu()) && resident_menu_bar_present(),
			   "the resident menu bar returns after the close");
		expect(report.LongestGapMs == 0 && !report.GapOpen, "with no gap");
		expect(statusItemPresent(), "and Dif's status item is still in the menu bar");
		if (report.BeganActive) {
			++closes_watched_while_active;
			if (report.StayedActive && native::application_is_active()) {
				++closes_that_kept_activation;
			}
		}
	};

	expect(native::send_dock_reopen(), "AppKit's Dock reopen message reaches the application delegate");
	auto* const byDock = reopenedExactlyOnce("the Dock's reopen message reopens the most recently used profile");

	// The arrangement the pin before the close could not reach: there, the desktop
	// had never built its bar. Qt gives a parentless menu bar to any focused window
	// that carries none, so a desktop bar still alive now would take the menu bar
	// from the workbench the moment Settings was focused - and offer that window's
	// own commands disabled while the window is right behind it.
	expect(desktop->desktop_menu() == nullptr, "the desktop's own menu bar is retired once a window is active again");
	byDock->window().open_settings(qt::settings_page::general);
	wait_for([&] { return byDock->window().settings() != nullptr && byDock->window().settings()->isVisible(); },
			 "Settings opens over the reopened window");
	wait_for([&] { return byDock->window().settings()->windowHandle() != nullptr; }, "Settings has a native window");
	focus(*byDock->window().settings(), "Settings takes focus while the resident bar exists");
	settle(200);
	note("main menu with Settings focused and a resident desktop bar alive: " + native::describe_main_menu());
	expect(native::main_menu_is(byDock->window().menu_bar()->toNSMenu()),
		   "a menubar-less Settings window keeps the workbench window's own menu bar");
	check_update_menu();
	close_window(*byDock->window().settings());
	focus(byDock->window(), "focus returns to the workbench");

	closedAgain(*byDock);

	if (native_key_windows) {
		native::hide_application();
		wait_for([&] { return !native::application_is_active(); }, "hiding the application resigns activation");
		native::unhide_application();
		wait_for([&] { return !native::application_is_hidden(); }, "unhiding puts the application back on the desktop");
		// `unhide:` restores an application's windows; it does not take
		// activation, and an application with no window has none to restore.
		// Measured on a plain AppKit application on this machine, four runs out
		// of four: `isActive` stays NO after `unhide:`, and stays NO after a
		// following `activateIgnoringOtherApps:`. This process is sometimes
		// granted that activation and sometimes refused it, because current
		// macOS reserves activation for the person - the Dock tile,
		// Command-Tab, Show All. So this asks and records the answer instead of
		// asserting something that is not the application's to decide. Until
		// [[BPR-0796]] the course asserted `unhide:` alone would activate, which
		// is untrue of AppKit; it had never run, because every earlier session
		// was screen-locked.
		native::activate_application();
		if (wait_quietly([&] { return native::application_is_active(); }, 3000)) {
			auto* const byActivation = reopenedExactlyOnce("native application activation reopens the most recently used profile");
			closedAgain(*byActivation);
		} else {
			note("not run: macOS refused this process activation after unhiding (" + native::frontmost_application_name()
				 + " holds the front), so what activation reopens stays the owner's to see; the course goes on delivering "
				   "key-window changes through the QPA entry point the cocoa plugin uses");
			native_key_windows = false;
		}
	} else {
		note("not run: native application activation (hide and unhide) needs a session that activates this process");
	}

	expect(native::send_command_key('n'), "Command-N is sent through the application with no key window");
	auto* const byShortcut = reopenedExactlyOnce("Command-N on the resident menu bar reopens the most recently used profile");
	closedAgain(*byShortcut);

	expect(native::perform_main_menu_item("Profiles", "Default"), "the Profiles menu offers the profile by name as an enabled item");
	auto* const byMenu = reopenedExactlyOnce("choosing the profile from the native Profiles menu reopens it");
	closedAgain(*byMenu);

	// The half of the owner's sentence that `[NSApp mainMenu]` is blind to, settled
	// over every close this course watched rather than over one of them: an
	// application that resigns activation as its last window closes is in the Dock
	// and nowhere else, however intact its main menu is.
	if (closes_watched_while_active > 0) {
		expect(closes_that_kept_activation > 0,
			   "at least one close, watched while this application was active, left it active - otherwise closing the last "
			   "window is what puts this application in the Dock and out of the menu bar");
		note("closes watched while active: " + std::to_string(closes_watched_while_active) + ", of which kept activation: "
			 + std::to_string(closes_that_kept_activation));
	} else {
		note("not run: no close was watched while this process was the active application, so whether a close resigns "
			 "activation is unobserved here and stays the owner's to see");
	}

	expect(native::perform_main_menu_item("", "Quit"), "Quit is reachable on the resident menu bar");
	wait_for([&] { return desktop->is_quiescent(); }, "the resident desktop quits from its own menu bar");
	desktop.reset();
	note(native_key_windows ? "key windows were native throughout" : "key windows were delivered by the course; the activation gesture was not run");
	std::cout << "profile-empty-desktop-native-menu-bar:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
