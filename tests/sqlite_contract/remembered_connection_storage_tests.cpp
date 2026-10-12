#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "scratch_root.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#if !defined(_WIN32)
	#include <sys/wait.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	using failure      = application::remembered_connection_store_failure;
	using scratch_root = tests::sqlite_contract::scratch_root;
	auto database(const scratch_root& aRoot) -> std::filesystem::path { return aRoot.path() / "state/v1/remembered-connections.sqlite3"; }
	auto raw_open(const std::filesystem::path& aPath) -> sqlite3*
	{
		sqlite3* raw = nullptr;
		expect(sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "fixture opens existing database");
		return raw;
	}
	void execute(sqlite3* aDatabase, const char* aSql) { expect(sqlite3_exec(aDatabase, aSql, nullptr, nullptr, nullptr) == SQLITE_OK, "fixture SQL executes"); }
	auto bytes(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	}
	auto connection(std::uint64_t aProfile = 2U, std::uint64_t aAccount = 17U) -> core::remembered_connection
	{
		core::remembered_connection row;
		row.Profile         = core::host_profile_id{ aProfile };
		row.ProfileRevision = 1U;
		row.Host            = core::git_host_id{ 1U };
		row.Hostname        = "github.com";
		row.Account         = core::github_account_id{ aAccount };
		row.Login           = "fixture-user";
		row.Transport       = core::remembered_connection_transport::github_cli;
		return row;
	}
	auto make_store(const scratch_root& aRoot, core::bpr_profile_id aProfile = core::default_bpr_profile_id)
	{
		return adapters::sqlite::make_remembered_connection_store(aRoot.path(), aProfile);
	}
	void reopen_and_stale_saves()
	{
		const auto root  = scratch_root{ "1408-reopen" };
		auto       store = make_store(root);
		const auto empty = store->load({ });
		expect(empty && empty->Connections.empty() && !core::select_remembered_connection(*empty), "fresh or upgraded profile without metadata authorizes no CLI attempt");
		const auto first = *core::remember_connection(*empty, empty->Revision, connection());
		expect(store->save(first, empty->Revision, { }).has_value(), "public port commits complete accepted selection");
		store.reset();
		store = make_store(root);
		expect(store->load({ }) == first, "new store reopens exact host login numeric binding and stable identities");
		const auto disabled = *core::disable_connection_reuse(first, first.Revision, "github.com");
		expect(store->save(disabled, first.Revision, { }).has_value(), "disable reuse commits promptly");
		auto       stale       = make_store(root);
		const auto replacement = *core::remember_connection(first, first.Revision, connection(3U, 18U));
		const auto conflict    = stale->save(replacement, first.Revision, { });
		expect(!conflict && conflict.error() == failure::conflict && stale->load({ }) == disabled, "late stale replacement cannot overwrite a durable sign out");
		store.reset();
		stale.reset();
		store = make_store(root);
		expect(store->load({ }) == disabled && !core::select_remembered_connection(*store->load({ })), "sign out remains disabled after reopen");
		const auto fresh = *core::remember_connection(disabled, disabled.Revision, connection(3U, 18U));
		expect(store->save(fresh, disabled.Revision, { }).has_value(), "successful explicit replacement commits atomically");
		const auto forgotten = *core::forget_connection(fresh, fresh.Revision, "github.com");
		expect(store->save(forgotten, fresh.Revision, { }).has_value(), "Forget removes metadata");
		store.reset();
		store = make_store(root);
		expect(store->load({ }) == forgotten && forgotten.LastProfileId == 3U, "Forget survives reopen with identity high-water retained");
		const auto wrongScope = make_store(root, core::bpr_profile_id{ 2U });
		expect(!wrongScope->load({ }), "another profile cannot borrow this root's selection");
		const auto otherRoot = scratch_root{ "1408-other-profile" };
		const auto other     = make_store(otherRoot, core::bpr_profile_id{ 2U })->load({ });
		expect(other && other->BprProfile == core::bpr_profile_id{ 2U } && other->Connections.empty(), "a different profile owns an independent snapshot");
	}
	void refused_content_is_unchanged()
	{
		for (const auto mutation : {
				 "PRAGMA user_version=99",
				 "PRAGMA application_id=42",
				 "UPDATE remembered_connection SET account='0'",
				 "UPDATE remembered_connection SET login='bad login'",
				 "UPDATE remembered_connection SET host='01'",
				 "UPDATE connection_meta SET last_profile='0'",
				 "UPDATE connection_meta SET bpr_profile='2'",
				 "DELETE FROM connection_meta",
				 "CREATE TABLE unexpected(value TEXT)",
				 "DROP TABLE remembered_connection" }) {
			const auto root    = scratch_root{ "1408-corrupt" };
			auto       store   = make_store(root);
			const auto initial = *store->load({ });
			const auto first   = *core::remember_connection(initial, initial.Revision, connection());
			expect(store->save(first, initial.Revision, { }).has_value(), "seed complete record before malformed fixture");
			store.reset();
			auto* raw = raw_open(database(root));
			execute(raw, mutation);
			sqlite3_close(raw);
			const auto before = bytes(database(root));
			store             = make_store(root);
			const auto loaded = store->load({ });
			expect(!loaded && (loaded.error() == failure::corrupt || loaded.error() == failure::newer_schema), "malformed or newer storage cannot produce a validation input");
			expect(!store->save(first, initial.Revision, { }), "refused load cannot be reset by save");
			store.reset();
			expect(bytes(database(root)) == before, "refused content is not silently reset or changed");
		}
		const auto root = scratch_root{ "1408-nondatabase" };
		{
			auto store = make_store(root);
			expect(store->load({ }).has_value(), "initialize private root");
		}
		{
			std::ofstream file{ database(root), std::ios::binary | std::ios::trunc };
			file << "not a database";
		}
		const auto before = bytes(database(root));
		expect(!make_store(root)->load({ }) && bytes(database(root)) == before, "unreadable non-database stays intact and refused");
	}
	std::stop_source* WriteStop = nullptr;
	void              cancel_write() { WriteStop->request_stop(); }
	void              cancellation_and_write_refusal()
	{
		const auto root  = scratch_root{ "1408-write-refusal" };
		auto       store = make_store(root);
		const auto empty = *store->load({ });
		const auto first = *core::remember_connection(empty, empty.Revision, connection());
		expect(store->save(first, empty.Revision, { }).has_value(), "initial record saved");
		const auto       candidate = *core::disable_connection_reuse(first, first.Revision, "github.com");
		std::stop_source stop;
		stop.request_stop();
		const auto cancelled = store->save(candidate, first.Revision, stop.get_token());
		expect(!cancelled && cancelled.error() == failure::cancelled && store->load({ }) == first, "cancelled admitted input cannot replace complete durable record");
		expect(!store->load(stop.get_token()), "cancelled load grants no validation attempt");
		auto* rival = raw_open(database(root));
		execute(rival, "BEGIN IMMEDIATE");
		const auto refused = store->save(candidate, first.Revision, { });
		expect(!refused && refused.error() == failure::unavailable, "write contention returns typed unavailable for host's session-only reporting");
		execute(rival, "ROLLBACK");
		sqlite3_close(rival);
		expect(store->load({ }) == first, "refused save keeps prior complete record");
		std::stop_source midWriteStop;
		WriteStop = &midWriteStop;
		adapters::sqlite::detail::testing::set_remembered_connection_write_checkpoint(cancel_write);
		const auto interrupted = store->save(candidate, first.Revision, midWriteStop.get_token());
		adapters::sqlite::detail::testing::set_remembered_connection_write_checkpoint(nullptr);
		WriteStop = nullptr;
		expect(!interrupted && interrupted.error() == failure::cancelled && store->load({ }) == first, "cancellation after complete writes rolls back both metadata and connection rows");
		expect(store->save(candidate, first.Revision, { }).has_value(), "ordinary retry succeeds after refusal");
	}
#if !defined(_WIN32)
	void interrupted_writer()
	{
		const auto                        root = scratch_root{ "1408-interrupt" };
		core::remembered_connection_state first;
		{
			auto       store = make_store(root);
			const auto empty = *store->load({ });
			first            = *core::remember_connection(empty, empty.Revision, connection());
			expect(store->save(first, empty.Revision, { }).has_value(), "seed before interrupted write");
		}
		const auto child = fork();
		expect(child >= 0, "start fixture writer");
		if (child == 0) {
			auto       store     = make_store(root);
			const auto candidate = *core::remember_connection(first, first.Revision, connection(3U, 18U));
			adapters::sqlite::detail::testing::set_remembered_connection_write_checkpoint([] { _exit(77); });
			static_cast<void>(store->save(candidate, first.Revision, { }));
			_exit(1);
		}
		int status = 0;
		expect(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77, "writer exits with incomplete transaction");
		expect(make_store(root)->load({ }) == first, "interrupted transaction recovers old complete selection and revision");
	}
#endif
}
int main()
{
	reopen_and_stale_saves();
	refused_content_is_unchanged();
	cancellation_and_write_refusal();
#if !defined(_WIN32)
	interrupted_writer();
#endif
	std::cout << "remembered-connection-storage-tests:pass\n";
}
