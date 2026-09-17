#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::application;
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class scratch_directory final
	{
	public:
		scratch_directory()
		{
			Root = std::filesystem::temp_directory_path() / ("bpr-0691-registry-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directory(Root);
			expect(bpr::adapters::sqlite::make_storage_root_owner_private(Root), "scratch root is owner-private under the canonical storage policy");
			Root = std::filesystem::canonical(Root);
		}
		~scratch_directory() { std::filesystem::remove_all(Root); }
		scratch_directory(const scratch_directory&)                                     = delete;
		auto                  operator=(const scratch_directory&) -> scratch_directory& = delete;
		std::filesystem::path Root;
		[[nodiscard]] auto    database() const -> std::filesystem::path { return Root / "state/v1/profile-registry.sqlite3"; }
	};

	[[nodiscard]] auto raw_open(const std::filesystem::path& aPath) -> sqlite3*
	{
		sqlite3* raw = nullptr;
		expect(sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "fixture opens directly");
		return raw;
	}
	void execute(sqlite3* aDatabase, const std::string_view aSql)
	{
		const auto result = sqlite3_exec(aDatabase, std::string{ aSql }.c_str(), nullptr, nullptr, nullptr);
		if (result != SQLITE_OK) {
			std::cerr << sqlite3_errmsg(aDatabase) << ": " << aSql << '\n';
		}
		expect(result == SQLITE_OK, "fixture SQL executes");
	}
	[[nodiscard]] auto snapshot(sqlite3* aDatabase) -> std::vector<std::string>
	{
		auto       cells   = std::vector<std::string>{ };
		const auto collect = [](void* aContext, int aCount, char** aValues, char**) -> int {
			auto& result = *static_cast<std::vector<std::string>*>(aContext);
			for (int index = 0; index < aCount; ++index) {
				result.emplace_back(aValues[index] == nullptr ? "<NULL>" : aValues[index]);
			}
			return 0;
		};
		expect(sqlite3_exec(aDatabase, "PRAGMA application_id; PRAGMA user_version; SELECT type,name,sql FROM sqlite_schema ORDER BY name; SELECT * FROM profile_registry_meta; SELECT * FROM profile_registry_profiles ORDER BY profile_id", collect, &cells, nullptr) == SQLITE_OK, "complete registry schema and contents are captured");
		if (adapters::sqlite::detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema WHERE name='profile_registry_windows'") == 1) {
			expect(sqlite3_exec(aDatabase, "SELECT * FROM profile_registry_windows ORDER BY profile_id,ordinal", collect, &cells, nullptr) == SQLITE_OK, "retained rows captured");
		}
		return cells;
	}
	[[nodiscard]] auto added(profile_registry_snapshot aBefore, const std::string_view aLabel) -> profile_registry_snapshot
	{
		++aBefore.Revision;
		++aBefore.LastProfileId;
		const auto id = core::bpr_profile_id{ aBefore.LastProfileId };
		aBefore.Profiles.push_back({ { id, std::string{ aLabel } }, profile_root_mode::isolated, profile_root_identity(id) });
		return aBefore;
	}

	void test_adoption_and_cas()
	{
		const auto scratch  = scratch_directory{ };
		const auto sentinel = scratch.Root / "legacy-epoch-and-settings.fixture";
		{
			std::ofstream output{ sentinel, std::ios::binary };
			output << "existing epoch:17; theme:custom; remembered account binding:fixture";
		}
		const auto size  = std::filesystem::file_size(sentinel);
		auto       store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		const auto first = store->load({ });
		// Named: GCC 15.2 misreads a conditionally built snapshot temporary as dangling (BPR-0816).
		const auto defaultOnlyRegistry = profile_registry_snapshot{ };
		expect(first && *first == defaultOnlyRegistry, "fresh legacy environment adopts exactly one immutable Default");
		const auto& original = *first;
		store.reset();
		store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(store->load({ }) == first, "reopen does not repeat adoption or bump revision");
		const auto work = added(original, "../Work");
		expect(store->save(work, original.Revision, { }) == work, "new profile metadata commits with ID-derived private root identity");
		auto       rival = adapters::sqlite::make_profile_registry_store(scratch.Root);
		const auto stale = rival->save(added(original, "Other"), original.Revision, { });
		expect(!stale && stale.error() == profile_registry_failure::conflict && rival->load({ }) == work, "competing stale writer cannot overwrite a committed registry");
		auto renamed = work;
		++renamed.Revision;
		renamed.Profiles.back().Profile.Label = "Personal";
		expect(store->save(renamed, work.Revision, { }) == renamed, "rename persists without changing identity or root");
		store.reset();
		rival.reset();
		store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(store->load({ }) == renamed, "created and renamed profiles reopen intact");
		expect(std::filesystem::file_size(sentinel) == size, "adoption does not replace existing domain content");
		std::ifstream input{ sentinel, std::ios::binary };
		const auto    bytes = std::string{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
		expect(bytes == "existing epoch:17; theme:custom; remembered account binding:fixture", "legacy epoch and preference fixture bytes are untouched");
		expect(!std::filesystem::exists(scratch.Root / "profiles"), "registry metadata does not silently create or move profile roots");
	}

	void test_corruptions_refuse_without_mutation()
	{
		for (const auto mutation : {
				 "PRAGMA user_version=99",
				 "PRAGMA application_id=42",
				 "PRAGMA ignore_check_constraints=ON; UPDATE profile_registry_meta SET schema_format=99",
				 "DELETE FROM profile_registry_meta",
				 "PRAGMA ignore_check_constraints=ON; UPDATE profile_registry_meta SET default_adopted=0",
				 "UPDATE profile_registry_meta SET last_profile_id='1'",
				 "UPDATE profile_registry_profiles SET root_mode=0 WHERE profile_id='2'",
				 "UPDATE profile_registry_profiles SET root_identity='profile-77' WHERE profile_id='2'",
				 "UPDATE profile_registry_profiles SET label=char(0) WHERE profile_id='2'",
				 "UPDATE profile_registry_profiles SET label=printf('%0130d',1) WHERE profile_id='2'",
				 "ALTER TABLE profile_registry_profiles ADD COLUMN future_data TEXT",
				 "ALTER TABLE profile_registry_profiles RENAME TO old; CREATE TABLE profile_registry_profiles AS SELECT * FROM old; INSERT INTO profile_registry_profiles SELECT * FROM old; DROP TABLE old" }) {
			const auto scratch = scratch_directory{ };
			{
				auto       store  = adapters::sqlite::make_profile_registry_store(scratch.Root);
				const auto loaded = store->load({ });
				expect(loaded && store->save(added(*loaded, "Work"), loaded->Revision, { }).has_value(), "corruption fixture has two committed profiles");
			}
			auto* raw = raw_open(scratch.database());
			execute(raw, mutation);
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			auto       store   = adapters::sqlite::make_profile_registry_store(scratch.Root);
			const auto refused = store->load({ });
			expect(!refused && (refused.error() == profile_registry_failure::schema_mismatch || refused.error() == profile_registry_failure::corrupt), "future or malformed registry fails explicitly");
			store.reset();
			raw = raw_open(scratch.database());
			expect(snapshot(raw) == before, "refusal leaves original versions, metadata, schema, labels and roots unchanged");
			sqlite3_close(raw);
		}
	}

	struct interruption
	{
		static inline std::stop_source* Stop = nullptr;
		static inline std::string_view  Prefix;
		static auto                     trace(unsigned int, void*, void* aStatement, void*) -> int
		{
			const auto* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement));
			if (Stop != nullptr && sql != nullptr && std::string_view{ sql }.starts_with(Prefix)) {
				Stop->request_stop();
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT, trace, nullptr);
		}
	};

	void test_interrupted_adoption_and_save_roll_back()
	{
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == SQLITE_OK, "test-local SQLite trace injection registers");
		const auto scratch     = scratch_directory{ };
		auto       stop        = std::stop_source{ };
		interruption::Stop     = &stop;
		interruption::Prefix   = "INSERT INTO profile_registry_profiles";
		auto       store       = adapters::sqlite::make_profile_registry_store(scratch.Root);
		const auto interrupted = store->load(stop.get_token());
		expect(stop.stop_requested() && !interrupted && interrupted.error() == profile_registry_failure::cancelled, "adoption is interrupted after schema creation before marker commit");
		interruption::Stop = nullptr;
		store.reset();
		auto* raw = raw_open(scratch.database());
		expect(adapters::sqlite::detail::scalar_integer(raw, "PRAGMA user_version") == 0
				   && adapters::sqlite::detail::scalar_integer(raw, "PRAGMA application_id") == 0
				   && adapters::sqlite::detail::scalar_integer(raw, "SELECT count(*) FROM sqlite_schema") == 0,
			   "interrupted initialization rolls back identity, schema, Default and marker together");
		sqlite3_close(raw);
		store              = adapters::sqlite::make_profile_registry_store(scratch.Root);
		const auto adopted = store->load({ });
		// Named: GCC 15.2 misreads a conditionally built snapshot temporary as dangling (BPR-0816).
		const auto defaultOnlyRegistry = profile_registry_snapshot{ };
		expect(adopted && *adopted == defaultOnlyRegistry, "restart adopts exactly one Default after interrupted transaction");
		stop                       = std::stop_source{ };
		interruption::Stop         = &stop;
		interruption::Prefix       = "DELETE FROM profile_registry_profiles";
		const auto interruptedSave = store->save(added(*adopted, "Work"), adopted->Revision, stop.get_token());
		expect(stop.stop_requested() && !interruptedSave && interruptedSave.error() == profile_registry_failure::cancelled, "save interruption observes the destructive transaction step");
		interruption::Stop = nullptr;
		store.reset();
		store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(store->load({ }) == adopted, "rollback restores rows and high-water revision after interrupted replacement");
		store.reset();
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == 1, "test injection unregisters");
	}

	void make_legacy(sqlite3* aDatabase)
	{
		execute(aDatabase, "DROP TABLE profile_registry_windows; DROP TABLE profile_registry_meta; CREATE TABLE profile_registry_meta(singleton INTEGER PRIMARY KEY CHECK(singleton=1),schema_format INTEGER NOT NULL CHECK(schema_format=1),revision TEXT NOT NULL,last_profile_id TEXT NOT NULL,default_adopted INTEGER NOT NULL CHECK(default_adopted=1)) STRICT; INSERT INTO profile_registry_meta VALUES(1,1,'17','2',1); PRAGMA user_version=1");
	}

	void test_window_migration_and_rollback()
	{
		for (const auto prefix : { "CREATE TABLE profile_registry_windows", "DROP TABLE profile_registry_legacy_meta", "PRAGMA user_version=2" }) {
			const auto scratch  = scratch_directory{ };
			auto       store    = adapters::sqlite::make_profile_registry_store(scratch.Root);
			const auto initial  = store->load({ });
			auto       expected = added(*initial, "Work");
			expect(store->save(expected, initial->Revision, { }).has_value(), "migration fixture profiles persist");
			store.reset();
			auto* raw = raw_open(scratch.database());
			make_legacy(raw);
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == SQLITE_OK, "migration interruption installs");
			auto stop            = std::stop_source{ };
			interruption::Stop   = &stop;
			interruption::Prefix = prefix;
			store                = adapters::sqlite::make_profile_registry_store(scratch.Root);
			const auto refused   = store->load(stop.get_token());
			expect(stop.stop_requested() && !refused && refused.error() == profile_registry_failure::cancelled, "migration interrupted after actual DDL");
			interruption::Stop = nullptr;
			store.reset();
			expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == 1, "migration injection removed");
			raw = raw_open(scratch.database());
			expect(snapshot(raw) == before, "migration rollback restores exact schema1 rows and stamps");
			sqlite3_close(raw);
			expected.Revision = 17;
			store             = adapters::sqlite::make_profile_registry_store(scratch.Root);
			expect(store->load({ }) == expected, "schema1 migrates preserving revision and profiles without invented windows");
			store.reset();
			raw = raw_open(scratch.database());
			expect(adapters::sqlite::detail::scalar_integer(raw, "PRAGMA user_version") == 2, "migration stamps schema2");
			sqlite3_close(raw);
		}
	}

	void test_retained_windows()
	{
		const auto scratch                   = scratch_directory{ };
		auto       store                     = adapters::sqlite::make_profile_registry_store(scratch.Root);
		const auto initial                   = store->load({ });
		auto       retained                  = added(*initial, "Work");
		retained.LastWindowId                = 9;
		retained.Profiles[0].RetainedWindows = { { core::bpr_window_id{ 7 }, core::bpr_window_generation{ 3 } }, { core::bpr_window_id{ 2 }, core::bpr_window_generation{ 9 } } };
		retained.Profiles[1].RetainedWindows = { { core::bpr_window_id{ 9 }, core::bpr_window_generation{ 1 } } };
		expect(store->save(retained, initial->Revision, { }) == retained, "multi-profile MRU persists");
		store.reset();
		store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(store->load({ }) == retained, "reopen preserves exact least-to-most-focused order");
		auto rival = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(rival->load({ }) == retained, "second connection sees same registry");
		auto next = retained;
		++next.Revision;
		std::ranges::reverse(next.Profiles[0].RetainedWindows);
		next.Profiles[0].RetainedWindows[0].Generation = core::bpr_window_generation{ 10 };
		expect(store->save(next, retained.Revision, { }) == next, "reorder and generation advance accepted");
		const auto stale = rival->save(next, retained.Revision, { });
		expect(!stale && stale.error() == profile_registry_failure::conflict, "window replacement uses global CAS");
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == SQLITE_OK, "replacement injection installs");
		store.reset();
		store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(store->load({ }) == next, "injected connection opens before cancellation");
		for (const auto prefix : { "DELETE FROM profile_registry_windows", "INSERT INTO profile_registry_windows", "UPDATE profile_registry_meta" }) {
			auto candidate = next;
			++candidate.Revision;
			++candidate.LastWindowId;
			candidate.Profiles[1].RetainedWindows.push_back({ core::bpr_window_id{ candidate.LastWindowId }, core::bpr_window_generation{ 1 } });
			auto stop            = std::stop_source{ };
			interruption::Stop   = &stop;
			interruption::Prefix = prefix;
			const auto refused   = store->save(candidate, next.Revision, stop.get_token());
			expect(stop.stop_requested() && !refused && refused.error() == profile_registry_failure::cancelled, "replacement interrupted inside transaction");
			interruption::Stop = nullptr;
			expect(store->load({ }) == next, "all retained rows, revision and allocator roll back together");
		}
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == 1, "replacement injection removed");
	}

	void test_window_corruption()
	{
		for (const auto mutation : { "UPDATE profile_registry_meta SET last_window_id='00'", "UPDATE profile_registry_meta SET last_window_id='0'",
									 "UPDATE profile_registry_windows SET window_id='01'", "UPDATE profile_registry_windows SET generation='0'",
									 "PRAGMA foreign_keys=OFF; UPDATE profile_registry_windows SET profile_id='99'", "UPDATE profile_registry_windows SET ordinal=1",
									 "UPDATE profile_registry_windows SET generation='18446744073709551616'",
									 "PRAGMA ignore_check_constraints=ON; UPDATE profile_registry_windows SET ordinal=-1" }) {
			const auto scratch   = scratch_directory{ };
			auto       store     = adapters::sqlite::make_profile_registry_store(scratch.Root);
			const auto initial   = store->load({ });
			auto       candidate = *initial;
			++candidate.Revision;
			candidate.LastWindowId                = 1;
			candidate.Profiles[0].RetainedWindows = { { core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } } };
			expect(store->save(candidate, initial->Revision, { }).has_value(), "window corruption fixture persists");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, mutation);
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			store = adapters::sqlite::make_profile_registry_store(scratch.Root);
			expect(!store->load({ }), "malformed window metadata refuses");
			store.reset();
			raw = raw_open(scratch.database());
			expect(snapshot(raw) == before, "window refusal preserves exact file facts");
			sqlite3_close(raw);
		}
	}

	struct bounded_read_probe
	{
		static inline bool Active      = false;
		static inline int  LengthLimit = 0;
		static inline int  Steps       = 0;
		static inline int  Sorts       = 0;
		static auto        trace(unsigned int aEvent, void*, void* aStatement, void*) -> int
		{
			auto*       statement = static_cast<sqlite3_stmt*>(aStatement);
			const auto* sql       = sqlite3_sql(statement);
			if (!Active || sql == nullptr || !std::string_view{ sql }.starts_with("SELECT profile_id,label,root_mode,root_identity")) {
				return 0;
			}
			if (aEvent == SQLITE_TRACE_STMT) {
				LengthLimit = sqlite3_limit(sqlite3_db_handle(statement), SQLITE_LIMIT_LENGTH, -1);
			} else if (aEvent == SQLITE_TRACE_PROFILE) {
				Steps = std::max(Steps, sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_VM_STEP, 0));
				Sorts += sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_SORT, 0);
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, trace, nullptr);
		}
	};

	void test_hostile_preflight_has_bounded_materialization_and_work()
	{
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(bounded_read_probe::install)) == SQLITE_OK, "read-only preflight probe registers");
		for (const auto mutation : {
				 "UPDATE profile_registry_profiles SET label=hex(zeroblob(2097152))",
				 "UPDATE profile_registry_profiles SET root_identity=hex(zeroblob(2097152))",
				 "WITH RECURSIVE ids(n) AS (VALUES(2) UNION ALL SELECT n+1 FROM ids WHERE n<10001) INSERT INTO profile_registry_profiles SELECT CAST(n AS TEXT),'Hostile row',1,'profile-'||n FROM ids; UPDATE profile_registry_meta SET last_profile_id='10001'" }) {
			const auto scratch = scratch_directory{ };
			{
				auto store = adapters::sqlite::make_profile_registry_store(scratch.Root);
				expect(store->load({ }).has_value(), "hostile fixture starts with canonical schema");
			}
			auto* raw = raw_open(scratch.database());
			execute(raw, mutation);
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			bounded_read_probe::LengthLimit = 0;
			bounded_read_probe::Steps       = 0;
			bounded_read_probe::Sorts       = 0;
			bounded_read_probe::Active      = true;
			auto       store                = adapters::sqlite::make_profile_registry_store(scratch.Root);
			const auto refused              = store->load({ });
			bounded_read_probe::Active      = false;
			expect(!refused && refused.error() == profile_registry_failure::corrupt, "oversized fields and hostile row counts refuse during admission");
			expect(bounded_read_probe::LengthLimit > 0 && bounded_read_probe::LengthLimit <= 4096,
				   "SQLite enforces a small byte limit before the first profile row is materialized");
			expect(bounded_read_probe::Steps > 0 && bounded_read_probe::Steps < 2000 && bounded_read_probe::Sorts == 0,
				   "preflight reads only the bounded prefix without sorting or scanning the hostile table");
			store.reset();
			raw = raw_open(scratch.database());
			expect(snapshot(raw) == before, "bounded refusal preserves complete hostile fields, rows, schema and metadata");
			sqlite3_close(raw);
		}
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(bounded_read_probe::install)) == 1, "preflight probe unregisters");
	}

	void test_bounds_and_nonpristine_refusal()
	{
		const auto scratch = scratch_directory{ };
		auto       store   = adapters::sqlite::make_profile_registry_store(scratch.Root);
		const auto initial = store->load({ });
		expect(initial.has_value(), "bounded fixture adopts Default");
		auto full = *initial;
		for (std::size_t index = 1U; index < core::maximum_bpr_profiles; ++index) {
			full = added(full, "Profile");
		}
		full.Revision = initial->Revision + 1U;
		for (auto& profile : full.Profiles) {
			for (std::size_t index = 0; index < core::maximum_bpr_windows_per_profile; ++index) {
				profile.RetainedWindows.push_back({ core::bpr_window_id{ ++full.LastWindowId }, core::bpr_window_generation{ 1 } });
			}
		}
		expect(store->save(full, initial->Revision, { }) == full, "maximum bounded registry persists");
		auto excessWindows = full;
		++excessWindows.Revision;
		excessWindows.Profiles[0].RetainedWindows.push_back({ core::bpr_window_id{ ++excessWindows.LastWindowId }, core::bpr_window_generation{ 1 } });
		expect(!store->save(excessWindows, full.Revision, { }) && store->load({ }) == full, "33rd retained window refuses without mutation at global capacity");
		const auto tooMany = added(full, "Overflow");
		expect(!store->save(tooMany, full.Revision, { }) && store->load({ }) == full, "overflow cannot partially save");
		store.reset();
		auto* raw = raw_open(scratch.database());
		execute(raw, "INSERT INTO profile_registry_profiles VALUES('65','Overflow',1,'profile-65'); UPDATE profile_registry_meta SET last_profile_id='65'");
		const auto before = snapshot(raw);
		sqlite3_close(raw);
		store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(!store->load({ }), "over-limit stored collection refuses instead of truncating");
		store.reset();
		raw = raw_open(scratch.database());
		expect(snapshot(raw) == before, "over-limit refusal preserves stored rows");
		execute(raw, "PRAGMA application_id=0; PRAGMA user_version=0");
		sqlite3_close(raw);
		store = adapters::sqlite::make_profile_registry_store(scratch.Root);
		expect(!store->load({ }), "an unstamped populated database is never treated as pristine adoption");
	}
}

int main()
{
	expect(adapters::sqlite::detail::sqlite_runtime_ready(), "SQLite runtime initializes");
	test_window_migration_and_rollback();
	test_retained_windows();
	test_window_corruption();
	test_adoption_and_cas();
	test_corruptions_refuse_without_mutation();
	test_interrupted_adoption_and_save_roll_back();
	test_bounds_and_nonpristine_refusal();
	test_hostile_preflight_has_bounded_materialization_and_work();
	std::cout << "profile-registry-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
