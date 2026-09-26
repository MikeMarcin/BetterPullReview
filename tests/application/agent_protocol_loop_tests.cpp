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
		std::size_t                             CloseInputCount = 0U;
		std::size_t                             TerminateCount  = 0U;
		bool                                    TerminateResult = true;

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
			.DeferredContext = "selected-context",
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
			"{\"id\":0,\"result\":{}}\n"
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

} // namespace

int main()
{
	test_all_write_statuses_are_classified();
	test_app_server_phases_follow_successful_writes_only();
	test_partial_and_oversized_stdout();
	test_stderr_and_every_pipe_status();
	test_wait_outcomes_and_exit_classification();
	test_teardown_failure_preserves_the_first_cause();
	test_finish_backpressure_first_cause_and_termination_order();
	test_a_failed_turn_carries_no_findings();
	std::cout << "agent-protocol-loop-tests:pass\n";
	return EXIT_SUCCESS;
}
