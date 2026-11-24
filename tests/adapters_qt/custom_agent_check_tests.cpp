/// \file
/// \brief [[BPR-2320]]. A custom agent's Check through the real process
/// launcher. The owner's custom Codex (JSONL) agent always failed Check: its
/// instructions probe named no working folder, so the launcher refused it
/// before the program started, and the result was named after another
/// agent. Here a fixture Codex program that follows its instructions passes,
/// and its probe ran in a temporary folder that is gone afterwards; the
/// launcher still refuses a request that names no folder; a custom Claude
/// agent whose program prints plain text fails with the protocol named; and
/// the result's words name the checked entry, not the preferred one, and
/// never say "signed in" for a custom agent.

#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/agent_setup_view.hpp"

#include <QCoreApplication>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	/// Forwards to the real launcher and keeps every request and result.
	class recording_port final : public application::agent_process_port
	{
	public:
		explicit recording_port(application::agent_process_port& aInner)
			: Inner{ aInner }
		{ }
		[[nodiscard]] auto run(const application::agent_launch_request& aRequest, const int aTimeout, const application::agent_output_sink& aSink = { }, std::stop_token aStop = { })
			-> application::agent_run_result override
		{
			Requests.push_back(aRequest);
			auto result = Inner.run(aRequest, aTimeout, aSink, std::move(aStop));
			Results.push_back(result);
			return result;
		}
		void cancel() noexcept override { Inner.cancel(); }

		std::vector<application::agent_launch_request> Requests;
		std::vector<application::agent_run_result>     Results;

	private:
		application::agent_process_port& Inner;
	};

	[[nodiscard]] auto text(const std::filesystem::path& aPath) -> std::string
	{
		const auto bytes = aPath.u8string();
		return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
	}

	[[nodiscard]] auto copy_canary(const std::filesystem::path& aRoot, const std::string_view aStem) -> std::filesystem::path
	{
#if defined(_WIN32)
		const auto target = aRoot / (std::string{ aStem } + ".exe");
#else
		const auto target = aRoot / std::string{ aStem };
#endif
		std::filesystem::copy_file(BPR_AGENT_CANARY_EXECUTABLE, target);
		std::filesystem::permissions(target, std::filesystem::perms::owner_all);
		return target;
	}

	/// The value of the first `name\tvalue` line the canary reported.
	[[nodiscard]] auto reported(const application::agent_run_result& aResult, const std::string_view aName) -> std::optional<std::string>
	{
		const auto prefix = std::string{ aName } + "\t";
		for (const auto& event : aResult.Events) {
			if (event.Text.starts_with(prefix)) {
				return event.Text.substr(prefix.size());
			}
		}
		for (const auto& item : aResult.AssistantItems) {
			if (item.starts_with(prefix)) {
				return item.substr(prefix.size());
			}
		}
		return std::nullopt;
	}

	struct fixture
	{
		application::manual_executor                       Effects;
		application::manual_executor                       Checks;
		application::manual_executor                       Decision;
		adapters::agent::agent_process_launcher            Launcher;
		recording_port                                     Process{ Launcher };
		std::shared_ptr<application::agent_program_source> Programs = std::make_shared<application::agent_program_source>();
		std::unique_ptr<application::agent_setup_host>     Host;

		fixture()
		{
			Host = std::make_unique<application::agent_setup_host>(application::agent_setup_dependencies{
				.Process  = &Process,
				.Locate   = [](core::agent_program) { return std::optional<core::agent_search_candidate>{ }; },
				.Inspect  = [](const std::string_view aPath) { return adapters::agent::inspect_claude_process_wrapper(aPath); },
				.Programs = Programs,
				.Parent   = [](const std::string& aName) -> std::optional<std::string> {
					const auto value = qEnvironmentVariable(aName.c_str());
					return value.isEmpty() ? std::nullopt : std::optional{ value.toStdString() };
				},
				.Profiles = core::built_in_agent_profiles(),
				.Effects  = &Effects,
				.Checks   = &Checks,
				.Decision = &Decision,
			});
			Host->observe_policy(core::built_in_agent_policy(core::git_host_id{ 2U }, core::built_in_agent_host_kind::explicit_ghes));
			Host->load();
			settle();
		}
		~fixture() { Host->shutdown(); }

		void settle()
		{
			for (auto turns = 0; turns < 64 && (Effects.pending() != 0U || Checks.pending() != 0U || Decision.pending() != 0U); ++turns) {
				Effects.drain();
				Checks.drain();
				Decision.drain();
			}
		}

		[[nodiscard]] auto state() const -> application::agent_setup_projection { return Host->projection(); }
	};

	/// AC1. A custom Codex JSONL agent whose program follows its
	/// instructions passes Check, with both Instructions settings, and its
	/// probe ran in an empty temporary folder that is gone afterwards.
	void test_a_custom_codex_check_passes_through_the_launcher(const std::filesystem::path& aRoot)
	{
		const auto program = copy_canary(aRoot, "codex-launcher");
		for (const auto delivery : { core::agent_instruction_delivery::message, core::agent_instruction_delivery::system_prompt }) {
			auto f = fixture{ };
			f.Host->add_agent({ .Name = "Launcher Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = text(program), .Arguments = { "--codex" }, .CheckArguments = { "--version" }, .Instructions = delivery }, true);
			f.settle();
			expect(f.Process.Requests.size() == 1U && f.Process.Requests.front().Purpose == application::agent_launch_purpose::model_catalog, "adding Codex starts only its catalog probe");
			f.Host->check();
			f.settle();
			const auto state = f.state();
			expect(f.Process.Requests.size() == 3U && f.Process.Results.size() == 3U, "after the catalog, Check runs the check command and one instructions probe");
			expect(f.Process.Results[2].Outcome == application::agent_run_outcome::success, "the probe started and answered: " + f.Process.Results[2].StandardError);
			expect(state.State.AgentCheck.State == core::setup_check_state::passed, "a custom Codex that follows its instructions passes Check: " + state.CheckDetail);
			expect(state.State.AgentCheck.Version == "1.2.3 (wrapper fixture)", "and names the version its own check printed");
			expect(state.CheckedEntry == std::optional{ state.Preferences.Agents.front().Id } && state.CheckStep == application::agent_check_step::instructions_probe, "the result names the entry and its probe");
			const auto folder = reported(f.Process.Results[2], "cwd");
			expect(folder.has_value() && !folder->empty(), "the probe reported its working folder");
			const auto where = std::filesystem::path{ *folder };
			expect(where.filename().string().starts_with("bpr-writing-"), "the probe ran in a fresh empty folder of its own: " + *folder);
			expect(!std::filesystem::exists(where), "and that folder is gone once the probe ended");
		}
	}

	/// The launcher still refuses a request that names no folder, before
	/// anything starts; the probe request is not refused.
	void test_the_launcher_refuses_a_request_with_no_folder(const std::filesystem::path& aRoot)
	{
		const auto program  = copy_canary(aRoot, "codex-refused");
		const auto profiles = core::built_in_agent_profiles();
		const auto profile  = std::ranges::find(profiles, core::agent_protocol::codex_jsonl_exec, &core::agent_profile::Protocol);
		expect(profile != profiles.end(), "a Codex JSONL profile is built in");
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto none     = std::vector<std::string>{ };
		auto       probe    = application::build_codex_instructions_probe(*profile, text(program), none, core::agent_instruction_delivery::message, [](const std::string&) { return std::optional<std::string>{ }; });
		auto       nowhere  = probe;
		nowhere.WritingOnly = false;
		expect(!application::names_agent_folder(nowhere), "the request names no folder");
		const auto refused = launcher.run(nowhere, 20'000);
		expect(refused.Outcome == application::agent_run_outcome::failed_to_start && refused.StandardError == "agent working directory is not an accepted directory" && refused.Events.empty(),
			   "a request with neither a working folder nor WritingOnly is refused with today's message, before it starts");
		const auto ran = launcher.run(probe, 20'000);
		expect(ran.Outcome == application::agent_run_outcome::success && reported(ran, "cwd").has_value(), "the probe request is not refused: " + ran.StandardError);
	}

	/// AC4. A custom Claude agent whose program does not answer in stream
	/// JSON fails Check with the protocol named; one that does passes.
	void test_a_custom_claude_check_needs_stream_json(const std::filesystem::path& aRoot)
	{
		{
			auto       f       = fixture{ };
			const auto program = copy_canary(aRoot, "claude-launcher");
			f.Host->add_agent({ .Name = "Launcher Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = text(program), .Arguments = { "--skip-skills-upgrade" } }, true);
			f.settle();
			f.Host->check();
			f.settle();
			expect(f.Process.Requests.size() == 1U && f.Process.Requests.front().WritingOnly, "one tool-free stream JSON turn in an empty folder");
			expect(f.state().State.AgentCheck.State == core::setup_check_state::passed, "a program that answers in stream JSON passes: " + f.state().CheckDetail);
		}
#if defined(Q_OS_UNIX)
		auto       f      = fixture{ };
		const auto script = aRoot / "plain-launcher";
		{
			auto stream = std::ofstream{ script, std::ios::binary };
			stream << "#!/bin/sh\nprintf 'Welcome to the launcher\\n'\nexit 0\n";
		}
		std::filesystem::permissions(script, std::filesystem::perms::owner_all);
		f.Host->add_agent({ .Name = "Plain Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = text(script) }, true);
		f.settle();
		f.Host->check();
		f.settle();
		const auto state = f.state();
		expect(state.State.AgentCheck.Failure == core::agent_setup_failure::output_unreadable && state.CheckDetail.find("wrote: Welcome to the launcher\n") != std::string::npos,
			   "a program that prints plain text and exits 0 fails as not the protocol, with its first line: " + state.CheckDetail);
		const auto words = qt::agent_check_failure_words(state, QStringLiteral("Claude Code")).toStdString();
		expect(words.starts_with("Plain Claude answered, but not in Claude Code's stream JSON.") && words.find("Its first line: Welcome to the launcher") != std::string::npos,
			   "the failure names the entry and the protocol: " + words);
#endif
	}

	/// AC3. The words name the checked entry and what was checked.
	void test_the_result_names_the_checked_entry()
	{
		auto setup                       = application::agent_setup_projection{ };
		auto claude                      = core::agent_entry{ };
		claude.Id                        = 1U;
		claude.Name                      = "Launcher Claude";
		claude.Protocol                  = core::agent_protocol::claude_stream_json;
		auto codex                       = core::agent_entry{ };
		codex.Id                         = 2U;
		codex.Name                       = "Launcher Codex";
		codex.Protocol                   = core::agent_protocol::codex_jsonl_exec;
		codex.CheckArguments             = { "--version" };
		setup.Preferences.Agents         = { claude, codex };
		setup.Preferences.PreferredAgent = 1U;
		setup.State.Chosen               = core::setup_agent::claude_code;
		setup.ChecksSignIn               = true;
		setup.State.AgentCheck           = core::fail_check(core::agent_setup_failure::failed_to_start);
		setup.CheckDetail                = "failed_to_start\nstderr:\nagent working directory is not an accepted directory";
		setup.CheckedEntry               = 2U;
		setup.CheckStep                  = application::agent_check_step::instructions_probe;
		const auto fallback              = QStringLiteral("Claude Code");
		const auto row                   = qt::agent_check_words(setup, fallback).toStdString();
		const auto why                   = qt::agent_check_failure_words(setup, fallback).toStdString();
		expect(row == "Launcher Codex's instructions reach the model", "the row names the Codex entry and what its check proves: " + row);
		expect(why.starts_with("Launcher Codex's instructions probe didn't start."), "the failure names the Codex entry and the step: " + why);
		for (const auto& said : { row, why }) {
			expect(said.find("Claude Code") == std::string::npos && said.find("signed in") == std::string::npos, "never the preferred agent's name, never signed in: " + said);
		}
		setup.CheckStep = application::agent_check_step::check_command;
		expect(qt::agent_check_words(setup, fallback) == QStringLiteral("Launcher Codex's check command passes")
				   && qt::agent_check_failure_words(setup, fallback).startsWith(QStringLiteral("Launcher Codex's check command didn't start.")),
			   "a check command that failed is named as that");
		setup.State.AgentCheck = core::setup_check{ .State = core::setup_check_state::passed };
		setup.CheckedEntry     = 1U;
		setup.CheckStep        = application::agent_check_step::protocol_probe;
		expect(qt::agent_check_words(setup, fallback) == QStringLiteral("Launcher Claude answers in Claude Code's stream JSON"), "a custom Claude says it answers in stream JSON");
		setup.Checking      = true;
		setup.CheckingEntry = 2U;
		expect(qt::checked_agent_name(setup, fallback) == QStringLiteral("Launcher Codex"), "a running check names the entry it runs on");
		setup.Checking     = false;
		setup.CheckedEntry = std::nullopt;
		expect(qt::agent_check_words(setup, fallback) == QStringLiteral("Claude Code is signed in"), "with no entry known, a popular agent keeps its sign-in words");
	}
} // namespace

auto main(int argc, char** argv) -> int
try {
	QCoreApplication                application{ argc, argv };
	const auto                      root = bpr::tests::make_scratch_directory("bpr-custom-agent-check");
	const bpr::tests::scratch_guard owned{ root };
	test_the_result_names_the_checked_entry();
	test_the_launcher_refuses_a_request_with_no_folder(root);
	test_a_custom_codex_check_passes_through_the_launcher(root);
	test_a_custom_claude_check_needs_stream_json(root);
	std::cout << "custom-agent-check-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
