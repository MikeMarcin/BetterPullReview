// [[BPR-PENDING-AQ2]], RFC-BPR-0092. A profile's agent setup preferences:
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
#include <vector>
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

	// [[BPR-PENDING-AQ3]]. Clone folders and repository links in the same file.
	auto links = adapters::sqlite::make_clone_link_store(scratch.path());
	expect(links->load_clone_folders() == std::vector<std::string>{ } && links->load_links() == std::vector<application::repository_link>{ }, "no folders or links at first");
	const auto folders = std::vector<std::string>{ "/Users/me/Documents/GitHub", "/Users/me/src" };
	expect(links->store_clone_folders(folders) && links->load_clone_folders() == folders, "folders keep their order");
	expect(links->store_clone_folders(std::vector<std::string>{ "/Users/me/src" }) && links->load_clone_folders() == std::vector<std::string>{ "/Users/me/src" },
		   "storing folders replaces them");
	auto tooMany = std::vector<std::string>(core::maximum_clone_folders + 1U, "/x");
	expect(!links->store_clone_folders(tooMany), "more than the folder bound is refused");
	auto link       = application::repository_link{ };
	link.Host       = "github.com";
	link.Repository = 42U;
	link.Owner      = "bpr-fixtures";
	link.Name       = "engine";
	link.Path       = "/Users/me/src/engine";
	link.Remote     = "origin";
	expect(links->store_link(link), "a link is stored");
	auto copy        = link;
	copy.Repository  = 43U;
	copy.Name        = "other";
	copy.Path        = { };
	copy.Remote      = { };
	copy.PrivateCopy = true;
	expect(links->store_link(copy), "a private copy choice is stored");
	auto invalid        = link;
	invalid.PrivateCopy = true;
	expect(!links->store_link(invalid), "a private copy with a path is refused");
	links.reset();
	links          = adapters::sqlite::make_clone_link_store(scratch.path());
	const auto all = links->load_links();
	expect(all && all->size() == 2U && (*all)[0] == link && (*all)[1] == copy, "links read back after a restart");
	expect(links->forget_link("github.com", 42U, "bpr-fixtures", "engine") && links->load_links()->size() == 1U, "Forget removes one link");
	expect(store->load() == none, "the preferences are untouched by links");

	// [[BPR-PENDING-AQ5]]. The edited review skill and its record.
	using loaded_skill = std::optional<std::optional<application::stored_review_skill>>;
	const auto noSkill = loaded_skill{ std::in_place, std::nullopt };
	auto       skills  = adapters::sqlite::make_review_skill_store(scratch.path());
	expect(skills->load_review_skill() == noSkill, "no edited skill reads as none");
	auto edited               = application::stored_review_skill{ .Text = "---\nname: bpr-pr-review\ndescription: d\n---\nMine.\n" };
	edited.Record.BaseVersion = 3U;
	edited.Record.BaseDigest.fill(0xABU);
	expect(skills->store_review_skill(edited), "an edited skill is stored");
	skills.reset();
	skills = adapters::sqlite::make_review_skill_store(scratch.path());
	expect(skills->load_review_skill() == loaded_skill{ std::in_place, edited }, "and reads back whole after a restart, with no kept version");
	edited.Record.KeptVersion = 4U;
	expect(skills->store_review_skill(edited) && skills->load_review_skill() == loaded_skill{ std::in_place, edited }, "Keep mine's version is stored");
	expect(!skills->store_review_skill({ .Text = std::string(core::maximum_review_skill_bytes + 1U, 'x') }), "a skill over its bound is refused");
	expect(skills->forget_review_skill() && skills->load_review_skill() == noSkill, "Restore default removes it");
	expect(store->load() == none && links->load_links()->size() == 1U, "the preferences and links are untouched by the skill");

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
