#include "../../src/adapters/sqlite/src/workbench_settings_database.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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
		const auto chosen  = distinct_layout();

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
			opened->handle(), "SELECT count(*) FROM workbench_layout");
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
					"UPDATE workbench_layout SET files_state = 7 "
					"WHERE singleton = 1",
					nullptr,
					nullptr,
					nullptr)
					== SQLITE_OK,
				"a drawer state outside the vocabulary is written past the check");
			(void)::sqlite3_close_v2(raw);
		}

		auto reopened = workbench_settings_database::open(path, { });
		expect(reopened.has_value(), "the tampered database still opens");
		const auto loaded = reopened->load({ });
		expect(
			!loaded.has_value(),
			"a drawer state outside the closed vocabulary is refused rather "
			"than cast into a state the core would be asked to reason about");
		expect(
			loaded.error() == catalog_database_failure::integrity_failure,
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
} // namespace

auto main() -> int
{
	test_nothing_stored_answers_the_defaults();
	test_the_layout_round_trips_whole();
	test_the_layout_is_a_singleton();
	test_widths_are_bounded_by_the_published_range();
	test_a_stored_state_outside_the_vocabulary_is_refused();
	test_a_newer_schema_is_refused_for_the_right_reason();
	test_a_version_one_store_migrates_and_keeps_its_row();
	test_cancellation_is_reported_distinctly();
	test_the_schema_stores_no_review_content();
	std::cout << "workbench-settings-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
