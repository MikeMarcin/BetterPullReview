// [[BPR-1602]]. Each Troubleshooting reset removes exactly its stores and
// folders from a real private profile root and leaves every other file
// byte-identical; no reset touches credentials; a reset stopped part-way is
// finished by recovery; a refusal removes nothing; the optional copy holds the
// removed stores.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "platform_test_support.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace {
	using namespace bpr;
	using reason = core::release_maintenance_reason;
	using reset  = core::profile_data_reset;
	namespace fs = std::filesystem;
	using bpr::tests::expect;

	constexpr auto cache_hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

	struct scratch
	{
		fs::path Path;
		explicit scratch(std::string_view aName)
		{
			Path               = fs::temp_directory_path() / ("bpr-reset-" + std::string{ aName } + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			const auto created = fs::create_directory(Path);
			auto       made    = bpr::tests::scratch_guard{ created ? Path : fs::path{ } };
			expect(created && adapters::sqlite::make_storage_root_owner_private(Path), "private disposable root");
			Path = fs::canonical(Path);
			made.release();
		}
		~scratch()
		{
			std::error_code ignored;
			fs::permissions(Path / "repositories/v1" / (std::string{ cache_hash } + ".git") / "objects/pack", fs::perms::owner_all, ignored);
			fs::remove_all(Path, ignored);
		}
		scratch(const scratch&)                    = delete;
		auto operator=(const scratch&) -> scratch& = delete;
	};

	void write(const fs::path& aRoot, const std::string& aRelative)
	{
		const auto path = aRoot / aRelative;
		fs::create_directories(path.parent_path());
		for (auto folder = path.parent_path(); folder != aRoot && folder != folder.parent_path(); folder = folder.parent_path()) {
			expect(adapters::sqlite::make_storage_root_owner_private(folder), "private fixture folder");
		}
		{
			std::ofstream file{ path, std::ios::binary };
			const auto    bytes = "bytes of " + aRelative;
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			expect(file.good(), "write fixture");
		}
		fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
	}

	auto read(const fs::path& aPath) -> std::string
	{
		std::ifstream file{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
	}

	/// Every file under the root, by relative path, with its bytes.
	auto snapshot(const fs::path& aRoot) -> std::map<std::string, std::string>
	{
		std::map<std::string, std::string> files;
		for (const auto& item : fs::recursive_directory_iterator{ aRoot }) {
			if (item.is_regular_file()) {
				files.emplace(fs::relative(item.path(), aRoot).generic_string(), read(item.path()));
			}
		}
		return files;
	}

	auto cache_files() -> std::set<std::string>
	{
		return {
			"state/v1/epoch-catalog.sqlite3", "state/v1/epoch-catalog.sqlite3-wal", "state/v1/epoch-catalog.lock",
			"state/v1/markdown-images.sqlite3", "state/v1/review-overviews.sqlite3",
			std::string{ "repositories/v1/" } + cache_hash + ".git/HEAD",
			std::string{ "repositories/v1/" } + cache_hash + ".git/betterpullreview-cache-manifest-v1",
			std::string{ "repositories/v1/" } + cache_hash + ".git/objects/pack/tmp_pack_unfinished",
			std::string{ "locks/v1/" } + cache_hash + ".lock",
			"incoming/v1/partial-spool", "quarantine/v1/left-by-a-crash.deleted", "mcp-snapshot/stale/context.json"
		};
	}
	auto settings_files() -> std::set<std::string>
	{
		return {
			"state/v1/appearance-settings.sqlite3", "state/v1/workbench-settings.sqlite3", "state/v1/workbench-settings.sqlite3-shm",
			"state/v1/diagnostic-preferences.sqlite3", "state/v1/window-restoration.sqlite3"
		};
	}
	auto review_files() -> std::set<std::string>
	{
		return {
			"state/v1/review-ledger.sqlite3", "state/v1/review-ledger.sqlite3-wal", "state/v1/review-tours.sqlite3",
			"state/v1/review-cycles.sqlite3", "state/v1/review-mutation-recovery.sqlite3",
			"publication-git-v1/publication/v1/sha1.git/HEAD"
		};
	}
	auto saved_source_files() -> std::set<std::string>
	{
		return { "state/v1/review-queue.sqlite3", "state/v1/review-queue.sqlite3-wal", "state/v1/insights.sqlite3", "state/v1/insights.sqlite3-wal", "state/v1/insights.sqlite3-shm" };
	}
	/// Never part of any reset: sign-in, logs, shared stores, agent work,
	/// Git isolation, sibling profiles and anything the policy does not name.
	auto kept_files() -> std::set<std::string>
	{
		return {
			"state/v1/device-bindings.sqlite3", "state/v1/remembered-connections.sqlite3", "state/v1/profile-registry.sqlite3",
			"state/v1/release-update.sqlite3", "state/v1/update-draft-recovery.sqlite3", "state/v1/a-store-added-later.sqlite3",
			"logs/session-0123456789abcdef0123456789abcdef-00000001.jsonl", "logs/not-a-log-name",
			"agent-workspace-ownership-v1/claim", "config-global", "home/.gitconfig", "profiles/other/state/v1/review-ledger.sqlite3",
			"updates/pending", "an-unknown-top-level-file"
		};
	}

	void populate(const fs::path& aRoot)
	{
		for (const auto& set : { cache_files(), settings_files(), review_files(), saved_source_files(), kept_files() }) {
			for (const auto& file : set) {
				write(aRoot, file);
			}
		}
	}

	auto expected_removed(const reset aReset) -> std::set<std::string>
	{
		std::set<std::string> removed;
		const auto            add = [&removed](const std::set<std::string>& aFiles) { removed.insert(aFiles.begin(), aFiles.end()); };
		if (aReset == reset::caches || aReset == reset::everything) {
			add(cache_files());
		}
		if (aReset == reset::settings || aReset == reset::everything) {
			add(settings_files());
		}
		if (aReset == reset::review_data || aReset == reset::everything) {
			add(review_files());
		}
		if (aReset == reset::everything) {
			add(saved_source_files());
		}
		return removed;
	}

	auto lease(const fs::path& aPath) -> std::shared_ptr<application::profile_runtime_lease>
	{
		auto result = adapters::sqlite::acquire_profile_runtime_lease(aPath);
		expect(result.has_value(), "native lease admitted");
		return *result;
	}

	/// Fails the test on any credential call: a reset never reads or deletes one.
	class credentials final : public application::release_profile_cleanup_port
	{
	public:
		int  Calls = 0;
		auto inspect(std::stop_token) -> std::expected<application::release_profile_resource_inventory, reason> override
		{
			++Calls;
			return application::release_profile_resource_inventory{ { }, 2U };
		}
		auto erase(const application::release_profile_resource_inventory&) -> application::release_maintenance_result override
		{
			++Calls;
			return { };
		}
	};

	/// Exactly the expected files are gone, every other file is byte-identical.
	void expect_exact(const std::map<std::string, std::string>& aBefore, const fs::path& aRoot, const std::set<std::string>& aRemoved, std::string_view aWhat)
	{
		const auto after = snapshot(aRoot);
		for (const auto& [path, bytes] : aBefore) {
			const auto found = after.find(path);
			if (aRemoved.contains(path)) {
				if (found != after.end()) {
					std::cerr << "still present: " << path << '\n';
				}
				expect(found == after.end(), aWhat);
			} else {
				if (found == after.end() || found->second != bytes) {
					std::cerr << "changed or missing: " << path << '\n';
				}
				expect(found != after.end() && found->second == bytes, aWhat);
			}
		}
		for (const auto& [path, bytes] : after) {
			static_cast<void>(bytes);
			if (!aBefore.contains(path)) {
				std::cerr << "appeared: " << path << '\n';
			}
			expect(aBefore.contains(path), aWhat);
		}
	}

	void test_each_reset_removes_exactly_its_scope()
	{
		for (const auto each : { reset::caches, reset::settings, reset::review_data, reset::everything }) {
			const auto  name = core::profile_data_reset_name(each);
			scratch     root{ name };
			credentials vault;
			populate(root.Path);
			const auto before = snapshot(root.Path);
			{
				adapters::sqlite::release_maintenance_files files{ root.Path, lease(root.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U }, &vault };
				const auto                                  removed = files.reset(each, std::nullopt, { });
				expect(removed.has_value(), "the reset completes");
				expect(*removed >= expected_removed(each).size(), "the count covers every removed file");
			}
			expect_exact(before, root.Path, expected_removed(each), "the reset removed exactly its scope");
			expect(vault.Calls == 0, "no reset reads or deletes a credential");
			expect(!fs::exists(root.Path / ".bpr-maintenance-v1"), "the journal is removed last");
			expect(fs::exists(root.Path / "state/v1"), "the state folder stays while kept stores remain");
			if (each == reset::caches || each == reset::everything) {
				expect(!fs::exists(root.Path / "repositories") && !fs::exists(root.Path / "mcp-snapshot"), "emptied cache folders go too");
			}
		}
	}

	void test_negative_control_another_scope_keeps_this_one()
	{
		// The same populated root, reset for settings only: every cache and
		// review file must survive. Run against the caches scope this check
		// fails, which is what the first course asserts.
		scratch root{ "negative" };
		populate(root.Path);
		const auto before = snapshot(root.Path);
		{
			adapters::sqlite::release_maintenance_files files{ root.Path, lease(root.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U } };
			expect(files.reset(reset::settings, std::nullopt, { }).has_value(), "settings reset completes");
		}
		for (const auto& file : cache_files()) {
			expect(fs::exists(root.Path / file), "Reset settings keeps every cache");
		}
		for (const auto& file : saved_source_files()) {
			expect(fs::exists(root.Path / file), "Reset settings keeps saved repositories and observer rules");
		}
		expect_exact(before, root.Path, settings_files(), "only settings went");
	}

	void test_refusal_removes_nothing()
	{
		{
			scratch root{ "store-folder" };
			populate(root.Path);
			fs::remove(root.Path / "state/v1/review-tours.sqlite3");
			fs::create_directory(root.Path / "state/v1/review-tours.sqlite3");
			fs::permissions(root.Path / "state/v1/review-tours.sqlite3", fs::perms::owner_all, fs::perm_options::replace);
			const auto                                  before = snapshot(root.Path);
			adapters::sqlite::release_maintenance_files files{ root.Path, lease(root.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U } };
			const auto                                  refused = files.reset(reset::review_data, std::nullopt, { });
			expect(!refused && refused.error() == reason::incomplete_inventory, "a folder where a store belongs refuses");
			expect_exact(before, root.Path, { }, "a refused reset removes nothing");
		}
#if !defined(_WIN32)
		{
			scratch root{ "link" };
			populate(root.Path);
			fs::create_symlink(root.Path / "logs", root.Path / "incoming/v1/link-out");
			const auto                                  before = snapshot(root.Path);
			adapters::sqlite::release_maintenance_files files{ root.Path, lease(root.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U } };
			const auto                                  refused = files.reset(reset::caches, std::nullopt, { });
			expect(!refused, "a link inside a cache folder refuses rather than following it");
			expect_exact(before, root.Path, { }, "a refused cache reset removes nothing");
		}
#endif
	}

#if !defined(_WIN32)
	void test_interrupted_reset_is_finished_by_recovery()
	{
		scratch root{ "interrupted" };
		populate(root.Path);
		const auto before = snapshot(root.Path);
		const auto pack   = root.Path / "repositories/v1" / (std::string{ cache_hash } + ".git") / "objects/pack";
		{
			adapters::sqlite::release_maintenance_files files{ root.Path, lease(root.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U } };
			// The pack folder refuses the unlink inside it, as a crash would stop
			// the deletion part-way: after the journal, before the last file.
			fs::permissions(pack, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);
			const auto stopped = files.reset(reset::caches, std::nullopt, { });
			expect(!stopped && stopped.error() == reason::recovery_required, "a deletion stopped part-way needs recovery");
			expect(fs::exists(root.Path / ".bpr-maintenance-v1"), "its journal stays for the next start");
			const auto again = files.reset(reset::settings, std::nullopt, { });
			expect(!again && again.error() == reason::recovery_required, "no other reset runs over an unfinished one");
		}
		fs::permissions(pack, fs::perms::owner_all, fs::perm_options::replace);
		{
			adapters::sqlite::release_maintenance_files files{ root.Path, lease(root.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U } };
			expect(files.recover().has_value(), "the next start finishes the recorded reset");
		}
		expect(!fs::exists(root.Path / ".bpr-maintenance-v1"), "recovery removes the journal");
		expect_exact(before, root.Path, expected_removed(reset::caches), "recovery removed exactly the interrupted scope");
	}
#endif

	void test_copy_holds_the_removed_stores()
	{
		scratch root{ "copy" };
		scratch destination{ "copy-out" };
		populate(root.Path);
		const auto before = snapshot(root.Path);
		const auto folder = destination.Path / "BetterPullReview reset 2026-10-18 14.02.33";
		{
			adapters::sqlite::release_maintenance_files files{ root.Path, lease(root.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U } };
			expect(files.reset(reset::review_data, folder, { }).has_value(), "the reset with a copy completes");
		}
		expect_exact(before, root.Path, expected_removed(reset::review_data), "the copy changes nothing the reset keeps");
		std::set<std::string> copied;
		for (const auto& item : fs::directory_iterator{ folder }) {
			copied.insert(item.path().filename().string());
			expect(read(item.path()) == before.at("state/v1/" + item.path().filename().string()), "each copy holds the removed bytes");
		}
		expect(copied == std::set<std::string>{ "review-ledger.sqlite3", "review-ledger.sqlite3-wal", "review-tours.sqlite3", "review-cycles.sqlite3", "review-mutation-recovery.sqlite3" },
			   "the copy holds exactly the removed review stores");
#if !defined(_WIN32)
		expect((fs::status(folder).permissions() & (fs::perms::group_all | fs::perms::others_all)) == fs::perms::none, "the copy is private");
#endif
		// A second reset may not reuse that folder; it refuses and removes nothing.
		scratch again{ "copy-again" };
		populate(again.Path);
		const auto second = snapshot(again.Path);
		{
			adapters::sqlite::release_maintenance_files files{ again.Path, lease(again.Path), core::bpr_profile_id{ 7U }, core::application_session_id{ 9U } };
			const auto                                  refused = files.reset(reset::everything, folder, { });
			expect(!refused, "an existing copy folder is never overwritten");
		}
		expect_exact(second, again.Path, { }, "a refused copy removes nothing");
		expect(fs::exists(folder / "review-ledger.sqlite3"), "the earlier copy is untouched");
	}
}

auto main() -> int
{
	test_each_reset_removes_exactly_its_scope();
	test_negative_control_another_scope_keeps_this_one();
	test_refusal_removes_nothing();
#if !defined(_WIN32)
	test_interrupted_reset_is_finished_by_recovery();
#endif
	test_copy_holds_the_removed_stores();
	std::cout << "sqlite_profile_data_reset_files passed\n";
	return EXIT_SUCCESS;
}
