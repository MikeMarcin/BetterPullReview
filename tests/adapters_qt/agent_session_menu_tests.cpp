/// \file
/// \brief [[BPR-NNNN]], owner 2026-11-24 (agent panel review). The History
/// menu's session rows: each session's title (open it), a pencil (rename in
/// place; empty restores the default), a trash can (delete after a question
/// that names what stays) and an i (turns, revision and cost on hover); Delete
/// all sessions… below; delete waits while a turn runs. Saves a render when
/// `BPR_AGENT_SESSION_RENDER_DIR` names a folder.
#include "bpr/qt/agent_session_menu.hpp"
#include "bpr/qt/destructive_question.hpp"

#include <QAction>
#include <QApplication>
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
	expect(firstTitle->text() == QStringLiteral("Parser review") && firstTitle->font().bold(), "a row shows its title; the open session is marked");
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

	// A click on a title opens it and closes the menu.
	{
		auto* const title = shown[1]->findChild<QLabel*>(QStringLiteral("agentSessionTitle"));
		QMouseEvent release{ QEvent::MouseButtonRelease, QPointF{ 4.0, 4.0 }, title->mapToGlobal(QPointF{ 4.0, 4.0 }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(title, &release);
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
