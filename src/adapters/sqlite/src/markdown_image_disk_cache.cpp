#include "bpr/adapters/sqlite/markdown_image_disk_cache.hpp"

#include "bpr/core/sha256.hpp"
#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <sqlite3.h>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr std::string_view file_name      = "markdown-images.sqlite3";
		constexpr std::int64_t     application_id = 0x42505249;
		constexpr std::string_view schema         = "CREATE TABLE images(sequence INTEGER PRIMARY KEY,digest BLOB NOT NULL UNIQUE,bytes BLOB NOT NULL,size INTEGER NOT NULL,used INTEGER NOT NULL) STRICT";
		// 160 MiB of 4 KiB pages: the 128 MiB of images, their overflow pages and the index.
		constexpr std::int64_t maximum_pages  = 40'960;
		constexpr std::int64_t expiry_seconds = std::int64_t{ core::markdown_image_disk_cache_days } * 24 * 60 * 60;

		struct connection
		{
			sqlite3* Database                                = nullptr;
			connection(const connection&)                    = delete;
			auto operator=(const connection&) -> connection& = delete;
			explicit connection(sqlite3* aDatabase)
				: Database{ aDatabase }
			{
			}
			~connection()
			{
				if (Database != nullptr) {
					sqlite3_close_v2(Database);
				}
			}
		};

		struct rollback
		{
			sqlite3* Database;
			bool     Committed                           = false;
			rollback(const rollback&)                    = delete;
			auto operator=(const rollback&) -> rollback& = delete;
			explicit rollback(sqlite3* aDatabase)
				: Database{ aDatabase }
			{
			}
			~rollback()
			{
				if (!Committed) {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
				}
			}
		};

		auto digest_of(const std::string& aKey) -> std::array<std::uint8_t, 32>
		{
			return core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(aKey.data()), aKey.size() });
		}

		auto schema_valid(sqlite3* aDatabase) -> bool
		{
			const auto tables     = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%' LIMIT 2)");
			const auto definition = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE name='images' AND type='table'");
			return detail::scalar_integer(aDatabase, "PRAGMA application_id") == application_id
				&& detail::scalar_integer(aDatabase, "PRAGMA user_version") == 1
				&& tables == 1 && definition && detail::canonical_sql(*definition) == detail::canonical_sql(schema);
		}

		/// Applies the project's connection policy, then this store's own limits,
		/// and creates the schema in an empty file.
		auto establish(sqlite3* aDatabase, const std::stop_token& aStop) -> bool
		{
			if (!detail::configure_connection(aDatabase)) {
				return false;
			}
			sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, static_cast<int>(core::markdown_image_encoded_bytes + 4096U));
			sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 4096);
			sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 16);
			const auto empty = detail::scalar_integer(aDatabase, "PRAGMA user_version") == 0
				&& detail::scalar_integer(aDatabase, "PRAGMA application_id") == 0
				&& detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema") == 0;
			if ((!empty && !schema_valid(aDatabase)) || aStop.stop_requested()
				|| !detail::execute(aDatabase, "PRAGMA max_page_count=40960", aStop)
				|| detail::scalar_integer(aDatabase, "PRAGMA max_page_count") != maximum_pages) {
				return false;
			}
			if (!empty) {
				return true;
			}
			// Freed pages go back to the file system, so the file follows its bound.
			if (!detail::execute(aDatabase, "PRAGMA auto_vacuum=FULL", aStop) || !detail::execute(aDatabase, "BEGIN IMMEDIATE", aStop)) {
				return false;
			}
			auto transaction = rollback{ aDatabase };
			if (!detail::execute(aDatabase, schema, aStop)
				|| !detail::execute(aDatabase, "PRAGMA application_id=1112560201", aStop)
				|| !detail::execute(aDatabase, "PRAGMA user_version=1", aStop)
				|| !schema_valid(aDatabase)
				|| !detail::execute(aDatabase, "COMMIT", aStop)) {
				return false;
			}
			transaction.Committed = true;
			return true;
		}

		auto bind_blob(sqlite3_stmt* aStatement, const int aColumn, const void* aData, const std::size_t aSize) -> bool
		{
			return aSize <= static_cast<std::size_t>(std::numeric_limits<int>::max())
				&& sqlite3_bind_blob(aStatement, aColumn, aData, static_cast<int>(aSize), SQLITE_TRANSIENT) == SQLITE_OK;
		}

		auto system_seconds() -> std::int64_t
		{
			return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}

		class disk_cache final : public application::markdown_image_disk_cache_port
		{
		public:
			disk_cache(std::filesystem::path aRoot, markdown_image_clock aNow, const std::size_t aBound)
				: Root{ std::move(aRoot) }
				, Now{ std::move(aNow) }
				, Bound{ aBound }
			{
				if (!Now) {
					Now = &system_seconds;
				}
			}

			auto load(const std::string& aKey, std::stop_token aStop) -> std::optional<std::string> override
			{
				const auto lock     = std::scoped_lock{ Serial };
				auto       database = open(aStop);
				if (!database) {
					return std::nullopt;
				}
				const auto digest   = digest_of(aKey);
				const auto now      = Now();
				const auto progress = detail::progress_scope{ database->Database, aStop };
				if (!detail::execute(database->Database, "BEGIN IMMEDIATE", aStop)) {
					return std::nullopt;
				}
				auto transaction = rollback{ database->Database };
				auto row         = detail::prepare(database->Database, "SELECT bytes,size,used FROM images WHERE digest=?1");
				if (!row || !bind_blob(row->Value, 1, digest.data(), digest.size()) || sqlite3_step(row->Value) != SQLITE_ROW) {
					return std::nullopt;
				}
				const auto size = sqlite3_column_bytes(row->Value, 0);
				const auto used = sqlite3_column_int64(row->Value, 2);
				if (sqlite3_column_type(row->Value, 0) != SQLITE_BLOB || size <= 0 || static_cast<std::size_t>(size) > core::markdown_image_encoded_bytes
					|| sqlite3_column_int64(row->Value, 1) != size) {
					return std::nullopt;
				}
				// Expired without use: a miss, and gone.
				if (now - used > expiry_seconds || used > now + expiry_seconds) {
					row = { };
					if (erase(database->Database, digest, aStop) && detail::execute(database->Database, "COMMIT", aStop)) {
						transaction.Committed = true;
					}
					return std::nullopt;
				}
				auto bytes = std::string(static_cast<std::size_t>(size), '\0');
				std::memcpy(bytes.data(), sqlite3_column_blob(row->Value, 0), bytes.size());
				row        = { };
				auto touch = detail::prepare(database->Database, "UPDATE images SET sequence=(SELECT max(sequence)+1 FROM images),used=?1 WHERE digest=?2");
				if (!touch || sqlite3_bind_int64(touch->Value, 1, now) != SQLITE_OK || !bind_blob(touch->Value, 2, digest.data(), digest.size())
					|| sqlite3_step(touch->Value) != SQLITE_DONE) {
					return std::nullopt;
				}
				touch = { };
				if (!detail::execute(database->Database, "COMMIT", aStop)) {
					return std::nullopt;
				}
				transaction.Committed = true;
				return bytes;
			}

			auto store(const std::string& aKey, const std::string_view aBytes, std::stop_token aStop) -> bool override
			{
				if (aBytes.empty() || aBytes.size() > core::markdown_image_encoded_bytes || aBytes.size() > Bound) {
					return false;
				}
				const auto lock     = std::scoped_lock{ Serial };
				auto       database = open(aStop);
				if (!database) {
					return false;
				}
				const auto digest   = digest_of(aKey);
				const auto now      = Now();
				const auto progress = detail::progress_scope{ database->Database, aStop };
				if (!detail::execute(database->Database, "BEGIN IMMEDIATE", aStop)) {
					return false;
				}
				auto transaction = rollback{ database->Database };
				auto expired     = detail::prepare(database->Database, "DELETE FROM images WHERE used<?1 OR used>?2");
				if (!expired || sqlite3_bind_int64(expired->Value, 1, now - expiry_seconds) != SQLITE_OK
					|| sqlite3_bind_int64(expired->Value, 2, now + expiry_seconds) != SQLITE_OK || sqlite3_step(expired->Value) != SQLITE_DONE) {
					return false;
				}
				expired  = { };
				auto row = detail::prepare(database->Database,
										   "INSERT OR REPLACE INTO images(sequence,digest,bytes,size,used) VALUES((SELECT coalesce(max(sequence),0)+1 FROM images),?1,?2,?3,?4)");
				if (!row || !bind_blob(row->Value, 1, digest.data(), digest.size()) || !bind_blob(row->Value, 2, aBytes.data(), aBytes.size())
					|| sqlite3_bind_int64(row->Value, 3, static_cast<std::int64_t>(aBytes.size())) != SQLITE_OK
					|| sqlite3_bind_int64(row->Value, 4, now) != SQLITE_OK || sqlite3_step(row->Value) != SQLITE_DONE) {
					return false;
				}
				row = { };
				// Least recently used first, until the images fit their bound.
				for (;;) {
					const auto total = detail::scalar_integer(database->Database, "SELECT coalesce(sum(size),0) FROM images");
					if (!total) {
						return false;
					}
					if (static_cast<std::uint64_t>(*total) <= Bound) {
						break;
					}
					if (!detail::execute(database->Database, "DELETE FROM images WHERE sequence=(SELECT min(sequence) FROM images)", aStop)) {
						return false;
					}
				}
				if (!detail::execute(database->Database, "COMMIT", aStop)) {
					return false;
				}
				transaction.Committed = true;
				return true;
			}

		private:
			auto open(const std::stop_token& aStop) const -> std::unique_ptr<connection>
			{
				if (aStop.stop_requested() || !detail::sqlite_runtime_ready()) {
					return nullptr;
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory) {
					return nullptr;
				}
				auto opened = detail::open_private_store_database(*directory, file_name, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					return nullptr;
				}
				auto database = std::make_unique<connection>(*opened);
				if (!establish(database->Database, aStop)) {
					return nullptr;
				}
				return database;
			}

			static auto erase(sqlite3* aDatabase, const std::array<std::uint8_t, 32>& aDigest, const std::stop_token& aStop) -> bool
			{
				auto row = detail::prepare(aDatabase, "DELETE FROM images WHERE digest=?1");
				return !aStop.stop_requested() && row && bind_blob(row->Value, 1, aDigest.data(), aDigest.size()) && sqlite3_step(row->Value) == SQLITE_DONE;
			}

			std::filesystem::path Root;
			markdown_image_clock  Now;
			std::size_t           Bound;
			std::mutex            Serial;
		};
	} // namespace

	auto make_markdown_image_disk_cache(std::filesystem::path aRoot, markdown_image_clock aNow, const std::size_t aBound)
		-> std::unique_ptr<application::markdown_image_disk_cache_port>
	{
		return std::make_unique<disk_cache>(std::move(aRoot), std::move(aNow), aBound);
	}
} // namespace bpr::adapters::sqlite
