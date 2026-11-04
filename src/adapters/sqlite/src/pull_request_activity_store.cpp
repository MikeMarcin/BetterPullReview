#include "bpr/adapters/sqlite/pull_request_activity_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <memory>
#include <sqlite3.h>
#include <string>

namespace bpr::adapters::sqlite {
	namespace {
		/// 'BPPA'.
		constexpr std::int64_t ApplicationId = 0x42505041;

		struct connection
		{
			detail::state_directory                               Directory;
			std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> Database{ nullptr, sqlite3_close_v2 };
		};

		auto open(const std::filesystem::path& aRoot) -> std::unique_ptr<connection>
		{
			if (!detail::sqlite_runtime_ready()) {
				return { };
			}
			auto directory = detail::open_state_directory(aRoot);
			if (!directory) {
				return { };
			}
			auto handle = detail::open_private_store_database(*directory, "pull-request-activity.sqlite3",
															  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_NOFOLLOW);
			if (!handle) {
				return { };
			}
			auto  result = std::make_unique<connection>(connection{ std::move(*directory), { *handle, sqlite3_close_v2 } });
			auto* db     = result->Database.get();
			if (!detail::configure_connection(db)) {
				return { };
			}
			const auto identity = detail::scalar_integer(db, "PRAGMA application_id");
			if (!identity || (*identity != 0 && *identity != ApplicationId)) {
				return { };
			}
			if (!detail::execute(db, "PRAGMA application_id=1112559681")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS activity(singleton INTEGER PRIMARY KEY CHECK(singleton=1), version INTEGER NOT NULL, enabled INTEGER, baselined INTEGER, account TEXT, last_modified TEXT) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS activity_seen(thread TEXT PRIMARY KEY, updated_at INTEGER NOT NULL) STRICT, WITHOUT ROWID")) {
				return { };
			}
			return result;
		}

		/// An integer column, or a value no version writes.
		auto integer(sqlite3_stmt* aRow, const int aColumn, const std::int64_t aMissing) -> std::int64_t
		{
			switch (sqlite3_column_type(aRow, aColumn)) {
			case SQLITE_NULL:    return aMissing;
			case SQLITE_INTEGER: return sqlite3_column_int64(aRow, aColumn);
			default:             return -1;
			}
		}

		auto text(sqlite3_stmt* aRow, const int aColumn) -> std::string
		{
			if (sqlite3_column_type(aRow, aColumn) != SQLITE_TEXT) {
				return { };
			}
			const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(aRow, aColumn));
			return bytes == nullptr ? std::string{ } : std::string{ bytes, static_cast<std::size_t>(sqlite3_column_bytes(aRow, aColumn)) };
		}

		void bind_text(sqlite3_stmt* aRow, const int aIndex, const std::string& aValue)
		{
			sqlite3_bind_text(aRow, aIndex, aValue.data(), static_cast<int>(aValue.size()), SQLITE_TRANSIENT);
		}
	}

	auto load_pull_request_activity(const std::filesystem::path& aRoot) noexcept -> core::activity_memory_read
	{
		auto damaged      = core::activity_memory_read{ };
		damaged.Recovered = true;
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return damaged;
			}
			auto* const db  = connection->Database.get();
			auto        row = detail::prepare(db, "SELECT version,enabled,baselined,account,last_modified FROM activity WHERE singleton=1");
			if (!row) {
				return damaged;
			}
			const auto code = sqlite3_step(row->Value);
			if (code == SQLITE_DONE) {
				return core::read_activity_memory(std::nullopt);
			}
			if (code != SQLITE_ROW) {
				return damaged;
			}
			auto stored         = core::stored_activity_memory{ };
			stored.Version      = sqlite3_column_type(row->Value, 0) == SQLITE_INTEGER ? sqlite3_column_int64(row->Value, 0) : 0;
			stored.Enabled      = integer(row->Value, 1, 1);
			stored.Baselined    = integer(row->Value, 2, 0);
			stored.Account      = text(row->Value, 3);
			stored.LastModified = text(row->Value, 4);
			auto seen           = detail::prepare(db, "SELECT thread,updated_at FROM activity_seen ORDER BY thread LIMIT 1024");
			if (!seen) {
				return damaged;
			}
			while (sqlite3_step(seen->Value) == SQLITE_ROW) {
				stored.Seen.push_back({ .Thread = text(seen->Value, 0), .UpdatedAt = integer(seen->Value, 1, 0) });
			}
			return core::read_activity_memory(stored);
		} catch (...) {
			return damaged;
		}
	}

	auto store_pull_request_activity(const std::filesystem::path& aRoot, const core::activity_memory& aMemory) noexcept -> bool
	{
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return false;
			}
			auto* const db = connection->Database.get();
			if (!detail::execute(db, "BEGIN IMMEDIATE")) {
				return false;
			}
			const auto rollback = [db] {
				static_cast<void>(detail::execute(db, "ROLLBACK"));
				return false;
			};
			const auto stored = core::stored_activity_rows(aMemory);
			auto       row    = detail::prepare(db,
												"INSERT INTO activity VALUES(1,?1,?2,?3,?4,?5) ON CONFLICT(singleton) DO UPDATE SET "
												"version=excluded.version,enabled=excluded.enabled,baselined=excluded.baselined,account=excluded.account,last_modified=excluded.last_modified "
												"WHERE activity.version<=excluded.version");
			if (!row) {
				return rollback();
			}
			sqlite3_bind_int64(row->Value, 1, stored.Version);
			sqlite3_bind_int64(row->Value, 2, stored.Enabled);
			sqlite3_bind_int64(row->Value, 3, stored.Baselined);
			bind_text(row->Value, 4, stored.Account);
			bind_text(row->Value, 5, stored.LastModified);
			// A newer build's row is left as it was, and so are its threads.
			if (sqlite3_step(row->Value) != SQLITE_DONE || sqlite3_changes(db) != 1) {
				return rollback();
			}
			if (!detail::execute(db, "DELETE FROM activity_seen")) {
				return rollback();
			}
			auto insert = detail::prepare(db, "INSERT INTO activity_seen VALUES(?1,?2)");
			if (!insert) {
				return rollback();
			}
			for (const auto& entry : stored.Seen) {
				sqlite3_reset(insert->Value);
				bind_text(insert->Value, 1, entry.Thread);
				sqlite3_bind_int64(insert->Value, 2, entry.UpdatedAt);
				if (sqlite3_step(insert->Value) != SQLITE_DONE) {
					return rollback();
				}
			}
			return detail::execute(db, "COMMIT").has_value() || rollback();
		} catch (...) {
			return false;
		}
	}
}
