#include "bpr/adapters/sqlite/device_binding_store.hpp"

#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		using application::device_binding_failure;
		using application::device_binding_result;
		constexpr std::string_view LegacySchema  = "CREATE TABLE device_binding(profile TEXT PRIMARY KEY,revision TEXT NOT NULL,host TEXT NOT NULL,hostname TEXT NOT NULL,account TEXT NOT NULL,server INTEGER NOT NULL CHECK(server BETWEEN 0 AND 1),provenance INTEGER NOT NULL CHECK(provenance=1),issued INTEGER NOT NULL CHECK(issued>0),access_expiry INTEGER NOT NULL CHECK(access_expiry>issued),refresh_expiry INTEGER NOT NULL CHECK(refresh_expiry>issued),tombstone INTEGER NOT NULL CHECK(tombstone IN(0,1))) STRICT";
		constexpr std::string_view Schema        = "CREATE TABLE device_binding(profile TEXT PRIMARY KEY,revision TEXT NOT NULL,host TEXT NOT NULL,hostname TEXT NOT NULL,account TEXT NOT NULL,server INTEGER NOT NULL CHECK(server BETWEEN 0 AND 1),provenance INTEGER NOT NULL CHECK(provenance=1),issued INTEGER NOT NULL CHECK(issued>0),access_expiry INTEGER NOT NULL CHECK(access_expiry>issued),refresh_expiry INTEGER NOT NULL CHECK(refresh_expiry>issued),tombstone INTEGER NOT NULL CHECK(tombstone IN(0,1)),bpr_profile TEXT NOT NULL) STRICT";
		constexpr std::string_view ScopeSchema   = "CREATE TABLE binding_scope(profile TEXT PRIMARY KEY) STRICT";
		constexpr std::int64_t     ApplicationId = 0x42505242;
		constexpr auto             corrupt() { return std::unexpected(device_binding_failure::corrupt); }
		auto                       text_of(sqlite3_stmt* aStatement, int aColumn) -> std::string_view
		{
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_TEXT) {
				return { };
			}
			const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(aStatement, aColumn));
			return bytes == nullptr ? std::string_view{ } : std::string_view{ bytes, static_cast<std::size_t>(sqlite3_column_bytes(aStatement, aColumn)) };
		}
		auto id_of(sqlite3_stmt* aStatement, int aColumn) -> std::uint64_t
		{
			const auto    text  = text_of(aStatement, aColumn);
			std::uint64_t value = 0;
			if (text.empty() || text.size() > 20 || text.front() == '0') {
				return 0;
			}
			const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
			return parsed.ec == std::errc{ } && parsed.ptr == text.data() + text.size() ? value : 0;
		}
		auto bind_text(sqlite3_stmt* aStatement, int aColumn, std::string_view aValue) -> bool
		{
			return sqlite3_bind_text(aStatement, aColumn, aValue.data(), static_cast<int>(aValue.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		auto bind_authority(sqlite3_stmt* aStatement, const application::github_credential_authority_key& aAuthority) -> bool
		{
			return bind_text(aStatement, 1, std::to_string(aAuthority.Profile.value()))
				&& bind_text(aStatement, 2, std::to_string(aAuthority.ProfileRevision.value()))
				&& bind_text(aStatement, 3, std::to_string(aAuthority.Host.value()))
				&& bind_text(aStatement, 4, aAuthority.Hostname.value())
				&& bind_text(aStatement, 5, std::to_string(aAuthority.Account.value()))
				&& sqlite3_bind_int(aStatement, 6, static_cast<int>(aAuthority.ServerContract)) == SQLITE_OK;
		}

		// Connection-local limits and progress checks must precede all untrusted reads.
		// Unlike configure_connection, these cannot mutate a refused database.
		// The step ceiling holds for the whole scope: a nested execute's own
		// progress scope reinstates it when it ends ([[BPR-0887]]).
		// configure_connection does replace the limits, with the connection policy.
		class binding_read_scope final
		{
		public:
			binding_read_scope(sqlite3* aDatabase, std::stop_token aStop)
				: Stop{ aStop }
				, Handler{ aDatabase, 100, progress, this }
			{
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, 1'048'576));
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 8'192));
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 16));
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_VDBE_OP, 10'000));
			}
			binding_read_scope(const binding_read_scope&)                    = delete;
			auto operator=(const binding_read_scope&) -> binding_read_scope& = delete;

		private:
			static auto progress(void* aContext) -> int
			{
				auto& scope = *static_cast<binding_read_scope*>(aContext);
				return scope.Stop.stop_requested() || ++scope.Steps >= 200U ? 1 : 0;
			}
			std::stop_token                Stop;
			std::size_t                    Steps = 0U;
			detail::progress_handler_scope Handler;
		};

		class sqlite_device_binding_store final : public application::device_binding_store_port
		{
		public:
			explicit sqlite_device_binding_store(std::filesystem::path aRoot, core::bpr_profile_id aBprProfile)
				: Root{ std::move(aRoot) }
				, BprProfile{ aBprProfile }
			{ }
			~sqlite_device_binding_store() override
			{
				if (Database != nullptr) {
					sqlite3_close_v2(Database);
				}
			}
			auto load(std::stop_token aStop) -> application::device_binding_load_result override
			{
				if (auto ready = ensure_open(aStop); !ready) {
					return std::unexpected(ready.error());
				}
				return read_rows(aStop);
			}
			auto store(const application::device_binding_record& aRecord, std::stop_token aStop) -> device_binding_result override
			{
				if (!application::valid(aRecord) || aRecord.Authority.BprProfile != BprProfile || aRecord.Tombstone) {
					return corrupt();
				}
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				if (!detail::execute(Database, "BEGIN IMMEDIATE", aStop)) {
					return std::unexpected(device_binding_failure::unavailable);
				}
				auto result = write_record(aRecord, aStop);
				if (result && !detail::execute(Database, "COMMIT", aStop)) {
					result = std::unexpected(device_binding_failure::unavailable);
				}
				if (!result) {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
				}
				return result;
			}
			auto erase(const application::github_credential_authority_key& aAuthority, std::stop_token aStop) -> device_binding_result override
			{
				if (!application::valid(aAuthority) || aAuthority.BprProfile != BprProfile) {
					return corrupt();
				}
				if (auto ready = ensure_open(aStop); !ready) {
					return ready;
				}
				auto statement = detail::prepare(Database, "UPDATE device_binding SET tombstone=1 WHERE profile=?1 AND revision=?2 AND host=?3 AND hostname=?4 AND account=?5 AND server=?6 AND bpr_profile=?7");
				if (!statement || !bind_authority(statement->Value, aAuthority) || !bind_text(statement->Value, 7, std::to_string(BprProfile.value())) || !read_rows(aStop) || sqlite3_step(statement->Value) != SQLITE_DONE) {
					return std::unexpected(device_binding_failure::unavailable);
				}
				return { };
			}

		private:
			auto read_rows(std::stop_token aStop, bool aLegacy = false) -> application::device_binding_load_result
			{
				auto statement = detail::prepare(Database, aLegacy ? "SELECT profile,revision,host,hostname,account,server,provenance,issued,access_expiry,refresh_expiry,tombstone FROM device_binding LIMIT 65" : "SELECT profile,revision,host,hostname,account,server,provenance,issued,access_expiry,refresh_expiry,tombstone,bpr_profile FROM device_binding LIMIT 65");
				if (!statement) {
					return corrupt();
				}
				std::vector<application::device_binding_record> rows;
				for (;;) {
					if (aStop.stop_requested()) {
						return std::unexpected(device_binding_failure::cancelled);
					}
					const auto step = sqlite3_step(statement->Value);
					if (step == SQLITE_DONE) {
						return rows;
					}
					if (step != SQLITE_ROW || rows.size() >= application::maximum_device_bindings) {
						return corrupt();
					}
					const auto server    = sqlite3_column_int64(statement->Value, 5);
					const auto tombstone = sqlite3_column_int64(statement->Value, 10);
					if (server < 0 || server > 1 || tombstone < 0 || tombstone > 1 || sqlite3_column_int64(statement->Value, 6) != 1) {
						return corrupt();
					}
					application::device_binding_record record{
						.Authority = { .Profile = core::host_profile_id{ id_of(statement->Value, 0) }, .ProfileRevision = application::github_profile_revision{ id_of(statement->Value, 1) }, .Host = core::git_host_id{ id_of(statement->Value, 2) }, .Hostname = application::github_canonical_hostname{ text_of(statement->Value, 3) }, .Account = core::github_account_id{ id_of(statement->Value, 4) }, .ServerContract = static_cast<application::github_server_contract>(server), .BprProfile = aLegacy ? core::default_bpr_profile_id : core::bpr_profile_id{ id_of(statement->Value, 11) } },
						.Lifetime  = { .IssuedAt = sqlite3_column_int64(statement->Value, 7), .AccessExpiresAt = sqlite3_column_int64(statement->Value, 8), .RefreshExpiresAt = sqlite3_column_int64(statement->Value, 9) },
						.Tombstone = tombstone == 1
					};
					if (!application::valid(record) || record.Authority.BprProfile != BprProfile || std::ranges::any_of(rows, [&record](const auto& aRow) { return aRow.Authority.Profile == record.Authority.Profile; })) {
						return corrupt();
					}
					rows.push_back(record);
				}
			}
			auto write_record(const application::device_binding_record& aRecord, std::stop_token aStop) -> device_binding_result
			{
				auto rows = read_rows(aStop);
				if (!rows) {
					return std::unexpected(rows.error());
				}
				const auto existing = std::ranges::find_if(*rows, [&aRecord](const auto& aRow) { return aRow.Authority.Profile == aRecord.Authority.Profile; });
				if (existing == rows->end() && rows->size() >= application::maximum_device_bindings) {
					return std::unexpected(device_binding_failure::capacity);
				}
				if (existing != rows->end() && !existing->Tombstone && existing->Authority != aRecord.Authority) {
					return corrupt();
				}
				auto statement = detail::prepare(Database, "INSERT INTO device_binding VALUES(?1,?2,?3,?4,?5,?6,1,?7,?8,?9,0,?10) ON CONFLICT(profile) DO UPDATE SET revision=excluded.revision,host=excluded.host,hostname=excluded.hostname,account=excluded.account,server=excluded.server,issued=excluded.issued,access_expiry=excluded.access_expiry,refresh_expiry=excluded.refresh_expiry,tombstone=0");
				if (!statement || !bind_authority(statement->Value, aRecord.Authority)
					|| sqlite3_bind_int64(statement->Value, 7, aRecord.Lifetime.IssuedAt) != SQLITE_OK
					|| sqlite3_bind_int64(statement->Value, 8, aRecord.Lifetime.AccessExpiresAt) != SQLITE_OK
					|| sqlite3_bind_int64(statement->Value, 9, aRecord.Lifetime.RefreshExpiresAt) != SQLITE_OK
					|| !bind_text(statement->Value, 10, std::to_string(BprProfile.value()))
					|| sqlite3_step(statement->Value) != SQLITE_DONE) {
					return std::unexpected(device_binding_failure::unavailable);
				}
				return { };
			}
			auto ensure_open(std::stop_token aStop) -> device_binding_result
			{
				if (aStop.stop_requested()) {
					return std::unexpected(device_binding_failure::cancelled);
				}
				if (Failed || BprProfile.value() == 0U) {
					return corrupt();
				}
				if (Database != nullptr) {
					return { };
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return std::unexpected(device_binding_failure::unavailable);
				}
				Directory.emplace(std::move(*directory));
				// [[BPR-0841]]. Refusing to admit the file is not corruption: the
				// next call retries, as it did before the shared open helper. Once
				// the file was admitted, a refused open latches the store, and so
				// does a verdict on its content ([[BPR-0884]]).
				if (!detail::admit_private_store_file(*Directory, "device-bindings.sqlite3")) {
					return std::unexpected(device_binding_failure::unavailable);
				}
				auto opened = detail::open_private_store_database(*Directory, "device-bindings.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					Failed = true;
					return std::unexpected(device_binding_failure::unavailable);
				}
				Database         = *opened;
				auto established = [this, &aStop] {
					const auto readScope = binding_read_scope{ Database, aStop };
					return establish(aStop);
				}();
				if (established) {
					return { };
				}
				// [[BPR-0884]]. Cancellation and contention are not corruption. The
				// connection closes either way, and only a `corrupt` verdict latches.
				// Anything else leaves the next call to establish again from the start,
				// rereading and rechecking the whole file, so a retry can never skip a
				// check that the failed attempt would have made.
				const auto cancelled = aStop.stop_requested();
				Failed               = !cancelled && established.error() == device_binding_failure::corrupt;
				sqlite3_close_v2(Database);
				Database = nullptr;
				if (cancelled) {
					return std::unexpected(device_binding_failure::cancelled);
				}
				return established;
			}
			enum class catalog_kind { fresh,
									  legacy,
									  current };
			// `corrupt` means the admitted file was read and judged. The judgement
			// reads inside one transaction, so after its first read holds a snapshot
			// no read can meet another connection's lock or see another writer's
			// commit. A failed first read, a failed configuration and a failed
			// migration are contention or environment and report `unavailable`.
			auto establish(std::stop_token aStop) -> device_binding_result
			{
				if (!detail::execute(Database, "BEGIN", aStop)) {
					return std::unexpected(device_binding_failure::unavailable);
				}
				const auto kind = inspect(aStop);
				static_cast<void>(detail::execute(Database, "ROLLBACK"));
				if (!kind) {
					return std::unexpected(kind.error());
				}
				if (!detail::configure_connection(Database)) {
					return std::unexpected(device_binding_failure::unavailable);
				}
				// The migration's inputs were judged above, and a failed one rolls back.
				if (*kind != catalog_kind::current && !migrate(*kind == catalog_kind::legacy, aStop)) {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
					return std::unexpected(device_binding_failure::unavailable);
				}
				auto rows = read_rows(aStop);
				return rows ? device_binding_result{ } : device_binding_result{ std::unexpected(rows.error()) };
			}
			auto inspect(std::stop_token aStop) -> std::expected<catalog_kind, device_binding_failure>
			{
				auto version = detail::scalar_integer(Database, "PRAGMA user_version");
				if (!version) {
					// SQLite's own finding that this is not a database is a verdict.
					return std::unexpected(version.error() == detail::failure::integrity_failure ? device_binding_failure::corrupt : device_binding_failure::unavailable);
				}
				auto identity = detail::scalar_integer(Database, "PRAGMA application_id");
				auto count    = detail::scalar_integer(Database, "SELECT count(*) FROM sqlite_schema");
				if (!identity || !count) {
					return corrupt();
				}
				const auto fresh  = *version == 0 && *identity == 0 && *count == 0;
				const auto legacy = *version == 1 && *identity == ApplicationId;
				if (!fresh && ((*version != 2 && !legacy) || *identity != ApplicationId)) {
					return corrupt();
				}
				if (legacy && BprProfile != core::default_bpr_profile_id) {
					return corrupt();
				}
				if (fresh) {
					return catalog_kind::fresh;
				}
				auto schema    = detail::scalar_text(Database, "SELECT sql FROM sqlite_schema WHERE name='device_binding' AND type='table'");
				auto objects   = detail::scalar_integer(Database, "SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'");
				auto integrity = detail::scalar_text(Database, "PRAGMA quick_check");
				if (!schema || *schema != (legacy ? LegacySchema : std::string_view{ Schema }) || !objects || *objects != (legacy ? 1 : 2) || !integrity || *integrity != "ok") {
					return corrupt();
				}
				if (!legacy) {
					auto scopeSchema = detail::scalar_text(Database, "SELECT sql FROM sqlite_schema WHERE name='binding_scope' AND type='table'");
					auto scope       = detail::scalar_text(Database, "SELECT profile FROM binding_scope");
					auto scopes      = detail::scalar_integer(Database, "SELECT count(*) FROM binding_scope");
					if (!scopeSchema || *scopeSchema != ScopeSchema || !scope || *scope != std::to_string(BprProfile.value()) || !scopes || *scopes != 1) {
						return corrupt();
					}
				}
				if (!read_rows(aStop, legacy)) {
					return corrupt();
				}
				return legacy ? catalog_kind::legacy : catalog_kind::current;
			}
			auto migrate(const bool aLegacy, std::stop_token aStop) -> bool
			{
				auto succeeded = detail::execute(Database, "BEGIN IMMEDIATE", aStop).has_value();
				if (succeeded && aLegacy) {
					succeeded = detail::execute(Database, "ALTER TABLE device_binding RENAME TO legacy_binding", aStop).has_value();
				}
				succeeded = succeeded && detail::execute(Database, Schema, aStop) && detail::execute(Database, ScopeSchema, aStop)
					&& detail::execute(Database, "INSERT INTO binding_scope VALUES('" + std::to_string(BprProfile.value()) + "')", aStop);
				if (succeeded && aLegacy) {
					succeeded = detail::execute(Database, "INSERT INTO device_binding SELECT *, '1' FROM legacy_binding", aStop)
						&& detail::execute(Database, "DROP TABLE legacy_binding", aStop);
				}
				return succeeded && read_rows(aStop) && detail::execute(Database, "PRAGMA application_id=1112560194", aStop)
					&& detail::execute(Database, "PRAGMA user_version=2", aStop) && detail::execute(Database, "COMMIT", aStop);
			}
			std::filesystem::path                  Root;
			const core::bpr_profile_id             BprProfile;
			std::optional<detail::state_directory> Directory;
			sqlite3*                               Database = nullptr;
			bool                                   Failed   = false;
		};
	}
	auto make_device_binding_store(std::filesystem::path aStorageRoot, core::bpr_profile_id aBprProfile) -> std::unique_ptr<application::device_binding_store_port>
	{
		return std::make_unique<sqlite_device_binding_store>(std::move(aStorageRoot), aBprProfile);
	}
}
