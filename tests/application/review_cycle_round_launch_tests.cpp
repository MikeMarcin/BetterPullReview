#include "bpr/application/agent_launch.hpp"
#include "bpr/application/review_cycle_agent_sender.hpp"
#include "bpr/core/agent_profile.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1180]]. The Send to Agent launch for each built-in protocol,
/// and how the adapter reads a run's end, without starting a process. The
/// real processes are `application_review_cycle_send_public_path`'s.

namespace {
	using namespace bpr;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle round launch failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto profile(const core::agent_profile_id aId) -> core::agent_profile
	{
		for (auto& candidate : core::built_in_agent_profiles()) {
			if (candidate.Id == aId) {
				return candidate;
			}
		}
		expect(false, "a built-in profile");
		return { };
	}

	constexpr auto document = std::string_view{ "# Review Cycle RC-1 \xE2\x80\x94 F1\n\nSay \"why\".\n" };

	auto parent(const std::string& aName) -> std::optional<std::string>
	{
		if (aName == "PATH") {
			return "/usr/bin";
		}
		if (aName == "GITHUB_TOKEN") {
			return "secret";
		}
		return std::nullopt;
	}

	void frames_the_round_for_each_protocol()
	{
		const auto claude = application::build_round_launch(profile(core::claude_stream_json_profile_id), document, std::nullopt, "/work", parent);
		expect(claude && claude->Arguments == std::vector<std::string>{ "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose" }, "Claude: print mode, stream JSON both ways");
		expect(claude->Input == "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"# Review Cycle RC-1 \xE2\x80\x94 F1\\n\\nSay \\\"why\\\".\\n\"}]}}\n",
			   "the document as the one user message, escaped and nothing else");
		expect(claude->WorkingDirectory == "/work" && claude->Executable == "claude", "in the cycle's folder, the profile's executable");
		expect(claude->Environment == std::vector<std::pair<std::string, std::string>>{ { "PATH", "/usr/bin" } }, "only allowed names; no credential");
		const auto resumed = application::build_round_launch(profile(core::claude_stream_json_profile_id), document, std::optional<std::string>{ "s-1" }, "/work", parent);
		expect(resumed && resumed->Arguments.size() == 8U && resumed->Arguments[6] == "--resume" && resumed->Arguments[7] == "s-1", "continuing exactly the named session");

		const auto jsonl = application::build_round_launch(profile(core::codex_jsonl_profile_id), document, std::nullopt, "/work", parent);
		expect(jsonl && jsonl->Arguments == std::vector<std::string>{ "exec", "--json", "-" } && jsonl->Input == document, "Codex JSONL: the document is the prompt");
		const auto server = application::build_round_launch(profile(core::codex_app_server_profile_id), document, std::nullopt, "/work", parent);
		expect(server && server->Arguments == std::vector<std::string>{ "app-server" } && server->DeferredContext == document && server->Input.starts_with("{\"method\":\"initialize\""),
			   "Codex App Server: initialize first, the document as the turn's text");
		const auto terminal = application::build_round_launch(profile(core::external_terminal_profile_id), document, std::nullopt, "/work", parent);
		expect(terminal && terminal->Arguments.empty() && terminal->Input == document, "the external terminal reads the document itself");

		for (const auto id : { core::codex_jsonl_profile_id, core::codex_app_server_profile_id, core::external_terminal_profile_id }) {
			expect(!application::build_round_launch(profile(id), document, std::optional<std::string>{ "s-1" }, "/work", parent), "no other harness is told to continue a session");
		}
		expect(!application::build_round_launch(profile(core::claude_stream_json_profile_id), document, std::optional<std::string>{ "--dangerously-skip-permissions" }, "/work", parent),
			   "a session that would read as an option is refused");
		const auto oversized = application::build_round_launch(profile(core::claude_stream_json_profile_id), std::string(application::maximum_agent_input_bytes, 'x'), std::nullopt, "/work", parent);
		expect(!oversized && oversized.error() == application::agent_launch_failure::input_too_large, "over the input bound: nothing to launch");
	}

	/// A process port that answers as told and keeps what it was asked.
	struct scripted_processes final : application::agent_process_port
	{
		application::agent_run_result                  Answer;
		std::vector<std::string>                       Lines;
		std::vector<application::agent_launch_request> Requests;
		std::size_t                                    Cancels = 0;
		[[nodiscard]] auto                             run(const application::agent_launch_request& aRequest, int, const application::agent_output_sink& aSink, std::stop_token) -> application::agent_run_result override
		{
			Requests.push_back(aRequest);
			for (const auto& line : Lines) {
				aSink({ .Text = line });
			}
			return Answer;
		}
		void cancel() noexcept override
		{
			++Cancels;
		}
	};

	void reads_how_a_run_ended()
	{
		auto processes = scripted_processes{ };
		auto sender    = application::agent_round_sender{ processes, parent };
		auto work      = application::cycle_send_assignment{ .Profile = profile(core::claude_stream_json_profile_id), .Document = std::string{ document }, .WorkingDirectory = "/work" };

		processes.Answer = { .Outcome = application::agent_run_outcome::success, .AssistantItems = { "First.", "Second." } };
		processes.Answer.Metadata.ProtocolReported.push_back({ .Basis = "claude", .RemoteSession = "abc-1" });
		auto seen = sender.send(work, { });
		expect(seen.End == core::cycle_send_end::completed && seen.Session == std::optional<std::string>{ "abc-1" } && seen.Reply == "First.\n\nSecond.", "finished: the session it reported and its reply");
		expect(processes.Requests.size() == 1U && processes.Requests.front().Input.find("Say \\\"why\\\"") != std::string::npos, "one launch, with the document");

		processes.Answer = { .Outcome = application::agent_run_outcome::success };
		processes.Answer.Metadata.ProtocolReported.push_back({ .Basis = "codex", .RemoteThread = "thread-9" });
		processes.Lines = { "line one", "line two" };
		seen            = sender.send(work, { });
		expect(seen.Session == std::optional<std::string>{ "thread-9" } && seen.Reply == "line one\nline two", "a thread is a session too; plain lines are the reply");

		const auto ends = std::vector<std::pair<application::agent_run_outcome, core::cycle_send_end>>{
			{ application::agent_run_outcome::failed_to_start, core::cycle_send_end::not_started },
			{ application::agent_run_outcome::workspace_unavailable, core::cycle_send_end::not_started },
			{ application::agent_run_outcome::nonzero_exit, core::cycle_send_end::stopped },
			{ application::agent_run_outcome::protocol_failure, core::cycle_send_end::stopped },
			{ application::agent_run_outcome::timeout, core::cycle_send_end::timed_out },
			{ application::agent_run_outcome::cancelled, core::cycle_send_end::cancelled },
		};
		for (const auto& [outcome, end] : ends) {
			processes.Answer = { .Outcome = outcome, .AssistantItems = { "partial" } };
			seen             = sender.send(work, { });
			expect(seen.End == end && seen.Reply.empty(), "each way a run ends, and no reply from an unfinished turn");
		}
		processes.Answer = { .Outcome = application::agent_run_outcome::success };
		processes.Answer.Metadata.ProtocolReported.push_back({ .Basis = "claude", .RemoteSession = "not a name" });
		expect(!sender.send(work, { }).Session, "a session that is not a name is not reported");

		const auto launches   = processes.Requests.size();
		work.Session.Continue = "--resume";
		expect(sender.send(work, { }).End == core::cycle_send_end::not_started && processes.Requests.size() == launches, "an assignment that cannot be launched starts nothing");
		sender.cancel();
		expect(processes.Cancels == 1U, "Stop reaches the process port");
	}
}

auto main() -> int
{
	frames_the_round_for_each_protocol();
	reads_how_a_run_ended();
	std::cout << "review-cycle-round-launch-tests:pass\n";
	return EXIT_SUCCESS;
}
