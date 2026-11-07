// [[BPR-1914]] v005. The owner on the v004 build (macOS): "There's something
// eating click on the side chat tabs. I cant reliably select them with a
// single click." One press and one release on a tab that is not current makes
// it current, with the tab bar selecting on the press (as offscreen Fusion
// does) and on the release (as macOS's own style does), and whether the
// keyboard was in the composer or not.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QTabBar>
#include <QWindow>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void settle()
	{
		for (int round = 0; round < 6; ++round) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
		}
	}
	/// A tab bar that selects on the release of the button, as macOS's style
	/// asks (`SH_TabBar_SelectMouseType`).
	class select_on_release final : public QProxyStyle
	{
	public:
		using QProxyStyle::QProxyStyle;
		int styleHint(StyleHint aHint, const QStyleOption* aOption, const QWidget* aWidget, QStyleHintReturn* aReturn) const override
		{
			if (aHint == QStyle::SH_TabBar_SelectMouseType) {
				return QEvent::MouseButtonRelease;
			}
			return QProxyStyle::styleHint(aHint, aOption, aWidget, aReturn);
		}
	};

	/// One click as a person makes it, through the window as the platform
	/// delivers it (the window gives the click focus and routes the press):
	/// press, the event loop turning, release, then long enough for every
	/// timer a click sets off (the Tour tab's scroll hold runs 600 ms).
	void click(QTabBar* aBar, const int aTab, const bool aLeftEdge = false)
	{
		auto* const window = aBar->window()->windowHandle();
		const auto  rect   = aBar->tabRect(aTab);
		const auto  local  = aLeftEdge ? QPoint{ rect.left() + 8, rect.center().y() } : rect.center();
		const auto  inside = QPointF{ aBar->mapTo(aBar->window(), local) };
		const auto  global = QPointF{ aBar->mapToGlobal(local) };
		auto        press  = QMouseEvent{ QEvent::MouseButtonPress, inside, inside, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		// Nothing lies over the tab: the click is the tab bar's.
		expect(aBar->window()->childAt(inside.toPoint()) == aBar, "the tab bar is what is under tab " + std::to_string(aTab + 1));
		QApplication::sendEvent(window, &press);
		settle();
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, inside, inside, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(window, &release);
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < 700) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		settle();
	}

	void course(const char* aName, const bool aRelease)
	{
		auto draft     = tests::review_tour_fixture();
		draft.Problem  = "Reads crash.";
		draft.Solution = "Each view clears its own target.";
		auto editor    = qt::review_tour_editor{ };
		editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
		editor.set_tour_ask_dispatch([](const qt::tour_ask&) { });
		editor.resize(380, 900);
		editor.show();
		editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
		editor.set_agent_readiness(qt::agent_setup_readiness::ready);
		auto* const chat = editor.side_chat();
		chat->resize(380, 700);
		// The tour's selected stop is Stop 1, as on the owner's screen.
		editor.show_stops();
		editor.go_to_stop(core::tour_stop_id{ 1 });
		settle();
		auto* const bar   = chat->tab_strip()->tab_bar();
		auto        style = std::unique_ptr<select_on_release>{ };
		if (aRelease) {
			style = std::make_unique<select_on_release>();
			bar->setStyle(style.get());
		}
		chat->show();
		chat->activateWindow();
		chat->open(core::tour_thread{ .Kind = core::tour_thread_kind::solution });
		chat->open(core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } });
		chat->open(core::tour_thread{ });
		settle();
		expect(chat->chat_count() == 3U && chat->current_index() == 2, std::string{ aName } + ": three chats, the last open");
		// Each tab that is not current, once, with the keyboard first in the
		// composer, as when the person has just typed.
		for (const auto target : { 0, 1, 2, 0 }) {
			chat->composer()->setFocus(Qt::MouseFocusReason);
			settle();
			click(bar, target);
			expect(chat->current_index() == target && bar->currentIndex() == target, std::string{ aName } + ": one click selects tab " + std::to_string(target + 1) + " (now " + std::to_string(chat->current_index()) + ")");
		}
		// The owner's other try: the tab's left edge.
		for (const auto target : { 0, 1, 0 }) {
			chat->composer()->setFocus(Qt::MouseFocusReason);
			settle();
			click(bar, target, true);
			expect(chat->current_index() == target, std::string{ aName } + ": a click on the left edge of tab " + std::to_string(target + 1) + " selects it");
		}
		// Nothing of the pane lies over its tab row, anywhere along it.
		auto* const strip = chat->tab_strip();
		for (int x = 1; x < strip->width() - 1; x += 6) {
			const auto  at    = strip->mapTo(chat, QPoint{ x, strip->height() / 2 });
			auto* const under = chat->childAt(at);
			expect(under != nullptr && (under == strip || strip->isAncestorOf(under)), std::string{ aName } + ": the tab row is the strip's at x " + std::to_string(x) + " (" + (under == nullptr ? std::string{ "nothing" } : under->objectName().toStdString()) + ")");
		}
		// And with the keyboard elsewhere.
		for (const auto target : { 2, 1 }) {
			bar->setFocus(Qt::OtherFocusReason);
			settle();
			click(bar, target);
			expect(chat->current_index() == target, std::string{ aName } + ": one click selects tab " + std::to_string(target + 1) + " with the keyboard on the tabs");
		}
		chat->hide();
		bar->setStyle(nullptr);
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	course("select on press", false);
	course("select on release", true);
	std::cout << "tour-side-chat-tab-click:pass\n";
	return EXIT_SUCCESS;
}
