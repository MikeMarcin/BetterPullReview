#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <string_view>
#include <utility>
namespace bpr::adapters::sqlite {
	namespace {
		using application::window_review_snapshot;
		using application::window_review_store_failure;
		using application::window_review_store_result;
		[[nodiscard]] auto fail(window_review_store_failure aFailure)
		{
			return std::unexpected(application::window_review_store_error{ aFailure, { } });
		}
		constexpr std::int64_t     ApplicationId       = 0x42505252;
		constexpr std::string_view MetaSchema          = "CREATE TABLE restoration_meta(singleton INTEGER PRIMARY KEY CHECK(singleton=1),revision TEXT NOT NULL) STRICT";
		constexpr std::string_view LegacyRecordsSchema = "CREATE TABLE restoration_records(window_id TEXT PRIMARY KEY,mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL) STRICT";
		constexpr std::string_view RecordsSchema       = "CREATE TABLE restoration_records(window_id TEXT PRIMARY KEY,mode INTEGER NOT NULL CHECK(mode IN(0,1)),hostname TEXT NOT NULL,owner TEXT NOT NULL,repository TEXT NOT NULL,number TEXT NOT NULL,position BLOB) STRICT";
		// Version 1 payload: fixed-width identities, big-endian integers, strict flags.
		class position_codec final
		{
		public:
			std::vector<std::uint8_t>     Bytes;
			std::span<const std::uint8_t> Input;
			bool                          Reading = false;
			bool                          Good    = true;
			void                          byte(std::uint8_t& aValue)
			{
				if (!Reading) {
					Bytes.push_back(aValue);
					return;
				}
				if (Input.empty()) {
					Good = false;
					return;
				}
				aValue = Input.front();
				Input  = Input.subspan(1);
			}
			void integer(std::uint64_t& aValue, unsigned aWidth)
			{
				auto result = std::uint64_t{ 0 };
				for (unsigned index = 0; index < aWidth; ++index) {
					auto value = static_cast<std::uint8_t>(aValue >> ((aWidth - index - 1U) * 8U));
					byte(value);
					result = (result << 8U) | value;
				}
				if (Reading) {
					aValue = result;
				}
			}
			template <typename TEnum>
			void enumeration(TEnum& aValue)
			{
				auto value = static_cast<std::uint8_t>(aValue);
				byte(value);
				aValue = static_cast<TEnum>(value);
			}
			template <typename TIdentity>
			void identity(TIdentity& aValue)
			{
				auto bytes = aValue.bytes();
				for (auto& value : bytes) {
					byte(value);
				}
				aValue = TIdentity{ bytes };
			}
			template <typename TValue, typename TFunction>
			void optional(std::optional<TValue>& aValue, TFunction aFunction)
			{
				auto present = static_cast<std::uint8_t>(aValue.has_value());
				byte(present);
				if (present > 1U) {
					Good = false;
					return;
				}
				if (present == 0U) {
					aValue.reset();
					return;
				}
				if (!aValue) {
					aValue.emplace();
				}
				aFunction(*aValue);
			}
			void anchor(core::comparison_anchor& aAnchor)
			{
				identity(aAnchor.Identity.Authority);
				identity(aAnchor.Identity.Incarnation);
				enumeration(aAnchor.Identity.Kind);
				auto epoch = static_cast<std::uint64_t>(aAnchor.Identity.Epoch.value());
				integer(epoch, 8);
				if (epoch > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					Good = false;
				}
				aAnchor.Identity.Epoch = core::epoch_ordinal{ static_cast<std::int64_t>(epoch) };
				identity(aAnchor.Identity.Review);
				auto algorithm = aAnchor.Commit.algorithm();
				enumeration(algorithm);
				auto bytes = aAnchor.Commit.bytes();
				for (auto& value : bytes) {
					byte(value);
				}
				aAnchor.Commit = core::git_commit_oid{ algorithm, bytes };
				enumeration(aAnchor.Availability);
			}
			void position(core::window_review_position& aPosition)
			{
				auto version = std::uint8_t{ 1 };
				byte(version);
				Good = Good && version == 1U;
				anchor(aPosition.Comparison.Left);
				anchor(aPosition.Comparison.Right);
				optional(aPosition.File, [this](auto& aValue) { this->identity(aValue); });
				optional(aPosition.Selection, [this](auto& aSelection) {
					identity(aSelection.Comparison);
					enumeration(aSelection.Kind);
					identity(aSelection.PreferredLine.Comparison);
					enumeration(aSelection.PreferredLine.Side);
					auto line = static_cast<std::uint64_t>(aSelection.PreferredLine.Line);
					integer(line, 4);
					aSelection.PreferredLine.Line = static_cast<std::uint32_t>(line);
					optional(aSelection.Segment, [this](auto& aValue) { this->identity(aValue); });
					optional(aSelection.Group, [this](auto& aValue) { this->identity(aValue); });
				});
			}
		};
		[[nodiscard]] auto bind_position(sqlite3_stmt* aStatement, const std::optional<core::window_review_position>& aPosition) -> bool
		{
			if (!aPosition) {
				return sqlite3_bind_null(aStatement, 7) == SQLITE_OK;
			}
			auto codec    = position_codec{ };
			auto position = *aPosition;
			codec.position(position);
			return codec.Good && sqlite3_bind_blob(aStatement, 7, codec.Bytes.data(), static_cast<int>(codec.Bytes.size()), SQLITE_TRANSIENT) == SQLITE_OK;
		}
		[[nodiscard]] auto read_position(sqlite3_stmt* aStatement, std::optional<core::window_review_position>& aPosition) -> bool
		{
			if (sqlite3_column_type(aStatement, 6) == SQLITE_NULL) {
				return true;
			}
			const auto size = sqlite3_column_bytes(aStatement, 6);
			if (sqlite3_column_type(aStatement, 6) != SQLITE_BLOB || size <= 0 || size > 512) {
				return false;
			}
			const auto* bytes = static_cast<const std::uint8_t*>(sqlite3_column_blob(aStatement, 6));
			if (!bytes) {
				return false;
			}
			auto codec = position_codec{ { }, { bytes, static_cast<std::size_t>(size) }, true, true };
			aPosition.emplace();
			codec.position(*aPosition);
			return codec.Good && codec.Input.empty() && core::valid(*aPosition);
		}
		// These connection-local limits perform no SQL and cannot change a refused file.
		void restrict_reads(sqlite3* aDatabase)
		{
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, 4'096));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 8'192));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 16));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_VDBE_OP, 10'000));
		}

		class bounded_read_scope final
		{
		public:
			bounded_read_scope(sqlite3* aDatabase, std::stop_token aStop)
				: Database{ aDatabase }
				, Stop{ aStop }
			{
				sqlite3_progress_handler(Database, 100, progress, this);
			}
			~bounded_read_scope() { sqlite3_progress_handler(Database, 0, nullptr, nullptr); }
			bounded_read_scope(const bounded_read_scope&)                    = delete;
			auto operator=(const bounded_read_scope&) -> bounded_read_scope& = delete;

		private:
			static auto progress(void* aContext) -> int
			{
				auto& scope = *static_cast<bounded_read_scope*>(aContext);
				return scope.Stop.stop_requested() || ++scope.Steps >= 200U ? 1 : 0;
			}
			sqlite3*        Database;
			std::stop_token Stop;
			std::size_t     Steps = 0U;
		};

		[[nodiscard]] auto translate(const detail::failure aFailure) -> window_review_store_failure
		{
			return aFailure == detail::failure::cancelled ? window_review_store_failure::cancelled : window_review_store_failure::unavailable;
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
		[[nodiscard]] auto run(sqlite3* aDatabase, const std::string_view aSql, const std::stop_token aStop = { }) -> window_review_store_result<void>
		{
			const auto result = detail::execute(aDatabase, aSql, aStop);
			return result ? window_review_store_result<void>{ } : fail(translate(result.error()));
		}

		class transaction_scope final
		{
		public:
			explicit transaction_scope(sqlite3* aDatabase)
				: Database{ aDatabase }
			{ }
			~transaction_scope() { static_cast<void>(sqlite3_exec(Database, "ROLLBACK", nullptr, nullptr, nullptr)); }
			transaction_scope(const transaction_scope&)                    = delete;
			auto operator=(const transaction_scope&) -> transaction_scope& = delete;

		private:
			sqlite3* Database;
		};

		[[nodiscard]] auto read_snapshot(sqlite3* aDatabase, std::stop_token aStop) -> window_review_store_result<window_review_snapshot>
		{
			if (aStop.stop_requested()) {
				return fail(window_review_store_failure::cancelled);
			}
			const auto scope    = bounded_read_scope{ aDatabase, aStop };
			const auto version  = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			const auto identity = detail::scalar_integer(aDatabase, "PRAGMA application_id");
			const auto count    = detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 3)");
			const auto meta     = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='restoration_meta'");
			const auto records  = detail::scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='restoration_records'");
			if (!version || (*version != 1 && *version != 2) || !identity || *identity != ApplicationId || !count || *count != 2
				|| !meta || detail::canonical_sql(*meta) != detail::canonical_sql(MetaSchema)
				|| !records || detail::canonical_sql(*records) != detail::canonical_sql(*version == 1 ? LegacyRecordsSchema : RecordsSchema)) {
				return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::schema_mismatch);
			}
			auto revision = detail::prepare(aDatabase, "SELECT singleton,revision FROM restoration_meta LIMIT 2");
			if (!revision || sqlite3_step(revision->Value) != SQLITE_ROW || sqlite3_column_int64(revision->Value, 0) != 1) {
				return fail(window_review_store_failure::corrupt);
			}
			auto result     = window_review_snapshot{ };
			result.Revision = id_of(revision->Value, 1);
			if (sqlite3_step(revision->Value) != SQLITE_DONE) {
				return fail(window_review_store_failure::corrupt);
			}
			auto rows = detail::prepare(aDatabase, *version == 1 ? "SELECT window_id,mode,hostname,owner,repository,number FROM restoration_records LIMIT 33" : "SELECT window_id,mode,hostname,owner,repository,number,position FROM restoration_records LIMIT 33");
			if (!rows) {
				return fail(window_review_store_failure::corrupt);
			}
			for (;;) {
				if (aStop.stop_requested()) {
					return fail(window_review_store_failure::cancelled);
				}
				const auto step = sqlite3_step(rows->Value);
				if (step == SQLITE_DONE) {
					break;
				}
				if (step != SQLITE_ROW || result.Records.size() >= core::maximum_bpr_windows_per_profile) {
					return fail(window_review_store_failure::corrupt);
				}
				const auto mode   = sqlite3_column_int64(rows->Value, 1);
				const auto number = id_of(rows->Value, 5);
				if (sqlite3_column_type(rows->Value, 1) != SQLITE_INTEGER || mode < 0 || mode > 1 || number > core::maximum_pull_request_number) {
					return fail(window_review_store_failure::corrupt);
				}
				result.Records.push_back({ core::bpr_window_id{ id_of(rows->Value, 0) },
										   { static_cast<core::github_access_mode>(mode), std::string{ text_of(rows->Value, 2) }, { std::string{ text_of(rows->Value, 3) }, std::string{ text_of(rows->Value, 4) } }, core::pull_request_number{ static_cast<std::uint32_t>(number) } } });
				if (*version == 2 && !read_position(rows->Value, result.Records.back().Position)) {
					return fail(window_review_store_failure::corrupt);
				}
			}
			std::ranges::sort(result.Records, { }, &core::window_review_record::Window);
			if (!application::valid(result)) {
				return fail(window_review_store_failure::corrupt);
			}
			if (aStop.stop_requested()) {
				return fail(window_review_store_failure::cancelled);
			}
			return result;
		}
		[[nodiscard]] auto pristine(sqlite3* aDatabase, std::stop_token aStop) -> bool
		{
			const auto scope = bounded_read_scope{ aDatabase, aStop };
			return detail::scalar_integer(aDatabase, "PRAGMA user_version") == 0
				&& detail::scalar_integer(aDatabase, "PRAGMA application_id") == 0
				&& detail::scalar_integer(aDatabase, "SELECT count(*) FROM (SELECT 1 FROM sqlite_schema LIMIT 1)") == 0;
		}
		class sqlite_window_review_store final : public application::window_review_restoration_port
		{
		public:
			explicit sqlite_window_review_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			~sqlite_window_review_store() override
			{
				if (Database) {
					sqlite3_close_v2(Database);
				}
			}
			auto load(std::stop_token aStop) -> window_review_store_result<window_review_snapshot> override
			{
				if (auto opened = ensure_open(aStop); !opened) {
					return std::unexpected(opened.error());
				}
				if (auto begin = run(Database, "BEGIN", aStop); !begin) {
					return std::unexpected(begin.error());
				}
				const auto transaction = transaction_scope{ Database };
				return read_snapshot(Database, aStop);
			}
			auto save(const window_review_snapshot& aCandidate, std::uint64_t aExpectedRevision, std::stop_token aStop) -> window_review_store_result<window_review_snapshot> override
			{
				if (!application::valid(aCandidate) || aExpectedRevision == 0 || aExpectedRevision == std::numeric_limits<std::uint64_t>::max() || aCandidate.Revision != aExpectedRevision + 1U) {
					return fail(window_review_store_failure::corrupt);
				}
				if (auto opened = ensure_open(aStop); !opened) {
					return std::unexpected(opened.error());
				}
				if (auto begin = run(Database, "BEGIN IMMEDIATE", aStop); !begin) {
					return std::unexpected(begin.error());
				}
				const auto transaction = transaction_scope{ Database };
				const auto current     = read_snapshot(Database, aStop);
				if (!current) {
					return std::unexpected(current.error());
				}
				if (current->Revision != aExpectedRevision) {
					return std::unexpected(application::window_review_store_error{ window_review_store_failure::conflict, *current });
				}
				if (auto erased = run(Database, "DELETE FROM restoration_records", aStop); !erased) {
					return std::unexpected(erased.error());
				}
				for (const auto& record : aCandidate.Records) {
					if (aStop.stop_requested()) {
						return fail(window_review_store_failure::cancelled);
					}
					auto row = detail::prepare(Database, "INSERT INTO restoration_records VALUES(?1,?2,?3,?4,?5,?6,?7)");
					if (!row || !bind_text(row->Value, 1, std::to_string(record.Window.value()))
						|| sqlite3_bind_int(row->Value, 2, static_cast<int>(record.Destination.Mode)) != SQLITE_OK
						|| !bind_text(row->Value, 3, record.Destination.Hostname)
						|| !bind_text(row->Value, 4, record.Destination.Repository.Owner)
						|| !bind_text(row->Value, 5, record.Destination.Repository.Name)
						|| !bind_text(row->Value, 6, std::to_string(record.Destination.Number.value()))
						|| !bind_position(row->Value, record.Position)
						|| sqlite3_step(row->Value) != SQLITE_DONE) {
						return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
					}
				}
				auto meta = detail::prepare(Database, "UPDATE restoration_meta SET revision=?1 WHERE singleton=1");
				if (!meta || !bind_text(meta->Value, 1, std::to_string(aCandidate.Revision)) || sqlite3_step(meta->Value) != SQLITE_DONE) {
					return fail(aStop.stop_requested() ? window_review_store_failure::cancelled : window_review_store_failure::unavailable);
				}
				if (auto commit = run(Database, "COMMIT", aStop); !commit) {
					return std::unexpected(commit.error());
				}
				return aCandidate;
			}

		private:
			[[nodiscard]] auto establish(std::stop_token aStop) -> window_review_store_result<void>
			{
				if (!pristine(Database, aStop)) {
					const auto current = read_snapshot(Database, aStop);
					if (!current) {
						return std::unexpected(current.error());
					}
				}
				if (auto configured = detail::configure_connection(Database); !configured) {
					return fail(translate(configured.error()));
				}
				if (auto begin = run(Database, "BEGIN IMMEDIATE", aStop); !begin) {
					return begin;
				}
				const auto transaction = transaction_scope{ Database };
				if (!pristine(Database, aStop)) {
					const auto current = read_snapshot(Database, aStop);
					if (!current) {
						return std::unexpected(current.error());
					}
					if (detail::scalar_integer(Database, "PRAGMA user_version") == 1) {
						// Rebuild inside the transaction: ALTER's internal quick-check view
						// is unavailable on this deliberately view-disabled connection.
						for (const auto sql : { std::string_view{ "CREATE TEMP TABLE restoration_migration AS SELECT * FROM restoration_records" }, std::string_view{ "DROP TABLE restoration_records" }, RecordsSchema, std::string_view{ "INSERT INTO restoration_records SELECT *,NULL FROM restoration_migration" }, std::string_view{ "DROP TABLE restoration_migration" }, std::string_view{ "PRAGMA user_version=2" }, std::string_view{ "COMMIT" } }) {
							if (auto result = run(Database, sql, aStop); !result) {
								return result;
							}
						}
					}
					return { };
				}
				for (const auto sql : { MetaSchema, RecordsSchema, std::string_view{ "INSERT INTO restoration_meta VALUES(1,'1')" }, std::string_view{ "PRAGMA application_id=1112560210" }, std::string_view{ "PRAGMA user_version=2" }, std::string_view{ "COMMIT" } }) {
					if (auto result = run(Database, sql, aStop); !result) {
						return result;
					}
				}
				return { };
			}

			[[nodiscard]] auto ensure_open(const std::stop_token aStop) -> window_review_store_result<void>
			{
				if (aStop.stop_requested()) {
					return fail(window_review_store_failure::cancelled);
				}
				if (Database != nullptr) {
					return { };
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory || !detail::sqlite_runtime_ready()) {
					return fail(window_review_store_failure::unavailable);
				}
				Directory.emplace(std::move(*directory));
				const auto      path = Directory->path() / "window-restoration.sqlite3";
				std::error_code error;
				const auto      status = std::filesystem::symlink_status(path, error);
				if (std::filesystem::exists(status) && (!std::filesystem::is_regular_file(status) || (status.permissions() & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) != std::filesystem::perms::none)) {
					return fail(window_review_store_failure::unavailable);
				}
				if (sqlite3_open_v2(path.string().c_str(), &Database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_NOFOLLOW, nullptr) != SQLITE_OK) {
					if (Database != nullptr) {
						sqlite3_close_v2(Database);
						Database = nullptr;
					}
					return fail(window_review_store_failure::unavailable);
				}
				restrict_reads(Database);
				std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, error);
				auto result = error ? window_review_store_result<void>{ fail(window_review_store_failure::unavailable) } : establish(aStop);
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
	auto make_window_review_restoration_store(std::filesystem::path aStorageRoot) -> std::unique_ptr<application::window_review_restoration_port> { return std::make_unique<sqlite_window_review_store>(std::move(aStorageRoot)); }
}
