#include "../git_contract/windows_system_root.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/local_draft_workbench.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0108 AC01-AC04 and AC07 ([[BPR-2019]], [[BPR-2020]]) on
/// real repositories made by the build's Git, read by the product's
/// workspace source, its isolated local-draft Git reader and its SQLite
/// store, through `open_local_draft`, the call the desktop's Prepare pull
/// request and `bpr draft open` both make.
/// \details The primary clone's `origin` names GitHub while its objects came
/// from a local bare repository, as a real clone's do after a fetch. Every
/// case checks that the worktree, its index, HEAD and refs are byte for byte
/// what they were.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	struct fixture
	{
		const git_runner&                                     Git;
		std::filesystem::path                                 Root;
		std::unique_ptr<application::workspace_source_port>   Source;
		std::unique_ptr<application::local_draft_git_port>    Reader;
		std::unique_ptr<application::local_draft_store>       Store;
		std::filesystem::path                                 Profile;
		int                                                   Ids = 0;

		[[nodiscard]] auto rev(const std::filesystem::path& aDirectory, const std::string& aName) const -> std::string
		{
			return Git.output(aDirectory, { "rev-parse", aName }, Root / "rev.txt");
		}

		[[nodiscard]] auto open(const std::filesystem::path& aLocation, const std::optional<std::string>& aContext = std::nullopt)
			-> std::expected<application::local_draft_open_result, application::local_draft_open_error>
		{
			auto id = std::string(32U, '0');
			auto n  = std::to_string(++Ids);
			id.replace(id.size() - n.size(), n.size(), n);
			return application::open_local_draft(*Store, *Source, *Reader, aLocation.string(), aContext, { id }, 1'000 + Ids, { });
		}
	};

	/// What a read-only open must leave exactly as it was.
	struct checkout_state
	{
		std::string Head;
		std::string Index;
		std::string Status;
		std::string Refs;
		auto        operator==(const checkout_state&) const -> bool = default;
	};

	auto state_of(const fixture& aFixture, const std::filesystem::path& aCheckout) -> checkout_state
	{
		const auto gitDirectory = std::filesystem::path{ aFixture.Git.output(aCheckout, { "rev-parse", "--absolute-git-dir" }, aFixture.Root / "dir.txt") };
		const auto common       = std::filesystem::path{ aFixture.Git.output(aCheckout, { "rev-parse", "--path-format=absolute", "--git-common-dir" }, aFixture.Root / "common.txt") };
		return { .Head   = read_file(gitDirectory / "HEAD"),
				 .Index  = read_file(gitDirectory / "index"),
				 .Status = aFixture.Git.output(aCheckout, { "--no-optional-locks", "status", "--porcelain=v2", "--untracked-files=all" }, aFixture.Root / "status.txt"),
				 .Refs   = aFixture.Git.output(common, { "for-each-ref", "--format=%(refname) %(objectname)" }, aFixture.Root / "refs.txt") };
	}

	auto commit(const git_runner& aGit, const std::filesystem::path& aCheckout, const std::string& aPath, const std::string& aBytes, const std::string& aMessage)
	{
		write_file(aCheckout / aPath, aBytes);
		aGit(aCheckout, { "add", "-A" });
		aGit(aCheckout, { "commit", "--quiet", "-m", aMessage });
	}

	auto paths(const std::vector<core::draft_path_change>& aChanges) -> std::vector<std::string>
	{
		auto names = std::vector<std::string>{ };
		for (const auto& change : aChanges) {
			names.push_back(change.Path);
		}
		return names;
	}

	/// A hosted repository as a local bare one, and a primary clone of it
	/// whose origin names GitHub, with main moved after `feature` branched.
	auto make_primary(fixture& aFixture, const std::string& aName) -> std::filesystem::path
	{
		const auto& git     = aFixture.Git;
		const auto  seed    = aFixture.Root / (aName + "-seed");
		const auto  hosted  = aFixture.Root / (aName + "-hosted.git");
		const auto  primary = aFixture.Root / aName;
		std::filesystem::create_directories(seed);
		git(seed, { "init", "--quiet" });
		commit(git, seed, "README.md", "engine\n", "initial");
		commit(git, seed, "src/engine.cpp", "int engine() { return 1; }\n", "engine");
		git(aFixture.Root, { "clone", "--quiet", "--bare", seed.string(), hosted.string() });
		git(aFixture.Root, { "clone", "--quiet", hosted.string(), primary.string() });
		git(primary, { "remote", "set-url", "origin", "https://github.com/acme/engine.git" });
		// Main moves on after the branch point; the pull request must not show it.
		commit(git, seed, "docs/main-only.md", "main moved\n", "main moves");
		git(seed, { "push", "--quiet", hosted.string(), "main" });
		git(primary, { "fetch", "--quiet", hosted.string(), "+refs/heads/main:refs/remotes/origin/main" });
		git(primary, { "checkout", "--quiet", "-b", "feature/search-results" });
		commit(git, primary, "src/search.cpp", "int search() { return 2; }\n", "search");
		commit(git, primary, "src/engine.cpp", "int engine() { return 3; }\n", "engine uses search");
		// The branch tracks a branch of its own name: never the target.
		git(primary, { "config", "branch.feature/search-results.remote", "origin" });
		git(primary, { "config", "branch.feature/search-results.merge", "refs/heads/feature/search-results" });
		return primary;
	}

	void test_primary_clone(fixture& aFixture)
	{
		const auto primary = make_primary(aFixture, "primary");
		const auto before  = state_of(aFixture, primary);
		auto       opened  = aFixture.open(primary);
		expect(opened.has_value() && opened->Outcome == application::local_draft_open_outcome::created && opened->Draft, "AC01: a clean worktree opens as a new local draft");
		const auto& draft = *opened->Draft;
		const auto& first = draft.Candidates.front();
		expect(draft.Branch == "feature/search-results" && draft.Target.Target.Repository.Owner == "acme" && draft.Target.Target.Repository.Name == "engine"
				   && draft.Target.Target.Branch == "main" && draft.Target.Origin == core::target_origin::remote_default,
			   "AC01: source, hosted target and default branch; the tracking branch is not the target");
		expect(std::string{ core::canonical_hex(first.Base).view() } == aFixture.rev(primary, "main") && std::string{ core::canonical_hex(first.TargetTip).view() } == aFixture.rev(primary, "origin/main")
				   && first.Base != first.TargetTip,
			   "AC01: the comparison base is the merge base, recorded apart from the target tip");
		expect(paths(first.Changes) == std::vector<std::string>{ "src/engine.cpp", "src/search.cpp" }, "AC01: only the branch's changes, not what main gained");
		expect(state_of(aFixture, primary) == before, "AC01: opening changed nothing in the checkout");

		// The material is kept: both sides of each change, readable without Git.
		const auto engine = std::ranges::find(first.Changes, "src/engine.cpp", &core::draft_path_change::Path);
		const auto left   = aFixture.Store->object(engine->Left->Object);
		const auto right  = aFixture.Store->object(engine->Right->Object);
		expect(left && *left && right && *right && tests::workspace_contract::text(**left) == "int engine() { return 1; }\n"
				   && tests::workspace_contract::text(**right) == "int engine() { return 3; }\n",
			   "AC01: the candidate keeps exact bytes of both sides");

		// AC02: dirty and untracked material stays out until selected.
		write_file(primary / "src/search.cpp", "int search() { return 4; }\n");
		write_file(primary / "notes/plan.md", "untracked plan\n");
		write_file(primary / "build/out.o", "ignored\n");
		write_file(primary / ".git/info/exclude", "build/\n");
		auto again = aFixture.open(primary, std::string{ "Context from the harness." });
		expect(again && again->Outcome == application::local_draft_open_outcome::resumed && again->Draft->Id == draft.Id, "AC05: the same worktree resumes, never a second draft");
		expect(again->Draft->Notes.size() == 1U && again->Draft->Notes[0].Origin == core::note_origin::harness, "AC05: harness context becomes a note");
		const auto& working = again->Discovery.Working;
		expect(working.size() == 2U && working[0].Path == "notes/plan.md" && working[0].Edit == core::working_edit::untracked && working[1].Path == "src/search.cpp"
				   && working[1].Edit == core::working_edit::modified,
			   "AC02: working edits are listed apart; an ignored file is not");
		expect(again->Draft->Candidates.size() == 1U && again->Draft->Candidates[0].Included.empty(), "AC02: nothing unselected entered");

		const auto selected = std::vector<std::string>{ "src/search.cpp" };
		auto       capture  = application::capture_local_draft(*aFixture.Source, *aFixture.Reader, again->Discovery, selected, 2'000, { });
		expect(capture.has_value(), "AC02: the selected file is captured");
		const auto search = std::ranges::find(capture->Candidate.Changes, "src/search.cpp", &core::draft_path_change::Path);
		expect(search->Right->Origin == core::draft_material_origin::working && search->Change == core::draft_change::added
				   && std::ranges::find(capture->Candidate.Qualifications, core::candidate_qualification::includes_working_files) != capture->Candidate.Qualifications.end(),
			   "AC02: the candidate holds the working bytes and says it is not the branch head");
		expect(std::ranges::find(capture->Candidate.Changes, "notes/plan.md", &core::draft_path_change::Path) == capture->Candidate.Changes.end(), "AC02: the untracked file was not selected");
		const auto kept = std::ranges::find(capture->Objects, search->Right->Object, &application::local_draft_object::Object);
		expect(kept != capture->Objects.end() && tests::workspace_contract::text(kept->Bytes) == "int search() { return 4; }\n", "AC02: exact bytes");

		auto next = core::add_candidate(*again->Draft, again->Draft->Version, capture->Candidate, 2'000);
		expect(next && aFixture.Store->put(*next, again->Draft->Version, capture->Objects).has_value(), "AC04: a successor candidate is recorded");
		expect(!aFixture.Store->put(*next, again->Draft->Version, capture->Objects).has_value()
				   && aFixture.Store->put(*next, again->Draft->Version, capture->Objects).error().Failure == application::local_draft_store_failure::stale,
			   "AC04: a competing completion against the older version is refused");

		// An external edit after capture: the candidate is unchanged.
		write_file(primary / "src/search.cpp", "int search() { return 5; }\n");
		auto stored = aFixture.Store->find(draft.Id);
		expect(stored && stored->Candidates.size() == 2U && stored->Candidates[0] == first, "AC04: the earlier candidate is retained");
		const auto edited = std::ranges::find(stored->Candidates[1].Changes, "src/search.cpp", &core::draft_path_change::Path);
		const auto bytes  = aFixture.Store->object(edited->Right->Object);
		expect(bytes && *bytes && tests::workspace_contract::text(**bytes) == "int search() { return 4; }\n", "AC04: an external edit does not move captured material");
		auto stale = application::capture_local_draft(*aFixture.Source, *aFixture.Reader, again->Discovery, selected, 2'100, { });
		expect(!stale && stale.error().Failure == application::local_draft_capture_failure::changed, "AC04: bytes that changed since listing are not captured");

		// Restart: a new store over the same profile reads everything back.
		aFixture.Store = adapters::sqlite::open_local_draft_store(aFixture.Profile);
		auto restarted = aFixture.open(primary);
		expect(restarted && restarted->Outcome == application::local_draft_open_outcome::resumed && restarted->Draft->Candidates.size() == 2U && restarted->Draft->Notes.size() == 1U,
			   "AC04: notes and candidates survive a restart");

		// A branch switch: the draft is not retargeted.
		// The working edits are set aside by hand (never `git stash`).
		aFixture.Git(primary, { "checkout", "--quiet", "--", "src/search.cpp" });
		std::filesystem::remove_all(primary / "notes");
		aFixture.Git(primary, { "checkout", "--quiet", "-b", "other-work", "main" });
		commit(aFixture.Git, primary, "other.txt", "other\n", "other");
		auto switched = aFixture.open(primary);
		expect(switched && switched->Outcome == application::local_draft_open_outcome::created && switched->Draft->Id != draft.Id && switched->Draft->Branch == "other-work",
			   "AC04: another branch is another draft");
		auto original = aFixture.Store->find(draft.Id);
		expect(original && original->Branch == "feature/search-results" && original->Candidates.size() == 2U, "AC04: the first draft is untouched by the switch");
		aFixture.Git(primary, { "checkout", "--quiet", "feature/search-results" });
		write_file(primary / "src/search.cpp", "int search() { return 5; }\n");
		write_file(primary / "notes/plan.md", "untracked plan\n");

		// A moved folder asks for an explicit rebind.
		const auto moved = aFixture.Root / "primary-moved";
		std::filesystem::rename(primary, moved);
		auto movedOpen = aFixture.open(moved);
		expect(movedOpen && movedOpen->Outcome == application::local_draft_open_outcome::moved && movedOpen->Draft->Id == draft.Id, "AC04: a moved folder is offered for rebind, not taken");
		expect(aFixture.Store->find(draft.Id)->Worktree.Location == primary.string(), "AC04: nothing was rebound silently");

		// Path reuse: another repository at the old path is not the draft.
		std::filesystem::create_directories(primary);
		aFixture.Git(primary, { "init", "--quiet" });
		aFixture.Git(primary, { "checkout", "--quiet", "-b", "feature/search-results" });
		commit(aFixture.Git, primary, "x.txt", "x\n", "x");
		aFixture.Git(primary, { "remote", "add", "origin", "https://github.com/acme/engine.git" });
		auto reused = aFixture.open(primary);
		expect(reused && reused->Outcome != application::local_draft_open_outcome::resumed && reused->Outcome != application::local_draft_open_outcome::moved,
			   "AC04: a reused path does not resume the earlier draft");
		std::filesystem::remove_all(primary);
		std::filesystem::rename(moved, primary);

		// AC07: discarding removes only the private preparation.
		const auto worktreeBefore = state_of(aFixture, primary);
		expect(aFixture.Store->remove(draft.Id).has_value(), "AC07: discard");
		expect(!aFixture.Store->find(draft.Id).has_value() && !aFixture.Store->object(edited->Right->Object).value().has_value(), "AC07: the draft and its material are gone");
		expect(state_of(aFixture, primary) == worktreeBefore && aFixture.rev(primary, "feature/search-results").size() == 40U, "AC07: the worktree and its branch are untouched");
	}

	void test_isolated_clones(fixture& aFixture)
	{
		const auto primary = make_primary(aFixture, "upstream-primary");
		aFixture.Git(primary, { "checkout", "--quiet", "main" });
		// The usual isolated worktree: a shared local clone whose origin is
		// the primary clone; its own origin/main is the primary's main, which
		// is behind the hosted tip.
		const auto isolated = aFixture.Root / "isolated";
		aFixture.Git(aFixture.Root, { "clone", "--quiet", "--shared", primary.string(), isolated.string() });
		aFixture.Git(isolated, { "checkout", "--quiet", "-b", "isolated-change" });
		commit(aFixture.Git, isolated, "src/isolated.cpp", "int isolated();\n", "isolated");
		const auto before = state_of(aFixture, isolated);
		auto       opened = aFixture.open(isolated);
		expect(opened && opened->Outcome == application::local_draft_open_outcome::created, "AC01: an isolated local clone opens");
		expect(opened->Discovery.Chain && opened->Discovery.Chain->Hops.size() == 1U && opened->Draft->Chain.size() == 1U
				   && opened->Draft->Target.Target.Repository == core::remote_location{ .Host = "github.com", .Owner = "acme", .Name = "engine" },
			   "AC03: the chain resolves to the hosted remote through the primary clone");
		expect(std::string{ core::canonical_hex(opened->Draft->Candidates[0].TargetTip).view() } == aFixture.rev(primary, "origin/main"),
			   "AC03: the target tip is the hosted remote's, not the primary's local main");
		expect(paths(opened->Draft->Candidates[0].Changes) == std::vector<std::string>{ "src/isolated.cpp" }, "AC03: correctly based");
		expect(state_of(aFixture, isolated) == before, "AC03: following the chain fetches nothing and changes nothing");

		// A plain local clone that does not hold the hosted tip's objects
		// reads them through the chain.
		const auto copied = aFixture.Root / "copied";
		aFixture.Git(aFixture.Root, { "clone", "--quiet", "--no-local", primary.string(), copied.string() });
		aFixture.Git(copied, { "checkout", "--quiet", "-b", "copied-change" });
		commit(aFixture.Git, copied, "src/copied.cpp", "int copied();\n", "copied");
		auto copiedOpen = aFixture.open(copied);
		expect(copiedOpen && copiedOpen->Outcome == application::local_draft_open_outcome::created
				   && paths(copiedOpen->Draft->Candidates[0].Changes) == std::vector<std::string>{ "src/copied.cpp" },
			   "AC03: a hosted tip only the primary holds is read through the chain");

		// A linked worktree of the primary clone.
		const auto linked = aFixture.Root / "linked";
		aFixture.Git(primary, { "worktree", "add", "--quiet", "-b", "linked-change", linked.string(), "feature/search-results" });
		commit(aFixture.Git, linked, "src/linked.cpp", "int linked();\n", "linked");
		auto linkedOpen = aFixture.open(linked);
		expect(linkedOpen && linkedOpen->Outcome == application::local_draft_open_outcome::created
				   && paths(linkedOpen->Draft->Candidates[0].Changes) == std::vector<std::string>{ "src/engine.cpp", "src/linked.cpp", "src/search.cpp" },
			   "AC01: a linked worktree opens with its own branch's comparison");

		// A cycle of local remotes is refused, not followed forever.
		const auto loopA = aFixture.Root / "loop-a";
		const auto loopB = aFixture.Root / "loop-b";
		aFixture.Git(aFixture.Root, { "clone", "--quiet", "--shared", primary.string(), loopA.string() });
		aFixture.Git(aFixture.Root, { "clone", "--quiet", "--shared", loopA.string(), loopB.string() });
		aFixture.Git(loopA, { "remote", "set-url", "origin", loopB.string() });
		aFixture.Git(loopB, { "checkout", "--quiet", "-b", "loop" });
		commit(aFixture.Git, loopB, "loop.txt", "loop\n", "loop");
		auto loop = aFixture.open(loopB);
		expect(loop && loop->Outcome == application::local_draft_open_outcome::recovery && loop->Recovery == application::local_draft_recovery::remote_chain
				   && loop->Discovery.ChainRefusal && loop->Discovery.ChainRefusal->Reason == core::remote_chain_refusal::cycle && !loop->Draft,
			   "AC03: a cycle is a specific recovery and creates nothing");
	}

	void test_ambiguity_and_missing_base(fixture& aFixture)
	{
		const auto primary = make_primary(aFixture, "fork");
		// A fork: origin is the author's, upstream the parent.
		aFixture.Git(primary, { "remote", "set-url", "origin", "https://github.com/me/engine.git" });
		aFixture.Git(primary, { "remote", "add", "upstream", "https://github.com/acme/engine.git" });
		aFixture.Git(primary, { "update-ref", "refs/remotes/upstream/main", "origin/main" });
		aFixture.Git(primary, { "symbolic-ref", "refs/remotes/upstream/HEAD", "refs/remotes/upstream/main" });
		auto opened = aFixture.open(primary);
		expect(opened && opened->Outcome == application::local_draft_open_outcome::choose_target && opened->Discovery.Selection
				   && opened->Discovery.Selection->Question == core::target_question::several_repositories && !opened->Draft,
			   "AC03: a fork and its parent ask which is the destination; origin is not assumed");
		auto drafts = aFixture.Store->list();
		auto chosen = core::choose_target_explicitly(opened->Discovery.Destinations, { .Remote = "upstream", .Repository = { .Host = "github.com", .Owner = "acme", .Name = "engine" }, .Branch = "main" });
		expect(chosen.has_value(), "AC03: the parent is chosen");
		auto retargeted = application::retarget_discovery(*aFixture.Source, *aFixture.Reader, opened->Discovery, *chosen, { });
		expect(retargeted && retargeted->ready() && paths(retargeted->Committed) == std::vector<std::string>{ "src/engine.cpp", "src/search.cpp" }, "AC03: the chosen target compares correctly");

		// A target with no history in common.
		const auto lonely = aFixture.Root / "lonely";
		std::filesystem::create_directories(lonely);
		aFixture.Git(lonely, { "init", "--quiet" });
		commit(aFixture.Git, lonely, "a.txt", "a\n", "a");
		aFixture.Git(lonely, { "checkout", "--quiet", "--orphan", "unrelated" });
		commit(aFixture.Git, lonely, "b.txt", "b\n", "b");
		aFixture.Git(lonely, { "remote", "add", "origin", "https://github.com/acme/lonely.git" });
		aFixture.Git(lonely, { "update-ref", "refs/remotes/origin/main", "main" });
		aFixture.Git(lonely, { "symbolic-ref", "refs/remotes/origin/HEAD", "refs/remotes/origin/main" });
		auto unrelated = aFixture.open(lonely);
		expect(unrelated && unrelated->Recovery == application::local_draft_recovery::no_common_base && !unrelated->Draft, "AC03: no common base is said, not invented");

		// A target whose tip the clone does not hold.
		aFixture.Git(lonely, { "checkout", "--quiet", "-b", "topic", "main" });
		write_file(lonely / ".git/refs/remotes/origin/main", std::string(40U, '7') + "\n");
		auto missing = aFixture.open(lonely);
		expect(missing && missing->Recovery == application::local_draft_recovery::target_unavailable && !missing->Draft, "AC03: a missing target tip is unavailable, never fetched");

		// No default branch known: main is not assumed.
		std::filesystem::remove(lonely / ".git/refs/remotes/origin/HEAD");
		auto noDefault = aFixture.open(lonely);
		expect(noDefault && noDefault->Outcome == application::local_draft_open_outcome::choose_target && noDefault->Discovery.Selection->Question == core::target_question::no_default_branch,
			   "AC03: an unknown default branch is asked");

		// A detached head.
		aFixture.Git(lonely, { "checkout", "--quiet", "--detach", "main" });
		auto detached = aFixture.open(lonely);
		expect(detached && detached->Recovery == application::local_draft_recovery::detached_head && !detached->Draft, "AC03: a detached head is a specific recovery");

		// A plain folder is Review Cycles' material.
		const auto plain = aFixture.Root / "plain";
		write_file(plain / "file.txt", "plain\n");
		auto folder = aFixture.open(plain);
		expect(!folder && folder.error().Failure == application::local_draft_open_failure::not_git, "a folder without Git is directed to Review Cycles");
		expect(!std::filesystem::exists(plain / ".git"), "and is not initialized");
		(void)drafts;
	}

	void test_store_bounds(fixture& aFixture)
	{
		// AC07: a full store refuses a new draft with its reason and evicts nothing.
		auto existing = aFixture.Store->list();
		expect(existing.has_value(), "list");
		const auto keep = existing->front();
		auto       made = std::size_t{ existing->size() };
		for (auto index = 0; made < core::maximum_local_drafts; ++index) {
			auto copy = keep;
			copy.Id   = { std::string(31U, 'e') + std::string(1U, "0123456789abcdef"[index % 16]) };
			copy.Id.Value[30] = "0123456789abcdef"[index / 16];
			copy.Worktree.Location += std::to_string(index);
			if (aFixture.Store->put(copy, std::nullopt, { })) {
				++made;
			}
		}
		const auto primary = make_primary(aFixture, "overflow");
		auto       full    = aFixture.open(primary);
		expect(full && full->Recovery == application::local_draft_recovery::store && full->Store && full->Store->Failure == application::local_draft_store_failure::full
				   && full->Store->Limit == core::maximum_local_drafts && !full->Draft,
			   "AC07: a full store refuses with a reason");
		auto after = aFixture.Store->list();
		expect(after && after->size() == core::maximum_local_drafts && std::ranges::any_of(*after, [&keep](const core::local_draft& aDraft) { return aDraft.Id == keep.Id; }),
			   "AC07: nothing was evicted");
	}
}

auto main(int argc, char** argv) -> int
{
	const auto git = tests::workspace_contract::git_from(argc, argv);
	if (!git) {
		std::cout << "local-draft:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	auto scratch = tests::sqlite_contract::scratch_root{ "local-draft" };
	auto root    = std::filesystem::canonical(scratch.path());
	auto profile = root / "profile";
	expect(adapters::sqlite::create_storage_root_owner_private(profile), "the profile root");
	const auto isolation = root / "local-draft-git";
	expect(adapters::sqlite::create_storage_root_owner_private(isolation), "the reader's private root");
	auto reader = adapters::git::make_installed_git_local_draft_reader({
		.GitExecutable     = std::filesystem::canonical(std::filesystem::path{ argv[2] }),
		.IsolationRoot     = std::filesystem::canonical(isolation),
		.WindowsSystemRoot = tests::git_contract::windows_system_root(),
	});
	expect(reader.has_value(), "installed Git local draft reader");
	auto store = adapters::sqlite::open_local_draft_store(profile);
	expect(store != nullptr, "the local draft store");
	auto fixture = ::fixture{ .Git = *git, .Root = root, .Source = adapters::workspace::make_local_workspace_source(), .Reader = std::move(*reader), .Store = std::move(store), .Profile = profile };
	test_primary_clone(fixture);
	test_isolated_clones(fixture);
	test_ambiguity_and_missing_base(fixture);
	test_store_bounds(fixture);
	std::cout << "local-draft:pass\n";
	return EXIT_SUCCESS;
}
