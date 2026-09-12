#include "../../src/adapters/sqlite/src/workbench_settings_database.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::sqlite::detail;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class scratch_directory final
	{
	public:
		scratch_directory()
		{
			Path = std::filesystem::temp_directory_path()
				/ ("bpr-workbench-" + std::to_string(Counter++));
			std::filesystem::remove_all(Path);
			std::filesystem::create_directories(Path);
			std::error_code ignored;
			if (auto resolved = std::filesystem::canonical(Path, ignored);
				!ignored) {
				Path = std::move(resolved);
			}
		}
		scratch_directory(const scratch_directory&)                    = delete;
		auto operator=(const scratch_directory&) -> scratch_directory& = delete;
		scratch_directory(scratch_directory&&)                         = delete;
		auto operator=(scratch_directory&&) -> scratch_directory&      = delete;
		~scratch_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}

		[[nodiscard]] auto file(const std::string_view aName) const
			-> std::filesystem::path
		{
			return Path / aName;
		}

	private:
		static inline int     Counter = 0;
		std::filesystem::path Path;
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
		chosen.Behaviour.DrawerControls = application::review_drawer_controls::shown;

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
				.ContinueModality = application::review_continue_modality::review,
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
				.ContinueModality = application::review_continue_modality::review,
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
				.ContinueModality = application::review_continue_modality::review,
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
		defaults.Behaviour.OpenModality = application::review_open_modality::read;
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
		expect(sqlite3_exec(raw, "ALTER TABLE workbench_preferences RENAME TO workbench_layout; ALTER TABLE workbench_layout DROP COLUMN drawer_controls; ALTER TABLE workbench_layout DROP COLUMN one_pane; DROP TABLE window_layout; UPDATE workbench_meta SET schema_format=4; PRAGMA user_version=4", nullptr, nullptr, nullptr) == SQLITE_OK, "canonical legacy singleton fixture is isolated");
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
		for (const auto version : { 4, 5, 6 }) {
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
				} else {
					establish_version_six(path);
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
		layout->Behaviour.OpenModality = application::review_open_modality::read;
		expect((*window)->store(*layout, { }).has_value(), "bound geometry stores");
		const auto retained = (*defaults)->load_preferences({ });
		expect(retained && *retained == *preferences, "bound write cannot rewrite defaults or behaviour");
		expect((*window)->erase_window(core::bpr_window_id{ 4 }, { }).has_value(), "exact bound row erases");
		const auto erased = (*window)->load({ });
		expect(erased && erased->FilesWidth == 432, "erased row returns current defaults");
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
{
	expect(sqlite_runtime_ready(), "SQLite runtime initializes before trace injection");
	test_authoritative_window_retention();
	test_invalid_one_pane_window_is_refused();
	test_hostile_window_rows_refuse_unchanged();
	test_explicit_defaults_and_bound_factory_views();
	test_interrupted_migration_preserves_legacy_and_reopens();
	test_window_rows_are_independent_from_profile_defaults();
	test_legacy_metadata_and_shape_refuse_before_migration();
	test_nothing_stored_answers_the_defaults();
	test_the_layout_round_trips_whole();
	test_the_layout_is_a_singleton();
	test_widths_are_bounded_by_the_published_range();
	test_a_stored_state_outside_the_vocabulary_is_refused();
	test_a_newer_schema_is_refused_for_the_right_reason();
	test_a_version_one_store_migrates_and_keeps_its_row();
	test_a_version_two_store_migrates_and_gains_the_repositories_drawer();
	test_a_version_three_store_migrates_and_gains_the_behaviour_preferences();
	test_a_version_five_store_migrates_and_gains_the_drawer_controls();
	test_a_version_six_store_migrates_and_gains_one_pane();
	test_cancellation_is_reported_distinctly();
	test_the_schema_stores_no_review_content();
	std::cout << "workbench-settings-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
