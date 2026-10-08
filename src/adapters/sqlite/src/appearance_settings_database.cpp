#include "appearance_settings_database.hpp"

#include "storage_internal.hpp"

#include "bpr/core/theme_exchange.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::sqlite::detail {
	namespace {
		// "BPRA". Distinct from the ledger's "BPRD", the queue's "BPRQ", and
		// the workbench's "BPRW", so that pointing one store at another's file
		// fails at the identity check rather than at the first query for a
		// table that is not there.
		constexpr std::int32_t AppearanceApplicationId = 0x42505241;

		constexpr auto OpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
			| SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE;

		/// \brief The statements that build schema version 3.
		///
		/// \details One row. There is no column for a repository, a path, a
		/// pull request, or any review content: an appearance is a reviewer's
		/// preference and this file must not become a record of what they read.
		///
		/// `palette_document` holds a `core::export_theme` document rather than
		/// decomposed colour components. See the header for why.
		///
		/// `palette_variant` names which built-in profile the stored palette
		/// customises. Version 1 had no such column, and a reader with only a
		/// palette can do nothing but assume the variant the current desktop
		/// resolves, which turned a light palette into forty-six overrides
		/// pinned over the dark profile.
		constexpr std::array SchemaVersion3{
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
				"palette_variant TEXT NOT NULL,"
				"palette_stored INTEGER NOT NULL CHECK(palette_stored IN (0, 1)),"
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

		/// \brief Returns the variant at one ordinal of the closed vocabulary.
		[[nodiscard]] constexpr auto variant_at(const std::size_t aIndex) noexcept
			-> core::appearance_variant
		{
			return static_cast<core::appearance_variant>(
				static_cast<std::uint8_t>(aIndex));
		}

		/// \brief Returns the column one variant's palette document is stored in.
		///
		/// \details Named from the core's own stable identifier rather than from a
		/// list restated here, so the closed variant vocabulary stays the single
		/// authority on what a theme is called. A variant added to that vocabulary
		/// therefore needs a schema version of its own rather than silently landing
		/// in another variant's column.
		[[nodiscard]] auto variant_document_column(
			const core::appearance_variant aVariant) -> std::string
		{
			return std::string{ core::stable_name(aVariant) } + "_palette_document";
		}

		/// \brief Returns the column one variant's stored flag is kept in.
		[[nodiscard]] auto variant_stored_column(
			const core::appearance_variant aVariant) -> std::string
		{
			return std::string{ core::stable_name(aVariant) } + "_palette_stored";
		}

		/// \brief The columns that carry no palette, in their declared order.
		constexpr std::array<std::string_view, 13> ScalarColumnNames{
			"ui_family", "ui_point_size", "ui_weight", "ui_line_height",
			"code_family", "code_point_size", "code_weight", "code_tab_width",
			"code_ligatures", "code_syntax_bold", "code_syntax_italic",
			"dif_visibility", "motion_mode"
		};

		/// \brief [[BPR-1107]]. The one column version 8 adds, last.
		constexpr std::string_view WhiteSpaceColumn = "code_white_space";

		/// \brief What a version-7 record carries into version 8: the default a
		/// fresh installation has, Selection, which is Sublime Text's own.
		constexpr auto MigratedWhiteSpace = core::code_font_settings{ }.WhiteSpace;

		/// \brief [[BPR-1336]]. The one column version 9 adds, last.
		constexpr std::string_view CodeZoomColumn = "code_zoom_percent";

		/// \brief What a version-8 record carries into version 9: Actual Size,
		/// so a reviewer who never zoomed sees the code at the size they chose.
		constexpr auto MigratedCodeZoom = core::code_font_settings{ }.ZoomPercent;

		/// \brief Returns every column the settings table declares, in order.
		///
		/// \details `aVersion` names a version from 7 on: version 7's shape
		/// has neither trailing column, 8 adds white space and 9 the zoom. Only
		/// the migration steps and the admission check of an older file ask for
		/// an older shape.
		[[nodiscard]] auto settings_column_names(const std::int64_t aVersion = appearance_settings_schema_version) -> std::vector<std::string>
		{
			auto names = std::vector<std::string>{ "singleton", "mode" };
			for (std::size_t index = 0U; index < core::appearance_variant_count;
				 ++index) {
				names.push_back(variant_document_column(variant_at(index)));
				names.push_back(variant_stored_column(variant_at(index)));
			}
			for (const auto scalar : ScalarColumnNames) {
				names.emplace_back(scalar);
			}
			if (aVersion >= 8) {
				names.emplace_back(WhiteSpaceColumn);
			}
			if (aVersion >= 9) {
				names.emplace_back(CodeZoomColumn);
			}
			return names;
		}

		[[nodiscard]] auto joined(
			const std::vector<std::string>& aNames, const std::string_view aSeparator)
			-> std::string
		{
			auto joinedNames = std::string{ };
			for (const auto& name : aNames) {
				if (!joinedNames.empty()) {
					joinedNames += aSeparator;
				}
				joinedNames += name;
			}
			return joinedNames;
		}

		/// \brief Returns the statement that builds the current settings table.
		///
		/// \details One row, as every earlier version had. The palette columns are
		/// generated so their names come from the core vocabulary; the rest is
		/// written out, so the CHECK constraints stay readable rather than being
		/// assembled. `validate` compares the created table against
		/// `settings_column_names` on every open, so the two cannot drift apart
		/// without the next open refusing the file.
		[[nodiscard]] auto settings_schema(const std::int64_t aVersion = appearance_settings_schema_version) -> std::string
		{
			auto sql = std::string{
				"CREATE TABLE appearance_settings("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"mode TEXT NOT NULL,"
			};
			for (std::size_t index = 0U; index < core::appearance_variant_count;
				 ++index) {
				const auto stored = variant_stored_column(variant_at(index));
				sql += variant_document_column(variant_at(index)) + " TEXT NOT NULL,";
				sql += stored + " INTEGER NOT NULL CHECK(" + stored + " IN (0, 1)),";
			}
			sql += "ui_family TEXT NOT NULL,"
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
				   "CHECK(code_syntax_italic IN (0, 1)),"
				   "dif_visibility INTEGER NOT NULL CHECK(dif_visibility IN (0, 1)),"
				   "motion_mode INTEGER NOT NULL CHECK(motion_mode IN (0, 1, 2))";
			if (aVersion >= 8) {
				sql += "," + std::string{ WhiteSpaceColumn } + " INTEGER NOT NULL CHECK("
					+ std::string{ WhiteSpaceColumn } + " BETWEEN 0 AND "
					+ std::to_string(core::white_space_drawing_count - 1U) + ")";
			}
			if (aVersion >= 9) {
				const auto bounds = core::bounds_of(core::typography_field::code_zoom);
				sql += "," + std::string{ CodeZoomColumn } + " INTEGER NOT NULL CHECK("
					+ std::string{ CodeZoomColumn } + " BETWEEN " + std::to_string(bounds.Minimum)
					+ " AND " + std::to_string(bounds.Maximum) + ")";
			}
			sql += ") STRICT";
			return sql;
		}

		/// \brief One record in the shape versions 1 to 6 stored it.
		///
		/// \details Kept as its own type rather than reusing the durable record,
		/// because the durable record no longer has a place to put a single palette
		/// and the variant it belonged to. This is read only to admit or refuse a
		/// legacy row on the way through a migration; nothing outside this file
		/// ever sees it.
		struct legacy_appearance_record
		{
			core::appearance_mode     Mode = core::appearance_mode::system;
			core::appearance_palette  Palette{ };
			core::appearance_variant  Variant = core::appearance_variant::light;
			bool                      Stored  = false;
			core::typography_settings Typography;
			core::dif_preferences     Dif;
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

		/// \brief Decodes one stored variant name against the core vocabulary.
		///
		/// \details Resolved through the core's own lookup for the same reason
		/// `mode_of` is: a name the core does not publish names no variant, and
		/// is refused rather than mapped onto whatever sits at some ordinal.
		[[nodiscard]] auto variant_of(const std::string_view aName)
			-> std::expected<core::appearance_variant, failure>
		{
			if (const auto found = core::find_appearance_variant(aName)) {
				return *found;
			}
			return std::unexpected(failure::integrity_failure);
		}

		[[nodiscard]] auto create_schema(
			::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			if (auto created = run_all(aDatabase, SchemaVersion3, aStop); !created) {
				return created;
			}
			if (auto app = execute(
					aDatabase,
					"PRAGMA application_id = "
						+ std::to_string(AppearanceApplicationId),
					aStop);
				!app) {
				return app;
			}
			return execute(
				aDatabase,
				"INSERT INTO appearance_meta(singleton, schema_format) "
				"VALUES(1, "
					+ std::to_string(appearance_settings_schema_version) + ")",
				aStop);
		}

		/// \brief Preserves legacy customization and supplies the missing v1 variant.
		///
		/// \details The derived value is the variant the version-1 reader
		/// already assumed, which is what `resolve_appearance` answers for the
		/// stored mode under an empty observation. Deriving it here rather than
		/// in SQL keeps the one authority on what a mode means in the core: SQL
		/// restating the mapping would be a second copy of a rule, and the
		/// second copy is the one that goes stale.
		///
		/// An unrecognised stored mode leaves the column at its `light`
		/// default rather than failing the open. The load path is the authority
		/// that refuses such a record, and it still does; a migration that
		/// refused instead would turn a reported integrity failure into a
		/// database that cannot be opened at all.
		[[nodiscard]] auto migrate_to_version_3(
			::sqlite3* aDatabase, const std::int64_t aVersion, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			// Rebuilt rather than altered. ALTER TABLE ADD COLUMN on a STRICT
			// table runs SQLite's quick check through the pragma table-valued
			// function, and the hardened connection policy refuses that
			// function, so the row is carried across a fresh current-format table.
			const auto columns = std::string{ "singleton, mode, palette_document, ui_family, ui_point_size,"
											  " ui_weight, ui_line_height, code_family, code_point_size,"
											  " code_weight, code_tab_width, code_ligatures, code_syntax_bold,"
											  " code_syntax_italic" }
				+ (aVersion == 2 ? ", palette_variant" : "");
			const auto rebuild = std::array{
				std::string{ "CREATE TABLE appearance_settings_previous AS SELECT " }
					+ columns + " FROM appearance_settings",
				std::string{ "DROP TABLE appearance_settings" },
				std::string{ SchemaVersion3[1] },
				std::string{ "INSERT INTO appearance_settings(singleton, mode,"
							 " palette_document, palette_variant, palette_stored, ui_family,"
							 " ui_point_size, ui_weight, ui_line_height,"
							 " code_family, code_point_size, code_weight,"
							 " code_tab_width, code_ligatures, code_syntax_bold,"
							 " code_syntax_italic) SELECT singleton, mode,"
							 " palette_document, " }
					+ (aVersion == 2 ? "palette_variant" : "'light'") + std::string{ ", 1, ui_family,"
																					 " ui_point_size, ui_weight, ui_line_height,"
																					 " code_family, code_point_size, code_weight,"
																					 " code_tab_width, code_ligatures, code_syntax_bold,"
																					 " code_syntax_italic FROM appearance_settings_previous" },
				std::string{ "DROP TABLE appearance_settings_previous" },
			};
			for (const auto& statement : rebuild) {
				if (auto ran = execute(aDatabase, statement, aStop); !ran) {
					return ran;
				}
			}

			if (aVersion == 2) {
				return { };
			}

			auto reader = prepare(
				aDatabase,
				"SELECT mode FROM appearance_settings WHERE singleton = 1");
			if (!reader) {
				return std::unexpected(reader.error());
			}
			const auto stepped = ::sqlite3_step(reader->Value);
			if (stepped == SQLITE_DONE) {
				// No record to carry forward. The column exists for the next
				// write, which is all a fresh-but-versioned file needs.
				return { };
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			const auto stored = mode_of(text_of(reader->Value, 0));
			if (!stored) {
				return { };
			}

			const auto derived = std::string{ core::stable_name(
				core::resolve_appearance(*stored, core::appearance_observation{ })
					.Variant) };
			auto       writer  = prepare(
				aDatabase,
				"UPDATE appearance_settings SET palette_variant = ?1 "
				"WHERE singleton = 1");
			if (!writer) {
				return std::unexpected(writer.error());
			}
			if (::sqlite3_bind_text(
					writer->Value, 1, derived.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			if (::sqlite3_step(writer->Value) != SQLITE_DONE) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			return { };
		}

		/// \brief Brings a version 1, 2 or 3 file to the current format.
		///
		/// \details Version 4 declares the version-3 columns unchanged. What
		/// it does is reset: the built-in dark palette moved to the Visual
		/// Studio dark / Visual Assist scheme ([[BPR-0720]]) and the owner
		/// asked for the saved customisation of the old defaults to be
		/// discarded rather than carried over it, so `palette_stored` is
		/// cleared once on the way through. The document stays in the row
		/// (the reader still imports it, so it must stay a complete
		/// document) and the next store writes whatever the user chooses
		/// next.
		///
		/// A version-3 row is carried across a fresh table exactly as the
		/// older versions are, so its CHECK constraints are verified again
		/// and a tampered legacy value is refused at the migration rather
		/// than admitted at open and refused at load.
		[[nodiscard]] auto migrate_to_version_4(
			::sqlite3* aDatabase, const std::int64_t aVersion, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			if (aVersion < 3) {
				if (auto carried = migrate_to_version_3(aDatabase, aVersion, aStop); !carried) {
					return carried;
				}
			} else {
				constexpr auto columns = std::string_view{
					"singleton, mode, palette_document, palette_variant, palette_stored,"
					" ui_family, ui_point_size, ui_weight, ui_line_height, code_family,"
					" code_point_size, code_weight, code_tab_width, code_ligatures,"
					" code_syntax_bold, code_syntax_italic"
				};
				const auto rebuild = std::array{
					std::string{ "CREATE TABLE appearance_settings_previous AS SELECT " }
						+ std::string{ columns } + " FROM appearance_settings",
					std::string{ "DROP TABLE appearance_settings" },
					std::string{ SchemaVersion3[1] },
					std::string{ "INSERT INTO appearance_settings(" } + std::string{ columns }
						+ ") SELECT " + std::string{ columns }
						+ " FROM appearance_settings_previous",
					std::string{ "DROP TABLE appearance_settings_previous" },
				};
				for (const auto& statement : rebuild) {
					if (auto ran = execute(aDatabase, statement, aStop); !ran) {
						return ran;
					}
				}
			}
			return execute(
				aDatabase,
				"UPDATE appearance_settings SET palette_stored = 0 WHERE singleton = 1",
				aStop);
		}

		/// \brief Brings a version 1 to 4 file to the current format.
		///
		/// \details Version 5 declares the version-3 columns unchanged. The
		/// vocabulary grew a `syntax_macro` role ([[BPR-0738]]); the stored
		/// palette document is complete by construction and the core's
		/// importer refuses one that omits a role, so a version-4 document
		/// is given the row it lacks. Its value is the document's own
		/// `syntax_preprocessor` colour, which is what macros were painted
		/// with before the role existed, so nothing the user sees changes
		/// and `palette_stored` is left exactly as it was: no second reset.
		[[nodiscard]] auto migrate_to_version_5(
			::sqlite3* aDatabase, const std::int64_t aVersion, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			if (aVersion < 4) {
				if (auto carried = migrate_to_version_4(aDatabase, aVersion, aStop); !carried) {
					return carried;
				}
			}
			auto reader = prepare(
				aDatabase,
				"SELECT palette_document FROM appearance_settings WHERE singleton = 1");
			if (!reader) {
				return std::unexpected(reader.error());
			}
			const auto stepped = ::sqlite3_step(reader->Value);
			if (stepped == SQLITE_DONE) {
				return { };
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			const auto     document         = std::string{ text_of(reader->Value, 0) };
			constexpr auto macroName        = std::string_view{ "syntax_macro " };
			constexpr auto preprocessorName = std::string_view{ "syntax_preprocessor " };
			const auto     lineStarting     = [&document](const std::string_view aName) {
				auto at = document.find(aName);
				while (at != std::string::npos && at != 0U && document[at - 1U] != '\n') {
					at = document.find(aName, at + 1U);
				}
				return at;
			};
			if (lineStarting(macroName) != std::string::npos) {
				return { };
			}
			const auto preprocessor = lineStarting(preprocessorName);
			if (preprocessor == std::string::npos) {
				// Not a document this store wrote; the reader refuses it as
				// it always has, and the migration does not invent a value.
				return { };
			}
			const auto valueBegin = preprocessor + preprocessorName.size();
			const auto valueEnd   = std::min(document.find('\n', valueBegin), document.size());
			auto       amended    = document;
			if (amended.empty() || amended.back() != '\n') {
				amended.push_back('\n');
			}
			amended.append(macroName);
			amended.append(document, valueBegin, valueEnd - valueBegin);
			amended.push_back('\n');
			auto writer = prepare(
				aDatabase,
				"UPDATE appearance_settings SET palette_document = ?1 WHERE singleton = 1");
			if (!writer) {
				return std::unexpected(writer.error());
			}
			if (::sqlite3_bind_text(writer->Value, 1, amended.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			if (::sqlite3_step(writer->Value) != SQLITE_DONE) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			return { };
		}

		[[nodiscard]] auto legacy_columns_match(
			::sqlite3* aDatabase, const std::string_view aTable,
			const std::span<const std::string_view> aNames) -> bool
		{
			// Use the PRAGMA statement, as sibling stores do: the hardened
			// connection does not admit pragma table-valued functions.
			auto columns = prepare(aDatabase, "PRAGMA table_xinfo(" + std::string{ aTable } + ")");
			if (!columns) {
				return false;
			}
			std::size_t count = 0;
			for (auto step = ::sqlite3_step(columns->Value);; step = ::sqlite3_step(columns->Value)) {
				if (step == SQLITE_DONE) {
					return count == aNames.size();
				}
				if (step != SQLITE_ROW) {
					return false;
				}
				const auto name = text_of(columns->Value, 1);
				const auto text = name == "mode" || name == "palette_document"
					|| name == "palette_variant" || name == "ui_family" || name == "code_family"
					|| name.ends_with("_palette_document");
				const auto singleton = name == "singleton";
				if (std::ranges::find(aNames, name) == aNames.end()
					|| text_of(columns->Value, 2) != (text ? "TEXT" : "INTEGER")
					|| ::sqlite3_column_int(columns->Value, 3) != (singleton ? 0 : 1)
					|| ::sqlite3_column_int(columns->Value, 5) != (singleton ? 1 : 0)
					|| ::sqlite3_column_int(columns->Value, 6) != 0) {
					return false;
				}
				++count;
			}
		}

		[[nodiscard]] auto validate_legacy(::sqlite3* aDatabase, const std::int64_t aVersion)
			-> std::expected<void, failure>
		{
			constexpr std::array<std::string_view, 2>  metaNames{ "singleton", "schema_format" };
			constexpr std::array<std::string_view, 18> legacyNames{
				"singleton", "mode", "palette_document", "ui_family", "ui_point_size",
				"ui_weight", "ui_line_height", "code_family", "code_point_size",
				"code_weight", "code_tab_width", "code_ligatures", "code_syntax_bold",
				"code_syntax_italic", "palette_variant", "palette_stored", "dif_visibility", "motion_mode"
			};
			// Version 1 had neither trailing column, version 2 the variant,
			// versions 3 to 5 both. Version 6 adds the two Dif columns. Version 7
			// is a different set rather than a longer prefix of the same one: it
			// drops the three single-palette columns, so it carries its own list.
			const auto columnCount = aVersion == 1 ? 14U : aVersion == 2 ? 15U
				: aVersion >= 6                                          ? 18U
																		 : 16U;
			// Versions 7 to 9 carry a column per variant; 8 adds white space and
			// 9 the comparison's zoom.
			const auto current       = settings_column_names(aVersion);
			auto       settingsNames = std::vector<std::string_view>{ };
			if (aVersion >= 7) {
				for (const auto& name : current) {
					settingsNames.emplace_back(name);
				}
			} else {
				for (const auto name :
					 std::span{ legacyNames }.first(columnCount)) {
					settingsNames.push_back(name);
				}
			}
			const auto objects = scalar_integer(aDatabase,
												"SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'");
			if ((aVersion < 1 || aVersion > appearance_settings_schema_version)
				|| !objects || *objects != 2
				|| !legacy_columns_match(aDatabase, "appearance_meta", metaNames)
				|| !legacy_columns_match(aDatabase, "appearance_settings",
										 std::span{ settingsNames })) {
				return std::unexpected(failure::schema_mismatch);
			}
			for (const auto table : { "appearance_meta", "appearance_settings" }) {
				auto shape = prepare(aDatabase, "PRAGMA table_list(" + std::string{ table } + ")");
				if (!shape || ::sqlite3_step(shape->Value) != SQLITE_ROW
					|| text_of(shape->Value, 2) != "table"
					|| ::sqlite3_column_int(shape->Value, 4) != 0
					|| ::sqlite3_column_int(shape->Value, 5) != 1
					|| ::sqlite3_step(shape->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
			}
			auto meta = prepare(aDatabase, "SELECT singleton, schema_format FROM appearance_meta");
			if (!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
				|| ::sqlite3_column_int64(meta->Value, 0) != 1
				|| ::sqlite3_column_int64(meta->Value, 1) != aVersion
				|| ::sqlite3_step(meta->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}

		[[nodiscard]] auto read_legacy_settings(
			::sqlite3* aDatabase, std::stop_token aStop, bool aHasDif)
			-> std::expected<std::optional<legacy_appearance_record>, failure>;

		/// \brief Adds profile preferences only after the version-5 values are admitted.
		[[nodiscard]] auto migrate_to_version_6(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto rows = scalar_integer(aDatabase, "SELECT count(*) FROM appearance_settings WHERE singleton != 1");
			if (!rows || *rows != 0) {
				return std::unexpected(failure::integrity_failure);
			}
			if (const auto settings = read_legacy_settings(aDatabase, aStop, false);
				!settings) {
				return std::unexpected(settings.error());
			}
			auto schema = std::string{ SchemaVersion3[1] };
			schema.insert(schema.rfind(") STRICT"),
						  ",dif_visibility INTEGER NOT NULL CHECK(dif_visibility IN (0, 1)),"
						  "motion_mode INTEGER NOT NULL CHECK(motion_mode IN (0, 1, 2))");
			const auto statements = std::array{
				std::string{ "CREATE TABLE appearance_settings_previous AS SELECT * FROM appearance_settings" },
				std::string{ "DROP TABLE appearance_settings" },
				schema,
				std::string{ "INSERT INTO appearance_settings SELECT singleton, mode, palette_document, palette_variant, palette_stored,"
							 "ui_family, ui_point_size, ui_weight, ui_line_height, code_family, code_point_size, code_weight,"
							 "code_tab_width, code_ligatures, code_syntax_bold, code_syntax_italic, 0, 0 FROM appearance_settings_previous" },
				std::string{ "DROP TABLE appearance_settings_previous" },
			};
			for (const auto& statement : statements) {
				if (auto ran = execute(aDatabase, statement, aStop); !ran) {
					return ran;
				}
			}
			return { };
		}

		/// \brief Files the one stored palette under the variant it names.
		///
		/// \details RFC-BPR-0034 gives every theme its own colours, so version 7
		/// replaces the single `palette_document` / `palette_variant` /
		/// `palette_stored` triple with a document and a stored flag per variant.
		/// The stored palette lands in exactly the slot its recorded variant names
		/// and keeps its exact document bytes. The other three slots carry the
		/// empty palette's document with their flag clear, which is the same "no
		/// customization" a fresh installation has, so a reviewer who had tuned
		/// Light finds Light tuned and Dark, High Contrast Light and High Contrast
		/// Dark built-in. A record whose palette was not stored carries no
		/// customization at all, exactly as it did before.
		///
		/// Every slot holds a *complete* document rather than empty text, because
		/// the reader imports each one through the core parser and that parser
		/// refuses a document that fails to assign every role. The stored flag, not
		/// the document, is what says whether a slot is a decision.
		///
		/// The row is carried across a fresh table, so its CHECK constraints are
		/// verified again on the way through, and the legacy values are admitted by
		/// the version-6 reader first, so a tampered document, an unpublished
		/// variant name, an out-of-range flag or illegal typography refuses the
		/// migration rather than being repaired into the new shape. The variant
		/// names travel as bound parameters rather than as interpolated SQL.
		[[nodiscard]] auto migrate_to_version_7(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto rows = scalar_integer(aDatabase, "SELECT count(*) FROM appearance_settings WHERE singleton != 1");
			if (!rows || *rows != 0) {
				return std::unexpected(failure::integrity_failure);
			}
			if (const auto admitted = read_legacy_settings(aDatabase, aStop, true);
				!admitted) {
				return std::unexpected(admitted.error());
			}

			const auto empty = core::export_theme(core::appearance_palette{ });
			auto       names = std::vector<std::string>{ };
			auto       carry = std::string{ "INSERT INTO appearance_settings(" }
				+ joined(settings_column_names(7), ", ") + ") SELECT singleton, mode";
			for (std::size_t index = 0U; index < core::appearance_variant_count;
				 ++index) {
				names.emplace_back(core::stable_name(variant_at(index)));
				const auto matched = std::string{ "palette_stored = 1 AND palette_variant = ?" }
					+ std::to_string(index + 2U);
				carry += ", CASE WHEN " + matched + " THEN palette_document ELSE ?1 END";
				carry += ", CASE WHEN " + matched + " THEN 1 ELSE 0 END";
			}
			for (const auto scalar : ScalarColumnNames) {
				carry += ", " + std::string{ scalar };
			}
			carry += " FROM appearance_settings_previous";

			for (const auto& statement : std::array{
					 std::string{ "CREATE TABLE appearance_settings_previous AS SELECT * FROM appearance_settings" },
					 std::string{ "DROP TABLE appearance_settings" },
					 settings_schema(7) }) {
				if (auto ran = execute(aDatabase, statement, aStop); !ran) {
					return ran;
				}
			}

			auto writer = prepare(aDatabase, carry);
			if (!writer) {
				return std::unexpected(writer.error());
			}
			if (::sqlite3_bind_text(
					writer->Value, 1, empty.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			for (std::size_t index = 0U; index < names.size(); ++index) {
				if (::sqlite3_bind_text(
						writer->Value,
						static_cast<int>(index + 2U),
						names[index].c_str(),
						-1,
						SQLITE_TRANSIENT)
					!= SQLITE_OK) {
					return std::unexpected(catalog_sql_failure(aDatabase));
				}
			}
			if (::sqlite3_step(writer->Value) != SQLITE_DONE) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
			return execute(
				aDatabase, "DROP TABLE appearance_settings_previous", aStop);
		}

		/// \brief [[BPR-1107]]. Adds when the code panes draw white space.
		///
		/// \details The row is carried across a fresh table, as every earlier
		/// step carries it, so its CHECK constraints are verified again on the way
		/// through. The new column takes the default a fresh installation has, so
		/// a reviewer who never chose sees what a new reviewer sees.
		[[nodiscard]] auto migrate_to_version_8(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto rows = scalar_integer(aDatabase, "SELECT count(*) FROM appearance_settings WHERE singleton != 1");
			if (!rows || *rows != 0) {
				return std::unexpected(failure::integrity_failure);
			}
			const auto previous = joined(settings_column_names(7), ", ");
			auto       carry    = std::string{ "INSERT INTO appearance_settings(" };
			carry += previous;
			carry += ", ";
			carry += WhiteSpaceColumn;
			carry += ") SELECT ";
			carry += previous;
			carry += ", ";
			carry += std::to_string(static_cast<int>(MigratedWhiteSpace));
			carry += " FROM appearance_settings_previous";
			for (const auto& statement : std::array{
					 std::string{ "CREATE TABLE appearance_settings_previous AS SELECT * FROM appearance_settings" },
					 std::string{ "DROP TABLE appearance_settings" },
					 settings_schema(8),
					 carry,
					 std::string{ "DROP TABLE appearance_settings_previous" } }) {
				if (auto ran = execute(aDatabase, statement, aStop); !ran) {
					return ran;
				}
			}
			return { };
		}

		/// \brief [[BPR-1336]]. Adds the comparison's zoom.
		///
		/// \details Carried across a fresh table as the version-8 step carries
		/// its row, so the CHECK constraints are verified on the way through.
		/// The new column takes Actual Size.
		[[nodiscard]] auto migrate_to_version_9(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto rows = scalar_integer(aDatabase, "SELECT count(*) FROM appearance_settings WHERE singleton != 1");
			if (!rows || *rows != 0) {
				return std::unexpected(failure::integrity_failure);
			}
			const auto previous = joined(settings_column_names(8), ", ");
			auto       carry    = std::string{ "INSERT INTO appearance_settings(" };
			carry += previous;
			carry += ", ";
			carry += CodeZoomColumn;
			carry += ") SELECT ";
			carry += previous;
			carry += ", ";
			carry += std::to_string(MigratedCodeZoom);
			carry += " FROM appearance_settings_previous";
			for (const auto& statement : std::array{
					 std::string{ "CREATE TABLE appearance_settings_previous AS SELECT * FROM appearance_settings" },
					 std::string{ "DROP TABLE appearance_settings" },
					 settings_schema(9),
					 carry,
					 std::string{ "DROP TABLE appearance_settings_previous" } }) {
				if (auto ran = execute(aDatabase, statement, aStop); !ran) {
					return ran;
				}
			}
			return { };
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
			// A stamped file is about to be altered in place, so its identity
			// is checked before the first write rather than by `validate`
			// afterwards. Another application's database must leave here
			// untouched, not carrying a column this store added to it.
			if (*current != 0) {
				const auto application
					= scalar_integer(aDatabase, "PRAGMA application_id");
				if (!application || *application != AppearanceApplicationId) {
					return std::unexpected(failure::schema_mismatch);
				}
			}

			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(
					aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};

			if (*current != 0) {
				if (auto admitted = validate_legacy(aDatabase, *current); !admitted) {
					rollback();
					return admitted;
				}
			}

			// An unstamped file has no tables at all and is created whole. A
			// stamped one is migrated in place, inside this same transaction,
			// so a file is never left carrying a column the version it claims
			// does not have.
			const auto established = *current == 0
				? create_schema(aDatabase, aStop)
				: *current < 5 ? migrate_to_version_5(aDatabase, *current, aStop)
							   : std::expected<void, failure>{ };
			if (!established) {
				rollback();
				return established;
			}
			// A created file is built in the version-3 shape and then walks the
			// same steps a stamped file walks, so every open exercises the
			// migrations rather than only an upgrade doing so. A version-6 file
			// skips the version-6 step: running it would carry the row across
			// without its Dif columns and silently reset those preferences.
			if (*current < 6) {
				if (auto migrated = migrate_to_version_6(aDatabase, aStop); !migrated) {
					rollback();
					return migrated;
				}
			}
			if (*current < 7) {
				if (auto migrated = migrate_to_version_7(aDatabase, aStop); !migrated) {
					rollback();
					return migrated;
				}
			}
			// A version-8 file skips the version-8 step, which would carry the
			// row across without its white-space column.
			if (*current < 8) {
				if (auto migrated = migrate_to_version_8(aDatabase, aStop); !migrated) {
					rollback();
					return migrated;
				}
			}
			if (auto migrated = migrate_to_version_9(aDatabase, aStop); !migrated) {
				rollback();
				return migrated;
			}
			if (auto metadata = execute(
					aDatabase,
					"UPDATE appearance_meta SET schema_format = "
						+ std::to_string(appearance_settings_schema_version)
						+ " WHERE singleton = 1",
					aStop);
				!metadata) {
				rollback();
				return metadata;
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

			if (auto shape = validate_legacy(aDatabase, appearance_settings_schema_version); !shape) {
				return shape;
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
		/// \brief Reads the shape versions 1 to 6 stored, refusing what the core does.
		///
		/// \details This is the admission check a migration runs before it rebuilds:
		/// every refusal the current reader makes, a legacy row must survive first,
		/// so a tampered document, an unpublished mode or variant name, an
		/// out-of-range flag or illegal typography stops the migration instead of
		/// being carried into the new shape and refused at the next load.
		[[nodiscard]] auto read_legacy_settings(::sqlite3* aDatabase, std::stop_token aStop, bool aHasDif)
			-> std::expected<std::optional<legacy_appearance_record>, failure>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			const auto guard = progress_scope{ aDatabase, aStop };

			auto statement = prepare(
				aDatabase,
				"SELECT mode, palette_document, ui_family, ui_point_size, ui_weight,"
				"ui_line_height, code_family, code_point_size, code_weight,"
				"code_tab_width, code_ligatures, code_syntax_bold,"
				"code_syntax_italic, palette_variant, palette_stored, "
					+ std::string{ aHasDif ? "dif_visibility, motion_mode " : "0, 0 " }
					+ "FROM appearance_settings WHERE singleton = 1");
			if (!statement) {
				return std::unexpected(statement.error());
			}

			const auto stepped = ::sqlite3_step(statement->Value);
			if (stepped == SQLITE_INTERRUPT || aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			if (stepped == SQLITE_DONE) {
				// No row to admit. A fresh-but-versioned file is not a corrupt one.
				return std::optional<legacy_appearance_record>{ };
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(catalog_sql_failure(aDatabase));
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

			// The variant the stored palette customises. Without it a reader has
			// only a palette and must guess which built-in profile it was chosen
			// against, and the guess made from the current desktop is what pinned
			// a light palette over the dark profile. Version 7 gives every variant
			// a slot, so this is the last version that needs to be told.
			const auto variant = variant_of(text_of(statement->Value, 13));
			if (!variant) {
				return std::unexpected(variant.error());
			}

			auto record       = legacy_appearance_record{ };
			record.Mode       = *mode;
			record.Palette    = *imported.Palette;
			record.Variant    = *variant;
			const auto stored = ::sqlite3_column_int64(statement->Value, 14);
			if (stored != 0 && stored != 1) {
				return std::unexpected(failure::integrity_failure);
			}
			record.Stored         = stored == 1;
			const auto visibility = ::sqlite3_column_int64(statement->Value, 15);
			const auto motion     = ::sqlite3_column_int64(statement->Value, 16);
			if (visibility < 0 || visibility > 1 || motion < 0 || motion > 2) {
				return std::unexpected(failure::integrity_failure);
			}
			record.Dif = { .Visibility = static_cast<core::dif_visibility>(visibility), .Motion = static_cast<core::motion_mode>(motion) };
			for (const auto column : { 10, 11, 12 }) {
				const auto flag = ::sqlite3_column_int64(statement->Value, column);
				if (flag != 0 && flag != 1) {
					return std::unexpected(failure::integrity_failure);
				}
			}

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

			record.Typography.Ui.Font.Family                 = text_of(statement->Value, 2);
			record.Typography.Ui.Font.PointSizeCentipoints   = *uiSize;
			record.Typography.Ui.Font.Weight                 = *uiWeight;
			record.Typography.Ui.LineHeightPercent           = *uiHeight;
			record.Typography.Code.Font.Family               = text_of(statement->Value, 6);
			record.Typography.Code.Font.PointSizeCentipoints = *codeSize;
			record.Typography.Code.Font.Weight               = *codeWeight;
			record.Typography.Code.TabWidth                  = *codeTab;
			record.Typography.Code.Ligatures
				= ::sqlite3_column_int64(statement->Value, 10) != 0;
			record.Typography.Code.SyntaxBold
				= ::sqlite3_column_int64(statement->Value, 11) != 0;
			record.Typography.Code.SyntaxItalic
				= ::sqlite3_column_int64(statement->Value, 12) != 0;

			if (!core::validate_typography(record.Typography).empty()) {
				return std::unexpected(failure::integrity_failure);
			}
			return std::optional{ record };
		}

		/// \brief Reads the current record: one customization per variant.
		///
		/// \details Every slot is imported through `core::import_theme`, including
		/// the slots whose stored flag is clear, because each column holds a
		/// complete document and a reader that skipped the unstored ones would stop
		/// refusing a tampered value in them. The stored flag, not the document, is
		/// what says whether a slot is a reviewer's decision.
		[[nodiscard]] auto read_settings(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<std::optional<application::appearance_settings>, failure>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			const auto guard = progress_scope{ aDatabase, aStop };

			// The scalar columns first, so their indices are fixed, then two
			// columns per variant in vocabulary order.
			auto columns = std::string{ "SELECT mode" };
			for (const auto scalar : ScalarColumnNames) {
				columns += ", " + std::string{ scalar };
			}
			for (std::size_t index = 0U; index < core::appearance_variant_count;
				 ++index) {
				columns += ", " + variant_document_column(variant_at(index));
				columns += ", " + variant_stored_column(variant_at(index));
			}
			columns += ", " + std::string{ WhiteSpaceColumn };
			columns += ", " + std::string{ CodeZoomColumn };
			auto statement = prepare(
				aDatabase,
				columns + " FROM appearance_settings WHERE singleton = 1");
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
				return std::unexpected(catalog_sql_failure(aDatabase));
			}

			const auto mode = mode_of(text_of(statement->Value, 0));
			if (!mode) {
				return std::unexpected(mode.error());
			}

			auto settings = application::appearance_settings{ };
			settings.Mode = *mode;
			for (std::size_t index = 0U; index < core::appearance_variant_count;
				 ++index) {
				const auto document
					= static_cast<int>(1U + ScalarColumnNames.size() + 2U * index);
				const auto imported
					= core::import_theme(text_of(statement->Value, document));
				if (!imported.Palette) {
					return std::unexpected(failure::integrity_failure);
				}
				const auto stored
					= ::sqlite3_column_int64(statement->Value, document + 1);
				if (stored != 0 && stored != 1) {
					return std::unexpected(failure::integrity_failure);
				}
				settings.Palettes[index]
					= application::appearance_palette_customisation{
						  .Palette = *imported.Palette,
						  .Stored  = stored == 1,
					  };
			}

			const auto visibility = ::sqlite3_column_int64(statement->Value, 12);
			const auto motion     = ::sqlite3_column_int64(statement->Value, 13);
			if (visibility < 0 || visibility > 1 || motion < 0 || motion > 2) {
				return std::unexpected(failure::integrity_failure);
			}
			settings.Dif = { .Visibility = static_cast<core::dif_visibility>(visibility), .Motion = static_cast<core::motion_mode>(motion) };
			for (const auto column : { 9, 10, 11 }) {
				const auto flag = ::sqlite3_column_int64(statement->Value, column);
				if (flag != 0 && flag != 1) {
					return std::unexpected(failure::integrity_failure);
				}
			}

			const auto uiSize     = bounded(::sqlite3_column_int64(statement->Value, 2));
			const auto uiWeight   = bounded(::sqlite3_column_int64(statement->Value, 3));
			const auto uiHeight   = bounded(::sqlite3_column_int64(statement->Value, 4));
			const auto codeSize   = bounded(::sqlite3_column_int64(statement->Value, 6));
			const auto codeWeight = bounded(::sqlite3_column_int64(statement->Value, 7));
			const auto codeTab    = bounded(::sqlite3_column_int64(statement->Value, 8));
			if (!uiSize || !uiWeight || !uiHeight || !codeSize || !codeWeight
				|| !codeTab) {
				return std::unexpected(failure::integrity_failure);
			}

			settings.Typography.Ui.Font.Family                 = text_of(statement->Value, 1);
			settings.Typography.Ui.Font.PointSizeCentipoints   = *uiSize;
			settings.Typography.Ui.Font.Weight                 = *uiWeight;
			settings.Typography.Ui.LineHeightPercent           = *uiHeight;
			settings.Typography.Code.Font.Family               = text_of(statement->Value, 5);
			settings.Typography.Code.Font.PointSizeCentipoints = *codeSize;
			settings.Typography.Code.Font.Weight               = *codeWeight;
			settings.Typography.Code.TabWidth                  = *codeTab;
			settings.Typography.Code.Ligatures
				= ::sqlite3_column_int64(statement->Value, 9) != 0;
			settings.Typography.Code.SyntaxBold
				= ::sqlite3_column_int64(statement->Value, 10) != 0;
			settings.Typography.Code.SyntaxItalic
				= ::sqlite3_column_int64(statement->Value, 11) != 0;
			const auto whiteSpace = ::sqlite3_column_int64(
				statement->Value, static_cast<int>(1U + ScalarColumnNames.size() + 2U * core::appearance_variant_count));
			if (whiteSpace < 0 || whiteSpace >= static_cast<std::int64_t>(core::white_space_drawing_count)) {
				return std::unexpected(failure::integrity_failure);
			}
			settings.Typography.Code.WhiteSpace = static_cast<core::white_space_drawing>(whiteSpace);
			const auto codeZoom                 = bounded(::sqlite3_column_int64(
				statement->Value, static_cast<int>(2U + ScalarColumnNames.size() + 2U * core::appearance_variant_count)));
			if (!codeZoom) {
				return std::unexpected(codeZoom.error());
			}
			settings.Typography.Code.ZoomPercent = *codeZoom;

			// A durable record can be written by another build or edited by hand.
			// The core is the authority on typography legality, so a record it
			// rejects is refused whole here rather than adopted and then quietly
			// repaired somewhere further in.
			if (!core::validate_typography(settings.Typography).empty()) {
				return std::unexpected(failure::integrity_failure);
			}
			return std::optional{ settings };
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
			native.c_str(), &database, OpenFlags, nullptr);
		auto owned = appearance_settings_database{ database };
		if (code != SQLITE_OK || database == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}
		return adopt(std::move(owned), std::move(aStop));
	}

	auto appearance_settings_database::open(
		state_directory& aDirectory, const std::string_view aName, std::stop_token aStop)
		-> std::expected<appearance_settings_database, failure>
	{
		if (!sqlite_runtime_ready()) {
			return std::unexpected(failure::unavailable);
		}
		auto opened = open_private_store_database(
			aDirectory, aName, OpenFlags | SQLITE_OPEN_NOFOLLOW);
		if (!opened) {
			return std::unexpected(private_store_database_failure(opened.error()));
		}
		return adopt(appearance_settings_database{ *opened }, std::move(aStop));
	}

	auto appearance_settings_database::adopt(
		appearance_settings_database aOwned, std::stop_token aStop)
		-> std::expected<appearance_settings_database, failure>
	{
		auto* const database = aOwned.Handle;
		if (auto configured = configure_connection(database); !configured) {
			return std::unexpected(configured.error());
		}
		if (auto established = establish_schema(database, aStop); !established) {
			return std::unexpected(established.error());
		}
		if (auto validated = validate(database); !validated) {
			return std::unexpected(validated.error());
		}
		return aOwned;
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

		// One statement generated from the declared column order, so a column
		// added to the schema cannot be left out of the write or bound to the
		// wrong parameter. Parameter n binds column n: the singleton is the
		// literal 1 and every other column takes a parameter in order.
		const auto columns = settings_column_names();
		auto       sql     = std::string{ "INSERT INTO appearance_settings(" }
			+ joined(columns, ", ") + ") VALUES(1";
		for (std::size_t index = 1U; index < columns.size(); ++index) {
			sql += ", ?" + std::to_string(index);
		}
		sql += ") ON CONFLICT(singleton) DO UPDATE SET ";
		for (std::size_t index = 1U; index < columns.size(); ++index) {
			sql += (index > 1U ? ", " : "") + columns[index] + " = excluded."
				+ columns[index];
		}

		auto statement = prepare(Handle, sql);
		if (!statement) {
			return std::unexpected(statement.error());
		}

		const auto mode      = std::string{ core::stable_name(aSettings.Mode) };
		auto       documents = std::vector<std::string>{ };
		for (const auto& customisation : aSettings.Palettes) {
			documents.push_back(core::export_theme(customisation.Palette));
		}

		auto       parameter = 1;
		const auto text      = [&statement, &parameter](const std::string& aValue) {
			return ::sqlite3_bind_text(
					   statement->Value, parameter++, aValue.c_str(), -1, SQLITE_TRANSIENT)
				== SQLITE_OK;
		};
		const auto integer = [&statement, &parameter](const std::int64_t aValue) {
			return ::sqlite3_bind_int64(statement->Value, parameter++, aValue)
				== SQLITE_OK;
		};

		const auto& ui    = aSettings.Typography.Ui;
		const auto& code  = aSettings.Typography.Code;
		auto        bound = text(mode);
		for (std::size_t index = 0U;
			 bound && index < core::appearance_variant_count;
			 ++index) {
			bound = text(documents[index])
				&& integer(aSettings.Palettes[index].Stored ? 1 : 0);
		}
		bound = bound && text(ui.Font.Family)
			&& integer(ui.Font.PointSizeCentipoints) && integer(ui.Font.Weight)
			&& integer(ui.LineHeightPercent) && text(code.Font.Family)
			&& integer(code.Font.PointSizeCentipoints) && integer(code.Font.Weight)
			&& integer(code.TabWidth) && integer(code.Ligatures ? 1 : 0)
			&& integer(code.SyntaxBold ? 1 : 0) && integer(code.SyntaxItalic ? 1 : 0)
			&& integer(static_cast<std::int64_t>(aSettings.Dif.Visibility))
			&& integer(static_cast<std::int64_t>(aSettings.Dif.Motion))
			&& integer(static_cast<std::int64_t>(code.WhiteSpace))
			&& integer(static_cast<std::int64_t>(code.ZoomPercent));
		if (!bound) {
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
		-> std::expected<std::optional<application::appearance_settings>, failure>
	{
		return read_settings(Handle, aStop);
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
