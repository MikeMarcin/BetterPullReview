/// \file
/// \brief [[BPR-PENDING-AQ2]], RFC-BPR-0092 M2. The setup host on manual
/// lanes and fake ports: detection starts no process (Q2), the bounded status
/// check reaches the setup state from scripted answers (Q3 A), Stop and a
/// timeout end it, and the choice is kept.

#include "bpr/application/agent_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
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
	class scripted_port final : public application::agent_process_port
	{
	public:
		[[nodiscard]] auto run(const application::agent_launch_request& aRequest, const int aTimeout, const application::agent_output_sink& = { }, std::stop_token = { })
			-> application::agent_run_result override
		{
			Requests.push_back(aRequest);
			Timeouts.push_back(aTimeout);
			auto result = application::agent_run_result{ };
			if (!Answers.empty()) {
				result = Answers.front();
				Answers.erase(Answers.begin());
			}
			result.Session = aRequest.Session;
			return result;
		}
		void cancel() noexcept override { ++Cancels; }

		std::vector<application::agent_launch_request> Requests;
		std::vector<int>                               Timeouts;
		std::vector<application::agent_run_result>     Answers;
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

	struct fixture
	{
		application::manual_executor                                Effects;
		application::manual_executor                                Checks;
		application::manual_executor                                Decision;
		scripted_port                                               Process;
		memory_store                                                Store;
		std::shared_ptr<application::claude_process_wrapper_source> Wrapper  = std::make_shared<application::claude_process_wrapper_source>();
		std::shared_ptr<application::agent_program_source>          Programs = std::make_shared<application::agent_program_source>();
		std::optional<core::agent_search_candidate>                 Claude   = core::agent_search_candidate{ .Path = "/opt/homebrew/bin/claude" };
		std::optional<core::agent_search_candidate>                 Codex;
		std::optional<core::agent_wrapper_refusal>                  Refusal;
		std::vector<std::string>                                    Inspected;
		std::vector<application::agent_setup_projection>            Published;
		std::unique_ptr<application::agent_setup_host>              Host;

		fixture()
		{
			Host = std::make_unique<application::agent_setup_host>(application::agent_setup_dependencies{
				.Store   = &Store,
				.Process = &Process,
				.Locate  = [this](const core::agent_program aProgram) { return aProgram == core::agent_program::claude ? Claude : Codex; },
				.Inspect =
					[this](const std::string_view aPath) {
						Inspected.emplace_back(aPath);
						return Refusal;
					},
				.ClaudeWrapper = Wrapper,
				.Programs      = Programs,
				.Parent        = [](const std::string& aName) -> std::optional<std::string> {
					return aName == "HOME" ? std::optional<std::string>{ "/home/me" } : std::nullopt;
				},
				.Profiles = core::built_in_agent_profiles(),
				.Effects  = &Effects,
				.Checks   = &Checks,
				.Decision = &Decision,
				.Publish  = [this](const application::agent_setup_projection& aProjection) { Published.push_back(aProjection); },
			});
			Host->observe_policy(core::built_in_agent_policy(core::git_host_id{ 1U }, core::built_in_agent_host_kind::github_com));
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
		auto f  = fixture{ };
		f.Codex = core::agent_search_candidate{ .Path = "/usr/local/bin/codex", .Origin = core::agent_found_in::usual_folder };
		f.Wrapper->set("/opt/tools/claude-gateway");
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
		expect(ghes.state().State.Chosen == setup_agent::claude_code && !ghes.state().State.Rows.back().Verdict.Permitted, "on GHES Codex is listed, refused, and never chosen");
		ghes.Host->choose(setup_agent::codex);
		ghes.settle();
		expect(ghes.state().State.Chosen == setup_agent::claude_code, "choosing a refused agent does not take it");
		ghes.Process.Answers = { said(application::agent_run_outcome::success, R"({"loggedIn": true})") };
		ghes.Host->check();
		ghes.settle();
		expect(!ghes.Process.Requests.empty() && ghes.Process.Requests.front().Arguments.front() == "auth", "only the permitted agent is checked");
	}

	/// [[BPR-PENDING-AQ6]], owner amendment 2026-10-23. The single settings
	/// move into the agent list once; a custom agent is added and chosen, and
	/// its program and fixed arguments are what setup checks.
	void test_the_agent_list()
	{
		auto f                       = fixture{ };
		f.Store.Stored->Chosen       = setup_agent::claude_code;
		f.Store.Stored->ClaudeEffort = "high";
		f.Wrapper->set("/opt/work/bin/claude-wrapper");
		f.Host->load();
		f.settle();
		const auto moved = f.state().Preferences.Agents;
		expect(moved.size() == 1U && moved.front().Name == "Claude Code" && moved.front().Program == "/opt/work/bin/claude-wrapper" && moved.front().Effort == "high"
				   && f.state().Preferences.AgentsMoved && f.state().Preferences.PreferredAgent == moved.front().Id && f.Store.Stored->Agents == moved,
			   "the chosen Claude Code and its wrapper move into the list, once, and are kept");
		f.Host->remove_agent(moved.front().Id);
		f.settle();
		expect(f.state().Preferences.Agents.empty() && f.state().Preferences.PreferredAgent == 0U, "an agent is removed");
		auto again         = fixture{ };
		again.Store.Stored = f.Store.Stored;
		again.Wrapper->set("/opt/work/bin/claude-wrapper");
		again.Host->load();
		again.settle();
		expect(again.state().Preferences.Agents.empty(), "a removed agent does not come back at the next load");
		// Owner look v004: the Agent tab's Suggestions toggle is kept.
		expect(!again.state().Preferences.SuggestionsHidden, "the suggestions show by default");
		again.Host->set_suggestions_hidden(true);
		again.settle();
		expect(again.state().Preferences.SuggestionsHidden && again.Store.Stored->SuggestionsHidden, "hiding them is kept for the profile");

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

	/// Owner look v006 ("Codex is totally missing its model settings. And
	/// its effort is missing max"; "reset agents which clears all settings").
	void test_harness_catalogs_and_reset()
	{
		auto f = fixture{ };
		f.Host->load();
		f.settle();
		f.Host->add_agent({ .Name = "Work Codex", .Protocol = core::agent_protocol::codex_app_server, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } }, true);
		f.Host->add_agent({ .Name = "Claude Code", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/homebrew/bin/claude" });
		f.settle();
		const auto agents = f.state().Preferences.Agents;
		expect(agents.size() == 2U, "two agents are added");
		auto listed         = application::agent_run_result{ .Outcome = application::agent_run_outcome::success };
		listed.ModelCatalog = "{\"data\":[{\"model\":\"gpt-6.1-sol\",\"displayName\":\"GPT-6.1-Sol\",\"hidden\":false,\"isDefault\":true,\"defaultReasoningEffort\":\"low\","
							  "\"supportedReasoningEfforts\":[{\"reasoningEffort\":\"low\",\"description\":\"\"},{\"reasoningEffort\":\"max\",\"description\":\"\"}]}]}";
		f.Process.Answers   = { listed, said(application::agent_run_outcome::success, "  --effort <level>   Effort level\n                     (low, medium, high, xhigh, max)") };
		const auto before   = f.Process.Requests.size();
		f.Host->load_catalog(agents[0].Id);
		expect(f.state().Cataloguing == std::vector<std::uint32_t>{ agents[0].Id }, "while the harness is asked, the projection says so");
		f.settle();
		expect(f.Process.Requests.size() == before + 1U && f.Process.Requests.back().Executable == "/opt/work/bin/wrapper"
				   && f.Process.Requests.back().Arguments == std::vector<std::string>{ "--codex", "app-server" } && f.Process.Requests.back().Purpose == application::agent_launch_purpose::model_catalog,
			   "a Codex-protocol agent is asked through its own program and fixed arguments");
		const auto codex = f.state().Catalogs.at(agents[0].Id);
		expect(codex.Models.size() == 1U && codex.Models.front().DisplayName == "GPT-6.1-Sol" && codex.Efforts == std::vector<std::string>{ "low", "max" } && f.state().Cataloguing.empty(),
			   "its models and their efforts are the harness's own");
		f.Host->load_catalog(agents[1].Id);
		f.settle();
		expect(f.Process.Requests.back().Arguments == std::vector<std::string>{ "--help" } && f.state().Catalogs.at(agents[1].Id).Efforts == std::vector<std::string>{ "low", "medium", "high", "xhigh", "max" },
			   "Claude Code's effort levels are read from its own --help");
		const auto asked = f.Process.Requests.size();
		f.Host->load_catalog(agents[0].Id);
		f.settle();
		expect(f.Process.Requests.size() == asked, "an agent already asked is not asked again in this window");

		expect(f.Host->set_run_limit(30U), "a run limit is set");
		f.Host->reset();
		f.settle();
		expect(f.state().Preferences.Agents.empty() && f.state().Preferences.PreferredAgent == 0U && f.state().Preferences.RunLimitMinutes == 0U && f.state().Preferences.AgentsMoved
				   && f.state().Catalogs.empty() && f.Store.Stored->Agents.empty() && f.state().Step == core::setup_step::agent,
			   "Reset agents clears every agent and setting, keeps the list from moving back in, and setup starts at Agent");
	}
}

auto main() -> int
{
	test_harness_catalogs_and_reset();
	test_detection_starts_nothing();
	test_saved_programs_win();
	test_status_check();
	test_codex_and_stop();
	test_choice_and_steps();
	test_the_agent_list();
	std::cout << "agent-setup-host-tests:pass\n";
	return EXIT_SUCCESS;
}
