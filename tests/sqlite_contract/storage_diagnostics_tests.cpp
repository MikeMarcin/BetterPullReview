// [[BPR-1831]]. While a storage diagnostics scope lives, the SQLite stores
// record their opens (with the schema version), their migrations and their
// failed statements as `storage.database` lines of enums, counts and a
// duration; repeated opens are aggregated; nothing is recorded without a
// scope; and no path, file name, key, statement or value reaches a line.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
#include "bpr/adapters/sqlite/markdown_image_disk_cache.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/adapters/sqlite/storage_diagnostics.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "bpr/core/operation_diagnosis.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::operation_action;
	using core::operation_failure;
	using core::operation_store;
	using tests::expect;

	class recording_log final : public application::diagnostic_log_port
	{
	public:
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			const auto lock = std::scoped_lock{ Mutex };
			Events.push_back(aEvent);
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }

		[[nodiscard]] auto lines() -> std::vector<core::operation_diagnosis>
		{
			const auto lock  = std::scoped_lock{ Mutex };
			auto       found = std::vector<core::operation_diagnosis>{ };
			for (const auto& event : Events) {
				const auto line = core::parse_operation_diagnosis(event.Summary.text());
				expect(line.has_value() && event.Operation == application::diagnostic_operation::storage, "every record is exactly a storage line");
				found.push_back(*line);
			}
			return found;
		}

		[[nodiscard]] auto count() -> std::size_t
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Events.size();
		}

		void expect_private(const std::filesystem::path& aRoot)
		{
			const auto lock = std::scoped_lock{ Mutex };
			for (const auto& event : Events) {
				const auto text = event.Summary.text();
				for (const std::string_view word : { std::string_view{ "PRIVATE" }, std::string_view{ "sqlite3" }, std::string_view{ "/" }, std::string_view{ "\\" }, std::string_view{ "SELECT" } }) {
					expect(text.find(word) == std::string_view::npos, "no path, file name, key or statement reaches a line");
				}
				expect(text.find(aRoot.filename().string()) == std::string_view::npos, "not even the root's name");
			}
		}

	private:
		std::mutex                                 Mutex;
		std::vector<application::diagnostic_event> Events;
	};

	[[nodiscard]] auto any(const std::vector<core::operation_diagnosis>& aLines, const operation_action aAction, const operation_store aStore, const operation_failure aFailure) -> bool
	{
		return std::ranges::any_of(aLines, [&](const core::operation_diagnosis& aLine) { return aLine.Action == aAction && aLine.Store == aStore && aLine.Failure == aFailure; });
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "storage-diagnostics-PRIVATE" };
	const auto                           log = std::make_shared<recording_log>();
	{
		auto scope = adapters::sqlite::storage_diagnostics_scope{ log, std::chrono::minutes{ 1 } };

		auto cache = adapters::sqlite::make_markdown_image_disk_cache(scratch.path());
		expect(cache->store("PRIVATE_KEY", "PRIVATE_BYTES", { }), "an image is stored");
		expect(cache->load("PRIVATE_KEY", { }) == std::string{ "PRIVATE_BYTES" }, "and read back");
		auto lines = log->lines();
		expect(any(lines, operation_action::storage_open, operation_store::markdown_images, operation_failure::none), "the store's first open records at once with its store");
		const auto migrated = std::ranges::find_if(lines, [](const core::operation_diagnosis& aLine) { return aLine.Action == operation_action::storage_migrate; });
		expect(migrated != lines.end() && migrated->Store == operation_store::markdown_images && migrated->Schema == 1U && migrated->Failure == operation_failure::none, "creating the store records its migration to schema 1");

		const auto opened = std::ranges::count_if(lines, [](const core::operation_diagnosis& aLine) { return aLine.Action == operation_action::storage_open; });
		for (auto index = 0; index < 5; ++index) {
			static_cast<void>(cache->load("PRIVATE_KEY", { }));
		}
		lines = log->lines();
		expect(std::ranges::count_if(lines, [](const core::operation_diagnosis& aLine) { return aLine.Action == operation_action::storage_open; }) == opened, "repeated opens inside the window are held back");
		scope.flush();
		lines             = log->lines();
		const auto reopen = lines.back();
		expect(reopen.Action == operation_action::storage_open && reopen.Store == operation_store::markdown_images && reopen.Count >= 5U && reopen.Schema == 1U,
			   "and one line then counts them, with the schema version the store opened at");

		// A store file that is not a database: its open fails, and the
		// failing statement is attributed to its store.
		const auto garbage = scratch.path() / "state/v1/diagnostic-preferences.sqlite3";
		{
			auto file = std::ofstream{ garbage, std::ios::binary };
			file << std::string(8192U, 'P') << "PRIVATE";
		}
		std::filesystem::permissions(garbage, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
		static_cast<void>(adapters::sqlite::load_diagnostic_preferences(scratch.path()));
		lines = log->lines();
		expect(std::ranges::any_of(lines, [](const core::operation_diagnosis& aLine) { return aLine.Action == operation_action::storage_open && aLine.Store == operation_store::diagnostic_preferences && aLine.Failure != operation_failure::none; }),
			   "a store that cannot open records its failed open");
#if !defined(_WIN32)
		expect(std::ranges::any_of(lines, [](const core::operation_diagnosis& aLine) { return aLine.Store == operation_store::diagnostic_preferences && aLine.Failure == operation_failure::not_a_database; }),
			   "and the statement that found it is not a database names its store and why");
#endif

		// A statement of no store this adapter opened is still classified.
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(":memory:", &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK, "a bare connection opens");
		expect(sqlite3_exec(database, "SELECT * FROM PRIVATE_missing_table", nullptr, nullptr, nullptr) != SQLITE_OK, "a bad statement fails");
		sqlite3_close(database);
		lines = log->lines();
		expect(any(lines, operation_action::storage_statement, operation_store::unknown, operation_failure::sql_error), "a statement SQLite reports failing is recorded with its class and an unknown store");
		log->expect_private(scratch.path());
	}

	const auto ended = log->count();
	auto       cache = adapters::sqlite::make_markdown_image_disk_cache(scratch.path());
	static_cast<void>(cache->load("PRIVATE_KEY", { }));
	expect(log->count() == ended, "without a scope nothing is recorded");

	expect(adapters::sqlite::storage_store_of("review-ledger.sqlite3") == operation_store::review_ledger && adapters::sqlite::storage_store_of("ledger.db") == operation_store::clone_ledger
			   && adapters::sqlite::storage_store_of("PRIVATE.sqlite3") == operation_store::other,
		   "a store is told by its file's fixed name alone");
	// [[BPR-2255]]. A launch's stores, opened, read and written as a launch
	// does, record no failure: a transaction guard rolls back only what its
	// scope did not commit. Before, each guard ran ROLLBACK after every
	// committed scope, and every launch logged a storage_statement sql_error.
	{
		tests::sqlite_contract::scratch_root quiet{ "storage-diagnostics-guards" };
		const auto                           guarded = std::make_shared<recording_log>();
		{
			auto       guardScope = adapters::sqlite::storage_diagnostics_scope{ guarded, std::chrono::minutes{ 1 } };
			auto       registry   = adapters::sqlite::make_profile_registry_store(quiet.path());
			const auto first      = registry->load({ });
			expect(first.has_value(), "the profile registry is made and read");
			const auto added = [](application::profile_registry_snapshot aBefore, const std::string& aLabel) {
				++aBefore.Revision;
				++aBefore.LastProfileId;
				const auto id = core::bpr_profile_id{ aBefore.LastProfileId };
				aBefore.Profiles.push_back({ { id, aLabel }, application::profile_root_mode::isolated, application::profile_root_identity(id) });
				return aBefore;
			};
			const auto work = added(*first, "Work");
			expect(registry->save(work, first->Revision, { }) == work, "a profile is saved");
			// AC2: a scope that does not commit is still rolled back, so the
			// next write can begin.
			expect(!registry->save(added(*first, "Stale"), first->Revision, { }).has_value(), "a stale save is refused inside its transaction");
			const auto home = added(work, "Home");
			expect(registry->save(home, work.Revision, { }) == home, "and the next save begins and commits: the refused one was rolled back");
			expect(registry->load({ }) == home, "the registry reads back the committed profiles");
			expect(adapters::sqlite::make_window_review_restoration_store(quiet.path())->load({ }).has_value(), "window restoration is made and read");
			expect(adapters::sqlite::make_remembered_connection_store(quiet.path(), core::default_bpr_profile_id)->load({ }).has_value(), "remembered connections are made and read");
			guardScope.flush();
		}
		const auto lines = guarded->lines();
		expect(!lines.empty(), "the stores' opens are recorded");
		expect(std::ranges::none_of(lines, [](const core::operation_diagnosis& aLine) { return aLine.Failure != operation_failure::none; }),
			   "and a launch's reads and writes record no failure, a refused stale save included");
	}
	std::cout << "storage-diagnostics:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "FAILED: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
