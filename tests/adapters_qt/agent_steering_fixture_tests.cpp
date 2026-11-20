/// \file
/// \brief [[BPR-2153]], RFC-BPR-0126 AC05 and AC07. Native steering through
/// the real launcher, its kept connection and protocol loop, against the
/// fixture agent run as Claude Code: a correction written while the turn
/// works is taken after its current tool call, settled by the result's
/// `user_message_uuids` and counted once; an agent that reports no
/// qualified version keeps Queue, and nothing is written to it. No network,
/// no inference, no cost.
///
///     agent_steering_fixture_tests FIXTURE_AGENT_FOLDER

#include "../support/plain_agent_launch.hpp"
#include "bpr/adapters/agent/agent_process_launcher.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
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

	[[nodiscard]] auto answer_text(const application::agent_run_result& aResult) -> std::string
	{
		auto text = std::string{ };
		for (const auto& item : aResult.AssistantItems) {
			text += item;
		}
		return text;
	}

	struct steered_run
	{
		application::agent_run_result    Result;
		application::agent_steer_receipt Receipt;
		std::uint64_t                    NativeTurn = 0U;
	};

	/// One slow native turn of the fixture agent, steered until a correction
	/// is answered by something other than "not running yet".
	[[nodiscard]] auto run_and_steer(const std::filesystem::path& aClaude, const bool aSteerable) -> steered_run
	{
		auto workspace = QTemporaryDir{ };
		expect(workspace.isValid(), "a scratch workspace");
		auto profiles      = core::built_in_agent_profiles();
		auto profile       = *std::ranges::find(profiles, core::agent_protocol::claude_stream_json, &core::agent_profile::Protocol);
		profile.Executable = aClaude.string();
		auto launch        = tests::plain_agent_launch(profile, "Take your time, then say what you read.", workspace.path().toStdString(), [](const std::string&) { return std::optional<std::string>{ }; });
		expect(launch.has_value(), "the fixture's launch is built as Claude's");
		launch->NativeConversation = aSteerable ? "steered-fixture" : "unqualified-fixture";
		launch->Run                = core::agent_run_id{ 7U };
		launch->Path               = core::agent_run_path::agent_tab;
		// As the Agent tab's runs do: activity is reported, so steps count.
		launch->ReportActivity = true;
		launch->Environment.emplace_back("BPR_FIXTURE_AGENT_SLOW_STEP_MS", "60");
		launch->Environment.emplace_back("BPR_FIXTURE_AGENT_PAUSE_MS", "1");
		launch->Environment.emplace_back("BPR_FIXTURE_AGENT_STEERABLE", aSteerable ? "1" : "0");
		auto launcher = adapters::agent::agent_process_launcher{ };
		auto result   = application::agent_run_result{ };
		auto runner   = std::thread{ [&launcher, &launch, &result] { result = launcher.run(*launch, 60000); } };
		auto mutex    = std::mutex{ };
		auto receipt  = std::optional<application::agent_steer_receipt>{ };
		auto waited   = QElapsedTimer{ };
		waited.start();
		while (waited.elapsed() < 20000) {
			auto answered = std::optional<application::agent_steer_receipt>{ };
			launcher.steer({ .Conversation = launch->NativeConversation, .Run = launch->Run, .NativeTurn = launch->NativeTurn, .Text = "Look at b.cpp instead" },
						   [&mutex, &receipt](const application::agent_steer_receipt& aReceipt) {
							   const auto lock = std::scoped_lock{ mutex };
							   receipt         = aReceipt;
						   });
			while (!answered && waited.elapsed() < 20000) {
				{
					const auto lock = std::scoped_lock{ mutex };
					answered        = std::exchange(receipt, std::nullopt);
				}
				QThread::msleep(1);
			}
			// Before the turn has started, or before its init, it is not yet
			// running: never written, so asking again sends nothing twice.
			const auto early = answered && answered->Disposition == core::agent_steer_disposition::unsent && (answered->Support == core::agent_steer_support::not_running || answered->Support == core::agent_steer_support::no_connection);
			if (answered && !early) {
				runner.join();
				launcher.cancel();
				return { .Result = std::move(result), .Receipt = *answered, .NativeTurn = launch->NativeTurn };
			}
			QThread::msleep(20);
		}
		runner.join();
		std::cerr << "run: " << application::name(result.Outcome) << " protocol=" << (result.ProtocolFailure ? application::name(*result.ProtocolFailure) : "none") << " stderr=" << result.StandardError
				  << " refusal=" << result.RefusalReason << '\n';
		expect(false, "a correction was answered while the turn ran");
		return { };
	}

	void test_the_fixture_takes_a_correction_after_its_tool_call(const std::filesystem::path& aClaude)
	{
		const auto run = run_and_steer(aClaude, true);
		std::cout << "steerable: disposition=" << static_cast<int>(run.Receipt.Disposition) << " support=" << static_cast<int>(run.Receipt.Support) << " identity=" << run.Receipt.Identity
				  << " outcome=" << application::name(run.Result.Outcome) << " consumed=" << run.Result.Steers.Consumed << " unknown=" << run.Result.Steers.Unknown << '\n';
		expect(run.Receipt.Disposition == core::agent_steer_disposition::sent && run.Receipt.Identity == core::agent_steer_identity(run.NativeTurn, 0U),
			   "the correction was written to the running turn, under the identity of its native turn");
		expect(run.Result.Outcome == application::agent_run_outcome::success, "the steered run succeeds");
		expect(run.Result.Steers == core::agent_steer_tally{ .Consumed = 1U }, "the result names the correction: consumed, once");
		const auto text = answer_text(run.Result);
		expect(text.find("Took your correction after step") != std::string::npos, "the agent took it after a tool call, in the same turn");
		// Steered as soon as the turn runs, so before its first step; the
		// window course steers mid-turn and sees the step.
		expect(run.Result.SteeredInputs.size() == 1U && run.Result.SteeredInputs.front().Text == "Look at b.cpp instead"
				   && run.Result.SteeredInputs.front().Disposition == core::agent_steer_disposition::consumed,
			   "the run's result keeps the steered message, consumed");
		expect(run.Result.ReportedCostUsd == "0", "the run reports one figure for all its parts");
	}

	/// [[BPR-2252]]. A turn with a helper reports a figure before it ends:
	/// the real launcher streams it while the helper is still out, and the
	/// run's last figure settles the turn.
	void test_a_helper_turn_streams_its_cost_while_it_runs(const std::filesystem::path& aClaude)
	{
		auto workspace = QTemporaryDir{ };
		expect(workspace.isValid(), "a scratch workspace");
		auto profiles      = core::built_in_agent_profiles();
		auto profile       = *std::ranges::find(profiles, core::agent_protocol::claude_stream_json, &core::agent_profile::Protocol);
		profile.Executable = aClaude.string();
		auto launch        = tests::plain_agent_launch(profile, "FIXTURE_HELPER then say what you read.", workspace.path().toStdString(), [](const std::string&) { return std::optional<std::string>{ }; });
		expect(launch.has_value(), "the fixture's launch is built as Claude's");
		launch->NativeConversation = "helper-fixture";
		launch->Run                = core::agent_run_id{ 8U };
		launch->Path               = core::agent_run_path::agent_tab;
		launch->ReportActivity     = true;
		launch->Environment.emplace_back("BPR_FIXTURE_AGENT_SLOW_STEP_MS", "20");
		launch->Environment.emplace_back("BPR_FIXTURE_AGENT_PAUSE_MS", "1");
		auto seen    = std::vector<std::pair<std::string, bool>>{ };
		auto helpers = 0;
		auto mutex   = std::mutex{ };
		auto sink    = [&seen, &helpers, &mutex](const application::agent_stream_event& aEvent) {
			const auto lock = std::scoped_lock{ mutex };
			if (aEvent.Kind == application::agent_stream_kind::activity && aEvent.Activity) {
				helpers += aEvent.Activity->Kind == core::agent_activity_kind::helper_started ? 1 : aEvent.Activity->Kind == core::agent_activity_kind::helper_ended ? -1
																																									 : 0;
			}
			if (aEvent.Kind == application::agent_stream_kind::cost) {
				seen.emplace_back(aEvent.Text, helpers > 0);
			}
		};
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(*launch, 60000, sink);
		launcher.cancel();
		expect(result.Outcome == application::agent_run_outcome::success && result.ReportedCostUsd == "0.0587", "the turn ends once, at the run's last figure");
		expect(seen.size() == 2U && seen[0] == std::pair<std::string, bool>{ "0.0412", true } && seen[1] == std::pair<std::string, bool>{ "0.0587", false },
			   "the first figure is streamed while the helper is out, the last as the turn ends");
	}

	void test_an_unqualified_agent_keeps_queue(const std::filesystem::path& aClaude)
	{
		const auto run = run_and_steer(aClaude, false);
		expect(run.Receipt.Disposition == core::agent_steer_disposition::unsent && run.Receipt.Support == core::agent_steer_support::version_unknown, "an agent that reports no version is never written to, and says why");
		expect(run.Result.Outcome == application::agent_run_outcome::success && run.Result.Steers == core::agent_steer_tally{ }, "its run is unchanged");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };
	if (argc != 2) {
		std::cerr << "usage: agent_steering_fixture_tests FIXTURE_AGENT_FOLDER\n";
		return EXIT_FAILURE;
	}
	// [[BPR-2205]]. The build copies the fixture in as `claude.exe` on
	// Windows (src/app/CMakeLists.txt).
#if defined(_WIN32)
	const auto claude = std::filesystem::path{ argv[1] } / "claude.exe";
#else
	const auto claude = std::filesystem::path{ argv[1] } / "claude";
#endif
	expect(std::filesystem::is_regular_file(claude), "the fixture agent is built as claude");
	test_the_fixture_takes_a_correction_after_its_tool_call(claude);
	test_an_unqualified_agent_keeps_queue(claude);
	test_a_helper_turn_streams_its_cost_while_it_runs(claude);
	std::cout << "agent-steering-fixture-tests:pass\n";
	return EXIT_SUCCESS;
}
