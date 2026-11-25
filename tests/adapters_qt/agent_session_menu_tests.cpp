/// \file
/// \brief [[BPR-2377]], owner 2026-11-24 (agent panel review). The History
/// menu's session rows: each session's title (open it), a pencil (rename in
/// place; empty restores the default), a trash can (delete after a question
/// that names what stays) and an i (turns, revision and cost on hover); Delete
/// all sessions… below; delete waits while a turn runs. A row is lit under
/// the pointer and under the arrow keys, as the menu's own items are (owner,
/// attended review 2026-11-24: "there no highlight on the mouse over for the
/// session rows"), and the open session is the ticked one. Saves renders when
/// `BPR_AGENT_SESSION_RENDER_DIR` names a folder.
#include "bpr/qt/agent_session_menu.hpp"
#include "bpr/qt/destructive_question.hpp"

#include <QAction>
#include <QApplication>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 6; ++round) {
			QApplication::processEvents();
		}
	}

	auto history() -> application::native_agent_history_projection
	{
		auto view     = application::native_agent_history_projection{ };
		view.Selected = "b";
		auto first    = application::native_agent_history_entry{ };
		first.Id      = "b";
		first.Title   = "Parser review";
		first.Name    = "Parser review";
		first.Turns   = 3U;
		first.Cost    = core::agent_cost{ 40000000U };
		auto second   = application::native_agent_history_entry{ };
		second.Id     = "a";
		second.Title  = "What changed?";
		second.Turns  = 1U;
		view.Entries  = { first, second };
		return view;
	}

	/// The rows of `aRows`, in order.
	auto rows_of(QWidget& aRows) -> std::vector<QWidget*>
	{
		auto found = std::vector<QWidget*>{ };
		for (auto* const row : aRows.findChildren<QWidget*>(QStringLiteral("agentSessionRow"))) {
			found.push_back(row);
		}
		return found;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	auto         owner = QWidget{ };
	owner.resize(600, 500);
	owner.show();

	auto  opened  = std::vector<std::string>{ };
	auto  renamed = std::vector<std::pair<std::string, std::string>>{ };
	auto  deleted = std::vector<std::string>{ };
	auto  all     = 0;
	auto* menu    = new QMenu{ &owner };
	auto* rows    = new qt::agent_session_menu{ owner, &owner };
	rows->set_dispatch({
		.Open      = [&opened](const std::string& aId) { opened.push_back(aId); },
		.Rename    = [&renamed](const std::string& aId, std::string aName) { renamed.emplace_back(aId, std::move(aName)); },
		.Delete    = [&deleted](const std::string& aId) { deleted.push_back(aId); },
		.DeleteAll = [&all] { ++all; },
	});
	rows->present(history());
	menu->addAction(rows);
	menu->addAction(rows->delete_all_action());
	menu->popup(owner.mapToGlobal(QPoint{ 10, 10 }));
	settle();

	auto* const list = menu->findChild<QWidget*>(QStringLiteral("agentSessionList"));
	expect(list != nullptr && list->isVisible(), "the rows are an item of the menu");
	auto shown = rows_of(*list);
	expect(shown.size() == 2U, "one row per session");
	auto* const firstTitle = shown[0]->findChild<QLabel*>(QStringLiteral("agentSessionTitle"));
	expect(firstTitle->text() == QStringLiteral("Parser review") && shown[0]->property("sessionOpen").toBool() && !shown[1]->property("sessionOpen").toBool(), "a row shows its title; the open session is the ticked one");
	expect(firstTitle->mapTo(list, QPoint{ 0, 0 }).x() > 12 && shown[0]->width() == list->width() && shown[0]->x() == 0, "a row spans the menu, its title where an item's words start");
	for (auto* const row : shown) {
		expect(row->findChild<QToolButton*>(QStringLiteral("agentSessionRename")) != nullptr && row->findChild<QToolButton*>(QStringLiteral("agentSessionDelete")) != nullptr
				   && row->findChild<QToolButton*>(QStringLiteral("agentSessionInfo")) != nullptr,
			   "every row has a pencil, a trash can and an i");
	}
	const auto info = shown[0]->findChild<QToolButton*>(QStringLiteral("agentSessionInfo"))->toolTip();
	expect(info.startsWith(QStringLiteral("The open session.")) && info.contains(QStringLiteral("3 turns")) && info.contains(QStringLiteral("$0.04 so far")), "the i says turns and cost: " + info.toStdString());
	expect(shown[1]->findChild<QToolButton*>(QStringLiteral("agentSessionInfo"))->toolTip().startsWith(QStringLiteral("1 turn,")), "and one turn reads as one");
	if (const auto folder = qEnvironmentVariable("BPR_AGENT_SESSION_RENDER_DIR"); !folder.isEmpty()) {
		static_cast<void>(menu->grab().save(folder + QStringLiteral("/agent-session-menu.png")));
	}

	// A row is lit under the pointer, one at a time, and is then the menu's
	// active item, so no other item stays lit beside it.
	const auto lit = [](const QWidget* const aRow) { return aRow->property("highlighted").toBool(); };
	const auto key = [](const int aKey) {
		QKeyEvent press{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QApplication::sendEvent(QApplication::focusWidget() != nullptr ? QApplication::focusWidget() : QApplication::activePopupWidget(), &press);
		settle();
	};
	expect(!lit(shown[0]) && !lit(shown[1]), "no row is lit until the pointer or the keys reach it");
	{
		QEnterEvent enter{ QPointF{ 30.0, 5.0 }, QPointF{ 30.0, 5.0 }, shown[1]->mapToGlobal(QPointF{ 30.0, 5.0 }) };
		QApplication::sendEvent(shown[1], &enter);
		settle();
		expect(lit(shown[1]) && !lit(shown[0]) && menu->activeAction() == rows, "the row under the pointer is lit, and is the menu's active item");
		if (const auto folder = qEnvironmentVariable("BPR_AGENT_SESSION_RENDER_DIR"); !folder.isEmpty()) {
			static_cast<void>(menu->grab().save(folder + QStringLiteral("/agent-session-menu-hover.png")));
		}
		QEvent leave{ QEvent::Leave };
		QApplication::sendEvent(shown[1], &leave);
		settle();
		expect(!lit(shown[1]), "and is not once the pointer leaves it");
	}
	if (qEnvironmentVariable("BPR_NATIVE_LOOK") == QStringLiteral("1")) {
		// A native-look run keeps its windows off screen, where no window
		// holds the keyboard: it is for the renders above.
		menu->close();
		std::cout << "agent-session-menu-tests:renders\n";
		return EXIT_SUCCESS;
	}
	// The arrow keys walk the rows as they walk the menu's items: Down from
	// the top reaches the first row, Down again the next, Down again leaves
	// the rows for Delete all sessions…, and Up comes back to the last row.
	menu->setActiveAction(nullptr);
	key(Qt::Key_Down);
	expect(lit(shown[0]) && !lit(shown[1]) && QApplication::focusWidget() == shown[0], "Down reaches the first row");
	key(Qt::Key_Down);
	expect(lit(shown[1]) && !lit(shown[0]), "Down again the next");
	key(Qt::Key_Right);
	expect(QApplication::focusWidget() == shown[1]->findChild<QToolButton*>(QStringLiteral("agentSessionRename")) && lit(shown[1]), "Right moves to the row's pencil; the row stays lit");
	key(Qt::Key_Left);
	expect(QApplication::focusWidget() == shown[1], "Left comes back to the row");
	key(Qt::Key_Down);
	expect(!lit(shown[0]) && !lit(shown[1]) && menu->activeAction() == rows->delete_all_action(), "Down past the last row is the menu's next item");
	key(Qt::Key_Up);
	expect(lit(shown[1]) && QApplication::focusWidget() == shown[1], "Up comes back to the last row");
	key(Qt::Key_Return);
	expect(opened == std::vector<std::string>{ "a" } && !menu->isVisible(), "Return opens the lit row's session and the menu closes");
	opened.clear();
	menu->popup(owner.mapToGlobal(QPoint{ 10, 10 }));
	settle();
	shown = rows_of(*menu->findChild<QWidget*>(QStringLiteral("agentSessionList")));

	// Rename in place: the pencil opens the field, Return keeps the name.
	shown[1]->findChild<QToolButton*>(QStringLiteral("agentSessionRename"))->click();
	auto* field = shown[1]->findChild<QLineEdit*>(QStringLiteral("agentSessionRenameField"));
	expect(field->isVisible() && field->text() == QStringLiteral("What changed?"), "the pencil opens the name in place");
	field->setText(QStringLiteral("Second look"));
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(field, &enter);
	expect(renamed.size() == 1U && renamed.back() == std::pair<std::string, std::string>{ "a", "Second look" }, "Return renames that session");
	rows->rename("b", QString{ });
	expect(renamed.size() == 2U && renamed.back() == std::pair<std::string, std::string>{ "b", "" }, "an emptied name asks for the default title");
	rows->rename("a", QStringLiteral("What changed?"));
	expect(renamed.size() == 2U, "an unchanged title sends nothing");

	// A click anywhere on a row outside its marks opens it and closes the menu.
	{
		shown = rows_of(*menu->findChild<QWidget*>(QStringLiteral("agentSessionList")));
		QMouseEvent release{ QEvent::MouseButtonRelease, QPointF{ 30.0, 4.0 }, shown[1]->mapToGlobal(QPointF{ 30.0, 4.0 }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(shown[1], &release);
	}
	settle();
	expect(opened == std::vector<std::string>{ "a" } && !menu->isVisible(), "a title opens its session and the menu closes");

	// Delete asks first, names what stays, and sends only once confirmed.
	rows->ask_delete("a");
	settle();
	auto* question = rows->question();
	expect(question != nullptr && question->isVisible(), "the trash can asks first");
	auto said = QString{ };
	for (auto* const label : question->findChildren<QLabel*>()) {
		said += label->text() + QLatin1Char{ '\n' };
	}
	expect(said.contains(QStringLiteral("Delete “What changed?”?")) && said.contains(QStringLiteral("Claude Code’s or Codex’s, are left alone")) && said.contains(QStringLiteral("unless another session")),
		   "the question names the session, what goes, and what stays");
	question->reject();
	settle();
	expect(deleted.empty(), "cancelling deletes nothing");
	rows->ask_delete("a");
	settle();
	rows->question()->findChild<QPushButton*>(QStringLiteral("deleteAgentSessionConfirm"))->click();
	settle();
	expect(deleted == std::vector<std::string>{ "a" }, "confirming deletes that session");

	// Delete all sessions…, asked once.
	expect(rows->delete_all_action()->isEnabled() && rows->delete_all_action()->text() == QStringLiteral("Delete all sessions…"), "Delete all sessions… is offered");
	rows->delete_all_action()->trigger();
	settle();
	expect(rows->question() != nullptr && rows->question()->windowTitle().contains(QStringLiteral("Delete all 2 sessions?")), "Delete all asks, counting them");
	rows->question()->findChild<QPushButton*>(QStringLiteral("deleteAllAgentSessionsConfirm"))->click();
	settle();
	expect(all == 1, "confirming deletes them all");

	// While a turn runs: delete and Delete all wait; rename does not.
	rows->set_running(true);
	menu->popup(owner.mapToGlobal(QPoint{ 10, 10 }));
	settle();
	auto* const running = menu->findChild<QWidget*>(QStringLiteral("agentSessionList"));
	shown               = rows_of(*running);
	expect(!shown[0]->findChild<QToolButton*>(QStringLiteral("agentSessionDelete"))->isEnabled() && shown[0]->findChild<QToolButton*>(QStringLiteral("agentSessionRename"))->isEnabled()
			   && !rows->delete_all_action()->isEnabled(),
		   "while a turn runs, delete waits and rename does not");
	rows->ask_delete("a");
	settle();
	expect(rows->question() == nullptr || !rows->question()->isVisible(), "no question is asked while a turn runs");
	rows->set_running(false);
	rows->present_status(QStringLiteral("Deleted, with its agent workspace."));
	settle();
	auto* const status = menu->findChild<QLabel*>(QStringLiteral("agentSessionStatus"));
	expect(status != nullptr && status->text().startsWith(QStringLiteral("Deleted")), "the outcome is said under the rows");
	menu->close();
	std::cout << "agent-session-menu-tests:pass\n";
	return EXIT_SUCCESS;
}
