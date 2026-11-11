/// \file
/// \brief [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104. The grant one turn
/// launches with, the words that state it, and the commands a turn lists.

#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_grant.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto facts(const core::agent_program aProgram) -> core::agent_grant_facts
	{
		auto value    = core::agent_grant_facts{ };
		value.Program = aProgram;
		return value;
	}

	void test_entry_defaults()
	{
		// AC08: a new agent and one carried over start with the folder on and
		// builds off.
		const auto added = core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { });
		expect(added.ScratchStarts, "a new agent's scratch folder starts on");
		expect(!added.BuildsAndTests, "a new agent's builds start off");
		auto legacy   = core::legacy_agent_settings{ };
		legacy.Chosen = core::setup_agent::codex;
		for (const auto& entry : core::migrate_agent_entries(legacy)) {
			expect(entry.ScratchStarts && !entry.BuildsAndTests, "a carried-over agent starts with the folder on and builds off");
		}
	}

	void test_scratch_follows_start_until_chosen()
	{
		auto value = facts(core::agent_program::claude);
		expect(core::scratch_granted(core::resolve_agent_grant(value)), "on by default");
		value.ScratchStarts = false;
		expect(!core::scratch_granted(core::resolve_agent_grant(value)), "an unchanged pull request follows the starting state");
		value.ScratchChoice = true;
		expect(core::scratch_granted(core::resolve_agent_grant(value)), "the pull request's own choice wins");
		value.ScratchStarts = true;
		value.ScratchChoice = false;
		expect(!core::scratch_granted(core::resolve_agent_grant(value)), "turned off in this pull request stays off");
		auto harness = core::agent_grant_facts{ };
		expect(!core::resolve_agent_grant(harness).Applies && !core::scratch_granted(core::resolve_agent_grant(harness)), "a plain-text harness is given nothing");
	}

	void test_builds()
	{
		auto value = facts(core::agent_program::codex);
		expect(core::resolve_agent_grant(value).Builds == core::agent_builds_state::off, "off until turned on");
		value.Builds = true;
		expect(core::build_question_due(core::resolve_agent_grant(value)), "on in a disposable checkout asks first");
		expect(!core::builds_granted(core::resolve_agent_grant(value)), "unasked carries no build allowance");
		value.BuildAnswer = false;
		expect(core::resolve_agent_grant(value).Builds == core::agent_builds_state::declined && !core::builds_granted(core::resolve_agent_grant(value)), "declining leaves no build allowance");
		value.BuildAnswer = true;
		expect(core::builds_granted(core::resolve_agent_grant(value)), "allowed carries it");
		value.OwnCheckout = true;
		expect(core::resolve_agent_grant(value).Builds == core::agent_builds_state::own_checkout && !core::builds_granted(core::resolve_agent_grant(value)), "never in the reviewer's own checkout");
		expect(!core::build_question_due(core::resolve_agent_grant(value)), "nothing to ask there");
	}

	void test_words()
	{
		auto value = facts(core::agent_program::claude);
		expect(core::agent_grant_line(core::grant_level(core::resolve_agent_grant(value))) == "Writes only in its scratch folder", "scratch line");
		value.ScratchChoice = false;
		expect(core::agent_grant_line(core::grant_level(core::resolve_agent_grant(value))) == "Reads only", "reads-only line");
		value.Builds      = true;
		value.BuildAnswer = true;
		expect(core::agent_grant_line(core::grant_level(core::resolve_agent_grant(value))) == "Builds and tests in its disposable checkout", "builds line");
		expect(core::agent_builds_line(core::agent_builds_state::off, "Claude Code") == "Builds and tests are off for Claude Code", "builds-off item");
		// AC05: only an enforcing runtime is said to have a sandbox.
		auto       scratch = core::resolve_agent_grant(facts(core::agent_program::claude));
		const auto claude  = core::agent_grant_enforcement(scratch, core::agent_program::claude);
		expect(claude.find("sandbox") == std::string::npos && claude.find("permission rule") != std::string::npos, "Claude's grant is a rule, never a sandbox");
		expect(core::agent_grant_enforcement(scratch, core::agent_program::codex).find("sandbox enforces") != std::string::npos, "Codex's is enforced");
		expect(core::agent_build_question == "Building runs this pull request's code on this computer with your account's rights. Allow for this pull request?", "the question as written");
		const auto told = core::agent_scratch_instruction("/s", true);
		expect(told.find("/s") != std::string::npos && told.find("256 MiB") != std::string::npos && told.find("may not write") != std::string::npos, "the agent is told the path, the limit and why");
	}

	void test_commands_listed()
	{
		auto update    = core::agent_activity_update{ };
		update.Kind    = core::agent_activity_kind::doing;
		update.Doing   = core::agent_doing::running;
		update.Subject = "python3 /s/count.py";
		auto activity  = core::apply_agent_activity({ }, update, 1U);
		activity       = core::apply_agent_activity(activity, update, 2U);
		expect(activity.Commands == 2U && activity.CommandsRun.size() == 1U && activity.CommandsRun.front() == "python3 /s/count.py", "each command listed once, counted each time");
		for (auto index = 0U; index < 40U; ++index) {
			update.Subject = "echo " + std::to_string(index);
			activity       = core::apply_agent_activity(activity, update, 3U + index);
		}
		expect(activity.CommandsRun.size() == core::maximum_agent_activity_listed, "the list is bounded");
	}
} // namespace

auto main() -> int
{
	test_entry_defaults();
	test_scratch_follows_start_until_chosen();
	test_builds();
	test_words();
	test_commands_listed();
	std::cout << "agent-grant-tests:pass\n";
	return EXIT_SUCCESS;
}
