/// \file
/// \brief [[BPR-1753]], RFC-BPR-0092 owner amendment 2026-10-23 ("I
/// should be able to define more than one custom agent"). The agent list: what
/// an entry may hold, its edits, the profile slot each entry launches as and
/// the policy that allows it, and the move from the single Claude wrapper and
/// Codex program settings.

#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_profile.hpp"

#include <algorithm>
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

	[[nodiscard]] auto work(const std::string& aName, const core::agent_protocol aProtocol, std::vector<std::string> aArguments) -> core::agent_entry
	{
		return { .Name = aName, .Protocol = aProtocol, .Program = "/opt/work/bin/agent-wrapper", .Arguments = std::move(aArguments) };
	}

	void test_the_owners_two_wrappers()
	{
		auto list = core::add_agent_entry({ }, work("Work Claude", core::agent_protocol::claude_stream_json, { }));
		expect(list.has_value() && list->front().Id == 1U, "a custom agent is added under the first identity");
		list = core::add_agent_entry(*list, work("Work Codex", core::agent_protocol::codex_jsonl_exec, { "--codex" }));
		expect(list && list->size() == 2U && (*list)[1].Id == 2U && (*list)[1].Arguments == std::vector<std::string>{ "--codex" } && (*list)[1].Program == (*list)[0].Program,
			   "two entries may run the same wrapper with different fixed arguments");
		expect(core::add_agent_entry(*list, work("Work Codex", core::agent_protocol::codex_app_server, { })).error() == core::agent_entry_refusal::duplicate_name,
			   "a name is used once");
		const auto claude = core::entry_profile_id((*list)[0]);
		const auto codex  = core::entry_profile_id((*list)[1]);
		expect(claude != codex && core::is_agent_entry_profile(claude) && core::is_agent_entry_profile(codex) && !core::is_agent_entry_profile(core::claude_stream_json_profile_id),
			   "each entry launches as its own slot, apart from the built-in profiles");
		expect(core::entry_of_profile(*list, codex) == &(*list)[1] && core::entry_of_profile(*list, core::codex_jsonl_profile_id) == nullptr, "a slot names its entry");
		const auto slots = core::agent_entry_profiles();
		const auto slot  = std::ranges::find(slots, codex, &core::agent_profile::Id);
		expect(slot != slots.end() && slot->Protocol == core::agent_protocol::codex_jsonl_exec && core::valid(*slot) && core::entry_profile((*list)[1]).DisplayName == "Work Codex",
			   "every slot is a valid profile of its protocol, shown under the entry's name");
		const auto policy = core::built_in_agent_policy(core::git_host_id{ 1U }, core::built_in_agent_host_kind::github_com);
		const auto ghes   = core::built_in_agent_policy(core::git_host_id{ 2U }, core::built_in_agent_host_kind::explicit_ghes);
		expect(std::ranges::find(policy.Allowed, codex) != policy.Allowed.end() && std::ranges::find(ghes.Allowed, claude) != ghes.Allowed.end()
				   && std::ranges::find(ghes.Allowed, codex) == ghes.Allowed.end(),
			   "an entry is allowed where its protocol's built-in profile is");
	}

	void test_what_an_entry_may_hold()
	{
		const auto refused = [](core::agent_entry aEntry) { return core::inspect_agent_entry(aEntry); };
		auto       entry   = work("Work Claude", core::agent_protocol::claude_stream_json, { });
		expect(!refused(entry), "a named entry with an absolute program is kept");
		auto relative    = entry;
		relative.Program = "agent-wrapper";
		expect(refused(relative) == core::agent_entry_refusal::invalid_program, "a program is an absolute path");
		auto windows    = entry;
		windows.Program = "C:\\Tools\\agent.exe";
		expect(!refused(windows), "a Windows path is absolute too");
		auto none    = entry;
		none.Program = { };
		expect(refused(none) == core::agent_entry_refusal::no_program, "a custom agent names its program");
		auto preset = core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { });
		expect(!refused(preset) && preset.Name == "Claude Code" && preset.Protocol == core::agent_protocol::claude_stream_json, "a popular agent may look for its own program");
		auto control      = entry;
		control.Arguments = { "--codex\n--danger" };
		expect(refused(control) == core::agent_entry_refusal::invalid_argument, "an argument holds no control character");
		auto many      = entry;
		many.Arguments = std::vector<std::string>(core::maximum_agent_entry_arguments + 1U, "-v");
		expect(refused(many) == core::agent_entry_refusal::too_many_arguments, "fixed arguments are bounded");
		auto effort   = entry;
		effort.Effort = "very high";
		expect(refused(effort) == core::agent_entry_refusal::invalid_effort, "an effort is one lowercase word; the harness says which it takes");
		auto codex   = work("Codex", core::agent_protocol::codex_app_server, { });
		codex.Effort = "minimal";
		codex.Model  = "gpt-5";
		expect(!refused(codex), "Codex takes minimal and a model name");
		auto harness  = work("Harness", core::agent_protocol::external_terminal, { });
		harness.Model = "opus";
		expect(refused(harness) == core::agent_entry_refusal::invalid_model, "the plain-text harness takes no model");
		auto unnamed = entry;
		unnamed.Name = " ";
		expect(refused(unnamed) == core::agent_entry_refusal::invalid_name, "an entry has a name");
	}

	void test_edits()
	{
		auto list        = *core::add_agent_entry({ }, work("A", core::agent_protocol::claude_stream_json, { }));
		list             = *core::add_agent_entry(list, work("B", core::agent_protocol::claude_stream_json, { }));
		list             = *core::add_agent_entry(list, work("C", core::agent_protocol::claude_stream_json, { }));
		auto edited      = list[1];
		edited.Name      = "B2";
		edited.Arguments = { "--profile", "work" };
		list             = *core::update_agent_entry(list, edited);
		expect(list[1].Name == "B2" && list[1].Id == 2U && list[1].Arguments.size() == 2U, "an edit keeps the entry's place and identity");
		list = *core::move_agent_entry(list, 3U, -1);
		expect(list[1].Id == 3U && list[2].Id == 2U, "an entry moves up");
		list = *core::move_agent_entry(list, 1U, -1);
		expect(list[0].Id == 1U, "and stays in the list at the top");
		list = *core::remove_agent_entry(list, 3U);
		expect(list.size() == 2U && core::remove_agent_entry(list, 3U).error() == core::agent_entry_refusal::unknown_entry, "an entry is removed once");
		list = *core::add_agent_entry(list, work("D", core::agent_protocol::claude_stream_json, { }));
		expect(list.back().Id == 3U, "the freed identity is used again");
		auto full = std::vector<core::agent_entry>{ };
		for (auto index = 0; index < static_cast<int>(core::maximum_agent_entries); ++index) {
			full = *core::add_agent_entry(full, work("Agent " + std::to_string(index), core::agent_protocol::claude_stream_json, { }));
		}
		expect(core::add_agent_entry(full, work("One more", core::agent_protocol::claude_stream_json, { })).error() == core::agent_entry_refusal::too_many_entries, "the list is bounded");
	}

	void test_migration()
	{
		auto settings          = core::legacy_agent_settings{ };
		settings.Chosen        = core::setup_agent::codex;
		settings.CodexProtocol = core::codex_protocol_choice::jsonl;
		settings.CodexProgram  = "/opt/codex/bin/codex";
		settings.CodexEffort   = "high";
		settings.ClaudeWrapper = "/opt/work/bin/claude-wrapper";
		settings.ClaudeModel   = "opus";
		settings.ClaudeEffort  = "not one";
		const auto moved       = core::migrate_agent_entries(settings);
		expect(moved.size() == 2U && moved[0].Name == "Codex" && moved[0].Protocol == core::agent_protocol::codex_jsonl_exec && moved[0].Program == settings.CodexProgram
				   && moved[0].Effort == "high" && moved[0].Preset == core::setup_agent::codex,
			   "the chosen Codex comes first, with its program, protocol and effort");
		expect(moved[1].Name == "Claude Code" && moved[1].Program == settings.ClaudeWrapper && moved[1].Model == "opus" && moved[1].Effort.empty(),
			   "the Claude wrapper becomes Claude Code's program, and an effort that is not one word is left at its default");
		expect(!core::inspect_agent_entries(moved), "the moved list is one that may be kept");
		expect(core::migrate_agent_entries({ }).empty(), "nothing set up moves to an empty list, so no stock agent appears unasked");
		auto harness           = core::legacy_agent_settings{ };
		harness.HarnessProgram = "/usr/local/bin/my-harness";
		const auto plain       = core::migrate_agent_entries(harness);
		expect(plain.size() == 1U && plain[0].Protocol == core::agent_protocol::external_terminal && !plain[0].Preset, "a plain-text harness with a program moves as a custom agent");
	}
} // namespace

auto main() -> int
{
	test_the_owners_two_wrappers();
	test_what_an_entry_may_hold();
	test_edits();
	test_migration();
	std::cout << "agent-entry-tests:pass\n";
	return EXIT_SUCCESS;
}
