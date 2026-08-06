#include "workbench_settings_database.hpp"

#include <sqlite3.h>

#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::sqlite::detail {
	namespace {
		// "BPRW". Distinct from the ledger's "BPRD" and the queue's "BPRQ" so
		// that pointing one store at another's file fails at the identity check
		// rather than at the first query for a table that is not there.
		constexpr std::int32_t WorkbenchApplicationId = 0x42505257;

		/// \brief Builds the statements that create schema version 1.
		///
		/// \details The width bounds are interpolated from
		/// `application::minimum_workbench_drawer_width` and its maximum rather
		/// than written out, so the constraint the database enforces and the
		/// bound the application publishes cannot drift apart. That is also why
		/// this is a function returning strings instead of the `constexpr
		/// std::array<std::string_view>` the other stores use.
		///
		/// The layout is one row. `singleton INTEGER PRIMARY KEY CHECK(singleton
		/// = 1)` is what makes a second layout unstorable rather than merely
		/// unwritten.
		///
		/// There is no column for a path, a repository, a pull request, or any
		/// review content. Drawer geometry is a reviewer preference, and this
		/// file holds nothing that names what they were reading.
		[[nodiscard]] auto schema_version_1() -> std::vector<std::string>
		{
			const auto minimum
				= std::to_string(application::minimum_workbench_drawer_width);
			const auto maximum
				= std::to_string(application::maximum_workbench_drawer_width);
			return {
				std::string{
					"CREATE TABLE workbench_meta("
					"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
					"schema_format INTEGER NOT NULL"
					") STRICT" },
				"CREATE TABLE workbench_layout("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"files_state INTEGER NOT NULL CHECK(files_state BETWEEN 0 AND 1),"
				"conversations_state INTEGER NOT NULL "
				"CHECK(conversations_state BETWEEN 0 AND 1),"
				"files_width INTEGER NOT NULL CHECK(files_width BETWEEN "
					+ minimum + " AND " + maximum + "),"
													"conversations_width INTEGER NOT NULL "
													"CHECK(conversations_width BETWEEN "
					+ minimum + " AND " + maximum + ")"
													") STRICT",
			};
		}

		[[nodiscard]] auto run_all(
			::sqlite3*                         aDatabase,
			const std::span<const std::string> aStatements,
			std::stop_token                    aStop)
			-> std::expected<void, failure>
		{
			for (const auto& sql : aStatements) {
				if (auto ran = execute(aDatabase, sql, aStop); !ran) {
					return ran;
				}
			}
			return { };
		}

		[[nodiscard]] auto set_version(
			::sqlite3* aDatabase, const std::int64_t aVersion, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto sql = "PRAGMA user_version = " + std::to_string(aVersion);
			return execute(aDatabase, sql, aStop);
		}

		/// \brief Creates the schema inside one transaction.
		[[nodiscard]] auto establish_schema(
			::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto current = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!current) {
				return std::unexpected(current.error());
			}
			if (*current == workbench_settings_schema_version) {
				return { };
			}
			// Forward only. A newer database is refused rather than opened,
			// because this build cannot know what the newer version added and
			// would silently drop it on the next write.
			if (*current > workbench_settings_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(
					aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};

			const auto statements = schema_version_1();
			if (auto created = run_all(aDatabase, statements, aStop); !created) {
				rollback();
				return created;
			}
			// Derived from the constant rather than written out, so the stamp
			// and the validation cannot disagree.
			if (auto app = execute(
					aDatabase,
					"PRAGMA application_id = "
						+ std::to_string(WorkbenchApplicationId),
					aStop);
				!app) {
				rollback();
				return app;
			}
			if (auto meta = execute(
					aDatabase,
					"INSERT INTO workbench_meta(singleton, schema_format) "
					"VALUES(1, "
						+ std::to_string(workbench_settings_schema_version) + ")",
					aStop);
				!meta) {
				rollback();
				return meta;
			}
			if (auto stamped = set_version(
					aDatabase, workbench_settings_schema_version, aStop);
				!stamped) {
				rollback();
				return stamped;
			}
			if (auto commit = execute(aDatabase, "COMMIT", aStop); !commit) {
				rollback();
				return commit;
			}
			return { };
		}

		[[nodiscard]] auto validate(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			const auto application
				= scalar_integer(aDatabase, "PRAGMA application_id");
			const auto version = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!application || *application != WorkbenchApplicationId || !version
				|| *version != workbench_settings_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto meta = prepare(
				aDatabase,
				"SELECT schema_format FROM workbench_meta WHERE singleton = 1");
			if (!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
				|| ::sqlite3_column_int64(meta->Value, 0)
					!= workbench_settings_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}

		[[nodiscard]] auto text_of(::sqlite3_stmt* aStatement, const int aColumn)
			-> std::string
		{
			const auto* const bytes = ::sqlite3_column_text(aStatement, aColumn);
			if (bytes == nullptr) {
				return { };
			}
			return std::string{ reinterpret_cast<const char*>(bytes) };
		}

		/// \brief Reads one stored drawer state, refusing an unknown value.
		///
		/// \details `STRICT` and the column's `CHECK` already refuse a value
		/// this build never wrote, but a durable file can be replaced wholesale
		/// by another build or by hand. The vocabulary is closed, so a value
		/// outside it is refused here rather than cast into a state the core
		/// would then be asked to reason about.
		[[nodiscard]] auto drawer_state_of(const std::int64_t aValue)
			-> std::expected<core::workbench_drawer_state, failure>
		{
			switch (aValue) {
			case 0:
				return core::workbench_drawer_state::collapsed;
			case 1:
				return core::workbench_drawer_state::expanded;
			default:
				return std::unexpected(failure::integrity_failure);
			}
		}
	} // namespace

	workbench_settings_database::workbench_settings_database(
		::sqlite3* const aHandle) noexcept
		: Handle{ aHandle }
	{
	}

	workbench_settings_database::workbench_settings_database(
		workbench_settings_database&& aOther) noexcept
		: Handle{ std::exchange(aOther.Handle, nullptr) }
	{
	}

	auto workbench_settings_database::operator=(
		workbench_settings_database&& aOther) noexcept
		-> workbench_settings_database&
	{
		if (this != &aOther) {
			if (Handle != nullptr) {
				(void)::sqlite3_close_v2(Handle);
			}
			Handle = std::exchange(aOther.Handle, nullptr);
		}
		return *this;
	}

	workbench_settings_database::~workbench_settings_database()
	{
		if (Handle != nullptr) {
			(void)::sqlite3_close_v2(Handle);
			Handle = nullptr;
		}
	}

	auto workbench_settings_database::open(
		const std::filesystem::path& aPath, std::stop_token aStop)
		-> std::expected<workbench_settings_database, failure>
	{
		if (aPath.empty() || !aPath.is_absolute()) {
			return std::unexpected(failure::invalid_path);
		}
		if (!sqlite_runtime_ready()) {
			return std::unexpected(failure::unavailable);
		}

		::sqlite3* database = nullptr;
		const auto native   = aPath.string();
		const auto code     = ::sqlite3_open_v2(
			native.c_str(),
			&database,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX
				| SQLITE_OPEN_EXRESCODE,
			nullptr);
		auto owned = workbench_settings_database{ database };
		if (code != SQLITE_OK || database == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}

		if (auto configured = configure_connection(database); !configured) {
			return std::unexpected(configured.error());
		}
		if (auto established = establish_schema(database, aStop); !established) {
			return std::unexpected(established.error());
		}
		if (auto validated = validate(database); !validated) {
			return std::unexpected(validated.error());
		}
		return owned;
	}

	auto workbench_settings_database::handle() const noexcept -> ::sqlite3*
	{
		return Handle;
	}

	auto workbench_settings_database::store(
		const application::workbench_layout_settings& aSettings,
		std::stop_token                               aStop)
		-> std::expected<void, failure>
	{
		// Checked before the progress handler is installed, not only through
		// it. The handler runs every 256 virtual-machine steps, and this
		// statement finishes in fewer, so a caller that had already cancelled
		// would otherwise have its write performed anyway. `execute` guards the
		// same way for the same reason.
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		const auto guard = progress_scope{ Handle, aStop };

		auto statement = prepare(
			Handle,
			"INSERT INTO workbench_layout("
			"singleton, files_state, conversations_state, files_width,"
			"conversations_width) VALUES(1, ?1, ?2, ?3, ?4) "
			"ON CONFLICT(singleton) DO UPDATE SET "
			"files_state = excluded.files_state,"
			"conversations_state = excluded.conversations_state,"
			"files_width = excluded.files_width,"
			"conversations_width = excluded.conversations_width");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		if (::sqlite3_bind_int64(
				statement->Value, 1, static_cast<std::int64_t>(aSettings.Files))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   2,
				   static_cast<std::int64_t>(aSettings.Conversations))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   3,
				   static_cast<std::int64_t>(aSettings.FilesWidth))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   4,
				   static_cast<std::int64_t>(aSettings.ConversationsWidth))
				!= SQLITE_OK) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

		const auto code = ::sqlite3_step(statement->Value);
		if (code == SQLITE_INTERRUPT || aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		if (code != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

	auto workbench_settings_database::load(std::stop_token aStop)
		-> std::expected<application::workbench_layout_settings, failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		const auto guard = progress_scope{ Handle, aStop };

		auto statement = prepare(
			Handle,
			"SELECT files_state, conversations_state, files_width,"
			"conversations_width FROM workbench_layout WHERE singleton = 1");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		const auto stepped = ::sqlite3_step(statement->Value);
		if (stepped == SQLITE_INTERRUPT || aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		if (stepped == SQLITE_DONE) {
			// Nothing stored. The port's contract is that this is not a
			// failure and answers the defaults, so the defaults come from the
			// record's own declaration rather than from a second copy here.
			return application::workbench_layout_settings{ };
		}
		if (stepped != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

		const auto files = drawer_state_of(
			::sqlite3_column_int64(statement->Value, 0));
		if (!files) {
			return std::unexpected(files.error());
		}
		const auto conversations = drawer_state_of(
			::sqlite3_column_int64(statement->Value, 1));
		if (!conversations) {
			return std::unexpected(conversations.error());
		}

		return application::workbench_layout_settings{
			.Files         = *files,
			.Conversations = *conversations,
			.FilesWidth    = static_cast<std::uint32_t>(
				::sqlite3_column_int64(statement->Value, 2)),
			.ConversationsWidth = static_cast<std::uint32_t>(
				::sqlite3_column_int64(statement->Value, 3)),
		};
	}

	auto workbench_settings_columns(::sqlite3* const aDatabase)
		-> std::expected<std::vector<std::string>, failure>
	{
		auto tables = prepare(
			aDatabase,
			"SELECT name FROM sqlite_schema WHERE type = 'table' "
			"AND name NOT LIKE 'sqlite_%' ORDER BY name");
		if (!tables) {
			return std::unexpected(tables.error());
		}

		auto names = std::vector<std::string>{ };
		auto found = std::vector<std::string>{ };
		for (;;) {
			const auto stepped = ::sqlite3_step(tables->Value);
			if (stepped == SQLITE_DONE) {
				break;
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(failure::integrity_failure);
			}
			names.push_back(text_of(tables->Value, 0));
		}

		for (const auto& table : names) {
			// `PRAGMA table_info` rather than the `pragma_table_info`
			// table-valued function, because `prepare` refuses a virtual table
			// and that refusal is a policy worth keeping.
			auto columns = prepare(aDatabase, "PRAGMA table_info(" + table + ")");
			if (!columns) {
				return std::unexpected(columns.error());
			}
			for (;;) {
				const auto stepped = ::sqlite3_step(columns->Value);
				if (stepped == SQLITE_DONE) {
					break;
				}
				if (stepped != SQLITE_ROW) {
					return std::unexpected(failure::integrity_failure);
				}
				found.push_back(table + "." + text_of(columns->Value, 1));
			}
		}
		return found;
	}
} // namespace bpr::adapters::sqlite::detail
