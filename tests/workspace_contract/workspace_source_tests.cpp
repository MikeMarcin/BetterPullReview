#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/core/review_cycle.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using application::workspace_entry_kind;
	using application::workspace_failure;
	using tests::expect;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::text;
	using tests::workspace_contract::try_symlink;
	using tests::workspace_contract::write_file;

	template <typename T>
	auto refused(const std::expected<T, workspace_failure>& aResult, const workspace_failure aFailure) -> bool
	{
		return !aResult.has_value() && aResult.error() == aFailure;
	}

	auto open(application::workspace_source_port& aSource, const std::filesystem::path& aPath) -> std::unique_ptr<application::workspace_root_session>
	{
		auto session = aSource.open(aPath.string(), { });
		expect(session.has_value(), "open root");
		return std::move(*session);
	}

	auto read_text(application::workspace_root_session& aSession, const std::string& aPath) -> std::expected<std::string, workspace_failure>
	{
		auto       bytes    = std::string{ };
		const auto observed = aSession.read(aPath, 1U << 20U, [&bytes](const std::span<const std::uint8_t> aChunk) {
			bytes.append(aChunk.begin(), aChunk.end());
			return true;
		},
											{ });
		if (!observed) {
			return std::unexpected(observed.error());
		}
		return bytes;
	}

	/// A path from UTF-8 bytes on every platform.
	auto utf8_path(const std::string& aBytes) -> std::filesystem::path
	{
		return std::filesystem::path{ std::u8string{ aBytes.begin(), aBytes.end() } };
	}

	auto names(const std::vector<application::workspace_entry_observation>& aEntries) -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (const auto& entry : aEntries) {
			result.push_back(entry.Name);
		}
		return result;
	}

	void opens_folders_and_single_files(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto folder = aScratch / "plain";
		write_file(folder / "a.txt", "alpha\n");
		write_file(folder / "sub" / "b.txt", "beta");
		auto session = open(*source, folder);
		expect(session->observation().Kind == core::workspace_root_kind::folder && !session->observation().Repository, "plain folder");
		expect(core::valid(session->observation()), "valid folder observation");
		const auto listed = session->list("", { });
		expect(listed && names(*listed) == std::vector<std::string>{ "a.txt", "sub" }, "sorted root listing");
		expect((*listed)[1].Kind == workspace_entry_kind::directory, "directory entry");
		expect(read_text(*session, "sub/b.txt") == "beta", "descendant read");
		expect(refused(session->read_git(application::workspace_git_area::worktree, "HEAD", 64, { }), workspace_failure::unsupported), "no Git metadata for a folder");

		auto file = open(*source, folder / "a.txt");
		expect(file->observation().Kind == core::workspace_root_kind::file, "single file");
		expect(file->observation().Root == session->observation().Root, "a file root is identified by its directory");
		const auto only = file->list("", { });
		expect(only && only->size() == 1 && only->front().Name == "a.txt", "a file root lists itself");
		expect(read_text(*file, "a.txt") == "alpha\n", "file root read");
		expect(refused(file->stat("sub/b.txt", { }), workspace_failure::missing), "nothing else is in a file root");
		expect(refused(file->list("sub", { }), workspace_failure::missing), "no directories in a file root");

		// An editor that saves by replacing the file does not replace the root.
		std::filesystem::rename(folder / "a.txt", folder / "a.old");
		write_file(folder / "a.txt", "alpha 2\n");
		expect(file->verify({ }) == core::workspace_verification::matches, "file replacement keeps the root");
		expect(read_text(*file, "a.txt") == "alpha 2\n", "reads what the path names now");
		expect(refused(source->open((folder / "absent").string(), { }), workspace_failure::missing), "missing location");
	}

	void opens_git_checkouts_and_linked_worktrees(const std::filesystem::path& aScratch, const tests::workspace_contract::git_runner& aGit)
	{
		auto source = adapters::workspace::make_local_workspace_source();

		// A repository with no commit and only uncommitted content.
		const auto fresh = aScratch / "fresh";
		std::filesystem::create_directories(fresh);
		aGit(fresh, { "init", "--quiet" });
		write_file(fresh / "draft.txt", "uncommitted\n");
		auto uncommitted = open(*source, fresh);
		expect(uncommitted->observation().Kind == core::workspace_root_kind::git_checkout, "Git checkout without a commit");
		expect(uncommitted->observation().Repository == uncommitted->observation().Worktree, "main checkout: one Git directory");
		const auto listed = uncommitted->list("", { });
		expect(listed && names(*listed) == std::vector<std::string>{ "draft.txt" }, ".git is never listed");
		expect(read_text(*uncommitted, "draft.txt") == "uncommitted\n", "uncommitted bytes readable");
		expect(uncommitted->read_git(application::workspace_git_area::worktree, "HEAD", 4096, { }).has_value(), "HEAD readable as metadata");
		expect(refused(uncommitted->stat(".git/config", { }), workspace_failure::unsafe_path), ".git is not material");

		// A linked worktree shares the repository and has its own Git directory.
		const auto main = aScratch / "main";
		std::filesystem::create_directories(main);
		aGit(main, { "init", "--quiet" });
		write_file(main / "tracked.txt", "tracked\n");
		aGit(main, { "add", "tracked.txt" });
		aGit(main, { "commit", "--quiet", "--no-verify", "-m", "fixture" });
		aGit(main, { "worktree", "add", "--quiet", "-b", "linked", (aScratch / "linked").string() });
		write_file(aScratch / "linked" / "tracked.txt", "edited in the worktree\n");
		auto        mainSession   = open(*source, main);
		auto        linkedSession = open(*source, aScratch / "linked");
		const auto& linked        = linkedSession->observation();
		expect(linked.Kind == core::workspace_root_kind::git_linked_worktree && core::valid(linked), "linked worktree");
		expect(linked.Repository == mainSession->observation().Repository, "same repository");
		expect(linked.Worktree != mainSession->observation().Worktree, "own worktree directory");
		expect(linkedSession->read_git(application::workspace_git_area::worktree, "HEAD", 4096, { }).has_value(), "worktree HEAD");
		expect(linkedSession->read_git(application::workspace_git_area::common, "config", 1U << 20U, { }).has_value(), "shared config");
		expect(refused(linkedSession->read_git(application::workspace_git_area::common, "../escape", 64, { }), workspace_failure::unsafe_path), "metadata paths admitted");
		expect(read_text(*linkedSession, "tracked.txt") == "edited in the worktree\n", "dirty worktree bytes");
		std::cout << "NATIVE: git checkout without commit, dirty checkout and linked worktree opened\n";
	}

	void refuses_escapes_and_replaced_roots(const std::filesystem::path& aScratch)
	{
		auto       source  = adapters::workspace::make_local_workspace_source();
		const auto outside = aScratch / "outside";
		const auto root    = aScratch / "guarded";
		write_file(outside / "secret.txt", "secret");
		write_file(root / "inside.txt", "inside");
		write_file(root / "sub" / "file.txt", "sub file");
		const auto links   = try_symlink(outside, root / "escape_dir", true) && try_symlink(outside / "secret.txt", root / "escape_file", false);
		auto       session = open(*source, root);

		for (const auto* path : { "../outside/secret.txt", "/etc/passwd", "a//b", "./inside.txt", "sub/../inside.txt", ".git/config", "C:/Windows" }) {
			expect(refused(session->stat(path, { }), workspace_failure::unsafe_path), "unsafe descendant path refused");
			expect(refused(read_text(*session, path), workspace_failure::unsafe_path), "unsafe read refused");
		}
		if (links) {
			const auto listed = session->list("", { });
			expect(listed.has_value(), "list with links");
			const auto kinds = [&](const std::string& aName) {
				const auto found = std::ranges::find(*listed, aName, &application::workspace_entry_observation::Name);
				expect(found != listed->end(), "link listed");
				return found == listed->end() ? workspace_entry_kind::other : found->Kind;
			};
			expect(kinds("escape_dir") == workspace_entry_kind::symlink && kinds("escape_file") == workspace_entry_kind::symlink, "links listed as links");
			expect(refused(session->list("escape_dir", { }), workspace_failure::escape), "no listing through a directory link");
			expect(refused(session->stat("escape_dir/secret.txt", { }), workspace_failure::escape), "no stat through a directory link");
			expect(refused(read_text(*session, "escape_dir/secret.txt"), workspace_failure::escape), "no read through a directory link");
			expect(refused(read_text(*session, "escape_file"), workspace_failure::escape), "a file link is not followed");
#if !defined(_WIN32)
			const auto target = session->read_link("escape_file", { });
			expect(target && *target == (outside / "secret.txt").string(), "link target kept as data");
#endif

			// A directory replaced by a link after it was first seen.
			expect(read_text(*session, "sub/file.txt") == "sub file", "before the swap");
			std::filesystem::rename(root / "sub", aScratch / "sub-moved");
			expect(try_symlink(outside, root / "sub", true), "swap in a link");
			expect(refused(read_text(*session, "sub/secret.txt"), workspace_failure::escape), "a swapped-in link is refused");
			std::cout << "NATIVE: symbolic-link escapes refused\n";
		} else {
			std::cout << "NATIVE: symbolic links unavailable to this account; link escape rows not run\n";
		}

		// The root replaced at its location: verify says so, and the session
		// keeps reading the root that was opened, not the newcomer.
		const auto replaced = aScratch / "replaced";
		write_file(replaced / "a.txt", "original");
		auto held = open(*source, replaced);
#if defined(_WIN32)
		// Windows refuses to rename a directory another handle holds open
		// without delete sharing: the held root cannot be replaced at all.
		auto error = std::error_code{ };
		std::filesystem::rename(replaced, aScratch / "replaced-old", error);
		expect(static_cast<bool>(error), "held root cannot be renamed away");
		std::cout << "NATIVE: held root pinned against rename\n";
#else
		std::filesystem::rename(replaced, aScratch / "replaced-old");
		write_file(replaced / "a.txt", "impostor");
		expect(held->verify({ }) == core::workspace_verification::replaced, "replacement detected");
		expect(read_text(*held, "a.txt") == "original", "reads stay beneath the opened root");
		auto       impostor = open(*source, replaced);
		const auto binding  = core::cycle_workspace_binding{ .Project = core::workspace_project_id{ std::array<std::uint8_t, 32>{ 1 } }, .Version = 1, .Root = held->observation() };
		expect(core::verify_workspace(binding, impostor->observation()) == core::workspace_verification::replaced, "the impostor is not the bound root");

		// A moved root keeps its identity at the new location.
		std::filesystem::rename(aScratch / "replaced-old", aScratch / "moved");
		auto moved = open(*source, aScratch / "moved");
		expect(core::verify_workspace(binding, moved->observation()) == core::workspace_verification::matches, "moved root is the same object");
		expect(moved->observation().Location != binding.Root.Location, "at a different location");
		std::cout << "NATIVE: root replacement detected, moved root recognized\n";
#endif
	}

	void keeps_same_named_checkouts_apart(const std::filesystem::path& aScratch)
	{
		auto source = adapters::workspace::make_local_workspace_source();
		write_file(aScratch / "a" / "project" / "same.txt", "identical\n");
		write_file(aScratch / "b" / "project" / "same.txt", "identical\n");
		auto first  = open(*source, aScratch / "a" / "project");
		auto second = open(*source, aScratch / "b" / "project");

		auto       authority = core::local_cycle_authority{ .Profile = core::bpr_profile_id{ 1 }, .Incarnation = core::cycle_catalog_incarnation{ std::array<std::uint8_t, 32>{ 3 } } };
		const auto created   = core::create({ .Authority = authority }, { .Authority = authority, .Title = "Same name", .Scope = { } });
		const auto bound     = core::apply(created->Cycle, { .Subject = created->Cycle.Subject, .ExpectedRevision = 1, .Change = core::bind_cycle_workspace{ .Root = first->observation() } });
		expect(bound.Refusal == core::cycle_refusal::none, "bind the first checkout");
		expect(core::verify_workspace(*bound.Accepted.Workspace, second->observation()) == core::workspace_verification::replaced, "same name, same bytes, different checkout");
		const auto silent = core::apply(bound.Accepted, { .Subject = bound.Accepted.Subject, .ExpectedRevision = 2, .Change = core::bind_cycle_workspace{ .Root = second->observation() } });
		expect(silent.Refusal == core::cycle_refusal::unconfirmed, "the other checkout needs an explicit rebind");
	}

	void reports_native_name_collisions(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "collisions";
		write_file(root / "Readme.md", "one");
		const auto caseInsensitive = std::filesystem::exists(root / "README.md");
		if (!caseInsensitive) {
			write_file(root / "README.md", "two");
		}
		const auto nfc = std::string{ "caf\xC3\xA9.txt" };
		const auto nfd = std::string{ "cafe\xCC\x81.txt" };
		write_file(root / utf8_path(nfc), "composed");
		const auto normalizationInsensitive = std::filesystem::exists(root / utf8_path(nfd));
		if (!normalizationInsensitive) {
			write_file(root / utf8_path(nfd), "decomposed");
		}
		auto       session = open(*source, root);
		const auto listed  = session->list("", { });
		expect(listed.has_value(), "list collision fixture");
		const auto listedNames = names(*listed);
		expect(std::ranges::find(listedNames, nfc) != listedNames.end(), "composed name bytes preserved");
		const auto collisions = core::find_path_collisions(listedNames);
		expect(caseInsensitive || std::ranges::any_of(collisions, [](const auto& aCollision) { return aCollision.Kind == core::workspace_collision_kind::case_fold; }),
			   "both case spellings kept and flagged");
		expect(normalizationInsensitive || std::ranges::any_of(collisions, [](const auto& aCollision) { return aCollision.Kind == core::workspace_collision_kind::unicode_normalization; }),
			   "both normalization spellings kept and flagged");
		expect(listedNames.size() == (caseInsensitive ? 1U : 2U) + (normalizationInsensitive ? 1U : 2U), "no entry combined or dropped");
		std::cout << "NATIVE: case-" << (caseInsensitive ? "insensitive" : "sensitive") << ", normalization-" << (normalizationInsensitive ? "insensitive" : "sensitive")
				  << " volume; " << collisions.size() << " collision(s) flagged\n";
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "workspace-source" };
	opens_folders_and_single_files(scratch.path());
	if (const auto git = tests::workspace_contract::git_from(aArgc, aArgv)) {
		opens_git_checkouts_and_linked_worktrees(scratch.path(), *git);
	} else {
		std::cout << "NATIVE: no Git in this build; Git root rows not run\n";
	}
	refuses_escapes_and_replaced_roots(scratch.path());
	keeps_same_named_checkouts_apart(scratch.path());
	reports_native_name_collisions(scratch.path());
	std::cout << "workspace-source-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
