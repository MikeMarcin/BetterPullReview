#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/cached_review_overview_store.hpp"
#include "bpr/core/sha256.hpp"
#include "scratch_root.hpp"
#include <cstdlib>
#include <functional>
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
		result.FileCatalogue = std::vector<core::cached_review_file>{
			{ "src/new.cpp", "src/old.cpp", core::cached_file_change::renamed, 23, 8 },
			{ "removed.cpp", std::nullopt, core::cached_file_change::removed, 0, 42 }
		};
		result.ReviewThreads = std::vector<core::cached_review_thread>{
			{ .Id = "T1", .Path = "src/new.cpp", .Line = 7, .OriginalLine = 5, .Side = core::cached_thread_side::right, .Resolved = true, .Comments = { { .Id = "C1", .AuthorLogin = "author", .Body = "<literal> root\ntext", .Line = 7 }, { .Id = "C2", .ReplyTo = "C1", .AuthorLogin = "reader", .Body = "Reply", .OriginalLine = 5, .Outdated = true, .Deleted = true } } }
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
	void alter_payload(const std::filesystem::path& aPath, const std::function<void(std::vector<std::uint8_t>&)>& aAlter)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(aPath.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "open owned payload fixture");
		sqlite3_stmt* select = nullptr;
		expect(sqlite3_prepare_v2(database, "SELECT payload FROM overviews ORDER BY sequence DESC LIMIT 1", -1, &select, nullptr) == SQLITE_OK
				   && sqlite3_step(select) == SQLITE_ROW,
			   "read current encoded fixture");
		const auto* data    = static_cast<const std::uint8_t*>(sqlite3_column_blob(select, 0));
		auto        payload = std::vector<std::uint8_t>{ data, data + sqlite3_column_bytes(select, 0) };
		sqlite3_finalize(select);
		aAlter(payload);
		const auto    digest = core::sha256(payload);
		sqlite3_stmt* update = nullptr;
		expect(sqlite3_prepare_v2(database, "UPDATE overviews SET payload=?1,digest=?2 WHERE sequence=(SELECT max(sequence) FROM overviews)", -1, &update, nullptr) == SQLITE_OK, "prepare payload fixture mutation");
		expect(sqlite3_bind_blob(update, 1, payload.data(), static_cast<int>(payload.size()), SQLITE_TRANSIENT) == SQLITE_OK
				   && sqlite3_bind_blob(update, 2, digest.data(), static_cast<int>(digest.size()), SQLITE_TRANSIENT) == SQLITE_OK
				   && sqlite3_step(update) == SQLITE_DONE,
			   "publish fixture payload and matching integrity digest");
		sqlite3_finalize(update);
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
	auto next                   = original;
	next.Title                  = "New server truth";
	next.HeadOid                = std::string(40, '3');
	next.BaseOid                = std::string(40, '4');
	next.FileCatalogue->front() = { "new-server.cpp", std::nullopt, core::cached_file_change::added, 99, 0 };
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
	invalid = next;
	invalid.FileCatalogue->push_back(invalid.FileCatalogue->front());
	invalid.Files = 3;
	expect(!store->save(invalid, { }), "duplicate catalogue paths refused");
	invalid       = next;
	invalid.Files = 3;
	expect(!store->save(invalid, { }), "catalogue count cannot disagree with its exact overview");
	invalid                               = next;
	invalid.FileCatalogue->front().Change = static_cast<core::cached_file_change>(255);
	expect(!store->save(invalid, { }), "unknown file operation refused");
	invalid                                     = next;
	invalid.FileCatalogue->front().PreviousPath = std::string(4097, 'p');
	expect(!store->save(invalid, { }), "oversized previous path refused");
	invalid = next;
	invalid.FileCatalogue->clear();
	for (unsigned index = 0; index < 40; ++index) {
		invalid.FileCatalogue->push_back({ std::to_string(index) + std::string(4090, 'p'), std::nullopt, core::cached_file_change::modified });
	}
	invalid.Files = 40;
	expect(!store->save(invalid, { }), "aggregate catalogue byte budget refuses whole content, never truncates");
	invalid = next;
	invalid.FileCatalogue->resize(core::maximum_cached_files + 1);
	invalid.Files = static_cast<std::uint32_t>(invalid.FileCatalogue->size());
	expect(!store->save(invalid, { }), "file count allocation bound enforced");
	invalid = next;
	invalid.ReviewThreads->front().Comments.front().Body.resize(64 * 1024 + 1, 'x');
	expect(!store->save(invalid, { }), "thread body limit refuses complete record without truncation");
	invalid = next;
	invalid.ReviewThreads->resize(core::maximum_cached_threads + 1);
	expect(!store->save(invalid, { }), "thread count bounded before allocation");
	invalid = next;
	invalid.ReviewThreads->front().Comments.resize(core::maximum_cached_thread_comments + 1);
	expect(!store->save(invalid, { }), "aggregate comment count bounded");
	invalid = next;
	invalid.ReviewThreads->front().Comments[0].Body.resize(64 * 1024, 'x');
	invalid.ReviewThreads->front().Comments[1].Body.resize(64 * 1024, 'x');
	expect(!store->save(invalid, { }) && store->load(next.Key, { }) == next, "aggregate thread budget refuses without replacing accepted evidence");
	auto catalogueOnly = next;
	catalogueOnly.ReviewThreads.reset();
	expect(store->save(catalogueOnly, { }), "unavailable threads save independently of catalogue");
	alter_payload(path, [](auto& aPayload) { aPayload.resize(aPayload.size() - 16); });
	expect(store->load(next.Key, { }) == catalogueOnly, "previous catalogue payload keeps threads unavailable");
	auto empty  = next;
	empty.Files = 0;
	empty.FileCatalogue->clear();
	empty.ReviewThreads->clear();
	expect(store->save(empty, { }) && store->load(empty.Key, { }) == empty, "observed empty catalogue remains present and empty");
	alter_payload(path, [](auto& aPayload) { aPayload.back() = 255; });
	expect(!store->load(empty.Key, { }), "forged count with valid digest refuses before file allocation");
	auto legacy = next;
	legacy.FileCatalogue.reset();
	legacy.ReviewThreads.reset();
	expect(store->save(legacy, { }), "unavailable catalogue overview saves");
	alter_payload(path, [](auto& aPayload) { aPayload.resize(aPayload.size() - 32); });
	expect(store->load(legacy.Key, { }) == legacy, "original payload loads with unavailable catalogue, never observed empty");
	expect(store->save(next, { }) && store->load(next.Key, { }) == next, "new accepted record replaces legacy payload atomically");
	auto malformed = next;
	malformed.FileCatalogue.reset();
	malformed.ReviewThreads.reset();
	expect(store->save(malformed, { }), "extension-version control saved");
	alter_payload(path, [](auto& aPayload) { aPayload[aPayload.size() - 9] = 99; });
	expect(!store->load(next.Key, { }), "unknown extension version refuses all historical content");
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
