#include "../support/review_tour_fixture.hpp"
#include "bpr/core/review_tour_destination.hpp"
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
	decisions[0] = { representation::conversation_permalink, false, true, false, false };
	expect(core::assess_tour_publication(draft, decisions, false) == readiness::compatibility_choice, "visible fallback requires explicit choice");
	expect(core::assess_tour_publication(draft, decisions, true) == readiness::compatibility_comments, "explicit qualified conversation plan permitted");
	decisions[0].NeedsQualification = true;
	expect(core::assess_tour_publication(draft, decisions, true) == readiness::unavailable, "user choice cannot create missing capability");
	draft.Problem = " ";
	expect(core::assess_tour_publication(draft, decisions, true) == readiness::incomplete, "unfinished prose blocks publishing but remains saveable");
	std::cout << "tour-destinations:pass\n";
}
