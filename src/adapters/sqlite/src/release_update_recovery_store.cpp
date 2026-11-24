#include "bpr/adapters/sqlite/release_update_recovery_store.hpp"
#include "bpr/core/sha256.hpp"
#include "sqlite_connection.hpp"
#include "storage_internal.hpp"
#include <sqlite3.h>

#include <algorithm>
#include <limits>
#include <map>
#include <mutex>

namespace bpr::adapters::sqlite {
	namespace {
		using draft                             = application::release_update_saved_draft;
		using row                               = application::release_update_recovery_row;
		using row_id                            = application::release_update_recovery_id;
		using saved_drafts                      = application::release_update_saved_drafts;
		using fingerprint                       = decltype(row_id::Fingerprint);
		using database_ptr                      = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
		constexpr auto             unavailable  = application::release_update_recovery_failure::unavailable;
		constexpr std::string_view draft_schema = "CREATE TABLE drafts(sequence INTEGER PRIMARY KEY,identity TEXT NOT NULL,description TEXT NOT NULL,body TEXT NOT NULL,indeterminate INTEGER NOT NULL CHECK(indeterminate IN(0,1)),UNIQUE(identity,description,body,indeterminate)) STRICT";
		// Keep the v1 draft schema readable by the previous executable after updater rollback.
		constexpr std::string_view shown_schema = "CREATE TABLE shown(sequence INTEGER PRIMARY KEY,fingerprint BLOB NOT NULL CHECK(length(fingerprint)=32)) STRICT";

		class transaction
		{
		public:
			explicit transaction(sqlite3* aDatabase)
				: Database{ aDatabase }
				, Active{ detail::execute(Database, "BEGIN IMMEDIATE").has_value() }
			{ }
			~transaction()
			{
				if (Active) {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
				}
			}
			transaction(const transaction&)                    = delete;
			auto operator=(const transaction&) -> transaction& = delete;
			auto active() const -> bool { return Active; }
			auto commit() -> bool
			{
				if (!Active || !detail::execute(Database, "COMMIT")) {
					return false;
				}
				Active = false;
				return true;
			}

		private:
			sqlite3* Database;
			bool     Active;
		};

		enum class store_schema { fresh,
								  current,
								  refused };

		auto check_schema(sqlite3* aDatabase, std::int64_t aApplicationId, std::string_view aSchema, std::string_view aSchemaQuery) -> store_schema
		{
			const auto identity = detail::scalar_integer(aDatabase, "PRAGMA application_id");
			const auto version  = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			const auto tables   = detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'");
			if (identity == 0 && version == 0 && tables == 0) {
				return store_schema::fresh;
			}
			const auto sql = detail::scalar_text(aDatabase, aSchemaQuery);
			return identity == aApplicationId && version == 1 && tables == 1 && sql
					&& detail::canonical_sql(*sql) == detail::canonical_sql(aSchema)
				? store_schema::current
				: store_schema::refused;
		}

		auto open_store(detail::state_directory& aDirectory, std::string_view aName, std::int64_t aApplicationId,
						std::string_view aSchema, std::string_view aSchemaQuery) -> database_ptr
		{
			auto         opened = detail::open_private_store_database(aDirectory, aName, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
			database_ptr database{ opened.value_or(nullptr), sqlite3_close };
			const auto   schema = database ? check_schema(database.get(), aApplicationId, aSchema, aSchemaQuery) : store_schema::refused;
			if (schema == store_schema::refused || !detail::configure_connection(database.get())) {
				return { nullptr, sqlite3_close };
			}
			if (schema == store_schema::current) {
				return database;
			}

			transaction creation{ database.get() };
			if (!creation.active()) {
				return { nullptr, sqlite3_close };
			}
			const auto current = check_schema(database.get(), aApplicationId, aSchema, aSchemaQuery);
			const bool created = current == store_schema::fresh
				&& detail::execute(database.get(), aSchema)
				&& detail::execute(database.get(), "PRAGMA application_id=" + std::to_string(aApplicationId))
				&& detail::execute(database.get(), "PRAGMA user_version=1");
			if ((created || current == store_schema::current) && creation.commit()) {
				return database;
			}
			return { nullptr, sqlite3_close };
		}

		auto digest(const draft& aDraft) -> fingerprint
		{
			core::sha256_stream hash;
			for (const auto* field : { &aDraft.Identity, &aDraft.Description, &aDraft.Body }) {
				const auto length = std::to_string(field->size()) + ":";
				hash.update({ reinterpret_cast<const std::uint8_t*>(length.data()), length.size() });
				hash.update({ reinterpret_cast<const std::uint8_t*>(field->data()), field->size() });
			}
			const std::uint8_t indeterminate = aDraft.Indeterminate ? 1U : 0U;
			hash.update({ &indeterminate, 1U });
			return hash.finish();
		}

		auto admitted(const draft& aDraft) -> bool
		{
			return !aDraft.Identity.empty() && aDraft.Identity.size() <= 256U && aDraft.Description.size() <= 4096U && aDraft.Body.size() <= 65536U;
		}

		auto bind_text(sqlite3_stmt* aQuery, int aColumn, const std::string& aText) -> bool
		{
			return sqlite3_bind_text(aQuery, aColumn, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}

		auto read_row(sqlite3_stmt* aQuery) -> std::optional<row>
		{
			row entry{ };
			entry.Id.Sequence = sqlite3_column_int64(aQuery, 0);
			const std::array<std::string*, 3> fields{ &entry.Draft.Identity, &entry.Draft.Description, &entry.Draft.Body };
			constexpr std::array              limits{ 256, 4096, 65536 };
			for (std::size_t index = 0; index < fields.size(); ++index) {
				const auto column = static_cast<int>(index + 1U);
				const auto length = sqlite3_column_bytes(aQuery, column);
				if (sqlite3_column_type(aQuery, column) != SQLITE_TEXT || length < 0 || length > limits[index]) {
					return std::nullopt;
				}
				const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(aQuery, column));
				if (!bytes) {
					return std::nullopt;
				}
				fields[index]->assign(bytes, static_cast<std::size_t>(length));
			}
			const auto flag = sqlite3_column_int(aQuery, 4);
			if (entry.Id.Sequence <= 0 || entry.Draft.Identity.empty()
				|| sqlite3_column_type(aQuery, 4) != SQLITE_INTEGER || (flag != 0 && flag != 1)) {
				return std::nullopt;
			}
			entry.Draft.Indeterminate = flag == 1;
			entry.Id.Fingerprint      = digest(entry.Draft);
			return entry;
		}

		auto read(sqlite3* aDatabase) -> saved_drafts
		{
			auto query = detail::prepare(aDatabase, "SELECT sequence,identity,description,body,indeterminate FROM drafts ORDER BY sequence");
			if (!query) {
				return std::unexpected(unavailable);
			}
			std::vector<row> result;
			std::size_t      total  = 0;
			int              status = SQLITE_ROW;
			while ((status = sqlite3_step(query->Value)) == SQLITE_ROW) {
				auto entry = read_row(query->Value);
				if (!entry || result.size() >= application::release_update_maximum_saved_drafts) {
					return std::unexpected(unavailable);
				}
				total += entry->Draft.Identity.size() + entry->Draft.Description.size() + entry->Draft.Body.size();
				if (total > application::release_update_maximum_saved_bytes) {
					return std::unexpected(unavailable);
				}
				result.push_back(std::move(*entry));
			}
			return status == SQLITE_DONE ? saved_drafts{ std::move(result) } : std::unexpected(unavailable);
		}

		using shown_rows = std::map<std::int64_t, fingerprint>;

		auto read_shown(sqlite3* aDatabase) -> std::optional<shown_rows>
		{
			auto query = detail::prepare(aDatabase, "SELECT sequence,fingerprint FROM shown ORDER BY sequence");
			if (!query) {
				return std::nullopt;
			}
			shown_rows shown;
			int        status = SQLITE_ROW;
			while ((status = sqlite3_step(query->Value)) == SQLITE_ROW) {
				const auto  sequence = sqlite3_column_int64(query->Value, 0);
				fingerprint hash{ };
				if (sequence <= 0 || sqlite3_column_type(query->Value, 1) != SQLITE_BLOB
					|| sqlite3_column_bytes(query->Value, 1) != static_cast<int>(hash.size())
					|| shown.size() >= application::release_update_maximum_saved_drafts + 1U) {
					return std::nullopt;
				}
				const auto* bytes = static_cast<const std::uint8_t*>(sqlite3_column_blob(query->Value, 1));
				if (!bytes) {
					return std::nullopt;
				}
				std::copy_n(bytes, hash.size(), hash.begin());
				shown.emplace(sequence, hash);
			}
			return status == SQLITE_DONE ? std::optional{ std::move(shown) } : std::nullopt;
		}

		auto is_shown(const shown_rows& aShown, const row_id& aId) -> bool
		{
			const auto found = aShown.find(aId.Sequence);
			return found != aShown.end() && found->second == aId.Fingerprint;
		}

		auto write_shown(sqlite3* aDatabase, const shown_rows& aShown) -> bool
		{
			transaction write{ aDatabase };
			if (!write.active() || !detail::execute(aDatabase, "DELETE FROM shown")) {
				return false;
			}
			for (const auto& [sequence, hash] : aShown) {
				auto query = detail::prepare(aDatabase, "INSERT INTO shown(sequence,fingerprint) VALUES(?1,?2)");
				if (!query || sqlite3_bind_int64(query->Value, 1, sequence) != SQLITE_OK
					|| sqlite3_bind_blob(query->Value, 2, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT) != SQLITE_OK
					|| sqlite3_step(query->Value) != SQLITE_DONE) {
					return false;
				}
			}
			return write.commit();
		}

		auto store_drafts(sqlite3* aDatabase, std::int64_t aFrontier, const std::vector<draft>& aDrafts) -> bool
		{
			for (const auto& entry : aDrafts) {
				if (aFrontier == std::numeric_limits<std::int64_t>::max()) {
					return false;
				}
				auto query = detail::prepare(aDatabase, "INSERT INTO drafts(sequence,identity,description,body,indeterminate) VALUES(?1,?2,?3,?4,?5) ON CONFLICT(identity,description,body,indeterminate) DO NOTHING");
				if (!query || sqlite3_bind_int64(query->Value, 1, ++aFrontier) != SQLITE_OK
					|| !bind_text(query->Value, 2, entry.Identity) || !bind_text(query->Value, 3, entry.Description) || !bind_text(query->Value, 4, entry.Body)
					|| sqlite3_bind_int(query->Value, 5, entry.Indeterminate ? 1 : 0) != SQLITE_OK || sqlite3_step(query->Value) != SQLITE_DONE) {
					return false;
				}
			}
			return true;
		}

		class recovery_store final : public application::release_update_recovery_port
		{
		public:
			explicit recovery_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			auto load() -> saved_drafts override
			{
				const std::scoped_lock lock{ Mutex };
				if (!open()) {
					return std::unexpected(unavailable);
				}
				auto rows = read(Database.get());
				if (rows && !rows->empty() && open_shown()) {
					if (const auto shown = read_shown(Shown.get())) {
						for (auto& entry : *rows) {
							entry.Shown = is_shown(*shown, entry.Id);
						}
					}
				}
				return rows;
			}

			auto save(const std::vector<draft>& aDrafts) -> bool override
			{
				const std::scoped_lock lock{ Mutex };
				if (!open() || aDrafts.size() > application::release_update_maximum_saved_drafts || !std::ranges::all_of(aDrafts, admitted)) {
					return false;
				}
				transaction write{ Database.get() };
				const auto  rows = write.active() ? read(Database.get()) : saved_drafts{ std::unexpected(unavailable) };
				if (!rows) {
					return false;
				}
				const auto frontier = rows->empty() ? 0 : rows->back().Id.Sequence;
				const auto shown    = open_shown() ? read_shown(Shown.get()) : std::nullopt;
				const auto retired  = shown && !shown->empty() ? shown->rbegin()->first : 0;
				return store_drafts(Database.get(), std::max(frontier, retired), aDrafts) && read(Database.get()) && write.commit();
			}

			auto discard(std::span<const row_id> aIdentities) -> bool override
			{
				const std::scoped_lock lock{ Mutex };
				if (!open() || aIdentities.size() > application::release_update_maximum_saved_drafts) {
					return false;
				}
				transaction write{ Database.get() };
				const auto  rows = write.active() ? read(Database.get()) : saved_drafts{ std::unexpected(unavailable) };
				if (!rows) {
					return false;
				}
				for (const auto& identity : aIdentities) {
					if (std::ranges::find(*rows, identity, &row::Id) == rows->end()) {
						return false;
					}
					auto query = detail::prepare(Database.get(), "DELETE FROM drafts WHERE sequence=?1");
					if (!query || sqlite3_bind_int64(query->Value, 1, identity.Sequence) != SQLITE_OK
						|| sqlite3_step(query->Value) != SQLITE_DONE || sqlite3_changes(Database.get()) != 1) {
						return false;
					}
				}
				return write.commit();
			}

			auto mark_shown(std::span<const row_id> aIdentities) -> bool override
			{
				const std::scoped_lock lock{ Mutex };
				if (!open() || aIdentities.size() > application::release_update_maximum_saved_drafts) {
					return false;
				}
				transaction snapshot{ Database.get() };
				const auto  rows  = snapshot.active() ? read(Database.get()) : saved_drafts{ std::unexpected(unavailable) };
				const auto  shown = rows && open_shown() ? read_shown(Shown.get()) : std::nullopt;
				if (!shown || std::ranges::any_of(aIdentities, [&](const auto& aId) { return std::ranges::find(*rows, aId, &row::Id) == rows->end(); })) {
					return false;
				}
				shown_rows next;
				// Keep the high-water mark so a new save cannot inherit a removed row's identity.
				if (!shown->empty()) {
					next.insert(*shown->rbegin());
				}
				for (const auto& entry : *rows) {
					if (is_shown(*shown, entry.Id) || std::ranges::find(aIdentities, entry.Id) != aIdentities.end()) {
						next.insert_or_assign(entry.Id.Sequence, entry.Id.Fingerprint);
					}
				}
				return write_shown(Shown.get(), next) && snapshot.commit();
			}

		private:
			auto open() -> bool
			{
				if (!Database && detail::sqlite_runtime_ready()) {
					if (auto directory = detail::open_state_directory(Root)) {
						Directory = std::make_unique<detail::state_directory>(std::move(*directory));
						Database  = open_store(*Directory, "update-draft-recovery.sqlite3", 1112560214, draft_schema,
											   "SELECT sql FROM sqlite_schema WHERE type='table' AND name='drafts'");
					}
				}
				return Database != nullptr;
			}

			auto open_shown() -> bool
			{
				if (!Shown) {
					Shown = open_store(*Directory, "update-draft-recovery-shown.sqlite3", 1112560215, shown_schema,
									   "SELECT sql FROM sqlite_schema WHERE type='table' AND name='shown'");
				}
				return Shown != nullptr;
			}
			std::filesystem::path                    Root;
			std::unique_ptr<detail::state_directory> Directory;
			database_ptr                             Database{ nullptr, sqlite3_close };
			database_ptr                             Shown{ nullptr, sqlite3_close };
			std::mutex                               Mutex;
		};
	}

	auto make_release_update_recovery_store(std::filesystem::path aRoot) -> std::shared_ptr<application::release_update_recovery_port>
	{
		return std::make_shared<recovery_store>(std::move(aRoot));
	}
}
