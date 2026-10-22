/// \file
/// \brief [[BPR-PENDING-AQ1]], RFC-BPR-0091. The agent setup's rows, its
/// choice without asking, the status check's effect and the step order.

#include "bpr/core/agent_setup.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::agent_detection;
	using core::agent_found_how;
	using core::agent_setup_failure;
	using core::agent_setup_row;
	using core::codex_protocol_choice;
	using core::setup_agent;
	using core::setup_check_state;
	using core::setup_step;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto github_com() -> core::agent_host_policy
	{
		return core::built_in_agent_policy(core::git_host_id{ 1U }, core::built_in_agent_host_kind::github_com);
	}

	[[nodiscard]] auto ghes() -> core::agent_host_policy
	{
		return core::built_in_agent_policy(core::git_host_id{ 2U }, core::built_in_agent_host_kind::explicit_ghes);
	}

	[[nodiscard]] auto agents(const std::vector<agent_setup_row>& aRows) -> std::vector<setup_agent>
	{
		auto result = std::vector<setup_agent>{ };
		for (const auto& row : aRows) {
			result.push_back(row.Agent);
		}
		return result;
	}

	void test_names_and_profiles()
	{
		expect(core::plain_name(setup_agent::claude_code) == "Claude Code", "Claude by its plain name (Q4)");
		expect(core::plain_name(setup_agent::codex) == "Codex", "Codex by its plain name (Q4)");
		expect(core::plain_name(setup_agent::plain_text_harness) == "A harness that reads plain text", "the external terminal protocol, said plainly");
		expect(core::setup_agent_of(core::agent_protocol::codex_app_server) == setup_agent::codex
				   && core::setup_agent_of(core::agent_protocol::codex_jsonl_exec) == setup_agent::codex,
			   "both Codex protocols are one agent");
		expect(core::setup_agent_of(core::agent_protocol::external_terminal) == setup_agent::plain_text_harness, "the external terminal is the plain-text harness");
		expect(core::setup_profile(setup_agent::codex, codex_protocol_choice::app_server) == core::codex_app_server_profile_id, "App Server is Codex's default profile");
		expect(core::setup_profile(setup_agent::codex, codex_protocol_choice::jsonl) == core::codex_jsonl_profile_id, "JSONL when chosen in Settings");
		expect(core::setup_profile(setup_agent::claude_code, codex_protocol_choice::jsonl) == core::claude_stream_json_profile_id, "the Codex choice leaves Claude alone");
		auto names = std::set<std::string_view>{ };
		for (const auto agent : { setup_agent::claude_code, setup_agent::codex, setup_agent::plain_text_harness }) {
			names.insert(core::stable_name(agent));
		}
		expect(names.size() == core::setup_agent_count, "each setup agent has its own stable name");
	}

	void test_rows()
	{
		const auto profiles = core::built_in_agent_profiles();
		const auto found    = std::vector<agent_detection>{
			{ .Agent = setup_agent::codex, .Path = "/Users/me/.local/bin/codex", .How = agent_found_how::usual_folder },
			{ .Agent = setup_agent::plain_text_harness, .Path = "/opt/harness", .How = agent_found_how::on_path },
			{ .Agent = setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude", .How = agent_found_how::on_path },
			{ .Agent = setup_agent::claude_code, .Path = "/usr/local/bin/claude", .How = agent_found_how::usual_folder },
		};
		const auto rows = core::agent_setup_rows(found, profiles, github_com(), codex_protocol_choice::app_server);
		expect(agents(rows) == std::vector{ setup_agent::claude_code, setup_agent::codex }, "Claude Code first, then Codex; a searched-for plain-text harness is dropped");
		expect(rows.front().Path == "/opt/homebrew/bin/claude" && rows.front().How == agent_found_how::on_path, "an agent's first detection is its row");
		expect(rows.back().Profile == core::codex_app_server_profile_id, "Codex launches its chosen protocol's profile");
		expect(rows.front().Verdict.Permitted && rows.back().Verdict.Permitted, "github.com permits both");

		const auto jsonl = core::agent_setup_rows(found, profiles, github_com(), codex_protocol_choice::jsonl);
		expect(jsonl.back().Profile == core::codex_jsonl_profile_id, "the Codex row follows the protocol setting");

		const auto saved     = std::vector<agent_detection>{ { .Agent = setup_agent::plain_text_harness, .Path = "/opt/harness", .How = agent_found_how::saved_program } };
		const auto harnessed = core::agent_setup_rows(saved, profiles, github_com(), codex_protocol_choice::app_server);
		expect(agents(harnessed) == std::vector{ setup_agent::plain_text_harness }, "the plain-text harness is offered once its program is chosen");

		const auto onGhes = core::agent_setup_rows(found, profiles, ghes(), codex_protocol_choice::app_server);
		expect(agents(onGhes) == std::vector{ setup_agent::claude_code, setup_agent::codex }, "a forbidden agent keeps its row");
		expect(onGhes.front().Verdict.Permitted, "explicit GHES permits Claude");
		expect(!onGhes.back().Verdict.Permitted && onGhes.back().Verdict.Refusal == core::agent_invocation_refusal::profile_forbidden_on_host,
			   "explicit GHES refuses Codex and says why");
		expect(core::agent_setup_rows(found, profiles, std::nullopt, codex_protocol_choice::app_server).front().Verdict.Refusal == core::agent_invocation_refusal::host_not_mapped,
			   "an unmapped host permits nothing");

		const auto bad = std::vector<agent_detection>{ { .Agent = setup_agent::claude_code, .Path = "" },
													   { .Agent = setup_agent::codex, .Path = std::string(core::maximum_agent_text_bytes + 1U, 'a') } };
		expect(core::agent_setup_rows(bad, profiles, github_com(), codex_protocol_choice::app_server).empty(), "an empty or over-long path is not a row");
		expect(core::agent_setup_rows(found, { }, github_com(), codex_protocol_choice::app_server).empty(), "an agent whose profile is not installed is not a row");
	}

	void test_choice()
	{
		const auto profiles = core::built_in_agent_profiles();
		const auto claude   = agent_detection{ .Agent = setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude" };
		const auto codex    = agent_detection{ .Agent = setup_agent::codex, .Path = "/opt/homebrew/bin/codex" };
		const auto one      = core::agent_setup_rows(std::vector{ claude }, profiles, github_com(), codex_protocol_choice::app_server);
		const auto both     = core::agent_setup_rows(std::vector{ claude, codex }, profiles, github_com(), codex_protocol_choice::app_server);
		expect(core::choose_setup_agent(one, std::nullopt) == setup_agent::claude_code, "one found agent is chosen without asking");
		expect(!core::choose_setup_agent(both, std::nullopt), "two found agents wait for the reviewer");
		expect(core::choose_setup_agent(both, setup_agent::codex) == setup_agent::codex, "a saved choice that is still found wins");
		expect(core::choose_setup_agent(one, setup_agent::codex) == setup_agent::claude_code, "a saved choice that is gone gives way to the only found agent");
		expect(!core::choose_setup_agent({ }, setup_agent::claude_code), "nothing found chooses nothing");
		const auto onGhes = core::agent_setup_rows(std::vector{ claude, codex }, profiles, ghes(), codex_protocol_choice::app_server);
		expect(core::choose_setup_agent(onGhes, std::nullopt) == setup_agent::claude_code, "the only permitted agent is chosen; a forbidden one never is");
		expect(core::choose_setup_agent(onGhes, setup_agent::codex) == setup_agent::claude_code, "a saved forbidden choice is not taken");

		expect(core::searched_setup_agents(profiles, github_com(), codex_protocol_choice::app_server) == std::vector{ setup_agent::claude_code, setup_agent::codex },
			   "github.com looks for Claude and Codex");
		expect(core::searched_setup_agents(profiles, ghes(), codex_protocol_choice::app_server) == std::vector{ setup_agent::claude_code },
			   "explicit GHES looks only for what it permits");
	}

	void test_checks_and_steps()
	{
		auto signedIn = core::apply_status_check({ .Status = core::agent_status::signed_in, .Version = "2.1.296" });
		expect(signedIn.State == setup_check_state::passed && signedIn.Version == "2.1.296" && !signedIn.Failure, "signed in passes with its version");
		expect(core::apply_status_check({ .Status = core::agent_status::signed_in, .Version = std::string(core::maximum_agent_version_bytes + 1U, '1') }).Version.empty(),
			   "an over-long version is not kept");
		expect(core::apply_status_check({ .Status = core::agent_status::not_signed_in }).Failure == agent_setup_failure::not_signed_in, "not signed in is its own failure");
		expect(core::apply_status_check({ .Status = core::agent_status::unreadable }).Failure == agent_setup_failure::status_unreadable,
			   "an unreadable answer is not taken as signed in");
		auto failureNames = std::set<std::string_view>{ };
		for (const auto failure :
			 { agent_setup_failure::agent_not_found, agent_setup_failure::program_refused, agent_setup_failure::no_program_chosen, agent_setup_failure::not_signed_in,
			   agent_setup_failure::failed_to_start, agent_setup_failure::status_unreadable, agent_setup_failure::timed_out, agent_setup_failure::workspace_unavailable }) {
			failureNames.insert(core::stable_name(failure));
			expect(core::fail_check(failure).State == setup_check_state::failed && core::fail_check(failure).Failure == failure, "a failure is kept with its reason");
		}
		expect(failureNames.size() == 8U, "each failure has its own stable name");

		const auto profiles = core::built_in_agent_profiles();
		auto       state    = core::agent_setup_state{ };
		expect(core::current_setup_step(state) == setup_step::agent, "nothing chosen: the Agent step");
		state.Rows   = core::agent_setup_rows(std::vector{ agent_detection{ .Agent = setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude" } }, profiles,
											  github_com(), codex_protocol_choice::app_server);
		state.Chosen = setup_agent::codex;
		expect(core::current_setup_step(state) == setup_step::agent, "a chosen agent with no row is not a choice");
		state.Chosen = core::choose_setup_agent(state.Rows, std::nullopt);
		expect(core::current_setup_step(state) == setup_step::repository, "an agent chosen: the Repository step");
		state.RepositoryChosen = true;
		expect(core::current_setup_step(state) == setup_step::check, "a repository chosen: Check it works");
		state.AgentCheck = signedIn;
		expect(core::current_setup_step(state) == setup_step::check, "one passed row is not a passed check");
		state.WorkspaceCheck = core::fail_check(agent_setup_failure::workspace_unavailable);
		expect(core::current_setup_step(state) == setup_step::check, "a failed row keeps the check open");
		state.WorkspaceCheck = { .State = setup_check_state::passed };
		expect(core::current_setup_step(state) == setup_step::ready, "both rows passed: Ready");
		state.Rows = core::agent_setup_rows(std::vector{ agent_detection{ .Agent = setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude" } }, profiles, ghes(),
											codex_protocol_choice::app_server);
		expect(core::current_setup_step(state) == setup_step::ready, "a permitted agent stays ready on GHES");
	}
}

auto main() -> int
{
	test_names_and_profiles();
	test_rows();
	test_choice();
	test_checks_and_steps();
	std::cout << "agent-setup-tests:pass\n";
	return EXIT_SUCCESS;
}
