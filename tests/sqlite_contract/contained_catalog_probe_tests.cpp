// BPR-0427: inside an AppContainer the ancestor chain is unreadable
// (BPR-0362), and the catalog once answered that by hanging — thirty
// seconds of silence for the storage contract, three minutes for the
// behavior contract, where its sibling stores failed fast. This probe runs
// *contained* on Windows, deliberately: it hands the catalog a root the
// container can reach, requires a typed answer — construction or a typed
// refusal — inside a bound far below the registration's timeout, and
// treats an escaped exception as the failure it is. On the other
// platforms the sandbox leaves the chain readable and the same probe
// asserts the constructive answer arrives just as promptly.
#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace {
	using bpr::tests::expect;
} // namespace

int main()
try {
	namespace sqlite_adapter = bpr::adapters::sqlite;
	// [[BPR-1128]], [[BPR-1138]]. Made at a name nothing had, owner-private from creation, and
	// owned from here, so every way out removes it.
	const auto                      root = bpr::tests::make_scratch_directory("bpr-contained-catalog-probe");
	const bpr::tests::scratch_guard owned{ root };

	const auto started = std::chrono::steady_clock::now();
	auto       typed   = false;
	auto       code    = std::string{ };
	try {
		const auto made = sqlite_adapter::make_epoch_catalog({
			.StorageRoot = root,
		});
		typed           = true;
		code            = made.has_value()
			? std::string{ "constructed" }
			: std::to_string(static_cast<unsigned int>(made.error()));
	} catch (const std::exception& caught) {
		std::cerr << "FAILED: catalog construction threw instead of "
					 "returning a typed failure: "
				  << caught.what() << '\n';
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "FAILED: catalog construction threw an untyped "
					 "exception\n";
		return EXIT_FAILURE;
	}
	const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - started);
	expect(typed, "the catalog answered");
	expect(
		elapsed < std::chrono::seconds{ 20 },
		"the catalog answers in bounded time rather than hanging");

	std::cout << "CONTAINED_CATALOG_PROBE_OK:" << code << ":"
			  << elapsed.count() << "s\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
