// [[BPR-2256]]. The Agent tab's setup while a pull request loads, on the real
// composition. Until the window has taken the pull request (its host is known
// from the accepted handoff) the setup is one loading state, owner amendment
// 2026-11-18: "If we can't get past step 1 then the whole drawer should just
// be in a loading state". Once it has, the host's own policy decides each
// agent: on an Enterprise host Claude Code and an added Claude stream JSON
// agent are allowed, and since [[BPR-2305]] (owner, 2026-11-19) Codex is
// too, popular or added. Opening
// another pull request drops the previous review's policy at once, so its
// verdicts are never shown while the next one loads.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QTemporaryDir>

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	void wait(const std::function<bool()>& aCondition, const std::string& aWhat)
	{
		if (!e2e::wait_until_plain(aCondition, e2e::extended_agent_budget)) {
			throw std::runtime_error{ "timed out: " + aWhat };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	/// The files page of every pull request read waits while `Held` is set.
	struct read_gate
	{
		std::mutex                  Mutex;
		std::condition_variable_any Changed;
		bool                        Held    = false;
		bool                        Entered = false;

		void hold()
		{
			const auto lock = std::scoped_lock{ Mutex };
			Held            = true;
			Entered         = false;
		}
		void release()
		{
			{
				const auto lock = std::scoped_lock{ Mutex };
				Held            = false;
			}
			Changed.notify_all();
		}
		[[nodiscard]] auto entered() -> bool
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Entered;
		}
		void before_files_page(const std::stop_token& aStop)
		{
			auto lock = std::unique_lock{ Mutex };
			if (Held) {
				Entered = true;
				Changed.wait(lock, aStop, [this] { return !Held; });
			}
		}
	};

	[[nodiscard]] auto rows_text(const application::agent_setup_projection& aSetup) -> std::string
	{
		auto text = std::string{ aSetup.PolicyPending ? "pending: " : "decided: " };
		for (const auto& choice : aSetup.Choices) {
			text += choice.Name + (choice.Verdict.Permitted ? "+" : "-") + std::string{ core::stable_name(choice.Verdict.Refusal) } + " ";
		}
		text += "| rows: ";
		for (const auto& row : aSetup.State.Rows) {
			text += std::string{ core::stable_name(row.Agent) } + "@" + row.Path + (row.Verdict.Permitted ? "+" : "-") + std::string{ core::stable_name(row.Verdict.Refusal) } + " ";
		}
		return text;
	}

	/// While the read is held the setup is pending, and the tab shows its
	/// loading state alone: no card and no refusal.
	void expect_loading(qt::workbench_window& aWindow, const std::string& aWhen)
	{
		auto* const tab   = aWindow.agent_tab();
		auto* const setup = aWindow.agent_setup();
		wait([&] { return setup->projection().Detected && tab->state() == qt::agent_tab_state::setting_up; }, "the Agent tab is in its setup " + aWhen);
		expect(setup->projection().PolicyPending, "no host policy is observed " + aWhen + ": " + rows_text(setup->projection()));
		expect(setup->loading_state() != nullptr && setup->loading_state()->isVisibleTo(setup), "the drawer is its loading state " + aWhen);
		expect(setup->findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow")).isEmpty() && !setup->step_text().contains(QStringLiteral("Not allowed"))
				   && !setup->step_text().contains(QStringLiteral("aren't allowed")),
			   "and shows no agent card and no refusal " + aWhen);
	}

	struct course
	{
		QTemporaryDir                            Data;
		read_gate                                Gate;
		std::unique_ptr<app::github_application> Application;

		[[nodiscard]] auto agents() const -> std::filesystem::path { return std::filesystem::path{ Data.path().toStdString() } / "agents"; }
		[[nodiscard]] auto marked() const -> std::filesystem::path { return std::filesystem::path{ Data.path().toStdString() } / "agent-started"; }

		void start(const app::detail::github_public_path_fixture aFixture, const bool aTwoPullRequests = false)
		{
			auto dataRoot = e2e::private_data_root(Data);
			expect(dataRoot.has_value(), "a private data root");
			// Claude Code and Codex, found on PATH, and a wrapper of the
			// reviewer's own. Only Codex's model-list probe is expected to run.
			std::filesystem::create_directories(agents());
			for (const auto* const name : { "claude", "codex", "claude-wrapper" }) {
				const auto    program = agents() / name;
				std::ofstream script{ program };
				script << "#!/bin/sh\n";
				if (std::string_view{ name } == "codex") {
					script << R"(if [ "$#" -eq 1 ] && [ "$1" = app-server ]; then
    IFS= read -r initialize
    case "$initialize" in *'"initialize"'*) ;; *) exit 2 ;; esac
    printf '%s\n' '{"id":0,"result":{}}'
    IFS= read -r initialized
    IFS= read -r list
    case "$list" in *'"model/list"'*) ;; *) exit 3 ;; esac
    printf '%s\n' '{"id":1,"result":{"data":[{"model":"fixture-model","displayName":"Fixture Model"}],"nextCursor":null}}'
    exit 0
fi
)";
				}
				script << "echo \"$0 $*\" >> '" << marked().string() << "'\nexit 1\n";
				std::filesystem::permissions(program, std::filesystem::perms::owner_all);
			}
			auto options               = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
			options.ClaudeSearch       = app::detail::claude_search_environment{ .PathVariable = agents().string() };
			options.CodexSearch        = app::detail::claude_search_environment{ .PathVariable = agents().string() };
			options.RepeatReadSessions = true;
			// #17 and #18, for the move.
			options.IndependentWindowReads = aTwoPullRequests;
			options.BeforeFilesPage        = [this](const std::stop_token aStop) { Gate.before_files_page(aStop); };
			auto terminal                  = false;
			auto startup                   = access::start(aFixture, std::move(options), [&](auto aResult) {
				terminal = true;
				if (aResult) {
					Application = std::move(*aResult);
				}
			});
			wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
			expect(Application != nullptr, "startup succeeds");
			Application->show();
			auto& window = Application->window();
			window.resize(1440, 960);
			window.github_connect_action()->trigger();
			wait([&] { return window.github_open_action()->isEnabled(); }, "the fixture account connects");
		}

		void open(const QString& aPullRequest)
		{
			auto& window = Application->window();
			window.home_open_bar()->setText(aPullRequest);
			window.home_open_action()->trigger();
		}

		/// Settings › Agents › Add agent… › Custom agent…: one agent of the
		/// reviewer's own.
		void add_custom_agent(const std::string& aName, const core::agent_protocol aProtocol, const char* const aProgram)
		{
			auto&       window   = Application->window();
			auto* const programs = window.agent_programs();
			const auto  wrapper  = (agents() / aProgram).string();
			programs->set_editor([wrapper, aName, aProtocol](const core::agent_entry&, QWidget*) -> std::optional<core::agent_entry> {
				return core::agent_entry{ .Name = aName, .Protocol = aProtocol, .Program = wrapper };
			});
			programs->add_agent(std::nullopt);
			wait([&] { return std::ranges::any_of(window.agent_setup()->projection().Preferences.Agents, [&aName](const core::agent_entry& aEntry) { return aEntry.Name == aName; }); },
				 "the custom agent is added");
		}

		/// A Claude stream JSON wrapper and, [[BPR-2305]], a Codex JSONL
		/// launcher of the reviewer's own.
		void add_custom_agent()
		{
			add_custom_agent("Claude stream JSON", core::agent_protocol::claude_stream_json, "claude-wrapper");
			add_custom_agent("Codex launcher", core::agent_protocol::codex_jsonl_exec, "codex");
		}

		void stop()
		{
			Gate.release();
			if (Application) {
				Application->begin_shutdown();
				wait([&] { return Application->status().ShutdownComplete; }, "shutdown");
				Application.reset();
			}
		}
	};

	/// AC1, AC2, AC3: an Enterprise pull request whose read is held.
	void run_enterprise_course()
	{
		auto test = course{ };
		try {
			test.start(app::detail::github_public_path_fixture::ghes_3_18);
			auto& window = test.Application->window();
			test.add_custom_agent();
			// The composed Settings path has loaded the catalog before any
			// pull request is opened or question can be asked.
			wait([&] {
				const auto& setup    = window.agent_setup()->projection();
				const auto  launcher = std::ranges::find(setup.Preferences.Agents, "Codex launcher", &core::agent_entry::Name);
				if (launcher == setup.Preferences.Agents.end()) {
					return false;
				}
				const auto catalog = setup.Catalogs.find(launcher->Id);
				return catalog != setup.Catalogs.end() && catalog->second.Models.size() == 1U && catalog->second.Models.front().DisplayName == "Fixture Model";
			},
				 "the custom launcher lists models before opening the Agent tab");
			test.Gate.hold();
			test.open(QStringLiteral("octo-org/review-repository#17"));
			wait([&] { return test.Gate.entered(); }, "the pull request read reaches its files page and is held");
			window.open_agent_question();
			expect_loading(window, "while the Enterprise read is held");
			const auto& offered = window.agent_tab()->run_settings_source()->shown();
			const auto  picker  = std::ranges::find(offered, QStringLiteral("Codex launcher"), &qt::agent_run_settings::Agent);
			expect(picker != offered.end() && picker->Listing == qt::agent_model_listing::ready
					   && picker->Models == std::vector{ std::pair{ std::string{ "fixture-model" }, QStringLiteral("Fixture Model") } },
				   "the real picker has the launcher's models while the review still loads");

			test.Gate.release();
			wait([&] { return window.session_panel()->status() == core::github_status::pull_request_ready && !window.agent_setup()->projection().PolicyPending; },
				 "the read completes and the host's policy is observed");
			const auto& setup  = window.agent_setup()->projection();
			const auto  choice = [&setup](const std::string& aName) { return std::ranges::find(setup.Choices, aName, &core::agent_setup_choice::Name); };
			expect(choice("Claude stream JSON") != setup.Choices.end() && choice("Claude stream JSON")->Verdict.Permitted, "the custom agent is allowed: " + rows_text(setup));
			expect(choice("Codex") != setup.Choices.end() && choice("Codex")->Verdict.Permitted, "Codex is allowed on the Enterprise host: " + rows_text(setup));
			expect(choice("Codex launcher") != setup.Choices.end() && choice("Codex launcher")->Verdict.Permitted, "and so is an added Codex: " + rows_text(setup));
			expect(std::ranges::none_of(setup.State.Rows, [](const core::agent_setup_row& aRow) { return aRow.Agent == core::setup_agent::codex && !aRow.Verdict.Permitted; }),
				   "its found row is allowed too: " + rows_text(setup));
			expect(std::ranges::none_of(setup.State.Rows, [](const core::agent_setup_row& aRow) { return aRow.Agent == core::setup_agent::claude_code && !aRow.Verdict.Permitted; }),
				   "Claude Code is allowed: " + rows_text(setup));
			expect(qt::agent_verdict_words(choice("Codex")->Verdict, !setup.PolicyPending).isEmpty(), "and Codex says no refusal");
			expect(!window.agent_setup()->loading_state()->isVisibleTo(window.agent_setup()), "the loading state gives way to the steps");
			test.stop();
		} catch (...) {
			test.stop();
			throw;
		}
		expect(!std::filesystem::exists(test.marked()), "no agent program was started");
	}

	/// AC4: opening another pull request drops the previous review's policy
	/// at once. A fixture serves one host, so the next pull request is on the
	/// same host; what shows while it loads is the loading state, not the
	/// previous review's verdicts.
	void run_move_course()
	{
		auto test = course{ };
		try {
			test.start(app::detail::github_public_path_fixture::github_com, true);
			auto& window = test.Application->window();
			test.open(QStringLiteral("octo-org/review-repository#17"));
			wait([&] { return window.session_panel()->status() == core::github_status::pull_request_ready && !window.agent_setup()->projection().PolicyPending; },
				 "the first pull request is read and its host's policy observed");
			window.open_agent_question();
			wait([&] { return window.agent_setup()->projection().Detected; }, "the agents are looked for");
			const auto first = window.agent_setup()->projection();
			const auto codex = std::ranges::find(first.Choices, std::string{ "Codex" }, &core::agent_setup_choice::Name);
			expect(codex != first.Choices.end() && codex->Verdict.Permitted, "github.com allows Codex: " + rows_text(first));

			test.Gate.hold();
			test.open(QStringLiteral("octo-org/review-repository#18"));
			if (!e2e::wait_until_plain([&] { return test.Gate.entered(); }, std::chrono::milliseconds{ 15'000 })) {
				throw std::runtime_error{ "the next pull request's read is held; status " + std::to_string(static_cast<int>(window.session_panel()->status())) + ", home " + std::to_string(window.showing_home())
										  + ", bar " + window.home_open_bar()->text().toStdString() + ", " + rows_text(window.agent_setup()->projection()) };
			}
			const auto moving = window.agent_setup()->projection();
			expect(moving.PolicyPending && std::ranges::none_of(moving.Choices, [](const core::agent_setup_choice& aChoice) { return aChoice.Verdict.Permitted; }),
				   "while the next pull request loads, no agent shows the previous review's verdict: " + rows_text(moving));
			window.open_agent_question();
			expect_loading(window, "while the next read is held");
			test.Gate.release();
			wait([&] { return !window.agent_setup()->projection().PolicyPending; }, "the next pull request's policy is observed once it is taken");
			test.stop();
		} catch (...) {
			test.stop();
			throw;
		}
		expect(!std::filesystem::exists(test.marked()), "no agent program was started");
	}
} // namespace

auto main(int argc, char** argv) -> int
try {
	QApplication application{ argc, argv };
	run_enterprise_course();
	run_move_course();
	std::cout << "agent-setup-host-policy:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "agent-setup-host-policy: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
