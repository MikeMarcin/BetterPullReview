#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
	#include <process.h>
	#include <windows.h>
#else
	#include <unistd.h>
#endif

namespace {
	inline constexpr auto workspace_failure_marker = ".bpr-fail-workspace-pack";
	inline constexpr auto raw_git_diagnostic
		= "fatal: could not materialize /tmp/bpr-agent-workspace from "
		  "https://fixture-token@remote.invalid/owner/repo.git\n";
}

auto main(int argc, char** argv) -> int
{
#if defined(_WIN32)
	// The adapter's exec-path handshake compares reply bytes exactly, and
	// the CRT's default text mode rewrites the reply's '\n' as "\r\n": the
	// adapter strips one trailing byte, the dangling '\r' fails its
	// earliest wire check, and every git call in the course reads as a
	// process failure (BPR-0240). Same repair as the agent canary.
	for (FILE* stream : { stdin, stdout, stderr }) {
		const auto descriptor = _fileno(stream);
		if (descriptor >= 0) {
			static_cast<void>(_setmode(descriptor, _O_BINARY));
		}
	}
#endif
	std::error_code pathError;
#if defined(_WIN32)
	// Real git's fsck spawns its own subcommands - "git refs verify",
	// "git commit-graph verify", "git multi-pack-index verify" - with a bare
	// "git" argv[0] resolved through GIT_EXEC_PATH, so those children re-enter
	// this wrapper under a name canonical() cannot resolve and the wrapper
	// exited 126 before forwarding (BPR-0240). The module path names this
	// image regardless of how it was invoked.
	auto       moduleName   = std::wstring(1U << 15U, L'\0');
	const auto moduleLength = GetModuleFileNameW(
		nullptr,
		moduleName.data(),
		static_cast<DWORD>(moduleName.size()));
	if (moduleLength == 0U || moduleLength >= moduleName.size()) {
		return 126;
	}
	moduleName.resize(moduleLength);
	const auto executable = std::filesystem::canonical(
		std::filesystem::path{ moduleName },
		pathError);
#else
	const auto executable = std::filesystem::canonical(argv[0], pathError);
#endif
	if (pathError) {
		return 126;
	}

	if (argc == 2 && std::string_view{ argv[1] } == "--exec-path") {
		std::cout << executable.parent_path().string() << '\n';
		return EXIT_SUCCESS;
	}
	const auto hasArgument = [argc, argv](const std::string_view aExpected) {
		return std::any_of(argv + 1, argv + argc, [aExpected](const char* aArgument) {
			return std::string_view{ aArgument } == aExpected;
		});
	};
	if (
		hasArgument("pack-objects") && hasArgument("--revs")
		&& std::filesystem::is_regular_file(
			executable.parent_path() / workspace_failure_marker)) {
		std::cerr << raw_git_diagnostic;
		return 128;
	}

#if defined(_WIN32)
	// _execv cannot replace a Windows process image: the CRT spawns the new
	// process and destroys this one at once, so the wrapper's caller
	// observes the wrapper exit while git still runs, and git's exit code
	// is lost. Spawning and waiting keeps both. The CRT also joins the
	// argument vector with spaces and no quoting, so every argument is
	// quoted whole; the harness passes no argument with an embedded quote.
	auto quoted = std::vector<std::string>{ };
	quoted.reserve(static_cast<std::size_t>(argc));
	quoted.push_back("\"" + std::string{ BPR_REAL_GIT_EXECUTABLE } + "\"");
	for (auto index = 1; index < argc; ++index) {
		quoted.push_back("\"" + std::string{ argv[index] } + "\"");
	}
	auto forwarded = std::vector<char*>{ };
	forwarded.reserve(quoted.size() + 1U);
	for (auto& argument : quoted) {
		forwarded.push_back(argument.data());
	}
	forwarded.push_back(nullptr);
	const auto status
		= ::_spawnv(_P_WAIT, BPR_REAL_GIT_EXECUTABLE, forwarded.data());
	return status < 0 ? 126 : static_cast<int>(status);
#else
	auto forwarded = std::vector<char*>{ };
	forwarded.reserve(static_cast<std::size_t>(argc) + 1U);
	forwarded.push_back(const_cast<char*>(BPR_REAL_GIT_EXECUTABLE));
	for (auto index = 1; index < argc; ++index) {
		forwarded.push_back(argv[index]);
	}
	forwarded.push_back(nullptr);
	::execv(BPR_REAL_GIT_EXECUTABLE, forwarded.data());
	return 126;
#endif
}
