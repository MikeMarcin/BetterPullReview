/// \file
/// \brief A stand-in agent that reports exactly what it was started with.
///
/// \details The canary RFC-BPR-0011's acceptance criteria name. It runs no
/// model, opens no network, and reads no repository; it writes its own argv,
/// its own environment, and everything it received on standard input, so a
/// test can assert on all three **in full** rather than searching them for one
/// known secret.
///
/// Reading them in full is the point. A test that grepped the child's
/// environment for `GITHUB_TOKEN` would pass against a child that inherited
/// `AWS_SECRET_ACCESS_KEY` instead.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
extern "C" char** _environ;
	#define BPR_ENVIRON _environ
#else
extern "C" char** environ;
	#define BPR_ENVIRON environ
#endif

auto main(int argc, char** argv) -> int
{
	// One record per line, so a reader never has to guess where a value ends.
	for (auto index = 0; index < argc; ++index) {
		std::cout << "argv\t" << argv[index] << '\n';
	}
	for (auto** entry = BPR_ENVIRON; entry != nullptr && *entry != nullptr;
		 ++entry) {
		std::cout << "env\t" << *entry << '\n';
	}
	auto line = std::string{ };
	while (std::getline(std::cin, line)) {
		std::cout << "stdin\t" << line << '\n';
	}
	for (auto index = 0; index < argc; ++index) {
		// Only ever passed by the cancellation case, which needs a child that
		// outlives its bound so that terminating one can be observed.
		if (std::string_view{ argv[index] } == "--wait-forever") {
			std::cout.flush();
			std::this_thread::sleep_for(std::chrono::hours{ 1 });
		}
	}
	std::cout << "canary-complete\n";
	return EXIT_SUCCESS;
}
