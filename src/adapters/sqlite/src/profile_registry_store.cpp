#include "bpr/adapters/sqlite/profile_registry_store.hpp"

#include "sqlite_connection.hpp"
#include "state_directory.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		using application::profile_registry_failure;
		using application::profile_registry_result;
		using application::profile_registry_snapshot;
		constexpr std::int64_t     ApplicationId  = 0x42505250;
		constexpr std::string_view MetaSchema     = "CREATE TABLE profile_registry_meta(singleton INTEGER PRIMARY KEY CHECK(singleton=1),schema_format INTEGER NOT NULL CHECK(schema_format=1),revision TEXT NOT NULL,last_profile_id TEXT NOT NULL,default_adopted INTEGER NOT NULL CHECK(default_adopted=1)) STRICT";
		constexpr std::string_view ProfilesSchema = "CREATE TABLE profile_registry_profiles(profile_id TEXT PRIMARY KEY,label TEXT NOT NULL,root_mode INTEGER NOT NULL CHECK(root_mode IN(0,1)),root_identity TEXT NOT NULL UNIQUE) STRICT";

		// These connection-local limits perform no SQL and cannot change a refused file.
		void restrict_registry_reads(sqlite3* aDatabase)
		{
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, 4'096));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 8'192));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 16));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_VDBE_OP, 10'000));
		}

		class registry_read_scope final
		{
		public:
			registry_read_scope(sqlite3* aDatabase, std::stop_token aStop)
				: Database{ aDatabase }
				, Stop{ aStop }
			{
				sqlite3_progress_handler(Database, 100, progress, this);
			}
			~registry_read_scope() { sqlite3_progress_handler(Database, 0, nullptr, nullptr); }
			registry_read_scope(const registry_read_scope&)                    = delete;
			auto operator=(const registry_read_scope&) -> registry_read_scope& = delete;

		private:
			static auto progress(void* aContext) -> int
			{
				auto& scope = *static_cast<registry_read_scope*>(aContext);
				return scope.Stop.stop_requested() || ++scope.Steps >= 200U ? 1 : 0;
			}
			sqlite3*        Database;
			std::stop_token Stop;
			std::size_t     Steps = 0U;
		};

		[[nodiscard]] auto translate(const detail::failure aFailure) -> profile_registry_failure
		{
			return aFailure == detail::failure::cancelled ? profile_registry_failure::cancelled : profile_registry_failure::unavailable;
		}
		[[nodiscard]] auto text_of(sqlite3_stmt* aStatement, const int aColumn) -> std::string_view
		{
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_TEXT) {
				return { };
			}
			const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(aStatement, aColumn));
			return text == nullptr ? std::string_view{ } : std::string_view{ text, static_cast<std::size_t>(sqlite3_column_bytes(aStatement, aColumn)) };
		}
		[[nodiscard]] auto id_of(sqlite3_stmt* aStatement, const int aColumn) -> std::uint64_t
		{
			const auto text = text_of(aStatement, aColumn);
			if (text.empty() || text.size() > 20U || text.front() == '0') {
				return 0U;
			}
			std::uint64_t result = 0U;
			const auto    parsed = std::from_chars(text.data(), text.data() + text.size(), result);
			return parsed.ec == std::errc{ } && parsed.ptr == text.data() + text.size() ? result : 0U;
		}
		[[nodiscard]] auto bind_text(sqlite3_stmt* aStatement, const int aColumn, const std::string_view aText) -> bool
		{
			return sqlite3_bind_text(aStatement, aColumn, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		[[nodiscard]] auto run(sqlite3* aDatabase, const std::string_view aSql, const std::stop_token aStop = { }) -> profile_registry_result<void>
		{
			const auto result = detail::execute(aDatabase, aSql, aStop);
			return result ? profile_registry_result<void>{ } : std::unexpected(translate(result.error()));
		}

		class registry_transaction final
		{
		public:
			explicit registry_transaction(sqlite3* aDatabase)
				: Database{ aDatabase }
			{ }
			~registry_transaction() { static_cast<void>(sqlite3_exec(Database, "ROLLBACK", nullptr, nullptr, nullptr)); }
			registry_transaction(const registry_transaction&)                    = delete;
			auto operator=(const registry_transaction&) -> registry_transaction& = delete;

		private:
			sqlite3* Database;
		};

		[[nodiscard]] auto validate_schema(sqlite3* aDatabase) -> profile_registry_result<void>
		{
			const auto version  = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			const auto identity = detail::scalar_integer(aDatabase, "PRAGMA application_id");
			const auto count    = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 3)");
			const auto meta     = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='profile_registry_meta'");
			const auto profiles = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='profile_registry_profiles'");
			if (!version || *version != 1 || !identity || *identity != ApplicationId || !count || *count != 2
				|| !meta || detail::canonical_sql(*meta) != detail::canonical_sql(MetaSchema)
				|| !profiles || detail::canonical_sql(*profiles) != detail::canonical_sql(ProfilesSchema)) {
				return std::unexpected(profile_registry_failure::schema_mismatch);
			}
			return { };
		}

		[[nodiscard]] auto read_profiles(sqlite3* aDatabase, const std::stop_token aStop,
										 profile_registry_snapshot& aSnapshot) -> profile_registry_result<void>
		{
			auto rows = detail::prepare(aDatabase, "SELECT profile_id,label,root_mode,root_identity FROM profile_registry_profiles LIMIT 65");
			if (!rows) {
				return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::corrupt);
			}
			aSnapshot.Profiles.clear();
			for (;;) {
				if (aStop.stop_requested()) {
					return std::unexpected(profile_registry_failure::cancelled);
				}
				const auto step = sqlite3_step(rows->Value);
				if (step == SQLITE_DONE) {
					std::ranges::sort(aSnapshot.Profiles, { }, [](const auto& aEntry) { return aEntry.Profile.Id; });
					return { };
				}
				if (step != SQLITE_ROW || aSnapshot.Profiles.size() >= core::maximum_bpr_profiles) {
					return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::corrupt);
				}
				const auto label = text_of(rows->Value, 1);
				const auto root  = text_of(rows->Value, 3);
				const auto mode  = sqlite3_column_int64(rows->Value, 2);
				if (label.size() > core::maximum_bpr_profile_label_bytes || root.size() > 28U
					|| sqlite3_column_type(rows->Value, 2) != SQLITE_INTEGER || mode < 0 || mode > 1) {
					return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::corrupt);
				}
				aSnapshot.Profiles.push_back({ { core::bpr_profile_id{ id_of(rows->Value, 0) }, std::string{ label } },
											   static_cast<application::profile_root_mode>(mode),
											   std::string{ root } });
			}
		}

		[[nodiscard]] auto read_snapshot(sqlite3* aDatabase, const std::stop_token aStop) -> profile_registry_result<profile_registry_snapshot>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(profile_registry_failure::cancelled);
			}
			const auto readScope = registry_read_scope{ aDatabase, aStop };
			if (auto schema = validate_schema(aDatabase); !schema) {
				return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : schema.error());
			}
			auto meta = detail::prepare(aDatabase, "SELECT singleton,schema_format,revision,last_profile_id,default_adopted FROM profile_registry_meta LIMIT 2");
			if (!meta || sqlite3_step(meta->Value) != SQLITE_ROW || sqlite3_column_int64(meta->Value, 0) != 1
				|| sqlite3_column_int64(meta->Value, 1) != 1 || sqlite3_column_int64(meta->Value, 4) != 1) {
				return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::corrupt);
			}
			auto result          = profile_registry_snapshot{ };
			result.Revision      = id_of(meta->Value, 2);
			result.LastProfileId = id_of(meta->Value, 3);
			if (sqlite3_step(meta->Value) != SQLITE_DONE) {
				return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::corrupt);
			}
			if (auto rows = read_profiles(aDatabase, aStop, result); !rows) {
				return std::unexpected(rows.error());
			}
			if (!application::valid(result)) {
				return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::corrupt);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(profile_registry_failure::cancelled);
			}
			return result;
		}

		[[nodiscard]] auto insert_profiles(sqlite3* aDatabase, const profile_registry_snapshot& aSnapshot,
										   const std::stop_token aStop) -> profile_registry_result<void>
		{
			for (const auto& entry : aSnapshot.Profiles) {
				if (aStop.stop_requested()) {
					return std::unexpected(profile_registry_failure::cancelled);
				}
				auto row = detail::prepare(aDatabase, "INSERT INTO profile_registry_profiles(profile_id,label,root_mode,root_identity) VALUES(?1,?2,?3,?4)");
				if (!row || !bind_text(row->Value, 1, std::to_string(entry.Profile.Id.value()))
					|| !bind_text(row->Value, 2, entry.Profile.Label)
					|| sqlite3_bind_int(row->Value, 3, static_cast<int>(entry.RootMode)) != SQLITE_OK
					|| !bind_text(row->Value, 4, entry.RootIdentity) || sqlite3_step(row->Value) != SQLITE_DONE) {
					return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::unavailable);
				}
			}
			return { };
		}

		[[nodiscard]] auto pristine(sqlite3* aDatabase, const std::stop_token aStop) -> bool
		{
			const auto readScope = registry_read_scope{ aDatabase, aStop };
			const auto version   = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			const auto identity  = detail::scalar_integer(aDatabase, "PRAGMA application_id");
			const auto count     = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema LIMIT 1)");
			return version && *version == 0 && identity && *identity == 0 && count && *count == 0;
		}

		class sqlite_profile_registry_store final : public application::profile_registry_port
		{
		public:
			explicit sqlite_profile_registry_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			~sqlite_profile_registry_store() override
			{
				if (Database != nullptr) {
					sqlite3_close_v2(Database);
				}
			}

			auto load(const std::stop_token aStop) -> profile_registry_result<profile_registry_snapshot> override
			{
				if (auto opened = ensure_open(aStop); !opened) {
					return std::unexpected(opened.error());
				}
				if (auto begin = run(Database, "BEGIN", aStop); !begin) {
					return std::unexpected(begin.error());
				}
				const auto transaction = registry_transaction{ Database };
				const auto progress    = detail::progress_scope{ Database, aStop };
				return read_snapshot(Database, aStop);
			}

			auto save(const profile_registry_snapshot& aCandidate, const std::uint64_t aExpectedRevision,
					  const std::stop_token aStop) -> profile_registry_result<profile_registry_snapshot> override
			{
				if (!application::valid(aCandidate)) {
					return std::unexpected(profile_registry_failure::corrupt);
				}
				if (auto opened = ensure_open(aStop); !opened) {
					return std::unexpected(opened.error());
				}
				restrict_registry_reads(Database);
				if (auto begin = run(Database, "BEGIN IMMEDIATE", aStop); !begin) {
					return std::unexpected(begin.error());
				}
				const auto transaction = registry_transaction{ Database };
				const auto progress    = detail::progress_scope{ Database, aStop };
				const auto current     = read_snapshot(Database, aStop);
				if (!current) {
					return std::unexpected(current.error());
				}
				if (current->Revision != aExpectedRevision) {
					return std::unexpected(profile_registry_failure::conflict);
				}
				if (!application::valid_profile_registry_update(*current, aCandidate)) {
					return std::unexpected(profile_registry_failure::corrupt);
				}
				if (auto erased = run(Database, "DELETE FROM profile_registry_profiles", aStop); !erased) {
					return std::unexpected(erased.error());
				}
				if (auto rows = insert_profiles(Database, aCandidate, aStop); !rows) {
					return std::unexpected(rows.error());
				}
				auto meta = detail::prepare(Database, "UPDATE profile_registry_meta SET revision=?1,last_profile_id=?2 WHERE singleton=1");
				if (!meta || !bind_text(meta->Value, 1, std::to_string(aCandidate.Revision))
					|| !bind_text(meta->Value, 2, std::to_string(aCandidate.LastProfileId)) || sqlite3_step(meta->Value) != SQLITE_DONE) {
					return std::unexpected(aStop.stop_requested() ? profile_registry_failure::cancelled : profile_registry_failure::unavailable);
				}
				if (auto commit = run(Database, "COMMIT", aStop); !commit) {
					return std::unexpected(commit.error());
				}
				return aCandidate;
			}

		private:
			[[nodiscard]] auto establish(const std::stop_token aStop) -> profile_registry_result<void>
			{
				// Refuse stamped or populated incompatible files before connection setup can write.
				if (!pristine(Database, aStop)) {
					const auto existing = read_snapshot(Database, aStop);
					if (!existing) {
						return std::unexpected(existing.error());
					}
				}
				if (auto configured = detail::configure_connection(Database); !configured) {
					return std::unexpected(translate(configured.error()));
				}
				restrict_registry_reads(Database);
				if (auto begin = run(Database, "BEGIN IMMEDIATE", aStop); !begin) {
					return begin;
				}
				const auto transaction = registry_transaction{ Database };
				// Recheck under the write transaction: another opener may have adopted Default.
				if (!pristine(Database, aStop)) {
					const auto existing = read_snapshot(Database, aStop);
					return existing ? profile_registry_result<void>{ } : std::unexpected(existing.error());
				}
				for (const auto schema : { MetaSchema, ProfilesSchema }) {
					if (auto created = run(Database, schema, aStop); !created) {
						return created;
					}
				}
				if (auto rows = insert_profiles(Database, profile_registry_snapshot{ }, aStop); !rows) {
					return rows;
				}
				for (const auto sql : { "INSERT INTO profile_registry_meta VALUES(1,1,'1','1',1)",
										"PRAGMA application_id=1112560208", "PRAGMA user_version=1", "COMMIT" }) {
					if (auto stored = run(Database, sql, aStop); !stored) {
						return stored;
					}
				}
				return { };
			}

			[[nodiscard]] auto ensure_open(const std::stop_token aStop) -> profile_registry_result<void>
			{
				if (aStop.stop_requested()) {
					return std::unexpected(profile_registry_failure::cancelled);
				}
				if (Database != nullptr) {
					return { };
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return std::unexpected(profile_registry_failure::unavailable);
				}
				Directory.emplace(std::move(*directory));
				const auto      path = Directory->path() / "profile-registry.sqlite3";
				std::error_code error;
				const auto      status = std::filesystem::symlink_status(path, error);
				if (std::filesystem::exists(status) && (!std::filesystem::is_regular_file(status) || (status.permissions() & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) != std::filesystem::perms::none)) {
					return std::unexpected(profile_registry_failure::unavailable);
				}
				if (sqlite3_open_v2(path.string().c_str(), &Database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW, nullptr) != SQLITE_OK) {
					if (Database != nullptr) {
						sqlite3_close_v2(Database);
						Database = nullptr;
					}
					return std::unexpected(profile_registry_failure::unavailable);
				}
				restrict_registry_reads(Database);
				std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, error);
				auto result = error ? profile_registry_result<void>{ std::unexpected(profile_registry_failure::unavailable) } : establish(aStop);
				if (!result) {
					sqlite3_close_v2(Database);
					Database = nullptr;
				}
				return result;
			}
			std::filesystem::path                  Root;
			std::optional<detail::state_directory> Directory;
			sqlite3*                               Database = nullptr;
		};
	}

	auto make_profile_registry_store(std::filesystem::path aStorageRoot) -> std::unique_ptr<application::profile_registry_port>
	{
		return std::make_unique<sqlite_profile_registry_store>(std::move(aStorageRoot));
	}
}
