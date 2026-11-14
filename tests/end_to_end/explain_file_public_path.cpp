/// \file
/// \brief RFC-BPR-0117 ([[BPR-2076]]). Explain this file through the shipped
/// window, on nested-folders-v1 with the scoped writing stub as the installed
/// Claude.
///
/// Courses, by the first argument:
///
/// - `explain STUB`: opening the pull request and a file, and scrolling it,
///   launch no agent; Explain this file in the file header's Ask (RFC-BPR-0128,
///   Q3) sends one fresh,
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
#include "bpr/qt/ask_menu.hpp"
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

	[[nodiscard]] auto last_answer(const qt::workbench_window& aWindow) -> std::string
	{
		const auto* const tab = aWindow.agent_tab();
		return tab->turn_count() == 0U ? std::string{ } : tab->turn_answer(tab->turn_count() - 1U).toStdString();
	}

	[[nodiscard]] auto settled(app::github_application& aApplication, const qt::workbench_window& aWindow) -> bool
	{
		return last_answer(aWindow).find("EXPLAIN_ECHO") != std::string::npos && access::agent_invocation_evidence(aApplication).Quiescent && aWindow.agent_tab()->state() != qt::agent_tab_state::working;
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
		auto* const tab = window.agent_tab();
		// RFC-BPR-0128 (Q3): the file header's Ask stands in Explain this
		// file's place, and Explain this file is its first quick ask.
		auto* const button = window.file_ask_button();
		const auto  row    = [button]() -> QAction* {
			button->refresh_menu();
			return button->menu()->findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.explain-file_file"));
		};
		wait([&] { return button != nullptr && button->isEnabled() && button->isVisible() && row() != nullptr && row()->isEnabled(); }, "Explain this file is offered in the file header's Ask");
		expect(button->text() == QStringLiteral("Ask") && row()->text().startsWith(QStringLiteral("Explain this file")), "the file's Ask lists Explain this file");
		const auto setupLaunches = launchesAt();
		const auto mutation      = access::review_mutation_evidence(*application);
		const auto turnsBefore   = tab->turn_count();

		// AC01, AC02, AC04. The press: a queued ask that runs at once, its
		// turn shown in the Agent tab (owner look ask-v001: no card over the
		// file; the Agent tab opens at the question and its answer).
		row()->trigger();
		wait([&] { return tab->turn_count() == turnsBefore + 1U && settled(*application, window); }, "the answer arrives in the Agent tab");
		const auto answer = last_answer(window);
		const auto last   = tab->turn_count() - 1U;
		expect(launchesAt() == setupLaunches + 1U, "one press, one call");
		expect(tab->turn_explain(last) == path, "the question is a turn in the Agent tab's history, about the file");
		expect(answer.find("system=1") != std::string::npos, "its own prompt was the system prompt:\n" + answer);
		expect(answer.find("change=1") != std::string::npos && answer.find("content=1") != std::string::npos, "the file's change and its text were in the request:\n" + answer);
		expect(answer.find("subject=1") != std::string::npos, "the file is the question's subject:\n" + answer);
		expect(window.explain_card() == nullptr || window.explain_card()->isHidden(), "no card stands over the file");
		expect(tab->isVisible(), "the Agent tab is open at the turn");
		std::cout << "explain-file: answer=" << answer.substr(0U, answer.find('\n')) << '\n';

		if (attended) {
			window.setWindowTitle(QStringLiteral("Explain this file, synthetic data, close to finish"));
			QApplication::setQuitOnLastWindowClosed(true);
			window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
			QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
			std::cout << "explain-file-attended:ready\n"
					  << std::flush;
			const auto result = QApplication::exec();
			shutdown(*application);
			return result;
		}

		// AC01. The tab's menu sends the same question.
		auto*      strip  = window.document_tabs();
		const auto active = std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Active; });
		expect(active != strip->tabs().end(), "a tab is active");
		auto        menu    = std::unique_ptr<QMenu>{ strip->make_tab_menu(active->Id, &window) };
		auto* const ask     = menu != nullptr ? menu->findChild<QMenu*>(QStringLiteral("documentTabAskMenu")) : nullptr;
		auto* const explain = ask != nullptr ? ask->findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.explain-file_file")) : nullptr;
		expect(explain != nullptr && explain->isEnabled(), "the tab's menu offers Explain this file under Ask");
		explain->trigger();
		wait([&] { return tab->turn_count() == turnsBefore + 2U && settled(*application, window); }, "the menu's question is answered");
		expect(tab->turn_explain(tab->turn_count() - 1U) == path && launchesAt() == setupLaunches + 2U, "the menu sends the same question about the file");

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
			std::cerr << "github-status: " << window.github_status_text() << "\nagent-panel: " << window.agent_panel()->status_text().toStdString() << "\nlast: " << last_answer(window) << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
