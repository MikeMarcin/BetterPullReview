// [[BPR-2014]], RFC-BPR-0106. A tour stop is accepted the way a file is:
// the core records what the author accepted and decides Accepted, Not
// accepted yet and Changed since accepted from it; only Accept, Save (an
// edit followed by Accept) and the stop's own proposal accept a stop; and
// publication requires every stop Accepted. AC02, AC03 and AC05; the push
// with real comparisons (AC04) is in diff_semantics_tests.cpp.
#include "../support/review_tour_fixture.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/tour_publication_head.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto command(const review_tour& aTour, const tour_command_kind aKind, const tour_stop_id aStop) -> tour_command
	{
		return { .Draft = aTour.Id, .Owner = aTour.Owner, .ExpectedRevision = aTour.Revision, .Kind = aKind, .Stop = aStop };
	}

	[[nodiscard]] auto applied(const review_tour& aTour, const tour_command& aCommand) -> review_tour
	{
		const auto outcome = apply(aTour, aCommand);
		expect(outcome.Refusal == tour_refusal::none, "command " + std::to_string(static_cast<int>(aCommand.Kind)) + " applies (refusal " + std::to_string(static_cast<int>(outcome.Refusal)) + ")");
		return outcome.Accepted;
	}

	[[nodiscard]] auto head2() -> git_commit_oid { return *parse_git_commit_oid("2222222222222222222222222222222222222222"); }

	/// Two stops on the fixture's head: the first names a line of another
	/// changed file in its text, the second only its own lines.
	[[nodiscard]] auto two_stops() -> review_tour
	{
		auto tour   = bpr::tests::review_tour_fixture();
		auto first  = tour.Stops.front();
		first.Body  = "The pool hands back the frame (`src/pool.cpp:41-47`), so clear first.";
		auto other  = tour.Stops.front();
		other.Id    = tour_stop_id{ 2 };
		other.Title = "Second";
		other.Body  = "Read these lines.";
		first.Acceptance.reset();
		other.Acceptance.reset();
		tour.Stops      = { first, other };
		tour.LastStopId = 2;
		return tour;
	}

	void test_states_and_rings()
	{
		auto       tour  = two_stops();
		const auto files = std::vector<std::string>{ "src/item.cpp", "src/pool.cpp" };
		// Not accepted yet: a new stop.
		expect(decide_tour_stop_acceptance(tour, tour.Stops[0]) == tour_acceptance_decision{ tour_acceptance_state::not_accepted, tour_acceptance_reason::not_accepted, { } }, "a new stop is Not accepted yet");
		// Accept records the text, the destination and the code at its lines and its reference.
		auto accept        = command(tour, tour_command_kind::accept, tour.Stops[0].Id);
		accept.Files       = files;
		tour               = applied(tour, accept);
		const auto& record = tour.Stops[0].Acceptance;
		expect(record && record->Title == "Why" && record->Head == tour.Head && record->Destination == publication_source(tour.Stops[0]), "Accept records the text, destination and head");
		expect(record->Material.size() == 2U && !record->Material[0].Reference && record->Material[1].Reference && record->Material[1].Range.Path == "src/pool.cpp" && record->Material[1].Range.FirstLine == 40U && record->Material[1].Range.LastLine == 46U && record->Material[1].Written == "src/pool.cpp:41-47", "Accept records its own lines and the range its text names");
		expect(decide_tour_stop_acceptance(tour, tour.Stops[0]).State == tour_acceptance_state::accepted, "what was accepted is what the stop is now: Accepted");

		// Changed since accepted: a new head, compared.
		tour = applied(tour, { .Draft = tour.Id, .Owner = tour.Owner, .ExpectedRevision = tour.Revision, .Kind = tour_command_kind::observe_head, .Head = head2() });
		expect(decide_tour_stop_acceptance(tour, tour.Stops[0]) == tour_acceptance_decision{ tour_acceptance_state::changed, tour_acceptance_reason::unchecked, { } }, "a new head not compared yet is not Accepted");
		auto lines = tour;
		for (auto& material : lines.Stops[0].Acceptance->Material) {
			material.CheckedAt = head2();
			material.Verdict   = material.Reference ? tour_material_verdict::unchanged : tour_material_verdict::changed;
		}
		expect(decide_tour_stop_acceptance(lines, lines.Stops[0]) == tour_acceptance_decision{ tour_acceptance_state::changed, tour_acceptance_reason::lines_changed, { } }, "its own lines changed in a push");
		auto linked = tour;
		for (auto& material : linked.Stops[0].Acceptance->Material) {
			material.CheckedAt = head2();
			material.Verdict   = material.Reference ? tour_material_verdict::missing : tour_material_verdict::unchanged;
		}
		expect(decide_tour_stop_acceptance(linked, linked.Stops[0]) == tour_acceptance_decision{ tour_acceptance_state::changed, tour_acceptance_reason::reference_missing, "src/pool.cpp:41-47" }, "a range its text names can no longer be found");
		auto same = tour;
		for (auto& material : same.Stops[0].Acceptance->Material) {
			material.CheckedAt = head2();
			material.Verdict   = tour_material_verdict::unchanged;
		}
		expect(decide_tour_stop_acceptance(same, same.Stops[0]).State == tour_acceptance_state::accepted, "byte-identical at the new head stays Accepted");

		// Changed since accepted: moved to another destination.
		auto moved                          = same;
		moved.Stops[0].Original->First.Line = 7U;
		moved.Stops[0].Original->Last.Line  = 9U;
		expect(decide_tour_stop_acceptance(moved, moved.Stops[0]) == tour_acceptance_decision{ tour_acceptance_state::changed, tour_acceptance_reason::moved, { } }, "a stop moved to other lines is Changed since accepted");

		// Not accepted yet: text the author did not accept (Accept all, a website edit).
		auto written = same;
		written.Stops[0].Body += " More.";
		expect(decide_tour_stop_acceptance(written, written.Stops[0]) == tour_acceptance_decision{ tour_acceptance_state::not_accepted, tour_acceptance_reason::text_changed, { } }, "text the author did not accept is Not accepted yet");
	}

	void test_what_accepts()
	{
		auto       tour = two_stops();
		const auto id   = tour.Stops[1].Id;
		// Opening a stop (visit) and the ring accept nothing; the ring sends no command at all.
		tour = applied(tour, [&] {
			auto visit      = command(tour, tour_command_kind::visit, id);
			visit.Attention = { .EditorVisible = true, .Source = tour.Stops[1].Original };
			return visit;
		}());
		expect(!tour.Stops[1].Acceptance, "opening a stop accepts nothing");
		// An edit (as typed) changes the text and keeps no acceptance; Save is the edit and then Accept.
		tour            = applied(tour, command(tour, tour_command_kind::accept, id));
		auto edit       = command(tour, tour_command_kind::edit_stop, id);
		edit.Value      = tour.Stops[1];
		edit.Value.Body = "Read these lines carefully.";
		edit.Value.Acceptance.reset();
		tour = applied(tour, edit);
		expect(tour.Stops[1].Acceptance && decide_tour_stop_acceptance(tour, tour.Stops[1]).State == tour_acceptance_state::not_accepted, "an edit keeps what was accepted and is not accepted until saved");
		auto forged                       = command(tour, tour_command_kind::edit_stop, id);
		forged.Value                      = tour.Stops[1];
		forged.Value.Acceptance->Body     = forged.Value.Body;
		forged.Value.Acceptance->Material = { };
		tour                              = applied(tour, forged);
		expect(tour.Stops[1].Acceptance->Body == "Read these lines.", "an edit never writes what was accepted");
		tour = applied(tour, command(tour, tour_command_kind::accept, id));
		expect(tour_stop_accepted(tour, tour.Stops[1]), "Save (Accept after the edit) leaves the stop Accepted");
		// A relayed Ready (an agent through the CLI) accepts nothing.
		auto fresh             = two_stops();
		auto ready             = command(fresh, tour_command_kind::complete, fresh.Stops[1].Id);
		ready.CompletionOrigin = tour_completion_origin::relayed;
		fresh                  = applied(fresh, ready);
		expect(!fresh.Stops[1].Acceptance, "a relayed Ready accepts nothing");
		// The terminal walk's Ready shows the code: it accepts.
		auto walk             = command(fresh, tour_command_kind::complete, fresh.Stops[0].Id);
		walk.CompletionOrigin = tour_completion_origin::observed;
		walk.Attention        = { .EditorVisible = true, .Source = fresh.Stops[0].Original };
		walk.Files            = { "src/item.cpp", "src/pool.cpp" };
		fresh                 = applied(fresh, walk);
		expect(tour_stop_accepted(fresh, fresh.Stops[0]), "the walk's Ready, with the code shown, accepts");
		// Mark not accepted forgets it.
		fresh = applied(fresh, command(fresh, tour_command_kind::unaccept, fresh.Stops[0].Id));
		expect(!fresh.Stops[0].Acceptance, "Mark not accepted");
		// Duplicate and split are new stops.
		tour = applied(tour, command(tour, tour_command_kind::duplicate, id));
		expect(!tour.Stops.back().Acceptance || !tour_stop_accepted(tour, tour.Stops[2]), "a duplicate is Not accepted yet");
		// A stop with no lines cannot be accepted.
		auto bare = two_stops();
		bare.Stops[1].Original.reset();
		bare.Stops[1].Mapping = tour_mapping::unavailable;
		expect(apply(bare, command(bare, tour_command_kind::accept, bare.Stops[1].Id)).Refusal == tour_refusal::incomplete, "Accept needs lines");
		// An agent-proposed stop has a destination and no confirmed lines: accepting it accepts what it shows.
		auto proposed                 = two_stops();
		proposed.Stops[1].Destination = destination_of(*proposed.Stops[1].Original);
		proposed.Stops[1].Original.reset();
		proposed.Stops[1].Mapping = tour_mapping::unavailable;
		proposed                  = applied(proposed, command(proposed, tour_command_kind::accept, proposed.Stops[1].Id));
		expect(tour_stop_accepted(proposed, proposed.Stops[1]) && tour_stop_publishable(proposed, proposed.Stops[1]), "accepting an agent-proposed stop confirms the lines shown");
	}

	void test_publication()
	{
		auto       tour = two_stops();
		const auto plan = std::vector<tour_destination_plan>(2U, tour_destination_plan{ .Representation = tour_representation::native_line, .JoinsPendingReview = true, .NeedsQualification = false });
		expect(assess_tour_publication(tour, plan, false) == tour_publication_readiness::incomplete, "no publication while any stop is yellow");
		const auto reasons = tour_publication_reasons(tour, plan, false);
		expect(std::ranges::find(reasons, tour_status_reason::not_accepted) != reasons.end(), "the refusal names the stops not accepted");
		expect(decide_tour_head(&tour, { }).ToAccept == 2U, "the head counts both stops to accept");
		auto accept  = command(tour, tour_command_kind::accept, tour.Stops[0].Id);
		accept.Files = { "src/item.cpp", "src/pool.cpp" };
		tour         = applied(tour, accept);
		expect(assess_tour_publication(tour, plan, false) == tour_publication_readiness::incomplete && decide_tour_head(&tour, { }).ToAccept == 1U, "one yellow stop still refuses");
		tour = applied(tour, command(tour, tour_command_kind::accept, tour.Stops[1].Id));
		expect(assess_tour_publication(tour, plan, false) == tour_publication_readiness::single_review, "every stop Accepted publishes");
		expect(decide_tour_head(&tour, { }).State == tour_head_state::publishable, "the head offers Publish once all are accepted");
		// Ready alone, the old mark, publishes nothing.
		auto old = two_stops();
		for (auto& stop : old.Stops) {
			stop.Preparation = tour_preparation::ready;
		}
		expect(assess_tour_publication(old, plan, false) == tour_publication_readiness::incomplete, "Ready is no longer acceptance");
	}

	/// A stop Ready from the author's own look (the window or the walk).
	void author_ready(tour_stop& aStop)
	{
		aStop.Preparation      = tour_preparation::ready;
		aStop.CompletionOrigin = tour_completion_origin::observed;
	}

	void test_carry_saved_drafts()
	{
		auto tour = two_stops();
		author_ready(tour.Stops[1]);
		carry_tour_acceptance(tour);
		expect(tour_stop_accepted(tour, tour.Stops[1]), "a stop the author made Ready is Accepted");
		auto moved = two_stops();
		author_ready(moved.Stops[0]);
		author_ready(moved.Stops[1]);
		moved.ObservedHead            = head2();
		moved.Stops[0].Body           = "Read these lines.";
		moved.Stops[1].ReaffirmedHead = head2();
		carry_tour_acceptance(moved);
		expect(!moved.Stops[0].Acceptance && moved.Stops[1].Acceptance, "Ready and reaffirmed at the head carries; Ready at an older head does not");
		expect(validate(moved) == tour_refusal::none, "a carried draft validates");
	}

	/// Critical review finding 4: a carried stop is Accepted only for the
	/// author's own Ready, never an agent's or a script's, never an
	/// outline-suggested stop (AC07), and never with its references dropped.
	void test_carry_grants_only_the_author()
	{
		const auto carried = [](const auto aChange) {
			auto tour = two_stops();
			author_ready(tour.Stops[1]);
			aChange(tour.Stops[1]);
			carry_tour_acceptance(tour);
			return tour;
		};
		expect(!carried([](tour_stop& aStop) { aStop.CompletionOrigin = tour_completion_origin::relayed; }).Stops[1].Acceptance, "a relayed Ready (an agent, a script) carries no acceptance");
		expect(!carried([](tour_stop& aStop) { aStop.CompletionOrigin = tour_completion_origin::unspecified; }).Stops[1].Acceptance, "a Ready of unknown origin carries no acceptance");
		expect(!carried([](tour_stop& aStop) { aStop.Suggestion = tour_suggestion{ .AnchorRef = "a", .Reason = "b", .Visited = true }; }).Stops[1].Acceptance, "an outline-suggested stop opens Not accepted yet even when it was Ready");
		expect(!carried([](tour_stop& aStop) { aStop.Body = "The pool hands back the frame (`src/pool.cpp:41-47`)."; }).Stops[1].Acceptance,
			   "a stop whose text names another file cannot record that reference without the changed files: Not accepted yet");
		const auto bare = carried([](tour_stop& aStop) { aStop.Body = "Read line 5 first."; });
		expect(bare.Stops[1].Acceptance && tour_stop_accepted(bare, bare.Stops[1]) && std::ranges::any_of(bare.Stops[1].Acceptance->Material, [](const auto& aMaterial) { return aMaterial.Reference && aMaterial.Range.Path == "src/item.cpp" && aMaterial.Range.FirstLine == 4U; }),
			   "a bare line reference names the stop's own file and is kept when carried");
	}

	/// Critical review finding 2: the terminal walk's Ready accepts only with
	/// the changed files its text's references resolve against, so a push that
	/// changes a referenced range turns the stop yellow.
	void test_walk_ready_records_references()
	{
		const auto tour       = two_stops();
		auto       walk       = command(tour, tour_command_kind::complete, tour.Stops[0].Id);
		walk.CompletionOrigin = tour_completion_origin::observed;
		walk.Attention        = { .EditorVisible = true, .Source = tour.Stops[0].Original };
		const auto blind      = applied(tour, walk);
		expect(blind.Stops[0].Preparation == tour_preparation::ready && !blind.Stops[0].Acceptance, "a walk's Ready without the changed files is Ready, not Accepted");
		walk.Files = { "src/item.cpp", "src/pool.cpp" };
		auto seen  = applied(tour, walk);
		expect(tour_stop_accepted(seen, seen.Stops[0]) && std::ranges::any_of(seen.Stops[0].Acceptance->Material, [](const auto& aMaterial) { return aMaterial.Reference && aMaterial.Range.Path == "src/pool.cpp"; }), "a walk's Ready with the changed files records the reference");
		seen = applied(seen, { .Draft = seen.Id, .Owner = seen.Owner, .ExpectedRevision = seen.Revision, .Kind = tour_command_kind::observe_head, .Head = head2() });
		for (auto& material : seen.Stops[0].Acceptance->Material) {
			material.CheckedAt = head2();
			material.Verdict   = material.Reference ? tour_material_verdict::changed : tour_material_verdict::unchanged;
		}
		expect(decide_tour_stop_acceptance(seen, seen.Stops[0]).Reason == tour_acceptance_reason::reference_changed, "a push that changes the referenced lines turns the walk-accepted stop yellow");
		expect(apply(tour, command(tour, tour_command_kind::accept, tour.Stops[0].Id)).Refusal == tour_refusal::incomplete, "Accept without the changed files refuses a stop whose text names another file");
		expect(apply(tour, command(tour, tour_command_kind::accept, tour.Stops[1].Id)).Refusal == tour_refusal::none, "a stop that names no other file needs no file list");
	}

	/// Critical review finding 3: accepting at a head where the stop's own
	/// lines are not mapped keeps them to compare; it never yields Accepted
	/// with nothing to compare.
	void test_reaccept_keeps_own_lines()
	{
		const auto at2 = [](review_tour aTour) { return applied(aTour, { .Draft = aTour.Id, .Owner = aTour.Owner, .ExpectedRevision = aTour.Revision, .Kind = tour_command_kind::observe_head, .Head = head2() }); };
		const auto own = [](const review_tour& aTour) {
			const auto& record = aTour.Stops[1].Acceptance;
			return record ? std::ranges::count_if(record->Material, [](const auto& aMaterial) { return !aMaterial.Reference; }) : 0;
		};
		auto tour = two_stops();
		tour      = at2(applied(tour, command(tour, tour_command_kind::accept, tour.Stops[1].Id)));
		expect(!tour.Stops[1].Mapped && decide_tour_stop_acceptance(tour, tour.Stops[1]).Reason == tour_acceptance_reason::unchecked, "after a push the stop's lines are not compared yet");
		auto again = applied(tour, command(tour, tour_command_kind::accept, tour.Stops[1].Id));
		expect(own(again) == 1 && decide_tour_stop_acceptance(again, again.Stops[1]).Reason == tour_acceptance_reason::unchecked, "Accept while not compared yet keeps the stop's own lines and stays not compared yet");
		auto compared = again;
		for (auto& material : compared.Stops[1].Acceptance->Material) {
			material.CheckedAt = head2();
			material.Verdict   = tour_material_verdict::changed;
		}
		expect(tour_stop_accept_need(compared, compared.Stops[1]) == tour_accept_need::lines_at_head && apply(compared, command(compared, tour_command_kind::accept, compared.Stops[1].Id)).Refusal == tour_refusal::incomplete,
			   "Accept at a head where its lines changed and none are mapped asks for its lines there");
		compared.Stops[1].Mapped         = *compared.Stops[1].Original;
		compared.Stops[1].Mapped->Commit = head2();
		compared.Stops[1].Mapping        = tour_mapping::author_selected;
		compared                         = applied(compared, command(compared, tour_command_kind::accept, compared.Stops[1].Id));
		expect(tour_stop_accepted(compared, compared.Stops[1]) && compared.Stops[1].Acceptance->Material.front().Range.Commit == head2(), "its lines chosen at the new head, Accept accepts them there");
		for (auto& material : again.Stops[1].Acceptance->Material) {
			material.CheckedAt = head2();
			material.Verdict   = tour_material_verdict::unchanged;
		}
		expect(tour_stop_accepted(again, again.Stops[1]), "compared unchanged, it is Accepted");
		auto first = at2(two_stops());
		first      = applied(first, command(first, tour_command_kind::accept, first.Stops[1].Id));
		expect(own(first) == 1 && decide_tour_stop_acceptance(first, first.Stops[1]).Reason == tour_acceptance_reason::unchecked, "a first Accept after a push records the stop's lines at the tour's head, not compared yet");
		auto historical                      = two_stops();
		historical.Stops[1].Original->Commit = *parse_git_commit_oid("3333333333333333333333333333333333333333");
		historical                           = applied(historical, command(historical, tour_command_kind::accept, historical.Stops[1].Id));
		expect(own(historical) == 0 && tour_stop_accepted(historical, historical.Stops[1]), "a historical stop's pinned lines are not recorded: no push can change them");
	}
	/// Re-review finding 1 (2026-11-10): a stop authored at a head the tour
	/// observed after its own (the tour's `Head` is the authored head and never
	/// advances) is compared against later heads like one authored at the
	/// tour's head. Accept never leaves it Accepted with nothing to compare;
	/// only a stop pinned at a commit the tour never had as its head is.
	void test_intermediate_head_stop_is_compared()
	{
		const auto head3 = *parse_git_commit_oid("3333333333333333333333333333333333333333");
		const auto at    = [](review_tour aTour, const git_commit_oid& aHead) { return applied(aTour, { .Draft = aTour.Id, .Owner = aTour.Owner, .ExpectedRevision = aTour.Revision, .Kind = tour_command_kind::observe_head, .Head = aHead }); };
		const auto own   = [](const review_tour& aTour) {
			const auto& record = aTour.Stops[1].Acceptance;
			return record ? std::ranges::count_if(record->Material, [](const auto& aMaterial) { return !aMaterial.Reference; }) : 0;
		};
		// Authored at H1 (the second head the tour saw), first Accept after a push to H2.
		auto tour                      = at(two_stops(), head2());
		tour.Stops[1].Original->Commit = head2();
		tour.Stops[1].Mapping          = tour_mapping::exact_original;
		expect(tour_stop_accept_need(tour, tour.Stops[1]) == tour_accept_need::nothing, "a stop authored at the head now can be accepted");
		auto pushed = at(tour, head3);
		auto first  = applied(pushed, command(pushed, tour_command_kind::accept, pushed.Stops[1].Id));
		expect(own(first) == 1 && first.Stops[1].Acceptance->Material.front().Range.Commit == head2(), "a first Accept after a push records the lines at the head they were authored at");
		expect(decide_tour_stop_acceptance(first, first.Stops[1]).Reason == tour_acceptance_reason::unchecked && !tour_stop_publishable(first, first.Stops[1]),
			   "and the stop stays not compared yet, unpublishable, until they are compared");
		// Accepted at H1, compared changed at H2: Accept asks for its lines; Mark not accepted and Accept again keeps them to compare.
		auto accepted = applied(tour, command(tour, tour_command_kind::accept, tour.Stops[1].Id));
		accepted      = at(accepted, head3);
		for (auto& material : accepted.Stops[1].Acceptance->Material) {
			material.CheckedAt = head3;
			material.Verdict   = tour_material_verdict::changed;
		}
		expect(apply(accepted, command(accepted, tour_command_kind::accept, accepted.Stops[1].Id)).Refusal == tour_refusal::incomplete, "compared changed, Accept asks for its lines at this head");
		auto unaccepted = applied(accepted, command(accepted, tour_command_kind::unaccept, accepted.Stops[1].Id));
		auto again      = applied(unaccepted, command(unaccepted, tour_command_kind::accept, unaccepted.Stops[1].Id));
		expect(own(again) == 1 && !tour_stop_accepted(again, again.Stops[1]), "Mark not accepted then Accept keeps its lines to compare: never Accepted with nothing to compare");
		// A commit the tour never had as its head stays historical.
		auto historical                      = at(two_stops(), head2());
		historical.Stops[1].Original->Commit = head3;
		historical                           = at(historical, *parse_git_commit_oid("4444444444444444444444444444444444444444"));
		historical                           = applied(historical, command(historical, tour_command_kind::accept, historical.Stops[1].Id));
		expect(own(historical) == 0 && tour_stop_accepted(historical, historical.Stops[1]), "a stop pinned at a commit the tour never had as its head is historical");
	}
}

auto main() -> int
{
	test_states_and_rings();
	test_what_accepts();
	test_publication();
	test_carry_saved_drafts();
	test_carry_grants_only_the_author();
	test_walk_ready_records_references();
	test_reaccept_keeps_own_lines();
	test_intermediate_head_stop_is_compared();
	std::cout << "tour-stop-acceptance:pass\n";
	return EXIT_SUCCESS;
}
