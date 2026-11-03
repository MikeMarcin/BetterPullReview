// [[BPR-1844]]. `view-filters.sqlite3`: the remembered view filters round-trip
// through a relaunch, damaged values degrade view by view, and a newer
// build's row is left for that build.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/storage_diagnostics.hpp"
#include "bpr/adapters/sqlite/view_filter_memory_store.hpp"
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

	/// Runs one statement on the store file as another program would.
	void tamper(const std::filesystem::path& aRoot, const std::string_view aSql)
	{
		sqlite3*   raw    = nullptr;
		const auto path   = (aRoot / "state/v1/view-filters.sqlite3").string();
		const auto opened = sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
		auto       db     = std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)>{ raw, sqlite3_close_v2 };
		expect(opened == SQLITE_OK, "the store file opens for tampering");
		expect(sqlite3_exec(db.get(), std::string{ aSql }.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "the tampering statement runs");
	}

	constexpr auto only_new = core::view_filter_choice{ .OnlyNew = true, .HideResolved = false };
	constexpr auto both     = core::view_filter_choice{ .OnlyNew = true, .HideResolved = true };
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "view-filters" };
	const auto&                          root = scratch.path();

	const auto empty = adapters::sqlite::load_view_filter_memory(root);
	expect(empty == core::view_filter_memory_read{ }, "a profile with no store reads nothing remembered, writable");

	auto defaults           = core::view_filter_defaults{ };
	defaults.Discuss        = both;
	defaults.ReviewComments = core::view_filter_choice{ };
	expect(adapters::sqlite::store_view_filter_memory(root, defaults), "the memory is written");
	expect(std::filesystem::exists(root / "state/v1/view-filters.sqlite3"), "into its own file under state/v1");

	// A relaunch opens the store afresh.
	const auto relaunched = adapters::sqlite::load_view_filter_memory(root);
	expect(relaunched.Defaults == defaults && !relaunched.Recovered && relaunched.Writable, "a relaunch reads the memory as written, unchosen views included");

	defaults.Threads = only_new;
	expect(adapters::sqlite::store_view_filter_memory(root, defaults), "a later write replaces the row");
	expect(adapters::sqlite::load_view_filter_memory(root).Defaults == defaults, "and reads back whole");

	// One damaged value: only its view is lost.
	tamper(root, "UPDATE view_filters SET discuss=9 WHERE singleton=1");
	const auto damaged = adapters::sqlite::load_view_filter_memory(root);
	expect(!damaged.Defaults.Discuss && damaged.Defaults.Threads == only_new && damaged.Defaults.ReviewComments == core::view_filter_choice{ } && damaged.Recovered && damaged.Writable,
		   "an unknown value degrades its own view to nothing remembered and is reported");
	expect(adapters::sqlite::store_view_filter_memory(root, damaged.Defaults) && !adapters::sqlite::load_view_filter_memory(root).Recovered, "the next write repairs it");

	// An older version than any known: read as nothing, then rewritten.
	tamper(root, "UPDATE view_filters SET version=0 WHERE singleton=1");
	const auto older = adapters::sqlite::load_view_filter_memory(root);
	expect(older.Defaults == core::view_filter_defaults{ } && older.Recovered && older.Writable, "an obsolete version reads as nothing remembered");
	expect(adapters::sqlite::store_view_filter_memory(root, defaults) && adapters::sqlite::load_view_filter_memory(root).Defaults == defaults, "and is rewritten as this version");

	// A newer build's row is never overwritten.
	tamper(root, "UPDATE view_filters SET version=2,discuss=99 WHERE singleton=1");
	const auto newer = adapters::sqlite::load_view_filter_memory(root);
	expect(newer.Defaults == core::view_filter_defaults{ } && !newer.Writable, "a newer build's row reads as nothing and is not writable");
	expect(!adapters::sqlite::store_view_filter_memory(root, defaults), "a write refuses to replace it");
	tamper(root, "UPDATE view_filters SET version=1,discuss=3 WHERE singleton=1 AND version=2 AND discuss=99");
	expect(adapters::sqlite::load_view_filter_memory(root).Defaults.Discuss == both, "the newer row was left exactly as it was");

	// A file that is not a database reads as nothing, reported.
	{
		auto       foreign = tests::sqlite_contract::scratch_root{ "view-filters-foreign" };
		const auto file    = foreign.path() / "state/v1/view-filters.sqlite3";
		std::filesystem::create_directories(file.parent_path());
		{
			auto out = std::ofstream{ file, std::ios::binary };
			out << std::string(8192U, 'x');
		}
		std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
		const auto garbage = adapters::sqlite::load_view_filter_memory(foreign.path());
		expect(garbage.Defaults == core::view_filter_defaults{ } && garbage.Recovered, "a damaged file reads as nothing remembered, reported");
		expect(!adapters::sqlite::store_view_filter_memory(foreign.path(), defaults), "and is not written over");
	}

	expect(adapters::sqlite::storage_store_of("view-filters.sqlite3") == core::operation_store::view_filters, "storage lines name the store");
	expect(core::profile_data_store_area("view-filters") == core::profile_data_area::settings, "Reset settings removes it");

	std::cout << "view-filter-memory-store:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "FAILED: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
