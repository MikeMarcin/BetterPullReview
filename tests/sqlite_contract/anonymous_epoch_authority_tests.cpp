// The catalog's authority is discriminated by access mode. These courses prove
// the three things that makes structural rather than conventional: a catalog
// written under the undiscriminated v1 schema migrates in one transaction to
// the v2 schema with every row rewritten as authenticated and nothing else
// changed; the v2 constraints admit a zero account or viewer only under the
// public mode and a nonzero one only under the authenticated mode, so no zero
// identity is ever a sentinel; and a version this build cannot read is refused
// rather than repaired, migrated, or recreated.
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "epoch_catalog_legacy_fixture.hpp"
#include "platform_test_support.hpp"

#include <sqlite3.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
	namespace app           = bpr::application;
	namespace core          = bpr::core;
	namespace sqlite        = bpr::adapters::sqlite;
	namespace test_platform = bpr::tests::sqlite_contract;

	constexpr std::uint64_t Profile         = 0x1102030405060701ULL;
	constexpr std::uint64_t ProfileRevision = 0x1102030405060702ULL;
	constexpr std::uint64_t Host            = 0x1102030405060703ULL;
	constexpr std::uint64_t Account         = 0x1102030405060704ULL;
	constexpr std::uint64_t Viewer          = 0x1102030405060705ULL;
	constexpr std::uint64_t Repository      = 0x1102030405060706ULL;
	constexpr std::uint64_t PullRequest     = 0x1102030405060707ULL;
	constexpr std::uint64_t Session         = 0x1102030405060708ULL;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Value>
	void expect_failure(
		const app::epoch_catalog_result<Value>& aResult,
		const app::epoch_catalog_failure        aFailure,
		const std::string_view                  aMessage)
	{
		expect(!aResult && aResult.error() == aFailure, aMessage);
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = aSeed;
		bytes[17]  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel)
		{
			const auto base = std::filesystem::temp_directory_path();
			for (std::uint32_t attempt = 0U; attempt < 1'000U; ++attempt) {
				const auto name = std::string{ "bpr-sqlite-anon-" }
					+ std::string{ aLabel } + "-"
					+ std::to_string(test_platform::process_id()) + "-"
					+ std::to_string(attempt);
				auto            candidate = base / name;
				std::error_code error;
				if (std::filesystem::create_directory(candidate, error)) {
					expect(
						test_platform::make_owner_private(candidate, true),
						"temporary root becomes owner-private");
					Path = std::filesystem::canonical(candidate);
					return;
				}
				expect(
					!error || error == std::errc::file_exists,
					"temporary root creation fails only for a collision");
			}
			expect(false, "a unique temporary root is available");
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		~temporary_root()
		{
			if (!Path.empty()) {
				std::error_code ignored;
				(void)test_platform::make_owner_private(Path, true);
				std::filesystem::remove_all(Path, ignored);
			}
		}

		[[nodiscard]] auto path() const -> const std::filesystem::path&
		{
			return Path;
		}

	private:
		std::filesystem::path Path;
	};

	class direct_database final
	{
	public:
		explicit direct_database(const std::filesystem::path& aPath)
		{
			const auto native = test_platform::sqlite_path(aPath);
			expect(
				::sqlite3_open_v2(
					native.c_str(),
					&Handle,
					SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX
						| SQLITE_OPEN_NOFOLLOW,
					test_platform::sqlite_vfs())
						== SQLITE_OK
					&& Handle != nullptr,
				"the catalog is directly inspectable through pinned SQLite");
		}

		direct_database(const direct_database&)                    = delete;
		auto operator=(const direct_database&) -> direct_database& = delete;

		~direct_database()
		{
			if (Handle != nullptr) {
				expect(
					::sqlite3_close_v2(Handle) == SQLITE_OK,
					"direct SQLite inspection closes cleanly");
			}
		}

		void execute(const std::string_view aSql)
		{
			char*      message = nullptr;
			const auto text    = std::string{ aSql };
			if (::sqlite3_exec(Handle, text.c_str(), nullptr, nullptr, &message)
				!= SQLITE_OK) {
				const auto detail = message == nullptr ? std::string{ }
													   : std::string{ message };
				::sqlite3_free(message);
				std::cerr << "FAIL: SQLite execution: " << detail << '\n'
						  << text << '\n';
				std::exit(EXIT_FAILURE);
			}
		}

		[[nodiscard]] auto execute_code(const std::string_view aSql) const -> int
		{
			const auto text    = std::string{ aSql };
			char*      message = nullptr;
			const auto code
				= ::sqlite3_exec(Handle, text.c_str(), nullptr, nullptr, &message);
			::sqlite3_free(message);
			return static_cast<int>(static_cast<unsigned>(code) & 0xFFU);
		}

		[[nodiscard]] auto integer(const std::string_view aSql) const
			-> std::int64_t
		{
			::sqlite3_stmt* statement = nullptr;
			const auto      text      = std::string{ aSql };
			expect(
				::sqlite3_prepare_v3(
					Handle,
					text.c_str(),
					static_cast<int>(text.size()),
					SQLITE_PREPARE_NO_VTAB,
					&statement,
					nullptr)
					== SQLITE_OK,
				"integer inspection query prepares");
			expect(
				::sqlite3_step(statement) == SQLITE_ROW,
				"integer inspection query yields a row");
			const auto value = ::sqlite3_column_int64(statement, 0);
			expect(
				::sqlite3_finalize(statement) == SQLITE_OK,
				"integer inspection query finalizes");
			return value;
		}

	private:
		::sqlite3* Handle = nullptr;
	};

	// Exactly the three v1 objects whose constraints the v2 schema relaxed,
	// plus the indexes that fall with them. This is the schema as BPR shipped
	// it before the access mode existed; a catalog rebuilt onto it is a real
	// v1 catalog rather than a v2 one with a rewritten version number.
	constexpr std::string_view V1CatalogMeta
		= "CREATE TABLE catalog_meta("
		  "singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),"
		  "schema_format INTEGER NOT NULL CHECK(schema_format = 1),"
		  "incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND "
		  "incarnation != zeroblob(32))) STRICT, WITHOUT ROWID";
	constexpr std::string_view V1RepositoryScope
		= "CREATE TABLE repository_scope("
		  "incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),"
		  "cache_scope BLOB NOT NULL CHECK(length(cache_scope) = 32 AND cache_scope != zeroblob(32)),"
		  "profile_id BLOB NOT NULL CHECK(length(profile_id) = 8 AND profile_id != zeroblob(8)),"
		  "profile_revision BLOB NOT NULL CHECK(length(profile_revision) = 8 AND profile_revision != zeroblob(8)),"
		  "host_id BLOB NOT NULL CHECK(length(host_id) = 8 AND host_id != zeroblob(8)),"
		  "host_authority BLOB NOT NULL CHECK(length(host_authority) = 32 AND host_authority != zeroblob(32)),"
		  "account_id BLOB NOT NULL CHECK(length(account_id) = 8 AND account_id != zeroblob(8)),"
		  "repository_id BLOB NOT NULL CHECK(length(repository_id) = 8 AND repository_id != zeroblob(8)),"
		  "object_format INTEGER NOT NULL CHECK(object_format IN (1, 2)),"
		  "lifecycle INTEGER NOT NULL CHECK(lifecycle IN (1, 2, 3, 4)),"
		  "PRIMARY KEY(incarnation, cache_scope),"
		  "FOREIGN KEY(incarnation) REFERENCES catalog_meta(incarnation)"
		  ") STRICT, WITHOUT ROWID";
	constexpr std::string_view V1PullRequestScope
		= "CREATE TABLE pull_request_scope("
		  "incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),"
		  "pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),"
		  "cache_scope BLOB NOT NULL CHECK(length(cache_scope) = 32 AND cache_scope != zeroblob(32)),"
		  "viewer_id BLOB NOT NULL CHECK(length(viewer_id) = 8 AND viewer_id != zeroblob(8)),"
		  "pull_request_id BLOB NOT NULL CHECK(length(pull_request_id) = 8 AND pull_request_id != zeroblob(8)),"
		  "pull_request_number INTEGER NOT NULL CHECK(pull_request_number BETWEEN 1 AND 2147483647),"
		  "next_ordinal INTEGER CHECK(next_ordinal IS NULL OR next_ordinal > 0),"
		  "catalog_revision INTEGER NOT NULL CHECK(catalog_revision >= 0),"
		  "retained_epoch_count INTEGER NOT NULL CHECK(retained_epoch_count BETWEEN 0 AND 100000),"
		  "latest_live_ordinal INTEGER CHECK(latest_live_ordinal IS NULL OR latest_live_ordinal > 0),"
		  "previous_distinct_head_ordinal INTEGER CHECK(previous_distinct_head_ordinal IS NULL OR previous_distinct_head_ordinal > 0),"
		  "lifecycle INTEGER NOT NULL CHECK(lifecycle IN (1, 2, 3, 4)),"
		  "retention_rehydration_required INTEGER NOT NULL CHECK(retention_rehydration_required IN (0, 1)),"
		  "PRIMARY KEY(incarnation, pr_scope),"
		  "FOREIGN KEY(incarnation, cache_scope) REFERENCES repository_scope(incarnation, cache_scope),"
		  "FOREIGN KEY(incarnation, pr_scope, latest_live_ordinal) REFERENCES epoch(incarnation, pr_scope, ordinal),"
		  "FOREIGN KEY(incarnation, pr_scope, previous_distinct_head_ordinal) REFERENCES epoch(incarnation, pr_scope, ordinal)"
		  ") STRICT, WITHOUT ROWID";

	// Rebuilds a live v2 catalog back onto the v1 schema, carrying every row
	// across unchanged and dropping only the column v2 added. The result is
	// the input the migration is specified against.
	void downgrade_to_v1(const std::filesystem::path& aDatabase)
	{
		direct_database database{ aDatabase };
		database.execute("PRAGMA foreign_keys = OFF");
		database.execute("BEGIN IMMEDIATE");
		database.execute("DROP TABLE append_receiving");
		database.execute("CREATE TABLE t_meta AS SELECT * FROM catalog_meta");
		database.execute(
			"CREATE TABLE t_repo AS SELECT * FROM repository_scope");
		database.execute(
			"CREATE TABLE t_prs AS SELECT * FROM pull_request_scope");
		database.execute("DROP TABLE pull_request_scope");
		database.execute("DROP TABLE repository_scope");
		database.execute("DROP TABLE catalog_meta");
		database.execute(V1CatalogMeta);
		database.execute(
			"CREATE UNIQUE INDEX catalog_meta_incarnation ON "
			"catalog_meta(incarnation)");
		database.execute(V1RepositoryScope);
		database.execute(
			"CREATE UNIQUE INDEX repository_scope_active_authority ON "
			"repository_scope(incarnation, profile_id, profile_revision, "
			"host_id, host_authority, account_id, repository_id, "
			"object_format) WHERE lifecycle != 4");
		database.execute(V1PullRequestScope);
		database.execute(
			"CREATE UNIQUE INDEX pull_request_scope_cache_id ON "
			"pull_request_scope(incarnation, cache_scope, pull_request_id)");
		database.execute(
			"CREATE UNIQUE INDEX pull_request_scope_cache_number ON "
			"pull_request_scope(incarnation, cache_scope, "
			"pull_request_number)");
		database.execute(
			"CREATE UNIQUE INDEX pull_request_scope_cache_binding ON "
			"pull_request_scope(incarnation, cache_scope, pr_scope)");
		database.execute(
			"INSERT INTO catalog_meta SELECT singleton, 1, incarnation "
			"FROM t_meta");
		database.execute(
			"INSERT INTO repository_scope SELECT incarnation, cache_scope, "
			"profile_id, profile_revision, host_id, host_authority, "
			"account_id, repository_id, object_format, lifecycle FROM t_repo");
		database.execute(
			"INSERT INTO pull_request_scope SELECT incarnation, pr_scope, "
			"cache_scope, viewer_id, pull_request_id, pull_request_number, "
			"next_ordinal, catalog_revision, retained_epoch_count, "
			"latest_live_ordinal, previous_distinct_head_ordinal, lifecycle, "
			"retention_rehydration_required FROM t_prs");
		database.execute("DROP TABLE t_prs");
		database.execute("DROP TABLE t_repo");
		database.execute("DROP TABLE t_meta");
		database.execute("PRAGMA user_version = 1");
		database.execute("COMMIT");
	}

	[[nodiscard]] auto authenticated_key(
		const std::uint64_t aRepository  = Repository,
		const std::uint64_t aPullRequest = PullRequest,
		const std::uint32_t aNumber      = 9001U)
		-> app::epoch_catalog_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ Profile },
			.ProfileRevision = core::github_profile_revision{ ProfileRevision },
			.Host            = core::git_host_id{ Host },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(0x41U),
			.Account         = core::github_account_id{ Account },
			.Viewer          = core::github_viewer_actor_id{ Viewer },
			.Repository      = core::repository_id{ aRepository },
			.PullRequest     = core::pull_request_id{ aPullRequest },
			.Number          = core::pull_request_number{ aNumber },
			.ObjectFormat    = core::git_hash_algorithm::sha1,
			.Mode            = core::github_access_mode::authenticated,
		};
	}

	// The same repository and pull request, read signed out. Account and
	// viewer are absent rather than zeroed: the mode is what says there is no
	// reader, and `valid` refuses this key if either carries a value.
	[[nodiscard]] auto anonymous_key(
		const std::uint64_t aRepository  = Repository,
		const std::uint64_t aPullRequest = PullRequest,
		const std::uint32_t aNumber      = 9001U)
		-> app::epoch_catalog_authority_key
	{
		auto key    = authenticated_key(aRepository, aPullRequest, aNumber);
		key.Account = core::github_account_id{ };
		key.Viewer  = core::github_viewer_actor_id{ };
		key.Mode    = core::github_access_mode::public_preview;
		return key;
	}

	[[nodiscard]] auto open_request(
		const app::epoch_catalog_authority_key& aAuthority,
		const std::uint64_t                     aSession = Session)
		-> app::epoch_catalog_open_request
	{
		return {
			.ApplicationSession = core::application_session_id{ aSession },
			.Authority          = aAuthority,
		};
	}

	[[nodiscard]] auto database_path(const std::filesystem::path& aRoot)
		-> std::filesystem::path
	{
		return aRoot / "state" / "v1" / "epoch-catalog.sqlite3";
	}

	// A v1 catalog carrying real authenticated rows migrates to v2 with every
	// row preserved and rewritten as authenticated, and the identities it
	// allocated before the migration resolve to exactly the same values after.
	void migration_contract()
	{
		temporary_root                        root{ "migrate" };
		core::repository_cache_scope_id       cache;
		core::pull_request_authority_scope_id scope;
		core::epoch_catalog_incarnation_id    incarnation;
		{
			auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "v1 fixture root bootstraps a catalog");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(authenticated_key()), { });
			expect(opened.has_value(), "authenticated authority opens");
			const auto binding = (*opened)->binding();
			cache              = binding.Authority.Cache;
			scope              = binding.Authority.Scope;
			incarnation        = binding.Catalog.Incarnation;
			auto second        = port->open(
				open_request(
					authenticated_key(Repository + 1U, PullRequest + 1U, 9002U),
					Session + 1U),
				{ });
			expect(second.has_value(), "second authenticated authority opens");
		}

		downgrade_to_v1(database_path(root.path()));
		std::int64_t repositoryRows  = 0;
		std::int64_t pullRequestRows = 0;
		{
			direct_database database{ database_path(root.path()) };
			expect(
				database.integer("PRAGMA user_version") == 1,
				"the fixture is a v1 catalog before the migration");
			expect(
				database.integer(
					"SELECT count(*) FROM sqlite_schema "
					"WHERE name = 'repository_scope' "
					"AND instr(sql, 'access_mode') != 0")
					== 0,
				"the v1 fixture carries no access mode column");
			repositoryRows
				= database.integer("SELECT count(*) FROM repository_scope");
			pullRequestRows
				= database.integer("SELECT count(*) FROM pull_request_scope");
			expect(
				repositoryRows == 2 && pullRequestRows == 2,
				"the v1 fixture holds the rows the migration must carry");
		}

		{
			auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "a v1 catalog opens through the migration");
			auto port     = std::move(*made);
			auto reopened = port->open(
				open_request(authenticated_key(), Session + 2U), { });
			expect(reopened.has_value(), "the migrated authority reopens");
			const auto binding = (*reopened)->binding();
			expect(
				binding.Authority.Cache == cache
					&& binding.Authority.Scope == scope
					&& binding.Catalog.Incarnation == incarnation,
				"migration preserves every allocated identity exactly");
			expect(
				binding.Authority.Mode == core::github_access_mode::authenticated,
				"a migrated authority is the authenticated authority");
		}

		{
			direct_database database{ database_path(root.path()) };
			expect(
				database.integer("PRAGMA user_version") == 3
					&& database.integer(
						   "SELECT schema_format FROM catalog_meta "
						   "WHERE singleton = 1")
						== 3,
				"the migrated catalog reports the v3 version everywhere");
			expect(
				database.integer("SELECT count(*) FROM repository_scope")
						== repositoryRows
					&& database.integer(
						   "SELECT count(*) FROM pull_request_scope")
						== pullRequestRows,
				"the migration changes no row count");
			expect(
				database.integer(
					"SELECT count(*) FROM repository_scope "
					"WHERE access_mode = 1")
						== repositoryRows
					&& database.integer(
						   "SELECT count(*) FROM pull_request_scope "
						   "WHERE access_mode = 1")
						== pullRequestRows,
				"every migrated row is an authenticated row");
			expect(
				database.integer(
					"SELECT count(*) FROM repository_scope "
					"WHERE account_id = zeroblob(8)")
					== 0,
				"the migration invents no zero account");
		}
	}

	// The migration is one transaction. Interrupted at its commit, it leaves a
	// complete v1 catalog: the version, the schema, and the rows are all as
	// they were, and the next open migrates it again and succeeds.
	void interrupted_migration_contract()
	{
		temporary_root root{ "interrupt" };
		{
			auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "interruption fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(authenticated_key()), { });
			expect(opened.has_value(), "interruption fixture allocates a scope");
		}
		downgrade_to_v1(database_path(root.path()));

		sqlite::detail::testing::interrupt_next_migration_before_commit();
		expect_failure(
			sqlite::make_epoch_catalog({ .StorageRoot = root.path() }),
			app::epoch_catalog_failure::cancelled,
			"an interrupted migration fails closed");
		{
			direct_database database{ database_path(root.path()) };
			expect(
				database.integer("PRAGMA user_version") == 1,
				"an interrupted migration leaves the v1 version");
			expect(
				database.integer(
					"SELECT count(*) FROM sqlite_schema "
					"WHERE name = 'repository_scope' "
					"AND instr(sql, 'access_mode') != 0")
					== 0,
				"an interrupted migration leaves the v1 schema");
			expect(
				database.integer("SELECT count(*) FROM repository_scope") == 1
					&& database.integer(
						   "SELECT count(*) FROM pull_request_scope")
						== 1,
				"an interrupted migration leaves every v1 row");
			expect(
				database.integer(
					"SELECT count(*) FROM sqlite_schema WHERE name LIKE "
					"'migration_%'")
					== 0,
				"an interrupted migration leaves no staging table behind");
		}

		auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "the next open migrates the same catalog");
		direct_database database{ database_path(root.path()) };
		expect(
			database.integer("PRAGMA user_version") == 3,
			"the retried migration completes");
	}

	// A version this build never wrote is refused, and refusing it changes
	// nothing: the catalog is still there, still at its own version, with its
	// rows intact.
	void version_two_migration_rollback_contract()
	{
		temporary_root             root{ "v2-migration" };
		app::epoch_catalog_binding original;
		{
			auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "v2 migration fixture bootstraps");
			auto opened = (*made)->open(open_request(authenticated_key()), { });
			expect(opened.has_value(), "v2 migration authority opens");
			original = (*opened)->binding();
		}
		{
			direct_database database{ database_path(root.path()) };
			expect(test_platform::make_version_two_catalog(database), "restore exact legacy v2 schema");
		}
		sqlite::detail::testing::interrupt_next_migration_before_commit();
		expect_failure(sqlite::make_epoch_catalog({ .StorageRoot = root.path() }),
					   app::epoch_catalog_failure::cancelled, "interrupted v2 migration rolls back");
		{
			direct_database database{ database_path(root.path()) };
			expect(database.integer("PRAGMA user_version") == 2
					   && database.integer("SELECT schema_format FROM catalog_meta") == 2
					   && database.integer("SELECT count(*) FROM sqlite_schema WHERE name = 'append_receiving' OR name LIKE 'migration_%'") == 0
					   && database.integer("SELECT count(*) FROM repository_scope") == 1
					   && database.integer("SELECT count(*) FROM pull_request_scope") == 1,
				   "rollback preserves complete v2 schema and authority rows without migration residue");
		}
		auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "v2 migration retry succeeds");
		auto opened = (*made)->open(open_request(authenticated_key()), { });
		expect(opened.has_value() && (*opened)->binding() == original, "v2 migration preserves exact catalog and authority identity");
	}

	void unknown_version_contract()
	{
		for (const auto version : { 0, 4, 99 }) {
			temporary_root root{ "unknown-version" };
			{
				auto made
					= sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
				expect(made.has_value(), "unknown-version fixture bootstraps");
				auto port   = std::move(*made);
				auto opened = port->open(open_request(authenticated_key()), { });
				expect(opened.has_value(), "unknown-version fixture allocates");
			}
			{
				direct_database database{ database_path(root.path()) };
				database.execute(
					"PRAGMA user_version = " + std::to_string(version));
			}
			expect_failure(
				sqlite::make_epoch_catalog({ .StorageRoot = root.path() }),
				app::epoch_catalog_failure::schema_mismatch,
				"an unknown catalog version is refused");
			direct_database database{ database_path(root.path()) };
			expect(
				database.integer("PRAGMA user_version") == version,
				"a refused catalog is not migrated");
			expect(
				database.integer("SELECT count(*) FROM repository_scope") == 1,
				"a refused catalog is not recreated");
		}
	}

	// The two modes are two namespaces. The same repository read signed out and
	// signed in gets two rows, two cache scopes, and two pull-request scopes,
	// and neither can be reached with the other's key.
	void discriminated_namespace_contract()
	{
		temporary_root root{ "namespace" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "namespace fixture bootstraps");
		auto port = std::move(*made);

		auto authenticated = port->open(open_request(authenticated_key()), { });
		expect(authenticated.has_value(), "the authenticated authority opens");
		const auto authenticatedBinding = (*authenticated)->binding();

		auto anonymous
			= port->open(open_request(anonymous_key(), Session + 1U), { });
		expect(
			anonymous.has_value(),
			"the public-preview authority opens for the same repository");
		const auto anonymousBinding = (*anonymous)->binding();

		expect(
			anonymousBinding.Authority.Mode
				== core::github_access_mode::public_preview,
			"the anonymous binding carries the public-preview mode");
		expect(
			anonymousBinding.Authority.Account.value() == 0U
				&& anonymousBinding.Authority.Viewer.value() == 0U,
			"the anonymous binding names no reader");
		expect(
			anonymousBinding.Authority.Cache != authenticatedBinding.Authority.Cache,
			"the two modes never share a cache scope");
		expect(
			anonymousBinding.Authority.Scope != authenticatedBinding.Authority.Scope,
			"the two modes never share a pull-request scope");

		auto anonymousAgain
			= port->open(open_request(anonymous_key(), Session + 2U), { });
		expect(anonymousAgain.has_value(), "the anonymous authority reopens");
		expect(
			(*anonymousAgain)->binding().Authority.Cache
				== anonymousBinding.Authority.Cache,
			"the anonymous authority resolves its own exact cache scope");
		expect(
			(*anonymousAgain)->binding().Authority.Scope
				== anonymousBinding.Authority.Scope,
			"the anonymous authority resolves its own exact pull-request scope");

		direct_database database{ database_path(root.path()) };
		expect(
			database.integer(
				"SELECT count(*) FROM repository_scope WHERE access_mode = 2 "
				"AND account_id = zeroblob(8)")
				== 1,
			"the anonymous repository row carries a zero account under mode two");
		expect(
			database.integer(
				"SELECT count(*) FROM pull_request_scope WHERE access_mode = 2 "
				"AND viewer_id = zeroblob(8)")
				== 1,
			"the anonymous pull-request row carries a zero viewer under mode two");
		expect(
			database.integer(
				"SELECT count(*) FROM repository_scope WHERE access_mode = 1 "
				"AND account_id != zeroblob(8)")
				== 1,
			"the authenticated repository row keeps its exact account");
	}

	// The schema, not the caller, is what refuses a zero identity outside the
	// public mode and a named one inside it. Each of these is written straight
	// at the table with the adapter out of the way.
	void constraint_contract()
	{
		temporary_root root{ "constraints" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "constraint fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(authenticated_key()), { });
		expect(opened.has_value(), "constraint fixture allocates a scope");
		const auto binding = (*opened)->binding();
		static_cast<void>(binding);

		direct_database database{ database_path(root.path()) };
		const auto      insertRepository =
			[](const std::string_view aAccount, const int aMode) {
				return std::string{
					"INSERT INTO repository_scope(incarnation, cache_scope, "
					"profile_id, profile_revision, host_id, host_authority, "
					"account_id, repository_id, object_format, access_mode, "
					"lifecycle) SELECT incarnation, randomblob(32), profile_id, "
					"profile_revision, host_id, host_authority, "
				}
				+ std::string{ aAccount }
				+ ", randomblob(8), 1, " + std::to_string(aMode)
					+ ", 1 FROM repository_scope LIMIT 1";
			};
		expect(
			database.execute_code(insertRepository("zeroblob(8)", 1))
				== SQLITE_CONSTRAINT,
			"a zero account is refused under the authenticated mode");
		expect(
			database.execute_code(insertRepository("randomblob(8)", 2))
				== SQLITE_CONSTRAINT,
			"a named account is refused under the public-preview mode");
		expect(
			database.execute_code(insertRepository("randomblob(8)", 3))
				== SQLITE_CONSTRAINT,
			"a mode outside the enumeration is refused");
		expect(
			database.execute_code(insertRepository("zeroblob(8)", 2))
				== SQLITE_OK,
			"a zero account is admitted only under the public-preview mode");

		// The pull-request scope's mode must be its repository scope's. A row
		// that reads one repository under the other mode is a mixed-authority
		// row, and the foreign key refuses it.
		expect(
			database.execute_code(
				"INSERT INTO pull_request_scope(incarnation, pr_scope, "
				"cache_scope, viewer_id, access_mode, pull_request_id, "
				"pull_request_number, next_ordinal, catalog_revision, "
				"retained_epoch_count, latest_live_ordinal, "
				"previous_distinct_head_ordinal, lifecycle, "
				"retention_rehydration_required) SELECT incarnation, "
				"randomblob(32), cache_scope, zeroblob(8), 2, randomblob(8), "
				"4242, 1, 0, 0, NULL, NULL, 1, 0 FROM repository_scope "
				"WHERE access_mode = 1 LIMIT 1")
				== SQLITE_CONSTRAINT,
			"a public-preview scope cannot reference an authenticated cache");
	}

	// A catalog written signed out reopens signed out. The scopes it allocated
	// come back exactly, with no reader identity anywhere in them, from a
	// process that never saw the first one.
	void anonymous_restart_contract()
	{
		temporary_root                        root{ "restart" };
		core::repository_cache_scope_id       cache;
		core::pull_request_authority_scope_id scope;
		core::epoch_catalog_incarnation_id    incarnation;
		{
			auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "restart fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(anonymous_key()), { });
			expect(opened.has_value(), "the anonymous authority opens");
			const auto binding = (*opened)->binding();
			cache              = binding.Authority.Cache;
			scope              = binding.Authority.Scope;
			incarnation        = binding.Catalog.Incarnation;
		}

		auto made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "the anonymous catalog reopens after restart");
		auto port     = std::move(*made);
		auto reopened = port->open(open_request(anonymous_key(), Session + 9U), { });
		expect(reopened.has_value(), "the anonymous authority reopens");
		const auto binding = (*reopened)->binding();
		expect(
			binding.Authority.Cache == cache && binding.Authority.Scope == scope
				&& binding.Catalog.Incarnation == incarnation,
			"an anonymous restart resolves its exact persisted identities");
		expect(
			binding.Authority.Mode == core::github_access_mode::public_preview
				&& binding.Authority.Account.value() == 0U
				&& binding.Authority.Viewer.value() == 0U,
			"an anonymous restart reopens without a reader identity");
	}
} // namespace

auto main() -> int
{
	migration_contract();
	interrupted_migration_contract();
	version_two_migration_rollback_contract();
	unknown_version_contract();
	discriminated_namespace_contract();
	constraint_contract();
	anonymous_restart_contract();
	std::cout << "sqlite-anonymous-epoch-authority-tests:pass\n";
	return 0;
}
