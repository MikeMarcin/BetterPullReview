/// \file
/// \brief [[BPR-1301]]. The host half of the Claude process wrapper: the one
/// substitution point, its launch-time check, and saving only what the
/// adapter accepted, without any process or file.

#include "bpr/application/claude_process_wrapper.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/workbench_preferences_host.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
			return { .Session = aRequest.Session, .Outcome = application::agent_run_outcome::success };
		}
		void cancel() noexcept override { ++Cancels; }

		std::vector<application::agent_launch_request> Requests;
		int                                            Cancels = 0;
	};

	[[nodiscard]] auto request(const core::agent_protocol aProtocol, std::string aExecutable = "claude") -> application::agent_launch_request
	{
		auto value             = application::agent_launch_request{ };
		value.Protocol         = aProtocol;
		value.Session          = core::agent_session_id{ 7U };
		value.Executable       = std::move(aExecutable);
		value.Arguments        = { "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose" };
		value.WorkingDirectory = "/review/workspace";
		value.Environment      = { { "HOME", "/home/me" }, { "PATH", "/usr/bin" } };
		value.Input            = "{\"type\":\"user\"}\n";
		value.WritingOnly      = aProtocol == core::agent_protocol::claude_stream_json;
		return value;
	}

	struct inspector_log
	{
		std::vector<std::string>             Asked;
		std::optional<agent_wrapper_refusal> Answer;
	};

	[[nodiscard]] auto inspector(inspector_log& aLog) -> application::agent_wrapper_inspector
	{
		return [&aLog](const std::string_view aPath) {
			aLog.Asked.emplace_back(aPath);
			return aLog.Answer;
		};
	}

	void test_substitution_point()
	{
		auto inner  = recording_port{ };
		auto log    = inspector_log{ };
		auto source = std::make_shared<application::claude_process_wrapper_source>();
		auto port   = application::wrapped_claude_process{ inner, source, inspector(log) };

		const auto claude = request(core::agent_protocol::claude_stream_json);
		static_cast<void>(port.run(claude, 1));
		expect(inner.Requests.back() == claude && log.Asked.empty(), "with no wrapper the Claude request passes through unchanged and nothing is inspected");

		source->set("/opt/tools/claude-gateway");
		static_cast<void>(port.run(claude, 1));
		auto expected       = claude;
		expected.Executable = "/opt/tools/claude-gateway";
		expect(inner.Requests.back() == expected, "the wrapper replaces the executable and nothing else");
		expect(log.Asked == std::vector<std::string>{ "/opt/tools/claude-gateway" }, "the wrapper is inspected again at launch");

		for (const auto protocol : { core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec, core::agent_protocol::external_terminal }) {
			const auto other = request(protocol, "/usr/local/bin/other");
			static_cast<void>(port.run(other, 1));
			expect(inner.Requests.back() == other, "Codex and the external terminal are unchanged with a wrapper saved");
		}
		expect(log.Asked.size() == 1U, "a non-Claude launch inspects nothing");

		source->set("/opt/tools/next-gateway");
		static_cast<void>(port.run(claude, 1));
		expect(inner.Requests.back().Executable == "/opt/tools/next-gateway", "the launch reads the wrapper saved now, not the one saved first");

		log.Answer         = agent_wrapper_refusal::missing;
		const auto before  = inner.Requests.size();
		const auto refused = port.run(claude, 1);
		expect(inner.Requests.size() == before, "a refused wrapper starts nothing, the bare claude included");
		expect(refused.Outcome == application::agent_run_outcome::failed_to_start && refused.Session == claude.Session, "a refused wrapper ends the request as failed_to_start");
		expect(refused.StandardError == "Claude process wrapper /opt/tools/next-gateway: Nothing exists at this path.", "the failure names the wrapper and the reason");

		port.cancel();
		expect(inner.Cancels == 1, "cancel reaches the launcher");
	}

	/// [[BPR-1301]], owner 2026-10-07. A scripted locator: what it answers
	/// now, and how often it was asked.
	struct locator_log
	{
		std::optional<core::claude_search_candidate> Answer;
		std::size_t                                  Asked = 0U;
	};

	[[nodiscard]] auto locator(locator_log& aLog) -> application::claude_locator
	{
		return [&aLog] {
			++aLog.Asked;
			return aLog.Answer;
		};
	}

	void test_claude_found_without_a_wrapper()
	{
		auto inner   = recording_port{ };
		auto log     = inspector_log{ };
		auto located = locator_log{ .Answer = core::claude_search_candidate{ .Path = "/opt/homebrew/bin/claude", .Origin = core::claude_found_in::path } };
		auto source  = std::make_shared<application::claude_process_wrapper_source>();
		auto port    = application::wrapped_claude_process{ inner, source, inspector(log), locator(located) };

		const auto claude   = request(core::agent_protocol::claude_stream_json);
		auto       expected = claude;
		expected.Executable = "/opt/homebrew/bin/claude";
		static_cast<void>(port.run(claude, 1));
		expect(inner.Requests.back() == expected, "with no wrapper the found claude replaces the bare name and nothing else");
		expect(located.Asked == 1U && log.Asked.empty(), "the search ran for this launch and no wrapper was inspected");

		located.Answer = core::claude_search_candidate{ .Path = "/Users/me/.local/bin/claude", .Origin = core::claude_found_in::usual_folder };
		static_cast<void>(port.run(claude, 1));
		expect(inner.Requests.back().Executable == "/Users/me/.local/bin/claude" && located.Asked == 2U, "each launch searches again and runs what is found now");

		// Removed between Settings showing it and the launch: nothing is found.
		located.Answer     = std::nullopt;
		const auto before  = inner.Requests.size();
		const auto missing = port.run(claude, 1);
		expect(inner.Requests.size() == before, "with no claude found nothing is started, the bare name included");
		expect(missing.Outcome == application::agent_run_outcome::failed_to_start && missing.Session == claude.Session, "no claude found ends the request as failed_to_start");
		expect(missing.StandardError == application::claude_not_found_message, "the failure says claude was not found and where to set a wrapper");

		// A saved wrapper always wins: the search is not even asked.
		located.Answer = core::claude_search_candidate{ .Path = "/opt/homebrew/bin/claude" };
		source->set("/opt/tools/claude-gateway");
		const auto asked = located.Asked;
		static_cast<void>(port.run(claude, 1));
		expect(inner.Requests.back().Executable == "/opt/tools/claude-gateway" && located.Asked == asked, "a saved wrapper wins and nothing is searched");
		log.Answer        = agent_wrapper_refusal::missing;
		const auto gone   = inner.Requests.size();
		const auto failed = port.run(claude, 1);
		expect(failed.Outcome == application::agent_run_outcome::failed_to_start && inner.Requests.size() == gone && located.Asked == asked,
			   "a refused wrapper never falls back to the claude on PATH");
		log.Answer = std::nullopt;
		source->set({ });

		// A request that already names its program by path is left alone.
		const auto named = request(core::agent_protocol::claude_stream_json, "/fixtures/fake-claude");
		static_cast<void>(port.run(named, 1));
		expect(inner.Requests.back() == named && located.Asked == asked, "an absolute executable is run as built and nothing is searched");
		for (const auto protocol : { core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec, core::agent_protocol::external_terminal }) {
			const auto other = request(protocol, "codex");
			static_cast<void>(port.run(other, 1));
			expect(inner.Requests.back() == other, "Codex and the external terminal are never pointed at claude");
		}
		expect(located.Asked == asked, "a non-Claude launch searches nothing");

		// No locator: the request passes through exactly as before.
		auto unlocated = application::wrapped_claude_process{ inner, source, inspector(log) };
		static_cast<void>(unlocated.run(claude, 1));
		expect(inner.Requests.back() == claude, "with no locator the bare request passes through unchanged");
	}

	struct durable_state
	{
		application::workbench_profile_preferences Preferences;
		std::size_t                                Writes = 0U;
	};

	struct settings_port final : application::workbench_settings_port
	{
		explicit settings_port(durable_state& aState)
			: State{ aState }
		{ }
		auto load(std::stop_token) -> application::workbench_settings_result<application::workbench_layout_settings> override { return application::workbench_layout_settings{ }; }
		auto store(const application::workbench_layout_settings&, std::stop_token) -> application::workbench_settings_result<void> override { return { }; }
		auto profile_scoped() const noexcept -> bool override { return true; }
		auto load_preferences(std::stop_token) -> application::workbench_settings_result<application::workbench_profile_preferences> override { return State.Preferences; }
		auto store_preferences(const application::workbench_profile_preferences& aPreferences, std::stop_token) -> application::workbench_settings_result<void> override
		{
			++State.Writes;
			State.Preferences = aPreferences;
			return { };
		}
		durable_state& State;
	};

	void test_editor_saves_only_accepted_paths()
	{
		auto durable  = durable_state{ };
		auto port     = settings_port{ durable };
		auto effects  = application::manual_executor{ };
		auto decision = application::manual_executor{ };
		auto host     = application::workbench_preferences_host{ port, effects, decision, core::bpr_profile_id{ 1U } };
		host.adopt_loaded(durable.Preferences);
		auto       log     = inspector_log{ };
		auto       editor  = application::claude_wrapper_editor{ host, effects, decision, inspector(log) };
		auto       answers = std::vector<application::claude_wrapper_answer>{ };
		const auto record  = [&answers](const application::claude_wrapper_answer& aAnswer) { answers.push_back(aAnswer); };

		log.Answer = agent_wrapper_refusal::not_executable;
		editor.choose("/opt/tools/plain", record);
		expect(answers.empty() && log.Asked.empty() && !editor.is_quiescent(), "the inspection waits for the effect lane");
		effects.drain();
		expect(log.Asked == std::vector<std::string>{ "/opt/tools/plain" } && answers.empty(), "the effect lane inspects, the decision lane answers");
		decision.drain();
		effects.drain();
		decision.drain();
		expect(answers.size() == 1U && answers.back().Refusal == agent_wrapper_refusal::not_executable && !answers.back().Submitted, "a refused path is answered with its reason");
		expect(durable.Writes == 0U && host.accepted()->ClaudeWrapper.empty() && editor.is_quiescent(), "a refused path is not saved");

		log.Answer = std::nullopt;
		editor.choose("/opt/tools/claude-gateway", record);
		effects.drain();
		decision.drain();
		effects.drain();
		decision.drain();
		expect(answers.back().Submitted && !answers.back().Refusal, "an accepted path is submitted");
		expect(durable.Writes == 1U && durable.Preferences.ClaudeWrapper == "/opt/tools/claude-gateway" && host.accepted()->ClaudeWrapper == "/opt/tools/claude-gateway",
			   "an accepted path is stored for the profile");

		editor.choose("/bin/claude\n--x", record);
		expect(answers.back().Refusal == agent_wrapper_refusal::control_character && log.Asked.size() == 2U, "unstorable text is refused without inspection");

		editor.choose("/opt/first", record);
		editor.choose("/opt/second", record);
		effects.drain();
		decision.drain();
		effects.drain();
		decision.drain();
		expect(answers.back().Path == "/opt/second" && durable.Preferences.ClaudeWrapper == "/opt/second", "only the latest request is answered and saved");

		editor.choose({ }, record);
		effects.drain();
		decision.drain();
		expect(answers.back().Submitted && durable.Preferences.ClaudeWrapper.empty(), "empty text clears the wrapper without inspection");

		// An agent-free build never reads the wrapper, but every other
		// preference write must carry it through untouched.
		editor.choose("/opt/kept", record);
		effects.drain();
		decision.drain();
		effects.drain();
		decision.drain();
		auto behaviour     = host.accepted()->Behaviour;
		behaviour.MarkRead = behaviour.MarkRead == application::review_mark_read_behaviour::by_hand ? application::review_mark_read_behaviour::when_fully_seen
																									: application::review_mark_read_behaviour::by_hand;
		expect(host.apply_behaviour(behaviour), "another preference is written");
		expect(host.apply_writing_hook(core::writing_hook::problem, "Be brief."), "a writing hook is written");
		effects.drain();
		decision.drain();
		effects.drain();
		decision.drain();
		expect(durable.Preferences.ClaudeWrapper == "/opt/kept" && durable.Preferences.Behaviour.MarkRead == behaviour.MarkRead, "other writes keep the stored wrapper");
		expect(!host.apply_claude_wrapper(std::string(core::maximum_agent_wrapper_bytes + 1U, '/')), "the host refuses an unstorable wrapper itself");

		const auto answered = answers.size();
		editor.choose("/opt/after", record);
		editor.shutdown();
		effects.drain();
		decision.drain();
		expect(answers.size() == answered && durable.Preferences.ClaudeWrapper == "/opt/kept" && editor.is_quiescent(), "a shut-down editor answers and saves nothing");
	}

	void test_editor_locates_for_settings()
	{
		auto durable  = durable_state{ };
		auto port     = settings_port{ durable };
		auto effects  = application::manual_executor{ };
		auto decision = application::manual_executor{ };
		auto host     = application::workbench_preferences_host{ port, effects, decision, core::bpr_profile_id{ 1U } };
		host.adopt_loaded(durable.Preferences);
		auto       log     = inspector_log{ };
		auto       located = locator_log{ .Answer = core::claude_search_candidate{ .Path = "/opt/homebrew/bin/claude" } };
		auto       editor  = application::claude_wrapper_editor{ host, effects, decision, inspector(log), locator(located) };
		auto       answers = std::vector<std::optional<core::claude_search_candidate>>{ };
		const auto record  = [&answers](const std::optional<core::claude_search_candidate>& aFound) { answers.push_back(aFound); };

		editor.locate(record);
		expect(located.Asked == 0U && answers.empty() && !editor.is_quiescent(), "the search waits for the effect lane");
		effects.drain();
		expect(located.Asked == 1U && answers.empty(), "the effect lane searches");
		decision.drain();
		expect(answers.size() == 1U && answers.back() && answers.back()->Path == "/opt/homebrew/bin/claude" && editor.is_quiescent(), "the decision lane answers with the found path");

		editor.locate(record);
		located.Answer = std::nullopt;
		editor.locate(record);
		effects.drain();
		decision.drain();
		expect(answers.size() == 2U && !answers.back(), "only the latest search is answered, and not found is an answer");
		expect(durable.Writes == 0U, "searching saves nothing");

		auto none = application::claude_wrapper_editor{ host, effects, decision, inspector(log) };
		none.locate(record);
		expect(answers.size() == 3U && !answers.back() && none.is_quiescent(), "with no locator nothing is found, at once");

		editor.locate(record);
		editor.shutdown();
		effects.drain();
		decision.drain();
		expect(answers.size() == 3U && editor.is_quiescent(), "a shut-down editor searches and answers nothing");
	}
}

auto main() -> int
{
	test_substitution_point();
	test_editor_saves_only_accepted_paths();
	test_claude_found_without_a_wrapper();
	test_editor_locates_for_settings();
	std::cout << "claude-process-wrapper-host-tests:pass\n";
	return EXIT_SUCCESS;
}
