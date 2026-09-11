#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
namespace {
	using namespace bpr;
	using namespace bpr::application;
	void expect(const bool aCondition, const std::string_view aMessage)
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
			Root = std::filesystem::temp_directory_path() / ("bpr-0709-restoration-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directory(Root);
			std::filesystem::permissions(Root, std::filesystem::perms::owner_all);
			Root = std::filesystem::canonical(Root);
		}
		~scratch_directory() { std::filesystem::remove_all(Root); }
		scratch_directory(const scratch_directory&)                                     = delete;
		auto                  operator=(const scratch_directory&) -> scratch_directory& = delete;
		std::filesystem::path Root;
		[[nodiscard]] auto    database() const -> std::filesystem::path { return Root / "state/v1/window-restoration.sqlite3"; }
	};

	[[nodiscard]] auto raw_open(const std::filesystem::path& aPath) -> sqlite3*
	{
		sqlite3* raw = nullptr;
		expect(sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "fixture opens directly");
		return raw;
	}
	void execute(sqlite3* aDatabase, const std::string_view aSql)
	{
		const auto result = sqlite3_exec(aDatabase, std::string{ aSql }.c_str(), nullptr, nullptr, nullptr);
		if (result != SQLITE_OK) {
			std::cerr << sqlite3_errmsg(aDatabase) << ": " << aSql << '\n';
		}
		expect(result == SQLITE_OK, "fixture SQL executes");
	}
	[[nodiscard]] auto snapshot(sqlite3* aDatabase) -> std::vector<std::string>
	{
		std::vector<std::string> cells;
		const auto               collect = [](void* aContext, int aCount, char** aValues, char**) -> int {auto& values=*static_cast<std::vector<std::string>*>(aContext);for(int index=0;index<aCount;++index){values.emplace_back(aValues[index]?aValues[index]:"<NULL>");
}return 0; };
		expect(sqlite3_exec(aDatabase, "PRAGMA application_id; PRAGMA user_version; SELECT type,name,sql FROM sqlite_schema ORDER BY name; SELECT * FROM restoration_meta; SELECT * FROM restoration_records ORDER BY window_id", collect, &cells, nullptr) == SQLITE_OK, "snapshot captures schema and contents");
		return cells;
	}
	[[nodiscard]] auto file_bytes(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream stream{ aPath, std::ios::binary };
		return std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}
	struct interruption
	{
		static inline std::stop_source* Stop = nullptr;
		static inline std::string_view  Prefix;
		static auto                     trace(unsigned int, void*, void* aStatement, void*) -> int
		{
			const auto* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement));
			if (Stop != nullptr && sql != nullptr && std::string_view{ sql }.starts_with(Prefix)) {
				Stop->request_stop();
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT, trace, nullptr);
		}
	};

	[[nodiscard]] auto filled(std::uint64_t aRevision, std::size_t aCount) -> window_review_snapshot
	{
		auto result     = window_review_snapshot{ };
		result.Revision = aRevision;
		for (std::size_t index = 0; index < aCount; ++index) {
			result.Records.push_back({ core::bpr_window_id{ index + 1U }, { core::github_access_mode::authenticated, "github.com", { "octo", "repo" }, core::pull_request_number{ 17 } } });
		}
		return result;
	}
	void test_partition_bounds_and_conflict()
	{
		const auto left  = scratch_directory{ };
		const auto right = scratch_directory{ };
		auto       store = adapters::sqlite::make_window_review_restoration_store(left.Root);
		expect(!std::filesystem::exists(left.database()), "factory performs no filesystem effects");
		const auto initial = store->load({ });
		expect(initial && initial->Revision == 1 && initial->Records.empty(), "fresh snapshot is empty revision1");
		auto full                            = filled(2, 32);
		full.Records.back().Destination.Mode = core::github_access_mode::public_preview;
		expect(store->save(full, 1, { }) == full, "32 destinations persist");
		auto       rival    = adapters::sqlite::make_window_review_restoration_store(left.Root);
		const auto conflict = rival->save(filled(2, 1), 1, { });
		expect(!conflict && conflict.error().Failure == window_review_store_failure::conflict && conflict.error().Current == full, "stale CAS returns exact validated current");
		expect(!store->save(filled(3, 33), 2, { }) && store->load({ }) == full, "33rd destination refuses without loss");
		auto invalid = full;
		++invalid.Revision;
		invalid.Records.back().Window = invalid.Records.front().Window;
		expect(!store->save(invalid, 2, { }) && store->load({ }) == full, "duplicate identity refuses");
		auto other = adapters::sqlite::make_window_review_restoration_store(right.Root);
		expect(other->load({ }) == *initial, "another profile starts empty");
		auto separate                          = filled(2, 1);
		separate.Records[0].Destination.Number = core::pull_request_number{ 18 };
		expect(other->save(separate, 1, { }) == separate && store->load({ }) == full, "same stable ID remains profile private");
		auto wide = separate;
		++wide.Revision;
		wide.Records[0].Window = core::bpr_window_id{ std::numeric_limits<std::uint64_t>::max() };
		expect(other->save(wide, separate.Revision, { }) == wide && other->load({ }) == wide, "full-width window ID round trips canonical decimal");
		store.reset();
		store = adapters::sqlite::make_window_review_restoration_store(left.Root);
		expect(store->load({ }) == full, "reopen preserves full snapshot");
		auto pruned = full;
		++pruned.Revision;
		pruned.Records.erase(pruned.Records.begin());
		expect(store->save(pruned, 2, { }) == pruned, "full snapshot CAS prunes orphan only");
		auto stop = std::stop_source{ };
		stop.request_stop();
		expect(!store->save(filled(4, 0), 3, stop.get_token()) && store->load({ }) == pruned, "cancel leaves retained destinations intact");
	}
	template <typename TIdentity>
	auto identity(std::uint8_t aByte) -> TIdentity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.fill(aByte);
		return TIdentity{ bytes };
	}
	auto position() -> core::window_review_position
	{
		auto result                           = core::window_review_position{ };
		result.Comparison.Left                = { { identity<core::pull_request_authority_scope_id>(1), identity<core::epoch_catalog_incarnation_id>(2), core::comparison_anchor_kind::epoch_base, core::epoch_ordinal{ 0x0102030405060708 }, { } }, *core::parse_git_commit_oid(std::string(40, 'a')), core::anchor_availability::available };
		result.Comparison.Right               = result.Comparison.Left;
		result.Comparison.Right.Identity.Kind = core::comparison_anchor_kind::epoch_head;
		result.Comparison.Right.Commit        = *core::parse_git_commit_oid(std::string(40, 'b'));
		result.File                           = identity<core::file_identity>(3);
		const auto digest                     = identity<core::file_comparison_digest>(4);
		result.Selection                      = core::semantic_selection{ digest, core::semantic_selection_kind::group, { digest, core::diff_side::right, 0x01020304 }, identity<core::alignment_segment_id>(5), identity<core::change_group_id>(6) };
		expect(core::valid(result), "position fixture valid");
		return result;
	}
	void test_positions()
	{
		const auto scratch = scratch_directory{ };
		auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }).has_value(), "position store initializes");
		auto candidate                = filled(2, 4);
		candidate.Records[0].Position = position();
		candidate.Records[1].Position = position();
		candidate.Records[1].Position->Selection.reset();
		candidate.Records[2].Position = position();
		candidate.Records[2].Position->Selection.reset();
		candidate.Records[2].Position->File.reset();
		candidate.Records[1].Position->Comparison.Left.Identity.Kind   = core::comparison_anchor_kind::review_commit;
		candidate.Records[1].Position->Comparison.Left.Identity.Epoch  = core::epoch_ordinal{ };
		candidate.Records[1].Position->Comparison.Left.Identity.Review = identity<core::review_association_id>(9);
		candidate.Records[1].Position->Comparison.Left.Commit          = *core::parse_git_commit_oid(std::string(64, 'c'));
		candidate.Records[1].Position->Comparison.Right.Commit         = *core::parse_git_commit_oid(std::string(64, 'd'));
		expect(store->save(candidate, 1, { }) == candidate, "positions save");
		store.reset();
		store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }) == candidate, "full exact position and optional shapes survive restart");
		store.reset();
		auto*      raw     = raw_open(scratch.database());
		const auto payload = adapters::sqlite::detail::scalar_text(raw, "SELECT hex(position) FROM restoration_records WHERE window_id='1'");
		expect(payload && payload->substr(132, 16) == "0102030405060708", "epoch uses portable big endian bytes");
		for (const auto expression : { "x''", "zeroblob(513)", "substr(position,1,length(position)-1)", "CAST(position || x'00' AS BLOB)", "CAST(x'02' || substr(position,2) AS BLOB)", "zeroblob(length(position))" }) {
			execute(raw, "UPDATE restoration_records SET position=" + std::string{ expression } + " WHERE window_id='1'");
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(!store->load({ }), "malformed position refuses");
			store.reset();
			execute(raw, "UPDATE restoration_records SET position=x'" + *payload + "' WHERE window_id='1'");
		}
		for (const auto offset : { 65U, 66U, 106U, 127U, 139U, 140U, 172U, 279U, 312U, 345U, 346U, 378U, 383U, 416U }) {
			auto malformed = *payload;
			malformed.replace(offset * 2U, 2U, "FF");
			execute(raw, "UPDATE restoration_records SET position=x'" + malformed + "' WHERE window_id='1'");
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(!store->load({ }), "invalid enum, range, identity, padding, digest, or flag refuses");
			store.reset();
			expect(adapters::sqlite::detail::scalar_text(raw, "SELECT hex(position) FROM restoration_records WHERE window_id='1'") == malformed, "refused payload remains intact");
		}
		sqlite3_close(raw);
	}
	void test_migration()
	{
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == SQLITE_OK, "migration interruption installs");
		for (const auto prefix : { "CREATE TEMP TABLE restoration_migration", "DROP TABLE restoration_records", "CREATE TABLE restoration_records", "INSERT INTO restoration_records", "DROP TABLE restoration_migration", "PRAGMA user_version=2" }) {
			const auto scratch = scratch_directory{ };
			auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }).has_value() && store->save(filled(2, 2), 1, { }).has_value(), "legacy fixture starts");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, "DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN position; PRAGMA user_version=1");
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			auto stop            = std::stop_source{ };
			interruption::Stop   = &stop;
			interruption::Prefix = prefix;
			store                = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto cancelled = store->load(stop.get_token());
			interruption::Stop   = nullptr;
			store.reset();
			expect(stop.stop_requested() && !cancelled && cancelled.error().Failure == window_review_store_failure::cancelled, "migration cancels at durable boundary");
			raw = raw_open(scratch.database());
			expect(snapshot(raw) == before, "interrupted migration retains exact old schema and rows");
			sqlite3_close(raw);
			// Both windows name one destination, so the list keeps the newest as one entry.
			auto migrated = filled(2, 2);
			migrated.Recent.push_back({ migrated.Records.back().Destination, "", 0, std::nullopt });
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == migrated, "migration retry preserves revision, PR-only records and seeds the list");
			store.reset();
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == migrated, "second restart preserves migrated snapshot");
		}
		for (const auto corruption : { "UPDATE restoration_meta SET revision='01'", "UPDATE restoration_records SET owner='../invalid'", "CREATE TABLE extra(value TEXT)", "PRAGMA user_version=3", "PRAGMA user_version=4" }) {
			const auto scratch = scratch_directory{ };
			auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }).has_value() && store->save(filled(2, 2), 1, { }).has_value(), "refused migration fixture starts");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, "DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN position; PRAGMA user_version=1");
			execute(raw, corruption);
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(!store->load({ }), "invalid old data/schema and future schema refuse migration");
			store.reset();
			raw = raw_open(scratch.database());
			expect(snapshot(raw) == before, "refused migration preserves old data/schema");
			sqlite3_close(raw);
		}
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == 1, "migration interruption removed");
	}
	void test_corruptions()
	{
		for (const auto sql : { "PRAGMA user_version=99", "UPDATE restoration_meta SET revision='01'", "UPDATE restoration_meta SET revision='0'", "UPDATE restoration_records SET window_id='01'", "UPDATE restoration_records SET number='2147483648'", "UPDATE restoration_records SET number='017'", "UPDATE restoration_records SET hostname='GitHub.com'", "UPDATE restoration_records SET owner='../bad'", "UPDATE restoration_records SET hostname=hex(zeroblob(2097152))", "PRAGMA ignore_check_constraints=ON; UPDATE restoration_records SET mode=9", "WITH RECURSIVE ids(n) AS (VALUES(2) UNION ALL SELECT n+1 FROM ids WHERE n<33) INSERT INTO restoration_records SELECT CAST(n AS TEXT),0,'github.com','octo','repo','17',NULL FROM ids" }) {
			const auto scratch = scratch_directory{ };
			auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }).has_value() && store->save(filled(2, 1), 1, { }).has_value(), "corruption fixture persists");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, sql);
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			store              = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto refused = store->load({ });
			expect(!refused && !refused.error().Current, "corrupt/future store refuses without Current fabrication");
			store.reset();
			raw = raw_open(scratch.database());
			expect(snapshot(raw) == before, "refused database remains unchanged");
			sqlite3_close(raw);
		}
	}
	void test_interruption()
	{
		expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == SQLITE_OK, "interruption installs");
		const auto scratch   = scratch_directory{ };
		auto       store     = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		auto       stop      = std::stop_source{ };
		interruption::Stop   = &stop;
		interruption::Prefix = "PRAGMA user_version=3";
		const auto first     = store->load(stop.get_token());
		expect(stop.stop_requested() && !first && first.error().Failure == window_review_store_failure::cancelled, "initialization cancels after schema stamp");
		interruption::Stop = nullptr;
		store.reset();
		auto* raw = raw_open(scratch.database());
		expect(adapters::sqlite::detail::scalar_integer(raw, "SELECT count(*) FROM sqlite_schema") == 0 && adapters::sqlite::detail::scalar_integer(raw, "PRAGMA user_version") == 0, "initialization rolls back all schema and stamps");
		sqlite3_close(raw);
		store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }).has_value(), "retry initializes");
		const auto before = filled(2, 2);
		expect(store->save(before, 1, { }) == before, "interruption fixture persists");
		for (const auto prefix : { "DELETE FROM restoration_records", "INSERT INTO restoration_records", "UPDATE restoration_meta" }) {
			stop                 = std::stop_source{ };
			interruption::Stop   = &stop;
			interruption::Prefix = prefix;
			const auto refused   = store->save(filled(3, 1), 2, stop.get_token());
			interruption::Stop   = nullptr;
			expect(stop.stop_requested() && !refused && refused.error().Failure == window_review_store_failure::cancelled && store->load({ }) == before, "mid-transaction interruption restores rows and revision");
		}
		store.reset();
		expect(sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(interruption::install)) == 1, "interruption removed");
	}
	[[nodiscard]] auto destination(const std::string_view aRepository, const std::uint32_t aNumber, const core::github_access_mode aMode = core::github_access_mode::authenticated) -> core::window_review_destination
	{
		return { aMode, "github.com", { "octo", std::string{ aRepository } }, core::pull_request_number{ aNumber } };
	}
	[[nodiscard]] auto listed() -> window_review_snapshot
	{
		auto result     = window_review_snapshot{ };
		result.Revision = 2;
		result.Records.push_back({ core::bpr_window_id{ 1 }, destination("repo", 17), position() });
		result.Recent.push_back({ destination("repo", 17), "Fix the thing", 1'700'000'000, position() });
		result.Recent.push_back({ destination("other", 18, core::github_access_mode::public_preview), "", 0, std::nullopt });
		return result;
	}
	// [[BPR-0733]]. AC-01: the one-row schema becomes the list schema and the old
	// row is the newest entry.
	void test_recent_migration()
	{
		for (const auto legacy : { false, true }) {
			const auto scratch = scratch_directory{ };
			auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }).has_value(), "list migration fixture initializes");
			auto saved     = window_review_snapshot{ };
			saved.Revision = 2;
			saved.Records.push_back({ core::bpr_window_id{ 4 }, destination("repo", 17), position() });
			expect(store->save(saved, 1, { }) == saved, "one retained row persists");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, legacy ? "DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN position; PRAGMA user_version=1" : "DROP TABLE recent_reviews; PRAGMA user_version=2");
			sqlite3_close(raw);
			auto expected = saved;
			if (legacy) {
				expected.Records.front().Position.reset();
			}
			expected.Recent.push_back({ expected.Records.front().Destination, "", 0, expected.Records.front().Position });
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == expected, "the one row migrates to the list as the newest entry with no title and no viewing time");
			store.reset();
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == expected, "the migrated list is durable");
		}
	}
	void test_recent_collapse()
	{
		const auto scratch = scratch_directory{ };
		auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }).has_value(), "collapse fixture initializes");
		auto older = position();
		older.Selection.reset();
		auto saved     = window_review_snapshot{ };
		saved.Revision = 2;
		saved.Records.push_back({ core::bpr_window_id{ 2 }, destination("repo", 17), older });
		saved.Records.push_back({ core::bpr_window_id{ 9 }, destination("other", 18), std::nullopt });
		saved.Records.push_back({ core::bpr_window_id{ 11 }, destination("repo", 17), position() });
		expect(store->save(saved, 1, { }) == saved, "three windows over two destinations persist");
		store.reset();
		auto* raw = raw_open(scratch.database());
		execute(raw, "DROP TABLE recent_reviews; PRAGMA user_version=2");
		sqlite3_close(raw);
		store               = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		const auto migrated = store->load({ });
		expect(migrated.has_value() && migrated->Records == saved.Records && migrated->Revision == saved.Revision, "migration leaves the retained rows alone");
		expect(migrated->Recent.size() == 2, "two windows on one destination collapse to one entry");
		expect(migrated->Recent[0] == core::window_recent_review{ destination("repo", 17), "", 0, position() }, "the newest window leads and carries its own position");
		expect(migrated->Recent[1] == core::window_recent_review{ destination("other", 18), "", 0, std::nullopt }, "the older destination follows without a position");
	}
	void test_recent_refusals()
	{
		struct refusal
		{
			std::string_view            Sql;
			window_review_store_failure Failure;
		};
		for (const auto& refused : { refusal{ "UPDATE recent_reviews SET ordinal=5 WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET mode=0,repository='repo',number='17' WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET title=substr(hex(zeroblob(300)),1,513) WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET viewed_at=-1 WHERE ordinal=0", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET position=x'' WHERE ordinal=0", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET owner='../bad' WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "PRAGMA ignore_check_constraints=ON; UPDATE recent_reviews SET mode=9 WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "PRAGMA ignore_check_constraints=ON; WITH RECURSIVE ids(n) AS (VALUES(2) UNION ALL SELECT n+1 FROM ids WHERE n<10) INSERT INTO recent_reviews SELECT n,0,'github.com','octo','repo'||n,'17','',0,NULL FROM ids", window_review_store_failure::corrupt },
									 refusal{ "DROP TABLE recent_reviews", window_review_store_failure::schema_mismatch },
									 refusal{ "PRAGMA user_version=4", window_review_store_failure::schema_mismatch },
									 // A future stamp over a shape this build could otherwise migrate.
									 refusal{ "DROP TABLE recent_reviews; PRAGMA user_version=4", window_review_store_failure::schema_mismatch } }) {
			const auto scratch = scratch_directory{ };
			auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto listing = listed();
			expect(store->load({ }).has_value() && store->save(listing, 1, { }) == listing, "refusal fixture persists a list");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, refused.Sql);
			sqlite3_close(raw);
			const auto before = file_bytes(scratch.database());
			store             = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto opened = store->load({ });
			expect(!opened && opened.error().Failure == refused.Failure && !opened.error().Current, "a malformed or future list refuses without fabricating a current snapshot");
			store.reset();
			expect(file_bytes(scratch.database()) == before && !std::filesystem::exists(scratch.database().string() + "-wal"), "the refusal writes nothing at all");
		}
	}
	void test_recent_round_trip()
	{
		const auto scratch = scratch_directory{ };
		auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		auto       listing = listed();
		listing.Recent[1].Title.assign(core::maximum_recent_review_title, 'x');
		expect(store->load({ }).has_value() && store->save(listing, 1, { }) == listing, "a list with and without a saved position saves");
		store.reset();
		store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }) == listing, "titles, viewing times, positions and order round trip");
		auto pruned = listing;
		++pruned.Revision;
		pruned.Recent.erase(pruned.Recent.begin());
		expect(store->save(pruned, 2, { }) == pruned && store->load({ }) == pruned, "a save rewrites the whole list");
		auto invalid = pruned;
		++invalid.Revision;
		invalid.Recent.push_back(invalid.Recent.front());
		expect(!store->save(invalid, 3, { }) && store->load({ }) == pruned, "a repeated destination refuses without loss");
	}
}
int main()
{
	expect(adapters::sqlite::detail::sqlite_runtime_ready(), "SQLite runtime initializes");
	test_partition_bounds_and_conflict();
	test_positions();
	test_migration();
	test_corruptions();
	test_interruption();
	test_recent_migration();
	test_recent_collapse();
	test_recent_refusals();
	test_recent_round_trip();
	std::cout << "window-review-restoration-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
