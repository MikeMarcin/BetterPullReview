#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/release_update_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sqlite3.h>
#include <string_view>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	// Use the previous app's exact schema and append/read queries. A helper
	// rollback still has to open this store after the new app presents rows.
	void expect_legacy_store(const std::filesystem::path& aFile, bool aSeed)
	{
		sqlite3*                                                 raw  = nullptr;
		const auto                                               path = aFile.u8string();
		const auto                                               code = sqlite3_open_v2(reinterpret_cast<const char*>(path.c_str()), &raw, aSeed ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, nullptr);
		const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database{ raw, sqlite3_close };
		expect(code == SQLITE_OK && database, "the original database is readable in both directions");
		sqlite3_stmt* schema = nullptr;
		expect(sqlite3_prepare_v2(raw, "SELECT sql,(SELECT application_id FROM pragma_application_id),(SELECT user_version FROM pragma_user_version) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'", -1, &schema, nullptr) == SQLITE_OK,
			   "prepare original schema check");
		const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement{ schema, sqlite3_finalize };
		expect(sqlite3_step(schema) == SQLITE_ROW, "the legacy schema has one user table");
		const auto* sql = reinterpret_cast<const char*>(sqlite3_column_text(schema, 0));
		expect(sql && std::string_view{ sql } == "CREATE TABLE drafts(sequence INTEGER PRIMARY KEY,identity TEXT NOT NULL,description TEXT NOT NULL,body TEXT NOT NULL,indeterminate INTEGER NOT NULL CHECK(indeterminate IN(0,1)),UNIQUE(identity,description,body,indeterminate)) STRICT"
				   && sqlite3_column_int(schema, 1) == 1112560214 && sqlite3_column_int(schema, 2) == 1
				   && sqlite3_step(schema) == SQLITE_DONE,
			   "the previous release still recognizes the exact schema");
		if (aSeed) {
			expect(sqlite3_exec(raw, "INSERT OR IGNORE INTO drafts(identity,description,body,indeterminate) VALUES('draft:1','inline pull request head','First private draft',0),('draft:2','uncertain review','May already be published',1)", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "the previous release's writer creates two rows");
		}
	}
}
auto main(int aArgc, char** aArgv) -> int
try {
	QCoreApplication application{ aArgc, aArgv };
	QTemporaryDir    temporary;
#if defined(_WIN32)
	// [[BPR-1031]]. The store admits a root only in native spelling, so Qt's
	// '/' is made native here, as bpr::app::native_filesystem_path does for
	// the application; the wide form keeps a non-ASCII profile path intact.
	const auto root = std::filesystem::path{ QDir::toNativeSeparators(QFileInfo{ temporary.path() }.canonicalFilePath()).toStdWString() };
#else
	const auto root = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
#endif
	expect(temporary.isValid() && adapters::sqlite::make_storage_root_owner_private(root), "disposable private state");
	const auto file  = root / "state/v1/update-draft-recovery.sqlite3";
	auto       store = adapters::sqlite::make_release_update_recovery_store(root);
	expect(store->load() && store->load()->empty(), "fresh store is empty");
	store.reset();
	expect_legacy_store(file, true);
	store = adapters::sqlite::make_release_update_recovery_store(root);
	const std::vector<application::release_update_saved_draft> original{ { "draft:1", "inline pull request head", "First private draft", false }, { "draft:2", "uncertain review", "May already be published", true } };
	expect(store->save(original) && store->save(original), "exact duplicates do not lose or multiply text");
	auto changed         = original;
	changed.front().Body = "Second private draft";
	expect(store->save(changed), "later private text appends without overwriting prior recovery");
	store.reset();
	store      = adapters::sqlite::make_release_update_recovery_store(root);
	auto saved = store->load();
	expect(saved && saved->size() == 3U && (*saved)[0].Draft == original[0] && (*saved)[1].Draft == original[1] && (*saved)[2].Draft == changed[0], "restart preserves text and indeterminate state exactly");
	changed.front().Body.assign(65537U, 'x');
	expect(!store->save(changed) && store->load() == saved, "oversized save refuses atomically");
	changed.assign(application::release_update_maximum_saved_drafts + 1U, original.front());
	expect(!store->save(changed) && store->load() == saved, "row bound refuses without changing retained text");
	const auto first   = (*saved)[0].Id;
	const auto second  = (*saved)[1].Id;
	const auto revised = (*saved)[2].Id;
	expect(first != revised, "versions of the same logical identity have distinct row identities");
	expect(!(*saved)[0].Shown && !(*saved)[1].Shown && !(*saved)[2].Shown, "legacy-format rows start unshown");
	const std::array seen{ first, second };
	expect(store->mark_shown(seen), "presentation is recorded only for named rows");
	store.reset();
	store = adapters::sqlite::make_release_update_recovery_store(root);
	saved = store->load();
	expect(saved && (*saved)[0].Shown && (*saved)[1].Shown && !(*saved)[2].Shown, "only the named rows remain shown after restart");
	expect(store->save(original) && store->load() == saved, "an exact re-save preserves each row's shown state");
	auto wrong = revised;
	++wrong.Fingerprint.front();
	const std::array unknown{ first, wrong };
	expect(!store->discard(unknown) && store->load() == saved, "one unknown row refuses the entire discard without changing any other bytes");
	expect(!store->mark_shown(unknown) && store->load() == saved, "one unknown row refuses the entire shown update");
	const std::array discard{ first };
	expect(store->discard(discard), "discard one version");
	const auto survivor = store->load();
	expect(survivor && survivor->size() == 2U && (*survivor)[0] == (*saved)[1] && (*survivor)[1] == (*saved)[2],
		   "the uncertain and revised rows remain byte-identical");
	const std::array all{ second, revised };
	expect(store->discard(all) && store->load()->empty(), "discard the remaining rows in one transaction");
	expect(store->save(original), "a later update can save discarded text again");
	const auto newRows = store->load();
	expect(newRows && newRows->size() == 2U && !(*newRows)[0].Shown && !(*newRows)[1].Shown, "new saves never inherit a deleted row's shown flag");
	store.reset();
	expect_legacy_store(file, false);
	std::filesystem::rename(file, root / "retained.sqlite3");
	const auto foreign = root / "foreign";
	{
		std::ofstream output{ foreign };
		output << "untouched foreign file";
	}
	std::filesystem::create_symlink(foreign, file);
	store = adapters::sqlite::make_release_update_recovery_store(root);
	expect(!store->save(original) && !store->load(), "symlink substitution refuses before opening foreign data");
	std::ifstream     input{ foreign };
	const std::string text{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	expect(text == "untouched foreign file", "foreign data is unchanged");
	std::cout << "release-update-recovery:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
