#include "../support/review_tour_fixture.hpp"
#include "bpr/core/review_tour_repair.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
	using namespace bpr::core;
	void expect(const bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
	}
	struct repair_fixture
	{
		review_tour                   Tour = bpr::tests::review_tour_fixture();
		std::vector<tour_anchor_file> Files;
		repair_fixture()
		{
			Tour.Head           = *parse_git_commit_oid("2222222222222222222222222222222222222222");
			Tour.ObservedHead   = Tour.Head;
			auto stop           = Tour.Stops.front();
			stop.Original->Blob = *parse_git_blob_oid("3333333333333333333333333333333333333333");
			stop.Original->Path = "docs/guide.md";
			stop.ReaffirmedHead = Tour.Head;
			stop.Preparation    = tour_preparation::ready;
			Tour.Stops.clear();
			for (std::uint64_t number = 1U; number <= 10U; ++number) {
				stop.Id               = tour_stop_id{ number };
				stop.Title            = "Stop " + std::to_string(number);
				stop.Original->Commit = Tour.Head;
				Tour.Stops.push_back(stop);
			}
			Tour.LastStopId              = 10U;
			auto& historical             = Tour.Stops.front();
			historical.Mapped            = historical.Original;
			historical.Original->Commit  = Tour.Base;
			historical.Mapping           = tour_mapping::unchanged_material;
			auto& offHunk                = Tour.Stops[8U];
			offHunk.Original->Path       = "src/item.cpp";
			offHunk.Original->First.Line = 18U;
			offHunk.Original->Last.Line  = 18U;
			Files                        = { { "docs/guide.md", true, 40U, std::vector<tour_line_span>{ { 1U, 12U } } }, { "src/item.cpp", true, 100U, std::vector<tour_line_span>{ { 20U, 26U }, { 40U, 46U } } } };
		}
		auto observation(const tour_stop& aStop) const -> tour_destination_observation
		{
			auto value                = publication_observation(*publication_source(aStop), Tour.Head, Tour.Base, aStop.Body, false, true);
			value.PendingOffHunkBatch = tour_operation_support::unsupported;
			value.PendingFileThread   = tour_operation_support::unsupported;
			value.StandaloneFile      = tour_operation_support::unsupported;
			return value;
		}
		auto observations() const -> std::vector<tour_destination_observation>
		{
			auto result = std::vector<tour_destination_observation>{ };
			for (const auto& stop : Tour.Stops) {
				result.push_back(observation(stop));
			}
			return result;
		}
		auto destinations() const -> std::vector<tour_destination_plan>
		{
			auto result = std::vector<tour_destination_plan>{ };
			for (const auto& stop : Tour.Stops) {
				result.push_back(plan_tour_stop_repair(Tour, stop, observation(stop), Files).Destination);
			}
			return result;
		}
	};
	void repairs_compose_as_one_review()
	{
		auto       fixture  = repair_fixture{ };
		const auto before   = fixture.Tour;
		const auto findings = tour_destination_repairs(fixture.Tour, fixture.observations(), fixture.Files);
		expect(findings.size() == 2U && findings[0].Number == 1U && findings[1].Number == 9U, "every incompatible stop identified in route order");
		expect(findings[0].Stop == before.Stops[0].Id && findings[0].Title == "Stop 1" && findings[0].Problem == tour_destination_problem::historical_exact && findings[0].Repin, "historical exact mapping supplies typed re-pin");
		expect(findings[1].Problem == tour_destination_problem::off_hunk && findings[1].Relocation && findings[1].Relocation->First.Line == 19U && !findings[1].FileComment, "off-hunk source moves to first context line, not first changed line");
		expect(assess_tour_publication(fixture.Tour, fixture.destinations(), false) == tour_publication_readiness::compatibility_choice, "unrepaired route needs explicit outcome");
		const auto repin = prepare_tour_source_repair(fixture.Tour, before.Stops[0].Id, tour_source_repair_kind::repin_head, fixture.observation(fixture.Tour.Stops[0]), fixture.Files);
		expect(repin.has_value(), "one action prepares exact re-pin");
		const auto first = apply(fixture.Tour, *repin);
		expect(first.Refusal == tour_refusal::none && first.Changed, "re-pin is an ordinary accepted local edit");
		fixture.Tour = first.Accepted;
		expect(apply(fixture.Tour, *repin).Refusal == tour_refusal::conflict, "captured repair cannot apply twice or against another revision");
		expect(publication_source(fixture.Tour.Stops[0])->Commit == fixture.Tour.Head && fixture.Tour.Stops[0].Original == before.Stops[0].Original, "re-pin changes effective source without losing provenance");
		const auto relocate = prepare_tour_source_repair(fixture.Tour, before.Stops[8U].Id, tour_source_repair_kind::nearest_hunk, fixture.observation(fixture.Tour.Stops[8U]), fixture.Files);
		expect(relocate && relocate->Restore->Stops[8U].Mapped->First.Line == 19U, "relocation destination is available to preview before applying");
		const auto second = apply(fixture.Tour, *relocate);
		expect(second.Refusal == tour_refusal::none && second.Accepted.Revision == before.Revision + 2U, "each repair is one revision");
		fixture.Tour = second.Accepted;
		expect(tour_destination_repairs(fixture.Tour, fixture.observations(), fixture.Files).empty(), "both blockers disappear after repairs");
		expect(assess_tour_publication(fixture.Tour, fixture.destinations(), false) == tour_publication_readiness::single_review, "ten repaired stops assess as one review");
		for (std::size_t index = 0U; index < before.Stops.size(); ++index) {
			expect(fixture.Tour.Stops[index].Id == before.Stops[index].Id && fixture.Tour.Stops[index].Body == before.Stops[index].Body && fixture.Tour.Stops[index].Preparation == tour_preparation::ready, "ordinary prose, stable IDs and readiness survive");
		}
		const auto undone = apply(fixture.Tour, { .Draft = fixture.Tour.Id, .Owner = fixture.Tour.Owner, .ExpectedRevision = fixture.Tour.Revision, .Kind = tour_command_kind::restore, .Restore = first.Accepted });
		expect(undone.Refusal == tour_refusal::none && undone.Accepted.Stops[8U] == before.Stops[8U] && undone.Accepted.Stops[0] == first.Accepted.Stops[0], "ordinary restore undoes only the last repair");
	}
	void unsafe_sources_stay_unrepaired()
	{
		for (const auto mapping : { tour_mapping::changed_material, tour_mapping::ambiguous, tour_mapping::missing, tour_mapping::unavailable, tour_mapping::author_selected }) {
			auto  fixture = repair_fixture{ };
			auto& stop    = fixture.Tour.Stops[0];
			stop.Mapping  = mapping;
			if (mapping == tour_mapping::author_selected) {
				stop.Mapped->Commit = stop.Original->Commit;
			}
			expect(!plan_tour_stop_repair(fixture.Tour, stop, fixture.observation(stop), fixture.Files).Repin, "changed ambiguous missing unavailable and author-selected sources never infer an exact re-pin");
			expect(!prepare_tour_source_repair(fixture.Tour, stop.Id, tour_source_repair_kind::repin_head, fixture.observation(stop), fixture.Files), "unoffered re-pin cannot be prepared");
		}
		auto  fixture       = repair_fixture{ };
		auto& stop          = fixture.Tour.Stops[0];
		stop.Mapped->Commit = fixture.Tour.Base;
		expect(!plan_tour_stop_repair(fixture.Tour, stop, fixture.observation(stop), fixture.Files).Repin, "stale mapping cannot re-pin");
		fixture = repair_fixture{ };
		fixture.Files[0].Hunks.reset();
		expect(!plan_tour_stop_repair(fixture.Tour, fixture.Tour.Stops[0], fixture.observation(fixture.Tour.Stops[0]), fixture.Files).Repin, "unknown head patch cannot promise a native exact re-pin");
		fixture                    = repair_fixture{ };
		auto observation           = fixture.observation(fixture.Tour.Stops[8U]);
		observation.StandaloneFile = tour_operation_support::supported;
		expect(plan_tour_stop_repair(fixture.Tour, fixture.Tour.Stops[8U], observation, fixture.Files).FileComment, "qualified standalone file choice is reported separately");
		const auto fileChoice = prepare_tour_file_comment(fixture.Tour, fixture.Tour.Stops[8U].Id, observation, fixture.Files);
		expect(fileChoice && fileChoice->Restore->Stops[8U].FileCommentRequested && !fixture.Tour.Stops[8U].FileCommentRequested, "qualified file choice prepares one ordinary restore without touching source");
		const auto chosen = apply(fixture.Tour, *fileChoice);
		expect(chosen.Changed && chosen.Accepted.Stops[8U].Original == fixture.Tour.Stops[8U].Original && chosen.Accepted.Stops[8U].Body == fixture.Tour.Stops[8U].Body, "file preference preserves original selection and prose");
		expect(!apply(chosen.Accepted, *fileChoice).Changed, "stale file choice refuses a replaced revision");
		for (const auto support : { tour_operation_support::unknown, tour_operation_support::denied, tour_operation_support::rate_limited, tour_operation_support::indeterminate }) {
			auto unknown           = observation;
			unknown.StandaloneFile = support;
			expect(!prepare_tour_file_comment(fixture.Tour, fixture.Tour.Stops[8U].Id, unknown, fixture.Files), "unqualified standalone file operation supplies no edit");
		}

		for (const auto support : { tour_operation_support::unknown, tour_operation_support::denied, tour_operation_support::rate_limited, tour_operation_support::indeterminate }) {
			observation.PendingOffHunkBatch = support;
			const auto repair               = plan_tour_stop_repair(fixture.Tour, fixture.Tour.Stops[8U], observation, fixture.Files);
			expect(!repair.FileComment && repair.Destination.NeedsQualification, "unknown denied throttled or uncertain operation never grants a fallback write");
		}
		observation = fixture.observation(fixture.Tour.Stops[8U]);
		observation.Source.FirstLine++;
		expect(!prepare_tour_source_repair(fixture.Tour, fixture.Tour.Stops[8U].Id, tour_source_repair_kind::nearest_hunk, observation, fixture.Files), "mismatched source observations refuse a repair");
		fixture.Tour.ReadOnly = true;
		expect(!prepare_tour_source_repair(fixture.Tour, fixture.Tour.Stops[0].Id, tour_source_repair_kind::repin_head, fixture.observation(fixture.Tour.Stops[0]), fixture.Files), "read-only tours cannot repair");
	}
	void line_references_require_reaffirmation()
	{
		for (const auto text : { "See line 19.", "Lines 19 to 20 explain it.", "See #L19.", "See L19-L20.", "See lines 1–19.", "Lines 1 through 20.", "L1-L19" }) {
			auto  fixture      = repair_fixture{ };
			auto& stop         = fixture.Tour.Stops[8U];
			stop.Body          = text;
			const auto command = prepare_tour_source_repair(fixture.Tour, stop.Id, tour_source_repair_kind::nearest_hunk, fixture.observation(stop), fixture.Files);
			expect(command && command->Restore->Stops[8U].Body == text && command->Restore->Stops[8U].Preparation == tour_preparation::needs_attention && !command->Restore->Stops[8U].ReaffirmedHead, "changed line references retain exact explanation and need explicit reaffirmation");
		}
		auto  fixture      = repair_fixture{ };
		auto& stop         = fixture.Tour.Stops[8U];
		stop.Body          = "Retry 19 times; baseline19 is the identifier.";
		const auto command = prepare_tour_source_repair(fixture.Tour, stop.Id, tour_source_repair_kind::nearest_hunk, fixture.observation(stop), fixture.Files);
		expect(command && command->Restore->Stops[8U].Preparation == tour_preparation::ready, "unrelated numbers and identifiers do not demand reaffirmation");
	}
	void nearest_hunk_boundaries()
	{
		const auto file = tour_anchor_file{ "file", true, 60U, std::vector<tour_line_span>{ { 10U, 16U }, { 30U, 36U } } };
		expect(nearest_tour_hunk({ 8U, 10U }, file) == tour_line_span{ 10U, 12U }, "preserves length inside context");
		expect(nearest_tour_hunk({ 18U, 26U }, file) == tour_line_span{ 10U, 16U }, "nearest hunk shortens an overlong range");
		expect(nearest_tour_hunk({ 23U, 23U }, file) == tour_line_span{ 16U, 16U }, "ties use provider order deterministically");
		auto invalid                 = file;
		invalid.Hunks->front().First = 0U;
		expect(!nearest_tour_hunk({ 8U, 10U }, invalid), "invalid provider span refuses");
		invalid        = file;
		invalid.OnHead = false;
		expect(!nearest_tour_hunk({ 8U, 10U }, invalid), "deleted file refuses");
		expect(!nearest_tour_hunk({ 0U, 0U }, file) && !nearest_tour_hunk({ 2U, 1U }, file), "invalid source ranges refuse without underflow");
		auto upper = tour_anchor_file{ "file", true, std::numeric_limits<std::uint32_t>::max(), std::vector<tour_line_span>{ { 0xFFFF'FFFAU, 0xFFFF'FFFFU } } };
		expect(nearest_tour_hunk({ 0xFFFF'FFF8U, 0xFFFF'FFF9U }, upper) == tour_line_span{ 0xFFFF'FFFAU, 0xFFFF'FFFBU }, "upper line bound arithmetic does not wrap");
	}
}

int main()
{
	repairs_compose_as_one_review();
	unsafe_sources_stay_unrepaired();
	line_references_require_reaffirmation();
	nearest_hunk_boundaries();
	std::cout << "tour-repairs:pass\n";
}
