#include "../support/review_tour_fixture.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
using namespace bpr;
namespace {
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
	}
}
int main()
{
	{
		auto  tour  = tests::review_tour_fixture();
		auto& stop  = tour.Stops.front();
		auto  state = core::project_tour_stop_status(tour, stop);
		expect(state.ConfirmedLines && !state.SourceAttention, "confirmed original has no source warning");
		stop.Mapped  = stop.Original;
		stop.Mapping = core::tour_mapping::author_selected;
		state        = core::project_tour_stop_status(tour, stop);
		expect(state.ConfirmedLines && !state.SourceAttention, "confirmed author replacement alone has no attention warning");
		stop.Body.clear();
		state = core::project_tour_stop_status(tour, stop);
		expect(state.ConfirmedLines && !state.SourceAttention && !state.ReadyEligible, "missing explanation blocks Ready without unconfirming lines");
		stop.Body       = "Revised";
		stop.Suggestion = core::tour_suggestion{ };
		state           = core::project_tour_stop_status(tour, stop);
		expect(state.ConfirmedLines && !state.ReadyEligible && !state.PublicationReasons.empty(), "generated visitation and preparation are separate from line confirmation");
		stop.Mapping = core::tour_mapping::ambiguous;
		state        = core::project_tour_stop_status(tour, stop);
		expect(state.ConfirmedLines && state.SourceAttention && std::ranges::find(state.SourceReasons, core::tour_status_reason::source_ambiguous) != state.SourceReasons.end(), "ambiguous mapping keeps original confirmation and names its attention reason");
		stop.Original.reset();
		stop.Mapped.reset();
		stop.Destination = core::destination_of(*tests::review_tour_fixture().Stops.front().Original);
		stop.Mapping     = core::tour_mapping::unavailable;
		state            = core::project_tour_stop_status(tour, stop);
		expect(!state.ConfirmedLines && state.SourceAttention, "a proposed path and range alone never confirm lines");
		const auto reasons = core::tour_publication_reasons(tour, { }, false);
		expect(std::ranges::find(reasons, core::tour_status_reason::destination_unknown) != reasons.end(), "unknown operation evidence remains an explicit blocker");
	}
	{
		auto  tour       = tests::review_tour_fixture();
		auto& stop       = tour.Stops.front();
		stop.Remote      = core::tour_remote_binding{ .Kind = core::tour_remote_kind::line_comment, .Entity = "imported-stop", .Own = true };
		stop.Destination = core::destination_of(*stop.Original);
		stop.Mapped      = stop.Original;
		stop.Original.reset();
		stop.Mapping = core::tour_mapping::unchanged_material;
		stop.ReaffirmedHead.reset();
		tour.ObservedHead = tour.Head;
		expect(core::validate(tour) == core::tour_refusal::none, "imported stop remapped at unchanged material is valid without invented original confirmation");
		const auto command = [&](core::tour_command_kind aKind) { return core::apply(tour, { .Draft = tour.Id, .Owner = tour.Owner, .ExpectedRevision = tour.Revision, .Kind = aKind, .Stop = stop.Id }); };
		auto       status  = core::project_tour_stop_status(tour, stop);
		expect(status.ConfirmedLines && !status.SourceAttention && !status.ReadyEligible && !status.ExplanationCurrent, "imported lines are confirmed while the explanation still requires reaffirmation");
		expect(status.ReadyReasons == std::vector{ core::tour_status_reason::explanation_not_reaffirmed } && std::ranges::find(status.AvailableActions, "Ready, next") == status.AvailableActions.end(), "a refused Ready has an honest blocker and no offered Ready action");
		expect(command(core::tour_command_kind::complete).Refusal == core::tour_refusal::incomplete, "projection agrees with actual complete refusal");
		const auto reaffirmed = command(core::tour_command_kind::reaffirm);
		expect(reaffirmed.Refusal == core::tour_refusal::none, "author reaffirmation is the available correction");
		status = core::project_tour_stop_status(reaffirmed.Accepted, reaffirmed.Accepted.Stops.front());
		expect(status.ReadyEligible && status.ReadyReasons.empty() && core::ready(reaffirmed.Accepted.Stops.front()), "actual reaffirmation makes both core and projection Ready eligible");
	}

	// Valid source histories must expose the same Ready decision as complete.
	for (const auto mapping : { core::tour_mapping::exact_original, core::tour_mapping::unchanged_material, core::tour_mapping::changed_material, core::tour_mapping::ambiguous, core::tour_mapping::missing, core::tour_mapping::unavailable, core::tour_mapping::author_selected }) {
		for (const auto flags : { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U }) {
			auto  tour   = tests::review_tour_fixture();
			auto& stop   = tour.Stops.front();
			stop.Mapped  = stop.Original;
			stop.Mapping = mapping;
			if (flags & 1U) {
				stop.Remote      = core::tour_remote_binding{ .Kind = core::tour_remote_kind::line_comment, .Entity = "imported", .Own = true };
				stop.Destination = core::destination_of(*stop.Original);
			}
			if (flags & 2U) {
				stop.ReaffirmedHead = tour.Head;
			}
			if (flags & 4U) {
				stop.Original.reset();
			}
			if (core::validate(tour) != core::tour_refusal::none) {
				continue;
			}
			const auto status    = core::project_tour_stop_status(tour, stop);
			const auto completed = core::apply(tour, { .Draft = tour.Id, .Owner = tour.Owner, .ExpectedRevision = tour.Revision, .Kind = core::tour_command_kind::complete, .Stop = stop.Id });
			expect(status.ReadyEligible == (completed.Refusal == core::tour_refusal::none) && status.ReadyEligible == status.ReadyReasons.empty(), "Ready reasons/actions agree with the actual command across valid original, imported, remapped and reaffirmed sources");
		}
	}

	using support        = core::tour_operation_support;
	using subject        = core::tour_subject;
	using representation = core::tour_representation;
	auto source          = core::destination_of(*tests::review_tour_fixture().Stops.front().Original);
	source.FirstLine     = 1;
	source.LastLine      = 3;
	core::tour_destination_observation observation{ .Source = source, .Subject = subject::current_diff, .SourceAvailable = true };
	expect(core::plan_tour_destination(observation).NeedsQualification, "source availability grants no batch support");
	observation.PendingLineBatch = support::supported;
	auto plan                    = core::plan_tour_destination(observation);
	expect(plan.Representation == representation::native_line && plan.JoinsPendingReview && !plan.RequiresCompatibility, "qualified exact multiline batch");
	observation.SourceAvailable = false;
	expect(core::plan_tour_destination(observation).Representation == representation::unresolved, "missing blob refuses native placement");
	observation.SourceAvailable = true;
	observation.Source.LastLine = 0;
	expect(core::plan_tour_destination(observation).NeedsQualification, "reversed range refused");
	observation.Source  = source;
	observation.Subject = subject::current_off_hunk;
	expect(core::plan_tour_destination(observation).Representation == representation::unresolved, "diff batch support is not off-hunk support");
	observation.PendingOffHunkBatch = support::supported;
	expect(core::plan_tour_destination(observation).JoinsPendingReview, "exact qualified off-hunk operation");
	observation.PendingOffHunkBatch = support::unsupported;
	observation.PendingFileThread   = support::supported;
	expect(core::plan_tour_destination(observation).Representation == representation::file_permalink && core::plan_tour_destination(observation).JoinsPendingReview, "only exact pending file operation joins same review");
	observation.PendingFileThread = support::unsupported;
	observation.StandaloneFile    = support::supported;
	plan                          = core::plan_tour_destination(observation);
	expect(plan.Representation == representation::file_permalink && !plan.JoinsPendingReview && plan.RequiresCompatibility, "standalone file subject is explicitly non-atomic");
	for (auto failure : { support::unknown, support::denied, support::rate_limited, support::indeterminate }) {
		observation.PendingFileThread = failure;
		expect(core::plan_tour_destination(observation).Representation == representation::unresolved, "permission rate-limit unknown timeout never authorize fallback");
		observation.PendingFileThread   = support::supported;
		observation.PendingOffHunkBatch = failure;
		expect(core::plan_tour_destination(observation).Representation == representation::unresolved, "failed native cannot choose a second write");
		observation.PendingOffHunkBatch = support::unsupported;
	}
	for (auto kind : { subject::historical, subject::unchanged_file }) {
		observation.Subject      = kind;
		observation.Conversation = support::supported;
		plan                     = core::plan_tour_destination(observation);
		expect(plan.Representation == representation::conversation_permalink && plan.RequiresCompatibility && !plan.JoinsPendingReview, "historical/base/deleted/unchanged never invent review subject");
		observation.Body = core::classify_review_body("```suggestion\nchanged\n```");
		expect(core::plan_tour_destination(observation).IncompatibleBody, "suggestions cannot silently become permalink prose");
		observation.Body = core::review_body_compatibility::portable;
	}
	source.Path = "renamed/old.md";
	source.Side = core::diff_side::left;
	expect(core::valid(source), "exact renamed/base source is representable");
	for (const auto path : { "../bad", "/bad", "a/../b", "a//b", "a\\b" }) {
		source.Path = path;
		expect(!core::valid(source), "unsafe source path refused");
	}
	auto                                     draft = tests::review_tour_fixture();
	std::vector<core::tour_destination_plan> decisions{ { representation::native_line, true, false, false, false } };
	using readiness = core::tour_publication_readiness;
	expect(core::assess_tour_publication(draft, decisions, false) == readiness::single_review, "core permits only ready qualified native plan");

	{
		auto mixed = draft;
		auto stop  = mixed.Stops.front();
		stop.Id    = core::tour_stop_id{ 2 };
		mixed.Stops.push_back(stop);
		mixed.LastStopId = 2;
		std::vector<core::tour_destination_plan> split{ { representation::native_line, true, false, false, false }, { representation::conversation_permalink, false, true, false, false } };
		expect(core::assess_tour_publication(mixed, split, false) == readiness::compatibility_choice, "mixed remains an explicit outcome");
		expect(core::assess_tour_publication(mixed, split, false, true) == readiness::mixed_comments, "exact qualified mixed destination set admitted");
		expect(core::assess_tour_publication(mixed, split, true, true) == readiness::unavailable, "conflicting explicit outcomes refused");
		split.back().NeedsQualification = true;
		expect(core::assess_tour_publication(mixed, split, false, true) == readiness::unavailable, "mixed cannot manufacture unknown support");
		split.back() = { representation::file_permalink, false, true, false, false };
		expect(core::assess_tour_publication(mixed, split, false, true) == readiness::mixed_comments, "standalone file remains explicitly outside pending membership");
	}
	decisions[0] = { representation::conversation_permalink, false, true, false, false };
	expect(core::assess_tour_publication(draft, decisions, false) == readiness::compatibility_choice, "visible fallback requires explicit choice");
	expect(core::assess_tour_publication(draft, decisions, true) == readiness::compatibility_comments, "explicit qualified conversation plan permitted");
	decisions[0].NeedsQualification = true;
	expect(core::assess_tour_publication(draft, decisions, true) == readiness::unavailable, "user choice cannot create missing capability");
	draft.Problem = " ";
	expect(core::assess_tour_publication(draft, decisions, true) == readiness::incomplete, "unfinished prose blocks publishing but remains saveable");
	std::cout << "tour-destinations:pass\n";
}
