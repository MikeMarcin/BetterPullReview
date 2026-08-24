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
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
} // namespace

int main()
{
	namespace sqlite_adapter = bpr::adapters::sqlite;
	std::error_code error;
	const auto      base = std::filesystem::temp_directory_path(error);
	expect(!error, "temporary directory is available");
	const auto root = base
		/ ("bpr-contained-catalog-probe-"
		   + std::to_string(
			   std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root, error);
	expect(!error, "probe root creates");

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

	std::filesystem::remove_all(root, error);
	std::cout << "CONTAINED_CATALOG_PROBE_OK:" << code << ":"
			  << elapsed.count() << "s\n";
	return EXIT_SUCCESS;
}
