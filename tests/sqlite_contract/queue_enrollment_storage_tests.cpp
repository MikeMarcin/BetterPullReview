#include "../../src/adapters/sqlite/src/queue_enrollment_database.hpp"
#include "../../src/adapters/sqlite/src/review_ledger_database.hpp"

#include <sqlite3.h>

#if !defined(_WIN32)
	#include <sys/wait.h>
	#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
				/ ("bpr-queue-" + std::to_string(Counter++));
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

	[[nodiscard]] auto ref(const std::int64_t aNumber) -> core::pull_request_ref
	{
		return core::pull_request_ref{
			.Host       = "github.com",
			.Account    = "reviewer",
			.Repository = "octo/app",
			.Number     = aNumber,
		};
	}

	[[nodiscard]] auto watching(const std::string_view aPattern)
		-> core::queue_membership
	{
		return core::queue_membership{
			.Role        = core::github_role::none,
			.MatchedRule = true,
			.Rule        = core::observer_rule{
				.Kind      = core::observer_rule_kind::path,
				.Pattern   = std::string{ aPattern },
				.Excluding = false,
			},
		};
	}

	[[nodiscard]] auto opened_ok(const auto& aResult) -> bool
	{
		return aResult.has_value();
	}

#if !defined(_WIN32)
	// A restart claim needs a second process. Reopening a handle in the same
	// process proves the rows are in memory somewhere, which was never in
	// doubt; only a fresh process proves they reached the disk.
	[[noreturn]] void write_in_a_child(
		const std::filesystem::path& aPath,
		const std::filesystem::path& aMarker)
	{
		auto store = queue_enrollment_database::open(aPath, { });
		if (!store) {
			std::_Exit(EXIT_FAILURE);
		}
		const auto written  = store->store(application::enrollment_record{
			.Ref        = ref(7),
			.Membership = watching("src/parser"),
			.Departed   = application::enrollment_departure::none,
		});
		const auto departed = store->store(application::enrollment_record{
			.Ref        = ref(9),
			.Membership = watching("src/billing"),
			.Departed   = application::enrollment_departure::access_lost,
		});
		if (!written || !departed) {
			std::_Exit(EXIT_FAILURE);
		}
		std::ofstream marker{ aMarker };
		marker << "done";
		marker.close();
		std::_Exit(EXIT_SUCCESS);
	}

	void test_enrollment_survives_a_separate_process()
	{
		const scratch_directory directory;
		const auto              path   = directory.file("queue.db");
		const auto              marker = directory.file("written");

		const auto child = ::fork();
		expect(child >= 0, "the child process starts");
		if (child == 0) {
			write_in_a_child(path, marker);
		}
		auto status = 0;
		expect(::waitpid(child, &status, 0) == child, "the child is reaped");
		expect(
			WIFEXITED(status) != 0 && WEXITSTATUS(status) == EXIT_SUCCESS,
			"and wrote its enrollments before exiting");
		expect(
			std::filesystem::exists(marker),
			"the child reached the end of its writes");

		// A new process — this one — opens the file the child left behind.
		auto reopened = queue_enrollment_database::open(path, { });
		expect(opened_ok(reopened), "this process opens what the child wrote");

		const auto records = reopened->load_all();
		expect(records.has_value() && records->size() == 2U, "both rows are there");

		const auto watched = std::ranges::find(
			*records, ref(7), &application::enrollment_record::Ref);
		expect(watched != records->end(), "the enrolled item survived");
		expect(
			watched->Membership.MatchedRule
				&& watched->Membership.Rule.Pattern == "src/parser",
			"with the rule that brought it in — the reason a reviewer needs "
			"when the path has since left the pull request");
		expect(watched->present(), "and it is still in the queue");

		const auto gone = std::ranges::find(
			*records, ref(9), &application::enrollment_record::Ref);
		expect(gone != records->end(), "the departed item survived too");
		expect(
			gone->Departed == application::enrollment_departure::access_lost,
			"with the reason it left, so the reviewer is told rather than "
			"finding it silently absent");
	}
#endif

	void test_a_newer_database_fails_closed()
	{
		const scratch_directory directory;
		const auto              path = directory.file("queue.db");

		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store), "a fresh store opens");
		}

		// Stamp it as something this build cannot understand.
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(),
					&raw,
					SQLITE_OPEN_READWRITE | SQLITE_OPEN_EXRESCODE,
					nullptr)
					== SQLITE_OK,
				"the file opens for tampering");
			expect(
				::sqlite3_exec(raw, "PRAGMA user_version = 99", nullptr, nullptr, nullptr)
					== SQLITE_OK,
				"and takes a newer version");
			(void)::sqlite3_close_v2(raw);
		}

		auto refused = queue_enrollment_database::open(path, { });
		expect(!refused.has_value(), "the newer database is refused");
		// The *reason* matters, and asserting only that the open failed would
		// not test the forward-only rule at all. Without the version guard the
		// open still fails — the first CREATE hits a table that already
		// exists — so a test that stopped at "it failed" would pass against a
		// build that had no forward-only policy whatsoever.
		expect(
			refused.error() == catalog_database_failure::schema_mismatch,
			"because this build will not read a schema it does not know, not "
			"because it tripped over its own CREATE");

		// And the refusal changed nothing. A build that refused *after*
		// writing would have destroyed exactly the data it was protecting.
		::sqlite3* raw = nullptr;
		expect(
			::sqlite3_open_v2(
				path.string().c_str(),
				&raw,
				SQLITE_OPEN_READONLY | SQLITE_OPEN_EXRESCODE,
				nullptr)
				== SQLITE_OK,
			"the refused file still opens");
		::sqlite3_stmt* statement = nullptr;
		expect(
			::sqlite3_prepare_v2(
				raw, "PRAGMA user_version", -1, &statement, nullptr)
				== SQLITE_OK,
			"its version can be read");
		expect(::sqlite3_step(statement) == SQLITE_ROW, "the version is there");
		const auto version = ::sqlite3_column_int64(statement, 0);
		(void)::sqlite3_finalize(statement);
		(void)::sqlite3_close_v2(raw);
		expect(version == 99, "and is untouched by the refusal");
	}

	// The store holds no source and no comment body. Checked against the
	// schema the build actually creates rather than the insert statements
	// somebody remembered to read.
	void test_the_schema_holds_no_repository_content()
	{
		const scratch_directory directory;
		auto                    store = queue_enrollment_database::open(directory.file("queue.db"), { });
		expect(opened_ok(store), "the store opens");

		const auto columns = queue_enrollment_columns(store->handle());
		expect(columns.has_value() && !columns->empty(), "the schema has columns");

		constexpr auto prohibited = std::array<std::string_view, 8>{
			"body",
			"text",
			"comment",
			"diff",
			"source",
			"title",
			"branch",
			"patch",
		};
		for (const auto& column : *columns) {
			const auto name = column.substr(column.find('.') + 1U);
			for (const auto forbidden : prohibited) {
				expect(
					name.find(forbidden) == std::string::npos,
					"no column names anything a pull request contains");
			}
		}

		expect(
			std::ranges::find(*columns, "enrollment.rule_pattern")
				!= columns->end(),
			"the rule pattern is stored, because it is what the reviewer typed "
			"into their own rule rather than anything the repository holds");
	}

	// Deleting the queue must lose only the queue. It is a separate file from
	// the review ledger for the same reason the ledger is separate from the
	// epoch catalog.
	void test_deleting_the_queue_leaves_the_ledger_intact()
	{
		const scratch_directory directory;
		const auto              queuePath  = directory.file("queue.db");
		const auto              ledgerPath = directory.file("ledger.db");

		std::int64_t scope   = 0;
		const auto   receipt = core::file_receipt{
			.File     = core::file_identity{ std::array<std::uint8_t, 32>{ } },
			.Material = core::file_material_id{ std::array<std::uint8_t, 32>{ } },
			.Read     = core::file_read_state::read,
		};
		{
			auto queue  = queue_enrollment_database::open(queuePath, { });
			auto ledger = review_ledger_database::open(ledgerPath, { });
			expect(opened_ok(queue) && opened_ok(ledger), "both stores open");
			expect(
				queue
					->store(application::enrollment_record{
						.Ref        = ref(7),
						.Membership = watching("src/parser"),
					})
					.has_value(),
				"the queue records an enrollment");
			const auto id = ledger->scope_id(review_scope{
				.Host        = "github.com",
				.Account     = "reviewer",
				.Repository  = "octo/app",
				.PullRequest = 7,
			});
			expect(id.has_value(), "the ledger records its scope");
			scope = *id;
			expect(
				ledger->store_receipt(scope, receipt).has_value(),
				"and a receipt");
		}

		for (const auto suffix : { "", "-wal", "-shm" }) {
			std::error_code ignored;
			std::filesystem::remove(queuePath.string() + suffix, ignored);
		}
		expect(!std::filesystem::exists(queuePath), "the queue file is gone");

		auto ledger = review_ledger_database::open(ledgerPath, { });
		expect(opened_ok(ledger), "the ledger still opens");
		const auto kept = ledger->load_receipt(scope, receipt.File);
		expect(
			kept.has_value() && kept->has_value(),
			"and the reviewer's reading progress is untouched — clearing a "
			"queue must not lose what they had already read");

		auto recreated = queue_enrollment_database::open(queuePath, { });
		expect(opened_ok(recreated), "a clean queue is recreated in its place");
		const auto empty = recreated->load_all();
		expect(
			empty.has_value() && empty->empty(),
			"holding nothing, which is what deletion means");
	}

	void test_storing_the_same_item_twice_replaces_rather_than_duplicates()
	{
		const scratch_directory directory;
		auto                    store = queue_enrollment_database::open(directory.file("queue.db"), { });
		expect(opened_ok(store), "the store opens");

		expect(
			store
				->store(application::enrollment_record{
					.Ref        = ref(7),
					.Membership = watching("src/parser"),
				})
				.has_value(),
			"the first write lands");
		expect(
			store
				->store(application::enrollment_record{
					.Ref        = ref(7),
					.Membership = watching("src/parser"),
					.Departed   = application::enrollment_departure::closed,
				})
				.has_value(),
			"and so does the second");

		const auto records = store->load_all();
		expect(records.has_value() && records->size() == 1U, "one row, not two");
		expect(
			records->front().Departed
				== application::enrollment_departure::closed,
			"carrying the later state");
	}
} // namespace

auto main() -> int
{
#if !defined(_WIN32)
	test_enrollment_survives_a_separate_process();
#endif
	test_a_newer_database_fails_closed();
	test_the_schema_holds_no_repository_content();
	test_deleting_the_queue_leaves_the_ledger_intact();
	test_storing_the_same_item_twice_replaces_rather_than_duplicates();

	std::cout << "queue-enrollment-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
