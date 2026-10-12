#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "scratch_root.hpp"
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
	using bpr::tests::expect;

	/// \brief This contract's scratch root: `bpr-0709-restoration-` and a random part.
	/// \details [[BPR-1128]]. Made by the SQLite contracts' helper ([[BPR-1126]])
	/// at a name nothing had, owner-private from creation, and removed only by
	/// the course that made it.
	class scratch_directory final : public bpr::tests::sqlite_contract::scratch_root
	{
	public:
		scratch_directory()
			: scratch_root{ "0709-restoration" }
			, Root{ path() }
		{
		}
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
			execute(raw, "DROP TABLE review_tabs; DROP TABLE review_tab_sets; DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN account; ALTER TABLE restoration_records DROP COLUMN position; PRAGMA user_version=1");
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
		for (const auto corruption : { "UPDATE restoration_meta SET revision='01'", "UPDATE restoration_records SET owner='../invalid'", "CREATE TABLE extra(value TEXT)", "PRAGMA user_version=3", "PRAGMA user_version=4", "PRAGMA user_version=5" }) {
			const auto scratch = scratch_directory{ };
			auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }).has_value() && store->save(filled(2, 2), 1, { }).has_value(), "refused migration fixture starts");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, "DROP TABLE review_tabs; DROP TABLE review_tab_sets; DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN account; ALTER TABLE restoration_records DROP COLUMN position; PRAGMA user_version=1");
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
		for (const auto sql : { "PRAGMA user_version=99", "UPDATE restoration_meta SET revision='01'", "UPDATE restoration_meta SET revision='0'", "UPDATE restoration_records SET window_id='01'", "UPDATE restoration_records SET number='2147483648'", "UPDATE restoration_records SET number='017'", "UPDATE restoration_records SET hostname='GitHub.com'", "UPDATE restoration_records SET owner='../bad'", "UPDATE restoration_records SET hostname=hex(zeroblob(2097152))", "PRAGMA ignore_check_constraints=ON; UPDATE restoration_records SET mode=9", "WITH RECURSIVE ids(n) AS (VALUES(2) UNION ALL SELECT n+1 FROM ids WHERE n<33) INSERT INTO restoration_records SELECT CAST(n AS TEXT),0,'github.com','octo','repo','17',NULL,'0' FROM ids" }) {
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
		interruption::Prefix = "PRAGMA user_version=6";
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
			execute(raw, legacy ? "DROP TABLE review_tabs; DROP TABLE review_tab_sets; DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN account; ALTER TABLE restoration_records DROP COLUMN position; PRAGMA user_version=1" : "DROP TABLE review_tabs; DROP TABLE review_tab_sets; DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN account; PRAGMA user_version=2");
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
		execute(raw, "DROP TABLE review_tabs; DROP TABLE review_tab_sets; DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN account; PRAGMA user_version=2");
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
		for (const auto& refused : { refusal{ "UPDATE recent_reviews SET account='01' WHERE ordinal=0", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET account='18446744073709551616' WHERE ordinal=0", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET account='7' WHERE mode=1", window_review_store_failure::corrupt },
									 refusal{ "UPDATE restoration_records SET account='-1'", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET ordinal=5 WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET mode=0,repository='repo',number='17' WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET title=substr(hex(zeroblob(300)),1,513) WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET viewed_at=-1 WHERE ordinal=0", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET position=x'' WHERE ordinal=0", window_review_store_failure::corrupt },
									 refusal{ "UPDATE recent_reviews SET owner='../bad' WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "PRAGMA ignore_check_constraints=ON; UPDATE recent_reviews SET mode=9 WHERE ordinal=1", window_review_store_failure::corrupt },
									 refusal{ "PRAGMA ignore_check_constraints=ON; WITH RECURSIVE ids(n) AS (VALUES(2) UNION ALL SELECT n+1 FROM ids WHERE n<10) INSERT INTO recent_reviews SELECT n,0,'github.com','octo','repo'||n,'17','',0,NULL,'0',NULL FROM ids", window_review_store_failure::corrupt },
									 refusal{ "PRAGMA ignore_check_constraints=ON; UPDATE recent_reviews SET draft=3 WHERE ordinal=0", window_review_store_failure::corrupt },
									 refusal{ "DROP TABLE recent_reviews", window_review_store_failure::schema_mismatch },
									 refusal{ "PRAGMA user_version=7", window_review_store_failure::schema_mismatch },
									 // A future stamp over a shape this build could otherwise migrate.
									 refusal{ "DROP TABLE review_tabs; DROP TABLE review_tab_sets; DROP TABLE recent_reviews; ALTER TABLE restoration_records DROP COLUMN account; PRAGMA user_version=5", window_review_store_failure::schema_mismatch },
									 // [[BPR-1131]]. A version 3 stamp over the version 4 shape.
									 refusal{ "PRAGMA user_version=3", window_review_store_failure::schema_mismatch } }) {
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
	/// \brief [[BPR-1131]]. A collection of `aCount` tabs, two of which, when
	/// there are two, show one file on different pairs.
	[[nodiscard]] auto tabs(const std::size_t aCount, const std::optional<std::uint32_t> aActive) -> core::window_review_tabs
	{
		auto result             = core::window_review_tabs{ };
		result.WindowComparison = position().Comparison;
		for (std::size_t index = 0U; index < aCount; ++index) {
			auto view                            = position();
			view.Comparison.Right.Identity.Epoch = core::epoch_ordinal{ static_cast<std::int64_t>(index + 1U) };
			if (index % 2U == 1U) {
				view.Selection.reset();
			}
			result.Tabs.push_back(view);
		}
		result.Active = aActive;
		expect(core::valid(result), "tab fixture valid");
		return result;
	}
	[[nodiscard]] auto tabbed() -> window_review_snapshot
	{
		auto result            = listed();
		result.Records[0].Tabs = tabs(2U, 1U);
		result.Recent[0].Tabs  = tabs(3U, 0U);
		result.Recent[1].Tabs  = tabs(0U, std::nullopt);
		return result;
	}
	// [[BPR-1131]]. Tab order, the active tab, each tab's exact view and the
	// window's comparison survive a restart, on a record and on the list.
	void test_tabs_round_trip()
	{
		const auto scratch = scratch_directory{ };
		auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		const auto saved   = tabbed();
		expect(saved.Records[0].Tabs->Tabs[0].File == saved.Records[0].Tabs->Tabs[1].File
				   && saved.Records[0].Tabs->Tabs[0].Comparison != saved.Records[0].Tabs->Tabs[1].Comparison,
			   "fixture: two tabs show one file on different pairs");
		expect(store->load({ }).has_value() && store->save(saved, 1, { }) == saved, "tab collections save");
		store.reset();
		store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }) == saved, "order, active tab, exact views, empty collections and the window's comparison round trip");
		auto trimmed = saved;
		++trimmed.Revision;
		trimmed.Records[0].Tabs.reset();
		trimmed.Recent[0].Tabs->Tabs.pop_back();
		expect(store->save(trimmed, 2, { }) == trimmed && store->load({ }) == trimmed, "a save rewrites every collection");
		auto invalid = trimmed;
		++invalid.Revision;
		invalid.Recent[0].Tabs->Active = 5U;
		expect(!store->save(invalid, 3, { }) && store->load({ }) == trimmed, "an invalid collection refuses without loss");
	}
	void test_tabs_bound()
	{
		const auto scratch = scratch_directory{ };
		auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		auto       full    = filled(2, core::maximum_bpr_windows_per_profile);
		for (auto& record : full.Records) {
			record.Tabs = tabs(core::maximum_document_tabs, 15U);
		}
		for (std::uint32_t index = 0U; index < core::maximum_recent_reviews; ++index) {
			full.Recent.push_back({ destination("repo", 100U + index), "", 0, position(), tabs(core::maximum_document_tabs, 0U) });
		}
		expect(store->load({ }).has_value() && store->save(full, 1, { }) == full, "the largest valid file saves");
		store.reset();
		store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }) == full, "the largest valid file reads back within the bounded read");
		auto over = full;
		++over.Revision;
		over.Records[0].Tabs->Tabs.push_back(position());
		expect(!store->save(over, 2, { }) && store->load({ }) == full, "a seventeenth tab refuses");
	}
	void test_tabs_refusals()
	{
		for (const auto sql : { "DELETE FROM review_tabs WHERE scope=0 AND ordinal=0",
								"UPDATE review_tab_sets SET owner='9' WHERE scope=0",
								"UPDATE review_tab_sets SET owner='7' WHERE scope=1 AND owner='1'",
								"UPDATE review_tab_sets SET active=NULL WHERE scope=0",
								"UPDATE review_tab_sets SET active=2 WHERE scope=0",
								"UPDATE review_tab_sets SET window_comparison=x'' WHERE scope=0",
								"UPDATE review_tabs SET view=x'00' WHERE scope=0 AND ordinal=1",
								"UPDATE review_tabs SET view=(SELECT window_comparison FROM review_tab_sets WHERE scope=0) WHERE scope=0 AND ordinal=1",
								"INSERT INTO review_tabs SELECT 1,'1',0,view FROM review_tabs WHERE scope=0 AND ordinal=0",
								"UPDATE review_tabs SET owner='2' WHERE scope=1 AND owner='0' AND ordinal=2",
								"PRAGMA ignore_check_constraints=ON; UPDATE review_tabs SET ordinal=16 WHERE scope=0 AND ordinal=1",
								"DROP TABLE review_tabs" }) {
			const auto scratch = scratch_directory{ };
			auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto saved   = tabbed();
			expect(store->load({ }).has_value() && store->save(saved, 1, { }) == saved, "tab refusal fixture persists");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, sql);
			sqlite3_close(raw);
			const auto before = file_bytes(scratch.database());
			store             = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto opened = store->load({ });
			if (opened) {
				std::cerr << "accepted: " << sql << '\n';
			}
			expect(!opened && !opened.error().Current, "a malformed tab collection refuses without fabricating a current snapshot");
			store.reset();
			expect(file_bytes(scratch.database()) == before, "the refusal writes nothing");
		}
	}
	void test_account_migration()
	{
		for (const auto prefix : { "CREATE TEMP TABLE account_records_migration", "DROP TABLE restoration_records", "INSERT INTO restoration_records", "CREATE TEMP TABLE account_recent_migration", "DROP TABLE recent_reviews", "INSERT INTO recent_reviews", "PRAGMA user_version=5", "COMMIT" }) {
			const auto scratch    = scratch_directory{ };
			auto       store      = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			auto       saved      = listed();
			saved.Records[0].Tabs = tabs(1, 0);
			saved.Recent[0].Tabs  = tabs(1, 0);
			expect(store->load({ }).has_value() && store->save(saved, 1, { }) == saved, "legacy account fixture saved");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, "ALTER TABLE restoration_records DROP COLUMN account; ALTER TABLE recent_reviews DROP COLUMN draft; ALTER TABLE recent_reviews DROP COLUMN account; PRAGMA user_version=4");
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			std::stop_source stop;
			interruption::Stop   = &stop;
			interruption::Prefix = prefix;
			expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(&interruption::install)) == SQLITE_OK, "migration trace installed");
			store                = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto cancelled = store->load(stop.get_token());
			expect(!cancelled && cancelled.error().Failure == window_review_store_failure::cancelled, "every account migration boundary cancels");
			store.reset();
			sqlite3_reset_auto_extension();
			interruption::Stop = nullptr;
			raw                = raw_open(scratch.database());
			if (std::string_view{ prefix } == "COMMIT") {
				expect(adapters::sqlite::detail::scalar_integer(raw, "PRAGMA user_version") == 6, "commit may finish before cancellation is observed");
			} else {
				expect(snapshot(raw) == before, "pre-commit cancellation preserves schema and records");
			}
			sqlite3_close(raw);
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == saved, "legacy metadata, positions and tabs survive with unknown ownership");
			auto claimed = saved;
			++claimed.Revision;
			claimed.Records[0].Destination.Account = core::github_account_id{ std::numeric_limits<std::uint64_t>::max() };
			claimed.Recent[0].Destination.Account  = claimed.Records[0].Destination.Account;
			expect(store->save(claimed, saved.Revision, { }) == claimed, "full width account identity persists");
			store.reset();
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == claimed, "account, metadata and exact reading survive restart");
		}
	}
	// [[BPR-1131]]. A version 3 file gains empty tab tables and keeps every row.
	void test_tabs_migration()
	{
		const auto scratch = scratch_directory{ };
		auto       store   = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		const auto saved   = listed();
		expect(store->load({ }).has_value() && store->save(saved, 1, { }) == saved, "version 3 fixture persists");
		store.reset();
		auto* raw = raw_open(scratch.database());
		execute(raw, "DROP TABLE review_tabs; DROP TABLE review_tab_sets; ALTER TABLE restoration_records DROP COLUMN account; ALTER TABLE recent_reviews DROP COLUMN draft; ALTER TABLE recent_reviews DROP COLUMN account; PRAGMA user_version=3");
		sqlite3_close(raw);
		store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
		expect(store->load({ }) == saved, "a version 3 file migrates with its records and list intact");
		store.reset();
		raw = raw_open(scratch.database());
		expect(adapters::sqlite::detail::scalar_integer(raw, "PRAGMA user_version") == 6
				   && adapters::sqlite::detail::scalar_integer(raw, "SELECT count(*) FROM review_tabs") == 0,
			   "the migrated file is version 6 with no tabs");
		sqlite3_close(raw);
	}
	void test_recent_draft_migration_and_rollback()
	{
		for (const auto prefix : { "CREATE TEMP TABLE draft_recent_migration", "DROP TABLE recent_reviews", "INSERT INTO recent_reviews", "PRAGMA user_version=6", "COMMIT" }) {
			const auto scratch    = scratch_directory{ };
			auto       store      = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			auto       saved      = listed();
			saved.Recent[0].Draft = true;
			saved.Recent[1].Draft = false;
			expect(store->load({ }).has_value() && store->save(saved, 1, { }) == saved, "known draft and ready persist with exact destinations");
			store.reset();
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == saved, "recent draft metadata survives reopening");
			store.reset();
			auto* raw = raw_open(scratch.database());
			execute(raw, "ALTER TABLE recent_reviews DROP COLUMN draft; PRAGMA user_version=5");
			const auto before = snapshot(raw);
			sqlite3_close(raw);
			saved.Recent[0].Draft.reset();
			saved.Recent[1].Draft.reset();
			std::stop_source stop;
			interruption::Stop   = &stop;
			interruption::Prefix = prefix;
			expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(&interruption::install)) == SQLITE_OK, "draft migration trace installed");
			store             = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			const auto result = store->load(stop.get_token());
			expect(!result && result.error().Failure == window_review_store_failure::cancelled, "migration cancellation is explicit");
			store.reset();
			sqlite3_reset_auto_extension();
			interruption::Stop = nullptr;
			raw                = raw_open(scratch.database());
			if (std::string_view{ prefix } != "COMMIT") {
				expect(snapshot(raw) == before, "cancelled draft migration rolls back all changes");
			}
			sqlite3_close(raw);
			store = adapters::sqlite::make_window_review_restoration_store(scratch.Root);
			expect(store->load({ }) == saved, "older Recent metadata remains unknown and preserves reading");
		}
	}

}
int main()
try {
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
	test_tabs_round_trip();
	test_tabs_bound();
	test_tabs_refusals();
	test_tabs_migration();
	test_account_migration();
	test_recent_draft_migration_and_rollback();
	std::cout << "window-review-restoration-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
