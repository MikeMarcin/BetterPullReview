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
		tour = applied(tour, command(tour, tour_command_kind::accept, tour.Stops[0].Id));
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

	void test_carry_saved_drafts()
	{
		auto tour                 = two_stops();
		tour.Stops[0].Preparation = tour_preparation::ready;
		tour.Stops[1].Suggestion  = tour_suggestion{ .AnchorRef = "a", .Reason = "b" };
		carry_tour_acceptance(tour);
		expect(tour_stop_accepted(tour, tour.Stops[0]), "a stop that was Ready is Accepted");
		expect(!tour.Stops[1].Acceptance, "every other stop, outline-suggested ones too, is Not accepted yet");
		auto moved                    = two_stops();
		moved.ObservedHead            = head2();
		moved.Stops[0].Preparation    = tour_preparation::ready;
		moved.Stops[1].Preparation    = tour_preparation::ready;
		moved.Stops[1].ReaffirmedHead = head2();
		carry_tour_acceptance(moved);
		expect(!moved.Stops[0].Acceptance && moved.Stops[1].Acceptance, "Ready and reaffirmed at the head carries; Ready at an older head does not");
		expect(validate(moved) == tour_refusal::none, "a carried draft validates");
	}
}

auto main() -> int
{
	test_states_and_rings();
	test_what_accepts();
	test_publication();
	test_carry_saved_drafts();
	std::cout << "tour-stop-acceptance:pass\n";
	return EXIT_SUCCESS;
}
