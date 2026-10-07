/// \file
/// \brief [[BPR-1301]]. A Claude process wrapper through the real launcher:
/// the path checks on real files, and a recording wrapper that receives
/// exactly what today's Claude receives.
///
/// \details The wrapper is a copy of the agent canary, which reports its
/// argv, environment, working directory and standard input as events. Each
/// course runs the same request twice through the supervised launcher: once
/// with the canary named as the profile's executable, which is how a Claude
/// launch runs today, and once with the shipped bare `claude` and the copy
/// saved as the wrapper. Everything but argv[0] must agree.

#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/claude_process_wrapper.hpp"
#include "bpr/core/agent_context_supplement.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(Q_OS_UNIX)
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	using core::agent_wrapper_refusal;

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < bytes.size(); ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
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

	[[nodiscard]] auto supplement() -> core::agent_context_supplement
	{
		const auto anchor = core::source_line_ref{ .Comparison = identity<core::file_comparison_digest>(5U), .Side = core::diff_side::right, .Line = 17U };
		const auto built  = core::build_agent_supplement({
			.ApplicationSession = core::application_session_id{ 11U },
			.Session            = {
				.Id      = core::agent_session_id{ 40U },
				.Profile = core::claude_stream_json_profile_id,
				.Binding = {
					.Host        = core::git_host_id{ 10U },
					.Repository  = core::repository_id{ 20U },
					.PullRequest = core::pull_request_id{ 30U },
					.BaseOid     = commit(1U),
					.HeadOid     = commit(2U),
					.LeftEpoch   = core::epoch_ordinal{ 1U },
					.RightEpoch  = core::epoch_ordinal{ 2U },
				},
			},
			.PullRequestUrl   = "https://github.example/octo/app/pull/30",
			.PreviousUpload   = commit(3U),
			.LastReviewedHead = commit(4U),
			.SelectedDrafts   = { { .Anchor = anchor, .Text = "Would a bounded retry be safer?" } },
		});
		expect(built.has_value(), "the request supplement builds");
		return *built;
	}

	/// The Claude profile as it ships, but with the canary's two names
	/// allowed, so the child's whole environment is something to compare.
	[[nodiscard]] auto claude_profile(std::string aExecutable, const core::agent_protocol aProtocol = core::agent_protocol::claude_stream_json)
		-> core::agent_profile
	{
		auto profile               = core::built_in_agent_profiles().at(2U);
		profile.Executable         = std::move(aExecutable);
		profile.Protocol           = aProtocol;
		profile.AllowedEnvironment = { "BPR_CANARY_AGENT_IDENTITY", "BPR_CANARY_ALLOWED" };
		return profile;
	}

	[[nodiscard]] auto parent(const std::string& aName) -> std::optional<std::string>
	{
		return qEnvironmentVariableIsSet(aName.c_str()) ? std::optional{ qEnvironmentVariable(aName.c_str()).toStdString() } : std::nullopt;
	}

	enum class entry : std::uint8_t {
		agent_panel,
		writing,
		round_send,
		round_continue,
	};

	[[nodiscard]] auto build(const entry aEntry, const core::agent_profile& aProfile, const std::filesystem::path& aWorkingDirectory)
		-> application::agent_launch_request
	{
		auto built = std::expected<application::agent_launch_request, application::agent_launch_failure>{ std::unexpected(application::agent_launch_failure::invalid_profile) };
		switch (aEntry) {
		case entry::agent_panel:
			built = application::build_agent_launch(aProfile, supplement(), parent);
			break;
		case entry::writing:
			built = application::build_agent_launch(aProfile, supplement(), parent, true);
			break;
		case entry::round_send:
			built = application::build_round_launch(aProfile, "# Round 1\n\nPlease look at retry().\n", std::nullopt, aWorkingDirectory.string(), parent);
			break;
		case entry::round_continue:
			built = application::build_round_launch(aProfile, "# Round 2\n\nAnd now?\n", std::string{ "session-7" }, aWorkingDirectory.string(), parent);
			break;
		}
		expect(built.has_value(), "the Claude request builds");
		built->WorkingDirectory = aWorkingDirectory.string();
		return *built;
	}

	[[nodiscard]] auto event_values(const application::agent_run_result& aResult, const std::string_view aTag) -> std::vector<std::string>
	{
		auto values = std::vector<std::string>{ };
		for (const auto& event : aResult.Events) {
			if (event.Text.starts_with(std::string{ aTag } + "\t")) {
				values.push_back(event.Text.substr(aTag.size() + 1U));
			}
		}
		return values;
	}

	/// Counts what reaches the real launcher, so "nothing was started" is
	/// something observed rather than assumed.
	class counting_port final : public application::agent_process_port
	{
	public:
		explicit counting_port(application::agent_process_port& aInner)
			: Inner{ aInner }
		{ }
		[[nodiscard]] auto run(const application::agent_launch_request& aRequest, const int aTimeout, const application::agent_output_sink& aSink = { },
							   std::stop_token aStop = { }) -> application::agent_run_result override
		{
			++Runs;
			Executables.push_back(aRequest.Executable);
			return Inner.run(aRequest, aTimeout, aSink, std::move(aStop));
		}
		void                     cancel() noexcept override { Inner.cancel(); }
		std::size_t              Runs = 0U;
		std::vector<std::string> Executables;

	private:
		application::agent_process_port& Inner;
	};

	[[nodiscard]] auto program_name(const std::string_view aStem) -> std::string
	{
#if defined(Q_OS_WIN)
		return std::string{ aStem } + ".exe";
#else
		return std::string{ aStem };
#endif
	}

	/// A recording wrapper: a copy of the canary at a path of its own.
	[[nodiscard]] auto copy_canary(const std::filesystem::path& aRoot, const std::string_view aStem) -> std::filesystem::path
	{
		const auto target = aRoot / program_name(aStem);
		std::filesystem::copy_file(BPR_AGENT_CANARY_EXECUTABLE, target);
		std::filesystem::permissions(target, std::filesystem::perms::owner_all);
		return target;
	}

	void write_file(const std::filesystem::path& aPath, const std::string_view aText)
	{
		auto stream = std::ofstream{ aPath, std::ios::binary };
		stream << aText;
		expect(static_cast<bool>(stream), "the scratch file is written");
	}

	[[nodiscard]] auto inspect(const std::filesystem::path& aPath) -> std::optional<agent_wrapper_refusal>
	{
		const auto text = aPath.u8string();
		return adapters::agent::inspect_claude_process_wrapper({ reinterpret_cast<const char*>(text.data()), text.size() });
	}

	[[nodiscard]] auto text(const std::filesystem::path& aPath) -> std::string
	{
		const auto value = aPath.u8string();
		return { reinterpret_cast<const char*>(value.data()), value.size() };
	}

	void test_path_checks_on_real_files(const std::filesystem::path& aRoot)
	{
		const auto program = copy_canary(aRoot, "checked-wrapper");
		expect(!inspect(program), "an absolute path to an executable regular file is accepted");
		expect(adapters::agent::inspect_claude_process_wrapper("claude") == agent_wrapper_refusal::relative, "the bare name is refused as relative");
		expect(adapters::agent::inspect_claude_process_wrapper(program_name("bin/claude")) == agent_wrapper_refusal::relative, "a relative path is refused");
		expect(adapters::agent::inspect_claude_process_wrapper("~/bin/" + program_name("claude")) == agent_wrapper_refusal::home_relative, "a ~ path is refused");
		expect(inspect(aRoot / program_name("absent")) == agent_wrapper_refusal::missing, "a missing file is refused");
		const auto folder = aRoot / program_name("folder");
		std::filesystem::create_directory(folder);
		expect(inspect(folder) == agent_wrapper_refusal::directory, "a folder is refused");
		expect(adapters::agent::inspect_claude_process_wrapper(text(program) + "\n") == agent_wrapper_refusal::control_character, "a line break is refused");
#if defined(Q_OS_UNIX)
		const auto link = aRoot / "linked-wrapper";
		std::filesystem::create_symlink(program, link);
		expect(!inspect(link), "a symbolic link that ends at an executable is accepted");
		const auto dangling = aRoot / "dangling-wrapper";
		std::filesystem::create_symlink(aRoot / "nowhere", dangling);
		expect(inspect(dangling) == agent_wrapper_refusal::missing, "a link that ends nowhere is missing");
		const auto plain = aRoot / "plain-file";
		write_file(plain, "#!/bin/sh\n");
		std::filesystem::permissions(plain, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
		expect(inspect(plain) == agent_wrapper_refusal::not_executable, "a file this user may not execute is refused");
		const auto pipe = aRoot / "pipe";
		expect(::mkfifo(pipe.c_str(), 0700) == 0, "the scratch pipe is made");
		expect(inspect(pipe) == agent_wrapper_refusal::not_regular_file, "a pipe is not a regular file");
#endif
#if defined(Q_OS_WIN)
		for (const auto* const name : { "wrapper.cmd", "wrapper.bat", "WRAPPER.CMD" }) {
			const auto script = aRoot / name;
			write_file(script, "@echo off\r\n");
			expect(inspect(script) == agent_wrapper_refusal::windows_script, "a .bat or .cmd wrapper is refused, it would run through cmd.exe");
		}
		const auto script = aRoot / "wrapper.ps1";
		write_file(script, "exit 0\r\n");
		expect(inspect(script) == agent_wrapper_refusal::windows_not_program, "a file that is not an .exe is refused");
#endif
	}

	void test_wrapper_receives_exactly_claudes_launch(const std::filesystem::path& aRoot)
	{
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		qputenv("BPR_WRAPPER_PARENT_ONLY", QByteArray{ "never-crosses" });
		const auto wrapper = copy_canary(aRoot, "recording-wrapper");
		auto       source  = std::make_shared<application::claude_process_wrapper_source>();
		source->set(text(wrapper));
		auto launcher = adapters::agent::agent_process_launcher{ };
		auto counted  = counting_port{ launcher };
		auto wrapped  = application::wrapped_claude_process{ counted, source, &adapters::agent::inspect_claude_process_wrapper };
		for (const auto kind : { entry::agent_panel, entry::writing, entry::round_send, entry::round_continue }) {
			// Today: the profile names the program that runs.
			const auto today = build(kind, claude_profile(BPR_AGENT_CANARY_EXECUTABLE), aRoot);
			const auto was   = launcher.run(today, 20'000);
			expect(was.Outcome == application::agent_run_outcome::success && !event_values(was, "argv").empty(), "today's Claude launch runs and reports itself");
			// Shipped: the profile names the bare `claude`, the wrapper is saved.
			const auto shipped = build(kind, claude_profile("claude"), aRoot);
			auto       same    = shipped;
			same.Executable    = today.Executable;
			expect(same == today, "the shipped request differs from today's in its executable alone");
			// Failing-before control: the shipped request run as it is.
			const auto bare = launcher.run(shipped, 20'000);
			expect(bare.Outcome == application::agent_run_outcome::failed_to_start, "without a wrapper the shipped bare claude cannot start");
			const auto before = counted.Runs;
			const auto now    = wrapped.run(shipped, 20'000);
			expect(now.Outcome == application::agent_run_outcome::success, "the wrapper runs in place of claude");
			expect(counted.Runs == before + 1U && counted.Executables.back() == text(wrapper), "the one process started is the wrapper");
			auto argv    = event_values(now, "argv");
			auto argvWas = event_values(was, "argv");
			expect(!argv.empty() && argv.front() == text(wrapper), "argv[0] is the wrapper path");
			expect(argv.size() == argvWas.size() && std::equal(argv.begin() + 1, argv.end(), argvWas.begin() + 1), "the wrapper gets exactly Claude's arguments, with nothing added");
			// The canary reports a repeated argument once, so the first is
			// compared directly: nothing sits between argv[0] and Claude's own.
			expect(argv.size() > 1U && argv.at(1) == shipped.Arguments.front(), "no bundled path or other argument is prepended");
			auto env    = event_values(now, "env");
			auto envWas = event_values(was, "env");
			std::ranges::sort(env);
			std::ranges::sort(envWas);
			expect(env == envWas, "the wrapper's environment is Claude's, name for name and value for value");
			expect(std::ranges::none_of(env, [](const std::string& aEntry) { return aEntry.starts_with("BPR_WRAPPER_PARENT_ONLY="); }), "no parent name outside the allowlist crosses");
			expect(event_values(now, "cwd") == event_values(was, "cwd"), "the wrapper runs in Claude's working directory");
			expect(event_values(now, "stdin-hex") == event_values(was, "stdin-hex") && !event_values(now, "stdin-hex").front().empty(), "the wrapper reads Claude's exact input");
		}
	}

#if defined(Q_OS_UNIX)
	/// A Unix wrapper script with a `#!` line is started by the kernel from
	/// the file. Its interpreter adds variables of its own, so this course
	/// compares what the wrapper passes on: Claude's arguments and input.
	void test_unix_script_wrapper(const std::filesystem::path& aRoot)
	{
		const auto script = aRoot / "claude-gateway";
		write_file(script, std::string{ "#!/bin/sh\nexec '" } + BPR_AGENT_CANARY_EXECUTABLE + "' \"$@\"\n");
		std::filesystem::permissions(script, std::filesystem::perms::owner_all);
		auto source = std::make_shared<application::claude_process_wrapper_source>();
		source->set(text(script));
		auto       launcher = adapters::agent::agent_process_launcher{ };
		auto       wrapped  = application::wrapped_claude_process{ launcher, source, &adapters::agent::inspect_claude_process_wrapper };
		const auto today    = launcher.run(build(entry::agent_panel, claude_profile(BPR_AGENT_CANARY_EXECUTABLE), aRoot), 20'000);
		const auto now      = wrapped.run(build(entry::agent_panel, claude_profile("claude"), aRoot), 20'000);
		expect(now.Outcome == application::agent_run_outcome::success, "a #! script wrapper runs without a shell string");
		const auto argv    = event_values(now, "argv");
		const auto argvWas = event_values(today, "argv");
		expect(argv.size() == argvWas.size() && std::equal(argv.begin() + 1, argv.end(), argvWas.begin() + 1), "the script passes Claude's exact arguments on");
		expect(event_values(now, "stdin-hex") == event_values(today, "stdin-hex"), "the script passes Claude's exact input on");
	}
#endif

	void test_launch_recheck(const std::filesystem::path& aRoot)
	{
		const auto wrapper = copy_canary(aRoot, "removed-wrapper");
		auto       source  = std::make_shared<application::claude_process_wrapper_source>();
		source->set(text(wrapper));
		auto       launcher = adapters::agent::agent_process_launcher{ };
		auto       counted  = counting_port{ launcher };
		auto       wrapped  = application::wrapped_claude_process{ counted, source, &adapters::agent::inspect_claude_process_wrapper };
		const auto request  = build(entry::agent_panel, claude_profile("claude"), aRoot);
		expect(wrapped.run(request, 20'000).Outcome == application::agent_run_outcome::success, "the saved wrapper runs");
#if defined(Q_OS_UNIX)
		std::filesystem::permissions(wrapper, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
		const auto blocked = wrapped.run(request, 20'000);
		expect(blocked.Outcome == application::agent_run_outcome::failed_to_start, "a wrapper made non-executable fails to start");
		expect(blocked.StandardError == application::describe_wrapper_refusal(text(wrapper), agent_wrapper_refusal::not_executable), "the failure names the wrapper and why");
		expect(counted.Runs == 1U, "no process is attempted for a non-executable wrapper");
#endif
		std::filesystem::remove(wrapper);
		const auto gone = wrapped.run(request, 20'000);
		expect(gone.Outcome == application::agent_run_outcome::failed_to_start, "a removed wrapper fails to start");
		expect(gone.StandardError.find(text(wrapper)) != std::string::npos && gone.StandardError.find("Nothing exists") != std::string::npos, "the failure names the removed wrapper");
		expect(gone.Session == request.Session, "the failure is the request's own");
		expect(counted.Runs == 1U && counted.Executables == std::vector<std::string>{ text(wrapper) }, "no process is attempted, the bare claude included");
	}

	void test_other_harnesses_unchanged(const std::filesystem::path& aRoot)
	{
		const auto wrapper = copy_canary(aRoot, "ignored-wrapper");
		auto       source  = std::make_shared<application::claude_process_wrapper_source>();
		source->set(text(wrapper));
		auto launcher = adapters::agent::agent_process_launcher{ };
		auto counted  = counting_port{ launcher };
		auto wrapped  = application::wrapped_claude_process{ counted, source, &adapters::agent::inspect_claude_process_wrapper };
		for (const auto protocol : { core::agent_protocol::codex_jsonl_exec, core::agent_protocol::codex_app_server, core::agent_protocol::external_terminal }) {
			const auto request = build(entry::round_send, claude_profile(BPR_AGENT_CANARY_EXECUTABLE, protocol), aRoot);
			const auto result  = wrapped.run(request, 20'000);
			expect(counted.Executables.back() == request.Executable, "a non-Claude launch keeps its own executable with a wrapper saved");
			const auto argv = event_values(result, "argv");
			expect(!argv.empty() && argv.front() == request.Executable, "the non-Claude child is the profile's program");
		}
		// No wrapper saved: a Claude request passes through untouched.
		source->set({ });
		const auto request = build(entry::agent_panel, claude_profile(BPR_AGENT_CANARY_EXECUTABLE), aRoot);
		expect(wrapped.run(request, 20'000).Outcome == application::agent_run_outcome::success && counted.Executables.back() == request.Executable,
			   "with no wrapper the Claude request runs as built");
	}
}

auto main(int argc, char** argv) -> int
try {
	QCoreApplication                application{ argc, argv };
	const auto                      root = bpr::tests::make_scratch_directory("bpr-claude-wrapper");
	const bpr::tests::scratch_guard owned{ root };
	test_path_checks_on_real_files(root);
	test_wrapper_receives_exactly_claudes_launch(root);
#if defined(Q_OS_UNIX)
	test_unix_script_wrapper(root);
#endif
	test_launch_recheck(root);
	test_other_harnesses_unchanged(root);
	std::cout << "claude-process-wrapper-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
