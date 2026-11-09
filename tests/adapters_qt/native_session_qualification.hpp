#ifndef BPR_TESTS_ADAPTERS_QT_NATIVE_SESSION_QUALIFICATION_HPP
#define BPR_TESTS_ADAPTERS_QT_NATIVE_SESSION_QUALIFICATION_HPP

#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/application/tour_agent_conversation.hpp"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace bpr::tests {
	// Opt-in installed course. The caller supplies an existing disposable root
	// and the exact installed executable; ordinary ctest never invokes inference.
	inline auto qualify_native_sessions(const core::agent_protocol aProtocol, const std::string& aExecutable, const std::filesystem::path& aRoot, const bool aReplay, const bool aInterrupt = false) -> bool
	{
		auto       profiles = core::built_in_agent_profiles();
		const auto found    = std::ranges::find(profiles, aProtocol, &core::agent_profile::Protocol);
		if (found == profiles.end() || !std::filesystem::is_directory(aRoot)) {
			return false;
		}
		auto profile                   = *found;
		profile.Executable             = aExecutable;
		const auto environment         = [](const std::string& aName) -> std::optional<std::string> { return qEnvironmentVariableIsSet(aName.c_str()) ? std::optional{ qEnvironmentVariable(aName.c_str()).toStdString() } : std::nullopt; };
		auto       tour                = review_tour_fixture();
		const auto source              = *tour.Stops.front().Original;
		tour.Stops.front().Destination = core::destination_of(source);
		tour.Stops.front().Original.reset();
		tour.Stops.front().Related.clear();
		tour.Stops.front().Mapping = core::tour_mapping::unavailable;
		tour.Stops.front().Title   = "Unconfirmed source";
		tour.Stops.front().Body    = "Explain the arithmetic.";
		auto confirmed             = core::tour_stop{ };
		confirmed.Id               = core::tour_stop_id{ 2 };
		confirmed.Title            = "Confirmed lines, unfinished prose";
		confirmed.Original         = source;
		confirmed.Mapping          = core::tour_mapping::exact_original;
		tour.Stops.push_back(std::move(confirmed));
		tour.LastStopId            = 2;
		const auto memory          = "qualification-memory-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
		const auto code            = "// disposable native-session qualification\n// memory_token: " + memory + "\nint identity(int value) {\n    return value + 7;\n}\n// source range ends here\n";
		auto       session         = core::tour_agent_session{ };
		auto       context         = application::tour_agent_context{ };
		context.Session            = &session;
		context.PinnedBase         = tour.Base;
		context.PinnedHead         = tour.Head;
		context.ObservedBase       = tour.Base;
		context.ObservedHead       = tour.Head;
		context.PublicationAllowed = false;
		context.PublicationStatus  = "Unconfirmed source and unfinished explanation block publication.";
		auto history               = std::vector<core::brief_turn>{ };
		auto launcher              = std::make_unique<adapters::agent::agent_process_launcher>();
		auto handle                = std::string{ };
		auto firstProcess          = std::uint64_t{ 0U };
		auto records               = QJsonArray{ };
		auto questions             = std::array<std::string, 3>{
			"Read src/item.cpp using an allowed tool. Explain why Stop 1 needs source confirmation, while Stop 2 has the identical path/range confirmed but lacks prose. Give the real corrective BPR action names and distinguish confirmation, Ready and publication. Do not include the memory_token value in this first answer.",
			"What exact memory_token did you observe in src/item.cpp, and what arithmetic does the function perform? Reuse prior tool observations if available; otherwise read that fixture file. Do not edit anything.",
			"The latest snapshot now confirms Stop 1 at the same source range with author-selected mapping, at the same draft revision. Explain its current source-warning state separately from Ready and Stop 2's unfinished prose. Name the applicable BPR actions."
		};
		if (aInterrupt) {
			questions = { "Reply only READY inside the required reply JSON. Do not call tools.", "Inside the required reply JSON, write 60 numbered paragraphs explaining source confirmation versus prose readiness. Do not call tools.", "The previous request was explicitly stopped. Do not continue it or repeat it. Reply only AFTER_INTERRUPTION in the required reply JSON. Do not call tools." };
		}
		for (std::size_t index = 0; index < questions.size(); ++index) {
			const auto workspace = aRoot / (aReplay ? "replay-" + std::to_string(index + 1U) : "native-workspace");
			std::filesystem::create_directories(workspace / "src");
			{
				auto file = std::ofstream{ workspace / "src/item.cpp" };
				file << code;
			}
			if (index == 2U) {
				tour.Stops.front().Original = source;
				tour.Stops.front().Mapping  = core::tour_mapping::author_selected;
				context.PublicationStatus   = "Stop 2's unfinished explanation still blocks publication.";
				if (!aReplay && aProtocol != core::agent_protocol::codex_jsonl_exec) {
					launcher.reset();
					launcher = std::make_unique<adapters::agent::agent_process_launcher>();
				}
			}
			const auto assignment = "Work only in the disposable fixture " + aRoot.string() + ". Do not read or write any other repository or BetterPullReview source. Do not modify files, use network, or spawn subagents. The installed runtime may retain its own native session metadata. " + questions[index];
			const auto prompt     = application::build_tour_ask_prompt("Qualification fixture", "Two stops share one source range.", std::vector<application::tour_draft_file>{ { "src/item.cpp", "modified" } }, tour, { }, { }, std::nullopt, assignment, false, std::nullopt, { }, { }, context);
			if (!prompt) {
				return false;
			}
			auto supplement                        = core::agent_context_supplement{ };
			supplement.ApplicationSession          = core::application_session_id{ 1U };
			supplement.Session.Id                  = core::agent_session_id{ 1U };
			supplement.Session.Profile             = profile.Id;
			supplement.Session.Binding.Host        = core::git_host_id{ 1U };
			supplement.Session.Binding.Repository  = core::repository_id{ 1U };
			supplement.Session.Binding.PullRequest = core::pull_request_id{ 1U };
			supplement.Session.Binding.BaseOid     = tour.Base;
			supplement.Session.Binding.HeadOid     = tour.Head;
			supplement.Session.Binding.LeftEpoch   = core::epoch_ordinal{ 1U };
			supplement.Session.Binding.RightEpoch  = core::epoch_ordinal{ 1U };
			supplement.TourPrompt                  = *prompt;
			if (aReplay) {
				supplement.Replay = core::bound_transcript_replay(history, true);
			}
			auto request = application::build_agent_launch(profile, supplement, environment, { .ReadOnlyTools = true });
			if (!request) {
				return false;
			}
			request->WorkingDirectory = workspace.string();
			if (!aReplay) {
				request->NativeConversation = "installed-readonly-qualification";
				request->NativeHandle       = handle;
				request->NativeTurn         = index + 2U;
				if (aProtocol == core::agent_protocol::codex_jsonl_exec) {
					auto prepared = application::prepare_native_agent_turn(std::move(*request), false);
					if (!prepared) {
						return false;
					}
					request = std::move(*prepared);
					request->NativeConversation.clear();
				}
			}
			auto clock = QElapsedTimer{ };
			clock.start();
			auto       stop   = std::stop_source{ };
			const auto sink   = aInterrupt && index == 1U ? application::agent_output_sink{ [&stop](const application::agent_stream_event&) { stop.request_stop(); } } : application::agent_output_sink{ };
			const auto result = launcher->run(*request, 120000, sink, stop.get_token());
			auto       answer = result.AssistantItems.empty() ? std::string{ } : result.AssistantItems.back();
			if (answer.empty()) {
				for (const auto& event : result.Events) {
					answer += event.Text;
				}
			}
			const auto parsed               = application::decode_tour_ask_answer(answer);
			auto       record               = QJsonObject{ };
			record["mode"]                  = aReplay ? "Transcript replay" : "native";
			record["turn"]                  = static_cast<int>(index + 1U);
			record["outcome"]               = QString::fromUtf8(application::name(result.Outcome));
			record["elapsed_ms"]            = clock.elapsed();
			record["tool_events"]           = static_cast<qint64>(result.ToolActivity.size());
			record["workspace_starts"]      = aReplay || index == 0U ? 1 : 0;
			record["same_process_as_first"] = index > 0U && result.NativeProcess != 0U && result.NativeProcess == firstProcess;
			record["exact_native_handle"]   = aReplay ? false : !result.NativeHandle.empty() && (handle.empty() || result.NativeHandle == handle);
			record["cost_usd"]              = result.ReportedCostUsd ? QString::fromStdString(*result.ReportedCostUsd) : "unavailable";
			record["answer"]                = QString::fromStdString(parsed ? parsed->Reply : answer);
			record["protocol_failure"]      = result.ProtocolFailure ? QString::fromUtf8(application::name(*result.ProtocolFailure)) : "none";
			record["qt"]                    = qVersion();
			record["course"]                = aInterrupt ? "explicit interruption/reconnect" : "equivalent conversation";
			auto models                     = QJsonArray{ };
			for (const auto& report : result.Metadata.ProtocolReported) {
				if (report.Settings.Model) {
					models.push_back(QString::fromStdString(*report.Settings.Model));
				}
			}
			record["reported_models"] = models;
#if defined(NDEBUG)
			record["build"] = "Release";
#else
			record["build"] = "Debug (timing nongating)";
#endif
			records.push_back(record);
			{
				auto output = std::ofstream{ aRoot / "qualification-results.json" };
				output << QJsonDocument{ records }.toJson(QJsonDocument::Indented).toStdString();
			}
			std::cout << "installed-qualification mode=" << (aReplay ? "replay" : "native") << " turn=" << index + 1U << " outcome=" << application::name(result.Outcome) << " protocol=" << (result.ProtocolFailure ? application::name(*result.ProtocolFailure) : "none") << " elapsed_ms=" << clock.elapsed() << " tool_events=" << result.ToolActivity.size() << "\n";
			if (aInterrupt && index == 1U) {
				if (result.Outcome != application::agent_run_outcome::cancelled) {
					return false;
				}
				if (!result.NativeHandle.empty()) {
					handle = result.NativeHandle;
				}
				continue;
			}
			if (aInterrupt && index == 2U && (!parsed || parsed->Reply.find("AFTER_INTERRUPTION") == std::string::npos)) {
				return false;
			}
			if (result.Outcome != application::agent_run_outcome::success || !parsed || (!aReplay && (result.NativeHandle.empty() || (!handle.empty() && result.NativeHandle != handle)))) {
				return false;
			}
			if (index == 0U && !aInterrupt) {
				const auto reply = QString::fromStdString(parsed->Reply);
				if (!reply.contains(QStringLiteral("Use selected lines"), Qt::CaseInsensitive) || !reply.contains(QStringLiteral("Ready"), Qt::CaseInsensitive) || !reply.contains(QStringLiteral("confirm"), Qt::CaseInsensitive) || answer.find(memory) != std::string::npos) {
					return false;
				}
				firstProcess = result.NativeProcess;
			}
			handle = result.NativeHandle;
			history.push_back({ questions[index], parsed->Reply });
		}
		launcher.reset();
		return true;
	}
}
#endif
