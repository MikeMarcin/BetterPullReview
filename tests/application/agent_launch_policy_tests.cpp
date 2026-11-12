/// \file
/// \brief The owner's agent launch policy of 2026-11-12: every launch path's
/// session mode ([[BPR-2138]], RFC-BPR-0101 AC10), MCP servers per agent
/// ([[BPR-2139]], RFC-BPR-0104 AC10), a built-in prompt's instructions in
/// the system prompt ([[BPR-2140]], RFC-BPR-0107 AC09), Claude's options
/// with several values before `-p` and a wrapper's failed sign-in
/// ([[BPR-2142]], RFC-BPR-0104 AC11, RFC-BPR-0103 AC10).

#include "bpr/application/agent_launch.hpp"
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
		const auto off = std::vector<std::string>{ "--strict-mcp-config", "--mcp-config", R"({"mcpServers":{}})" };
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
		const auto rule   = std::string{ "Canary rule: end every reply with the single word PINEAPPLE." };
		const auto claude = profile(core::agent_protocol::claude_stream_json);
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

	/// [[BPR-2140]]. The writing prompt splits into the instructions and the
	/// material, and joined they are the prompt as before.
	void test_the_writing_prompt_splits_into_instructions_and_material()
	{
		auto settings     = core::writing_hook_settings{ };
		auto request      = core::writing_request{ };
		request.Target.Hook = core::writing_hook::enhance_writing;
		request.Raw         = "The cache reduces repeated work.";
		request.Selection.End = request.Raw.size();
		const auto sources  = std::vector<application::writing_source>{ { "pr-description", "Adds a cache." } };
		const auto parts    = application::build_writing_prompt_parts(settings, request, sources);
		const auto joined   = application::build_writing_prompt(settings, request, sources);
		expect(parts && joined && parts->Instructions + parts->Message == *joined, "the two parts joined are the prompt");
		expect(parts && parts->Message.starts_with("Untrusted evidence JSON") && parts->Instructions.find("Untrusted evidence") == std::string::npos, "the evidence is the message, never the instructions");
		expect(parts && parts->Message.find(request.Raw) != std::string::npos, "the text to enhance is in the message");
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
	// BPR builds with a rule only in the system prompt, as NUL-separated
	// arguments, then the framed input after a blank separator.
	if (aCount == 2 && std::string_view{ aArguments[1] } == "--print-canary-launch") {
		const auto launch = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), supplement("Reply with: ok"), reader(),
															 { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = "Canary rule: end every reply with the single word PINEAPPLE." });
		if (!launch) {
			return EXIT_FAILURE;
		}
		for (const auto& argument : launch->Arguments) {
			std::cout << argument << '\0';
		}
		std::cout << '\0' << launch->Input;
		return EXIT_SUCCESS;
	}
	test_every_launch_path_has_the_owners_session_mode();
	test_each_launch_path_launches_in_its_session_mode();
	test_mcp_servers_follow_the_agents_setting();
	test_options_with_several_values_come_before_the_prompt_flag();
	test_built_in_instructions_reach_the_system_prompt();
	test_the_writing_prompt_splits_into_instructions_and_material();
	test_a_failed_sign_in_ends_the_run_at_once();
	std::cout << "agent-launch-policy-tests:pass\n";
	return EXIT_SUCCESS;
}
