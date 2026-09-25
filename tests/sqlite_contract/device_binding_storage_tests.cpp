#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/testing.hpp"

#include <sqlite3.h>
#if !defined(_WIN32)
	#include <sys/wait.h>
	#include <unistd.h>
#endif
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
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
	auto database_bytes(const std::filesystem::path& aRoot) -> std::string
	{
		std::ifstream input{ aRoot / "state/v1/device-bindings.sqlite3", std::ios::binary };
		return { std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	}

	auto version(const std::filesystem::path& aRoot) -> std::int64_t
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open((aRoot / "state/v1/device-bindings.sqlite3").string().c_str(), &database) == SQLITE_OK, "open version observation");
		const auto result = adapters::sqlite::detail::scalar_integer(database, "PRAGMA user_version");
		sqlite3_close(database);
		expect(result.has_value(), "read version");
		return *result;
	}
	void legacy(const std::filesystem::path& aRoot)
	{
		std::filesystem::create_directory(aRoot);
		expect(adapters::sqlite::make_storage_root_owner_private(aRoot), "fixture root is owner-private under the canonical storage policy");
		seed(aRoot);
		mutate(aRoot, "DROP TABLE binding_scope; DROP TABLE device_binding; CREATE TABLE device_binding(profile TEXT PRIMARY KEY,revision TEXT NOT NULL,host TEXT NOT NULL,hostname TEXT NOT NULL,account TEXT NOT NULL,server INTEGER NOT NULL CHECK(server BETWEEN 0 AND 1),provenance INTEGER NOT NULL CHECK(provenance=1),issued INTEGER NOT NULL CHECK(issued>0),access_expiry INTEGER NOT NULL CHECK(access_expiry>issued),refresh_expiry INTEGER NOT NULL CHECK(refresh_expiry>issued),tombstone INTEGER NOT NULL CHECK(tombstone IN(0,1))) STRICT; INSERT INTO device_binding VALUES('1','2','3','github.com','4',0,1,1000,4600,10000,0); PRAGMA user_version=1");
	}
	struct migration_interruption
	{
		static inline std::stop_source* Stop = nullptr;
		static auto                     trace(unsigned int, void*, void* aStatement, void*) -> int
		{
			const auto* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement));
			if (Stop != nullptr && sql != nullptr && std::string_view{ sql }.starts_with("INSERT INTO device_binding SELECT")) {
				Stop->request_stop();
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT, trace, nullptr);
		}
	};
	struct preflight_observer
	{
		static inline std::stop_source* Stop        = nullptr;
		static inline int               LengthLimit = 0;
		static inline int               QuickSteps  = 0;
		static auto                     trace(unsigned int aEvent, void*, void* aStatement, void*) -> int
		{
			auto*       statement = static_cast<sqlite3_stmt*>(aStatement);
			const auto* sql       = sqlite3_sql(statement);
			if (sql != nullptr && std::string_view{ sql } == "PRAGMA quick_check") {
				if (aEvent == SQLITE_TRACE_STMT) {
					LengthLimit = sqlite3_limit(sqlite3_db_handle(statement), SQLITE_LIMIT_LENGTH, -1);
					if (Stop != nullptr) {
						Stop->request_stop();
					}
				} else if (aEvent == SQLITE_TRACE_PROFILE) {
					QuickSteps = sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_VM_STEP, 0);
				}
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, trace, nullptr);
		}
	};
	void bounded_preflight(const std::filesystem::path& aRoot)
	{
		expect(adapters::sqlite::detail::sqlite_runtime_ready(), "initialize parent runtime before installing observer");
		const auto bytes = database_bytes(aRoot);
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(preflight_observer::install)) == SQLITE_OK, "observe preflight connection locally");
		std::stop_source stop;
		preflight_observer::Stop = &stop;
		auto       store         = adapters::sqlite::make_device_binding_store(aRoot);
		const auto cancelled     = store->load(stop.get_token());
		preflight_observer::Stop = nullptr;
		expect(!cancelled && cancelled.error() == application::device_binding_failure::cancelled && stop.stop_requested(), "cancellation during current-schema quick_check refuses explicitly");
		expect(preflight_observer::LengthLimit == (1'048'576) && preflight_observer::QuickSteps <= 256, "preflight has 1MiB bound and interrupts quick_check within a bounded short instruction budget");
		expect(database_bytes(aRoot) == bytes, "cancelled preflight leaves database bytes unchanged");
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(preflight_observer::install)) == 1, "remove preflight observer");
		// [[BPR-0884]]. Cancellation is not corruption. Only a second call on the
		// same store can show whether the first one latched it.
		expect(store->load({ }).has_value(), "a cancelled open leaves the same store usable on its next call");
		store.reset();
		for (const auto legacyVersion : { false, true }) {
			const auto hostileRoot = aRoot / (legacyVersion ? "hostile-legacy" : "hostile-current");
			if (legacyVersion) {
				legacy(hostileRoot);
			} else {
				std::filesystem::create_directory(hostileRoot);
				expect(adapters::sqlite::make_storage_root_owner_private(hostileRoot), "fixture root is owner-private under the canonical storage policy");
				seed(hostileRoot);
			}
			mutate(hostileRoot, "UPDATE device_binding SET hostname=CAST(zeroblob(2097152) AS TEXT)");
			const auto hostileBytes         = database_bytes(hostileRoot);
			preflight_observer::LengthLimit = 0;
			expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(preflight_observer::install)) == SQLITE_OK, "observe hostile preflight");
			expect(!adapters::sqlite::make_device_binding_store(hostileRoot)->load({ }), "oversized hostile legacy/current field refuses");
			expect(preflight_observer::LengthLimit == (1'048'576) && database_bytes(hostileRoot) == hostileBytes, "hostile preflight applies length bound before reads without rewriting file");
			expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(preflight_observer::install)) == 1, "remove hostile observer");
		}
	}

	void scoped_storage_and_migration(const std::filesystem::path& aRoot)
	{
		const auto firstRoot              = aRoot / "first";
		const auto secondRoot             = aRoot / "second";
		auto       firstRecord            = record();
		firstRecord.Authority.BprProfile  = core::bpr_profile_id{ 2U };
		auto secondRecord                 = record();
		secondRecord.Authority.BprProfile = core::bpr_profile_id{ 3U };
		auto first                        = adapters::sqlite::make_device_binding_store(firstRoot, core::bpr_profile_id{ 2U });
		auto second                       = adapters::sqlite::make_device_binding_store(secondRoot, core::bpr_profile_id{ 3U });
		expect(!first->store(secondRecord, { }) && !first->erase(secondRecord.Authority, { }) && !std::filesystem::exists(firstRoot), "foreign writes refuse before creating a root");
		std::filesystem::create_directory(firstRoot);
		expect(adapters::sqlite::make_storage_root_owner_private(firstRoot), "fixture root is owner-private under the canonical storage policy");
		std::filesystem::create_directory(secondRoot);
		expect(adapters::sqlite::make_storage_root_owner_private(secondRoot), "fixture root is owner-private under the canonical storage policy");
		expect(first->store(firstRecord, { }).has_value() && second->store(secondRecord, { }).has_value(), "separate BPR profiles store identical host and account");
		expect(first->load({ })->front() == firstRecord && second->load({ })->front() == secondRecord, "scope survives independent store reads");
		first.reset();
		second.reset();
		expect(!adapters::sqlite::make_device_binding_store(firstRoot, core::bpr_profile_id{ 3U })->load({ }), "foreign store owner refuses whole catalog");
		mutate(firstRoot, "UPDATE device_binding SET bpr_profile='3'");
		expect(!adapters::sqlite::make_device_binding_store(firstRoot, core::bpr_profile_id{ 2U })->load({ }), "foreign row refuses whole catalog");
		const auto invalidRoot = aRoot / "invalid";
		expect(!adapters::sqlite::make_device_binding_store(invalidRoot, core::bpr_profile_id{ 0U })->load({ }) && !std::filesystem::exists(invalidRoot), "invalid scope refuses before filesystem effects");
		const auto legacyRoot = aRoot / "legacy";
		legacy(legacyRoot);
		const auto legacyBytes = database_bytes(legacyRoot);
		expect(!adapters::sqlite::make_device_binding_store(legacyRoot, core::bpr_profile_id{ 2U })->load({ }) && version(legacyRoot) == 1, "nonDefault refuses legacy without migration");
		expect(database_bytes(legacyRoot) == legacyBytes, "nonDefault refusal leaves legacy database bytes unchanged");
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(migration_interruption::install)) == SQLITE_OK, "register migration interruption");
		std::stop_source stop;
		migration_interruption::Stop = &stop;
		// [[BPR-0884]]. The interrupted store is the one that retries.
		auto       adopted = adapters::sqlite::make_device_binding_store(legacyRoot);
		const auto stopped = adopted->load(stop.get_token());
		expect(!stopped && stopped.error() == application::device_binding_failure::cancelled && stop.stop_requested(), "interrupt Default adoption after row copy begins");
		migration_interruption::Stop = nullptr;
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(migration_interruption::install)) == 1, "remove migration interruption");
		expect(version(legacyRoot) == 1, "interrupted migration rolls back schema version");
		const auto rows = adopted->load({ });
		expect(rows && rows->size() == 1U && rows->front() == record() && version(legacyRoot) == 2, "the interrupted store adopts the legacy row exactly into Default schema2 on its next call");
		expect(adopted->erase(record().Authority, { }).has_value(), "adopted authority tombstones exactly");
		adopted.reset();
		expect(adapters::sqlite::make_device_binding_store(legacyRoot)->load({ })->front().Tombstone, "adopted tombstone survives restart");
		mutate(legacyRoot, "PRAGMA user_version=3");
		const auto futureBytes = database_bytes(legacyRoot);
		expect(!adapters::sqlite::make_device_binding_store(legacyRoot)->load({ }) && version(legacyRoot) == 3, "future store refuses without fallback");
		expect(database_bytes(legacyRoot) == futureBytes, "future refusal leaves database bytes unchanged");
		const auto malformedRoot = aRoot / "malformed";
		legacy(malformedRoot);
		mutate(malformedRoot, "PRAGMA ignore_check_constraints=ON; UPDATE device_binding SET access_expiry=issued");
		const auto malformedBytes = database_bytes(malformedRoot);
		expect(!adapters::sqlite::make_device_binding_store(malformedRoot)->load({ }) && version(malformedRoot) == 1, "malformed legacy row refuses without migration");
		expect(database_bytes(malformedRoot) == malformedBytes, "malformed refusal leaves legacy database bytes unchanged");
	}

	// [[BPR-0884]]. Another connection holding the file is contention, not
	// corruption, so the same store opens once the hold ends. A verdict on the
	// admitted file's content is different: it latches for the store's lifetime.
	auto hold(const std::filesystem::path& aRoot, const char* aBegin) -> sqlite3*
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open((aRoot / "state/v1/device-bindings.sqlite3").string().c_str(), &database) == SQLITE_OK, "open a second connection to hold the store");
		expect(sqlite3_exec(database, aBegin, nullptr, nullptr, nullptr) == SQLITE_OK, "the second connection takes its lock");
		return database;
	}
	void release(sqlite3* aDatabase)
	{
		expect(sqlite3_exec(aDatabase, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK, "the second connection releases its lock");
		sqlite3_close(aDatabase);
	}
	// Another connection tries to commit between the store's first and third reads.
	struct intervening_writer
	{
		static inline const std::filesystem::path* Root    = nullptr;
		static inline int                          Written = SQLITE_OK;
		static auto                                trace(unsigned int, void*, void* aStatement, void*) -> int
		{
			const auto* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement));
			if (Root != nullptr && sql != nullptr && std::string_view{ sql } == "SELECT count(*) FROM sqlite_schema") {
				const auto* root   = std::exchange(Root, nullptr);
				sqlite3*    writer = nullptr;
				Written            = sqlite3_open((*root / "state/v1/device-bindings.sqlite3").string().c_str(), &writer);
				if (Written == SQLITE_OK) {
					Written = sqlite3_exec(writer, "CREATE TABLE intruder(value TEXT)", nullptr, nullptr, nullptr);
				}
				sqlite3_close(writer);
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT, trace, nullptr);
		}
	};
	void contention_and_corruption(const std::filesystem::path& aRoot)
	{
		using application::device_binding_failure;
		// A writer holds a legacy file the store must migrate: the store's
		// BEGIN IMMEDIATE waits out the busy timeout and fails.
		const auto migrating = aRoot / "contended-migration";
		legacy(migrating);
		{
			auto       store   = adapters::sqlite::make_device_binding_store(migrating);
			auto*      writer  = hold(migrating, "BEGIN IMMEDIATE");
			const auto blocked = store->load({ });
			release(writer);
			expect(!blocked && blocked.error() == device_binding_failure::unavailable && version(migrating) == 1, "a busy migration is unavailable, not corrupt, and changes nothing");
			const auto rows = store->load({ });
			expect(rows && rows->size() == 1U && rows->front() == record() && version(migrating) == 2, "the same store migrates once the writer is gone");
		}
		// A rollback-journal file under an exclusive lock refuses even the
		// store's first read.
		const auto reading = aRoot / "contended-read";
		std::filesystem::create_directory(reading);
		expect(adapters::sqlite::make_storage_root_owner_private(reading), "fixture root is owner-private under the canonical storage policy");
		seed(reading);
		mutate(reading, "PRAGMA journal_mode=DELETE");
		{
			auto       store   = adapters::sqlite::make_device_binding_store(reading);
			auto*      writer  = hold(reading, "BEGIN EXCLUSIVE");
			const auto blocked = store->load({ });
			release(writer);
			expect(!blocked && blocked.error() == device_binding_failure::unavailable, "a locked first read is unavailable, not corrupt");
			const auto rows = store->load({ });
			expect(rows && rows->size() == 1U && rows->front() == record(), "the same store reads once the lock is gone");
		}
		// The judgement reads one snapshot. A writer that committed between two of
		// its reads would otherwise make a fresh file look like a corrupt one.
		const auto racing = aRoot / "racing";
		std::filesystem::create_directory(racing);
		expect(adapters::sqlite::make_storage_root_owner_private(racing), "fixture root is owner-private under the canonical storage policy");
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(intervening_writer::install)) == SQLITE_OK, "register the intervening writer");
		intervening_writer::Root = &racing;
		const auto raced         = adapters::sqlite::make_device_binding_store(racing)->load({ });
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(intervening_writer::install)) == 1, "remove the intervening writer");
		expect(intervening_writer::Root == nullptr && intervening_writer::Written == SQLITE_BUSY, "no writer can commit while the store is judging the file");
		expect(raced && raced->empty() && version(racing) == 2, "the store judges and creates the fresh file it read");
		// An unknown schema object is a verdict on the file. Repairing the file
		// does not reopen a store that has already refused it.
		const auto latched = aRoot / "latched";
		std::filesystem::create_directory(latched);
		expect(adapters::sqlite::make_storage_root_owner_private(latched), "fixture root is owner-private under the canonical storage policy");
		seed(latched);
		mutate(latched, "CREATE TABLE extra(value TEXT)");
		auto       store   = adapters::sqlite::make_device_binding_store(latched);
		const auto refused = store->load({ });
		expect(!refused && refused.error() == device_binding_failure::corrupt, "an unknown schema object is corrupt");
		mutate(latched, "DROP TABLE extra");
		const auto again = store->load({ });
		expect(!again && again.error() == device_binding_failure::corrupt && !store->erase(record().Authority, { }), "a corrupt verdict latches the store for its lifetime");
		expect(adapters::sqlite::make_device_binding_store(latched)->load({ }).has_value(), "the repaired file is valid, so the refusal above is the latch");
	}

	// [[BPR-1111]]. The main file's reads fail on demand, as a failing disk or
	// a vanished network share makes them fail: the store's connection keeps
	// its own I/O methods, wrapped. The `Occurrence`-th run of `Statement`
	// arms the failure, after releasing the pages the connection has cached,
	// and that run's next read of the file answers SQLITE_IOERR_READ.
	struct read_fault
	{
		static inline const sqlite3_io_methods* Original = nullptr;
		static inline sqlite3_io_methods        Methods{ };
		static inline std::string_view          Statement;
		static inline int                       Occurrence = 0;
		static inline bool                      Armed      = false;
		static inline int                       Injected   = 0;
		static auto                             read(sqlite3_file* aFile, void* aBuffer, int aAmount, sqlite3_int64 aOffset) -> int
		{
			if (std::exchange(Armed, false)) {
				++Injected;
				return SQLITE_IOERR_READ;
			}
			return Original->xRead(aFile, aBuffer, aAmount, aOffset);
		}
		static auto trace(unsigned int, void*, void* aStatement, void*) -> int
		{
			auto* const statement = static_cast<sqlite3_stmt*>(aStatement);
			const auto* sql       = sqlite3_sql(statement);
			if (Occurrence == 0 || sql == nullptr || std::string_view{ sql } != Statement) {
				return 0;
			}
			--Occurrence;
			if (Occurrence == 0) {
				static_cast<void>(sqlite3_db_release_memory(sqlite3_db_handle(statement)));
				Armed = true;
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			sqlite3_file* file = nullptr;
			if (sqlite3_file_control(aDatabase, "main", SQLITE_FCNTL_FILE_POINTER, static_cast<void*>(&file)) != SQLITE_OK || file == nullptr || file->pMethods == nullptr) {
				return SQLITE_ERROR;
			}
			if (file->pMethods != &Methods) {
				Original       = file->pMethods;
				Methods        = *Original;
				Methods.xRead  = read;
				file->pMethods = &Methods;
			}
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT, trace, nullptr);
		}
	};
	// [[BPR-1111]]. Every SQLite allocation fails from the end of `Statement`'s
	// run until the connection closes, as it does when the process runs out of
	// memory: the statement the store prepares next meets SQLITE_NOMEM. The
	// heap limits are the process's, so both are put back when it closes.
	struct memory_fault
	{
		static inline std::string_view Statement;
		static inline bool             Active    = false;
		static inline sqlite3_int64    PriorSoft = 0;
		static auto                    trace(unsigned int aEvent, void*, void* aSubject, void*) -> int
		{
			if (aEvent == SQLITE_TRACE_CLOSE) {
				if (std::exchange(Active, false)) {
					static_cast<void>(sqlite3_hard_heap_limit64(0));
					static_cast<void>(sqlite3_soft_heap_limit64(PriorSoft));
				}
				return 0;
			}
			const auto* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(aSubject));
			if (!Statement.empty() && sql != nullptr && std::string_view{ sql } == Statement) {
				Statement = { };
				Active    = true;
				PriorSoft = sqlite3_soft_heap_limit64(-1);
				static_cast<void>(sqlite3_hard_heap_limit64(1));
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_PROFILE | SQLITE_TRACE_CLOSE, trace, nullptr);
		}
	};
	constexpr std::string_view current_rows = "SELECT profile,revision,host,hostname,account,server,provenance,issued,access_expiry,refresh_expiry,tombstone,bpr_profile FROM device_binding LIMIT 65";
	// A failure the environment caused is not a verdict on the file: the call
	// reports `unavailable`, and the same store reads the file on its next
	// call. Each fault is injected into one real read and counted.
	void transient_read_failures(const std::filesystem::path& aRoot)
	{
		using application::device_binding_failure;
		const auto root = aRoot / "transient";
		std::filesystem::create_directory(root);
		expect(adapters::sqlite::make_storage_root_owner_private(root), "fixture root is owner-private under the canonical storage policy");
		seed(root);
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(read_fault::install)) == SQLITE_OK, "register the read fault");
		struct injected_read
		{
			std::string_view Statement;
			int              Occurrence;
			std::string_view Where;
		};
		for (const auto& [statement, occurrence, where] : {
				 injected_read{ "PRAGMA quick_check", 1, "a failed page read inside quick_check, one of the judgement's later reads" },
				 injected_read{ "SELECT profile FROM binding_scope", 1, "a failed step of the judgement's scope read" },
				 injected_read{ current_rows, 1, "a failed step of the judgement's row read" },
				 injected_read{ current_rows, 2, "a failed step of the load's row read, after the judgement" },
			 }) {
			auto store             = adapters::sqlite::make_device_binding_store(root);
			read_fault::Injected   = 0;
			read_fault::Statement  = statement;
			read_fault::Occurrence = occurrence;
			const auto failed      = store->load({ });
			expect(read_fault::Injected == 1 && !read_fault::Armed, std::string{ where } + ": the fault was injected once");
			expect(!failed && failed.error() == device_binding_failure::unavailable, std::string{ where } + " is unavailable, not corrupt");
			const auto rows = store->load({ });
			expect(rows && rows->size() == 1U && rows->front() == record(), std::string{ where } + ": the same store reads the file on its next call");
		}
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(read_fault::install)) == 1, "remove the read fault");
		// Out of memory when the judgement prepares its row read.
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(memory_fault::install)) == SQLITE_OK, "register the memory fault");
		{
			auto store                = adapters::sqlite::make_device_binding_store(root);
			memory_fault::Statement   = "SELECT count(*) FROM binding_scope";
			const auto failed         = store->load({ });
			const auto armedAndLifted = memory_fault::Statement.empty() && !memory_fault::Active;
			expect(armedAndLifted && sqlite3_hard_heap_limit64(-1) == 0 && sqlite3_soft_heap_limit64(-1) == memory_fault::PriorSoft, "the memory fault ran and was lifted when its connection closed");
			expect(!failed && failed.error() == device_binding_failure::unavailable, "a failed prepare of the judgement's row read is unavailable, not corrupt");
			const auto rows = store->load({ });
			expect(rows && rows->size() == 1U && rows->front() == record(), "a failed prepare: the same store reads the file on its next call");
		}
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(memory_fault::install)) == 1, "remove the memory fault");
		// The results no contract can make SQLite produce on demand, pinned at
		// the classifier the reads share. PROTOCOL and BUSY_RECOVERY come from
		// the WAL index while another connection holds or recovers it.
		const auto judged = [](const int aCode, const bool aStop = false) { return adapters::sqlite::detail::testing::classify_device_binding_read(aCode, aStop); };
		for (const auto code : { SQLITE_IOERR, SQLITE_IOERR_READ, SQLITE_IOERR_SHORT_READ, SQLITE_IOERR_SHMMAP, SQLITE_NOMEM, SQLITE_PROTOCOL, SQLITE_BUSY, SQLITE_BUSY_RECOVERY, SQLITE_BUSY_SNAPSHOT, SQLITE_LOCKED, SQLITE_CANTOPEN, SQLITE_FULL, SQLITE_ERROR }) {
			expect(judged(code) == device_binding_failure::unavailable, "an environment failure is unavailable: " + std::to_string(code));
		}
		for (const auto code : { SQLITE_CORRUPT, SQLITE_CORRUPT_INDEX, SQLITE_NOTADB, SQLITE_TOOBIG, SQLITE_INTERRUPT }) {
			expect(judged(code) == device_binding_failure::corrupt, "a verdict on the file is corrupt: " + std::to_string(code));
		}
		expect(judged(SQLITE_INTERRUPT, true) == device_binding_failure::cancelled && judged(SQLITE_IOERR_READ, true) == device_binding_failure::cancelled,
			   "a read that was asked to stop is cancelled");
	}

	// [[BPR-0887]]. SQLite keeps one progress handler per connection. A nested
	// scope used to clear it on the way out, so a store's step ceiling lapsed at
	// the first nested execute. These checks read the handler by its effect: the
	// long query needs far more steps than any ceiling here.
	struct step_ceiling
	{
		std::size_t Limit = 0U;
		std::size_t Steps = 0U;
	};
	auto count_steps(void* aContext) -> int
	{
		auto& ceiling = *static_cast<step_ceiling*>(aContext);
		return ++ceiling.Steps >= ceiling.Limit ? 1 : 0;
	}
	auto run_query(sqlite3* aDatabase, const int aRows) -> int
	{
		const auto    sql       = "WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<" + std::to_string(aRows) + ") SELECT count(*) FROM n";
		sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(aDatabase, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK, "prepare a counting query");
		const auto result = sqlite3_step(statement);
		sqlite3_finalize(statement);
		return result;
	}
	auto long_query(sqlite3* aDatabase) -> int { return run_query(aDatabase, 100'000); }
	auto open_memory() -> sqlite3*
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(":memory:", &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK, "open an in-memory connection");
		return database;
	}
	void nested_progress_scopes()
	{
		namespace detail = adapters::sqlite::detail;
		expect(detail::sqlite_runtime_ready(), "initialize SQLite for the nesting checks");
		auto* const first  = open_memory();
		auto* const second = open_memory();
		expect(long_query(first) == SQLITE_ROW, "the long query completes with no handler installed");
		{
			auto       outer = step_ceiling{ .Limit = 50U };
			const auto scope = detail::progress_handler_scope{ first, 100, count_steps, &outer };
			expect(run_query(first, 100) == SQLITE_ROW && outer.Steps > 0U && outer.Steps < outer.Limit, "a short read counts against the outer ceiling");
			const auto counted = outer.Steps;
			expect(detail::execute(first, "CREATE TABLE t(x)").has_value(), "a nested execute succeeds inside a store scope");
			expect(outer.Steps == counted, "the nested execute's own handler governed while it ran");
			expect(long_query(first) == SQLITE_INTERRUPT && outer.Steps == outer.Limit, "the outer ceiling is still installed after a nested execute returns, with its count intact");
			expect(detail::execute(first, "CREATE TABLE big AS WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<100000) SELECT x FROM n").has_value(), "a spent outer ceiling does not cut short a nested execute that has no stop token");
			{
				auto       other  = step_ceiling{ .Limit = 50U };
				const auto nested = detail::progress_handler_scope{ second, 100, count_steps, &other };
				{
					const auto inner = detail::progress_scope{ first, { } };
					expect(long_query(first) == SQLITE_ROW, "the innermost scope on a connection governs it");
				}
				expect(long_query(first) == SQLITE_INTERRUPT, "ending a scope reinstates the nearest enclosing scope on the same connection");
				expect(long_query(second) == SQLITE_INTERRUPT && other.Steps == other.Limit, "a scope on another connection keeps its own ceiling");
			}
			expect(long_query(first) == SQLITE_INTERRUPT, "ending a scope on another connection leaves this one's ceiling alone");
			expect(long_query(second) == SQLITE_ROW, "the outermost scope on the other connection leaves no handler behind");
		}
		expect(long_query(first) == SQLITE_ROW, "the outermost scope leaves no handler behind");
		sqlite3_close(first);
		sqlite3_close(second);
	}

#if !defined(_WIN32)
	void in_child(void (*aPhase)(const std::filesystem::path&), const std::filesystem::path& aRoot)
	{
		const auto child = fork();
		expect(child >= 0, "spawn metadata-only process");
		if (child == 0) {
			// [[BPR-1121]]. The child never unwinds into this process's frames.
			bpr::tests::end_forked_child([&] { aPhase(aRoot); });
		}
		int status = 0;
		expect(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS, "metadata-only child passes");
	}
#endif
}
int main()
try {
	const auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) / ("bpr-device-binding-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directory(root);
	// [[BPR-1121]]. Owned from here, so a failure anywhere below removes it.
	const bpr::tests::scratch_guard owned{ root };
	expect(bpr::adapters::sqlite::make_storage_root_owner_private(root), "scratch root is owner-private under the canonical storage policy");
#if !defined(_WIN32)
	in_child(seed, root);
	in_child(reopen, root);
#else
	seed(root);
	reopen(root);
#endif
	bounded_preflight(root);
	scoped_storage_and_migration(root);
	contention_and_corruption(root);
	transient_read_failures(root);
	mutate(root, "CREATE TABLE extra(value TEXT)");
	expect(!adapters::sqlite::make_device_binding_store(root)->load({ }), "unknown schema object refuses whole catalog");
	mutate(root, "DROP TABLE extra; PRAGMA ignore_check_constraints=ON; UPDATE device_binding SET access_expiry=issued");
	expect(!adapters::sqlite::make_device_binding_store(root)->load({ }), "corrupt lifetime refuses whole catalog");
	std::filesystem::remove_all(root);
	std::stop_source stop;
	stop.request_stop();
	expect(!adapters::sqlite::make_device_binding_store(root)->load(stop.get_token()), "cancelled load refuses before filesystem work");
	expect(!std::filesystem::exists(root), "cancelled load creates no root");
	nested_progress_scopes();
	std::cout << "device-binding-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
