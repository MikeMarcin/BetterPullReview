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
	store.reset();
	store = adapters::sqlite::make_insights_storage(scratch.path(), [] { return std::vector<application::insights_storage_row>{ }; });
	expect(store->measure().TotalBytes.has_value(), "restart reuses the Insights store");
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
