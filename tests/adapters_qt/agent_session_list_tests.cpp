/// \file
/// \brief [[BPR-NNNN]]. The Agent tab's session list: each session's title,
/// the open one marked; editing a row renames it, an emptied name restores the
/// default; Delete… asks first and names what it leaves alone; both wait while
/// a turn runs. Saves a render when `BPR_AGENT_SESSION_RENDER_DIR` names a folder.
#include "bpr/qt/agent_session_list.hpp"
#include "bpr/qt/destructive_question.hpp"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

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
		auto first    = application::native_agent_history_entry{ .Id = "b", .Title = "Parser review", .Name = "Parser review", .Turns = 3U };
		auto second   = application::native_agent_history_entry{ .Id = "a", .Title = "What changed?", .Turns = 1U };
		view.Entries  = { first, second };
		return view;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	auto         owner = QWidget{ };
	owner.resize(600, 500);
	owner.show();
	auto  sent = std::vector<std::pair<std::string, std::optional<std::string>>>{ };
	auto* list = new qt::agent_session_list{ &owner };
	list->set_dispatch([&sent](const std::string& aId, std::optional<std::string> aName) { sent.emplace_back(aId, std::move(aName)); });
	list->present(history());
	list->show();
	settle();

	auto* const rows = list->list();
	expect(rows->count() == 2 && rows->item(0)->text() == QStringLiteral("Parser review") && rows->item(1)->text() == QStringLiteral("What changed?"), "each session shows its title, newest first");
	expect(rows->item(0)->font().bold() && !rows->item(1)->font().bold() && rows->item(0)->toolTip().startsWith(QStringLiteral("The open session")), "the open session is marked");
	expect(rows->item(1)->toolTip().contains(QStringLiteral("1 turn")), "a row says how many turns it holds");
	expect(rows->currentRow() == 0 && list->rename_button()->isEnabled() && list->delete_button()->isEnabled(), "a session is chosen and can be renamed or deleted");

	// Renaming in place: the typed words go to the dispatch; empty restores.
	rows->item(1)->setText(QStringLiteral("Second look"));
	expect(sent.size() == 1U && sent.back() == std::pair<std::string, std::optional<std::string>>{ "a", std::string{ "Second look" } }, "editing a row renames that session");
	rows->item(0)->setText(QString{ });
	expect(sent.size() == 2U && sent.back().first == "b" && sent.back().second == std::string{ }, "an emptied name asks for the default title");
	list->present(history());
	rows->item(0)->setText(QStringLiteral("Parser review"));
	expect(sent.size() == 2U, "an unchanged title sends nothing");

	// Delete asks first, says what it leaves alone, and sends only once confirmed.
	rows->setCurrentRow(1);
	list->delete_button()->click();
	settle();
	auto* question = list->question();
	expect(question != nullptr && question->isVisible(), "Delete… asks first");
	const auto words = question->findChildren<QLabel*>();
	auto       said  = QString{ };
	for (auto* const label : words) {
		said += label->text() + QLatin1Char{ '\n' };
	}
	expect(said.contains(QStringLiteral("Delete “What changed?”?")) && said.contains(QStringLiteral("Claude Code’s or Codex’s, are left alone")) && said.contains(QStringLiteral("unless another session")),
		   "the question names the session, what goes, and what stays");
	question->reject();
	settle();
	expect(sent.size() == 2U, "cancelling deletes nothing");
	list->ask_delete();
	settle();
	question = list->question();
	question->findChild<QPushButton*>(QStringLiteral("deleteAgentSessionConfirm"))->click();
	settle();
	expect(sent.size() == 3U && sent.back() == std::pair<std::string, std::optional<std::string>>{ "a", std::nullopt }, "confirming deletes that session");

	// A running turn: both wait.
	list->set_running(true);
	expect(!list->rename_button()->isEnabled() && !list->delete_button()->isEnabled() && list->delete_button()->toolTip().contains(QStringLiteral("Stop the running turn")),
		   "while a turn runs, rename and delete wait and say why");
	list->ask_delete();
	settle();
	expect(list->question() == nullptr || !list->question()->isVisible(), "no question is asked while a turn runs");
	list->set_running(false);
	list->present_status(QStringLiteral("Deleted, with its agent workspace."));
	expect(list->status_label()->isVisibleTo(list) && list->status_label()->text().startsWith(QStringLiteral("Deleted")), "the outcome is said in the list");

	if (const auto folder = qEnvironmentVariable("BPR_AGENT_SESSION_RENDER_DIR"); !folder.isEmpty()) {
		static_cast<void>(list->grab().save(folder + QStringLiteral("/agent-session-list.png")));
	}
	std::cout << "agent-session-list-tests:pass\n";
	return EXIT_SUCCESS;
}
