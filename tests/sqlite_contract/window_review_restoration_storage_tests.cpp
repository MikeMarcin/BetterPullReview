#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
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
	void test_corruptions()
	{
		for (const auto sql : { "PRAGMA user_version=99", "UPDATE restoration_meta SET revision='01'", "UPDATE restoration_meta SET revision='0'", "UPDATE restoration_records SET window_id='01'", "UPDATE restoration_records SET number='2147483648'", "UPDATE restoration_records SET number='017'", "UPDATE restoration_records SET hostname='GitHub.com'", "UPDATE restoration_records SET owner='../bad'", "UPDATE restoration_records SET hostname=hex(zeroblob(2097152))", "PRAGMA ignore_check_constraints=ON; UPDATE restoration_records SET mode=9", "WITH RECURSIVE ids(n) AS (VALUES(2) UNION ALL SELECT n+1 FROM ids WHERE n<33) INSERT INTO restoration_records SELECT CAST(n AS TEXT),0,'github.com','octo','repo','17' FROM ids" }) {
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
		interruption::Prefix = "PRAGMA user_version=1";
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
}
int main()
{
	expect(adapters::sqlite::detail::sqlite_runtime_ready(), "SQLite runtime initializes");
	test_partition_bounds_and_conflict();
	test_corruptions();
	test_interruption();
	std::cout << "window-review-restoration-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
