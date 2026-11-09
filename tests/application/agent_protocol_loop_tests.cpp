/// \file
/// \brief Proves the platform-neutral agent protocol loop through a fake transport.

#include "agent_protocol_loop.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

	[[nodiscard]] auto status_name(
		const agent_protocol_pipe_read_status aStatus) -> std::string_view
	{
		switch (aStatus) {
		case agent_protocol_pipe_read_status::no_data:
			return "no_data";
		case agent_protocol_pipe_read_status::data:
			return "data";
		case agent_protocol_pipe_read_status::closed:
			return "closed";
		case agent_protocol_pipe_read_status::failed:
			return "failed";
		}
		return "unknown";
	}

	struct read_step
	{
		agent_protocol_pipe_read_status Status
			= agent_protocol_pipe_read_status::no_data;
		std::string Bytes;
		bool        BatchComplete = true;
	};

	class fake_transport final
		: public agent_protocol_transport
	{
	public:
		[[nodiscard]] auto write(const std::string_view aBytes)
			-> agent_protocol_write_status override
		{
			Operations.emplace_back("write");
			Writes.emplace_back(aBytes);
			if (aBytes.find("turn/start") != std::string_view::npos) {
				TurnInputWasOpen = CloseInputCount == 0U;
			}
			if (WriteStatuses.empty()) {
				return agent_protocol_write_status::success;
			}
			const auto status = WriteStatuses.front();
			WriteStatuses.pop_front();
			return status;
		}

		void close_input() noexcept override
		{
			Operations.emplace_back("close_input");
			++CloseInputCount;
		}

		[[nodiscard]] auto read_standard_output(std::span<char> aBuffer)
			-> agent_protocol_pipe_read_result override
		{
			return read("read_output", Output, aBuffer);
		}

		[[nodiscard]] auto read_standard_error(std::span<char> aBuffer)
			-> agent_protocol_pipe_read_result override
		{
			return read("read_error", Error, aBuffer);
		}

		[[nodiscard]] auto wait() -> agent_protocol_wait_result override
		{
			Operations.emplace_back("wait");
			if (Waits.empty()) {
				return { .Status = agent_protocol_wait_status::exited };
			}
			const auto result = Waits.front();
			Waits.pop_front();
			return result;
		}

		[[nodiscard]] auto terminate() noexcept -> bool override
		{
			Operations.emplace_back("terminate");
			++TerminateCount;
			return TerminateResult;
		}

		std::deque<agent_protocol_write_status> WriteStatuses;
		std::deque<read_step>                   Output;
		std::deque<read_step>                   Error;
		std::deque<agent_protocol_wait_result>  Waits;
		std::vector<std::string>                Writes;
		std::vector<std::string>                Operations;
		std::size_t                             CloseInputCount  = 0U;
		std::size_t                             TerminateCount   = 0U;
		bool                                    TerminateResult  = true;
		bool                                    TurnInputWasOpen = false;

	private:
		[[nodiscard]] auto read(
			const std::string_view aOperation,
			std::deque<read_step>& aSteps,
			const std::span<char>  aBuffer) -> agent_protocol_pipe_read_result
		{
			if (aSteps.empty()) {
				Operations.emplace_back(std::string{ aOperation } + ":no_data");
				return { };
			}
			auto step = std::move(aSteps.front());
			aSteps.pop_front();
			Operations.emplace_back(
				std::string{ aOperation } + ":" + std::string{ status_name(step.Status) });
			if (step.Status != agent_protocol_pipe_read_status::data) {
				return { .Status = step.Status };
			}
			expect(
				step.Bytes.size() <= aBuffer.size(),
				"fake data steps fit the caller-owned transport buffer");
			std::ranges::copy(step.Bytes, aBuffer.begin());
			return {
				.Status        = agent_protocol_pipe_read_status::data,
				.Count         = step.Bytes.size(),
				.BatchComplete = step.BatchComplete,
			};
		}
	};

	[[nodiscard]] auto request(const core::agent_protocol aProtocol)
		-> application::agent_launch_request
	{
		return {
			.Protocol        = aProtocol,
			.Session         = core::agent_session_id{ 41U },
			.Input           = "initial-input",
			.DeferredContext = aProtocol == core::agent_protocol::codex_app_server ? "selected-context" : "",
		};
	}

	[[nodiscard]] auto run(
		const core::agent_protocol            aProtocol,
		fake_transport&                       aTransport,
		const application::agent_output_sink& aSink = { })
		-> application::agent_run_result
	{
		return run_agent_protocol_loop(request(aProtocol), aSink, aTransport);
	}

	void expect_single_termination(
		const fake_transport& aTransport, const std::string_view aContext)
	{
		expect(aTransport.TerminateCount == 1U, aContext);
		expect(
			!aTransport.Operations.empty()
				&& aTransport.Operations.back() == "terminate",
			"termination is the final transport operation");
	}

	void test_all_write_statuses_are_classified()
	{
		const auto cases = std::vector<std::pair<
			agent_protocol_write_status,
			application::agent_run_outcome>>{
			{ agent_protocol_write_status::success,
			  application::agent_run_outcome::success },
			{ agent_protocol_write_status::cancelled,
			  application::agent_run_outcome::cancelled },
			{ agent_protocol_write_status::timeout,
			  application::agent_run_outcome::timeout },
			{ agent_protocol_write_status::process_exited,
			  application::agent_run_outcome::protocol_failure },
			{ agent_protocol_write_status::failed,
			  application::agent_run_outcome::protocol_failure },
		};
		for (const auto& [status, expected] : cases) {
			auto transport = fake_transport{ };
			transport.WriteStatuses.push_back(status);
			const auto result = run(core::agent_protocol::external_terminal, transport);
			expect(result.Outcome == expected, "each initial write status stays distinct");
			if (expected == application::agent_run_outcome::protocol_failure) {
				expect(
					result.ProtocolFailure
						== application::agent_protocol_failure::malformed_frame,
					"process-exited and failed writes are protocol failures");
			}
			expect_single_termination(
				transport, "every initial write outcome terminates exactly once");
		}
	}

	void test_app_server_phases_follow_successful_writes_only()
	{
		const auto initialized = std::string{
			"{\"id\":0,\"result\":{}}\n{\"method\":\"remoteControl/status/changed\",\"params\":{}}\n"
		};
		for (const auto& [status, expected] : {
				 std::pair{ agent_protocol_write_status::cancelled,
							application::agent_run_outcome::cancelled },
				 std::pair{ agent_protocol_write_status::timeout,
							application::agent_run_outcome::timeout },
			 }) {
			auto transport          = fake_transport{ };
			transport.WriteStatuses = {
				agent_protocol_write_status::success,
				status,
			};
			transport.Output.push_back({
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = initialized,
			});
			const auto result = run(core::agent_protocol::codex_app_server, transport);
			expect(
				result.Outcome == expected,
				"mid-follow-up cancellation and timeout remain distinct");
			expect(
				transport.Writes.size() == 2U && transport.CloseInputCount == 0U,
				"a failed follow-up write is not acknowledged or closed as phase two");
			expect_single_termination(
				transport, "a failed App Server follow-up terminates once");
		}

		auto transport   = fake_transport{ };
		transport.Output = {
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = initialized,
			},
			{ .Status = agent_protocol_pipe_read_status::no_data },
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = "{\"id\":1,\"result\":{\"thread\":{\"id\":\"thread-7\"}}}\n",
			},
			{ .Status = agent_protocol_pipe_read_status::no_data },
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes
				= "{\"id\":2,\"result\":{\"turn\":{\"id\":\"turn-8\","
				  "\"items\":[],\"status\":\"inProgress\"}}}\n"
				  "{\"method\":\"item/agentMessage/delta\",\"params\":{"
				  "\"threadId\":\"thread-7\",\"turnId\":\"turn-8\","
				  "\"itemId\":\"item-9\",\"delta\":\"reviewed\"}}\n"
				  "{\"method\":\"turn/completed\",\"params\":{"
				  "\"threadId\":\"thread-7\",\"turn\":{\"id\":\"turn-8\","
				  "\"items\":[],\"status\":\"completed\"}}}\n",
			},
			{ .Status = agent_protocol_pipe_read_status::no_data },
		};
		transport.Waits = {
			{ .Status = agent_protocol_wait_status::running },
			{ .Status = agent_protocol_wait_status::running },
			{ .Status = agent_protocol_wait_status::exited },
		};
		auto       sinkEvents = std::vector<application::agent_stream_event>{ };
		const auto result     = run(
			core::agent_protocol::codex_app_server,
			transport,
			[&](const auto& aEvent) { sinkEvents.push_back(aEvent); });
		expect(
			result.Outcome == application::agent_run_outcome::success
				&& result.Events.size() == 1U
				&& result.Events.front().Text == "reviewed"
				&& sinkEvents == result.Events,
			"App Server phases zero, one, and two publish one ordered event");
		expect(
			transport.Writes.size() == 3U
				&& transport.Writes[1U].find("thread/start") != std::string::npos
				&& transport.Writes[2U].find("selected-context")
					!= std::string::npos
				&& transport.CloseInputCount == 1U,
			"each App Server acknowledgement follows a complete successful write");
		expect(transport.TurnInputWasOpen, "one-shot App Server stdin remains open when explicit question is submitted");
		const auto closed        = std::ranges::find(transport.Operations, std::string{ "close_input" });
		const auto afterLastData = std::find(transport.Operations.rbegin(), transport.Operations.rend(), std::string{ "read_output:data" }).base();
		expect(closed >= afterLastData, "App Server stdin closes only after terminal output, so EOF cannot cancel the submitted replay turn");
		expect_single_termination(
			transport, "the complete App Server exchange terminates once");

		auto preemptive = fake_transport{ };
		preemptive.Output.push_back({
			.Status = agent_protocol_pipe_read_status::data,
			.Bytes
			= initialized
				+ "{\"id\":1,\"result\":{\"thread\":{\"id\":\"too-early\"}}}\n",
		});
		const auto preemptiveResult
			= run(core::agent_protocol::codex_app_server, preemptive);
		expect(
			preemptiveResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& preemptiveResult.ProtocolFailure
					== application::agent_protocol_failure::causal_violation
				&& preemptive.Writes.size() == 1U,
			"a later App Server phase cannot arrive before its follow-up write");
		expect_single_termination(
			preemptive, "a causal phase violation terminates once");

		auto paddedInitialization = initialized;
		paddedInitialization.insert(
			paddedInitialization.size() - 1U,
			16U * 1024U - paddedInitialization.size(),
			' ');
		auto splitPreemptive   = fake_transport{ };
		splitPreemptive.Output = {
			{
				.Status        = agent_protocol_pipe_read_status::data,
				.Bytes         = std::move(paddedInitialization),
				.BatchComplete = false,
			},
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes
				= "{\"id\":1,\"result\":{\"thread\":{\"id\":\"same-batch\"}}}\n",
			},
		};
		const auto splitPreemptiveResult
			= run(core::agent_protocol::codex_app_server, splitPreemptive);
		expect(
			splitPreemptiveResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& splitPreemptiveResult.ProtocolFailure
					== application::agent_protocol_failure::causal_violation
				&& splitPreemptive.Writes.size() == 1U,
			"a split availability batch is decoded before any follow-up write");
		expect_single_termination(
			splitPreemptive, "a split-batch causal violation terminates once");
	}

	void test_partial_and_oversized_stdout()
	{
		const auto transcript = std::string{
			"{\"type\":\"thread.started\",\"thread_id\":\"thread-7\"}\n"
			"{\"type\":\"turn.started\"}\n"
			"{\"type\":\"item.completed\",\"item\":{\"id\":\"item-1\","
			"\"type\":\"agent_message\",\"text\":\"split-output\"}}\n"
			"{\"type\":\"turn.completed\",\"usage\":{}}\n"
		};
		auto split = fake_transport{ };
		for (const auto& [offset, count] : {
				 std::pair{ std::size_t{ 0U }, std::size_t{ 7U } },
				 std::pair{ std::size_t{ 7U }, std::size_t{ 53U } },
				 std::pair{ std::size_t{ 60U }, transcript.size() - 60U },
			 }) {
			split.Output.push_back({
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = transcript.substr(offset, count),
			});
		}
		split.Output.push_back({ .Status = agent_protocol_pipe_read_status::closed });
		const auto splitResult = run(core::agent_protocol::codex_jsonl_exec, split);
		expect(
			splitResult.Outcome == application::agent_run_outcome::success
				&& splitResult.Events.size() == 1U
				&& splitResult.Events.front().Text == "split-output",
			"structured stdout decodes across arbitrary partial reads");
		expect_single_termination(split, "split stdout terminates once");

		auto       oversized   = fake_transport{ };
		const auto boundedLine = std::string(8U * 1024U - 1U, 'x') + '\n';
		for (auto index = std::size_t{ 0U }; index < 129U; ++index) {
			oversized.Output.push_back({
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = boundedLine,
			});
		}
		const auto oversizedResult
			= run(core::agent_protocol::external_terminal, oversized);
		expect(
			oversizedResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& oversizedResult.ProtocolFailure
					== application::agent_protocol_failure::output_too_large
				&& oversizedResult.Events.empty(),
			"total stdout overflow clears every provisional event");
		expect_single_termination(oversized, "oversized stdout terminates once");
	}

	void test_stderr_and_every_pipe_status()
	{
		auto stderrOverflow = fake_transport{ };
		auto chunk          = std::string{ "BPR_DIAGNOSTIC_PRIVATE_CANARY agent stderr" };
		chunk.resize(8U * 1024U, 'e');
		for (auto index = std::size_t{ 0U }; index < 128U; ++index) {
			stderrOverflow.Error.push_back({
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = chunk,
			});
		}
		stderrOverflow.Error.push_back({
			.Status = agent_protocol_pipe_read_status::data,
			.Bytes  = "e",
		});
		stderrOverflow.Output.push_back({
			.Status = agent_protocol_pipe_read_status::failed,
		});
		const auto stderrResult
			= run(core::agent_protocol::external_terminal, stderrOverflow);
		expect(
			stderrResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& stderrResult.ProtocolFailure
					== application::agent_protocol_failure::output_too_large
				&& stderrResult.StandardError.size()
					== application::maximum_agent_stderr_bytes
				&& stderrOverflow.Output.size() == 1U,
			"stderr stops at its exact ceiling and its first cause wins");
		expect_single_termination(
			stderrOverflow, "oversized stderr terminates once");
		// [[BPR-0886]]. A transport that reads ahead of the loop keeps one byte
		// past each ceiling, so the loop still meets its own refusal.
		expect(
			agent_protocol_read_ahead_limit(agent_protocol_stream::standard_error)
					== application::maximum_agent_stderr_bytes + 1U
				&& agent_protocol_read_ahead_limit(agent_protocol_stream::standard_output)
					== application::maximum_agent_stdout_bytes + 1U,
			"the loop's read-ahead limits are one past its ceilings");

		for (const auto useError : { false, true }) {
			auto  failed = fake_transport{ };
			auto& steps  = useError ? failed.Error : failed.Output;
			steps.push_back({ .Status = agent_protocol_pipe_read_status::failed });
			const auto result
				= run(core::agent_protocol::external_terminal, failed);
			expect(
				result.Outcome == application::agent_run_outcome::protocol_failure
					&& result.ProtocolFailure
						== application::agent_protocol_failure::malformed_frame,
				"failed stdout and stderr reads are protocol failures");
			expect_single_termination(failed, "a failed pipe read terminates once");
		}

		auto closed = fake_transport{ };
		closed.Error.push_back({ .Status = agent_protocol_pipe_read_status::closed });
		closed.Output.push_back({ .Status = agent_protocol_pipe_read_status::closed });
		const auto closedResult
			= run(core::agent_protocol::external_terminal, closed);
		expect(
			closedResult.Outcome == application::agent_run_outcome::success,
			"closed and no-data pipe statuses are non-failures");
		expect_single_termination(closed, "closed pipes terminate once");
	}

	void test_wait_outcomes_and_exit_classification()
	{
		for (const auto& [waitStatus, expected] : {
				 std::pair{ agent_protocol_wait_status::cancelled,
							application::agent_run_outcome::cancelled },
				 std::pair{ agent_protocol_wait_status::timeout,
							application::agent_run_outcome::timeout },
				 std::pair{ agent_protocol_wait_status::failed,
							application::agent_run_outcome::protocol_failure },
			 }) {
			auto transport = fake_transport{ };
			transport.Waits.push_back({ .Status = waitStatus });
			const auto result = run(core::agent_protocol::external_terminal, transport);
			expect(result.Outcome == expected, "wait terminal states remain distinct");
			expect_single_termination(transport, "every wait outcome terminates once");
		}

		auto nonzero = fake_transport{ };
		nonzero.Output.push_back({
			.Status = agent_protocol_pipe_read_status::data,
			.Bytes  = "provisional\n",
		});
		nonzero.Output.push_back({ .Status = agent_protocol_pipe_read_status::no_data });
		nonzero.Waits = {
			{ .Status = agent_protocol_wait_status::running },
			{ .Status = agent_protocol_wait_status::exited, .ExitCode = 7 },
		};
		const auto nonzeroResult
			= run(core::agent_protocol::external_terminal, nonzero);
		expect(
			nonzeroResult.Outcome == application::agent_run_outcome::nonzero_exit
				&& nonzeroResult.ExitCode == 7 && nonzeroResult.Events.empty(),
			"a running-to-exited transition exposes a nonzero code and no events");
		expect_single_termination(nonzero, "nonzero exit terminates once");

		auto abnormal = fake_transport{ };
		abnormal.Waits.push_back({
			.Status         = agent_protocol_wait_status::exited,
			.ExitCode       = 0,
			.ExitedNormally = false,
		});
		const auto abnormalResult
			= run(core::agent_protocol::external_terminal, abnormal);
		expect(
			abnormalResult.Outcome == application::agent_run_outcome::nonzero_exit,
			"an abnormal zero-code exit is still a nonzero-exit outcome");
		expect_single_termination(abnormal, "abnormal exit terminates once");
	}

	/// [[BPR-1827]]. A run says how much it wrote and which line was refused,
	/// and a child that exits with an error mid-line is reported by its exit.
	void test_a_failed_run_keeps_its_evidence()
	{
		const auto init = std::string{ R"({"type":"system","subtype":"init","session_id":"s"})" } + "\n";
		auto       cut  = fake_transport{ };
		cut.Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = init + R"({"type":"assistant","ses)" });
		cut.Error.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = "gateway closed\n" });
		cut.Waits            = { { .Status = agent_protocol_wait_status::exited, .ExitCode = 3 } };
		const auto cutResult = run(core::agent_protocol::claude_stream_json, cut);
		expect(cutResult.Outcome == application::agent_run_outcome::nonzero_exit && cutResult.ExitCode == 3 && !cutResult.ProtocolFailure
				   && cutResult.StandardOutputBytes == init.size() + 24U && cutResult.StandardError == "gateway closed\n",
			   "an error exit mid-line is the exit, with what it wrote counted");

		auto cleanCut = fake_transport{ };
		cleanCut.Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = init + R"({"type":"assistant","ses)" });
		const auto cleanCutResult = run(core::agent_protocol::claude_stream_json, cleanCut);
		expect(cleanCutResult.Outcome == application::agent_run_outcome::protocol_failure && cleanCutResult.ProtocolFailure == application::agent_protocol_failure::incomplete_frame
				   && cleanCutResult.RejectedOutputLine == R"({"type":"assistant","ses)",
			   "a clean exit mid-line is still an incomplete transcript, naming the line");

		auto broken = fake_transport{ };
		broken.Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = init + "{\"type\":\"result\",\"subtype\":7,\"session_id\":\"s\"}\n" });
		const auto brokenResult = run(core::agent_protocol::claude_stream_json, broken);
		expect(brokenResult.Outcome == application::agent_run_outcome::protocol_failure && brokenResult.ProtocolFailure == application::agent_protocol_failure::malformed_frame
				   && brokenResult.RejectedOutputLine == "{\"type\":\"result\",\"subtype\":7,\"session_id\":\"s\"}" && brokenResult.Events.empty(),
			   "a refused line is kept for the log while the run's events are cleared");
	}

	void test_teardown_failure_preserves_the_first_cause()
	{
		struct teardown_case
		{
			std::string_view                                   Name;
			agent_protocol_wait_result                         Wait;
			application::agent_run_outcome                     ExpectedOutcome;
			std::optional<int>                                 ExpectedExitCode;
			std::optional<application::agent_protocol_failure> ExpectedFailure;
		};
		const auto cases = std::vector<teardown_case>{
			{
				.Name            = "clean exit",
				.Wait            = { .Status = agent_protocol_wait_status::exited },
				.ExpectedOutcome = application::agent_run_outcome::protocol_failure,
				.ExpectedFailure
				= application::agent_protocol_failure::malformed_frame,
			},
			{
				.Name = "nonzero exit",
				.Wait = {
					.Status   = agent_protocol_wait_status::exited,
					.ExitCode = 7,
				},
				.ExpectedOutcome  = application::agent_run_outcome::nonzero_exit,
				.ExpectedExitCode = 7,
			},
			{
				.Name            = "cancelled",
				.Wait            = { .Status = agent_protocol_wait_status::cancelled },
				.ExpectedOutcome = application::agent_run_outcome::cancelled,
			},
			{
				.Name            = "timeout",
				.Wait            = { .Status = agent_protocol_wait_status::timeout },
				.ExpectedOutcome = application::agent_run_outcome::timeout,
			},
		};

		for (const auto& test : cases) {
			auto transport            = fake_transport{ };
			transport.TerminateResult = false;
			transport.Output          = {
				{
					.Status = agent_protocol_pipe_read_status::data,
					.Bytes  = "provisional\n",
				},
				{ .Status = agent_protocol_pipe_read_status::no_data },
			};
			transport.Waits.push_back(test.Wait);
			const auto result
				= run(core::agent_protocol::external_terminal, transport);
			expect(
				result.Outcome == test.ExpectedOutcome
					&& result.ExitCode == test.ExpectedExitCode
					&& result.ProtocolFailure == test.ExpectedFailure
					&& result.Events.empty(),
				std::string{ "teardown failure preserves " } + std::string{ test.Name });
			expect_single_termination(
				transport, "a failing teardown is still attempted exactly once and last");
		}
	}

	void test_finish_backpressure_first_cause_and_termination_order()
	{
		auto trailing   = fake_transport{ };
		trailing.Output = {
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = "trailing-text",
			},
			{ .Status = agent_protocol_pipe_read_status::no_data },
		};
		const auto trailingResult = run(
			core::agent_protocol::external_terminal,
			trailing,
			[&](const auto&) { trailing.Operations.emplace_back("sink"); });
		expect(
			trailingResult.Outcome == application::agent_run_outcome::success
				&& trailingResult.Events.size() == 1U
				&& trailingResult.Events.front().Text == "trailing-text",
			"external-terminal trailing bytes publish only through finish");
		expect(
			trailing.Operations.size() >= 2U
				&& trailing.Operations[trailing.Operations.size() - 2U] == "sink",
			"finish publication precedes termination");
		expect_single_termination(
			trailing, "drain, finish, publish, and terminate stay ordered");

		auto incomplete   = fake_transport{ };
		incomplete.Output = {
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = "{",
			},
			{ .Status = agent_protocol_pipe_read_status::closed },
		};
		const auto incompleteResult
			= run(core::agent_protocol::codex_jsonl_exec, incomplete);
		expect(
			incompleteResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& incompleteResult.ProtocolFailure
					== application::agent_protocol_failure::incomplete_frame,
			"structured trailing bytes retain the exact finish failure");
		expect_single_termination(incomplete, "incomplete finish terminates once");

		auto backpressure = fake_transport{ };
		backpressure.Output.push_back({
			.Status = agent_protocol_pipe_read_status::data,
			.Bytes  = "first\nsecond\n",
		});
		auto       sinkCalls          = std::size_t{ 0U };
		const auto backpressureResult = run(
			core::agent_protocol::external_terminal,
			backpressure,
			[&](const auto&) {
				++sinkCalls;
				throw std::runtime_error{ "consumer refused" };
			});
		expect(
			backpressureResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& backpressureResult.Events.empty() && sinkCalls == 1U,
			"synchronous sink refusal stops publication and clears provisional events");
		expect_single_termination(backpressure, "sink refusal terminates once");

		auto firstCause            = fake_transport{ };
		firstCause.TerminateResult = false;
		for (auto index = std::size_t{ 0U }; index < 5U; ++index) {
			firstCause.Output.push_back({
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = std::string(16U * 1024U, 'f'),
			});
		}
		const auto firstCauseResult
			= run(core::agent_protocol::external_terminal, firstCause);
		expect(
			firstCauseResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& firstCauseResult.ProtocolFailure
					== application::agent_protocol_failure::frame_too_large,
			"a later termination failure cannot replace the decoder's first cause");
		expect_single_termination(firstCause, "first-cause failure terminates once");
	}
	void test_a_failed_turn_carries_no_findings()
	{
		const auto payload = std::string{
			"{\"session\":41,\"findings\":[]}"
		};
		auto ok   = fake_transport{ };
		ok.Output = {
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = "{\"type\":\"thread.started\",\"thread_id\":\"t\"}\n"
						  "{\"type\":\"turn.started\"}\n"
						  "{\"type\":\"turn.completed\",\"usage\":{},"
						  "\"bprFindings\":"
					+ payload + "}\n",
			},
			{ .Status = agent_protocol_pipe_read_status::closed },
		};
		const auto accepted = run(core::agent_protocol::codex_jsonl_exec, ok);
		expect(
			accepted.Outcome == application::agent_run_outcome::success
				&& accepted.Findings.has_value()
				&& accepted.Findings->Findings.empty(),
			"a completed turn carries the payload it declared, empty or not");

		auto nonzero   = fake_transport{ };
		nonzero.Output = ok.Output;
		nonzero.Output = {
			{
				.Status = agent_protocol_pipe_read_status::data,
				.Bytes  = "{\"type\":\"thread.started\",\"thread_id\":\"t\"}\n"
						  "{\"type\":\"turn.started\"}\n"
						  "{\"type\":\"turn.completed\",\"usage\":{},"
						  "\"bprFindings\":"
					+ payload + "}\n",
			},
			{ .Status = agent_protocol_pipe_read_status::closed },
		};
		nonzero.Waits = {
			{ .Status = agent_protocol_wait_status::exited, .ExitCode = 3 },
		};
		const auto refused
			= run(core::agent_protocol::codex_jsonl_exec, nonzero);
		expect(
			refused.Outcome == application::agent_run_outcome::nonzero_exit
				&& !refused.Findings.has_value(),
			"a run that did not succeed reports no findings of its own");
		expect_single_termination(nonzero, "a payload-bearing failure terminates once");
	}

	void test_run_metadata_and_input_admission()
	{
		auto launched        = request(core::agent_protocol::claude_stream_json);
		launched.Run         = core::agent_run_id{ 71 };
		const auto init      = std::string{ R"({"type":"system","subtype":"init","session_id":"session","model":"initial","claude_code_version":"2.0"})"
											"\n" };
		const auto body      = std::string{ R"({"type":"assistant","session_id":"session","message":{"role":"assistant","model":"served","content":[{"type":"text","text":"response"}]}})"
											"\n" };
		const auto terminal  = std::string{ R"({"type":"result","session_id":"session","subtype":"success","is_error":false,"result":"response"})"
											"\n" };
		auto       transport = fake_transport{ };
		transport.Output     = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = init + body + terminal } };
		const auto result    = run_agent_protocol_loop(launched, { }, transport);
		expect(result.Outcome == application::agent_run_outcome::success && result.Run == launched.Run
				   && result.Events.size() == 1 && result.Events[0].Run == launched.Run && result.Metadata.ProtocolReported.size() == 2
				   && result.ProvenanceConflicts.size() == 1,
			   "protocol loop delivers exact run and conflicting reports");
		for (const auto status : { agent_protocol_wait_status::cancelled, agent_protocol_wait_status::timeout }) {
			auto stopped       = fake_transport{ };
			stopped.Output     = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = init } };
			stopped.Waits      = { { .Status = status } };
			const auto partial = run_agent_protocol_loop(launched, { }, stopped);
			expect(partial.Run == launched.Run && partial.Metadata.ProtocolReported.size() == 1 && partial.Events.empty()
					   && partial.Outcome != application::agent_run_outcome::success,
				   "cancelled/timeout retains admitted facts without complete response");
		}
		auto oversized       = request(core::agent_protocol::external_terminal);
		oversized.Input      = std::string(application::maximum_agent_input_bytes + 1, 'x');
		auto       untouched = fake_transport{ };
		const auto refused   = run_agent_protocol_loop(oversized, { }, untouched);
		expect(refused.ProtocolFailure == application::agent_protocol_failure::input_too_large && untouched.Writes.empty(), "oversized input makes zero writes");
		auto invalid                     = request(core::agent_protocol::codex_app_server);
		invalid.Metadata.Requested.Model = std::string(257, 'x');
		auto       unstarted             = fake_transport{ };
		const auto invalidResult         = run_agent_protocol_loop(invalid, { }, unstarted);
		expect(invalidResult.ProtocolFailure == application::agent_protocol_failure::invalid_runtime_metadata && unstarted.Writes.empty()
				   && invalidResult.Metadata == core::agent_runtime_metadata{ },
			   "invalid launch facts are refused without copying into result");
		auto late   = fake_transport{ };
		late.Output = {
			{ .Status = agent_protocol_pipe_read_status::data, .Bytes = "{\"id\":0,\"result\":{}}\n" },
			{ .Status = agent_protocol_pipe_read_status::no_data },
			{ .Status = agent_protocol_pipe_read_status::data, .Bytes = R"({"id":1,"result":{"thread":{"id":")" + std::string(1025, 'x') + "\"}}}\n" }
		};
		late.Waits           = { { .Status = agent_protocol_wait_status::running } };
		const auto badThread = run_agent_protocol_loop(request(core::agent_protocol::codex_app_server), { }, late);
		expect(badThread.ProtocolFailure == application::agent_protocol_failure::invalid_runtime_metadata && late.Writes.size() == 2,
			   "invalid late thread permits handshake but no private context write");
	}

	void test_complete_assistant_items_and_claims()
	{
		using namespace application;
		using core::agent_protocol;
		const auto     prefix = std::string{ R"({"type":"thread.started","thread_id":"t"})" } + "\n" + R"({"type":"turn.started"})" + "\n";
		const auto     item   = std::string{ R"({"type":"item.completed","item":{"id":"a","type":"agent_message","text":"first\r\n"}})" } + "\n";
		const auto     second = std::string{ R"({"type":"item.completed","item":{"id":"b","type":"agent_message","text":"second"}})" } + "\n";
		const auto     end    = std::string{ R"({"type":"turn.completed","usage":{}})" } + "\n";
		fake_transport jsonl;
		jsonl.Output = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = prefix + item + second + item + end } };
		auto result  = run(agent_protocol::codex_jsonl_exec, jsonl);
		expect(result.Outcome == agent_run_outcome::success && result.AssistantItems == std::vector<std::string>{ "first\r\n", "second" } && result.Events.size() == 2, "duplicate item coalesces while full ordered body boundaries persist");
		fake_transport conflict;
		conflict.Output = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = prefix + item + R"({"type":"item.completed","item":{"id":"a","type":"agent_message","text":"conflict"}})" + "\n" + end } };
		expect(run(agent_protocol::codex_jsonl_exec, conflict).Outcome == agent_run_outcome::protocol_failure, "same item with different body refuses complete output");
		fake_transport claude;
		const auto     message = std::string{ R"({"type":"assistant","session_id":"s","message":{"id":"m","role":"assistant","content":[{"type":"text","text":"one"},{"type":"text","text":"two"}]}})" } + "\n";
		claude.Output          = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = std::string{ R"({"type":"system","subtype":"init","session_id":"s"})" } + "\n" + message + message + R"({"type":"result","subtype":"success","session_id":"s","is_error":false,"result":"summary must not repeat"})" + "\n" } };
		result                 = run(agent_protocol::claude_stream_json, claude);
		expect(result.Outcome == agent_run_outcome::success && result.AssistantItems == std::vector<std::string>{ "one", "two" }, "Claude keeps assistant blocks once and excludes result summary");
		expect(!result.ReportedCostUsd, "a result frame that names no cost reports none");
		// [[BPR-1753]], owner look v002. The turn's cost, as Claude
		// writes it in the result frame, and nothing that is not a plain decimal.
		fake_transport costed;
		costed.Output = { { .Status = agent_protocol_pipe_read_status::data,
							.Bytes  = std::string{ R"({"type":"system","subtype":"init","session_id":"s"})" } + "\n" + message
								+ R"({"type":"result","subtype":"success","session_id":"s","is_error":false,"result":"done","total_cost_usd":0.0423})" + "\n" } };
		result        = run(agent_protocol::claude_stream_json, costed);
		expect(result.Outcome == agent_run_outcome::success && result.ReportedCostUsd == "0.0423", "Claude's reported cost reaches the run result");
		fake_transport signedCost;
		signedCost.Output = { { .Status = agent_protocol_pipe_read_status::data,
								.Bytes  = std::string{ R"({"type":"system","subtype":"init","session_id":"s"})" } + "\n" + message
									+ R"({"type":"result","subtype":"success","session_id":"s","is_error":false,"result":"done","total_cost_usd":-1e3})" + "\n" } };
		result            = run(agent_protocol::claude_stream_json, signedCost);
		expect(result.Outcome == agent_run_outcome::success && !result.ReportedCostUsd, "a cost that is not a plain decimal is left out");
		agent_protocol_decoder app{ agent_protocol::codex_app_server };
		expect(app.push("{\"id\":0,\"result\":{}}\n").has_value() && app.acknowledge_followup(agent_protocol_event_kind::initialization_complete).has_value(), "App Server initialization");
		expect(app.push("{\"id\":1,\"result\":{\"thread\":{\"id\":\"t\"}}}\n").has_value() && app.acknowledge_followup(agent_protocol_event_kind::context_channel_ready).has_value(), "App Server thread");
		expect(app.push("{\"id\":2,\"result\":{\"turn\":{\"id\":\"v\",\"status\":\"inProgress\",\"items\":[]}}}\n").has_value(), "App Server turn");
		const auto delta     = std::string{ R"({"method":"item/agentMessage/delta","params":{"threadId":"t","turnId":"v","itemId":"a","delta":"part"}})" } + "\n";
		const auto completed = std::string{ R"({"method":"item/completed","params":{"threadId":"t","turnId":"v","item":{"id":"a","type":"agentMessage","text":"part"}}})" } + "\n";
		expect(app.push(delta + completed + completed).has_value(), "App Server deltas agree with deduplicated complete item");
		expect(app.push(std::string{ R"({"method":"turn/completed","params":{"threadId":"t","turn":{"id":"v","status":"completed","items":[]}}})" } + "\n").has_value() && app.finish().has_value() && app.assistant_items() == std::vector<std::string>{ "part" }, "App Server complete bytes contain no doubled delta or item");
		fake_transport terminal;
		terminal.Output = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = "first\r\nsecond\ntrailing" } };
		result          = run(agent_protocol::external_terminal, terminal);
		expect(result.Outcome == agent_run_outcome::success && result.AssistantItems == std::vector<std::string>{ "first\r\nsecond\ntrailing" }, "terminal preserves exact newline and trailing bytes");
		struct claim final : agent_input_claim
		{
			bool     Allowed  = true;
			unsigned Releases = 0;
			auto     permitted() const noexcept -> bool override { return Allowed; }
			void     release() noexcept override
			{
				++Releases;
				Allowed = false;
			}
		};
		auto held                      = std::make_shared<claim>();
		held->Allowed                  = false;
		auto requestValue              = request(agent_protocol::codex_jsonl_exec);
		requestValue.PrivateInputClaim = held;
		fake_transport revoked;
		result = run_agent_protocol_loop(requestValue, { }, revoked);
		expect(result.Outcome == agent_run_outcome::cancelled && revoked.Writes.empty() && held->Releases > 0, "revoked claim refuses initial private input");
		held                           = std::make_shared<claim>();
		held->Allowed                  = false;
		requestValue                   = request(agent_protocol::codex_app_server);
		requestValue.PrivateInputClaim = held;
		fake_transport deferred;
		deferred.Output = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = "{\"id\":0,\"result\":{}}\n" }, { .Status = agent_protocol_pipe_read_status::no_data }, { .Status = agent_protocol_pipe_read_status::data, .Bytes = "{\"id\":1,\"result\":{\"thread\":{\"id\":\"t\"}}}\n" } };
		deferred.Waits  = { { .Status = agent_protocol_wait_status::running } };
		result          = run_agent_protocol_loop(requestValue, { }, deferred);
		expect(result.Outcome == agent_run_outcome::cancelled && deferred.Writes.size() == 2 && held->Releases > 0, "revoked App Server claim permits handshake but no deferred private write");
	}

	/// Owner look v006 ("Codex is totally missing its model settings"). A
	/// catalog run asks Codex's App Server for `model/list` after its
	/// handshake, reads past the notifications Codex 0.163 sends between, and
	/// closes the input once the list is in. Frames as that Codex wrote them.
	void test_a_catalog_run_lists_the_harness_models()
	{
		auto transport   = fake_transport{ };
		transport.Output = {
			{ .Status = agent_protocol_pipe_read_status::data,
			  .Bytes  = "{\"id\":0,\"result\":{\"userAgent\":\"codex/0.163.0\"}}\n{\"method\":\"remoteControl/status/changed\",\"params\":{}}\n" },
			{ .Status = agent_protocol_pipe_read_status::no_data },
			{ .Status = agent_protocol_pipe_read_status::data,
			  .Bytes  = "{\"method\":\"account/updated\",\"params\":{}}\n"
						"{\"id\":1,\"result\":{\"data\":["
						"{\"id\":\"gpt-6.1-sol\",\"model\":\"gpt-6.1-sol\",\"displayName\":\"GPT-6.1-Sol\",\"description\":\"\",\"hidden\":false,\"isDefault\":true,\"defaultReasoningEffort\":\"low\","
						"\"supportedReasoningEfforts\":[{\"reasoningEffort\":\"low\",\"description\":\"\"},{\"reasoningEffort\":\"medium\",\"description\":\"\"},{\"reasoningEffort\":\"high\",\"description\":\"\"},"
						"{\"reasoningEffort\":\"xhigh\",\"description\":\"\"},{\"reasoningEffort\":\"max\",\"description\":\"\"},{\"reasoningEffort\":\"ultra\",\"description\":\"\"}]},"
						"{\"id\":\"gpt-6-luna\",\"model\":\"gpt-6-luna\",\"displayName\":\"GPT-6-Luna\",\"description\":\"\",\"hidden\":false,\"isDefault\":false,\"defaultReasoningEffort\":\"medium\","
						"\"supportedReasoningEfforts\":[{\"reasoningEffort\":\"low\",\"description\":\"\"},{\"reasoningEffort\":\"max\",\"description\":\"\"}]},"
						"{\"id\":\"internal\",\"model\":\"internal\",\"displayName\":\"Internal\",\"description\":\"\",\"hidden\":true,\"isDefault\":false,\"defaultReasoningEffort\":\"low\",\"supportedReasoningEfforts\":[]}"
						"],\"nextCursor\":null}}\n" },
			{ .Status = agent_protocol_pipe_read_status::no_data },
		};
		transport.Waits   = { { .Status = agent_protocol_wait_status::running }, { .Status = agent_protocol_wait_status::running }, { .Status = agent_protocol_wait_status::exited } };
		auto catalog      = application::agent_launch_request{ .Protocol = core::agent_protocol::codex_app_server, .Session = core::agent_session_id{ 1U }, .Input = "initialize\n" };
		catalog.Purpose   = application::agent_launch_purpose::model_catalog;
		const auto result = run_agent_protocol_loop(catalog, { }, transport);
		expect(result.Outcome == application::agent_run_outcome::success && result.ModelCatalog.has_value() && result.Events.empty(), "a catalog run succeeds with the list and no output");
		expect(transport.Writes.size() == 2U && transport.Writes[1U].find("\"method\":\"initialized\"") != std::string::npos && transport.Writes[1U].find("\"method\":\"model/list\"") != std::string::npos
				   && transport.Writes[1U].find("thread/start") == std::string::npos && transport.CloseInputCount == 1U,
			   "after the handshake it asks for the models alone, and closes the input once they are in");
		const auto models = application::decode_agent_model_catalog(result);
		expect(models.size() == 2U && models[0].Model == "gpt-6.1-sol" && models[0].DisplayName == "GPT-6.1-Sol" && models[0].IsDefault && models[0].DefaultEffort == "low"
				   && models[0].Efforts == std::vector<std::string>{ "low", "medium", "high", "xhigh", "max", "ultra" } && models[1].Efforts == std::vector<std::string>{ "low", "max" },
			   "the list is the harness's own: names as it shows them, each model's efforts with max and ultra, hidden models left out");
		expect_single_termination(transport, "a catalog run terminates once");

		// An answer that is an error lists nothing.
		auto refused      = fake_transport{ };
		refused.Output    = { { .Status = agent_protocol_pipe_read_status::data, .Bytes = "{\"id\":0,\"result\":{}}\n" },
							  { .Status = agent_protocol_pipe_read_status::no_data },
							  { .Status = agent_protocol_pipe_read_status::data, .Bytes = "{\"id\":1,\"error\":{\"code\":-1,\"message\":\"no\"}}\n" } };
		const auto failed = run_agent_protocol_loop(catalog, { }, refused);
		expect(failed.Outcome != application::agent_run_outcome::success && application::decode_agent_model_catalog(failed).empty(), "a refused list offers no models");
	}

	/// Owner look v006: Claude Code's effort levels as its own `--help` lists them.
	void test_claude_help_names_its_efforts()
	{
		auto help         = application::agent_run_result{ .Outcome = application::agent_run_outcome::success };
		help.Events       = { { .Text = "  --effort <level>                      Effort level for the current session\n" }, { .Text = "                                        (low, medium, high, xhigh, max)\n" } };
		const auto levels = application::decode_agent_help_efforts(help);
		expect(levels == std::vector<std::string>{ "low", "medium", "high", "xhigh", "max" }, "the levels are read from --help, in its order");
		help.Events = { { .Text = "  --model <model>   Model for the current session (e.g. 'sonnet')\n" } };
		expect(application::decode_agent_help_efforts(help).empty(), "a help without --effort names none");
	}
} // namespace

namespace {
	void test_native_idle_output_bound_and_no_inference()
	{
		class connection final : public application::agent_native_connection_port
		{
		public:
			std::size_t Completed = 0U, NextCalls = 0U;
			auto        next() -> std::optional<application::agent_launch_request> override
			{
				++NextCalls;
				return std::nullopt;
			}
			void complete(application::agent_run_result) override { ++Completed; }
			auto timeout_milliseconds() const -> int override { return 1000; }
			auto cancelled() const noexcept -> bool override { return false; }
		};
		for (const auto extra : { 0U, 1U }) {
			auto port                   = connection{ };
			auto launched               = request(core::agent_protocol::claude_stream_json);
			launched.NativeConversation = "idle-bound";
			launched.NativeConnection   = &port;
			auto wire                   = fake_transport{ };
			wire.Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = R"({"type":"system","subtype":"init","session_id":"idle"})"
																							  "\n"
																							  R"({"type":"assistant","session_id":"idle","message":{"role":"assistant","id":"one","content":[{"type":"text","text":"complete"}]}})"
																							  "\n"
																							  R"({"type":"result","subtype":"success","session_id":"idle","is_error":false,"result":"done"})"
																							  "\n" });
			wire.Output.push_back({ });
			for (std::size_t chunk = 0; chunk < 16U; ++chunk) {
				wire.Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = chunk == 15U ? std::string(4095U, ' ') + "\n" : std::string(4096U, ' ') });
			}
			if (extra) {
				wire.Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = " " });
			}
			const auto result = run_agent_protocol_loop(launched, { }, wire);
			if (result.Outcome != application::agent_run_outcome::success) {
				std::cerr << "idle-bound outcome=" << application::name(result.Outcome) << " protocol=" << (result.ProtocolFailure ? application::name(*result.ProtocolFailure) : "none") << " completed=" << port.Completed << " writes=" << wire.Writes.size() << " terminated=" << wire.TerminateCount << "\n";
			}
			expect(result.Outcome == application::agent_run_outcome::success && port.Completed == 1U && wire.Writes.size() == 1U && wire.TerminateCount == 1U, "idle output cannot generate another question or alter the completed result");
			expect(port.NextCalls == (extra ? 0U : 1U), "exact64KiB idle output fits; first extra byte closes before another turn can be selected");
		}
	}

	void test_connected_turns_retain_claude_frame_fences()
	{
		const auto message = [](std::string_view aId, std::string_view aText) {
			return std::string{ R"({"type":"assistant","session_id":"same-native","message":{"role":"assistant","id":")" } + std::string{ aId } + R"(","content":[{"type":"text","text":")" + std::string{ aText } + "\"}]}}\n";
		};
		const auto result = [](std::string_view aUuid) {
			return std::string{ R"({"type":"result","subtype":"success","session_id":"same-native","is_error":false,"result":"done","uuid":")" } + std::string{ aUuid } + "\"}\n";
		};
		for (const auto scenario : { 0U, 1U, 2U, 3U, 4U, 5U }) {
			class connection final : public application::agent_native_connection_port
			{
			public:
				fake_transport*                            Wire{ };
				std::string                                Followup;
				std::vector<application::agent_run_result> Results;
				auto                                       next() -> std::optional<application::agent_launch_request> override
				{
					if (Results.size() != 1U) {
						return std::nullopt;
					}
					// This arrives after idle() and only once the next question is sent.
					Wire->Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = Followup });
					auto next               = request(core::agent_protocol::claude_stream_json);
					next.NativeConversation = "same-conversation";
					next.NativeTurn         = 3U;
					next.Input              = "second question";
					return next;
				}
				void complete(application::agent_run_result aResult) override { Results.push_back(std::move(aResult)); }
				auto timeout_milliseconds() const -> int override { return 1000; }
				auto cancelled() const noexcept -> bool override { return Results.size() == 2U; }
			} port;
			auto wire       = fake_transport{ };
			port.Wire       = &wire;
			auto oldMessage = message("old-message", "OLD ANSWER");
			if (scenario == 4U) {
				oldMessage.erase(oldMessage.find("\"id\":\"old-message\","), std::string_view{ "\"id\":\"old-message\"," }.size());
			}
			if (scenario == 5U) {
				oldMessage = std::string{ R"({"type":"assistant","session_id":"same-native","message":{"role":"assistant","id":"old-tool","content":[{"type":"tool_use","id":"old-use","name":"Read","input":{}}]}})" } + "\n";
			}
			port.Followup = scenario == 0U         ? message("fresh-message", "NEW ANSWER") + result("new-result")
				: scenario == 1U || scenario >= 4U ? oldMessage + result("old-result")
				: scenario == 2U                   ? message("fresh-message", "NEW ANSWER") + result("old-result")
												   : result("old-result");
			wire.Output.push_back({ .Status = agent_protocol_pipe_read_status::data, .Bytes = std::string{ R"({"type":"system","subtype":"init","session_id":"same-native"})" } + "\n" + oldMessage + result("old-result") });
			auto initial               = request(core::agent_protocol::claude_stream_json);
			initial.NativeConversation = "same-conversation";
			initial.NativeConnection   = &port;
			static_cast<void>(run_agent_protocol_loop(initial, { }, wire));
			expect(wire.Writes.size() == 2U && port.Results.size() == 2U, "production connection sends each explicit question once and reports two terminals");
			expect(port.Results.front().Outcome == application::agent_run_outcome::success, "initial turn succeeds");
			expect(scenario == 0U ? port.Results.back().Outcome == application::agent_run_outcome::success && port.Results.back().AssistantItems == std::vector<std::string>{ "NEW ANSWER" }
								  : port.Results.back().Outcome == application::agent_run_outcome::protocol_failure,
				   "same-session late assistant/result frames cannot complete a later turn");
			expect(wire.CloseInputCount == 0U, "native connection keeps input open through its normal turns");
		}
	}

	void test_connection_identity_capacity_never_forgets_old_turns()
	{
		auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		expect(decoder.push("{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"capacity\"}\n").has_value(), "capacity connection initializes");
		const auto message = [](std::size_t aId) { return std::string{ R"({"type":"assistant","session_id":"capacity","message":{"role":"assistant","id":")" } + std::to_string(aId) + R"(","content":[]}})" + "\n"; };
		for (std::size_t index = 0U; index < 4096U; ++index) {
			expect(decoder.push(message(index)).has_value(), "bounded identities fit without retaining tool text as assistant output");
		}
		expect(decoder.push("{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"capacity\",\"is_error\":false,\"result\":\"done\"}\n").has_value() && decoder.turn_complete(), "full identity registry still completes its admitted turn");
		decoder.continue_native("capacity", 3U);
		auto duplicate = decoder;
		expect(!duplicate.push(message(0U)), "the oldest tool-only identity still fences the next turn at capacity");
		const auto exhausted = decoder.push(message(4096U));
		expect(!exhausted && exhausted.error() == application::agent_protocol_failure::output_too_large, "one extra identity refuses without evicting an earlier fence");
	}

	void test_native_turns_leave_the_stream_open_and_fence_the_prior_turn()
	{
		auto first               = request(core::agent_protocol::claude_stream_json);
		first.NativeConversation = "durable-conversation";
		auto savedHandle         = std::string{ };
		first.SaveNativeHandle   = std::make_shared<std::function<bool(const std::string&)>>([&savedHandle](const std::string& aHandle) { savedHandle = aHandle; return true; });
		auto wire                = fake_transport{ };
		wire.Output.push_back({ .Status = agent_protocol_pipe_read_status::data,
								.Bytes  = "{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"native-a\"}\n{\"type\":\"assistant\",\"session_id\":\"native-a\",\"message\":{\"role\":\"assistant\",\"id\":\"one\",\"content\":[{\"type\":\"text\",\"text\":\"first answer\"}]}}\n{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"native-a\",\"is_error\":false,\"result\":\"done\"}\n" });
		const auto firstResult = run_agent_protocol_loop(first, { }, wire);
		if (firstResult.Outcome != application::agent_run_outcome::success) {
			std::cerr << application::name(firstResult.Outcome) << " protocol=" << (firstResult.ProtocolFailure ? application::name(*firstResult.ProtocolFailure) : "none") << " line=" << firstResult.RejectedOutputLine << "\n";
		}
		expect(firstResult.Outcome == application::agent_run_outcome::success && firstResult.NativeHandle == "native-a", "native result ends the first turn and captures its handle");
		expect(wire.CloseInputCount == 0U && wire.TerminateCount == 0U, "native result neither closes input nor terminates its process");
		expect(savedHandle == "native-a", "Claude initialization durably announces its native handle before terminal delivery");

		auto second         = first;
		second.NativeHandle = firstResult.NativeHandle;
		second.NativeTurn   = 3U;
		second              = *application::prepare_native_agent_turn(std::move(second), true);
		auto followup       = fake_transport{ };
		followup.Output.push_back({ .Status = agent_protocol_pipe_read_status::data,
									.Bytes  = "{\"type\":\"assistant\",\"session_id\":\"native-a\",\"message\":{\"role\":\"assistant\",\"id\":\"two\",\"content\":[{\"type\":\"text\",\"text\":\"tool memory absent from replay\"}]}}\n{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"native-a\",\"is_error\":false,\"result\":\"done\"}\n" });
		const auto result = run_agent_protocol_loop(second, { }, followup);
		expect(result.Outcome == application::agent_run_outcome::success && result.NativeHandle == firstResult.NativeHandle, "later turn uses initialized Claude session without another init");
		expect(followup.CloseInputCount == 0U && followup.TerminateCount == 0U, "second turn preserves its connection");
		auto wrong = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		wrong.continue_native("native-a", 3U);
		expect(!wrong.push("{\"type\":\"result\",\"subtype\":\"success\",\"session_id\":\"native-b\",\"is_error\":false,\"result\":\"done\"}\n"), "other native session cannot supply this turn");
		auto late = application::agent_protocol_decoder{ core::agent_protocol::codex_app_server };
		late.continue_native("thread-a", 7U);
		expect(late.push("{\"method\":\"remoteControl/status/changed\",\"params\":{}}\n").has_value(), "installed connection status notification cannot complete a turn");
		expect(!late.push("{\"id\":2,\"result\":{\"turn\":{\"id\":\"old\",\"status\":\"inProgress\"}}}\n"), "late RPC reply cannot initialize a later turn");
		auto reconnect            = second;
		reconnect.NativeConnected = false;
		reconnect.Arguments       = { "-p" };
		reconnect                 = *application::prepare_native_agent_turn(std::move(reconnect), false);
		expect(reconnect.Arguments == std::vector<std::string>{ "-p", "--resume", "native-a" }, "reconnect uses the exact native ID");
	}
}

int main()
{
	test_connection_identity_capacity_never_forgets_old_turns();
	test_connected_turns_retain_claude_frame_fences();
	test_native_turns_leave_the_stream_open_and_fence_the_prior_turn();
	test_native_idle_output_bound_and_no_inference();
	test_a_catalog_run_lists_the_harness_models();
	test_claude_help_names_its_efforts();
	test_complete_assistant_items_and_claims();
	test_run_metadata_and_input_admission();
	test_all_write_statuses_are_classified();
	test_app_server_phases_follow_successful_writes_only();
	test_partial_and_oversized_stdout();
	test_stderr_and_every_pipe_status();
	test_wait_outcomes_and_exit_classification();
	test_a_failed_run_keeps_its_evidence();
	test_teardown_failure_preserves_the_first_cause();
	test_finish_backpressure_first_cause_and_termination_order();
	test_a_failed_turn_carries_no_findings();
	std::cout << "agent-protocol-loop-tests:pass\n";
	return EXIT_SUCCESS;
}
