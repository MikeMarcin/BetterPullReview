#include "../../src/adapters/sqlite/src/queue_enrollment_database.hpp"
#include "../../src/adapters/sqlite/src/review_ledger_database.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/manual_executor.hpp"
#include "scratch_root.hpp"

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
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::sqlite::detail;

	using bpr::tests::expect;

	/// \brief This contract's scratch root: `bpr-queue-` and a random part.
	/// \details [[BPR-1126]]. Made at a name nothing had, owner-private from
	/// creation, and removed only by the course that made it.
	class scratch_directory final : public bpr::tests::sqlite_contract::scratch_root
	{
	public:
		scratch_directory()
			: scratch_root{ "queue" }
		{
		}

		/// \brief Narrows the root to owner-only, as the state directory requires.
		[[nodiscard]] auto make_private() const -> bool
		{
#if defined(_WIN32)
			// [[BPR-0845]]. The stores' own rule: on Windows a directory under the
			// temporary directory inherits a list the stores refuse, and changing
			// mode bits does not change it.
			return bpr::adapters::sqlite::make_storage_root_owner_private(path());
#else
			std::error_code failed;
			std::filesystem::permissions(
				path(),
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace,
				failed);
			return !failed;
#endif
		}
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

	[[nodiscard]] auto read_binary(const std::filesystem::path& aPath)
		-> std::string
	{
		auto input = std::ifstream{ aPath, std::ios::binary };
		expect(input.good(), "queue database bytes are readable");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ },
		};
	}

	auto execute_raw_sql(
		const std::filesystem::path& aPath, const std::string_view aSql) -> void
	{
		::sqlite3* raw = nullptr;
		expect(
			::sqlite3_open_v2(
				aPath.string().c_str(),
				&raw,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXRESCODE,
				nullptr)
				== SQLITE_OK,
			"the raw queue fixture opens");
		const auto sql = std::string{ aSql };
		expect(
			::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
			"the raw queue fixture statements run");
		expect(::sqlite3_close_v2(raw) == SQLITE_OK, "the raw queue fixture closes");
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
			// [[BPR-1121]]. The child never unwinds into this process's frames.
			bpr::tests::end_forked_child([&] { write_in_a_child(path, marker); });
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

	void test_a_foreign_version_one_database_is_not_migrated()
	{
		const scratch_directory directory;
		const auto              path = directory.file("foreign.db");
		::sqlite3*              raw  = nullptr;
		expect(
			::sqlite3_open_v2(
				path.string().c_str(),
				&raw,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXRESCODE,
				nullptr)
				== SQLITE_OK,
			"the foreign fixture opens");
		const auto schema = "CREATE TABLE enrollment_meta(singleton INTEGER PRIMARY KEY "
							"CHECK(singleton = 1),schema_format INTEGER NOT NULL) STRICT;"
							"CREATE TABLE enrollment(host TEXT NOT NULL,account TEXT NOT NULL,"
							"repository TEXT NOT NULL,pull_request INTEGER NOT NULL,role INTEGER "
							"NOT NULL CHECK(role BETWEEN 0 AND 4),matched_rule INTEGER NOT NULL "
							"CHECK(matched_rule IN (0, 1)),rule_kind INTEGER NOT NULL CHECK(rule_kind "
							"BETWEEN 0 AND 2),rule_pattern TEXT NOT NULL,rule_excluding INTEGER NOT "
							"NULL CHECK(rule_excluding IN (0, 1)),departed INTEGER NOT NULL "
							"CHECK(departed BETWEEN 0 AND 4),PRIMARY KEY(host, account, repository, "
							"pull_request)) STRICT;"
							"INSERT INTO enrollment_meta VALUES(1, 1);"
							"INSERT INTO enrollment VALUES('foreign.example','account','owner/repo',"
							"7,0,1,0,'sentinel',0,0);"
							"PRAGMA application_id = 305419896;"
							"PRAGMA user_version = 1;";
		expect(
			::sqlite3_exec(raw, schema, nullptr, nullptr, nullptr) == SQLITE_OK,
			"the foreign v1 schema and sentinel are created");
		expect(::sqlite3_close_v2(raw) == SQLITE_OK, "the fixture closes");
		const auto before = read_binary(path);

		const auto refused = queue_enrollment_database::open(path, { });
		expect(!refused && refused.error() == catalog_database_failure::schema_mismatch,
			   "a foreign v1 database is refused before migration");
		expect(read_binary(path) == before,
			   "refusal preserves every byte of the foreign database");

		raw = nullptr;
		expect(
			::sqlite3_open_v2(
				path.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr)
				== SQLITE_OK,
			"the refused fixture remains readable");
		::sqlite3_stmt* statement = nullptr;
		expect(
			::sqlite3_prepare_v2(
				raw,
				"SELECT (SELECT count(*) FROM sqlite_schema WHERE name = "
				"'observer_rule'), schema_format, (SELECT rule_pattern FROM "
				"enrollment LIMIT 1) FROM enrollment_meta",
				-1,
				&statement,
				nullptr)
					== SQLITE_OK
				&& ::sqlite3_step(statement) == SQLITE_ROW,
			"the exact refused metadata remains queryable");
		expect(
			::sqlite3_column_int64(statement, 0) == 0
				&& ::sqlite3_column_int64(statement, 1) == 1
				&& std::string_view{ reinterpret_cast<const char*>(
					   ::sqlite3_column_text(statement, 2)) }
					== "sentinel",
			"no rule table, metadata stamp, or sentinel row was changed");
		(void)::sqlite3_finalize(statement);
		(void)::sqlite3_close_v2(raw);
	}

	// [[BPR-0989]]. The historical shapes the older-version fixtures carry.
	// Version seven rebuilt the enrollment and preference tables, so a fixture
	// made from the current store rebuilds them as they were before.
	// [[BPR-1683]]. Every version before eleven had no ignored table.
	constexpr std::string_view downgrade_enrollment_to_version_one
		= "DROP TABLE home_ignore;"
		  "CREATE TABLE old_enrollment AS SELECT host, account, repository, pull_request, role, matched_rule,"
		  " rule_kind, rule_pattern, rule_excluding, departed FROM enrollment;"
		  "DROP TABLE enrollment;"
		  "CREATE TABLE enrollment(host TEXT NOT NULL,account TEXT NOT NULL,repository TEXT NOT NULL,pull_request INTEGER NOT NULL,"
		  "role INTEGER NOT NULL CHECK(role BETWEEN 0 AND 4),matched_rule INTEGER NOT NULL CHECK(matched_rule IN (0, 1)),"
		  "rule_kind INTEGER NOT NULL CHECK(rule_kind BETWEEN 0 AND 2),rule_pattern TEXT NOT NULL,"
		  "rule_excluding INTEGER NOT NULL CHECK(rule_excluding IN (0, 1)),departed INTEGER NOT NULL CHECK(departed BETWEEN 0 AND 4),"
		  "PRIMARY KEY(host, account, repository, pull_request)) STRICT;"
		  "INSERT INTO enrollment SELECT * FROM old_enrollment;"
		  "DROP TABLE old_enrollment;";
	constexpr std::string_view downgrade_preferences_to_version_three
		= "CREATE TABLE old_preferences AS SELECT singleton, role_filter, observer_filter, bucket_filter, repository_filter,"
		  " quiet_filter, sort_order FROM presentation_preferences;"
		  "DROP TABLE presentation_preferences;"
		  "CREATE TABLE presentation_preferences(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
		  "role_filter INTEGER NOT NULL CHECK(role_filter BETWEEN -1 AND 4),observer_filter INTEGER NOT NULL CHECK(observer_filter BETWEEN -1 AND 2),"
		  "bucket_filter INTEGER NOT NULL CHECK(bucket_filter BETWEEN -1 AND 6),repository_filter TEXT NOT NULL,"
		  "quiet_filter INTEGER NOT NULL CHECK(quiet_filter BETWEEN 0 AND 2),sort_order INTEGER NOT NULL CHECK(sort_order BETWEEN 0 AND 2)) STRICT;"
		  "INSERT INTO presentation_preferences SELECT * FROM old_preferences;"
		  "DROP TABLE old_preferences;";
	constexpr std::string_view downgrade_preferences_to_version_six
		= "CREATE TABLE old_preferences AS SELECT * FROM presentation_preferences;"
		  "DROP TABLE presentation_preferences;"
		  "CREATE TABLE presentation_preferences(singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
		  "role_filter INTEGER NOT NULL CHECK(role_filter BETWEEN -1 AND 4),observer_filter INTEGER NOT NULL CHECK(observer_filter BETWEEN -1 AND 2),"
		  "bucket_filter INTEGER NOT NULL CHECK(bucket_filter BETWEEN -1 AND 6),repository_filter TEXT NOT NULL,"
		  "quiet_filter INTEGER NOT NULL CHECK(quiet_filter BETWEEN 0 AND 2),sort_order INTEGER NOT NULL CHECK(sort_order BETWEEN 0 AND 2),"
		  "notifications_enabled INTEGER NOT NULL CHECK(notifications_enabled IN (0, 1)),"
		  "notifications_paused INTEGER NOT NULL CHECK(notifications_paused IN (0, 1))) STRICT;"
		  "INSERT INTO presentation_preferences SELECT * FROM old_preferences;"
		  "DROP TABLE old_preferences;";

	[[nodiscard]] auto downgraded(const std::string_view aPreferences, const std::string_view aRest) -> std::string
	{
		auto sql = std::string{ downgrade_enrollment_to_version_one };
		sql += aPreferences;
		sql += aRest;
		return sql;
	}

	void test_a_legal_version_one_database_migrates_to_current()
	{
		const scratch_directory directory;
		const auto              path = directory.file("version-one.db");
		execute_raw_sql(
			path,
			"CREATE TABLE enrollment_meta(singleton INTEGER PRIMARY KEY "
			"CHECK(singleton = 1),schema_format INTEGER NOT NULL) STRICT;"
			"CREATE TABLE enrollment(host TEXT NOT NULL,account TEXT NOT NULL,"
			"repository TEXT NOT NULL,pull_request INTEGER NOT NULL,role INTEGER "
			"NOT NULL CHECK(role BETWEEN 0 AND 4),matched_rule INTEGER NOT NULL "
			"CHECK(matched_rule IN (0, 1)),rule_kind INTEGER NOT NULL CHECK(rule_kind "
			"BETWEEN 0 AND 2),rule_pattern TEXT NOT NULL,rule_excluding INTEGER NOT "
			"NULL CHECK(rule_excluding IN (0, 1)),departed INTEGER NOT NULL "
			"CHECK(departed BETWEEN 0 AND 4),PRIMARY KEY(host, account, repository, "
			"pull_request)) STRICT;"
			"INSERT INTO enrollment_meta VALUES(1, 1);"
			"INSERT INTO enrollment VALUES('github.com','reviewer','octo/app',"
			"7,0,1,2,'src/parser',0,0);"
			"PRAGMA application_id = 1112560209;"
			"PRAGMA user_version = 1;");

		auto migrated = queue_enrollment_database::open(path, { });
		expect(opened_ok(migrated), "a canonical version-one queue migrates");
		expect(
			scalar_integer(migrated->handle(), "PRAGMA user_version")
				== queue_enrollment_schema_version,
			"the migrated file carries the current SQLite version stamp");
		expect(
			scalar_integer(
				migrated->handle(),
				"SELECT schema_format FROM enrollment_meta WHERE singleton = 1")
				== queue_enrollment_schema_version,
			"the migrated metadata row carries the current schema format");
		const auto records = migrated->load_all();
		expect(
			records.has_value() && records->size() == 1U
				&& records->front().Ref == ref(7)
				&& records->front().Membership.Rule.Pattern == "src/parser",
			"migration preserves the exact version-one enrollment and reason");
		const auto rules = migrated->load_rules();
		expect(
			rules.has_value() && rules->empty(),
			"migration does not promote a copied enrollment reason into rule authority");
	}

	void test_a_legal_version_two_database_migrates_without_rewriting_queue_state()
	{
		const scratch_directory directory;
		const auto              path = directory.file("version-two.db");
		const auto              rule = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store), "the current queue fixture opens");
			expect(store->store_rule(rule).has_value(), "the exact rule is stored");
			expect(
				store
					->store(application::enrollment_record{
						.Ref = ref(7), .Membership = watching("src/parser") })
					.has_value(),
				"the exact enrollment is stored");
		}
		execute_raw_sql(
			path,
			downgraded({ },
					   "DROP TABLE presentation_preferences;"
					   "DROP TABLE saved_repository;"
					   "DROP TABLE saved_server;"
					   "UPDATE enrollment_meta SET schema_format = 2 WHERE singleton = 1;"
					   "PRAGMA user_version = 2;"));

		auto migrated = queue_enrollment_database::open(path, { });
		expect(opened_ok(migrated), "a canonical version-two queue migrates");
		const auto rules       = migrated->load_rules();
		const auto enrollments = migrated->load_all();
		const auto preferences = migrated->load_preferences();
		expect(
			rules.has_value() && *rules == std::vector{ rule }
				&& enrollments.has_value() && enrollments->size() == 1U
				&& enrollments->front().Ref == ref(7),
			"migration preserves rule authority and derived enrollment separately");
		expect(
			preferences
				== application::queue_presentation_preferences{ },
			"a version-two queue gains only default reviewer preferences");
	}

	// [[BPR-0659]]. A canonical version-three queue (no saved repositories)
	// migrates forward keeping its rules, enrollments and preferences, and
	// gains an empty saved-repository list.
	void test_a_legal_version_three_database_migrates_without_dropping_queue_state()
	{
		const scratch_directory directory;
		const auto              path = directory.file("version-three.db");
		const auto              rule = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store), "the current queue fixture opens");
			expect(store->store_rule(rule).has_value(), "the exact rule is stored");
			expect(
				store
					->store(application::enrollment_record{
						.Ref = ref(7), .Membership = watching("src/parser") })
					.has_value(),
				"the exact enrollment is stored");
		}
		execute_raw_sql(
			path,
			downgraded(downgrade_preferences_to_version_three,
					   "DROP TABLE saved_repository;"
					   "DROP TABLE saved_server;"
					   "UPDATE enrollment_meta SET schema_format = 3 WHERE singleton = 1;"
					   "PRAGMA user_version = 3;"));

		auto migrated = queue_enrollment_database::open(path, { });
		expect(opened_ok(migrated), "a canonical version-three queue migrates");
		const auto rules       = migrated->load_rules();
		const auto enrollments = migrated->load_all();
		const auto saved       = migrated->load_saved_repositories();
		expect(
			rules.has_value() && *rules == std::vector{ rule }
				&& enrollments.has_value() && enrollments->size() == 1U
				&& enrollments->front().Ref == ref(7),
			"migration preserves rule authority and derived enrollment");
		expect(saved.has_value() && saved->empty(),
			   "a version-three queue gains an empty saved-repository list");
	}

	// [[BPR-0660]]. A version-four queue gains an empty saved-server list and
	// keeps its saved repositories; saved servers are one row per hostname,
	// remembering again replaces the account, forgetting removes the row,
	// and the application port carries them.
	void test_a_legal_version_four_database_migrates_and_saved_servers_round_trip()
	{
		const scratch_directory directory;
		const auto              path = directory.file("version-four.db");
		const auto              kept = application::saved_repository{
			.Host = "github.com", .Account = "octo", .Repository = "octo/app"
		};
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store) && store->store_saved_repository(kept).has_value(),
				   "the current queue fixture opens and keeps a saved repository");
		}
		execute_raw_sql(
			path,
			downgraded(downgrade_preferences_to_version_three,
					   "DROP TABLE saved_server;"
					   "UPDATE enrollment_meta SET schema_format = 4 WHERE singleton = 1;"
					   "PRAGMA user_version = 4;"));
		auto migrated = queue_enrollment_database::open(path, { });
		expect(opened_ok(migrated), "a canonical version-four queue migrates");
		const auto saved   = migrated->load_saved_repositories();
		const auto servers = migrated->load_saved_servers();
		expect(saved.has_value() && *saved == std::vector{ kept } && servers.has_value() && servers->empty(),
			   "migration keeps the saved repositories and gains an empty saved-server list");
		const auto ghe = application::saved_server{ .Hostname = "ghe.example.com", .Account = "octo" };
		const auto lab = application::saved_server{ .Hostname = "code.example.org", .Account = "" };
		expect(migrated->store_saved_server(ghe).has_value() && migrated->store_saved_server(lab).has_value()
				   && migrated->store_saved_server(application::saved_server{ .Hostname = "ghe.example.com", .Account = "octo-two" }).has_value(),
			   "remembering stores, and remembering again is accepted");
		const auto listed = migrated->load_saved_servers();
		// Named: GCC 15.2 misreads a conditionally built list temporary as dangling (BPR-0816).
		const auto latestByHostname = std::vector{ lab, application::saved_server{ .Hostname = "ghe.example.com", .Account = "octo-two" } };
		expect(listed.has_value() && *listed == latestByHostname,
			   "the list is one row per hostname ordered by hostname, the account the latest remembered");
		expect(migrated->erase_saved_server(ghe).has_value(), "forgetting a server is accepted");
		const auto after = migrated->load_saved_servers();
		expect(after.has_value() && *after == std::vector{ lab }, "forgetting removes exactly the named hostname");
		expect(!migrated->store_saved_server(application::saved_server{ .Hostname = "https://ghe.example.com" }).has_value()
				   && !migrated->store_saved_server(application::saved_server{ .Hostname = "" }).has_value(),
			   "a hostname with a scheme, or none, is refused");
		expect(directory.make_private(), "the scratch root becomes owner-private");
		auto       port   = bpr::adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = directory.path() });
		const auto before = port->load({ });
		expect(before.has_value() && before->SavedServers.empty(), "the application port loads the saved servers with the rest");
		expect(port->apply_saved_server_change({ .Action = application::saved_server_action::add, .Server = ghe }, { }).has_value()
				   && port->load({ })->SavedServers == std::vector{ ghe },
			   "the port remembers a server");
		expect(port->apply_saved_server_change({ .Action = application::saved_server_action::remove, .Server = ghe }, { }).has_value()
				   && port->load({ })->SavedServers.empty(),
			   "the port forgets it");
	}

	struct addition_refresh final : application::queue_refresh_effect_port
	{
		auto refresh(const application::queue_refresh_request& aRequest, std::stop_token) -> application::queue_refresh_completion override
		{
			++Requests;
			return { .Session = aRequest.Session, .Ref = aRequest.Ref, .Status = application::queue_refresh_status::offline };
		}
		std::size_t Requests = 0U;
	};
	struct addition_projection final : application::queue_landing_projection_port
	{
		void                                  publish(const application::queue_landing_projection& aProjection) override { Latest = aProjection; }
		application::queue_landing_projection Latest;
	};

	void test_repository_addition_host_survives_sqlite_reopen()
	{
		const scratch_directory directory;
		expect(directory.make_private(), "addition SQLite course owns a private disposable root");
		const auto source  = core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ 1U }, core::bpr_window_generation{ 1U } };
		const auto session = application::queue_session_id{ "github.com", "reviewer", 4U };
		const auto saved   = application::saved_repository{ "github.com", "reviewer", "octo/app" };
		auto       refresh = addition_refresh{ };
		for (const auto reopen : { false, true }) {
			auto store      = bpr::adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = directory.path() });
			auto effects    = application::manual_executor{ };
			auto decisions  = application::manual_executor{ };
			auto projection = addition_projection{ };
			auto host       = application::review_queue_application_host{ *store, refresh, effects, decisions, projection, session };
			host.restore();
			effects.run_next();
			decisions.run_next();
			expect(host.saved_repositories().size() == (reopen ? 1U : 0U) && !projection.Latest.Addition, "reopen loads only saved records, never transient addition confirmation");
			host.apply(application::repository_addition_intent{ .Source = source, .Gesture = 1U, .Destination = session, .Text = "https://github.com/octo/app/" });
			if (!reopen) {
				expect(projection.Latest.Addition->Status == core::repository_addition_status::saving && host.saved_repositories().empty(), "SQLite save is visible only after actual durable terminal");
				effects.run_next();
				decisions.run_next();
			}
			expect(projection.Latest.SavedRepositories == std::vector{ saved } && projection.Latest.Addition->Status == (reopen ? core::repository_addition_status::already_added : core::repository_addition_status::saved) && host.statistics().PersistenceRequests == (reopen ? 0U : 1U), "same host API stores once, reopens exact identity and reports duplicate without another write");
			host.shutdown();
			expect(host.is_quiescent() && effects.pending() == 0U && decisions.pending() == 0U, "SQLite addition owner drains before store destruction");
		}
		expect(refresh.Requests == 0U, "SQLite addition requires no PR/network existence check");
	}

	// [[BPR-0659]]. Saved repositories: one row per host, account and name,
	// a second save is no second row, a forget removes exactly one, and the
	// application port carries both.
	void test_saved_repositories_round_trip_by_account()
	{
		const scratch_directory directory;
		const auto              path = directory.file("saved.db");
		const auto              app  = application::saved_repository{
			.Host = "github.com", .Account = "octo", .Repository = "octo/app"
		};
		const auto tools = application::saved_repository{
			.Host = "github.com", .Account = "octo", .Repository = "octo/tools"
		};
		const auto elsewhere = application::saved_repository{
			.Host = "ghe.example.com", .Account = "octo", .Repository = "octo/app"
		};
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store), "the queue opens");
			expect(store->store_saved_repository(tools).has_value()
					   && store->store_saved_repository(app).has_value()
					   && store->store_saved_repository(app).has_value()
					   && store->store_saved_repository(elsewhere).has_value(),
				   "saving stores, and saving again is accepted");
			const auto loaded = store->load_saved_repositories();
			expect(loaded.has_value() && *loaded == std::vector{ elsewhere, app, tools },
				   "the list is every saved repository, ordered by host, account and name, once each");
			expect(store->erase_saved_repository(app).has_value(),
				   "forgetting a saved repository is accepted");
			const auto after = store->load_saved_repositories();
			expect(after.has_value() && *after == std::vector{ elsewhere, tools },
				   "forgetting removes exactly the named row");
			expect(!store->store_saved_repository(application::saved_repository{
													  .Host = "github.com", .Account = "octo", .Repository = "no-slash" })
						.has_value(),
				   "a repository that is not owner/name is refused");
		}
		// The application port opens the queue under its own owner-private
		// state directory beneath the root.
		expect(directory.make_private(), "the scratch root becomes owner-private");
		auto       port   = bpr::adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = directory.path() });
		const auto before = port->load({ });
		expect(before.has_value() && before->SavedRepositories.empty(),
			   "the application port loads the saved repositories with the rest of the queue state");
		expect(port->apply_saved_repository_change(
					   application::saved_repository_intent{
						   .Action = application::saved_repository_action::add, .Repository = app },
					   { })
					   .has_value()
				   && port->apply_saved_repository_change(
							  application::saved_repository_intent{
								  .Action = application::saved_repository_action::add, .Repository = tools },
							  { })
						  .has_value(),
			   "the port saves repositories");
		const auto saved2 = port->load({ });
		expect(saved2.has_value() && saved2->SavedRepositories == std::vector{ app, tools },
			   "the port's saves are what the next load reads");
		expect(port->apply_saved_repository_change(
					   application::saved_repository_intent{
						   .Action = application::saved_repository_action::remove, .Repository = tools },
					   { })
				   .has_value(),
			   "the port forgets a repository");
		const auto after = port->load({ });
		expect(after.has_value() && after->SavedRepositories == std::vector{ app },
			   "the port's forget is what the next load reads");
		expect(!port->apply_saved_repository_change(
						application::saved_repository_intent{
							.Action     = application::saved_repository_action::add,
							.Repository = application::saved_repository{ .Host = "github.com", .Account = "octo", .Repository = "no-slash" } },
						{ })
					.has_value(),
			   "the port refuses a repository that is not owner/name before the database");
	}

	void test_stamped_current_schema_is_validated_canonically()
	{
		const scratch_directory directory;
		constexpr auto          tampering = std::array<std::string_view, 8>{
			"ALTER TABLE enrollment_meta ADD COLUMN unexpected INTEGER NOT NULL DEFAULT 0",
			"ALTER TABLE enrollment ADD COLUMN unexpected INTEGER",
			"DROP TABLE observer_rule",
			"DROP TABLE observer_rule;"
			"CREATE TABLE observer_rule("
			"rule_kind INTEGER NOT NULL CHECK(rule_kind BETWEEN 0 AND 2),"
			"rule_pattern TEXT NOT NULL,"
			"rule_excluding INTEGER NOT NULL CHECK(rule_excluding IN (0, 1)),"
			"PRIMARY KEY(rule_kind, rule_pattern, rule_excluding)) STRICT",
			"ALTER TABLE presentation_preferences ADD COLUMN unexpected INTEGER",
			"DROP TABLE presentation_preferences",
			"DROP TABLE saved_repository",
			"ALTER TABLE saved_repository ADD COLUMN unexpected INTEGER",
		};

		for (std::size_t index = 0; index < tampering.size(); ++index) {
			const auto path
				= directory.file("altered-" + std::to_string(index) + ".db");
			{
				auto store = queue_enrollment_database::open(path, { });
				expect(opened_ok(store), "a canonical current queue is created");
			}
			execute_raw_sql(path, tampering[index]);
			const auto refused = queue_enrollment_database::open(path, { });
			expect(
				!refused
					&& refused.error()
						== catalog_database_failure::schema_mismatch,
				"a stamped current file with an altered or missing canonical table is refused");
		}
	}

	void test_version_five_retains_filters_and_adds_notification_defaults()
	{
		const scratch_directory directory;
		const auto              path        = directory.file("version-five.db");
		auto                    preferences = application::queue_presentation_preferences{ };
		preferences.Repository              = "octo/app";
		preferences.Sort                    = application::queue_sort_order::repository;
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store) && store->store_preferences(preferences).has_value(), "version-five fixture starts with nondefault preferences");
		}
		execute_raw_sql(path, downgraded(downgrade_preferences_to_version_three, "UPDATE enrollment_meta SET schema_format = 5 WHERE singleton = 1; PRAGMA user_version = 5;"));
		auto migrated = queue_enrollment_database::open(path, { });
		expect(opened_ok(migrated) && migrated->load_preferences() == preferences, "version-five filters survive with enabled, unpaused notification defaults");
	}

	// [[BPR-0989]]. A version-six queue migrates to seven keeping every
	// enrollment, membership, departure and preference; the new reasons start
	// false. Version seven round-trips discovery, follow, review, the
	// `undiscovered` departure and the `detail_unknown` bucket filter.
	void test_version_six_gains_discovery_reasons_and_they_round_trip()
	{
		const scratch_directory directory;
		const auto              path        = directory.file("version-six.db");
		auto                    preferences = application::queue_presentation_preferences{ };
		preferences.Repository              = "octo/app";
		preferences.Notifications           = { .Enabled = false, .Paused = true };
		const auto kept                     = application::enrollment_record{ .Ref = ref(7), .Membership = watching("src/parser") };
		const auto departed                 = application::enrollment_record{ .Ref = ref(8), .Membership = { .Role = core::github_role::reviewer }, .Departed = application::enrollment_departure::closed };
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store) && store->store(kept).has_value() && store->store(departed).has_value() && store->store_preferences(preferences).has_value(),
				   "the version-six fixture starts with enrollments and nondefault preferences");
		}
		execute_raw_sql(path, downgraded(downgrade_preferences_to_version_six, "UPDATE enrollment_meta SET schema_format = 6 WHERE singleton = 1; PRAGMA user_version = 6;"));
		{
			auto migrated = queue_enrollment_database::open(path, { });
			expect(opened_ok(migrated), "a canonical version-six queue migrates");
			expect(migrated->load_all() == std::vector{ kept, departed } && migrated->load_preferences() == preferences,
				   "every enrollment, departure and preference survives, with no discovery reason invented");
			expect(scalar_integer(migrated->handle(), "PRAGMA user_version") == queue_enrollment_schema_version, "the file is stamped version seven");
		}
		auto discovered = application::enrollment_record{
			.Ref        = ref(9),
			.Membership = { .Authored = true, .Requested = true, .Followed = true, .Reviewed = true },
		};
		auto gone = application::enrollment_record{
			.Ref        = ref(10),
			.Membership = { .Reviewed = false },
			.Departed   = application::enrollment_departure::undiscovered,
		};
		auto unknown   = preferences;
		unknown.Bucket = core::attention_bucket::detail_unknown;
		{
			auto       store = queue_enrollment_database::open(path, { });
			const auto batch = std::array{ discovered, gone };
			expect(opened_ok(store) && store->store_all(batch, { }).has_value() && store->store_preferences(unknown).has_value(),
				   "discovery reasons, a follow, a review, the undiscovered departure and the unknown bucket are stored");
		}
		auto reopened = queue_enrollment_database::open(path, { });
		expect(opened_ok(reopened) && reopened->load_all() == std::vector{ kept, departed, discovered, gone } && reopened->load_preferences() == unknown,
			   "they round-trip exactly");
	}

	void test_presentation_preferences_round_trip_without_a_derived_filter_result()
	{
		const scratch_directory directory;
		const auto              path        = directory.file("preferences.db");
		const auto              preferences = application::queue_presentation_preferences{
			.Role           = core::github_role::author,
			.ObserverReason = core::observer_rule_kind::path,
			.Bucket         = core::attention_bucket::new_comments_or_author_replies,
			.Repository     = "octo/app",
			.Quiet          = application::queue_quiet_filter::attention,
			.Sort           = application::queue_sort_order::pull_request_number,
			.Notifications  = { .Enabled = false, .Paused = true },
		};
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store), "the preference store opens");
			expect(
				store->store_preferences(preferences).has_value(),
				"the exact reviewer preference is stored");
		}
		auto reopened = queue_enrollment_database::open(path, { });
		expect(opened_ok(reopened), "the preference store reopens");
		expect(
			reopened->load_preferences() == preferences,
			"every filter and explicit alternate sort survives restart");
		expect(
			scalar_integer(reopened->handle(), "SELECT count(*) FROM enrollment")
				== 0,
			"persisting presentation preferences writes no derived queue membership");
		execute_raw_sql(
			path,
			"UPDATE presentation_preferences "
			"SET repository_filter = char(0) || 'octo/app' WHERE singleton = 1");
		expect(
			!reopened->load_preferences().has_value(),
			"an embedded NUL cannot silently widen the persisted repository filter");
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

	void test_reverse_sorted_overlapping_rule_precedence_survives_reopen()
	{
		const scratch_directory directory;
		const auto              path     = directory.file("queue.db");
		const auto              specific = core::observer_rule{
			.Kind    = core::observer_rule_kind::path,
			.Pattern = "src/parser/generated*",
		};
		const auto broad = core::observer_rule{
			.Kind    = core::observer_rule_kind::path,
			.Pattern = "src/parser*",
		};
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store), "the store opens");
			expect(
				store->store_rule(specific).has_value(),
				"the lexically later specific rule is stored first");
			expect(
				store->store_rule(broad).has_value(),
				"the overlapping broad rule is stored second");
			expect(
				store->store_rule(specific).has_value(),
				"storing the same rule is idempotent and keeps its precedence");
		}

		auto reopened = queue_enrollment_database::open(path, { });
		expect(opened_ok(reopened), "the durable rule list reopens");
		const auto rules = reopened->load_rules();
		expect(rules.has_value() && rules->size() == 2U, "two exact rules reopen");
		expect(
			*rules == std::vector{ specific, broad },
			"the reverse-sorted user precedence survives restart rather than becoming lexical order");
		const auto membership = core::enrollment_for(
			core::pull_request_observation{
				.Ref          = ref(7),
				.ChangedPaths = { "src/parser/generated/output.cpp" },
			},
			*rules);
		expect(
			membership.MatchedRule && membership.Rule == specific,
			"the restored first overlapping rule remains the exact recorded reason");
		expect(
			reopened->erase_rule(specific).has_value(),
			"one exact rule is erased");
		const auto remaining = reopened->load_rules();
		expect(remaining.has_value() && *remaining == std::vector{ broad },
			   "erasing one rule preserves the other");
	}

	void test_rule_and_enrollment_reconciliation_is_atomic_at_every_row()
	{
		const auto rule = core::observer_rule{
			.Kind = core::observer_rule_kind::path, .Pattern = "src/parser"
		};
		const auto intent = application::queue_rule_intent{
			.Action = application::queue_rule_action::add,
			.Rule   = rule,
		};
		const auto records = std::array{
			application::enrollment_record{ .Ref = ref(7), .Membership = watching("src/parser") },
			application::enrollment_record{ .Ref = ref(8), .Membership = watching("src/parser") },
			application::enrollment_record{ .Ref = ref(9), .Membership = watching("src/parser") },
		};
		for (const auto failedNumber : { 7, 8, 9 }) {
			const scratch_directory directory;
			const auto              path = directory.file("queue.db");
			{
				auto store = queue_enrollment_database::open(path, { });
				expect(opened_ok(store), "the atomic rule store opens");
				const auto trigger
					= "CREATE TEMP TRIGGER fail_enrollment BEFORE INSERT ON main.enrollment "
					  "WHEN NEW.pull_request = "
					+ std::to_string(failedNumber)
					+ " BEGIN SELECT RAISE(ABORT, 'forced'); END";
				expect(
					::sqlite3_exec(
						store->handle(), trigger.c_str(), nullptr, nullptr, nullptr)
						== SQLITE_OK,
					"the selected reconciliation row is fault injected");
				expect(
					!store->apply_rule_change(intent, records, { }).has_value(),
					"the atomic rule mutation reports the injected row failure");
			}

			auto reopened = queue_enrollment_database::open(path, { });
			expect(opened_ok(reopened), "the failed atomic store reopens");
			const auto rules       = reopened->load_rules();
			const auto enrollments = reopened->load_all();
			expect(
				rules.has_value() && rules->empty() && enrollments.has_value()
					&& enrollments->empty(),
				"first, middle, and final row failures roll back the rule and every enrollment");
		}
	}
	void test_collection_limits_on_load_and_write()
	{
		auto scratch = scratch_directory{ };
		expect(scratch.make_private(), "queue limit fixture is private");
		auto port = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = scratch.path() });
		expect(port->load({ }).has_value(), "queue port creates its state directory");
		auto database = queue_enrollment_database::open(scratch.path() / "state" / "v1" / "review-queue.sqlite3", { });
		expect(opened_ok(database), "queue limit database opens");
		const auto seed = [&](const std::string& aSql) {
			expect(::sqlite3_exec(database->handle(), aSql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "bounded hostile fixture is seeded");
		};
		seed("WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < " + std::to_string(application::maximum_queue_rules - 1) + ") INSERT INTO observer_rule SELECT x, 0, 'repo' || x, 0 FROM n");
		seed("WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < " + std::to_string(application::maximum_saved_repositories - 1) + ") INSERT INTO saved_repository SELECT 'github.com', 'reviewer', 'octo/repo' || x FROM n");
		seed("WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < " + std::to_string(application::maximum_saved_servers - 1) + ") INSERT INTO saved_server SELECT 'server' || x || '.example', 'reviewer' FROM n");
		seed("WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < " + std::to_string(application::maximum_queue_enrollments - 1) + ") INSERT INTO enrollment SELECT 'github.com', 'reviewer', 'octo/app', x, 1, 0, 0, '', 0, 0, 0, 0, 0, 0,NULL,'0','','0' FROM n");
		expect(port->apply_rule_change({ .Rule = { .Kind = core::observer_rule_kind::repository, .Pattern = "last-rule" } }, { }, { }).has_value(), "last rule slot is writable");
		expect(port->apply_saved_repository_change({ .Repository = { "github.com", "reviewer", "octo/last" } }, { }).has_value(), "last repository slot is writable");
		expect(port->apply_saved_server_change({ .Server = { "last.example", "reviewer" } }, { }).has_value(), "last server slot is writable");
		expect(port->store_enrollment({ .Ref = ref(static_cast<std::int64_t>(application::maximum_queue_enrollments)), .Membership = { .Role = core::github_role::reviewer } }, { }).has_value(), "last enrollment slot is writable");
		const auto accepted = port->load({ });
		expect(accepted.has_value() && accepted->Rules.size() == application::maximum_queue_rules && accepted->SavedRepositories.size() == application::maximum_saved_repositories && accepted->SavedServers.size() == application::maximum_saved_servers && accepted->Enrollments.size() == application::maximum_queue_enrollments, "maximum durable collections load in full");
		const auto rule       = core::observer_rule{ .Kind = core::observer_rule_kind::repository, .Pattern = "overflow" };
		const auto repository = application::saved_repository{ "github.com", "reviewer", "octo/overflow" };
		const auto server     = application::saved_server{ "overflow.example", "reviewer" };
		const auto record     = application::enrollment_record{ .Ref = ref(20000), .Membership = { .Role = core::github_role::reviewer } };
		const auto refused    = [](const auto& aResult) { return !aResult && aResult.error() == application::queue_store_failure::collection_limit; };
		expect(refused(port->apply_rule_change({ .Rule = rule }, { }, { })) && refused(port->apply_saved_repository_change({ .Repository = repository }, { })) && refused(port->apply_saved_server_change({ .Server = server }, { })) && refused(port->store_enrollment(record, { })), "every new row at capacity has a typed refusal");
		expect(port->apply_rule_change({ .Rule = accepted->Rules.front() }, { }, { }).has_value() && port->apply_saved_repository_change({ .Repository = accepted->SavedRepositories.front() }, { }).has_value() && port->apply_saved_server_change({ .Server = accepted->SavedServers.front() }, { }).has_value() && port->store_enrollment(accepted->Enrollments.front(), { }).has_value(), "existing identities remain writable at capacity");
		const auto after = port->load({ });
		expect(after.has_value() && after->Rules == accepted->Rules && after->SavedRepositories == accepted->SavedRepositories && after->SavedServers == accepted->SavedServers && after->Enrollments == accepted->Enrollments && after->Preferences == accepted->Preferences, "refused additions preserve every accepted domain");
		expect(database->erase_rule(accepted->Rules.front()).has_value(), "one rule slot is available for atomic enrollment refusal");
		const auto changed = std::array{ record };
		expect(refused(port->apply_rule_change({ .Rule = rule }, changed, { })), "enrollment capacity rejects an otherwise legal rule transaction");
		const auto rolledBack = database->load_rules();
		expect(rolledBack.has_value() && rolledBack->size() == application::maximum_queue_rules - 1 && std::ranges::find(*rolledBack, rule) == rolledBack->end(), "enrollment overflow rolls the rule back");
		expect(database->store_rule(accepted->Rules.front()).has_value(), "rule capacity is restored");
		const auto oversized = std::vector<application::enrollment_record>(application::maximum_queue_enrollments + 1, accepted->Enrollments.front());
		expect(refused(port->apply_rule_change({ .Rule = rule }, oversized, { })), "oversized enrollment edit batch is refused even for repeated identities");
		const auto hostile = [&](const std::string& aInsert, const std::string& aDelete) {
			seed(aInsert);
			expect(refused(port->load({ })), "hostile limit plus one load is refused without truncation");
			seed(aDelete);
			expect(port->load({ }).has_value(), "removing overflow makes maximum state usable again");
		};
		hostile("INSERT INTO observer_rule VALUES(20000,0,'overflow',0)", "DELETE FROM observer_rule WHERE ordinal=20000");
		hostile("INSERT INTO saved_repository VALUES('github.com','reviewer','octo/overflow')", "DELETE FROM saved_repository WHERE repository='octo/overflow'");
		hostile("INSERT INTO saved_server VALUES('overflow.example','reviewer')", "DELETE FROM saved_server WHERE hostname='overflow.example'");
		hostile("INSERT INTO enrollment VALUES('github.com','reviewer','octo/app',20000,1,0,0,'',0,0,0,0,0,0,NULL,'0','','0')", "DELETE FROM enrollment WHERE pull_request=20000");
	}

	void test_waiting_metadata_round_trip()
	{
		auto scratch = scratch_directory{ };
		expect(scratch.make_private(), "waiting store root is private");
		auto port = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = scratch.path() });
		expect(port->load({ }).has_value(), "waiting store opens");
		auto record                             = application::enrollment_record{ };
		record.Ref                              = ref(17);
		record.Membership.Role                  = core::github_role::author;
		record.Membership.Followed              = true;
		record.LastFacts.Title                  = "Waiting title";
		record.LastFacts.Waiting.Requests       = 3;
		record.LastFacts.Waiting.CodeOwners     = { "@org/team-a", "@org/team-b", "@org/team-c" };
		record.LastFacts.Waiting.NamesKnown     = true;
		record.LastFacts.Waiting.Teams          = 3;
		record.LastFacts.Waiting.RequiredReview = true;
		record.LastFacts.Waiting.ChecksKnown    = true;
		record.LastFacts.Waiting.FailedChecks   = { "Branch Freshness" };
		record.FactsAccount                     = core::github_account_id{ 1234 };
		expect(port->store_enrollment(record, { }).has_value(), "bounded waiting facts commit with enrollment");
		port.reset();
		port              = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = scratch.path() });
		const auto loaded = port->load({ });
		expect(loaded && loaded->Enrollments.size() == 1 && loaded->Enrollments.front().FactsAccount == record.FactsAccount && loaded->Enrollments.front().LastFacts.Waiting.CodeOwners == record.LastFacts.Waiting.CodeOwners && loaded->Enrollments.front().LastFacts.Waiting.RequestsStale, "restart preserves names and exact account as last-known");
	}

#if !defined(_WIN32)
	void test_partial_failed_check_survives_separate_process()
	{
		auto scratch = scratch_directory{ };
		expect(scratch.make_private(), "partial failure store root is private");
		const auto child = ::fork();
		expect(child >= 0, "partial failure writer process starts");
		if (child == 0) {
			bpr::tests::end_forked_child([&] {
				auto port = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = scratch.path() });
				expect(port->load({ }).has_value(), "child public store opens");
				auto record                           = application::enrollment_record{ };
				record.Ref                            = ref(17);
				record.Membership.Followed            = true;
				record.FactsAccount                   = core::github_account_id{ 1234 };
				record.LastFacts.Waiting.FailedChecks = { "Branch Freshness" };
				record.LastFacts.Waiting.ChecksStale  = true;
				expect(port->store_enrollment(record, { }).has_value(), "child commits a partial check failure through the public store port");
				port.reset();
				std::_Exit(EXIT_SUCCESS);
			});
		}
		auto status = 0;
		expect(::waitpid(child, &status, 0) == child && WIFEXITED(status) != 0 && WEXITSTATUS(status) == EXIT_SUCCESS, "partial failure writer finishes and is reaped");
		auto       port   = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = scratch.path() });
		const auto loaded = port->load({ });
		expect(loaded && loaded->Enrollments.size() == 1, "fresh process loads the public-port durable record");
		const auto& record  = loaded->Enrollments.front();
		const auto& waiting = record.LastFacts.Waiting;
		expect(record.FactsAccount == core::github_account_id{ 1234 } && waiting.FailedChecks == std::vector<std::string>{ "Branch Freshness" } && !waiting.ChecksKnown && waiting.ChecksStale && !waiting.Requests && !waiting.RequiredReview && !waiting.MergeBlocked && !waiting.RequestsStale && !waiting.AssessmentStale, "SQLite restart retains partial failure and only its last-known flag without other observed facts");
	}
#endif

	void test_optional_draft_round_trip_and_upgrade()
	{
		const scratch_directory directory;
		const auto              path   = directory.file("queue.db");
		auto                    store  = queue_enrollment_database::open(path, { });
		auto                    record = application::enrollment_record{ .Ref = ref(17), .Membership = { .Followed = true }, .Draft = true, .DraftAccount = core::github_account_id{ 1001U } };
		expect(opened_ok(store) && store->store(record).has_value() && store->load_all() == std::vector{ record }, "draft and accepted numeric account persist");
		record.Draft = false;
		expect(store->store(record).has_value() && store->load_all() == std::vector{ record }, "ready remains observed false");
		store = std::unexpected(failure::unavailable);
		execute_raw_sql(path, "DROP TABLE home_ignore; ALTER TABLE enrollment DROP COLUMN home_facts; ALTER TABLE enrollment DROP COLUMN facts_account; ALTER TABLE enrollment DROP COLUMN draft; ALTER TABLE enrollment DROP COLUMN draft_account; UPDATE enrollment_meta SET schema_format=7; PRAGMA user_version=7;");
		record.Draft.reset();
		record.DraftAccount = { };
		auto migrated       = queue_enrollment_database::open(path, { });
		expect(opened_ok(migrated) && migrated->load_all() == std::vector{ record }, "version-seven upgrade retains enrollment without inventing draft or account observations");
	}

	// [[BPR-1407]]. Version ten keeps version nine's tables; a nine file's
	// facts, written in the first encoding, read without change facts, and the
	// change facts then round-trip.
	void test_version_nine_facts_upgrade_and_change_facts_round_trip()
	{
		const scratch_directory directory;
		const auto              path   = directory.file("queue.db");
		auto                    record = application::enrollment_record{ .Ref = ref(17), .Membership = { .Followed = true }, .FactsAccount = core::github_account_id{ 1001U } };
		record.LastFacts.Title         = "Version nine title";
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store) && store->store(record).has_value(), "the version-nine fixture starts with a titled enrollment");
		}
		execute_raw_sql(path, "DROP TABLE home_ignore; UPDATE enrollment SET home_facts = '1;' || substr(home_facts, 3, length(home_facts) - 10); UPDATE enrollment_meta SET schema_format = 9 WHERE singleton = 1; PRAGMA user_version = 9;");
		auto migrated = queue_enrollment_database::open(path, { });
		expect(opened_ok(migrated) && migrated->load_all() == std::vector{ record }, "a version-nine file's first-encoding facts survive the upgrade with nothing invented");
		expect(scalar_integer(migrated->handle(), "PRAGMA user_version") == queue_enrollment_schema_version, "the file is stamped current");
		record.LastFacts.Change = { .OpenedAt = 1'791'285'300, .Additions = 236U, .Deletions = 0U, .Comments = 3U };
		expect(migrated->store(record).has_value() && migrated->load_all() == std::vector{ record }, "age, size and comment count round-trip, a zero kept as zero");
	}

	// [[BPR-1683]]. Ignores are kept by whole identity, survive a reopen,
	// are pruned by the same write path, and a version-ten file gains the table.
	void test_home_ignores_round_trip_and_upgrade()
	{
		const scratch_directory directory;
		const auto              path  = directory.file("queue.db");
		auto                    other = ref(17);
		other.Account                 = "second-account";
		{
			auto store = queue_enrollment_database::open(path, { });
			expect(opened_ok(store), "the store opens");
			expect(store->load_home_ignores() == std::vector<core::pull_request_ref>{ }, "a new store ignores nothing");
			const auto ignored = std::vector<core::home_ignore_delta>{
				{ .Ref = ref(18), .Change = core::home_ignore_change::ignored },
				{ .Ref = other, .Change = core::home_ignore_change::ignored },
				{ .Ref = ref(17), .Change = core::home_ignore_change::ignored },
				{ .Ref = ref(17), .Change = core::home_ignore_change::ignored },
			};
			expect(store->apply_home_ignores(ignored, { }).has_value(), "ignores are written, a repeat included");
			const auto absent = std::vector<core::home_ignore_delta>{ { .Ref = ref(99), .Change = core::home_ignore_change::unignored } };
			expect(store->apply_home_ignores(absent, { }).has_value(), "unignoring what is not there succeeds");
			const auto invalid = std::vector<core::home_ignore_delta>{
				{ .Ref = ref(19), .Change = core::home_ignore_change::ignored },
				{ .Ref = ref(0), .Change = core::home_ignore_change::ignored },
			};
			expect(!store->apply_home_ignores(invalid, { }).has_value(), "an unusable reference refuses the whole write");
		}
		{
			auto reopened = queue_enrollment_database::open(path, { });
			expect(opened_ok(reopened), "the store reopens");
			expect(reopened->load_home_ignores() == std::vector{ ref(17), ref(18), other },
				   "every ignore survives the reopen, in identity order, the other account's apart, and the refused write left nothing");
			const auto pruned = std::vector<core::home_ignore_delta>{
				{ .Ref = ref(17), .Change = core::home_ignore_change::pruned_closed },
				{ .Ref = ref(18), .Change = core::home_ignore_change::unignored },
			};
			expect(reopened->apply_home_ignores(pruned, { }).has_value() && reopened->load_home_ignores() == std::vector{ other },
				   "a prune and an unignore remove exactly their own pull requests");
		}

		const scratch_directory older;
		const auto              olderPath = older.file("queue.db");
		{
			auto store = queue_enrollment_database::open(olderPath, { });
			expect(opened_ok(store) && store->store(application::enrollment_record{ .Ref = ref(17), .Membership = { .Followed = true } }).has_value(),
				   "the version-ten fixture starts with an enrollment");
		}
		execute_raw_sql(olderPath, "DROP TABLE home_ignore; UPDATE enrollment_meta SET schema_format = 10 WHERE singleton = 1; PRAGMA user_version = 10;");
		auto migrated = queue_enrollment_database::open(olderPath, { });
		expect(opened_ok(migrated) && migrated->load_all().has_value() && migrated->load_all()->size() == 1U, "a version-ten file opens with its enrollment");
		expect(scalar_integer(migrated->handle(), "PRAGMA user_version") == queue_enrollment_schema_version, "and is stamped current");
		expect(migrated->load_home_ignores() == std::vector<core::pull_request_ref>{ }, "with nothing ignored");
	}

} // namespace

auto main() -> int
try {
	test_repository_addition_host_survives_sqlite_reopen();
	test_collection_limits_on_load_and_write();
#if !defined(_WIN32)
	test_waiting_metadata_round_trip();
	test_partial_failed_check_survives_separate_process();
	test_optional_draft_round_trip_and_upgrade();
	test_enrollment_survives_a_separate_process();
#endif
	test_version_nine_facts_upgrade_and_change_facts_round_trip();
	test_home_ignores_round_trip_and_upgrade();
	test_a_newer_database_fails_closed();
	test_a_foreign_version_one_database_is_not_migrated();
	test_a_legal_version_one_database_migrates_to_current();
	test_a_legal_version_two_database_migrates_without_rewriting_queue_state();
	test_a_legal_version_three_database_migrates_without_dropping_queue_state();
	test_saved_repositories_round_trip_by_account();
	test_a_legal_version_four_database_migrates_and_saved_servers_round_trip();
	test_stamped_current_schema_is_validated_canonically();
	test_version_five_retains_filters_and_adds_notification_defaults();
	test_version_six_gains_discovery_reasons_and_they_round_trip();
	test_presentation_preferences_round_trip_without_a_derived_filter_result();
	test_the_schema_holds_no_repository_content();
	test_deleting_the_queue_leaves_the_ledger_intact();
	test_storing_the_same_item_twice_replaces_rather_than_duplicates();
	test_reverse_sorted_overlapping_rule_precedence_survives_reopen();
	test_rule_and_enrollment_reconciliation_is_atomic_at_every_row();

	std::cout << "queue-enrollment-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
