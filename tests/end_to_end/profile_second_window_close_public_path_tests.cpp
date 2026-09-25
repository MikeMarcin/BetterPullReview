/// \file
/// \brief Closing a New Window window while the first window stays open.
///
/// \details [[BPR-1027]]. `open_window` keeps the first window's session in
/// `Implementation` as well as in `Sessions`, so closing the first window
/// erases one of two references and destroys nothing then. A window opened
/// later is held only by `Sessions`, so its close runs the whole session
/// destructor from the `OnClosed` erase. The multi-window courses close the
/// first window and quit the rest, so none of them ran that destructor from
/// the erase. This course does, five times over, through the real composition
/// and the window's own close.
///
/// On the Windows desktop the same close ended the owner with a Control Flow
/// Guard fast fail in that destructor. Three options reproduce the desktop
/// probe's shape more closely, so the difference can be narrowed down:
/// `--shipped-composition` composes the shipped graph, not a fixture;
/// `--through-strip-menu` (Windows) opens each window through the strip's
/// Application menu with accessible invokes, as the probe did; and
/// `--walk-accessible` turns accessibility on and walks every window's
/// accessible tree before each close, as a UI Automation client's descendant
/// search does. Offscreen, every combination passes.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QMenu>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>

namespace {
	using namespace bpr;

	using bpr::tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const char* const aMessage, const int aTimeoutMs = 30'000)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	void run_interval(const int aMilliseconds)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	auto action(qt::workbench_window& aWindow, const char* const aName) -> QAction*
	{
		auto* const result = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(result != nullptr, "the window has the profile action");
		wait_for([result] { return result->isEnabled(); }, "the profile action becomes enabled");
		return result;
	}
	/// Walks every accessible interface under aInterface, as a UI Automation
	/// client's descendant search does, so each one is created and cached.
	auto walk_accessible(QAccessibleInterface* const aInterface, const int aDepth) -> std::size_t
	{
		if (aInterface == nullptr || aDepth > 64) {
			return 0U;
		}
		auto visited = std::size_t{ 1U };
		static_cast<void>(aInterface->text(QAccessible::Name));
		static_cast<void>(aInterface->rect());
		static_cast<void>(aInterface->state());
		for (auto index = 0; index < aInterface->childCount(); ++index) {
			visited += walk_accessible(aInterface->child(index), aDepth + 1);
		}
		return visited;
	}

	auto walk_every_window() -> std::size_t
	{
		auto visited = std::size_t{ 0U };
		for (auto* const widget : QApplication::topLevelWidgets()) {
			if (widget->isVisible()) {
				visited += walk_accessible(QAccessible::queryAccessibleInterface(widget), 0);
			}
		}
		return visited;
	}

#if defined(_WIN32)
	/// The accessible child of aMenu that presents aAction.
	auto accessible_item(QMenu& aMenu, QAction* const aAction) -> QAccessibleInterface*
	{
		auto* const menu  = QAccessible::queryAccessibleInterface(&aMenu);
		const auto  index = aMenu.actions().indexOf(aAction);
		expect(menu != nullptr && index >= 0 && index < menu->childCount(), "the menu presents the action as an accessible item");
		auto* const item = menu->child(static_cast<int>(index));
		expect(item != nullptr && item->actionInterface() != nullptr, "the accessible menu item offers its actions");
		return item;
	}

	/// Chooses aAction through the Windows strip's application menu the way the
	/// desktop probe did: the menu control is pressed, the action's menu is
	/// shown through its accessible item, and the action's accessible press
	/// triggers it with both menus open. The menus are closed afterwards, as the
	/// probe's Escape closed them.
	void choose_through_strip_menu(qt::workbench_window& aWindow, QAction* const aAction)
	{
		auto* const owning = qobject_cast<QMenu*>(aAction->parent());
		expect(owning != nullptr, "the action is in one of the window's menus");
		auto context  = QObject{ };
		auto ran      = false;
		auto attempts = 0;
		auto poll     = std::function<void()>{ };
		poll          = [&] {
			// [[BPR-1121]]. The event loop calls this, and a throw may not cross it, so a failure here exits.
			const bpr::tests::exit_on_failure inTheEventLoop;
			if (auto* const popup = qobject_cast<QMenu*>(QApplication::activePopupWidget()); popup != nullptr) {
				ran = true;
				accessible_item(*popup, owning->menuAction())->actionInterface()->doAction(QAccessibleActionInterface::showMenuAction());
				accessible_item(*owning, aAction)->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
				QTimer::singleShot(500, &context, [] {
					while (auto* const open = QApplication::activePopupWidget()) {
						open->close();
					}
				});
			} else if (++attempts < 250) {
				QTimer::singleShot(20, &context, poll);
			}
		};
		QTimer::singleShot(0, &context, poll);
		aWindow.strip_menu_button()->click();
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while ((!ran || QApplication::activePopupWidget() != nullptr) && attempts < 250 && elapsed.elapsed() < 10'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		expect(ran && QApplication::activePopupWidget() == nullptr, "the strip menu chose the action and closed");
	}
#endif
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "a private test root");
	// The data root one owned level down, as [[BPR-0875]]'s courses have it:
	// the agent workspace neighbourhood beside the root refuses a parent that
	// another principal can write, and a refusal there leaves the agent host,
	// with its realigner, out of the composition this course is about.
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "the test root and its parent are owner-private under the storage policy");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = root;

	app::github_application* owner      = nullptr;
	auto                     terminal   = false;
	const auto               completion = [&](app::profile_desktop_startup_result aResult) {
		terminal = true;
		if (aResult) {
			owner = *aResult;
		} else {
			std::cerr << "startup failure: " << static_cast<int>(aResult.error()) << '\n';
		}
	};
	auto desktop = QCoreApplication::arguments().contains(QStringLiteral("--shipped-composition"))
		? app::profile_desktop::start(app::github_application_config{ .ApplicationDataRoot = root }, completion)
		: app::detail::github_application_fixture_access::start_desktop(
			  app::detail::github_public_path_fixture::profile_window_restoration_v1, options, completion);
	wait_for([&] { return terminal; }, "the profile owner starts");
	expect(owner != nullptr, "the profile owner is admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "the first window is admitted");
	const auto first  = owner->windows().front();
	auto*      window = owner->window(first);
	expect(window != nullptr && window->isVisible(), "the first window shows");

	constexpr auto rounds           = 5;
	const auto     throughStripMenu = QCoreApplication::arguments().contains(QStringLiteral("--through-strip-menu"));
	const auto     walkAccessible   = QCoreApplication::arguments().contains(QStringLiteral("--walk-accessible"));
	if (walkAccessible) {
		QAccessible::setActive(true);
	}
	for (auto round = 0; round < rounds; ++round) {
		if (throughStripMenu) {
#if defined(_WIN32)
			choose_through_strip_menu(*window, action(*window, "newBprWindowAction"));
#else
			expect(false, "only the Windows strip has the menu control");
#endif
		} else {
			action(*window, "newBprWindowAction")->trigger();
		}
		wait_for([&] { return owner->windows().size() == 2U; }, "New Window opens a second window");
		const auto contexts = owner->windows();
		const auto second   = contexts.front() == first ? contexts.back() : contexts.front();
		auto*      sibling  = owner->window(second);
		expect(second != first && sibling != nullptr && sibling->isVisible(), "the second window shows");
		run_interval(250);
		if (walkAccessible) {
			std::cout << "profile-second-window-close:accessible:" << walk_every_window() << '\n';
		}
		sibling->close();
		wait_for([&] { return owner->window(second) == nullptr && owner->windows().size() == 1U; }, "closing the second window retires only its session");
		wait_for([&] { return !owner->status(second).has_value(); }, "the closed window's session finishes its shutdown");
		expect(owner->window(first) == window && window->isVisible(), "the first window stays open");
		std::cout << "profile-second-window-close:round:" << round + 1 << '\n';
	}

	action(*window, "quitProfilesAction")->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "Quit drains the last session and the profile's services");
	desktop.reset();
	std::cout << "profile-second-window-close:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
