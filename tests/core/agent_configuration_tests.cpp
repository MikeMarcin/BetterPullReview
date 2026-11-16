/// \file
/// \brief [[BPR-2016]], RFC-BPR-0107. The portable agent and prompt file:
/// what it carries, what it refuses, and what an import of it changes.

#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/agent_entry.hpp"
#include "bpr/core/review_preparation.hpp"

#include <algorithm>
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

	[[nodiscard]] auto custom(const std::uint32_t aId, std::string aName) -> core::agent_entry
	{
		auto entry      = core::agent_entry{ };
		entry.Id        = aId;
		entry.Name      = std::move(aName);
		entry.Protocol  = core::agent_protocol::claude_stream_json;
		entry.Program   = "/Users/someone/bin/work-claude";
		entry.Arguments = { "--profile", "work" };
		entry.Model     = "opus";
		entry.Effort    = "high";
		entry.Identity  = "0123456789abcdef0123456789abcdef";
		return entry;
	}

	[[nodiscard]] auto prompts() -> std::vector<core::local_prompt>
	{
		auto hook               = core::local_prompt{ };
		hook.Purpose            = "tour.problem";
		hook.BuiltIn            = "Write the problem.";
		hook.BuiltInVersion     = "review-preparation-v1";
		hook.BuiltInDigest      = core::writing_digest(hook.BuiltIn);
		auto feedback           = hook;
		feedback.Purpose        = "enhance.writing";
		feedback.BuiltIn        = "Enhance it.";
		feedback.BuiltInVersion = "enhance-writing-v1";
		feedback.BuiltInDigest  = core::writing_digest(feedback.BuiltIn);
		feedback.Override       = "Enhance it, briefly. /team-style";
		feedback.WrittenAgainst = "enhance-writing-v1";
		auto skill              = core::local_prompt{ };
		skill.Purpose           = "review.skill";
		skill.Kind              = core::prompt_kind::review_skill;
		skill.BuiltIn           = "---\nname: bpr-pr-review\ndescription: Review.\nmetadata:\n  bpr-version: 5\n---\nReview.\n";
		skill.BuiltInVersion    = "bpr-pr-review-v5";
		return { hook, feedback, skill };
	}

	constexpr auto Aliases = std::array{ core::prompt_alias{ .Retired = "enhance.comment", .Current = "enhance.writing" } };

	[[nodiscard]] auto exported(const std::vector<core::agent_entry>& aAgents, const std::vector<core::local_prompt>& aPrompts, const bool aBuiltIns) -> std::string
	{
		auto names = std::vector<std::string>{ };
		for (const auto& entry : aAgents) {
			names.push_back(entry.Name);
		}
		const auto ids           = core::portable_ids(names);
		auto       configuration = core::agent_configuration{ .Label = "Team", .ExportedBy = "BetterPullReview test" };
		for (std::size_t index = 0U; index < aAgents.size(); ++index) {
			configuration.Agents.push_back(core::portable_agent_of(aAgents[index], ids[index]));
		}
		for (const auto& prompt : aPrompts) {
			if (aBuiltIns || !prompt.Override.empty()) {
				configuration.Prompts.push_back(core::portable_prompt_of(prompt, aBuiltIns && prompt.Override.empty()));
			}
		}
		auto bytes = core::encode_agent_configuration(configuration);
		expect(bytes.has_value(), "a configuration encodes");
		return *bytes;
	}

	void test_round_trip_carries_only_portable_material()
	{
		auto entry           = custom(3U, "Work Claude");
		entry.ScratchStarts  = false;
		entry.BuildsAndTests = true;
		entry.CheckArguments = { "--version" };
		const auto local     = prompts();
		const auto bytes     = exported({ entry }, local, true);
		// AC02: no path, numeric ID or identity in the bytes.
		expect(bytes.find("/Users/someone") == std::string::npos, "the program's folder is not exported");
		expect(bytes.find("0123456789abcdef0123456789abcdef") == std::string::npos, "the identity is not exported");
		expect(bytes.find("\"id\": 3") == std::string::npos && bytes.find("\"id\": \"work-claude\"") != std::string::npos, "the portable ID is the name's, not the profile's");
		expect(bytes.find("\"program_name\": \"work-claude\"") != std::string::npos, "the program's file name is a binding hint");
		const auto decoded = core::decode_agent_configuration(bytes, local, Aliases);
		expect(decoded.has_value() && decoded->Label == "Team", "the file reads back");
		expect(decoded->Agents.size() == 1U && !decoded->Agents[0].Problem, "the agent reads back whole");
		const auto& agent = decoded->Agents[0].Agent;
		expect(agent.Name == "Work Claude" && agent.Arguments == entry.Arguments && agent.CheckArguments == entry.CheckArguments && agent.Model == "opus" && agent.Effort == "high",
			   "its settings read back");
		// AC07: both RFC-BPR-0104 settings ride the definition.
		expect(!agent.ScratchStarts && agent.BuildsAndTests, "scratch and builds are carried as requested");
		expect(decoded->Prompts.size() == 3U, "every prompt was chosen, edited or shipped");
		const auto& hook = decoded->Prompts[0].Prompt;
		expect(hook.Origin == core::portable_prompt_origin::built_in && hook.Text == local[0].BuiltIn && hook.WrittenAgainst == "review-preparation-v1"
				   && hook.WrittenAgainstDigest == local[0].BuiltInDigest,
			   "a shipped default is marked built-in with its version and digest");
		const auto& edited = decoded->Prompts[1].Prompt;
		expect(edited.Origin == core::portable_prompt_origin::edited && edited.Text == local[1].Override && edited.WrittenAgainst == "enhance-writing-v1", "an edit carries the version it was written against");
		expect(decoded->Prompts[2].Prompt.Origin == core::portable_prompt_origin::built_in && !decoded->Prompts[2].Problem, "the review skill's default is exportable");
	}

	/// [[BPR-2139]], [[BPR-2140]], RFC-BPR-0107 AC08 (owner, 2026-11-12). An
	/// agent's MCP servers and instructions settings ride its definition; a
	/// file written before them reads as Off and the system prompt.
	void test_mcp_and_instructions_ride_the_agent()
	{
		auto entry         = custom(3U, "Work Claude");
		entry.McpServers   = core::agent_mcp_servers::own;
		entry.Instructions = core::agent_instruction_delivery::message;
		const auto local   = prompts();
		const auto bytes   = exported({ entry }, local, false);
		expect(bytes.find("\"mcp_servers\": \"own\"") != std::string::npos && bytes.find("\"instructions\": \"message\"") != std::string::npos, "both settings are written by name");
		const auto decoded = core::decode_agent_configuration(bytes, local, Aliases);
		expect(decoded && decoded->Agents.size() == 1U && decoded->Agents[0].Agent.McpServers == core::agent_mcp_servers::own
				   && decoded->Agents[0].Agent.Instructions == core::agent_instruction_delivery::message,
			   "both read back");
		const auto imported = core::imported_entry(decoded->Agents[0].Agent, core::agent_import_choice{ .Program = "/usr/local/bin/work-claude" }, nullptr);
		expect(imported.McpServers == core::agent_mcp_servers::own && imported.Instructions == core::agent_instruction_delivery::message, "an import applies them as the file says");
		const auto before = core::agent_differences(core::agent_entry{ }, imported);
		expect(std::ranges::find(before, core::agent_field::mcp_servers) != before.end() && std::ranges::find(before, core::agent_field::instructions) != before.end(), "a replacement shows them among its differences");
		const auto older = core::decode_agent_configuration(
			R"({"format":"bpr.agent-configuration","version":1,"exported_by":"x","agents":[{"id":"c","name":"C","protocol":"claude_stream_json","program_name":"c","arguments":[],"check_arguments":[],"model":"","effort":"","strict_run_checks":true,"scratch_starts":true,"builds_and_tests":false}],"prompts":[]})",
			local);
		expect(older && older->Agents[0].Agent.McpServers == core::agent_mcp_servers::off && older->Agents[0].Agent.Instructions == core::agent_instruction_delivery::system_prompt,
			   "a file written before them reads as Off and the system prompt");
		const auto unknown = core::decode_agent_configuration(
			R"({"format":"bpr.agent-configuration","version":1,"exported_by":"x","agents":[{"id":"c","name":"C","protocol":"claude_stream_json","program_name":"c","arguments":[],"check_arguments":[],"model":"","effort":"","strict_run_checks":true,"scratch_starts":true,"builds_and_tests":false,"mcp_servers":"all"}],"prompts":[]})",
			local);
		expect(!unknown && unknown.error() == core::configuration_file_refusal::malformed, "a value this version does not know refuses the file");
	}

	void test_file_refusals()
	{
		const auto local = prompts();
		expect(core::decode_agent_configuration(std::string(core::maximum_agent_configuration_bytes + 1U, ' '), local).error() == core::configuration_file_refusal::too_large, "over 1 MiB");
		expect(core::decode_agent_configuration("\xff", local).error() == core::configuration_file_refusal::not_text, "not UTF-8");
		expect(core::decode_agent_configuration("{\"format\":", local).error() == core::configuration_file_refusal::malformed, "malformed");
		expect(core::decode_agent_configuration(R"({"format":"x","version":1})", local).error() == core::configuration_file_refusal::not_a_configuration, "foreign");
		expect(core::decode_agent_configuration(R"({"format":"bpr.agent-configuration","version":3,"future":[]})", local).error() == core::configuration_file_refusal::unsupported_version,
			   "newer");
		expect(core::decode_agent_configuration(R"({"format":"bpr.agent-configuration","version":1,"exported_by":"","agents":[],"prompts":[],"secret":"x"})", local).error()
				   == core::configuration_file_refusal::malformed,
			   "a field this version does not know is not silently dropped");
		expect(core::decode_agent_configuration(R"({"format":"bpr.agent-configuration","format":"bpr.agent-configuration","version":1})", local).error()
				   == core::configuration_file_refusal::malformed,
			   "a duplicate key");
	}

	void test_item_problems()
	{
		const auto local   = prompts();
		const auto bytes   = std::string{ R"({"format":"bpr.agent-configuration","version":1,"exported_by":"t","agents":[
			{"id":"a","name":"A","protocol":"future_protocol","arguments":[],"check_arguments":[],"model":"","effort":"","strict_run_checks":true,"scratch_starts":true,"builds_and_tests":false},
			{"id":"b","name":"B","protocol":"claude_stream_json","program_name":"b","arguments":[""],"check_arguments":[],"model":"","effort":"","strict_run_checks":true,"scratch_starts":true,"builds_and_tests":false},
			{"id":"c","name":"C","protocol":"claude_stream_json","program_name":"c","arguments":[],"check_arguments":[],"model":"","effort":"","strict_run_checks":true,"scratch_starts":true,"builds_and_tests":false},
			{"id":"c","name":"C2","protocol":"claude_stream_json","program_name":"c","arguments":[],"check_arguments":[],"model":"","effort":"","strict_run_checks":true,"scratch_starts":true,"builds_and_tests":false},
			{"id":"d","name":"D","protocol":"claude_stream_json","program_name":"../d","arguments":[],"check_arguments":[],"model":"","effort":"","strict_run_checks":true,"scratch_starts":true,"builds_and_tests":false}],
			"prompts":[
			{"purpose":"release.notes","origin":"edited","text":"later"},
			{"purpose":"enhance.comment","origin":"edited","text":"Old comment prompt"},
			{"purpose":"enhance.writing","origin":"edited","text":"Twice"},
			{"purpose":"tour.problem","origin":"edited","text":""},
			{"purpose":"review.skill","origin":"edited","text":"not a skill"}]})" };
		const auto decoded = core::decode_agent_configuration(bytes, local, Aliases);
		expect(decoded.has_value(), "item problems do not refuse the file");
		expect(decoded->Agents[0].Problem == core::configuration_item_problem::unsupported_protocol && decoded->Agents[0].Unsupported == "future_protocol", "an unknown protocol");
		expect(decoded->Agents[1].Problem == core::configuration_item_problem::invalid_agent && decoded->Agents[1].Refusal == core::agent_entry_refusal::invalid_argument,
			   "the profile's own validator speaks");
		expect(!decoded->Agents[2].Problem && decoded->Agents[3].Problem == core::configuration_item_problem::duplicate, "a portable ID twice");
		expect(decoded->Agents[4].Problem == core::configuration_item_problem::invalid_agent, "a program name with a folder");
		expect(decoded->Prompts[0].Problem == core::configuration_item_problem::unsupported_purpose, "a purpose of a later version is unsupported, not an inert success");
		expect(!decoded->Prompts[1].Problem && decoded->Prompts[1].Local == 1U, "a retired purpose ID names the prompt that replaced it");
		expect(decoded->Prompts[2].Problem == core::configuration_item_problem::duplicate, "the same purpose twice");
		expect(decoded->Prompts[3].Problem == core::configuration_item_problem::invalid_prompt, "empty text");
		expect(decoded->Prompts[4].Problem == core::configuration_item_problem::invalid_prompt, "a review skill the skill rules refuse");
		const auto apply = core::apply_agent_configuration({ }, local, *decoded, std::array{ core::agent_import_choice{ .Item = 0U, .Action = core::agent_import_action::add } }, { });
		expect(!apply && apply.error().Refusal == core::configuration_apply_refusal::unusable_item, "an item with a problem cannot be chosen");
	}

	void test_binding_and_checks()
	{
		auto relaxed            = custom(1U, "Relaxed");
		relaxed.StrictRunChecks = false;
		relaxed.BuildsAndTests  = true;
		relaxed.ScratchStarts   = false;
		const auto local        = prompts();
		const auto decoded      = core::decode_agent_configuration(exported({ relaxed }, { }, false), local);
		expect(decoded.has_value(), "decodes");
		// AC03: a custom agent needs a program on this computer.
		auto       choice  = core::agent_import_choice{ .Item = 0U, .Action = core::agent_import_action::add };
		const auto unbound = core::apply_agent_configuration({ }, local, *decoded, std::array{ choice }, { });
		expect(!unbound && unbound.error().Refusal == core::configuration_apply_refusal::unbound_program, "an unbound custom agent is not added");
		choice.Program = "relative/program";
		expect(core::apply_agent_configuration({ }, local, *decoded, std::array{ choice }, { }).error().Refusal == core::configuration_apply_refusal::unbound_program, "a relative program");
		choice.Program   = "/opt/bin/relaxed";
		const auto added = core::apply_agent_configuration({ }, local, *decoded, std::array{ choice }, { });
		expect(added.has_value() && added->Agents.size() == 1U, "bound, it is added");
		const auto& entry = added->Agents[0];
		// AC04, AC07: nothing relaxed or granted without acceptance; Off applied.
		expect(entry.StrictRunChecks && !entry.BuildsAndTests && !entry.ScratchStarts, "strict and no builds unless accepted; scratch Off as requested");
		expect(entry.Identity.empty() && entry.Id == 1U && entry.Program == "/opt/bin/relaxed", "a new entry, its identity left for the host to mint");
		choice.AcceptRelaxedChecks = true;
		choice.AcceptBuilds        = true;
		const auto accepted        = core::apply_agent_configuration({ }, local, *decoded, std::array{ choice }, { });
		expect(!accepted->Agents[0].StrictRunChecks && accepted->Agents[0].BuildsAndTests, "accepted for this entry, they apply");
	}

	void test_replace_and_identity()
	{
		auto       existing = custom(2U, "Work Claude");
		const auto local    = prompts();
		auto       other    = existing;
		other.Model         = "sonnet";
		const auto decoded  = core::decode_agent_configuration(exported({ other }, { }, false), local);
		expect(core::colliding_agent(std::array{ existing }, decoded->Agents[0].Agent) != nullptr, "the same name collides");
		// Only the model changes and the program is kept: same identity.
		auto       choice = core::agent_import_choice{ .Item = 0U, .Action = core::agent_import_action::replace, .Target = 2U };
		const auto kept   = core::apply_agent_configuration(std::array{ existing }, local, *decoded, std::array{ choice }, { });
		expect(kept.has_value() && kept->Detached.empty() && kept->Agents[0].Identity == existing.Identity && kept->Agents[0].Model == "sonnet", "a model change keeps conversations");
		expect(core::agent_differences(existing, kept->Agents[0]) == std::vector{ core::agent_field::model }, "the difference is the model");
		choice.Program      = "/usr/local/bin/other";
		const auto detached = core::apply_agent_configuration(std::array{ existing }, local, *decoded, std::array{ choice }, { });
		expect(detached->Detached == std::vector<std::uint32_t>{ 2U } && detached->Agents[0].Identity.empty(), "a program change detaches conversations");
		// Use existing changes nothing.
		choice.Action     = core::agent_import_action::use_existing;
		const auto mapped = core::apply_agent_configuration(std::array{ existing }, local, *decoded, std::array{ choice }, { });
		expect(mapped->Agents == std::vector{ existing } && mapped->AgentsApplied == 1U, "mapping does not update the existing agent");
		// A popular agent meets the one already added.
		const auto claude  = core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { });
		auto       mine    = claude;
		mine.Id            = 5U;
		mine.Name          = "My Claude";
		const auto  preset = core::decode_agent_configuration(exported({ claude }, { }, false), local);
		const auto  roster = std::array{ mine };
		const auto* met    = core::colliding_agent(roster, preset->Agents[0].Agent);
		expect(met != nullptr && met->Id == 5U, "an imported Claude Code meets the existing one");
		expect(preset->Agents[0].Agent.ProgramName.empty(), "a popular agent's own program has no binding hint");
	}

	void test_final_profile_limits()
	{
		const auto local  = prompts();
		auto       agents = std::vector<core::agent_entry>{ };
		for (std::uint32_t id = 1U; id <= core::maximum_agent_entries; ++id) {
			agents.push_back(custom(id, "Agent " + std::to_string(id)));
		}
		const auto decoded = core::decode_agent_configuration(exported({ custom(1U, "One more") }, { }, false), local);
		const auto choice  = core::agent_import_choice{ .Item = 0U, .Action = core::agent_import_action::add, .Program = "/bin/more" };
		const auto full    = core::apply_agent_configuration(agents, local, *decoded, std::array{ choice }, { });
		expect(!full && full.error().Entry == core::agent_entry_refusal::too_many_entries, "the import must fit the profile's 16 agents");
		const auto clash = core::apply_agent_configuration(std::array{ custom(4U, "One more") }, local, *decoded, std::array{ choice }, { });
		expect(!clash && clash.error().Entry == core::agent_entry_refusal::duplicate_name, "adding under a name in use is refused");
	}

	void test_prompts_apply()
	{
		auto local                 = prompts();
		auto improved              = local;
		improved[0].Override       = "Write the problem in two sentences.";
		improved[0].WrittenAgainst = "review-preparation-v1";
		const auto decoded         = core::decode_agent_configuration(exported({ }, improved, false), local, Aliases);
		expect(decoded->Prompts.size() == 2U, "the two edited prompts");
		const auto none = core::apply_agent_configuration({ }, local, *decoded, { }, { });
		expect(none->Prompts == local, "nothing chosen keeps every prompt");
		const auto taken = core::apply_agent_configuration({ }, local, *decoded, { }, std::array{ std::size_t{ 0U } });
		expect(taken->Prompts[0].Override == improved[0].Override && taken->Prompts[0].WrittenAgainst == "review-preparation-v1" && taken->Prompts[1] == local[1],
			   "the chosen prompt becomes the override, the rest are kept");
		// A file whose text is the shipped default restores the default.
		const auto shipped = core::decode_agent_configuration(exported({ }, local, true), local, Aliases);
		const auto reset   = core::apply_agent_configuration({ }, local, *shipped, { }, std::array{ std::size_t{ 1U } });
		expect(shipped->Prompts[1].Prompt.Origin == core::portable_prompt_origin::edited, "an edited prompt stays edited when built-ins are included");
		auto withBuiltIn = local;
		withBuiltIn[1].Override.clear();
		const auto plain = core::decode_agent_configuration(exported({ }, withBuiltIn, true), local, Aliases);
		const auto back  = core::apply_agent_configuration({ }, local, *plain, { }, std::array{ std::size_t{ 1U } });
		expect(back->Prompts[1].Override.empty() && !back->Prompts[1].WrittenAgainst, "importing the shipped text restores the default");
		static_cast<void>(reset);
	}

	/// [[BPR-2032]] (design workflow-import-v1, B). The core decides what
	/// each item is here and its starting choice: everything the file can
	/// bring starts chosen, an update included; builds start unaccepted.
	void test_import_plan()
	{
		const auto local     = prompts();
		auto       same      = custom(2U, "Work Claude");
		auto       changed   = custom(3U, "Lint");
		auto       fresh     = custom(4U, "Brand new");
		fresh.BuildsAndTests = true;
		auto theirs          = changed;
		theirs.Model         = "sonnet";
		auto improved        = local;
		improved[0].Override = "Write the problem in two sentences.";
		auto file            = core::agent_configuration{ .Label = "Team" };
		file.Agents          = { core::portable_agent_of(same, "work-claude"), core::portable_agent_of(theirs, "lint"), core::portable_agent_of(fresh, "brand-new") };
		file.Prompts         = { core::portable_prompt_of(improved[0], false), core::portable_prompt_of(local[1], false) };
		auto workflow        = core::workflow_configuration{ .PortableId = "org", .Name = "Org", .Origin = "Org", .Guidance = "Why.", .Actions = { { .Id = "check", .Slot = core::workflow_slot::check_preparation, .Task = "Check", .Binding = core::workflow_binding::command, .ProgramName = "org", .Arguments = { "{request}", "{response}" }, .Effects = { core::workflow_effect::run_check } } } };
		auto newer           = workflow;
		newer.Actions[0].Effects.push_back(core::workflow_effect::change_ticket);
		auto other         = workflow;
		other.PortableId   = "other";
		file.Workflows     = { newer, other };
		const auto decoded = core::decode_agent_configuration(core::encode_agent_configuration(file).value(), local, Aliases);
		expect(decoded.has_value(), "the planned file reads back");
		const auto mineWorkflows = std::array{ core::local_workflow{ .Workflow = workflow } };
		const auto plan          = core::plan_configuration_import(*decoded, std::array{ same, changed }, local, mineWorkflows);
		expect(plan.Agents.size() == 3U && plan.Prompts.size() == 2U && plan.Workflows.size() == 2U, "one plan per item");
		expect(plan.Agents[0].Standing == core::import_standing::unchanged && plan.Agents[0].Default.Action == core::agent_import_action::leave, "an agent already here as the file says is unchanged");
		expect(plan.Agents[1].Standing == core::import_standing::updated && plan.Agents[1].Default.Action == core::agent_import_action::replace && plan.Agents[1].Default.Target == 3U,
			   "an update starts chosen, replacing the agent of its name");
		expect(plan.Agents[2].Standing == core::import_standing::added && plan.Agents[2].Default.Action == core::agent_import_action::add && plan.Agents[2].NeedsProgram,
			   "a new custom agent starts added and waits for its program");
		expect(!plan.Agents[2].Default.AcceptBuilds && !plan.Agents[2].Default.AcceptRelaxedChecks, "build and run tests starts unaccepted");
		expect(plan.Agents[2].StandIns == std::vector<std::uint32_t>{ 2U, 3U }, "agents of its kind may stand for it");
		expect(plan.Prompts[0].Standing == core::import_standing::updated && plan.Prompts[0].Default, "a different prompt starts chosen");
		expect(plan.Prompts[1].Standing == core::import_standing::unchanged && !plan.Prompts[1].Default, "the same prompt has nothing to choose");
		expect(plan.Workflows[0].Standing == core::import_standing::updated && plan.Workflows[0].Default == core::workflow_import_action::replace && plan.Workflows[0].Local == 0U,
			   "a workflow update starts chosen");
		expect(std::ranges::contains(plan.Workflows[0].Differences, core::workflow_difference{ .Change = core::workflow_change::effects_widened, .Action = "check" }), "its widened effect is marked");
		expect(plan.Workflows[1].Standing == core::import_standing::added && plan.Workflows[1].Default == core::workflow_import_action::add, "a new workflow starts added");
		const auto again = core::plan_configuration_import(*decoded, std::array{ same, changed }, local, std::array{ core::local_workflow{ .Workflow = newer } });
		expect(again.Workflows[0].Standing == core::import_standing::unchanged && again.Workflows[0].Default == core::workflow_import_action::leave, "the same revision is unchanged");
		// An agent of the same name but another kind cannot be updated.
		auto harness     = same;
		harness.Protocol = core::agent_protocol::external_terminal;
		const auto clash = core::plan_configuration_import(*decoded, std::array{ harness }, local, { });
		expect(clash.Agents[0].Standing == core::import_standing::updated && clash.Agents[0].Default.Action == core::agent_import_action::leave, "another kind of the same name starts left out");
	}

	void test_portable_ids()
	{
		const auto names = std::vector<std::string>{ "Work Claude", "work  claude!", "", "Ünïcode" };
		const auto ids   = core::portable_ids(names);
		expect(ids == std::vector<std::string>{ "work-claude", "work-claude-2", "agent", "n-code" }, "portable IDs are slugs, unique in the file");
	}
} // namespace

auto main() -> int
{
	test_round_trip_carries_only_portable_material();
	test_file_refusals();
	test_item_problems();
	test_binding_and_checks();
	test_replace_and_identity();
	test_final_profile_limits();
	test_prompts_apply();
	test_portable_ids();
	test_import_plan();
	test_mcp_and_instructions_ride_the_agent();
	std::cout << "agent-configuration-tests:pass\n";
	return EXIT_SUCCESS;
}
