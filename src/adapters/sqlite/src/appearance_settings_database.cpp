#include "appearance_settings_database.hpp"

#include "bpr/core/theme_exchange.hpp"

#include <sqlite3.h>

#include <array>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::adapters::sqlite::detail {
	namespace {
		// "BPRA". Distinct from the ledger's "BPRD", the queue's "BPRQ", and
		// the workbench's "BPRW", so that pointing one store at another's file
		// fails at the identity check rather than at the first query for a
		// table that is not there.
		constexpr std::int32_t AppearanceApplicationId = 0x42505241;

		/// \brief The statements that build schema version 1.
		///
		/// \details One row. There is no column for a repository, a path, a
		/// pull request, or any review content: an appearance is a reviewer's
		/// preference and this file must not become a record of what they read.
		///
		/// `palette_document` holds a `core::export_theme` document rather than
		/// decomposed colour components. See the header for why.
		constexpr std::array SchemaVersion1{
			std::string_view{
				"CREATE TABLE appearance_meta("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"schema_format INTEGER NOT NULL"
				") STRICT" },
			std::string_view{
				"CREATE TABLE appearance_settings("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"mode TEXT NOT NULL,"
				"palette_document TEXT NOT NULL,"
				"ui_family TEXT NOT NULL,"
				"ui_point_size INTEGER NOT NULL,"
				"ui_weight INTEGER NOT NULL,"
				"ui_line_height INTEGER NOT NULL,"
				"code_family TEXT NOT NULL,"
				"code_point_size INTEGER NOT NULL,"
				"code_weight INTEGER NOT NULL,"
				"code_tab_width INTEGER NOT NULL,"
				"code_ligatures INTEGER NOT NULL CHECK(code_ligatures IN (0, 1)),"
				"code_syntax_bold INTEGER NOT NULL CHECK(code_syntax_bold IN (0, 1)),"
				"code_syntax_italic INTEGER NOT NULL "
				"CHECK(code_syntax_italic IN (0, 1))"
				") STRICT" },
		};

		[[nodiscard]] auto run_all(
			::sqlite3*                              aDatabase,
			const std::span<const std::string_view> aStatements,
			std::stop_token                         aStop)
			-> std::expected<void, failure>
		{
			for (const auto sql : aStatements) {
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
			return execute(
				aDatabase, "PRAGMA user_version = " + std::to_string(aVersion), aStop);
		}

		[[nodiscard]] auto establish_schema(
			::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto current = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!current) {
				return std::unexpected(current.error());
			}
			if (*current == appearance_settings_schema_version) {
				return { };
			}
			// Forward only. A newer database is refused rather than opened,
			// because this build cannot know what the newer version added and
			// would silently drop it on the next write.
			if (*current > appearance_settings_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(
					aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};

			if (auto created = run_all(aDatabase, SchemaVersion1, aStop); !created) {
				rollback();
				return created;
			}
			if (auto app = execute(
					aDatabase,
					"PRAGMA application_id = "
						+ std::to_string(AppearanceApplicationId),
					aStop);
				!app) {
				rollback();
				return app;
			}
			if (auto meta = execute(
					aDatabase,
					"INSERT INTO appearance_meta(singleton, schema_format) "
					"VALUES(1, "
						+ std::to_string(appearance_settings_schema_version) + ")",
					aStop);
				!meta) {
				rollback();
				return meta;
			}
			if (auto stamped = set_version(
					aDatabase, appearance_settings_schema_version, aStop);
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
			if (!application || *application != AppearanceApplicationId || !version
				|| *version != appearance_settings_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto meta = prepare(
				aDatabase,
				"SELECT schema_format FROM appearance_meta WHERE singleton = 1");
			if (!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
				|| ::sqlite3_column_int64(meta->Value, 0)
					!= appearance_settings_schema_version) {
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

		/// \brief Decodes one stored mode name against the published vocabulary.
		///
		/// \details Compared against `core::stable_name` for every declared mode
		/// rather than against a list restated here, so a mode added to the core
		/// is storable without editing this function, and a name the core does
		/// not publish is refused rather than mapped to whatever sits at that
		/// ordinal.
		[[nodiscard]] auto mode_of(const std::string_view aName)
			-> std::expected<core::appearance_mode, failure>
		{
			for (std::size_t index = 0U; index < core::appearance_mode_count;
				 ++index) {
				const auto candidate
					= static_cast<core::appearance_mode>(
						static_cast<std::uint8_t>(index));
				if (core::stable_name(candidate) == aName) {
					return candidate;
				}
			}
			return std::unexpected(failure::integrity_failure);
		}

		[[nodiscard]] auto bounded(
			const std::int64_t aValue) -> std::expected<std::uint32_t, failure>
		{
			if (aValue < 0
				|| aValue > static_cast<std::int64_t>(
					   std::numeric_limits<std::uint32_t>::max())) {
				return std::unexpected(failure::integrity_failure);
			}
			return static_cast<std::uint32_t>(aValue);
		}
	} // namespace

	appearance_settings_database::appearance_settings_database(
		::sqlite3* const aHandle) noexcept
		: Handle{ aHandle }
	{
	}

	appearance_settings_database::appearance_settings_database(
		appearance_settings_database&& aOther) noexcept
		: Handle{ std::exchange(aOther.Handle, nullptr) }
	{
	}

	auto appearance_settings_database::operator=(
		appearance_settings_database&& aOther) noexcept
		-> appearance_settings_database&
	{
		if (this != &aOther) {
			if (Handle != nullptr) {
				(void)::sqlite3_close_v2(Handle);
			}
			Handle = std::exchange(aOther.Handle, nullptr);
		}
		return *this;
	}

	appearance_settings_database::~appearance_settings_database()
	{
		if (Handle != nullptr) {
			(void)::sqlite3_close_v2(Handle);
			Handle = nullptr;
		}
	}

	auto appearance_settings_database::open(
		const std::filesystem::path& aPath, std::stop_token aStop)
		-> std::expected<appearance_settings_database, failure>
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
		auto owned = appearance_settings_database{ database };
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

	auto appearance_settings_database::handle() const noexcept -> ::sqlite3*
	{
		return Handle;
	}

	auto appearance_settings_database::store(
		const application::appearance_settings& aSettings, std::stop_token aStop)
		-> std::expected<void, failure>
	{
		// Checked before any work, not only through the progress handler: that
		// handler runs every 256 virtual-machine steps and this statement
		// finishes in fewer, so a caller who had already cancelled would
		// otherwise have their write performed and then be told it was not.
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		const auto guard = progress_scope{ Handle, aStop };

		const auto document = core::export_theme(aSettings.Palette);
		const auto mode     = std::string{ core::stable_name(aSettings.Mode) };

		auto statement = prepare(
			Handle,
			"INSERT INTO appearance_settings("
			"singleton, mode, palette_document, ui_family, ui_point_size,"
			"ui_weight, ui_line_height, code_family, code_point_size,"
			"code_weight, code_tab_width, code_ligatures, code_syntax_bold,"
			"code_syntax_italic) "
			"VALUES(1, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13) "
			"ON CONFLICT(singleton) DO UPDATE SET "
			"mode = excluded.mode,"
			"palette_document = excluded.palette_document,"
			"ui_family = excluded.ui_family,"
			"ui_point_size = excluded.ui_point_size,"
			"ui_weight = excluded.ui_weight,"
			"ui_line_height = excluded.ui_line_height,"
			"code_family = excluded.code_family,"
			"code_point_size = excluded.code_point_size,"
			"code_weight = excluded.code_weight,"
			"code_tab_width = excluded.code_tab_width,"
			"code_ligatures = excluded.code_ligatures,"
			"code_syntax_bold = excluded.code_syntax_bold,"
			"code_syntax_italic = excluded.code_syntax_italic");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		const auto& ui   = aSettings.Typography.Ui;
		const auto& code = aSettings.Typography.Code;
		if (::sqlite3_bind_text(
				statement->Value, 1, mode.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value, 2, document.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value, 3, ui.Font.Family.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 4, ui.Font.PointSizeCentipoints)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 5, ui.Font.Weight) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 6, ui.LineHeightPercent)
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value,
				   7,
				   code.Font.Family.c_str(),
				   -1,
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value, 8, code.Font.PointSizeCentipoints)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 9, code.Font.Weight)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 10, code.TabWidth) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 11, code.Ligatures ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 12, code.SyntaxBold ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 13, code.SyntaxItalic ? 1 : 0)
				!= SQLITE_OK) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

		const auto stepped = ::sqlite3_step(statement->Value);
		if (stepped == SQLITE_INTERRUPT || aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		if (stepped != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

	auto appearance_settings_database::load(std::stop_token aStop)
		-> std::expected<
			std::optional<application::appearance_settings>,
			failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		const auto guard = progress_scope{ Handle, aStop };

		auto statement = prepare(
			Handle,
			"SELECT mode, palette_document, ui_family, ui_point_size, ui_weight,"
			"ui_line_height, code_family, code_point_size, code_weight,"
			"code_tab_width, code_ligatures, code_syntax_bold,"
			"code_syntax_italic FROM appearance_settings WHERE singleton = 1");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		const auto stepped = ::sqlite3_step(statement->Value);
		if (stepped == SQLITE_INTERRUPT || aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		if (stepped == SQLITE_DONE) {
			// Nothing stored. Reported as an absent value rather than as the
			// defaults, so a fresh installation stays distinguishable from one
			// that deliberately stored them.
			return std::optional<application::appearance_settings>{ };
		}
		if (stepped != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

		const auto mode = mode_of(text_of(statement->Value, 0));
		if (!mode) {
			return std::unexpected(mode.error());
		}

		// Read back through the core's own importer, which is what refuses a
		// tampered document. A store that parsed colours itself would have to
		// restate every one of those refusals.
		const auto imported = core::import_theme(text_of(statement->Value, 1));
		if (!imported.Palette) {
			return std::unexpected(failure::integrity_failure);
		}

		auto settings    = application::appearance_settings{ };
		settings.Mode    = *mode;
		settings.Palette = *imported.Palette;

		const auto uiSize     = bounded(::sqlite3_column_int64(statement->Value, 3));
		const auto uiWeight   = bounded(::sqlite3_column_int64(statement->Value, 4));
		const auto uiHeight   = bounded(::sqlite3_column_int64(statement->Value, 5));
		const auto codeSize   = bounded(::sqlite3_column_int64(statement->Value, 7));
		const auto codeWeight = bounded(::sqlite3_column_int64(statement->Value, 8));
		const auto codeTab    = bounded(::sqlite3_column_int64(statement->Value, 9));
		if (!uiSize || !uiWeight || !uiHeight || !codeSize || !codeWeight
			|| !codeTab) {
			return std::unexpected(failure::integrity_failure);
		}

		settings.Typography.Ui.Font.Family                 = text_of(statement->Value, 2);
		settings.Typography.Ui.Font.PointSizeCentipoints   = *uiSize;
		settings.Typography.Ui.Font.Weight                 = *uiWeight;
		settings.Typography.Ui.LineHeightPercent           = *uiHeight;
		settings.Typography.Code.Font.Family               = text_of(statement->Value, 6);
		settings.Typography.Code.Font.PointSizeCentipoints = *codeSize;
		settings.Typography.Code.Font.Weight               = *codeWeight;
		settings.Typography.Code.TabWidth                  = *codeTab;
		settings.Typography.Code.Ligatures
			= ::sqlite3_column_int64(statement->Value, 10) != 0;
		settings.Typography.Code.SyntaxBold
			= ::sqlite3_column_int64(statement->Value, 11) != 0;
		settings.Typography.Code.SyntaxItalic
			= ::sqlite3_column_int64(statement->Value, 12) != 0;

		// A durable record can be written by another build or edited by hand.
		// The core is the authority on typography legality, so a record it
		// rejects is refused whole here rather than adopted and then quietly
		// repaired somewhere further in.
		if (!core::validate_typography(settings.Typography).empty()) {
			return std::unexpected(failure::integrity_failure);
		}
		return std::optional{ settings };
	}

	auto appearance_settings_columns(::sqlite3* const aDatabase)
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
