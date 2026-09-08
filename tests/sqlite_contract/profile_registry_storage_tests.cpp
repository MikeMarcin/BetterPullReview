#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"

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
			std::filesystem::permissions(Root, std::filesystem::perms::owner_all);
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
		expect(sqlite3_exec(aDatabase, std::string{ aSql }.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "fixture SQL executes");
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
		expect(first && *first == profile_registry_snapshot{ }, "fresh legacy environment adopts exactly one immutable Default");
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
		expect(adopted && *adopted == profile_registry_snapshot{ }, "restart adopts exactly one Default after interrupted transaction");
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
		expect(store->save(full, initial->Revision, { }) == full, "maximum bounded registry persists");
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
	test_adoption_and_cas();
	test_corruptions_refuse_without_mutation();
	test_interrupted_adoption_and_save_roll_back();
	test_bounds_and_nonpristine_refusal();
	test_hostile_preflight_has_bounded_materialization_and_work();
	std::cout << "profile-registry-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
