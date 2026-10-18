#include "../support/test_expectation.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "native_main_menu_probe.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QThread>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// [[BPR-1295]]. Hide Drawers on the native macOS menu bar. The View menu's item
// carries Command-B as an AppKit key equivalent, and AppKit offers a key
// equivalent to the menu as well as to the key window. An editable multi-line
// editor claims the key in `ShortcutOverride` so that a note or a reply being
// written in the Conversations drawer is not hidden mid-sentence; this course
// proves that claim holds on the cocoa platform, where Qt asks the focus object
// before the menu takes the key, and that the same key from a list does hide
// the drawers. The offscreen window course cannot see the native menu; this
// one reads it from AppKit and sends the key through `-[NSApplication
// sendEvent:]`. It needs the owner's Aqua session.

namespace {
	using namespace bpr;
	namespace native = bpr::tests::native_menu;

	void note(const std::string& aLine)
	{
		std::cout << "hide-drawers-native-menu: " << aLine << '\n';
		std::cout.flush();
	}
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			note(std::string{ "main menu now: " } + native::describe_main_menu());
			bpr::tests::fail(aMessage);
		}
	}
	auto wait_quietly(const std::function<bool()>& aCondition, const int aMilliseconds) -> bool
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		return aCondition();
	}
	void settle(const int aMilliseconds)
	{
		static_cast<void>(wait_quietly([] { return false; }, aMilliseconds));
	}
} // namespace

int main(int argc, char** argv)
{
	QApplication application{ argc, argv };
	expect(QGuiApplication::platformName() == QStringLiteral("cocoa"), "the course runs on the cocoa platform");

	// The window's gestures are transitions of a real core state, published
	// back as the host publishes them.
	auto  state   = core::workbench_interaction_state{ };
	auto  toggles = 0;
	auto* shown   = static_cast<qt::workbench_window*>(nullptr);
	auto  publish = [&state, &shown] {
		auto projection          = application::workbench_projection{ };
		projection.Drawers       = core::workbench_drawer_delta{ .Files = state.FilesDrawer, .Conversations = state.ConversationsDrawer, .Repositories = state.RepositoriesDrawer };
		projection.DrawerRestore = core::project_workbench_drawer_restore(state);
		projection.Commands      = core::project_workbench_commands(state);
		shown->publish(projection);
	};
	qt::workbench_window window{
		[&](core::workbench_interaction_intent aIntent) {
			if (std::holds_alternative<core::toggle_workbench_drawers_intent>(aIntent)) {
				++toggles;
			}
			static_cast<void>(core::transition(state, aIntent));
			publish();
		}
	};
	shown = &window;
	window.resize(1'280, 820);
	window.show();
	window.raise();
	window.activateWindow();
	auto activationRefused = false;
	if (!wait_quietly([&] { return QGuiApplication::focusWindow() == window.windowHandle(); }, 5000)) {
		activationRefused = true;
		// A session that refuses activation (a locked screen, another
		// application in front) gets the key-window change the cocoa plugin
		// would deliver; Qt's menu bar arbitration and AppKit's menu are real
		// either way.
		note("activation refused; the key-window change is delivered through the QPA entry point");
		QWindowSystemInterface::handleFocusWindowChanged(window.windowHandle(), Qt::ActiveWindowFocusReason);
		QApplication::processEvents();
	}
	expect(QGuiApplication::focusWindow() == window.windowHandle(), "the window is the focus window");
	// [[BPR-1304]]. A session that refused activation (the screen locked, the
	// login window frontmost) gives the process no key window, so there is no
	// window for a keystroke to be addressed to. AppKit then resolves the key
	// equivalent against the main menu alone, which is the path the menu item's
	// key takes, and the one [[BPR-0796]]'s course sends Command-N through.
	// Before this the course failed at the send whenever the screen was locked.
	const auto sendCommandB = [activationRefused] {
		if (native::send_command_key_to_key_window('b')) {
			return true;
		}
		if (!activationRefused) {
			return false;
		}
		note("no key window in a session that refused activation; Command-B resolves against the main menu");
		return native::send_command_key('b');
	};

	const auto file  = core::file_identity_of("src/file.cpp");
	state.Navigation = core::workbench_navigation_cursor{ .File = file, .Ordinal = 1U, .ActionableCount = 1U };
	auto first       = application::workbench_projection{ };
	first.Selection  = core::workbench_selection_delta{ .Cursor = state.Navigation };
	first.Files      = std::vector<application::workbench_file_projection>{ { .File = file, .Path = "src/file.cpp" } };
	window.publish(first);
	publish();
	window.choose_modality(qt::review_modality::review, core::workbench_modality_entry::choose);
	expect(wait_quietly([] { return native::main_menu_offers("View", "Hide Drawers"); }, 5000),
		   "AppKit's View menu offers Hide Drawers");
	note("View menu: " + [] {
		std::string line;
		for (const auto& item : native::main_menu_items("View")) {
			line += item + "; ";
		}
		return line;
	}());
	const auto open = [&window] {
		return window.files_drawer()->state() == core::workbench_drawer_state::expanded
			&& window.conversations_drawer()->state() == core::workbench_drawer_state::expanded;
	};
	expect(open(), "Review's preset opens Files and Comments");

	// A note being written in the Conversations drawer keeps Command-B.
	window.ai_tools_action()->setChecked(true);
	window.findChild<QTabWidget*>(QStringLiteral("privateConversationTabs"))->setCurrentIndex(1);
	window.private_notes()->present({ .Scope = 1U, .Available = true });
	window.private_notes()->new_review_action()->trigger();
	auto* const body = window.private_notes()->body_editor();
	body->setFocus(Qt::OtherFocusReason);
	body->setPlainText(QStringLiteral("half a sentence"));
	expect(wait_quietly([body] { return QApplication::focusWidget() == body; }, 2000) && !body->isReadOnly(),
		   "an editable note has the keyboard");
	expect(!native::main_menu_item_is_enabled("View", "Hide Drawers"),
		   "while the note has the keyboard AppKit's item is disabled, so its key equivalent cannot fire");
	expect(sendCommandB(), "Command-B is sent through AppKit");
	settle(400);
	note("note focused: toggles=" + std::to_string(toggles) + " open=" + std::to_string(open() ? 1 : 0));
	expect(toggles == 0 && open() && body->hasFocus() && body->toPlainText() == QStringLiteral("half a sentence"),
		   "with a note focused, Command-B on the native menu bar hides nothing and types nothing");

	// The same key from a list goes to the command, by AppKit's menu or Qt's
	// shortcut, whichever takes it first.
	window.ai_tools_action()->setChecked(false);
	window.files_tree()->setFocus(Qt::OtherFocusReason);
	expect(wait_quietly([&window] { return QApplication::focusWidget() == window.files_tree(); }, 2000),
		   "the files list has the keyboard");
	expect(sendCommandB(), "Command-B is sent through AppKit again");
	expect(wait_quietly([&] { return toggles == 1 && !open(); }, 2000),
		   "with the files list focused, Command-B hides the drawers");
	expect(native::main_menu_item_is_enabled("View", "Hide Drawers"), "the native item stays enabled in Review");
	expect(sendCommandB(), "Command-B is sent a third time");
	expect(wait_quietly([&] { return toggles == 2 && open(); }, 2000), "and again shows them");
	note("list focused: toggles=" + std::to_string(toggles));
	window.close();
	std::cout << "hide-drawers-native-menu:pass\n";
	return EXIT_SUCCESS;
}
