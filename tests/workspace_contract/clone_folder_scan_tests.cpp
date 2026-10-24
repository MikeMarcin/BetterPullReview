/// \file
/// \brief [[BPR-1750]], RFC-BPR-0092 owner decision D2. The clone
/// folder scan over a disposable root on the native filesystem: what it finds
/// at each depth, what it never follows or reads, and that it writes nothing.

#include "../sqlite_contract/scratch_root.hpp"
#include "workspace_fixture.hpp"

#include "bpr/adapters/workspace/clone_folder_scan.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
	#include <windows.h>
#endif

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

	struct inventory_entry
	{
		fs::file_type Type;
		std::int64_t  Written;
		auto          operator==(const inventory_entry&) const -> bool = default;
	};
	using inventory_snapshot = std::map<std::string, inventory_entry>;
	enum class inventory_reader { cached_entry,
								  current_handle };

	[[nodiscard]] auto current_write_time(const fs::path& aPath, std::error_code& aError) -> std::int64_t
	{
#if defined(_WIN32)
		const auto handle = ::CreateFileW(aPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
										  nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (handle == INVALID_HANDLE_VALUE) {
			aError = std::error_code{ static_cast<int>(::GetLastError()), std::system_category() };
			return 0;
		}
		auto       info    = FILE_BASIC_INFO{ };
		const auto read    = ::GetFileInformationByHandleEx(handle, FileBasicInfo, &info, static_cast<DWORD>(sizeof(info)));
		const auto failure = read ? ERROR_SUCCESS : ::GetLastError();
		const auto closed  = ::CloseHandle(handle);
		if (!read) {
			aError = std::error_code{ static_cast<int>(failure), std::system_category() };
		} else if (!closed) {
			aError = std::error_code{ static_cast<int>(::GetLastError()), std::system_category() };
		} else {
			aError.clear();
		}
		return info.LastWriteTime.QuadPart;
#else
		return static_cast<std::int64_t>(fs::last_write_time(aPath, aError).time_since_epoch().count());
#endif
	}

	/// Every file and directory under the root with its type and last write
	/// time, so a scan that wrote anything shows up.
	[[nodiscard]] auto inventory(const fs::path& aRoot, const std::string_view aStage, const inventory_reader aReader = inventory_reader::current_handle) -> inventory_snapshot
	{
		auto seen        = inventory_snapshot{ };
		auto error       = std::error_code{ };
		auto currentPath = aRoot;
		for (auto entry = fs::recursive_directory_iterator{ aRoot, fs::directory_options::none, error }; !error && entry != fs::recursive_directory_iterator{ };
			 entry.increment(error)) {
			currentPath        = entry->path();
			const auto type    = fs::symlink_status(currentPath, error).type();
			auto       written = std::int64_t{ };
			if (!error && type != fs::file_type::symlink) {
				written = aReader == inventory_reader::current_handle ? current_write_time(currentPath, error) : static_cast<std::int64_t>(entry->last_write_time(error).time_since_epoch().count());
			}
			if (error) {
				break;
			}
			seen[text(currentPath)] = { type, written };
		}
		if (error) {
			std::cerr << "CLONE_SCAN_INVENTORY_ERROR stage=" << aStage << " path=" << text(currentPath) << " error=" << error.value() << " message=" << error.message() << '\n';
		}
		expect(!error, "the no-write inventory reads every fixture entry");
		return seen;
	}

	void report_inventory_changes(const inventory_snapshot& aBefore, const inventory_snapshot& aAfter, const std::string_view aStage)
	{
		if (aBefore == aAfter) {
			return;
		}
		std::cerr << "CLONE_SCAN_INVENTORY stage=" << aStage << " before=" << aBefore.size() << " after=" << aAfter.size() << '\n';
		auto remaining = 8U;
		for (const auto& [path, stamp] : aBefore) {
			const auto current = aAfter.find(path);
			if (remaining != 0U && (current == aAfter.end() || current->second != stamp)) {
				std::cerr << "CLONE_SCAN_CHANGED path=" << path << " present=" << (current != aAfter.end())
						  << " before-type=" << static_cast<int>(stamp.Type) << " before-ticks=" << stamp.Written;
				if (current != aAfter.end()) {
					std::cerr << " after-type=" << static_cast<int>(current->second.Type) << " after-ticks=" << current->second.Written;
				}
				std::cerr << '\n';
				--remaining;
			}
		}
		for (const auto& [path, stamp] : aAfter) {
			if (remaining != 0U && !aBefore.contains(path)) {
				std::cerr << "CLONE_SCAN_ADDED path=" << path << " type=" << static_cast<int>(stamp.Type) << " ticks=" << stamp.Written << '\n';
				--remaining;
			}
		}
	}
} // namespace

int main(const int aCount, char** aValues)
try {
	const auto observationControl = aCount == 2 && std::string_view{ aValues[1] } == "--inventory-observation-control";
	expect(aCount == 1 || observationControl, "the clone course receives a supported argument");
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

	if (observationControl) {
		const auto cachedBefore = inventory(scratch.path(), "cached-no-scan-before", inventory_reader::cached_entry);
		const auto cachedAfter  = inventory(scratch.path(), "cached-no-scan-after", inventory_reader::cached_entry);
		std::cout << "CLONE_INVENTORY_CONTROL cached_equal_without_scan=" << (cachedBefore == cachedAfter) << '\n';
		report_inventory_changes(cachedBefore, cachedAfter, "cached-no-scan");
		const auto freshBefore = inventory(scratch.path(), "handle-no-scan-before", inventory_reader::current_handle);
		const auto freshAfter  = inventory(scratch.path(), "handle-no-scan-after", inventory_reader::current_handle);
		std::cout << "CLONE_INVENTORY_CONTROL cached_vs_handle_equal_without_scan=" << (cachedBefore == freshBefore) << '\n';
		report_inventory_changes(cachedBefore, freshBefore, "cached-vs-handle-no-scan");
		report_inventory_changes(freshBefore, freshAfter, "handle-no-scan");
		expect(freshBefore == freshAfter, "fresh metadata is unchanged without a scanner invocation");
		std::cout << "CLONE_INVENTORY_CONTROL handle_equal_without_scan=1\n";
		const auto cachedAfterHandles = inventory(scratch.path(), "cached-after-handles-no-scan", inventory_reader::cached_entry);
		std::cout << "CLONE_INVENTORY_CONTROL cached_equal_after_handles_without_scan=" << (cachedBefore == cachedAfterHandles) << '\n';
		report_inventory_changes(cachedBefore, cachedAfterHandles, "cached-after-handles-no-scan");
	}
	const auto before  = inventory(scratch.path(), "before");
	const auto folders = std::vector<std::string>{ text(github), text(src), text(scratch.path() / "missing") };
	const auto found   = adapters::workspace::scan_clone_folders(folders);
	const auto after   = inventory(scratch.path(), "after");
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
	report_inventory_changes(before, after, "scan");
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
	if (observationControl) {
		const auto beforeWrite   = inventory(scratch.path(), "before-intentional-write", inventory_reader::current_handle);
		const auto changedFile   = github / "engine/README.md";
		const auto previousWrite = fs::last_write_time(changedFile);
		write_file(changedFile, "intentional changed fixture bytes\n");
		fs::last_write_time(changedFile, previousWrite + std::chrono::seconds{ 2 });
		const auto addedFile = root / "intentional-write-canary";
		write_file(addedFile, "intentional new fixture name\n");
		const auto afterWrite = inventory(scratch.path(), "after-intentional-write", inventory_reader::current_handle);
		expect(beforeWrite != afterWrite && afterWrite.at(text(changedFile)).Written != beforeWrite.at(text(changedFile)).Written,
			   "fresh metadata still detects an intentional content and timestamp write");
		expect(!beforeWrite.contains(text(addedFile)) && afterWrite.contains(text(addedFile)), "the no-write inventory still detects an added fixture name");
		std::cout << "CLONE_INVENTORY_CONTROL handle_equal_across_scan=1 intentional_write_detected=1 added_name_detected=1\n"
				  << "clone-inventory-observation-control:pass\n";
	}
	std::cout << "clone-folder-scan:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return EXIT_FAILURE;
}
