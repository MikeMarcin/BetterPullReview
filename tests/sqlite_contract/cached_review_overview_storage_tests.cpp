#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/cached_review_overview_store.hpp"
#include "scratch_root.hpp"
#include <cstdlib>
#include <iostream>
#include <sqlite3.h>

namespace {
	using namespace bpr;
	using tests::expect;
	auto overview() -> core::cached_review_overview
	{
		core::cached_review_overview result{
			.Key         = { .Authority = { 1 }, .Profile = core::host_profile_id{ 1 }, .Host = core::git_host_id{ 1 }, .Account = core::github_account_id{ 7 }, .Locator = { "owner", "repo" }, .Number = core::pull_request_number{ 17 } },
			.Repository  = core::repository_id{ 22 },
			.PullRequest = core::pull_request_id{ 33 },
			.BaseOid     = std::string(40, '1'),
			.HeadOid     = std::string(40, '2'),
			.Title       = "Retained overview",
			.Body        = "Historical body\nwith text",
			.AuthorLogin = "author",
			.BaseRef     = "main",
			.HeadRef     = "review",
			.Files       = 2,
			.Commits     = 1
		};
		return result;
	}
	struct commit_cancellation
	{
		static inline std::stop_source* Stop = nullptr;
		static auto                     trace(unsigned int, void*, void* aStatement, void*) -> int
		{
			const auto* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement));
			if (Stop && sql && std::string_view{ sql } == "COMMIT") {
				Stop->request_stop();
			}
			return 0;
		}
		static auto install(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
		{
			return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_PROFILE, trace, nullptr);
		}
	};
	void execute(const std::filesystem::path& aPath, const char* aSql)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(aPath.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "test opens its owned store");
		expect(sqlite3_exec(database, aSql, nullptr, nullptr, nullptr) == SQLITE_OK, "test mutates its owned store");
		sqlite3_close(database);
	}
}
int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "0969-overviews" };
	const auto                           path     = scratch.path() / "state/v1/review-overviews.sqlite3";
	auto                                 store    = adapters::sqlite::make_cached_review_overview_store(scratch.path());
	auto                                 original = overview();
	expect(!store->load(original.Key, { }), "absent cached content is unavailable");
	expect(store->save(original, { }), "accepted overview saves");
	store.reset();
	store = adapters::sqlite::make_cached_review_overview_store(scratch.path());
	expect(store->load(original.Key, { }) == original, "exact overview survives connection and store recreation");
	auto other    = original.Key;
	other.Account = core::github_account_id{ 8 };
	expect(!store->load(other, { }), "another account cannot read this row");
	other              = original.Key;
	other.Authority[0] = 2;
	expect(!store->load(other, { }), "profile revision or host authority change misses the row");
	other              = original.Key;
	other.Locator.Name = "other";
	expect(!store->load(other, { }), "another repository cannot read this row");
	other        = original.Key;
	other.Number = core::pull_request_number{ 18 };
	expect(!store->load(other, { }), "another PR cannot read this row");
	std::stop_source stopped;
	stopped.request_stop();
	expect(!store->load(original.Key, stopped.get_token()), "cancelled read exposes no metadata");
	auto next    = original;
	next.Title   = "New server truth";
	next.HeadOid = std::string(40, '3');
	expect(!store->save(next, stopped.get_token()) && store->load(original.Key, { }) == original, "cancelled write preserves previous exact revision");
	expect(store->save(next, { }) && store->load(next.Key, { }) == next, "new accepted server metadata atomically replaces old revision");
	std::stop_source lateStop;
	commit_cancellation::Stop = &lateStop;
	expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(commit_cancellation::install)) == SQLITE_OK, "late cancellation fixture installed");
	expect(store->save(next, lateStop.get_token()) && lateStop.stop_requested(), "durable commit wins cancellation delivered after its completion");
	sqlite3_reset_auto_extension();
	commit_cancellation::Stop = nullptr;
	expect(store->load(next.Key, { }) == next, "late cancellation never conceals the persisted record");
	auto invalid    = next;
	invalid.HeadOid = "not-a-commit";
	expect(!store->save(invalid, { }) && store->load(next.Key, { }) == next, "invalid revision cannot replace durable evidence");
	invalid = next;
	invalid.Body.resize(256 * 1024 + 1, 'x');
	expect(!store->save(invalid, { }), "oversized body refused without truncation");
	for (std::uint32_t number = 20; number < 30; ++number) {
		auto entry        = original;
		entry.Key.Number  = core::pull_request_number{ number };
		entry.PullRequest = core::pull_request_id{ number };
		expect(store->save(entry, { }), "bounded eviction accepts another exact destination");
	}
	expect(!store->load(original.Key, { }), "eleventh distinct row evicts oldest");
	expect(store->save(next, { }), "evicted target can be observed again");
	execute(path, "UPDATE overviews SET digest=zeroblob(32)");
	expect(!store->load(next.Key, { }), "corrupted durable payload never renders");
	execute(path, "PRAGMA user_version=99");
	expect(!store->load(next.Key, { }) && !store->save(next, { }), "unknown schema fails closed without rewriting");
	std::cout << "cached-review-overview-storage:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
