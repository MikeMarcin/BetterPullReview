/// \file
/// \brief Hostile real-process coverage for agent protocol and teardown policy.

#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/application/agent_launch.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
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

	template <typename Identity>
	[[nodiscard]] constexpr auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto session() -> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 40U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = {
				.Host        = core::git_host_id{ 10U },
				.Repository  = core::repository_id{ 20U },
				.PullRequest = core::pull_request_id{ 30U },
				.BaseOid     = commit(1U),
				.HeadOid     = commit(2U),
				.LeftEpoch   = core::epoch_ordinal{ 1U },
				.RightEpoch  = core::epoch_ordinal{ 2U },
			},
		};
	}

	[[nodiscard]] auto canary_profile(const core::agent_protocol aProtocol)
		-> core::agent_profile
	{
		return {
			.Id          = core::agent_profile_id{ 1U },
			.DisplayName = "Canary",
			.Executable  = BPR_AGENT_CANARY_EXECUTABLE,
			.Protocol    = aProtocol,
			.Workspace
			= core::agent_workspace_policy::disposable_review_workspace,
			.AllowedEnvironment = {
				"BPR_CANARY_AGENT_IDENTITY",
				"BPR_CANARY_ALLOWED",
			},
		};
	}

	[[nodiscard]] auto supplement() -> core::agent_context_supplement
	{
		const auto comparison = identity<core::file_comparison_digest>(5U);
		const auto anchor     = core::source_line_ref{
			.Comparison = comparison,
			.Side       = core::diff_side::right,
			.Line       = 17U,
		};
		const auto segment = identity<core::alignment_segment_id>(6U);
		const auto group   = identity<core::change_group_id>(7U);
		const auto built   = core::build_agent_supplement({
			.ApplicationSession = core::application_session_id{ 11U },
			.Session            = session(),
			.PullRequestUrl     = "https://github.example/octo/app/pull/30",
			.PreviousUpload     = commit(3U),
			.LastReviewedHead   = commit(4U),
			.Files              = {
				{
					.File               = identity<core::file_identity>(3U),
					.Material           = identity<core::file_material_id>(4U),
					.Path               = "src/retry.cpp",
					.Read               = core::file_read_state::read,
					.ChangedSinceReview = true,
					.Accepted           = true,
				},
			},
			.Defects = {
				{
					.File         = identity<core::file_identity>(3U),
					.Conversation = core::review_conversation_id{ 70U },
					.History      = {
						.Defect = {
							.Id       = core::defect_id{ 80U },
							.Anchor   = anchor,
							.Severity = core::defect_severity::critical,
							.Category = core::defect_category::security,
							.State    = core::defect_state::verified,
						},
						.OriginalAnchor = anchor,
					},
				},
			},
			.Selection = core::supplement_semantic_context{
				.File      = identity<core::file_identity>(3U),
				.Selection = {
					.Comparison    = comparison,
					.Kind          = core::semantic_selection_kind::group,
					.PreferredLine = anchor,
					.Segment       = segment,
					.Group         = group,
				},
				.Confidence = core::supplement_mapping_confidence::similar,
				.Segments   = {
					{
						.Id    = segment,
						.Kind  = core::alignment_kind::replacement,
						.Left  = { 16U, 18U },
						.Right = { 16U, 19U },
						.Group = group,
					},
				},
				.Groups = {
					{
						.Id      = group,
						.Members = { 0U, 1U },
						.Left    = { 16U, 18U },
						.Right   = { 16U, 19U },
						.First   = { 16U, 16U },
						.Last    = { 18U, 19U },
					},
				},
				.SelectedSourceText = std::string{ "return retry();" },
			},
			.SelectedNotes = {
				{
					.File       = identity<core::file_identity>(3U),
					.Comparison = comparison,
					.Text       = "check retry exhaustion",
				},
			},
			.SelectedDrafts = {
				{ .Anchor = anchor, .Text = "Would a bounded retry be safer?" },
			},
		});
		expect(built.has_value(), "the hostile fixture supplement builds");
		return *built;
	}

	[[nodiscard]] auto request(const core::agent_protocol aProtocol)
		-> application::agent_launch_request
	{
		auto built = application::build_agent_launch(
			canary_profile(aProtocol),
			supplement(),
			[](const std::string& aName) -> std::optional<std::string> {
				return qEnvironmentVariableIsSet(aName.c_str())
					? std::optional{
						  qEnvironmentVariable(aName.c_str()).toStdString()
					  }
					: std::nullopt;
			});
		expect(built.has_value(), "the matching canary request builds");
		built->WorkingDirectory = std::filesystem::current_path().string();
		return *built;
	}

	[[nodiscard]] auto event_values(
		const application::agent_run_result& aResult, const std::string_view aTag)
		-> std::vector<std::string>
	{
		auto values = std::vector<std::string>{ };
		for (const auto& event : aResult.Events) {
			if (event.Text.starts_with(std::string{ aTag } + "\t")) {
				values.push_back(event.Text.substr(aTag.size() + 1U));
			}
		}
		return values;
	}

	void plant_parent_secrets()
	{
		for (const auto& [name, value] : {
				 std::pair{ "GITHUB_TOKEN", "ghp_the_reviewers_own_authority" },
				 std::pair{ "AWS_SECRET_ACCESS_KEY", "aws_secret_value" },
				 std::pair{ "ANTHROPIC_API_KEY", "sk-ant-secret" },
			 }) {
			qputenv(name, QByteArray{ value });
			expect(
				qEnvironmentVariableIsSet(name),
				"the parent really holds each planted credential");
		}
	}

	void test_all_four_protocols_stream_partial_frames_under_stderr_pressure()
	{
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto protocols = std::array{
			core::agent_protocol::codex_app_server,
			core::agent_protocol::codex_jsonl_exec,
			core::agent_protocol::claude_stream_json,
			core::agent_protocol::external_terminal,
		};
		for (const auto protocol : protocols) {
			auto launched = request(protocol);
			launched.Arguments.emplace_back("--stderr-pressure");
			auto streamed     = std::vector<application::agent_stream_event>{ };
			auto firstEventAt = std::optional<qint64>{ };
			auto timer        = QElapsedTimer{ };
			timer.start();
			auto       launcher = adapters::agent::agent_process_launcher{ };
			const auto result   = launcher.run(
				launched,
				20'000,
				[&](const application::agent_stream_event& aEvent) {
					if (!firstEventAt) {
						firstEventAt = timer.elapsed();
					}
					streamed.push_back(aEvent);
				});
			const auto finishedAt = timer.elapsed();
			expect(
				result.Outcome == application::agent_run_outcome::success
					&& result.ExitCode == 0,
				"each selected protocol reaches a typed successful terminal");
			expect(
				firstEventAt && *firstEventAt < finishedAt && streamed == result.Events
					&& result.Events.size() > 3U,
				"partial stdout frames publish incrementally before process completion");
			expect(
				result.StandardError.size() == 256U * 1024U,
				"stderr pressure is drained concurrently and retained within its bound");
			const auto complete = std::ranges::find_if(
				result.Events,
				[](const application::agent_stream_event& aEvent) {
					return aEvent.Text == "canary-complete";
				});
			expect(
				complete != result.Events.end(),
				"each protocol preserves its framing through hostile partial reads");
			for (auto index = std::size_t{ 0U }; index < result.Events.size(); ++index) {
				expect(
					result.Events[index].Session == session().Id
						&& result.Events[index].Sequence == index,
					"every streamed frame carries exact ordered session identity");
			}
		}
	}

	void test_real_child_receives_no_parent_credential_or_context_in_argv()
	{
		plant_parent_secrets();
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		auto       launched         = request(core::agent_protocol::codex_jsonl_exec);
		const auto workingDirectory = std::filesystem::temp_directory_path()
			/ ("bpr-agent-cwd-"
			   + std::to_string(
				   std::chrono::steady_clock::now().time_since_epoch().count()));
		std::error_code filesystemError;
		expect(
			std::filesystem::create_directory(workingDirectory, filesystemError)
				&& !filesystemError,
			"the canary receives a dedicated accepted working directory");
		launched.WorkingDirectory = workingDirectory.string();
		auto       launcher       = adapters::agent::agent_process_launcher{ };
		const auto result         = launcher.run(launched, 20'000);
		static_cast<void>(std::filesystem::remove(workingDirectory, filesystemError));
		expect(
			result.Outcome == application::agent_run_outcome::success,
			"the boundary canary succeeds");

		auto environment = event_values(result, "env");
		auto names       = std::vector<std::string>{ };
		for (const auto& entry : environment) {
			names.push_back(entry.substr(0U, entry.find('=')));
		}
		std::ranges::sort(names);
		expect(
			names
				== std::vector<std::string>{
					"BPR_CANARY_AGENT_IDENTITY", "BPR_CANARY_ALLOWED" },
			"the child's complete environment equals the profile allowlist");
		for (const auto secret :
			 { "GITHUB_TOKEN", "AWS_SECRET_ACCESS_KEY", "ANTHROPIC_API_KEY" }) {
			expect(
				qEnvironmentVariableIsSet(secret)
					&& std::ranges::find(names, secret) == names.end(),
				"each credential exists only in the parent");
		}

		const auto arguments = event_values(result, "argv");
		expect(
			arguments.size() == 4U && arguments.at(1) == "exec"
				&& arguments.at(2) == "--json" && arguments.at(3) == "-",
			"the child reports only its executable and exact protocol switches");
		for (const auto& argument : arguments) {
			expect(
				argument.find("github.example") == std::string::npos
					&& argument.find("retry exhaustion") == std::string::npos,
				"PR identity and private context never reach argv");
		}
		const auto childWorkingDirectories = event_values(result, "cwd");
		expect(
			childWorkingDirectories.size() == 1U
				&& std::filesystem::path{ childWorkingDirectories.front() }
						.lexically_normal()
					== workingDirectory.lexically_normal(),
			"the child reports the exact request working directory, not the parent cwd");
		const auto input = event_values(result, "stdin");
		expect(
			std::ranges::any_of(input, [](const std::string& aInput) {
				return aInput.find("schema\t2\n") != std::string::npos
					&& aInput.find("pull_request_url\t") != std::string::npos
					&& aInput.find("last_reviewed_head.present\t1")
					!= std::string::npos
					&& aInput.find("file.0.changed_since_review\t1")
					!= std::string::npos
					&& aInput.find("defect.0.id\t80") != std::string::npos
					&& aInput.find("selection.source_text.value\treturn retry();")
					!= std::string::npos
					&& aInput.find("note.0.text\tcheck retry exhaustion")
					!= std::string::npos
					&& aInput.find("draft.0.text\tWould a bounded retry be safer?")
					!= std::string::npos;
			}),
			"the child reads every required local context class only from stdin");
	}

	void test_independent_agent_identity_alone_controls_canary_authority()
	{
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto allowedRequest = request(core::agent_protocol::claude_stream_json);
		auto       launcher       = adapters::agent::agent_process_launcher{ };
		const auto allowed        = launcher.run(allowedRequest, 20'000);
		expect(
			allowed.Outcome == application::agent_run_outcome::success,
			"the independently configured allowed identity succeeds");

		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "denied" });
		const auto deniedRequest = request(core::agent_protocol::claude_stream_json);
		const auto denied        = launcher.run(deniedRequest, 20'000);
		expect(
			denied.Outcome == application::agent_run_outcome::nonzero_exit
				&& denied.ExitCode == 23,
			"changing only the agent's own synthetic identity changes its authority outcome");
		expect(
			allowedRequest.Arguments == deniedRequest.Arguments
				&& allowedRequest.Input == deniedRequest.Input,
			"BetterPullReview context and argv do not carry or substitute that authority");
	}

	void test_every_terminal_condition_is_typed_and_failed_runs_publish_no_result()
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		auto launcher = adapters::agent::agent_process_launcher{ };

		auto unmaterialized = request(core::agent_protocol::external_terminal);
		unmaterialized.WorkingDirectory.clear();
		const auto refusedDirectory = launcher.run(unmaterialized, 2'000);
		expect(
			refusedDirectory.Outcome
					== application::agent_run_outcome::failed_to_start
				&& !refusedDirectory.StandardError.empty()
				&& refusedDirectory.Events.empty(),
			"the launcher refuses before start when no materialized working directory was accepted");

		auto failedStart       = request(core::agent_protocol::external_terminal);
		failedStart.Executable = "/definitely/not/a/betterpullreview-agent";
		const auto failed      = launcher.run(failedStart, 2'000);
		expect(
			failed.Outcome == application::agent_run_outcome::failed_to_start,
			"a missing executable is failed-to-start, not cancellation");

		auto nonzero = request(core::agent_protocol::codex_jsonl_exec);
		nonzero.Arguments.emplace_back("--nonzero-exit");
		const auto nonzeroResult = launcher.run(nonzero, 20'000);
		expect(
			nonzeroResult.Outcome == application::agent_run_outcome::nonzero_exit
				&& nonzeroResult.ExitCode == 17 && nonzeroResult.Events.empty(),
			"a nonzero child exit is typed and does not become a successful answer");

		auto malformed = request(core::agent_protocol::codex_jsonl_exec);
		malformed.Arguments.emplace_back("--malformed-frame");
		const auto malformedResult = launcher.run(malformed, 20'000);
		expect(
			malformedResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& malformedResult.Events.empty()
				&& malformedResult.ProtocolFailure
					== application::agent_protocol_failure::malformed_frame,
			"a malformed structured frame is a typed protocol failure");

		auto malformedJson = request(core::agent_protocol::codex_jsonl_exec);
		malformedJson.Arguments.emplace_back("--malformed-json");
		const auto malformedJsonResult = launcher.run(malformedJson, 20'000);
		expect(
			malformedJsonResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& malformedJsonResult.ProtocolFailure
					== application::agent_protocol_failure::malformed_frame,
			"a real child cannot disguise malformed JSON as a known event");

		auto preemptive = request(core::agent_protocol::codex_app_server);
		preemptive.Arguments.emplace_back("--preemptive-app-server");
		const auto preemptiveResult = launcher.run(preemptive, 20'000);
		expect(
			preemptiveResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& preemptiveResult.ProtocolFailure
					== application::agent_protocol_failure::causal_violation
				&& preemptiveResult.Events.empty(),
			"a child that emits the whole App Server exchange before reading follow-ups is refused");

		for (const auto& [statusSwitch, expectedFailure] : {
				 std::pair{
					 std::string_view{ "--remote-failed" },
					 application::agent_protocol_failure::remote_failed },
				 std::pair{
					 std::string_view{ "--remote-interrupted" },
					 application::agent_protocol_failure::remote_interrupted },
			 }) {
			auto remote = request(core::agent_protocol::codex_app_server);
			remote.Arguments.emplace_back(statusSwitch);
			const auto remoteResult = launcher.run(remote, 20'000);
			expect(
				remoteResult.Outcome
						== application::agent_run_outcome::protocol_failure
					&& remoteResult.ProtocolFailure == expectedFailure
					&& remoteResult.Events.empty(),
				"App Server terminal status is retained as an exact typed failure");
		}

		auto oversized = request(core::agent_protocol::claude_stream_json);
		oversized.Arguments.emplace_back("--oversize-frame");
		const auto oversizedResult = launcher.run(oversized, 20'000);
		expect(
			oversizedResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& oversizedResult.Events.empty(),
			"an output frame beyond the bound fails without unbounded retention");

		const auto refusedBySink = launcher.run(
			request(core::agent_protocol::external_terminal),
			20'000,
			[](const application::agent_stream_event&) {
				throw std::runtime_error{ "consumer refused output" };
			});
		expect(
			refusedBySink.Outcome
					== application::agent_run_outcome::protocol_failure
				&& refusedBySink.Events.empty(),
			"the synchronous sink applies backpressure and can refuse publication");

		auto cancelled = request(core::agent_protocol::codex_app_server);
		cancelled.Arguments.emplace_back("--wait-forever");
		auto preCancelled = std::stop_source{ };
		static_cast<void>(preCancelled.request_stop());
		const auto preCancelledResult = launcher.run(
			cancelled,
			10'000,
			{ },
			preCancelled.get_token());
		expect(
			preCancelledResult.Outcome
				== application::agent_run_outcome::cancelled,
			"an invocation-specific cancellation cannot be reset before process start");
		auto cancelThread = std::jthread{ [&launcher] {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 150 });
			launcher.cancel();
		} };
		const auto cancelledResult = launcher.run(cancelled, 10'000);
		expect(
			cancelledResult.Outcome == application::agent_run_outcome::cancelled
				&& cancelledResult.Events.empty(),
			"explicit cancellation is distinct and retains no partial final answer");
		cancelThread.join();

		auto timedOut = request(core::agent_protocol::external_terminal);
		timedOut.Arguments.emplace_back("--wait-forever");
		const auto timeoutResult = launcher.run(timedOut, 250);
		expect(
			timeoutResult.Outcome == application::agent_run_outcome::timeout
				&& timeoutResult.Events.empty(),
			"deadline expiry is timeout, not explicit cancellation");

		const auto recovered = launcher.run(
			request(core::agent_protocol::external_terminal), 20'000);
		expect(
			recovered.Outcome == application::agent_run_outcome::success,
			"the same owner launches cleanly after failures, cancellation, and timeout");

		auto moved       = session().Binding;
		moved.HeadOid    = commit(9U);
		const auto stale = application::finalize_agent_run(
			recovered, session(), moved);
		expect(
			stale.Outcome == application::agent_run_outcome::stale_result
				&& stale.Events.empty(),
			"head movement rejects a successful old-revision result as stale");
	}

	void test_timeout_terminates_the_descendant_process_tree()
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto marker = std::filesystem::temp_directory_path()
			/ ("bpr-agent-descendant-"
			   + std::to_string(
				   std::chrono::steady_clock::now().time_since_epoch().count()));
		std::error_code ignored;
		std::filesystem::remove(marker, ignored);

		auto launched = request(core::agent_protocol::external_terminal);
		launched.Arguments.insert(
			launched.Arguments.end(),
			{ "--spawn-descendant", marker.string(), "--wait-forever" });
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(launched, 750);
		expect(
			result.Outcome == application::agent_run_outcome::timeout,
			"the descendant fixture reaches the bounded timeout");
		expect(
			std::filesystem::exists(marker),
			"the descendant wrote its heartbeat, so teardown evidence is non-vacuous");
		const auto stoppedSize = std::filesystem::file_size(marker);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 250 });
		expect(
			std::filesystem::file_size(marker) == stoppedSize,
			"the descendant heartbeat stops with its process group - no child leaks");
		std::filesystem::remove(marker, ignored);
	}

#if defined(Q_OS_WIN) && defined(BPR_BUILD_TESTING)
	void test_windows_assignment_failure_has_no_pre_assignment_side_effect()
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto marker = std::filesystem::temp_directory_path()
			/ ("bpr-agent-pre-assignment-"
			   + std::to_string(
				   std::chrono::steady_clock::now().time_since_epoch().count()));
		std::error_code ignored;
		std::filesystem::remove(marker, ignored);
		auto launched = request(core::agent_protocol::external_terminal);
		launched.Arguments.insert(
			launched.Arguments.end(),
			{ "--pre-assignment-side-effect", marker.string() });

		auto launcher = adapters::agent::agent_process_launcher{ };
		adapters::agent::agent_process_launcher::
			force_job_assignment_failure_for_testing(true);
		const auto result = launcher.run(launched, 20'000);
		adapters::agent::agent_process_launcher::
			force_job_assignment_failure_for_testing(false);
		expect(
			result.Outcome == application::agent_run_outcome::failed_to_start
				&& !result.StandardError.empty() && !result.ExitCode.has_value()
				&& result.Events.empty() && !std::filesystem::exists(marker),
			"forced Job Object assignment failure leaves the suspended child without a side effect");

		const auto recovered = launcher.run(
			request(core::agent_protocol::external_terminal), 20'000);
		expect(
			recovered.Outcome == application::agent_run_outcome::success,
			"the native launcher recovers after a pre-resume assignment refusal");
		std::filesystem::remove(marker, ignored);
	}
#endif
} // namespace

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };
	test_all_four_protocols_stream_partial_frames_under_stderr_pressure();
	test_real_child_receives_no_parent_credential_or_context_in_argv();
	test_independent_agent_identity_alone_controls_canary_authority();
	test_every_terminal_condition_is_typed_and_failed_runs_publish_no_result();
	test_timeout_terminates_the_descendant_process_tree();
#if defined(Q_OS_WIN) && defined(BPR_BUILD_TESTING)
	test_windows_assignment_failure_has_no_pre_assignment_side_effect();
#endif
	std::cout << "agent-launch-canary-tests:pass\n";
	return EXIT_SUCCESS;
}
