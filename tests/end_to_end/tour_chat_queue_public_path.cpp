/// \file
/// \brief RFC-BPR-0126 AC01 ([[BPR-2154]]). The tour chats and a local
/// draft's discussion queue and steer, through the desktop on
/// returning-review-v1 with the fixture agent as Claude:
///
/// - `--stop-chat`: the tour drafted and accepted, a stop's chat asks a slow
///   question; while it runs, Send reads Queue, Stop stands beside it, and two
///   follow-ups wait in the stop's pending list (Return), one edited in place;
///   the whole tour's chat shows none of them (tour parts never share
///   inputs). Each goes as its own turn after the one before, in order, once.
/// - `--whole-tour`: the whole-tour conversation after its first draft: a
///   follow-up queued, one steered with Cmd+Return (Ctrl+Return off macOS)
///   that shows in the transcript at once as the author's, while the turn
///   runs, and stays on that turn ([[BPR-2154]], look queue-tour-v002), Pause
///   queue holding the rest past the running turn's end and Resume queue
///   sending them, in order, once.
/// - `--local-draft`: a local draft's discussion (Q2) on a real worktree: a
///   follow-up typed while its question runs waits in the pending list and
///   goes as the draft's next question once that one ended; then one steered
///   with Cmd+Return into a running whole-tour question shows in the
///   transcript at once and lands in that turn ([[BPR-2154]], looks
///   queue-tour-v001 and v002).
///
/// The session is a Fixture agent session, as an owner's look is: the agent
/// is a saved entry, so a tour turn runs on a kept native session.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/fixture_shell.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_pending_list.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWindow>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	void open_review(qt::workbench_window& aWindow)
	{
		if (!aWindow.github_open_action()->isEnabled()) {
			aWindow.github_connect_action()->trigger();
		}
		wait([&] { return aWindow.github_open_action()->isEnabled(); }, "connected");
		aWindow.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		aWindow.home_open_action()->trigger();
		wait([&] { return aWindow.review_ask_button()->isVisible() && aWindow.github_open_action()->isEnabled(); }, "the window shows the pull request");
	}

	void agent_ready(qt::workbench_window& aWindow)
	{
		aWindow.open_agent_question();
		auto*      tab   = aWindow.agent_tab();
		const auto ready = [tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; };
		wait([tab] { return tab->setup_readiness() != qt::agent_setup_readiness::checking; }, "the agent setup looks for agents");
		// The Fixture agent session saves its entry as it starts.
		static_cast<void>(e2e::wait_until_plain(ready, std::chrono::milliseconds{ 15000 }));
		if (!ready()) {
			aWindow.open_agent_setup();
			const auto check = [&aWindow] {
				auto* const button = aWindow.agent_setup()->findChild<QPushButton*>(QStringLiteral("agentSetupCheck"));
				return button != nullptr && button->isVisible() && button->isEnabled() ? button : nullptr;
			};
			wait([&] { return check() != nullptr; }, "the setup offers Check it works on the fixture agent");
			check()->click();
			wait(ready, "Check it works passes on the fixture agent");
			aWindow.open_agent_question();
		}
		auto* const selector = aWindow.agent_panel()->profile_selector();
		// The saved Fixture agent entry, as a person's setup saves Claude
		// Code: a tour turn runs on its kept native session, so it can be
		// steered; the built-in profile has none.
		if (const auto index = selector->findText(QStringLiteral("Fixture agent"), Qt::MatchStartsWith); index >= 0) {
			selector->setCurrentIndex(index);
			return;
		}
		if (const auto index = selector->findData(QVariant::fromValue<qulonglong>(core::claude_stream_json_profile_id.value())); index >= 0) {
			selector->setCurrentIndex(index);
		}
	}

	/// Return in the chat's composer; Cmd+Return (Ctrl+Return off macOS)
	/// with `aOpposite`.
	/// The tour host has retired its last run: Send sends, not queues.
	void wait_sends(qt::tour_side_chat& aChat, const QString& aText)
	{
		aChat.composer()->setPlainText(aText);
		wait([&] { return aChat.send_button()->accessibleName() == QStringLiteral("Send") && aChat.send_button()->isEnabled(); }, "the tour agent is free: Send sends");
	}

	void type_and_return(qt::tour_side_chat& aChat, const QString& aText, const bool aOpposite = false)
	{
		aChat.composer()->setPlainText(aText);
		QApplication::processEvents();
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, aOpposite ? Qt::ControlModifier : Qt::NoModifier };
		QApplication::sendEvent(aChat.composer(), &press);
		QApplication::processEvents();
	}

	[[nodiscard]] auto rows_of(const qt::tour_side_chat& aChat) -> QStringList
	{
		auto labels = QStringList{ };
		for (auto* const label : aChat.pending_list()->findChildren<QLabel*>(QStringLiteral("agentQueueLabel"))) {
			if (label->isVisibleTo(aChat.pending_list())) {
				labels << label->text();
			}
		}
		return labels;
	}

	/// The steered messages the chat's transcript shows, in order
	/// ([[BPR-2154]], look queue-tour-v002).
	[[nodiscard]] auto steered_in(const qt::tour_side_chat& aChat) -> QStringList
	{
		auto texts = QStringList{ };
		for (auto* const label : aChat.findChildren<QLabel*>(QStringLiteral("tourSideChatSteeredText"))) {
			if (label->isVisibleTo(&aChat)) {
				texts << label->text();
			}
		}
		return texts;
	}

	/// The words over each steered message the transcript shows.
	[[nodiscard]] auto steered_states(const qt::tour_side_chat& aChat) -> QStringList
	{
		auto texts = QStringList{ };
		for (auto* const label : aChat.findChildren<QLabel*>(QStringLiteral("tourSideChatSteeredState"))) {
			if (label->isVisibleTo(&aChat)) {
				texts << label->text();
			}
		}
		return texts;
	}

	/// The shown button `aName` in the card labelled `aLabel`, or in the
	/// list's head with no label.
	auto press_in(const qt::tour_side_chat& aChat, const QString& aName, const QString& aLabel = { }) -> bool
	{
		auto* const list = aChat.pending_list();
		for (auto* const button : list->findChildren<QPushButton*>(aName)) {
			if (!button->isVisibleTo(list)) {
				continue;
			}
			if (!aLabel.isEmpty()) {
				auto* const label = button->parentWidget()->findChild<QLabel*>(QStringLiteral("agentQueueLabel"));
				if (label == nullptr || label->text() != aLabel) {
					continue;
				}
			}
			button->click();
			QApplication::processEvents();
			return true;
		}
		return false;
	}

	/// The questions of a thread's ask turns, in order, and whether all ended answered.
	[[nodiscard]] auto asked_in(const qt::tour_agent_panel& aPanel, const core::tour_thread& aThread) -> std::vector<std::string>
	{
		auto asked = std::vector<std::string>{ };
		for (const auto& run : aPanel.session().Runs) {
			if (run.Request == core::tour_agent_request::ask && run.Thread == aThread) {
				asked.push_back(run.Question);
			}
		}
		return asked;
	}

	[[nodiscard]] auto running(const qt::tour_agent_panel& aPanel) -> bool
	{
		return std::ranges::any_of(aPanel.session().Runs, [](const core::tour_agent_run& aRun) { return aRun.State == core::tour_agent_run_state::running; });
	}

	/// With `BPR_TOUR_QUEUE_CAPTURE` set, the chat as rendered, for a look's
	/// Now image.
	void capture(QWidget& aWidget, const QString& aName)
	{
		if (const auto folder = qEnvironmentVariable("BPR_TOUR_QUEUE_CAPTURE"); !folder.isEmpty()) {
			static_cast<void>(aWidget.grab().save(folder + QLatin1Char('/') + aName + QStringLiteral(".png")));
		}
	}

	[[nodiscard]] auto joined(const std::vector<std::string>& aTexts) -> std::string
	{
		auto text = std::string{ };
		for (const auto& item : aTexts) {
			text += (text.empty() ? "" : " | ") + item;
		}
		return text;
	}

	/// [[BPR-2368]]. Looks at a part's chat while a released message's turn
	/// runs: that message is the running turn, so its pending list must not
	/// also show it. `aRunning` names the running question, or is empty.
	struct running_watch
	{
		int         Looks = 0;
		std::string StillPending;

		void look(const qt::tour_side_chat& aChat, const QString& aRunning)
		{
			if (aRunning.isEmpty()) {
				return;
			}
			++Looks;
			const auto rows = rows_of(aChat);
			if (rows.contains(aRunning.section(QLatin1Char('\n'), 0, 0)) && StillPending.empty()) {
				StillPending = aRunning.toStdString() + " while pending: " + rows.join(QLatin1Char('|')).toStdString();
			}
		}
	};

	/// The question of the running ask turn of `aThread`, if one runs.
	[[nodiscard]] auto running_question(const qt::tour_agent_panel& aPanel, const core::tour_thread& aThread) -> QString
	{
		for (const auto& run : aPanel.session().Runs) {
			if (run.Request == core::tour_agent_request::ask && run.Thread == aThread && run.State == core::tour_agent_run_state::running) {
				return QString::fromStdString(run.Question);
			}
		}
		return { };
	}

	/// Drafts the tour on the fixture agent and accepts it whole.
	void draft_tour(qt::workbench_window& aWindow)
	{
		aWindow.open_review_tour();
		auto* const editor = aWindow.tour_editor();
		auto* const tour   = editor->tour_agent();
		wait([&] { return editor->shown().Draft && !editor->shown().Loading && tour->draft_button()->isEnabled(); }, "the tour is ready to draft");
		tour->draft_button()->click();
		wait([&] {
			const auto& runs = tour->session().Runs;
			return !runs.empty() && runs.back().Request == core::tour_agent_request::draft_tour && runs.back().State != core::tour_agent_run_state::running;
		},
			 "the draft ends");
		expect(tour->session().Runs.back().State == core::tour_agent_run_state::answered, "the fixture agent drafts the tour: " + tour->transcript_text().toStdString());
		wait([&] { return tour->accept_all_button()->isVisible() || (editor->shown().Draft && editor->shown().Draft->Stops.size() >= 2U); }, "the draft is proposed");
		if (tour->accept_all_button()->isVisible()) {
			tour->accept_all_button()->click();
		}
		wait([&] { return editor->shown().Draft && editor->shown().Draft->Stops.size() >= 2U && !editor->shown().Saving; }, "the draft's two stops are the tour's");
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	// A slow turn of the fixture agent takes about twelve seconds here.
	qputenv("BPR_FIXTURE_AGENT_SLOW_STEP_MS", "600");
	QApplication qtApplication{ argc, argv };
	if (argc < 2) {
		std::cerr << "usage: tour_chat_queue_public_path FIXTURE_AGENT [--stop-chat|--whole-tour|--local-draft WORKTREE_SCRIPT]\n";
		return EXIT_FAILURE;
	}
	const auto    agent = std::filesystem::path{ argv[1] };
	const auto    mode  = argc > 2 ? std::string{ argv[2] } : std::string{ "--stop-chat" };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	auto search = e2e::stub_claude_search(std::filesystem::path{ data.path().toStdString() } / "fixture-agent-search", agent);
	if (!search) {
		std::cerr << "fixture agent search refused\n";
		return EXIT_FAILURE;
	}
	app::github_application*              owner = nullptr;
	std::unique_ptr<app::profile_desktop> desktop;
	try {
		auto options                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.ClaudeAgentExecutable = agent;
		options.ClaudeSearch          = search;
		// As an owner's look runs: the Fixture agent is a saved agent entry,
		// so a tour turn runs on a kept native session and can be steered.
		options.FixtureAgentSession = true;
		auto terminal               = false;
		desktop                     = access::start_desktop(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](app::profile_desktop_startup_result aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
		wait([&] { return terminal; }, "the profile owner starts");
		expect(owner != nullptr, "the profile owner is admitted");
		owner->show();
		wait([&] { return owner->windows().size() == 1U; }, "the window is admitted");
		auto* const window = owner->window(owner->windows().front());
		window->resize(1440, 960);
		open_review(*window);
		agent_ready(*window);
		auto* const editor = window->tour_editor();
		auto* const chat   = editor->side_chat();
		auto* const panel  = editor->tour_agent();
		const auto  quit   = [&] {
			window->findChild<QAction*>(QStringLiteral("quitProfilesAction"))->trigger();
			wait([&] { return desktop->is_quiescent(); }, "Quit drains every session");
		};

		if (mode == "--stop-chat") {
			draft_tour(*window);
			const auto stop = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = editor->shown().Draft->Stops.front().Id };
			// The stop's chat toggle: the stop selected, its chat open.
			editor->go_to_stop(stop.Stop);
			editor->open_side_chat(stop);
			QApplication::processEvents();
			wait_sends(*chat, QStringLiteral("Take your time: what does this stop show?"));
			type_and_return(*chat, QStringLiteral("Take your time: what does this stop show?"));
			wait([&] { return running(*panel) && asked_in(*panel, stop).size() == 1U; }, "the stop's first question runs");
			// AC01: while it runs, Send reads Queue and Stop stays reachable.
			chat->composer()->setPlainText(QStringLiteral("Second: is resize covered?"));
			QApplication::processEvents();
			expect(chat->send_button()->text() == QStringLiteral("Queue") && chat->send_button()->isEnabled() && chat->stop_button()->isVisible(),
				   "while the agent runs, Send reads Queue, with Stop beside it: " + chat->send_button()->text().toStdString());
			type_and_return(*chat, QStringLiteral("Second: is resize covered?"));
			type_and_return(*chat, QStringLiteral("Third: anything else?"));
			wait([&] { return rows_of(*chat).size() == 2; }, "two follow-ups wait in the stop's pending list");
			expect(chat->composer()->toPlainText().isEmpty(), "each queued message leaves the composer");
			// Edited in place, held while edited.
			expect(press_in(*chat, QStringLiteral("agentQueueEdit"), QStringLiteral("Second: is resize covered?")), "the pencil edits the waiting message");
			wait([&] { return chat->pending_list()->findChild<QLineEdit*>(QStringLiteral("agentQueueEditField")) != nullptr; }, "the message is edited in place");
			chat->pending_list()->findChild<QLineEdit*>(QStringLiteral("agentQueueEditField"))->setText(QStringLiteral("Second, edited: is resize covered?"));
			expect(press_in(*chat, QStringLiteral("agentQueueSave")), "Save keeps the edit");
			wait([&] { return rows_of(*chat).contains(QStringLiteral("Second, edited: is resize covered?")); }, "the edit shows in the pending list");
			chat->composer()->setPlainText(QStringLiteral("Fourth, typed while it runs"));
			QApplication::processEvents();
			capture(*chat, QStringLiteral("stop-chat-queue"));
			chat->composer()->clear();
			// Tour parts never share inputs: the whole tour's chat waits on nothing.
			editor->open_side_chat(core::tour_thread{ });
			QApplication::processEvents();
			expect(rows_of(*chat).isEmpty(), "the whole tour's chat shows none of the stop's messages");
			editor->go_to_stop(stop.Stop);
			editor->open_side_chat(stop);
			QApplication::processEvents();
			wait([&] { return rows_of(*chat).size() == 2; }, "back in the stop's chat, its pending list");
			auto followUps = running_watch{ };
			if (!e2e::wait_until_plain(
					[&] {
						// [[BPR-2368]]. A follow-up whose turn runs is not also pending.
						if (asked_in(*panel, stop).size() >= 2U) {
							followUps.look(*chat, running_question(*panel, stop));
						}
						return asked_in(*panel, stop).size() == 3U && !running(*panel) && rows_of(*chat).isEmpty();
					},
					e2e::extended_agent_budget)) {
				throw std::runtime_error{ "each follow-up goes as its own turn: " + joined(asked_in(*panel, stop)) + " rows " + rows_of(*chat).join(QLatin1Char('|')).toStdString() };
			}
			expect(followUps.Looks > 0, "a follow-up's turn was seen running");
			expect(followUps.StillPending.empty(), "while a follow-up's turn runs it is not also pending in the stop's queue: " + followUps.StillPending);
			const auto asked = asked_in(*panel, stop);
			expect(asked[1] == "Second, edited: is resize covered?" && asked[2] == "Third: anything else?", "in order, as edited, once each: " + joined(asked));
			for (const auto& run : panel->session().Runs) {
				if (run.Thread == stop) {
					expect(run.State == core::tour_agent_run_state::answered && run.Reply.find("Fixture agent: read your message") != std::string::npos, "each turn is answered: " + run.Reply);
				}
			}
			expect(asked_in(*panel, core::tour_thread{ }).empty(), "nothing went to the whole tour");
			std::cout << "tour-chat-queue-stop:pass\n";
		} else if (mode == "--whole-tour") {
			draft_tour(*window);
			const auto whole = core::tour_thread{ };
			editor->open_side_chat(whole);
			QApplication::processEvents();
			wait_sends(*chat, QStringLiteral("Take your time: is the tour in a good order?"));
			type_and_return(*chat, QStringLiteral("Take your time: is the tour in a good order?"));
			wait([&] { return running(*panel) && asked_in(*panel, whole).size() == 1U; }, "the whole tour's first question runs");
			type_and_return(*chat, QStringLiteral("Queued for the tour"));
			wait([&] { return rows_of(*chat).size() == 1; }, "the follow-up waits in the whole tour's pending list");
			capture(*chat, QStringLiteral("whole-tour-queue"));
			// The fixture agent has started and said its version: the turn is
			// under way on its connection, as when a person steers it.
			auto started = QElapsedTimer{ };
			started.start();
			wait([&] { return started.elapsed() > 3000; }, "the question has run a while");
			expect(running(*panel), "the whole tour's question still runs");
			// Cmd+Return: the other follow-up, Steer, for this one message.
			chat->composer()->setPlainText(QStringLiteral("Steered into the tour"));
			QApplication::processEvents();
			type_and_return(*chat, QStringLiteral("Steered into the tour"), true);
			// [[BPR-2154]] (look queue-tour-v002). Steered, it is the author's
			// in the transcript at once, while the turn still runs.
			auto shownWhileRunning = false;
			wait([&] {
				shownWhileRunning = shownWhileRunning || (running(*panel) && steered_in(*chat).contains(QStringLiteral("Steered into the tour")));
				return shownWhileRunning || !running(*panel);
			},
				 "the steered message shows in the transcript, or the turn ends");
			const auto notice = [&] {
				auto* const label = editor->findChild<QLabel*>(QStringLiteral("tourWritingStatus"));
				return label != nullptr ? label->text().toStdString() : std::string{ };
			};
			expect(shownWhileRunning, "a steered message shows in the transcript while its turn runs: " + steered_in(*chat).join(QLatin1Char('|')).toStdString() + "; " + notice());
			capture(*chat, QStringLiteral("whole-tour-steered"));
			// Pause queue holds what waits past the running turn's end.
			expect(press_in(*chat, QStringLiteral("agentQueuePauseResume")), "Pause queue is offered");
			wait([&] { return !running(*panel); }, "the first turn ends");
			for (auto settle = 0; settle < 40; ++settle) {
				QApplication::processEvents();
			}
			expect(std::ranges::none_of(asked_in(*panel, whole), [](const std::string& aAsked) { return aAsked == "Steered into the tour"; }), "the steered message is no question of its own");
			expect(steered_in(*chat) == QStringList{ QStringLiteral("Steered into the tour") } && !steered_states(*chat).join(QLatin1Char('|')).contains(QStringLiteral("may not")),
				   "the steered message stays in the turn it went into once it ended: " + steered_in(*chat).join(QLatin1Char('|')).toStdString() + " / "
					   + steered_states(*chat).join(QLatin1Char('|')).toStdString());
			const auto before = asked_in(*panel, whole).size();
			expect(rows_of(*chat).contains(QStringLiteral("Queued for the tour")) && !running(*panel), "paused, the follow-up waits: " + joined(asked_in(*panel, whole)));
			expect(press_in(*chat, QStringLiteral("agentQueuePauseResume")), "Resume queue is offered");
			auto resumed = running_watch{ };
			wait([&] {
				// [[BPR-2368]]. The follow-up whose turn runs is not also pending.
				resumed.look(*chat, running_question(*panel, whole));
				return rows_of(*chat).isEmpty() && !running(*panel) && asked_in(*panel, whole).size() > before;
			},
				 "resumed, what waited goes: " + joined(asked_in(*panel, whole)));
			expect(resumed.Looks > 0, "the follow-up's turn was seen running");
			expect(resumed.StillPending.empty(), "while the follow-up's turn runs it is not also pending in the whole tour's queue: " + resumed.StillPending);
			const auto asked = asked_in(*panel, whole);
			expect(std::ranges::count(asked, std::string{ "Queued for the tour" }) == 1, "the queued follow-up went once: " + joined(asked));
			std::cout << "tour-chat-queue-whole:steered:pass\n";
		} else if (mode == "--local-draft") {
			expect(argc > 3, "the worktree script is named");
			const auto root    = std::filesystem::path{ data.path().toStdString() } / "worktree";
			const auto refused = bpr::tests::run_fixture_script(std::filesystem::path{ argv[3] }, root);
			expect(!refused, "the worktree is made: " + refused.value_or(std::string{ }));
			owner->open_local_draft((root / "engine").string());
			wait([&] { return window->showing_local_draft() && window->local_draft()->projection().Draft.has_value(); }, "the local draft shows");
			wait([&] { return window->local_draft()->projection().AgentAvailable; }, "the draft's agent is available");
			// A local draft's question is let go when its window loses focus
			// (`local_draft_desktop::writing_stop`): the window keeps it.
			window->activateWindow();
			wait([&] { return QGuiApplication::focusWindow() == window->windowHandle(); }, "the window has focus");
			editor->open_side_chat(core::tour_thread{ });
			QApplication::processEvents();
			wait_sends(*chat, QStringLiteral("Take your time: what could break?"));
			// Return asks at once: the question is with the agent before the
			// event loop runs again.
			chat->composer()->setPlainText(QStringLiteral("Take your time: what could break?"));
			QKeyEvent first{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(chat->composer(), &first);
			expect(window->local_draft()->projection().Asking, "the draft's first question runs: " + window->local_draft()->projection().AskFailure);
			chat->composer()->setPlainText(QStringLiteral("Next: and the tests?"));
			expect(chat->send_button()->text() == QStringLiteral("Queue") && chat->send_button()->isEnabled(),
				   "while it runs, Send reads Queue: " + chat->send_button()->text().toStdString());
			QKeyEvent next{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(chat->composer(), &next);
			expect(window->local_draft()->projection().Asking && chat->composer()->toPlainText().isEmpty(), "the follow-up left the composer while the first question runs");
			const auto queued = rows_of(*chat);
			expect(queued == QStringList{ QStringLiteral("Next: and the tests?") }, "the follow-up waits in the discussion's pending list: " + queued.join(QLatin1Char('|')).toStdString());
			capture(*chat, QStringLiteral("local-draft-queue"));
			const auto said = [&] {
				auto texts = std::vector<std::string>{ };
				for (const auto& conversation : window->local_draft()->projection().Draft->Preparation.Conversations) {
					for (const auto& turn : conversation.Turns) {
						if (turn.Author) {
							texts.push_back(turn.Text);
						}
					}
				}
				return texts;
			};
			auto followUp = running_watch{ };
			if (!e2e::wait_until_plain(
					[&] {
						// [[BPR-2368]]. The follow-up whose question runs is not also pending.
						const auto& projection = window->local_draft()->projection();
						followUp.look(*chat, projection.Asking && projection.AskingText == "Next: and the tests?" ? QString::fromStdString(projection.AskingText) : QString{ });
						return said().size() == 2U && !projection.Asking && rows_of(*chat).isEmpty();
					},
					e2e::extended_agent_budget)) {
				throw std::runtime_error{ "the follow-up goes as the draft's next question: " + joined(said()) + " rows " + rows_of(*chat).join(QLatin1Char('|')).toStdString() + " asking "
										  + std::to_string(window->local_draft()->projection().Asking) + " failure " + window->local_draft()->projection().AskFailure };
			}
			expect(followUp.Looks > 0, "the follow-up's question was seen running");
			expect(followUp.StillPending.empty(), "while the follow-up's question runs it is not also pending in the discussion's queue: " + followUp.StillPending);
			expect(said()[1] == "Next: and the tests?", "after the first, once: " + joined(said()));
			// Steer (Cmd+Return, Ctrl+Return off macOS) while a question
			// runs: the message goes into that turn, on the kept connection
			// the question runs on, and never as a question of its own.
			wait_sends(*chat, QStringLiteral("Take your time: anything else?"));
			type_and_return(*chat, QStringLiteral("Take your time: anything else?"));
			expect(window->local_draft()->projection().Asking, "the draft's third question runs: " + window->local_draft()->projection().AskFailure);
			// The fixture agent has started and said its version.
			auto started = QElapsedTimer{ };
			started.start();
			wait([&] { return started.elapsed() > 3000; }, "the third question has run a while");
			expect(window->local_draft()->projection().Asking, "the third question still runs");
			type_and_return(*chat, QStringLiteral("Steered into the draft"), true);
			expect(chat->composer()->toPlainText().isEmpty(), "the steered message left the composer");
			// The window's status line says it once; the tour editor's label
			// keeps it for assistive technology.
			const auto notice = [&] {
				auto* const label = editor->findChild<QLabel*>(QStringLiteral("tourWritingStatus"));
				return label != nullptr ? label->text() : QString{ };
			};
			// [[BPR-2154]] (look queue-tour-v002). It is the author's in the
			// transcript at once, while the question still runs.
			wait([&] { return steered_in(*chat).contains(QStringLiteral("Steered into the draft")) || !window->local_draft()->projection().Asking; }, "the steered message shows in the transcript");
			expect(window->local_draft()->projection().Asking, "the steered message showed while the question still runs");
			capture(*chat, QStringLiteral("local-draft-steered-transcript"));
			wait([&] { return notice().contains(QStringLiteral("Steered into the running turn")) || !window->local_draft()->projection().Asking; }, "the steer is answered while the question runs");
			expect(notice().contains(QStringLiteral("Steered into the running turn")),
				   "the message was steered: " + notice().toStdString() + " failure " + window->local_draft()->projection().AskFailure);
			capture(*chat, QStringLiteral("local-draft-steered"));
			if (!e2e::wait_until_plain([&] { return !window->local_draft()->projection().Asking && rows_of(*chat).isEmpty(); }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "the steered turn ends and the steered message settles: rows " + rows_of(*chat).join(QLatin1Char('|')).toStdString() };
			}
			for (auto settle = 0; settle < 40; ++settle) {
				QApplication::processEvents();
			}
			const auto all = said();
			expect(all.size() == 4U && all[2] == "Take your time: anything else?" && all[3] == "Steered into the draft", "the steered message is the author's, in the turn it went into: " + joined(all));
			auto reply = std::string{ };
			for (const auto& conversation : window->local_draft()->projection().Draft->Preparation.Conversations) {
				if (!conversation.Turns.empty() && !conversation.Turns.back().Author) {
					reply = conversation.Turns.back().Text;
				}
			}
			expect(reply.find("Took your correction") != std::string::npos, "the agent read it in the running turn: " + reply);
			// It stays on that turn, which ended answered, and never became a
			// question of its own.
			expect(steered_in(*chat) == QStringList{ QStringLiteral("Steered into the draft") }, "the steered message stays in its turn: " + steered_in(*chat).join(QLatin1Char('|')).toStdString());
			const auto& runs     = panel->session().Runs;
			const auto  answered = std::ranges::find(runs, std::string{ "Take your time: anything else?" }, &core::tour_agent_run::Question);
			expect(answered != runs.end() && answered->State == core::tour_agent_run_state::answered && answered->Steered.size() == 1U && answered->Steered.front().Text == "Steered into the draft",
				   "the turn it went into ended answered, with it");
			expect(std::ranges::none_of(runs, [](const core::tour_agent_run& aRun) { return aRun.Question == "Steered into the draft"; }), "it is no question of its own");
			expect(!window->local_draft()->projection().Asking, "nothing went after it as a question of its own");
			std::cout << "tour-chat-queue-local-draft:pass\n";
		} else {
			throw std::runtime_error{ "unknown mode " + mode };
		}
		quit();
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "tour-chat-queue: " << aError.what() << '\n';
		if (owner != nullptr && !owner->windows().empty()) {
			if (auto* const window = owner->window(owner->windows().front()); window != nullptr) {
				std::cerr << "status: " << window->status_text().toStdString() << "\ntranscript: " << window->tour_editor()->tour_agent()->transcript_text().toStdString() << '\n';
				for (const auto& run : window->tour_editor()->tour_agent()->session().Runs) {
					std::cerr << "run request=" << static_cast<int>(run.Request) << " thread=" << static_cast<int>(run.Thread.Kind) << ":" << run.Thread.Stop.value() << " state=" << static_cast<int>(run.State)
							  << " question=" << run.Question << '\n';
				}
			}
		}
		return EXIT_FAILURE;
	}
}
