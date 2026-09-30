#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_publisher.hpp"
#include "cycle_course.hpp"
#include "publication_course.hpp"
#include "simulated_github.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Publishing exact Git objects and a new branch through the
/// publication catalog's port (RFC-BPR-0059, BPR-1175): capability is read
/// before any write, each write is journaled first, identities are the ones
/// computed locally, an existing branch is never overwritten or adopted, and
/// an uncertain write is recovered by reading. The provider is an in-memory
/// simulation; the REST contract is `github_publication_contract` and live
/// GitHub is not contacted.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_publish_limit;
	using application::cycle_publish_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::publication_git;
	using tests::workspace_contract::simulated_fault;
	using tests::workspace_contract::simulated_github;
	using tests::workspace_contract::write_file;

	constexpr auto reviewer = core::github_account_id{ 21U };

	auto operation(const std::uint16_t aSeed) -> application::cycle_operation_id
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0x7A;
		bytes[30]  = static_cast<std::uint8_t>(aSeed >> 8U);
		bytes[31]  = static_cast<std::uint8_t>(aSeed & 0xFFU);
		return application::cycle_operation_id{ bytes };
	}

	struct publishing
	{
		application::review_cycle_outgoing_host    Outgoing;
		application::review_cycle_publication_host Journal;
		simulated_github                           Remote;
		std::uint16_t                              Next = 1;

		publishing(application::workspace_source_port& aSource, course& aCycle, publication_git& aGit)
			: Outgoing{ aSource, *aCycle.Store, *aGit.Port }
			, Journal{ Outgoing, *aCycle.Store }
		{
		}

		/// Prepares and authorizes a plan for a new branch.
		auto authorized(course& aCycle, const std::string& aBranch) -> application::stored_cycle_publication
		{
			auto request = application::cycle_publication_request{
				.Candidate   = { .Outgoing  = { .Base = "HEAD", .Target = "refs/heads/main" },
								 .Candidate = { .Metadata = { .Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
															  .Committer = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
															  .Title     = "Harden parsing" } } },
				.Destination = { .Host = "github.com", .Account = reviewer, .Login = "fixture-reviewer", .Owner = "octo-org", .Repository = "review-repository", .BaseBranch = "main", .NewBranch = aBranch },
				.PullTitle   = "Harden parsing",
				.PullBody    = "Reviewed locally.",
			};
			const auto prepared = Journal.prepare(aCycle.state(), operation(Next++), request, { });
			expect(prepared.Publication.has_value(), "plan prepared");
			const auto confirmed = Journal.authorize(aCycle.Subject, operation(Next++), prepared.Publication->Id.Id, prepared.Publication->Revision,
													 { .Actor = core::cycle_actor::reviewer, .PlanDigest = prepared.Publication->Journal.PlanDigest, .Account = reviewer }, { });
			expect(confirmed.has_value(), "plan authorized");
			return confirmed->Publication;
		}

		auto reauthorize(course& aCycle, const application::stored_cycle_publication& aPublication) -> application::stored_cycle_publication
		{
			const auto confirmed = Journal.authorize(aCycle.Subject, operation(Next++), aPublication.Id.Id, aPublication.Revision,
													 { .Actor = core::cycle_actor::reviewer, .PlanDigest = aPublication.Journal.PlanDigest, .Account = reviewer }, { });
			expect(confirmed.has_value(), "fresh authorization");
			return confirmed->Publication;
		}
	};

	auto effect_states(const application::stored_cycle_publication& aPublication) -> std::vector<core::publication_effect_state>
	{
		auto states = std::vector<core::publication_effect_state>{ };
		for (const auto& effect : aPublication.Journal.Effects) {
			states.push_back(effect.State);
		}
		return states;
	}

	void publishes_objects_and_a_branch(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aExecutable };
		const auto root   = aScratch / "project";
		std::filesystem::create_directories(root);
		aGit(root, { "init", "-q" });
		write_file(root / "README.md", "# Parser\n");
		write_file(root / "src" / "parse.cpp", "int parse() { return 0; }\n");
		write_file(root / "src" / "old.cpp", "int old() { return 0; }\n");
		aGit(root, { "add", "." });
		aGit(root, { "commit", "-q", "-m", "base" });
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R0");
		write_file(root / "src" / "parse.cpp", "int parse() { return validate(); }\n");
		write_file(root / "src" / "validate.cpp", "bool validate() { return true; }\n");
		std::filesystem::remove(root / "src" / "old.cpp");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");

		auto       run    = publishing{ *source, cycle, git };
		auto       first  = run.authorized(cycle, "bpr/harden");
		const auto parent = git.Port->list_commit({ .Checkout = root.string(), .Algorithm = core::git_hash_algorithm::sha1 }, first.Plan.Parent, { });
		expect(parent.has_value(), "P listed");
		run.Remote.seed(first.Plan.Parent, parent->Tree, parent->Entries, "main");
		auto publisher = application::review_cycle_publisher{ run.Journal, *cycle.Store, run.Remote, application::github_server_contract::github_com_2022_11_28, reviewer };

		// Capability first: every limit leaves the remote untouched.
		const auto limits = [&](const cycle_publish_limit aLimit, const char* aWhat) {
			const auto before  = run.Remote.writes();
			const auto outcome = publisher.publish(cycle.Subject, first.Id.Id, { });
			expect(outcome.Status == cycle_publish_status::limited && outcome.Limit == aLimit && run.Remote.writes() == before
					   && outcome.Publication->Journal.Effects.empty(),
				   aWhat);
		};
		run.Remote.CanPush = false;
		limits(cycle_publish_limit::no_push_permission, "no push permission");
		run.Remote.CanPush  = true;
		run.Remote.Archived = true;
		limits(cycle_publish_limit::archived, "an archived repository");
		run.Remote.Archived = false;
		run.Remote.Hosted   = false;
		limits(cycle_publish_limit::unsupported_host, "a host the catalog does not admit");
		run.Remote.Hosted = true;
		const auto p      = simulated_github::hex(first.Plan.Parent);
		auto       kept   = run.Remote.Commits.extract(p);
		limits(cycle_publish_limit::parent_not_on_remote, "P is not on the server");
		run.Remote.Commits.insert(std::move(kept));
		run.Remote.Branches["main"] = core::retag<core::git_commit_oid_kind>(*core::parse_git_entry_oid(std::string(40, 'b')));
		limits(cycle_publish_limit::target_moved, "the base moved since the plan");
		run.Remote.Branches["main"]       = first.Plan.Parent;
		run.Remote.Branches["bpr/harden"] = first.Plan.Parent;
		limits(cycle_publish_limit::branch_exists, "a branch of that name that this plan never made");
		run.Remote.Branches.erase("bpr/harden");
		auto elsewhere = application::review_cycle_publisher{ run.Journal, *cycle.Store, run.Remote, application::github_server_contract::github_com_2022_11_28, core::github_account_id{ 9U } };
		expect(elsewhere.publish(cycle.Subject, first.Id.Id, { }).Limit == cycle_publish_limit::unsupported_host, "an account other than the plan's");

		// The whole run: blobs, tree, commit, then the new branch, journaled in order.
		const auto published = publisher.publish(cycle.Subject, first.Id.Id, { });
		expect(published.Status == cycle_publish_status::linked && published.Publication->Journal.Phase == core::publication_phase::linked, "published through the branch and linked");
		expect(std::ranges::all_of(effect_states(*published.Publication), [](const auto aState) { return aState == core::publication_effect_state::confirmed; })
				   && published.Publication->Journal.Effects.size() == first.Plan.Blobs.size() + 4U,
			   "each effect recorded and confirmed");
		expect(run.Remote.Branches.at("bpr/harden") == first.Plan.Commit && run.Remote.Commits.contains(simulated_github::hex(first.Plan.Commit)),
			   "the server holds exactly the local commit on a new branch");
		expect(run.Remote.Commits.at(simulated_github::hex(first.Plan.Commit)).Parents == std::vector{ first.Plan.Parent }, "one parent: P");
		expect(publisher.publish(cycle.Subject, first.Id.Id, { }).Status == cycle_publish_status::unauthorized && run.Remote.Writes.at("branch") == 1, "nothing is written twice");

		// A lost answer after the tree was stored: read first, then continue.
		auto lost                 = run.authorized(cycle, "bpr/lost-tree");
		run.Remote.Faults["tree"] = simulated_fault::lose_answer;
		auto stopped              = publisher.publish(cycle.Subject, lost.Id.Id, { });
		expect(stopped.Status == cycle_publish_status::stopped && stopped.Publication->Journal.Phase == core::publication_phase::uncertain, "an unanswered tree is uncertain");
		expect(publisher.publish(cycle.Subject, lost.Id.Id, { }).Status == cycle_publish_status::unauthorized, "no blind retry");
		const auto trees = run.Remote.Writes.at("tree");
		const auto read  = publisher.recover(cycle.Subject, lost.Id.Id, { });
		expect(read.Status == cycle_publish_status::recovered && read.Publication->Journal.Effects.back().State == core::publication_effect_state::confirmed && read.Publication->Journal.Generation == 0U,
			   "the read finds the tree; the authorization is spent");
		expect(publisher.publish(cycle.Subject, lost.Id.Id, { }).Status == cycle_publish_status::unauthorized, "remaining writes wait for the reviewer");
		run.reauthorize(cycle, *read.Publication);
		expect(publisher.publish(cycle.Subject, lost.Id.Id, { }).Status == cycle_publish_status::linked && run.Remote.Writes.at("tree") == trees, "continued without writing the tree again");

		// A lost answer after the branch was created: the journal's intent and
		// the planned commit together make it this plan's branch.
		auto branch                 = run.authorized(cycle, "bpr/lost-branch");
		run.Remote.Faults["branch"] = simulated_fault::lose_answer;
		expect(publisher.publish(cycle.Subject, branch.Id.Id, { }).Publication->Journal.Phase == core::publication_phase::uncertain, "an unanswered branch is uncertain");
		const auto adopted = publisher.recover(cycle.Subject, branch.Id.Id, { });
		expect(adopted.Publication->Journal.Phase == core::publication_phase::branch_created, "recovered as this plan's branch");

		// Another actor takes the name between the check and the write.
		auto raced                  = run.authorized(cycle, "bpr/raced");
		run.Remote.Faults["branch"] = simulated_fault::race_branch;
		const auto foreign          = publisher.publish(cycle.Subject, raced.Id.Id, { });
		expect(foreign.Status == cycle_publish_status::stopped && foreign.Publication->Journal.Phase == core::publication_phase::target_drift
				   && run.Remote.Branches.at("bpr/raced") != raced.Plan.Commit,
			   "an existing branch is reported, never overwritten or adopted");

		// A lost answer where the read finds another commit on the branch.
		auto moved                  = run.authorized(cycle, "bpr/moved");
		run.Remote.Faults["branch"] = simulated_fault::not_sent;
		const auto unsent           = publisher.publish(cycle.Subject, moved.Id.Id, { });
		expect(unsent.Status == cycle_publish_status::stopped && unsent.Publication->Journal.Effects.back().State == core::publication_effect_state::not_sent
				   && unsent.Publication->Journal.Generation != 0U,
			   "a branch request that never left keeps the authorization");
		expect(publisher.publish(cycle.Subject, moved.Id.Id, { }).Status == cycle_publish_status::linked, "and may be tried again");

		// Refusals fail the publication; a rewritten identity is never adopted.
		auto forbidden = run.authorized(cycle, "bpr/forbidden");
		run.Remote.Blobs.clear();
		run.Remote.Faults["blob"] = simulated_fault::forbid;
		const auto refused        = publisher.publish(cycle.Subject, forbidden.Id.Id, { });
		expect(refused.Status == cycle_publish_status::stopped && refused.Publication->Journal.Phase == core::publication_phase::failed && refused.Provider->HttpStatus == 403U, "a 403 fails it");
		auto rewritten              = run.authorized(cycle, "bpr/rewritten");
		run.Remote.Faults["commit"] = simulated_fault::rewrite;
		const auto mismatch         = publisher.publish(cycle.Subject, rewritten.Id.Id, { });
		expect(mismatch.Publication->Journal.Phase == core::publication_phase::failed && !run.Remote.Branches.contains("bpr/rewritten"),
			   "a commit the server encoded differently stops before any branch");

		// Workflow files and oversize blobs are prepared externally.
		write_file(root / ".github" / "workflows" / "ci.yml", "on: push\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R2");
		auto workflow = run.authorized(cycle, "bpr/workflow");
		expect(publisher.publish(cycle.Subject, workflow.Id.Id, { }).Limit == cycle_publish_limit::protected_path, "workflow files need a scope this build cannot verify");
		std::filesystem::remove_all(root / ".github");
		write_file(root / "assets" / "table.dat", std::string(application::maximum_github_publication_blob_bytes + 1U, 'x'));
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R3");
		auto large = run.authorized(cycle, "bpr/large");
		expect(publisher.publish(cycle.Subject, large.Id.Id, { }).Limit == cycle_publish_limit::blob_too_large, "a blob over the upload bound");
		std::cout << "NATIVE: objects and a new branch published through the catalog port, capability first, journaled, recovered by reading\n";
	}

	auto pulls_for(const simulated_github& aRemote, const std::string& aHead) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count(aRemote.Pulls, aHead, &application::github_pull_request_observation::HeadRef));
	}

	/// [[BPR-1176]]. The pull request: created only after the branch is
	/// confirmed, reconciled after a lost answer without a second one, never
	/// adopted from another author, and read again for concurrent drift.
	void creates_and_reconciles_the_pull_request(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aExecutable };
		const auto root   = aScratch / "pulls";
		std::filesystem::create_directories(root);
		aGit(root, { "init", "-q" });
		write_file(root / "src" / "parse.cpp", "int parse() { return 0; }\n");
		aGit(root, { "add", "." });
		aGit(root, { "commit", "-q", "-m", "base" });
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R0");
		write_file(root / "src" / "parse.cpp", "int parse() { return validate(); }\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");

		auto       run    = publishing{ *source, cycle, git };
		auto       linked = run.authorized(cycle, "bpr/pr");
		const auto parent = git.Port->list_commit({ .Checkout = root.string(), .Algorithm = core::git_hash_algorithm::sha1 }, linked.Plan.Parent, { });
		expect(parent.has_value(), "P listed");
		run.Remote.seed(linked.Plan.Parent, parent->Tree, parent->Entries, "main");
		auto publisher = application::review_cycle_publisher{ run.Journal, *cycle.Store, run.Remote, application::github_server_contract::github_com_2022_11_28, reviewer };

		// Created after the branch, linked with what the server says.
		const auto created = publisher.publish(cycle.Subject, linked.Id.Id, { });
		expect(created.Status == cycle_publish_status::linked && created.Publication->Journal.Phase == core::publication_phase::linked, "linked");
		const auto& link = *created.Publication->Journal.Link;
		expect(link.Number == 17U && link.Head == linked.Plan.Commit && link.BaseBranch == "main" && !link.Drifted && pulls_for(run.Remote, "bpr/pr") == 1U, "one pull request, the planned head");
		expect(run.Remote.Pulls.front().Title == "Harden parsing" && !run.Remote.Pulls.front().Draft, "the confirmed title and draft choice");
		expect(publisher.publish(cycle.Subject, linked.Id.Id, { }).Status == cycle_publish_status::unauthorized && pulls_for(run.Remote, "bpr/pr") == 1U, "a linked publication never creates another");

		// The create's answer is lost; a restart; recovery finds exactly ours.
		auto lost                 = run.authorized(cycle, "bpr/pr-lost");
		run.Remote.Faults["pull"] = simulated_fault::lose_answer;
		const auto unanswered     = publisher.publish(cycle.Subject, lost.Id.Id, { });
		expect(unanswered.Status == cycle_publish_status::stopped && unanswered.Publication->Journal.Phase == core::publication_phase::uncertain, "an unanswered pull request is uncertain");
		expect(publisher.publish(cycle.Subject, lost.Id.Id, { }).Status == cycle_publish_status::unauthorized, "never created again blindly");
		expect(run.Journal.recover_after_restart(cycle.Subject, { }).has_value(), "restart");
		const auto reconciled = publisher.recover(cycle.Subject, lost.Id.Id, { });
		expect(reconciled.Publication->Journal.Phase == core::publication_phase::linked && reconciled.Publication->Journal.Link->Head == lost.Plan.Commit
				   && pulls_for(run.Remote, "bpr/pr-lost") == 1U,
			   "the uniquely evidenced pull request is associated, not recreated");

		// Two candidates after a lost answer: the reviewer chooses.
		auto ambiguous            = run.authorized(cycle, "bpr/pr-ambiguous");
		run.Remote.Faults["pull"] = simulated_fault::lose_answer;
		expect(publisher.publish(cycle.Subject, ambiguous.Id.Id, { }).Publication->Journal.Phase == core::publication_phase::uncertain, "uncertain");
		auto second   = run.Remote.Pulls.back();
		second.Number = 99U;
		second.Author = "someone-else";
		run.Remote.Pulls.push_back(second);
		const auto undecided = publisher.recover(cycle.Subject, ambiguous.Id.Id, { });
		expect(undecided.Publication->Journal.Phase == core::publication_phase::uncertain && !undecided.Publication->Journal.Link, "ambiguous matches stay uncertain");

		// Another actor opens a pull request for the head just before ours.
		auto raced                = run.authorized(cycle, "bpr/pr-raced");
		run.Remote.Faults["pull"] = simulated_fault::race_pull;
		const auto foreign        = publisher.publish(cycle.Subject, raced.Id.Id, { });
		expect(foreign.Status == cycle_publish_status::stopped && foreign.Publication->Journal.Phase == core::publication_phase::target_drift && pulls_for(run.Remote, "bpr/pr-raced") == 1U
				   && run.Remote.Pulls.back().Author == "someone-else",
			   "a foreign pull request is never duplicated or adopted");

		// A pull request already there before the step: the reviewer's call.
		auto existing             = run.authorized(cycle, "bpr/pr-existing");
		run.Remote.Faults["pull"] = simulated_fault::not_sent;
		expect(publisher.publish(cycle.Subject, existing.Id.Id, { }).Publication->Journal.Effects.back().State == core::publication_effect_state::not_sent, "not sent");
		auto theirs    = run.Remote.Pulls.front();
		theirs.Number  = 120U;
		theirs.HeadRef = "bpr/pr-existing";
		theirs.Author  = "fixture-reviewer";
		run.Remote.Pulls.push_back(theirs);
		const auto writes = run.Remote.Writes["pull"];
		expect(publisher.publish(cycle.Subject, existing.Id.Id, { }).Limit == application::cycle_publish_limit::pull_request_exists && run.Remote.Writes["pull"] == writes,
			   "an existing pull request is read first and not duplicated");

		// A permission refusal fails it.
		auto forbidden            = run.authorized(cycle, "bpr/pr-forbidden");
		run.Remote.Faults["pull"] = simulated_fault::forbid;
		const auto refused        = publisher.publish(cycle.Subject, forbidden.Id.Id, { });
		expect(refused.Publication->Journal.Phase == core::publication_phase::failed && pulls_for(run.Remote, "bpr/pr-forbidden") == 0U, "refused");

		// Another actor pushes to the head right after creation: drift is recorded.
		auto pushed          = run.authorized(cycle, "bpr/pr-pushed");
		run.Remote.AfterPull = [&](application::github_pull_request_observation& aPull) { aPull.HeadCommit = linked.Plan.Parent; };
		const auto drifted   = publisher.publish(cycle.Subject, pushed.Id.Id, { });
		run.Remote.AfterPull = nullptr;
		expect(drifted.Status == cycle_publish_status::linked && drifted.Publication->Journal.Link->Drifted && drifted.Publication->Journal.Link->Head == linked.Plan.Parent,
			   "the re-read head differs from the plan: linked, with drift recorded");

		// A restart between recording the attempt and making it.
		auto interrupted = run.authorized(cycle, "bpr/pr-interrupted");
		auto state       = interrupted;
		while (const auto step = core::next_publication_step(state.Plan, state.Journal)) {
			if (step->Kind == core::publication_effect_kind::pull_request) {
				break;
			}
			const auto begun = run.Journal.change(cycle.Subject, operation(run.Next++), state.Id.Id, state.Revision,
												  core::begin_publication_effect{ .Kind = step->Kind, .Subject = step->Subject, .Generation = state.Journal.Generation, .Account = reviewer }, { });
			state            = run.Journal.change(cycle.Subject, operation(run.Next++), state.Id.Id, begun->Publication.Revision,
												  core::complete_publication_effect{ .Sequence = begun->Sequence, .Outcome = core::publication_effect_state::confirmed }, { })
								   ->Publication;
		}
		run.Remote.Branches["bpr/pr-interrupted"] = interrupted.Plan.Commit;
		const auto begun                          = run.Journal.change(cycle.Subject, operation(run.Next++), state.Id.Id, state.Revision,
																	   core::begin_publication_effect{ .Kind = core::publication_effect_kind::pull_request, .Subject = "pull", .Generation = state.Journal.Generation, .Account = reviewer },
																	   { });
		expect(begun.has_value(), "the attempt is recorded, then the process ends");
		expect(run.Journal.recover_after_restart(cycle.Subject, { }).has_value(), "restart");
		const auto nothing = publisher.recover(cycle.Subject, interrupted.Id.Id, { });
		expect(nothing.Publication->Journal.Effects.back().State == core::publication_effect_state::absent && pulls_for(run.Remote, "bpr/pr-interrupted") == 0U, "the read finds none");
		run.reauthorize(cycle, *nothing.Publication);
		expect(publisher.publish(cycle.Subject, interrupted.Id.Id, { }).Status == cycle_publish_status::linked && pulls_for(run.Remote, "bpr/pr-interrupted") == 1U,
			   "created once, after a fresh confirmation");
		std::cout << "NATIVE: the pull request is created once, reconciled after a lost answer, never duplicated or adopted, and read again for drift\n";
	}

	void refuses_another_object_format(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aExecutable };
		const auto root   = aScratch / "sha256";
		std::filesystem::create_directories(root);
		aGit(root, { "init", "-q", "--object-format=sha256" });
		write_file(root / "a.txt", "one\n");
		aGit(root, { "add", "." });
		aGit(root, { "commit", "-q", "-m", "base" });
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R0");
		write_file(root / "a.txt", "two\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		auto       run         = publishing{ *source, cycle, git };
		auto       publication = run.authorized(cycle, "bpr/sha256");
		auto       publisher   = application::review_cycle_publisher{ run.Journal, *cycle.Store, run.Remote, application::github_server_contract::github_com_2022_11_28, reviewer };
		const auto limit       = publisher.publish(cycle.Subject, publication.Id.Id, { });
		expect(limit.Limit == cycle_publish_limit::object_format && run.Remote.writes() == 0, "a SHA-256 candidate is not converted or sent");
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	const auto executable = tests::workspace_contract::git_executable(aArgc, aArgv);
	const auto fixtures   = tests::workspace_contract::git_from(aArgc, aArgv);
	expect(executable.has_value() && fixtures.has_value(), "publication needs the build's Git (--git)");
	tests::sqlite_contract::scratch_root scratch{ "cycle-publisher" };
	publishes_objects_and_a_branch(scratch.path(), *fixtures, *executable);
	creates_and_reconciles_the_pull_request(scratch.path(), *fixtures, *executable);
	refuses_another_object_format(scratch.path(), *fixtures, *executable);
	std::cout << "review-cycle-publisher-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle publisher failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
