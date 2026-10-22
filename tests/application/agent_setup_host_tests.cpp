/// \file
/// \brief [[BPR-PENDING-AQ2]], RFC-BPR-0091 M2. The setup host on manual
/// lanes and fake ports: detection starts no process (Q2), the bounded status
/// check reaches the setup state from scripted answers (Q3 A), Stop and a
/// timeout end it, and the choice is kept.

#include "bpr/application/agent_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"

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
}

auto main() -> int
{
	test_detection_starts_nothing();
	test_saved_programs_win();
	test_status_check();
	test_codex_and_stop();
	test_choice_and_steps();
	std::cout << "agent-setup-host-tests:pass\n";
	return EXIT_SUCCESS;
}
