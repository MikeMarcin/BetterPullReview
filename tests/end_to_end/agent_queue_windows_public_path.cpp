/// \file
/// \brief RFC-BPR-0126 AC04 ([[BPR-2151]]), Q10 as overridden by the owner:
/// "Queue must be tied to session not active UI windows". Two windows of one
/// profile on the same pull request (returning-review-v1, the scoped writing
/// stub as the installed Claude), through the desktop:
///
/// - messages queued in one window show in the other, in the same order;
/// - a message one window edits shows as being edited in the other, with no
///   controls there, and its edit shows in both on Save;
/// - a removal in one window shows in the other, and the other's later edit
///   on the removed message is refused and says so;
/// - a message typed in the second window joins the one queue;
/// - Resume in either window drains it one turn at a time; closing the window
///   running a turn neither pauses the queue nor touches that turn: the
///   profile's runtime of the session runs it to its end and what is left
///   after it, in order, each once ([[BPR-2181]] AC09).
///
/// `--close-runs-on` ([[BPR-2181]] AC01): two messages queued behind a
/// running one, the session's only window closed (another window, on Home,
/// keeps the profile open): every turn still runs with no window showing the
/// session; reopened, the pull request's Agent tab shows each turn once.
///
/// `--restart` ([[BPR-2181]] AC01, AC04): first, two messages queued behind
/// a running one and the session's only window closed (another, on Home,
/// keeps the profile open): the turns run with no window, the app quits
/// before any window shows the session again, and on relaunch the pull
/// request's Agent tab shows each turn once, in order. Then a message queued
/// and the app quit; on relaunch the queue is read back from disk, paused
/// because the app restarted, and nothing runs until the reviewer resumes it.
///
/// `--finish-pauses` (RFC-BPR-0126 AC02, [[BPR-2181]]): on the fixture agent,
/// a run that fails pauses the queue as failed with the next message still
/// queued; resumed, a run past the agent's usage limit pauses it as usage
/// limit, the last message still queued; resumed again, that one answers.
///
/// `--tour-feedback-crash` ([[BPR-2181]] AC11): the store holds the pull
/// request's tour feedback as a crash left it, one feedback being sent and
/// one waiting for the draft. On the next launch neither waits forever: both
/// come back to the tour chat as one turn that was not sent, the uncertain
/// one discarded by the core's transition, and the tour's queue is empty and
/// not paused, so the next draft's feedback queues again.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>

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

	void open_review(qt::workbench_window& aWindow, const std::string& aWhich)
	{
		if (!aWindow.github_open_action()->isEnabled()) {
			aWindow.github_connect_action()->trigger();
		}
		wait([&] { return aWindow.github_open_action()->isEnabled(); }, aWhich + " connected");
		aWindow.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		aWindow.home_open_action()->trigger();
		// The review bar shows once the pull request is read. (Only one window
		// holds the review ledger, so only the first offers What changed.)
		if (!e2e::wait_until_plain([&] { return aWindow.review_ask_button()->isVisible() && aWindow.github_open_action()->isEnabled(); }, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aWhich + " shows the pull request: " + aWindow.status_text().toStdString() + " / " + aWindow.github_status_text() };
		}
	}

	void agent_ready(qt::workbench_window& aWindow)
	{
		aWindow.open_agent_question();
		auto*      tab   = aWindow.agent_tab();
		const auto ready = [tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; };
		wait([tab] { return tab->setup_readiness() != qt::agent_setup_readiness::checking; }, "the agent setup looks for agents");
		if (!ready()) {
			aWindow.open_agent_setup();
			const auto check = [&aWindow] {
				auto* const button = aWindow.agent_setup()->findChild<QPushButton*>(QStringLiteral("agentSetupCheck"));
				return button != nullptr && button->isVisible() && button->isEnabled() ? button : nullptr;
			};
			wait([&] { return check() != nullptr; }, "the setup offers Check it works on the stub claude");
			check()->click();
			wait(ready, "Check it works passes on the stub claude");
			aWindow.open_agent_question();
		}
		auto* const selector = aWindow.agent_panel()->profile_selector();
		for (const auto claude : { core::claude_stream_json_profile_id, core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U) }) {
			if (const auto index = selector->findData(QVariant::fromValue<qulonglong>(claude.value())); index >= 0) {
				selector->setCurrentIndex(index);
				break;
			}
		}
	}

	[[nodiscard]] auto rows_of(qt::workbench_window& aWindow) -> QStringList
	{
		auto labels = QStringList{ };
		for (auto* const label : aWindow.agent_tab()->queue_list()->findChildren<QLabel*>(QStringLiteral("agentQueueLabel"))) {
			labels << label->text();
		}
		return labels;
	}

	[[nodiscard]] auto card_of(qt::workbench_window& aWindow, const QString& aLabel) -> QWidget*
	{
		for (auto* const card : aWindow.agent_tab()->queue_list()->findChildren<QWidget*>(QStringLiteral("agentQueueRow"))) {
			auto* const label = card->findChild<QLabel*>(QStringLiteral("agentQueueLabel"));
			if (label != nullptr && label->text() == aLabel) {
				return card;
			}
		}
		return nullptr;
	}

	auto press_in(qt::workbench_window& aWindow, const QString& aName, const QString& aLabel = { }) -> bool
	{
		auto* const list = aWindow.agent_tab()->queue_list();
		auto* const card = aLabel.isEmpty() ? nullptr : card_of(aWindow, aLabel);
		auto* const in   = card != nullptr ? card : static_cast<QWidget*>(list);
		if (!aLabel.isEmpty() && card == nullptr) {
			return false;
		}
		// A row the list rebuilt waits for deletion beside its replacement:
		// the shown one is pressed.
		const auto buttons = in->findChildren<QPushButton*>(aName);
		const auto shown   = std::ranges::find_if(buttons, [list](const QPushButton* aButton) { return aButton->isVisibleTo(list); });
		if (shown == buttons.end()) {
			std::cerr << "agent-queue-windows: " << buttons.size() << " " << aName.toStdString() << " buttons, none shown\n";
			return false;
		}
		auto* const button = *shown;
		button->click();
		QApplication::processEvents();
		return true;
	}

	void type_and_return(qt::workbench_window& aWindow, const QString& aText)
	{
		auto* const composer = aWindow.agent_tab()->question_editor();
		composer->setPlainText(aText);
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(composer, &press);
		QApplication::processEvents();
	}

	[[nodiscard]] auto questions_of(const qt::agent_tab_view& aTab) -> QStringList
	{
		auto asked = QStringList{ };
		for (std::size_t turn = 0U; turn < aTab.turn_count(); ++turn) {
			asked << aTab.turn_question(turn);
		}
		return asked;
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	if (argc < 2) {
		std::cerr << "usage: agent_queue_windows_public_path STUB\n";
		return EXIT_FAILURE;
	}
	const auto    stub        = std::filesystem::path{ argv[1] };
	const auto    mode        = argc > 2 ? std::string{ argv[2] } : std::string{ };
	const auto    loading     = mode == "--second-window-loading";
	const auto    closeRunsOn = mode == "--close-runs-on";
	const auto    restart     = mode == "--restart";
	const auto    finishes    = mode == "--finish-pauses";
	const auto    tourCrash   = mode == "--tour-feedback-crash";
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	const auto searchFolder = std::filesystem::path{ data.path().toStdString() } / "stub-claude-search";
	auto       search       = e2e::stub_claude_search(searchFolder, stub);
	if (!search) {
		std::cerr << "stub claude search refused\n";
		return EXIT_FAILURE;
	}
	app::github_application*              owner = nullptr;
	std::unique_ptr<app::profile_desktop> desktop;
	try {
		// The same profile data each time the desktop starts: a restart reads
		// what the last run left on disk.
		const auto start = [&] {
			auto options = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
			// The loading course is at the next push, where a second window's
			// read takes the repository's cache while the first window's run ends.
			options.EpochPhase            = loading ? app::detail::github_epoch_fixture_phase::reuse : app::detail::github_epoch_fixture_phase::single;
			options.ClaudeAgentExecutable = mode.empty() ? searchFolder / ("claude" + stub.extension().string()) : stub;
			options.ClaudeSearch          = search;
			// The second window reads the pull request again.
			options.RepeatReadSessions = true;
			auto terminal              = false;
			owner                      = nullptr;
			desktop                    = access::start_desktop(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](app::profile_desktop_startup_result aResult) {
				terminal = true;
				if (aResult) {
					owner = *aResult;
				}
			});
			wait([&] { return terminal; }, "the profile owner starts");
			expect(owner != nullptr, "the profile owner is admitted");
			owner->show();
			wait([&] { return owner->windows().size() == 1U; }, "the first window is admitted");
		};
		start();
		const auto firstContext = owner->windows().front();
		auto*      first        = owner->window(firstContext);
		first->resize(1440, 960);
		open_review(*first, "the first window");
		agent_ready(*first);
		const auto quit = [&](qt::workbench_window& aWindow) {
			aWindow.findChild<QAction*>(QStringLiteral("quitProfilesAction"))->trigger();
			wait([&] { return desktop->is_quiescent(); }, "Quit drains every session and the profile's services");
			desktop.reset();
		};
		if (finishes) {
			const auto session = access::agent_session_key(*owner);
			expect(!session.empty(), "the window shows the pull request's Agent-tab session");
			auto* const tab = first->agent_tab();
			// The fixture agent fails a question naming FIXTURE_CRASH, and
			// answers one naming FIXTURE_USAGE_LIMIT as past its usage limit.
			// The first is sent at once; the queue releases the rest.
			type_and_return(*first, QStringLiteral("First message"));
			wait([&] { return tab->state() == qt::agent_tab_state::working; }, "the first message runs: " + first->agent_panel()->status_text().toStdString());
			type_and_return(*first, QStringLiteral("FIXTURE_CRASH fails"));
			type_and_return(*first, QStringLiteral("FIXTURE_USAGE_LIMIT next"));
			type_and_return(*first, QStringLiteral("Last message"));
			expect(rows_of(*first).size() == 3, "three messages wait behind it: " + rows_of(*first).join(QLatin1Char('|')).toStdString());
			const auto pausedAs = [&](const core::queue_pause aPause, const std::size_t aQueued) {
				const auto now = access::agent_session(*owner, session);
				return now.Paused == aPause && now.Queued == aQueued && !now.Sending && now.Quiescent;
			};
			const auto describe = [&] {
				const auto now = access::agent_session(*owner, session);
				return " paused=" + std::string{ core::stable_name(now.Paused) } + " queued=" + std::to_string(now.Queued) + " sending=" + std::to_string(now.Sending ? 1 : 0) + " rows=" + rows_of(*first).join(QLatin1Char('|')).toStdString();
			};
			if (!e2e::wait_until_plain([&] { return pausedAs(core::queue_pause::failed, 2U); }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "a failed run pauses the queue as failed, both messages still queued:" + describe() };
			}
			// The host pauses first; the list shows its Resume on the next
			// projection, so the course waits for the button it presses.
			const auto resumeShown = [&] {
				const auto buttons = tab->queue_list()->findChildren<QPushButton*>(QStringLiteral("agentQueuePauseResume"));
				return std::ranges::any_of(buttons, [&](const QPushButton* aButton) { return aButton->isVisibleTo(tab->queue_list()) && aButton->text() == QStringLiteral("Resume queue"); });
			};
			expect(e2e::wait_until_plain(resumeShown, e2e::extended_agent_budget), "the paused queue offers Resume");
			expect(press_in(*first, QStringLiteral("agentQueuePauseResume")), "Resume after the failure");
			if (!e2e::wait_until_plain([&] { return pausedAs(core::queue_pause::usage_limit, 1U); }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "a run past the usage limit pauses the queue as usage limit, the last message still queued:" + describe() };
			}
			expect(rows_of(*first) == QStringList{ QStringLiteral("Last message") }, "the last message waits:" + describe());
			expect(e2e::wait_until_plain(resumeShown, e2e::extended_agent_budget), "the queue paused at the usage limit offers Resume");
			expect(press_in(*first, QStringLiteral("agentQueuePauseResume")), "Resume after the usage limit");
			if (!e2e::wait_until_plain([&] { return pausedAs(core::queue_pause::none, 0U) && tab->turn_count() >= 4U && tab->turn_outcome(tab->turn_count() - 1U) == core::agent_turn_outcome::answered; }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "resumed, the last message is answered:" + describe() };
			}
			quit(*first);
			std::cout << "agent-queue-finish-pauses:pass\n";
			return EXIT_SUCCESS;
		}
		if (closeRunsOn) {
			const auto session = access::agent_session_key(*owner);
			expect(!session.empty(), "the first window shows the pull request's Agent-tab session");
			// Another window, on Home, keeps the profile open.
			first->findChild<QAction*>(QStringLiteral("newBprWindowAction"))->trigger();
			wait([&] { return owner->windows().size() == 2U; }, "New Window opens a second window");
			const auto contexts = owner->windows();
			auto*      second   = owner->window(contexts.front() == firstContext ? contexts.back() : contexts.front());
			type_and_return(*first, QStringLiteral("First message"));
			wait([&] { return first->agent_tab()->state() == qt::agent_tab_state::working; }, "the first message runs: " + first->agent_panel()->status_text().toStdString());
			type_and_return(*first, QStringLiteral("Second message"));
			type_and_return(*first, QStringLiteral("Third message"));
			expect(rows_of(*first).contains(QStringLiteral("Second message")) && rows_of(*first).contains(QStringLiteral("Third message")), "two messages wait behind it: " + rows_of(*first).join(QLatin1Char('|')).toStdString());
			first->close();
			wait([&] { return owner->window(firstContext) == nullptr && owner->windows().size() == 1U; }, "the session's only window closes");
			// No window shows the session: its runtime runs every turn, and
			// nothing pauses.
			auto       launches = std::size_t{ 0U };
			auto       paused   = false;
			const auto drained  = e2e::wait_until_plain([&] {
				const auto now = access::agent_session(*owner, session);
				launches       = std::max(launches, now.Launches);
				paused         = paused || now.Paused != core::queue_pause::none;
				return now.Queued == 0U && !now.Sending && now.Quiescent;
			},
														e2e::extended_agent_budget);
			const auto last     = access::agent_session(*owner, session);
			expect(drained, "the queue drains with no window showing the session: queued=" + std::to_string(last.Queued) + " sending=" + std::to_string(last.Sending ? 1 : 0) + " runtime=" + std::to_string(last.Runtime ? 1 : 0));
			expect(!paused, "closing the window never paused the queue");
			expect(launches >= 3U, "the runtime started every turn itself: launches=" + std::to_string(launches));
			// Reopened, the session's one transcript shows each turn once.
			second->resize(1440, 960);
			open_review(*second, "the reopened pull request");
			agent_ready(*second);
			if (!e2e::wait_until_plain([&] {
					const auto asked = questions_of(*second->agent_tab());
					return asked.count(QStringLiteral("First message")) == 1 && asked.count(QStringLiteral("Second message")) == 1 && asked.count(QStringLiteral("Third message")) == 1;
				},
									   e2e::extended_agent_budget)) {
				throw std::runtime_error{ "the reopened Agent tab shows each turn once: " + questions_of(*second->agent_tab()).join(QLatin1Char('|')).toStdString() };
			}
			const auto asked = questions_of(*second->agent_tab());
			expect(asked.indexOf(QStringLiteral("First message")) < asked.indexOf(QStringLiteral("Second message")) && asked.indexOf(QStringLiteral("Second message")) < asked.indexOf(QStringLiteral("Third message")),
				   "in the order they were sent: " + asked.join(QLatin1Char('|')).toStdString());
			quit(*second);
			std::cout << "agent-queue-close-runs-on:pass\n";
			return EXIT_SUCCESS;
		}
		if (restart) {
			const auto session = access::agent_session_key(*owner);
			expect(!session.empty(), "the first window shows the pull request's Agent-tab session");
			// [[BPR-2181]] AC01. Turns no window saw, then a quit before any
			// window shows the session again.
			{
				first->findChild<QAction*>(QStringLiteral("newBprWindowAction"))->trigger();
				wait([&] { return owner->windows().size() == 2U; }, "New Window opens a second window");
				const auto contexts = owner->windows();
				auto*      home     = owner->window(contexts.front() == firstContext ? contexts.back() : contexts.front());
				type_and_return(*first, QStringLiteral("Asked before the quit"));
				wait([&] { return first->agent_tab()->state() == qt::agent_tab_state::working; }, "the first message runs: " + first->agent_panel()->status_text().toStdString());
				type_and_return(*first, QStringLiteral("Queued before the quit"));
				type_and_return(*first, QStringLiteral("Last before the quit"));
				expect(rows_of(*first).contains(QStringLiteral("Queued before the quit")) && rows_of(*first).contains(QStringLiteral("Last before the quit")),
					   "two messages wait behind it: " + rows_of(*first).join(QLatin1Char('|')).toStdString());
				first->close();
				wait([&] { return owner->windows().size() == 1U; }, "the session's only window closes");
				const auto drained = e2e::wait_until_plain([&] {
					const auto now = access::agent_session(*owner, session);
					return now.Queued == 0U && !now.Sending && now.Quiescent;
				},
														   e2e::extended_agent_budget);
				expect(drained, "the turns run with no window showing the session");
				quit(*home);
				start();
				first = owner->window(owner->windows().front());
				first->resize(1440, 960);
				open_review(*first, "the relaunched pull request");
				agent_ready(*first);
				const auto once = [&] {
					const auto asked = questions_of(*first->agent_tab());
					return asked.count(QStringLiteral("Asked before the quit")) == 1 && asked.count(QStringLiteral("Queued before the quit")) == 1 && asked.count(QStringLiteral("Last before the quit")) == 1;
				};
				if (!e2e::wait_until_plain(once, e2e::extended_agent_budget)) {
					throw std::runtime_error{ "after the quit, the Agent tab shows each turn once: " + questions_of(*first->agent_tab()).join(QLatin1Char('|')).toStdString() };
				}
				const auto asked = questions_of(*first->agent_tab());
				expect(asked.indexOf(QStringLiteral("Asked before the quit")) < asked.indexOf(QStringLiteral("Queued before the quit"))
						   && asked.indexOf(QStringLiteral("Queued before the quit")) < asked.indexOf(QStringLiteral("Last before the quit")),
					   "in the order they were sent: " + asked.join(QLatin1Char('|')).toStdString());
			}
			auto* const ask   = first->review_ask_button();
			const auto  turns = first->agent_tab()->turn_count();
			const auto  path  = QStringLiteral("src/engine/renderer.cpp");
			wait([&] { return first->go_to_file_line(path, 12U, false) && first->active_document_path() == path; }, "the file opens");
			for (auto times = 0; times < 2; ++times) {
				ask->refresh_menu();
				auto* const action = ask->menu()->findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.explain-file_file"));
				expect(action != nullptr, "the Ask menu lists Explain this file");
				action->trigger();
				QApplication::processEvents();
			}
			wait([&] { return first->agent_tab()->queue_list()->isVisible(); }, "a waiting message shows in the pending list");
			// The first goes once the relaunched session's agent is ready.
			wait([&] { return access::agent_session(*owner, session).Sending; }, "the first ask runs");
			auto* const list  = first->agent_tab()->queue_list();
			auto* const pause = list->findChild<QPushButton*>(QStringLiteral("agentQueuePauseResume"));
			expect(pause != nullptr, "the queue offers Pause");
			pause->click();
			QApplication::processEvents();
			if (!e2e::wait_until_plain([&] { return first->agent_tab()->turn_count() > turns && first->agent_tab()->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*owner).Quiescent && !access::agent_session(*owner, session).Sending; },
									   e2e::extended_agent_budget)) {
				const auto now = access::agent_session(*owner, session);
				throw std::runtime_error{ "the running ask finishes: turns=" + std::to_string(first->agent_tab()->turn_count()) + " before=" + std::to_string(turns) + " working=" + std::to_string(first->agent_tab()->state() == qt::agent_tab_state::working ? 1 : 0)
										  + " quiescent=" + std::to_string(access::agent_invocation_evidence(*owner).Quiescent ? 1 : 0) + " sending=" + std::to_string(now.Sending ? 1 : 0) + " queued=" + std::to_string(now.Queued)
										  + " paused=" + std::string{ core::stable_name(now.Paused) } + " status=" + first->agent_panel()->status_text().toStdString() };
			}
			type_and_return(*first, QStringLiteral("Kept across the restart"));
			expect(rows_of(*first).contains(QStringLiteral("Kept across the restart")), "typed, it waits: " + rows_of(*first).join(QLatin1Char('|')).toStdString());
			const auto waiting = rows_of(*first);
			quit(*first);
			// Relaunched on the same profile data.
			start();
			auto* const again = owner->window(owner->windows().front());
			again->resize(1440, 960);
			const auto restored = access::agent_session(*owner, session);
			expect(restored.Paused == core::queue_pause::restarted && restored.Queued == static_cast<std::size_t>(waiting.size()),
				   "the queue is read back from disk, paused because the app restarted: queued=" + std::to_string(restored.Queued) + " of " + std::to_string(waiting.size()));
			open_review(*again, "the relaunched window");
			agent_ready(*again);
			wait([&] { return rows_of(*again) == waiting; }, "the relaunched window shows the queue as it was: " + rows_of(*again).join(QLatin1Char('|')).toStdString());
			auto* const toggle = again->agent_tab()->queue_list()->findChild<QPushButton*>(QStringLiteral("agentQueuePauseResume"));
			expect(toggle != nullptr && toggle->text() == QStringLiteral("Resume queue"), "and offers Resume");
			for (auto settle = 0; settle < 20; ++settle) {
				QApplication::processEvents();
			}
			const auto still = access::agent_session(*owner, session);
			expect(still.Launches == 0U && still.Queued == static_cast<std::size_t>(waiting.size()), "nothing runs until the reviewer resumes it");
			quit(*again);
			// [[BPR-2181]] AC04. A crash while a message was sending: the store
			// holds it as sending, as the queue left it when the app died (a
			// quit stops the turn and settles it, so only the store can say
			// this). On the next launch it may not have been delivered: Resume
			// is refused, Discard removes it, and Resume sends what waited.
			{
				auto crashed = core::agent_queue{ };
				crashed.Entries.push_back({ .Id = 1U, .Input = { .Scope = "message", .Label = "Sending at the crash", .Text = "Sending at the crash" }, .State = core::queued_state::sending });
				crashed.Entries.push_back({ .Id = 2U, .Input = { .Scope = "message", .Label = "Waiting at the crash", .Text = "Waiting at the crash" } });
				crashed.LastId   = 2U;
				crashed.Revision = 7U;
				expect(adapters::sqlite::make_agent_queue_store(*dataRoot)->store(session, crashed), "the store holds the queue as the crash left it");
			}
			start();
			auto* const after = owner->window(owner->windows().front());
			after->resize(1440, 960);
			expect(access::agent_session(*owner, session).Paused == core::queue_pause::restarted, "after the crash the queue opens paused");
			open_review(*after, "the window after the crash");
			agent_ready(*after);
			wait([&] { return after->agent_tab()->queue_list() != nullptr && after->agent_tab()->queue_list()->findChild<QPushButton*>(QStringLiteral("agentQueueDiscard")) != nullptr; },
				 "the message sending at the crash may not have been delivered, and offers Discard: " + rows_of(*after).join(QLatin1Char('|')).toStdString());
			auto* const resume = after->agent_tab()->queue_list()->findChild<QPushButton*>(QStringLiteral("agentQueuePauseResume"));
			expect(resume != nullptr, "the queue offers Resume");
			resume->click();
			QApplication::processEvents();
			expect(access::agent_session(*owner, session).Launches == 0U && access::agent_session(*owner, session).Paused != core::queue_pause::none, "Resume is refused while delivery is unknown");
			after->agent_tab()->queue_list()->findChild<QPushButton*>(QStringLiteral("agentQueueDiscard"))->click();
			QApplication::processEvents();
			wait([&] { return !rows_of(*after).contains(QStringLiteral("Sending at the crash")); }, "Discard removes it: " + rows_of(*after).join(QLatin1Char('|')).toStdString());
			after->agent_tab()->queue_list()->findChild<QPushButton*>(QStringLiteral("agentQueuePauseResume"))->click();
			QApplication::processEvents();
			if (!e2e::wait_until_plain([&] { return questions_of(*after->agent_tab()).count(QStringLiteral("Waiting at the crash")) == 1 && !access::agent_session(*owner, session).Sending && access::agent_session(*owner, session).Queued == 0U; },
									   e2e::extended_agent_budget)) {
				throw std::runtime_error{ "Resume sends what waited, once: " + questions_of(*after->agent_tab()).join(QLatin1Char('|')).toStdString() };
			}
			expect(questions_of(*after->agent_tab()).count(QStringLiteral("Sending at the crash")) == 0, "the discarded message was never sent again");
			quit(*after);
			std::cout << "agent-queue-restart:pass\n";
			return EXIT_SUCCESS;
		}
		if (tourCrash) {
			const auto session = access::agent_session_key(*owner);
			const auto typed   = core::parse_session_key(session);
			expect(typed.has_value(), "the window shows the pull request's Agent-tab session");
			const auto tourKey = core::session_key(core::with_kind(*typed, core::queued_session_kind::tour_feedback));
			quit(*first);
			{
				auto crashed = core::agent_queue{ };
				crashed.Entries.push_back({ .Id = 1U, .Input = { .Scope = "tour", .Label = "Feedback sending at the crash", .Text = "Feedback sending at the crash" }, .State = core::queued_state::sending });
				crashed.Entries.push_back({ .Id = 2U, .Input = { .Scope = "tour", .Label = "Feedback waiting at the crash", .Text = "Feedback waiting at the crash" } });
				crashed.LastId   = 2U;
				crashed.Revision = 5U;
				expect(adapters::sqlite::make_agent_queue_store(*dataRoot)->store(tourKey, crashed), "the store holds the tour's feedback as the crash left it");
			}
			start();
			auto* const after = owner->window(owner->windows().front());
			after->resize(1440, 960);
			const auto restored = access::agent_session(*owner, tourKey);
			expect(restored.Paused == core::queue_pause::restarted, "the tour's queue is read back from disk, paused because the app restarted");
			open_review(*after, "the window after the crash");
			agent_ready(*after);
			auto* const panel    = after->tour_editor()->tour_agent();
			const auto  returned = [&] {
				return std::ranges::find_if(panel->session().Runs, [](const core::tour_agent_run& aRun) {
					return aRun.Question.find("Feedback sending at the crash") != std::string::npos && aRun.Question.find("Feedback waiting at the crash") != std::string::npos;
				});
			};
			wait([&] { return returned() != panel->session().Runs.end(); }, "both feedbacks come back to the tour chat: " + panel->transcript_text().toStdString());
			expect(returned()->State == core::tour_agent_run_state::failed, "as a turn that was not sent");
			const auto emptied = access::agent_session(*owner, tourKey);
			expect(emptied.Queued == 0U && !emptied.Sending && emptied.Paused == core::queue_pause::none, "the tour's queue is empty and not paused: the next draft's feedback queues again");
			expect(!panel->session().Waiting, "nothing is shown waiting for a draft");
			for (auto settle = 0; settle < 20; ++settle) {
				QApplication::processEvents();
			}
			expect(access::agent_session(*owner, tourKey).Launches == 0U && std::ranges::count(panel->session().Runs, core::tour_agent_run_state::running, &core::tour_agent_run::State) == 0,
				   "nothing was sent by itself");
			quit(*after);
			std::cout << "agent-queue-tour-feedback-crash:pass\n";
			return EXIT_SUCCESS;
		}
		if (loading) {
			// [[BPR-2181]]. A run that ends while a second window is still
			// loading the same pull request retires, and the queue goes on:
			// two asks queued, the second window opened as the first runs.
			const auto path = QStringLiteral("src/engine/renderer.cpp");
			wait([&] { return first->go_to_file_line(path, 12U, false) && first->active_document_path() == path; }, "the file opens");
			auto* const ask = first->review_ask_button();
			for (auto times = 0; times < 2; ++times) {
				ask->refresh_menu();
				auto* const action = ask->menu()->findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.explain-file_file"));
				expect(action != nullptr, "the Ask menu lists Explain this file");
				action->trigger();
				QApplication::processEvents();
			}
			first->findChild<QAction*>(QStringLiteral("newBprWindowAction"))->trigger();
			wait([&] { return owner->windows().size() == 2U; }, "New Window opens a second window");
			const auto contexts = owner->windows();
			auto*      second   = owner->window(contexts.front() == firstContext ? contexts.back() : contexts.front());
			second->home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
			second->home_open_action()->trigger();
			if (!e2e::wait_until_plain([&] { return first->agent_tab()->turn_count() == 2U && first->agent_tab()->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*owner).Quiescent && !first->agent_tab()->queue_list()->isVisible(); },
									   e2e::extended_agent_budget)) {
				throw std::runtime_error{ "both asks run and the first window's runs retire while the second loads: turns=" + std::to_string(first->agent_tab()->turn_count()) + " quiescent=" + std::to_string(access::agent_invocation_evidence(*owner).Quiescent ? 1 : 0) + " rows=" + rows_of(*first).join(QLatin1Char('|')).toStdString() };
			}
			quit(*first);
			std::cout << "agent-queue-second-window-loading:pass\n";
			return EXIT_SUCCESS;
		}

		// The second window, on the same pull request: the same session.
		first->findChild<QAction*>(QStringLiteral("newBprWindowAction"))->trigger();
		wait([&] { return owner->windows().size() == 2U; }, "New Window opens a second window");
		const auto contexts      = owner->windows();
		const auto secondContext = contexts.front() == firstContext ? contexts.back() : contexts.front();
		auto*      second        = owner->window(secondContext);
		second->resize(1440, 960);
		open_review(*second, "the second window");
		agent_ready(*second);
		const auto idle = [&] { return access::agent_invocation_evidence(*owner).Quiescent && first->agent_tab()->state() != qt::agent_tab_state::working && second->agent_tab()->state() != qt::agent_tab_state::working; };

		// Messages waiting in the first window, the queue paused.
		auto* const ask = first->review_ask_button();
		wait([&] { return ask->isVisible(); }, "the review bar's Ask shows");
		const auto quick = [&](const char* aId, const char* aScope) {
			ask->refresh_menu();
			auto* const action = ask->menu()->findChild<QAction*>(QStringLiteral("quickAsk_%1_%2").arg(QString::fromLatin1(aId), QString::fromLatin1(aScope)));
			expect(action != nullptr, std::string{ "the Ask menu lists " } + aId);
			action->trigger();
			QApplication::processEvents();
		};
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return first->go_to_file_line(path, 12U, false) && first->active_document_path() == path; }, "the file opens");
		// Keep the fixture's first answer pending until the test pauses the queue.
		const auto holdPath = QString::fromStdString((searchFolder / "hold-turns").string());
		{
			QFile hold{ holdPath };
			expect(hold.open(QIODevice::WriteOnly), "the course holds its fixture agent");
		}
		quick("agent-tab.explain-file", "file");
		quick("agent-tab.explain-file", "file");
		wait([&] { return first->agent_tab()->queue_list()->isVisible(); }, "a waiting message shows in the first window's pending list");
		expect(press_in(*first, QStringLiteral("agentQueuePauseResume")), "Pause queue in the first window");
		expect(QFile::remove(holdPath), "the fixture agent can finish the first answer");
		if (!e2e::wait_until_plain(idle, e2e::extended_agent_budget)) {
			throw std::runtime_error{ "the running ask finishes: " + first->agent_panel()->status_text().toStdString() + " / " + rows_of(*first).join(QLatin1Char('|')).toStdString() };
		}
		type_and_return(*first, QStringLiteral("First follow-up"));
		type_and_return(*first, QStringLiteral("Second follow-up"));
		const auto firstRows = rows_of(*first);
		expect(firstRows.contains(QStringLiteral("First follow-up")) && firstRows.contains(QStringLiteral("Second follow-up")), "typed in the first window, queued: " + firstRows.join(QLatin1Char('|')).toStdString());
		wait([&] { return rows_of(*second) == rows_of(*first) && second->agent_tab()->queue_list()->isVisible(); },
			 "the second window shows the same queue, in the same order: " + rows_of(*second).join(QLatin1Char('|')).toStdString());

		// An edit in the second window holds the message in both.
		expect(press_in(*second, QStringLiteral("agentQueueEdit"), QStringLiteral("First follow-up")), "Edit in the second window");
		wait([&] { return rows_of(*first).contains(QStringLiteral("First follow-up (being edited)")); }, "the first window shows it being edited: " + rows_of(*first).join(QLatin1Char('|')).toStdString());
		auto* const heldThere = card_of(*first, QStringLiteral("First follow-up (being edited)"));
		expect(heldThere != nullptr && heldThere->findChild<QPushButton*>(QStringLiteral("agentQueueEdit")) == nullptr && heldThere->findChild<QPushButton*>(QStringLiteral("agentQueueRemove")) == nullptr,
			   "with no Edit or Remove there while it is edited");
		// With `BPR_QUEUE_WINDOWS_RENDER_ROOT` set, each window's Agent tab is
		// saved there (the owner's look; native with BPR_NATIVE_LOOK=1).
		if (const auto root = qEnvironmentVariable("BPR_QUEUE_WINDOWS_RENDER_ROOT"); !root.isEmpty()) {
			QApplication::processEvents();
			static_cast<void>(first->agent_tab()->grab().save(root + QStringLiteral("/first-window.png")));
			static_cast<void>(second->agent_tab()->grab().save(root + QStringLiteral("/second-window.png")));
		}
		auto* const field = second->agent_tab()->queue_list()->findChild<QLineEdit*>(QStringLiteral("agentQueueEditField"));
		expect(field != nullptr && field->text() == QStringLiteral("First follow-up"), "edited in place in the second window");
		field->setText(QStringLiteral("First follow-up, edited"));
		expect(press_in(*second, QStringLiteral("agentQueueSave")), "Save in the second window");
		wait([&] { return rows_of(*first).contains(QStringLiteral("First follow-up, edited")) && rows_of(*second) == rows_of(*first); }, "the edit shows in both");

		// A removal in the first window crossing the second's press on the
		// same message: the later press is refused and says so.
		auto* const removedCard = card_of(*second, QStringLiteral("Second follow-up"));
		auto* const firstCard   = card_of(*first, QStringLiteral("Second follow-up"));
		expect(removedCard != nullptr && firstCard != nullptr, "both windows show the message");
		const auto stale = QPointer<QPushButton>{ removedCard->findChild<QPushButton*>(QStringLiteral("agentQueueRemove")) };
		firstCard->findChild<QPushButton*>(QStringLiteral("agentQueueRemove"))->click();
		expect(!stale.isNull(), "the second window's press is still on its way");
		stale->click();
		QApplication::processEvents();
		expect(second->agent_panel()->status_text().contains(QStringLiteral("Another window changed this queue first")),
			   "the press that crossed the first window's removal is refused and says so: " + second->agent_panel()->status_text().toStdString());
		wait([&] { return !rows_of(*second).contains(QStringLiteral("Second follow-up")) && rows_of(*first) == rows_of(*second); }, "removed in both windows");

		// Typed in the second window, it joins the one queue.
		type_and_return(*second, QStringLiteral("Third follow-up"));
		wait([&] { return rows_of(*first).contains(QStringLiteral("Third follow-up")) && rows_of(*first) == rows_of(*second); }, "typed in the second window, it shows in the first");

		// Resume: the session's runtime runs the next message. Closing the
		// first window while it runs does not touch that turn: the runtime is
		// the profile's ([[BPR-2181]] AC09). Q10: the close neither pauses nor
		// hands over the queue, and what is left runs in order, each once,
		// with no Resume.
		const auto before = second->agent_tab()->turn_count();
		expect(press_in(*second, QStringLiteral("agentQueuePauseResume")), "Resume in the second window");
		wait([&] { return first->agent_tab()->state() == qt::agent_tab_state::working || second->agent_tab()->state() == qt::agent_tab_state::working; }, "a turn starts");
		expect(first->agent_tab()->state() == qt::agent_tab_state::working, "the first free window showing the session runs it");
		const auto waiting = rows_of(*second);
		first->close();
		auto       everPaused = false;
		const auto toggle     = [&] { return second->agent_tab()->queue_list()->findChild<QPushButton*>(QStringLiteral("agentQueuePauseResume")); };
		const auto watch      = [&] { everPaused = everPaused || (toggle() != nullptr && toggle()->text() == QStringLiteral("Resume queue")); };
		const auto closed     = e2e::wait_until_plain([&] {
			watch();
			return owner->window(firstContext) == nullptr && owner->windows().size() == 1U;
		},
													  e2e::extended_agent_budget);
		expect(closed, "the first window closes");
		if (!e2e::wait_until_plain([&] {
				watch();
				const auto asked = questions_of(*second->agent_tab());
				// The first window is gone: only the second's state is read.
				return asked.contains(QStringLiteral("Third follow-up")) && second->agent_tab()->state() != qt::agent_tab_state::working && rows_of(*second).isEmpty();
			},
								   e2e::extended_agent_budget)) {
			throw std::runtime_error{ "the second window runs what is left: asked=" + questions_of(*second->agent_tab()).join(QLatin1Char('|')).toStdString() + " rows=" + rows_of(*second).join(QLatin1Char('|')).toStdString() + " status="
									  + second->agent_panel()->status_text().toStdString() + " toggle=" + (toggle() != nullptr ? toggle()->text().toStdString() : std::string{ "none" }) };
		}
		expect(!everPaused, "closing the window never paused the queue");
		const auto asked = questions_of(*second->agent_tab());
		auto       order = std::vector<qsizetype>{ };
		// The typed messages, which ask as they read (a quick ask's label is
		// not its question).
		for (const auto& label : waiting) {
			if (label.startsWith(QStringLiteral("Explain"))) {
				continue;
			}
			expect(asked.count(label) == 1, "each message ran once, the one running at the close too: " + asked.join(QLatin1Char('|')).toStdString() + " / waiting " + waiting.join(QLatin1Char('|')).toStdString());
			order.push_back(asked.indexOf(label));
		}
		expect(std::ranges::is_sorted(order) && asked.back() == QStringLiteral("Third follow-up") && static_cast<std::size_t>(asked.size()) > before, "in the order the list showed: " + asked.join(QLatin1Char('|')).toStdString());

		quit(*second);
	} catch (const std::exception& aError) {
		std::cerr << "agent-queue-windows:fail: " << aError.what() << '\n';
		return EXIT_FAILURE;
	}
	std::cout << "agent-queue-windows:pass\n";
	return EXIT_SUCCESS;
}
