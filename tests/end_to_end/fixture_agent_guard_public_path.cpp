/// \file
/// \brief [[BPR-2177]] (owner, 2026-11-14: "we need a fixture agent for
/// testing why am I wasting a ton of money just seeing how buttons and UI flow
/// works"). A person's session on a tests-on build (`FixtureAgentSession`, as
/// any `--attended…` launch sets it) runs the fixture agent and never a real
/// one: a `claude` and a `codex` first on `PATH` that would leave a mark, an
/// absolute and a bare `claude` are refused by the launch guard; the agent
/// picker says "Fixture agent"; a question is answered by the fixture agent.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

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
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot || argc < 2) {
		std::cerr << "usage: fixture_agent_guard_public_path FIXTURE_AGENT_FOLDER\n";
		return EXIT_FAILURE;
	}
	const auto fixtureFolder = std::filesystem::path{ argv[1] };
	// The person's own agents, first on PATH: they leave a mark if started.
	const auto real   = std::filesystem::path{ data.path().toStdString() } / "real-agents";
	const auto marked = std::filesystem::path{ data.path().toStdString() } / "real-agent-started";
	std::filesystem::create_directories(real);
	for (const auto* const name : { "claude", "codex" }) {
		const auto program = real / name;
		std::ofstream{ program } << "#!/bin/sh\necho \"$0 $*\" >> '" << marked.string() << "'\nexit 1\n";
		std::filesystem::permissions(program, std::filesystem::perms::owner_all);
	}
	qputenv("PATH", QByteArray::fromStdString(real.string() + ":" + qgetenv("PATH").toStdString()));
	std::unique_ptr<app::github_application> application;
	try {
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.FixtureAgentSession = true;
		auto terminal               = false;
		auto startup                = access::start(app::detail::github_public_path_fixture::nested_folders_v1, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		// The file at a line, as the ask courses open it: the question is about
		// that line, so its comparison and selection must be in (BPR-2180 v004
		// shows a code skeleton first).
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return window.go_to_file_line(path, 12U, false) && window.active_document_path() == path; }, "a file opens at a line");
		wait([&] {
			const auto* const comparison = window.comparison_surface();
			return comparison != nullptr && comparison->current_result() != nullptr && comparison->selection().has_value();
		},
			 "the comparison and its selected line are in");

		// The guard: only the fixture agent's folder is admitted.
		expect(access::fixture_agent_admits(*application, (fixtureFolder / "claude").string()) && access::fixture_agent_admits(*application, (fixtureFolder / "codex").string()),
			   "the fixture agent is admitted");
		const auto bare = access::fixture_agent_admits(*application, "claude");
		expect(bare && std::filesystem::path{ *bare }.parent_path() == fixtureFolder, "a bare claude starts the fixture agent, never the PATH's: " + bare.value_or("refused"));
		for (const auto& refused : { (real / "claude").string(), (real / "codex").string(), std::string{ "/opt/homebrew/bin/claude" }, std::string{ "/usr/local/bin/some-wrapper" } }) {
			expect(!access::fixture_agent_admits(*application, refused), "a real agent program is refused: " + refused);
		}

		// The setup finds the fixture agent, already set up and chosen: the
		// owner's look ask-v002 (2026-11-14) opened on Codex, which no fixture
		// answered ("Aren't we supposed to have a mack agent for testing now?").
		window.open_agent_question();
		auto* const tab = window.agent_tab();
		wait([tab] { return tab->setup_readiness() != qt::agent_setup_readiness::checking; }, "the agent setup looks for agents");
		static_cast<void>(e2e::wait_until_plain([tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; }, e2e::extended_agent_budget));
		if (tab->setup_readiness() != qt::agent_setup_readiness::ready) {
			const auto& setup = window.agent_setup()->projection();
			auto        rows  = std::string{ };
			for (const auto& row : setup.State.Rows) {
				rows += std::string{ core::stable_name(row.Agent) } + (row.Verdict.Permitted ? "+" : "-") + std::to_string(static_cast<int>(row.Verdict.Refusal)) + " ";
			}
			throw std::runtime_error{ "the fixture agent is set up without a step: step " + std::string{ core::stable_name(setup.Step) } + ", chosen "
									  + (setup.State.Chosen ? std::string{ core::stable_name(*setup.State.Chosen) } : std::string{ "none" }) + ", rows " + rows
									  + ", repository " + (setup.State.RepositoryChosen ? "chosen" : "not chosen") + ", checks " + std::to_string(static_cast<int>(setup.State.AgentCheck.State)) + "/" + std::to_string(static_cast<int>(setup.State.WorkspaceCheck.State)) + ", tab state setting-up " + std::to_string(tab->isVisible()) };
		}
		auto* const selector = window.agent_panel()->profile_selector();
		auto        names    = QStringList{ };
		for (auto index = 0; index < selector->count(); ++index) {
			names << selector->itemText(index);
		}
		expect(names.join(QLatin1Char('|')).contains(QStringLiteral("Fixture agent")), "the picker names the fixture agent: " + names.join(QLatin1Char('|')).toStdString());
		if (!selector->currentText().startsWith(QStringLiteral("Fixture agent"))) {
			auto entries = std::string{ };
			for (const auto& entry : window.agent_setup()->projection().Preferences.Agents) {
				entries += entry.Name + "=" + entry.Program + " (" + std::to_string(static_cast<int>(entry.Protocol)) + ", preset " + (entry.Preset ? std::string{ core::stable_name(*entry.Preset) } : std::string{ "none" }) + ") ";
			}
			throw std::runtime_error{ "the fixture agent is chosen: " + selector->currentText().toStdString() + " of " + names.join(QLatin1Char('|')).toStdString() + "; entries " + entries
									  + "; preferred " + std::to_string(window.agent_setup()->projection().Preferences.PreferredAgent) };
		}
		for (auto index = 0; index < selector->count(); ++index) {
			expect(selector->itemText(index).startsWith(QStringLiteral("Fixture agent")), "every offered agent is the fixture: " + names.join(QLatin1Char('|')).toStdString());
		}
		// Look ask-v003 (owner, 2026-11-14: "The footer does not say fixture
		// agent"): every place the agent is named says so.
		auto footers = 0;
		for (auto* const footer : window.findChildren<QWidget*>()) {
			const auto text = footer->property("agentRunSettingsText").toString();
			if (!text.isEmpty()) {
				++footers;
				expect(text.startsWith(QStringLiteral("Fixture agent")), "the footer names the fixture agent: " + text.toStdString());
			}
		}
		if (footers == 0) {
			auto seen = std::string{ };
			for (auto* const footer : window.findChildren<QWidget*>()) {
				seen += footer->accessibleName().toStdString() + (footer->isVisible() ? " (shown) " : " (hidden) ");
			}
			throw std::runtime_error{ "a footer names the agent: " + seen };
		}
		expect(window.agent_setup()->ready_title() == QStringLiteral("Fixture agent is ready"),
			   "the setup and its Check name the fixture agent: " + window.agent_setup()->ready_title().toStdString());

		// A question: the fixture agent answers, and no real program ran.
		const auto turns = tab->turn_count();
		// [[BPR-2153]], BPR-2181: the fixture agent is a saved entry, so the
		// turn runs on the session's runtime, which takes it once the window
		// has lent it the review's authority; a send before that is refused
		// and said, and is sent again here.
		auto asking = QElapsedTimer{ };
		asking.start();
		while (tab->turn_count() == turns && tab->state() != qt::agent_tab_state::working && asking.elapsed() < 30000) {
			tab->question_editor()->setPlainText(QStringLiteral("What does this change do?"));
			QApplication::processEvents();
			tab->ask_button()->click();
			static_cast<void>(e2e::wait_until_plain([&] { return tab->state() == qt::agent_tab_state::working || tab->turn_count() > turns; }, std::chrono::milliseconds{ 1500 }));
		}
		wait([&] { return tab->turn_count() > turns && tab->turn_answer(tab->turn_count() - 1U).contains(QStringLiteral("fixture agent")) && access::agent_invocation_evidence(*application).Quiescent; },
			 "the fixture agent answers");
		expect(!std::filesystem::exists(marked), "no real agent program was started");
		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
		std::cout << "fixture-agent-guard:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "fixture-agent-guard: " << aError.what() << '\n';
		if (application) {
			const auto* const tab = application->window().agent_tab();
			std::cerr << "agent-panel: " << application->window().agent_panel()->status_text().toStdString() << "\nturns: " << tab->turn_count()
					  << "\nlast: " << (tab->turn_count() > 0U ? tab->turn_answer(tab->turn_count() - 1U).toStdString() + " / " + tab->turn_footer(tab->turn_count() - 1U).toStdString() : std::string{ })
					  << "\nrefusal: " << tab->send_refusal_text().toStdString() << '\n';
			application->begin_shutdown();
		}
		return EXIT_FAILURE;
	}
}
