#include "bpr/core/review_cycle_publication.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

/// \file
/// \brief The finite publication plan, its authorization and the recovery
/// journal (BPR-1174): invalidation by any change, forged approvals, order,
/// duplicates, lost answers, restart, read-first recovery, cancellation and
/// the exact encodings.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle publication failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto digest(const std::uint8_t aSeed) -> cycle_material_digest
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[31]  = 0xD1;
		return cycle_material_digest{ bytes };
	}

	auto blob(const std::uint8_t aSeed) -> git_entry_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[19]  = 0xB1;
		return git_entry_oid{ git_hash_algorithm::sha1, bytes };
	}

	auto file(std::string aPath, const std::uint8_t aSeed) -> manifest_entry
	{
		return { .Path = std::move(aPath), .Origin = manifest_origin::tracked, .Mode = git_mode_regular, .Size = 1, .Content = digest(aSeed), .Profile = { .Kind = material_text_kind::utf8, .Size = 1 } };
	}

	auto capture(std::vector<manifest_entry> aEntries) -> cycle_manifest
	{
		return { .ScopeVersion = 1, .RootKind = workspace_root_kind::git_checkout, .Coverage = cycle_capture_coverage::complete, .ObjectFormat = git_hash_algorithm::sha1, .Entries = std::move(aEntries) };
	}

	auto parent() -> git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0xC0;
		return git_commit_oid{ git_hash_algorithm::sha1, bytes };
	}

	const auto account = github_account_id{ 4242 };

	auto destination() -> publication_destination
	{
		return { .Host = "github.com", .Account = account, .Login = "ada", .Owner = "example", .Repository = "parser", .BaseBranch = "main", .NewBranch = "bpr/harden-parsing" };
	}

	/// P has a.cpp and b.cpp; the final capture changes both.
	auto made_plan(const publication_destination& aDestination = destination(), const std::string& aTitle = "Harden parsing", const std::string& aBody = "Two rounds.")
		-> publication_plan
	{
		auto       entries = std::vector<git_tree_entry>{ { .Path = "a.cpp", .Mode = git_mode_regular, .Object = blob(1) }, { .Path = "b.cpp", .Mode = git_mode_regular, .Object = blob(2) } };
		const auto tree    = build_git_trees(entries, git_hash_algorithm::sha1);
		auto       request = outgoing_request{
			.StartRevision = cycle_source_revision_id{ 1 },
			.FinalRevision = cycle_source_revision_id{ 2 },
			.Start         = capture({ file("a.cpp", 1), file("b.cpp", 2) }),
			.Final         = capture({ file("a.cpp", 11), file("b.cpp", 12) }),
			.Base          = { .Commit = parent(), .Tree = tree->Root, .Entries = std::move(entries) },
		};
		for (const auto seed : std::array<std::uint8_t, 4>{ 1, 2, 11, 12 }) {
			request.Identities.push_back({ .Content = digest(seed), .Raw = blob(seed) });
		}
		const auto outgoing  = plan_outgoing(request);
		const auto candidate = prepare_candidate(*outgoing, request.Identities,
												 { .Metadata = { .Author    = { .Name = "Ada", .Email = "ada@example.invalid", .Seconds = 1700000000 },
																 .Committer = { .Name = "Ada", .Email = "ada@example.invalid", .Seconds = 1700000000 },
																 .Title     = "Harden parsing" } });
		expect(candidate.has_value(), "candidate");
		const auto plan = make_publication_plan(*outgoing, *candidate, aDestination, aTitle, aBody, false);
		expect(plan.has_value(), "plan");
		return *plan;
	}

	/// Applies one command that must be accepted.
	auto step(const publication_plan& aPlan, publication_journal& aJournal, const publication_command& aCommand, const char* aWhat) -> std::uint64_t
	{
		const auto outcome = apply_publication(aPlan, aJournal, aCommand);
		expect(outcome.has_value(), aWhat);
		aJournal = outcome->Journal;
		return outcome->Sequence;
	}

	auto refused(const publication_plan& aPlan, const publication_journal& aJournal, const publication_command& aCommand) -> publication_refusal
	{
		const auto outcome = apply_publication(aPlan, aJournal, aCommand);
		expect(!outcome.has_value(), "refused");
		return outcome.error();
	}

	auto subject() -> local_cycle_subject
	{
		auto incarnation = std::array<std::uint8_t, 32>{ };
		incarnation[0]   = 0x1C;
		return { .Authority = { .Profile = bpr_profile_id{ 3 }, .Incarnation = cycle_catalog_incarnation{ incarnation } }, .Cycle = review_cycle_id{ 1 }, .Generation = 1 };
	}

	auto reviewer(const publication_plan& aPlan) -> authorize_publication
	{
		return { .Subject = subject(), .Actor = cycle_actor::reviewer, .PlanDigest = publication_plan_digest(aPlan), .Account = account };
	}

	auto begin(const publication_journal& aJournal, const publication_step& aStep) -> begin_publication_effect
	{
		return { .Kind = aStep.Kind, .Subject = aStep.Subject, .Generation = aJournal.Generation, .Account = account };
	}

	auto link() -> publication_link
	{
		return { .Number = 17, .NodeId = "PR_kw17", .Url = "https://github.com/example/parser/pull/17", .Head = made_plan().Commit, .BaseBranch = "main" };
	}

	void plans_bind_everything()
	{
		const auto plan = made_plan();
		expect(plan.Parent == parent() && plan.Content.Parents == std::vector{ parent() } && plan.Blobs.size() == 2U, "the candidate");
		const auto bytes = encode_publication_plan(plan);
		expect(decode_publication_plan(bytes) == plan, "the plan's bytes are exact");
		auto damaged = bytes;
		damaged[97] ^= 1U;
		expect(!decode_publication_plan(damaged).has_value(), "a commit identity its content does not hash to is refused");
		damaged = bytes;
		damaged.pop_back();
		expect(!decode_publication_plan(damaged).has_value(), "truncated bytes are refused");

		// Every public or authority field is part of the digest.
		const auto original = publication_plan_digest(plan);
		auto       changed  = destination();
		changed.BaseBranch  = "release";
		auto other          = destination();
		other.Account       = github_account_id{ 7 };
		auto renamed        = destination();
		renamed.NewBranch   = "bpr/other";
		for (const auto& variant : { made_plan(destination(), "Harden the parser"), made_plan(destination(), "Harden parsing", "Three rounds."), made_plan(changed), made_plan(other), made_plan(renamed) }) {
			expect(publication_plan_digest(variant) != original, "a changed plan is another plan");
		}
		auto draft  = plan;
		draft.Draft = true;
		expect(publication_plan_digest(draft) != original, "the draft choice too");

		auto invalid      = destination();
		invalid.NewBranch = "main";
		auto unsafe       = destination();
		unsafe.NewBranch  = "bpr/../x";
		auto host         = destination();
		host.Host         = "GitHub.com";
		for (const auto& bad : { invalid, unsafe, host }) {
			expect(!valid(bad), "destination refused");
		}
		const auto outgoingless = outgoing_plan{ .BaseCommit = parent() };
		expect(make_publication_plan(outgoingless, publication_candidate{ }, destination(), "t", "", false).error() == publication_refusal::invalid_plan, "a candidate that is not built");
	}

	void authorizes_only_the_reviewer()
	{
		const auto plan    = made_plan();
		auto       journal = start_publication(plan);
		expect(journal.Phase == publication_phase::prepared && journal.Generation == 0U, "prepared, unauthorized");
		expect(refused(plan, journal, begin(journal, *next_publication_step(plan, journal))) == publication_refusal::unauthorized, "nothing is sent unauthorized");
		auto forged  = reviewer(plan);
		forged.Actor = cycle_actor::imported_content;
		expect(refused(plan, journal, forged) == publication_refusal::not_reviewer, "an imported approval authorizes nothing");
		forged.Actor = cycle_actor::granted_consumer;
		expect(refused(plan, journal, forged) == publication_refusal::not_reviewer, "nor does a consumer's grant");
		auto stale       = reviewer(plan);
		stale.PlanDigest = publication_plan_digest(made_plan(destination(), "Harden the parser"));
		expect(refused(plan, journal, stale) == publication_refusal::stale_plan, "a confirmation of an edited plan");
		auto switched    = reviewer(plan);
		switched.Account = github_account_id{ 9 };
		expect(refused(plan, journal, switched) == publication_refusal::authority_changed, "another account");
		step(plan, journal, reviewer(plan), "the reviewer's confirmation");
		expect(journal.Phase == publication_phase::authorized && journal.Generation == 1U, "authorized once");
		auto other    = begin(journal, *next_publication_step(plan, journal));
		other.Account = github_account_id{ 9 };
		expect(refused(plan, journal, other) == publication_refusal::authority_changed, "an account switch stops the writes");
		auto elsewhere = made_plan(destination(), "Harden the parser");
		expect(refused(elsewhere, journal, reviewer(elsewhere)) == publication_refusal::stale_plan, "a journal belongs to one plan");

		// A newer plan supersedes an untried one: its confirmation is spent.
		step(plan, journal, supersede_publication{ }, "superseded");
		expect(journal.Phase == publication_phase::superseded && journal.Generation == 0U, "superseded");
		expect(refused(plan, journal, reviewer(plan)) == publication_refusal::closed, "and closed");
		auto tried = start_publication(plan);
		step(plan, tried, reviewer(plan), "authorize");
		const auto sequence = step(plan, tried, begin(tried, *next_publication_step(plan, tried)), "begin");
		step(plan, tried, complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::confirmed }, "one effect");
		expect(refused(plan, tried, supersede_publication{ }) == publication_refusal::unresolved, "a plan with effects is recovered or cancelled, never silently replaced");

		const auto preview = render_publication_preview(plan);
		for (const auto text : { "Publish as a new pull request: nothing has been sent.", "On github.com as ada (account 4242): example/parser.", "New branch bpr/harden-parsing at commit ",
								 "Pull request into main, ready for review.", "  a.cpp\n", "Author: Ada <ada@example.invalid>; committer: Ada <ada@example.invalid>; unsigned.",
								 "Pull request title: Harden parsing", "Pull request body:\nTwo rounds.", "Private cycle history, notes and rounds are not uploaded.",
								 "Confirming authorizes exactly this plan: " }) {
			expect(preview.find(text) != std::string::npos, text);
		}
	}

	void journals_every_effect()
	{
		const auto plan    = made_plan();
		auto       journal = start_publication(plan);
		step(plan, journal, reviewer(plan), "authorize");
		auto order = std::vector<publication_effect_kind>{ };
		while (const auto next = next_publication_step(plan, journal)) {
			if (next->Kind == publication_effect_kind::branch) {
				auto skipped    = *next;
				skipped.Kind    = publication_effect_kind::pull_request;
				skipped.Subject = "pull";
				expect(refused(plan, journal, begin(journal, skipped)) == publication_refusal::out_of_order, "no pull request before its branch");
			}
			const auto sequence = step(plan, journal, begin(journal, *next), "begin");
			expect(journal.Effects.back().State == publication_effect_state::intended && sequence == journal.Effects.size(), "recorded before the attempt");
			expect(refused(plan, journal, begin(journal, *next)) == publication_refusal::unresolved, "one attempt at a time");
			auto done = complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::confirmed, .Remote = next->Subject };
			if (next->Kind == publication_effect_kind::pull_request) {
				expect(refused(plan, journal, done) == publication_refusal::unknown_effect, "a pull request is confirmed with its link");
				done.Link = link();
			}
			step(plan, journal, done, "confirmed");
			order.push_back(next->Kind);
			if (next->Kind == publication_effect_kind::blob) {
				expect(refused(plan, journal, begin(journal, *next)) == publication_refusal::duplicate, "a confirmed effect is never sent twice");
			}
		}
		expect(order == std::vector{ publication_effect_kind::blob, publication_effect_kind::blob, publication_effect_kind::tree, publication_effect_kind::commit, publication_effect_kind::branch, publication_effect_kind::pull_request },
			   "objects, tree, commit, branch, pull request");
		expect(journal.Phase == publication_phase::linked && journal.Link && journal.Link->Number == 17U, "linked");
		expect(refused(plan, journal, reviewer(plan)) == publication_refusal::closed && refused(plan, journal, cancel_publication{ }) == publication_refusal::closed, "closed");
		auto drift    = link();
		drift.Drifted = true;
		step(plan, journal, refresh_publication_link{ .Link = drift }, "a later read records drift");
		expect(journal.Link->Drifted && journal.Phase == publication_phase::linked, "linked, with drift recorded");
		expect(decode_publication_journal(encode_publication_journal(journal)) == journal, "the journal's bytes are exact");
	}

	void recovers_by_reading_first()
	{
		const auto plan    = made_plan();
		auto       journal = start_publication(plan);
		step(plan, journal, reviewer(plan), "authorize");
		for (int blobs = 0; blobs < 2; ++blobs) {
			const auto next     = *next_publication_step(plan, journal);
			const auto sequence = step(plan, journal, begin(journal, next), "blob");
			step(plan, journal, complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::confirmed }, "blob confirmed");
		}
		// The tree's answer is lost.
		auto next     = *next_publication_step(plan, journal);
		auto sequence = step(plan, journal, begin(journal, next), "tree");
		step(plan, journal, complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::uncertain }, "lost answer");
		expect(journal.Phase == publication_phase::uncertain && journal.Generation == 0U, "uncertain, and the authorization is spent");
		expect(refused(plan, journal, begin(journal, next)) == publication_refusal::unresolved, "no blind retry");
		expect(refused(plan, journal, reviewer(plan)) == publication_refusal::unresolved, "no new authorization before reading");
		step(plan, journal, observe_publication_effect{ .Sequence = sequence, .Observed = publication_effect_state::confirmed, .Remote = next.Subject }, "the read finds the tree");
		expect(journal.Phase == publication_phase::uploading_objects && journal.Generation == 0U, "recovered by reading only");
		expect(refused(plan, journal, begin(journal, *next_publication_step(plan, journal))) == publication_refusal::unauthorized, "remaining writes need a fresh authorization");
		step(plan, journal, reviewer(plan), "fresh authorization");
		expect(journal.Generation == 2U, "a new generation");

		// A crash between recording and the answer: restart makes it uncertain.
		next     = *next_publication_step(plan, journal);
		sequence = step(plan, journal, begin(journal, next), "commit");
		step(plan, journal, restart_publication{ }, "restart");
		expect(journal.Phase == publication_phase::uncertain && journal.Effects.back().State == publication_effect_state::uncertain && journal.Generation == 0U, "restart: uncertain");
		step(plan, journal, observe_publication_effect{ .Sequence = sequence, .Observed = publication_effect_state::absent }, "the read finds no commit");
		step(plan, journal, reviewer(plan), "authorize again");
		sequence = step(plan, journal, begin(journal, next), "the commit, attempted again");
		step(plan, journal, complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::not_sent }, "it never left");
		sequence = step(plan, journal, begin(journal, next), "not sent: the same authorization may try again");
		step(plan, journal, complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::confirmed }, "commit confirmed");

		// Cancelling keeps what exists and stops the rest.
		step(plan, journal, cancel_publication{ }, "cancel");
		expect(journal.Phase == publication_phase::cancelled && journal.Generation == 0U, "cancelled");
		expect(refused(plan, journal, begin(journal, *next_publication_step(plan, journal))) == publication_refusal::closed, "nothing more is sent");
		expect(std::ranges::count(journal.Effects, publication_effect_state::confirmed, &publication_effect::State) == 4, "and nothing is claimed undone");
		step(plan, journal, reviewer(plan), "a deliberate resume");
		expect(journal.Phase == publication_phase::uploading_objects, "resumed where it stopped");

		// The branch name is taken by something else.
		next     = *next_publication_step(plan, journal);
		sequence = step(plan, journal, begin(journal, next), "branch");
		expect(refused(plan, journal, cancel_publication{ }) == publication_refusal::unresolved, "an attempt in flight is resolved first");
		step(plan, journal, complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::foreign }, "the ref exists elsewhere");
		expect(journal.Phase == publication_phase::target_drift && refused(plan, journal, reviewer(plan)) == publication_refusal::closed, "never adopted or overwritten");

		auto refusedJournal = start_publication(plan);
		step(plan, refusedJournal, reviewer(plan), "authorize");
		next     = *next_publication_step(plan, refusedJournal);
		sequence = step(plan, refusedJournal, begin(refusedJournal, next), "blob");
		step(plan, refusedJournal, complete_publication_effect{ .Sequence = sequence, .Outcome = publication_effect_state::refused }, "permission refused");
		expect(refusedJournal.Phase == publication_phase::failed && refused(plan, refusedJournal, reviewer(plan)) == publication_refusal::closed, "failed");
		expect(refused(plan, refusedJournal, observe_publication_effect{ .Sequence = sequence, .Observed = publication_effect_state::confirmed }) == publication_refusal::unknown_effect,
			   "only an uncertain effect is recovered");
	}
}

auto main() -> int
{
	plans_bind_everything();
	authorizes_only_the_reviewer();
	journals_every_effect();
	recovers_by_reading_first();
	std::cout << "review-cycle-publication-tests:pass\n";
	return EXIT_SUCCESS;
}
