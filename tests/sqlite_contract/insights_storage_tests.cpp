#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "scratch_root.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
	void write(const std::filesystem::path& aPath, const std::string& aBytes)
	{
		std::ofstream file{ aPath, std::ios::binary };
		file.write(aBytes.data(), static_cast<std::streamsize>(aBytes.size()));
		bpr::tests::expect(file.good(), "fixture bytes written");
	}
}

int main()
{
	using namespace bpr;
	using tests::expect;
	namespace fs = std::filesystem;
	tests::sqlite_contract::scratch_root scratch{ "insights-storage" };
	auto                                 store = adapters::sqlite::make_insights_storage(scratch.path(), [] { return std::vector<application::insights_storage_row>{ { "repository-canary", 6789, 65536, true } }; });
	expect(fs::is_regular_file(scratch.path() / "state/v1/insights.sqlite3"), "first launch creates the private lifetime store");
	const auto state = scratch.path() / "state/v1";
	write(state / "repository-canary.sqlite3", "ACCOUNT_CANARY PR_CANARY PATH_CANARY BODY_CANARY");
	write(state / "repository-canary.sqlite3-wal", std::string(1234, 'w'));
	write(state / "repository-canary.sqlite3-shm", std::string(4321, 's'));
	write(state / "markdown-images.sqlite3", std::string(2345, 'm'));
	write(state / "review-cycles.sqlite3", std::string(3456, 'c'));
	fs::create_directory(scratch.path() / "logs");
	write(scratch.path() / "logs/diagnostics.log", std::string(4567, 'l'));
	const auto    snapshot = store->measure();
	std::uint64_t walked   = 6789;
	for (const auto& item : fs::recursive_directory_iterator{ scratch.path() }) {
		if (item.is_regular_file()) {
			walked += item.file_size();
		}
	}
	if (snapshot.TotalBytes != walked) {
		std::cerr << "measured=" << snapshot.TotalBytes.value_or(0) << " walked=" << walked << '\n';
		for (const auto& row : snapshot.Storage) {
			std::cerr << row.Name << '=' << row.Bytes.value_or(0) << '\n';
		}
		for (const auto& item : fs::recursive_directory_iterator{ scratch.path() }) {
			if (item.is_regular_file()) {
				std::cerr << item.path().filename().string() << '=' << item.file_size() << '\n';
			}
		}
	}
	expect(snapshot.TotalBytes == walked && snapshot.GitBytes == 6789, "total exactly matches an independent filesystem walk plus supplied Git inventory, without double counting");
	const auto named = [&](std::string_view aName) -> const application::insights_storage_row& {
		const auto found = std::ranges::find(snapshot.Storage, aName, &application::insights_storage_row::Name);
		expect(found != snapshot.Storage.end(), "named store is reported");
		return *found;
	};
	expect(named("repository-canary.sqlite3").Bytes == fs::file_size(state / "repository-canary.sqlite3") + 1234 + 4321, "WAL and SHM bytes belong to their store");
	expect(named("markdown-images.sqlite3").Budget == 128ULL * 1024 * 1024 && named("review-cycles.sqlite3").Budget == 768ULL * 1024 * 1024 && named("Diagnostic logs").Budget == 50ULL * 1024 * 1024, "every existing fixed budget is shown");
	const auto exported = adapters::sqlite::insights_storage_export(scratch.path());
	expect(exported.find("\"section\":\"insights\"") != std::string::npos && exported.find("local_store_bytes") != std::string::npos, "anonymized export has the byte section");
	for (const auto canary : { "repository-canary", "ACCOUNT_CANARY", "PR_CANARY", "PATH_CANARY", "BODY_CANARY", "state/v1" }) {
		expect(exported.find(canary) == std::string::npos, "no identity, path or body reaches the export");
	}
	core::insights_event request;
	request.Scope            = { 1, 7 };
	request.UnixSeconds      = 1000;
	request.Limit            = 5000;
	request.Remaining        = 3760;
	request.ResetUnixSeconds = 4000;
	store->record({ request }, request.Scope);
	request.UnixSeconds = 1010;
	request.Remaining   = 3800;
	store->record({ request }, std::nullopt);
	request.UnixSeconds = 1020;
	request.Limit.reset();
	request.Remaining.reset();
	request.ResetUnixSeconds = 0;
	store->record({ request }, std::nullopt);
	expect(store->github(1100).Budgets[0].OwnRequests == 3, "real store records headerless and observed requests");
	store.reset();
	store = adapters::sqlite::make_insights_storage(scratch.path(), [] { return std::vector<application::insights_storage_row>{ }; });
	expect(store->measure().TotalBytes.has_value(), "restart reuses the Insights store");
	const auto github = store->github(1100);
	expect(github.Budgets[0].OwnRequests == 3 && github.Budgets[0].OtherClients == 1237, "restart inside the window keeps BPR requests out of other clients");
	expect(!store->github(4001).Budgets[0].Limit, "reset without a response remains no reading and issues no request");
	request.UnixSeconds      = 4010;
	request.Limit            = 5000;
	request.Remaining        = 4999;
	request.ResetUnixSeconds = 7600;
	store->record({ request }, std::nullopt);
	expect(store->github(4020).Budgets[0].OwnRequests == 1, "a new reset does not inherit the preceding window count");
	using metric     = core::insights_review_metric;
	const auto count = [](const core::insights_reviews_snapshot& aSnapshot, metric aMetric) { return aSnapshot.Ranges[2][static_cast<std::size_t>(aMetric)]; };
	expect(count(store->reviews(2000000000), metric::submitted) == 0, "existing ledger-like canary files are never backfilled");
	std::vector<core::insights_review_event> activity;
	for (unsigned kind = 0; kind <= 9; ++kind) {
		core::insights_review_event event;
		event.Kind         = static_cast<core::insights_review_kind>(kind);
		event.Host         = "github.com";
		event.Repository   = "PRIVATE_REPOSITORY_CANARY";
		event.PullRequest  = 42;
		event.Identity     = "EVENT_CANARY";
		event.Head         = kind == 5 || kind == 6 ? "HEAD_CANARY" : "";
		event.UnixSeconds  = 1792368000;
		event.LocalDay     = 20743;
		event.FilesChanged = 125;
		event.Lines        = 37;
		event.Suggestion   = true;
		event.Disposition  = 0;
		activity.push_back(event);
	}
	store->record_reviews(activity);
	store->record_reviews(activity);
	auto reviews = store->reviews(1792454400);
	for (const auto selected : { metric::opened, metric::submitted, metric::approved, metric::comments, metric::replies, metric::resolved, metric::seen, metric::accepted, metric::notes, metric::defects, metric::rounds }) {
		expect(count(reviews, selected) == 1, "a duplicate persisted event does not change its count");
	}
	expect(count(reviews, metric::suggestions) == 2 && count(reviews, metric::lines) == 37, "suggestions count comments and replies, and duplicate file acceptance never adds lines");
	auto opened = activity.front();
	++opened.LocalDay;
	opened.UnixSeconds += 86400;
	store->record_reviews({ opened });
	auto accepted = activity[6];
	++accepted.LocalDay;
	accepted.UnixSeconds += 86400;
	store->record_reviews({ accepted });
	expect(count(store->reviews(1792454400), metric::opened) == 2 && count(store->reviews(1792454400), metric::accepted) == 1, "next-day reopening counts, next-day file retry does not");
	accepted.Head = "NEXT_HEAD";
	store->record_reviews({ accepted });
	expect(count(store->reviews(1792454400), metric::accepted) == 2, "same path on a new head counts once more");
	store.reset();
	store = adapters::sqlite::make_insights_storage(scratch.path(), [] { return std::vector<application::insights_storage_row>{ }; });
	static_cast<void>(store->github(2000000000));
	expect(count(store->reviews(2000000000), metric::submitted) == 1 && count(store->reviews(2000000000), metric::lines) == 74, "restart and history retention leave lifetime review totals intact");
	store->record_reviews(activity);
	expect(count(store->reviews(2000000000), metric::submitted) == 1, "event identity persists beyond history retention and restart");
	const auto safeExport = adapters::sqlite::insights_storage_export(scratch.path());
	expect(safeExport.find("PRIVATE_REPOSITORY_CANARY") == std::string::npos && safeExport.find("EVENT_CANARY") == std::string::npos && safeExport.find("HEAD_CANARY") == std::string::npos, "review identities never enter the numeric anonymized export");

	core::insights_review_event dst;
	dst.Kind        = core::insights_review_kind::submitted;
	dst.Host        = "github.com";
	dst.Repository  = "dst/repository";
	dst.PullRequest = 99;
	dst.Identity    = "dst-first";
	dst.UnixSeconds = 1793521800;
	dst.LocalDay    = application::local_insights_day(dst.UnixSeconds);
	store->record_reviews({ dst });
	dst.Identity    = "dst-second";
	dst.UnixSeconds = 1793525400;
	dst.LocalDay    = application::local_insights_day(dst.UnixSeconds);
	store->record_reviews({ dst });
	store->record_reviews({ dst });
	const auto afterDst = store->reviews(1793606400);
	const auto overlap  = std::ranges::find(afterDst.Calendar, std::int64_t{ 20758 }, &core::insights_review_calendar_day::Day);
	expect(overlap != afterDst.Calendar.end() && overlap->Reviews == 2 && afterDst.Ranges[0][static_cast<std::size_t>(metric::submitted)] == 0 && afterDst.Ranges[1][static_cast<std::size_t>(metric::submitted)] == 3, "seeded store spans the repeated DST hour and local Monday boundary without duplicating a receipt");

#if !defined(_WIN32)
	fs::create_symlink(state / "markdown-images.sqlite3", state / "link.sqlite3");
	bool refused = false;
	try {
		static_cast<void>(store->measure());
	} catch (...) {
		refused = true;
	}
	expect(refused, "a linked store cannot masquerade as an owned measurement");
#endif
	std::cout << "insights-storage:pass\n";
}
