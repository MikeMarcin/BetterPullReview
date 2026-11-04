// [[BPR-1850]]. `pull-request-activity.sqlite3`: the activity setting,
// `Last-Modified` and seen threads round-trip through a relaunch, damage
// degrades to a new baseline, and a newer build's rows are left for it.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/pull_request_activity_store.hpp"
#include "bpr/adapters/sqlite/storage_diagnostics.hpp"
#include "bpr/core/operation_diagnosis.hpp"
#include "bpr/core/profile_data_reset.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using tests::expect;

	void tamper(const std::filesystem::path& aRoot, const std::string_view aSql)
	{
		sqlite3*   raw    = nullptr;
		const auto path   = (aRoot / "state/v1/pull-request-activity.sqlite3").string();
		const auto opened = sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
		auto       db     = std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)>{ raw, sqlite3_close_v2 };
		expect(opened == SQLITE_OK, "the store file opens for tampering");
		expect(sqlite3_exec(db.get(), std::string{ aSql }.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "the tampering statement runs");
	}

	auto memory() -> core::activity_memory
	{
		auto value         = core::activity_memory{ };
		value.Baselined    = true;
		value.Account      = "github.com/42";
		value.LastModified = "Fri, 30 Oct 2026 04:02:16 GMT";
		value.Seen         = { { .Thread = "101", .UpdatedAt = 1793707200 }, { .Thread = "102", .UpdatedAt = 1793707260 } };
		return value;
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "pull-request-activity" };
	const auto&                          root = scratch.path();

	const auto empty = adapters::sqlite::load_pull_request_activity(root);
	expect(empty == core::activity_memory_read{ } && empty.Memory.Enabled, "a profile with no store reads a fresh memory, enabled and writable");

	auto value = memory();
	expect(adapters::sqlite::store_pull_request_activity(root, value), "the memory is written");
	expect(std::filesystem::exists(root / "state/v1/pull-request-activity.sqlite3"), "into its own file under state/v1");
	const auto relaunched = adapters::sqlite::load_pull_request_activity(root);
	expect(relaunched.Memory == value && relaunched.Writable && !relaunched.Recovered, "a relaunch reads it back whole");

	value.Enabled = false;
	value.Seen    = { { .Thread = "103", .UpdatedAt = 1793707300 } };
	expect(adapters::sqlite::store_pull_request_activity(root, value), "a later write replaces it");
	expect(adapters::sqlite::load_pull_request_activity(root).Memory == value, "seen threads included");

	tamper(root, "INSERT INTO activity_seen VALUES('not-a-thread', 1)");
	const auto damaged = adapters::sqlite::load_pull_request_activity(root);
	expect(damaged.Recovered && damaged.Memory.Seen == value.Seen && !damaged.Memory.Enabled, "a damaged seen row is dropped and reported");
	expect(adapters::sqlite::store_pull_request_activity(root, damaged.Memory) && !adapters::sqlite::load_pull_request_activity(root).Recovered, "the next write repairs it");

	tamper(root, "UPDATE activity SET version=0 WHERE singleton=1");
	const auto older = adapters::sqlite::load_pull_request_activity(root);
	expect(older.Recovered && older.Writable && !older.Memory.Baselined && !older.Memory.Enabled, "an obsolete version starts a new baseline, keeping the setting");

	value.Enabled = true;
	expect(adapters::sqlite::store_pull_request_activity(root, value), "and is rewritten");
	tamper(root, "UPDATE activity SET version=2,last_modified='x' WHERE singleton=1");
	const auto newer = adapters::sqlite::load_pull_request_activity(root);
	expect(!newer.Writable && newer.Recovered && newer.Memory == core::activity_memory{ }, "a newer build's rows read as nothing and are not writable");
	expect(!adapters::sqlite::store_pull_request_activity(root, memory()), "a write refuses to replace them");
	tamper(root, "UPDATE activity SET version=1 WHERE singleton=1 AND version=2 AND last_modified='x'");
	const auto kept = adapters::sqlite::load_pull_request_activity(root);
	expect(kept.Memory.Seen == value.Seen, "and the newer build's threads were left as they were");

	{
		auto       foreign = tests::sqlite_contract::scratch_root{ "pull-request-activity-foreign" };
		const auto file    = foreign.path() / "state/v1/pull-request-activity.sqlite3";
		std::filesystem::create_directories(file.parent_path());
		{
			auto out = std::ofstream{ file, std::ios::binary };
			out << std::string(8192U, 'x');
		}
		std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
		const auto garbage = adapters::sqlite::load_pull_request_activity(foreign.path());
		expect(garbage.Recovered && garbage.Memory.Enabled && !garbage.Memory.Baselined, "a damaged file reads as a fresh memory, reported");
		expect(!adapters::sqlite::store_pull_request_activity(foreign.path(), memory()), "and is not written over");
	}

	expect(adapters::sqlite::storage_store_of("pull-request-activity.sqlite3") == core::operation_store::pull_request_activity, "storage lines name the store");
	expect(core::profile_data_store_area("pull-request-activity") == core::profile_data_area::settings, "Reset settings removes it");

	std::cout << "pull-request-activity-store:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "FAILED: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
