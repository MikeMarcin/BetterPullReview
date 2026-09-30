#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_publication.hpp"
#include "cycle_course.hpp"
#include "publication_course.hpp"
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
/// \brief The finite publication plan, its authorization and recovery
/// journal through the production hosts and store (RFC-BPR-0059, BPR-1174):
/// an exact preview, invalidation by title, body, source, account and base
/// changes, forged approvals, operation replay, a lost answer, a restart with
/// an attempt in flight, read-first recovery and cancellation. No remote
/// effect is made: outcomes a provider would report are supplied by the
/// course, and the provider catalog is BPR-1175's.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_store_failure;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::publication_git;
	using tests::workspace_contract::write_file;

	constexpr auto reviewer_account = core::github_account_id{ 5150 };

	auto operation(const std::uint16_t aSeed) -> application::cycle_operation_id
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0x9B;
		bytes[30]  = static_cast<std::uint8_t>(aSeed >> 8U);
		bytes[31]  = static_cast<std::uint8_t>(aSeed & 0xFFU);
		return application::cycle_operation_id{ bytes };
	}

	auto destination() -> core::publication_destination
	{
		return { .Host = "github.com", .Account = reviewer_account, .Login = "ada", .Owner = "example", .Repository = "parser", .BaseBranch = "main", .NewBranch = "bpr/harden-parsing" };
	}

	auto request(const std::string& aTitle = "Harden parsing", const std::string& aBody = "Reviewed locally in two rounds.", const core::publication_destination& aDestination = destination())
		-> application::cycle_publication_request
	{
		return { .Candidate   = { .Outgoing  = { .Base = "HEAD" },
								  .Candidate = { .Metadata = { .Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
															   .Committer = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
															   .Title     = aTitle,
															   .Body      = aBody } } },
				 .Destination = aDestination,
				 .PullTitle   = aTitle,
				 .PullBody    = aBody };
	}

	struct hosts
	{
		application::review_cycle_outgoing_host    Outgoing;
		application::review_cycle_publication_host Publication;

		hosts(application::workspace_source_port& aSource, course& aCycle, publication_git& aGit)
			: Outgoing{ aSource, *aCycle.Store, *aGit.Port }
			, Publication{ Outgoing, *aCycle.Store }
		{
		}
	};

	auto confirmation(const application::stored_cycle_publication& aPublication, const core::cycle_actor aActor = core::cycle_actor::reviewer,
					  const core::github_account_id aAccount = reviewer_account) -> application::cycle_publication_confirmation
	{
		return { .Actor = aActor, .PlanDigest = aPublication.Journal.PlanDigest, .Account = aAccount };
	}

	void journals_one_publication(const std::filesystem::path& aScratch, const git_runner& aGit, const std::filesystem::path& aExecutable)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       git    = publication_git{ aExecutable };
		const auto root   = aScratch / "project";
		std::filesystem::create_directories(root);
		aGit(root, { "init", "-q" });
		write_file(root / "src" / "parse.cpp", "int parse() { return 0; }\n");
		write_file(root / "README.md", "# Parser\n");
		aGit(root, { "add", "." });
		aGit(root, { "commit", "-q", "-m", "base" });
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R0");
		write_file(root / "src" / "parse.cpp", "int parse() { return validate(); }\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		auto       run     = hosts{ *source, cycle, git };
		const auto subject = cycle.Subject;

		// The exact preview names everything a confirmation authorizes.
		auto prepared = run.Publication.prepare(cycle.state(), operation(1), request(), { });
		expect(prepared.Publication.has_value(), "plan recorded");
		const auto first = *prepared.Publication;
		expect(first.Journal.Phase == core::publication_phase::prepared && first.Journal.Generation == 0U, "nothing authorized by preparing");
		for (const auto text : { "Publish as a new pull request: nothing has been sent.", "On github.com as ada (account 5150): example/parser.", "New branch bpr/harden-parsing at commit ",
								 "  src/parse.cpp", "Author: Ada Lovelace <ada@example.invalid>", "Pull request title: Harden parsing", "Pull request body:\nReviewed locally in two rounds.",
								 "Confirming authorizes exactly this plan: ", "Candidate commit " }) {
			expect(prepared.Preview.find(text) != std::string::npos, text);
		}
		const auto replayed = run.Publication.prepare(cycle.state(), operation(1), request(), { });
		expect(replayed.Publication && replayed.Publication->Id == first.Id, "a retried preparation records nothing new");

		// Forged, foreign and stale confirmations authorize nothing.
		const auto imported = run.Publication.authorize(subject, operation(2), first.Id.Id, first.Revision, confirmation(first, core::cycle_actor::imported_content), { });
		expect(!imported && imported.error().Publication == core::publication_refusal::not_reviewer, "an imported approval string");
		const auto granted = run.Publication.authorize(subject, operation(3), first.Id.Id, first.Revision, confirmation(first, core::cycle_actor::granted_consumer), { });
		expect(!granted && granted.error().Publication == core::publication_refusal::not_reviewer, "a consumer's grant");
		const auto switched = run.Publication.authorize(subject, operation(4), first.Id.Id, first.Revision, confirmation(first, core::cycle_actor::reviewer, core::github_account_id{ 9 }), { });
		expect(!switched && switched.error().Publication == core::publication_refusal::authority_changed, "another account than the plan's");

		// Title, body, source, account and base changes are new plans; the old confirmation is spent.
		auto retitled = run.Publication.prepare(cycle.state(), operation(5), request("Harden the parser"), { });
		expect(retitled.Publication && retitled.Publication->Journal.PlanDigest != first.Journal.PlanDigest, "an edited title is another plan");
		const auto old = run.Publication.load(subject, first.Id.Id, { });
		expect(old && old->Journal.Phase == core::publication_phase::superseded, "the untried plan is superseded");
		const auto spent = run.Publication.authorize(subject, operation(6), first.Id.Id, old->Revision, confirmation(first), { });
		expect(!spent && spent.error().Publication == core::publication_refusal::closed, "and its confirmation authorizes nothing");
		auto other         = destination();
		other.Account      = core::github_account_id{ 77 };
		other.Login        = "grace";
		auto rebased       = destination();
		rebased.BaseBranch = "release";
		const auto digests = std::vector{ run.Publication.prepare(cycle.state(), operation(7), request("Harden the parser", "Another body."), { }).Publication->Journal.PlanDigest,
										  run.Publication.prepare(cycle.state(), operation(8), request("Harden the parser", "Another body.", other), { }).Publication->Journal.PlanDigest,
										  run.Publication.prepare(cycle.state(), operation(9), request("Harden the parser", "Another body.", rebased), { }).Publication->Journal.PlanDigest };
		expect(std::ranges::find(digests, retitled.Publication->Journal.PlanDigest) == digests.end() && digests[0] != digests[1] && digests[1] != digests[2], "body, account and base each change the plan");
		write_file(root / "src" / "parse.cpp", "int parse() { return validate() ? 1 : 0; }\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R2");
		auto current = run.Publication.prepare(cycle.state(), operation(10), request(), { });
		expect(current.Publication && current.Publication->Plan.Final == core::cycle_source_revision_id{ 3 } && current.Publication->Journal.PlanDigest != first.Journal.PlanDigest,
			   "a new capture is a new plan");
		const auto listed = cycle.Store->publications(subject, std::nullopt, { });
		expect(listed && listed->Publications.size() == 6U
				   && std::ranges::count(listed->Publications, core::publication_phase::superseded, [](const application::stored_cycle_publication& aPublication) { return aPublication.Journal.Phase; }) == 5,
			   "every older untried plan is superseded; history is kept");

		// The reviewer confirms the current plan.
		const auto id         = current.Publication->Id.Id;
		const auto authorized = run.Publication.authorize(subject, operation(11), id, current.Publication->Revision, confirmation(*current.Publication), { });
		expect(authorized && authorized->Publication.Journal.Phase == core::publication_phase::authorized && authorized->Publication.Journal.Generation == 1U, "authorized");
		auto state = authorized->Publication;

		// Each attempt is journaled before it is made; a retried operation replays.
		const auto begin = [&](const std::uint16_t aOperation) {
			const auto next = core::next_publication_step(state.Plan, state.Journal);
			expect(next.has_value(), "a next step");
			return run.Publication.change(subject, operation(aOperation), id, state.Revision,
										  core::begin_publication_effect{ .Kind = next->Kind, .Subject = next->Subject, .Generation = state.Journal.Generation, .Account = reviewer_account }, { });
		};
		auto blob = begin(12);
		expect(blob && blob->Sequence == 1U && blob->Publication.Journal.Effects[0].State == core::publication_effect_state::intended, "the blob attempt is recorded first");
		const auto again = begin(12);
		expect(again && again->Replayed && again->Sequence == 1U && again->Publication.Journal.Effects.size() == 1U, "a retried begin replays; no second attempt is recorded");
		state = blob->Publication;
		auto done
			= run.Publication.change(subject, operation(13), id, state.Revision, core::complete_publication_effect{ .Sequence = 1, .Outcome = core::publication_effect_state::confirmed }, { });
		expect(done.has_value(), "blob confirmed");
		state = done->Publication;

		// The tree's answer is lost: recovery reads first and needs a fresh confirmation.
		auto tree = begin(14);
		expect(tree.has_value(), "tree attempted");
		state     = tree->Publication;
		auto lost = run.Publication.change(subject, operation(15), id, state.Revision,
										   core::complete_publication_effect{ .Sequence = tree->Sequence, .Outcome = core::publication_effect_state::uncertain }, { });
		expect(lost && lost->Publication.Journal.Phase == core::publication_phase::uncertain && lost->Publication.Journal.Generation == 0U, "a lost answer is uncertain");
		state            = lost->Publication;
		const auto blind = begin(16);
		expect(!blind && blind.error().Publication == core::publication_refusal::unresolved, "no blind retry");
		auto read = run.Publication.change(subject, operation(17), id, state.Revision,
										   core::observe_publication_effect{ .Sequence = tree->Sequence, .Observed = core::publication_effect_state::confirmed }, { });
		expect(read && read->Publication.Journal.Phase == core::publication_phase::uploading_objects && read->Publication.Journal.Generation == 0U, "read-first recovery writes nothing more");
		state = read->Publication;
		expect(begin(18).error().Publication == core::publication_refusal::unauthorized, "the remaining writes wait for the reviewer");
		auto fresh = run.Publication.authorize(subject, operation(19), id, state.Revision, confirmation(state), { });
		expect(fresh && fresh->Publication.Journal.Generation == 2U, "a fresh confirmation");
		state = fresh->Publication;

		// A restart with the commit attempt in flight.
		auto commit = begin(20);
		expect(commit.has_value(), "commit attempted");
		cycle.Store.reset();
		cycle.Store    = adapters::sqlite::make_review_cycle_store(cycle.StoreRoot.path(), tests::workspace_contract::profile);
		auto restarted = hosts{ *source, cycle, git };
		auto recovered = restarted.Publication.recover_after_restart(subject, { });
		expect(recovered && recovered->size() == 6U, "every publication is looked at after a restart");
		const auto mine = std::ranges::find(*recovered, id, [](const application::stored_cycle_publication& aPublication) { return aPublication.Id.Id; });
		expect(mine->Journal.Phase == core::publication_phase::uncertain && mine->Journal.Generation == 0U && mine->Journal.Effects.back().State == core::publication_effect_state::uncertain,
			   "the in-flight commit is uncertain and no authorization survives");
		expect(restarted.Publication.recover_after_restart(subject, { })->at(5).Revision == mine->Revision, "recovering again changes nothing");
		state       = *mine;
		auto absent = restarted.Publication.change(subject, operation(21), id, state.Revision,
												   core::observe_publication_effect{ .Sequence = commit->Sequence, .Observed = core::publication_effect_state::absent }, { });
		expect(absent.has_value(), "the read finds no commit");
		state = absent->Publication;
		expect(restarted.Publication.change(subject, operation(22), id, state.Revision, core::cancel_publication{ }, { }).has_value(), "the reviewer cancels");
		const auto cancelled = restarted.Publication.load(subject, id, { });
		expect(cancelled && cancelled->Journal.Phase == core::publication_phase::cancelled
				   && std::ranges::count(cancelled->Journal.Effects, core::publication_effect_state::confirmed, &core::publication_effect::State) == 2,
			   "cancelled; the confirmed blob and tree are recorded, nothing is claimed undone");
		std::cout << "NATIVE: publication plans, confirmations and the journal persist and recover by reading first\n";
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	const auto executable = tests::workspace_contract::git_executable(aArgc, aArgv);
	const auto fixtures   = tests::workspace_contract::git_from(aArgc, aArgv);
	expect(executable.has_value() && fixtures.has_value(), "publication needs the build's Git (--git)");
	tests::sqlite_contract::scratch_root scratch{ "cycle-publication" };
	journals_one_publication(scratch.path(), *fixtures, *executable);
	std::cout << "review-cycle-publication-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle publication failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
