/// \file
/// \brief RFC-BPR-0128 ([[BPR-2163]], [[BPR-2164]]). The Ask menus through
/// the shipped window, on returning-review-v1 served at its next push, with
/// the scoped writing stub as the installed Claude.
///
/// Courses, by the first argument:
///
/// - `menu STUB`: the review bar, the open file's header and the open
///   thread's reply row each have one Ask; a thread's and a file tab's
///   right-click menus have one Ask item and none of the agent items they
///   replace (AC01); a menu lists only quick asks, each naming what a press
///   reads, a scope's subject absent is not listed and one that cannot run is
///   greyed with its reason as tooltip, status tip and accessible text, and
///   building every menu launches nothing (AC02, AC03); Custom question…
///   attaches the thread and sends nothing, Edit Quick Asks… opens Settings
///   at the suggested prompts (AC02); in Settings, Quick ask off on a
///   built-in and on with File and Pull request on an own prompt are saved
///   with the prompt in the profile's store and change the menus, and a
///   scope narrower than a prompt's subject cannot be set (AC04); What
///   changed pressed from the review bar sends the action's own prompt as
///   before (AC04); greyed rows are reachable by keyboard and every control
///   is named (AC08); nothing reaches GitHub.
/// - `addressed STUB`: Was this addressed? on a thread I did not comment in,
///   on each thread of a file and every thread of the pull request, one ask
///   each press (RFC-BPR-0128 as amended), listed for every thread
///   of the pull request; each claim is a chip in its thread's header that
///   changes no card's height, its press shows the thread's lines when it
///   started and now; a thread commits have not followed is greyed with its
///   reason; nothing is resolved, posted or published (AC05).
/// - `report STUB`: What changed from the review bar's Ask answers as a report
///   card in the Agent tab, its material listing the commits (AC07's listing
///   in the request); Open report opens it as a tab in the files' strip with
///   its outline, the comparison's file controls off and the Agent tab still
///   usable; a code reference opens the file's tab at the line, and the
///   report's tab returns to the report (AC06).
/// - `setup-review STUB`: [[BPR-2245]]. Review this pull request pressed
///   while the agent's setup waits on Check is queued; Check passes, the
///   review runs once, off the pending list while its turn runs, and its
///   turn ends answered, not stopped and not drawn
///   as an error, and once it retires the footer's Stop all is hidden and
///   nothing is queued; restarted, the pull request opens with nothing
///   queued and no Stop all.
/// - `menu|addressed|report STUB --attended`: the same, then the window is left on
///   Review with a thread open for a person to look; closing it ends the
///   course.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/core/agent_queue.hpp"
#include "bpr/core/profile_data_reset.hpp"
#include "bpr/core/prompt_pack.hpp"
#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/profile_data_reset_views.hpp"
#include "bpr/qt/prompts_page.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <sqlite3.h>

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

	/// With `BPR_ASK_RENDER_ROOT` set, a widget is saved there as a PNG (the
	/// owner's looks; native with BPR_NATIVE_LOOK=1 on macOS).
	void render(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_ASK_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QApplication::processEvents();
		static_cast<void>(aWidget.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")));
	}

	/// The same, with `aShown` scrolled into view first.
	void render_showing(QWidget& aWidget, QWidget& aShown, const QString& aName)
	{
		for (auto* parent = aShown.parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			if (auto* const scroll = qobject_cast<QScrollArea*>(parent); scroll != nullptr) {
				scroll->ensureWidgetVisible(&aShown, 0, 0);
				break;
			}
		}
		render(aWidget, aName);
	}

	void shutdown(app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		wait([&] { return aApplication.status().ShutdownComplete; }, "shutdown");
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

	[[nodiscard]] auto row(const QMenu& aMenu, const QString& aId, const QString& aScope) -> QAction*
	{
		return aMenu.findChild<QAction*>(QStringLiteral("quickAsk_%1_%2").arg(aId, aScope));
	}

	[[nodiscard]] auto rows(const QMenu& aMenu) -> QStringList
	{
		auto names = QStringList{ };
		for (auto* const action : aMenu.actions()) {
			if (action->objectName().startsWith(QStringLiteral("quickAsk_"))) {
				names << action->objectName();
			}
		}
		return names;
	}

	[[nodiscard]] auto sections(const QMenu& aMenu) -> QStringList
	{
		auto titles = QStringList{ };
		for (auto* const action : aMenu.actions()) {
			if (action->isSeparator() && !action->text().isEmpty()) {
				titles << action->text();
			}
		}
		return titles;
	}

	[[nodiscard]] auto answer_of(const qt::agent_tab_view& aTab) -> std::string
	{
		return aTab.turn_count() == 0U ? std::string{ } : aTab.turn_answer(aTab.turn_count() - 1U).toStdString();
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	const auto   course = argc > 1 ? std::string_view{ argv[1] } : std::string_view{ };
	if (argc < 3 || (course != "menu" && course != "addressed" && course != "report" && course != "queue" && course != "macros" && course != "tour-queue" && course != "setup-review")) {
		std::cerr << "usage: ask_menu_public_path menu|addressed|report|queue|macros|tour-queue|setup-review STUB [--attended]\n";
		return EXIT_FAILURE;
	}
	const auto    stub     = std::filesystem::path{ argv[2] };
	const auto    attended = argc > 3 && std::string_view{ argv[3] } == "--attended";
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	// The tour course's Claude drafts slowly (the stub's
	// `attended-drafting-claude`), so feedback can be typed while it runs.
	const auto searchFolder = std::filesystem::path{ data.path().toStdString() } / (course == "tour-queue" ? "attended-drafting-claude" : "stub-claude-search");
	auto       search       = e2e::stub_claude_search(searchFolder, stub);
	if (!search) {
		std::cerr << "stub claude search refused\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.EpochPhase            = app::detail::github_epoch_fixture_phase::reuse;
		options.ClaudeAgentExecutable = course == "tour-queue" ? searchFolder / ("claude" + stub.extension().string()) : stub;
		options.ClaudeSearch          = search;
		// [[BPR-2179]] look prompts-v002: the queue course starts with the
		// look's seeded prompts, whose conditions nest.
		options.PromptExamplesLook = course == "queue";
		auto terminal              = false;
		auto startup               = access::start(app::detail::github_public_path_fixture::returning_review_v1,
												   std::move(options), [&](auto aResult) { terminal = true; if (aResult) { application = std::move(*aResult); } });
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		const auto  launchesAt = [&] { return access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations; };
		auto* const tab        = window.agent_tab();
		auto* const cards      = window.conversation_panel()->cards();
		const auto  path       = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return window.what_changed_action()->isEnabled(); }, "the returning pull request's commits are read");
		if (course == "setup-review") {
			// [[BPR-2245]] (owner, 2026-11-17): "after I clicked check it did
			// set up the workspace and the agent and it looks like it did
			// actually run the review and gave me a report back ... but it
			// shows up as red and says canceled", and "stop all is showing up
			// in the status bar after that even though no agent stuff is
			// running."
			wait([&] { return window.go_to_file_line(path, 12U, false) && window.active_document_path() == path; }, "the file opens");
			auto* const ask = window.review_ask_button();
			wait([&] { return ask->isVisible(); }, "the review bar's Ask shows");
			window.open_agent_question();
			auto* const selector = window.agent_panel()->profile_selector();
			if (const auto index = selector->findData(QVariant::fromValue<qulonglong>(core::claude_stream_json_profile_id.value())); index >= 0) {
				selector->setCurrentIndex(index);
			}
			expect(tab->setup_readiness() != qt::agent_setup_readiness::ready, "the setup is unfinished");
			ask->refresh_menu();
			auto* const review = row(*ask->menu(), QStringLiteral("agent-tab.review-pull-request"), QStringLiteral("pull-request"));
			expect(review != nullptr, "Review this pull request is listed before setup");
			review->trigger();
			QApplication::processEvents();
			expect(tab->queue_note()->text().contains(QStringLiteral("waiting for the agent's setup")) && launchesAt() == 0U && tab->turn_count() == 0U,
				   "before setup it is queued and nothing runs: " + tab->queue_note()->text().toStdString());
			agent_ready(window);
			auto* const footer = window.agent_footer_widget();
			const auto  idle   = [&] {
				return tab->turn_count() >= 1U && tab->turn_outcome(0U).has_value() && tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent
					&& tab->queue_note()->text().isEmpty();
			};
			const auto said = [&] {
				return "turns=" + std::to_string(tab->turn_count()) + " outcome=" + (tab->turn_count() > 0U && tab->turn_outcome(0U) ? std::to_string(static_cast<int>(*tab->turn_outcome(0U))) : std::string{ "-" })
					+ " footer=" + (tab->turn_count() > 0U ? tab->turn_footer(0U).toStdString() : std::string{ }) + " queue=" + tab->queue_note()->text().toStdString() + " launches=" + std::to_string(launchesAt())
					+ " stopAll=" + std::to_string(footer != nullptr && !footer->stop_all_button()->isHidden() ? 1 : 0) + " state=" + std::to_string(static_cast<int>(tab->state()));
			};
			// The owner, 2026-11-24: the queued review "stays in the queue as
			// 'sending' even during the entire time the prompt is running".
			// Each look while its turn shows running: no pending row.
			auto       runningLooks = 0;
			auto       stillPending = std::string{ };
			const auto pendingRows  = [tab] { return tab->queue_list() != nullptr ? tab->queue_list()->findChildren<QWidget*>(QStringLiteral("agentQueueRow")).size() : qsizetype{ 0 }; };
			const auto watched      = [&] {
				if (tab->state() == qt::agent_tab_state::working && tab->turn_count() >= 1U) {
					++runningLooks;
					if (pendingRows() != 0 && stillPending.empty()) {
						stillPending = said();
					}
				}
				return idle();
			};
			if (!e2e::wait_until_plain(watched, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "once set up, the queued review runs: " + said() };
			}
			expect(runningLooks > 0, "the review's turn was seen running");
			expect(stillPending.empty(), "while the review's turn runs it is not also pending in the queue: " + stillPending);
			// Settled: nothing more starts, and the footer has caught up.
			{
				auto settle = QElapsedTimer{ };
				settle.start();
				while (settle.elapsed() < 1500) {
					QApplication::processEvents(QEventLoop::AllEvents, 50);
				}
			}
			std::cout << "setup-review: " << said() << '\n';
			expect(tab->turn_count() == 1U, "the review ran once: " + said());
			expect(tab->turn_outcome(0U) == std::optional{ core::agent_turn_outcome::answered } && !tab->turn_footer(0U).startsWith(QStringLiteral("Stopped")),
				   "the review that answered says it answered: " + said());
			expect(tab->turn_name(0U) == QStringLiteral("Review this pull request"), "the turn is the review: " + tab->turn_name(0U).toStdString());
			expect(footer != nullptr && footer->stop_all_button()->isHidden(), "with nothing running or waiting, Stop all is hidden: " + said());
			const auto rowsLeft = [](const qt::agent_tab_view& aTab) { return aTab.queue_list() != nullptr ? aTab.queue_list()->findChildren<QWidget*>(QStringLiteral("agentQueueRow")).size() : qsizetype{ 0 }; };
			expect(rowsLeft(*tab) == 0 && tab->queue_note()->text().isEmpty(), "nothing is left in the queue: " + said());
			shutdown(*application);
			application.reset();
			// The owner, 2026-11-17: "When I opened a different PR there was
			// a review this pull request in the queue". Restarted, the pull
			// request opens with nothing queued and no Stop all.
			auto again                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
			again.EpochPhase            = app::detail::github_epoch_fixture_phase::reuse;
			again.ClaudeAgentExecutable = stub;
			again.ClaudeSearch          = search;
			auto restarted              = false;
			auto second                 = access::start(app::detail::github_public_path_fixture::returning_review_v1, std::move(again), [&](auto aResult) {
				restarted = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
			wait([&] { return restarted && second && second->is_quiescent(); }, "restart terminal");
			expect(application != nullptr, "the restart succeeds");
			application->show();
			auto& reopened = application->window();
			reopened.resize(1440, 960);
			reopened.github_connect_action()->trigger();
			wait([&] { return reopened.github_open_action()->isEnabled(); }, "connected again");
			reopened.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
			reopened.home_open_action()->trigger();
			wait([&] { return reopened.what_changed_action()->isEnabled(); }, "the pull request opens again");
			reopened.open_agent_question();
			{
				auto settle = QElapsedTimer{ };
				settle.start();
				while (settle.elapsed() < 1500) {
					QApplication::processEvents(QEventLoop::AllEvents, 50);
				}
			}
			auto* const kept = reopened.agent_tab();
			expect(rowsLeft(*kept) == 0 && kept->queue_note()->text().isEmpty(), "restarted, nothing is queued: " + kept->queue_note()->text().toStdString());
			expect(reopened.agent_footer_widget() != nullptr && reopened.agent_footer_widget()->stop_all_button()->isHidden(), "restarted, Stop all is hidden");
			shutdown(*application);
			std::cout << "ask-menu-setup-review:pass\n";
			return EXIT_SUCCESS;
		}
		if (course == "queue") {
			wait([&] {
				const auto& library = window.prompts()->library();
				const auto  seeded  = std::ranges::find(library, std::string{ "own.example-look-again" }, &core::prompt_macro::Id);
				return seeded != library.end() && seeded->Body.Groups.size() == 2U && seeded->Body.Groups.front().Match == core::prompt_match::none;
			},
				 "--attended-prompt-examples seeds a prompt whose conditions nest");
			// Owner, 2026-11-14: "a plain click queues it. We might not have
			// set up the agent yet or we might be running other things
			// already." Before setup: queued, the Agent tab offers setup, and
			// nothing runs; once set up it runs. Busy: the next waits its turn.
			wait([&] { return window.go_to_file_line(path, 12U, false) && window.active_document_path() == path; }, "the file opens");
			auto* const ask = window.review_ask_button();
			wait([&] { return ask->isVisible() && window.what_changed_action()->isEnabled(); }, "the review bar's Ask shows, and the commits since the review are read");
			// The installed Claude (the stub) is the agent this session asks.
			window.open_agent_question();
			auto* const selector = window.agent_panel()->profile_selector();
			if (const auto index = selector->findData(QVariant::fromValue<qulonglong>(core::claude_stream_json_profile_id.value())); index >= 0) {
				selector->setCurrentIndex(index);
			}
			ask->refresh_menu();
			auto* const changed = row(*ask->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"));
			expect(changed != nullptr, "What changed is listed before setup");
			changed->trigger();
			QApplication::processEvents();
			expect(tab->queue_note()->text().contains(QStringLiteral("waiting for the agent's setup")) && launchesAt() == 0U,
				   "before setup it is queued and nothing runs: " + tab->queue_note()->text().toStdString());
			agent_ready(window);
			wait([&] { return answer_of(*tab).find("RETURNING_ECHO") != std::string::npos && access::agent_invocation_evidence(*application).Quiescent && tab->state() != qt::agent_tab_state::working; },
				 "once set up, the queued ask runs");
			expect(tab->queue_note()->text().isEmpty(), "and leaves the queue");
			const auto turns = tab->turn_count();
			ask->refresh_menu();
			row(*ask->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"))->trigger();
			ask->refresh_menu();
			row(*ask->menu(), QStringLiteral("agent-tab.explain-file"), QStringLiteral("file"))->trigger();
			QApplication::processEvents();
			expect(tab->queue_note()->text().contains(QStringLiteral("Explain this file")) || tab->turn_count() > turns, "while one runs, the next waits its turn: " + tab->queue_note()->text().toStdString());
			wait([&] { return tab->turn_count() == turns + 2U && tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent && tab->queue_note()->text().isEmpty(); },
				 "both run, one after the other");
			expect(tab->turn_returning(turns) == core::returning_operation::latest_upload && tab->turn_explain(turns + 1U) == path, "in the order they were queued");

			// [[BPR-2181]] (AC13, Prompts review B1). The seeded Ready to look
			// again?, kept to its one condition (pushes since my review), is
			// offered as a chip and on the Ask menu on the same facts, and
			// the agent receives it pressed either way: sent at once, and
			// from the queue with the expansion decided at the press, which
			// an edit made while it waits does not change.
			{
				auto* const page = window.prompts();
				page->select("own.example-look-again");
				page->edit_selected([](core::prompt_macro& aMacro) { aMacro.Body.Groups.clear(); });
				const auto lookAgain = [&]() -> QPushButton* {
					for (auto* const chip : tab->suggestion_buttons()) {
						if (chip->isVisibleTo(tab) && chip->text() == QStringLiteral("Ready to look again?")) {
							return chip;
						}
					}
					return nullptr;
				};
				window.open_agent_question();
				const auto shown = [&] {
					auto texts = std::string{ };
					for (auto* const chip : tab->suggestion_buttons()) {
						texts += chip->text().toStdString() + (chip->isVisibleTo(tab) ? "" : "(hidden)") + "|";
					}
					const auto& library = page->library();
					const auto  seeded  = std::ranges::find(library, std::string{ "own.example-look-again" }, &core::prompt_macro::Id);
					ask->refresh_menu();
					auto* const listed = row(*ask->menu(), QStringLiteral("own.example-look-again"), QStringLiteral("pull-request"));
					return texts + " groups=" + (seeded == library.end() ? std::string{ "none" } : std::to_string(seeded->Body.Groups.size()) + " conditions=" + std::to_string(seeded->Body.Conditions.size()) + " quick=" + std::to_string(seeded->Body.QuickAsk ? 1 : 0) + " on=" + std::to_string(seeded->On ? 1 : 0))
						+ " row=" + (listed == nullptr ? std::string{ "none" } : std::to_string(listed->isEnabled() ? 1 : 0) + ":" + listed->toolTip().toStdString());
				};
				if (!e2e::wait_until_plain([&] { return lookAgain() != nullptr && lookAgain()->isEnabled(); }, e2e::extended_agent_budget)) {
					throw std::runtime_error{ "pushes since my review: the Agent tab offers Ready to look again?: " + shown() };
				}
				const auto before = tab->turn_count();
				lookAgain()->click();
				if (!e2e::wait_until_plain([&] { return tab->turn_count() == before + 1U && answer_of(*tab).find("LOOK_AGAIN_ECHO") != std::string::npos && tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent; },
										   e2e::extended_agent_budget)) {
					throw std::runtime_error{ "pressed as a chip, the agent receives it: turns=" + std::to_string(tab->turn_count() - before) + " question=" + (tab->turn_count() > before ? tab->turn_question(before).toStdString() : std::string{ })
											  + " answer=" + answer_of(*tab) + " status=" + window.agent_panel()->status_text().toStdString() };
				}
				expect(tab->turn_question(before).contains(QStringLiteral("changed enough since my review that I should look again?")), "as expanded on the pull request's facts: " + tab->turn_question(before).toStdString());
				// Busy with What changed, a press on the menu row queues it.
				ask->refresh_menu();
				row(*ask->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"))->trigger();
				ask->refresh_menu();
				auto* const queued = row(*ask->menu(), QStringLiteral("own.example-look-again"), QStringLiteral("pull-request"));
				expect(queued != nullptr && queued->isEnabled(), "the review bar's Ask lists it on the same facts");
				queued->trigger();
				QApplication::processEvents();
				expect(tab->queue_note()->text().contains(QStringLiteral("Ready to look again?")), "while What changed runs it waits its turn: " + tab->queue_note()->text().toStdString());
				// Edited while it waits: the release sends what was decided.
				page->select("own.example-look-again");
				page->edit_selected([](core::prompt_macro& aMacro) { aMacro.Body.Template = "EDITED_AFTER_THE_PRESS {pr.title}"; });
				wait([&] { return tab->turn_count() == before + 3U && tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent && tab->queue_note()->text().isEmpty(); },
					 "from the queue it runs after What changed");
				expect(answer_of(*tab).find("LOOK_AGAIN_ECHO") != std::string::npos && !tab->turn_question(before + 2U).contains(QStringLiteral("EDITED_AFTER_THE_PRESS")),
					   "the agent receives the expansion decided at the press: " + tab->turn_question(before + 2U).toStdString());
				page->select("own.example-look-again");
				page->edit_selected([](core::prompt_macro& aMacro) { aMacro.Body.Template = "Has {pr.title} changed enough since my review that I should look again?"; });
			}

			// RFC-BPR-0126 ([[BPR-2151]]): the pending list and typed
			// follow-ups. With a message waiting and the queue paused, typed
			// messages queue (Send says Queue), are edited in place (held,
			// never sent), move, are removed, and run in order on Resume.
			const auto list   = [&] { return tab->queue_list(); };
			const auto rowsOf = [&] {
				auto labels = QStringList{ };
				for (auto* const label : list()->findChildren<QLabel*>(QStringLiteral("agentQueueLabel"))) {
					labels << label->text();
				}
				return labels;
			};
			const auto pressIn = [&](const QString& aName, const QString& aLabel = { }) {
				for (auto* const row : list()->findChildren<QWidget*>(QStringLiteral("agentQueueRow"))) {
					auto* const label = row->findChild<QLabel*>(QStringLiteral("agentQueueLabel"));
					if (aLabel.isEmpty() || (label != nullptr && label->text() == aLabel)) {
						if (auto* const button = row->findChild<QPushButton*>(aName); button != nullptr) {
							button->click();
							QApplication::processEvents();
							return true;
						}
					}
				}
				if (auto* const button = list()->findChild<QPushButton*>(aName); button != nullptr && aLabel.isEmpty()) {
					button->click();
					QApplication::processEvents();
					return true;
				}
				return false;
			};
			const auto settledTurns = tab->turn_count();
			ask->refresh_menu();
			row(*ask->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"))->trigger();
			// [[BPR-2215]]. What changed runs before the next is queued and the
			// queue paused: on Windows the fixture agent starts later, and a
			// pause that came first held What changed itself.
			wait([&] { return tab->state() == qt::agent_tab_state::working; }, "What changed runs");
			ask->refresh_menu();
			row(*ask->menu(), QStringLiteral("agent-tab.explain-file"), QStringLiteral("file"))->trigger();
			QApplication::processEvents();
			wait([&] { return list()->isVisible(); }, "a waiting message shows in the pending list");
			expect(pressIn(QStringLiteral("agentQueuePauseResume")), "Pause queue");
			auto* const composer      = tab->question_editor();
			const auto  typeAndReturn = [&](const QString& aText, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) {
				composer->setPlainText(aText);
				QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, aModifiers };
				QApplication::sendEvent(composer, &press);
				QApplication::processEvents();
			};
			wait([&] { return tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent; }, "the running ask finishes");
			composer->setPlainText(QStringLiteral("A follow-up"));
			QApplication::processEvents();
			expect(tab->ask_button()->text() == QStringLiteral("Queue"), "with messages waiting, Send says Queue: " + tab->ask_button()->text().toStdString());
			composer->clear();
			typeAndReturn(QStringLiteral("First follow-up"));
			typeAndReturn(QStringLiteral("Second follow-up"));
			expect(composer->toPlainText().isEmpty() && rowsOf().contains(QStringLiteral("First follow-up")) && rowsOf().contains(QStringLiteral("Second follow-up")),
				   "typed messages are queued, the field cleared: " + rowsOf().join(QLatin1Char('|')).toStdString() + " composer: [" + composer->toPlainText().toStdString() + "] status: ["
					   + window.agent_panel()->status_text().toStdString() + "] last refusal: [" + window.agent_panel()->last_refusal().toStdString() + "]");
			typeAndReturn(QStringLiteral("Third, steered"), Qt::ControlModifier);
			// [[BPR-2153]]: no turn runs, so there is nothing to steer; the
			// message is kept, goes next, and the panel says why (Q17).
			// [[BPR-2152]]: said once its claim is stored.
			if (!e2e::wait_until_plain([&] { return window.agent_panel()->status_text().contains(QStringLiteral("goes next")); }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "the panel says what became of the steered message: [" + window.agent_panel()->status_text().toStdString() + "] last refusal: ["
										  + window.agent_panel()->last_refusal().toStdString() + "] queued: "
										  + rowsOf().join(QLatin1Char('|')).toStdString() };
			}
			expect(rowsOf().contains(QStringLiteral("Third, steered")) && window.agent_panel()->status_text().contains(QStringLiteral("goes next")),
				   "Cmd+Return asks to steer; with no steering it is queued and says so (Q17): " + window.agent_panel()->status_text().toStdString());
			render_showing(window, *list(), QStringLiteral("queue-pending-list"));
			expect(pressIn(QStringLiteral("agentQueueEdit"), QStringLiteral("First follow-up")), "Edit a typed message");
			render_showing(window, *list(), QStringLiteral("queue-edit-hold"));
			auto* const field = list()->findChild<QLineEdit*>(QStringLiteral("agentQueueEditField"));
			expect(field != nullptr && field->text() == QStringLiteral("First follow-up"), "edited in place");
			field->setText(QStringLiteral("First follow-up, edited"));
			expect(pressIn(QStringLiteral("agentQueueSave")), "Save");
			expect(rowsOf().contains(QStringLiteral("First follow-up, edited")), "the edit is kept: " + rowsOf().join(QLatin1Char('|')).toStdString());
			expect(pressIn(QStringLiteral("agentQueueRemove"), QStringLiteral("Third, steered")), "Remove");
			expect(!rowsOf().contains(QStringLiteral("Third, steered")), "removed");
			// Look prompts-v001: a card is dragged to reorder; dropped above
			// the second waiting card, the third goes there.
			{
				auto moved = false;
				for (auto* const card : list()->findChildren<QWidget*>(QStringLiteral("agentQueueRow"))) {
					auto* const label = card->findChild<QLabel*>(QStringLiteral("agentQueueLabel"));
					if (label != nullptr && label->text() == QStringLiteral("Second follow-up")) {
						tab->drop_queued(card->property("agentQueueId").toULongLong(), 1);
						moved = true;
						break;
					}
				}
				expect(moved, "Move up by a drag");
				wait([&] { return rowsOf().indexOf(QStringLiteral("Second follow-up")) < rowsOf().indexOf(QStringLiteral("First follow-up, edited")); }, "the drop lands");
			}
			const auto order = rowsOf();
			expect(order.indexOf(QStringLiteral("Second follow-up")) < order.indexOf(QStringLiteral("First follow-up, edited")), "moved up: " + order.join(QLatin1Char('|')).toStdString());
			expect(launchesAt() > 0U && tab->turn_count() == settledTurns + 1U, "nothing more ran while paused");
			expect(pressIn(QStringLiteral("agentQueuePauseResume")), "Resume queue");
			wait([&] { return tab->turn_count() == settledTurns + 4U && tab->state() != qt::agent_tab_state::working && access::agent_invocation_evidence(*application).Quiescent && !list()->isVisible(); },
				 "on Resume every waiting message runs, in order");
			expect(tab->turn_explain(settledTurns + 1U) == path && tab->turn_question(settledTurns + 2U) == QStringLiteral("Second follow-up") && tab->turn_question(settledTurns + 3U) == QStringLiteral("First follow-up, edited"),
				   "in the order the list shows: " + tab->turn_question(settledTurns + 2U).toStdString() + " then " + tab->turn_question(settledTurns + 3U).toStdString());
			// The Follow-up behaviour setting, for the owner's look.
			if (!qEnvironmentVariable("BPR_ASK_RENDER_ROOT").isEmpty()) {
				window.open_settings(qt::settings_page::agents_and_context);
				window.settings()->resize(1100, 860);
				if (auto* const behaviour = window.settings()->findChild<QWidget*>(QStringLiteral("settingsFollowUp")); behaviour != nullptr) {
					if (auto* const scroll = qobject_cast<QScrollArea*>(window.settings()->pages()->currentWidget()); scroll != nullptr) {
						scroll->ensureWidgetVisible(behaviour);
					}
				}
				render(*window.settings(), QStringLiteral("queue-follow-up-setting"));
				window.settings()->hide();
			}
			shutdown(*application);
			std::cout << "ask-menu-queue:pass\n";
			return EXIT_SUCCESS;
		}
		if (course == "tour-queue") {
			// RFC-BPR-0126 AC03, Q1 ([[BPR-2151]]): whole-tour feedback typed
			// while Draft the tour runs is the whole-tour conversation's first
			// queued message. More feedback joins it; nothing is sent while
			// the draft runs; once the draft has really finished (answer
			// handled, proposal made, run retired) it goes as one turn, with
			// the draft's answer as the Ask more seed and the proposal in the
			// request, and revises that proposal.
			agent_ready(window);
			window.open_review_tour();
			auto* const editor = window.tour_editor();
			auto* const panel  = editor->tour_agent();
			auto* const chat   = editor->side_chat();
			wait([&] { return editor->shown().Draft && !editor->shown().Loading && panel->draft_button()->isVisible() && panel->draft_button()->isEnabled(); }, "Draft the tour is offered");
			panel->draft_button()->click();
			const auto drafting = [&] {
				const auto& runs = panel->session().Runs;
				return !runs.empty() && runs.back().Request == core::tour_agent_request::draft_tour && runs.back().State == core::tour_agent_run_state::running;
			};
			wait(drafting, "the draft runs");
			panel->chat_button()->click();
			QApplication::processEvents();
			const auto runsWhileDrafting = panel->session().Runs.size();
			for (const auto* const feedback : { "Fewer stops, please.", "Start at the header that declares the change." }) {
				chat->composer()->setPlainText(QString::fromLatin1(feedback));
				wait([&] { return chat->send_button()->isEnabled(); }, "feedback can be sent while it drafts");
				chat->send_button()->click();
				QApplication::processEvents();
			}
			expect(drafting() && panel->session().Runs.size() == runsWhileDrafting, "nothing is sent while the draft runs");
			expect(chat->waiting_feedback() != nullptr && panel->session().Waiting && panel->session().Waiting->find("Fewer stops") != std::string::npos && panel->session().Waiting->find("header") != std::string::npos,
				   "the feedback waits as one message, the second joined to the first");
			// [[BPR-2368]]. While the feedback's turn runs, the tour's queue
			// still holds it but no longer lists it as pending.
			const auto tourKey = [&] {
				const auto session = core::parse_session_key(access::agent_session_key(*application));
				return session ? core::session_key(core::with_kind(*session, core::queued_session_kind::tour_feedback)) : std::string{ };
			}();
			expect(!tourKey.empty(), "the tour's queue is named");
			auto       feedbackLooks = 0;
			auto       heldLooks     = 0;
			auto       stillListed   = std::string{ };
			const auto feedbackRuns  = [&] {
				const auto& runs = panel->session().Runs;
				if (runs.size() > runsWhileDrafting && runs.back().Request == core::tour_agent_request::ask && runs.back().State == core::tour_agent_run_state::running) {
					++feedbackLooks;
					const auto now = access::agent_session(*application, tourKey);
					heldLooks += now.Sending ? 1 : 0;
					if (now.Listed != 0U && stillListed.empty()) {
						stillListed = "listed=" + std::to_string(now.Listed) + " sending=" + std::to_string(now.Sending ? 1 : 0);
					}
				}
				return runs.size() > runsWhileDrafting && runs.back().Request != core::tour_agent_request::draft_tour && runs.back().State != core::tour_agent_run_state::running && access::agent_invocation_evidence(*application).Quiescent;
			};
			wait(feedbackRuns, "after the draft, the feedback is the tour chat's next turn: " + panel->transcript_text().toStdString());
			expect(feedbackLooks > 0 && heldLooks > 0, "the feedback's turn was seen running, its message still held by the queue");
			expect(stillListed.empty(), "while the feedback's turn runs it is not also pending in the tour's queue: " + stillListed);
			wait([&] { return access::agent_session(*application, tourKey).Listed == 0U && !access::agent_session(*application, tourKey).Sending; }, "its run's retirement settles it");
			const auto& runs  = panel->session().Runs;
			const auto  draft = std::ranges::find(runs, core::tour_agent_request::draft_tour, &core::tour_agent_run::Request);
			expect(draft != runs.end() && draft->State == core::tour_agent_run_state::answered, "the draft answered first");
			const auto& turn = runs.back();
			expect(turn.Question.find("Fewer stops") != std::string::npos && turn.Question.find("header") != std::string::npos && turn.State == core::tour_agent_run_state::answered,
				   "one turn, with both feedbacks as written: " + turn.Question + " / " + turn.Reply);
			expect(turn.Reply.find("I revised the draft before you accept it") != std::string::npos, "its request carried the draft's proposal, which it revised: " + turn.Reply);
			expect(turn.Reply.find("I started from the draft's own answer") != std::string::npos, "and the draft's answer as the Ask more seed: " + turn.Reply);
			expect(!panel->session().Waiting && chat->waiting_feedback() == nullptr, "nothing waits any more");
			expect(std::ranges::count(runs, core::tour_agent_run_state::failed, &core::tour_agent_run::State) == 0, "no turn was refused for a run still finishing");
			shutdown(*application);
			std::cout << "ask-menu-tour-queue:pass\n";
			return EXIT_SUCCESS;
		}
		agent_ready(window);
		wait([&] { return window.go_to_file_line(path, 12U, false) && window.active_document_path() == path && !cards->cards().empty(); }, "the file opens beside its threads");
		// Owner, look report-keeps-v001: the Agent tab, open on Overview,
		// stays open in Review's drawer; the threads are on Comments.
		expect(window.tray_tabs()->currentIndex() == 2, "Review's drawer is still on the Agent tab opened on Overview");
		window.tray_tabs()->setCurrentIndex(0);
		if (course == "macros") {
			// RFC-BPR-0125 milestone 3 ([[BPR-2146]] AC11 to AC13) through the
			// window: a chip turned off and a new order reach an open window
			// and survive a restart; the suggestions toggle hides every chip;
			// Reset agents changes no prompt; Restore built-ins… returns the
			// built-ins and keeps the reviewer's own.
			// A modal question is answered as a person would, with its
			// destructive or Yes button.
			auto* const answerer = new QTimer{ &qtApplication };
			answerer->setInterval(50);
			QObject::connect(answerer, &QTimer::timeout, [] {
				if (auto* const box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); box != nullptr) {
					for (const auto standard : { QMessageBox::Reset, QMessageBox::Yes, QMessageBox::Ok }) {
						if (auto* const button = box->button(standard); button != nullptr) {
							button->click();
							return;
						}
					}
				}
			});
			answerer->start();
			const auto chips = [&] {
				auto texts = QStringList{ };
				for (auto* const chip : tab->suggestion_buttons()) {
					if (chip->isVisibleTo(tab)) {
						texts << chip->text();
					}
				}
				return texts;
			};
			const auto openLineAsk = [&](qt::workbench_window& aWindow) {
				wait([&] { return aWindow.go_to_file_line(path, 12U, false); }, "the file accepts line navigation");
				wait([&] {
					const auto context = aWindow.agent_tab()->suggestion_context_now();
					return context.OpenFile == path && context.Lines && context.Lines->first == 12U && context.Lines->second == 12U;
				},
					 "Ask has the exact file and selected line");
				aWindow.open_agent_question();
			};
			window.open_agent_question();
			wait([&] { return chips().contains(QStringLiteral("Review this pull request")) && chips().contains(QStringLiteral("What could this change break?")); }, "the shipped chips show");
			// [[BPR-2179]]: Settings › Prompts, Surfaces above the library.
			window.reveal_prompt_macros();
			auto* const page = window.prompts();
			expect(page != nullptr && window.settings()->current_page() == qt::settings_page::prompts && page->library_list()->count() > 0, "Settings › Prompts lists the prompts");
			if (!qEnvironmentVariable("BPR_ASK_RENDER_ROOT").isEmpty()) {
				window.settings()->resize(1180, 840);
				page->select("conversation.summarize");
				render(*window.settings(), QStringLiteral("prompts-settings-page"));
			}
			auto* const prChips = page->surface_list(core::prompt_surface::pull_request_chips);
			const auto  rowOf   = [prChips](const QString& aName) {
				for (auto at = 0; at < prChips->count(); ++at) {
					if (prChips->item(at)->text() == aName) {
						return at;
					}
				}
				return -1;
			};
			expect(rowOf(QStringLiteral("What could this change break?")) > 0 && !prChips->item(rowOf(QStringLiteral("What could this change break?")))->toolTip().isEmpty(),
				   "a surface lists its placements, each named");
			// A new order by keyboard (Option-Up on a placement) reaches the chips.
			const auto before = chips();
			prChips->setFocus(Qt::OtherFocusReason);
			prChips->setCurrentRow(rowOf(QStringLiteral("What could this change break?")));
			QApplication::processEvents();
			{
				QKeyEvent up{ QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier };
				QApplication::sendEvent(prChips, &up);
			}
			window.settings()->hide();
			window.open_agent_question();
			wait([&] {
				const auto now = chips();
				return now != before && now.indexOf(QStringLiteral("What could this change break?")) < before.indexOf(QStringLiteral("What could this change break?"));
			},
				 "a new order made by keyboard reaches the chips");
			// AC11: a new order made by drag. The surface's last placement is
			// dragged above its first, as a person's drag delivers it: enter,
			// move and drop on the list, carrying what the list itself puts in
			// a drag.
			window.reveal_prompt_macros();
			QApplication::processEvents();
			const auto first   = prChips->item(0)->text();
			const auto dragged = prChips->item(prChips->count() - 1)->text();
			expect(first != dragged, "the surface has two placements to order");
			{
				const auto source  = prChips->model()->index(prChips->count() - 1, 0);
				auto*      payload = prChips->model()->mimeData({ source });
				expect(payload != nullptr, "a placement can be dragged");
				const auto      target = prChips->visualItemRect(prChips->item(0));
				const auto      spot   = QPointF{ static_cast<qreal>(target.center().x()), static_cast<qreal>(target.top()) + 2.0 };
				QDragEnterEvent enter{ spot, Qt::MoveAction | Qt::CopyAction, payload, Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(prChips->viewport(), &enter);
				QDragMoveEvent moved{ spot, Qt::MoveAction | Qt::CopyAction, payload, Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(prChips->viewport(), &moved);
				QDropEvent dropped{ spot, Qt::MoveAction | Qt::CopyAction, payload, Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(prChips->viewport(), &dropped);
				expect(dropped.isAccepted(), "the surface takes the drop");
				delete payload;
			}
			wait([&] { return prChips->count() > 1 && prChips->item(0)->text() == dragged && prChips->item(1)->text() == first; }, "the dragged placement stands first on its surface");
			window.settings()->hide();
			window.open_agent_question();
			wait([&] {
				const auto now = chips();
				return now.contains(dragged) && now.indexOf(dragged) < now.indexOf(first);
			},
				 "a new order made by drag reaches the chips");
			// Turning one off removes its chip from the open window.
			page->set_on("agent-tab.review-pull-request", false);
			// Compare the prompts with the same selection before and after reset and restart.
			openLineAsk(window);
			wait([&] { return !chips().isEmpty() && !chips().contains(QStringLiteral("Review this pull request")); }, "its chip leaves the open window, no restart");
			const auto explainLine = QStringLiteral("Explain %1 line 12").arg(path);
			wait([&] { return chips().size() >= 3 && chips().contains(explainLine); }, "the chips show in their new order with the selected-line prompt");
			const auto ordered = chips();
			// AC13: the suggestions toggle still hides every chip.
			tab->suggestions_toggle()->click();
			QApplication::processEvents();
			expect(chips().isEmpty(), "the suggestions toggle hides every chip");
			tab->suggestions_toggle()->click();
			QApplication::processEvents();
			if (!e2e::wait_until_plain([&] { return chips() == ordered; }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "and shows them again as they were: " + chips().join(QLatin1Char('|')).toStdString() + " not " + ordered.join(QLatin1Char('|')).toStdString() + " toggle checked " + std::to_string(tab->suggestions_toggle()->isChecked()) };
			}
			// Remove every agent and set up again changes no prompt.
			// [[BPR-2377]]. Reset agents… opens Clear local data with Agent
			// setup checked; its one Clear… confirmation sends the request,
			// which runs as the profile reopens. The desktop does that
			// reopen; this course, started without it, takes the request
			// where the desktop would and runs it at the restart below.
			auto requested = std::optional<core::profile_data_reset_request>{ };
			window.set_profile_data_reset_dispatch([&requested](const core::profile_data_reset_request aRequest) { requested = aRequest; });
			tab->reset_agents();
			QApplication::processEvents();
			auto* const clear      = window.profile_reset_panel_widget();
			const auto  agentSetup = core::profile_data_reset_selection{ core::profile_data_reset::agent_setup };
			wait([&] { return clear != nullptr && clear->isVisibleTo(window.settings()) && clear->checked() == core::plan_profile_data_reset(agentSetup).Selection; }, "Reset agents… opens Clear local data with Agent setup checked");
			clear->clear_button()->click();
			QApplication::processEvents();
			auto* const question = dynamic_cast<qt::profile_reset_confirmation*>(clear->window()->findChild<QDialog*>(QStringLiteral("profileResetConfirmation")));
			expect(question != nullptr && question->isVisible() && question->reset() == agentSetup, "Clear… asks one confirmation for Agent setup");
			if (question->copy_box() != nullptr) {
				question->copy_box()->setChecked(false);
			}
			if (question->acknowledge_box() != nullptr) {
				question->acknowledge_box()->setChecked(true);
			}
			question->confirm_button()->click();
			wait([&] { return requested.has_value(); }, "the confirmed clear is requested");
			expect(requested->Reset == agentSetup, "the request clears exactly Agent setup");
			shutdown(*application);
			application.reset();
			// Both survive the reset and a restart.
			auto again                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
			again.DataReset             = *requested;
			again.EpochPhase            = app::detail::github_epoch_fixture_phase::reuse;
			again.ClaudeAgentExecutable = stub;
			again.ClaudeSearch          = search;
			auto restarted              = false;
			auto restart                = access::start(app::detail::github_public_path_fixture::returning_review_v1, std::move(again), [&](auto aResult) { restarted = true; if (aResult) { application = std::move(*aResult); } });
			wait([&] { return restarted && restart && restart->is_quiescent(); }, "the restart");
			expect(application != nullptr, "the restart succeeds");
			const auto cleared = access::take_profile_data_reset_report(*application);
			expect(cleared && cleared->Outcome.Reason == core::release_maintenance_reason::none && cleared->Outcome.Failed.empty() && cleared->Outcome.Reset == agentSetup, "Agent setup clears as the profile reopens");
			const auto agents = adapters::sqlite::make_agent_setup_store(*dataRoot)->load();
			expect(agents && agents->Agents.empty(), "the clear removed every agent");
			application->show();
			auto& reopened = application->window();
			reopened.resize(1440, 960);
			reopened.github_connect_action()->trigger();
			wait([&] { return reopened.github_open_action()->isEnabled(); }, "connected again");
			reopened.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
			reopened.home_open_action()->trigger();
			wait([&] { return reopened.what_changed_action()->isEnabled(); }, "the pull request opens again");
			agent_ready(reopened);
			openLineAsk(reopened);
			auto* const tab2   = reopened.agent_tab();
			const auto  chips2 = [tab2] {
				auto texts = QStringList{ };
				for (auto* const chip : tab2->suggestion_buttons()) {
					if (chip->isVisibleTo(tab2)) {
						texts << chip->text();
					}
				}
				return texts;
			};
			if (!e2e::wait_until_plain([&] { return chips2() == ordered; }, e2e::extended_agent_budget)) {
				throw std::runtime_error{ "the chip turned off and the new order survive Reset agents and a restart: " + chips2().join(QLatin1Char('|')).toStdString() + " not " + ordered.join(QLatin1Char('|')).toStdString() };
			}
			// Restore built-ins… returns the shipped text, order and On, after
			// its question, and keeps the reviewer's own prompt.
			reopened.reveal_prompt_macros();
			auto* const page2 = reopened.prompts();
			const auto  own   = page2->new_prompt();
			page2->edit_selected([](core::prompt_macro& aMacro) {
				aMacro.Body.Name     = "My own check";
				aMacro.Body.Template = "Check the pull request for my own concern.";
			});
			expect(page2->place(own, core::prompt_surface::pull_request_chips) == core::placement_refusal::none, "the reviewer's own prompt is placed on the pull request's chips");
			reopened.settings()->hide();
			reopened.open_agent_question();
			wait([&] { return chips2().contains(QStringLiteral("My own check")); }, "the reviewer's own prompt is a chip");
			reopened.reveal_prompt_macros();
			page2->findChild<QPushButton*>(QStringLiteral("promptsRestore"))->click();
			QApplication::processEvents();
			reopened.settings()->hide();
			reopened.open_agent_question();
			wait([&] { return chips2().contains(QStringLiteral("Review this pull request")) && chips2().contains(QStringLiteral("My own check")); }, "Restore built-ins… turns the built-in back on and keeps the reviewer's own");
			const auto restored = chips2();
			expect(restored.indexOf(QStringLiteral("Review this pull request")) < restored.indexOf(QStringLiteral("What could this change break?")), "in the shipped order: " + restored.join(QLatin1Char('|')).toStdString());
			answerer->stop();
			shutdown(*application);
			application.reset();
			// AC12 on screen: a store whose prompts can't be read (its orders
			// table is not one this build can read) leaves the shipped chips
			// working, and Settings › Prompts says edits are unavailable.
			{
				auto file = std::filesystem::path{ };
				for (const auto& entry : std::filesystem::recursive_directory_iterator{ *dataRoot }) {
					if (entry.path().filename() == "agent-setup.sqlite3") {
						file = entry.path();
					}
				}
				expect(!file.empty(), "the profile has its agent setup store");
				sqlite3* db = nullptr;
				expect(sqlite3_open_v2(file.string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the store opens");
				const auto broken = sqlite3_exec(db, "DROP TABLE prompt_macro_orders; CREATE TABLE prompt_macro_orders(unreadable INTEGER)", nullptr, nullptr, nullptr) == SQLITE_OK;
				sqlite3_close(db);
				expect(broken, "the store's prompts are made unreadable");
			}
			auto third                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
			third.EpochPhase            = app::detail::github_epoch_fixture_phase::reuse;
			third.ClaudeAgentExecutable = stub;
			third.ClaudeSearch          = search;
			auto started                = false;
			auto start                  = access::start(app::detail::github_public_path_fixture::returning_review_v1, std::move(third), [&](auto aResult) { started = true; if (aResult) { application = std::move(*aResult); } });
			wait([&] { return started && start && start->is_quiescent(); }, "the start on an unreadable store");
			expect(application != nullptr, "the application starts on an unreadable store");
			application->show();
			auto& unreadable = application->window();
			unreadable.resize(1440, 960);
			unreadable.github_connect_action()->trigger();
			wait([&] { return unreadable.github_open_action()->isEnabled(); }, "connected on an unreadable store");
			unreadable.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
			unreadable.home_open_action()->trigger();
			wait([&] { return unreadable.what_changed_action()->isEnabled(); }, "the pull request opens on an unreadable store");
			agent_ready(unreadable);
			auto* const tab3   = unreadable.agent_tab();
			const auto  chips3 = [tab3] {
				auto texts = QStringList{ };
				for (auto* const chip : tab3->suggestion_buttons()) {
					if (chip->isVisibleTo(tab3)) {
						texts << chip->text();
					}
				}
				return texts;
			};
			unreadable.open_agent_question();
			wait([&] { return chips3().contains(QStringLiteral("Review this pull request")) && chips3().contains(QStringLiteral("What could this change break?")); }, "the shipped chips work");
			unreadable.reveal_prompt_macros();
			auto* const status = unreadable.prompts()->findChild<QLabel*>(QStringLiteral("promptsStatus"));
			wait([&] { return status != nullptr && status->isVisibleTo(unreadable.settings()) && status->text().contains(QStringLiteral("can't be read")); }, "Settings says edits are unavailable");
			if (!qEnvironmentVariable("BPR_ASK_RENDER_ROOT").isEmpty()) {
				unreadable.settings()->resize(1180, 840);
				render(*unreadable.settings(), QStringLiteral("prompts-unreadable-store"));
			}
			unreadable.settings()->hide();
			shutdown(*application);
			std::cout << "ask-menu-macros:pass\n";
			return EXIT_SUCCESS;
		}
		const auto setupLaunches = launchesAt();
		const auto mutation      = access::review_mutation_evidence(*application);

		// AC01. One Ask at each anchor; their menus are decided when they open.
		auto* const reviewAsk = window.review_ask_button();
		auto* const fileAsk   = window.file_ask_button();
		expect(reviewAsk != nullptr && reviewAsk->isVisible() && qobject_cast<QToolBar*>(reviewAsk->parentWidget()) != nullptr, "the review bar has Ask");
		// Look ask-v005 (owner, 2026-11-14): "Filetab ask should be on the tab
		// and smaller than the tab so it doesnt read like another tab. Should
		// only be shown for the active tab. Shouldn't be shown for report card
		// tabs."
		{
			auto* const bar     = window.document_tabs()->tab_bar();
			const auto  current = bar->currentIndex();
			expect(fileAsk != nullptr && fileAsk->isVisible() && fileAsk->isEnabled(), "the file has Ask");
			auto onTab = -1;
			for (auto index = 0; index < bar->count(); ++index) {
				for (const auto side : { QTabBar::LeftSide, QTabBar::RightSide }) {
					if (auto* const button = bar->tabButton(index, side); button != nullptr && (button == fileAsk || button->isAncestorOf(fileAsk))) {
						onTab = index;
					}
				}
			}
			expect(onTab == current, "the file's Ask is on the active tab itself: on tab " + std::to_string(onTab) + " of active " + std::to_string(current));
			expect(fileAsk->height() < bar->tabRect(current).height() && fileAsk->width() < bar->tabRect(current).width() / 3, "and smaller than the tab");
			expect(fileAsk->text().isEmpty() || fileAsk->toolButtonStyle() == Qt::ToolButtonIconOnly, "it is a glyph, not a labelled button that reads like a tab");
			expect(fileAsk->accessibleName() == QStringLiteral("Ask about this file"), "named for assistive technology");
		}
		expect(window.findChild<QToolButton*>(QStringLiteral("explainFileButton")) == nullptr, "Explain this file's control is gone from the header");
		const auto line12 = std::ranges::find_if(cards->cards(), [&path](const qt::thread_card& aCard) { return aCard.Path == path && aCard.Line == 12U; });
		expect(line12 != cards->cards().end(), "the line-12 thread is listed");
		const auto identity  = line12->Id;
		auto*      threadAsk = static_cast<qt::ask_menu_button*>(nullptr);
		wait([&] {
			cards->open_thread(identity);
			QApplication::processEvents();
			threadAsk = static_cast<qt::ask_menu_button*>(cards->findChild<QToolButton*>(QStringLiteral("threadAsk")));
			return threadAsk != nullptr;
		},
			 "the open thread has Ask beside Reply");
		// Look ask-v006 (owner, 2026-11-14: "Yes but Can we get the same style
		// as the comments use."): the file tab's Ask and the thread's Ask wear
		// the comments' rounded outlined button.
		expect(fileAsk->property("bprAskLook").toString() == QStringLiteral("comment") && threadAsk->property("bprAskLook").toString() == QStringLiteral("comment"),
			   "the file tab's and the thread's Ask wear the comments' button");
		expect(cards->findChild<QToolButton*>(QStringLiteral("threadAskAgent")) == nullptr && cards->findChild<QToolButton*>(QStringLiteral("threadWasAddressed")) == nullptr,
			   "the thread's header agent controls are gone");

		if (course == "report") {
			reviewAsk->refresh_menu();
			row(*reviewAsk->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"))->trigger();
			wait([&] { return answer_of(*tab).find("RETURNING_ECHO") != std::string::npos && access::agent_invocation_evidence(*application).Quiescent && tab->state() != qt::agent_tab_state::working; },
				 "What changed is answered");
			const auto last   = tab->turn_count() - 1U;
			const auto answer = answer_of(*tab);
			expect(answer.find("commits=1") != std::string::npos, "the request listed the commits since the review:\n" + answer);
			auto* const card = tab->turn_report_card(last);
			// docs/design/report-card-v4 (owner, 2026-11-14: round 1's C,
			// Attachment): the summary as the answer, rendered, and under it
			// one row naming the report.
			expect(card != nullptr && card->isVisibleTo(tab) && tab->turn_answer_view(last)->isVisibleTo(tab), "a long answer is its summary and a report row where it was asked");
			const auto summary = tab->turn_answer_view(last)->original_body();
			expect(summary.find("The new commits change one file") != std::string::npos && summary.find("## Rendering") == std::string::npos && summary.find("RETURNING_ECHO") == std::string::npos,
				   "the summary is what stands before the first section: " + summary);
			auto* const title     = card->findChild<QLabel*>(QStringLiteral("agentTurnReportTitle"));
			auto* const meta      = card->findChild<QLabel*>(QStringLiteral("agentTurnReportMeta"));
			auto* const rowButton = card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
			expect(title != nullptr && title->text() == QStringLiteral("What changed since your review") && meta != nullptr && meta->text() == QStringLiteral("Report, 2 sections"),
				   "the row names the report and its sections, without the title: " + (meta != nullptr ? meta->text().toStdString() : std::string{ }));
			expect(rowButton != nullptr && rowButton->accessibleName() == QStringLiteral("Open report: What changed since your review") && rowButton->focusPolicy() == Qt::StrongFocus,
				   "the row is one named focus stop");
			// Look ask-v007 ("Why is your mockup report attachment cutting of
			// text"): the row is as tall as its words at the width it is given;
			// narrower, its title wraps (two lines at most) and it grows.
			expect(rowButton->hasHeightForWidth() && rowButton->heightForWidth(140) > rowButton->heightForWidth(600) && rowButton->height() >= rowButton->heightForWidth(rowButton->width()),
				   "the row's height follows its width: " + std::to_string(rowButton->heightForWidth(140)) + " at 140, " + std::to_string(rowButton->heightForWidth(600)) + " at 600, "
					   + std::to_string(rowButton->height()) + " shown for " + std::to_string(rowButton->heightForWidth(rowButton->width())));
			expect(title->height() >= title->heightForWidth(title->width()) && meta->geometry().bottom() <= rowButton->height(), "and its words are whole");
			expect(card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"))->property("bprReportSelected").toBool() == false, "at rest the row is not selected");
			auto* const strip = window.document_tabs();
			const auto  files = strip->tabs().size();
			card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"))->click();
			QApplication::processEvents();
			const auto active = [strip] { return std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Active; }); };
			expect(strip->tabs().size() == files + 1U && active() != strip->tabs().end() && active()->Report && active()->Name == QStringLiteral("What changed since your review"),
				   "Open report opens it as a tab in the files' strip");
			auto* const report = window.report_view();
			expect(window.report_shown() && report->isVisible() && !window.comparison_surface()->isVisible(), "the report shows in the comparison's place");
			expect(rowButton->property("bprReportSelected").toBool(), "while its tab is active, the card's row is selected");
			// The straight read: the title once, the byline under it, the
			// whole report open; a floating menu lists Summary and the
			// sections, then Show in conversation, View source, Copy source.
			expect(report->title() == QStringLiteral("What changed since your review") && report->body()->original_body().find("# What changed since your review") == std::string::npos,
				   "the title is said once in the pane (defect 5)");
			auto* const byline = report->findChild<QLabel*>(QStringLiteral("agentReportByline"));
			expect(byline != nullptr && byline->text().contains(QStringLiteral("What changed since my review")), "the question under the title");
			// [[BPR-2184]] (review N5): keyed by the turn's stable identity, its
			// byline handed over as data.
			expect(report->document().Turn != 0U && tab->turn_of(report->document().Turn).has_value() && !report->document().Byline.Ending.isEmpty()
					   && byline->text().contains(report->document().Byline.Ending.toHtmlEscaped()),
				   "the report names its turn by identity, and its byline is the turn's own ending: " + byline->text().toStdString());
			// The reader shown is as tall as its content: the dressed page, or
			// the body when the report is shown as written. (A hidden body's
			// width is no measure: it passed here only while the page was laid
			// out at a narrow column.)
			if (report->dressed_shown()) {
				auto* const page = report->dressed_view();
				expect(report->body()->in_place() && page->isVisible() && page->height() >= static_cast<int>(page->document()->size().height()),
					   "the page is in place, as tall as its content (defect 1): " + std::to_string(page->height()) + " for " + std::to_string(page->document()->size().height()));
			} else {
				expect(report->body()->in_place() && report->body()->height() >= report->body()->heightForWidth(report->body()->width()) - 2, "the body is in place, as tall as its content (defect 1)");
			}
			auto* const menuButton = report->menu_button();
			expect(menuButton != nullptr && menuButton->isVisible() && menuButton->accessibleName() == QStringLiteral("Report menu") && menuButton->width() == 32 && menuButton->height() == 28,
				   "the floating Report menu button");
			expect(menuButton->geometry().right() == report->width() - 14 - 1 && menuButton->geometry().top() == 12, "at the pane's top right");
			menuButton->click();
			QApplication::processEvents();
			auto names = QStringList{ };
			for (auto at = 0; at < report->outline()->count(); ++at) {
				names << report->outline()->item(at)->data(Qt::AccessibleTextRole).toString();
			}
			expect(report->popover()->isVisible() && names == QStringList({ QStringLiteral("Summary"), QStringLiteral("Rendering"), QStringLiteral("Your conversations"), QString{ }, QStringLiteral("Show in conversation"), QStringLiteral("View source"), QStringLiteral("Copy source") }),
				   "the popover: Summary, the sections, then the actions: " + names.join(QLatin1Char('|')).toStdString());
			expect(report->outline()->currentRow() == 0, "it opens on the section being read: Summary at the top");
			report->outline()->setCurrentRow(2);
			{
				QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
				QApplication::sendEvent(report->outline(), &press);
			}
			QApplication::processEvents();
			expect(!report->popover()->isVisible() && report->current_section() == 2,
				   "choosing a section scrolls to it and closes the popover: popover=" + std::to_string(report->popover()->isVisible()) + " section=" + std::to_string(report->current_section())
					   + " top=" + std::to_string(report->section_top(2)) + " pane=" + std::to_string(report->width()) + "x" + std::to_string(report->height())
					   + " column=" + std::to_string(report->column()->width()) + "x" + std::to_string(report->column()->height()));
			expect(!window.findChild<QToolButton*>(QStringLiteral("workbenchProgressSeenStep"))->isEnabled(), "the file controls are drawn in place and off (Q9)");
			expect(!window.file_ask_button()->isVisible(), "a report's tab has no file Ask (look ask-v005)");
			expect(tab->isVisible() && tab->ask_button() != nullptr, "the Agent tab keeps its place beside the report");
			// AC06: the Tour tab keeps its place and takes input while the
			// report shows; the Agent tab's question field still takes input.
			{
				// As a person does: the drawer's Tour tab.
				auto* const tray = window.findChild<QTabBar*>(QStringLiteral("workbenchTrayTabs"));
				for (auto index = 0; tray != nullptr && index < tray->count(); ++index) {
					if (tray->tabText(index) == QStringLiteral("Tour")) {
						tray->setCurrentIndex(index);
					}
				}
				QApplication::processEvents();
			}
			expect(window.tour_editor() != nullptr && window.tour_editor()->isVisible() && window.tour_editor()->isEnabled() && window.report_shown() && report->isVisible(),
				   "the Tour tab opens beside the report and takes input: editor " + std::to_string(window.tour_editor() != nullptr) + " visible "
					   + std::to_string(window.tour_editor() != nullptr && window.tour_editor()->isVisible()) + " report " + std::to_string(window.report_shown()) + " shown " + std::to_string(report->isVisible()));
			window.open_agent_question();
			QApplication::processEvents();
			auto* const typed = window.agent_tab()->question_editor();
			typed->setPlainText(QStringLiteral("About this report"));
			expect(typed->isEnabled() && typed->toPlainText() == QStringLiteral("About this report") && window.report_shown(), "the Agent tab's field takes input beside the report");
			typed->clear();
			// A code reference read in the report: its file's tab, at the line.
			const auto& body = report->body()->original_body();
			const auto  open = body.find("](");
			expect(open != std::string::npos, "the report's references are links");
			const auto link = body.substr(open + 2U, body.find(')', open) - open - 2U);
			tab->open_reference(QString::fromStdString(link));
			wait([&] { return !window.report_shown() && window.active_document_path() == path && window.comparison_surface()->isVisible(); }, "a reference opens its file's tab");
			expect(strip->tabs().size() == files + 1U && window.file_ask_button()->isEnabled() && window.file_ask_button()->isVisible(), "the report's tab stays beside it, and the file controls are back");
			// The report's tab returns to the report.
			const auto  reportAt = std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Report; });
			auto* const bar      = strip->findChild<QTabBar*>(QStringLiteral("documentTabBar"));
			bar->setCurrentIndex(static_cast<int>(std::distance(strip->tabs().begin(), reportAt)));
			wait([&] { return window.report_shown() && window.report_view()->isVisible(); }, "the report's tab returns to the report");
			// [[BPR-2218]], owner 2026-11-16: "Opening that attachment should
			// bring it to the front, like how opening a file on overview
			// switches to it." With the file's tab in front, the row opened
			// from a mode without the files' strip enters Review on the
			// report; from Review it only comes to the front.
			const auto fileInFront = [&] {
				const auto fileAt = std::ranges::find_if(strip->tabs(), [&](const qt::document_tab_presentation& aTab) { return !aTab.Report && aTab.Path == path; });
				expect(fileAt != strip->tabs().end(), "the file's tab is still open");
				bar->setCurrentIndex(static_cast<int>(std::distance(strip->tabs().begin(), fileAt)));
				wait([&] { return !window.report_shown() && window.active_document_path() == path; }, "the file's tab is in front");
			};
			for (const auto from : { qt::review_modality::overview, qt::review_modality::discuss, qt::review_modality::threads, qt::review_modality::review }) {
				fileInFront();
				window.choose_modality(from, core::workbench_modality_entry::choose);
				QApplication::processEvents();
				const auto name = qt::review_modality_title(from).toStdString();
				expect(window.current_modality() == from, "the window is in " + name);
				rowButton->click();
				wait([&] { return window.current_modality() == qt::review_modality::review && window.report_shown() && window.report_view()->isVisible() && !window.comparison_surface()->isVisible(); },
					 "opening the report from " + name + " shows it in Review");
				expect(active() != strip->tabs().end() && active()->Report && strip->tabs().size() == files + 1U && rowButton->property("bprReportSelected").toBool(),
					   "from " + name + " the report's own tab is in front, not a second one");
			}
			std::cout << "ask-menu-report: link=" << link << '\n';
			if (!attended) {
				shutdown(*application);
				std::cout << "ask-menu-report:pass\n";
				return EXIT_SUCCESS;
			}
		}
		if (course == "addressed") {
			// The pull request's Was this addressed? reads every thread
			// commits followed: six, whoever started them.
			reviewAsk->refresh_menu();
			auto* const everything = row(*reviewAsk->menu(), QStringLiteral("conversation.was-addressed"), QStringLiteral("pull-request"));
			expect(everything != nullptr && everything->isEnabled() && everything->text().endsWith(QStringLiteral("6 threads")), "the pull request's row reads six threads: " + (everything != nullptr ? everything->text().toStdString() : std::string{ }));
			// README.md's thread started on the head: greyed, saying why.
			auto readme   = window.ask_request(core::quick_ask_anchor::thread);
			readme.Thread = QStringLiteral("review-thread:PRRT_cc_5");
			QMenu quiet;
			window.fill_quick_ask(quiet, readme);
			auto* const greyed = row(quiet, QStringLiteral("conversation.was-addressed"), QStringLiteral("thread"));
			expect(greyed != nullptr && !greyed->isEnabled() && greyed->toolTip() == QStringLiteral("No commits since this thread started."), "a thread nothing followed is greyed with its reason");
			// The file's row: RFC-BPR-0128 as amended (owner, look ask-v005:
			// "Why would we do it this one instead of having one large prompt
			// with all 6 threads?"): one ask about both threads, a claim each.
			fileAsk->refresh_menu();
			auto* const file = row(*fileAsk->menu(), QStringLiteral("conversation.was-addressed"), QStringLiteral("file"));
			expect(file != nullptr && file->isEnabled() && file->text().endsWith(QStringLiteral("2 threads")), "the file's row reads its two threads");
			const auto heights = [cards] {
				auto each = std::vector<int>{ };
				for (const auto& card : cards->cards()) {
					if (card.Id != cards->open_identity()) {
						each.push_back(card.Rect.height());
					}
				}
				return each;
			};
			const auto closedBefore = heights();
			const auto headerBefore = cards->open_card()->findChild<QToolButton*>(QStringLiteral("threadMarkRead"))->parentWidget()->sizeHint().height();
			file->trigger();
			wait([&] { return cards->thread_claims().size() == 2U && access::agent_invocation_evidence(*application).Quiescent && tab->state() != qt::agent_tab_state::working; }, "both threads are answered");
			expect(launchesAt() == setupLaunches + 1U, "in one ask: " + std::to_string(launchesAt() - setupLaunches));
			expect(tab->turn_returning(tab->turn_count() - 1U) == core::returning_operation::addressed_threads && tab->turn_question(tab->turn_count() - 1U) == QStringLiteral("Was this addressed? (2 threads)"),
				   "one turn, asking about both: " + tab->turn_question(tab->turn_count() - 1U).toStdString());
			// The pull request's six: one more ask, with no confirmation, and
			// a claim on every thread.
			reviewAsk->refresh_menu();
			row(*reviewAsk->menu(), QStringLiteral("conversation.was-addressed"), QStringLiteral("pull-request"))->trigger();
			wait([&] { return cards->thread_claims().size() == 6U && access::agent_invocation_evidence(*application).Quiescent && tab->state() != qt::agent_tab_state::working; }, "all six threads are answered");
			expect(launchesAt() == setupLaunches + 2U, "the six in one ask");
			expect(cards->thread_claims().contains("review-thread:PRRT_cc_1") && cards->thread_claims().contains(identity), "a claim on each thread, the one I did not comment in too");
			QApplication::processEvents();
			expect(heights() == closedBefore, "the claims change no closed card's height");
			cards->open_thread("review-thread:PRRT_cc_1");
			QApplication::processEvents();
			auto* const claimChip = cards->open_card()->findChild<QToolButton*>(QStringLiteral("threadClaimChip"));
			const auto  words     = QStringList{ QStringLiteral("Addressed"), QStringLiteral("Partly"), QStringLiteral("Not addressed"), QStringLiteral("Unclear") };
			expect(claimChip != nullptr && words.contains(claimChip->text()) && claimChip->accessibleName() == QStringLiteral("Agent's claim: %1").arg(claimChip->text()),
				   "tbutler's thread shows the claim as one chip in its header");
			expect(claimChip->parentWidget()->sizeHint().height() == headerBefore, "the chip takes no height in the header");
			claimChip->click();
			QApplication::processEvents();
			auto* const evidence = window.findChild<QFrame*>(QStringLiteral("threadClaimEvidence"));
			expect(evidence != nullptr && evidence->isVisible(), "a press shows the evidence over the code");
			auto* const then = evidence->findChild<QPlainTextEdit*>(QStringLiteral("threadClaimThen"));
			auto* const now  = evidence->findChild<QPlainTextEdit*>(QStringLiteral("threadClaimNow"));
			expect(then != nullptr && now != nullptr && !then->toPlainText().isEmpty() && !now->toPlainText().isEmpty() && then->toPlainText().contains(QStringLiteral("   6  ")),
				   "the thread's lines as it started and as they are:\n" + (then != nullptr ? then->toPlainText().toStdString() : std::string{ }));
			evidence->close();
			const auto after = access::review_mutation_evidence(*application);
			const auto line6 = std::ranges::find_if(cards->cards(), [](const qt::thread_card& aCard) { return aCard.Id == "review-thread:PRRT_cc_1"; });
			expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations && after.StoredComments == mutation.StoredComments
					   && line6 != cards->cards().end() && !line6->Resolved,
				   "nothing was resolved, posted or published");
			std::cout << "ask-menu-addressed: claims=" << cards->thread_claims().size() << " chip=" << claimChip->text().toStdString() << '\n';
			if (attended) {
				window.setWindowTitle(QStringLiteral("Ask: Was this addressed? \u2014 synthetic data \u2014 close to finish"));
				QApplication::setQuitOnLastWindowClosed(true);
				window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
				QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
				std::cout << "ask-menu-attended:ready\n"
						  << std::flush;
				const auto result = QApplication::exec();
				shutdown(*application);
				return result;
			}
			shutdown(*application);
			std::cout << "ask-menu-addressed:pass\n";
			return EXIT_SUCCESS;
		}

		// AC02, AC03. The review bar's menu: the pull request's, the file's,
		// then the thread's quick asks.
		reviewAsk->refresh_menu();
		auto* const barMenu = reviewAsk->menu();
		expect(sections(*barMenu) == QStringList({ QStringLiteral("Pull request"), QStringLiteral("renderer.cpp"), QStringLiteral("Thread on line 12") }),
			   "three sections in order: " + sections(*barMenu).join(QStringLiteral(", ")).toStdString());
		const auto barRows = rows(*barMenu);
		std::cout << "ask-menu: review-bar rows=" << barRows.join(QLatin1Char(' ')).toStdString() << '\n';
		auto* const changed = row(*barMenu, QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"));
		expect(changed != nullptr && changed->isEnabled() && changed->text().startsWith(QStringLiteral("What changed since my review")),
			   "What changed names the commits a press reads: " + (changed != nullptr ? changed->text().toStdString() : std::string{ }));
		expect(row(*barMenu, QStringLiteral("agent-tab.explain-line"), QStringLiteral("file")) == nullptr && !barRows.join(' ').contains(QStringLiteral("explain-line")),
			   "a prompt without Quick ask is not listed");
		expect(row(*barMenu, QStringLiteral("agent-tab.explain-file"), QStringLiteral("file")) != nullptr && row(*barMenu, QStringLiteral("conversation.summarize"), QStringLiteral("thread")) != nullptr,
			   "the file's and the thread's quick asks are reached from the review bar");
		expect(barMenu->findChild<QAction*>(QStringLiteral("quickAskCustomQuestion")) != nullptr && barMenu->findChild<QAction*>(QStringLiteral("quickAskEdit")) != nullptr,
			   "Custom question… and Edit Quick Asks… close the menu");
		// A file with no review threads: its Was this addressed? is greyed,
		// with the reason; its own section only in the file's menu.
		auto request = window.ask_request(core::quick_ask_anchor::file);
		request.File = QStringLiteral("docs/guide.md");
		QMenu quiet;
		window.fill_quick_ask(quiet, request);
		auto* const greyed = row(quiet, QStringLiteral("conversation.was-addressed"), QStringLiteral("file"));
		expect(greyed != nullptr && !greyed->isEnabled() && greyed->toolTip() == QStringLiteral("No review threads on this file.") && greyed->statusTip() == greyed->toolTip()
				   && greyed->whatsThis() == greyed->toolTip(),
			   "a quick ask that cannot run is greyed, its reason its tooltip and accessible text");
		expect(sections(quiet).isEmpty(), "a one-scope menu has no section titles");
		// AC08: the greyed row is reachable by keyboard.
		expect(quiet.style()->styleHint(QStyle::SH_Menu_AllowActiveAndDisabled, nullptr, &quiet) == 1, "greyed rows are reached by the arrow keys");
		// No thread open: the thread section is not listed.
		auto noThread   = window.ask_request(core::quick_ask_anchor::review_bar);
		noThread.Thread = QString{ };
		QMenu bare;
		window.fill_quick_ask(bare, noThread);
		expect(!sections(bare).contains(QStringLiteral("Thread on line 12")) && sections(bare).size() == 2, "a scope whose subject is absent is not listed");
		// The thread's own menus.
		threadAsk->refresh_menu();
		expect(rows(*threadAsk->menu()).size() >= 3 && row(*threadAsk->menu(), QStringLiteral("conversation.was-addressed"), QStringLiteral("thread")) != nullptr,
			   "the thread's Ask lists its quick asks, Was this addressed? among them");
		auto threadMenu = std::unique_ptr<QMenu>{ cards->thread_menu(identity) };
		expect(threadMenu != nullptr, "the thread has a right-click menu");
		auto* const askItem = threadMenu->findChild<QMenu*>(QStringLiteral("threadAskMenu"));
		expect(askItem != nullptr && rows(*askItem) == rows(*threadAsk->menu()), "the thread's menu has one Ask item with the same quick asks");
		for (auto* const action : threadMenu->actions()) {
			expect(action->objectName() != QStringLiteral("threadAskAgentAction") && action->objectName() != QStringLiteral("threadWasAddressedAction") && !action->text().startsWith(QStringLiteral("Ask agent")),
				   "the agent items it replaces are gone");
		}
		auto*       strip   = window.document_tabs();
		const auto  active  = std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Active; });
		auto        tabMenu = std::unique_ptr<QMenu>{ strip->make_tab_menu(active->Id, &window) };
		auto* const tabAsk  = tabMenu != nullptr ? tabMenu->findChild<QMenu*>(QStringLiteral("documentTabAskMenu")) : nullptr;
		expect(tabAsk != nullptr && row(*tabAsk, QStringLiteral("agent-tab.explain-file"), QStringLiteral("file")) != nullptr, "the file tab's menu has Ask with the file's quick asks");
		expect(tabMenu->findChild<QAction*>(QStringLiteral("documentTabExplainFile")) == nullptr, "Explain This File is gone from the tab's menu");
		expect(launchesAt() == setupLaunches, "building and filtering every menu launched nothing");

		// AC02. Custom question… attaches the thread and sends nothing.
		const auto turns = tab->turn_count();
		threadAsk->menu()->findChild<QAction*>(QStringLiteral("quickAskCustomQuestion"))->trigger();
		QApplication::processEvents();
		expect(tab->isVisible() && tab->conversation_chip_text().contains(QStringLiteral("renderer.cpp")) && tab->turn_count() == turns && launchesAt() == setupLaunches,
			   "Custom question… opens the Agent tab on the thread and sends nothing");
		// Edit Quick Asks… opens Settings › Prompts ([[BPR-2179]]).
		barMenu->findChild<QAction*>(QStringLiteral("quickAskEdit"))->trigger();
		QApplication::processEvents();
		auto* const page = window.prompts();
		expect(page != nullptr && window.settings() != nullptr && window.settings()->current_page() == qt::settings_page::prompts, "Edit Quick Asks… opens Settings at Prompts");
		expect(page->library_list()->count() > 0 && page->surface_list(core::prompt_surface::pull_request_menu)->count() > 0, "the library and the surfaces are listed");
		// AC04. The built-in rows are in the library and on their menus.
		const auto has = [page](const std::string& aId) { return std::ranges::any_of(page->library(), [&aId](const core::prompt_macro& aMacro) { return aMacro.Id == aId; }); };
		expect(has("agent-tab.what-changed") && has("conversation.was-addressed") && has("agent-tab.explain-file"), "What changed, Was this addressed? and Explain this file have built-in rows");
		// Off the pull request's menu, and the reviewer's own on the file's.
		page->remove("agent-tab.what-could-break", core::prompt_surface::pull_request_menu);
		const auto own = page->new_prompt();
		page->edit_selected([](core::prompt_macro& aMacro) {
			aMacro.Body.Name     = "Security pass on {file.name}";
			aMacro.Body.Template = "Check {file} for injection.";
		});
		expect(page->place(own, core::prompt_surface::pull_request_menu) == core::placement_refusal::placeholder_unfilled && !page->last_refusal().isEmpty(),
			   "the pull request's menu refuses {file}, with a reason");
		expect(page->place(own, core::prompt_surface::file_menu) == core::placement_refusal::none, "the file's menu takes it");
		wait([&] {
			fileAsk->refresh_menu();
			// RFC-BPR-0125 AC24: the host mints a UUID for a new prompt.
			return core::valid_prompt_uuid(own) && std::ranges::any_of(rows(*fileAsk->menu()), [&own](const QString& aName) { return aName.startsWith(QStringLiteral("quickAsk_") + QString::fromStdString(own)); });
		},
			 "the own quick ask is in the file's Ask");
		reviewAsk->refresh_menu();
		expect(row(*reviewAsk->menu(), QStringLiteral("agent-tab.what-could-break"), QStringLiteral("pull-request")) == nullptr, "taken off its menu: no longer in the review bar's Ask");
		auto store = adapters::sqlite::make_prompt_macro_store(*dataRoot);
		wait([&] {
			const auto stored = store->load();
			if (!stored || stored->Differences.Own.size() != 1U) {
				return false;
			}
			const auto changes = core::merge_prompt_macros(core::shipped_prompt_macros(), stored->Differences).Surfaces;
			const auto on      = [&changes](const core::prompt_surface aSurface, const std::string& aId) {
				const auto& list = changes.Lists.at(static_cast<std::size_t>(aSurface));
				return std::ranges::find(list, aId) != list.end();
			};
			return on(core::prompt_surface::file_menu, stored->Differences.Own.front().Id) && !on(core::prompt_surface::pull_request_menu, "agent-tab.what-could-break");
		},
			 "both edits, with their surfaces, are in the profile's store");
		const auto sheetLaunches = launchesAt();
		expect(sheetLaunches == setupLaunches, "editing prompts launched nothing");

		// AC08. Every control is named and takes focus.
		for (auto* const button : { static_cast<QToolButton*>(reviewAsk), static_cast<QToolButton*>(fileAsk), static_cast<QToolButton*>(threadAsk) }) {
			expect(!button->accessibleName().isEmpty() && button->focusPolicy() == Qt::StrongFocus && button->popupMode() == QToolButton::InstantPopup, "an Ask control is named and reached by keyboard");
		}

		// AC04. What changed from the review bar sends the action's own prompt.
		reviewAsk->refresh_menu();
		row(*reviewAsk->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"))->trigger();
		wait([&] { return answer_of(*tab).find("RETURNING_ECHO") != std::string::npos && access::agent_invocation_evidence(*application).Quiescent; }, "What changed is answered");
		const auto answer = answer_of(*tab);
		expect(launchesAt() == setupLaunches + 1U && answer.find("system=1") != std::string::npos && answer.find("changes=1") != std::string::npos,
			   "one press, one call, with its own prompt and the change:\n" + answer);
		expect(tab->turn_returning(tab->turn_count() - 1U) == core::returning_operation::latest_upload, "the turn is What changed");

		const auto after = access::review_mutation_evidence(*application);
		expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations && after.StoredComments == mutation.StoredComments, "nothing reached GitHub");
		if (attended) {
			window.setWindowTitle(QStringLiteral("Ask — synthetic data — close to finish"));
			QApplication::setQuitOnLastWindowClosed(true);
			window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
			QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
			std::cout << "ask-menu-attended:ready\n"
					  << std::flush;
			const auto result = QApplication::exec();
			shutdown(*application);
			return result;
		}
		shutdown(*application);
		std::cout << "ask-menu:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "ask-menu: " << aError.what() << '\n';
		if (application) {
			const auto& tab = *application->window().agent_tab();
			std::cerr << "queue: " << tab.queue_note()->text().toStdString() << " turns=" << tab.turn_count() << " claims=" << application->window().conversation_panel()->cards()->thread_claims().size() << '\n';
			std::cerr << "github-status: " << application->window().github_status_text() << "\nagent-panel: " << application->window().agent_panel()->status_text().toStdString()
					  << "\nlast-answer: " << answer_of(tab) << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
