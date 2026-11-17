#include "../git_contract/windows_system_root.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/local_draft_publishing.hpp"
#include "bpr/application/local_draft_workbench.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0111 AC01-AC05 ([[BPR-2029]], [[BPR-2030]]) on real
/// repositories made by the build's Git, read by the product's workspace
/// source, isolated local-draft Git reader and SQLite store, through the
/// calls Create draft PR… makes. GitHub is a scripted pull request catalog
/// whose branches are the hosted bare repository's own refs: the author's
/// push is a real `git push` into it, and BPR pushes nothing.
/// \details Covers the named push for a missing branch, a different head,
/// working files and an isolated local clone; the exact plan and its
/// digest; one creation with the author's commits unchanged; a lost answer
/// found by reading and one never created; a branch that moved after the
/// preview; an existing pull request associated, never duplicated; and a
/// restart mid-creation. The checkout is byte for byte unchanged by every
/// step BPR takes.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	/// GitHub as the course scripts it: repository `acme/engine`, branches
	/// read from the hosted bare repository, pull requests in memory.
	class scripted_github final : public application::github_publication_port
	{
	public:
		scripted_github(const git_runner& aGit, std::filesystem::path aScratch)
			: Git{ aGit }
			, Scratch{ std::move(aScratch) }
		{
		}

		const git_runner&                                         Git;
		std::filesystem::path                                     Scratch;
		std::filesystem::path                                     Hosted;
		std::vector<application::github_pull_request_observation> Pulls;
		int                                                       Creates = 0;
		/// The next creation reaches GitHub and its answer is lost.
		bool LoseAnswer = false;
		/// The next creation's answer is lost before it reaches GitHub.
		bool                  LoseRequest = false;
		std::function<void()> OnCreate;

		auto head(const std::string& aBranch) const -> std::optional<core::git_commit_oid>
		{
			const auto text = Git.output(Hosted, { "for-each-ref", "--format=%(objectname)", "refs/heads/" + aBranch }, Scratch / "ref.txt");
			return text.empty() ? std::nullopt : core::parse_git_commit_oid(text);
		}

		auto observe_repository(const application::github_publication_target& aTarget, std::stop_token) -> application::github_publication_result<application::github_publication_capability> override
		{
			return application::github_publication_capability{ .Owner = aTarget.Owner, .Repository = aTarget.Repository, .CanPush = true, .DefaultBranch = "main" };
		}
		auto observe_branch(const application::github_publication_target&, const std::string& aBranch, std::stop_token)
			-> application::github_publication_result<std::optional<core::git_commit_oid>> override
		{
			return head(aBranch);
		}
		auto observe_commit(const application::github_publication_target&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto observe_tree(const application::github_publication_target&, const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto observe_blob(const application::github_publication_target&, const core::git_entry_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_blob(const application::github_publication_target&, std::span<const std::uint8_t>, const core::git_entry_oid&, std::stop_token)
			-> application::github_publication_result<void> override
		{
			expect(false, "a local draft never writes a Git object");
			return { };
		}
		auto create_tree(const application::github_publication_target&, const core::git_tree_oid&, const std::vector<core::git_tree_entry>&, const std::vector<std::string>&,
						 const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<void> override
		{
			expect(false, "a local draft never writes a tree");
			return { };
		}
		auto create_commit(const application::github_publication_target&, const core::git_commit_content&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<void> override
		{
			expect(false, "a local draft never writes a commit");
			return { };
		}
		auto create_branch(const application::github_publication_target&, const std::string&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<application::github_branch_creation> override
		{
			expect(false, "a local draft never creates or moves a branch");
			return { };
		}
		auto create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token)
			-> application::github_publication_result<application::github_pull_request_observation> override
		{
			++Creates;
			if (OnCreate) {
				OnCreate();
			}
			if (LoseRequest) {
				LoseRequest = false;
				return std::unexpected{ application::github_publication_error{ .Transmission = application::github_publication_transmission::indeterminate } };
			}
			for (const auto& pull : Pulls) {
				if (pull.State == "open" && pull.HeadRef == aDraft.Head && pull.BaseRef == aDraft.Base) {
					return std::unexpected{ application::github_publication_error{ .Failure = application::github_publication_failure::refused, .HttpStatus = 422U } };
				}
			}
			const auto at = head(aDraft.Head);
			if (!at) {
				return std::unexpected{ application::github_publication_error{ .Failure = application::github_publication_failure::refused, .HttpStatus = 422U } };
			}
			auto pull = application::github_pull_request_observation{ .Number         = 100U + Pulls.size(),
																	  .NodeId         = "PR_" + std::to_string(Pulls.size()),
																	  .Url            = "https://github.com/acme/engine/pull/" + std::to_string(100U + Pulls.size()),
																	  .State          = "open",
																	  .Draft          = aDraft.Draft,
																	  .Title          = aDraft.Title,
																	  .HeadRef        = aDraft.Head,
																	  .HeadOwner      = aTarget.Owner,
																	  .HeadRepository = aTarget.Repository,
																	  .HeadCommit     = *at,
																	  .BaseRef        = aDraft.Base,
																	  .Author         = aTarget.Login };
			Pulls.push_back(pull);
			Bodies[pull.Number] = aDraft.Body;
			if (LoseAnswer) {
				LoseAnswer = false;
				return std::unexpected{ application::github_publication_error{ .Transmission = application::github_publication_transmission::indeterminate } };
			}
			return pull;
		}
		auto find_pull_requests(const application::github_publication_target&, const std::string& aHead, const std::string& aBase, std::stop_token)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
		{
			auto out = std::vector<application::github_pull_request_observation>{ };
			for (const auto& pull : Pulls) {
				if (pull.HeadRef == aHead && pull.BaseRef == aBase) {
					out.push_back(pull);
				}
			}
			return out;
		}
		auto observe_pull_request(const application::github_publication_target&, const std::uint64_t aNumber, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
		{
			for (const auto& pull : Pulls) {
				if (pull.Number == aNumber) {
					return pull;
				}
			}
			return std::optional<application::github_pull_request_observation>{ };
		}

		std::map<std::uint64_t, std::string> Bodies;
	};

	struct fixture
	{
		const git_runner&                                   Git;
		std::filesystem::path                               Root;
		std::unique_ptr<application::workspace_source_port> Source;
		std::unique_ptr<application::local_draft_git_port>  Reader;
		std::unique_ptr<application::local_draft_store>     Store;
		std::shared_ptr<scripted_github>                    GitHub;
		int                                                 Ids = 0;

		[[nodiscard]] auto account() const -> application::github_publication_account
		{
			return { .Provider = GitHub, .Account = core::github_account_id{ 7U }, .Login = "author", .Host = "github.com" };
		}

		[[nodiscard]] auto open(const std::filesystem::path& aLocation) -> core::local_draft
		{
			auto id = std::string(32U, '0');
			auto n  = std::to_string(++Ids);
			id.replace(id.size() - n.size(), n.size(), n);
			auto opened = application::open_local_draft(*Store, *Source, *Reader, aLocation.string(), std::nullopt, { id }, 1'000 + Ids, { });
			expect(opened && opened->Draft, "the worktree opens as a local draft");
			auto draft = *opened->Draft;
			auto saved = core::edit_description(draft, draft.Version, "Search results", "## Why\n\nResults were slow.\n", 1'100);
			expect(saved && Store->put(*saved, draft.Version, { }).has_value(), "the description is saved");
			return *saved;
		}

		/// What the author's own Git leaves on the hosted repository.
		void push(const std::filesystem::path& aCheckout, const std::string& aBranch) const { Git(aCheckout, { "push", "--quiet", GitHub->Hosted.string(), "HEAD:refs/heads/" + aBranch }); }

		[[nodiscard]] auto rev(const std::filesystem::path& aDirectory, const std::string& aName) const -> std::string
		{
			return Git.output(aDirectory, { "rev-parse", aName }, Root / "rev.txt");
		}
	};

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

	void commit(const git_runner& aGit, const std::filesystem::path& aCheckout, const std::string& aPath, const std::string& aBytes, const std::string& aMessage)
	{
		write_file(aCheckout / aPath, aBytes);
		aGit(aCheckout, { "add", "-A" });
		aGit(aCheckout, { "commit", "--quiet", "-m", aMessage });
	}

	/// The hosted repository as a bare one and a primary clone whose origin names GitHub.
	auto make_primary(fixture& aFixture, const std::string& aName, const std::string& aBranch) -> std::filesystem::path
	{
		const auto& git     = aFixture.Git;
		const auto  primary = aFixture.Root / aName;
		git(aFixture.Root, { "clone", "--quiet", aFixture.GitHub->Hosted.string(), primary.string() });
		git(primary, { "remote", "set-url", "origin", "https://github.com/acme/engine.git" });
		git(primary, { "checkout", "--quiet", "-b", aBranch });
		commit(git, primary, "src/search.cpp", "int search() { return 2; }\n", "search");
		return primary;
	}

	template <typename T>
	auto ok(T aResult, const std::string_view aMessage)
	{
		expect(aResult.has_value(), aMessage);
		return std::move(*aResult);
	}

	void test_create_from_pushed_branch(fixture& aFixture)
	{
		const auto primary = make_primary(aFixture, "primary", "feature/search");
		auto       draft   = aFixture.open(primary);
		const auto before  = state_of(aFixture, primary);
		const auto account = aFixture.account();

		auto signedOut = application::check_local_publication(*aFixture.Store, draft, nullptr, false, 1'200, { });
		expect(signedOut.Refusal == core::local_publication_refusal::no_account && signedOut.Branch == core::hosted_branch_state::unobserved && signedOut.Push,
			   "AC01: signed out, preparation stops at the named push");

		auto missing = application::check_local_publication(*aFixture.Store, draft, &account, false, 1'200, { });
		if (missing.Push) {
			std::cout << "LOCAL_PUBLICATION_PUSH " << missing.Push->Command << '\n';
		}
		expect(missing.Branch == core::hosted_branch_state::missing && missing.Refusal == core::local_publication_refusal::branch_missing && missing.Push
				   && missing.Push->Command.starts_with("git -C " + primary.string() + " push https://github.com/acme/engine.git " + aFixture.rev(primary, "HEAD") + ":refs/heads/feature/search"),
			   "AC01: an unpushed branch names the exact push to the hosted repository");
		expect(aFixture.GitHub->Creates == 0 && state_of(aFixture, primary) == before, "AC01: nothing pushed, nothing created, the checkout unchanged");

		// The author pushes with their own Git.
		aFixture.push(primary, "feature/search");
		auto checked = application::check_local_publication(*aFixture.Store, draft, &account, false, 1'300, { });
		expect(checked.Branch == core::hosted_branch_state::matches && !checked.Refusal && !checked.Push && checked.Draft && checked.Draft->Publication,
			   "AC01: the pushed branch is the candidate: a plan is recorded");
		draft              = *checked.Draft;
		const auto  digest = draft.Publication->Journal.Digest;
		const auto& plan   = draft.Publication->Plan;
		expect(plan.Title == "Search results" && plan.Body == "## Why\n\nResults were slow.\n" && plan.Destination.Login == "author" && plan.Destination.BaseBranch == "main",
			   "AC02: the plan binds title, body, destination and account");

		auto wrong = application::create_local_pull_request(*aFixture.Store, draft, account, std::string(64U, '0'), 1'400, { });
		expect(wrong.Refusal == core::local_publication_refusal::stale_plan && aFixture.GitHub->Creates == 0, "AC02: another digest authorizes nothing");
		auto otherAccount    = account;
		otherAccount.Account = core::github_account_id{ 8U };
		auto changed         = application::create_local_pull_request(*aFixture.Store, draft, otherAccount, digest, 1'400, { });
		expect(changed.Refusal == core::local_publication_refusal::authority_changed && aFixture.GitHub->Creates == 0, "AC02: another account authorizes nothing");

		auto created = application::create_local_pull_request(*aFixture.Store, draft, account, digest, 1'500, { });
		expect(created.Draft && created.Draft->Publication && created.Draft->Publication->Journal.Phase == core::local_publication_phase::linked && aFixture.GitHub->Creates == 1,
			   "AC01: one draft pull request is created");
		const auto& pull = aFixture.GitHub->Pulls.back();
		expect(pull.Draft && std::string{ core::canonical_hex(pull.HeadCommit).view() } == aFixture.rev(primary, "HEAD") && aFixture.GitHub->Bodies[pull.Number] == plan.Body,
			   "AC01: a GitHub draft of the author's own commits, with the accepted body");
		expect(created.Draft->Association && created.Draft->Association->Number == pull.Number, "AC05: the draft is associated with exactly that pull request");
		auto stored = aFixture.Store->find(draft.Id);
		expect(stored && stored->Publication == created.Draft->Publication, "the journal is in the store");
		auto again = application::create_local_pull_request(*aFixture.Store, *stored, account, digest, 1'600, { });
		expect(again.Refusal == core::local_publication_refusal::linked && aFixture.GitHub->Creates == 1, "AC04: never a second creation");
		expect(state_of(aFixture, primary) == before, "AC01: checkout data unchanged throughout");
	}

	void test_named_push_cases(fixture& aFixture)
	{
		const auto account = aFixture.account();
		// A branch whose hosted head is elsewhere.
		const auto primary = make_primary(aFixture, "differs", "feature/differs");
		aFixture.push(primary, "feature/differs");
		commit(aFixture.Git, primary, "src/later.cpp", "int later();\n", "later, not pushed");
		auto draft   = aFixture.open(primary);
		auto differs = application::check_local_publication(*aFixture.Store, draft, &account, false, 1'200, { });
		expect(differs.Branch == core::hosted_branch_state::differs && differs.Refusal == core::local_publication_refusal::branch_differs && differs.Push
				   && differs.Push->Command.find(aFixture.rev(primary, "HEAD") + ":refs/heads/feature/differs") != std::string::npos,
			   "AC01: a different hosted head names the push of the candidate");

		// Working files: the candidate is not the branch.
		write_file(primary / "src/dirty.cpp", "int dirty();\n");
		auto opened  = application::open_local_draft(*aFixture.Store, *aFixture.Source, *aFixture.Reader, primary.string(), std::nullopt, { std::string(32U, 'd') }, 1'300, { });
		auto capture = application::capture_local_draft(*aFixture.Source, *aFixture.Reader, opened->Discovery, std::vector<std::string>{ "src/dirty.cpp" }, 1'300, { });
		expect(capture.has_value(), "the dirty file is captured");
		auto dirty = ok(core::add_candidate(*opened->Draft, opened->Draft->Version, capture->Candidate, 1'300), "a dirty candidate");
		expect(aFixture.Store->put(dirty, opened->Draft->Version, capture->Objects).has_value(), "stored");
		auto uncommitted = application::check_local_publication(*aFixture.Store, dirty, &account, false, 1'300, { });
		expect(uncommitted.Branch == core::hosted_branch_state::uncommitted && uncommitted.Refusal == core::local_publication_refusal::uncommitted,
			   "AC03: dirty material is represented honestly and is never published as the branch");
		std::filesystem::remove(primary / "src/dirty.cpp");

		// The usual isolated worktree: a local clone whose origin is the primary.
		const auto isolated = aFixture.Root / "isolated";
		aFixture.Git(aFixture.Root, { "clone", "--quiet", "--shared", primary.string(), isolated.string() });
		aFixture.Git(isolated, { "checkout", "--quiet", "-b", "isolated-change" });
		commit(aFixture.Git, isolated, "src/isolated.cpp", "int isolated();\n", "isolated");
		const auto before        = state_of(aFixture, isolated);
		auto       isolatedDraft = aFixture.open(isolated);
		auto       named         = application::check_local_publication(*aFixture.Store, isolatedDraft, &account, false, 1'400, { });
		expect(named.Branch == core::hosted_branch_state::missing && named.Push && named.Push->Repository == "https://github.com/acme/engine.git"
				   && named.Push->Command.find(" origin ") == std::string::npos && named.Push->Directory == isolated.string(),
			   "AC01: from an isolated clone the push names the hosted repository, never origin, and runs in that worktree");
		expect(state_of(aFixture, isolated) == before, "the isolated checkout is unchanged");
	}

	void test_lost_answers_drift_and_existing(fixture& aFixture)
	{
		const auto account = aFixture.account();
		// Lost after GitHub created it: the read finds it, nothing is sent again.
		const auto lost = make_primary(aFixture, "lost", "feature/lost");
		aFixture.push(lost, "feature/lost");
		auto draft                  = aFixture.open(lost);
		draft                       = *application::check_local_publication(*aFixture.Store, draft, &account, false, 1'200, { }).Draft;
		aFixture.GitHub->LoseAnswer = true;
		const auto creates          = aFixture.GitHub->Creates;
		auto       found            = application::create_local_pull_request(*aFixture.Store, draft, account, draft.Publication->Journal.Digest, 1'300, { });
		expect(found.Draft && found.Draft->Publication->Journal.Phase == core::local_publication_phase::linked && aFixture.GitHub->Creates == creates + 1,
			   "AC04: a lost answer is found by reading, without a second creation");

		// Lost before GitHub had it: uncertain, read, prepared; a new press.
		const auto never = make_primary(aFixture, "never", "feature/never");
		aFixture.push(never, "feature/never");
		auto unsent                  = aFixture.open(never);
		unsent                       = *application::check_local_publication(*aFixture.Store, unsent, &account, false, 1'200, { }).Draft;
		aFixture.GitHub->LoseRequest = true;
		auto read                    = application::create_local_pull_request(*aFixture.Store, unsent, account, unsent.Publication->Journal.Digest, 1'300, { });
		expect(read.Draft && read.Draft->Publication->Journal.Phase == core::local_publication_phase::prepared
				   && read.Draft->Publication->Journal.Create == core::local_step_state::not_sent && !read.Draft->Association,
			   "AC04: nothing found after a lost answer: nothing is pretended and nothing resent");
		auto retried = application::create_local_pull_request(*aFixture.Store, *read.Draft, account, read.Draft->Publication->Journal.Digest, 1'400, { });
		expect(retried.Draft && retried.Draft->Publication->Journal.Phase == core::local_publication_phase::linked, "AC04: the author's new press creates it");

		// Restart mid-creation: uncertain, recovered only by reading.
		const auto restart = make_primary(aFixture, "restart", "feature/restart");
		aFixture.push(restart, "feature/restart");
		auto midway = aFixture.open(restart);
		midway      = *application::check_local_publication(*aFixture.Store, midway, &account, false, 1'200, { }).Draft;
		auto begun  = ok(core::authorize_local_publication(midway, midway.Version, midway.Publication->Journal.Digest, account.Account, 1'300), "confirmed");
		begun       = ok(core::begin_local_creation(begun, begun.Version, begun.Publication->Journal.Generation, account.Account, 1'300), "recorded");
		expect(aFixture.Store->put(begun, midway.Version, { }).has_value(), "the creation is on disk; then the process dies");
		auto reopened = core::restart_local_publication(*aFixture.Store->find(midway.Id));
		expect(reopened.Publication->Journal.Phase == core::local_publication_phase::uncertain, "AC04: after a restart the creation is uncertain");
		expect(aFixture.Store->put(reopened, begun.Version, { }).has_value(), "stored");
		const auto beforeRecover = aFixture.GitHub->Creates;
		auto       recovered     = application::recover_local_publication(*aFixture.Store, reopened, account, 1'400, { });
		expect(recovered.Draft && recovered.Draft->Publication->Journal.Phase == core::local_publication_phase::prepared && aFixture.GitHub->Creates == beforeRecover,
			   "AC04: recovery only reads");

		// The branch moves after the preview: nothing is sent.
		const auto moved = make_primary(aFixture, "moved", "feature/moved");
		aFixture.push(moved, "feature/moved");
		auto drift = aFixture.open(moved);
		drift      = *application::check_local_publication(*aFixture.Store, drift, &account, false, 1'200, { }).Draft;
		commit(aFixture.Git, moved, "src/moved.cpp", "int moved();\n", "pushed after the preview");
		aFixture.push(moved, "feature/moved");
		const auto beforeDrift = aFixture.GitHub->Creates;
		auto       refused     = application::create_local_pull_request(*aFixture.Store, drift, account, drift.Publication->Journal.Digest, 1'300, { });
		expect(refused.Refusal == core::local_publication_refusal::branch_differs && aFixture.GitHub->Creates == beforeDrift && refused.Push,
			   "AC04: concurrent drift sends nothing and names the push again; no force update");

		// An existing pull request from the branch: associated explicitly, never duplicated.
		const auto existing = make_primary(aFixture, "existing", "feature/existing");
		aFixture.push(existing, "feature/existing");
		auto prior = aFixture.GitHub->create_pull_request({ .Host = "github.com", .Account = account.Account, .Login = "author", .Owner = "acme", .Repository = "engine" },
														  { .Title = "Earlier", .Body = "Made elsewhere", .Head = "feature/existing", .Base = "main", .Draft = true }, { });
		expect(prior.has_value(), "a pull request exists already");
		const auto beforeExisting = aFixture.GitHub->Creates;
		auto       adopt          = aFixture.open(existing);
		auto       seen           = application::check_local_publication(*aFixture.Store, adopt, &account, false, 1'200, { });
		expect(seen.Refusal == core::local_publication_refusal::pull_request_exists && seen.Existing && seen.Existing->Number == prior->Number && seen.Draft->Publication,
			   "AC04: the existing pull request is offered for association");
		auto associated = application::associate_local_pull_request(*aFixture.Store, *seen.Draft, account, prior->Number, 1'300, { });
		expect(associated.Draft && associated.Draft->Publication->Journal.Link && associated.Draft->Publication->Journal.Link->Adopted
				   && associated.Draft->Association->Number == prior->Number && aFixture.GitHub->Creates == beforeExisting,
			   "AC04: associated without a second creation; its description is not touched");
		expect(aFixture.GitHub->Bodies[prior->Number] == "Made elsewhere", "AC05: an independently written description is not overwritten");
	}
}

auto main(int argc, char** argv) -> int
{
	const auto git = tests::workspace_contract::git_from(argc, argv);
	if (!git) {
		std::cout << "local-draft-publication:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	auto scratch = tests::sqlite_contract::scratch_root{ "local-draft-publication" };
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
	auto github    = std::make_shared<scripted_github>(*git, root);
	github->Hosted = root / "hosted.git";
	(*git)(root, { "init", "--quiet", "--bare", "-b", "main", github->Hosted.string() });
	const auto seed = root / "seed";
	std::filesystem::create_directories(seed);
	(*git)(seed, { "init", "--quiet", "-b", "main" });
	commit(*git, seed, "README.md", "engine\n", "initial");
	(*git)(seed, { "push", "--quiet", github->Hosted.string(), "main" });
	auto fixture = ::fixture{ .Git = *git, .Root = root, .Source = adapters::workspace::make_local_workspace_source(), .Reader = std::move(*reader), .Store = std::move(store), .GitHub = github };
	test_create_from_pushed_branch(fixture);
	test_named_push_cases(fixture);
	test_lost_answers_drift_and_existing(fixture);
	std::cout << "local-draft-publication:pass\n";
	return EXIT_SUCCESS;
}
