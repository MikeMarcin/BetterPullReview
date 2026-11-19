/// \file
/// \brief [[BPR-2004]], RFC-BPR-0103. A turn with helpers stays open, accepts
/// their later frames and ends once; a runtime that ends under its helpers
/// ends the turn as an error that keeps what arrived; a frame for no open turn
/// is refused.
///
/// The frames are the installed runtimes' own, recorded on 2026-11-10 and
/// trimmed of account and machine details (`tests/fixtures/agent/traces`):
/// Claude Code 2.1.319 on `-p --input-format stream-json --output-format
/// stream-json --verbose`, with its input held open and with it closed, and
/// Codex 0.167.0 on `app-server`, each asked to start three helpers, say so
/// and end its message without waiting.

#include "agent_protocol_loop.hpp"
#include "bpr/application/agent_run_diagnosis.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#if !defined(BPR_AGENT_TRACE_DIR)
	#error "BPR_AGENT_TRACE_DIR names the recorded traces"
#endif

namespace {
	using namespace bpr;
	using namespace bpr::adapters::agent::detail;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto trace(const std::string_view aName) -> std::vector<std::string>
	{
		auto file  = std::ifstream{ std::string{ BPR_AGENT_TRACE_DIR } + "/" + std::string{ aName } };
		auto lines = std::vector<std::string>{ };
		for (auto line = std::string{ }; std::getline(file, line);) {
			if (!line.empty()) {
				lines.push_back(line);
			}
		}
		expect(!lines.empty(), "the recorded trace is readable");
		return lines;
	}

	/// One line per read, as a runtime writes them; then the process exits.
	class trace_transport final : public agent_protocol_transport
	{
	public:
		explicit trace_transport(const std::vector<std::string>& aLines)
		{
			for (const auto& line : aLines) {
				Output.push_back(line + "\n");
			}
		}

		[[nodiscard]] auto write(const std::string_view aBytes) -> agent_protocol_write_status override
		{
			Writes.emplace_back(aBytes);
			return agent_protocol_write_status::success;
		}
		void               close_input() noexcept override { ++Closes; }
		[[nodiscard]] auto read_standard_output(std::span<char> aBuffer) -> agent_protocol_pipe_read_result override
		{
			if (Output.empty()) {
				return { };
			}
			const auto bytes = std::move(Output.front());
			Output.pop_front();
			expect(bytes.size() <= aBuffer.size(), "a recorded line fits the read buffer");
			std::ranges::copy(bytes, aBuffer.begin());
			return { .Status = agent_protocol_pipe_read_status::data, .Count = bytes.size(), .BatchComplete = true };
		}
		[[nodiscard]] auto read_standard_error(std::span<char>) -> agent_protocol_pipe_read_result override { return { }; }
		[[nodiscard]] auto wait() -> agent_protocol_wait_result override { return { .Status = agent_protocol_wait_status::exited, .ExitCode = 0, .ExitedNormally = true }; }
		[[nodiscard]] auto terminate() noexcept -> bool override
		{
			++Terminations;
			return true;
		}

		std::deque<std::string>  Output;
		std::vector<std::string> Writes;
		std::size_t              Closes       = 0U;
		std::size_t              Terminations = 0U;
	};

	struct observed
	{
		application::agent_run_result                Result;
		std::vector<application::agent_stream_event> Stream;
	};

	[[nodiscard]] auto run(application::agent_launch_request aRequest, const std::vector<std::string>& aLines) -> observed
	{
		auto       seen = observed{ };
		auto       wire = trace_transport{ aLines };
		const auto sink = application::agent_output_sink{ [&seen](const application::agent_stream_event& aEvent) { seen.Stream.push_back(aEvent); } };
		seen.Result     = run_agent_protocol_loop(aRequest, sink, wire);
		return seen;
	}

	[[nodiscard]] auto request(const core::agent_protocol aProtocol) -> application::agent_launch_request
	{
		auto launched           = application::agent_launch_request{ };
		launched.Protocol       = aProtocol;
		launched.Session        = core::agent_session_id{ 41U };
		launched.Input          = "initial-input";
		launched.ReportActivity = true;
		if (aProtocol == core::agent_protocol::codex_app_server) {
			launched.DeferredContext = "selected-context";
		}
		return launched;
	}

	[[nodiscard]] auto answer(const application::agent_run_result& aResult) -> std::string
	{
		auto text = std::string{ };
		for (const auto& event : aResult.Events) {
			text += event.Text;
		}
		return text;
	}

	[[nodiscard]] auto count(const observed& aSeen, const core::agent_activity_kind aKind) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(aSeen.Stream, [aKind](const application::agent_stream_event& aEvent) {
			return aEvent.Kind == application::agent_stream_kind::activity && aEvent.Activity && aEvent.Activity->Kind == aKind;
		}));
	}

	[[nodiscard]] auto state(const observed& aSeen) -> core::agent_turn_activity
	{
		auto turn = core::agent_turn_activity{ };
		auto at   = std::uint64_t{ 0U };
		for (const auto& event : aSeen.Stream) {
			++at;
			if (event.Activity) {
				turn = core::apply_agent_activity(turn, *event.Activity, at);
			}
		}
		return turn;
	}

	void expect_three_helpers_one_turn(const observed& aSeen, const std::string_view aCourse)
	{
		const auto& result = aSeen.Result;
		if (result.Outcome != application::agent_run_outcome::success) {
			std::cerr << aCourse << ": " << application::name(result.Outcome) << " " << (result.ProtocolFailure ? application::name(*result.ProtocolFailure) : "") << " line=" << result.RejectedOutputLine << '\n';
		}
		expect(result.Outcome == application::agent_run_outcome::success, "the turn ends once, as an answer");
		const auto said = answer(result);
		expect(said.find("I started three reviewers") != std::string::npos, "the text before the helpers finished is the turn's");
		expect(said.find("a.txt: 25 lines | b.txt: 23 lines | c.txt: 31 lines") != std::string::npos, "and the closing message, after them, is the same turn's");
		expect(std::ranges::none_of(result.Events, [](const auto& aEvent) { return aEvent.Text == "a.txt: 25 lines" || aEvent.Text == "b.txt: 23 lines"; }), "a helper's own words are never the answer");
		expect(result.Helpers.Started == 3U && result.Helpers.Finished == 3U && result.Helpers.Abandoned == 0U, "three helpers, all finished");
		expect(count(aSeen, core::agent_activity_kind::helper_started) == 3U && count(aSeen, core::agent_activity_kind::helper_ended) == 3U, "each helper's start and end reach the running block");
		const auto turn = state(aSeen);
		expect(turn.Helpers.size() == 3U && core::outstanding_agent_helpers(turn) == 0U, "three rows, none outstanding");
		expect(turn.Helpers[0].Description == "Count lines in a.txt", "a row has the helper's own description");
		expect(std::ranges::count_if(aSeen.Stream, [](const auto& aEvent) { return aEvent.Kind == application::agent_stream_kind::heartbeat; }) > 0, "frames that show nothing still say the runtime is alive");
	}

	// [[BPR-2261]]. Frames of one Claude turn whose helper reports back, as
	// Claude Code 2.1.319 writes them, trimmed to what the decoder reads.
	[[nodiscard]] auto claude_frame(const std::string& aBody) -> std::string { return R"({"session_id":"held-session",)" + aBody + "}"; }
	[[nodiscard]] auto claude_init() -> std::string { return claude_frame(R"("type":"system","subtype":"init","tools":[],"mcp_servers":[])"); }
	[[nodiscard]] auto claude_tool(const std::string& aMessage, const std::string& aUse, const std::string& aTool) -> std::string
	{
		return claude_frame(R"("type":"assistant","message":{"id":")" + aMessage + R"(","role":"assistant","content":[{"type":"tool_use","id":")" + aUse + R"(","name":")" + aTool + R"(","input":{}}]})");
	}
	[[nodiscard]] auto claude_text(const std::string& aMessage, const std::string& aText) -> std::string
	{
		return claude_frame(R"("type":"assistant","message":{"id":")" + aMessage + R"(","role":"assistant","content":[{"type":"text","text":")" + aText + R"("}]})");
	}
	[[nodiscard]] auto claude_thinking(const std::string& aMessage) -> std::string
	{
		return claude_frame(R"("type":"assistant","message":{"id":")" + aMessage + R"(","role":"assistant","content":[{"type":"thinking","thinking":"..."}]})");
	}
	[[nodiscard]] auto claude_tool_result(const std::string& aUse) -> std::string
	{
		return claude_frame(R"("type":"user","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":")" + aUse + R"(","content":"ok"}]})");
	}
	[[nodiscard]] auto claude_helper_started() -> std::string { return claude_frame(R"("type":"system","subtype":"task_started","task_id":"helper-1","tool_use_id":"use-agent","description":"Review the loop")"); }
	[[nodiscard]] auto claude_helper_reported() -> std::string { return claude_frame(R"("type":"system","subtype":"task_notification","task_id":"helper-1","status":"completed","summary":"The loop ends.")"); }
	[[nodiscard]] auto claude_success(const std::string& aUuid) -> std::string { return claude_frame(R"("type":"result","subtype":"success","is_error":false,"uuid":")" + aUuid + R"(","result":"done")"); }

	/// The helper reports back while the agent is still using tools; the
	/// next message, written after a tool result, answers it in the same part.
	[[nodiscard]] auto report_answered_in_its_part() -> std::vector<std::string>
	{
		return { claude_init(),
				 claude_tool("message-1", "use-agent", "Agent"),
				 claude_helper_started(),
				 claude_tool_result("use-agent"),
				 claude_tool("message-2", "use-read", "Read"),
				 claude_helper_reported(),
				 claude_tool_result("use-read"),
				 claude_text("message-3", "# Review of the loop\\n\\nIt ends."),
				 claude_success("result-1") };
	}

	/// The helper reports back while the agent writes its last message; the
	/// rest of that message did not see it, and no part follows.
	[[nodiscard]] auto report_left_unanswered() -> std::vector<std::string>
	{
		return { claude_init(),
				 claude_tool("message-1", "use-agent", "Agent"),
				 claude_helper_started(),
				 claude_tool_result("use-agent"),
				 claude_text("message-1", "I asked a helper about the loop."),
				 claude_thinking("message-2"),
				 claude_helper_reported(),
				 claude_text("message-2", "# Review of the loop\\n\\nIt ends."),
				 claude_success("result-1") };
	}

	/// A trace transport whose process keeps running, as a native connection
	/// does; after a few waits it is stopped (\p aEnding) so a turn that is
	/// never ended cannot hang the test.
	class held_transport final : public agent_protocol_transport
	{
	public:
		held_transport(const std::vector<std::string>& aLines, const agent_protocol_wait_status aEnding)
			: Lines{ aLines }
			, Ending{ aEnding }
		{ }
		[[nodiscard]] auto write(const std::string_view aBytes) -> agent_protocol_write_status override { return Lines.write(aBytes); }
		void               close_input() noexcept override { Lines.close_input(); }
		[[nodiscard]] auto read_standard_output(std::span<char> aBuffer) -> agent_protocol_pipe_read_result override { return Lines.read_standard_output(aBuffer); }
		[[nodiscard]] auto read_standard_error(std::span<char> aBuffer) -> agent_protocol_pipe_read_result override { return Lines.read_standard_error(aBuffer); }
		[[nodiscard]] auto wait() -> agent_protocol_wait_result override
		{
			++Waits;
			return Waits < 4U ? agent_protocol_wait_result{ .Status = agent_protocol_wait_status::running } : agent_protocol_wait_result{ .Status = Ending };
		}
		[[nodiscard]] auto terminate() noexcept -> bool override { return Lines.terminate(); }

		trace_transport            Lines;
		agent_protocol_wait_status Ending;
		std::size_t                Waits = 0U;
	};

	[[nodiscard]] auto run_held(application::agent_launch_request aRequest, const std::vector<std::string>& aLines, const agent_protocol_wait_status aEnding) -> application::agent_run_result
	{
		auto wire = held_transport{ aLines, aEnding };
		return run_agent_protocol_loop(aRequest, { }, wire);
	}

	void test_claude_with_its_input_open()
	{
		// AC02 on the native connection: input held open, as the Agent tab's
		// persistent session does. Claude writes its message's result while
		// two helpers are out, then answers each report in a part of its own.
		auto launched               = request(core::agent_protocol::claude_stream_json);
		launched.NativeConversation = "durable-conversation";
		const auto seen             = run(launched, trace("claude-2.1.319-helpers-stdin-open.jsonl"));
		expect_three_helpers_one_turn(seen, "stdin-open");
		expect(seen.Result.ReportedCostUsd == "0.016330895", "the cost is the runtime's last figure, covering all of it");
		// [[BPR-2252]]. Each figure reaches the running line as it is said:
		// the first while two helpers are still out, long before the end.
		auto costs = std::vector<std::string>{ };
		auto first = seen.Stream.end();
		for (auto at = seen.Stream.begin(); at != seen.Stream.end(); ++at) {
			if (at->Kind == application::agent_stream_kind::cost) {
				costs.push_back(at->Text);
				first = first == seen.Stream.end() ? at : first;
			}
		}
		expect(costs == std::vector<std::string>{ "0.012086645", "0.014288390000000002", "0.015761495", "0.016330895" }, "every figure the turn reported is streamed, in order");
		expect(first != seen.Stream.end() && std::ranges::count_if(first, seen.Stream.end(), [](const auto& aEvent) { return aEvent.Kind == application::agent_stream_kind::activity && aEvent.Activity && aEvent.Activity->Kind == core::agent_activity_kind::helper_ended; }) >= 2,
			   "the first figure arrives while helpers are still out");
		expect(seen.Result.ReadPast.find("system/post_turn_summary") != std::string::npos, "the frame types read past are named for the diagnosis");
	}

	void test_claude_with_its_input_closed()
	{
		// AC02 one-shot: input closed at once; Claude holds its results until
		// the helpers are done, then exits.
		const auto seen = run(request(core::agent_protocol::claude_stream_json), trace("claude-2.1.319-helpers-stdin-closed.jsonl"));
		expect_three_helpers_one_turn(seen, "stdin-closed");
		expect(seen.Result.ReportedCostUsd == "0.0132714", "one cost, the last");
	}

	void test_claude_ending_under_its_helpers()
	{
		// AC03. The same script, and the runtime ends after its message's
		// result while two helpers are out.
		auto       lines  = trace("claude-2.1.319-helpers-stdin-open.jsonl");
		const auto result = std::ranges::find_if(lines, [](const std::string& aLine) { return aLine.find("\"type\":\"result\"") != std::string::npos; });
		expect(result != lines.end(), "the trace has a result");
		lines.erase(result + 1, lines.end());
		const auto seen = run(request(core::agent_protocol::claude_stream_json), lines);
		expect(seen.Result.Outcome == application::agent_run_outcome::protocol_failure && seen.Result.ProtocolFailure == application::agent_protocol_failure::helpers_unfinished, "an error, never an answer that promises more");
		expect(seen.Result.UnfinishedHelpers == std::vector<std::string>{ "Count lines in b.txt", "Count lines in c.txt" }, "it names the helpers that never reported back");
		expect(answer(seen.Result).find("I started three reviewers") != std::string::npos, "and keeps the text that arrived");
		expect(seen.Result.Helpers.Started == 3U && seen.Result.Helpers.Finished == 1U && seen.Result.Helpers.Abandoned == 2U, "the diagnosis counts them abandoned");
	}

	void test_a_frame_for_no_open_turn_is_refused()
	{
		// AC04. Once the turn is over, a frame belongs to no turn.
		auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		for (const auto& line : trace("claude-2.1.319-helpers-stdin-open.jsonl")) {
			expect(decoder.push(line + "\n").has_value(), "every recorded frame of the turn is accepted");
		}
		expect(decoder.turn_complete(), "the turn is complete after its last result");
		auto late = decoder;
		expect(!late.push("{\"type\":\"assistant\",\"session_id\":\"c2ac7d37-cfae-431d-a7d4-6752e860f15b\",\"message\":{\"role\":\"assistant\",\"id\":\"late\",\"content\":[{\"type\":\"text\",\"text\":\"more\"}]}}\n"), "a late answer is refused");
		auto started = decoder;
		expect(!started.push("{\"type\":\"system\",\"subtype\":\"task_started\",\"session_id\":\"c2ac7d37-cfae-431d-a7d4-6752e860f15b\",\"task_id\":\"late\",\"description\":\"late\"}\n"), "a helper started after the turn is refused");
		// Before the message's result, the same frames are the turn's.
		auto early = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		auto lines = trace("claude-2.1.319-helpers-stdin-open.jsonl");
		for (const auto& line : lines) {
			expect(early.push(line + "\n").has_value(), "accepted");
			if (line.find("\"type\":\"result\"") != std::string::npos) {
				break;
			}
		}
		expect(!early.turn_complete() && early.helpers().Running.size() == 2U, "a result with helpers out does not end the turn");
	}

	void test_codex_app_server()
	{
		// AC01, AC02 for Codex 0.167.0 on the App Server: its turn completes
		// while two sub-agents run; they end it.
		const auto seen = run(request(core::agent_protocol::codex_app_server), trace("codex-0.167.0-app-server-helpers.jsonl"));
		if (seen.Result.Outcome != application::agent_run_outcome::success) {
			std::cerr << "codex: " << application::name(seen.Result.Outcome) << " " << (seen.Result.ProtocolFailure ? application::name(*seen.Result.ProtocolFailure) : "") << " line=" << seen.Result.RejectedOutputLine << '\n';
		}
		expect(seen.Result.Outcome == application::agent_run_outcome::success, "the Codex turn ends once");
		expect(answer(seen.Result).find("Started three reviewers, one each for a.txt, b.txt, and c.txt.") != std::string::npos, "its message is the answer");
		expect(answer(seen.Result).find("b.txt contains 23 lines.") == std::string::npos, "a sub-agent's message is not");
		expect(seen.Result.Helpers.Started == 3U && seen.Result.Helpers.Finished == 3U, "three helpers, all finished");
		const auto turn = state(seen);
		expect(turn.Helpers.size() == 3U && turn.Helpers[1].Description == "count b" && turn.Helpers[1].Last == "b.txt contains 23 lines.", "a row names the helper and what it last said");
		expect(turn.Commands == 3U, "the helpers' commands are counted");
	}

	void test_codex_helper_thread_before_its_report()
	{
		// Live on 2026-11-10, a sub-agent's thread spoke before the agent's
		// thread reported it. Read past, never the answer, never the end.
		auto       lines   = trace("codex-0.167.0-app-server-helpers.jsonl");
		const auto started = std::ranges::find_if(lines, [](const std::string& aLine) { return aLine.find("\"method\":\"turn/started\"") != std::string::npos; });
		expect(started != lines.end(), "the trace starts a turn");
		lines.insert(started + 1, "{\"method\":\"thread/status/changed\",\"params\":{\"threadId\":\"not-yet-reported\",\"status\":{\"type\":\"idle\"}}}");
		lines.insert(started + 1, "{\"method\":\"item/completed\",\"params\":{\"threadId\":\"not-yet-reported\",\"turnId\":\"t\",\"item\":{\"type\":\"agentMessage\",\"id\":\"m\",\"text\":\"early\"}}}");
		const auto seen = run(request(core::agent_protocol::codex_app_server), lines);
		expect(seen.Result.Outcome == application::agent_run_outcome::success, "the turn still ends once");
		expect(answer(seen.Result).find("early") == std::string::npos, "and the early frame is not the answer");
	}

	void test_activity_is_reported_only_when_asked()
	{
		auto launched           = request(core::agent_protocol::claude_stream_json);
		launched.ReportActivity = false;
		const auto seen         = run(launched, trace("claude-2.1.319-helpers-stdin-closed.jsonl"));
		expect(seen.Result.Outcome == application::agent_run_outcome::success, "the turn's lifecycle does not depend on the block");
		expect(std::ranges::all_of(seen.Stream, [](const auto& aEvent) { return aEvent.Kind == application::agent_stream_kind::text; }), "a caller that shows no block hears text only");
	}
	/// [[BPR-2261]], AC1. A helper that reports back while the agent is
	/// still using tools is answered in the part it is in: the result ends
	/// the turn, with no further init. Without the fix the turn was held for
	/// a part that never came.
	void test_a_report_answered_inside_its_part_ends_the_turn()
	{
		auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		for (const auto& line : report_answered_in_its_part()) {
			const auto pushed = decoder.push(line + "\n");
			if (!pushed) {
				std::cerr << application::name(pushed.error()) << ": " << line << '\n';
			}
			expect(pushed.has_value(), "every frame of the turn is accepted");
		}
		expect(decoder.turn_complete() && !decoder.awaiting_report_back() && decoder.answered(), "the result ends the turn: the report was answered");

		auto launched               = request(core::agent_protocol::claude_stream_json);
		launched.NativeConversation = "durable-conversation";
		const auto result           = run_held(launched, report_answered_in_its_part(), agent_protocol_wait_status::cancelled);
		expect(result.Outcome == application::agent_run_outcome::success && !result.HeldOut && !result.StoppedAfterAnswer && !result.HeldMs, "the native turn ends at its result, never held");
	}

	/// [[BPR-2261]], AC1. A result that leaves a report unanswered holds the
	/// turn for the part that answers it (the recorded traces' order), but
	/// never without a bound: past it the turn ends on the answer it gave,
	/// the hold and its bound are kept for the diagnosis, and the
	/// connection is not reused.
	void test_an_unanswered_report_holds_the_turn_only_to_its_bound()
	{
		auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		for (const auto& line : report_left_unanswered()) {
			expect(decoder.push(line + "\n").has_value(), "every frame of the turn is accepted");
		}
		expect(!decoder.turn_complete() && decoder.awaiting_report_back(), "the rest of a message written as the report came does not answer it");
		decoder.abandon_report_back();
		expect(decoder.turn_complete() && !decoder.native_connection_reusable(), "past the bound the turn ends, and a late part finds no connection");

		auto launched                 = request(core::agent_protocol::claude_stream_json);
		launched.NativeConversation   = "durable-conversation";
		launched.HeldTurnMilliseconds = 0;
		const auto result             = run_held(launched, report_left_unanswered(), agent_protocol_wait_status::cancelled);
		expect(result.Outcome == application::agent_run_outcome::success && result.HeldOut && result.HeldMs.has_value() && result.HeldBoundMs == 0U, "the bound ends the held turn on its answer, and says so");
		const auto said = answer(result);
		expect(said.find("I asked a helper") != std::string::npos && said.find("# Review of the loop") != std::string::npos, "with every word it gave");

		auto line = application::describe(application::agent_run_diagnosis{ .Protocol = core::agent_protocol::claude_stream_json, .Outcome = result.Outcome, .HeldMs = 5U, .HeldBoundMs = 30'000U, .HeldOut = true });
		expect(line.find(" held_ms=5 held_bound_ms=30000 held_out=on") != std::string::npos, "the diagnosis says the turn was held, and for how long: " + line);
		expect(application::parse_agent_run_diagnosis(line).has_value(), "and reads back");
	}

	/// [[BPR-2261]], AC2. A stop after the successful result with no helper
	/// out (the app quitting while the turn is held) leaves the answer; with
	/// a helper still out the run is stopped, and keeps what it wrote.
	void test_a_stop_after_the_answer_keeps_it()
	{
		auto launched               = request(core::agent_protocol::claude_stream_json);
		launched.NativeConversation = "durable-conversation";
		const auto held             = run_held(launched, report_left_unanswered(), agent_protocol_wait_status::cancelled);
		expect(held.Outcome == application::agent_run_outcome::success && held.StoppedAfterAnswer && !held.HeldOut, "stopped while held after its answer, the run stands as answered");
		expect(held.AssistantItems.size() == 2U && answer(held).find("# Review of the loop") != std::string::npos, "with its answer");
		const auto timedOut = run_held(launched, report_left_unanswered(), agent_protocol_wait_status::timeout);
		expect(timedOut.Outcome == application::agent_run_outcome::success && timedOut.StoppedAfterAnswer, "and so when its time runs out");

		auto out = report_left_unanswered();
		out.erase(out.begin() + 6);
		const auto running = run_held(launched, out, agent_protocol_wait_status::cancelled);
		expect(running.Outcome == application::agent_run_outcome::cancelled && !running.StoppedAfterAnswer, "with a helper still out, it was stopped");
		expect(running.AssistantItems.size() == 2U, "and what it wrote is kept for its turn");

		auto unanswered = report_left_unanswered();
		unanswered.pop_back();
		const auto early = run_held(launched, unanswered, agent_protocol_wait_status::cancelled);
		expect(early.Outcome == application::agent_run_outcome::cancelled && !early.StoppedAfterAnswer, "a stop before any result is a stop");
	}
}

namespace {
	/// [[BPR-2285]]. A Bash command reaches the running block whole, every
	/// line of it, trimmed; the Agent tab shortens it and opens to the rest.
	void test_a_command_is_reported_whole()
	{
		auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		decoder.report_activity(true);
		const auto bash    = claude_frame(R"("type":"assistant","message":{"id":"message-1","role":"assistant","content":[{"type":"tool_use","id":"use-bash","name":"Bash","input":{"command":"  python3 - <<'EOF'\nprint(42)\nEOF\n"}}]})");
		auto       subject = std::string{ };
		auto       doing   = core::agent_doing::starting;
		for (const auto& line : { claude_init(), bash }) {
			const auto pushed = decoder.push(line + "\n");
			expect(pushed.has_value(), "the frames are accepted");
			for (const auto& event : *pushed) {
				if (event.Activity && event.Activity->Kind == core::agent_activity_kind::doing) {
					doing   = event.Activity->Doing;
					subject = event.Activity->Subject;
				}
			}
		}
		expect(doing == core::agent_doing::running && subject == "python3 - <<'EOF'\nprint(42)\nEOF", "the command, every line, trimmed: " + subject);
	}
} // namespace

auto main() -> int
{
	test_a_command_is_reported_whole();
	test_claude_with_its_input_open();
	test_claude_with_its_input_closed();
	test_claude_ending_under_its_helpers();
	test_a_frame_for_no_open_turn_is_refused();
	test_codex_app_server();
	test_codex_helper_thread_before_its_report();
	test_activity_is_reported_only_when_asked();
	test_a_report_answered_inside_its_part_ends_the_turn();
	test_an_unanswered_report_holds_the_turn_only_to_its_bound();
	test_a_stop_after_the_answer_keeps_it();
	std::cout << "agent-helper-turn-tests:pass\n";
	return EXIT_SUCCESS;
}
