/// \file
/// \brief RFC-BPR-0130, header A. The History menu the Agent tab and the
/// tour's side chat share: New session first, the sessions one row each,
/// Delete all sessions…, what each chat adds, and last the revision read.
/// Owner, attended review 2026-11-24: "New session either needs to always
/// work or get greyed out if it won't. Right now you click it and it does
/// nothing which feels broken." New session is enabled exactly when core
/// offers it; refused, it is greyed, says why under its name and sends
/// nothing. Saves renders when `BPR_CHAT_HISTORY_RENDER_DIR` names a folder.
#include "bpr/qt/chat_history_menu.hpp"

#include <QAction>
#include <QApplication>
#include <QEnterEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
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

	auto history(const std::size_t aOpenTurns) -> application::native_agent_history_projection
	{
		auto view             = application::native_agent_history_projection{ };
		view.Selected         = "b";
		auto open             = application::native_agent_history_entry{ };
		open.Id               = "b";
		open.Title            = aOpenTurns == 0U ? "First look" : "Parser review";
		open.Name             = open.Title;
		open.Turns            = aOpenTurns;
		open.Binding.HeadOid  = *core::parse_git_commit_oid("2222222222222222222222222222222222222222");
		auto older            = application::native_agent_history_entry{ };
		older.Id              = "a";
		older.Title           = "What changed?";
		older.Turns           = 1U;
		older.Binding.HeadOid = open.Binding.HeadOid;
		view.Entries          = { open, older };
		return view;
	}

	auto names(const QMenu& aMenu) -> QStringList
	{
		auto found = QStringList{ };
		for (const auto* const action : aMenu.actions()) {
			found << (action->isSeparator() ? QStringLiteral("-") : action->objectName().isEmpty() ? action->text()
																								   : action->objectName());
		}
		return found;
	}

	void render(QMenu& aMenu, const char* const aName)
	{
		if (const auto folder = qEnvironmentVariable("BPR_CHAT_HISTORY_RENDER_DIR"); !folder.isEmpty()) {
			settle();
			expect(aMenu.grab().save(folder + QLatin1Char{ '/' } + QString::fromLatin1(aName) + QStringLiteral(".png")), "render saved");
		}
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	auto         owner = QWidget{ };
	owner.resize(600, 500);
	owner.show();

	// The Agent tab's menu.
	auto  button      = QToolButton{ &owner };
	auto* agent       = new qt::chat_history_menu{ core::chat_history_kind::agent_tab, owner, button, QStringLiteral("agent") };
	auto  started     = 0;
	auto  opened      = std::vector<std::string>{ };
	auto  carried     = std::vector<std::string>{ };
	auto  edits       = std::vector<std::pair<std::string, std::optional<std::string>>>{ };
	auto  everyRun    = 0;
	auto  resets      = 0;
	auto  shows       = 0;
	auto  agentCopies = 0;
	agent->set_dispatch({
		.AboutToShow      = [&shows] { ++shows; },
		.NewSession       = [&started] { ++started; },
		.Open             = [&opened](const std::string& aId) { opened.push_back(aId); },
		.CarryOver        = [&carried](const std::string& aId) { carried.push_back(aId); },
		.ViewOther        = { },
		.Edit             = [&edits](const std::string& aId, std::optional<std::string> aName) { edits.emplace_back(aId, std::move(aName)); },
		.CopyConversation = [&agentCopies] { ++agentCopies; },
		.ShowEveryRun     = [&everyRun] { ++everyRun; },
		.ResetAgents      = [&resets] { ++resets; },
	});
	auto state        = qt::chat_history_menu::chat_state{ };
	state.ShownTurns  = 3U;
	state.EveryRun    = true;
	state.ResetAgents = true;
	agent->set_state(state);
	agent->present(history(3U));
	auto* const menu = agent->menu();
	expect(button.menu() == menu && button.popupMode() == QToolButton::InstantPopup && menu->objectName() == QStringLiteral("agentHistoryMenu"), "the menu is its button's, opened at once");
	expect(names(*menu)
			   == QStringList{ QStringLiteral("agentNewSession"), QStringLiteral("-"), QStringLiteral("agentSessionRows"), QStringLiteral("-"), QStringLiteral("agentCarryOver"), QStringLiteral("agentSessionDeleteAll"),
							   QStringLiteral("-"), QStringLiteral("agentCopyChat"), QStringLiteral("agentShowEveryRun"), QStringLiteral("-"), QStringLiteral("agentResetAgentsAction"), QStringLiteral("-"),
							   QStringLiteral("agentRevisionRead") },
		   "New session first, the sessions, Delete all sessions…, Copy conversation, Show every run…, Reset agents…, the revision read last: " + names(*menu).join(QLatin1Char{ '|' }).toStdString());
	expect(agent->copy_action()->text() == QStringLiteral("Copy conversation") && agent->copy_action()->isEnabled() && agent->revision_action()->text().startsWith(QStringLiteral("Reads the pull request at ")),
		   "RFC-BPR-0130's names: Copy conversation, and the revision read as History's last line: " + agent->revision_action()->text().toStdString());
	agent->copy_action()->trigger();
	expect(agentCopies == 1, "the Agent tab's whole conversation is copied from History");
	expect(menu->findChild<QMenu*>(QStringLiteral("nativeAgentSessions")) == nullptr, "the sessions are the menu's own rows, not a level down");
	expect(agent->revision_action()->text() == QStringLiteral("Reads the pull request at 2222222") && !agent->revision_action()->isEnabled(), "the last line names the revision read and is never chosen");

	// A session with turns: New session can be chosen, and does it once.
	auto* const fresh = agent->new_session_action();
	expect(fresh->text() == QStringLiteral("New session") && fresh->isEnabled() && agent->new_session_reason().isEmpty(), "New session is offered on a session with turns");
	menu->popup(owner.mapToGlobal(QPoint{ 10, 10 }));
	settle();
	expect(shows == 1, "opening the menu asks for the sessions again");
	auto* row = menu->findChild<QWidget*>(QStringLiteral("agentNewSessionRow"));
	expect(row != nullptr && row->isVisible() && row->isEnabled() && !row->findChild<QLabel*>(QStringLiteral("chatHistoryNewSessionReason"))->isVisibleTo(row), "its row shows no reason while it can be chosen");
	{
		QEnterEvent enter{ QPointF{ 30.0, 5.0 }, QPointF{ 30.0, 5.0 }, row->mapToGlobal(QPointF{ 30.0, 5.0 }) };
		QApplication::sendEvent(row, &enter);
		settle();
		expect(row->property("highlighted").toBool() && menu->activeAction() == fresh, "New session is lit under the pointer, as the items below it are");
		render(*menu, "history-agent");
		QMouseEvent release{ QEvent::MouseButtonRelease, QPointF{ 30.0, 5.0 }, row->mapToGlobal(QPointF{ 30.0, 5.0 }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(row, &release);
		settle();
	}
	expect(started == 1 && !menu->isVisible(), "a click starts a new session and the menu closes");

	// The session nothing was asked in: greyed, with why, and it sends nothing.
	state.ShownTurns = 0U;
	agent->set_state(state);
	agent->present(history(0U));
	expect(!fresh->isEnabled() && agent->new_session_reason() == QStringLiteral("This session is new: nothing has been asked in it."), "New session is greyed on the session nothing was asked in, and says why: " + agent->new_session_reason().toStdString());
	fresh->trigger();
	expect(started == 1, "greyed, it starts nothing");
	menu->popup(owner.mapToGlobal(QPoint{ 10, 10 }));
	settle();
	row = menu->findChild<QWidget*>(QStringLiteral("agentNewSessionRow"));
	{
		auto* const why = row->findChild<QLabel*>(QStringLiteral("chatHistoryNewSessionReason"));
		expect(!row->isEnabled() && why->isVisibleTo(row) && why->text() == agent->new_session_reason() && row->accessibleDescription() == why->text(), "the reason is under its name, in the menu, and is its accessible description");
		QEnterEvent enter{ QPointF{ 30.0, 5.0 }, QPointF{ 30.0, 5.0 }, row->mapToGlobal(QPointF{ 30.0, 5.0 }) };
		QApplication::sendEvent(row, &enter);
		QMouseEvent release{ QEvent::MouseButtonRelease, QPointF{ 30.0, 5.0 }, row->mapToGlobal(QPointF{ 30.0, 5.0 }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(row, &release);
		settle();
		expect(started == 1 && !row->property("highlighted").toBool(), "a click on it does nothing, and it is not lit as something that would");
		render(*menu, "history-agent-new-session-refused");
	}
	menu->close();

	// The same session pinned to another revision (the tab shows its card):
	// New session opens one at the revision under review, so it is offered.
	state.Pinned = true;
	agent->set_state(state);
	expect(fresh->isEnabled() && agent->new_session_reason().isEmpty(), "New session is offered on a session pinned to another revision, though nothing was asked in it");
	fresh->trigger();
	expect(started == 2, "and it starts one");
	state.Pinned = false;
	agent->set_state(state);
	expect(!fresh->isEnabled(), "at the revision under review it is the new session again");

	// A running turn, the sessions being read, and no agent.
	state.ShownTurns = 3U;
	state.Running    = true;
	agent->set_state(state);
	agent->present(history(3U));
	expect(!fresh->isEnabled() && agent->new_session_reason() == QStringLiteral("Stop the running turn first.") && !agent->carry_over_action()->isEnabled() && !agent->rows()->delete_all_action()->isEnabled(),
		   "a running turn holds New session, carry over and Delete all back");
	state.Running = false;
	agent->set_state(state);
	expect(fresh->isEnabled() && agent->carry_over_action()->isEnabled(), "and they come back when it ends");
	auto loading    = application::native_agent_history_projection{ };
	loading.Loading = true;
	agent->present(loading);
	expect(agent->loading() && !fresh->isEnabled() && agent->new_session_reason().isEmpty() && agent->rows()->ids().size() == 2U, "while the sessions are read New session waits, the rows stay, and no reason flashes");
	agent->present(history(3U));
	expect(!agent->loading() && fresh->isEnabled(), "read, it is offered again");
	state.AgentReady = false;
	agent->set_state(state);
	expect(!fresh->isEnabled() && agent->new_session_reason() == QStringLiteral("Set up an agent first."), "no agent: greyed, with that reason");
	state.AgentReady = true;
	agent->set_state(state);

	// RFC-BPR-0130's accepted words: no "Transcript replay", no "Private
	// history and runs", no kibibytes; the chosen agent by its own name.
	{
		const auto said = [menu] {
			auto words = QStringList{ };
			for (const auto* const action : menu->actions()) {
				words << action->text() << action->toolTip();
			}
			return words.join(QLatin1Char{ '|' });
		};
		expect(agent->carry_over_action()->text() == QStringLiteral("Carry this conversation over to the chosen agent")
				   && agent->carry_over_action()->toolTip() == QStringLiteral("Long conversations are shortened: the oldest turns are left out."),
			   "with no agent named, carrying over names none: " + agent->carry_over_action()->text().toStdString());
		state.Agent = QStringLiteral("Codex");
		agent->set_state(state);
		expect(agent->carry_over_action()->text() == QStringLiteral("Carry this conversation over to Codex"), "carrying over names the chosen agent");
		state.Agent = QStringLiteral("Claude Code");
		agent->set_state(state);
		expect(agent->carry_over_action()->text() == QStringLiteral("Carry this conversation over to Claude Code"), "and another agent chosen, that one");
		expect(said().contains(QStringLiteral("Show every run…")), "the runs are shown by Show every run…: " + said().toStdString());
		for (const auto* const old : { "Transcript replay", "Native", "Private history", "KiB", "Replay " }) {
			expect(!said().contains(QString::fromLatin1(old)), std::string{ "no item of the menu says " } + old + ": " + said().toStdString());
		}
		using menu_words = qt::chat_history_menu;
		expect(menu_words::carried_over_words(QStringLiteral("Codex")) == QStringLiteral("Carried over from a saved transcript. Codex has the questions and answers, not what the earlier agent read.")
				   && menu_words::carried_over_words({ }) == QStringLiteral("Carried over from a saved transcript. The agent has the questions and answers, not what the earlier agent read."),
			   "a carried-over session's notice, with the agent's name or none");
		expect(menu_words::cannot_continue_words(QStringLiteral("Codex")) == QStringLiteral("Codex can't pick up this conversation where it stopped. Choose New session, or carry the transcript over."),
			   "the notice of a session the agent cannot go on with names New session as History does");
		expect(menu_words::carry_over_tip(0U, false) == QStringLiteral("Long conversations are shortened: the oldest turns are left out.")
				   && menu_words::carry_over_tip(1U, false).endsWith(QStringLiteral(" This one left out 1 older turn."))
				   && menu_words::carry_over_tip(7U, true).endsWith(QStringLiteral(" This one left out 7 older turns. Some of its longest text was cut.")),
			   "what carrying over leaves out is said in turns, and only when something was left out");
		auto shortened               = history(3U);
		shortened.Replay             = true;
		shortened.ReplayTurnsOmitted = 4U;
		agent->present(shortened);
		expect(agent->carry_over_action()->toolTip().endsWith(QStringLiteral(" This one left out 4 older turns.")), "a carried-over session that left turns out says how many: " + agent->carry_over_action()->toolTip().toStdString());
		agent->present(history(3U));
	}

	// The other items reach their dispatch.
	agent->carry_over_action()->trigger();
	expect(carried == std::vector<std::string>{ "b" }, "carry over names the open session");
	agent->rows()->open("a");
	agent->rows()->rename("a", QStringLiteral("Second look"));
	expect(opened == std::vector<std::string>{ "a" } && edits.size() == 1U && edits.back().first == "a" && edits.back().second == std::string{ "Second look" }, "a row's open and rename reach the chat");
	for (auto* const action : menu->actions()) {
		if (action->objectName() == QStringLiteral("agentShowEveryRun") || action->objectName() == QStringLiteral("agentResetAgentsAction")) {
			action->trigger();
		}
	}
	expect(everyRun == 1 && resets == 1, "Show every run… and Reset agents… reach the chat");
	state.ResetAgents = false;
	agent->set_state(state);
	expect(!names(*menu).contains(QStringLiteral("agentResetAgentsAction")) && names(*menu).contains(QStringLiteral("agentCopyChat")), "no setup to reset: no Reset agents…; Copy conversation stays");

	// The tour's side chat: the same menu, with the other parts' chats and
	// Copy conversation, and without the Agent tab's own items.
	auto  tourButton = QToolButton{ &owner };
	auto* tour       = new qt::chat_history_menu{ core::chat_history_kind::tour_side_chat, owner, tourButton, QStringLiteral("tourChat") };
	auto  copies     = 0;
	auto  viewed     = std::vector<std::string>{ };
	tour->set_dispatch({
		.AboutToShow      = { },
		.NewSession       = { },
		.Open             = { },
		.CarryOver        = { },
		.ViewOther        = [&viewed](const std::string& aId) { viewed.push_back(aId); },
		.Edit             = { },
		.CopyConversation = [&copies] { ++copies; },
		.ShowEveryRun     = { },
		.ResetAgents      = { },
	});
	auto partHistory       = history(2U);
	auto other             = application::native_agent_history_entry{ };
	other.Id               = "other";
	other.Scope            = "tour:1:0:3:7";
	other.Title            = "Why this stop?";
	partHistory.OtherChats = { other };
	auto tourState         = qt::chat_history_menu::chat_state{ };
	tourState.ShownTurns   = 2U;
	tour->set_state(tourState);
	tour->present(partHistory);
	expect(names(*tour->menu())
			   == QStringList{ QStringLiteral("tourChatNewSession"), QStringLiteral("-"), QStringLiteral("agentSessionRows"), QStringLiteral("-"), QStringLiteral("tourChatCarryOver"), QStringLiteral("agentSessionDeleteAll"),
							   QStringLiteral("-"), QStringLiteral("Chats about other parts of the tour"), QStringLiteral("tourChatCopyChat"), QStringLiteral("-"), QStringLiteral("tourChatRevisionRead") },
		   "the tour chat's History is the same menu with its own two items: " + names(*tour->menu()).join(QLatin1Char{ '|' }).toStdString());
	auto* const others = tour->menu()->findChild<QMenu*>(QStringLiteral("tourChatOtherChats"));
	expect(others != nullptr && others->actions().size() == 1 && others->actions().front()->text() == QStringLiteral("Stop ID 7: Why this stop?"), "another part's chat is named by its part");
	others->actions().front()->trigger();
	tour->copy_action()->trigger();
	expect(viewed == std::vector<std::string>{ "other" } && copies == 1 && tour->copy_action()->isEnabled(), "its chat is read, and the conversation copied");
	tourState.ShownTurns = 0U;
	tourState.Busy       = true;
	tour->set_state(tourState);
	expect(!tour->copy_action()->isEnabled() && !tour->new_session_action()->isEnabled() && tour->new_session_reason() == QStringLiteral("The agent is at work on another part. Wait for it to finish."),
		   "nothing to copy in an empty chat; the agent at work elsewhere holds New session back, with that reason");
	tourState.Busy   = false;
	tourState.Closed = true;
	tour->set_state(tourState);
	expect(tour->new_session_reason() == QStringLiteral("This part is no longer in the tour."), "a chat whose part is gone says so");
	tour->menu()->popup(owner.mapToGlobal(QPoint{ 10, 10 }));
	render(*tour->menu(), "history-tour");
	tour->menu()->close();

	std::cout << "chat-history-menu-tests:pass\n";
	return EXIT_SUCCESS;
}
