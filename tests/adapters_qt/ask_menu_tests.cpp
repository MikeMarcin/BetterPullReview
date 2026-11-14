// RFC-BPR-0128, owner look ask-v001 (BPR-2163-quick-ask-editor, Reject): "shift
// click ... draft the message instead of send it. I don't think right click
// should send the message ... there should be a context menu." And (owner,
// 2026-11-14): "a plain click queues it". An Ask menu row: a plain click asks
// (the window queues it), Shift+click drafts, a right click opens the row's
// own menu and never asks.
#include "bpr/qt/ask_menu.hpp"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void click(QMenu& aMenu, QAction& aAction, const Qt::MouseButton aButton, const Qt::KeyboardModifiers aModifiers)
	{
		const auto at      = QPointF{ aMenu.actionGeometry(&aAction).center() };
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, at, aMenu.mapToGlobal(at), aButton, aButton, aModifiers };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, at, aMenu.mapToGlobal(at), aButton, Qt::NoButton, aModifiers };
		QApplication::sendEvent(&aMenu, &press);
		QApplication::sendEvent(&aMenu, &release);
		QApplication::processEvents();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	auto         asked    = std::vector<std::string>{ };
	auto         drafted  = std::vector<std::string>{ };
	const auto   dispatch = qt::ask_menu_dispatch{
		.Ask = [&asked](const qt::ask_menu_request&, const core::quick_ask_row& aRow) { asked.push_back(aRow.Id); },
		.Draft = [&drafted](const qt::ask_menu_request&, const core::quick_ask_row& aRow) { drafted.push_back(aRow.Id); },
		.Custom = [](const qt::ask_menu_request&) { },
		.Edit   = [] { },
	};
	auto  rows = core::quick_ask_menu{ .Sections = { { .Scope = core::prompt_scope::pull_request, .Rows = { { .Id = "agent-tab.what-could-break", .Label = "What could this change break?" } } } } };
	QMenu menu;
	qt::fill_ask_menu(menu, rows, qt::ask_menu_request{ }, dispatch);
	menu.popup(QPoint{ 100, 100 });
	QApplication::processEvents();
	auto* const row = menu.findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.what-could-break_pull-request"));
	expect(row != nullptr && row->toolTip().contains(QStringLiteral("Shift+click drafts")), "the row says what a click and a Shift+click do");

	click(menu, *row, Qt::RightButton, Qt::NoModifier);
	expect(asked.empty() && drafted.empty(), "a right click never asks or drafts");
	auto* const rowMenu = menu.findChild<QMenu*>(QStringLiteral("askRowMenu"));
	expect(rowMenu != nullptr && rowMenu->isVisible(), "a right click opens the row's own menu");
	auto names = QStringList{ };
	for (auto* const action : rowMenu->actions()) {
		if (!action->isSeparator()) {
			names << action->text();
		}
	}
	// Look ask-v002 (owner, 2026-11-14): "Drop edit and hide from the
	// individual question menu, the edit quick ask on the top level ask
	// dropdown is sufficient."
	expect(names == QStringList({ QStringLiteral("Queue"), QStringLiteral("Draft in Agent Tab") }), "its menu: Queue, Draft in Agent Tab: " + names.join(QLatin1Char('|')).toStdString());
	rowMenu->findChild<QAction*>(QStringLiteral("askRowDraft"))->trigger();
	expect(drafted == std::vector<std::string>{ "agent-tab.what-could-break" } && asked.empty(), "Draft in Agent Tab drafts it");
	rowMenu->findChild<QAction*>(QStringLiteral("askRowQueue"))->trigger();
	expect(asked.size() == 1U, "Queue asks it");

	menu.popup(QPoint{ 100, 100 });
	QApplication::processEvents();
	click(menu, *row, Qt::LeftButton, Qt::ShiftModifier);
	expect(drafted.size() == 2U && asked.size() == 1U, "Shift+click drafts it, sending nothing");
	menu.popup(QPoint{ 100, 100 });
	QApplication::processEvents();
	click(menu, *row, Qt::LeftButton, Qt::NoModifier);
	expect(asked.size() == 2U && drafted.size() == 2U, "a plain click asks (the window queues it)");
	std::cout << "ask-menu-tests:pass\n";
	return EXIT_SUCCESS;
}
