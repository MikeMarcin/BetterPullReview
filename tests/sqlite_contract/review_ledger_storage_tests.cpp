#include "../../src/adapters/sqlite/src/review_ledger_database.hpp"
#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::adapters::sqlite::detail;

	[[nodiscard]] auto describe(const catalog_database_failure aFailure)
		-> std::string_view
	{
		switch (aFailure) {
		case catalog_database_failure::invalid_path:
			return "invalid_path";
		case catalog_database_failure::invalid_permissions:
			return "invalid_permissions";
		case catalog_database_failure::unavailable:
			return "unavailable";
		case catalog_database_failure::schema_mismatch:
			return "schema_mismatch";
		case catalog_database_failure::integrity_failure:
			return "integrity_failure";
		case catalog_database_failure::quota_exceeded:
			return "quota_exceeded";
		case catalog_database_failure::cancelled:
			return "cancelled";
		case catalog_database_failure::platform_unsupported:
			return "platform_unsupported";
		}
		return "unknown";
	}

	/// \brief Reports the failure a refused open carried, not merely that it failed.
	auto opened_ok(
		const std::expected<review_ledger_database, catalog_database_failure>&
			aOpened) -> bool
	{
		if (!aOpened) {
			std::cerr << "  open failed: " << describe(aOpened.error()) << '\n';
		}
		return aOpened.has_value();
	}

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
				/ ("bpr-ledger-" + std::to_string(Counter++));
			std::filesystem::remove_all(Path);
			std::filesystem::create_directories(Path);
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

	[[nodiscard]] auto scope_of(
		const std::string_view aHost,
		const std::string_view aAccount,
		const std::string_view aRepository,
		const std::int64_t     aPullRequest) -> review_scope
	{
		return review_scope{
			.Host        = aHost,
			.Account     = aAccount,
			.Repository  = aRepository,
			.PullRequest = aPullRequest,
		};
	}

	// The prohibition is checked against the schema the build creates, not
	// against the insert statements someone remembered to read. A presentation
	// coordinate added later fails here even if nothing ever writes to it.
	void test_the_schema_stores_no_presentation_coordinate()
	{
		const scratch_directory directory;
		auto                    opened = review_ledger_database::open(directory.file("l.db"), { });
		expect(opened_ok(opened), "the ledger opens");

		const auto columns = review_ledger_columns(opened->handle());
		expect(columns.has_value() && !columns->empty(), "the schema reports its columns");

		// Substrings rather than exact names: `visual_row`, `row_index`, and
		// `scroll_offset` must all be caught, not just one spelling.
		constexpr auto forbidden = std::array{
			std::string_view{ "row" },
			std::string_view{ "index" },
			std::string_view{ "ribbon" },
			std::string_view{ "scroll" },
			std::string_view{ "viewport" },
			std::string_view{ "pixel" },
			std::string_view{ "coordinate" },
			std::string_view{ "widget" },
			std::string_view{ "token" },
			std::string_view{ "password" },
			std::string_view{ "secret" },
			std::string_view{ "credential" },
			std::string_view{ "body" },
			std::string_view{ "content" },
		};

		for (const auto& column : *columns) {
			for (const auto banned : forbidden) {
				const auto found = column.find(banned) != std::string::npos;
				if (found) {
					std::cerr << "  prohibited column: " << column
							  << " matches \"" << banned << "\"\n";
				}
				expect(
					!found,
					"no ledger column stores a presentation coordinate, a "
					"credential, or a duplicated source body");
			}
		}

		// The check would pass vacuously against an empty schema, so the
		// columns it did inspect are named.
		expect(
			std::ranges::any_of(
				*columns,
				[](const std::string& aColumn) {
					return aColumn == "file_receipt.material_id";
				}),
			"the enumeration reached the receipt table rather than nothing");
	}

	void test_receipts_do_not_leak_across_identity()
	{
		const scratch_directory directory;
		auto                    opened = review_ledger_database::open(directory.file("l.db"), { });
		expect(opened_ok(opened), "the ledger opens");

		// The same pull request number and repository name on four different
		// identities. Anything shorter than the full scope collides here.
		const auto first  = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		const auto second = opened->scope_id(scope_of("ghe.example.com", "octo", "app", 7));
		const auto third  = opened->scope_id(scope_of("github.com", "other", "app", 7));
		const auto fourth = opened->scope_id(scope_of("github.com", "octo", "other", 7));
		expect(
			first && second && third && fourth,
			"every identity resolves to a scope");
		expect(
			*first != *second && *first != *third && *first != *fourth
				&& *second != *third && *second != *fourth && *third != *fourth,
			"a reused pull request number never shares a scope across hosts, "
			"accounts, or repositories");

		const auto again = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(
			again && *again == *first,
			"the same identity resolves to the same scope rather than a new one");
	}

	// A migration mechanism whose only version is the initial one has never
	// migrated anything. Version 1 is built directly here so the 1-to-2 step
	// runs against a database that really was written by an older schema.
	void test_a_version_one_database_migrates_forward()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		{
			::sqlite3* raw  = nullptr;
			const auto code = ::sqlite3_open_v2(
				path.string().c_str(),
				&raw,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
			expect(code == SQLITE_OK && raw != nullptr, "the fixture opens");
			expect(configure_connection(raw).has_value(), "the fixture is configured");

			for (const auto sql : std::array{
					 std::string_view{
						 "CREATE TABLE ledger_meta("
						 "singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
						 "schema_format INTEGER NOT NULL) STRICT" },
					 std::string_view{
						 "CREATE TABLE review_scope("
						 "scope_id INTEGER PRIMARY KEY,"
						 "host TEXT NOT NULL, account TEXT NOT NULL,"
						 "repository TEXT NOT NULL, pull_request INTEGER NOT NULL,"
						 "UNIQUE(host, account, repository, pull_request)) STRICT" },
					 std::string_view{
						 "CREATE TABLE file_receipt("
						 "scope_id INTEGER NOT NULL "
						 "REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
						 "file_identity BLOB NOT NULL CHECK(length(file_identity) = 32),"
						 "material_id BLOB NOT NULL CHECK(length(material_id) = 32),"
						 "read_state INTEGER NOT NULL CHECK(read_state BETWEEN 0 AND 2),"
						 "accepted INTEGER NOT NULL CHECK(accepted IN (0, 1)),"
						 "excluded INTEGER NOT NULL CHECK(excluded IN (0, 1)),"
						 "PRIMARY KEY(scope_id, file_identity)) STRICT" },
					 std::string_view{ "PRAGMA application_id = 1112560196" },
					 std::string_view{
						 "INSERT INTO ledger_meta(singleton, schema_format) VALUES(1, 1)" },
					 std::string_view{
						 "INSERT INTO review_scope"
						 "(host, account, repository, pull_request) "
						 "VALUES('github.com', 'octo', 'app', 7)" },
					 std::string_view{ "PRAGMA user_version = 1" },
				 }) {
				expect(
					execute(raw, sql, { }).has_value(),
					"the version one fixture is built");
			}
			(void)::sqlite3_close(raw);
		}

		auto opened = review_ledger_database::open(path, { });
		expect(opened_ok(opened), "a version one database opens and migrates");

		const auto version = scalar_integer(opened->handle(), "PRAGMA user_version");
		expect(
			version && *version == review_ledger_schema_version,
			"the migrated database reports the current schema version");

		const auto columns = review_ledger_columns(opened->handle());
		expect(
			columns.has_value()
				&& std::ranges::any_of(
					*columns,
					[](const std::string& aColumn) {
						return aColumn == "conversation_receipt.seen_edit";
					}),
			"the migration added the table version two introduced");

		// The row written under version one survives the migration.
		const auto carried = opened->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(carried && *carried == 1, "the pre-migration row is still there");
	}

	void test_a_newer_database_fails_closed()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		{
			auto opened = review_ledger_database::open(path, { });
			expect(opened.has_value(), "the ledger is created at the current version");
			expect(
				execute(opened->handle(), "PRAGMA user_version = 99", { })
					.has_value(),
				"the fixture is stamped with a newer version");
		}

		const auto reopened = review_ledger_database::open(path, { });
		expect(
			!reopened.has_value()
				&& reopened.error() == catalog_database_failure::schema_mismatch,
			"a database written by a newer build is refused rather than opened");

		// Refusing is not enough: the refusal must happen before anything is
		// written. Validating the version afterwards would report the same
		// failure while having already rewritten the metadata of a database
		// this build does not understand.
		::sqlite3* raw = nullptr;
		expect(
			::sqlite3_open_v2(
				path.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr)
					== SQLITE_OK
				&& raw != nullptr,
			"the refused database can still be inspected");
		const auto format = scalar_integer(
			raw,
			"SELECT schema_format FROM ledger_meta WHERE singleton = 1");
		const auto stamped = scalar_integer(raw, "PRAGMA user_version");
		if (format) {
			std::cerr << "  schema_format after refusal: " << *format << '\n';
		}
		(void)::sqlite3_close(raw);
		expect(
			format && *format == review_ledger_schema_version,
			"a refused open leaves the recorded schema format untouched");
		expect(
			stamped && *stamped == 99,
			"a refused open leaves the newer version stamp exactly as it was");
	}

	void test_reopening_is_idempotent()
	{
		const scratch_directory directory;
		const auto              path = directory.file("l.db");

		auto first = review_ledger_database::open(path, { });
		expect(opened_ok(first), "the ledger is created");
		const auto id = first->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(id.has_value(), "a scope is written");
		// Close before reopening: the second open must read a committed file,
		// not share this connection's state.
		first = std::unexpected(catalog_database_failure::unavailable);

		auto second = review_ledger_database::open(path, { });
		expect(opened_ok(second), "the ledger reopens without migrating again");
		const auto same = second->scope_id(scope_of("github.com", "octo", "app", 7));
		expect(same && *same == *id, "the scope survives a clean reopen");
	}
} // namespace

auto main() -> int
{
	test_the_schema_stores_no_presentation_coordinate();
	test_receipts_do_not_leak_across_identity();
	test_a_version_one_database_migrates_forward();
	test_a_newer_database_fails_closed();
	test_reopening_is_idempotent();

	std::cout << "review-ledger-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
