/// \file
/// \brief [[BPR-1749]], RFC-BPR-0092 M2. The setup host on manual
/// lanes and fake ports: detection starts no process (Q2), the bounded status
/// check reaches the setup state from scripted answers (Q3 A), Stop and a
/// timeout end it, and the choice is kept.

#include "bpr/application/agent_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/operation_diagnosis.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::setup_agent;
	using core::setup_check_state;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A process port that answers each run from a script and records it.
	/// [[BPR-2320]]. Like the process launcher, it takes no request that
	/// names neither a working folder nor a folder of its own.
	class scripted_port final : public application::agent_process_port
	{
	public:
		[[nodiscard]] auto run(const application::agent_launch_request& aRequest, const int aTimeout, const application::agent_output_sink& = { }, std::stop_token = { })
			-> application::agent_run_result override
		{
			expect(application::names_agent_folder(aRequest), "every request the setup host runs names a working folder or is WritingOnly");
			const auto catalog = aRequest.Purpose == application::agent_launch_purpose::model_catalog;
			(catalog ? CatalogRequests : Requests).push_back(aRequest);
			if (!catalog) {
				Timeouts.push_back(aTimeout);
			}
			auto& answers = catalog ? CatalogAnswers : Answers;
			auto  result  = application::agent_run_result{ };
			if (!answers.empty()) {
				result = answers.front();
				answers.erase(answers.begin());
			}
			result.Session = aRequest.Session;
			return result;
		}
		void cancel() noexcept override { ++Cancels; }

		std::vector<application::agent_launch_request> Requests;
		std::vector<int>                               Timeouts;
		std::vector<application::agent_run_result>     Answers;
		std::vector<application::agent_launch_request> CatalogRequests;
		std::vector<application::agent_run_result>     CatalogAnswers;
		int                                            Cancels = 0;
	};

	[[nodiscard]] auto said(const application::agent_run_outcome aOutcome, const std::string& aText, std::optional<int> aExit = 0) -> application::agent_run_result
	{
		auto result     = application::agent_run_result{ };
		result.Outcome  = aOutcome;
		result.ExitCode = aExit;
		if (!aText.empty()) {
			auto event = application::agent_stream_event{ };
			event.Text = aText;
			result.Events.push_back(std::move(event));
		}
		return result;
	}

	class memory_store final : public application::agent_setup_store_port
	{
	public:
		[[nodiscard]] auto load() -> std::optional<application::agent_setup_preferences> override
		{
			++Loads;
			return Stored;
		}
		[[nodiscard]] auto store(const application::agent_setup_preferences& aPreferences) -> bool override
		{
			Stored = aPreferences;
			++Stores;
			return true;
		}

		std::optional<application::agent_setup_preferences> Stored = application::agent_setup_preferences{ };
		int                                                 Loads  = 0;
		int                                                 Stores = 0;
	};

	/// [[BPR-1828]]. Keeps every diagnostic event a check records.
	class recording_log final : public application::diagnostic_log_port
	{
	public:
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }

		std::vector<application::diagnostic_event> Events;
	};

	constexpr auto unreadable_file = std::string_view{ "/home/me/profile/state/v1/agent-setup.sqlite3" };

	/// The kept agent list holding one Claude Code entry whose program is a wrapper.
	[[nodiscard]] auto kept_wrapper(const std::string& aPath) -> application::agent_setup_preferences
	{
		auto entry       = core::preset_agent_entry(setup_agent::claude_code, core::codex_protocol_choice::app_server, aPath);
		entry.Id         = 1U;
		auto kept        = application::agent_setup_preferences{ };
		kept.Agents      = { entry };
		kept.AgentsMoved = true;
		return kept;
	}

	struct fixture
	{
		std::shared_ptr<recording_log>                     Log = std::make_shared<recording_log>();
		application::manual_executor                       Effects;
		application::manual_executor                       Checks;
		application::manual_executor                       Decision;
		scripted_port                                      Process;
		memory_store                                       Store;
		std::shared_ptr<application::agent_program_source> Programs = std::make_shared<application::agent_program_source>();
		std::optional<core::agent_search_candidate>        Claude   = core::agent_search_candidate{ .Path = "/opt/homebrew/bin/claude" };
		std::optional<core::agent_search_candidate>        Codex;
		std::optional<core::agent_wrapper_refusal>         Refusal;
		std::vector<std::string>                           Inspected;
		std::vector<application::agent_setup_projection>   Published;
		std::unique_ptr<application::agent_setup_host>     Host;
		std::int64_t                                       Now = 40'000;

		explicit fixture(const bool aObservePolicy = true)
		{
			Host = std::make_unique<application::agent_setup_host>(application::agent_setup_dependencies{
				.Store     = &Store,
				.StoreFile = std::string{ unreadable_file },
				.Process   = &Process,
				.Locate    = [this](const core::agent_program aProgram) { return aProgram == core::agent_program::claude ? Claude : Codex; },
				.Inspect =
					[this](const std::string_view aPath) {
						Inspected.emplace_back(aPath);
						return Refusal;
					},
				.Programs = Programs,
				.Parent   = [](const std::string& aName) -> std::optional<std::string> {
					return aName == "HOME" ? std::optional<std::string>{ "/home/me" } : std::nullopt;
				},
				.Profiles           = core::built_in_agent_profiles(),
				.Effects            = &Effects,
				.Checks             = &Checks,
				.Decision           = &Decision,
				.Publish            = [this](const application::agent_setup_projection& aProjection) { Published.push_back(aProjection); },
				.Log                = Log,
				.SteadyMilliseconds = [this] { return Now; },
			});
			if (aObservePolicy) {
				Host->observe_policy(core::built_in_agent_policy(core::git_host_id{ 1U }, core::built_in_agent_host_kind::github_com));
			}
		}

		~fixture() { Host->shutdown(); }

		void settle()
		{
			for (auto turns = 0; turns < 32 && (Effects.pending() != 0U || Checks.pending() != 0U || Decision.pending() != 0U); ++turns) {
				Effects.drain();
				Checks.drain();
				Decision.drain();
			}
		}

		[[nodiscard]] auto state() const -> application::agent_setup_projection { return Host->projection(); }
	};

	void test_detection_starts_nothing()
	{
		auto f = fixture{ };
		f.Host->load();
		f.Host->detect();
		expect(f.state().Detecting, "detection is shown as running until it answers");
		f.settle();
		const auto state = f.state();
		expect(f.Process.Requests.empty(), "showing setup and detecting start no process (Q2)");
		expect(state.Detected && !state.Detecting, "detection answered");
		expect(state.State.Rows.size() == 1U && state.State.Rows.front().Agent == setup_agent::claude_code, "the found claude is the one row");
		expect(state.State.Chosen == setup_agent::claude_code, "one found agent is chosen without asking");
		expect(state.Step == core::setup_step::repository, "with an agent chosen the step is Repository");
		expect(state.Searched == std::vector{ setup_agent::claude_code, setup_agent::codex }, "github.com looked for Claude and Codex");
		expect(f.Store.Loads == 1, "preferences are read once");

		f.Claude = std::nullopt;
		f.Host->detect();
		f.settle();
		expect(f.state().State.Rows.empty() && !f.state().State.Chosen && f.state().Step == core::setup_step::agent, "Look again with nothing found clears the rows");
		expect(f.Process.Requests.empty(), "still no process");
	}

	void test_saved_programs_win()
	{
		auto f         = fixture{ };
		f.Codex        = core::agent_search_candidate{ .Path = "/usr/local/bin/codex", .Origin = core::agent_found_in::usual_folder };
		f.Store.Stored = kept_wrapper("/opt/tools/claude-gateway");
		f.Host->load();
		f.settle();
		auto answered = std::optional<std::optional<core::agent_wrapper_refusal>>{ };
		f.Host->set_program(setup_agent::plain_text_harness, "/opt/corp/harness", [&answered](const std::optional<core::agent_wrapper_refusal> aRefusal) { answered = aRefusal; });
		f.settle();
		expect(answered && !*answered, "an inspectable harness program is accepted");
		expect(f.Programs->current(setup_agent::plain_text_harness) == "/opt/corp/harness", "the window's launches get the harness program");
		expect(f.Store.Stored && f.Store.Stored->HarnessProgram == "/opt/corp/harness", "and it is saved for the profile");
		const auto rows = f.state().State.Rows;
		expect(rows.size() == 3U, "Claude by its wrapper, Codex found, and the chosen harness");
		expect(rows[0].Path == "/opt/tools/claude-gateway" && rows[0].How == core::agent_found_how::saved_program, "a saved wrapper is Claude's row and wins over the search");
		expect(rows[1].How == core::agent_found_how::usual_folder, "Codex found in a usual folder says so");
		expect(!f.state().State.Chosen, "three found agents wait for the reviewer");

		f.Refusal = core::agent_wrapper_refusal::not_executable;
		answered.reset();
		f.Host->set_program(setup_agent::codex, "/opt/tools/broken-codex", [&answered](const std::optional<core::agent_wrapper_refusal> aRefusal) { answered = aRefusal; });
		f.settle();
		expect(answered && *answered == core::agent_wrapper_refusal::not_executable, "a refused Codex program says why");
		expect(f.Store.Stored->CodexProgram.empty() && f.Programs->current(setup_agent::codex).empty(), "and is not saved or used");
		expect(f.Process.Requests.empty(), "setting programs starts no process");
	}

	void test_status_check()
	{
		auto f = fixture{ };
		f.Host->load();
		f.Host->detect();
		f.settle();
		f.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": true, "authMethod": "claude.ai"})"),
							  said(application::agent_run_outcome::success, "2.1.296 (Claude Code)") };
		f.Host->check();
		expect(f.state().Checking && f.state().State.AgentCheck.State == setup_check_state::running, "the check is shown running");
		f.settle();
		expect(f.Process.Requests.size() == 2U, "one status run and one version run");
		const auto& status = f.Process.Requests.front();
		expect(status.Executable == "/opt/homebrew/bin/claude" && status.Arguments == std::vector<std::string>{ "auth", "status", "--json" },
			   "the found claude's own status command");
		expect(status.Input.empty() && status.WritingOnly && status.Protocol == core::agent_protocol::external_terminal,
			   "no question, no context, an empty folder of its own, output read as lines");
		expect(f.Process.Requests.back().Arguments == std::vector<std::string>{ "--version" }, "then its version");
		expect(f.Process.Timeouts == std::vector<int>{ application::agent_status_timeout_milliseconds, application::agent_status_timeout_milliseconds },
			   "each run is bounded");
		for (const auto& [name, value] : status.Environment) {
			expect(name != "GITHUB_TOKEN" && name != "ANTHROPIC_API_KEY", "no credential reaches the check");
		}
		const auto passed = f.state();
		expect(passed.State.AgentCheck.State == setup_check_state::passed && passed.State.AgentCheck.Version == "2.1.296", "signed in passes with its version");
		expect(f.Store.Stored && f.Store.Stored->Chosen == setup_agent::claude_code, "a passed check of setup's own choice saves it");

		f.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": false})") };
		f.Host->restart();
		f.settle();
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::not_signed_in, "not signed in is its own failure");
		expect(!f.state().CheckDetail.empty(), "a failed check keeps bounded details");

		f.Process.Answers = { said(application::agent_run_outcome::success, "Welcome!") };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::status_unreadable, "an answer that is neither is never taken as signed in");

		f.Process.Answers = { said(application::agent_run_outcome::failed_to_start, "", std::nullopt) };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::failed_to_start, "a program that would not start fails the check");

		f.Process.Answers = { said(application::agent_run_outcome::timeout, "", std::nullopt) };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::timed_out, "a run past its bound fails the check as timed out");
	}

	void test_auto_chosen_wrapper_keeps_its_entry()
	{
		auto f         = fixture{ };
		f.Store.Stored = kept_wrapper("/opt/corp/bedrock-wrapper");
		f.Host->load();
		f.Host->detect();
		f.settle();
		auto entry      = f.state().Preferences.Agents.front();
		entry.Arguments = { "--no-extra-output" };
		f.Host->update_agent(entry);
		f.settle();
		expect(f.state().Preferences.PreferredAgent == 0U, "an auto-chosen wrapper can have no saved preferred entry yet");
		f.Process.Answers = { said(application::agent_run_outcome::success,
								   R"({
  "loggedIn": true,
  "authMethod": "third_party",
  "apiProvider": "bedrock",
  "analyticsDisabled": true,
  "projectsDirectory": "/home/fixture/.claude/projects",
  "configDirectory": "/home/fixture/.claude"
})"),
							  said(application::agent_run_outcome::success, "2.1.296 (Claude Code)") };
		f.Host->check();
		f.settle();
		expect(f.Process.Requests.size() == 2U && f.Process.Requests.front().Executable == entry.Program
				   && f.Process.Requests.front().Arguments == std::vector<std::string>{ "--no-extra-output", "auth", "status", "--json" },
			   "an auto-chosen wrapper keeps its saved entry's fixed arguments for the status check");
		expect(f.Process.Requests.back().Arguments == std::vector<std::string>{ "--no-extra-output", "--version" }, "the version check keeps the same fixed arguments");
		expect(f.state().ChecksWrapper && f.state().State.AgentCheck.State == setup_check_state::passed && f.state().State.AgentCheck.Version == "2.1.296", "the owner's pretty third-party Bedrock status is accepted without a normal Claude login");
		expect(f.state().Preferences.PreferredAgent == entry.Id && f.Store.Stored->PreferredAgent == entry.Id, "successful setup saves the exact checked entry for subsequent launches");

		entry.Program = "/opt/corp/second-wrapper";
		f.Host->update_agent(entry);
		f.settle();
		f.Process.Answers = { said(application::agent_run_outcome::success, "Welcome!") };
		f.Host->check();
		f.settle();
		expect(f.Process.Requests.back().Executable == entry.Program && f.Process.Requests.back().Arguments == std::vector<std::string>{ "--no-extra-output", "auth", "status", "--json" }, "an updated effective entry's program and arguments are checked");
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::status_unreadable, "a wrapper's unreadable response is still never accepted as signed in");
	}

	void test_reset_does_not_resurrect_saved_wrapper()
	{
		auto f         = fixture{ };
		f.Store.Stored = kept_wrapper("/opt/corp/saved-wrapper");
		f.Host->load();
		f.Host->detect();
		f.settle();
		expect(f.state().State.Rows.front().Path == "/opt/corp/saved-wrapper", "the saved wrapper is Claude's row");
		auto entry      = f.state().Preferences.Agents.front();
		entry.Arguments = { "--no-extra-output" };
		f.Host->update_agent(entry);
		f.settle();
		f.Host->reset();
		f.Host->restart();
		f.settle();
		expect(f.state().Preferences.Agents.empty() && f.state().Preferences.AgentsMoved && f.state().State.Rows.front().Path == f.Claude->Path
				   && f.state().State.Rows.front().How != core::agent_found_how::saved_program,
			   "Reset detects ordinary Claude without resurrecting the saved wrapper after dropping its fixed args");
		const auto saved = f.Store.Stored;
		f.Host->shutdown();
		auto next         = fixture{ };
		next.Store.Stored = saved;
		next.Host->load();
		next.Host->detect();
		next.settle();
		expect(next.state().Preferences.Agents.empty() && next.state().State.Rows.front().Path == next.Claude->Path, "a new host over the persisted reset never rediscovers the old wrapper");
	}

	void test_setup_details_are_bounded_and_separate()
	{
		auto f = fixture{ };
		f.Host->load();
		f.Host->detect();
		f.settle();
		auto result          = said(application::agent_run_outcome::nonzero_exit, std::string(8U * 1024U, 'x') + " last stdout", 7);
		result.StandardError = "SSO fixture expired";
		f.Process.Answers    = { result };
		f.Host->check();
		f.settle();
		expect(f.state().CheckDetail.size() <= application::maximum_agent_check_detail_bytes && f.state().CheckDetail.starts_with("nonzero_exit (exit 7)")
				   && f.state().CheckDetail.ends_with("SSO fixture expired"),
			   "bounded agent output retains the outcome, exit code and final error");
		const auto agentDetail = f.state().CheckDetail;
		f.Host->set_workspace_check(core::fail_check(core::agent_setup_failure::workspace_unavailable), std::string(8U * 1024U, 'g') + " Git fixture failed");
		expect(f.state().WorkspaceDetail.size() == application::maximum_agent_check_detail_bytes && f.state().WorkspaceDetail.ends_with("Git fixture failed")
				   && f.state().CheckDetail == agentDetail,
			   "bounded workspace log is separate from the agent's diagnostic");
		f.Host->set_workspace_check({ .State = setup_check_state::running });
		expect(f.state().WorkspaceDetail.empty(), "a new workspace check clears old output");
		// [[BPR-2215]] (owner, 0.5.0 build 76). The running check keeps the
		// instant it started across projections, and loses it when it ends.
		expect(f.state().WorkspaceCheckStartedMs == std::optional<std::int64_t>{ 40'000 }, "a running workspace check carries when it started");
		f.Now = 52'000;
		f.Host->set_repository_chosen(!f.state().State.RepositoryChosen);
		expect(f.state().WorkspaceCheckStartedMs == std::optional<std::int64_t>{ 40'000 }, "another projection keeps that start");
		f.Host->set_workspace_check({ .State = setup_check_state::passed });
		expect(!f.state().WorkspaceCheckStartedMs, "an ended check has no start");
		f.Host->set_workspace_check({ .State = setup_check_state::running });
		expect(f.state().WorkspaceCheckStartedMs == std::optional<std::int64_t>{ 52'000 }, "the next check starts its own time");
		f.Host->restart();
		expect(f.state().CheckDetail.empty() && f.state().WorkspaceDetail.empty(), "restart clears both details");
		f.settle();
	}

	void test_codex_and_stop()
	{
		auto f   = fixture{ };
		f.Claude = std::nullopt;
		f.Codex  = core::agent_search_candidate{ .Path = "/Users/me/.local/bin/codex", .Origin = core::agent_found_in::usual_folder };
		f.Host->load();
		f.Host->detect();
		f.settle();
		f.Process.Answers = { said(application::agent_run_outcome::nonzero_exit, "Not logged in", 1) };
		f.Host->check();
		f.settle();
		expect(f.Process.Requests.size() == 1U && f.Process.Requests.front().Arguments == std::vector<std::string>{ "login", "status" }, "Codex's own status command");
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::not_signed_in, "Codex not logged in is not signed in");

		f.Process.Answers = { said(application::agent_run_outcome::success, "Logged in using ChatGPT"), said(application::agent_run_outcome::success, "codex-cli 0.163.0") };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.State == setup_check_state::passed && f.state().State.AgentCheck.Version == "0.163.0", "Codex logged in passes with its version");

		const auto runs = f.Process.Requests.size();
		f.Host->check();
		expect(f.state().Checking, "a running check");
		f.Host->stop();
		expect(!f.state().Checking && f.state().State.AgentCheck.State == setup_check_state::not_run && f.Process.Cancels >= 1, "Stop cancels and resets the row");
		f.settle();
		expect(f.state().State.AgentCheck.State == setup_check_state::not_run, "a stopped check's late answer is not taken");
		expect(f.Process.Requests.size() >= runs, "stop never runs anything new");
	}

	/// [[BPR-1828]]. Each check records its start and its end as
	/// `agent.setup_check` lines: the typed failure, the last run's outcome
	/// and exit code, the run count and the duration; never the program path
	/// or what the agent printed. Stop records a cancellation.
	void test_check_diagnostics()
	{
		auto f = fixture{ };
		f.Host->load();
		f.Host->detect();
		f.settle();
		const auto lines = [&f] {
			auto result = std::vector<std::string>{ };
			for (const auto& event : f.Log->Events) {
				expect(event.Operation == application::diagnostic_operation::agent_setup_check, "every record is an agent setup check");
				const auto text = std::string{ event.Summary.text() };
				expect(text.find("/opt/homebrew") == std::string::npos && text.find("claude.ai") == std::string::npos && text.find("2.1.296") == std::string::npos, "no path or output reaches a record");
				expect(core::parse_operation_diagnosis(text).has_value(), "every record is exactly an operation line");
				result.push_back(text);
			}
			return result;
		};
		f.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": true, "authMethod": "claude.ai"})"),
							  said(application::agent_run_outcome::success, "2.1.296 (Claude Code)") };
		f.Host->check();
		expect(f.Log->Events.size() == 1U && f.Log->Events[0].Outcome == application::diagnostic_outcome::started, "a check records its start when it begins");
		f.settle();
		auto seen = lines();
		expect(seen.size() == 2U && f.Log->Events[1].Outcome == application::diagnostic_outcome::succeeded, "and its success when it passes");
		const auto passed = core::parse_operation_diagnosis(seen[1]);
		expect(passed && passed->Action == core::operation_action::agent_status_check && passed->Process == core::operation_process::success && passed->ExitCode == 0 && passed->Count == 2U,
			   "a passed check names its status and version runs and the last exit code");

		f.Process.Answers = { said(application::agent_run_outcome::nonzero_exit, R"({"loggedIn": false})", 1) };
		f.Host->check();
		f.settle();
		seen = lines();
		expect(seen.size() == 4U && f.Log->Events[3].Outcome == application::diagnostic_outcome::failed, "a failed check records a failure");
		expect(seen[3].starts_with("action=agent_status_check failure=not_signed_in process=nonzero_exit exit=1 "), "with its typed failure, run outcome and exit code");

		f.Host->check();
		f.Host->stop();
		f.settle();
		seen = lines();
		expect(seen.size() == 6U && f.Log->Events[5].Outcome == application::diagnostic_outcome::cancelled && seen[5].starts_with("action=agent_status_check failure=cancelled "),
			   "Stop records a cancellation, and the late answer records nothing more");
	}

	void test_choice_and_steps()
	{
		auto f  = fixture{ };
		f.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		f.Host->load();
		f.Host->detect();
		f.settle();
		expect(!f.state().State.Chosen, "two found agents wait for the reviewer");
		f.Host->choose(setup_agent::codex);
		f.settle();
		expect(f.state().State.Chosen == setup_agent::codex && f.Store.Stored->Chosen == setup_agent::codex, "the reviewer's choice is shown and saved");
		f.Host->set_codex_protocol(core::codex_protocol_choice::jsonl);
		f.settle();
		expect(f.state().State.Rows.back().Profile == core::codex_jsonl_profile_id && f.Store.Stored->CodexProtocol == core::codex_protocol_choice::jsonl,
			   "the Codex protocol setting follows into the row and the store");
		f.Host->set_repository_chosen(true);
		expect(f.state().Step == core::setup_step::check, "a chosen repository moves to Check it works");
		f.Host->set_workspace_check({ .State = setup_check_state::passed });
		f.Process.Answers = { said(application::agent_run_outcome::success, "Logged in using ChatGPT"), said(application::agent_run_outcome::success, "codex-cli 0.163.0") };
		f.Host->check();
		f.settle();
		expect(f.state().Step == core::setup_step::ready, "both rows passed: Ready");

		auto again         = fixture{ };
		again.Store.Stored = f.Store.Stored;
		again.Codex        = f.Codex;
		again.Host->load();
		again.Host->detect();
		again.settle();
		expect(again.state().State.Chosen == setup_agent::codex, "a new window opens on the saved agent");
		expect(again.Programs->current(setup_agent::codex).empty(), "no Codex program was saved, so launches search");

		auto ghes  = fixture{ };
		ghes.Codex = f.Codex;
		ghes.Host->observe_policy(core::built_in_agent_policy(core::git_host_id{ 2U }, core::built_in_agent_host_kind::explicit_ghes));
		ghes.Host->detect();
		ghes.settle();
		// [[BPR-2305]] (owner, 2026-11-19). Explicit GHES permits Codex.
		expect(!ghes.state().State.Chosen && ghes.state().State.Rows.back().Verdict.Permitted, "on GHES Codex is listed and permitted, so two agents wait for the reviewer");
		ghes.Host->choose(setup_agent::codex);
		ghes.settle();
		expect(ghes.state().State.Chosen == setup_agent::codex, "and choosing Codex there takes it");

		auto narrowed  = fixture{ };
		narrowed.Codex = f.Codex;
		narrowed.Host->observe_policy(core::agent_host_policy{ .Host = core::git_host_id{ 3U }, .Allowed = { core::claude_stream_json_profile_id } });
		narrowed.Host->detect();
		narrowed.settle();
		expect(narrowed.state().State.Chosen == setup_agent::claude_code && !narrowed.state().State.Rows.back().Verdict.Permitted, "on a host that forbids it Codex is listed, refused, and never chosen");
		narrowed.Host->choose(setup_agent::codex);
		narrowed.settle();
		expect(narrowed.state().State.Chosen == setup_agent::claude_code, "choosing a refused agent does not take it");
		narrowed.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": true})") };
		narrowed.Host->check();
		narrowed.settle();
		expect(!narrowed.Process.Requests.empty() && narrowed.Process.Requests.front().Arguments.front() == "auth", "only the permitted agent is checked");
	}

	/// [[BPR-1753]], owner amendment 2026-10-23. The single settings
	/// are not moved into the agent list; a custom agent is added and chosen, and
	/// its program and fixed arguments are what setup checks.
	void test_the_agent_list()
	{
		auto f                       = fixture{ };
		f.Store.Stored->Chosen       = setup_agent::claude_code;
		f.Store.Stored->ClaudeEffort = "high";
		f.Host->load();
		f.settle();
		expect(f.state().Preferences.Agents.empty() && f.state().Preferences.AgentsMoved && f.Store.Stored->AgentsMoved && f.Store.Stored->Agents.empty(),
			   "settings kept before agents were a list are not read into it (owner, 2026-11-20)");
		auto again         = fixture{ };
		again.Store.Stored = f.Store.Stored;
		again.Host->load();
		again.settle();
		expect(again.state().Preferences.Agents.empty(), "nor at the next load");
		// Owner look v004: the Agent tab's Suggestions toggle is kept.
		expect(!again.state().Preferences.SuggestionsHidden, "the suggestions show by default");
		again.Host->set_suggestions_hidden(true);
		again.settle();
		expect(again.state().Preferences.SuggestionsHidden && again.Store.Stored->SuggestionsHidden, "hiding them is kept for the profile");
		// RFC-BPR-0126 Q4/Q5 as answered: a follow-up is queued by default;
		// Steer is a setting kept for the profile.
		expect(!again.state().Preferences.FollowUpSteer, "follow-ups queue by default");
		again.Host->set_follow_up_steer(true);
		again.settle();
		expect(again.state().Preferences.FollowUpSteer && again.Store.Stored->FollowUpSteer, "Steer is kept for the profile");

		auto answer = std::optional<std::optional<core::agent_entry_refusal>>{ };
		again.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		again.Host->add_agent({ .Name = "Work Codex", .Protocol = core::agent_protocol::codex_app_server, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } }, true,
							  [&answer](const std::optional<core::agent_entry_refusal> aRefusal) { answer = aRefusal; });
		again.settle();
		expect(answer == std::optional<std::optional<core::agent_entry_refusal>>{ std::in_place, std::nullopt } && again.state().Preferences.Agents.size() == 1U
				   && again.state().State.Chosen == setup_agent::codex && again.state().Preferences.PreferredAgent == again.state().Preferences.Agents.front().Id,
			   "a custom agent added from setup is chosen");
		const auto rows = again.state().State.Rows;
		const auto row  = std::ranges::find(rows, setup_agent::codex, &core::agent_setup_row::Agent);
		expect(row != rows.end() && row->Path == "/opt/work/bin/wrapper", "its program is the Codex row's, in place of the codex found");
		// Owner decision 2026-10-23: a custom agent's check is that its program
		// can be launched; nothing runs unless it names a check of its own.
		again.Host->check();
		again.settle();
		expect(again.Process.Requests.empty() && again.state().State.AgentCheck.State == setup_check_state::passed && again.Inspected.back() == "/opt/work/bin/wrapper",
			   "a custom agent with no check of its own passes once its program can be launched, running nothing");
		expect(!again.state().ChecksSignIn, "the projection distinguishes program availability from authentication");
		auto own           = again.state().Preferences.Agents.front();
		own.CheckArguments = { "--codex", "login", "status" };
		again.Host->update_agent(own);
		again.settle();
		again.Process.Answers = { said(application::agent_run_outcome::nonzero_exit, "Not logged in", 1) };
		again.Host->check();
		again.settle();
		expect(again.Process.Requests.size() == 1U && again.Process.Requests.front().Executable == "/opt/work/bin/wrapper"
				   && again.Process.Requests.front().Arguments == std::vector<std::string>{ "--codex", "login", "status" } && again.state().State.AgentCheck.State == setup_check_state::failed,
			   "its own check runs its program with exactly those arguments, and must exit 0");
		again.Process.Answers = { said(application::agent_run_outcome::success, "Logged in") };
		again.Host->check();
		again.settle();
		expect(again.state().State.AgentCheck.State == setup_check_state::passed, "and passes when it does");
		auto preset           = core::preset_agent_entry(setup_agent::claude_code, core::codex_protocol_choice::app_server, { });
		preset.Id             = 1U;
		preset.CheckArguments = { "--version" };
		expect(core::inspect_agent_entry(preset) == core::agent_entry_refusal::invalid_argument, "a popular agent keeps its sign-in check and takes no check of its own");
		answer = std::nullopt;
		again.Host->add_agent({ .Name = "Relative", .Protocol = core::agent_protocol::claude_stream_json, .Program = "wrapper" }, false,
							  [&answer](const std::optional<core::agent_entry_refusal> aRefusal) { answer = aRefusal; });
		expect(answer == std::optional<std::optional<core::agent_entry_refusal>>{ core::agent_entry_refusal::invalid_program } && again.state().Preferences.Agents.size() == 1U,
			   "a refused agent is answered with why and kept nowhere");
		again.Host->choose(setup_agent::claude_code);
		again.settle();
		expect(again.state().Preferences.Agents.size() == 2U && again.state().Preferences.Agents.back().Preset == setup_agent::claude_code
				   && again.state().Preferences.PreferredAgent == again.state().Preferences.Agents.back().Id,
			   "picking a popular agent in setup adds it to the list and chooses it");
	}

	/// [[BPR-2305]]. A custom Codex JSONL agent's check asks it one short
	/// question with a rule. A fixture launcher that drops developer
	/// instructions fails the check and the setup offers the prompt form;
	/// switched, the check passes and names the launcher's version. A
	/// launcher whose output is not Codex's JSON lines is named with its
	/// version, not as a decoder error.
	void test_a_custom_codex_check_proves_its_instructions_arrive()
	{
		auto f  = fixture{ };
		f.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		f.Host->load();
		f.settle();
		f.Host->add_agent({ .Name = "Launcher Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/launcher", .Arguments = { "--no-extra-output", "--codex" }, .CheckArguments = { "--version" } }, true);
		f.settle();
		expect(f.state().Preferences.Agents.size() == 1U, "the custom Codex is added");
		const auto id       = f.state().Preferences.Agents.front().Id;
		auto       answered = said(application::agent_run_outcome::success, "I am ready.");
		auto       followed = said(application::agent_run_outcome::success, "I am ready.\nMARIGOLD-4127");
		f.Process.Answers   = { said(application::agent_run_outcome::success, "launcher 0.7.0-pre"), answered, followed };
		f.Host->check();
		f.settle();
		expect(f.Process.Requests.size() == 3U, "its own check, then the question as developer instructions, then at the start of the prompt");
		const auto& asDeveloper = f.Process.Requests[1];
		expect(asDeveloper.Protocol == core::agent_protocol::codex_jsonl_exec && asDeveloper.Executable == "/opt/work/bin/launcher" && asDeveloper.Arguments.front() == "--no-extra-output"
				   && std::ranges::any_of(asDeveloper.Arguments, [](const std::string& aArgument) { return aArgument.starts_with("developer_instructions="); }),
			   "the first question goes through the launcher with the rule as developer instructions");
		expect(f.Process.Requests[2].Input.starts_with(application::codex_instructions_probe_rule), "the second has the rule at the start of the prompt");
		expect(f.state().State.AgentCheck.State == setup_check_state::failed && f.state().State.AgentCheck.Failure == core::agent_setup_failure::instructions_dropped
				   && f.state().InstructionsSwitch == std::optional{ id },
			   "a launcher that drops developer instructions fails the check, and setup offers the prompt form");

		auto switched         = f.state().Preferences.Agents.front();
		switched.Instructions = core::agent_instruction_delivery::message;
		f.Host->update_agent(switched);
		f.settle();
		f.Process.Requests.clear();
		f.Process.Answers = { said(application::agent_run_outcome::success, "launcher 0.7.0-pre"), followed };
		f.Host->check();
		f.settle();
		expect(f.Process.Requests.size() == 2U && f.Process.Requests[1].Input.starts_with(application::codex_instructions_probe_rule), "switched, it is asked once, with the rule in the prompt");
		expect(f.state().State.AgentCheck.State == setup_check_state::passed && f.state().State.AgentCheck.Version == "launcher 0.7.0-pre" && !f.state().InstructionsSwitch,
			   "and passes, naming the version its own check printed");
		expect(f.state().CheckedEntry == std::optional{ id } && f.state().CheckStep == application::agent_check_step::instructions_probe, "the result names the entry it checked and its probe (BPR-2320)");
		expect(std::ranges::all_of(f.Process.Requests, [](const application::agent_launch_request& aRequest) { return aRequest.WritingOnly; }), "its check and its probe each run in an empty folder of their own");

		auto unreadable            = application::agent_run_result{ };
		unreadable.Outcome         = application::agent_run_outcome::protocol_failure;
		unreadable.ProtocolFailure = application::agent_protocol_failure::malformed_frame;
		f.Process.Requests.clear();
		f.Process.Answers = { said(application::agent_run_outcome::success, "launcher 0.6.2"), unreadable };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::output_unreadable && f.state().CheckDetail.starts_with("Ran: /opt/work/bin/launcher --no-extra-output --codex …and the check's own\nversion: launcher 0.6.2\n"),
			   "output that is not Codex's JSON lines is said as that, with the launcher's version");
	}

	/// [[BPR-2320]]. A custom Claude agent's check asks one short stream
	/// JSON turn through its program. A program that prints plain text and
	/// exits 0 fails with the protocol named, its first line and its
	/// version; one that answers with Claude's init frame passes. A failed
	/// own check names that step, and the result names the checked entry
	/// when another is preferred.
	void test_a_custom_claude_check_proves_it_answers_in_stream_json()
	{
		auto f = fixture{ };
		f.Host->load();
		f.settle();
		f.Host->add_agent({ .Name = "Launcher Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/launcher", .Arguments = { "--no-extra-output", "--skip-skills-upgrade" }, .CheckArguments = { "--version" } }, true);
		f.settle();
		const auto id = f.state().Preferences.Agents.front().Id;

		auto plain               = said(application::agent_run_outcome::protocol_failure, { });
		plain.ProtocolFailure    = application::agent_protocol_failure::malformed_frame;
		plain.RejectedOutputLine = "Welcome to the launcher";
		f.Process.Answers        = { said(application::agent_run_outcome::success, "launcher 0.7.0-pre"), plain };
		f.Host->check();
		f.settle();
		expect(f.Process.Requests.size() == 2U && f.Process.Requests[1].Protocol == core::agent_protocol::claude_stream_json && f.Process.Requests[1].Arguments.front() == "--no-extra-output"
				   && f.Process.Requests[1].WritingOnly,
			   "its own check, then one stream JSON turn through its program, in an empty folder");
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::output_unreadable && f.state().CheckDetail.starts_with("Ran: /opt/work/bin/launcher --no-extra-output --skip-skills-upgrade …and the check's own\nversion: launcher 0.7.0-pre\nwrote: Welcome to the launcher\n")
				   && f.state().CheckStep == application::agent_check_step::protocol_probe && f.state().CheckedEntry == std::optional{ id },
			   "plain text fails as not the protocol, with its first line and its version");

		f.Process.Requests.clear();
		f.Process.Answers = { said(application::agent_run_outcome::success, "launcher 0.7.0-pre"), said(application::agent_run_outcome::success, { }) };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::output_unreadable && f.state().CheckDetail.find("wrote: nothing\n") != std::string::npos,
			   "a program that writes nothing and exits 0 fails the same way");

		auto answered = said(application::agent_run_outcome::success, "I am ready.");
		answered.Metadata.ProtocolReported.push_back({ .Basis = "claude_stream_json.system/init: session_id,model,claude_code_version" });
		f.Process.Answers = { said(application::agent_run_outcome::success, "launcher 0.7.0-pre"), answered };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.State == setup_check_state::passed && f.state().State.AgentCheck.Version == "launcher 0.7.0-pre", "an answer with Claude's init frame passes");

		f.Process.Answers = { said(application::agent_run_outcome::failed_to_start, { }, std::nullopt) };
		f.Host->check();
		f.settle();
		expect(f.state().State.AgentCheck.Failure == core::agent_setup_failure::failed_to_start && f.state().CheckStep == application::agent_check_step::check_command,
			   "a check command that did not start is named as that step");

		// A Codex entry checked while a Claude one, never checked, is
		// preferred: the owner's report, where the Codex failure was named
		// "Claude Code".
		auto g  = fixture{ };
		g.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		g.Host->load();
		g.settle();
		g.Host->add_agent({ .Name = "Launcher Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/launcher" }, true);
		g.Host->add_agent({ .Name = "Launcher Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/launcher", .Arguments = { "--codex" } }, false);
		g.settle();
		const auto claude = g.state().Preferences.Agents.front().Id;
		const auto codex  = g.state().Preferences.Agents.back().Id;
		expect(codex != claude && g.state().Preferences.PreferredAgent == claude, "the Claude entry is preferred");
		g.Process.Answers = { said(application::agent_run_outcome::failed_to_start, { }, std::nullopt) };
		g.Host->check_entry(codex);
		g.settle();
		expect(g.Process.Requests.size() == 1U && g.Process.Requests.front().Protocol == core::agent_protocol::codex_jsonl_exec, "only the Codex entry's probe ran");
		expect(g.state().CheckedEntry == std::optional{ codex } && g.state().CheckStep == application::agent_check_step::instructions_probe
				   && g.state().State.AgentCheck.Failure == core::agent_setup_failure::failed_to_start,
			   "the result names the Codex entry checked, not the preferred one");
	}

	/// \brief [[BPR-2348]]. A custom entry is checked with its own Program
	/// and arguments, the command its turns run: never a program the search
	/// found for its kind (a shim on PATH), whichever entry is preferred,
	/// and whether or not the search finds one. A Program that cannot run is
	/// refused by name, and the details say what ran.
	void test_a_custom_entry_is_checked_with_its_own_program()
	{
		const auto launcher = std::string{ "/opt/corp/bin/launcher" };
		const auto entry    = [&launcher](std::string aName, std::string aProgram = { }) {
			auto value         = core::agent_entry{ .Name = std::move(aName), .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = aProgram.empty() ? launcher : std::move(aProgram), .Arguments = { "--no-extra-output", "--codex" }, .CheckArguments = { "--version" } };
			value.Instructions = core::agent_instruction_delivery::message;
			return value;
		};
		const auto passing = [] {
			return std::vector{ said(application::agent_run_outcome::success, "launcher 0.7.0-pre"), said(application::agent_run_outcome::success, "I am ready.\nMARIGOLD-4127") };
		};
		const auto ranOwn = [](const std::vector<application::agent_launch_request>& aRequests, const std::string& aProgram) {
			return !aRequests.empty() && std::ranges::all_of(aRequests, [&aProgram](const application::agent_launch_request& aRequest) { return aRequest.Executable == aProgram; });
		};
		// A shim on PATH for Codex, and a preferred Claude entry of another program.
		{
			auto f  = fixture{ };
			f.Codex = core::agent_search_candidate{ .Path = "/Users/me/.local/bin/codex" };
			f.Host->load();
			f.settle();
			f.Host->add_agent({ .Name = "Launcher Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/corp/bin/claude-launcher" }, true);
			f.Host->add_agent(entry("Launcher Codex"), false);
			f.Host->detect();
			f.settle();
			const auto codex = f.state().Preferences.Agents.back().Id;
			expect(f.state().Preferences.PreferredAgent == f.state().Preferences.Agents.front().Id, "the Claude entry is preferred");
			f.Process.Answers = passing();
			f.Host->check_entry(codex);
			f.settle();
			expect(f.Process.Requests.size() == 2U && ranOwn(f.Process.Requests, launcher), "its check command and its probe run the entry's own Program, never the shim found on PATH");
			expect(f.Process.Requests[0].Arguments == std::vector<std::string>{ "--version" } && f.Process.Requests[1].Arguments.size() > 2U
					   && f.Process.Requests[1].Arguments[0] == "--no-extra-output" && f.Process.Requests[1].Arguments[1] == "--codex"
					   && std::ranges::count(f.Process.Requests[1].Arguments, std::string{ "--no-extra-output" }) == 1,
				   "with its own check command, and its arguments once, first, as its turns get them");
			expect(f.state().State.AgentCheck.State == setup_check_state::passed && f.state().CheckedEntry == std::optional{ codex }, "and it passes");
		}
		// Check all with two custom Codex entries: each runs its own Program.
		{
			auto f  = fixture{ };
			f.Codex = core::agent_search_candidate{ .Path = "/Users/me/.local/bin/codex" };
			f.Host->load();
			f.settle();
			f.Host->add_agent(entry("First Codex", "/opt/corp/bin/first"), true);
			f.Host->add_agent(entry("Second Codex", "/opt/corp/bin/second"), false);
			f.Host->detect();
			f.settle();
			auto answers = passing();
			std::ranges::move(passing(), std::back_inserter(answers));
			f.Process.Answers = std::move(answers);
			f.Host->check_all();
			f.settle();
			const auto& requests = f.Process.Requests;
			expect(requests.size() == 4U && requests[0].Executable == "/opt/corp/bin/first" && requests[1].Executable == "/opt/corp/bin/first" && requests[2].Executable == "/opt/corp/bin/second"
					   && requests[3].Executable == "/opt/corp/bin/second",
				   "Check all runs each entry's own Program");
			const auto& kept = f.state().Preferences.EntryChecks;
			expect(kept.size() == 2U && std::ranges::all_of(kept, [](const application::agent_entry_check_record& aRecord) { return aRecord.Check.State == setup_check_state::passed; }), "and both pass");
		}
		// No Codex found anywhere: the entry is still checked, and passes.
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			f.Host->add_agent(entry("Launcher Codex"), true);
			f.Host->detect();
			f.settle();
			expect(std::ranges::none_of(f.state().State.Rows, [](const core::agent_setup_row& aRow) { return aRow.Agent == setup_agent::codex && aRow.Path != "/opt/corp/bin/launcher"; }),
				   "no other Codex is found");
			f.Process.Answers = passing();
			f.Host->check_all();
			f.settle();
			expect(ranOwn(f.Process.Requests, launcher) && f.state().Preferences.EntryChecks.size() == 1U && f.state().Preferences.EntryChecks.front().Check.State == setup_check_state::passed,
				   "with nothing found it runs its own Program and passes, never 'not found'");
			const auto failures = [&f] {
				auto texts = std::vector<std::string>{ };
				for (const auto& event : f.Log->Events) {
					texts.emplace_back(event.Summary.text());
				}
				return texts;
			}();
			expect(std::ranges::none_of(failures, [](const std::string& aLine) { return aLine.find("agent_not_found") != std::string::npos; }), "and no agent_not_found is settled");
		}
		// A Program that cannot run is refused by name, and runs nothing.
		{
			auto f    = fixture{ };
			f.Refusal = core::agent_wrapper_refusal::missing;
			f.Host->load();
			f.settle();
			f.Host->add_agent(entry("Gone Codex", "/opt/corp/bin/gone"), true);
			f.settle();
			// Added while the inspector refused nothing, then the file is gone.
			f.Host->check_all();
			f.settle();
			const auto& kept = f.state().Preferences.EntryChecks;
			expect(f.Process.Requests.empty() && kept.size() == 1U && kept.front().Check.Failure == core::agent_setup_failure::program_refused, "a missing Program is refused, running nothing");
			expect(f.state().CheckDetail.starts_with("/opt/corp/bin/gone: "), "naming the path, then why: " + f.state().CheckDetail);
		}
		// A failed check's details name what ran.
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			f.Host->add_agent(entry("Launcher Codex"), true);
			f.Host->detect();
			f.settle();
			f.Process.Answers = { said(application::agent_run_outcome::success, "launcher 0.7.0-pre"), said(application::agent_run_outcome::nonzero_exit, "must be the first argument", 2) };
			f.Host->check();
			f.settle();
			expect(f.state().State.AgentCheck.State == setup_check_state::failed && f.state().CheckDetail.starts_with("Ran: /opt/corp/bin/launcher --no-extra-output --codex …and the check's own\n"),
				   "the details name the program and the entry's arguments first: " + f.state().CheckDetail);
		}
	}

	[[nodiscard]] auto names(const application::agent_setup_projection& aProjection) -> std::vector<std::string>
	{
		auto shown = std::vector<std::string>{ };
		std::ranges::transform(aProjection.Choices, std::back_inserter(shown), &core::agent_setup_choice::Name);
		return shown;
	}

	/// Owner report ("If I add an agent through agent settings, the agent
	/// setup wizard doesn't update to show it"). Every published projection
	/// carries the Agent step's cards from the list as it is: an agent added,
	/// edited or removed in Settings is published at once, and choosing an
	/// added agent in setup chooses that entry and checks its program.
	void test_the_setup_follows_the_list()
	{
		auto f  = fixture{ };
		f.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		f.Host->load();
		f.Host->detect();
		f.settle();
		expect(names(f.state()) == std::vector<std::string>{ "Claude Code", "Codex" }, "before anything is added, the popular agents found are offered");
		const auto before = f.Published.size();
		f.Host->add_agent({ .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/wrapper" }, false);
		f.settle();
		expect(f.Published.size() > before && names(f.Published.back()) == std::vector<std::string>{ "Work Claude", "Claude Code", "Codex" },
			   "an agent added in Settings is published to the setup at once, first, as the list is");
		const auto work = f.state().Choices.front();
		expect(work.Entry == f.state().Preferences.Agents.front().Id && work.Path == "/opt/work/bin/wrapper" && work.How == core::agent_found_how::saved_program && work.Verdict.Permitted && !work.Chosen,
			   "its card is its entry, with its own program, permitted and not chosen");
		f.Host->add_agent(core::preset_agent_entry(setup_agent::claude_code, core::codex_protocol_choice::app_server, { }), false);
		f.settle();
		expect(names(f.state()) == std::vector<std::string>{ "Work Claude", "Claude Code", "Codex" } && f.state().Choices[1].Entry != 0U && f.state().Choices[1].Path == "/opt/homebrew/bin/claude",
			   "a popular agent added in Settings replaces its found card, with where it was found");
		auto renamed = f.state().Preferences.Agents.front();
		renamed.Name = "Work Claude 2";
		f.Host->update_agent(renamed);
		f.settle();
		expect(f.state().Choices.front().Name == "Work Claude 2", "an edit in Settings is published");

		// Chosen in setup: that entry, its program the Claude row.
		f.Host->choose_entry(renamed.Id);
		f.settle();
		expect(f.state().Preferences.PreferredAgent == renamed.Id && f.state().State.Chosen == setup_agent::claude_code && f.Store.Stored->PreferredAgent == renamed.Id,
			   "choosing an added agent in setup chooses its entry, and keeps it");
		expect(f.state().State.Rows.front().Path == "/opt/work/bin/wrapper" && f.state().Choices.front().Chosen && !f.state().Choices[1].Chosen,
			   "its program is what setup checks, and only its card is chosen");
		expect(f.state().Choices[1].Path == "/opt/homebrew/bin/claude", "the popular Claude Code keeps where it was found");
		const auto published = f.Published.size();
		f.Host->choose_entry(renamed.Id);
		f.Host->choose_entry(99U);
		f.settle();
		expect(f.Published.size() == published, "choosing the chosen entry again, or one not in the list, changes nothing");
		// The popular Claude Code's card, while a custom Claude agent is chosen.
		f.Host->choose(setup_agent::claude_code);
		f.settle();
		expect(f.state().Preferences.PreferredAgent == f.state().Choices[1].Entry && f.state().Choices[1].Chosen && !f.state().Choices.front().Chosen
				   && f.state().State.Rows.front().Path == "/opt/homebrew/bin/claude",
			   "picking the popular agent of the chosen kind switches to it and checks its program");
		f.Host->choose_entry(renamed.Id);
		f.settle();
		expect(f.state().Choices.front().Chosen && f.state().State.Rows.front().Path == "/opt/work/bin/wrapper", "and back");

		// Back to the popular Claude Code in setup: a different entry of the
		// same kind, so its program is what setup checks again.
		f.Host->choose(setup_agent::claude_code);
		f.settle();
		expect(f.state().Preferences.PreferredAgent == f.state().Choices[1].Entry && f.state().Choices[1].Chosen && !f.state().Choices.front().Chosen
				   && f.state().State.Rows.front().Path == "/opt/homebrew/bin/claude",
			   "picking the popular agent of the chosen kind switches to it and looks again");
		f.Host->choose_entry(renamed.Id);
		f.settle();
		expect(f.state().Preferences.PreferredAgent == renamed.Id && f.state().State.Rows.front().Path == "/opt/work/bin/wrapper", "and back to the added agent");
		f.Host->remove_agent(renamed.Id);
		f.settle();
		expect(names(f.state()) == std::vector<std::string>{ "Claude Code", "Codex" } && f.state().Preferences.PreferredAgent == 0U, "a removal in Settings is published");
		expect(f.state().Choices.front().Chosen, "the chosen kind's popular entry is the choice once its preferred entry is gone");
		expect(f.state().State.Rows.front().Path == "/opt/homebrew/bin/claude" && f.state().State.AgentCheck.State == setup_check_state::not_run,
			   "and the removed agent's program is no longer what setup checks");

		// The reverse: Custom agent… in setup is in the list Settings shows.
		f.Host->add_agent({ .Name = "Wizard Codex", .Protocol = core::agent_protocol::codex_app_server, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } }, true);
		f.settle();
		expect(std::ranges::contains(f.state().Preferences.Agents, std::string{ "Wizard Codex" }, &core::agent_entry::Name) && names(f.state()) == std::vector<std::string>{ "Claude Code", "Wizard Codex", "Codex" }
				   && f.state().Choices[1].Chosen && !f.state().Choices[2].Chosen,
			   "an agent added in setup is in the list and is the chosen card; the found Codex, not added, is still offered");
	}

	[[nodiscard]] auto catalog_reply() -> application::agent_run_result
	{
		auto listed         = said(application::agent_run_outcome::success, { });
		listed.ModelCatalog = R"({"data":[{"model":"gpt-6.1-sol","displayName":"GPT-6.1-Sol","isDefault":true,"supportedReasoningEfforts":[{"reasoningEffort":"low"},{"reasoningEffort":"max"}]}]})";
		return listed;
	}

	void test_catalog_at_load_and_add()
	{
		auto f                      = fixture{ false };
		f.Codex                     = core::agent_search_candidate{ .Path = "/opt/bin/codex" };
		auto preset                 = core::preset_agent_entry(setup_agent::codex, core::codex_protocol_choice::app_server, { });
		preset.Id                   = 1U;
		f.Store.Stored->Agents      = { preset };
		f.Store.Stored->AgentsMoved = true;
		f.Process.CatalogAnswers    = { catalog_reply(), catalog_reply() };
		f.Host->load();
		f.Effects.drain();
		f.Decision.drain();
		expect(f.state().PolicyPending && !f.state().Detected && f.state().Cataloguing == std::vector<std::uint32_t>{ preset.Id } && f.Process.CatalogRequests.empty(),
			   "loaded settings schedule a worker without showing the tab or observing a review");
		f.settle();
		expect(f.Process.CatalogRequests.size() == 1U && f.Process.CatalogRequests.back().Executable == "/opt/bin/codex"
				   && f.Process.CatalogRequests.back().WritingOnly && f.Process.CatalogRequests.back().Arguments == std::vector<std::string>{ "app-server" },
			   "the preset locates its own program and runs a review-free catalog probe");

		f.Host->add_agent({ .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } });
		f.Host->add_agent({ .Name = "Claude Code", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/homebrew/bin/claude" });
		const auto agents = f.state().Preferences.Agents;
		expect(agents.size() == 3U && f.state().Cataloguing == std::vector<std::uint32_t>{ agents[1].Id }, "adding Codex queues a probe while Claude remains on demand");
		f.Host->load_catalog(agents[1].Id);
		f.settle();
		expect(f.Process.CatalogRequests.size() == 2U && f.Process.CatalogRequests.back().Executable == "/opt/work/bin/wrapper"
				   && f.Process.CatalogRequests.back().Arguments == std::vector<std::string>{ "--codex", "app-server" },
			   "a custom agent is asked once through its program and arguments");
		const auto codex = f.state().Catalogs.at(agents[1].Id);
		expect(codex.Models.size() == 1U && codex.Models.front().DisplayName == "GPT-6.1-Sol" && codex.Efforts == std::vector<std::string>{ "low", "max" } && f.state().Cataloguing.empty(),
			   "its models and their efforts are the harness's own");
		expect(f.Log->Events.size() == 2U && f.Log->Events.back().Operation == application::diagnostic_operation::agent_catalog
				   && f.Log->Events.back().Outcome == application::diagnostic_outcome::succeeded
				   && f.Log->Events.back().Summary.text().starts_with("protocol=codex_jsonl_exec outcome=success failure=none exit=0 duration_ms=")
				   && f.Log->Events.back().Summary.text().ends_with(" models=1"),
			   "each probe logs a single terminal record with the entry's protocol and model count");
		f.Host->load_catalog(agents[1].Id);
		f.Process.Answers = { said(application::agent_run_outcome::success, "  --effort <level>   Effort level\n                     (low, medium, high, xhigh, max)") };
		f.Host->load_catalog(agents[2].Id);
		f.settle();
		expect(f.Process.CatalogRequests.size() == 2U, "a successful catalog is reused");
		expect(f.Process.Requests.back().Arguments == std::vector<std::string>{ "--help" } && f.state().Catalogs.at(agents[2].Id).Efforts == std::vector<std::string>{ "low", "medium", "high", "xhigh", "max" },
			   "Claude Code's effort levels are read from its own --help");

		expect(f.Host->set_run_limit(30U), "a run limit is set");
		f.Host->reset();
		f.settle();
		expect(f.state().Preferences.Agents.empty() && f.state().Preferences.PreferredAgent == 0U && f.state().Preferences.RunLimitMinutes == 0U && f.state().Preferences.AgentsMoved
				   && f.state().Catalogs.empty() && f.Store.Stored->Agents.empty() && f.state().Step == core::setup_step::agent,
			   "Reset agents clears every agent and setting, keeps the list from moving back in, and setup starts at Agent");
	}

	void test_catalog_failures_retry_on_request()
	{
		auto f                   = fixture{ };
		auto empty               = said(application::agent_run_outcome::success, { });
		empty.ModelCatalog       = R"({"data":[]})";
		f.Process.CatalogAnswers = { said(application::agent_run_outcome::timeout, "PRIVATE", std::nullopt),
									 said(application::agent_run_outcome::nonzero_exit, "PRIVATE", 7), empty, catalog_reply() };
		f.Host->add_agent({ .Name = "Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/bin/codex" });
		const auto id = f.state().Preferences.Agents.front().Id;
		f.settle();
		const auto checkFailure = [&f, id](const std::string_view aDetail) {
			const auto  state      = f.state();
			const auto& diagnostic = f.Log->Events.back();
			expect(!state.Catalogs.contains(id) && state.Cataloguing.empty() && state.CatalogFailures.contains(id), "only the failed attempt is shown");
			expect(diagnostic.Operation == application::diagnostic_operation::agent_catalog && diagnostic.Outcome == application::diagnostic_outcome::failed
					   && diagnostic.Summary.text().starts_with(aDetail) && diagnostic.Summary.text().ends_with(" models=0")
					   && !diagnostic.Summary.text().contains("PRIVATE"),
				   "the failure diagnosis carries outcome, exit, duration and count without output");
		};
		checkFailure("protocol=codex_jsonl_exec outcome=timeout failure=timeout exit=none duration_ms=");
		f.Now += 59'999;
		f.Host->load_catalog(id);
		f.settle();
		expect(f.Process.CatalogRequests.size() == 1U, "reopening within a minute does not start a run");
		++f.Now;
		f.Host->load_catalog(id);
		f.Host->load_catalog(id);
		f.settle();
		expect(f.Process.CatalogRequests.size() == 2U, "the minute boundary allows one new request");
		checkFailure("protocol=codex_jsonl_exec outcome=nonzero_exit failure=nonzero_exit exit=7 duration_ms=");
		f.Now += 60'000;
		f.Host->load_catalog(id);
		f.settle();
		checkFailure("protocol=codex_jsonl_exec outcome=success failure=empty exit=0 duration_ms=");
		f.Now += 60'000;
		f.Host->load_catalog(id);
		f.settle();
		expect(f.Process.CatalogRequests.size() == 4U && f.Log->Events.size() == 4U && f.state().Catalogs.contains(id) && f.state().CatalogFailures.empty(),
			   "a later list clears the failed state without restarting");
	}

	void test_catalog_command_changes()
	{
		auto f                   = fixture{ };
		f.Process.CatalogAnswers = { catalog_reply(), catalog_reply(), catalog_reply() };
		f.Host->add_agent({ .Name = "Codex", .Protocol = core::agent_protocol::codex_app_server, .Program = "/opt/bin/codex" });
		f.settle();
		auto entry = f.state().Preferences.Agents.front();
		expect(f.state().Catalogs.contains(entry.Id), "the original command has listed its models");
		entry.Program = "/opt/one/codex";
		f.Host->update_agent(entry);
		expect(!f.state().Catalogs.contains(entry.Id), "editing the program invalidates its successful list immediately");
		f.Checks.drain();
		entry.Arguments = { "--new" };
		f.Host->update_agent(entry);
		expect(f.Checks.pending() == 0U && f.Process.CatalogRequests.size() == 2U, "an edit waits for the previous command to settle before launching");
		f.Decision.drain();
		expect(!f.state().Catalogs.contains(entry.Id), "the previous command's late answer is not published");
		f.settle();
		expect(f.Process.CatalogRequests.size() == 3U && f.Process.CatalogRequests.back().Executable == "/opt/one/codex"
				   && f.Process.CatalogRequests.back().Arguments == std::vector<std::string>{ "--new", "app-server" } && f.state().Catalogs.contains(entry.Id),
			   "the latest arguments are probed after the superseded answer settles");
	}

	/// [[BPR-1859]]. Settings' Check agent runs the setup's own check:
	/// before the Agent tab looked for agents it looks first; Stop ends it
	/// while it waits; a saved agent no longer found says so; it is safe to
	/// ask again, and a pass after a fix finishes the setup's agent row.
	void test_check_from_settings()
	{
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			f.Host->choose(setup_agent::claude_code);
			f.settle();
			expect(!f.state().Detected, "the Agent tab has not looked for agents");
			auto facts = application::agent_check_facts_of(f.state(), false);
			expect(core::decide_agent_check(facts).Offer == core::agent_check_offer::available, "Settings offers the check before looking");
			f.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": false})") };
			f.Host->check();
			expect(f.state().Checking && f.state().Detecting && f.state().State.AgentCheck.State == setup_check_state::running, "the check is running while it looks for the agent");
			expect(core::decide_agent_check(application::agent_check_facts_of(f.state(), false)).Offer == core::agent_check_offer::running, "and is not offered again");
			f.Host->check();
			f.settle();
			expect(f.Process.Requests.size() == 1U, "a second ask while it runs starts nothing more");
			expect(f.state().Detected && !f.state().Checking && f.state().State.AgentCheck.Failure == core::agent_setup_failure::not_signed_in,
				   "it looked, then ran the status check: not signed in");
			const auto before = f.state().Step;
			// The reviewer signs in and checks again.
			f.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": true})"), said(application::agent_run_outcome::success, "2.1.300 (Claude Code)") };
			f.Host->check();
			f.settle();
			expect(f.state().State.AgentCheck.State == setup_check_state::passed && f.state().State.AgentCheck.Version == "2.1.300" && f.state().CheckDetail.empty(),
				   "checking again after the fix passes");
			expect(before == f.state().Step, "the step follows the rows exactly as in the setup");
			f.Host->set_repository_chosen(true);
			f.Host->set_workspace_check({ .State = setup_check_state::passed });
			expect(f.state().Step == core::setup_step::ready, "with the workspace made too, the setup is Ready");
		}
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			f.Host->choose(setup_agent::claude_code);
			f.settle();
			f.Host->check();
			f.Host->stop();
			expect(!f.state().Checking && f.state().State.AgentCheck.State == setup_check_state::not_run, "Stop ends a check still looking for the agent");
			f.settle();
			expect(f.Process.Requests.empty() && f.state().Detected && f.state().State.AgentCheck.State == setup_check_state::not_run, "and the search that answers later runs nothing");
		}
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			f.Host->choose(setup_agent::claude_code);
			f.Host->detect();
			f.settle();
			f.Claude = std::nullopt;
			f.Host->detect();
			f.settle();
			const auto facts = application::agent_check_facts_of(f.state(), true);
			expect(facts.SavedChoice && !facts.Chosen && core::decide_agent_check(facts).Offer == core::agent_check_offer::available, "a saved agent that is gone is still checked");
			f.Host->check();
			f.settle();
			expect(f.Process.Requests.empty() && f.state().State.AgentCheck.Failure == core::agent_setup_failure::agent_not_found, "the check says it was not found, running nothing");
		}
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			const auto facts = application::agent_check_facts_of(f.state(), true);
			expect(core::decide_agent_check(facts).Offer == core::agent_check_offer::no_agent, "nothing added: no check");
		}
	}

	/// \brief [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"). Check
	/// all checks each added agent in turn and keeps each one's result; the
	/// chosen agent stays chosen, and a removed agent's result goes with it.
	void test_check_all_keeps_each_result()
	{
		auto f = fixture{ };
		f.Host->load();
		f.settle();
		f.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		f.Host->add_agent(core::preset_agent_entry(setup_agent::claude_code, core::codex_protocol_choice::app_server, { }), true);
		f.Host->add_agent(core::preset_agent_entry(setup_agent::codex, core::codex_protocol_choice::app_server, { }));
		f.Host->detect();
		f.settle();
		const auto agents = f.state().Preferences.Agents;
		expect(agents.size() == 2U, "two agents are added");
		const auto preferred = f.state().Preferences.PreferredAgent;
		// Claude Code passes (status, then version); Codex is not signed in.
		f.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": true})"), said(application::agent_run_outcome::success, "2.1.321 (Claude Code)"),
							  said(application::agent_run_outcome::success, "Not logged in") };
		f.Host->check_all();
		expect(f.state().Checking && f.state().CheckingEntry == std::optional{ agents[0].Id }, "Check all starts on the first agent and says which");
		f.settle();
		const auto& kept = f.state().Preferences.EntryChecks;
		expect(kept.size() == 2U && kept[0].Id == agents[0].Id && kept[0].Check.State == setup_check_state::passed && kept[0].Check.Version == "2.1.321",
			   "the first agent's pass is kept with its version");
		expect(kept[1].Id == agents[1].Id && kept[1].Check.State == setup_check_state::failed && kept[1].Check.Failure.has_value(), "the second agent's failure is kept with why");
		expect(!f.state().Checking && !f.state().CheckingEntry && f.state().Preferences.PreferredAgent == preferred, "Check all ends and the chosen agent stays chosen");
		expect(f.Store.Stored && f.Store.Stored->EntryChecks == kept, "each result is saved, so it shows after a restart");
		expect(f.state().State.AgentCheck.State == setup_check_state::passed, "the setup shows the chosen agent's own result again");
		// One agent, from its row.
		f.Process.Answers = { said(application::agent_run_outcome::success, "Logged in using ChatGPT"), said(application::agent_run_outcome::success, "codex-cli 0.163.0") };
		f.Host->check_entry(agents[1].Id);
		f.settle();
		expect(f.state().Preferences.EntryChecks[1].Check.State == setup_check_state::passed && f.state().Preferences.PreferredAgent == preferred, "one agent's check keeps only its result");
		f.Host->remove_agent(agents[1].Id);
		f.settle();
		expect(f.state().Preferences.EntryChecks.size() == 1U && f.Store.Stored->EntryChecks.size() == 1U, "a removed agent's result goes with it");
	}

	/// [[BPR-2261]], AC4. A pull request workspace's passed check is kept
	/// under its workspace key across a restart: the reopened setup's row,
	/// not run, takes it for that pull request and no other; a failure
	/// forgets it.
	void test_a_workspace_check_survives_a_restart()
	{
		const auto key  = std::string{ "github.example/octo/review#17:base:head" };
		auto       kept = application::agent_setup_preferences{ };
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			f.Host->settle_workspace_check(key, { .State = setup_check_state::passed });
			f.settle();
			expect(f.state().State.WorkspaceCheck.State == setup_check_state::passed && f.state().Preferences.WorkspaceChecks == std::vector{ key }, "a pass is the row's and is kept");
			expect(f.Store.Stored && f.Store.Stored->WorkspaceChecks == std::vector{ key }, "and saved");
			kept = *f.Store.Stored;
		}
		auto f         = fixture{ };
		f.Store.Stored = kept;
		f.Host->load();
		f.settle();
		expect(f.state().State.WorkspaceCheck.State == setup_check_state::not_run, "a restart starts with the row not run");
		f.Host->restore_workspace_check("github.example/octo/review#18:base:head");
		expect(f.state().State.WorkspaceCheck.State == setup_check_state::not_run, "another pull request's workspace takes nothing");
		f.Host->restore_workspace_check(key);
		expect(f.state().State.WorkspaceCheck.State == setup_check_state::passed, "the same workspace's pass stands, with no check run");
		f.Host->settle_workspace_check(key, core::fail_check(core::agent_setup_failure::workspace_unavailable));
		f.settle();
		expect(f.state().State.WorkspaceCheck.State == setup_check_state::failed && f.state().Preferences.WorkspaceChecks.empty() && f.Store.Stored->WorkspaceChecks.empty(), "a failure forgets it");
	}
}

namespace {
	/// \brief [[BPR-2254]]. A store that cannot be read is not a fresh
	/// profile: the projection says so and names the file, every change to
	/// the agents is refused with that reason and nothing is written, and a
	/// check says the settings could not be read instead of "not found".
	void test_unreadable_settings()
	{
		{
			auto f = fixture{ };
			f.Host->load();
			f.settle();
			expect(!f.state().SettingsUnreadable && f.state().SettingsFile.empty(), "a store that reads, even empty, is not unreadable");
		}
		auto f         = fixture{ };
		f.Store.Stored = std::nullopt;
		f.Host->load();
		f.settle();
		expect(f.state().SettingsUnreadable && f.state().SettingsFile == unreadable_file, "a store that cannot be read says so and names its file");
		auto answer = std::optional<core::agent_entry_refusal>{ };
		f.Host->add_agent({ .Name = "Claude stream JSON", .Program = "/opt/work/bin/claude-wrapper" }, true, [&answer](const std::optional<core::agent_entry_refusal> aRefusal) { answer = aRefusal; });
		f.settle();
		expect(answer == core::agent_entry_refusal::settings_unreadable && f.state().Preferences.Agents.empty(), "adding an agent is refused with that reason and keeps nothing");
		answer.reset();
		f.Host->update_agent({ .Id = 1U, .Name = "Renamed", .Program = "/opt/work/bin/claude-wrapper" }, [&answer](const std::optional<core::agent_entry_refusal> aRefusal) { answer = aRefusal; });
		expect(answer == core::agent_entry_refusal::settings_unreadable, "so is editing one");
		f.Host->choose(setup_agent::claude_code);
		f.Host->prefer_agent(1U);
		f.Host->reset();
		expect(!f.Host->set_run_limit(30U) && !f.Host->set_scratch_policy(512U, 7U), "and every other setting");
		f.settle();
		expect(f.state().Preferences.Agents.empty() && f.Store.Stores == 0, "nothing was added in memory or written over the file");
		f.Host->check();
		f.settle();
		expect(f.Process.Requests.empty() && f.state().State.AgentCheck.State == setup_check_state::failed && f.state().State.AgentCheck.Failure == core::agent_setup_failure::settings_unreadable,
			   "a check runs nothing and says the settings could not be read");
		expect(f.state().CheckDetail == unreadable_file, "naming the file, not \"No process output was returned\"");
		const auto failureOf = [](const application::diagnostic_event& aEvent) {
			const auto parsed = core::parse_operation_diagnosis(std::string{ aEvent.Summary.text() });
			return parsed ? parsed->Failure : core::operation_failure::none;
		};
		expect(std::ranges::none_of(f.Log->Events, [&failureOf](const application::diagnostic_event& aEvent) { return failureOf(aEvent) == core::operation_failure::agent_not_found; })
				   && std::ranges::any_of(f.Log->Events, [&failureOf](const application::diagnostic_event& aEvent) { return failureOf(aEvent) == core::operation_failure::store_unreadable; }),
			   "the log records a storage failure, never agent_not_found");
		f.Host->check_all();
		f.settle();
		expect(f.Process.Requests.empty() && f.Store.Stores == 0, "Check all runs nothing and keeps nothing");
	}
}

namespace {
	/// \brief [[BPR-2256]]. Before a review's host policy is observed the
	/// setup is pending, not refused: every row is still refused by the core
	/// (a missing policy permits nothing), and the projection says nothing
	/// was decided. The Enterprise policy then permits Claude stream JSON, an
	/// added agent of that protocol and, since [[BPR-2305]], Codex; with no
	/// review the setup is pending again.
	void test_policy_pending_until_observed()
	{
		auto f  = fixture{ false };
		f.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		f.Host->load();
		f.settle();
		f.Host->add_agent({ .Name = "Claude stream JSON", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/claude-wrapper" });
		f.Host->detect();
		f.settle();
		const auto pending = f.state();
		expect(pending.PolicyPending, "a host that observed no policy is pending");
		expect(!pending.State.Rows.empty() && std::ranges::none_of(pending.State.Rows, [](const core::agent_setup_row& aRow) { return aRow.Verdict.Permitted; })
				   && std::ranges::none_of(pending.Choices, [](const core::agent_setup_choice& aChoice) { return aChoice.Verdict.Permitted; }),
			   "and no agent is permitted while it is (decide_agent_invocation is unchanged)");

		f.Host->observe_policy(core::built_in_agent_policy(core::git_host_id{ 2U }, core::built_in_agent_host_kind::explicit_ghes));
		f.settle();
		const auto ghes = f.state();
		expect(!ghes.PolicyPending, "an observed policy ends the wait");
		const auto row = [&ghes](const setup_agent aAgent) { return std::ranges::find(ghes.State.Rows, aAgent, &core::agent_setup_row::Agent); };
		expect(row(setup_agent::claude_code) != ghes.State.Rows.end() && row(setup_agent::claude_code)->Verdict.Permitted, "on Enterprise Claude stream JSON is permitted");
		expect(row(setup_agent::codex) != ghes.State.Rows.end() && row(setup_agent::codex)->Verdict.Permitted, "and Codex is permitted (owner, 2026-11-19)");
		const auto custom = std::ranges::find(ghes.Choices, std::string{ "Claude stream JSON" }, &core::agent_setup_choice::Name);
		expect(custom != ghes.Choices.end() && custom->Entry != 0U && custom->Verdict.Permitted, "the added Claude stream JSON agent is permitted");

		f.Host->observe_policy(std::nullopt);
		f.settle();
		expect(!f.state().PolicyPending && std::ranges::all_of(f.state().State.Rows, [](const core::agent_setup_row& aRow) { return aRow.Verdict.Refusal == core::agent_invocation_refusal::host_not_mapped; }),
			   "a host that maps no policy is decided: every agent refused as not mapped");
		f.Host->forget_policy();
		f.settle();
		expect(f.state().PolicyPending && std::ranges::none_of(f.state().State.Rows, [](const core::agent_setup_row& aRow) { return aRow.Verdict.Permitted; }), "with no review the setup is pending again");
	}
}

auto main() -> int
{
	test_policy_pending_until_observed();
	test_unreadable_settings();
	test_catalog_at_load_and_add();
	test_catalog_failures_retry_on_request();
	test_catalog_command_changes();
	test_detection_starts_nothing();
	test_saved_programs_win();
	test_status_check();
	test_auto_chosen_wrapper_keeps_its_entry();
	test_reset_does_not_resurrect_saved_wrapper();
	test_setup_details_are_bounded_and_separate();
	test_codex_and_stop();
	test_check_diagnostics();
	test_choice_and_steps();
	test_the_agent_list();
	test_check_from_settings();
	test_check_all_keeps_each_result();
	test_a_custom_entry_is_checked_with_its_own_program();
	test_a_workspace_check_survives_a_restart();
	test_the_setup_follows_the_list();
	test_a_custom_codex_check_proves_its_instructions_arrive();
	test_a_custom_claude_check_proves_it_answers_in_stream_json();
	std::cout << "agent-setup-host-tests:pass\n";
	return EXIT_SUCCESS;
}
