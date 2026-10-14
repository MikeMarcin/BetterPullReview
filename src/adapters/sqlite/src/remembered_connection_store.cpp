#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#if defined(BPR_SQLITE_TESTING)
	#include "bpr/adapters/sqlite/testing.hpp"
	#include <atomic>
#endif
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"

#include <charconv>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		using failure = application::remembered_connection_store_failure;
		template <typename T>
		using result = application::remembered_connection_store_result<T>;
#if defined(BPR_SQLITE_TESTING)
		std::atomic<detail::testing::remembered_connection_write_checkpoint> WriteCheckpoint{ nullptr };
#endif
		constexpr std::int64_t     ApplicationId = 0x42505243;
		constexpr std::string_view MetaSchema    = "CREATE TABLE connection_meta(singleton INTEGER PRIMARY KEY CHECK(singleton=1),bpr_profile TEXT NOT NULL,revision TEXT NOT NULL,last_profile TEXT NOT NULL,last_host TEXT NOT NULL,last_selected TEXT NOT NULL) STRICT";
		constexpr std::string_view RowSchema     = "CREATE TABLE remembered_connection(hostname TEXT PRIMARY KEY,profile TEXT NOT NULL,revision TEXT NOT NULL,host TEXT NOT NULL,account TEXT NOT NULL,login TEXT NOT NULL,server INTEGER NOT NULL CHECK(server IN(0,1)),transport INTEGER NOT NULL CHECK(transport IN(0,1)),selected_order TEXT NOT NULL,reuse_enabled INTEGER NOT NULL CHECK(reuse_enabled IN(0,1))) STRICT";
		// [[BPR-1494]]. Version 2 adds the native identities a different account's
		// sign-in displaced. Version 1 is upgraded in place once it is admitted.
		constexpr std::string_view RetiredSchema = "CREATE TABLE retired_connection(profile TEXT PRIMARY KEY,revision TEXT NOT NULL,host TEXT NOT NULL,hostname TEXT NOT NULL,account TEXT NOT NULL,login TEXT NOT NULL,server INTEGER NOT NULL CHECK(server IN(0,1)),selected_order TEXT NOT NULL) STRICT";
		constexpr std::int64_t     SchemaVersion = 2;

		auto read_failure(sqlite3* aDatabase, std::stop_token aStop) -> failure
		{
			if (aStop.stop_requested()) {
				return failure::cancelled;
			}
			const auto code = sqlite3_extended_errcode(aDatabase);
			if (detail::classify_sqlite_result(code) == detail::sqlite_result_classification::integrity_failure
				|| (code & 0xff) == SQLITE_OK || (code & 0xff) == SQLITE_ERROR || (code & 0xff) == SQLITE_DONE || (code & 0xff) == SQLITE_ROW
				|| (code & 0xff) == SQLITE_TOOBIG || (code & 0xff) == SQLITE_INTERRUPT) {
				return failure::corrupt;
			}
			return failure::unavailable;
		}
		auto text_of(sqlite3_stmt* aStatement, int aColumn) -> std::string_view
		{
			if (sqlite3_column_type(aStatement, aColumn) != SQLITE_TEXT) {
				return { };
			}
			const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(aStatement, aColumn));
			return bytes ? std::string_view{ bytes, static_cast<std::size_t>(sqlite3_column_bytes(aStatement, aColumn)) } : std::string_view{ };
		}
		auto id_of(sqlite3_stmt* aStatement, int aColumn) -> std::optional<std::uint64_t>
		{
			const auto text = text_of(aStatement, aColumn);
			if (text.empty() || text.size() > 20U || (text.size() > 1U && text.front() == '0')) {
				return std::nullopt;
			}
			std::uint64_t value  = 0U;
			const auto    parsed = std::from_chars(text.data(), text.data() + text.size(), value);
			return parsed.ec == std::errc{ } && parsed.ptr == text.data() + text.size() ? std::optional{ value } : std::nullopt;
		}
		auto bind_text(sqlite3_stmt* aStatement, int aColumn, std::string_view aText) -> bool
		{
			return sqlite3_bind_text(aStatement, aColumn, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		auto bind_id(sqlite3_stmt* aStatement, int aColumn, std::uint64_t aValue) -> bool { return bind_text(aStatement, aColumn, std::to_string(aValue)); }

		class transaction final
		{
		public:
			explicit transaction(sqlite3* aDatabase)
				: Database{ aDatabase }
			{ }
			~transaction() { static_cast<void>(detail::execute(Database, "ROLLBACK")); }
			transaction(const transaction&)                    = delete;
			auto operator=(const transaction&) -> transaction& = delete;

		private:
			sqlite3* Database;
		};
		class read_scope final
		{
		public:
			read_scope(sqlite3* aDatabase, std::stop_token aStop)
				: Stop{ aStop }
				, Handler{ aDatabase, 100, progress, this }
			{
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, 4'096));
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 8'192));
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 16));
				static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_VDBE_OP, 10'000));
			}
			read_scope(const read_scope&)                    = delete;
			auto operator=(const read_scope&) -> read_scope& = delete;

		private:
			static auto progress(void* aContext) -> int
			{
				auto& scope = *static_cast<read_scope*>(aContext);
				return scope.Stop.stop_requested() || ++scope.Steps > 3'000U ? 1 : 0;
			}
			std::stop_token                Stop;
			std::size_t                    Steps = 0U;
			detail::progress_handler_scope Handler;
		};

		/// Returns the admitted version: the current one, or 1 only where aLegacy permits it.
		auto validate_schema(sqlite3* aDatabase, std::stop_token aStop, bool aLegacy) -> result<std::int64_t>
		{
			const auto version  = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			const auto identity = detail::scalar_integer(aDatabase, "PRAGMA application_id");
			if (!version || !identity) {
				return std::unexpected(read_failure(aDatabase, aStop));
			}
			if (*identity != ApplicationId) {
				return std::unexpected(failure::corrupt);
			}
			if (*version > SchemaVersion) {
				return std::unexpected(failure::newer_schema);
			}
			const auto count = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 4)");
			const auto meta  = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='connection_meta'");
			const auto rows  = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='remembered_connection'");
			if (!count || !meta || !rows) {
				return std::unexpected(read_failure(aDatabase, aStop));
			}
			auto retired = std::string{ RetiredSchema };
			if (*version == SchemaVersion) {
				const auto stored = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='retired_connection'");
				if (!stored) {
					return std::unexpected(read_failure(aDatabase, aStop));
				}
				retired = *stored;
			}
			const auto legacy = *version == 1 && aLegacy;
			if ((*version != SchemaVersion && !legacy) || *count != (legacy ? 2 : 3) || detail::canonical_sql(*meta) != detail::canonical_sql(MetaSchema)
				|| detail::canonical_sql(*rows) != detail::canonical_sql(RowSchema) || detail::canonical_sql(retired) != detail::canonical_sql(RetiredSchema)) {
				return std::unexpected(failure::corrupt);
			}
			return *version;
		}

		auto read_snapshot(sqlite3* aDatabase, core::bpr_profile_id aProfile, std::stop_token aStop, bool aLegacy = false) -> result<core::remembered_connection_state>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			const auto limits = read_scope{ aDatabase, aStop };
			const auto schema = validate_schema(aDatabase, aStop, aLegacy);
			if (!schema) {
				return std::unexpected(schema.error());
			}
			auto meta = detail::prepare(aDatabase, "SELECT singleton,bpr_profile,revision,last_profile,last_host,last_selected FROM connection_meta LIMIT 2");
			if (!meta) {
				return std::unexpected(read_failure(aDatabase, aStop));
			}
			auto step = sqlite3_step(meta->Value);
			if (step != SQLITE_ROW) {
				return std::unexpected(step == SQLITE_DONE ? failure::corrupt : read_failure(aDatabase, aStop));
			}
			if (sqlite3_column_type(meta->Value, 0) != SQLITE_INTEGER || sqlite3_column_int64(meta->Value, 0) != 1) {
				return std::unexpected(failure::corrupt);
			}
			const auto profile      = id_of(meta->Value, 1);
			const auto revision     = id_of(meta->Value, 2);
			const auto lastProfile  = id_of(meta->Value, 3);
			const auto lastHost     = id_of(meta->Value, 4);
			const auto lastSelected = id_of(meta->Value, 5);
			if (!profile || !revision || !lastProfile || !lastHost || !lastSelected || *profile != aProfile.value()) {
				return std::unexpected(failure::corrupt);
			}
			step = sqlite3_step(meta->Value);
			if (step != SQLITE_DONE) {
				return std::unexpected(step == SQLITE_ROW ? failure::corrupt : read_failure(aDatabase, aStop));
			}
			core::remembered_connection_state state;
			state.BprProfile        = aProfile;
			state.Revision          = *revision;
			state.LastProfileId     = *lastProfile;
			state.LastHostId        = *lastHost;
			state.LastSelectedOrder = *lastSelected;
			auto rows               = detail::prepare(aDatabase, "SELECT hostname,profile,revision,host,account,login,server,transport,selected_order,reuse_enabled FROM remembered_connection ORDER BY hostname LIMIT 65");
			if (!rows) {
				return std::unexpected(read_failure(aDatabase, aStop));
			}
			while ((step = sqlite3_step(rows->Value)) == SQLITE_ROW) {
				if (aStop.stop_requested()) {
					return std::unexpected(failure::cancelled);
				}
				if (state.Connections.size() >= core::maximum_remembered_connections) {
					return std::unexpected(failure::corrupt);
				}
				const auto id              = id_of(rows->Value, 1);
				const auto profileRevision = id_of(rows->Value, 2);
				const auto host            = id_of(rows->Value, 3);
				const auto account         = id_of(rows->Value, 4);
				const auto order           = id_of(rows->Value, 8);
				if (!id || !profileRevision || !host || !account || !order) {
					return std::unexpected(failure::corrupt);
				}
				for (int column : { 6, 7, 9 }) {
					if (sqlite3_column_type(rows->Value, column) != SQLITE_INTEGER || sqlite3_column_int64(rows->Value, column) < 0 || sqlite3_column_int64(rows->Value, column) > 1) {
						return std::unexpected(failure::corrupt);
					}
				}
				core::remembered_connection row;
				row.Profile         = core::host_profile_id{ *id };
				row.ProfileRevision = *profileRevision;
				row.Host            = core::git_host_id{ *host };
				row.Hostname        = text_of(rows->Value, 0);
				row.Account         = core::github_account_id{ *account };
				row.Login           = text_of(rows->Value, 5);
				row.Server          = static_cast<core::remembered_connection_server>(sqlite3_column_int(rows->Value, 6));
				row.Transport       = static_cast<core::remembered_connection_transport>(sqlite3_column_int(rows->Value, 7));
				row.SelectedOrder   = *order;
				row.ReuseEnabled    = sqlite3_column_int(rows->Value, 9) == 1;
				state.Connections.push_back(std::move(row));
			}
			if (step != SQLITE_DONE) {
				return std::unexpected(read_failure(aDatabase, aStop));
			}
			if (*schema == SchemaVersion) {
				auto retired = detail::prepare(aDatabase, "SELECT profile,revision,host,hostname,account,login,server,selected_order FROM retired_connection ORDER BY CAST(selected_order AS INTEGER) LIMIT 17");
				if (!retired) {
					return std::unexpected(read_failure(aDatabase, aStop));
				}
				while ((step = sqlite3_step(retired->Value)) == SQLITE_ROW) {
					if (aStop.stop_requested()) {
						return std::unexpected(failure::cancelled);
					}
					if (state.Retired.size() >= core::maximum_retired_connections) {
						return std::unexpected(failure::corrupt);
					}
					const auto id              = id_of(retired->Value, 0);
					const auto profileRevision = id_of(retired->Value, 1);
					const auto host            = id_of(retired->Value, 2);
					const auto account         = id_of(retired->Value, 4);
					const auto order           = id_of(retired->Value, 7);
					if (!id || !profileRevision || !host || !account || !order || sqlite3_column_type(retired->Value, 6) != SQLITE_INTEGER
						|| sqlite3_column_int64(retired->Value, 6) < 0 || sqlite3_column_int64(retired->Value, 6) > 1) {
						return std::unexpected(failure::corrupt);
					}
					core::remembered_connection row;
					row.Profile         = core::host_profile_id{ *id };
					row.ProfileRevision = *profileRevision;
					row.Host            = core::git_host_id{ *host };
					row.Hostname        = text_of(retired->Value, 3);
					row.Account         = core::github_account_id{ *account };
					row.Login           = text_of(retired->Value, 5);
					row.Server          = static_cast<core::remembered_connection_server>(sqlite3_column_int(retired->Value, 6));
					row.Transport       = core::remembered_connection_transport::native_device;
					row.SelectedOrder   = *order;
					row.ReuseEnabled    = false;
					state.Retired.push_back(std::move(row));
				}
				if (step != SQLITE_DONE) {
					return std::unexpected(read_failure(aDatabase, aStop));
				}
			}
			if (!core::valid(state)) {
				return std::unexpected(failure::corrupt);
			}
			return state;
		}

		auto write_snapshot(sqlite3* aDatabase, const core::remembered_connection_state& aState, std::stop_token aStop) -> result<void>
		{
			if (!detail::execute(aDatabase, "DELETE FROM remembered_connection", aStop) || !detail::execute(aDatabase, "DELETE FROM retired_connection", aStop)
				|| !detail::execute(aDatabase, "DELETE FROM connection_meta", aStop)) {
				return std::unexpected(read_failure(aDatabase, aStop));
			}
			for (const auto& row : aState.Retired) {
				if (aStop.stop_requested()) {
					return std::unexpected(failure::cancelled);
				}
				auto statement = detail::prepare(aDatabase, "INSERT INTO retired_connection VALUES(?1,?2,?3,?4,?5,?6,?7,?8)");
				if (!statement || !bind_id(statement->Value, 1, row.Profile.value()) || !bind_id(statement->Value, 2, row.ProfileRevision)
					|| !bind_id(statement->Value, 3, row.Host.value()) || !bind_text(statement->Value, 4, row.Hostname)
					|| !bind_id(statement->Value, 5, row.Account.value()) || !bind_text(statement->Value, 6, row.Login)
					|| sqlite3_bind_int(statement->Value, 7, static_cast<int>(row.Server)) != SQLITE_OK || !bind_id(statement->Value, 8, row.SelectedOrder)
					|| sqlite3_step(statement->Value) != SQLITE_DONE) {
					return std::unexpected(read_failure(aDatabase, aStop));
				}
			}
			auto meta = detail::prepare(aDatabase, "INSERT INTO connection_meta VALUES(1,?1,?2,?3,?4,?5)");
			if (!meta || !bind_id(meta->Value, 1, aState.BprProfile.value()) || !bind_id(meta->Value, 2, aState.Revision)
				|| !bind_id(meta->Value, 3, aState.LastProfileId) || !bind_id(meta->Value, 4, aState.LastHostId)
				|| !bind_id(meta->Value, 5, aState.LastSelectedOrder) || sqlite3_step(meta->Value) != SQLITE_DONE) {
				return std::unexpected(read_failure(aDatabase, aStop));
			}
			for (const auto& row : aState.Connections) {
				if (aStop.stop_requested()) {
					return std::unexpected(failure::cancelled);
				}
				auto statement = detail::prepare(aDatabase, "INSERT INTO remembered_connection VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)");
				if (!statement || !bind_text(statement->Value, 1, row.Hostname) || !bind_id(statement->Value, 2, row.Profile.value())
					|| !bind_id(statement->Value, 3, row.ProfileRevision) || !bind_id(statement->Value, 4, row.Host.value())
					|| !bind_id(statement->Value, 5, row.Account.value()) || !bind_text(statement->Value, 6, row.Login)
					|| sqlite3_bind_int(statement->Value, 7, static_cast<int>(row.Server)) != SQLITE_OK
					|| sqlite3_bind_int(statement->Value, 8, static_cast<int>(row.Transport)) != SQLITE_OK
					|| !bind_id(statement->Value, 9, row.SelectedOrder) || sqlite3_bind_int(statement->Value, 10, row.ReuseEnabled ? 1 : 0) != SQLITE_OK
					|| sqlite3_step(statement->Value) != SQLITE_DONE) {
					return std::unexpected(read_failure(aDatabase, aStop));
				}
			}
			return { };
		}

		class store final : public application::remembered_connection_store_port
		{
		public:
			store(std::filesystem::path aRoot, core::bpr_profile_id aProfile)
				: Root{ std::move(aRoot) }
				, Profile{ aProfile }
			{ }
			~store() override
			{
				if (Database) {
					sqlite3_close_v2(Database);
				}
			}
			auto load(std::stop_token aStop) -> result<core::remembered_connection_state> override
			{
				if (auto opened = ensure_open(aStop); !opened) {
					return std::unexpected(opened.error());
				}
				if (!detail::execute(Database, "BEGIN", aStop)) {
					return std::unexpected(read_failure(Database, aStop));
				}
				const auto rollback = transaction{ Database };
				return read_snapshot(Database, Profile, aStop);
			}
			auto save(const core::remembered_connection_state& aCandidate, std::uint64_t aExpectedRevision, std::stop_token aStop) -> result<void> override
			{
				if (!core::valid(aCandidate) || aCandidate.BprProfile != Profile) {
					return std::unexpected(failure::corrupt);
				}
				if (auto opened = ensure_open(aStop); !opened) {
					return opened;
				}
				if (!detail::execute(Database, "BEGIN IMMEDIATE", aStop)) {
					return std::unexpected(read_failure(Database, aStop));
				}
				const auto rollback = transaction{ Database };
				const auto before   = read_snapshot(Database, Profile, aStop);
				if (!before) {
					return std::unexpected(before.error());
				}
				if (before->Revision != aExpectedRevision) {
					return std::unexpected(failure::conflict);
				}
				if (!core::valid_remembered_connection_update(*before, aCandidate)) {
					return std::unexpected(failure::corrupt);
				}
				if (auto written = write_snapshot(Database, aCandidate, aStop); !written) {
					return written;
				}
#if defined(BPR_SQLITE_TESTING)
				if (const auto checkpoint = WriteCheckpoint.load()) {
					checkpoint();
				}
#endif
				if (aStop.stop_requested()) {
					return std::unexpected(failure::cancelled);
				}
				if (!detail::execute(Database, "COMMIT", aStop)) {
					return std::unexpected(read_failure(Database, aStop));
				}
				return { };
			}

		private:
			auto establish(std::stop_token aStop) -> result<void>
			{
				const auto limits   = read_scope{ Database, aStop };
				const auto version  = detail::scalar_integer(Database, "PRAGMA user_version");
				const auto identity = detail::scalar_integer(Database, "PRAGMA application_id");
				const auto count    = detail::scalar_integer(Database, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema LIMIT 1)");
				if (!version || !identity || !count) {
					return std::unexpected(read_failure(Database, aStop));
				}
				const auto pristine = *version == 0 && *identity == 0 && *count == 0;
				if (!pristine) {
					if (auto current = read_snapshot(Database, Profile, aStop, true); !current) {
						return std::unexpected(current.error());
					}
				}
				// Refused content is read-only above; configure may change WAL only after admission.
				if (aStop.stop_requested()) {
					return std::unexpected(failure::cancelled);
				}
				if (!detail::configure_connection(Database)) {
					return std::unexpected(failure::unavailable);
				}
				if (!pristine && *version == SchemaVersion) {
					return { };
				}
				if (!detail::execute(Database, "BEGIN IMMEDIATE", aStop)) {
					return std::unexpected(read_failure(Database, aStop));
				}
				const auto rollback = transaction{ Database };
				// Another opener may have initialized or upgraded it before our write lock.
				const auto now = detail::scalar_integer(Database, "PRAGMA user_version");
				if (!now) {
					return std::unexpected(read_failure(Database, aStop));
				}
				if (*now == SchemaVersion) {
					const auto current = read_snapshot(Database, Profile, aStop);
					return current ? result<void>{ } : std::unexpected(current.error());
				}
				if (*now != 0) {
					// [[BPR-1494]]. An admitted version 1 snapshot gains the empty
					// retired table; its rows and counters are unchanged.
					if (const auto current = read_snapshot(Database, Profile, aStop, true); !current) {
						return std::unexpected(current.error());
					}
					for (const auto sql : { RetiredSchema, std::string_view{ "PRAGMA user_version=2" } }) {
						if (!detail::execute(Database, sql, aStop)) {
							return std::unexpected(read_failure(Database, aStop));
						}
					}
					if (!detail::execute(Database, "COMMIT", aStop)) {
						return std::unexpected(read_failure(Database, aStop));
					}
					return { };
				}
				for (const auto sql : { MetaSchema, RowSchema, RetiredSchema, std::string_view{ "PRAGMA application_id=1112560195" }, std::string_view{ "PRAGMA user_version=2" } }) {
					if (!detail::execute(Database, sql, aStop)) {
						return std::unexpected(read_failure(Database, aStop));
					}
				}
				core::remembered_connection_state initial;
				initial.BprProfile = Profile;
				if (auto written = write_snapshot(Database, initial, aStop); !written) {
					return written;
				}
				if (!detail::execute(Database, "COMMIT", aStop)) {
					return std::unexpected(read_failure(Database, aStop));
				}
				return { };
			}
			auto ensure_open(std::stop_token aStop) -> result<void>
			{
				if (aStop.stop_requested()) {
					return std::unexpected(failure::cancelled);
				}
				if (Profile.value() == 0U) {
					return std::unexpected(failure::corrupt);
				}
				if (Database) {
					return { };
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return std::unexpected(failure::unavailable);
				}
				Directory.emplace(std::move(*directory));
				auto opened = detail::open_private_store_database(*Directory, "remembered-connections.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW);
				if (!opened) {
					return std::unexpected(failure::unavailable);
				}
				Database         = *opened;
				auto established = establish(aStop);
				if (!established) {
					sqlite3_close_v2(Database);
					Database = nullptr;
				}
				return established;
			}
			std::filesystem::path                  Root;
			core::bpr_profile_id                   Profile;
			std::optional<detail::state_directory> Directory;
			sqlite3*                               Database = nullptr;
		};
	}

#if defined(BPR_SQLITE_TESTING)
	void detail::testing::set_remembered_connection_write_checkpoint(remembered_connection_write_checkpoint aCheckpoint) noexcept { WriteCheckpoint.store(aCheckpoint); }
#endif

	auto make_remembered_connection_store(std::filesystem::path aRoot, core::bpr_profile_id aProfile) -> std::unique_ptr<application::remembered_connection_store_port>
	{
		return std::make_unique<store>(std::move(aRoot), aProfile);
	}
}
