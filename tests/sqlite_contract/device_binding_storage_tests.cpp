#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

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
		const auto cancelled     = adapters::sqlite::make_device_binding_store(aRoot)->load(stop.get_token());
		preflight_observer::Stop = nullptr;
		expect(!cancelled && cancelled.error() == application::device_binding_failure::cancelled && stop.stop_requested(), "cancellation during current-schema quick_check refuses explicitly");
		expect(preflight_observer::LengthLimit == (1'048'576) && preflight_observer::QuickSteps <= 256, "preflight has 1MiB bound and interrupts quick_check within a bounded short instruction budget");
		expect(database_bytes(aRoot) == bytes, "cancelled preflight leaves database bytes unchanged");
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(preflight_observer::install)) == 1, "remove preflight observer");
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
		expect(!adapters::sqlite::make_device_binding_store(legacyRoot)->load(stop.get_token()) && stop.stop_requested(), "interrupt Default adoption after row copy begins");
		migration_interruption::Stop = nullptr;
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(migration_interruption::install)) == 1, "remove migration interruption");
		expect(version(legacyRoot) == 1, "interrupted migration rolls back schema version");
		auto       adopted = adapters::sqlite::make_device_binding_store(legacyRoot);
		const auto rows    = adopted->load({ });
		expect(rows && rows->size() == 1U && rows->front() == record() && version(legacyRoot) == 2, "restart adopts legacy row exactly into Default schema2");
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
}
