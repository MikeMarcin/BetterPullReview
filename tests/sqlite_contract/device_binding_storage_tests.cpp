#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"

#include <sqlite3.h>
#if !defined(_WIN32)
	#include <sys/wait.h>
	#include <unistd.h>
#endif
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	void expect(bool aCondition, std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	auto record(std::uint64_t aProfile = 1U) -> application::device_binding_record
	{
		return { .Authority = { .Profile = core::host_profile_id{ aProfile }, .ProfileRevision = application::github_profile_revision{ 2U }, .Host = core::git_host_id{ 3U }, .Hostname = application::github_canonical_hostname{ "github.com" }, .Account = core::github_account_id{ 4U } }, .Lifetime = { .IssuedAt = 1000, .AccessExpiresAt = 4600, .RefreshExpiresAt = 10000 } };
	}
	void mutate(const std::filesystem::path& aRoot, const char* aSql)
	{
		expect(adapters::sqlite::detail::sqlite_runtime_ready(), "initialize parent SQLite runtime after metadata-only children exit");
		sqlite3* database = nullptr;
		expect(sqlite3_open((aRoot / "state/v1/device-bindings.sqlite3").string().c_str(), &database) == SQLITE_OK, "open metadata for corruption test");
		expect(sqlite3_exec(database, aSql, nullptr, nullptr, nullptr) == SQLITE_OK, "apply exact corruption");
		sqlite3_close(database);
	}
	void seed(const std::filesystem::path& aRoot)
	{
		auto store = adapters::sqlite::make_device_binding_store(aRoot);
		expect(!std::filesystem::exists(aRoot / "state"), "construction performs no filesystem effect");
		expect(store->store(record(), { }).has_value(), "store metadata");
	}
	void reopen(const std::filesystem::path& aRoot)
	{
		auto store  = adapters::sqlite::make_device_binding_store(aRoot);
		auto loaded = store->load({ });
		expect(loaded && loaded->size() == 1U && loaded->front() == record(), "fresh process restores exact metadata");
		auto foreign    = record().Authority;
		foreign.Account = core::github_account_id{ 99U };
		expect(store->erase(foreign, { }).has_value(), "foreign erase succeeds as absent");
		expect(store->load({ })->front() == record(), "foreign erase cannot remove exact binding");
		expect(store->erase(record().Authority, { }).has_value(), "exact erase commits tombstone");
		store.reset();
		store = adapters::sqlite::make_device_binding_store(aRoot);
		expect(store->load({ })->front().Tombstone, "tombstone survives reopening");
		expect(store->store(record(), { }).has_value(), "new settled grant replaces tombstone");
		for (std::uint64_t index = 2U; index <= 64U; ++index) {
			expect(store->store(record(index), { }).has_value(), "bounded store accepts 64 profiles");
		}
		expect(!store->store(record(65U), { }), "capacity refuses profile 65");
	}
#if !defined(_WIN32)
	void in_child(void (*aPhase)(const std::filesystem::path&), const std::filesystem::path& aRoot)
	{
		const auto child = fork();
		expect(child >= 0, "spawn metadata-only process");
		if (child == 0) {
			aPhase(aRoot);
			_exit(EXIT_SUCCESS);
		}
		int status = 0;
		expect(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS, "metadata-only child passes");
	}
#endif
}
int main()
{
	const auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) / ("bpr-device-binding-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directory(root);
	std::filesystem::permissions(root, std::filesystem::perms::owner_all);
#if !defined(_WIN32)
	in_child(seed, root);
	in_child(reopen, root);
#else
	seed(root);
	reopen(root);
#endif
	mutate(root, "CREATE TABLE extra(value TEXT)");
	expect(!adapters::sqlite::make_device_binding_store(root)->load({ }), "unknown schema object refuses whole catalog");
	mutate(root, "DROP TABLE extra; PRAGMA ignore_check_constraints=ON; UPDATE device_binding SET access_expiry=issued");
	expect(!adapters::sqlite::make_device_binding_store(root)->load({ }), "corrupt lifetime refuses whole catalog");
	std::filesystem::remove_all(root);
	std::stop_source stop;
	stop.request_stop();
	expect(!adapters::sqlite::make_device_binding_store(root)->load(stop.get_token()), "cancelled load refuses before filesystem work");
	expect(!std::filesystem::exists(root), "cancelled load creates no root");
	std::cout << "device-binding-storage-tests:pass\n";
}
