#include "appearance_settings_database.hpp"

#include "bpr/core/theme_exchange.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace bpr::adapters::sqlite::detail {
	namespace {
		// "BPRA". Distinct from the ledger's "BPRD", the queue's "BPRQ", and
		// the workbench's "BPRW", so that pointing one store at another's file
		// fails at the identity check rather than at the first query for a
		// table that is not there.
		constexpr std::int32_t AppearanceApplicationId = 0x42505241;

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
					|| name == "palette_variant" || name == "ui_family" || name == "code_family";
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
			constexpr std::array<std::string_view, 18> settingsNames{
				"singleton", "mode", "palette_document", "ui_family", "ui_point_size",
				"ui_weight", "ui_line_height", "code_family", "code_point_size",
				"code_weight", "code_tab_width", "code_ligatures", "code_syntax_bold",
				"code_syntax_italic", "palette_variant", "palette_stored", "dif_visibility", "motion_mode"
			};
			// Version 1 had neither trailing column, version 2 the variant,
			// versions 3 to 5 both. Version 6 adds the two Dif columns.
			const auto columnCount = aVersion == 1 ? 14U : aVersion == 2 ? 15U
				: aVersion >= 6                                          ? 18U
																		 : 16U;
			const auto objects     = scalar_integer(aDatabase,
													"SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'");
			if ((aVersion < 1 || aVersion > 6) || !objects || *objects != 2
				|| !legacy_columns_match(aDatabase, "appearance_meta", metaNames)
				|| !legacy_columns_match(aDatabase, "appearance_settings",
										 std::span{ settingsNames }.first(columnCount))) {
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

		[[nodiscard]] auto read_settings(::sqlite3* aDatabase, std::stop_token aStop, bool aHasDif)
			-> std::expected<std::optional<application::appearance_settings>, failure>;

		/// \brief Adds profile preferences only after the version-5 values are admitted.
		[[nodiscard]] auto migrate_to_version_6(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto rows = scalar_integer(aDatabase, "SELECT count(*) FROM appearance_settings WHERE singleton != 1");
			if (!rows || *rows != 0) {
				return std::unexpected(failure::integrity_failure);
			}
			if (const auto settings = read_settings(aDatabase, aStop, false); !settings) {
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
			if (auto migrated = migrate_to_version_6(aDatabase, aStop); !migrated) {
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
		[[nodiscard]] auto read_settings(::sqlite3* aDatabase, std::stop_token aStop, bool aHasDif)
			-> std::expected<std::optional<application::appearance_settings>, failure>

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
			// a light palette over the dark profile.
			const auto variant = variant_of(text_of(statement->Value, 13));
			if (!variant) {
				return std::unexpected(variant.error());
			}

			auto settings           = application::appearance_settings{ };
			settings.Mode           = *mode;
			settings.Palette        = *imported.Palette;
			settings.PaletteVariant = *variant;
			const auto stored       = ::sqlite3_column_int64(statement->Value, 14);
			if (stored != 0 && stored != 1) {
				return std::unexpected(failure::integrity_failure);
			}
			settings.PaletteStored = stored == 1;
			const auto visibility  = ::sqlite3_column_int64(statement->Value, 15);
			const auto motion      = ::sqlite3_column_int64(statement->Value, 16);
			if (visibility < 0 || visibility > 1 || motion < 0 || motion > 2) {
				return std::unexpected(failure::integrity_failure);
			}
			settings.Dif = { .Visibility = static_cast<core::dif_visibility>(visibility), .Motion = static_cast<core::motion_mode>(motion) };
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
		const auto variant
			= std::string{ core::stable_name(aSettings.PaletteVariant) };

		auto statement = prepare(
			Handle,
			"INSERT INTO appearance_settings("
			"singleton, mode, palette_document, ui_family, ui_point_size,"
			"ui_weight, ui_line_height, code_family, code_point_size,"
			"code_weight, code_tab_width, code_ligatures, code_syntax_bold,"
			"code_syntax_italic, palette_variant, palette_stored, dif_visibility, motion_mode) "
			"VALUES(1, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13,"
			"?14, ?15, ?16, ?17) "
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
			"code_syntax_italic = excluded.code_syntax_italic,"
			"palette_variant = excluded.palette_variant,"
			"palette_stored = excluded.palette_stored, dif_visibility = excluded.dif_visibility, motion_mode = excluded.motion_mode");
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
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value, 14, variant.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 15, aSettings.PaletteStored ? 1 : 0) != SQLITE_OK
			|| ::sqlite3_bind_int(statement->Value, 16, static_cast<int>(aSettings.Dif.Visibility)) != SQLITE_OK
			|| ::sqlite3_bind_int(statement->Value, 17, static_cast<int>(aSettings.Dif.Motion)) != SQLITE_OK) {
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
		return read_settings(Handle, aStop, true);
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
