#include "bpr/adapters/sqlite/cached_review_overview_store.hpp"
#include "bpr/core/sha256.hpp"
#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <sqlite3.h>
#include <vector>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr std::int64_t     application_id = 0x4250524f;
		constexpr std::string_view schema         = "CREATE TABLE overviews(sequence INTEGER PRIMARY KEY,key BLOB NOT NULL UNIQUE,payload BLOB NOT NULL,digest BLOB NOT NULL) STRICT";
		static_assert(core::maximum_cached_overviews == 10);

		// Versioned, length-prefixed encoding. Read lengths are checked before allocation.
		struct codec
		{
			std::vector<std::uint8_t>     Bytes;
			std::span<const std::uint8_t> Input;
			bool                          Reading = false, Good = true;
			void                          integer(std::uint64_t& aValue)
			{
				if (!Reading) {
					for (unsigned shift = 64; shift != 0; shift -= 8) {
						Bytes.push_back(static_cast<std::uint8_t>(aValue >> (shift - 8)));
					}
					return;
				}
				if (Input.size() < 8) {
					Good = false;
					return;
				}
				aValue = 0;
				for (auto value : Input.first(8)) {
					aValue = (aValue << 8U) | static_cast<std::uint64_t>(value);
				}
				Input = Input.subspan(8);
			}
			template <typename T>
			void number(T& aValue)
			{
				auto value = static_cast<std::uint64_t>(aValue);
				integer(value);
				if (value > std::numeric_limits<T>::max()) {
					Good = false;
					return;
				}
				aValue = static_cast<T>(value);
			}
			template <typename T>
			void identity(T& aValue)
			{
				auto value = aValue.value();
				number(value);
				aValue = T{ value };
			}
			void text(std::string& aText, std::size_t aLimit)
			{
				auto size = static_cast<std::uint64_t>(aText.size());
				integer(size);
				if (!Good || size > aLimit || (Reading && size > Input.size())) {
					Good = false;
					return;
				}
				if (Reading) {
					aText.assign(reinterpret_cast<const char*>(Input.data()), static_cast<std::size_t>(size));
					Input = Input.subspan(static_cast<std::size_t>(size));
				} else {
					Bytes.insert(Bytes.end(), aText.begin(), aText.end());
				}
			}
			void key(core::cached_review_key& aKey)
			{
				auto version = std::uint64_t{ 1 };
				integer(version);
				if (version != 1) {
					Good = false;
				}
				for (auto& byte : aKey.Authority) {
					number(byte);
				}
				identity(aKey.Profile);
				identity(aKey.Host);
				identity(aKey.Account);
				text(aKey.Locator.Owner, 256);
				text(aKey.Locator.Name, 256);
				identity(aKey.Number);
			}
			void catalogue(std::optional<std::vector<core::cached_review_file>>& aFiles)
			{
				// The original payload has no extension: unknown, never observed empty.
				if (Reading && Input.empty()) {
					return;
				}
				auto version = std::uint64_t{ 2 };
				integer(version);
				bool available = aFiles.has_value();
				number(available);
				if (!Good || version != 2) {
					Good = false;
					return;
				}
				if (!available) {
					return;
				}
				auto count = static_cast<std::uint64_t>(aFiles ? aFiles->size() : 0);
				integer(count);
				if (!Good || count > core::maximum_cached_files || (Reading && count > Input.size() / 40)) {
					Good = false;
					return;
				}
				if (Reading) {
					aFiles.emplace(static_cast<std::size_t>(count));
				}
				for (auto& file : *aFiles) {
					text(file.Path, 4096);
					bool previous = file.PreviousPath.has_value();
					number(previous);
					if (previous) {
						if (Reading) {
							file.PreviousPath.emplace();
						}
						text(*file.PreviousPath, 4096);
					}
					auto change = static_cast<std::uint8_t>(file.Change);
					number(change);
					file.Change = static_cast<core::cached_file_change>(change);
					integer(file.Additions);
					integer(file.Deletions);
					if (!Good) {
						return;
					}
				}
			}
			void overview(core::cached_review_overview& aOverview)
			{
				key(aOverview.Key);
				identity(aOverview.Repository);
				identity(aOverview.PullRequest);
				text(aOverview.BaseOid, 64);
				text(aOverview.HeadOid, 64);
				text(aOverview.Title, 1024);
				text(aOverview.Body, 256 * 1024);
				text(aOverview.AuthorLogin, 256);
				text(aOverview.BaseRef, 256);
				text(aOverview.HeadRef, 256);
				number(aOverview.Files);
				number(aOverview.Commits);
				number(aOverview.State);
				number(aOverview.Draft);
				if (Good) {
					catalogue(aOverview.FileCatalogue);
				}
			}
		};
		auto encoded_key(core::cached_review_key aKey) -> std::vector<std::uint8_t>
		{
			codec bytes;
			bytes.key(aKey);
			return bytes.Good ? std::move(bytes.Bytes) : std::vector<std::uint8_t>{ };
		}
		auto bind_blob(sqlite3_stmt* aStatement, int aColumn, std::span<const std::uint8_t> aBytes) -> bool
		{
			return sqlite3_bind_blob(aStatement, aColumn, aBytes.data(), static_cast<int>(aBytes.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		auto blob(sqlite3_stmt* aStatement, int aColumn, std::size_t aLimit) -> std::span<const std::uint8_t>
		{
			const auto size = sqlite3_column_bytes(aStatement, aColumn);
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_BLOB || size <= 0 || static_cast<std::size_t>(size) > aLimit) {
				return { };
			}
			return { static_cast<const std::uint8_t*>(sqlite3_column_blob(aStatement, aColumn)), static_cast<std::size_t>(size) };
		}
		struct connection
		{
			sqlite3* Database = nullptr;
			~connection()
			{
				if (Database) {
					sqlite3_close_v2(Database);
				}
			}
		};
		struct rollback
		{
			sqlite3* Database;
			~rollback() { static_cast<void>(detail::execute(Database, "ROLLBACK")); }
		};
		struct read_budget
		{
			std::stop_token Stop;
			unsigned        Remaining = 10000;
			static auto     progress(void* aValue) -> int
			{
				auto& self = *static_cast<read_budget*>(aValue);
				return self.Stop.stop_requested() || self.Remaining-- == 0;
			}
		};
		auto schema_valid(sqlite3* aDatabase) -> bool
		{
			const auto tables     = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 2)");
			const auto definition = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE name='overviews' AND type='table'");
			return detail::scalar_integer(aDatabase, "PRAGMA application_id") == application_id
				&& detail::scalar_integer(aDatabase, "PRAGMA user_version") == 1
				&& tables == 1 && definition && detail::canonical_sql(*definition) == detail::canonical_sql(schema);
		}
		auto commit(sqlite3* aDatabase, std::stop_token aStop) -> bool
		{
			if (aStop.stop_requested()) {
				return false;
			}
			const auto progress  = detail::progress_scope{ aDatabase, aStop };
			auto       statement = detail::prepare(aDatabase, "COMMIT");
			// A durable COMMIT wins cancellation observed after sqlite3_step succeeds.
			return statement && sqlite3_step(statement->Value) == SQLITE_DONE;
		}
		auto establish(sqlite3* aDatabase, std::stop_token aStop) -> bool
		{
			sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, static_cast<int>(core::maximum_cached_overview_bytes + 4096));
			sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 4096);
			sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 16);
			const auto empty = detail::scalar_integer(aDatabase, "PRAGMA user_version") == 0
				&& detail::scalar_integer(aDatabase, "PRAGMA application_id") == 0
				&& detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema") == 0;
			if ((!empty && !schema_valid(aDatabase)) || aStop.stop_requested()
				|| !detail::configure_connection(aDatabase)
				|| !detail::execute(aDatabase, "PRAGMA max_page_count=2048", aStop)) {
				return false;
			}
			if (detail::scalar_integer(aDatabase, "PRAGMA max_page_count") != 2048) {
				return false;
			}
			if (!empty) {
				return true;
			}
			if (!detail::execute(aDatabase, "BEGIN IMMEDIATE", aStop)) {
				return false;
			}
			const auto transaction = rollback{ aDatabase };
			if (!detail::execute(aDatabase, schema, aStop)
				|| !detail::execute(aDatabase, "PRAGMA application_id=1112560207", aStop)
				|| !detail::execute(aDatabase, "PRAGMA user_version=1", aStop)) {
				return false;
			}
			return schema_valid(aDatabase) && commit(aDatabase, aStop);
		}
		class overview_store final : public application::cached_review_overview_port
		{
		public:
			explicit overview_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			auto load(const core::cached_review_key& aKey, std::stop_token aStop) -> std::optional<core::cached_review_overview> override
			{
				if (!core::valid(aKey) || aStop.stop_requested()) {
					return { };
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return { };
				}
				auto opened = detail::open_private_store_database(*directory, "review-overviews.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					return { };
				}
				connection  database{ *opened };
				read_budget budget{ aStop };
				const auto  bounded = detail::progress_handler_scope{ database.Database, 1000, &read_budget::progress, &budget };
				if (!establish(database.Database, aStop)) {
					return { };
				}
				auto       row = detail::prepare(database.Database, "SELECT payload,digest FROM overviews WHERE key=?1 LIMIT 1");
				const auto key = encoded_key(aKey);
				if (!row || !bind_blob(row->Value, 1, key) || sqlite3_step(row->Value) != SQLITE_ROW) {
					return { };
				}
				const auto payload = blob(row->Value, 0, core::maximum_cached_overview_bytes);
				const auto digest  = blob(row->Value, 1, 32);
				if (payload.empty() || digest.size() != 32 || !std::ranges::equal(core::sha256(payload), digest)) {
					return { };
				}
				core::cached_review_overview result;
				codec                        bytes{ { }, payload, true };
				bytes.overview(result);
				if (!bytes.Good || !bytes.Input.empty() || result.Key != aKey || !core::valid(result) || aStop.stop_requested()) {
					return { };
				}
				return result;
			}
			auto save(const core::cached_review_overview& aOverview, std::stop_token aStop) -> bool override
			{
				if (!core::valid(aOverview) || aStop.stop_requested()) {
					return false;
				}
				auto  candidate = aOverview;
				codec bytes;
				bytes.overview(candidate);
				if (!bytes.Good || bytes.Bytes.size() > core::maximum_cached_overview_bytes) {
					return false;
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return false;
				}
				auto opened = detail::open_private_store_database(*directory, "review-overviews.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					return false;
				}
				connection  database{ *opened };
				read_budget budget{ aStop };
				const auto  bounded = detail::progress_handler_scope{ database.Database, 1000, &read_budget::progress, &budget };
				if (!establish(database.Database, aStop) || !detail::execute(database.Database, "BEGIN IMMEDIATE", aStop)) {
					return false;
				}
				const auto transaction = rollback{ database.Database };
				const auto count       = detail::scalar_integer(database.Database, "SELECT count(*) FROM (SELECT 1 FROM overviews LIMIT 11)");
				const auto serial      = detail::scalar_integer(database.Database, "SELECT coalesce(max(sequence),0) FROM overviews");
				if (!count || *count > 10 || !serial || *serial < 0 || *serial == std::numeric_limits<std::int64_t>::max()) {
					return false;
				}
				auto       row = detail::prepare(database.Database, "INSERT OR REPLACE INTO overviews(sequence,key,payload,digest) VALUES(?1,?2,?3,?4)");
				const auto key = encoded_key(aOverview.Key);
				if (!row || sqlite3_bind_int64(row->Value, 1, *serial + 1) != SQLITE_OK
					|| !bind_blob(row->Value, 2, key) || !bind_blob(row->Value, 3, bytes.Bytes)
					|| !bind_blob(row->Value, 4, core::sha256(bytes.Bytes)) || sqlite3_step(row->Value) != SQLITE_DONE) {
					return false;
				}
				return detail::execute(database.Database, "DELETE FROM overviews WHERE sequence NOT IN (SELECT sequence FROM overviews ORDER BY sequence DESC LIMIT 10)", aStop)
					&& commit(database.Database, aStop);
			}

		private:
			std::filesystem::path Root;
		};
	}
	auto make_cached_review_overview_store(std::filesystem::path aRoot) -> std::unique_ptr<application::cached_review_overview_port>
	{
		return std::make_unique<overview_store>(std::move(aRoot));
	}
}
