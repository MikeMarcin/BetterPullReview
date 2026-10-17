#include "../../src/adapters/sqlite/src/workbench_settings_database.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::sqlite::detail;

	using bpr::tests::expect;

	/// \brief This contract's scratch root: `bpr-workbench-` and a random part.
	/// \details [[BPR-1126]]. Made at a name nothing had, owner-private from
	/// creation, and removed only by the course that made it.
	class scratch_directory final : public bpr::tests::sqlite_contract::scratch_root
	{
	public:
		scratch_directory()
			: scratch_root{ "workbench" }
		{
		}
	};

	/// \brief A layout that differs from the defaults in every field.
	[[nodiscard]] auto distinct_layout() -> application::workbench_layout_settings
	{
		return application::workbench_layout_settings{
			.Files              = core::workbench_drawer_state::collapsed,
			.Conversations      = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::hidden,
			.FilesWidth         = 512U,
			.ConversationsWidth = 288U,
		};
	}

	/// \brief Restores the pre-twenty profile and pre-nineteen Home schema of a legacy fixture, preserving its rows.
	[[nodiscard]] auto legacy_home_bound(sqlite3* aDatabase) -> sqlite3*
	{
		expect(sqlite3_exec(aDatabase, "ALTER TABLE workbench_preferences DROP COLUMN reading_width; ALTER TABLE workbench_preferences DROP COLUMN markdown_images; ALTER TABLE workbench_preferences DROP COLUMN center_ribbon_width", nullptr, nullptr, nullptr) == SQLITE_OK, "legacy fixture has no image preference or ribbon width column");
		const auto definition = scalar_text(aDatabase, "SELECT sql FROM sqlite_schema WHERE name='window_layout'");
		expect(definition.has_value(), "legacy fixture Home table exists");
		auto       sql   = *definition;
		const auto bound = sql.find("home_collapsed BETWEEN 0 AND 31");
		if (bound == std::string::npos) {
			return aDatabase;
		}
		sql.replace(bound, std::string_view{ "home_collapsed BETWEEN 0 AND 31" }.size(), "home_collapsed BETWEEN 0 AND 15");
		sql = "CREATE TABLE legacy_home_layout" + sql.substr(sql.find('('));
		sql += "; INSERT INTO legacy_home_layout SELECT * FROM window_layout; DROP TABLE window_layout; ALTER TABLE legacy_home_layout RENAME TO window_layout";
		expect(sqlite3_exec(aDatabase, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "legacy Home fixture has the real old constraint and every saved row");
		return aDatabase;
	}

	void test_authored_collapse_upgrade_and_independent_windows()
	{
		const auto scratch      = scratch_directory{ };
		const auto path         = scratch.file("workbench.sqlite3");
		auto       first        = application::workbench_window_layout{ };
		first.Home.Grouping     = application::home_grouping::organization;
		first.Home.StateHeaders = false;
		first.Home.Collapsed    = application::home_collapsed_panel::waiting_on_others;
		auto second             = application::workbench_window_layout{ };
		second.Home.FilterOwned = true;
		second.Home.Filter.Role = core::github_role::author;
		second.Home.Collapsed   = application::home_collapsed_panel::recently_viewed;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store_window(core::bpr_window_id{ 1 }, first, { }) && db->store_window(core::bpr_window_id{ 2 }, second, { }), "two distinct old Home choices seeded");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "UPDATE workbench_meta SET schema_format=18; PRAGMA user_version=18", nullptr, nullptr, nullptr) == SQLITE_OK, "exact version-eighteen Home constraint seeded");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && application::window_layout(*db->load_window(core::bpr_window_id{ 1 }, { })) == first && application::window_layout(*db->load_window(core::bpr_window_id{ 2 }, { })) == second, "upgrade preserves both views and the new authored panel starts expanded, independently of waiting collapse");
			first.Home.Collapsed |= application::home_collapsed_panel::my_pull_requests;
			expect(db->store_window(core::bpr_window_id{ 1 }, first, { }).has_value(), "authored collapse stores with the old waiting bit intact");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && application::window_layout(*db->load_window(core::bpr_window_id{ 1 }, { })) == first && application::window_layout(*db->load_window(core::bpr_window_id{ 2 }, { })) == second, "reopen retains independent authored collapse without changing the other window");
			expect(!application::valid(application::home_view_state{ .Collapsed = 32U }), "unknown collapse bits still refuse");
		}
	}

	auto test_nothing_stored_answers_the_defaults() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");

		auto fresh = workbench_settings_database::open(path, { });
		expect(fresh.has_value(), "a fresh store opens");

		const auto loaded = fresh->load({ });
		expect(loaded.has_value(), "an empty store is not a failure");
		// Compared against the record's own declaration rather than against
		// 320 written out again, so the test cannot disagree with the defaults
		// it is supposed to be checking.
		expect(
			*loaded == application::workbench_layout_settings{ },
			"an absent record answers exactly the declared defaults");

		// The other half of the same claim, and the reason both cases are
		// constructed. This port returns the record rather than an optional, so
		// a store holding the defaults deliberately is indistinguishable from
		// one holding nothing -- that is the contract, not an oversight, and it
		// differs from the appearance port on purpose.
		auto stored = fresh->store(application::workbench_layout_settings{ }, { });
		expect(stored.has_value(), "the defaults can be stored deliberately");
		const auto reloaded = fresh->load({ });
		expect(
			reloaded.has_value()
				&& *reloaded == application::workbench_layout_settings{ },
			"a deliberately stored default reads back the same as an absent one");
	}

	auto test_the_layout_round_trips_whole() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");
		auto       chosen  = distinct_layout();
		// [[BPR-0721]]. Away from its hidden default, so the column is
		// written and read rather than merely defaulted on both sides.
		chosen.Behaviour.DrawerControls    = application::review_drawer_controls::shown;
		chosen.Behaviour.CenterRibbonWidth = 56U;
		chosen.WritingHooks.Revision       = 8;
		chosen.WritingHooks.Overrides      = { "problem — preserve meaning", "solution", "outline", "comment" };

		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(opened->store(chosen, { }).has_value(), "the layout stores");
		}

		auto reopened = workbench_settings_database::open(path, { });
		expect(reopened.has_value(), "the store reopens");
		const auto loaded = reopened->load({ });
		// Compared whole. Checking field by field would pass a store that
		// silently dropped a field added later, because nobody would have
		// remembered to add it to the list.
		expect(
			loaded.has_value() && *loaded == chosen,
			"every stored field survives a close and reopen, compared whole");
	}

	void test_writing_hook_migration_and_rejection()
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("hooks.sqlite3");
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "hooks store opens");
			expect(db->store(distinct_layout(), { }).has_value(), "profile seed stores");
			application::workbench_window_layout window;
			window.UpdatesLine = core::workbench_updates_line::open;
			expect(db->store_window(core::bpr_window_id{ 1 }, window, { }).has_value(), "version10 window seed");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; ALTER TABLE workbench_preferences DROP COLUMN label_columns; ALTER TABLE workbench_preferences DROP COLUMN overview_counts; ALTER TABLE workbench_preferences DROP COLUMN label_helper; ALTER TABLE workbench_preferences DROP COLUMN feedback_marks; ALTER TABLE workbench_preferences DROP COLUMN writing_stop; ALTER TABLE workbench_preferences DROP COLUMN writing_outline; ALTER TABLE workbench_preferences DROP COLUMN writing_solution; ALTER TABLE workbench_preferences DROP COLUMN writing_problem; ALTER TABLE workbench_preferences DROP COLUMN writing_revision; UPDATE workbench_meta SET schema_format=10; PRAGMA user_version=10", nullptr, nullptr, nullptr) == SQLITE_OK, "canonical version10 source");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "version10 migration opens");
			expect(db->load({ })->WritingHooks == core::writing_hook_settings{ } && db->load_window(core::bpr_window_id{ 1 }, { })->UpdatesLine == core::workbench_updates_line::open, "migration defaults hooks and preserves Updates line");
			auto invalid                      = *db->load({ });
			invalid.WritingHooks.Overrides[0] = std::string(8193, 'x');
			expect(!db->store(invalid, { }), "oversize instructions refused");
			expect(sqlite3_exec(db->handle(), "PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET writing_problem=CAST(X'FF' AS TEXT)", nullptr, nullptr, nullptr) == SQLITE_OK, "malformed config injected");
		}
		expect(!workbench_settings_database::open(path, { }), "malformed UTF8 hook config fails closed");
	}
	/// \brief [[BPR-1083]]. How label marks are drawn: icons by default, kept
	/// as chosen, a version-eleven file migrates with its writing hooks, and a
	/// value outside the vocabulary fails closed.
	void test_feedback_marks_round_trip_and_migrate_from_eleven()
	{
		const auto scratch               = scratch_directory{ };
		const auto path                  = scratch.file("marks.sqlite3");
		auto       chosen                = distinct_layout();
		chosen.WritingHooks.Revision     = 3U;
		chosen.WritingHooks.Overrides[1] = "Say what changed.";
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && db->load({ })->Behaviour.FeedbackMarks == application::review_feedback_marks::icons, "a fresh profile draws icons");
			chosen.Behaviour.FeedbackMarks = application::review_feedback_marks::letters;
			expect(db->store(chosen, { }).has_value(), "letters store");
		}
		{
			auto       db     = workbench_settings_database::open(path, { });
			const auto loaded = db->load({ });
			expect(loaded.has_value() && loaded->Behaviour == chosen.Behaviour, "letters survive a reopen");
			expect(loaded->WritingHooks == chosen.WritingHooks, "with the writing hooks");
			expect(*loaded == chosen, "and every other field");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; ALTER TABLE workbench_preferences DROP COLUMN label_columns; ALTER TABLE workbench_preferences DROP COLUMN overview_counts; ALTER TABLE workbench_preferences DROP COLUMN label_helper; ALTER TABLE workbench_preferences DROP COLUMN feedback_marks; UPDATE workbench_meta SET schema_format=11; PRAGMA user_version=11", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "canonical version11 source");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "version11 migration opens");
			auto expected                    = chosen;
			expected.Behaviour.FeedbackMarks = application::review_feedback_marks::icons;
			expect(*db->load({ }) == expected, "a migrated row draws icons and keeps its writing hooks and every other field");
			expect(sqlite3_exec(db->handle(), "PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET feedback_marks=2", nullptr, nullptr, nullptr) == SQLITE_OK, "an out-of-range style is injected");
		}
		expect(!workbench_settings_database::open(path, { }), "a mark style outside the vocabulary fails closed");
	}

	auto test_the_layout_is_a_singleton() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");

		auto opened = workbench_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");
		expect(
			opened->store(application::workbench_layout_settings{ }, { })
				.has_value(),
			"the first layout stores");
		expect(
			opened->store(distinct_layout(), { }).has_value(),
			"a second layout replaces rather than accumulates");

		const auto rows = scalar_integer(
			opened->handle(), "SELECT count(*) FROM workbench_preferences");
		expect(
			rows.has_value() && *rows == 1,
			"a second store leaves exactly one row, so no earlier layout can "
			"be restored by accident");
		const auto loaded = opened->load({ });
		expect(
			loaded.has_value() && *loaded == distinct_layout(),
			"and the row that survives is the one most recently stored");
	}

	auto test_widths_are_bounded_by_the_published_range() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");

		auto opened = workbench_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");

		// Read from the application's own published constants. Restating 64 and
		// 4096 here would let the schema and the bound drift apart silently,
		// which is the exact failure the interpolated CHECK exists to prevent.
		auto tooNarrow       = application::workbench_layout_settings{ };
		tooNarrow.FilesWidth = application::minimum_workbench_drawer_width - 1U;
		expect(
			!opened->store(tooNarrow, { }).has_value(),
			"a width one below the published minimum is refused by the schema");

		auto tooWide               = application::workbench_layout_settings{ };
		tooWide.ConversationsWidth = application::maximum_workbench_drawer_width + 1U;
		expect(
			!opened->store(tooWide, { }).has_value(),
			"a width one above the published maximum is refused by the schema");

		auto atEdges               = application::workbench_layout_settings{ };
		atEdges.FilesWidth         = application::minimum_workbench_drawer_width;
		atEdges.ConversationsWidth = application::maximum_workbench_drawer_width;
		expect(
			opened->store(atEdges, { }).has_value(),
			"both published edges are storable, so the bounds are inclusive");

		const auto loaded = opened->load({ });
		expect(
			loaded.has_value() && *loaded == atEdges,
			"and the edge values read back unchanged rather than clamped");
	}

	auto test_a_stored_state_outside_the_vocabulary_is_refused() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");

		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(
				opened->store(distinct_layout(), { }).has_value(),
				"a legal layout stores");
		}

		// A durable file can be replaced wholesale by another build or by hand,
		// so the read guard is exercised against a row this build would never
		// write. The check constraint is suspended deliberately to produce one.
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr)
					== SQLITE_OK,
				"the stored database reopens directly");
			expect(
				::sqlite3_exec(
					raw,
					"PRAGMA ignore_check_constraints = ON;"
					"UPDATE workbench_preferences SET files_state = 7 "
					"WHERE singleton = 1",
					nullptr,
					nullptr,
					nullptr)
					== SQLITE_OK,
				"a drawer state outside the vocabulary is written past the check");
			(void)::sqlite3_close_v2(raw);
		}

		auto reopened = workbench_settings_database::open(path, { });
		expect(!reopened, "malformed stored drawer value is refused during non-mutating admission");
		expect(
			reopened.error() == catalog_database_failure::schema_mismatch,
			"and it is refused as an integrity failure, naming what is wrong");
	}

	auto test_a_newer_schema_is_refused_for_the_right_reason() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");

		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens once");
		}
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr)
					== SQLITE_OK,
				"the database reopens directly");
			const auto sql = "PRAGMA user_version = "
				+ std::to_string(workbench_settings_schema_version + 1);
			expect(
				::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr)
					== SQLITE_OK,
				"a newer schema version is stamped");
			(void)::sqlite3_close_v2(raw);
		}

		auto refused = workbench_settings_database::open(path, { });
		expect(!refused.has_value(), "the newer database is refused");
		// The reason is the assertion. Without the forward-only guard the open
		// still fails, because establish_schema would run its first CREATE
		// against a table that already exists -- so a test that stopped at "it
		// failed" would pass against a build with no forward-only policy at
		// all. This trap has already been hit twice in this repository.
		expect(
			refused.error() == catalog_database_failure::schema_mismatch,
			"because this build will not read a schema it does not know, not "
			"because it tripped over its own CREATE");
	}

	/// \brief The application identity a version-one database carried.
	///
	/// \details Duplicated from the adapter's own file-local constant on
	/// purpose. This test constructs a database as an older build left it, so
	/// it has to write that build's bytes rather than borrow today's value; if
	/// the adapter ever changes the identity, a version-one file will still
	/// have carried this one and the migration still has to cope.
	constexpr std::int32_t HistoricalApplicationId = 0x42505257;

	/// \brief Builds the exact version-one database this store used to write.
	///
	/// \details Written out rather than generated. The point of the test is
	/// that a database created by an older build still opens, so it has to
	/// embed that older build's schema; calling the current generator would
	/// prove only that the current schema migrates to itself.
	auto establish_version_one(const std::filesystem::path& aPath) -> void
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open_v2(
				   aPath.string().c_str(),
				   &raw,
				   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				   nullptr)
				   == SQLITE_OK,
			   "a version-one database is created directly");
		const auto minimum
			= std::to_string(application::minimum_workbench_drawer_width);
		const auto maximum
			= std::to_string(application::maximum_workbench_drawer_width);
		const auto sql = std::string{
			"CREATE TABLE workbench_meta("
			"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			"schema_format INTEGER NOT NULL) STRICT;"
			"CREATE TABLE workbench_layout("
			"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			"files_state INTEGER NOT NULL CHECK(files_state BETWEEN 0 AND 1),"
			"conversations_state INTEGER NOT NULL "
			"CHECK(conversations_state BETWEEN 0 AND 1),"
			"files_width INTEGER NOT NULL CHECK(files_width BETWEEN "
		} + minimum
			+ " AND " + maximum + ")," + "conversations_width INTEGER NOT NULL "
										 "CHECK(conversations_width BETWEEN "
			+ minimum + " AND " + maximum + ")) STRICT;" + "INSERT INTO workbench_meta(singleton, schema_format) VALUES(1, 1);" + "INSERT INTO workbench_layout(singleton, files_state,"
																																  "conversations_state, files_width, conversations_width) "
																																  "VALUES(1, 0, 1, 512, 288);"
			+ "PRAGMA application_id = " + std::to_string(HistoricalApplicationId) + ";" + "PRAGMA user_version = 1;";
		expect(::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr)
				   == SQLITE_OK,
			   "the version-one schema and its row are written");
		(void)::sqlite3_close_v2(raw);
	}

	[[nodiscard]] auto integer_pragma(
		const std::filesystem::path& aPath, const std::string& aPragma)
		-> std::int64_t
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open_v2(
				   aPath.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr)
				   == SQLITE_OK,
			   "the database reopens for inspection");
		::sqlite3_stmt* statement = nullptr;
		expect(::sqlite3_prepare_v2(
				   raw, aPragma.c_str(), -1, &statement, nullptr)
				   == SQLITE_OK,
			   "the inspection statement prepares");
		expect(::sqlite3_step(statement) == SQLITE_ROW,
			   "the inspection statement answers a row");
		const auto value = ::sqlite3_column_int64(statement, 0);
		(void)::sqlite3_finalize(statement);
		(void)::sqlite3_close_v2(raw);
		return value;
	}

	/// \brief Builds the exact version-two database this store used to write.
	auto establish_version_two(const std::filesystem::path& aPath) -> void
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open_v2(
				   aPath.string().c_str(),
				   &raw,
				   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				   nullptr)
				   == SQLITE_OK,
			   "a version-two database is created directly");
		const auto minimum
			= std::to_string(application::minimum_workbench_drawer_width);
		const auto maximum
			= std::to_string(application::maximum_workbench_drawer_width);
		const auto sql = std::string{
			"CREATE TABLE workbench_meta("
			"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			"schema_format INTEGER NOT NULL) STRICT;"
			"CREATE TABLE workbench_layout("
			"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			"files_state INTEGER NOT NULL CHECK(files_state BETWEEN 0 AND 1),"
			"conversations_state INTEGER NOT NULL "
			"CHECK(conversations_state BETWEEN 0 AND 1),"
			"files_width INTEGER NOT NULL CHECK(files_width BETWEEN "
		} + minimum
			+ " AND " + maximum + "),conversations_width INTEGER NOT NULL "
								  "CHECK(conversations_width BETWEEN "
			+ minimum + " AND " + maximum + "),line_numbers INTEGER NOT NULL DEFAULT 1 "
											"CHECK(line_numbers BETWEEN 0 AND 1)) STRICT;"
											"INSERT INTO workbench_meta(singleton, schema_format) VALUES(1, 2);"
											"INSERT INTO workbench_layout(singleton, files_state,"
											"conversations_state, files_width, conversations_width, line_numbers) "
											"VALUES(1, 1, 0, 400, 300, 0);"
			+ "PRAGMA application_id = " + std::to_string(HistoricalApplicationId) + ";" + "PRAGMA user_version = 2;";
		expect(::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr)
				   == SQLITE_OK,
			   "the version-two schema and its row are written");
		(void)::sqlite3_close_v2(raw);
	}

	/// \brief Builds the exact version-three database this store used to write.
	auto establish_version_three(const std::filesystem::path& aPath) -> void
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open_v2(
				   aPath.string().c_str(),
				   &raw,
				   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				   nullptr)
				   == SQLITE_OK,
			   "a version-three database is created directly");
		const auto minimum
			= std::to_string(application::minimum_workbench_drawer_width);
		const auto maximum
			= std::to_string(application::maximum_workbench_drawer_width);
		const auto sql = std::string{
			"CREATE TABLE workbench_meta("
			"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			"schema_format INTEGER NOT NULL) STRICT;"
			"CREATE TABLE workbench_layout("
			"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			"files_state INTEGER NOT NULL CHECK(files_state BETWEEN 0 AND 1),"
			"conversations_state INTEGER NOT NULL "
			"CHECK(conversations_state BETWEEN 0 AND 1),"
			"files_width INTEGER NOT NULL CHECK(files_width BETWEEN "
		} + minimum
			+ " AND " + maximum + "),conversations_width INTEGER NOT NULL "
								  "CHECK(conversations_width BETWEEN "
			+ minimum + " AND " + maximum + "),line_numbers INTEGER NOT NULL DEFAULT 1 "
											"CHECK(line_numbers BETWEEN 0 AND 1),"
											"repositories_state INTEGER NOT NULL DEFAULT 1 "
											"CHECK(repositories_state BETWEEN 0 AND 1),"
											"repositories_width INTEGER NOT NULL DEFAULT 300 "
											"CHECK(repositories_width BETWEEN "
			+ minimum + " AND " + maximum + ")) STRICT;"
											"INSERT INTO workbench_meta(singleton, schema_format) VALUES(1, 3);"
											"INSERT INTO workbench_layout(singleton, files_state,"
											"conversations_state, files_width, conversations_width, line_numbers,"
											"repositories_state, repositories_width) "
											"VALUES(1, 1, 0, 400, 300, 0, 0, 260);"
			+ "PRAGMA application_id = " + std::to_string(HistoricalApplicationId) + ";" + "PRAGMA user_version = 3;";
		expect(::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr)
				   == SQLITE_OK,
			   "the version-three schema and its row are written");
		(void)::sqlite3_close_v2(raw);
	}

	/// \brief Builds the exact version-five database this store used to write.
	///
	/// \details [[BPR-0721]]. Written out like the older fixtures: the
	/// profile table, already named `workbench_preferences`, beside the
	/// per-window table, with one profile row away from every default and
	/// one window row. The two tables share the geometry columns because the
	/// version-five build generated both from one definition. Every version
	/// has carried the same application identity.
	auto establish_version_five(const std::filesystem::path& aPath) -> void
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr)
				   == SQLITE_OK,
			   "a version-five database is created directly");
		const auto bounded = [](const std::string_view aColumn) {
			return "CHECK(" + std::string{ aColumn } + " BETWEEN "
				+ std::to_string(application::minimum_workbench_drawer_width) + " AND "
				+ std::to_string(application::maximum_workbench_drawer_width) + ")";
		};
		const auto geometry = "files_state INTEGER NOT NULL CHECK(files_state BETWEEN 0 AND 1),"
							  "conversations_state INTEGER NOT NULL CHECK(conversations_state BETWEEN 0 AND 1),"
							  "files_width INTEGER NOT NULL "
			+ bounded("files_width") + ",conversations_width INTEGER NOT NULL " + bounded("conversations_width")
			+ ",line_numbers INTEGER NOT NULL DEFAULT 1 CHECK(line_numbers BETWEEN 0 AND 1),"
			  "repositories_state INTEGER NOT NULL DEFAULT 1 CHECK(repositories_state BETWEEN 0 AND 1),"
			  "repositories_width INTEGER NOT NULL DEFAULT 300 "
			+ bounded("repositories_width");
		const auto sql = "CREATE TABLE workbench_meta(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
						 "schema_format INTEGER NOT NULL) STRICT;"
						 "CREATE TABLE workbench_preferences(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			+ geometry
			+ ",open_modality INTEGER NOT NULL DEFAULT 0 CHECK(open_modality BETWEEN 0 AND 2),"
			  "continue_modality INTEGER NOT NULL DEFAULT 0 CHECK(continue_modality BETWEEN 0 AND 1),"
			  "mark_read INTEGER NOT NULL DEFAULT 0 CHECK(mark_read BETWEEN 0 AND 1),"
			  "return_comparison INTEGER NOT NULL DEFAULT 0 CHECK(return_comparison BETWEEN 0 AND 2),"
			  "drawer_filter INTEGER NOT NULL DEFAULT 0 CHECK(drawer_filter BETWEEN 0 AND 2)) STRICT;"
			  "CREATE TABLE window_layout(window_id TEXT PRIMARY KEY NOT NULL CHECK(length(window_id) BETWEEN 1 AND 20 "
			  "AND window_id NOT GLOB '*[^0-9]*' AND substr(window_id,1,1) != '0'),"
			+ geometry
			+ ") STRICT;"
			  "INSERT INTO workbench_meta(singleton, schema_format) VALUES(1, 5);"
			  "INSERT INTO workbench_preferences(singleton, files_state, conversations_state, files_width,"
			  "conversations_width, line_numbers, repositories_state, repositories_width, open_modality,"
			  "continue_modality, mark_read, return_comparison, drawer_filter) "
			  "VALUES(1, 1, 0, 400, 300, 0, 0, 260, 2, 1, 1, 1, 2);"
			  "INSERT INTO window_layout(window_id, files_state, conversations_state, files_width,"
			  "conversations_width, line_numbers, repositories_state, repositories_width) "
			  "VALUES('7', 0, 1, 350, 410, 1, 0, 280);"
			  "PRAGMA application_id = "
			+ std::to_string(HistoricalApplicationId) + ";PRAGMA user_version = 5;";
		expect(::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
			   "the version-five schema and its rows are written");
		(void)::sqlite3_close_v2(raw);
	}

	auto establish_version_six(const std::filesystem::path& aPath) -> void
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr)
				   == SQLITE_OK,
			   "a version-six database is created directly");
		const auto bounded = [](const std::string_view aColumn) {
			return "CHECK(" + std::string{ aColumn } + " BETWEEN "
				+ std::to_string(application::minimum_workbench_drawer_width) + " AND "
				+ std::to_string(application::maximum_workbench_drawer_width) + ")";
		};
		const auto geometry = "files_state INTEGER NOT NULL CHECK(files_state BETWEEN 0 AND 1),"
							  "conversations_state INTEGER NOT NULL CHECK(conversations_state BETWEEN 0 AND 1),"
							  "files_width INTEGER NOT NULL "
			+ bounded("files_width") + ",conversations_width INTEGER NOT NULL " + bounded("conversations_width")
			+ ",line_numbers INTEGER NOT NULL DEFAULT 1 CHECK(line_numbers BETWEEN 0 AND 1),"
			  "repositories_state INTEGER NOT NULL DEFAULT 1 CHECK(repositories_state BETWEEN 0 AND 1),"
			  "repositories_width INTEGER NOT NULL DEFAULT 300 "
			+ bounded("repositories_width");
		const auto sql = "CREATE TABLE workbench_meta(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
						 "schema_format INTEGER NOT NULL) STRICT;"
						 "CREATE TABLE workbench_preferences(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			+ geometry
			+ ",open_modality INTEGER NOT NULL DEFAULT 0 CHECK(open_modality BETWEEN 0 AND 2),"
			  "continue_modality INTEGER NOT NULL DEFAULT 0 CHECK(continue_modality BETWEEN 0 AND 1),"
			  "mark_read INTEGER NOT NULL DEFAULT 0 CHECK(mark_read BETWEEN 0 AND 1),"
			  "return_comparison INTEGER NOT NULL DEFAULT 0 CHECK(return_comparison BETWEEN 0 AND 2),"
			  "drawer_filter INTEGER NOT NULL DEFAULT 0 CHECK(drawer_filter BETWEEN 0 AND 2),"
			  "drawer_controls INTEGER NOT NULL DEFAULT 0 CHECK(drawer_controls BETWEEN 0 AND 1)) STRICT;"
			  "CREATE TABLE window_layout(window_id TEXT PRIMARY KEY NOT NULL CHECK(length(window_id) BETWEEN 1 AND 20 "
			  "AND window_id NOT GLOB '*[^0-9]*' AND substr(window_id,1,1) != '0'),"
			+ geometry
			+ ") STRICT;"
			  "INSERT INTO workbench_meta(singleton, schema_format) VALUES(1, 6);"
			  "INSERT INTO workbench_preferences(singleton, files_state, conversations_state, files_width,"
			  "conversations_width, line_numbers, repositories_state, repositories_width, open_modality,"
			  "continue_modality, mark_read, return_comparison, drawer_filter, drawer_controls) "
			  "VALUES(1, 1, 0, 400, 300, 0, 0, 260, 2, 1, 1, 1, 2, 1);"
			  "INSERT INTO window_layout(window_id, files_state, conversations_state, files_width,"
			  "conversations_width, line_numbers, repositories_state, repositories_width) "
			  "VALUES('7', 0, 1, 350, 410, 1, 0, 280);"
			  "PRAGMA application_id = "
			+ std::to_string(HistoricalApplicationId) + ";PRAGMA user_version = 6;";
		expect(::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
			   "the version-six schema and its rows are written");
		(void)::sqlite3_close_v2(raw);
	}

	/// \brief [[BPR-0721]]. A version-five store gains the drawer-controls
	/// preference at hidden, keeps every profile and window value it held,
	/// the preference round-trips, and a value past it is refused.
	auto test_a_version_five_store_migrates_and_gains_the_drawer_controls() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");
		establish_version_five(path);
		const auto held = application::workbench_layout_settings{
			.Files              = core::workbench_drawer_state::expanded,
			.Conversations      = core::workbench_drawer_state::collapsed,
			.Repositories       = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::hidden,
			.FilesWidth         = 400U,
			.ConversationsWidth = 300U,
			.RepositoriesWidth  = 260U,
			.Behaviour          = {
				.OpenModality     = application::review_open_modality::last_used,
				.MarkRead         = application::review_mark_read_behaviour::when_fully_seen,
				.ReturnComparison = application::review_return_comparison::since_last_review,
				.CommentsFilter   = application::review_comments_filter::unresolved,
			},
		};
		const auto window = application::workbench_window_layout{
			.Files              = core::workbench_drawer_state::collapsed,
			.Conversations      = core::workbench_drawer_state::expanded,
			.Repositories       = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::shown,
			.FilesWidth         = 350U,
			.ConversationsWidth = 410U,
			.RepositoriesWidth  = 280U,
		};
		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "a version-five database opens rather than being refused");
			const auto loaded = opened->load({ });
			// Compared whole, so a column the rebuild forgot to copy fails here
			// rather than arriving silently at its default.
			expect(loaded.has_value() && *loaded == held,
				   "every value the version-five profile row held survived the migration");
			expect(loaded->Behaviour.DrawerControls == application::review_drawer_controls::hidden,
				   "the drawer controls arrive hidden, the owner's direction");
			const auto restored = opened->load_window(core::bpr_window_id{ 7 }, { });
			expect(restored.has_value() && application::window_layout(*restored) == window
					   && restored->Behaviour == held.Behaviour,
				   "the version-five window row survived in place beside the rebuilt profile row");
			auto changed                     = *loaded;
			changed.Behaviour.DrawerControls = application::review_drawer_controls::shown;
			expect(opened->store(changed, { }).has_value(), "the migrated store takes the new column");
			const auto reloaded = opened->load({ });
			expect(reloaded.has_value() && *reloaded == changed, "the drawer controls round-trip durably");
			const auto combined = opened->load_window(core::bpr_window_id{ 7 }, { });
			expect(combined.has_value() && application::window_layout(*combined) == window
					   && combined->Behaviour.DrawerControls == application::review_drawer_controls::shown,
				   "the window row keeps its geometry and reads the profile's drawer controls");
		}
		expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version,
			   "the migrated database is stamped with the current version");
		expect(integer_pragma(path, "SELECT schema_format FROM workbench_meta WHERE singleton = 1")
				   == workbench_settings_schema_version,
			   "the meta row agrees with the stamp");
		expect(integer_pragma(path, "PRAGMA application_id") == HistoricalApplicationId,
			   "the migration left the application identity alone");
		expect(integer_pragma(path, "SELECT count(*) FROM sqlite_schema WHERE name = 'workbench_layout'") == 0,
			   "the renamed version-five table is gone");
		{
			auto reopened = workbench_settings_database::open(path, { });
			expect(reopened.has_value(), "the migrated store reopens at the current version");
		}
		{
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open_v2(path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
				   "the stored database reopens directly");
			expect(::sqlite3_exec(raw,
								  "PRAGMA ignore_check_constraints = ON;"
								  "UPDATE workbench_preferences SET drawer_controls = 2 WHERE singleton = 1",
								  nullptr,
								  nullptr,
								  nullptr)
					   == SQLITE_OK,
				   "drawer controls past the vocabulary are written past the check");
			(void)::sqlite3_close_v2(raw);
		}
		auto refused = workbench_settings_database::open(path, { });
		expect(!refused.has_value() && refused.error() == catalog_database_failure::schema_mismatch,
			   "drawer controls past the closed vocabulary are refused before writable admission");
	}

	/// \brief [[BPR-0661]]. A version-three store gains the review-behaviour
	/// preferences at their defaults and keeps every value it held, the
	/// preferences round-trip, and a value past a vocabulary is refused.
	auto test_a_version_six_store_migrates_and_gains_one_pane() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");
		establish_version_six(path);
		const auto held = application::workbench_layout_settings{
			.Files              = core::workbench_drawer_state::expanded,
			.Conversations      = core::workbench_drawer_state::collapsed,
			.Repositories       = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::hidden,
			.FilesWidth         = 400U,
			.ConversationsWidth = 300U,
			.RepositoriesWidth  = 260U,
			.Behaviour          = {
				.OpenModality     = application::review_open_modality::last_used,
				.MarkRead         = application::review_mark_read_behaviour::when_fully_seen,
				.ReturnComparison = application::review_return_comparison::since_last_review,
				.CommentsFilter   = application::review_comments_filter::unresolved,
				.DrawerControls   = application::review_drawer_controls::shown,
			},
		};
		const auto window = application::workbench_window_layout{
			.Files              = core::workbench_drawer_state::collapsed,
			.Conversations      = core::workbench_drawer_state::expanded,
			.Repositories       = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::shown,
			.FilesWidth         = 350U,
			.ConversationsWidth = 410U,
			.RepositoriesWidth  = 280U,
		};
		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "a version-six database opens rather than being refused");
			const auto loaded = opened->load({ });
			// Compared whole, so a column the rebuild forgot to copy fails here
			// rather than arriving silently at its default.
			expect(loaded.has_value() && *loaded == held,
				   "every value the version-six profile row held survived the migration");
			expect(loaded->OnePane == core::workbench_one_pane::disabled,
				   "the migrated pane preference defaults to two panes");
			const auto restored = opened->load_window(core::bpr_window_id{ 7 }, { });
			expect(restored.has_value() && application::window_layout(*restored) == window
					   && restored->Behaviour == held.Behaviour,
				   "the version-six window row survived in place beside the rebuilt profile row");
			auto changed    = *loaded;
			changed.OnePane = core::workbench_one_pane::enabled;
			expect(opened->store(changed, { }).has_value(), "the migrated store takes the new column");
			const auto reloaded = opened->load({ });
			expect(reloaded.has_value() && *reloaded == changed, "one pane round-trips durably");
			auto changedWindow    = window;
			changedWindow.OnePane = core::workbench_one_pane::enabled;
			expect(opened->store_window(core::bpr_window_id{ 7 }, changedWindow, { }).has_value(), "one-pane window stores");
			const auto combined = opened->load_window(core::bpr_window_id{ 7 }, { });
			expect(combined.has_value() && application::window_layout(*combined) == changedWindow
					   && combined->Behaviour.DrawerControls == application::review_drawer_controls::shown,
				   "the window row keeps its geometry and reads the profile's drawer controls");
		}
		expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version,
			   "the migrated database is stamped with the current version");
		expect(integer_pragma(path, "SELECT schema_format FROM workbench_meta WHERE singleton = 1")
				   == workbench_settings_schema_version,
			   "the meta row agrees with the stamp");
		expect(integer_pragma(path, "PRAGMA application_id") == HistoricalApplicationId,
			   "the migration left the application identity alone");
		expect(integer_pragma(path, "SELECT count(*) FROM sqlite_schema WHERE name = 'workbench_layout'") == 0,
			   "the renamed version-six table is gone");
		{
			auto reopened = workbench_settings_database::open(path, { });
			expect(reopened.has_value(), "the migrated store reopens at the current version");
		}
		{
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open_v2(path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
				   "the stored database reopens directly");
			expect(::sqlite3_exec(raw,
								  "PRAGMA ignore_check_constraints = ON;"
								  "UPDATE workbench_preferences SET one_pane = 2 WHERE singleton = 1",
								  nullptr,
								  nullptr,
								  nullptr)
					   == SQLITE_OK,
				   "one pane past the vocabulary are written past the check");
			(void)::sqlite3_close_v2(raw);
		}
		auto refused = workbench_settings_database::open(path, { });
		expect(!refused.has_value() && refused.error() == catalog_database_failure::schema_mismatch,
			   "one pane past the closed vocabulary are refused before writable admission");
	}

	/// \brief Builds the exact version-seven database this store used to write.
	///
	/// \details [[BPR-0783]]. Written out like the older fixtures: the profile
	/// row away from every default with one pane enabled, and two window rows
	/// rather than one, so the migration has to carry each window's own values
	/// instead of any one row's. The first window has one pane enabled and the
	/// second does not, which is the pair a copy that dropped the column would
	/// silently flatten.
	auto establish_version_seven(const std::filesystem::path& aPath) -> void
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr)
				   == SQLITE_OK,
			   "a version-seven database is created directly");
		const auto bounded = [](const std::string_view aColumn) {
			return "CHECK(" + std::string{ aColumn } + " BETWEEN "
				+ std::to_string(application::minimum_workbench_drawer_width) + " AND "
				+ std::to_string(application::maximum_workbench_drawer_width) + ")";
		};
		const auto geometry = "files_state INTEGER NOT NULL CHECK(files_state BETWEEN 0 AND 1),"
							  "conversations_state INTEGER NOT NULL CHECK(conversations_state BETWEEN 0 AND 1),"
							  "files_width INTEGER NOT NULL "
			+ bounded("files_width") + ",conversations_width INTEGER NOT NULL " + bounded("conversations_width")
			+ ",line_numbers INTEGER NOT NULL DEFAULT 1 CHECK(line_numbers BETWEEN 0 AND 1),"
			  "one_pane INTEGER NOT NULL DEFAULT 0 CHECK(one_pane BETWEEN 0 AND 1),"
			  "repositories_state INTEGER NOT NULL DEFAULT 1 CHECK(repositories_state BETWEEN 0 AND 1),"
			  "repositories_width INTEGER NOT NULL DEFAULT 300 "
			+ bounded("repositories_width");
		const auto sql = "CREATE TABLE workbench_meta(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
						 "schema_format INTEGER NOT NULL) STRICT;"
						 "CREATE TABLE workbench_preferences(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
			+ geometry
			+ ",open_modality INTEGER NOT NULL DEFAULT 0 CHECK(open_modality BETWEEN 0 AND 2),"
			  "continue_modality INTEGER NOT NULL DEFAULT 0 CHECK(continue_modality BETWEEN 0 AND 1),"
			  "mark_read INTEGER NOT NULL DEFAULT 0 CHECK(mark_read BETWEEN 0 AND 1),"
			  "return_comparison INTEGER NOT NULL DEFAULT 0 CHECK(return_comparison BETWEEN 0 AND 2),"
			  "drawer_filter INTEGER NOT NULL DEFAULT 0 CHECK(drawer_filter BETWEEN 0 AND 2),"
			  "drawer_controls INTEGER NOT NULL DEFAULT 0 CHECK(drawer_controls BETWEEN 0 AND 1)) STRICT;"
			  "CREATE TABLE window_layout(window_id TEXT PRIMARY KEY NOT NULL CHECK(length(window_id) BETWEEN 1 AND 20 "
			  "AND window_id NOT GLOB '*[^0-9]*' AND substr(window_id,1,1) != '0'),"
			+ geometry
			+ ") STRICT;"
			  "INSERT INTO workbench_meta(singleton, schema_format) VALUES(1, 7);"
			  "INSERT INTO workbench_preferences(singleton, files_state, conversations_state, files_width,"
			  "conversations_width, line_numbers, one_pane, repositories_state, repositories_width, open_modality,"
			  "continue_modality, mark_read, return_comparison, drawer_filter, drawer_controls) "
			  "VALUES(1, 1, 0, 400, 300, 0, 1, 0, 260, 2, 1, 1, 1, 2, 1);"
			  "INSERT INTO window_layout(window_id, files_state, conversations_state, files_width,"
			  "conversations_width, line_numbers, one_pane, repositories_state, repositories_width) "
			  "VALUES('7', 0, 1, 350, 410, 1, 1, 0, 280);"
			  "INSERT INTO window_layout(window_id, files_state, conversations_state, files_width,"
			  "conversations_width, line_numbers, one_pane, repositories_state, repositories_width) "
			  "VALUES('9', 1, 0, 500, 260, 0, 0, 1, 330);"
			  "PRAGMA application_id = "
			+ std::to_string(HistoricalApplicationId) + ";PRAGMA user_version = 7;";
		expect(::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
			   "the version-seven schema and its rows are written");
		(void)::sqlite3_close_v2(raw);
	}

	/// \brief [[BPR-0783]]. A version-seven store gains the per-window Home
	/// view at its defaults and keeps every profile and window value it held,
	/// one pane included; two windows then carry their own view independently
	/// across a reopen; the profile row never carries one; and a stored value
	/// outside a vocabulary is refused by the read and by admission.
	auto test_a_version_seven_store_migrates_and_gains_the_home_view() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");
		establish_version_seven(path);
		const auto held = application::workbench_layout_settings{
			.Files              = core::workbench_drawer_state::expanded,
			.Conversations      = core::workbench_drawer_state::collapsed,
			.Repositories       = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::hidden,
			.OnePane            = core::workbench_one_pane::enabled,
			.FilesWidth         = 400U,
			.ConversationsWidth = 300U,
			.RepositoriesWidth  = 260U,
			.Behaviour          = {
				.OpenModality     = application::review_open_modality::last_used,
				.MarkRead         = application::review_mark_read_behaviour::when_fully_seen,
				.ReturnComparison = application::review_return_comparison::since_last_review,
				.CommentsFilter   = application::review_comments_filter::unresolved,
				.DrawerControls   = application::review_drawer_controls::shown,
			},
		};
		const auto first = application::workbench_window_layout{
			.Files              = core::workbench_drawer_state::collapsed,
			.Conversations      = core::workbench_drawer_state::expanded,
			.Repositories       = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::shown,
			.OnePane            = core::workbench_one_pane::enabled,
			.FilesWidth         = 350U,
			.ConversationsWidth = 410U,
			.RepositoriesWidth  = 280U,
		};
		const auto second = application::workbench_window_layout{
			.Files              = core::workbench_drawer_state::expanded,
			.Conversations      = core::workbench_drawer_state::collapsed,
			.Repositories       = core::workbench_drawer_state::expanded,
			.LineNumbers        = core::workbench_line_numbers::hidden,
			.OnePane            = core::workbench_one_pane::disabled,
			.FilesWidth         = 500U,
			.ConversationsWidth = 260U,
			.RepositoriesWidth  = 330U,
		};
		const auto left = application::home_view_state{
			.Grouping     = application::home_grouping::repository,
			.StateHeaders = false,
			.Collapsed    = static_cast<std::uint8_t>(application::home_collapsed_panel::needs_me
													  | application::home_collapsed_panel::following),
			.FilterOwned  = true,
			.Filter       = {
				.Role    = core::github_role::author,
				.AddedBy = core::observer_rule_kind::path,
				.Quiet   = application::queue_quiet_filter::quiet,
				.Sort    = application::queue_sort_order::pull_request_number,
			},
		};
		// `none` is a role a reviewer can filter by and zero means no filter at
		// all, so this view is the one that proves the two are stored apart:
		// every filter here is the first value of its vocabulary.
		const auto right = application::home_view_state{
			.Grouping    = application::home_grouping::account,
			.Collapsed   = application::home_collapsed_panel::all,
			.FilterOwned = true,
			.Filter      = {
				.Role    = core::github_role::none,
				.AddedBy = core::observer_rule_kind::repository,
				.Quiet   = application::queue_quiet_filter::attention,
				.Sort    = application::queue_sort_order::repository,
			},
		};
		auto leftWindow  = first;
		leftWindow.Home  = left;
		auto rightWindow = second;
		rightWindow.Home = right;
		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "a version-seven database opens rather than being refused");
			const auto loaded = opened->load({ });
			// Compared whole, so a column the rebuild forgot to copy fails here
			// rather than arriving silently at its default.
			expect(loaded.has_value() && *loaded == held,
				   "every value the version-seven profile row held survived the migration");
			expect(loaded->Home == application::home_view_state{ },
				   "the profile row answers the default Home view, which is the one it stores");
			const auto restored = opened->load_window(core::bpr_window_id{ 7 }, { });
			const auto sibling  = opened->load_window(core::bpr_window_id{ 9 }, { });
			expect(restored.has_value() && application::window_layout(*restored) == first
					   && restored->Behaviour == held.Behaviour,
				   "the first version-seven window row survived in place, one pane included");
			expect(sibling.has_value() && application::window_layout(*sibling) == second,
				   "and so did the second, with its own values rather than the first's");

			expect(opened->store_window(core::bpr_window_id{ 7 }, leftWindow, { }).has_value()
					   && opened->store_window(core::bpr_window_id{ 9 }, rightWindow, { }).has_value(),
				   "the migrated store takes a Home view for each window");
			expect(application::window_layout(*opened->load_window(core::bpr_window_id{ 7 }, { })) == leftWindow
					   && application::window_layout(*opened->load_window(core::bpr_window_id{ 9 }, { })) == rightWindow,
				   "two windows of one profile round-trip distinct Home views independently");
			expect(opened->load({ }) == std::expected<application::workbench_layout_settings, failure>{ held },
				   "a window's view cannot reach the profile row or replace what it holds");
			auto invalid           = leftWindow;
			invalid.Home.Collapsed = static_cast<std::uint8_t>(application::home_collapsed_panel::all + 1U);
			expect(!opened->store_window(core::bpr_window_id{ 7 }, invalid, { }),
				   "a collapsed bit outside the view cannot be stored");
			invalid               = leftWindow;
			invalid.Home.Grouping = static_cast<application::home_grouping>(4);
			expect(!opened->store_window(core::bpr_window_id{ 7 }, invalid, { }),
				   "nor can a grouping outside the vocabulary");
		}
		expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version,
			   "the migrated database is stamped with the current version");
		expect(integer_pragma(path, "SELECT schema_format FROM workbench_meta WHERE singleton = 1")
				   == workbench_settings_schema_version,
			   "the meta row agrees with the stamp");
		expect(integer_pragma(path, "PRAGMA application_id") == HistoricalApplicationId,
			   "the migration left the application identity alone");
		expect(integer_pragma(path, "SELECT count(*) FROM sqlite_schema WHERE name = 'workbench_layout'") == 0,
			   "the renamed version-seven table is gone");
		expect(integer_pragma(path, "SELECT count(*) FROM sqlite_schema WHERE name = 'old_window_layout'") == 0,
			   "and so is the renamed window table the rebuild copied from");
		{
			auto reopened = workbench_settings_database::open(path, { });
			expect(reopened.has_value(), "the migrated store reopens at the current version");
			expect(application::window_layout(*reopened->load_window(core::bpr_window_id{ 7 }, { })) == leftWindow
					   && application::window_layout(*reopened->load_window(core::bpr_window_id{ 9 }, { })) == rightWindow,
				   "and both windows still hold their own view after a close and reopen");
		}

		// A durable file can be replaced wholesale by another build or by hand,
		// so every column's read guard is exercised against a row this build
		// would never write. The check constraint is suspended to produce one.
		for (const auto mutation : { "home_grouping=4", "home_state_headers=2", "home_collapsed=32",
									 "home_filter_owned=2", "home_role=6", "home_role=-1", "home_added_by=4",
									 "home_quiet=3", "home_sort=3" }) {
			const auto hostile     = scratch_directory{ };
			const auto hostilePath = hostile.file("home.sqlite3");
			{
				auto database = workbench_settings_database::open(hostilePath, { });
				expect(database.has_value(), "the hostile Home fixture opens");
				expect(database->store_window(core::bpr_window_id{ 4 }, leftWindow, { }).has_value(),
					   "a legal window view stores first");
				const auto sql = std::string{ "PRAGMA ignore_check_constraints=ON; UPDATE window_layout SET " } + mutation;
				expect(sqlite3_exec(database->handle(), sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
					   "a Home value outside its vocabulary is written past the check");
				const auto refused = database->load_window(core::bpr_window_id{ 4 }, { });
				expect(!refused && refused.error() == failure::integrity_failure,
					   "the read refuses a stored Home value this build never wrote rather than casting it");
			}
			const auto reopened = workbench_settings_database::open(hostilePath, { });
			expect(!reopened && reopened.error() == failure::schema_mismatch,
				   "and the same row refuses admission before the store becomes writable");
		}
	}

	/// \brief [[BPR-0661]]. A version-three store gains the review-behaviour
	/// preferences at their defaults and keeps every value it held, the
	/// preferences round-trip, and a value past a vocabulary is refused.
	auto test_a_version_three_store_migrates_and_gains_the_behaviour_preferences() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");
		establish_version_three(path);
		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "a version-three database opens rather than being refused");
			const auto loaded = opened->load({ });
			expect(loaded.has_value(), "the migrated layout loads");
			expect(loaded->Files == core::workbench_drawer_state::expanded
					   && loaded->Conversations == core::workbench_drawer_state::collapsed
					   && loaded->FilesWidth == 400U && loaded->ConversationsWidth == 300U
					   && loaded->LineNumbers == core::workbench_line_numbers::hidden
					   && loaded->Repositories == core::workbench_drawer_state::collapsed
					   && loaded->RepositoriesWidth == 260U,
				   "every value the version-three row held survived the migration, the "
				   "repositories drawer included");
			expect(loaded->Behaviour == application::review_behaviour_settings{ },
				   "the preferences arrive at their defaults, the behaviour the window had");
			auto changed      = *loaded;
			changed.Behaviour = application::review_behaviour_settings{
				.OpenModality     = application::review_open_modality::last_used,
				.MarkRead         = application::review_mark_read_behaviour::when_fully_seen,
				.ReturnComparison = application::review_return_comparison::since_last_upload,
				.CommentsFilter   = application::review_comments_filter::unresolved,
			};
			expect(opened->store(changed, { }).has_value(), "the migrated store takes the new columns");
			const auto reloaded = opened->load({ });
			expect(reloaded.has_value() && *reloaded == changed,
				   "every preference round-trips durably beside the layout");
		}
		expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version,
			   "the migrated database is stamped with the current version");
		expect(integer_pragma(path, "SELECT schema_format FROM workbench_meta WHERE singleton = 1")
				   == workbench_settings_schema_version,
			   "the meta row agrees with the stamp");
		{
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open_v2(path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
				   "the stored database reopens directly");
			expect(::sqlite3_exec(raw,
								  "PRAGMA ignore_check_constraints = ON;"
								  "UPDATE workbench_preferences SET return_comparison = 3 WHERE singleton = 1",
								  nullptr,
								  nullptr,
								  nullptr)
					   == SQLITE_OK,
				   "a preference past its vocabulary is written past the check");
			(void)::sqlite3_close_v2(raw);
		}
		auto reopened = workbench_settings_database::open(path, { });
		expect(!reopened.has_value() && reopened.error() == catalog_database_failure::schema_mismatch,
			   "a preference past its closed vocabulary is refused before writable admission");
	}

	/// \brief [[BPR-0651]]. A version-two store gains the repositories drawer.
	auto test_a_version_two_store_migrates_and_gains_the_repositories_drawer() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");
		establish_version_two(path);
		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(), "a version-two database opens rather than being refused");
			const auto loaded = opened->load({ });
			expect(loaded.has_value(), "the migrated layout loads");
			expect(loaded->Files == core::workbench_drawer_state::expanded
					   && loaded->Conversations == core::workbench_drawer_state::collapsed
					   && loaded->FilesWidth == 400U && loaded->ConversationsWidth == 300U
					   && loaded->LineNumbers == core::workbench_line_numbers::hidden,
				   "every value the version-two row held survived the migration");
			expect(loaded->Repositories == core::workbench_drawer_state::expanded
					   && loaded->RepositoriesWidth == 300U,
				   "the repositories drawer arrives open at the design's width");
			auto changed              = *loaded;
			changed.Repositories      = core::workbench_drawer_state::collapsed;
			changed.RepositoriesWidth = 260U;
			expect(opened->store(changed, { }).has_value(), "the migrated store takes the new columns");
			const auto reloaded = opened->load({ });
			expect(reloaded.has_value() && reloaded->Repositories == core::workbench_drawer_state::collapsed
					   && reloaded->RepositoriesWidth == 260U,
				   "the repositories drawer round-trips durably");
		}
		expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version,
			   "the migrated database is stamped with the current version");
		expect(integer_pragma(path, "SELECT schema_format FROM workbench_meta WHERE singleton = 1")
				   == workbench_settings_schema_version,
			   "the meta row agrees with the stamp");
	}

	auto test_a_version_one_store_migrates_and_keeps_its_row() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");
		establish_version_one(path);

		{
			auto opened = workbench_settings_database::open(path, { });
			expect(opened.has_value(),
				   "a version-one database opens rather than being refused");

			const auto loaded = opened->load({ });
			expect(loaded.has_value(), "the migrated layout loads");
			// The row is the assertion. A migration that recreated the table
			// would also open cleanly and would also answer a valid layout --
			// the defaults -- so opening proves nothing on its own.
			expect(loaded->Files == core::workbench_drawer_state::collapsed
					   && loaded->Conversations
						   == core::workbench_drawer_state::expanded
					   && loaded->FilesWidth == 512U
					   && loaded->ConversationsWidth == 288U,
				   "every value the version-one row held survived the "
				   "migration");
			expect(loaded->LineNumbers == core::workbench_line_numbers::shown,
				   "the added column defaults to shown, so a reviewer who "
				   "upgrades sees the numbering they already had");
		}

		expect(integer_pragma(path, "PRAGMA user_version")
				   == workbench_settings_schema_version,
			   "the migrated database is stamped with the current version");
		expect(integer_pragma(
				   path,
				   "SELECT schema_format FROM workbench_meta WHERE singleton = 1")
				   == workbench_settings_schema_version,
			   "the meta row agrees with the stamp rather than still saying one");
		expect(integer_pragma(path, "PRAGMA application_id")
				   == HistoricalApplicationId,
			   "the migration left the application identity alone");

		// And the migrated store is a working store, not merely a readable one.
		{
			auto reopened = workbench_settings_database::open(path, { });
			expect(reopened.has_value(), "the migrated store reopens");
			auto chosen        = distinct_layout();
			chosen.LineNumbers = core::workbench_line_numbers::hidden;
			expect(reopened->store(chosen, { }).has_value(),
				   "the migrated store accepts a write to the new column");
			const auto loaded = reopened->load({ });
			expect(loaded.has_value() && *loaded == chosen,
				   "the new column round-trips through a migrated table");
		}
	}

	auto test_cancellation_is_reported_distinctly() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");

		auto opened = workbench_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");

		const auto established = application::workbench_layout_settings{ };
		expect(
			opened->store(established, { }).has_value(),
			"a layout is established first");

		auto stop = std::stop_source{ };
		stop.request_stop();

		const auto stored = opened->store(distinct_layout(), stop.get_token());
		expect(!stored.has_value(), "a cancelled store does not succeed");

		// The assertion that makes the guard load-bearing. Reporting
		// cancellation *after* writing satisfies the line above and is still
		// wrong: the caller is told their write did not happen while the bytes
		// on disk say it did. The progress handler runs every 256 virtual-machine
		// steps and this statement finishes in fewer, so nothing but an explicit
		// check before the write can hold this.
		const auto survived = opened->load({ });
		expect(
			survived.has_value() && *survived == established,
			"and a cancelled store leaves the stored layout untouched rather "
			"than writing and then reporting cancellation");
		// Distinct from unavailable on purpose: retrying after a caller's own
		// cancellation is correct, and retrying an unusable store is a loop.
		expect(
			stored.error() == catalog_database_failure::cancelled,
			"a cancelled store reports cancellation, not unavailability");

		const auto loaded = opened->load(stop.get_token());
		expect(
			!loaded.has_value()
				&& loaded.error() == catalog_database_failure::cancelled,
			"a cancelled load reports cancellation too");
	}

	auto test_the_schema_stores_no_review_content() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("workbench.sqlite3");

		auto opened = workbench_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");

		const auto columns = workbench_settings_columns(opened->handle());
		expect(columns.has_value(), "the created schema reports its columns");

		// Read off the schema the build actually creates, not off the insert
		// statements somebody remembered to read. Drawer geometry is a
		// preference; this file must not become a record of what was reviewed.
		constexpr auto prohibited = std::array{
			std::string_view{ "path" },
			std::string_view{ "repository" },
			std::string_view{ "pull" },
			std::string_view{ "host" },
			std::string_view{ "account" },
			std::string_view{ "title" },
			std::string_view{ "body" },
			std::string_view{ "comment" },
			std::string_view{ "branch" },
			std::string_view{ "token" },
		};
		for (const auto& column : *columns) {
			for (const auto banned : prohibited) {
				expect(
					column.find(banned) == std::string::npos,
					"the workbench layout schema names no review content: "
					"offending column is "
						+ std::string{ column });
			}
		}
		expect(!columns->empty(), "the schema declares columns at all");
	}
	void test_window_rows_are_independent_from_profile_defaults()
	{
		const auto scratch  = scratch_directory{ };
		auto       database = workbench_settings_database::open(scratch.file("windows.sqlite3"), { });
		expect(database.has_value(), "window store opens");
		auto defaults                   = distinct_layout();
		defaults.Behaviour.OpenModality = application::review_open_modality::review;
		defaults.OnePane                = core::workbench_one_pane::enabled;
		expect(database->store(defaults, { }).has_value(), "profile defaults and behaviour are durable");
		const auto first  = core::bpr_window_id{ std::numeric_limits<std::uint64_t>::max() };
		const auto second = core::bpr_window_id{ 2 };
		expect(database->load_window(first, { }) == database->load({ }), "missing window reads profile defaults without invention");
		expect(*scalar_integer(database->handle(), "SELECT count(*) FROM window_layout") == 0, "reading defaults creates zero window records");
		auto left         = application::window_layout(defaults);
		left.FilesWidth   = 444;
		auto right        = left;
		right.FilesWidth  = 777;
		right.LineNumbers = core::workbench_line_numbers::shown;
		right.OnePane     = core::workbench_one_pane::disabled;
		right.Files       = core::workbench_drawer_state::expanded;
		expect(database->store_window(first, left, { }).has_value() && database->store_window(second, right, { }).has_value(), "full-width stable window IDs persist independent layouts");
		expect(application::window_layout(*database->load_window(first, { })) == left && application::window_layout(*database->load_window(second, { })) == right, "drawer width/state and numbering remain window-local");
		expect(database->load({ }) == std::expected<application::workbench_layout_settings, failure>{ defaults }, "window writes cannot replace profile behaviour or defaults");
		expect(!database->store_window({ }, left, { }), "zero is not a persisted window identity");
		for (std::uint64_t id = 3; id <= 32; ++id) {
			expect(database->store_window(core::bpr_window_id{ id }, left, { }).has_value(), "window rows fill bounded capacity");
		}
		expect(!database->store_window(core::bpr_window_id{ 33 }, left, { }), "one beyond window capacity refuses without eviction");
		expect(database->store_window(first, right, { }).has_value(), "full capacity still permits exact existing-window update");
		expect(database->erase_window(second, { }).has_value(), "typed erase removes only requested window");
		expect(application::window_layout(*database->load_window(first, { })) == right && database->load_window(second, { }) == database->load({ }), "erase preserves sibling and restores missing row defaults");
		expect(database->store_window(core::bpr_window_id{ 33 }, left, { }).has_value(), "explicit erase releases capacity");
		auto stop = std::stop_source{ };
		stop.request_stop();
		expect(!database->erase_window(first, stop.get_token()) && application::window_layout(*database->load_window(first, { })) == right, "cancelled erase leaves exact window untouched");
	}
	void create_version_four(const std::filesystem::path& aPath)
	{
		{
			auto database                   = workbench_settings_database::open(aPath, { });
			auto record                     = distinct_layout();
			record.Behaviour.CommentsFilter = application::review_comments_filter::unresolved;
			expect(database && database->store(record, { }).has_value(), "canonical v4 fixture seeds all legacy fields");
		}
		sqlite3* raw = nullptr;
		expect(sqlite3_open(aPath.string().c_str(), &raw) == SQLITE_OK, "legacy fixture raw opens");
		// [[BPR-0721]]. Version four predates the drawer-controls column, and
		// DROP COLUMN rewrites the stored definition back to the text a
		// version-four build wrote.
		expect(sqlite3_exec(legacy_home_bound(raw), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; ALTER TABLE workbench_preferences DROP COLUMN label_columns; ALTER TABLE workbench_preferences DROP COLUMN overview_counts; ALTER TABLE workbench_preferences DROP COLUMN label_helper; ALTER TABLE workbench_preferences DROP COLUMN feedback_marks; ALTER TABLE workbench_preferences DROP COLUMN writing_stop;ALTER TABLE workbench_preferences DROP COLUMN writing_outline;ALTER TABLE workbench_preferences DROP COLUMN writing_solution;ALTER TABLE workbench_preferences DROP COLUMN writing_problem;ALTER TABLE workbench_preferences DROP COLUMN writing_revision;ALTER TABLE workbench_preferences RENAME TO workbench_layout; ALTER TABLE workbench_layout DROP COLUMN drawer_controls; ALTER TABLE workbench_layout DROP COLUMN one_pane; DROP TABLE window_layout; UPDATE workbench_meta SET schema_format=4; PRAGMA user_version=4", nullptr, nullptr, nullptr) == SQLITE_OK, "canonical legacy singleton fixture is isolated");
		sqlite3_close(raw);
	}
	auto logical_snapshot(const std::filesystem::path& aPath, const bool aLegacy) -> std::vector<std::string>
	{
		sqlite3* raw = nullptr;
		expect(sqlite3_open(aPath.string().c_str(), &raw) == SQLITE_OK, "snapshot raw opens");
		std::vector<std::string> cells;
		const auto               collect = [](void* aContext, int aCount, char** aValues, char**) -> int {
			auto& output = *static_cast<std::vector<std::string>*>(aContext);
			for (int index = 0; index < aCount; ++index) {
				output.emplace_back(aValues[index] ? aValues[index] : "<NULL>");
			}
			return 0;
		};
		const auto sql = std::string{ "PRAGMA application_id; PRAGMA user_version; SELECT type,name,sql FROM sqlite_schema ORDER BY name; SELECT * FROM workbench_meta; SELECT * FROM " }
			+ (aLegacy ? "workbench_layout" : "workbench_preferences; SELECT * FROM window_layout ORDER BY window_id");
		expect(sqlite3_exec(raw, sql.c_str(), collect, &cells, nullptr) == SQLITE_OK, "complete schema and rows captured");
		sqlite3_close(raw);
		return cells;
	}
	void test_hostile_window_rows_refuse_unchanged()
	{
		for (const auto mutation : {
				 "WITH RECURSIVE ids(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM ids WHERE id<33) INSERT INTO window_layout(window_id,files_state,conversations_state,files_width,conversations_width,line_numbers,repositories_state,repositories_width) SELECT CAST(id AS TEXT),0,0,300,300,0,0,300 FROM ids",
				 "PRAGMA ignore_check_constraints=ON; INSERT INTO window_layout(window_id,files_state,conversations_state,files_width,conversations_width,line_numbers,repositories_state,repositories_width) VALUES(printf('%020000d',1),0,0,300,300,0,0,300)" }) {
			const auto scratch = scratch_directory{ };
			const auto path    = scratch.file("hostile.sqlite3");
			{
				const auto database = workbench_settings_database::open(path, { });
				expect(database.has_value(), "hostile fixture begins canonical");
			}
			sqlite3* raw = nullptr;
			expect(sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "hostile fixture opens");
			expect(sqlite3_exec(raw, mutation, nullptr, nullptr, nullptr) == SQLITE_OK, "hostile window fixture seeds");
			sqlite3_close(raw);
			const auto before = logical_snapshot(path, false);
			expect(!workbench_settings_database::open(path, { }), "over-capacity or oversized window data refuses admission");
			expect(logical_snapshot(path, false) == before, "hostile row refusal preserves schema and every original value");
		}
	}

	void test_legacy_metadata_and_shape_refuse_before_migration()
	{
		for (const auto mutation : { "UPDATE workbench_meta SET schema_format=99", "DELETE FROM workbench_meta", "PRAGMA application_id=42", "PRAGMA user_version=99", "ALTER TABLE workbench_layout ADD COLUMN unexpected INTEGER", "PRAGMA ignore_check_constraints=ON; UPDATE workbench_layout SET files_width=2" }) {
			const auto scratch = scratch_directory{ };
			const auto path    = scratch.file("legacy.sqlite3");
			create_version_four(path);
			sqlite3* raw = nullptr;
			expect(sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "tamper fixture opens");
			expect(sqlite3_exec(raw, mutation, nullptr, nullptr, nullptr) == SQLITE_OK, "legacy malformed fixture seeded");
			sqlite3_close(raw);
			const auto before = logical_snapshot(path, true);
			expect(!workbench_settings_database::open(path, { }), "future or malformed legacy identity/shape/rows refuse");
			expect(logical_snapshot(path, true) == before, "refusal preserves exact original tables, columns, versions and values");
		}
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("canonical.sqlite3");
		create_version_four(path);
		auto migrated = workbench_settings_database::open(path, { });
		expect(migrated && migrated->load({ })->Behaviour.CommentsFilter == application::review_comments_filter::unresolved, "v4 behaviour migrates into profile preferences");
		expect(*scalar_integer(migrated->handle(), "SELECT count(*) FROM window_layout") == 0, "migration creates no invented legacy window ID");
		expect(migrated->load_window(core::bpr_window_id{ 9 }, { }) == migrated->load({ }), "future window uses migrated default layout");
	}

	struct interrupted_migration
	{
		static inline std::stop_source* Stop = nullptr;
		static inline std::string_view  Prefix;
		static auto                     trace(unsigned int, void*, void* aStatement, void*) -> int
		{
			const auto* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement));
			if (Stop && sql && std::string_view{ sql }.starts_with(Prefix)) {
				Stop->request_stop();
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT, trace, nullptr);
		}
	};
	void test_authoritative_window_retention()
	{
		const auto scratch  = scratch_directory{ };
		auto       database = workbench_settings_database::open(scratch.file("retention.sqlite3"), { });
		expect(database.has_value(), "retention database opens");
		auto defaults       = application::workbench_layout_settings{ };
		defaults.FilesWidth = 432;
		expect(database->store(defaults, { }).has_value(), "profile defaults persist");
		const auto                       layout = application::window_layout(defaults);
		std::vector<core::bpr_window_id> keep;
		for (std::uint64_t id = 1; id <= core::maximum_bpr_windows_per_profile; ++id) {
			keep.emplace_back(id);
			expect(database->store_window(keep.back(), layout, { }).has_value(), "retention fixture fills32 rows");
		}
		expect(database->retain_windows(keep, { }).has_value() && *scalar_integer(database->handle(), "SELECT count(*) FROM window_layout") == 32, "full authoritative set preserves every row");
		keep.emplace_back(33);
		expect(!database->retain_windows(keep, { }), "overbound keep refuses");
		keep = { core::bpr_window_id{ 1 }, core::bpr_window_id{ 1 } };
		expect(!database->retain_windows(keep, { }), "duplicate keep refuses");
		keep = { core::bpr_window_id{ 0 } };
		expect(!database->retain_windows(keep, { }) && *scalar_integer(database->handle(), "SELECT count(*) FROM window_layout") == 32, "invalid keep cannot delete rows");
		keep      = { core::bpr_window_id{ 1 }, core::bpr_window_id{ 32 } };
		auto stop = std::stop_source{ };
		stop.request_stop();
		expect(!database->retain_windows(keep, stop.get_token()), "pre-cancelled retention refuses");
		stop                          = std::stop_source{ };
		interrupted_migration::Stop   = &stop;
		interrupted_migration::Prefix = "DELETE FROM window_layout";
		expect(sqlite3_trace_v2(database->handle(), SQLITE_TRACE_STMT, interrupted_migration::trace, nullptr) == SQLITE_OK, "retention interruption installs");
		const auto refused          = database->retain_windows(keep, stop.get_token());
		interrupted_migration::Stop = nullptr;
		static_cast<void>(sqlite3_trace_v2(database->handle(), 0, nullptr, nullptr));
		expect(stop.stop_requested() && !refused && refused.error() == failure::cancelled
				   && *scalar_integer(database->handle(), "SELECT count(*) FROM window_layout") == 32,
			   "post-delete cancellation rolls back all orphan deletions");
		expect(database->retain_windows(keep, { }).has_value() && *scalar_integer(database->handle(), "SELECT count(*) FROM window_layout") == 2, "authoritative set deletes only orphan rows");
		expect(application::window_layout(*database->load_window(keep[0], { })) == layout
				   && application::window_layout(*database->load_window(keep[1], { })) == layout,
			   "retained layouts remain exact");
		expect(database->store_window(core::bpr_window_id{ 33 }, layout, { }).has_value(), "orphan cleanup releases capacity");
		expect(database->retain_windows({ }, { }).has_value() && *scalar_integer(database->handle(), "SELECT count(*) FROM window_layout") == 0, "empty authority removes all layouts");
		expect(database->load({ }) == std::expected<application::workbench_layout_settings, failure>{ defaults }, "retention preserves profile defaults and behaviour");
	}

	void test_interrupted_migration_preserves_legacy_and_reopens()
	{
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(interrupted_migration::install)) == SQLITE_OK, "migration interruption fixture registers");
		// Derived from the constant, so the stamp stage is still interrupted
		// after the next version bump rather than silently never matched.
		const auto                             stamp = "PRAGMA user_version = " + std::to_string(workbench_settings_schema_version);
		const std::array<std::string_view, 11> prefixes{ "SELECT name,sql FROM sqlite_schema", "ALTER TABLE workbench_preferences RENAME", "CREATE TABLE workbench_preferences", "INSERT INTO workbench_preferences", "DROP TABLE workbench_layout", "ALTER TABLE window_layout", "INSERT INTO window_layout", "DROP TABLE old_window_layout", "UPDATE workbench_meta", stamp, "COMMIT" };
		// [[BPR-0721]]. Version five renames its profile table first; that
		// rename is only in its migration, and it must roll back with the rest.
		for (const auto version : { 4, 5, 6, 7 }) {
			const auto legacy = version == 4;
			for (const auto prefix : prefixes) {
				if (legacy && (prefix.starts_with("ALTER TABLE") || prefix == "INSERT INTO window_layout" || prefix == "DROP TABLE old_window_layout")) {
					continue;
				}
				const auto scratch = scratch_directory{ };
				const auto path    = scratch.file("interrupted.sqlite3");
				if (legacy) {
					create_version_four(path);
				} else if (version == 5) {
					establish_version_five(path);
				} else if (version == 6) {
					establish_version_six(path);
				} else {
					establish_version_seven(path);
				}
				const auto before             = logical_snapshot(path, legacy);
				auto       stop               = std::stop_source{ };
				interrupted_migration::Stop   = &stop;
				interrupted_migration::Prefix = prefix;
				const auto refused            = workbench_settings_database::open(path, stop.get_token());
				interrupted_migration::Stop   = nullptr;
				expect(stop.stop_requested() && !refused && refused.error() == failure::cancelled, "injected preflight or migration stage reports typed cancellation");
				expect(logical_snapshot(path, legacy) == before, "interrupted migration restores original schema and all legacy values");
				const auto reopened = workbench_settings_database::open(path, { });
				expect(reopened.has_value(), "interrupted legacy file migrates on the next uncancelled open");
			}
		}
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(interrupted_migration::install)) == 1, "migration interruption fixture unregisters");
	}

	void test_explicit_defaults_and_bound_factory_views()
	{
		const auto scratch = scratch_directory{ };
		const auto root    = scratch.file("private-root");
		std::filesystem::create_directory(root);
		std::filesystem::permissions(root, std::filesystem::perms::owner_all);
#if defined(_WIN32)
		// [[BPR-0845]]. Mode bits do not make a Windows directory owner-private;
		// the stores' own rule does.
		expect(adapters::sqlite::make_storage_root_owner_private(root), "the explicit defaults root is owner-private");
#endif
		auto defaults = adapters::sqlite::make_workbench_defaults_store({ root, { } });
		expect(defaults.has_value(), "explicit defaults factory creates private storage");
		expect(!adapters::sqlite::make_workbench_settings_store({ root, { } }), "bound factory refuses absent window identity");
		expect(!adapters::sqlite::make_workbench_defaults_store({ root, core::bpr_window_id{ 4 } }), "defaults factory refuses a window identity");
		auto preferences = (*defaults)->load_preferences({ });
		expect(preferences.has_value(), "default preferences load");
		preferences->NewWindowDefaults.FilesWidth = 432;
		expect((*defaults)->store_preferences(*preferences, { }).has_value(), "defaults persist through preference seam");
		auto window = adapters::sqlite::make_workbench_settings_store({ root, core::bpr_window_id{ 4 } });
		expect(window.has_value(), "exact bound window factory opens");
		auto layout = (*window)->load({ });
		expect(layout && layout->FilesWidth == 432, "missing bound window receives profile defaults");
		layout->FilesWidth             = 654;
		layout->Behaviour.OpenModality = application::review_open_modality::review;
		expect((*window)->store(*layout, { }).has_value(), "bound geometry stores");
		const auto retained = (*defaults)->load_preferences({ });
		expect(retained && *retained == *preferences, "bound write cannot rewrite defaults or behaviour");
		expect((*window)->erase_window(core::bpr_window_id{ 4 }, { }).has_value(), "exact bound row erases");
		const auto erased = (*window)->load({ });
		expect(erased && erased->FilesWidth == 432, "erased row returns current defaults");
	}

	void test_native_geometry_is_window_local_and_migrates_from_eight()
	{
		const auto scratch   = scratch_directory{ };
		const auto path      = scratch.file("native-geometry.sqlite3");
		auto       first     = application::window_layout(distinct_layout());
		first.Home.Grouping  = application::home_grouping::organization;
		first.Home.Collapsed = application::home_collapsed_panel::recently_viewed;
		first.Geometry       = { -1200, 40, 1100, 760, true };
		auto second          = first;
		second.Geometry      = { 180, 120, 950, 600, false };
		{
			auto database = workbench_settings_database::open(path, { });
			expect(database && database->store_window(core::bpr_window_id{ 1 }, first, { })
					   && database->store_window(core::bpr_window_id{ 2 }, second, { }),
				   "two independent native placements persist");
			expect(database->load_window(core::bpr_window_id{ 1 }, { })->Geometry == first.Geometry
					   && database->load_window(core::bpr_window_id{ 2 }, { })->Geometry == second.Geometry
					   && database->load_window(core::bpr_window_id{ 3 }, { })->Geometry == application::workbench_window_geometry{ },
				   "each placement belongs only to its stable window, never defaults");
			auto invalid           = first;
			invalid.Geometry.Width = 1;
			expect(!database->store_window(core::bpr_window_id{ 1 }, invalid, { }), "invalid native geometry refuses before write");
			invalid.Geometry = { 1, 0, 0, 0, false };
			expect(!database->store_window(core::bpr_window_id{ 1 }, invalid, { }), "partial absent geometry is not a valid record");
			// Produce the exact previous schema; SQLite rewrites each dropped
			// suffix back to the version-eight canonical definition.
			expect(sqlite3_exec(legacy_home_bound(database->handle()),
								"ALTER TABLE window_layout DROP COLUMN updates_line;"
								"ALTER TABLE window_layout DROP COLUMN geometry_maximized;"
								"ALTER TABLE window_layout DROP COLUMN geometry_height;"
								"ALTER TABLE window_layout DROP COLUMN geometry_width;"
								"ALTER TABLE window_layout DROP COLUMN geometry_y;"
								"ALTER TABLE window_layout DROP COLUMN geometry_x;"
								"ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; ALTER TABLE workbench_preferences DROP COLUMN label_columns; ALTER TABLE workbench_preferences DROP COLUMN overview_counts; ALTER TABLE workbench_preferences DROP COLUMN label_helper; ALTER TABLE workbench_preferences DROP COLUMN feedback_marks; ALTER TABLE workbench_preferences DROP COLUMN writing_stop;ALTER TABLE workbench_preferences DROP COLUMN writing_outline;ALTER TABLE workbench_preferences DROP COLUMN writing_solution;ALTER TABLE workbench_preferences DROP COLUMN writing_problem;ALTER TABLE workbench_preferences DROP COLUMN writing_revision;UPDATE workbench_meta SET schema_format=8; PRAGMA user_version=8",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "canonical version-eight window fixture");
		}
		auto migrated   = workbench_settings_database::open(path, { });
		first.Geometry  = { };
		second.Geometry = { };
		expect(migrated.has_value(), "version-eight geometry migration opens");
		const auto firstLoaded = migrated->load_window(core::bpr_window_id{ 1 }, { });
		expect(firstLoaded.has_value(), "version-eight migrated first window reads");
		expect(migrated && application::window_layout(*migrated->load_window(core::bpr_window_id{ 1 }, { })) == first
				   && application::window_layout(*migrated->load_window(core::bpr_window_id{ 2 }, { })) == second,
			   "version-eight migration preserves each Home and drawer record with no invented native placement");
		expect(migrated->erase_window(core::bpr_window_id{ 1 }, { })
				   && application::window_layout(*migrated->load_window(core::bpr_window_id{ 2 }, { })) == second,
			   "explicit close erases only its own geometry row");
		expect(sqlite3_exec(migrated->handle(), "PRAGMA ignore_check_constraints=ON; UPDATE window_layout SET geometry_width=2", nullptr, nullptr, nullptr) == SQLITE_OK,
			   "hostile native placement fixture bypasses write constraints");
		const auto before = logical_snapshot(path, false);
		expect(!workbench_settings_database::open(path, { }) && logical_snapshot(path, false) == before,
			   "corrupt native placement is refused without modifying the durable file");
	}

	/// \brief [[BPR-1130]]. Each window keeps its own Updates line; the
	/// profile's defaults answer none, and a version-nine row reads folded
	/// with its native placement kept.
	void test_the_updates_line_is_window_local_and_migrates_from_nine()
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("updates-line.sqlite3");
		auto       open    = application::window_layout(distinct_layout());
		open.Geometry      = { 40, 60, 1280, 852, false };
		open.UpdatesLine   = core::workbench_updates_line::open;
		auto folded        = open;
		folded.Geometry    = { 200, 90, 1000, 700, true };
		folded.UpdatesLine = core::workbench_updates_line::folded;
		{
			auto database = workbench_settings_database::open(path, { });
			expect(database && database->store_window(core::bpr_window_id{ 1 }, open, { })
					   && database->store_window(core::bpr_window_id{ 2 }, folded, { }),
				   "two windows keep opposite folds");
			expect(database->load_window(core::bpr_window_id{ 1 }, { })->UpdatesLine == core::workbench_updates_line::open
					   && database->load_window(core::bpr_window_id{ 2 }, { })->UpdatesLine == core::workbench_updates_line::folded,
				   "each window reads back its own fold");
			expect(database->load_window(core::bpr_window_id{ 3 }, { })->UpdatesLine == core::workbench_updates_line::folded,
				   "a window with no row of its own starts folded");
			const auto defaults = database->load({ });
			expect(defaults && defaults->UpdatesLine == core::workbench_updates_line::folded,
				   "the profile's defaults answer folded whatever the windows chose");
			auto invalid        = open;
			invalid.UpdatesLine = static_cast<core::workbench_updates_line>(2);
			expect(!database->store_window(core::bpr_window_id{ 1 }, invalid, { }),
				   "a fold outside the vocabulary is refused before the write");
			expect(database->load_window(core::bpr_window_id{ 1 }, { })->UpdatesLine == core::workbench_updates_line::open,
				   "and the stored fold is unchanged");
			// The exact version-nine definition, as a version-nine build wrote it.
			expect(sqlite3_exec(legacy_home_bound(database->handle()),
								"ALTER TABLE window_layout DROP COLUMN updates_line;"
								"ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; ALTER TABLE workbench_preferences DROP COLUMN label_columns; ALTER TABLE workbench_preferences DROP COLUMN overview_counts; ALTER TABLE workbench_preferences DROP COLUMN label_helper; ALTER TABLE workbench_preferences DROP COLUMN feedback_marks; ALTER TABLE workbench_preferences DROP COLUMN writing_stop;ALTER TABLE workbench_preferences DROP COLUMN writing_outline;ALTER TABLE workbench_preferences DROP COLUMN writing_solution;ALTER TABLE workbench_preferences DROP COLUMN writing_problem;ALTER TABLE workbench_preferences DROP COLUMN writing_revision;UPDATE workbench_meta SET schema_format=9; PRAGMA user_version=9",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "canonical version-nine window fixture");
		}
		auto migrated = workbench_settings_database::open(path, { });
		expect(migrated.has_value(), "a version-nine store opens and migrates");
		auto expectedOpen        = open;
		expectedOpen.UpdatesLine = core::workbench_updates_line::folded;
		expect(application::window_layout(*migrated->load_window(core::bpr_window_id{ 1 }, { })) == expectedOpen
				   && application::window_layout(*migrated->load_window(core::bpr_window_id{ 2 }, { })) == folded,
			   "a version-nine row keeps its drawers, Home and native placement and reads folded");
		expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version,
			   "the migrated store is stamped with the current version");
		expect(migrated->store_window(core::bpr_window_id{ 1 }, open, { }).has_value()
				   && migrated->load_window(core::bpr_window_id{ 1 }, { })->UpdatesLine == core::workbench_updates_line::open,
			   "the migrated store keeps a fold opened after the migration");
		expect(sqlite3_exec(migrated->handle(), "PRAGMA ignore_check_constraints=ON; UPDATE window_layout SET updates_line=2", nullptr, nullptr, nullptr) == SQLITE_OK,
			   "hostile fold fixture bypasses the write constraint");
		const auto before = logical_snapshot(path, false);
		expect(!workbench_settings_database::open(path, { }) && logical_snapshot(path, false) == before,
			   "a fold outside the vocabulary is refused without modifying the durable file");
	}

	/// \brief The stored text of the profile row's label columns, as bytes.
	[[nodiscard]] auto stored_label_columns(::sqlite3* aDatabase) -> std::string
	{
		::sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(aDatabase, "SELECT label_columns FROM workbench_preferences WHERE singleton = 1", -1, &statement, nullptr) == SQLITE_OK, "the label columns can be read raw");
		auto text = std::string{ "<no row>" };
		if (sqlite3_step(statement) == SQLITE_ROW) {
			const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
			text.assign(bytes != nullptr ? bytes : "", static_cast<std::size_t>(sqlite3_column_bytes(statement, 0)));
		}
		sqlite3_finalize(statement);
		return text;
	}

	/// \brief A profile row away from its defaults in every field this card
	/// does not own, and a window row beside it, so a migration that loses or
	/// resets anything else is seen.
	[[nodiscard]] auto settled_profile() -> application::workbench_layout_settings
	{
		auto chosen                       = distinct_layout();
		chosen.Repositories               = core::workbench_drawer_state::collapsed;
		chosen.RepositoriesWidth          = 360U;
		chosen.OnePane                    = core::workbench_one_pane::enabled;
		chosen.Behaviour.OpenModality     = application::review_open_modality::last_used;
		chosen.Behaviour.MarkRead         = application::review_mark_read_behaviour::when_fully_seen;
		chosen.Behaviour.ReturnComparison = application::review_return_comparison::since_last_upload;
		chosen.Behaviour.CommentsFilter   = application::review_comments_filter::unresolved;
		chosen.Behaviour.DrawerControls   = application::review_drawer_controls::shown;
		chosen.Behaviour.FeedbackMarks    = application::review_feedback_marks::letters;
		chosen.WritingHooks.Revision      = 5U;
		chosen.WritingHooks.Overrides[0]  = "Name the problem first.";
		chosen.WritingHooks.Overrides[3]  = "One stop, one point.";
		return chosen;
	}

	[[nodiscard]] auto label_columns_of(std::initializer_list<core::feedback_label> aLabels) -> core::feedback_label_columns
	{
		const auto labels = std::vector<core::feedback_label>(aLabels);
		const auto made   = core::make_feedback_label_columns(labels);
		expect(made.has_value(), "the test's own label columns are a selection");
		return *made;
	}

	/// \brief [[BPR-1088]]. A version-twelve profile gains the label helper
	/// (off), the Overview's starting count (Open) and no label columns, and
	/// keeps every other field, its writing hooks and its window rows.
	/// \brief [[BPR-1295]]. Read merged into Review. A stored open_modality 1
	/// opens in Review under its new name; continue_modality of either value
	/// is accepted and ignored; and every write keeps that retired column
	/// valid with 1, Review. No schema change: the CHECKs are as they were.
	void test_read_settings_open_in_review_and_continue_modality_is_ignored()
	{
		const auto scratch            = scratch_directory{ };
		const auto path               = scratch.file("read-merged.sqlite3");
		auto       chosen             = settled_profile();
		chosen.Behaviour.OpenModality = application::review_open_modality::overview;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && db->store(chosen, { }).has_value(), "a full profile row stores");
			expect(integer_pragma(path, "SELECT continue_modality FROM workbench_preferences") == 1,
				   "a write keeps the retired continue_modality valid as 1, Review");
		}
		// The rows an older build wrote: opens in Read (1), Continue review
		// enters Read (0) and, in the second pass, Review (1).
		for (const auto continued : { 0, 1 }) {
			{
				::sqlite3* raw = nullptr;
				expect(::sqlite3_open_v2(path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
					   "the store reopens raw");
				const auto sql = "UPDATE workbench_preferences SET open_modality = 1, continue_modality = "
					+ std::to_string(continued) + ";";
				expect(::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
					   "an older build's Read row is written");
				(void)::sqlite3_close_v2(raw);
			}
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "the store with a Read row opens");
			const auto loaded = db->load({ });
			expect(loaded.has_value(), "a stored Read and either Continue value load");
			expect(loaded->Behaviour.OpenModality == application::review_open_modality::review,
				   "a pull request that opened in Read opens in Review");
			auto expected                   = chosen;
			expected.Behaviour.OpenModality = application::review_open_modality::review;
			expect(*loaded == expected, "nothing else in the row changes, and Continue's value is not read");
			expect(db->store(*loaded, { }).has_value(), "the loaded row stores again");
			expect(integer_pragma(path, "SELECT open_modality FROM workbench_preferences") == 1
					   && integer_pragma(path, "SELECT continue_modality FROM workbench_preferences") == 1,
				   "and is written back as Review, with continue_modality 1");
		}
	}

	void test_label_preferences_migrate_from_twelve_without_resetting_anything_else()
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("labels-v12.sqlite3");
		const auto chosen  = settled_profile();
		auto       window  = application::window_layout(chosen);
		window.FilesWidth  = 444U;
		window.UpdatesLine = core::workbench_updates_line::open;
		window.Geometry    = { .X = 40, .Y = 60, .Width = 1200, .Height = 800, .Maximized = false };
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && db->store(chosen, { }).has_value(), "a full profile row stores");
			expect(db->store_window(core::bpr_window_id{ 4 }, window, { }).has_value(), "and a window row beside it");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; ALTER TABLE workbench_preferences DROP COLUMN label_columns; ALTER TABLE workbench_preferences DROP COLUMN overview_counts; ALTER TABLE workbench_preferences DROP COLUMN label_helper; UPDATE workbench_meta SET schema_format=12; PRAGMA user_version=12", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "canonical version12 source");
		}
		auto db = workbench_settings_database::open(path, { });
		expect(db.has_value(), "version12 migration opens");
		expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version, "and stamps the current version");
		const auto loaded = db->load({ });
		expect(loaded.has_value(), "the migrated row loads");
		expect(loaded->Behaviour.LabelHelper == application::review_label_helper::off && loaded->Behaviour.OverviewCounts == application::review_overview_counts::open
				   && loaded->Behaviour.LabelColumns == core::feedback_label_columns{ } && !loaded->Behaviour.LabelColumnsRecovery.has_value(),
			   "absent fields take the safe defaults: helper off, Open, no columns, nothing to recover");
		expect(*loaded == chosen, "every other field, the mark style and the writing hooks included, is kept exactly");
		const auto windowLoaded = db->load_window(core::bpr_window_id{ 4 }, { });
		expect(windowLoaded.has_value() && application::window_layout(*windowLoaded) == window && windowLoaded->Behaviour == chosen.Behaviour, "the window row is kept and reads the migrated behaviour");
		expect(stored_label_columns(db->handle()).empty(), "the migrated row stores no columns");
	}

	/// \brief [[BPR-1093]]. The label settings migration (version 12 to the
	/// current schema, RFC-BPR-0049 R28, R41) meets another writer and an
	/// interruption without losing anything: while another connection holds
	/// the version-12 file, opening it migrates nothing and leaves every value
	/// as it was; a migration stopped at any of its stages rolls back to the
	/// exact version-12 file; and the migration that finally runs keeps every
	/// other field and window row, takes the label defaults, and runs once.
	void test_label_migration_from_twelve_meets_a_second_writer_and_an_interruption()
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("labels-v12-conflict.sqlite3");
		const auto chosen  = settled_profile();
		auto       window  = application::window_layout(chosen);
		window.FilesWidth  = 444U;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && db->store(chosen, { }).has_value() && db->store_window(core::bpr_window_id{ 4 }, window, { }).has_value(), "a full profile row and a window row store");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; ALTER TABLE workbench_preferences DROP COLUMN label_columns; ALTER TABLE workbench_preferences DROP COLUMN overview_counts; ALTER TABLE workbench_preferences DROP COLUMN label_helper; UPDATE workbench_meta SET schema_format=12; PRAGMA user_version=12", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "canonical version12 source");
		}
		const auto before = logical_snapshot(path, false);

		// Another process holds the file for writing while this one opens it.
		sqlite3* other = nullptr;
		expect(sqlite3_open(path.string().c_str(), &other) == SQLITE_OK && sqlite3_exec(other, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "a second writer holds the version-12 file");
		const auto busy = workbench_settings_database::open(path, { });
		expect(sqlite3_exec(other, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK, "the second writer lets go");
		sqlite3_close(other);
		expect(!busy, "a store another writer holds is not migrated underneath it");
		expect(logical_snapshot(path, false) == before, "and every version-12 table, column and value is exactly as it was");

		// Stopped at each stage of the migration, the file rolls back whole.
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(interrupted_migration::install)) == SQLITE_OK, "migration interruption fixture registers");
		const auto                             stamp = "PRAGMA user_version = " + std::to_string(workbench_settings_schema_version);
		const std::array<std::string_view, 11> prefixes{ "SELECT name,sql FROM sqlite_schema", "ALTER TABLE workbench_preferences RENAME", "CREATE TABLE workbench_preferences", "INSERT INTO workbench_preferences", "DROP TABLE workbench_layout", "ALTER TABLE window_layout", "INSERT INTO window_layout", "DROP TABLE old_window_layout", "UPDATE workbench_meta", stamp, "COMMIT" };
		for (const auto prefix : prefixes) {
			auto stop                     = std::stop_source{ };
			interrupted_migration::Stop   = &stop;
			interrupted_migration::Prefix = prefix;
			const auto refused            = workbench_settings_database::open(path, stop.get_token());
			interrupted_migration::Stop   = nullptr;
			expect(stop.stop_requested() && !refused && refused.error() == failure::cancelled, "the label migration stopped at " + std::string{ prefix } + " reports typed cancellation");
			expect(logical_snapshot(path, false) == before, "and leaves the exact version-12 file: " + std::string{ prefix });
		}
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(interrupted_migration::install)) == 1, "migration interruption fixture unregisters");

		// The migration that runs keeps everything and runs once.
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version, "the uncontested open migrates and stamps the current version");
			const auto loaded = db->load({ });
			expect(loaded.has_value() && *loaded == chosen && loaded->Behaviour.LabelColumns == core::feedback_label_columns{ } && loaded->Behaviour.LabelHelper == application::review_label_helper::off,
				   "every other field is kept and the label fields take their safe defaults");
			const auto windowLoaded = db->load_window(core::bpr_window_id{ 4 }, { });
			expect(windowLoaded.has_value() && application::window_layout(*windowLoaded) == window, "the window row is kept");
			auto labelled                     = chosen;
			labelled.Behaviour.LabelColumns   = label_columns_of({ core::feedback_label::question, core::feedback_label::issue });
			labelled.Behaviour.OverviewCounts = application::review_overview_counts::all;
			expect(db->store(labelled, { }).has_value(), "a label choice is saved after the migration");
		}
		const auto migrated = logical_snapshot(path, false);
		auto       again    = workbench_settings_database::open(path, { });
		expect(again.has_value() && logical_snapshot(path, false) == migrated && again->load({ })->Behaviour.LabelColumns == label_columns_of({ core::feedback_label::question, core::feedback_label::issue }),
			   "a second opener finds the current schema, migrates nothing again and reads the saved label choice");
	}

	/// \brief [[BPR-1088]]. All twelve label columns persist in the chosen
	/// order, each alone too, with the helper and the starting count.
	void test_all_twelve_label_columns_persist_in_order()
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("labels.sqlite3");
		auto       chosen  = settled_profile();
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "the store opens");
			for (std::size_t index = 0U; index < core::feedback_label_count; ++index) {
				const auto label              = static_cast<core::feedback_label>(index);
				chosen.Behaviour.LabelColumns = label_columns_of({ label });
				expect(db->store(chosen, { }).has_value(), "one column stores");
				expect(stored_label_columns(db->handle()) == core::stable_name(label), "and is kept as its stable ID");
				expect(db->load({ }) == std::expected<application::workbench_layout_settings, failure>{ chosen }, "each of the twelve labels reads back as the only column");
			}
			auto all = std::array<core::feedback_label, core::feedback_label_count>{ };
			for (std::size_t index = 0U; index < all.size(); ++index) {
				all[index] = static_cast<core::feedback_label>((index * 5U + 3U) % core::feedback_label_count);
			}
			chosen.Behaviour.LabelColumns   = *core::make_feedback_label_columns(all);
			chosen.Behaviour.LabelHelper    = application::review_label_helper::on;
			chosen.Behaviour.OverviewCounts = application::review_overview_counts::all;
			expect(db->store(chosen, { }).has_value(), "all twelve, out of catalog order, store");
			expect(stored_label_columns(db->handle()) == "issue,note,nitpick,thought,quibble,todo,typo,suggestion,chore,praise,question,polish", "in the order chosen, as stable IDs");
		}
		auto       db     = workbench_settings_database::open(path, { });
		const auto loaded = db->load({ });
		expect(loaded.has_value() && *loaded == chosen, "all twelve, the helper and All survive a reopen, compared whole");
		expect(std::ranges::equal(core::labels_of(loaded->Behaviour.LabelColumns), core::labels_of(chosen.Behaviour.LabelColumns)), "the labels come back in the same order");

		auto repeated                             = chosen;
		repeated.Behaviour.LabelColumns           = label_columns_of({ core::feedback_label::issue, core::feedback_label::note });
		repeated.Behaviour.LabelColumns.Labels[1] = core::feedback_label::issue;
		auto outside                              = chosen;
		outside.Behaviour.LabelColumns            = label_columns_of({ core::feedback_label::issue });
		outside.Behaviour.LabelColumns.Labels[0]  = static_cast<core::feedback_label>(12);
		const auto before                         = stored_label_columns(db->handle());
		const auto refusedRepeat                  = db->store(repeated, { });
		const auto refusedUnknown                 = db->store(outside, { });
		expect(!refusedRepeat && refusedRepeat.error() == catalog_database_failure::integrity_failure, "a record with a repeated column is refused, not deduplicated");
		expect(!refusedUnknown && refusedUnknown.error() == catalog_database_failure::integrity_failure, "a record with an unknown column is refused");
		expect(stored_label_columns(db->handle()) == before && *db->load({ }) == chosen, "a refused write leaves the stored row exactly as it was");
		auto stop = std::stop_source{ };
		stop.request_stop();
		auto other                   = chosen;
		other.Behaviour.LabelColumns = { };
		other.Behaviour.LabelHelper  = application::review_label_helper::off;
		const auto cancelled         = db->store(other, stop.get_token());
		expect(!cancelled && cancelled.error() == catalog_database_failure::cancelled && *db->load({ }) == chosen, "a cancelled write claims nothing and changes nothing");
	}

	/// \brief [[BPR-1088]]. Stored columns that name an unknown or repeated
	/// label load without them, report what was left out, keep every valid
	/// label in its place and every other setting, and are not rewritten
	/// until the reviewer saves.
	void test_unknown_or_repeated_stored_columns_load_safely_and_explain()
	{
		const auto scratch           = scratch_directory{ };
		const auto path              = scratch.file("labels-recovery.sqlite3");
		auto       chosen            = settled_profile();
		chosen.Behaviour.LabelHelper = application::review_label_helper::on;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && db->store(chosen, { }).has_value(), "a profile row stores");
			expect(sqlite3_exec(db->handle(), "UPDATE workbench_preferences SET label_columns='issue,blocker,question,issue,,note'", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "a hand-edited list with an unknown, a repeated and an empty entry is injected");
		}
		auto db = workbench_settings_database::open(path, { });
		expect(db.has_value(), "the store still opens: a bad list is not a broken file");
		const auto loaded = db->load({ });
		expect(loaded.has_value(), "and the row still loads");
		expect(loaded->Behaviour.LabelColumns == label_columns_of({ core::feedback_label::issue, core::feedback_label::question, core::feedback_label::note }),
			   "every valid label keeps its place, each once");
		expect(loaded->Behaviour.LabelColumnsRecovery == std::optional{ application::review_label_columns_recovery{ .Unknown = 2U, .Repeated = 1U } },
			   "what was left out is reported: two unknown entries and one repeat");
		auto expected                           = chosen;
		expected.Behaviour.LabelColumns         = loaded->Behaviour.LabelColumns;
		expected.Behaviour.LabelColumnsRecovery = loaded->Behaviour.LabelColumnsRecovery;
		expect(*loaded == expected && application::valid(loaded->Behaviour), "nothing else is reset, and the record read is valid");
		expect(stored_label_columns(db->handle()) == "issue,blocker,question,issue,,note", "reading never rewrites the stored list");
		expect(db->load({ }) == loaded, "and reads the same again");

		auto saved     = application::edited(loaded->Behaviour);
		auto next      = *loaded;
		next.Behaviour = saved;
		expect(db->store(next, { }).has_value(), "the reviewer's next save writes");
		expect(stored_label_columns(db->handle()) == "issue,question,note", "the list as shown, without what was left out");
		const auto clean = db->load({ });
		expect(clean.has_value() && !clean->Behaviour.LabelColumnsRecovery.has_value() && clean->Behaviour.LabelColumns == expected.Behaviour.LabelColumns, "and the next load has nothing to report");

		expect(sqlite3_exec(db->handle(), "UPDATE workbench_preferences SET label_columns='blocker'", nullptr, nullptr, nullptr) == SQLITE_OK, "a list with no valid label is injected");
		const auto none = db->load({ });
		expect(none.has_value() && none->Behaviour.LabelColumns == core::feedback_label_columns{ } && none->Behaviour.LabelColumnsRecovery == std::optional{ application::review_label_columns_recovery{ .Unknown = 1U, .Repeated = 0U } }
				   && none->Behaviour.LabelHelper == application::review_label_helper::on,
			   "it reads as no columns, reported, with the helper and the rest kept");
	}

	/// \brief [[BPR-1088]]. Storage the reader cannot judge fails closed like
	/// every other column: an over-long list, a list that is not text, and a
	/// helper or count outside its vocabulary.
	void test_label_preference_storage_outside_its_bounds_fails_closed()
	{
		for (const auto* const mutation : { "PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET label_columns=printf('%0300d', 1)",
											"PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET label_helper=2",
											"PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET overview_counts=2" }) {
			const auto scratch = scratch_directory{ };
			const auto path    = scratch.file("labels-bounds.sqlite3");
			{
				auto db = workbench_settings_database::open(path, { });
				expect(db.has_value() && db->store(settled_profile(), { }).has_value(), "a profile row stores");
				expect(sqlite3_exec(db->handle(), mutation, nullptr, nullptr, nullptr) == SQLITE_OK, "an out-of-bounds value is injected");
			}
			const auto before  = logical_snapshot(path, false);
			const auto refused = workbench_settings_database::open(path, { });
			expect(!refused && refused.error() == catalog_database_failure::schema_mismatch, "a value outside its bounds fails closed at admission");
			expect(logical_snapshot(path, false) == before, "and the file is left as it was");
		}
	}

	/// \brief The text one profile-row column holds, read past the store.
	[[nodiscard]] auto stored_profile_text(const std::filesystem::path& aPath, const std::string& aColumn) -> std::optional<std::string>
	{
		::sqlite3* raw = nullptr;
		if (sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
			sqlite3_close(raw);
			return std::nullopt;
		}
		::sqlite3_stmt*            statement = nullptr;
		std::optional<std::string> result;
		const auto                 sql = "SELECT " + aColumn + " FROM workbench_preferences WHERE singleton=1";
		if (sqlite3_prepare_v2(raw, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_type(statement, 0) == SQLITE_TEXT) {
			result = std::string{ reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), static_cast<std::size_t>(sqlite3_column_bytes(statement, 0)) };
		}
		sqlite3_finalize(statement);
		sqlite3_close(raw);
		return result;
	}

	/// \brief [[BPR-1090]]. The review-feedback hook's override is its own
	/// column: a version-fifteen file migrates with its four tour overrides and
	/// each window's summary fraction, and reads the feedback hook empty, the
	/// shipped default. An override is stored as written and survives a
	/// reopen beside the others; reset stores it empty again, so a later
	/// default reaches only the profiles that follow it.
	void test_feedback_hook_override_migrates_from_fifteen_and_survives()
	{
		const auto scratch               = scratch_directory{ };
		const auto path                  = scratch.file("feedback-v15.sqlite3");
		auto       chosen                = settled_profile();
		chosen.WritingHooks.Overrides[1] = "Say what changed.";
		chosen.WritingHooks.Overrides[2] = "Three stops at most.";
		auto window                      = application::workbench_window_layout{ };
		window.SummaryFraction           = 4100U;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(chosen, { }) && db->store_window(core::bpr_window_id{ 1 }, window, { }), "a full profile row and a window store");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; UPDATE workbench_meta SET schema_format=15; PRAGMA user_version=15", nullptr, nullptr, nullptr) == SQLITE_OK, "canonical version15 source");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "version15 migration opens");
			expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version, "and stamps the current version");
			const auto loaded = db->load({ });
			expect(loaded && *loaded == chosen, "the four tour overrides, their revision and every other field are kept exactly");
			expect(loaded && loaded->WritingHooks.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)].empty(), "the migrated feedback hook follows the shipped default");
			expect(db->load_window(core::bpr_window_id{ 1 }, { })->SummaryFraction == window.SummaryFraction, "each window keeps its summary fraction");
		}
		expect(stored_profile_text(path, "writing_feedback") == std::string{ }, "a profile following the default stores nothing for it");
		const std::string override                                                            = "Label every comment. Keep questions as questions — no inferred blocking.";
		chosen.WritingHooks.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)] = override;
		++chosen.WritingHooks.Revision;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(chosen, { }), "a feedback override stores");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "the store reopens");
			const auto loaded = db->load({ });
			expect(loaded && *loaded == chosen, "the override survives a reopen beside the tour overrides");
		}
		expect(stored_profile_text(path, "writing_feedback") == override && stored_profile_text(path, "writing_problem") == chosen.WritingHooks.Overrides[0], "the override is stored as written in its own column");
		chosen.WritingHooks.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)].clear();
		++chosen.WritingHooks.Revision;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(chosen, { }) && *db->load({ }) == chosen, "reset stores the hook empty and keeps the others");
		}
		expect(stored_profile_text(path, "writing_feedback") == std::string{ }, "after reset the profile follows the shipped default again");
		chosen.WritingHooks.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)] = std::string(core::maximum_writing_instruction_bytes + 1U, 'x');
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && !db->store(chosen, { }), "an oversize feedback override is refused before it is written");
		}
	}

	void test_summary_fraction_window_reopen_and_v14_migration()
	{
		const auto scratch     = scratch_directory{ };
		const auto path        = scratch.file("summary-v14.sqlite3");
		auto       first       = application::workbench_window_layout{ };
		first.SummaryFraction  = 6200U;
		auto second            = first;
		second.SummaryFraction = 3800U;
		auto profile           = settled_profile();
		profile.ClaudeWrapper  = "/Users/me/bin/claude-wrapper";
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(profile, { }) && db->store_window(core::bpr_window_id{ 1 }, first, { }) && db->store_window(core::bpr_window_id{ 2 }, second, { }), "distinct summary preferences store per window");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->load_window(core::bpr_window_id{ 1 }, { })->SummaryFraction == first.SummaryFraction && db->load_window(core::bpr_window_id{ 2 }, { })->SummaryFraction == second.SummaryFraction, "reopen preserves each fraction");
			expect(!db->load({ })->SummaryFraction && !db->load_window(core::bpr_window_id{ 3 }, { })->SummaryFraction, "profile and new window have no summary preference");
			first.SummaryFraction.reset();
			expect(db->store_window(core::bpr_window_id{ 1 }, first, { }) && !db->load_window(core::bpr_window_id{ 1 }, { })->SummaryFraction, "Reset clears only its window preference");
			first.SummaryFraction = 10000U;
			expect(!db->store_window(core::bpr_window_id{ 1 }, first, { }), "invalid share is refused without writing");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; UPDATE workbench_meta SET schema_format=14; PRAGMA user_version=14", nullptr, nullptr, nullptr) == SQLITE_OK, "canonical version14 source created");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && *db->load({ }) == profile && !db->load_window(core::bpr_window_id{ 1 }, { })->SummaryFraction && !db->load_window(core::bpr_window_id{ 2 }, { })->SummaryFraction, "v14 migration keeps profile wrapper and window fields with content-aware default");
			expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version, "summary migration stamps current schema");
		}
	}

	/// \brief [[BPR-1334]]. Line details: shown or hidden in the profile row,
	/// a height per window; a version-sixteen file shows it at the default.
	void test_line_details_persists_and_migrates_from_sixteen()
	{
		const auto scratch            = scratch_directory{ };
		const auto path               = scratch.file("line-details.sqlite3");
		auto       profile            = settled_profile();
		profile.Behaviour.LineDetails = application::review_line_details::hidden;
		auto first                    = application::workbench_window_layout{ };
		first.LineDetailsHeight       = 212U;
		first.SummaryFraction         = 6200U;
		auto second                   = first;
		second.LineDetailsHeight      = 96U;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(profile, { }) && db->store_window(core::bpr_window_id{ 1 }, first, { }) && db->store_window(core::bpr_window_id{ 2 }, second, { }),
				   "the Line details choice and each window's height store");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->load({ })->Behaviour.LineDetails == application::review_line_details::hidden, "the hidden choice survives a reopen");
			expect(db->load_window(core::bpr_window_id{ 1 }, { })->LineDetailsHeight == first.LineDetailsHeight
					   && db->load_window(core::bpr_window_id{ 2 }, { })->LineDetailsHeight == second.LineDetailsHeight,
				   "each window keeps its own height");
			expect(!db->load_window(core::bpr_window_id{ 3 }, { })->LineDetailsHeight && !db->load({ })->LineDetailsHeight,
				   "a new window and the profile defaults have no height");
			auto tall              = first;
			tall.LineDetailsHeight = application::maximum_line_details_height + 1U;
			expect(!db->store_window(core::bpr_window_id{ 1 }, tall, { }), "a height past the bound is refused");
			first.LineDetailsHeight.reset();
			expect(db->store_window(core::bpr_window_id{ 1 }, first, { }) && !db->load_window(core::bpr_window_id{ 1 }, { })->LineDetailsHeight,
				   "Reset clears only its window's height");
			expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; UPDATE workbench_meta SET schema_format=16; PRAGMA user_version=16", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "canonical version16 source");
		}
		auto db = workbench_settings_database::open(path, { });
		expect(db.has_value() && integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version, "version16 migrates and stamps the current version");
		auto expected                  = profile;
		expected.Behaviour.LineDetails = application::review_line_details::shown;
		expect(*db->load({ }) == expected, "a migrated profile shows Line details and keeps every other field");
		const auto migrated = db->load_window(core::bpr_window_id{ 2 }, { });
		expect(migrated && !migrated->LineDetailsHeight && migrated->SummaryFraction == second.SummaryFraction,
			   "a migrated window has the default height and keeps its summary fraction");
	}

	/// \brief [[BPR-1375]]. The profile's Claude model is stored as typed,
	/// survives a reopen, refuses what `--model` could not take as one
	/// argument, and a version-seventeen profile gains none without losing
	/// anything else.
	void test_claude_model_persists_and_migrates_from_seventeen()
	{
		{
			const auto scratch = scratch_directory{ };
			const auto path    = scratch.file("model.sqlite3");
			auto       chosen  = settled_profile();
			chosen.ClaudeModel = "claude-sonnet-5-5";
			{
				auto db = workbench_settings_database::open(path, { });
				expect(db.has_value() && db->store(chosen, { }).has_value(), "a profile row with a model stores");
			}
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && db->load({ }).has_value() && *db->load({ }) == chosen, "the model survives a restart exactly as typed");
			for (const auto* refused : { "--dangerously-skip-permissions", "claude sonnet", "model\n" }) {
				chosen.ClaudeModel = refused;
				expect(!db->store(chosen, { }).has_value(), "a model that is not one plain argument is refused before it is written");
			}
			chosen.ClaudeModel = std::string(core::maximum_claude_model_bytes, 'a');
			expect(db->store(chosen, { }).has_value() && db->load({ })->ClaudeModel == chosen.ClaudeModel, "the longest admitted model stores");
			expect(sqlite3_exec(db->handle(), "UPDATE workbench_preferences SET claude_model='-x'", nullptr, nullptr, nullptr) == SQLITE_OK && !db->load({ }).has_value(),
				   "a stored model the rule refuses is not read back");
		}
		{
			const auto scratch = scratch_directory{ };
			const auto path    = scratch.file("model-v17.sqlite3");
			const auto chosen  = settled_profile();
			{
				auto db = workbench_settings_database::open(path, { });
				expect(db.has_value() && db->store(chosen, { }).has_value(), "a full profile row stores");
				expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; UPDATE workbench_meta SET schema_format=17; PRAGMA user_version=17", nullptr, nullptr, nullptr) == SQLITE_OK,
					   "canonical version17 source");
			}
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value() && integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version, "version17 migrates and stamps the current version");
			const auto loaded = db->load({ });
			expect(loaded.has_value() && loaded->ClaudeModel.empty() && *loaded == chosen, "a migrated profile names no model and keeps every other field");
		}
	}

	/// \brief [[BPR-1301]]. The profile's Claude process wrapper is stored
	/// as typed, survives a reopen, is carried through every other write, and
	/// a version-thirteen profile gains none without losing anything else.
	void test_claude_wrapper_persists_and_migrates_from_thirteen()
	{
		{
			const auto scratch   = scratch_directory{ };
			const auto path      = scratch.file("wrapper.sqlite3");
			auto       chosen    = settled_profile();
			chosen.ClaudeWrapper = "/Users/me/bin/claude gateway (work)";
			{
				auto db = workbench_settings_database::open(path, { });
				expect(db.has_value() && db->store(chosen, { }).has_value(), "a profile row with a wrapper stores");
			}
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "the store reopens");
			const auto loaded = db->load({ });
			expect(loaded.has_value() && loaded->ClaudeWrapper == chosen.ClaudeWrapper && *loaded == chosen, "the wrapper survives a restart exactly as typed");
			chosen.ClaudeWrapper = std::string(core::maximum_agent_wrapper_bytes, '/');
			expect(db->store(chosen, { }).has_value() && db->load({ })->ClaudeWrapper == chosen.ClaudeWrapper, "the longest admitted path stores");
			chosen.ClaudeWrapper += "/";
			expect(!db->store(chosen, { }).has_value(), "a longer path is refused before it is written");
			chosen.ClaudeWrapper = "/bin/claude\n";
			expect(!db->store(chosen, { }).has_value(), "a line break is refused before it is written");
		}
		{
			const auto scratch = scratch_directory{ };
			const auto path    = scratch.file("wrapper-v13.sqlite3");
			const auto chosen  = settled_profile();
			{
				auto db = workbench_settings_database::open(path, { });
				expect(db.has_value() && db->store(chosen, { }).has_value(), "a full profile row stores");
				expect(sqlite3_exec(legacy_home_bound(db->handle()), "ALTER TABLE workbench_preferences DROP COLUMN claude_model; ALTER TABLE workbench_preferences DROP COLUMN line_details; ALTER TABLE window_layout DROP COLUMN line_details_height; ALTER TABLE workbench_preferences DROP COLUMN writing_feedback; ALTER TABLE window_layout DROP COLUMN summary_fraction; ALTER TABLE workbench_preferences DROP COLUMN claude_wrapper; UPDATE workbench_meta SET schema_format=13; PRAGMA user_version=13", nullptr, nullptr, nullptr) == SQLITE_OK,
					   "canonical version13 source");
			}
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "version13 migration opens");
			expect(integer_pragma(path, "PRAGMA user_version") == workbench_settings_schema_version, "and stamps the current version");
			const auto loaded = db->load({ });
			expect(loaded.has_value() && loaded->ClaudeWrapper.empty(), "a migrated profile has no wrapper");
			expect(*loaded == chosen, "every other field, the label preferences and writing hooks included, is kept exactly");
		}
		{
			const auto scratch = scratch_directory{ };
			const auto root    = scratch.file("wrapper-root");
			std::filesystem::create_directory(root);
			std::filesystem::permissions(root, std::filesystem::perms::owner_all);
#if defined(_WIN32)
			expect(adapters::sqlite::make_storage_root_owner_private(root), "the wrapper root is owner-private");
#endif
			{
				auto defaults = adapters::sqlite::make_workbench_defaults_store({ root, { } });
				expect(defaults.has_value(), "the profile store opens");
				auto preferences           = (*defaults)->load_preferences({ });
				preferences->ClaudeWrapper = "/opt/tools/claude";
				expect((*defaults)->store_preferences(*preferences, { }).has_value(), "the wrapper stores through the preferences seam");
				auto layout        = (*defaults)->load({ });
				layout->FilesWidth = 500U;
				layout->ClaudeWrapper.clear();
				expect((*defaults)->store(*layout, { }).has_value(), "a legacy layout write stores");
				auto window = adapters::sqlite::make_workbench_settings_store({ root, core::bpr_window_id{ 9 } });
				auto bound  = (*window)->load({ });
				expect(bound.has_value() && (*window)->store(*bound, { }).has_value(), "a window's own write stores");
			}
			auto       reopened = adapters::sqlite::make_workbench_defaults_store({ root, { } });
			const auto kept     = (*reopened)->load_preferences({ });
			expect(kept.has_value() && kept->ClaudeWrapper == "/opt/tools/claude", "layout and window writes keep the profile's wrapper across a reopen");
		}
	}

	/// \brief [[BPR-1301]]. A stored wrapper the reader cannot judge fails
	/// closed at admission like every other column: over its bound, or with
	/// a line break, and the file is left as it was.
	void test_claude_wrapper_storage_outside_its_bounds_fails_closed()
	{
		for (const auto* const mutation : { "PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET claude_wrapper=printf('/%01100d', 1)",
											"UPDATE workbench_preferences SET claude_wrapper='/bin/claude' || char(10) || '--x'",
											"UPDATE workbench_preferences SET claude_wrapper=char(0)" }) {
			const auto scratch = scratch_directory{ };
			const auto path    = scratch.file("wrapper-bounds.sqlite3");
			{
				auto db = workbench_settings_database::open(path, { });
				expect(db.has_value() && db->store(settled_profile(), { }).has_value(), "a profile row stores");
				expect(sqlite3_exec(db->handle(), mutation, nullptr, nullptr, nullptr) == SQLITE_OK, "an unreadable wrapper is injected");
			}
			const auto before  = logical_snapshot(path, false);
			const auto refused = workbench_settings_database::open(path, { });
			expect(!refused && refused.error() == catalog_database_failure::schema_mismatch, "an unreadable wrapper fails closed at admission");
			expect(logical_snapshot(path, false) == before, "and the file is left as it was");
		}
	}

	/// \brief [[BPR-1512]]. Version nineteen adopts the narrower default without losing saved choices.
	void test_center_ribbon_width_migration_and_bounds()
	{
		const auto scratch                 = scratch_directory{ };
		const auto path                    = scratch.file("ribbon-v19.sqlite3");
		auto       chosen                  = settled_profile();
		chosen.Behaviour.CenterRibbonWidth = 56U;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(chosen, { }), "an explicit 56-pixel choice stores");
			expect(sqlite3_exec(db->handle(), "ALTER TABLE workbench_preferences DROP COLUMN reading_width; ALTER TABLE workbench_preferences DROP COLUMN markdown_images; ALTER TABLE workbench_preferences DROP COLUMN center_ribbon_width; UPDATE workbench_meta SET schema_format=19; PRAGMA user_version=19", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "the actual pre-setting version-nineteen schema is seeded");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "the prior production settings schema migrates");
			chosen.Behaviour.CenterRibbonWidth = 28U;
			expect(db->load({ }) == chosen, "migration adopts exactly half the old 56-pixel width and preserves every existing preference");
			for (const auto supported : { 16U, 28U, 56U, 256U }) {
				chosen.Behaviour.CenterRibbonWidth = supported;
				expect(db->store(chosen, { }) && db->load({ }) == chosen, "each supported boundary/sample round-trips");
			}
			for (const auto unsupported : { 0U, 15U, 257U, 0xFFFFFFFFU }) {
				chosen.Behaviour.CenterRibbonWidth = unsupported;
				expect(!db->store(chosen, { }), "an out-of-bounds width cannot be written");
			}
		}
		auto reopened                      = workbench_settings_database::open(path, { });
		chosen.Behaviour.CenterRibbonWidth = 256U;
		expect(reopened && reopened->load({ }) == chosen, "the explicit choice remains intact across restart");
	}

	/// \brief RFC-BPR-0084 Q3. Version twenty-one's image preference: a
	/// version-twenty profile migrates to Load and keeps every other choice;
	/// Don't load round-trips; an unknown value is refused.
	void test_markdown_images_migration_and_round_trip()
	{
		const auto scratch                 = scratch_directory{ };
		const auto path                    = scratch.file("images-v20.sqlite3");
		auto       chosen                  = settled_profile();
		chosen.Behaviour.CenterRibbonWidth = 56U;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(chosen, { }), "a version-twenty-one profile stores");
			expect(sqlite3_exec(db->handle(), "ALTER TABLE workbench_preferences DROP COLUMN reading_width; ALTER TABLE workbench_preferences DROP COLUMN markdown_images; UPDATE workbench_meta SET schema_format=20; PRAGMA user_version=20", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "the actual version-twenty schema is seeded");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "version twenty migrates");
			expect(db->load({ }) == chosen && chosen.Behaviour.MarkdownImages == application::review_markdown_images::load, "a migrated profile loads images and keeps every other choice");
			chosen.Behaviour.MarkdownImages = application::review_markdown_images::off;
			expect(db->store(chosen, { }) && db->load({ }) == chosen, "Don't load round-trips");
			auto invalid                     = chosen;
			invalid.Behaviour.MarkdownImages = static_cast<application::review_markdown_images>(2);
			expect(!db->store(invalid, { }), "an unknown image preference cannot be written");
		}
		{
			auto reopened = workbench_settings_database::open(path, { });
			expect(reopened && reopened->load({ }) == chosen, "Don't load survives a restart");
			expect(sqlite3_exec(reopened->handle(), "PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET markdown_images=2", nullptr, nullptr, nullptr) == SQLITE_OK, "a hostile value bypasses the check for the fixture");
		}
		auto refused = workbench_settings_database::open(path, { });
		expect(!refused || !refused->load({ }), "a stored unknown image preference is refused");
	}

	/// \brief [[BPR-1584]]. Version twenty-two's reading width: a
	/// version-twenty-one profile migrates to Wide and keeps every other
	/// choice; every choice round-trips; an unknown value is refused.
	void test_reading_width_migration_and_round_trip()
	{
		const auto scratch              = scratch_directory{ };
		const auto path                 = scratch.file("reading-v21.sqlite3");
		auto       chosen               = settled_profile();
		chosen.Behaviour.MarkdownImages = application::review_markdown_images::off;
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db && db->store(chosen, { }), "a version-twenty-two profile stores");
			expect(sqlite3_exec(db->handle(), "ALTER TABLE workbench_preferences DROP COLUMN reading_width; UPDATE workbench_meta SET schema_format=21; PRAGMA user_version=21", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "the actual version-twenty-one schema is seeded");
		}
		{
			auto db = workbench_settings_database::open(path, { });
			expect(db.has_value(), "version twenty-one migrates");
			expect(db->load({ }) == chosen && chosen.Behaviour.ReadingWidth == core::reading_width::wide, "a migrated profile reads Wide and keeps every other choice");
			for (const auto width : { core::reading_width::narrow, core::reading_width::medium, core::reading_width::full, core::reading_width::wide, core::reading_width::medium }) {
				chosen.Behaviour.ReadingWidth = width;
				expect(db->store(chosen, { }) && db->load({ }) == chosen, "each reading width round-trips");
			}
			auto invalid                   = chosen;
			invalid.Behaviour.ReadingWidth = static_cast<core::reading_width>(4);
			expect(!db->store(invalid, { }), "an unknown reading width cannot be written");
		}
		{
			auto reopened = workbench_settings_database::open(path, { });
			expect(reopened && reopened->load({ }) == chosen, "Medium survives a restart");
			expect(sqlite3_exec(reopened->handle(), "PRAGMA ignore_check_constraints=ON; UPDATE workbench_preferences SET reading_width=4", nullptr, nullptr, nullptr) == SQLITE_OK, "a hostile value bypasses the check for the fixture");
		}
		auto refused = workbench_settings_database::open(path, { });
		expect(!refused || !refused->load({ }), "a stored unknown reading width is refused");
	}

	void test_invalid_one_pane_window_is_refused()
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("invalid-pane.sqlite3");
		{
			auto database = workbench_settings_database::open(path, { });
			expect(database.has_value(), "window flag fixture opens");
			auto layout    = application::workbench_window_layout{ };
			layout.OnePane = static_cast<core::workbench_one_pane>(2);
			expect(!database->store_window(core::bpr_window_id{ 1 }, layout, { }), "invalid typed one-pane value cannot be stored");
			layout.OnePane = core::workbench_one_pane::enabled;
			expect(database->store_window(core::bpr_window_id{ 1 }, layout, { }).has_value(), "valid window one-pane flag stores");
			expect(sqlite3_exec(database->handle(), "PRAGMA ignore_check_constraints=ON; UPDATE window_layout SET one_pane=2", nullptr, nullptr, nullptr) == SQLITE_OK, "hostile window flag bypasses check for fixture");
		}
		const auto refused = workbench_settings_database::open(path, { });
		expect(!refused && refused.error() == failure::schema_mismatch, "invalid window flag refuses admission");
	}

} // namespace

auto main() -> int
try {
	expect(sqlite_runtime_ready(), "SQLite runtime initializes before trace injection");
	test_authored_collapse_upgrade_and_independent_windows();
	test_center_ribbon_width_migration_and_bounds();
	test_markdown_images_migration_and_round_trip();
	test_reading_width_migration_and_round_trip();
	test_authoritative_window_retention();
	test_feedback_marks_round_trip_and_migrate_from_eleven();
	test_label_preferences_migrate_from_twelve_without_resetting_anything_else();
	test_label_migration_from_twelve_meets_a_second_writer_and_an_interruption();
	test_read_settings_open_in_review_and_continue_modality_is_ignored();
	test_all_twelve_label_columns_persist_in_order();
	test_unknown_or_repeated_stored_columns_load_safely_and_explain();
	test_label_preference_storage_outside_its_bounds_fails_closed();
	test_summary_fraction_window_reopen_and_v14_migration();
	test_line_details_persists_and_migrates_from_sixteen();
	test_claude_wrapper_persists_and_migrates_from_thirteen();
	test_claude_model_persists_and_migrates_from_seventeen();
	test_feedback_hook_override_migrates_from_fifteen_and_survives();
	test_claude_wrapper_storage_outside_its_bounds_fails_closed();
	test_native_geometry_is_window_local_and_migrates_from_eight();
	test_the_updates_line_is_window_local_and_migrates_from_nine();
	test_invalid_one_pane_window_is_refused();
	test_hostile_window_rows_refuse_unchanged();
	test_explicit_defaults_and_bound_factory_views();
	test_interrupted_migration_preserves_legacy_and_reopens();
	test_window_rows_are_independent_from_profile_defaults();
	test_legacy_metadata_and_shape_refuse_before_migration();
	test_nothing_stored_answers_the_defaults();
	test_the_layout_round_trips_whole();
	test_writing_hook_migration_and_rejection();
	test_the_layout_is_a_singleton();
	test_widths_are_bounded_by_the_published_range();
	test_a_stored_state_outside_the_vocabulary_is_refused();
	test_a_newer_schema_is_refused_for_the_right_reason();
	test_a_version_one_store_migrates_and_keeps_its_row();
	test_a_version_two_store_migrates_and_gains_the_repositories_drawer();
	test_a_version_three_store_migrates_and_gains_the_behaviour_preferences();
	test_a_version_five_store_migrates_and_gains_the_drawer_controls();
	test_a_version_six_store_migrates_and_gains_one_pane();
	test_a_version_seven_store_migrates_and_gains_the_home_view();
	test_cancellation_is_reported_distinctly();
	test_the_schema_stores_no_review_content();
	std::cout << "workbench-settings-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
