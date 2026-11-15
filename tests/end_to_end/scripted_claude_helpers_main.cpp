// [[BPR-2004]], [[BPR-2005]], RFC-BPR-0103. A scripted Claude runtime for the
// Agent tab's helper courses: it reads its question and replays the frames
// installed Claude Code 2.1.319 wrote when asked to start three background
// helpers, say so and end its message without waiting
// (`tests/fixtures/agent/traces/claude-2.1.319-helpers-stdin-open.jsonl`).
//
// Built twice. `BPR_SCRIPT_UNFINISHED` stops after its message's first result,
// with two helpers still out, and exits: the runtime ending under its helpers.
// Otherwise it pauses before each part of the turn that answers a helper's
// report, so the running block can be seen with helpers out, then replays to
// the end and waits for its input to close. `BPR_SCRIPT_ATTENDED`, for the
// owner's look, replays slowly and sends nothing for 130 seconds before its
// last part, so the quiet notice shows.

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#if !defined(BPR_AGENT_TRACE_DIR)
	#error "BPR_AGENT_TRACE_DIR names the recorded traces"
#endif

auto main() -> int
{
	auto question = std::string{ };
	if (!std::getline(std::cin, question) || question.empty()) {
		return EXIT_FAILURE;
	}
	auto trace = std::ifstream{ std::string{ BPR_AGENT_TRACE_DIR } + "/claude-2.1.319-helpers-stdin-open.jsonl" };
#if defined(BPR_SCRIPT_ATTENDED)
	constexpr auto between = std::chrono::milliseconds{ 400 };
	constexpr auto part    = std::chrono::milliseconds{ 6'000 };
	constexpr auto quiet   = std::chrono::milliseconds{ 130'000 };
#else
	constexpr auto between = std::chrono::milliseconds{ 15 };
	constexpr auto part    = std::chrono::milliseconds{ 1'500 };
	constexpr auto quiet   = part;
#endif
	auto parts = 0;
	for (auto line = std::string{ }; std::getline(trace, line);) {
		const auto init   = line.find("\"subtype\":\"init\"") != std::string::npos;
		const auto result = line.find("\"type\":\"result\"") != std::string::npos;
		if (init && parts++ > 0) {
			// The trace's fourth part is its last: the quiet comes before it.
			std::this_thread::sleep_for(parts == 4 ? quiet : part);
		}
		std::cout << line << '\n'
				  << std::flush;
		std::this_thread::sleep_for(between);
#if defined(BPR_SCRIPT_UNFINISHED)
		if (result) {
			return EXIT_SUCCESS;
		}
#else
		static_cast<void>(result);
#endif
	}
	// A persistent session holds its input open; it ends when that closes.
	for (auto rest = std::string{ }; std::getline(std::cin, rest);) {
	}
	return EXIT_SUCCESS;
}
