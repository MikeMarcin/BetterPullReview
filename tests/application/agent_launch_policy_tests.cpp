/// \file
/// \brief The owner's agent launch policy of 2026-11-12: every launch path's
/// session mode ([[BPR-2138]], RFC-BPR-0101 AC10), MCP servers per agent
/// ([[BPR-2139]], RFC-BPR-0104 AC10), a built-in prompt's instructions in
/// the system prompt ([[BPR-2140]], RFC-BPR-0107 AC09), Claude's options
/// with several values before `-p` and a wrapper's failed sign-in
/// ([[BPR-2142]], RFC-BPR-0104 AC11, RFC-BPR-0103 AC10).

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/tour_agent_conversation.hpp"
#include "bpr/application/tour_agent_drafting.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/agent_run_policy.hpp"

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <map>
#include <optional>
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

	[[nodiscard]] auto profile(const core::agent_protocol aProtocol) -> core::agent_profile
	{
		return { .Id = core::agent_profile_id{ 1U }, .DisplayName = "Canary", .Executable = "/opt/agents/canary", .Protocol = aProtocol };
	}

	[[nodiscard]] auto supplement(const std::string& aMessage = "What does this change do?") -> core::agent_context_supplement
	{
		auto value    = core::agent_context_supplement{ };
		value.Session = core::agent_session{ .Id = core::agent_session_id{ 1U }, .Profile = core::agent_profile_id{ 1U } };
		value.Message = aMessage;
		return value;
	}

	[[nodiscard]] auto reader() -> application::environment_reader
	{
		return [](const std::string&) -> std::optional<std::string> { return std::nullopt; };
	}

	[[nodiscard]] auto has(const std::vector<std::string>& aArguments, const std::string_view aArgument) -> bool
	{
		return std::ranges::find(aArguments, aArgument) != aArguments.end();
	}

	[[nodiscard]] auto index_of(const std::vector<std::string>& aArguments, const std::string_view aArgument) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::find(aArguments, aArgument) - aArguments.begin());
	}

	/// RFC-BPR-0101 AC10. The owner's classification, written out here so a
	/// change to it is a change to this test.
	void test_every_launch_path_has_the_owners_session_mode()
	{
		using path        = core::agent_run_path;
		const auto fresh  = core::agent_session_mode::fresh;
		const auto resume = core::agent_session_mode::resumed;
		const auto table  = std::map<path, core::agent_session_mode>{
			{ path::enhance_writing, fresh },
			{ path::tour_enhance, fresh },
			{ path::tour_field_draft, fresh },
			{ path::review_feedback_hook, fresh },
			{ path::draft_tour_first_draft, fresh },
			{ path::local_description_first_draft, fresh },
			{ path::local_commit_message, fresh },
			{ path::agent_check, fresh },
			{ path::assistance_first_answer, fresh },
			{ path::agent_tab, resume },
			{ path::tour_side_chat, resume },
			{ path::tour_conversation, resume },
			{ path::local_draft_discussion, resume },
			{ path::review_cycle_round, resume },
			{ path::self_review_loop, resume },
			{ path::address_feedback_run, resume },
		};
		expect(table.size() == core::agent_run_path_count, "every launch path is classified");
		for (const auto& [launchPath, mode] : table) {
			expect(core::session_mode_of(launchPath) == mode, "a launch path has the owner's session mode");
			expect(!core::stable_name(launchPath).empty(), "a launch path has a name");
			if (const auto promoted = core::ask_more_path(launchPath)) {
				expect(mode == fresh && core::session_mode_of(*promoted) == resume, "Ask more promotes a fresh answer into a resumed session");
			}
		}
		expect(core::ask_more_path(path::draft_tour_first_draft) == path::tour_conversation, "Draft Tour's Ask more is the whole-tour conversation");
		expect(!core::ask_more_path(path::enhance_writing), "an Enhance offers no conversation");
	}

	/// RFC-BPR-0101 AC10. Each path's launch keeps its mode: a fresh one
	/// saves no session and is never resumed; a resumed one resumes.
	void test_each_launch_path_launches_in_its_session_mode()
	{
		for (auto index = std::size_t{ 0U }; index < core::agent_run_path_count; ++index) {
			const auto launchPath = static_cast<core::agent_run_path>(index);
			const auto fresh      = core::session_mode_of(launchPath) == core::agent_session_mode::fresh;
			const auto name       = std::string{ core::stable_name(launchPath) };
			const auto claude     = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement(), reader(), { .Path = launchPath });
			expect(claude.has_value(), "a Claude launch builds on " + name);
			expect(has(claude->Arguments, "--no-session-persistence") == fresh, "Claude saves a session exactly on a resumed path: " + name);
			expect(!has(claude->Arguments, "--resume"), "no launch starts out resuming: " + name);
			auto turn         = *claude;
			turn.NativeTurn   = 2U;
			turn.NativeHandle = "0f8e7c1a-2b3d-4e5f-8a9b-0c1d2e3f4a5b";
			const auto native = application::prepare_native_agent_turn(turn, false);
			expect(native.has_value() != fresh, "a native turn is refused exactly on a fresh path: " + name);
			if (native) {
				expect(has(native->Arguments, "--resume"), "a resumed path's native turn resumes: " + name);
			}
			const auto codex = application::build_agent_launch(profile(core::agent_protocol::codex_jsonl_exec), supplement(), reader(), { .Path = launchPath });
			expect(codex && has(codex->Arguments, "--ephemeral") == fresh, "Codex exec is ephemeral exactly on a fresh path: " + name);
			expect(codex && codex->Arguments.back() == "-", "Codex exec still reads its prompt from standard input: " + name);
		}
		// A writing or drafting launch (the restricted sets) is fresh too.
		const auto writing = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement(), reader(), { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing });
		expect(writing && has(writing->Arguments, "--no-session-persistence") && writing->Path == core::agent_run_path::enhance_writing, "an Enhance launch saves no session and records its path");
	}

	/// RFC-BPR-0104 AC10. MCP servers are off unless the agent says to use
	/// its own, and a fresh path keeps them off regardless.
	void test_mcp_servers_follow_the_agents_setting()
	{
		const auto off      = std::vector<std::string>{ "--strict-mcp-config", "--mcp-config", R"({"mcpServers":{}})" };
		const auto contains = [](const std::vector<std::string>& aArguments, const std::vector<std::string>& aRun) {
			return std::ranges::search(aArguments, aRun).begin() != aArguments.end();
		};
		const auto claude      = profile(core::agent_protocol::claude_stream_json);
		const auto byDefault   = application::build_agent_launch(claude, supplement(), reader());
		const auto own         = application::build_agent_launch(claude, supplement(), reader(), { .Path = core::agent_run_path::agent_tab, .McpServers = core::agent_mcp_servers::own });
		const auto ownButFresh = application::build_agent_launch(claude, supplement(), reader(), { .Path = core::agent_run_path::assistance_first_answer, .McpServers = core::agent_mcp_servers::own });
		expect(byDefault && contains(byDefault->Arguments, off), "a Claude conversation starts with no MCP server by default");
		expect(own && !has(own->Arguments, "--mcp-config") && !has(own->Arguments, "--strict-mcp-config"), "the agent's own configuration passes nothing for MCP");
		expect(ownButFresh && contains(ownButFresh->Arguments, off), "a fresh one-off keeps MCP off whatever the setting says");
		for (const auto protocol : { core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec }) {
			const auto codexOff = application::build_agent_launch(profile(protocol), supplement(), reader());
			const auto codexOwn = application::build_agent_launch(profile(protocol), supplement(), reader(), { .McpServers = core::agent_mcp_servers::own });
			expect(codexOff && has(codexOff->Arguments, "mcp_servers={}"), "Codex starts with no MCP server by default");
			expect(codexOwn && !has(codexOwn->Arguments, "mcp_servers={}"), "Codex with its own configuration is given no MCP override");
		}
		const auto round    = application::build_round_launch(claude, "round", std::nullopt, "/tmp", reader());
		const auto roundOwn = application::build_round_launch(claude, "round", std::nullopt, "/tmp", reader(), core::agent_mcp_servers::own);
		expect(round && contains(round->Arguments, off) && roundOwn && !has(roundOwn->Arguments, "--mcp-config"), "a Review Cycle round follows the agent's setting");
		expect(core::parse_agent_mcp_servers("own") == core::agent_mcp_servers::own && !core::parse_agent_mcp_servers("on"), "the setting's names read back");
	}

	/// RFC-BPR-0104 AC11. In every Claude launch each option that takes
	/// several values comes before `-p` and is followed by another option.
	void test_options_with_several_values_come_before_the_prompt_flag()
	{
		const auto claude   = profile(core::agent_protocol::claude_stream_json);
		auto       launches = std::vector<application::agent_launch_request>{ };
		const auto add      = [&launches](std::expected<application::agent_launch_request, application::agent_launch_failure> aLaunch) {
			expect(aLaunch.has_value(), "the launch builds");
			launches.push_back(std::move(*aLaunch));
		};
		add(application::build_agent_launch(claude, supplement(), reader()));
		add(application::build_agent_launch(claude, supplement(), reader(), { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing }));
		add(application::build_agent_launch(claude, supplement(), reader(), { .ReadOnlyTools = true, .Path = core::agent_run_path::tour_field_draft }));
		add(application::build_agent_launch(claude, supplement(), reader(),
											{ .Grant = application::agent_launch_grant{ .ScratchFolder = "/tmp/bpr-scratch/run-1" }, .PullRequestCheckout = { "/tmp/bpr-checkout" } }));
		add(application::build_agent_launch(claude, supplement(), reader(), { .Path = core::agent_run_path::enhance_writing, .SystemInstructions = "Write plainly." }));
		add(application::build_round_launch(claude, "round", std::nullopt, "/tmp", reader()));
		for (const auto& launch : launches) {
			const auto& arguments = launch.Arguments;
			const auto  print     = index_of(arguments, "-p");
			expect(print < arguments.size(), "every Claude launch is in print mode");
			for (auto at = std::size_t{ 0U }; at < arguments.size(); ++at) {
				if (std::ranges::find(application::claude_variadic_options, arguments[at]) == application::claude_variadic_options.end()) {
					continue;
				}
				expect(at < print, "an option with several values comes before -p: " + arguments[at]);
				auto next = at + 2U;
				while (next < arguments.size() && !arguments[next].starts_with("-")) {
					++next;
				}
				expect(next <= print, "an option with several values is followed by another option: " + arguments[at]);
			}
			expect(!arguments.back().empty() && arguments.back() != "*", "a Claude launch never ends on an option's value that could take more");
		}
	}

	/// RFC-BPR-0107 AC09. The instructions go in Claude's inline system
	/// prompt and Codex exec's developer instructions; pasted where the agent
	/// says so, on the App Server, and past the Windows bound.
	void test_built_in_instructions_reach_the_system_prompt()
	{
		const auto rule    = std::string{ "Canary rule: end every reply with the single word PINEAPPLE." };
		const auto claude  = profile(core::agent_protocol::claude_stream_json);
		const auto inline_ = application::build_agent_launch(claude, supplement(), reader(), { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = rule });
		expect(inline_ && index_of(inline_->Arguments, "--append-system-prompt") + 1U < inline_->Arguments.size()
				   && inline_->Arguments[index_of(inline_->Arguments, "--append-system-prompt") + 1U] == rule,
			   "Claude takes the instructions as one inline --append-system-prompt argument");
		expect(inline_ && index_of(inline_->Arguments, "--append-system-prompt") < index_of(inline_->Arguments, "-p"), "the system prompt comes before -p");
		expect(inline_ && !has(inline_->Arguments, "--append-system-prompt-file"), "never the -file form, which the corporate wrapper drops");
		expect(inline_ && inline_->Input.find("PINEAPPLE") == std::string::npos && inline_->InstructionsRoute == application::agent_instructions_route::system_prompt,
			   "the message carries only the run's material");
		const auto pasted = application::build_agent_launch(claude, supplement(), reader(),
															{ .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = rule, .Instructions = core::agent_instruction_delivery::message });
		expect(pasted && !has(pasted->Arguments, "--append-system-prompt") && pasted->Input.find("PINEAPPLE") != std::string::npos && pasted->InstructionsRoute == application::agent_instructions_route::pasted,
			   "the agent's switch pastes the instructions into the message");
		const auto long_ = application::build_agent_launch(claude, supplement(), reader(), { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = std::string(core::maximum_system_prompt_argument_bytes, 'x') });
		expect(long_ && !has(long_->Arguments, "--append-system-prompt") && long_->InstructionsRoute == application::agent_instructions_route::pasted, "instructions past the Windows bound are pasted");
		const auto exec = application::build_agent_launch(profile(core::agent_protocol::codex_jsonl_exec), supplement(), reader(), { .ReadOnlyTools = true, .Path = core::agent_run_path::tour_field_draft, .SystemInstructions = rule });
		expect(exec && std::ranges::any_of(exec->Arguments, [](const std::string& aArgument) { return aArgument.starts_with("developer_instructions=\"Canary rule"); }) && exec->Input.find("PINEAPPLE") == std::string::npos,
			   "Codex exec takes the instructions as developer_instructions");
		const auto appServer = application::build_agent_launch(profile(core::agent_protocol::codex_app_server), supplement(), reader(), { .Path = core::agent_run_path::tour_side_chat, .SystemInstructions = rule });
		expect(appServer && appServer->InstructionsRoute == application::agent_instructions_route::pasted, "the App Server, unverified, pastes");
		const auto none = application::build_agent_launch(claude, supplement(), reader());
		expect(none && none->InstructionsRoute == application::agent_instructions_route::none && !has(none->Arguments, "--append-system-prompt"), "a launch without instructions carries none");
	}

	/// RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]), RFC-BPR-0101 AC13. A
	/// returning question's first answer: a fresh, tool-free Claude call with
	/// its own prompt in the system prompt and its material, both revisions
	/// and the change or the thread's lines then and now, in the request.
	void test_a_returning_question_is_one_fresh_tool_free_call()
	{
		const auto claude = profile(core::agent_protocol::claude_stream_json);
		const auto earlier = std::string(40U, 'a');
		const auto current = std::string(40U, 'b');
		for (const auto hook : { core::writing_hook::review_latest_upload, core::writing_hook::review_proposed_fix }) {
			const auto latest   = hook == core::writing_hook::review_latest_upload;
			auto       material = core::supplement_returning{ .Operation = latest ? core::returning_operation::latest_upload : core::returning_operation::proposed_fix, .EarlierHead = earlier, .CurrentHead = current };
			if (latest) {
				material.Changes    = "--- a/src/a.cpp\n+++ b/src/a.cpp\n@@ -1,1 +1,1 @@\n-old\n+new\n";
				material.FilesShown = 1U;
			} else {
				material.Then = core::returning_lines{ .Commit = earlier, .Path = "src/a.cpp", .FirstLine = 1U, .Lines = { "old" } };
				material.Now  = core::returning_lines{ .Commit = current, .Path = "src/a.cpp", .FirstLine = 1U, .Lines = { "new" } };
			}
			expect(core::valid(material), "the returning material is within its bounds");
			auto value      = supplement();
			value.Returning = material;
			const auto prompt = std::string{ application::default_writing_hook(hook) };
			expect(!prompt.empty() && application::writing_hook_purpose(hook) == (latest ? "review.latest-upload" : "review.proposed-fix")
					   && application::writing_hook_of_purpose(application::writing_hook_purpose(hook)) == hook && !application::writing_hook_version_of(hook).empty(),
				   "each returning question has its own prompt, purpose and version");
			const auto launch = application::build_agent_launch(claude, value, reader(), { .WritingOnly = true, .Path = core::agent_run_path::assistance_first_answer, .SystemInstructions = prompt });
			expect(launch && has(launch->Arguments, "--no-session-persistence") && !has(launch->Arguments, "--resume"), "a fresh run: no session saved or resumed");
			expect(launch && index_of(launch->Arguments, "--tools") + 1U < launch->Arguments.size() && launch->Arguments[index_of(launch->Arguments, "--tools") + 1U].empty(), "no tools");
			expect(launch && has(launch->Arguments, "--strict-mcp-config"), "no MCP server");
			expect(launch && launch->Arguments[index_of(launch->Arguments, "--append-system-prompt") + 1U] == prompt, "its own prompt is the system prompt");
			expect(launch && launch->Input.find("returning.earlier_head") != std::string::npos && launch->Input.find(earlier) != std::string::npos && launch->Input.find(current) != std::string::npos,
				   "the request names both revisions");
			expect(launch && (latest ? launch->Input.find("+new") != std::string::npos : launch->Input.find("returning.then.line.0") != std::string::npos && launch->Input.find("returning.now.line.0") != std::string::npos),
				   "and carries the change, or the lines then and now, up front");
		}
		expect(application::default_writing_hook(core::writing_hook::review_latest_upload) != application::default_writing_hook(core::writing_hook::review_proposed_fix), "two prompts, not one");
		expect(core::ask_more_path(core::agent_run_path::assistance_first_answer) == core::agent_run_path::agent_tab, "Ask more continues in the Agent tab's conversation");
	}

	/// [[BPR-2140]]. The writing prompt splits into the instructions and the
	/// material, and joined they are the prompt as before.
	void test_the_writing_prompt_splits_into_instructions_and_material()
	{
		auto settings         = core::writing_hook_settings{ };
		auto request          = core::writing_request{ };
		request.Target.Hook   = core::writing_hook::enhance_writing;
		request.Raw           = "The cache reduces repeated work.";
		request.Selection.End = request.Raw.size();
		const auto sources    = std::vector<application::writing_source>{ { "pr-description", "Adds a cache." } };
		const auto parts      = application::build_writing_prompt_parts(settings, request, sources);
		const auto joined     = application::build_writing_prompt(settings, request, sources);
		expect(parts && joined && parts->Instructions + parts->Message == *joined, "the two parts joined are the prompt");
		expect(parts && parts->Message.starts_with("Untrusted evidence JSON") && parts->Instructions.find("Untrusted evidence") == std::string::npos, "the evidence is the message, never the instructions");
		expect(parts && parts->Message.find(request.Raw) != std::string::npos, "the text to enhance is in the message");
	}

	/// [[BPR-2138]], RFC-BPR-0101 AC10, AC12. A local draft's discussion is
	/// a tool-free conversation that keeps its native session and resumes it
	/// by handle; its first description and its commit message are fresh.
	void test_a_local_draft_discussion_resumes_its_native_session()
	{
		const auto claude = profile(core::agent_protocol::claude_stream_json);
		const auto first  = application::build_agent_launch(claude, supplement(), reader(), { .WritingOnly = true, .Path = core::agent_run_path::local_draft_discussion });
		expect(first && !has(first->Arguments, "--no-session-persistence") && has(first->Arguments, "--disable-slash-commands") && has(first->Arguments, "--strict-mcp-config")
				   && !has(first->Arguments, "--resume"),
			   "the discussion's first turn is tool-free, saves its session and resumes none");
		auto next          = *first;
		next.NativeHandle  = "0f8e7c1a-2b3d-4e5f-8a9b-0c1d2e3f4a5b";
		const auto resumed = application::prepare_native_agent_turn(next, false);
		expect(resumed && has(resumed->Arguments, "--resume") && resumed->Arguments[index_of(resumed->Arguments, "--resume") + 1U] == next.NativeHandle
				   && has(resumed->Arguments, "--disable-slash-commands"),
			   "the next turn resumes that session, still tool-free");
		expect(!application::prepare_native_agent_turn(next, true), "a tool-free conversation has no connected session");
		for (const auto fresh : { core::agent_run_path::local_description_first_draft, core::agent_run_path::local_commit_message }) {
			const auto launch = application::build_agent_launch(claude, supplement(), reader(), { .WritingOnly = true, .Path = fresh });
			expect(launch && has(launch->Arguments, "--no-session-persistence"), "a first description and a commit message save no session");
			auto turn         = *launch;
			turn.NativeHandle = next.NativeHandle;
			expect(!application::prepare_native_agent_turn(turn, false), "and are never resumed");
		}
		expect(core::ask_more_path(core::agent_run_path::local_description_first_draft) == core::agent_run_path::local_draft_discussion,
			   "a first description's Ask more is the draft's discussion");
		expect(!core::ask_more_path(core::agent_run_path::local_commit_message), "a commit message offers no conversation");
		expect(core::ask_more_path(core::agent_run_path::tour_enhance) == core::agent_run_path::tour_side_chat, "a tour part's Enhance continues in that part's side chat");
	}

	/// [[BPR-2140]]. The review skill a review request carries is a built-in
	/// prompt: it rides the system prompt and leaves the brief.
	void test_the_review_skill_rides_the_system_prompt()
	{
		auto value        = supplement();
		value.Brief       = core::supplement_brief{ .Title = "Cache", .BaseBranch = "main", .Skill = core::brief_review_skill{ .Text = "SKILL-RULE: review the cache's eviction first." } };
		const auto claude = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), value, reader(), { .Path = core::agent_run_path::agent_tab });
		expect(claude && has(claude->Arguments, "--append-system-prompt") && claude->Arguments[index_of(claude->Arguments, "--append-system-prompt") + 1U].starts_with("SKILL-RULE")
				   && claude->Input.find("SKILL-RULE") == std::string::npos && claude->Input.find("brief.skill.location") != std::string::npos
				   && claude->InstructionsRoute == application::agent_instructions_route::system_prompt,
			   "the Agent tab's review skill is Claude's system prompt, and the brief says where it went");
		const auto exec = application::build_agent_launch(profile(core::agent_protocol::codex_jsonl_exec), value, reader(), { .Path = core::agent_run_path::agent_tab });
		expect(exec && exec->Input.find("SKILL-RULE") == std::string::npos
				   && std::ranges::any_of(exec->Arguments, [](const std::string& aArgument) { return aArgument.starts_with("developer_instructions=\"SKILL-RULE"); }),
			   "Codex exec takes it as developer_instructions");
		const auto pasted = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), value, reader(),
															{ .Path = core::agent_run_path::agent_tab, .Instructions = core::agent_instruction_delivery::message });
		expect(pasted && !has(pasted->Arguments, "--append-system-prompt") && pasted->Input.find("SKILL-RULE") != std::string::npos, "the agent's switch keeps it in the message");
		const auto appServer = application::build_agent_launch(profile(core::agent_protocol::codex_app_server), value, reader(), { .Path = core::agent_run_path::agent_tab });
		expect(appServer && appServer->DeferredContext.find("SKILL-RULE") != std::string::npos, "the App Server keeps it in the message");
	}

	/// [[BPR-2141]], RFC-BPR-0101 AC13. Draft Tour, a field's draft and an
	/// outline are one tool-free call with the diff up front, within a
	/// budget; past it, or with an agent that has no tool-free mode, they
	/// read the repository and say so. Their instructions split off for the
	/// system prompt ([[BPR-2140]]).
	void test_a_fresh_draft_carries_its_diff_up_front()
	{
		expect(core::choose_tour_context(0U, true) == core::tour_context_route::up_front && core::choose_tour_context(core::maximum_up_front_diff_bytes, true) == core::tour_context_route::up_front,
			   "a diff within the budget goes up front");
		expect(core::choose_tour_context(core::maximum_up_front_diff_bytes + 1U, true) == core::tour_context_route::reads_repository_diff_too_large, "one past it reads the repository, by name");
		expect(core::choose_tour_context(10U, false) == core::tour_context_route::reads_repository_no_tool_free_mode, "an agent without a tool-free mode reads the repository, by name");
		expect(core::stable_name(core::tour_context_route::reads_repository_diff_too_large) == "reads_repository_diff_too_large", "the route is named");

		const auto patch = std::string{ "@@ -1,2 +1,3 @@\n int cache_size()\n+{ return PATCH_MARKER; }\n" };
		const auto files = std::vector<application::tour_draft_file>{
			{ .Path = "src/cache.cpp", .Change = "modified", .Hunks = std::vector<std::pair<std::uint32_t, std::uint32_t>>{ { 1U, 3U } }, .Patch = patch },
			{ .Path = "art/logo.png", .Change = "added" },
		};
		expect(application::tour_diff_bytes(files) == patch.size(), "the diff's bytes are its patches'");
		const auto upFront = application::build_tour_draft_prompt("Cache", "Adds a cache.", files, core::review_tour{ }, { }, { }, { }, true);
		const auto reading = application::build_tour_draft_prompt("Cache", "Adds a cache.", files, core::review_tour{ });
		expect(upFront && upFront->find("PATCH_MARKER") != std::string::npos && upFront->find("You have no tools") != std::string::npos && upFront->find("read-only git") == std::string::npos,
			   "the up-front draft carries each patch and is told it has no tools");
		expect(reading && reading->find("PATCH_MARKER") == std::string::npos && reading->find("read-only git") != std::string::npos, "the repository-reading draft is told to read");
		const auto parts = application::split_tour_prompt(*upFront);
		expect(parts.Instructions + parts.Message == *upFront && parts.Message.starts_with("Untrusted evidence JSON") && parts.Instructions.find("PATCH_MARKER") == std::string::npos
				   && parts.Instructions.find("bpr.tour-draft.v1") != std::string::npos,
			   "[[BPR-2140]] the instructions split from the evidence, and joined they are the prompt");
		expect(parts.Instructions.size() <= core::maximum_system_prompt_argument_bytes, "Draft Tour's instructions fit the system prompt argument");
		const auto diff = application::render_up_front_diff(files);
		expect(diff.find("PATCH_MARKER") != std::string::npos && diff.find("art/logo.png") != std::string::npos && diff.find("No patch") != std::string::npos,
			   "a field draft's diff names each patch and the files without one");
		expect(application::up_front_writing_preamble().find("no tools") != std::string::npos, "and its preamble says it has no tools");
		// [[BPR-2140]]. The tour conversation's turn: its instructions, the
		// longest (the whole tour's, an update), fit the system prompt.
		const auto ask = application::build_tour_ask_prompt("Cache", "Adds a cache.", files, core::review_tour{ }, core::tour_conversation{ }, core::tour_thread{ }, std::nullopt, "Merge stops 1 and 2.", true, std::nullopt, "the cache", { }, { });
		expect(ask.has_value(), "a whole-tour update turn builds");
		const auto turn = application::split_tour_prompt(*ask);
		expect(turn.Instructions + turn.Message == *ask && turn.Message.find("Merge stops 1 and 2.") != std::string::npos && turn.Instructions.find("Merge stops 1 and 2.") == std::string::npos
				   && turn.Instructions.size() <= core::maximum_system_prompt_argument_bytes,
			   "the tour conversation's instructions split off, within the system prompt's bound; the question stays in the message");
		const auto launch = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement(), reader(),
															{ .WritingOnly = true, .Path = core::agent_run_path::draft_tour_first_draft, .SystemInstructions = parts.Instructions });
		expect(launch && has(launch->Arguments, "--tools") && launch->Arguments[index_of(launch->Arguments, "--tools") + 1U].empty() && has(launch->Arguments, "--strict-mcp-config")
				   && has(launch->Arguments, "--no-session-persistence") && has(launch->Arguments, "--append-system-prompt"),
			   "AC13 the fresh draft launches with no tool, no MCP server and no saved session, its instructions in the system prompt");
	}

	/// RFC-BPR-0103 AC10. A wrapper's failed sign-in ends the run as one.
	void test_a_failed_sign_in_ends_the_run_at_once()
	{
		const auto decode = [](const std::string& aFrames) -> std::optional<application::agent_protocol_failure> {
			auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json, false };
			if (auto pushed = decoder.push(aFrames); !pushed) {
				return pushed.error();
			}
			if (auto finished = decoder.finish(); !finished) {
				return finished.error();
			}
			return std::nullopt;
		};
		const auto init = std::string{ R"({"type":"system","subtype":"init","session_id":"s","tools":[],"mcp_servers":[]})" } + "\n";
		expect(decode(init + R"({"type":"system","subtype":"api_retry","attempt":1,"max_retries":10,"retry_delay_ms":600,"error_status":401,"error":"authentication_failed","session_id":"s"})" + "\n")
				   == application::agent_protocol_failure::not_signed_in,
			   "the first authentication_failed retry ends the run as a failed sign-in");
		expect(decode(init + R"({"type":"system","subtype":"api_retry","attempt":1,"max_retries":10,"retry_delay_ms":600,"error_status":529,"error":"overloaded","session_id":"s"})" + "\n")
				   != application::agent_protocol_failure::not_signed_in,
			   "a retry for another reason is not a sign-in failure");
		expect(decode("401 {\"error\":\"missing gateway token\"}\n") == application::agent_protocol_failure::not_signed_in, "a plain-text 401 is a failed sign-in");
		expect(decode(init + R"({"type":"result","subtype":"success","session_id":"s","is_error":true,"result":"API Error: 401 {\"error\":\"missing gateway token\"}"})" + "\n")
				   == application::agent_protocol_failure::not_signed_in,
			   "a result that is a 401 error is a failed sign-in");
		expect(decode(init + R"({"type":"result","subtype":"success","session_id":"s","is_error":true,"result":"Not logged in · Please run /login"})" + "\n") == application::agent_protocol_failure::not_signed_in,
			   "Claude's own Not logged in result is a failed sign-in");
		expect(decode("Warning: no stdin data received in 3s, proceeding without it\n" + init) != application::agent_protocol_failure::not_signed_in, "a wrapper's banner line is still read past");
		expect(application::agent_sign_in_failure_words.find("signed in") != std::string_view::npos, "the words say it is a sign-in problem");
	}
} // namespace

auto main(const int aCount, char** aArguments) -> int
{
	// [[BPR-2140]]. For the opt-in canary course: the exact Enhance launch
	// BPR builds with a rule only in the system prompt: the argument count,
	// each argument as its length and its bytes, then the framed input.
	if (aCount == 2 && std::string_view{ aArguments[1] } == "--print-canary-launch") {
		const auto launch = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement("Reply with: ok"), reader(),
															{ .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = "Canary rule: end every reply with the single word PINEAPPLE." });
		if (!launch) {
			return EXIT_FAILURE;
		}
		std::cout << launch->Arguments.size() << '\n';
		for (const auto& argument : launch->Arguments) {
			std::cout << argument.size() << '\n'
					  << argument;
		}
		std::cout << launch->Input;
		return EXIT_SUCCESS;
	}
	// [[BPR-2138]]. For the opt-in resume canary: a local draft discussion's
	// turn as BPR builds it, with the message and, when given, the session
	// to resume; printed as above.
	if ((aCount == 3 || aCount == 4) && std::string_view{ aArguments[1] } == "--print-discussion-launch") {
		auto launch = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement(aArguments[2]), reader(),
													  { .WritingOnly = true, .Path = core::agent_run_path::local_draft_discussion });
		if (!launch) {
			return EXIT_FAILURE;
		}
		launch->NativeHandle = aCount == 4 ? std::string{ aArguments[3] } : std::string{ };
		const auto turn      = application::prepare_native_agent_turn(*launch, false);
		if (!turn) {
			return EXIT_FAILURE;
		}
		std::cout << turn->Arguments.size() << '\n';
		for (const auto& argument : turn->Arguments) {
			std::cout << argument.size() << '\n'
					  << argument;
		}
		std::cout << turn->Input;
		return EXIT_SUCCESS;
	}
	test_every_launch_path_has_the_owners_session_mode();
	test_each_launch_path_launches_in_its_session_mode();
	test_mcp_servers_follow_the_agents_setting();
	test_options_with_several_values_come_before_the_prompt_flag();
	test_built_in_instructions_reach_the_system_prompt();
	test_the_writing_prompt_splits_into_instructions_and_material();
	test_a_local_draft_discussion_resumes_its_native_session();
	test_the_review_skill_rides_the_system_prompt();
	test_a_fresh_draft_carries_its_diff_up_front();
	test_a_failed_sign_in_ends_the_run_at_once();
	test_a_returning_question_is_one_fresh_tool_free_call();
	std::cout << "agent-launch-policy-tests:pass\n";
	return EXIT_SUCCESS;
}
