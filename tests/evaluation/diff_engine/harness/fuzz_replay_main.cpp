// [[RFC-BPR-0082]] [[BPR-1466]]. Replays a libFuzzer corpus directory through
// one target's entry point without libFuzzer, so the evaluation build can run
// the diff fuzz target's corpus against each candidate. Prints the number of
// inputs replayed; any failure inside the target aborts the process.
//
// Usage: tool_diff_fuzz_replay <corpus directory>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, std::size_t aSize) -> int;

auto main(const int aArgc, char** aArgv) -> int
{
	if (aArgc != 2) {
		std::fputs("usage: tool_diff_fuzz_replay <corpus directory>\n", stderr);
		return 2;
	}
	auto error = std::error_code{ };
	auto paths = std::vector<std::filesystem::path>{ };
	for (const auto& entry : std::filesystem::directory_iterator{ aArgv[1], error }) {
		if (entry.is_regular_file()) {
			paths.push_back(entry.path());
		}
	}
	if (error) {
		std::fputs("cannot read the corpus directory\n", stderr);
		return 2;
	}
	std::ranges::sort(paths);
	for (const auto& path : paths) {
		auto stream = std::ifstream{ path, std::ios::binary };
		auto bytes  = std::vector<std::uint8_t>{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
		static_cast<void>(LLVMFuzzerTestOneInput(bytes.data(), bytes.size()));
	}
	std::printf("FUZZ_REPLAY_INPUTS %zu\n", paths.size());
	return 0;
}
