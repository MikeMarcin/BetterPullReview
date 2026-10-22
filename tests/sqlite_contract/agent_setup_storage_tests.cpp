// [[BPR-PENDING-AQ2]], RFC-BPR-0091. A profile's agent setup preferences:
// defaults before any write, a restart reads them back, refused values are not
// written, an identity mismatch is refused, and the file is owner-private.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#ifndef _WIN32
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using tests::expect;
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "agent-setup" };
	const auto                           path  = scratch.path() / "state/v1/agent-setup.sqlite3";
	auto                                 store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(!std::filesystem::exists(path), "construction writes nothing");
	const auto first = store->load();
	expect(first && *first == application::agent_setup_preferences{ }, "a profile with nothing stored reads defaults");

	auto chosen           = application::agent_setup_preferences{ };
	chosen.Chosen         = core::setup_agent::codex;
	chosen.CodexProgram   = "/opt/tools/codex-wrapper";
	chosen.HarnessProgram = "/opt/corp/harness";
	chosen.CodexProtocol  = core::codex_protocol_choice::jsonl;
	expect(store->store(chosen), "preferences are stored");
	store.reset();
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == chosen, "a restarted profile reads the same preferences back");

	auto none   = chosen;
	none.Chosen = std::nullopt;
	expect(store->store(none) && store->load() == none, "no chosen agent is kept as none");

	auto tooLong         = chosen;
	tooLong.CodexProgram = "/" + std::string(2000U, 'a');
	expect(!store->store(tooLong), "an over-long program is not written");
	auto control           = chosen;
	control.HarnessProgram = "/bin/harness\n--flag";
	expect(!store->store(control), "a program with a control character is not written");
	expect(store->load() == none, "a refused write leaves the stored preferences as they were");

#ifndef _WIN32
	struct stat status{ };
	expect(::stat(path.c_str(), &status) == 0 && (status.st_mode & 0077) == 0, "the file is owner-private");
#endif

	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "UPDATE agent_setup SET chosen=7", nullptr, nullptr, nullptr) != SQLITE_OK, "an out-of-range agent is refused by the schema");
		expect(sqlite3_exec(database, "PRAGMA application_id=1112560199", nullptr, nullptr, nullptr) == SQLITE_OK, "another store's identity is written");
		sqlite3_close_v2(database);
	}
	expect(!store->load() && !store->store(chosen), "a file with another store's identity is refused, not read or overwritten");
	std::cout << "agent-setup-storage:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return EXIT_FAILURE;
}
