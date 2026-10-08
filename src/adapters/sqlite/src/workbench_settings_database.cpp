#include "workbench_settings_database.hpp"

#include "storage_internal.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <limits>
#include <optional>
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

		constexpr auto OpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
			| SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE;

		/// \brief [[BPR-1295]]. What every write stores in the retired
		/// `continue_modality` column: 1, the Review it named, which is what
		/// Continue review enters since Read merged into Review.
		constexpr std::int64_t retired_continue_modality = 1;

		/// \brief The widest table this store admits, in columns.
		///
		/// \details `window_layout` has twenty-three columns after the Updates
		/// line's fold ([[BPR-1130]]) joined native geometry and Home, and
		/// `workbench_preferences` twenty-five since the label helper, the
		/// Overview's starting count and its label columns ([[BPR-1088]],
		/// version thirteen) and the Claude process wrapper ([[BPR-1301]],
		/// version fourteen). `SQLITE_LIMIT_COLUMN` bounds a table
		/// definition, the values of one INSERT, and the result of one `SELECT *`
		/// alike, so below this a version-fourteen file could not even be parsed,
		/// let alone read: a column added later has to raise this in the same
		/// schema step. Three admissible tables bound the whole enumerated
		/// schema at three times this.
		constexpr std::size_t MaximumTableColumns = 25U;

		/// \brief Writes the layout table's definition under one name.
		///
		/// \details Taking the name is what lets the fresh create and the
		/// version-one rebuild share one definition. Two copies of this DDL
		/// would be two things to keep in agreement, and a migrated database
		/// that merely resembled a fresh one is the failure this store would
		/// find hardest to notice.
		///
		/// The width bounds are interpolated from
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
		/// review content. Drawer geometry and whether the panes are numbered
		/// are reviewer preferences, and this file holds nothing that names
		/// what they were reading.
		[[nodiscard]] auto layout_table(const std::string_view aName)
			-> std::string
		{
			const auto minimum
				= std::to_string(application::minimum_workbench_drawer_width);
			const auto maximum
				= std::to_string(application::maximum_workbench_drawer_width);
			return "CREATE TABLE " + std::string{ aName }
			+ "(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			  "files_state INTEGER NOT NULL "
			  "CHECK(files_state BETWEEN 0 AND 1),"
			  "conversations_state INTEGER NOT NULL "
			  "CHECK(conversations_state BETWEEN 0 AND 1),"
			  "files_width INTEGER NOT NULL CHECK(files_width BETWEEN "
				+ minimum + " AND " + maximum
				+ "),conversations_width INTEGER NOT NULL "
				  "CHECK(conversations_width BETWEEN "
				+ minimum + " AND " + maximum
				+ "),line_numbers INTEGER NOT NULL DEFAULT 1 "
				  "CHECK(line_numbers BETWEEN 0 AND 1),"
				  "one_pane INTEGER NOT NULL DEFAULT 0 CHECK(one_pane BETWEEN 0 AND 1),"
				  // [[BPR-0651]]. The repositories drawer, schema version 3.
				  // Defaulted so a rebuilt older row keeps the behaviour it had:
				  // the drawer open at the design's width.
				  "repositories_state INTEGER NOT NULL DEFAULT 1 "
				  "CHECK(repositories_state BETWEEN 0 AND 1),"
				  "repositories_width INTEGER NOT NULL DEFAULT 300 "
				  "CHECK(repositories_width BETWEEN "
				+ minimum + " AND " + maximum
				// [[BPR-0661]]. The review-behaviour preferences, schema
				// version 4. Each default is the behaviour the window had
				// before the preference existed.
				+ "),open_modality INTEGER NOT NULL DEFAULT 0 "
				  "CHECK(open_modality BETWEEN 0 AND 2),"
				  "continue_modality INTEGER NOT NULL DEFAULT 0 "
				  "CHECK(continue_modality BETWEEN 0 AND 1),"
				  "mark_read INTEGER NOT NULL DEFAULT 0 "
				  "CHECK(mark_read BETWEEN 0 AND 1),"
				  "return_comparison INTEGER NOT NULL DEFAULT 0 "
				  "CHECK(return_comparison BETWEEN 0 AND 2),"
				  "drawer_filter INTEGER NOT NULL DEFAULT 0 "
				  "CHECK(drawer_filter BETWEEN 0 AND 2),"
				  // [[BPR-0721]]. Whether the drawers carry their own
				  // controls, schema version 6. Defaulted hidden, the owner's
				  // direction, so a rebuilt older row hides them too.
				  "drawer_controls INTEGER NOT NULL DEFAULT 0 "
				  "CHECK(drawer_controls BETWEEN 0 AND 1),"
				  "writing_revision INTEGER NOT NULL DEFAULT 1 CHECK(writing_revision > 0),"
				  "writing_problem TEXT NOT NULL DEFAULT '' CHECK(length(CAST(writing_problem AS BLOB)) <= 8192),"
				  "writing_solution TEXT NOT NULL DEFAULT '' CHECK(length(CAST(writing_solution AS BLOB)) <= 8192),"
				  "writing_outline TEXT NOT NULL DEFAULT '' CHECK(length(CAST(writing_outline AS BLOB)) <= 8192),"
				  "writing_stop TEXT NOT NULL DEFAULT '' CHECK(length(CAST(writing_stop AS BLOB)) <= 8192),"
				  // [[BPR-1083]]. How label marks are drawn, schema version
				  // 12: zero is the pictorial icons, the default (owner,
				  // 2026-10-05), one the two-letter shortcode marks.
				  "feedback_marks INTEGER NOT NULL DEFAULT 0 CHECK(feedback_marks BETWEEN 0 AND 1),"
				  // [[BPR-1088]]. Schema version 13, each defaulted so a
				  // migrated row keeps what it had: the label helper off,
				  // the Overview's count starting on Open, and no label
				  // columns. The columns are stable catalog IDs and commas,
				  // read by `core::read_feedback_label_columns`; the bound is
				  // `core::maximum_feedback_label_columns_text`, written out
				  // because preflight compares this text with a stored file.
				  "label_helper INTEGER NOT NULL DEFAULT 0 CHECK(label_helper BETWEEN 0 AND 1),"
				  "overview_counts INTEGER NOT NULL DEFAULT 0 CHECK(overview_counts BETWEEN 0 AND 1),"
				  "label_columns TEXT NOT NULL DEFAULT '' CHECK(length(CAST(label_columns AS BLOB)) <= 256),"
				  // [[BPR-1301]]. Schema version 14: the profile's Claude
				  // process wrapper, empty for none, so a migrated row runs
				  // `claude` as it did. The bound is
				  // `core::maximum_agent_wrapper_bytes`, written out for the
				  // same reason as the label columns'.
				  "claude_wrapper TEXT NOT NULL DEFAULT '' CHECK(length(CAST(claude_wrapper AS BLOB)) <= 1024)) STRICT";
		}

		static_assert(core::maximum_feedback_label_columns_text == 256U);
		static_assert(core::maximum_agent_wrapper_bytes == 1024U);
		static_assert(static_cast<std::int64_t>(application::review_label_helper::on) == 1);
		static_assert(static_cast<std::int64_t>(application::review_overview_counts::all) == 1);

		void remove_one_pane_column(std::string& aSql)
		{
			constexpr std::string_view column = ",one_pane INTEGER NOT NULL DEFAULT 0 CHECK(one_pane BETWEEN 0 AND 1)";
			aSql.erase(aSql.find(column), column.size());
		}

		/// \brief [[BPR-0783]]. One window's Home view, schema version 8.
		///
		/// \details Only the per-window table carries these. Home is one
		/// window's arrangement of the same rows, so the profile row must not
		/// answer one: a default stored there would be a view every later
		/// window inherited without having chosen it.
		///
		/// Every default is what a version-seven window already showed —
		/// ungrouped, with its state headers, nothing collapsed, and the
		/// profile's own queue filter — so a migrated row keeps the Home it had.
		///
		/// The bounds are written out rather than interpolated because this text
		/// is what a version-eight build stored and what preflight compares a
		/// stored file against; the asserts below are what keeps them agreeing
		/// with the vocabularies they encode. Zero is `Any` for the two optional
		/// filters and a named value is stored one above its enumerator, so
		/// `github_role::none` — a role a reviewer can filter by — stays
		/// distinct from no role filter at all.
		///
		/// No name here says what was reviewed. `home_collapsed` is the four
		/// panels that carry no name; a group panel is keyed by an owner, a
		/// repository or an account, and its collapse stays in the window.
		constexpr std::string_view HomeColumns
			= ",home_grouping INTEGER NOT NULL DEFAULT 0 CHECK(home_grouping BETWEEN 0 AND 3),"
			  "home_state_headers INTEGER NOT NULL DEFAULT 1 CHECK(home_state_headers BETWEEN 0 AND 1),"
			  "home_collapsed INTEGER NOT NULL DEFAULT 0 CHECK(home_collapsed BETWEEN 0 AND 15),"
			  "home_filter_owned INTEGER NOT NULL DEFAULT 0 CHECK(home_filter_owned BETWEEN 0 AND 1),"
			  "home_role INTEGER NOT NULL DEFAULT 0 CHECK(home_role BETWEEN 0 AND 5),"
			  "home_added_by INTEGER NOT NULL DEFAULT 0 CHECK(home_added_by BETWEEN 0 AND 3),"
			  "home_quiet INTEGER NOT NULL DEFAULT 0 CHECK(home_quiet BETWEEN 0 AND 2),"
			  "home_sort INTEGER NOT NULL DEFAULT 0 CHECK(home_sort BETWEEN 0 AND 2)";
		static_assert(static_cast<std::int64_t>(application::home_grouping::account) == 3);
		static_assert(application::home_collapsed_panel::all == 15U);
		static_assert(static_cast<std::int64_t>(core::github_role::author) + 1 == 5);
		static_assert(static_cast<std::int64_t>(core::observer_rule_kind::path) + 1 == 3);
		static_assert(static_cast<std::int64_t>(application::queue_quiet_filter::quiet) == 2);
		static_assert(static_cast<std::int64_t>(application::queue_sort_order::pull_request_number) == 2);

		// Version nine adds only window-local native placement. Zero dimensions
		// keep older windows on the platform's initial placement.
		constexpr std::string_view GeometryColumns
			= ",geometry_x INTEGER NOT NULL DEFAULT 0 CHECK(geometry_x BETWEEN -1000000 AND 1000000),"
			  "geometry_y INTEGER NOT NULL DEFAULT 0 CHECK(geometry_y BETWEEN -1000000 AND 1000000),"
			  "geometry_width INTEGER NOT NULL DEFAULT 0 CHECK(geometry_width = 0 OR geometry_width BETWEEN 64 AND 16384),"
			  "geometry_height INTEGER NOT NULL DEFAULT 0 CHECK(geometry_height = 0 OR geometry_height BETWEEN 64 AND 16384),"
			  "geometry_maximized INTEGER NOT NULL DEFAULT 0 CHECK(geometry_maximized BETWEEN 0 AND 1)";

		/// \brief [[BPR-1130]]. The window's Updates line, schema version 10.
		///
		/// \details Only the per-window table carries it, as it carries Home:
		/// every new window starts folded, so the profile's defaults answer no
		/// fold. Zero is folded, the default a migrated row takes; the bound is
		/// written out because this text is what preflight compares a stored
		/// file against, and the assert keeps it agreeing with the vocabulary.
		constexpr std::string_view UpdatesLineColumns
			= ",updates_line INTEGER NOT NULL DEFAULT 0 CHECK(updates_line BETWEEN 0 AND 1)";
		static_assert(static_cast<std::int64_t>(core::workbench_updates_line::folded) == 0);
		static_assert(static_cast<std::int64_t>(core::workbench_updates_line::open) == 1);

		constexpr std::string_view MetaSql = "CREATE TABLE workbench_meta(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),schema_format INTEGER NOT NULL) STRICT";
		[[nodiscard]] auto         window_table(const std::int64_t aVersion = workbench_settings_schema_version) -> std::string
		{
			auto sql = layout_table("window_layout");
			if (aVersion < 7) {
				remove_one_pane_column(sql);
			}
			const auto first = sql.find("singleton INTEGER PRIMARY KEY CHECK(singleton = 1)");
			sql.replace(first, std::string_view{ "singleton INTEGER PRIMARY KEY CHECK(singleton = 1)" }.size(),
						"window_id TEXT PRIMARY KEY NOT NULL CHECK(length(window_id) BETWEEN 1 AND 20 AND window_id NOT GLOB '*[^0-9]*' AND substr(window_id,1,1) != '0')");
			sql.resize(sql.find(",open_modality"));
			if (aVersion >= 8) {
				sql += HomeColumns;
			}
			if (aVersion >= 9) {
				sql += GeometryColumns;
			}
			if (aVersion >= 10) {
				sql += UpdatesLineColumns;
			}
			if (aVersion >= 15) {
				sql += ",summary_fraction INTEGER NOT NULL DEFAULT 0 CHECK(summary_fraction BETWEEN 0 AND 9999)";
			}
			return sql + ") STRICT";
		}
		/// \brief Writes the profile table as the build that stamped one
		/// version left it.
		///
		/// \details Versions one to four named it `workbench_layout`; five
		/// renamed it `workbench_preferences` beside the per-window table. Each
		/// older shape removes the version-seven middle column and cuts before
		/// the first suffix column that version lacked, so a stored file is compared against the text its
		/// build wrote rather than against a second copy of that text.
		[[nodiscard]] auto profile_table(const std::int64_t aVersion) -> std::string
		{
			auto sql = layout_table(aVersion >= 5 ? "workbench_preferences" : "workbench_layout");
			if (aVersion < 7) {
				remove_one_pane_column(sql);
			}
			const std::string_view cut = aVersion == 1 ? ",line_numbers"
				: aVersion == 2                        ? ",repositories_state"
				: aVersion == 3                        ? ",open_modality"
				: aVersion <= 5                        ? ",drawer_controls"
				: aVersion <= 10                       ? ",writing_revision"
				: aVersion <= 11                       ? ",feedback_marks"
				: aVersion <= 12                       ? ",label_helper"
				: aVersion <= 13                       ? ",claude_wrapper"
													   : "";
			if (!cut.empty()) {
				sql.resize(sql.find(cut));
				sql += ") STRICT";
			}
			return sql;
		}
		[[nodiscard]] auto schema_version_1() -> std::vector<std::string>
		{
			return { std::string{ MetaSql }, layout_table("workbench_preferences"), window_table() };
		}
		/// \brief Carries one older version's profile row into today's table.
		///
		/// \details Version five already named the table
		/// `workbench_preferences` and kept `window_layout` beside it, so its
		/// table is first renamed to where versions one to four kept theirs and
		/// one rebuild serves all older versions. SQLite rewrites the stored definition
		/// of a renamed table; that is harmless only because the renamed table
		/// is the one dropped, and the table left behind is created fresh from
		/// `layout_table`. The per-window table is rebuilt too, preserving every older geometry
		/// value, including the one pane a version-seven row already held, while
		/// the new Home columns take the defaults above.
		[[nodiscard]] auto migrate_from(const std::int64_t aVersion) -> std::vector<std::string>
		{
			const auto columns = std::string{ "singleton,files_state,conversations_state,files_width,conversations_width" }
				+ (aVersion >= 2 ? ",line_numbers" : "") + (aVersion >= 3 ? ",repositories_state,repositories_width" : "")
				+ (aVersion >= 4 ? ",open_modality,continue_modality,mark_read,return_comparison,drawer_filter" : "")
				+ (aVersion >= 6 ? ",drawer_controls" : "") + (aVersion >= 7 ? ",one_pane" : "")
				+ (aVersion >= 11 ? ",writing_revision,writing_problem,writing_solution,writing_outline,writing_stop" : "")
				+ (aVersion >= 12 ? ",feedback_marks" : "") + (aVersion >= 13 ? ",label_helper,overview_counts,label_columns" : "")
				+ (aVersion >= 14 ? ",claude_wrapper" : "");
			std::vector<std::string> statements;
			if (aVersion >= 5) {
				statements.emplace_back("ALTER TABLE workbench_preferences RENAME TO workbench_layout");
			}
			statements.push_back(layout_table("workbench_preferences"));
			statements.push_back("INSERT INTO workbench_preferences(" + columns + ") SELECT " + columns + " FROM workbench_layout");
			statements.emplace_back("DROP TABLE workbench_layout");
			if (aVersion >= 5) {
				statements.emplace_back("ALTER TABLE window_layout RENAME TO old_window_layout");
			}
			statements.push_back(window_table());
			if (aVersion >= 5) {
				const auto windowColumns = std::string{ "window_id,files_state,conversations_state,files_width,conversations_width,line_numbers,repositories_state,repositories_width" }
					+ (aVersion >= 7 ? ",one_pane" : "")
					+ (aVersion >= 8 ? ",home_grouping,home_state_headers,home_collapsed,home_filter_owned,home_role,home_added_by,home_quiet,home_sort" : "")
					+ (aVersion >= 9 ? ",geometry_x,geometry_y,geometry_width,geometry_height,geometry_maximized" : "")
					+ (aVersion >= 10 ? ",updates_line" : "");
				statements.push_back("INSERT INTO window_layout(" + windowColumns + ") SELECT " + windowColumns + " FROM old_window_layout");
				statements.emplace_back("DROP TABLE old_window_layout");
			}
			statements.push_back("UPDATE workbench_meta SET schema_format="
								 + std::to_string(workbench_settings_schema_version) + " WHERE singleton=1");
			return statements;
		}
		[[nodiscard]] auto normalized_sql(const std::string_view aSql) -> std::string
		{
			std::string result;
			for (const auto value : aSql) {
				const auto byte = static_cast<unsigned char>(value);
				if (!std::isspace(byte) && value != '"' && value != ';') {
					result.push_back(static_cast<char>(std::tolower(byte)));
				}
			}
			return result;
		}
		class bounded_read_scope final
		{
		public:
			bounded_read_scope(::sqlite3* aDatabase, std::stop_token aStop)
				: Stop{ aStop }
				, Handler{ aDatabase, 100, progress, this }
			{
			}
			bounded_read_scope(const bounded_read_scope&)                    = delete;
			auto operator=(const bounded_read_scope&) -> bounded_read_scope& = delete;

		private:
			static auto progress(void* aContext) -> int
			{
				auto& scope = *static_cast<bounded_read_scope*>(aContext);
				return scope.Stop.stop_requested() || ++scope.Steps >= 400U ? 1 : 0;
			}
			std::stop_token        Stop;
			std::size_t            Steps = 0;
			progress_handler_scope Handler;
		};
		void limit_connection(::sqlite3* aDatabase)
		{
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, 65536));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, 16384));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, static_cast<int>(MaximumTableColumns)));
			static_cast<void>(sqlite3_limit(aDatabase, SQLITE_LIMIT_VDBE_OP, 10000));
		}
		/// \brief The largest value one stored preference column may carry.
		///
		/// \details [[BPR-0783]]. By name, because these columns are ranges
		/// rather than flags and they no longer sit at fixed positions: the Home
		/// view added eight of them to one table and not the other. Anything not
		/// named here is a flag, and each bound is read off the vocabulary it
		/// encodes so this and the stored `CHECK` cannot drift apart.
		[[nodiscard]] auto stored_maximum(const std::string_view aColumn) noexcept -> std::int64_t
		{
			if (aColumn == "summary_fraction") {
				return 9999;
			}
			if (aColumn == "home_collapsed") {
				return application::home_collapsed_panel::all;
			}
			if (aColumn == "home_role") {
				return static_cast<std::int64_t>(core::github_role::author) + 1;
			}
			if (aColumn == "home_grouping") {
				return static_cast<std::int64_t>(application::home_grouping::account);
			}
			if (aColumn == "home_added_by") {
				return static_cast<std::int64_t>(core::observer_rule_kind::path) + 1;
			}
			if (aColumn == "open_modality" || aColumn == "return_comparison" || aColumn == "drawer_filter"
				|| aColumn == "home_quiet" || aColumn == "home_sort") {
				return 2;
			}
			return 1;
		}
		[[nodiscard]] auto bounded_rows(::sqlite3* aDatabase, const std::string_view aTable,
										const std::size_t aMaximum) -> bool
		{
			auto rows = prepare(aDatabase, "SELECT * FROM " + std::string{ aTable } + " LIMIT " + std::to_string(aMaximum + 1U));
			if (!rows) {
				return false;
			}
			std::size_t count = 0;
			for (auto step = sqlite3_step(rows->Value); step != SQLITE_DONE; step = sqlite3_step(rows->Value)) {
				if (step != SQLITE_ROW || ++count > aMaximum) {
					return false;
				}
				const auto window  = aTable == "window_layout";
				const auto columns = sqlite3_column_count(rows->Value);
				for (int column = 0; column < columns; ++column) {
					if (window && column == 0) {
						if (sqlite3_column_type(rows->Value, column) != SQLITE_TEXT) {
							return false;
						}
						const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(rows->Value, column));
						const auto  size  = sqlite3_column_bytes(rows->Value, column);
						if (!bytes || size < 1 || size > 20) {
							return false;
						}
						std::uint64_t id     = 0;
						const auto    parsed = std::from_chars(bytes, bytes + size, id);
						if (parsed.ec != std::errc{ } || parsed.ptr != bytes + size || id == 0 || std::to_string(id) != std::string_view{ bytes, static_cast<std::size_t>(size) }) {
							return false;
						}
						continue;
					}
					const auto columnName = std::string_view{ sqlite3_column_name(rows->Value, column) };
					if (columnName.starts_with("writing_") && columnName != "writing_revision") {
						const auto  size = sqlite3_column_bytes(rows->Value, column);
						const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(rows->Value, column));
						if (sqlite3_column_type(rows->Value, column) != SQLITE_TEXT || !text || size < 0 || size > 8192 || !core::valid_writing_utf8({ text, static_cast<std::size_t>(size) })) {
							return false;
						}
						continue;
					}
					// [[BPR-1088]]. Text within its bound. Which IDs it names is
					// the reader's to judge, so an unknown or repeated label
					// is explained at load rather than refusing the store.
					if (columnName == "label_columns") {
						const auto size = sqlite3_column_bytes(rows->Value, column);
						if (sqlite3_column_type(rows->Value, column) != SQLITE_TEXT || size < 0
							|| static_cast<std::size_t>(size) > core::maximum_feedback_label_columns_text) {
							return false;
						}
						continue;
					}
					// [[BPR-1301]]. The Claude process wrapper: bounded text with
					// no control character, which is all a store can judge;
					// whether it names a program is asked at every launch.
					if (columnName == "claude_wrapper") {
						const auto  size = sqlite3_column_bytes(rows->Value, column);
						const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(rows->Value, column));
						if (sqlite3_column_type(rows->Value, column) != SQLITE_TEXT || !text || size < 0
							|| !core::storable_agent_wrapper({ text, static_cast<std::size_t>(size) })) {
							return false;
						}
						continue;
					}
					if (columnName == "writing_revision") {
						const auto revision = sqlite3_column_int64(rows->Value, column);
						if (sqlite3_column_type(rows->Value, column) != SQLITE_INTEGER || revision <= 0 || revision >= std::numeric_limits<std::int64_t>::max()) {
							return false;
						}
						continue;
					}
					if (sqlite3_column_type(rows->Value, column) != SQLITE_INTEGER) {
						return false;
					}
					const auto value = sqlite3_column_int64(rows->Value, column);
					const auto name  = std::string_view{ sqlite3_column_name(rows->Value, column) };
					if (aTable == "workbench_meta") {
						if (column == 0 && value != 1) {
							return false;
						}
						continue;
					}
					if (column == 0) {
						if (value != 1) {
							return false;
						}
					} else if (name == "files_width" || name == "conversations_width" || name == "repositories_width") {
						if (value < application::minimum_workbench_drawer_width || value > application::maximum_workbench_drawer_width) {
							return false;
						}
					} else if (name == "geometry_x" || name == "geometry_y") {
						if (value < -1'000'000 || value > 1'000'000) {
							return false;
						}
					} else if (name == "geometry_width" || name == "geometry_height") {
						if (value != 0 && (value < 64 || value > 16'384)) {
							return false;
						}
					} else if (value < 0 || value > stored_maximum(name)) {
						return false;
					}
				}
			}
			return aTable != "workbench_meta" || count == 1;
		}
		[[nodiscard]] auto preflight_schema(::sqlite3* aDatabase, std::stop_token aStop) -> std::expected<void, failure>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			const bounded_read_scope guard{ aDatabase, aStop };
			const auto               version = scalar_integer(aDatabase, "PRAGMA user_version");
			const auto               app     = scalar_integer(aDatabase, "PRAGMA application_id");
			if (!version || !app || *version < 0 || *version > workbench_settings_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto schema = prepare(aDatabase, "SELECT name,sql FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' LIMIT 4");
			if (!schema) {
				return std::unexpected(failure::schema_mismatch);
			}
			std::size_t count = 0;
			for (auto step = sqlite3_step(schema->Value); step != SQLITE_DONE; step = sqlite3_step(schema->Value)) {
				if (step != SQLITE_ROW || ++count > 3U || *version == 0) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(schema->Value, 0));
				const auto* sql  = reinterpret_cast<const char*>(sqlite3_column_text(schema->Value, 1));
				if (!name || !sql) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto  table = std::string_view{ name };
				std::string expected;
				if (table == "workbench_meta") {
					expected = MetaSql;
				} else if (table == (*version < 5 ? "workbench_layout" : "workbench_preferences")) {
					expected = profile_table(*version);
				} else if (*version >= 5 && table == "window_layout") {
					expected = window_table(*version);
				} else {
					return std::unexpected(failure::schema_mismatch);
				}
				if (normalized_sql(sql) != normalized_sql(expected)
					|| !bounded_rows(aDatabase, table, table == "window_layout" ? application::maximum_workbench_window_layouts : 1U)) {
					return std::unexpected(failure::schema_mismatch);
				}
			}
			if (*version == 0) {
				return *app == 0 && count == 0 ? std::expected<void, failure>{ } : std::unexpected(failure::schema_mismatch);
			}
			const auto meta = scalar_integer(aDatabase, "SELECT schema_format FROM workbench_meta WHERE singleton=1");
			if (*app != WorkbenchApplicationId || count != (*version >= 5 ? 3U : 2U) || !meta || *meta != *version) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}
		[[nodiscard]] auto preflight(::sqlite3* aDatabase, std::stop_token aStop) -> std::expected<void, failure>
		{
			auto result = preflight_schema(aDatabase, aStop);
			return aStop.stop_requested() ? std::unexpected(failure::cancelled) : result;
		}

		[[nodiscard]] auto commit_transaction(::sqlite3* aDatabase, std::stop_token aStop) -> std::expected<void, failure>
		{
			sqlite3_commit_hook(aDatabase, [](void* aContext) -> int { return static_cast<std::stop_token*>(aContext)->stop_requested() ? 1 : 0; }, &aStop);
			auto result = execute(aDatabase, "COMMIT", aStop);
			sqlite3_commit_hook(aDatabase, nullptr, nullptr);
			return aStop.stop_requested() ? std::unexpected(failure::cancelled) : result;
		}
		class write_transaction final
		{
		public:
			explicit write_transaction(::sqlite3* aDatabase)
				: Database{ aDatabase }
			{ }
			~write_transaction()
			{
				if (Active) {
					static_cast<void>(sqlite3_exec(Database, "ROLLBACK", nullptr, nullptr, nullptr));
				}
			}
			write_transaction(const write_transaction&)                    = delete;
			auto operator=(const write_transaction&) -> write_transaction& = delete;
			auto commit(std::stop_token aStop) -> std::expected<void, failure>
			{
				auto result = commit_transaction(Database, aStop);
				if (result) {
					Active = false;
				}
				return result;
			}

		private:
			::sqlite3* Database;
			bool       Active = true;
		};
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

			// Zero is an empty file; anything between that and the current
			// version is a store with rows in it that has to be carried
			// forward rather than recreated underneath the reviewer.
			//
			// Two things this branch is worth knowing about. It runs before
			// `validate` has checked the application identity, and unlike the
			// old create-only path it now issues a `DROP TABLE`; that is safe
			// only because the whole rebuild is inside one transaction and a
			// foreign file rolls back at the `INSERT ... FROM` step. And the
			// migration named here carries versions one to fourteen specifically,
			// written out rather than derived from the constant, so a version
			// this build has no step for is refused rather than handed to the
			// wrong one.
			if (*current < 0 || *current > 14) {
				rollback();
				return std::unexpected(failure::schema_mismatch);
			}
			const auto fresh      = *current == 0;
			const auto statements = fresh ? schema_version_1()
										  : migrate_from(*current);
			if (auto created = run_all(aDatabase, statements, aStop); !created) {
				rollback();
				return created;
			}
			// Derived from the constant rather than written out, so the stamp
			// and the validation cannot disagree.
			if (fresh) {
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
							+ std::to_string(workbench_settings_schema_version)
							+ ")",
						aStop);
					!meta) {
					rollback();
					return meta;
				}
			}
			if (auto stamped = set_version(
					aDatabase, workbench_settings_schema_version, aStop);
				!stamped) {
				rollback();
				return stamped;
			}
			if (auto admitted = preflight(aDatabase, aStop); !admitted) {
				rollback();
				return admitted;
			}
			if (auto commit = commit_transaction(aDatabase, aStop); !commit) {
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

		/// \brief Reads one stored line-number state, refusing an unknown value.
		///
		/// \details `STRICT` and the column's `CHECK` already refuse a value
		/// this build never wrote, but a durable file can be replaced wholesale
		/// by another build or by hand. The vocabulary is closed, so a value
		/// outside it is refused here rather than cast into a state the core
		/// would then be asked to reason about. `drawer_state_of` below refuses
		/// its own column for the same reason.
		[[nodiscard]] auto line_numbers_of(const std::int64_t aValue)
			-> std::expected<core::workbench_line_numbers, failure>
		{
			switch (aValue) {
			case 0:
				return core::workbench_line_numbers::hidden;
			case 1:
				return core::workbench_line_numbers::shown;
			default:
				return std::unexpected(failure::integrity_failure);
			}
		}

		/// \brief Reads one stored preference, refusing a value past its last.
		///
		/// \details [[BPR-0661]]. The vocabularies are closed and contiguous
		/// from zero, so the last enumerator bounds each column the way the
		/// `CHECK` does; a value past it is refused rather than cast.
		template <typename Preference>
		[[nodiscard]] auto preference_of(const std::int64_t aValue, const Preference aLast)
			-> std::expected<Preference, failure>
		{
			if (aValue < 0 || aValue > static_cast<std::int64_t>(aLast)) {
				return std::unexpected(failure::integrity_failure);
			}
			return static_cast<Preference>(aValue);
		}

		/// \brief Reads one stored drawer state, refusing an unknown value.
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

		/// \brief Reads one stored filter value where zero means no filter.
		///
		/// \details [[BPR-0783]]. A named value is stored one above its
		/// enumerator so that zero can mean `Any`; `github_role::none` is a role
		/// a reviewer filters by and must not collapse into no filter at all.
		template <typename Preference>
		[[nodiscard]] auto optional_preference_of(const std::int64_t aValue, const Preference aLast)
			-> std::expected<std::optional<Preference>, failure>
		{
			if (aValue == 0) {
				return std::optional<Preference>{ };
			}
			const auto value = preference_of(aValue - 1, aLast);
			if (!value) {
				return std::unexpected(value.error());
			}
			return std::optional{ *value };
		}

		/// \brief Reads one stored flag, refusing anything but zero or one.
		[[nodiscard]] auto flag_of(const std::int64_t aValue) -> std::expected<bool, failure>
		{
			if (aValue < 0 || aValue > 1) {
				return std::unexpected(failure::integrity_failure);
			}
			return aValue == 1;
		}

		/// \brief Reads one window row's stored Home view from `aFirst` onward.
		///
		/// \details [[BPR-0783]]. Every field is refused rather than cast when
		/// the stored value is outside its closed vocabulary, for the reason
		/// `preference_of` gives: a durable file can be replaced wholesale by
		/// another build or by hand, and Home would otherwise be asked to
		/// arrange its rows by a grouping that does not exist.
		[[nodiscard]] auto home_view_of(::sqlite3_stmt* aStatement, const int aFirst)
			-> std::expected<application::home_view_state, failure>
		{
			const auto grouping  = preference_of(sqlite3_column_int64(aStatement, aFirst), application::home_grouping::account);
			const auto headers   = flag_of(sqlite3_column_int64(aStatement, aFirst + 1));
			const auto collapsed = sqlite3_column_int64(aStatement, aFirst + 2);
			const auto owned     = flag_of(sqlite3_column_int64(aStatement, aFirst + 3));
			const auto role      = optional_preference_of(sqlite3_column_int64(aStatement, aFirst + 4), core::github_role::author);
			const auto addedBy   = optional_preference_of(sqlite3_column_int64(aStatement, aFirst + 5), core::observer_rule_kind::path);
			const auto quiet     = preference_of(sqlite3_column_int64(aStatement, aFirst + 6), application::queue_quiet_filter::quiet);
			const auto sort      = preference_of(sqlite3_column_int64(aStatement, aFirst + 7), application::queue_sort_order::pull_request_number);
			if (!grouping || !headers || !owned || !role || !addedBy || !quiet || !sort || collapsed < 0
				|| collapsed > application::home_collapsed_panel::all) {
				return std::unexpected(failure::integrity_failure);
			}
			return application::home_view_state{
				.Grouping     = *grouping,
				.StateHeaders = *headers,
				.Collapsed    = static_cast<std::uint8_t>(collapsed),
				.FilterOwned  = *owned,
				.Filter       = {
					.Role    = *role,
					.AddedBy = *addedBy,
					.Quiet   = *quiet,
					.Sort    = *sort,
				},
			};
		}

		/// \brief The eight stored columns of one Home view, in schema order.
		[[nodiscard]] auto home_columns_of(const application::home_view_state& aHome) -> std::array<std::int64_t, 8>
		{
			const auto named = [](const auto& aFilter) -> std::int64_t {
				return aFilter.has_value() ? static_cast<std::int64_t>(*aFilter) + 1 : 0;
			};
			return { static_cast<std::int64_t>(aHome.Grouping),
					 aHome.StateHeaders ? 1 : 0,
					 aHome.Collapsed,
					 aHome.FilterOwned ? 1 : 0,
					 named(aHome.Filter.Role),
					 named(aHome.Filter.AddedBy),
					 static_cast<std::int64_t>(aHome.Filter.Quiet),
					 static_cast<std::int64_t>(aHome.Filter.Sort) };
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
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
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
		auto owned = workbench_settings_database{ database };
		if (code != SQLITE_OK || database == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}
		return adopt(std::move(owned), std::move(aStop));
	}

	auto workbench_settings_database::open(
		state_directory& aDirectory, const std::string_view aName, std::stop_token aStop)
		-> std::expected<workbench_settings_database, failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		if (!sqlite_runtime_ready()) {
			return std::unexpected(failure::unavailable);
		}
		auto opened = open_private_store_database(
			aDirectory, aName, OpenFlags | SQLITE_OPEN_NOFOLLOW);
		if (!opened) {
			return std::unexpected(private_store_database_failure(opened.error()));
		}
		return adopt(workbench_settings_database{ *opened }, std::move(aStop));
	}

	auto workbench_settings_database::adopt(
		workbench_settings_database aOwned, std::stop_token aStop)
		-> std::expected<workbench_settings_database, failure>
	{
		auto* const database = aOwned.Handle;
		limit_connection(database);
		if (auto admitted = preflight(database, aStop); !admitted) {
			return std::unexpected(admitted.error());
		}
		if (auto configured = configure_connection(database); !configured) {
			return std::unexpected(configured.error());
		}
		limit_connection(database);
		if (auto established = establish_schema(database, aStop); !established) {
			return std::unexpected(established.error());
		}
		if (auto validated = validate(database); !validated) {
			return std::unexpected(validated.error());
		}
		return aOwned;
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
		if (!application::valid(application::window_layout(aSettings)) || !application::valid(aSettings.Behaviour) || !core::valid(aSettings.WritingHooks)
			|| !core::storable_agent_wrapper(aSettings.ClaudeWrapper)) {
			return std::unexpected(failure::integrity_failure);
		}
		if (auto begin = execute(Handle, "BEGIN IMMEDIATE", aStop); !begin) {
			return begin;
		}
		write_transaction transaction{ Handle };
		const auto        guard = progress_scope{ Handle, aStop };

		auto statement = prepare(
			Handle,
			"INSERT INTO workbench_preferences("
			"singleton, files_state, conversations_state, files_width,"
			"conversations_width, line_numbers, repositories_state,"
			"repositories_width, open_modality, continue_modality, mark_read,"
			"return_comparison, drawer_filter, drawer_controls, one_pane, writing_revision, writing_problem, writing_solution, writing_outline, writing_stop, feedback_marks,"
			"label_helper, overview_counts, label_columns, claude_wrapper) "
			"VALUES(1, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24) "
			"ON CONFLICT(singleton) DO UPDATE SET "
			"files_state = excluded.files_state,"
			"conversations_state = excluded.conversations_state,"
			"files_width = excluded.files_width,"
			"conversations_width = excluded.conversations_width,"
			"line_numbers = excluded.line_numbers,"
			"repositories_state = excluded.repositories_state,"
			"repositories_width = excluded.repositories_width,"
			"open_modality = excluded.open_modality,"
			"continue_modality = excluded.continue_modality,"
			"mark_read = excluded.mark_read,"
			"return_comparison = excluded.return_comparison,"
			"drawer_filter = excluded.drawer_filter,"
			"drawer_controls = excluded.drawer_controls,"
			"one_pane = excluded.one_pane, writing_revision = excluded.writing_revision, writing_problem = excluded.writing_problem, writing_solution = excluded.writing_solution, writing_outline = excluded.writing_outline, writing_stop = excluded.writing_stop, feedback_marks = excluded.feedback_marks,"
			"label_helper = excluded.label_helper, overview_counts = excluded.overview_counts, label_columns = excluded.label_columns,"
			"claude_wrapper = excluded.claude_wrapper");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		if (sqlite3_bind_int64(statement->Value, 15, static_cast<sqlite3_int64>(aSettings.WritingHooks.Revision)) != SQLITE_OK) {
			return std::unexpected(failure::integrity_failure);
		}
		for (std::size_t i = 0; i < core::writing_hook_count; ++i) {
			const auto& text = aSettings.WritingHooks.Overrides[i];
			if (sqlite3_bind_text(statement->Value, static_cast<int>(16 + i), text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
				return std::unexpected(failure::integrity_failure);
			}
		}
		// [[BPR-1088]]. `valid` above admitted the columns, so the text names
		// each catalog label at most once.
		const auto labelColumns = core::feedback_label_columns_text(aSettings.Behaviour.LabelColumns);
		if (sqlite3_bind_text(statement->Value, 23, labelColumns.data(), static_cast<int>(labelColumns.size()), SQLITE_TRANSIENT) != SQLITE_OK
			|| sqlite3_bind_text(statement->Value, 24, aSettings.ClaudeWrapper.data(), static_cast<int>(aSettings.ClaudeWrapper.size()), SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 21, static_cast<std::int64_t>(aSettings.Behaviour.LabelHelper)) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 22, static_cast<std::int64_t>(aSettings.Behaviour.OverviewCounts)) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 20, static_cast<std::int64_t>(aSettings.Behaviour.FeedbackMarks)) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 14, static_cast<std::int64_t>(aSettings.OnePane)) != SQLITE_OK
			|| ::sqlite3_bind_int64(
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
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   5,
				   static_cast<std::int64_t>(aSettings.LineNumbers))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   6,
				   static_cast<std::int64_t>(aSettings.Repositories))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   7,
				   static_cast<std::int64_t>(aSettings.RepositoriesWidth))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   8,
				   static_cast<std::int64_t>(aSettings.Behaviour.OpenModality))
				!= SQLITE_OK
			// [[BPR-1295]]. continue_modality is no longer read. The column
			// stays until a schema change that touches the table drops it,
			// and every write keeps it valid with 1, Review, which is what
			// Continue review enters now.
			|| ::sqlite3_bind_int64(statement->Value, 9, retired_continue_modality) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   10,
				   static_cast<std::int64_t>(aSettings.Behaviour.MarkRead))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   11,
				   static_cast<std::int64_t>(aSettings.Behaviour.ReturnComparison))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   12,
				   static_cast<std::int64_t>(aSettings.Behaviour.CommentsFilter))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   13,
				   static_cast<std::int64_t>(aSettings.Behaviour.DrawerControls))
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
		return transaction.commit(aStop);
	}

	auto workbench_settings_database::load_window(const core::bpr_window_id aWindow, std::stop_token aStop)
		-> std::expected<application::workbench_layout_settings, failure>
	{
		if (aWindow.value() == 0) {
			return std::unexpected(failure::integrity_failure);
		}
		auto defaults = load(aStop);
		if (!defaults) {
			return std::unexpected(defaults.error());
		}
		const bounded_read_scope guard{ Handle, aStop };
		auto                     statement = prepare(Handle, "SELECT files_state,conversations_state,files_width,conversations_width,line_numbers,repositories_state,repositories_width,one_pane,home_grouping,home_state_headers,home_collapsed,home_filter_owned,home_role,home_added_by,home_quiet,home_sort,geometry_x,geometry_y,geometry_width,geometry_height,geometry_maximized,updates_line,summary_fraction FROM window_layout WHERE window_id=?1");
		const auto               key       = std::to_string(aWindow.value());
		if (!statement || sqlite3_bind_text(statement->Value, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			return std::unexpected(failure::integrity_failure);
		}
		const auto stepped = sqlite3_step(statement->Value);
		if (stepped == SQLITE_DONE) {
			return defaults;
		}
		if (stepped != SQLITE_ROW) {
			return std::unexpected(catalog_failure_from_sqlite_result(stepped));
		}
		for (int column = 0; column < 23; ++column) {
			if (sqlite3_column_type(statement->Value, column) != SQLITE_INTEGER) {
				return std::unexpected(failure::integrity_failure);
			}
		}
		const auto files         = drawer_state_of(sqlite3_column_int64(statement->Value, 0));
		const auto conversations = drawer_state_of(sqlite3_column_int64(statement->Value, 1));
		const auto lines         = line_numbers_of(sqlite3_column_int64(statement->Value, 4));
		const auto repositories  = drawer_state_of(sqlite3_column_int64(statement->Value, 5));
		if (!files || !conversations || !lines || !repositories) {
			return std::unexpected(failure::integrity_failure);
		}
		for (const auto column : { 2, 3, 6 }) {
			const auto width = sqlite3_column_int64(statement->Value, column);
			if (width < 0 || width > 16384) {
				return std::unexpected(failure::integrity_failure);
			}
		}
		const auto onePane = preference_of(sqlite3_column_int64(statement->Value, 7), core::workbench_one_pane::enabled);
		if (!onePane) {
			return std::unexpected(onePane.error());
		}
		// [[BPR-0783]]. The window's own Home view. A row written before version
		// eight reads the defaults its columns were given, which is the Home it
		// was already showing.
		const auto home = home_view_of(statement->Value, 8);
		if (!home) {
			return std::unexpected(home.error());
		}
		const application::workbench_window_geometry geometry{
			static_cast<std::int32_t>(sqlite3_column_int64(statement->Value, 16)),
			static_cast<std::int32_t>(sqlite3_column_int64(statement->Value, 17)),
			static_cast<std::int32_t>(sqlite3_column_int64(statement->Value, 18)),
			static_cast<std::int32_t>(sqlite3_column_int64(statement->Value, 19)),
			sqlite3_column_int64(statement->Value, 20) != 0
		};
		// [[BPR-1130]]. The window's own fold; a row written before version ten
		// reads folded, where every window now starts.
		const auto updatesLine = preference_of(sqlite3_column_int64(statement->Value, 21), core::workbench_updates_line::open);
		if (!updatesLine) {
			return std::unexpected(updatesLine.error());
		}
		const auto fraction = sqlite3_column_int64(statement->Value, 22);
		if (fraction < 0 || fraction >= 10000) {
			return std::unexpected(failure::integrity_failure);
		}
		const application::workbench_window_layout layout{ *files, *conversations, *repositories, *lines, *onePane,
														   static_cast<std::uint32_t>(sqlite3_column_int64(statement->Value, 2)),
														   static_cast<std::uint32_t>(sqlite3_column_int64(statement->Value, 3)),
														   static_cast<std::uint32_t>(sqlite3_column_int64(statement->Value, 6)), *home, geometry, *updatesLine, fraction == 0 ? std::nullopt : std::optional{ static_cast<std::uint32_t>(fraction) } };
		if (!application::valid(layout)) {
			return std::unexpected(failure::integrity_failure);
		}
		return application::combined_layout(layout, defaults->Behaviour);
	}
	auto workbench_settings_database::store_window(const core::bpr_window_id                   aWindow,
												   const application::workbench_window_layout& aLayout, std::stop_token aStop) -> std::expected<void, failure>
	{
		if (aWindow.value() == 0 || !application::valid(aLayout)) {
			return std::unexpected(failure::integrity_failure);
		}
		if (auto admitted = preflight(Handle, aStop); !admitted) {
			return admitted;
		}
		if (auto begin = execute(Handle, "BEGIN IMMEDIATE", aStop); !begin) {
			return begin;
		}
		write_transaction        transaction{ Handle };
		const bounded_read_scope guard{ Handle, aStop };
		const auto               key      = std::to_string(aWindow.value());
		auto                     existing = prepare(Handle, "SELECT 1 FROM window_layout WHERE window_id=?1");
		if (!existing || sqlite3_bind_text(existing->Value, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			return std::unexpected(failure::integrity_failure);
		}
		const auto found = sqlite3_step(existing->Value);
		if (found != SQLITE_ROW && found != SQLITE_DONE) {
			return std::unexpected(catalog_failure_from_sqlite_result(found));
		}
		if (found == SQLITE_DONE) {
			const auto count = scalar_integer(Handle, "SELECT count(*) FROM (SELECT window_id FROM window_layout LIMIT 33)");
			if (!count || *count >= static_cast<std::int64_t>(application::maximum_workbench_window_layouts)) {
				return std::unexpected(failure::integrity_failure);
			}
		}
		static_cast<void>(sqlite3_reset(existing->Value));
		auto statement = prepare(Handle, "INSERT INTO window_layout(window_id,files_state,conversations_state,files_width,conversations_width,line_numbers,repositories_state,repositories_width,one_pane,home_grouping,home_state_headers,home_collapsed,home_filter_owned,home_role,home_added_by,home_quiet,home_sort,geometry_x,geometry_y,geometry_width,geometry_height,geometry_maximized,updates_line,summary_fraction) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24) ON CONFLICT(window_id) DO UPDATE SET files_state=excluded.files_state,conversations_state=excluded.conversations_state,files_width=excluded.files_width,conversations_width=excluded.conversations_width,line_numbers=excluded.line_numbers,repositories_state=excluded.repositories_state,repositories_width=excluded.repositories_width,one_pane=excluded.one_pane,home_grouping=excluded.home_grouping,home_state_headers=excluded.home_state_headers,home_collapsed=excluded.home_collapsed,home_filter_owned=excluded.home_filter_owned,home_role=excluded.home_role,home_added_by=excluded.home_added_by,home_quiet=excluded.home_quiet,home_sort=excluded.home_sort,geometry_x=excluded.geometry_x,geometry_y=excluded.geometry_y,geometry_width=excluded.geometry_width,geometry_height=excluded.geometry_height,geometry_maximized=excluded.geometry_maximized,updates_line=excluded.updates_line,summary_fraction=excluded.summary_fraction");
		if (!statement || sqlite3_bind_text(statement->Value, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			return std::unexpected(failure::integrity_failure);
		}
		std::array<std::int64_t, 23> values{ static_cast<std::int64_t>(aLayout.Files), static_cast<std::int64_t>(aLayout.Conversations), aLayout.FilesWidth, aLayout.ConversationsWidth, static_cast<std::int64_t>(aLayout.LineNumbers), static_cast<std::int64_t>(aLayout.Repositories), aLayout.RepositoriesWidth, static_cast<std::int64_t>(aLayout.OnePane) };
		std::ranges::copy(home_columns_of(aLayout.Home), values.begin() + 8);
		const std::array<std::int64_t, 5> geometry{ aLayout.Geometry.X, aLayout.Geometry.Y, aLayout.Geometry.Width, aLayout.Geometry.Height, aLayout.Geometry.Maximized };
		std::ranges::copy(geometry, values.begin() + 16);
		values[21] = static_cast<std::int64_t>(aLayout.UpdatesLine);
		values[22] = aLayout.SummaryFraction.value_or(0);
		for (std::size_t index = 0; index < values.size(); ++index) {
			if (sqlite3_bind_int64(statement->Value, static_cast<int>(index + 2U), values[index]) != SQLITE_OK) {
				return std::unexpected(failure::integrity_failure);
			}
		}
		const auto stepped = sqlite3_step(statement->Value);
		if (stepped != SQLITE_DONE) {
			return std::unexpected(catalog_failure_from_sqlite_result(stepped));
		}
		return transaction.commit(aStop);
	}
	auto workbench_settings_database::retain_windows(const std::span<const core::bpr_window_id> aWindows, const std::stop_token aStop) -> std::expected<void, failure>
	{
		if (aWindows.size() > core::maximum_bpr_windows_per_profile) {
			return std::unexpected(failure::integrity_failure);
		}
		for (std::size_t index = 0; index < aWindows.size(); ++index) {
			if (aWindows[index].value() == 0 || std::ranges::find(aWindows.first(index), aWindows[index]) != aWindows.first(index).end()) {
				return std::unexpected(failure::integrity_failure);
			}
		}
		if (auto admitted = preflight(Handle, aStop); !admitted) {
			return admitted;
		}
		if (auto begin = execute(Handle, "BEGIN IMMEDIATE", aStop); !begin) {
			return begin;
		}
		write_transaction transaction{ Handle };
		std::string       sql = "DELETE FROM window_layout";
		if (!aWindows.empty()) {
			sql += " WHERE window_id NOT IN (";
			for (std::size_t index = 0; index < aWindows.size(); ++index) {
				if (index != 0) {
					sql += ',';
				}
				sql += '?' + std::to_string(index + 1U);
			}
			sql += ')';
		}
		auto statement = prepare(Handle, sql);
		if (!statement) {
			return std::unexpected(failure::integrity_failure);
		}
		for (std::size_t index = 0; index < aWindows.size(); ++index) {
			const auto key = std::to_string(aWindows[index].value());
			if (sqlite3_bind_text(statement->Value, static_cast<int>(index + 1U), key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
				return std::unexpected(failure::integrity_failure);
			}
		}
		const auto stepped = sqlite3_step(statement->Value);
		if (stepped != SQLITE_DONE) {
			return std::unexpected(catalog_failure_from_sqlite_result(stepped));
		}
		return transaction.commit(aStop);
	}

	auto workbench_settings_database::erase_window(const core::bpr_window_id aWindow, std::stop_token aStop) -> std::expected<void, failure>
	{
		if (aWindow.value() == 0) {
			return std::unexpected(failure::integrity_failure);
		}
		if (auto admitted = preflight(Handle, aStop); !admitted) {
			return admitted;
		}
		if (auto begin = execute(Handle, "BEGIN IMMEDIATE", aStop); !begin) {
			return begin;
		}
		write_transaction transaction{ Handle };
		auto              statement = prepare(Handle, "DELETE FROM window_layout WHERE window_id=?1");
		const auto        key       = std::to_string(aWindow.value());
		if (!statement || sqlite3_bind_text(statement->Value, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			return std::unexpected(failure::integrity_failure);
		}
		const auto stepped = sqlite3_step(statement->Value);
		if (stepped != SQLITE_DONE) {
			return std::unexpected(catalog_failure_from_sqlite_result(stepped));
		}
		return transaction.commit(aStop);
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
			"conversations_width, line_numbers, repositories_state,"
			"repositories_width, open_modality, continue_modality, mark_read,"
			"return_comparison, drawer_filter, drawer_controls, one_pane, writing_revision, writing_problem, writing_solution, writing_outline, writing_stop, feedback_marks,"
			"label_helper, overview_counts, label_columns, claude_wrapper "
			"FROM workbench_preferences WHERE singleton = 1");
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

		const auto lineNumbers = line_numbers_of(
			::sqlite3_column_int64(statement->Value, 4));
		if (!lineNumbers) {
			return std::unexpected(lineNumbers.error());
		}
		const auto repositories = drawer_state_of(
			::sqlite3_column_int64(statement->Value, 5));
		if (!repositories) {
			return std::unexpected(repositories.error());
		}
		const auto openModality = preference_of(
			::sqlite3_column_int64(statement->Value, 7),
			application::review_open_modality::last_used);
		// [[BPR-1295]]. Column 8, continue_modality, is not read: a stored
		// Read and a stored Review both mean Review now. open_modality 1 is
		// Review under its new name.
		const auto markRead = preference_of(
			::sqlite3_column_int64(statement->Value, 9),
			application::review_mark_read_behaviour::when_fully_seen);
		const auto returnComparison = preference_of(
			::sqlite3_column_int64(statement->Value, 10),
			application::review_return_comparison::since_last_upload);
		const auto commentsFilter = preference_of(
			::sqlite3_column_int64(statement->Value, 11),
			application::review_comments_filter::unresolved);
		const auto drawerControls = preference_of(
			::sqlite3_column_int64(statement->Value, 12),
			application::review_drawer_controls::shown);
		const auto onePane       = preference_of(sqlite3_column_int64(statement->Value, 13), core::workbench_one_pane::enabled);
		const auto feedbackMarks = preference_of(
			::sqlite3_column_int64(statement->Value, 19),
			application::review_feedback_marks::letters);
		const auto labelHelper = preference_of(
			::sqlite3_column_int64(statement->Value, 20),
			application::review_label_helper::on);
		const auto overviewCounts = preference_of(
			::sqlite3_column_int64(statement->Value, 21),
			application::review_overview_counts::all);
		if (!onePane || !openModality || !markRead || !returnComparison
			|| !commentsFilter || !drawerControls || !feedbackMarks || !labelHelper || !overviewCounts) {
			return std::unexpected(failure::integrity_failure);
		}
		// [[BPR-1088]]. The label columns never fail the load: every catalog
		// ID keeps its place, and an unknown or repeated one is left out and
		// reported, so the rest of the reviewer's settings still load.
		const auto  columnsBytes = sqlite3_column_bytes(statement->Value, 22);
		const auto* columnsText  = reinterpret_cast<const char*>(sqlite3_column_text(statement->Value, 22));
		if (sqlite3_column_type(statement->Value, 22) != SQLITE_TEXT || columnsText == nullptr || columnsBytes < 0) {
			return std::unexpected(failure::integrity_failure);
		}
		const auto columns  = core::read_feedback_label_columns({ columnsText, static_cast<std::size_t>(columnsBytes) });
		auto       recovery = std::optional<application::review_label_columns_recovery>{ };
		if (columns.Unknown != 0U || columns.Repeated != 0U) {
			recovery = application::review_label_columns_recovery{ .Unknown = columns.Unknown, .Repeated = columns.Repeated };
		}

		core::writing_hook_settings hooks;
		hooks.Revision = static_cast<std::uint64_t>(sqlite3_column_int64(statement->Value, 14));
		for (std::size_t i = 0; i < core::writing_hook_count; ++i) {
			const auto  column = static_cast<int>(15 + i);
			const auto  count  = sqlite3_column_bytes(statement->Value, column);
			const auto* text   = reinterpret_cast<const char*>(sqlite3_column_text(statement->Value, column));
			if (!text || count < 0 || count > static_cast<int>(core::maximum_writing_instruction_bytes)) {
				return std::unexpected(failure::integrity_failure);
			}
			hooks.Overrides[i].assign(text, static_cast<std::size_t>(count));
		}
		if (!core::valid(hooks)) {
			return std::unexpected(failure::integrity_failure);
		}
		// [[BPR-1301]]. Read as stored, the CHECK having bounded it; whether
		// it still names a program is asked at every launch, not here.
		const auto  wrapperBytes = sqlite3_column_bytes(statement->Value, 23);
		const auto* wrapperText  = reinterpret_cast<const char*>(sqlite3_column_text(statement->Value, 23));
		if (sqlite3_column_type(statement->Value, 23) != SQLITE_TEXT || wrapperText == nullptr || wrapperBytes < 0) {
			return std::unexpected(failure::integrity_failure);
		}
		auto wrapper = std::string{ wrapperText, static_cast<std::size_t>(wrapperBytes) };
		if (!core::storable_agent_wrapper(wrapper)) {
			return std::unexpected(failure::integrity_failure);
		}
		return application::workbench_layout_settings{
			.Files         = *files,
			.Conversations = *conversations,
			.Repositories  = *repositories,
			.LineNumbers   = *lineNumbers,
			.OnePane       = *onePane,
			.FilesWidth    = static_cast<std::uint32_t>(
				::sqlite3_column_int64(statement->Value, 2)),
			.ConversationsWidth = static_cast<std::uint32_t>(
				::sqlite3_column_int64(statement->Value, 3)),
			.RepositoriesWidth = static_cast<std::uint32_t>(
				::sqlite3_column_int64(statement->Value, 6)),
			.Behaviour = {
				.OpenModality         = *openModality,
				.MarkRead             = *markRead,
				.ReturnComparison     = *returnComparison,
				.CommentsFilter       = *commentsFilter,
				.DrawerControls       = *drawerControls,
				.FeedbackMarks        = *feedbackMarks,
				.LabelHelper          = *labelHelper,
				.OverviewCounts       = *overviewCounts,
				.LabelColumns         = columns.Columns,
				.LabelColumnsRecovery = recovery,
			},
			.WritingHooks  = std::move(hooks),
			.ClaudeWrapper = std::move(wrapper),
		};
	}

	auto workbench_settings_columns(::sqlite3* const aDatabase)
		-> std::expected<std::vector<std::string>, failure>
	{
		auto tables = prepare(
			aDatabase,
			"SELECT name FROM sqlite_schema WHERE type = 'table' "
			"AND name NOT LIKE 'sqlite_%' LIMIT 4");
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
			if (names.size() >= 3U) {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto name = text_of(tables->Value, 0);
			if (name != "workbench_meta" && name != "workbench_preferences" && name != "window_layout" && name != "workbench_layout") {
				return std::unexpected(failure::schema_mismatch);
			}
			names.push_back(name);
		}

		std::ranges::sort(names);
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
				// Three admissible tables, each bounded by the connection's own
				// column limit, so this cap follows the schema rather than
				// having to be remembered whenever a column is added.
				if (found.size() >= 3U * MaximumTableColumns) {
					return std::unexpected(failure::schema_mismatch);
				}
				found.push_back(table + "." + text_of(columns->Value, 1));
			}
		}
		return found;
	}
} // namespace bpr::adapters::sqlite::detail
