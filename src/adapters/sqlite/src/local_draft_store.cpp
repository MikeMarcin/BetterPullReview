#include "bpr/adapters/sqlite/local_draft_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		/// 'BPLD'.
		constexpr std::int64_t application_id = 0x42504C44;

		using application::local_draft_store_error;
		using application::local_draft_store_failure;

		[[nodiscard]] auto failed(const local_draft_store_failure aFailure, const std::uint64_t aUsed = 0U, const std::uint64_t aLimit = 0U)
			-> std::unexpected<local_draft_store_error>
		{
			return std::unexpected{ local_draft_store_error{ .Failure = aFailure, .Used = aUsed, .Limit = aLimit } };
		}

		[[nodiscard]] auto text_of(sqlite3_stmt* aRow, const int aColumn) -> std::string
		{
			const auto* const bytes = sqlite3_column_blob(aRow, aColumn);
			const auto        size  = sqlite3_column_bytes(aRow, aColumn);
			return bytes == nullptr ? std::string{ } : std::string{ static_cast<const char*>(bytes), static_cast<std::size_t>(size) };
		}

		void bind_text(sqlite3_stmt* aRow, const int aIndex, const std::string_view aText)
		{
			sqlite3_bind_text(aRow, aIndex, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT);
		}

		/// Every object a draft's candidates name.
		[[nodiscard]] auto named_objects(const core::local_draft& aDraft) -> std::set<std::string>
		{
			auto names = std::set<std::string>{ };
			for (const auto& candidate : aDraft.Candidates) {
				for (const auto& change : candidate.Changes) {
					for (const auto* side : { &change.Left, &change.Right }) {
						if (*side) {
							names.emplace(core::canonical_hex((*side)->Object).view());
						}
					}
				}
			}
			return names;
		}

		class sqlite_local_draft_store final : public application::local_draft_store
		{
		public:
			sqlite_local_draft_store(detail::state_directory aDirectory, sqlite3* aDatabase)
				: Directory{ std::move(aDirectory) }
				, Database{ aDatabase, sqlite3_close_v2 }
			{
			}

			auto list() -> application::local_draft_store_result<std::vector<core::local_draft>> override
			{
				auto row = detail::prepare(db(), "SELECT body FROM drafts ORDER BY id");
				if (!row) {
					return failed(local_draft_store_failure::unavailable);
				}
				auto drafts = std::vector<core::local_draft>{ };
				auto code   = SQLITE_ROW;
				while ((code = sqlite3_step(row->Value)) == SQLITE_ROW) {
					// A record this build cannot read stays stored; the rest list.
					if (auto draft = core::decode_local_draft(text_of(row->Value, 0))) {
						drafts.push_back(std::move(*draft));
					}
				}
				if (code != SQLITE_DONE) {
					return failed(local_draft_store_failure::unavailable);
				}
				return drafts;
			}

			auto find(const core::local_draft_id& aId) -> application::local_draft_store_result<core::local_draft> override
			{
				auto row = detail::prepare(db(), "SELECT body FROM drafts WHERE id=?1");
				if (!row) {
					return failed(local_draft_store_failure::unavailable);
				}
				bind_text(row->Value, 1, aId.Value);
				const auto code = sqlite3_step(row->Value);
				if (code == SQLITE_DONE) {
					return failed(local_draft_store_failure::missing);
				}
				if (code != SQLITE_ROW) {
					return failed(local_draft_store_failure::unavailable);
				}
				auto draft = core::decode_local_draft(text_of(row->Value, 0));
				if (!draft) {
					return failed(local_draft_store_failure::corrupt);
				}
				return std::move(*draft);
			}

			auto put(const core::local_draft& aDraft, const std::optional<std::uint64_t> aPrevious, const std::span<const application::local_draft_object> aObjects)
				-> application::local_draft_store_result<void> override
			{
				if (!core::valid(aDraft)) {
					return failed(local_draft_store_failure::corrupt);
				}
				const auto body = core::encode_local_draft(aDraft);
				if (body.empty() || !detail::execute(db(), "BEGIN IMMEDIATE")) {
					return failed(local_draft_store_failure::unavailable);
				}
				auto result = write(aDraft, body, aPrevious, aObjects);
				if (result && detail::execute(db(), "COMMIT")) {
					return result;
				}
				(void)detail::execute(db(), "ROLLBACK");
				return result ? failed(local_draft_store_failure::unavailable) : result;
			}

			auto remove(const core::local_draft_id& aId) -> application::local_draft_store_result<void> override
			{
				if (!detail::execute(db(), "BEGIN IMMEDIATE")) {
					return failed(local_draft_store_failure::unavailable);
				}
				auto drop = detail::prepare(db(), "DELETE FROM drafts WHERE id=?1");
				auto pins = detail::prepare(db(), "DELETE FROM pins WHERE draft=?1");
				if (!drop || !pins) {
					(void)detail::execute(db(), "ROLLBACK");
					return failed(local_draft_store_failure::unavailable);
				}
				bind_text(drop->Value, 1, aId.Value);
				bind_text(pins->Value, 1, aId.Value);
				if (sqlite3_step(drop->Value) != SQLITE_DONE) {
					(void)detail::execute(db(), "ROLLBACK");
					return failed(local_draft_store_failure::unavailable);
				}
				if (sqlite3_changes(db()) == 0) {
					(void)detail::execute(db(), "ROLLBACK");
					return failed(local_draft_store_failure::missing);
				}
				if (sqlite3_step(pins->Value) != SQLITE_DONE || !detail::execute(db(), "DELETE FROM objects WHERE object NOT IN (SELECT object FROM pins)")
					|| !detail::execute(db(), "COMMIT")) {
					(void)detail::execute(db(), "ROLLBACK");
					return failed(local_draft_store_failure::unavailable);
				}
				return { };
			}

			auto object(const core::git_entry_oid& aObject) -> application::local_draft_store_result<std::optional<std::vector<std::uint8_t>>> override
			{
				auto row = detail::prepare(db(), "SELECT bytes FROM objects WHERE object=?1");
				if (!row) {
					return failed(local_draft_store_failure::unavailable);
				}
				bind_text(row->Value, 1, core::canonical_hex(aObject).view());
				const auto code = sqlite3_step(row->Value);
				if (code == SQLITE_DONE) {
					return std::optional<std::vector<std::uint8_t>>{ };
				}
				if (code != SQLITE_ROW) {
					return failed(local_draft_store_failure::unavailable);
				}
				const auto* const bytes = static_cast<const std::uint8_t*>(sqlite3_column_blob(row->Value, 0));
				const auto        size  = static_cast<std::size_t>(sqlite3_column_bytes(row->Value, 0));
				return std::optional{ bytes == nullptr ? std::vector<std::uint8_t>{ } : std::vector<std::uint8_t>(bytes, bytes + size) };
			}

		private:
			[[nodiscard]] auto db() const noexcept -> sqlite3*
			{
				return Database.get();
			}

			/// Inside the caller's transaction.
			auto write(const core::local_draft& aDraft, const std::string& aBody, const std::optional<std::uint64_t> aPrevious,
					   const std::span<const application::local_draft_object> aObjects) -> application::local_draft_store_result<void>
			{
				auto current = detail::prepare(db(), "SELECT version FROM drafts WHERE id=?1");
				if (!current) {
					return failed(local_draft_store_failure::unavailable);
				}
				bind_text(current->Value, 1, aDraft.Id.Value);
				const auto code = sqlite3_step(current->Value);
				if (code != SQLITE_ROW && code != SQLITE_DONE) {
					return failed(local_draft_store_failure::unavailable);
				}
				if (aPrevious) {
					if (code == SQLITE_DONE) {
						return failed(local_draft_store_failure::missing);
					}
					if (static_cast<std::uint64_t>(sqlite3_column_int64(current->Value, 0)) != *aPrevious) {
						return failed(local_draft_store_failure::stale);
					}
				} else {
					if (code == SQLITE_ROW) {
						return failed(local_draft_store_failure::stale);
					}
					const auto count = detail::scalar_integer(db(), "SELECT COUNT(*) FROM drafts");
					if (!count) {
						return failed(local_draft_store_failure::unavailable);
					}
					if (static_cast<std::uint64_t>(*count) >= core::maximum_local_drafts) {
						return failed(local_draft_store_failure::full, static_cast<std::uint64_t>(*count), core::maximum_local_drafts);
					}
				}
				const auto used = detail::scalar_integer(db(), "SELECT COALESCE(SUM(size),0) FROM objects");
				auto       have = detail::prepare(db(), "SELECT 1 FROM objects WHERE object=?1");
				if (!used || !have) {
					return failed(local_draft_store_failure::unavailable);
				}
				auto added = std::uint64_t{ 0U };
				auto seen  = std::set<std::string>{ };
				for (const auto& object : aObjects) {
					auto name = std::string{ core::canonical_hex(object.Object).view() };
					if (!seen.insert(name).second) {
						continue;
					}
					sqlite3_reset(have->Value);
					bind_text(have->Value, 1, name);
					const auto present = sqlite3_step(have->Value);
					if (present == SQLITE_DONE) {
						added += object.Bytes.size();
					} else if (present != SQLITE_ROW) {
						return failed(local_draft_store_failure::unavailable);
					}
				}
				const auto stored = static_cast<std::uint64_t>(*used);
				if (stored + added > application::maximum_local_draft_store_bytes) {
					return failed(local_draft_store_failure::full, stored, application::maximum_local_draft_store_bytes);
				}
				auto insert = detail::prepare(db(), "INSERT OR IGNORE INTO objects(object,size,bytes) VALUES(?1,?2,?3)");
				auto pin    = detail::prepare(db(), "INSERT OR IGNORE INTO pins(draft,object) VALUES(?1,?2)");
				auto record = detail::prepare(db(), "INSERT INTO drafts(id,version,body) VALUES(?1,?2,?3) ON CONFLICT(id) DO UPDATE SET version=excluded.version,body=excluded.body");
				if (!insert || !pin || !record) {
					return failed(local_draft_store_failure::unavailable);
				}
				const auto named = named_objects(aDraft);
				for (const auto& object : aObjects) {
					const auto name = std::string{ core::canonical_hex(object.Object).view() };
					if (!named.contains(name)) {
						continue;
					}
					sqlite3_reset(insert->Value);
					bind_text(insert->Value, 1, name);
					sqlite3_bind_int64(insert->Value, 2, static_cast<sqlite3_int64>(object.Bytes.size()));
					sqlite3_bind_blob64(insert->Value, 3, object.Bytes.empty() ? "" : static_cast<const void*>(object.Bytes.data()), object.Bytes.size(), SQLITE_TRANSIENT);
					if (sqlite3_step(insert->Value) != SQLITE_DONE) {
						return failed(local_draft_store_failure::unavailable);
					}
				}
				for (const auto& name : named) {
					sqlite3_reset(pin->Value);
					bind_text(pin->Value, 1, aDraft.Id.Value);
					bind_text(pin->Value, 2, name);
					if (sqlite3_step(pin->Value) != SQLITE_DONE) {
						return failed(local_draft_store_failure::unavailable);
					}
				}
				bind_text(record->Value, 1, aDraft.Id.Value);
				sqlite3_bind_int64(record->Value, 2, static_cast<sqlite3_int64>(aDraft.Version));
				sqlite3_bind_blob64(record->Value, 3, aBody.data(), aBody.size(), SQLITE_TRANSIENT);
				if (sqlite3_step(record->Value) != SQLITE_DONE) {
					return failed(local_draft_store_failure::unavailable);
				}
				return { };
			}

			detail::state_directory                               Directory;
			std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> Database;
		};
	}

	auto open_local_draft_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::local_draft_store>
	{
		try {
			if (!detail::sqlite_runtime_ready()) {
				return { };
			}
			auto directory = detail::open_state_directory(aRoot);
			if (!directory) {
				return { };
			}
			auto handle = detail::open_private_store_database(*directory, "local-drafts.sqlite3",
															  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_NOFOLLOW);
			if (!handle) {
				return { };
			}
			auto  store = std::make_unique<sqlite_local_draft_store>(std::move(*directory), *handle);
			auto* db    = *handle;
			if (!detail::configure_connection(db)) {
				return { };
			}
			const auto identity = detail::scalar_integer(db, "PRAGMA application_id");
			if (!identity || (*identity != 0 && *identity != application_id)) {
				return { };
			}
			if (!detail::execute(db, "PRAGMA application_id=1112558660")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS drafts(id TEXT PRIMARY KEY, version INTEGER NOT NULL, body BLOB NOT NULL) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS objects(object TEXT PRIMARY KEY, size INTEGER NOT NULL, bytes BLOB NOT NULL) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS pins(draft TEXT NOT NULL, object TEXT NOT NULL, PRIMARY KEY(draft,object)) STRICT, WITHOUT ROWID")) {
				return { };
			}
			return store;
		} catch (...) {
			return { };
		}
	}
}
