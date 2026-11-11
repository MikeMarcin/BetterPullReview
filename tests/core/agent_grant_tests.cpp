/// \file
/// \brief [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104. The grant one turn
/// launches with, the words that state it, and the commands a turn lists.

#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_grant.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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
		// Owner look e97c6a3d (2026-11-11): "Allow build and run tests" is a
		// toggle per pull request. Turned on there, it is the answer, whatever
		// the agent's setting; turned off, it stays off.
		auto toggled        = facts(core::agent_program::claude);
		toggled.BuildAnswer = true;
		expect(core::builds_granted(core::resolve_agent_grant(toggled)), "the pull request's toggle allows builds with the agent's setting off");
		expect(core::agent_builds_toggle_checked(core::resolve_agent_grant(toggled)) && core::agent_builds_toggle_enabled(core::resolve_agent_grant(toggled)), "the toggle reads checked");
		toggled.OwnCheckout = true;
		expect(!core::agent_builds_toggle_enabled(core::resolve_agent_grant(toggled)) && !core::agent_builds_toggle_checked(core::resolve_agent_grant(toggled)), "unavailable, unchecked, in the reviewer's own checkout");
	}

	// Review finding 4 (2026-11-11): the picker states only what the launch
	// carries. A folder that cannot be made, a full folder and a refused
	// grant each read as what they are.
	void test_what_the_launch_carries()
	{
		auto value         = facts(core::agent_program::claude);
		value.ScratchPlace = false;
		auto grant         = core::resolve_agent_grant(value);
		expect(!core::scratch_granted(grant) && !core::agent_scratch_toggle_enabled(grant) && !core::agent_scratch_toggle_checked(grant), "no place for a folder: no allowance and the toggle is unavailable");
		expect(core::agent_scratch_tooltip(grant, core::agent_program::claude, 256U).find("No scratch folder") != std::string::npos, "and it says why");
		value             = facts(core::agent_program::claude);
		value.ScratchFull = true;
		grant             = core::resolve_agent_grant(value);
		expect(core::scratch_granted(grant) && !core::scratch_writable(grant) && core::grant_level(grant) == core::agent_grant_level::reads_only, "a full folder may be run, not written");
		expect(core::agent_scratch_toggle_text(grant) == "Use scratch folder (full)", "the toggle names it full");
		expect(core::agent_scratch_tooltip(grant, core::agent_program::claude, 64U).find("64 MiB") != std::string::npos, "with the configured limit");
		value         = facts(core::agent_program::codex);
		value.Builds  = true;
		value.Refusal = core::agent_grant_refusal::codex_own_permissions;
		grant         = core::resolve_agent_grant(value);
		expect(!core::scratch_granted(grant) && !core::builds_granted(grant) && !core::build_question_due(grant), "a refused grant carries nothing and asks nothing");
		expect(!core::agent_scratch_toggle_enabled(grant) && !core::agent_builds_toggle_enabled(grant), "both toggles unavailable");
		expect(core::agent_scratch_tooltip(grant, core::agent_program::codex, 256U).find("permissions profile") != std::string::npos, "with the refusal's reason");
	}

	// Review finding 1 (2026-11-11): BPR's Codex profile replaces the
	// reviewer's, so it is passed only where it cannot widen it.
	void test_codex_composition()
	{
		const auto none = std::vector<std::string>{ };
		const auto ok   = [&](const std::string_view aConfig) { return !core::codex_grant_refusal(std::array{ aConfig }, false, none).has_value(); };
		expect(ok(""), "no configuration composes");
		expect(ok("model = \"gpt-5\"\nsandbox_mode = \"workspace-write\"\napproval_policy = \"on-request\"\n"), "a sandbox mode and on-request compose");
		expect(ok("# default_permissions = \"x\"\nmodel = \"a\"\n"), "a comment is not a setting");
		const auto refused = [&](const std::string_view aConfig, const core::agent_grant_refusal aWhy) { return core::codex_grant_refusal(std::array{ aConfig }, false, none) == std::optional{ aWhy }; };
		expect(refused("default_permissions = \"mine\"\n[permissions.mine.filesystem]\n\"~/.ssh\" = \"none\"\n", core::agent_grant_refusal::codex_own_permissions), "the reviewer's own profile");
		expect(refused("[profiles.work]\ndefault_permissions = \"mine\"\n", core::agent_grant_refusal::codex_own_permissions), "in a profile too");
		expect(refused("approval_policy = \"untrusted\"\n", core::agent_grant_refusal::codex_asks_approval), "untrusted is never replaced by never");
		expect(refused("[profiles.x]\napproval_policy = { granular = true }\n", core::agent_grant_refusal::codex_asks_approval), "an approval BPR cannot read");
		expect(core::codex_grant_refusal(std::array<std::string_view, 0>{ }, true, none) == std::optional{ core::agent_grant_refusal::codex_config_unreadable }, "an unreadable configuration");
		const auto arguments = std::vector<std::string>{ "-c", "sandbox_mode=\"danger-full-access\"" };
		expect(core::codex_grant_refusal(std::array<std::string_view, 0>{ }, false, arguments) == std::optional{ core::agent_grant_refusal::custom_arguments }, "an agent whose own arguments set its sandbox");
		const auto profile = std::vector<std::string>{ "--profile", "work" };
		expect(core::codex_grant_refusal(std::array<std::string_view, 0>{ }, false, profile) == std::optional{ core::agent_grant_refusal::custom_arguments }, "or choose a profile");
		const auto model = std::vector<std::string>{ "-c", "model=\"o3\"" };
		expect(!core::codex_grant_refusal(std::array<std::string_view, 0>{ }, false, model).has_value(), "a model setting composes");
	}

	// Re-review N1 (2026-11-11). What the first fix missed.
	void test_codex_composition_round_two()
	{
		const auto none    = std::vector<std::string>{ };
		const auto refuses = [&](const std::string_view aConfig) { return core::codex_grant_refusal(std::array{ aConfig }, false, none).has_value(); };
		// (a) In the reviewer's own checkout Codex also reads that checkout's
		// .codex/config.toml once they trusted it (reproduced with `codex
		// sandbox`): BPR passes Codex nothing there. Claude is not affected.
		auto own        = facts(core::agent_program::codex);
		own.OwnCheckout = true;
		auto grant      = core::resolve_agent_grant(own);
		expect(grant.Refusal == std::optional{ core::agent_grant_refusal::codex_own_checkout } && !core::scratch_granted(grant) && !core::agent_scratch_toggle_enabled(grant)
				   && !core::agent_builds_toggle_enabled(grant),
			   "Codex in the reviewer's own checkout: no grant, both toggles unavailable");
		expect(core::agent_scratch_tooltip(grant, core::agent_program::codex, 256U).find("own checkout") != std::string::npos, "and it says why");
		own.Refusal = core::agent_grant_refusal::codex_own_permissions;
		expect(core::resolve_agent_grant(own).Refusal == std::optional{ core::agent_grant_refusal::codex_own_permissions }, "an earlier refusal keeps its reason");
		auto claude        = facts(core::agent_program::claude);
		claude.OwnCheckout = true;
		expect(core::scratch_granted(core::resolve_agent_grant(claude)), "Claude's scratch folder is unaffected there");
		// (b), (d) Codex's own files, from the home the launch passes it.
		expect(core::codex_configuration_paths("/Users/r")
				   == std::optional{ std::vector<std::string>{ "/Users/r/.codex/config.toml", "/etc/codex/config.toml", "/etc/codex/managed_config.toml" } },
			   "the user's file under the passed home, the system file and the managed file");
		expect(!core::codex_configuration_paths("").has_value() && !core::codex_configuration_paths("relative").has_value(), "no home: nothing can be read");
		// (c) Attached and long forms of the profile, sandbox and approval
		// flags, and any other flag BPR cannot judge (a working folder, an
		// added folder, a feature, ignoring the user's file): only flags known
		// to leave permissions alone compose.
		for (const auto* const argument : { "-pwork", "-p=work", "-sdanger-full-access", "-anever", "--profile=work", "--sandbox=danger-full-access", "--ask-for-approval=never", "-Cdir", "--cd=/x",
											"--add-dir", "--enable", "--approve-for-me", "--ignore-user-config", "-mo3p", "-xp" }) {
			const auto arguments = std::vector<std::string>{ argument };
			const auto refused   = core::codex_grant_refusal(std::array<std::string_view, 0>{ }, false, arguments) == std::optional{ core::agent_grant_refusal::custom_arguments };
			expect(refused != (std::string_view{ argument } == "-mo3p"), std::string{ "the argument " } + argument);
		}
		for (const auto& arguments : std::vector<std::vector<std::string>>{ { "-mgpt-5.1-codex" }, { "-cmodel=\"o3\"" }, { "exec", "--json", "-m", "-p", "-" }, { "-ipic-sa.png" }, { "--model=o3", "--color", "never" } }) {
			expect(!core::codex_grant_refusal(std::array<std::string_view, 0>{ }, false, arguments).has_value(), "a value is not a flag: " + arguments.front());
		}
		const auto cluster = std::vector<std::string>{ "-csandbox_mode=\"danger-full-access\"" };
		expect(core::codex_grant_refusal(std::array<std::string_view, 0>{ }, false, cluster).has_value(), "an attached -c setting is read");
		// A byte-order mark before the first setting or table.
		expect(refuses("\xEF\xBB\xBF"
					   "default_permissions = \"mine\"\n"),
			   "a BOM before a setting");
		expect(refuses("\xEF\xBB\xBF[permissions.mine]\nfilesystem = { \":root\" = \"read\" }\n"), "a BOM before a table");
		// An inline table under profiles holds settings a line scan cannot place.
		expect(refuses("profiles = { work = { default_permissions = \"mine\" } }\n"), "profiles as an inline table");
		expect(refuses("[profiles]\nwork = { approval_policy = \"untrusted\" }\n"), "a profile as an inline table");
		expect(refuses("profiles.work = { sandbox_mode = \"read-only\" }\n"), "any inline table under profiles");
		expect(!refuses("[tui]\nnotifications = { enabled = true }\n"), "an inline table elsewhere composes");
	}

	// Re-review N3, N6 (2026-11-11).
	void test_scratch_unmade_and_full()
	{
		// N3: a folder the last question could not make is not shown as
		// granted; the next question still asks for one.
		auto value          = facts(core::agent_program::claude);
		value.ScratchUnmade = true;
		auto grant          = core::resolve_agent_grant(value);
		expect(core::scratch_granted(grant) && !core::agent_scratch_toggle_checked(grant) && core::agent_scratch_toggle_enabled(grant), "unmade: unchecked, and the next question tries again");
		expect(core::agent_scratch_tooltip(grant, core::agent_program::claude, 256U).find("could not be made") != std::string::npos, "and it says so");
		// N6: a full folder gives Claude, whose scripts are not confined, no
		// run rule either; Codex's sandbox holds a script's writes.
		value             = facts(core::agent_program::claude);
		value.ScratchFull = true;
		grant             = core::resolve_agent_grant(value);
		const auto claude = core::agent_scratch_tooltip(grant, core::agent_program::claude, 256U);
		const auto codex  = core::agent_scratch_tooltip(grant, core::agent_program::codex, 256U);
		expect(claude.find("may read what is there") != std::string::npos && claude.find("may run") == std::string::npos, "full, Claude: read only");
		expect(codex.find("may run what is there") != std::string::npos, "full, Codex: run, not write");
		expect(core::agent_scratch_instruction("/s", true, 256U).find("only read") != std::string::npos, "a full folder's instruction says read only");
	}

	void test_words()
	{
		auto value = facts(core::agent_program::claude);
		expect(core::agent_scratch_toggle_text(core::resolve_agent_grant(value)) == "Use scratch folder", "the scratch toggle");
		expect(core::agent_builds_toggle_text == "Allow build and run tests", "the builds toggle");
		// AC05: only an enforcing runtime is said to have a sandbox.
		const auto scratch = core::resolve_agent_grant(facts(core::agent_program::claude));
		const auto claude  = core::agent_scratch_tooltip(scratch, core::agent_program::claude, 256U);
		expect(claude.find("sandbox") == std::string::npos && claude.find("permission rule") != std::string::npos, "Claude's grant is a rule, never a sandbox");
		expect(core::agent_scratch_tooltip(scratch, core::agent_program::codex, 256U).find("sandbox holds") != std::string::npos, "Codex's is enforced");
		// Review finding 5: off is not "Reads only": the agent's own settings still apply.
		value.ScratchChoice = false;
		const auto off      = core::agent_scratch_tooltip(core::resolve_agent_grant(value), core::agent_program::claude, 256U);
		expect(off.find("its own settings allow") != std::string::npos && off.find("Reads only") == std::string::npos, "off says what still applies");
		// Review finding 6: Claude's build rules are disclosed as unconfined.
		auto builds        = facts(core::agent_program::claude);
		builds.BuildAnswer = true;
		const auto told    = core::agent_builds_tooltip(core::resolve_agent_grant(builds), core::agent_program::claude);
		expect(told.find("any arguments") != std::string::npos && told.find("publish") != std::string::npos, "Claude's build programs run with any arguments, anywhere");
		expect(core::agent_build_question == "Building runs this pull request's code on this computer with your account's rights. Allow for this pull request?", "the question as written");
		const auto instruction = core::agent_scratch_instruction("/s", true, 128U);
		expect(instruction.find("/s") != std::string::npos && instruction.find("128 MiB") != std::string::npos && instruction.find("may not write") != std::string::npos, "the agent is told the path, the limit and why");
		expect(core::agent_scratch_instruction("/s", false, 256U).find("python3") != std::string::npos, "and which programs run its files");
		expect(core::agent_scratch_limit_bytes(256U) == 256ULL * 1024ULL * 1024ULL && core::agent_scratch_limit_mib(0U) == core::default_agent_scratch_limit_mib && core::agent_scratch_limit_mib(1U << 30U) == core::maximum_agent_scratch_limit_mib, "the limit is bounded");
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
	test_what_the_launch_carries();
	test_codex_composition();
	test_codex_composition_round_two();
	test_scratch_unmade_and_full();
	test_words();
	test_commands_listed();
	std::cout << "agent-grant-tests:pass\n";
	return EXIT_SUCCESS;
}
