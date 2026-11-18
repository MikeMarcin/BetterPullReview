/// \file
/// \brief [[BPR-1748]], RFC-BPR-0092 Q5. The one place a Codex or
/// plain-text harness launch is given an absolute program: saved path first,
/// then the found `codex`, the harness never searched for, without any
/// process or file.

#include "bpr/application/agent_program_resolution.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using core::agent_wrapper_refusal;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class recording_port final : public application::agent_process_port
	{
	public:
		[[nodiscard]] auto run(const application::agent_launch_request& aRequest, int, const application::agent_output_sink& = { }, std::stop_token = { })
			-> application::agent_run_result override
		{
			Requests.push_back(aRequest);
			return { .Session = aRequest.Session, .Outcome = application::agent_run_outcome::success, .Run = aRequest.Run, .Metadata = aRequest.Metadata };
		}
		void cancel() noexcept override { ++Cancels; }

		std::vector<application::agent_launch_request> Requests;
		int                                            Cancels = 0;
	};

	[[nodiscard]] auto request(const core::agent_protocol aProtocol, std::string aExecutable) -> application::agent_launch_request
	{
		auto value                = application::agent_launch_request{ };
		value.Protocol            = aProtocol;
		value.Session             = core::agent_session_id{ 11U };
		value.Run                 = core::agent_run_id{ 73U };
		value.Executable          = std::move(aExecutable);
		value.Arguments           = { "exec", "--json", "-" };
		value.WorkingDirectory    = "/review/workspace";
		value.Environment         = { { "HOME", "/home/me" }, { "PATH", "/usr/bin" } };
		value.Input               = "{\"type\":\"user\"}\n";
		value.Metadata.Requested  = { "fixture-model", "high" };
		value.Metadata.Configured = { "fixture-model", "high" };
		return value;
	}

	struct inspector_log
	{
		std::vector<std::string>             Asked;
		std::optional<agent_wrapper_refusal> Answer;
	};

	struct locator_log
	{
		std::optional<core::agent_search_candidate> Answer;
		std::vector<core::agent_program>            Asked;
	};

	struct fixture
	{
		recording_port                                      Inner;
		inspector_log                                       Inspected;
		locator_log                                         Located;
		std::shared_ptr<application::agent_program_source>  Source = std::make_shared<application::agent_program_source>();
		std::unique_ptr<application::located_agent_process> Port;

		fixture()
		{
			Port = std::make_unique<application::located_agent_process>(
				Inner, Source,
				[this](const std::string_view aPath) {
					Inspected.Asked.emplace_back(aPath);
					return Inspected.Answer;
				},
				[this](const core::agent_program aProgram) {
					Located.Asked.push_back(aProgram);
					return Located.Answer;
				});
		}
	};

	void test_codex_is_found()
	{
		auto f = fixture{ };
		for (const auto protocol : { core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec }) {
			f.Located.Answer = core::agent_search_candidate{ .Path = "/Users/me/.local/bin/codex", .Origin = core::agent_found_in::usual_folder };
			const auto bare  = request(protocol, "codex");
			static_cast<void>(f.Port->run(bare, 1));
			auto expected       = bare;
			expected.Executable = "/Users/me/.local/bin/codex";
			expect(f.Inner.Requests.back() == expected, "the shipped bare codex is replaced by the found absolute program, and nothing else changes");
		}
		expect(f.Located.Asked == std::vector{ core::agent_program::codex, core::agent_program::codex }, "the locator is asked for codex at every launch");
		expect(f.Inspected.Asked.empty(), "a found program is not inspected again here: the locator only returns runnable files");

		f.Located.Answer   = std::nullopt;
		const auto before  = f.Inner.Requests.size();
		const auto missing = f.Port->run(request(core::agent_protocol::codex_app_server, "codex"), 1);
		expect(f.Inner.Requests.size() == before, "with no codex found nothing starts");
		expect(missing.Outcome == application::agent_run_outcome::failed_to_start && missing.Session == core::agent_session_id{ 11U },
			   "no codex found ends the request as failed_to_start");
		expect(missing.StandardError == application::codex_not_found_message, "and says where it looked and what to do");
	}

	void test_saved_programs()
	{
		auto f           = fixture{ };
		f.Located.Answer = core::agent_search_candidate{ .Path = "/usr/local/bin/codex" };
		f.Source->set(core::setup_agent::codex, "/opt/tools/codex-wrapper");
		static_cast<void>(f.Port->run(request(core::agent_protocol::codex_jsonl_exec, "codex"), 1));
		expect(f.Inner.Requests.back().Executable == "/opt/tools/codex-wrapper", "a saved Codex program wins over the search");
		expect(f.Located.Asked.empty(), "with a saved program nothing is searched");
		expect(f.Inspected.Asked == std::vector<std::string>{ "/opt/tools/codex-wrapper" }, "the saved program is inspected again at launch");

		f.Inspected.Answer = agent_wrapper_refusal::not_executable;
		const auto before  = f.Inner.Requests.size();
		const auto refused = f.Port->run(request(core::agent_protocol::codex_jsonl_exec, "codex"), 1);
		expect(f.Inner.Requests.size() == before && f.Located.Asked.empty(), "a refused saved program starts nothing and never falls back to the search");
		expect(refused.Outcome == application::agent_run_outcome::failed_to_start, "a refused program ends the request as failed_to_start");
		expect(refused.StandardError.starts_with("Codex program /opt/tools/codex-wrapper: "), "the failure names the program");
		f.Inspected.Answer = std::nullopt;

		const auto unset = f.Port->run(request(core::agent_protocol::external_terminal, "bpr-agent-terminal"), 1);
		expect(unset.Outcome == application::agent_run_outcome::failed_to_start && unset.StandardError == application::plain_text_harness_unset_message,
			   "the plain-text harness with no program chosen starts nothing and says how to choose one");
		expect(f.Located.Asked.empty(), "the plain-text harness is never searched for");

		f.Source->set(core::setup_agent::plain_text_harness, "/opt/corp/harness");
		static_cast<void>(f.Port->run(request(core::agent_protocol::external_terminal, "bpr-agent-terminal"), 1));
		expect(f.Inner.Requests.back().Executable == "/opt/corp/harness", "the chosen harness program runs in place of the shipped name");
		expect(f.Source->current(core::setup_agent::codex) == "/opt/tools/codex-wrapper", "each agent keeps its own program");
	}

	void test_refused_program_keeps_the_current_run()
	{
		auto       f       = fixture{ };
		const auto given   = request(core::agent_protocol::codex_jsonl_exec, "codex");
		const auto refused = f.Port->run(given, 1);
		expect(f.Inner.Requests.empty(), "a program that was not found starts nothing");
		expect(refused.Session == given.Session && refused.Run == given.Run,
			   "a refused program keeps the nonzero run it is answering");
		expect(refused.Metadata == given.Metadata, "a launch refusal keeps the admitted run's requested and configured facts");
		auto admitted        = core::agent_session{ };
		admitted.Id          = given.Session;
		const auto finalized = application::finalize_agent_run(refused, admitted, admitted.Binding, given.Run);
		expect(finalized.Outcome == application::agent_run_outcome::failed_to_start && finalized.StandardError == application::codex_not_found_message && finalized.Metadata == given.Metadata,
			   "the current run's finalization preserves its launch refusal and guidance");
		const auto wrongRun = application::finalize_agent_run(refused, admitted, admitted.Binding, core::agent_run_id{ given.Run.value() + 1U });
		expect(wrongRun.Outcome == application::agent_run_outcome::stale_result && wrongRun.Metadata == core::agent_runtime_metadata{ }, "a refusal for another run still fails the run identity check and attributes no facts");
	}

	void test_pass_through()
	{
		auto f = fixture{ };
		f.Source->set(core::setup_agent::codex, "/opt/tools/codex-wrapper");
		const auto claude = request(core::agent_protocol::claude_stream_json, "claude");
		static_cast<void>(f.Port->run(claude, 1));
		expect(f.Inner.Requests.back() == claude, "a Claude request passes through: its wrapper is wrapped_claude_process's");
		for (const auto protocol : { core::agent_protocol::codex_app_server, core::agent_protocol::external_terminal }) {
			const auto named = request(protocol, "/fixture/agent");
			static_cast<void>(f.Port->run(named, 1));
			expect(f.Inner.Requests.back() == named, "a request that already names its program by path passes through unchanged");
		}
		expect(f.Inspected.Asked.empty() && f.Located.Asked.empty(), "passing through inspects and searches nothing");
		f.Port->cancel();
		expect(f.Inner.Cancels == 1, "cancel reaches the launcher");
	}

	/// [[BPR-1753]], owner amendment 2026-10-23. An added agent runs
	/// its own program with its fixed arguments first; two entries may share
	/// one wrapper; a gone entry or a refused program starts nothing.
	void test_added_agents_run_as_their_entries()
	{
		recording_port direct;
		recording_port others;
		auto           source   = std::make_shared<application::agent_entry_source>();
		auto           asked    = std::vector<std::string>{ };
		auto           refusal  = std::optional<agent_wrapper_refusal>{ };
		auto           claudeAt = std::optional<core::claude_search_candidate>{ core::claude_search_candidate{ .Path = "/usr/local/bin/claude" } };
		auto           port     = application::agent_entry_process{ direct, others, source,
																	[&asked, &refusal](const std::string_view aPath) {
														  asked.emplace_back(aPath);
														  return refusal;
													  },
													  [&claudeAt] { return claudeAt; }, [](core::agent_program) { return std::optional<core::agent_search_candidate>{ }; } };
		auto list = *core::add_agent_entry({ }, { .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/wrapper" });
		list      = *core::add_agent_entry(list, { .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } });
		list      = *core::add_agent_entry(list, core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { }));
		source->set(list);

		auto codex    = request(core::agent_protocol::codex_jsonl_exec, "codex");
		codex.Profile = core::entry_profile_id(list[1]);
		static_cast<void>(port.run(codex, 1));
		auto expected       = codex;
		expected.Executable = "/opt/work/bin/wrapper";
		expected.Arguments  = { "--codex", "exec", "--json", "-" };
		expect(direct.Requests.size() == 1U && direct.Requests.back() == expected && others.Requests.empty(),
			   "Work Codex runs the wrapper with --codex before Codex's own arguments, straight to the launcher");
		auto claude    = request(core::agent_protocol::claude_stream_json, "claude");
		claude.Profile = core::entry_profile_id(list[0]);
		static_cast<void>(port.run(claude, 1));
		expect(direct.Requests.back().Executable == "/opt/work/bin/wrapper" && direct.Requests.back().Arguments == claude.Arguments, "Work Claude runs the same wrapper with nothing added");
		expect(asked == std::vector<std::string>{ "/opt/work/bin/wrapper", "/opt/work/bin/wrapper" }, "the program is inspected again at every launch");
		auto preset    = request(core::agent_protocol::claude_stream_json, "claude");
		preset.Profile = core::entry_profile_id(list[2]);
		static_cast<void>(port.run(preset, 1));
		expect(direct.Requests.back().Executable == "/usr/local/bin/claude", "Claude Code with no program of its own runs the claude that is found");

		refusal            = agent_wrapper_refusal::not_executable;
		const auto before  = direct.Requests.size();
		const auto refused = port.run(codex, 1);
		expect(direct.Requests.size() == before && refused.Outcome == application::agent_run_outcome::failed_to_start && refused.StandardError.starts_with("Work Codex program /opt/work/bin/wrapper: "),
			   "a program that cannot run starts nothing and says which agent's it is");
		expect(refused.Session == codex.Session && refused.Run == codex.Run, "an added agent's refused program keeps its session and run");
		refusal = std::nullopt;
		source->set({ list[0] });
		const auto gone = port.run(codex, 1);
		expect(direct.Requests.size() == before && gone.StandardError == application::agent_entry_removed_message, "a removed agent starts nothing");
		expect(gone.Session == codex.Session && gone.Run == codex.Run, "a removed agent's refusal keeps its session and run");

		const auto builtIn = request(core::agent_protocol::claude_stream_json, "claude");
		static_cast<void>(port.run(builtIn, 1));
		expect(others.Requests.size() == 1U && others.Requests.back() == builtIn, "a built-in profile's launch goes to its own resolution unchanged");
	}

	/// \brief [[BPR-2259]] (owner, 2026-11-18: "Enhance hit this failure even
	/// though my custom agent just drafted the comment it's trying to
	/// enhance"). Enhance on a comment launches the custom agent chosen in the
	/// picker, its own program and arguments, through nothing that looks for
	/// the stock `claude`. Its own `--settings`, where its sign-in can ride,
	/// is the one Claude keeps: this app's writing settings do not replace it.
	void test_enhance_runs_the_custom_agent_on_its_own_sign_in()
	{
		recording_port direct;
		recording_port others;
		auto           source       = std::make_shared<application::agent_entry_source>();
		auto           claudeLooked = 0;
		auto           port         = application::agent_entry_process{ direct, others, source, [](const std::string_view) { return std::optional<agent_wrapper_refusal>{ }; },
																		[&claudeLooked] {
														  ++claudeLooked;
														  return std::optional{ core::claude_search_candidate{ .Path = "/usr/local/bin/claude" } };
													  },
													  [](core::agent_program) { return std::optional<core::agent_search_candidate>{ }; } };
		auto           list         = *core::add_agent_entry({ }, { .Name = "Fixture custom agent", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/fixture-agent", .Arguments = { "--settings", "/opt/work/sign-in.json" } });
		list                        = *core::add_agent_entry(list, { .Name = "Fixture wrapper", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/fixture-wrapper" });
		list                        = *core::add_agent_entry(list, core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { }));
		source->set(list);
		const auto enhance = [](const core::agent_profile_id aProfile) {
			auto supplement    = core::agent_context_supplement{ };
			supplement.Session = core::agent_session{ .Id = core::agent_session_id{ 1U }, .Profile = aProfile };
			supplement.Message = "nit: this could maybe be simpler";
			const auto launch  = application::build_agent_launch({ .Id = aProfile, .DisplayName = "Picked", .Executable = "claude", .Protocol = core::agent_protocol::claude_stream_json }, supplement,
																 [](const std::string&) -> std::optional<std::string> { return std::nullopt; },
																 { .WritingOnly = true, .Path = core::agent_run_path::enhance_writing, .SystemInstructions = "Enhance the comment." });
			expect(launch.has_value(), "the Enhance launch builds");
			return *launch;
		};
		const auto lastSettings = [](const std::vector<std::string>& aArguments) {
			auto value = std::string{ };
			for (auto at = std::ranges::find(aArguments, "--settings"); at != aArguments.end() && std::next(at) != aArguments.end(); at = std::ranges::find(std::next(at), aArguments.end(), "--settings")) {
				value = *std::next(at);
			}
			return value;
		};

		// The custom agent with its sign-in in its own arguments.
		const auto custom = enhance(core::entry_profile_id(list[0]));
		expect(lastSettings(custom.Arguments) == application::claude_writing_settings, "before the agent's process: the launch carries this app's writing settings");
		static_cast<void>(port.run(custom, 1));
		expect(direct.Requests.size() == 1U && others.Requests.empty() && claudeLooked == 0, "Enhance runs the custom agent straight; the stock claude is never looked for or run");
		const auto& ran = direct.Requests.back();
		expect(ran.Executable == "/opt/work/bin/fixture-agent" && ran.Arguments.size() >= 2U && ran.Arguments[0] == "--settings" && ran.Arguments[1] == "/opt/work/sign-in.json",
			   "the custom agent's own program, its arguments first");
		expect(lastSettings(ran.Arguments) == "/opt/work/sign-in.json" && std::ranges::count(ran.Arguments, std::string{ "--settings" }) == 1,
			   "Claude keeps the agent's own settings, where its sign-in is; this app's do not replace them");
		expect(std::ranges::search(ran.Arguments, std::array<std::string_view, 2>{ "--tools", "" }).begin() != ran.Arguments.end()
				   && std::ranges::search(ran.Arguments, std::array<std::string_view, 2>{ "--setting-sources", "user" }).begin() != ran.Arguments.end()
				   && std::ranges::find(ran.Environment, std::pair<std::string, std::string>{ application::claude_writing_environment }) != ran.Environment.end(),
			   "the run stays tool-free on the user's settings, with auto-memory off");
		// A stand-in that signs in only through the settings Claude kept.
		const auto signsIn = lastSettings(ran.Arguments) == "/opt/work/sign-in.json";
		auto       decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json, true, application::agent_launch_purpose::conversation, false };
		const auto output  = std::string{ R"({"type":"system","subtype":"init","session_id":"s","tools":[],"mcp_servers":[]})" } + "\n"
			+ (signsIn ? std::string{ R"({"type":"assistant","session_id":"s","message":{"id":"m","role":"assistant","content":[{"type":"text","text":"answered"}]}})" } + "\n"
					   + R"({"type":"result","subtype":"success","session_id":"s","is_error":false,"result":"answered"})" + "\n"
					   : std::string{ R"({"type":"system","subtype":"api_retry","attempt":1,"error_status":401,"error":"authentication_failed","session_id":"s"})" } + "\n");
		const auto pushed = decoder.push(output);
		expect(pushed.has_value() && decoder.finish().has_value(), "the custom agent signs in and answers Enhance");

		// A wrapper that adds its own settings before what it passes on.
		static_cast<void>(port.run(enhance(core::entry_profile_id(list[1])), 1));
		expect(direct.Requests.back().Executable == "/opt/work/bin/fixture-wrapper" && std::ranges::count(direct.Requests.back().Arguments, std::string{ "--settings" }) == 0 && claudeLooked == 0,
			   "a custom wrapper is given no settings to replace its own with");

		// Claude Code added as itself keeps this app's writing settings.
		static_cast<void>(port.run(enhance(core::entry_profile_id(list[2])), 1));
		expect(direct.Requests.back().Executable == "/usr/local/bin/claude" && lastSettings(direct.Requests.back().Arguments) == application::claude_writing_settings && claudeLooked == 1,
			   "the stock Claude Code keeps this app's writing settings");
		expect(others.Requests.empty(), "no launch reached the built-in profiles' resolution");
	}
}

auto main() -> int
{
	test_codex_is_found();
	test_saved_programs();
	test_refused_program_keeps_the_current_run();
	test_pass_through();
	test_added_agents_run_as_their_entries();
	test_enhance_runs_the_custom_agent_on_its_own_sign_in();
	std::cout << "agent-program-resolution-tests:pass\n";
	return EXIT_SUCCESS;
}
