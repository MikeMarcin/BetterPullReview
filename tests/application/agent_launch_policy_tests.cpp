/// \file
/// \brief The owner's agent launch policy of 2026-11-12: every launch path's
/// session mode ([[BPR-2138]], RFC-BPR-0101 AC10), MCP servers per agent
/// ([[BPR-2139]], RFC-BPR-0104 AC10), a built-in prompt's instructions in
/// the system prompt ([[BPR-2140]], RFC-BPR-0107 AC09), Claude's options
/// with several values each closed by another option and a wrapper's failed sign-in
/// ([[BPR-2142]], RFC-BPR-0104 AC11, RFC-BPR-0103 AC10).

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/tour_agent_conversation.hpp"
#include "bpr/application/tour_agent_drafting.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/agent_run_policy.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <iterator>
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
			{ path::follow_up, resume },
			{ path::agent_tab, resume },
			{ path::tour_side_chat, resume },
			{ path::tour_conversation, resume },
			{ path::local_draft_discussion, resume },
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
		expect(core::parse_agent_mcp_servers("own") == core::agent_mcp_servers::own && !core::parse_agent_mcp_servers("on"), "the setting's names read back");
	}

	/// RFC-BPR-0104 AC11 (amended 2026-11-19, [[BPR-2306]]). In every Claude
	/// launch each option that takes several values is followed by another
	/// option, and `-p` comes first, as the owner's harness audit recommends.
	void test_options_with_several_values_are_followed_by_another_option()
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
		for (const auto& launch : launches) {
			const auto& arguments = launch.Arguments;
			expect(!arguments.empty() && arguments.front() == "-p", "every Claude launch is in print mode, -p first");
			for (auto at = std::size_t{ 0U }; at < arguments.size(); ++at) {
				if (std::ranges::find(core::claude_variadic_options, arguments[at]) == core::claude_variadic_options.end()) {
					continue;
				}
				auto next = at + 2U;
				while (next < arguments.size() && !arguments[next].starts_with("-")) {
					++next;
				}
				expect(next < arguments.size(), "an option with several values is followed by another option: " + arguments[at]);
			}
			expect(!arguments.back().empty() && arguments.back() != "*", "a Claude launch never ends on an option's value that could take more");
		}
	}

	/// [[BPR-2304]]. Every Claude launch this app builds keeps the rules the
	/// owner's harness audit of 2026-11-19 established, as the core checks
	/// them: inline instructions only, several-valued options closed by
	/// another option, an `Edit` rule beside every scoped `Write` rule, only the
	/// user's settings where the checkout or the run calls for it, and no MCP
	/// server where MCP is off.
	void test_every_claude_launch_keeps_the_audited_rules()
	{
		const auto claude  = profile(core::agent_protocol::claude_stream_json);
		const auto scratch = application::agent_launch_grant{ .ScratchFolder = "/tmp/bpr-scratch/run-1" };
		const auto grant   = application::agent_launch_grant{ .ScratchFolder = "/tmp/bpr-scratch/run-1", .BuildWorkspace = "/tmp/bpr-checkout" };
		const auto checked = [](const std::expected<application::agent_launch_request, application::agent_launch_failure>& aLaunch, const core::claude_argument_expectation aExpected, const std::string_view aWhat) {
			expect(aLaunch.has_value(), std::string{ aWhat } + ": the launch builds");
			const auto problem = core::inspect_claude_arguments(aLaunch->Arguments, aExpected);
			expect(!problem, std::string{ aWhat } + ": " + std::string{ problem ? core::stable_name(*problem) : "" });
			return *aLaunch;
		};
		const auto user   = core::claude_argument_expectation{ .UserSettingsOnly = true, .McpOff = true };
		const auto rule   = std::string{ "Answer in one sentence." };
		const auto writes = checked(application::build_agent_launch(claude, supplement(), reader(), { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = rule }), user, "a writing run");
		expect(std::ranges::search(writes.Arguments, std::array<std::string_view, 2>{ "--append-system-prompt", rule }).begin() != writes.Arguments.end(), "the instructions ride inline");
		static_cast<void>(checked(application::build_agent_launch(claude, supplement(), reader(), { .ReadOnlyTools = true, .Path = core::agent_run_path::tour_field_draft, .SystemInstructions = rule }), user, "a read-only run"));
		static_cast<void>(checked(application::build_agent_launch(claude, supplement(), reader(), { .WritingOnly = true, .Path = core::agent_run_path::local_draft_discussion }), user, "a resumed writing run"));
		const auto granted = checked(application::build_agent_launch(claude, supplement(), reader(), { .Grant = grant, .PullRequestCheckout = { "/tmp/bpr-checkout" }, .SystemInstructions = rule }),
									 { .UserSettingsOnly = true }, "a granted Agent tab run in a pull request's checkout");
		expect(has(granted.Arguments, "Write(//tmp/bpr-scratch/run-1/**)") && has(granted.Arguments, "Edit(//tmp/bpr-scratch/run-1/**)"), "a scoped write is granted by its Edit rule, the Write rule beside it");
		static_cast<void>(checked(application::build_agent_launch(claude, supplement(), reader(), { .Grant = scratch, .Path = core::agent_run_path::assistance_first_answer }), { .McpOff = true }, "a fresh granted run"));
		static_cast<void>(checked(application::build_agent_launch(claude, supplement(), reader(), { .McpServers = core::agent_mcp_servers::own }), { }, "an Agent tab run with the agent's own MCP servers"));
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
		expect(inline_ && index_of(inline_->Arguments, "-p") < index_of(inline_->Arguments, "--append-system-prompt"), "-p comes first, the system prompt after it");
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

	/// [[BPR-2248]]. A Commits selection question carries exactly its
	/// commits: their IDs, headlines, messages, which merge the base, and
	/// their own changes, beside the question.
	void test_selected_commits_ride_beside_the_question()
	{
		const auto changes  = core::returning_changes{ .Text = "Commit aaaaaaa: Fix the parser\n--- a/src/a.cpp\n+++ b/src/a.cpp\n@@ -1,1 +1,1 @@\n-old\n+new\n", .FilesShown = 1U };
		const auto material = core::bound_selected_commits(
			{ { .Oid = std::string(40U, 'a'), .Headline = "Fix the parser", .Message = "The parser read past the end." }, { .Oid = std::string(40U, 'b'), .Headline = "Merge branch 'main' into feature", .MergesBase = true } },
			changes);
		expect(material.has_value(), "the selection is within its bounds");
		auto value        = supplement("Explain commits aaaaaaa, bbbbbbb");
		value.Commits     = material;
		const auto launch = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), value, reader(), { .Path = core::agent_run_path::agent_tab });
		expect(launch.has_value(), "the question launches");
		const auto& input = launch ? launch->Input : std::string{ };
		expect(input.find("commits.scope") != std::string::npos && input.find("commits.count") != std::string::npos, "it says the question is about exactly these commits");
		expect(input.find(std::string(40U, 'a')) != std::string::npos && input.find(std::string(40U, 'b')) != std::string::npos && input.find("The parser read past the end.") != std::string::npos,
			   "it names each commit and carries its message");
		expect(input.find("commits.1.merges_base") != std::string::npos && input.find("+new") != std::string::npos, "the merge of main is flagged and the real commit's change is carried");
	}

	/// RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]), RFC-BPR-0101 AC13. A
	/// returning question's first answer: a fresh, tool-free Claude call with
	/// its own prompt in the system prompt and its material, both revisions
	/// and the change or the thread's lines then and now, in the request.
	void test_a_returning_question_is_one_fresh_tool_free_call()
	{
		const auto claude  = profile(core::agent_protocol::claude_stream_json);
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
			auto value        = supplement();
			value.Returning   = material;
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
		// AC05. An edit reaches that operation's prompt and no other; Reset
		// (an empty override) is the shipped default again.
		auto edited                                                                          = core::writing_hook_settings{ };
		edited.Overrides[static_cast<std::size_t>(core::writing_hook::review_latest_upload)] = "TEAM_LATEST_UPLOAD";
		expect(application::effective_writing_hook(edited, core::writing_hook::review_latest_upload) == "TEAM_LATEST_UPLOAD", "the edit is the operation's prompt");
		for (auto index = std::size_t{ 0U }; index < core::writing_hook_count; ++index) {
			const auto hook = static_cast<core::writing_hook>(index);
			if (hook != core::writing_hook::review_latest_upload) {
				expect(application::effective_writing_hook(edited, hook) == application::default_writing_hook(hook), "and no other prompt changes");
			}
		}
		edited.Overrides[static_cast<std::size_t>(core::writing_hook::review_latest_upload)].clear();
		expect(application::effective_writing_hook(edited, core::writing_hook::review_latest_upload) == application::default_writing_hook(core::writing_hook::review_latest_upload), "Reset restores the built-in");
		expect(core::ask_more_path(core::agent_run_path::assistance_first_answer) == core::agent_run_path::agent_tab, "Ask more continues in the Agent tab's conversation");
	}

	/// RFC-BPR-0117 ([[BPR-2076]]), RFC-BPR-0101 AC13. Explain this file's
	/// first answer: a fresh, tool-free Claude call with its own prompt as the
	/// system prompt and the file's change and text in the request. An edit
	/// to its prompt reaches it and no other prompt.
	void test_explain_this_file_is_one_fresh_tool_free_call()
	{
		const auto claude   = profile(core::agent_protocol::claude_stream_json);
		const auto material = core::bound_explain_material(core::explain_material_input{
			.Path = "src/a.cpp", .BaseCommit = std::string(40U, 'a'), .HeadCommit = std::string(40U, 'b'), .Change = "--- a/src/a.cpp\n+++ b/src/a.cpp\n@@ -1,1 +1,1 @@\n-old\n+new\n", .Head = std::string{ "new\n" } });
		expect(material && core::valid(*material), "the material is within its bounds");
		auto value        = supplement();
		value.Explain     = material;
		const auto hook   = core::writing_hook::review_explain_file;
		const auto prompt = std::string{ application::default_writing_hook(hook) };
		expect(!prompt.empty() && application::writing_hook_purpose(hook) == "review.explain-file" && application::writing_hook_of_purpose("review.explain-file") == hook
				   && application::writing_hook_version_of(hook) == "review-explain-file-v3",
			   "Explain this file has its own prompt, purpose and version");
		const auto launch = application::build_agent_launch(claude, value, reader(), { .WritingOnly = true, .Path = core::agent_run_path::assistance_first_answer, .SystemInstructions = prompt });
		expect(launch && has(launch->Arguments, "--no-session-persistence") && !has(launch->Arguments, "--resume"), "a fresh run: no session saved or resumed");
		expect(launch && index_of(launch->Arguments, "--tools") + 1U < launch->Arguments.size() && launch->Arguments[index_of(launch->Arguments, "--tools") + 1U].empty(), "no tools");
		expect(launch && launch->Arguments[index_of(launch->Arguments, "--append-system-prompt") + 1U] == prompt, "its own prompt is the system prompt");
		expect(launch && launch->Input.find("explain.path") != std::string::npos && launch->Input.find("+new") != std::string::npos && launch->Input.find("explain.content.state") != std::string::npos,
			   "the request carries the file's change and its text up front");
		auto edited                                      = core::writing_hook_settings{ };
		edited.Overrides[static_cast<std::size_t>(hook)] = "TEAM_EXPLAIN";
		expect(application::effective_writing_hook(edited, hook) == "TEAM_EXPLAIN", "the edit is the next Explain this file's prompt");
		for (auto index = std::size_t{ 0U }; index < core::writing_hook_count; ++index) {
			if (static_cast<core::writing_hook>(index) != hook) {
				expect(application::effective_writing_hook(edited, static_cast<core::writing_hook>(index)) == application::default_writing_hook(static_cast<core::writing_hook>(index)), "and no other prompt changes");
			}
		}
		edited.Overrides[static_cast<std::size_t>(hook)].clear();
		expect(application::effective_writing_hook(edited, hook) == prompt, "Reset restores the built-in");
	}

	/// RFC-BPR-0118 ([[BPR-2078]]) AC05. Suggest the change is one fresh,
	/// tool-free call, briefed by its own prompt, with the draft's lines up
	/// front; an edit of its prompt reaches it and no other.
	void test_suggest_the_change_is_one_fresh_tool_free_call()
	{
		const auto claude   = profile(core::agent_protocol::claude_stream_json);
		const auto head     = core::split_text_lines("int f(int* p)\n{\n\treturn *p;\n}\n");
		const auto material = core::bound_suggest_change_material({ .Path = "src/a.cpp", .HeadCommit = std::string(40U, 'b'), .Comment = "Check for null first.", .Head = head, .First = 2U, .Last = 2U });
		expect(material && core::valid(*material), "the material is within its bounds");
		auto value          = supplement();
		value.Operation     = core::workshop_operation::suggest_change;
		value.SuggestChange = material;
		const auto hook     = core::writing_hook::review_suggest_change;
		const auto prompt   = std::string{ application::default_writing_hook(hook) };
		expect(!prompt.empty() && application::writing_hook_purpose(hook) == "review.suggest-change" && application::writing_hook_of_purpose("review.suggest-change") == hook
				   && application::writing_hook_version_of(hook) == "review-suggest-change-v1",
			   "Suggest the change has its own prompt, purpose and version");
		const auto launch = application::build_agent_launch(claude, value, reader(), { .WritingOnly = true, .Path = core::agent_run_path::assistance_first_answer, .SystemInstructions = prompt });
		expect(launch && has(launch->Arguments, "--no-session-persistence") && !has(launch->Arguments, "--resume"), "a fresh run: no session saved or resumed");
		expect(launch && index_of(launch->Arguments, "--tools") + 1U < launch->Arguments.size() && launch->Arguments[index_of(launch->Arguments, "--tools") + 1U].empty(), "no tools");
		expect(launch && launch->Arguments[index_of(launch->Arguments, "--append-system-prompt") + 1U] == prompt, "its own prompt is the system prompt");
		expect(launch && launch->Input.find("suggest.lines") != std::string::npos && launch->Input.find("return *p;") != std::string::npos && launch->Input.find("suggest.first_line") != std::string::npos
				   && launch->Input.find("Check for null first.") != std::string::npos && launch->Input.find("suggest_change") != std::string::npos,
			   "the request carries the operation, exactly the draft's lines and its words up front");
		auto edited                                      = core::writing_hook_settings{ };
		edited.Overrides[static_cast<std::size_t>(hook)] = "TEAM_SUGGEST";
		expect(application::effective_writing_hook(edited, hook) == "TEAM_SUGGEST", "the edit is the next Suggest the change's prompt");
		for (auto index = std::size_t{ 0U }; index < core::writing_hook_count; ++index) {
			if (static_cast<core::writing_hook>(index) != hook) {
				expect(application::effective_writing_hook(edited, static_cast<core::writing_hook>(index)) == application::default_writing_hook(static_cast<core::writing_hook>(index)), "and no other prompt changes");
			}
		}
		edited.Overrides[static_cast<std::size_t>(hook)].clear();
		expect(application::effective_writing_hook(edited, hook) == prompt, "Reset restores the built-in");
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

	/// [[BPR-2305]]. A Codex agent set to "At the start of the prompt" gets
	/// BetterPullReview's rules first on stdin and the request after them, and
	/// no `developer_instructions`; set to developer instructions it gets the
	/// option and not the pasted rules. Through a launcher that drops the
	/// option, the prompt form is the one that reaches the model.
	void test_codex_rules_lead_the_prompt_at_the_start_of_the_prompt()
	{
		const auto rule   = std::string{ "RULE-7: answer in one paragraph." };
		const auto codex  = profile(core::agent_protocol::codex_jsonl_exec);
		const auto pasted = application::build_agent_launch(codex, supplement("What does this change do?"), reader(),
															{ .Path = core::agent_run_path::agent_tab, .SystemInstructions = rule, .Instructions = core::agent_instruction_delivery::message });
		expect(pasted.has_value(), "the prompt form launches");
		// The fixed protocol line, then the rules, then the request.
		const auto lead = "BPR-CONTEXT/2 CODEX-EXEC\n" + rule + "\n\n";
		expect(pasted->Input.starts_with(lead) && pasted->Input.find("What does this change do?") > lead.size(), "the rules come first on stdin, then the request");
		expect(pasted->Input.find(rule, lead.size()) == std::string::npos, "the rules are sent once");
		expect(std::ranges::none_of(pasted->Arguments, [](const std::string& aArgument) { return aArgument.starts_with("developer_instructions="); }), "and no developer_instructions");
		expect(pasted->InstructionsRoute == application::agent_instructions_route::pasted, "the route says they were pasted");
		const auto again = application::build_agent_launch(codex, supplement("Another question entirely?"), reader(),
														   { .Path = core::agent_run_path::agent_tab, .SystemInstructions = rule, .Instructions = core::agent_instruction_delivery::message });
		expect(again && again->Input.starts_with(lead), "another question keeps the same prefix, so it can be read from cache");
		const auto option = application::build_agent_launch(codex, supplement("What does this change do?"), reader(), { .Path = core::agent_run_path::agent_tab, .SystemInstructions = rule });
		expect(option && option->Input.find(rule) == std::string::npos && has(option->Arguments, "developer_instructions=" + std::string{ "\"RULE-7: answer in one paragraph.\"" }),
			   "developer instructions are the default, with nothing pasted");
	}

	/// [[BPR-2305]]. The instructions probe a custom Codex's check runs.
	void test_the_codex_instructions_probe()
	{
		const auto codex   = profile(core::agent_protocol::codex_jsonl_exec);
		const auto leading = std::vector<std::string>{ "--no-extra-output", "--codex" };
		const auto option  = application::build_codex_instructions_probe(codex, "/opt/tools/launcher", leading, core::agent_instruction_delivery::system_prompt, reader());
		expect(option.Executable == "/opt/tools/launcher" && option.Arguments.size() > 3U && option.Arguments[0] == "--no-extra-output" && option.Arguments[1] == "--codex" && option.Arguments[2] == "exec"
				   && option.Arguments.back() == "-",
			   "the launcher's own arguments come first, then exec, the prompt on stdin");
		expect(has(option.Arguments, "--sandbox") && option.Arguments[index_of(option.Arguments, "--sandbox") + 1U] == "read-only" && has(option.Arguments, "--ephemeral") && has(option.Arguments, "mcp_servers={}"),
			   "the probe is read-only, keeps no session and loads no MCP server");
		expect(std::ranges::any_of(option.Arguments, [](const std::string& aArgument) { return aArgument.starts_with("developer_instructions=") && aArgument.find(application::codex_instructions_canary) != std::string::npos; })
				   && option.Input == application::codex_instructions_probe_question,
			   "as developer instructions: the rule is the option, the question the prompt");
		const auto prompt = application::build_codex_instructions_probe(codex, "/opt/tools/launcher", leading, core::agent_instruction_delivery::message, reader());
		expect(std::ranges::none_of(prompt.Arguments, [](const std::string& aArgument) { return aArgument.starts_with("developer_instructions="); })
				   && prompt.Input.starts_with(application::codex_instructions_probe_rule) && prompt.Input.ends_with(application::codex_instructions_probe_question),
			   "at the start of the prompt: the rule, then the question");
		auto answered    = application::agent_run_result{ };
		answered.Outcome = application::agent_run_outcome::success;
		answered.AssistantItems.emplace_back("I am ready.");
		expect(!application::codex_instructions_followed(answered), "an answer without the word did not follow the rule");
		answered.AssistantItems.back() += "\nMARIGOLD-4127";
		expect(application::codex_instructions_followed(answered), "an answer with it did");
		answered.Outcome = application::agent_run_outcome::protocol_failure;
		expect(!application::codex_instructions_followed(answered), "and a failed run never did");
		// [[BPR-2320]]. The probe names no working folder: it runs in an
		// empty folder of its own, whichever way the rule is given, or the
		// process launcher refuses it before it starts.
		expect(option.WritingOnly && prompt.WritingOnly && option.WorkingDirectory.empty() && application::names_agent_folder(option) && application::names_agent_folder(prompt),
			   "the probe runs in an empty folder of its own with both Instructions settings");
		expect(has(option.Arguments, "--skip-git-repo-check"), "and skips Codex's repository check, which an empty folder needs");
		auto nowhere        = option;
		nowhere.WritingOnly = false;
		expect(!application::names_agent_folder(nowhere), "a request with neither a folder nor WritingOnly names none");
		nowhere.WorkingDirectory = "/srv/review";
		expect(application::names_agent_folder(nowhere), "a named working folder is one");
	}

	/// [[BPR-2320]]. The stream JSON turn a custom Claude agent's check runs.
	void test_the_claude_stream_probe()
	{
		const auto claude  = profile(core::agent_protocol::claude_stream_json);
		const auto leading = std::vector<std::string>{ "--no-extra-output", "--skip-skills-upgrade" };
		const auto probe   = application::build_claude_stream_probe(claude, "/opt/tools/launcher", leading, reader(), application::agent_run_checks::strict);
		expect(probe.Executable == "/opt/tools/launcher" && probe.Arguments.size() > 3U && probe.Arguments[0] == "--no-extra-output" && probe.Arguments[1] == "--skip-skills-upgrade" && probe.Arguments[2] == "-p",
			   "the launcher's own arguments come first, then -p");
		expect(has(probe.Arguments, "--disallowedTools") && has(probe.Arguments, "--no-session-persistence") && has(probe.Arguments, "--strict-mcp-config")
				   && probe.Arguments[index_of(probe.Arguments, "--output-format") + 1U] == "stream-json" && has(probe.Arguments, "--verbose"),
			   "the turn is tool-free, keeps no session, loads no MCP server and answers in stream JSON");
		expect(!core::inspect_claude_arguments(probe.Arguments, { .UserSettingsOnly = true, .McpOff = true }), "and keeps every rule a restricted Claude launch keeps");
		expect(probe.WritingOnly && application::names_agent_folder(probe) && probe.Path == core::agent_run_path::agent_check, "it runs in an empty folder of its own");
		expect(probe.Input.starts_with(R"({"type":"user")") && probe.Input.find(application::claude_stream_probe_question) != std::string::npos && probe.Input.ends_with("}}\n"),
			   "the question is one stream JSON user message");
		auto answered    = application::agent_run_result{ };
		answered.Outcome = application::agent_run_outcome::success;
		expect(!application::claude_stream_answered(answered), "an answer without Claude's init frame is not the protocol");
		answered.Metadata.ProtocolReported.push_back({ .Basis = "claude_stream_json.system/init: session_id,model,claude_code_version" });
		expect(application::claude_stream_answered(answered), "one with it is");
		answered.Outcome = application::agent_run_outcome::protocol_failure;
		expect(!application::claude_stream_answered(answered), "and a failed run never is");
	}

	/// [[BPR-2264]]. The Writing style comes after every other instruction:
	/// last in the system prompt, or the request's last field where the
	/// instructions are pasted. Named in the question too, it is sent once,
	/// and an override is what every route sends.
	void test_the_writing_style_follows_every_other_instruction()
	{
		const auto count = [](const std::string& aText, const std::string_view aNeedle) {
			auto found = std::size_t{ 0U };
			for (auto at = aText.find(aNeedle); at != std::string::npos; at = aText.find(aNeedle, at + 1U)) {
				++found;
			}
			return found;
		};
		const auto sent = [&count](const application::agent_launch_request& aLaunch, const std::string_view aNeedle) {
			auto all = aLaunch.Input + aLaunch.DeferredContext;
			for (const auto& argument : aLaunch.Arguments) {
				all += argument;
			}
			return count(all, aNeedle);
		};
		const auto defaults = core::writing_hook_settings{ };
		const auto style    = application::writing_style_for(defaults, true);
		const auto marker   = std::string_view{ "Writing style (the /bpr:writing-style Feature)." };
		auto       value    = supplement("Explain the cache. /bpr:writing-style");
		value.Brief         = core::supplement_brief{ .Title = "Cache", .BaseBranch = "main", .Skill = core::brief_review_skill{ .Text = "SKILL-RULE: review the cache's eviction first." } };
		const auto claude   = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), value, reader(), { .Path = core::agent_run_path::agent_tab, .WritingStyle = style });
		expect(claude && has(claude->Arguments, "--append-system-prompt"), "a Claude Agent tab turn has a system prompt");
		const auto& system = claude->Arguments[index_of(claude->Arguments, "--append-system-prompt") + 1U];
		expect(system.starts_with("SKILL-RULE") && system.ends_with(style) && sent(*claude, marker) == 1U, "Claude: the skill, then the style, last and once, though the question names it");
		const auto writing = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement(), reader(),
															 { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = "Return JSON only.", .WritingStyle = style });
		expect(writing && writing->Arguments[index_of(writing->Arguments, "--append-system-prompt") + 1U] == "Return JSON only.\n\n" + style, "a JSON writing run: its format first, the style after it");
		const auto exec = application::build_agent_launch(profile(core::agent_protocol::codex_jsonl_exec), value, reader(), { .Path = core::agent_run_path::agent_tab, .WritingStyle = style });
		expect(exec && std::ranges::any_of(exec->Arguments, [](const std::string& aArgument) { return aArgument.starts_with("developer_instructions=") && aArgument.find("Writing style (the") != std::string::npos; })
				   && exec->Input.find("writing_style") == std::string::npos,
			   "Codex exec: in developer_instructions");
		const auto appServer = application::build_agent_launch(profile(core::agent_protocol::codex_app_server), value, reader(), { .Path = core::agent_run_path::agent_tab, .WritingStyle = style });
		expect(appServer && appServer->InstructionsRoute == application::agent_instructions_route::pasted && appServer->DeferredContext.find("writing_style\t") != std::string::npos
				   && sent(*appServer, marker) == 1U,
			   "the App Server: the request's writing_style field, once");
		auto hooks                                                                   = core::writing_hook_settings{ };
		hooks.Overrides[static_cast<std::size_t>(core::writing_hook::writing_style)] = "STYLE-OVERRIDE: follow my house-style skill.";
		const auto overridden                                                        = application::writing_style_for(hooks, true);
		for (const auto protocol : { core::agent_protocol::claude_stream_json, core::agent_protocol::codex_jsonl_exec, core::agent_protocol::codex_app_server }) {
			const auto launch = application::build_agent_launch(profile(protocol), value, reader(), { .Path = core::agent_run_path::agent_tab, .WritingStyle = overridden });
			expect(launch && sent(*launch, "STYLE-OVERRIDE") == 1U && sent(*launch, "Put the point first") == 0U, "an override is what every route sends, in place of the shipped text");
		}
		hooks.Overrides[static_cast<std::size_t>(core::writing_hook::writing_style)] = std::string(core::maximum_writing_instruction_bytes, 'x');
		const auto longest                                                           = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), value, reader(),
																													   { .Path = core::agent_run_path::agent_tab, .WritingStyle = application::writing_style_for(hooks, true) });
		expect(longest.has_value(), "the longest override a profile keeps still launches");
		const auto none = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement(), reader());
		expect(none && sent(*none, marker) == 0U, "a launch given no style carries none");
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
				   && parts.Instructions.find("bpr.tour-draft.v2") != std::string::npos,
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

	/// [[BPR-2242]]. A stand-in for Claude run through a wrapper whose sign-in
	/// is an `apiKeyHelper` in the user's own settings, as Claude Code 2.1.319
	/// reads it: the user's settings load unless `--setting-sources` names
	/// sources without `user`. Signed in, it answers; otherwise it writes what
	/// the owner's Draft Tour got on 2026-11-16: an init frame with no key
	/// source, then the first 401 retry.
	[[nodiscard]] auto through_a_wrapper_signed_in_by_user_settings(const std::vector<std::string>& aArguments) -> std::string
	{
		auto userSettings = true;
		if (const auto named = std::ranges::find(aArguments, "--setting-sources"); named != aArguments.end()) {
			const auto value = std::next(named) == aArguments.end() ? std::string{ } : "," + *std::next(named) + ",";
			userSettings     = value.find(",user,") != std::string::npos;
		}
		const auto init = std::string{ R"({"type":"system","subtype":"init","session_id":"s","tools":[],"mcp_servers":[],"apiKeySource":")" } + (userSettings ? "apiKeyHelper" : "none") + "\"}\n";
		if (!userSettings) {
			return init + R"({"type":"system","subtype":"api_retry","attempt":1,"max_retries":10,"retry_delay_ms":547,"error_status":401,"error":"authentication_failed","session_id":"s"})" + "\n";
		}
		return init + R"({"type":"assistant","session_id":"s","message":{"id":"m","role":"assistant","content":[{"type":"text","text":"answered"}]}})" + "\n"
			+ R"({"type":"result","subtype":"success","session_id":"s","is_error":false,"result":"answered"})" + "\n";
	}

	/// [[BPR-2242]] (owner, 2026-11-16). Ask in the Agent tab answered
	/// through the owner's wrapper and Draft Tour failed to sign in: every
	/// harness run of the selected agent reads the user's settings, where
	/// the person's sign-in can live, as the Agent tab does.
	void test_every_harness_run_reads_the_users_sign_in()
	{
		const auto checkout = std::vector<std::string>{ "/review/checkout" };
		struct run
		{
			std::string_view                  Name;
			application::agent_launch_options Options;
		};
		const auto runs = std::vector<run>{
			{ "Ask in the Agent tab, in the pull request's checkout", { .PullRequestCheckout = checkout, .Path = core::agent_run_path::agent_tab } },
			{ "Ask in the Agent tab, in the reviewer's own checkout", { .Path = core::agent_run_path::agent_tab } },
			{ "an agent-assistance first answer", { .WritingOnly = true, .Path = core::agent_run_path::assistance_first_answer, .SystemInstructions = "Explain." } },
			{ "Draft Tour with the diff up front", { .WritingOnly = true, .Path = core::agent_run_path::draft_tour_first_draft, .SystemInstructions = "Draft a tour." } },
			{ "Draft Tour reading the repository", { .ReadOnlyTools = true, .PullRequestCheckout = checkout, .Path = core::agent_run_path::draft_tour_first_draft } },
			{ "a field draft", { .WritingOnly = true, .Path = core::agent_run_path::tour_field_draft } },
			{ "Enhance on a stop", { .WritingOnly = true, .Path = core::agent_run_path::tour_enhance } },
			{ "a tour side chat", { .ReadOnlyTools = true, .PullRequestCheckout = checkout, .Path = core::agent_run_path::tour_side_chat } },
			{ "the whole-tour conversation", { .ReadOnlyTools = true, .PullRequestCheckout = checkout, .Path = core::agent_run_path::tour_conversation } },
			{ "Enhance on a comment", { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing } },
			{ "a local draft's first description", { .WritingOnly = true, .Path = core::agent_run_path::local_description_first_draft } },
			{ "a local draft's discussion", { .WritingOnly = true, .Path = core::agent_run_path::local_draft_discussion } },
			{ "a local draft's check plan", { .ReadOnlyTools = true, .PullRequestCheckout = checkout, .Path = core::agent_run_path::local_draft_discussion } },
		};
		for (const auto& each : runs) {
			const auto launch = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement(), reader(), each.Options);
			expect(launch.has_value(), std::string{ each.Name } + " builds");
			auto       decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json, launch->WritingOnly, application::agent_launch_purpose::conversation, launch->ReadOnlyTools };
			const auto pushed  = decoder.push(through_a_wrapper_signed_in_by_user_settings(launch->Arguments));
			expect(pushed.has_value() && decoder.finish().has_value(), std::string{ each.Name } + " signs in through the wrapper and answers");
			const auto named = std::ranges::find(launch->Arguments, "--setting-sources");
			expect(named == launch->Arguments.end() || (std::next(named) != launch->Arguments.end() && *std::next(named) == "user"),
				   std::string{ each.Name } + " names no settings source but the user's");
		}
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
	test_options_with_several_values_are_followed_by_another_option();
	test_every_claude_launch_keeps_the_audited_rules();
	test_built_in_instructions_reach_the_system_prompt();
	test_the_writing_prompt_splits_into_instructions_and_material();
	test_a_local_draft_discussion_resumes_its_native_session();
	test_the_review_skill_rides_the_system_prompt();
	test_the_writing_style_follows_every_other_instruction();
	test_codex_rules_lead_the_prompt_at_the_start_of_the_prompt();
	test_the_codex_instructions_probe();
	test_the_claude_stream_probe();
	test_a_fresh_draft_carries_its_diff_up_front();
	test_a_failed_sign_in_ends_the_run_at_once();
	test_every_harness_run_reads_the_users_sign_in();
	test_a_returning_question_is_one_fresh_tool_free_call();
	test_selected_commits_ride_beside_the_question();
	test_explain_this_file_is_one_fresh_tool_free_call();
	test_suggest_the_change_is_one_fresh_tool_free_call();
	std::cout << "agent-launch-policy-tests:pass\n";
	return EXIT_SUCCESS;
}
