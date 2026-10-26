#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
	#include <process.h>
	#include <windows.h>

	#include <tlhelp32.h>

#else
	#include <csignal>
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
		// The adapter defines the reply's wire encoding as UTF-8: it re-encodes
		// its expectation with u8string(), so a native-code-page reply can
		// never byte-match a non-ASCII parent directory on Windows (BPR-0476).
		// On POSIX u8string() yields the same bytes string() did.
		const auto reply = executable.parent_path().u8string();
		std::cout.write(
			reinterpret_cast<const char*>(reply.data()),
			static_cast<std::streamsize>(reply.size()));
		std::cout << '\n';
		return EXIT_SUCCESS;
	}
	const auto hasArgument = [argc, argv](const std::string_view aExpected) {
		return std::any_of(argv + 1, argv + argc, [aExpected](const char* aArgument) {
			return std::string_view{ aArgument } == aExpected;
		});
	};
	// [[BPR-1769]]. A releasable local checkpoint lets the composed setup
	// prove that workspace progress outlives the independent agent check.
	const auto hold = executable.parent_path() / ".bpr-hold-workspace";
	if (hasArgument("pack-objects") && std::filesystem::is_regular_file(hold)) {
		auto waiting = std::ofstream{ executable.parent_path() / ".bpr-workspace-waiting" };
		waiting << "waiting";
		waiting.close();
		for (auto attempt = 0; attempt < 2'000 && std::filesystem::is_regular_file(hold); ++attempt) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
		}
		if (std::filesystem::is_regular_file(hold)) {
			return 126;
		}
	}
	if (
		hasArgument("pack-objects") && hasArgument("--revs")
		&& std::filesystem::is_regular_file(
			executable.parent_path() / workspace_failure_marker)) {
		std::cerr << raw_git_diagnostic;
		return 128;
	}

#if defined(_WIN32)
	// [[BPR-0622]]. The same materialization checkpoint. Windows has no SIGSTOP,
	// so the paused writer blocks here, alive inside its supervisor's job. It
	// reports its own and its parent's process IDs, then waits until the course
	// has opened handles to both; the course reads liveness from those handles,
	// never from PID existence. Only then does it publish the workspace the
	// application's checkpoint waits for.
	const auto checkpoint = executable.parent_path() / ".bpr-pause-workspace";
	if (std::filesystem::is_regular_file(checkpoint)) {
		auto        input = std::ifstream{ checkpoint };
		std::string command;
		std::getline(input, command);
		if ((command == "init" || command == "checkout-index") && hasArgument(command)) {
			const auto root      = executable.parent_path().parent_path();
			auto       workspace = std::filesystem::path{ argv[argc - 1] };
			if (command == "checkout-index") {
				for (int index = 1; index + 1 < argc; ++index) {
					if (std::string_view{ argv[index] } == "-C") {
						workspace = argv[index + 1];
					}
				}
			}
			if (!workspace.is_absolute() || !std::filesystem::is_directory(workspace)) {
				return 126;
			}
			auto       parent   = DWORD{ 0U };
			const auto self     = GetCurrentProcessId();
			const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0U);
			if (snapshot != INVALID_HANDLE_VALUE) {
				auto entry   = PROCESSENTRY32W{ };
				entry.dwSize = sizeof(entry);
				for (auto found = Process32FirstW(snapshot, &entry); found != FALSE; found = Process32NextW(snapshot, &entry)) {
					if (entry.th32ProcessID == self) {
						parent = entry.th32ParentProcessID;
						break;
					}
				}
				static_cast<void>(CloseHandle(snapshot));
			}
			if (parent == 0U) {
				return 126;
			}
			const auto publish = [&root](const std::string& aName, const std::string& aValue) {
				auto output = std::ofstream{ root / aName, std::ios::binary };
				output << aValue;
				output.close();
				return static_cast<bool>(output);
			};
			if (!publish("git.pid", std::to_string(self)) || !publish("supervisor.pid", std::to_string(parent))) {
				return 126;
			}
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 60 };
			while (!std::filesystem::exists(root / "handles-captured")) {
				if (std::chrono::steady_clock::now() >= deadline) {
					return 126;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
			}
			if (!publish("workspace-parent", workspace.parent_path().string())) {
				return 126;
			}
			for (;;) {
				Sleep(1'000U);
			}
		}
	}
#endif

#if !defined(_WIN32)
	// The containing composed fixture arms exactly one materialization stage.
	// Stop before exec so a resumed unsupervised `git init` could recreate a
	// checkout that recovery incorrectly removed while this writer survived.
	const auto checkpoint = executable.parent_path() / ".bpr-pause-workspace";
	if (std::filesystem::is_regular_file(checkpoint)) {
		auto        input = std::ifstream{ checkpoint };
		std::string command;
		std::getline(input, command);
		if ((command == "init" || command == "checkout-index") && hasArgument(command)) {
			const auto root      = executable.parent_path().parent_path();
			auto       workspace = std::filesystem::path{ argv[argc - 1] };
			if (command == "checkout-index") {
				for (int index = 1; index + 1 < argc; ++index) {
					if (std::string_view{ argv[index] } == "-C") {
						workspace = argv[index + 1];
					}
				}
			}
			if (!workspace.is_absolute() || !std::filesystem::is_directory(workspace)) {
				return 126;
			}
			for (const auto& [name, value] : std::vector<std::pair<std::string, std::string>>{
					 { "git.pid", std::to_string(::getpid()) },
					 { "supervisor.pid", std::to_string(::getppid()) },
					 { "workspace-parent", workspace.parent_path().string() } }) {
				auto output = std::ofstream{ root / name };
				output << value;
				output.close();
				if (!output) {
					return 126;
				}
			}
			static_cast<void>(::raise(SIGSTOP));
		}
	}
#endif

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
