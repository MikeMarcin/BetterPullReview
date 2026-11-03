#include "bpr/adapters/sqlite/view_filter_memory_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <memory>
#include <sqlite3.h>

namespace bpr::adapters::sqlite {
	namespace {
		/// 'BPVF'.
		constexpr std::int64_t ApplicationId = 0x42505646;

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
			auto handle = detail::open_private_store_database(*directory, "view-filters.sqlite3",
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
			// Each view's value is NULL until a reviewer chooses there, so a
			// later view joins as a NULL column without a migration.
			if (!detail::execute(db, "PRAGMA application_id=1112561222")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS view_filters(singleton INTEGER PRIMARY KEY CHECK(singleton=1), version INTEGER NOT NULL, discuss INTEGER, threads INTEGER, review_comments INTEGER) STRICT")) {
				return { };
			}
			return result;
		}

		/// A view's column: none when NULL; a value no version writes when it
		/// is not an integer, so the core reports it.
		auto column(sqlite3_stmt* aRow, const int aColumn) -> std::optional<std::int64_t>
		{
			switch (sqlite3_column_type(aRow, aColumn)) {
			case SQLITE_NULL:
				return std::nullopt;
			case SQLITE_INTEGER:
				return sqlite3_column_int64(aRow, aColumn);
			default:
				return -1;
			}
		}

		void bind(sqlite3_stmt* aRow, const int aIndex, const std::optional<std::int64_t>& aValue)
		{
			if (aValue) {
				sqlite3_bind_int64(aRow, aIndex, *aValue);
			} else {
				sqlite3_bind_null(aRow, aIndex);
			}
		}
	}

	auto load_view_filter_memory(const std::filesystem::path& aRoot) noexcept -> core::view_filter_memory_read
	{
		auto damaged      = core::view_filter_memory_read{ };
		damaged.Recovered = true;
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return damaged;
			}
			auto row = detail::prepare(connection->Database.get(), "SELECT version,discuss,threads,review_comments FROM view_filters WHERE singleton=1");
			if (!row) {
				return damaged;
			}
			const auto code = sqlite3_step(row->Value);
			if (code == SQLITE_DONE) {
				return core::read_view_filter_memory(std::nullopt);
			}
			if (code != SQLITE_ROW) {
				return damaged;
			}
			auto stored           = core::stored_view_filter_row{ };
			stored.Version        = sqlite3_column_type(row->Value, 0) == SQLITE_INTEGER ? sqlite3_column_int64(row->Value, 0) : 0;
			stored.Discuss        = column(row->Value, 1);
			stored.Threads        = column(row->Value, 2);
			stored.ReviewComments = column(row->Value, 3);
			return core::read_view_filter_memory(stored);
		} catch (...) {
			return damaged;
		}
	}

	auto store_view_filter_memory(const std::filesystem::path& aRoot, const core::view_filter_defaults& aDefaults) noexcept -> bool
	{
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return false;
			}
			auto* const db  = connection->Database.get();
			auto        row = detail::prepare(db,
											  "INSERT INTO view_filters VALUES(1,?1,?2,?3,?4) ON CONFLICT(singleton) DO UPDATE SET "
											  "version=excluded.version,discuss=excluded.discuss,threads=excluded.threads,review_comments=excluded.review_comments "
											  "WHERE view_filters.version<=excluded.version");
			if (!row) {
				return false;
			}
			const auto stored = core::stored_view_filter_memory(aDefaults);
			sqlite3_bind_int64(row->Value, 1, stored.Version);
			bind(row->Value, 2, stored.Discuss);
			bind(row->Value, 3, stored.Threads);
			bind(row->Value, 4, stored.ReviewComments);
			return sqlite3_step(row->Value) == SQLITE_DONE && sqlite3_changes(db) == 1;
		} catch (...) {
			return false;
		}
	}
}
