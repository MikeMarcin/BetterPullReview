/// \file
/// \brief [[BPR-1750]], RFC-BPR-0092 owner decision D2. The clone
/// folder scan over a disposable root on the native filesystem: what it finds
/// at each depth, what it never follows or reads, and that it writes nothing.

#include "../sqlite_contract/scratch_root.hpp"
#include "workspace_fixture.hpp"

#include "bpr/adapters/workspace/clone_folder_scan.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::write_file;
	namespace fs = std::filesystem;

	[[nodiscard]] auto text(const fs::path& aPath) -> std::string
	{
		auto       preferred = aPath;
		const auto value     = preferred.make_preferred().u8string();
		return std::string{ reinterpret_cast<const char*>(value.data()), value.size() };
	}

	void make_clone(const fs::path& aPath, const std::string& aRemote, const std::string& aBranch = "main")
	{
		write_file(aPath / ".git/config", "[core]\n\tbare = false\n[remote \"origin\"]\n\turl = " + aRemote + "\n");
		write_file(aPath / ".git/HEAD", "ref: refs/heads/" + aBranch + "\n");
		write_file(aPath / "README.md", "fixture\n");
	}

	/// Every file and directory under the root with its last write time, so a
	/// scan that wrote anything shows up.
	[[nodiscard]] auto inventory(const fs::path& aRoot) -> std::map<std::string, fs::file_time_type>
	{
		auto seen  = std::map<std::string, fs::file_time_type>{ };
		auto error = std::error_code{ };
		for (auto entry = fs::recursive_directory_iterator{ aRoot, fs::directory_options::none, error }; !error && entry != fs::recursive_directory_iterator{ };
			 entry.increment(error)) {
			seen[text(entry->path())] = fs::symlink_status(entry->path()).type() == fs::file_type::symlink ? fs::file_time_type{ } : entry->last_write_time();
		}
		return seen;
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "clone-scan" };
	const auto                           root   = scratch.path() / "home";
	const auto                           github = root / "Documents/GitHub";
	const auto                           src    = root / "src";
	make_clone(github / "engine", "https://ghp_canarytoken@github.com/bpr-fixtures/engine.git");
	make_clone(src / "org/engine-old", "git@github.com:bpr-fixtures/engine.git", "spike/retry");
	make_clone(src / "org/team/too-deep", "https://github.com/bpr-fixtures/engine");
	make_clone(src / ".hidden/engine", "https://github.com/bpr-fixtures/engine");
	write_file(src / "linked-worktree/.git", "gitdir: /elsewhere/.git/worktrees/x\n");
	make_clone(github / "other", "https://github.com/bpr-fixtures/other");
	fs::create_directories(src / "plain/folder");
	auto       linkError = std::error_code{ };
	const auto outside   = scratch.path() / "outside";
	make_clone(outside / "linked-clone", "https://github.com/bpr-fixtures/engine");
	fs::create_directory_symlink(outside / "linked-clone", src / "link-to-clone", linkError);
	fs::create_directory_symlink(outside, github / "link-to-folder", linkError);
	const auto links = !linkError;

	const auto before  = inventory(scratch.path());
	const auto folders = std::vector<std::string>{ text(github), text(src), text(scratch.path() / "missing") };
	const auto found   = adapters::workspace::scan_clone_folders(folders);
	const auto after   = inventory(scratch.path());
	auto       paths   = std::vector<std::string>{ };
	for (const auto& candidate : found) {
		paths.push_back(candidate.Path);
	}
	std::ranges::sort(paths);
	auto want = std::vector<std::string>{ text(github / "engine"), text(github / "other"), text(src / "org/engine-old") };
	std::ranges::sort(want);
	if (paths != want) {
		for (const auto& path : paths) {
			std::cerr << "CLONE_SCAN_FOUND:" << path << '\n';
		}
		for (const auto& path : want) {
			std::cerr << "CLONE_SCAN_WANTED:" << path << '\n';
		}
	}
	expect(paths == want, "clones one and two levels below a listed folder are found; three levels down, hidden folders and a .git file are not");
	if (links) {
		expect(std::ranges::none_of(found, [&outside](const core::clone_candidate& aFound) { return aFound.Path.find("linked-clone") != std::string::npos || aFound.Path.find(text(outside)) != std::string::npos; }),
			   "a link to a clone and a link to a folder are never followed");
	}
	expect(before == after, "the scan writes nothing");
	const auto engine = std::ranges::find(found, text(github / "engine"), &core::clone_candidate::Path);
	expect(engine != found.end() && engine->Folder == text(github) && engine->Branch == "main", "a candidate names its listed folder and branch");
	expect(engine->Remotes.size() == 1U && engine->Remotes.front().Name == "origin", "its remotes are read from .git/config");

	const auto target  = core::clone_target{ .Host = "github.com", .Owner = "bpr-fixtures", .Name = "engine" };
	const auto matches = core::match_clones(found, target, folders, "");
	expect(matches.size() == 2U && matches.front().Path == text(github / "engine"), "both clones of the repository match, ordered by folder");
	expect(std::ranges::none_of(matches, [](const core::clone_match& aMatch) { return aMatch.Location.find("canarytoken") != std::string::npos; }),
		   "the token in a remote URL appears in no match");
	expect(matches.back().Branch == "spike/retry", "the branch is read from .git/HEAD");

	expect(adapters::workspace::read_clone(text(github / "engine")).has_value(), "a chosen folder that is a clone reads");
	expect(!adapters::workspace::read_clone(text(src / "plain/folder")), "a chosen folder that is not a clone reads nothing");
	expect(!adapters::workspace::read_clone("relative/engine"), "a relative path reads nothing");
	if (links) {
		expect(!adapters::workspace::read_clone(text(src / "link-to-clone")), "a chosen link is not followed");
	}
	write_file(github / "big/.git/config", std::string(core::maximum_clone_config_bytes, '\n') + "[remote \"late\"]\n\turl = https://github.com/bpr-fixtures/engine\n");
	write_file(github / "big/.git/HEAD", "ref: refs/heads/main\n");
	const auto big = adapters::workspace::read_clone(text(github / "big"));
	expect(big && big->Remotes.empty(), "a config is read only up to its bound");

	const auto existing = adapters::workspace::existing_folders(core::usual_clone_folders(text(root), fs::path::preferred_separator == '\\'));
	expect(existing == std::vector<std::string>{ text(root / "Documents/GitHub"), text(root / "src") }, "only usual folders that exist are suggested");
	std::cout << "clone-folder-scan:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return EXIT_FAILURE;
}
