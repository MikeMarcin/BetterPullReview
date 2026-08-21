#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

#if defined(_WIN32)
	#include <process.h>
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
	std::error_code pathError;
	const auto      executable = std::filesystem::canonical(argv[0], pathError);
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

	auto forwarded = std::vector<char*>{ };
	forwarded.reserve(static_cast<std::size_t>(argc) + 1U);
	forwarded.push_back(const_cast<char*>(BPR_REAL_GIT_EXECUTABLE));
	for (auto index = 1; index < argc; ++index) {
		forwarded.push_back(argv[index]);
	}
	forwarded.push_back(nullptr);
#if defined(_WIN32)
	::_execv(BPR_REAL_GIT_EXECUTABLE, forwarded.data());
#else
	::execv(BPR_REAL_GIT_EXECUTABLE, forwarded.data());
#endif
	return 126;
}
