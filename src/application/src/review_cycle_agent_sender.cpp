#include "bpr/application/review_cycle_agent_sender.hpp"

#include <string>
#include <utility>

namespace bpr::application {
	namespace {
		auto end_of(const agent_run_outcome aOutcome) -> core::cycle_send_end
		{
			if (aOutcome == agent_run_outcome::success) {
				return core::cycle_send_end::completed;
			}
			if (aOutcome == agent_run_outcome::failed_to_start || aOutcome == agent_run_outcome::workspace_unavailable) {
				return core::cycle_send_end::not_started;
			}
			if (aOutcome == agent_run_outcome::timeout) {
				return core::cycle_send_end::timed_out;
			}
			if (aOutcome == agent_run_outcome::cancelled) {
				return core::cycle_send_end::cancelled;
			}
			return core::cycle_send_end::stopped;
		}

		/// The session the harness reported about itself, when it named one.
		auto reported_session(const core::agent_runtime_metadata& aMetadata) -> std::optional<std::string>
		{
			for (const auto& report : aMetadata.ProtocolReported) {
				for (const auto& name : { report.RemoteSession, report.RemoteThread }) {
					if (name && core::send_session_name(*name)) {
						return name;
					}
				}
			}
			return std::nullopt;
		}
	}

	agent_round_sender::agent_round_sender(agent_process_port& aProcesses, environment_reader aEnvironment)
		: Processes{ aProcesses }
		, Environment{ std::move(aEnvironment) }
	{
	}

	auto agent_round_sender::send(const cycle_send_assignment& aAssignment, std::stop_token aStop) -> core::cycle_send_result
	{
		auto launch = build_round_launch(aAssignment.Profile, aAssignment.Document, aAssignment.Session.Continue, aAssignment.WorkingDirectory, Environment);
		if (!launch) {
			return { .End = core::cycle_send_end::not_started };
		}
		// A harness without assistant framing (the external terminal) replies in lines.
		auto       lines  = std::string{ };
		const auto sink   = [&lines](const agent_stream_event& aEvent) { lines += (lines.empty() ? "" : "\n") + aEvent.Text; };
		const auto run    = Processes.run(*launch, review_cycle_send_timeout_milliseconds, sink, std::move(aStop));
		auto       result = core::cycle_send_result{ .End = end_of(run.Outcome), .Session = reported_session(run.Metadata) };
		if (result.End != core::cycle_send_end::completed) {
			return result;
		}
		for (const auto& item : run.AssistantItems) {
			result.Reply += (result.Reply.empty() ? "" : "\n\n") + item;
		}
		if (run.AssistantItems.empty()) {
			result.Reply = std::move(lines);
		}
		return result;
	}

	void agent_round_sender::cancel() noexcept
	{
		Processes.cancel();
	}
}
