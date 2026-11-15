#include "bpr/adapters/sqlite/discuss_preferences_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <memory>
#include <sqlite3.h>
#include <string>

namespace bpr::adapters::sqlite {
	namespace {
		/// 'BPDS'.
		constexpr std::int64_t ApplicationId = 0x42504453;

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
			auto handle = detail::open_private_store_database(*directory, "discuss.sqlite3",
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
			if (!detail::execute(db, "PRAGMA application_id=1112556627")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS discuss(singleton INTEGER PRIMARY KEY CHECK(singleton=1), version INTEGER NOT NULL, discuss_column INTEGER NOT NULL) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS author_marks(repository TEXT NOT NULL CHECK(length(CAST(repository AS BLOB)) <= 512), login TEXT NOT NULL CHECK(length(CAST(login AS BLOB)) <= 512), bot INTEGER NOT NULL, PRIMARY KEY(repository, login)) STRICT")) {
				return { };
			}
			return result;
		}

		[[nodiscard]] auto text_of(sqlite3_stmt* aRow, const int aColumn) -> std::string
		{
			const auto* const bytes = sqlite3_column_text(aRow, aColumn);
			return bytes == nullptr ? std::string{ } : std::string{ reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(sqlite3_column_bytes(aRow, aColumn)) };
		}
	}

	auto load_discuss_preferences(const std::filesystem::path& aRoot) noexcept -> core::discuss_preferences_read
	{
		auto damaged      = core::discuss_preferences_read{ };
		damaged.Recovered = true;
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return damaged;
			}
			auto* const db  = connection->Database.get();
			auto        row = detail::prepare(db, "SELECT version,discuss_column FROM discuss WHERE singleton=1");
			if (!row) {
				return damaged;
			}
			const auto code = sqlite3_step(row->Value);
			if (code == SQLITE_DONE) {
				return core::read_discuss_preferences(std::nullopt);
			}
			if (code != SQLITE_ROW) {
				return damaged;
			}
			auto stored    = core::stored_discuss_preferences{ };
			stored.Version = sqlite3_column_int64(row->Value, 0);
			stored.Column  = sqlite3_column_int64(row->Value, 1);
			auto marks     = detail::prepare(db, "SELECT repository,login,bot FROM author_marks ORDER BY repository,login");
			if (!marks) {
				return damaged;
			}
			for (auto step = sqlite3_step(marks->Value); step != SQLITE_DONE; step = sqlite3_step(marks->Value)) {
				if (step != SQLITE_ROW) {
					return damaged;
				}
				stored.Marks.push_back(core::stored_author_mark{ .Repository = text_of(marks->Value, 0), .Login = text_of(marks->Value, 1), .Bot = sqlite3_column_int64(marks->Value, 2) });
			}
			return core::read_discuss_preferences(stored);
		} catch (...) {
			return damaged;
		}
	}

	auto store_discuss_preferences(const std::filesystem::path& aRoot, const core::discuss_preferences& aPreferences) noexcept -> bool
	{
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return false;
			}
			auto* const db      = connection->Database.get();
			const auto  version = detail::scalar_integer(db, "SELECT coalesce(max(version),0) FROM discuss");
			if (!version || *version > core::discuss_preferences_version) {
				return false;
			}
			const auto stored = core::stored_discuss_preferences_of(aPreferences);
			if (!detail::execute(db, "BEGIN IMMEDIATE")) {
				return false;
			}
			auto ok  = true;
			auto row = detail::prepare(db, "INSERT INTO discuss VALUES(1,?1,?2) ON CONFLICT(singleton) DO UPDATE SET version=excluded.version,discuss_column=excluded.discuss_column");
			ok       = row && sqlite3_bind_int64(row->Value, 1, stored.Version) == SQLITE_OK && sqlite3_bind_int64(row->Value, 2, stored.Column) == SQLITE_OK && sqlite3_step(row->Value) == SQLITE_DONE;
			ok       = ok && detail::execute(db, "DELETE FROM author_marks");
			if (ok) {
				auto mark = detail::prepare(db, "INSERT INTO author_marks VALUES(?1,?2,?3)");
				ok        = static_cast<bool>(mark);
				for (const auto& one : stored.Marks) {
					if (!ok) {
						break;
					}
					sqlite3_reset(mark->Value);
					ok = sqlite3_bind_text(mark->Value, 1, one.Repository.data(), static_cast<int>(one.Repository.size()), SQLITE_TRANSIENT) == SQLITE_OK
						&& sqlite3_bind_text(mark->Value, 2, one.Login.data(), static_cast<int>(one.Login.size()), SQLITE_TRANSIENT) == SQLITE_OK
						&& sqlite3_bind_int64(mark->Value, 3, one.Bot) == SQLITE_OK && sqlite3_step(mark->Value) == SQLITE_DONE;
				}
			}
			if (!ok) {
				static_cast<void>(detail::execute(db, "ROLLBACK"));
				return false;
			}
			return static_cast<bool>(detail::execute(db, "COMMIT"));
		} catch (...) {
			return false;
		}
	}
}
