// [[BPR-2171]]. `discuss.sqlite3`: each repository's bot marks and the
// column's width round-trip through a relaunch; a damaged mark is left out;
// a newer build's store is left for that build.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/discuss_preferences_store.hpp"
#include "bpr/core/profile_data_reset.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
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
		const auto path   = (aRoot / "state/v1/discuss.sqlite3").string();
		const auto opened = sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
		auto       db     = std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)>{ raw, sqlite3_close_v2 };
		expect(opened == SQLITE_OK, "the store file opens for tampering");
		expect(sqlite3_exec(db.get(), std::string{ aSql }.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "the tampering statement runs");
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "discuss-preferences" };
	const auto&                          root = scratch.path();

	expect(adapters::sqlite::load_discuss_preferences(root) == core::discuss_preferences_read{ }, "a profile with no store reads nothing chosen, writable");

	const auto repository = core::repository_mark_key("github.com", "bpr-fixtures", "conversations");
	auto       chosen     = core::mark_author({ }, repository, "renovate-helper", true);
	chosen                = core::mark_author(std::move(chosen), repository, "codecov[bot]", false);
	chosen.Column         = core::discuss_column::wide;
	expect(adapters::sqlite::store_discuss_preferences(root, chosen), "the preferences are written");
	expect(std::filesystem::exists(root / "state/v1/discuss.sqlite3"), "into their own file under state/v1");
	const auto relaunched = adapters::sqlite::load_discuss_preferences(root);
	expect(relaunched.Preferences == chosen && !relaunched.Recovered && relaunched.Writable, "a relaunch reads them as written");

	chosen = core::mark_author(std::move(chosen), repository, "renovate-helper", false);
	expect(adapters::sqlite::store_discuss_preferences(root, chosen) && adapters::sqlite::load_discuss_preferences(root).Preferences == chosen, "a removed mark is gone after the next write");

	tamper(root, "INSERT INTO author_marks VALUES('github.com/a/b','someone',7)");
	const auto damaged = adapters::sqlite::load_discuss_preferences(root);
	expect(damaged.Recovered && damaged.Preferences == chosen, "a damaged mark is left out and reported");

	tamper(root, "UPDATE discuss SET version=2 WHERE singleton=1");
	const auto newer = adapters::sqlite::load_discuss_preferences(root);
	expect(!newer.Writable && newer.Preferences == core::discuss_preferences{ }, "a newer build's store reads as nothing and is not writable");
	expect(!adapters::sqlite::store_discuss_preferences(root, chosen), "a write refuses to replace it");

	expect(core::profile_data_store_area("discuss") == core::profile_data_area::settings, "Reset settings removes it");

	std::cout << "discuss-preferences-store:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
