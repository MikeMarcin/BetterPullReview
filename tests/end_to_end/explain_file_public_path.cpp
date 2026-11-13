/// \file
/// \brief RFC-BPR-0117 ([[BPR-2076]]). Explain this file through the shipped
/// window, on nested-folders-v1 with the scoped writing stub as the installed
/// Claude.
///
/// Courses, by the first argument:
///
/// - `explain STUB`: opening the pull request and a file, and scrolling it,
///   launch no agent; Explain this file at the file's header sends one fresh,
///   tool-free call about that file (`analyze_current_file`, its own prompt
///   as the system prompt, the file's change and text in the request) with
///   nothing selected; the answer shows in the card at the file's header with
///   its references as links, and is a turn in the Agent tab's history; Keep
///   as note makes a private note on the file that names the agent; Continue
///   in Agent tab opens the tab at that turn; the tab's menu sends the same
///   question; Dismiss removes the card and makes no note; nothing reaches
///   GitHub.
/// - `explain STUB --attended`: the first answer, then the window stays open
///   on Review for a person to look; closing it ends the course.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/explain_file_card.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	constexpr auto explained_path = std::string_view{ "src/engine/renderer.cpp" };

	template <typename Predicate>
	void wait(Predicate aPredicate, const char* aMessage)
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

	void shutdown(app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		wait([&] { return aApplication.status().ShutdownComplete; }, "shutdown");
	}

	/// The Agent tab's setup finds the stub claude and its Check passes, as a
	/// person's does; nothing is asked about the pull request.
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

	[[nodiscard]] auto card_answer(const qt::workbench_window& aWindow) -> std::string
	{
		const auto* const card = aWindow.explain_card();
		return card == nullptr ? std::string{ } : card->progress().AnswerText.toStdString();
	}

	[[nodiscard]] auto settled(app::github_application& aApplication, const qt::workbench_window& aWindow) -> bool
	{
		const auto* const card = aWindow.explain_card();
		return card != nullptr && card->progress().Answered && !card->progress().Running && card_answer(aWindow).find("EXPLAIN_ECHO") != std::string::npos
			&& access::agent_invocation_evidence(aApplication).Quiescent;
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	if (argc < 3 || std::string_view{ argv[1] } != "explain") {
		std::cerr << "usage: explain_file_public_path explain STUB [--attended]\n";
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
	auto search = e2e::stub_claude_search(std::filesystem::path{ data.path().toStdString() } / "stub-claude-search", stub);
	if (!search) {
		std::cerr << "stub claude search refused\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.ClaudeAgentExecutable = stub;
		options.ClaudeSearch          = search;
		auto terminal                 = false;
		auto startup                  = access::start(app::detail::github_public_path_fixture::nested_folders_v1,
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
		const auto launchesAt = [&] { return access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations; };
		const auto path       = QString::fromUtf8(explained_path.data(), static_cast<qsizetype>(explained_path.size()));

		// AC05. Opening the pull request, opening the file and scrolling it
		// launch no agent.
		wait([&] { return window.open_file_in_review(path) && window.active_document_path() == path; }, "the file opens in Review");
		if (auto* const surface = window.comparison_surface(); surface != nullptr) {
			for (auto* const bar : surface->findChildren<QScrollBar*>()) {
				bar->setValue(bar->maximum() / 2);
			}
		}
		QApplication::processEvents();
		expect(launchesAt() == 0U, "opening and scrolling a file launches no agent");
		agent_ready(window);
		auto* const tab    = window.agent_tab();
		auto* const button = window.explain_file_button();
		auto* const card   = window.explain_card();
		wait([&] { return button != nullptr && button->isEnabled() && button->isVisible(); }, "Explain this file is offered at the file's header");
		expect(button->text() == QStringLiteral("Explain this file") && qt::agent_run_control(*button), "the control says what it does and wears the Agent accent");
		expect(card != nullptr && card->isHidden(), "no card before a press");
		const auto setupLaunches = launchesAt();
		const auto mutation      = access::review_mutation_evidence(*application);
		const auto turnsBefore   = tab->turn_count();

		// AC01, AC02, AC04. The press.
		auto sawRunning = false;
		button->click();
		wait([&] {
			sawRunning = sawRunning || (card->isVisible() && card->progress().Running.has_value());
			return settled(*application, window);
		},
			 "the answer arrives in the card");
		const auto answer = card_answer(window);
		const auto last   = tab->turn_count() - 1U;
		expect(launchesAt() == setupLaunches + 1U, "one press, one call");
		expect(tab->turn_count() == turnsBefore + 1U && tab->turn_explain(last) == path, "the question is a turn in the Agent tab's history, about the file");
		expect(answer.find("system=1") != std::string::npos, "its own prompt was the system prompt:\n" + answer);
		expect(answer.find("change=1") != std::string::npos && answer.find("content=1") != std::string::npos, "the file's change and its text were in the request:\n" + answer);
		expect(answer.find("subject=1") != std::string::npos, "the file is the question's subject:\n" + answer);
		expect(card->isVisible() && card->path() == path, "the card stands at the file's header");
		expect(card->answer_view() != nullptr && card->answer_view()->original_body().find("](") != std::string::npos, "the answer's references are links");
		expect(card->status_line() != nullptr && !card->status_line()->live() && card->status_line()->text().contains(QStringLiteral("answered")), "the card's line says how the turn ended");
		std::cout << "explain-file: saw-running=" << (sawRunning ? 1 : 0) << " answer=" << answer.substr(0U, answer.find('\n')) << '\n';

		if (attended) {
			window.setWindowTitle(QStringLiteral("Explain this file — synthetic data — close to finish"));
			QApplication::setQuitOnLastWindowClosed(true);
			window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
			QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
			std::cout << "explain-file-attended:ready\n"
					  << std::flush;
			const auto result = QApplication::exec();
			shutdown(*application);
			return result;
		}

		// AC03. Keep as note: a Local comment on the file (RFC-BPR-0127),
		// naming the agent.
		auto* const cards       = window.conversation_panel()->cards();
		const auto  notesBefore = cards->local_comments().size();
		card->keep_button()->click();
		wait([&] { return card->note() != 0U; }, "Keep as note makes a note");
		wait([&] { return cards->local_comments().size() == notesBefore + 1U; }, "the Local comment is listed");
		const auto& notes = cards->local_comments();
		const auto  kept  = std::ranges::find(notes, card->note(), &qt::local_comment_card::Id);
		expect(kept != notes.end() && kept->Body.startsWith(QStringLiteral("Agent: Claude explained this file.")) && kept->Place == qt::local_comment_place::whole_file && !kept->Line,
			   "the Local comment is on the file and not a line, and names its agent origin");
		expect(card->keep_button()->text() == QStringLiteral("Go to note"), "Keep as note becomes Go to note");

		// AC03. Continue in Agent tab opens that turn.
		card->continue_button()->click();
		QApplication::processEvents();
		expect(tab->isVisible(), "the Agent tab is open");

		// AC01. The tab's menu sends the same question.
		auto*      strip  = window.document_tabs();
		const auto active = std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Active; });
		expect(active != strip->tabs().end(), "a tab is active");
		auto menu    = std::unique_ptr<QMenu>{ strip->make_tab_menu(active->Id, &window) };
		auto explain = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("documentTabExplainFile")) : nullptr;
		if (explain == nullptr && menu != nullptr) {
			for (auto* const action : menu->actions()) {
				if (action->objectName() == QStringLiteral("documentTabExplainFile")) {
					explain = action;
				}
			}
		}
		expect(explain != nullptr && explain->isEnabled(), "the tab's menu offers Explain This File");
		explain->trigger();
		wait([&] { return tab->turn_count() == turnsBefore + 2U && settled(*application, window); }, "the menu's question is answered");
		expect(tab->turn_explain(tab->turn_count() - 1U) == path && launchesAt() == setupLaunches + 2U, "the menu sends the same question about the file");

		// AC03. Dismiss removes the card and makes no note; the turn stays.
		const auto notesNow = cards->local_comments().size();
		card->dismiss_button()->click();
		QApplication::processEvents();
		expect(card->isHidden() && cards->local_comments().size() == notesNow && tab->turn_count() == turnsBefore + 2U, "Dismiss removes the card, makes no note, and the turn stays in the history");

		// AC05. Nothing reached GitHub.
		const auto after = access::review_mutation_evidence(*application);
		expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations && after.StoredComments == mutation.StoredComments, "nothing reached GitHub");
		shutdown(*application);
		std::cout << "explain-file:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "explain-file: " << aError.what() << '\n';
		if (application) {
			auto& window = application->window();
			std::cerr << "github-status: " << window.github_status_text() << "\nagent-panel: " << window.agent_panel()->status_text().toStdString() << "\ncard: " << card_answer(window)
					  << "\ncard-ending: " << (window.explain_card() != nullptr ? window.explain_card()->progress().Ending.toStdString() : std::string{ }) << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
