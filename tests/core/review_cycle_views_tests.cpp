#include "bpr/core/diff_compute.hpp"
#include "product_diff.hpp"
#include "bpr/core/review_cycle_tabs.hpp"
#include "bpr/core/review_cycle_views.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Named comparison views, reviewer checkpoints, pinned and semantic
/// tabs and the separate revision chronologies (BPR-1163).

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle views failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename TIdentity>
	auto identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return TIdentity{ bytes };
	}

	auto make_cycle() -> review_cycle_state
	{
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = identity<cycle_catalog_incarnation>(1) };
		return create({ .Authority = authority }, { .Authority = authority, .Title = "Views", .Scope = { } })->Cycle;
	}

	auto captured(const review_cycle_state& aCycle, const std::uint8_t aMaterial, const bool aDeliberate = false) -> review_cycle_state
	{
		const auto outcome = apply(aCycle, { .Subject = aCycle.Subject, .ExpectedRevision = aCycle.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = identity<cycle_material_digest>(aMaterial), .Entries = 1, .Coverage = cycle_capture_coverage::complete }, .Deliberate = aDeliberate } });
		expect(outcome.Refusal == cycle_refusal::none, "capture");
		return outcome.Accepted;
	}

	auto id(const std::uint64_t aValue) -> cycle_source_revision_id
	{
		return cycle_source_revision_id{ aValue };
	}

	auto ref(const review_cycle_state& aCycle, const std::uint64_t aValue) -> cycle_source_revision_ref
	{
		return { .Cycle = aCycle.Subject.Cycle, .Id = id(aValue) };
	}

	auto pair(const review_cycle_state& aCycle, const std::uint64_t aLeft, const std::uint64_t aRight) -> cycle_comparison_pair
	{
		return { .Left = ref(aCycle, aLeft), .Right = ref(aCycle, aRight) };
	}

	/// R0, F1, R1, F2 (discussion only, on R1), R2: nothing but captures and
	/// the reviewer's own marks decides what each view shows.
	void resolves_the_manual_course()
	{
		auto       cycle = make_cycle();
		auto       facts = cycle_view_facts{ };
		const auto f1    = this_round_request{ .Round = cycle_round_id{ 1 } };
		expect(resolve_view(cycle, facts, all_cycle_request{ }).error() == cycle_view_problem::uncaptured, "nothing before R0");
		expect(!mark_reviewed(cycle, std::nullopt, id(1)).has_value(), "no checkpoint before R0");

		// R0 and F1 against it. The round's payload names R1 (local R1 is R0).
		cycle = captured(cycle, 1);
		expect(round_start("R1") == id(1) && !round_start("R0") && !round_start("R01") && !round_start("F1") && !round_start("R") && !round_start("R1x"), "a round names its start");
		facts.Rounds.push_back({ .Round = cycle_round_id{ 1 }, .Start = *round_start("R1") });
		expect(resolve_view(cycle, facts, f1) == pair(cycle, 1, 1), "F1 before any result shows no change");
		expect(resolve_view(cycle, facts, since_last_review_request{ }).error() == cycle_view_problem::no_review_checkpoint, "no checkpoint is guessed");
		expect(resolve_view(cycle, facts, this_round_request{ .Round = cycle_round_id{ 2 } }).error() == cycle_view_problem::unknown_round, "unknown round");

		// The recipient works with ordinary tools; the reviewer captures R1.
		cycle = captured(cycle, 2);
		expect(resolve_view(cycle, facts, f1) == pair(cycle, 1, 2), "this round: F1's start to the capture");
		expect(resolve_view(cycle, facts, all_cycle_request{ }) == pair(cycle, 1, 2), "all cycle");
		const auto marked = mark_reviewed(cycle, facts.LastReview, id(2));
		expect(marked && marked->Ordinal == 1U && marked->Revision == id(2), "reviewer marks R1 reviewed");
		facts.LastReview = *marked;
		expect(mark_reviewed(cycle, facts.LastReview, id(2)).error() == cycle_checkpoint_problem::unchanged, "same mark again is no change");
		expect(mark_reviewed(cycle, facts.LastReview, id(9)).error() == cycle_checkpoint_problem::unknown_revision, "an uncaptured revision cannot be marked");

		// F2 is a discussion round on the same source: no new code revision.
		facts.Rounds.push_back({ .Round = cycle_round_id{ 2 }, .Start = id(2) });
		const auto f2 = this_round_request{ .Round = cycle_round_id{ 2 } };
		expect(resolve_view(cycle, facts, f2) == pair(cycle, 2, 2), "same-source discussion round");
		expect(resolve_view(cycle, facts, since_last_review_request{ }) == pair(cycle, 2, 2), "nothing since my review");

		// A capture of identical bytes is no revision; a deliberate one is.
		const auto repeat = apply(cycle, { .Subject = cycle.Subject, .ExpectedRevision = cycle.Revision, .Change = record_cycle_capture{ .Capture = cycle.Latest->Capture } });
		expect(!repeat.Changed && repeat.Accepted.Latest->Id.Id == id(2), "no-change capture records nothing");

		cycle = captured(cycle, 3);
		expect(resolve_view(cycle, facts, f2) == pair(cycle, 2, 3), "F2's round from R1 to R2");
		expect(resolve_view(cycle, facts, f1) == pair(cycle, 1, 3), "F1 with no named result follows the latest");
		expect(resolve_view(cycle, facts, this_round_request{ .Round = cycle_round_id{ 1 }, .Result = id(2) }) == pair(cycle, 1, 2), "F1 with its named result");
		expect(resolve_view(cycle, facts, since_last_review_request{ }) == pair(cycle, 2, 3), "since my last review");
		expect(resolve_view(cycle, facts, all_cycle_request{ }) == pair(cycle, 1, 3), "all cycle");
		expect(resolve_view(cycle, facts, chosen_pair_request{ id(3), id(1) }) == pair(cycle, 3, 1), "an exact pair in either order");
		expect(resolve_view(cycle, facts, chosen_pair_request{ id(1), id(4) }).error() == cycle_view_problem::unknown_revision, "an uncaptured result is unavailable, not the latest");
		expect(resolve_view(cycle, facts, this_round_request{ .Round = cycle_round_id{ 1 }, .Result = id(7) }).error() == cycle_view_problem::unknown_revision, "a reported result without bytes is unavailable");

		// Pinned views signal newer material and never move; semantic ones would.
		expect(pinned(chosen_pair_request{ id(1), id(2) }) && pinned(this_round_request{ .Round = cycle_round_id{ 1 }, .Result = id(2) }), "pinned views");
		expect(!pinned(f1) && !pinned(since_last_review_request{ }) && !pinned(all_cycle_request{ }), "semantic views");
		const auto older = captured(captured(cycle, 3, true), 4);
		expect(view_newer_available(older, facts, chosen_pair_request{ id(1), id(3) }, pair(older, 1, 3)), "pinned pair: newer exists");
		expect(view_newer_available(older, facts, all_cycle_request{ }, pair(older, 1, 3)), "semantic: would move");
		expect(!view_newer_available(older, facts, all_cycle_request{ }, pair(older, 1, 5)), "semantic: current");
		expect(older.Latest->Id.Id == id(5) && same_material(*older.Latest, *older.Latest), "deliberate same-bytes observation is R3");
	}

	void tabs_keep_their_views()
	{
		auto cycle   = captured(captured(captured(make_cycle(), 1), 2), 3);
		auto facts   = cycle_view_facts{ .Rounds = { { .Round = cycle_round_id{ 1 }, .Start = id(1) } } };
		auto tabs    = cycle_tabs_state{ .Revision = id(3) };
		auto subject = cycle.Subject;
		expect(open_path(tabs, "src/a.cpp", false).Changed, "full source tab");
		expect(show_comparison(tabs, all_cycle_request{ }, { .Right = ref(cycle, 3) }).Reason == document_tab_reason::invalid_comparison, "a comparison needs a left side");
		expect(show_comparison(tabs, chosen_pair_request{ id(1), id(2) }, *resolve_view(cycle, facts, chosen_pair_request{ id(1), id(2) })).Changed, "choose revisions");
		expect(shown_pair(subject, active_tab(tabs)->View) == pair(cycle, 1, 2), "the tab shows the exact pair");
		expect(!show_comparison(tabs, chosen_pair_request{ id(1), id(2) }, pair(cycle, 1, 2)).Changed, "same pair again");
		// Another file in the same tab keeps the pinned pair.
		expect(open_path(tabs, "src/b.cpp", false).Changed && shown_pair(subject, active_tab(tabs)->View) == pair(cycle, 1, 2), "another file, same pair");
		expect(open_path(tabs, "src/a.cpp", true).Changed && tabs.Tabs[1].View.Revision == id(3) && !tabs.Tabs[1].View.Comparison, "a new tab is full source on the new-tab revision");

		cycle = captured(cycle, 4);
		expect(view_newer_available(cycle, facts, active_tab(tabs)->View.Comparison->Request, shown_pair(subject, active_tab(tabs)->View)), "newer signalled");
		expect(refresh_active(tabs, std::optional{ *resolve_view(cycle, facts, chosen_pair_request{ id(1), id(2) }) }, id(4)).Reason == document_tab_reason::invalid_comparison, "a pinned pair never refreshes");
		expect(shown_pair(subject, active_tab(tabs)->View) == pair(cycle, 1, 2), "never retargeted");

		// A semantic view moves only on the reviewer's refresh.
		expect(show_comparison(tabs, this_round_request{ .Round = cycle_round_id{ 1 } }, pair(cycle, 1, 3)).Changed, "this round as it was");
		expect(refresh_active(tabs, std::optional{ *resolve_view(cycle, facts, this_round_request{ .Round = cycle_round_id{ 1 } }) }, id(4)).Changed, "refresh");
		expect(shown_pair(subject, active_tab(tabs)->View) == pair(cycle, 1, 4), "refresh moves the semantic view");
		expect(show_full_source(tabs).Changed && !active_tab(tabs)->View.Comparison && active_tab(tabs)->View.Revision == id(4), "back to full source at the right side");
		expect(!show_full_source(tabs).Changed, "already full source");
		expect(refresh_active(tabs, std::nullopt, id(4)).Changed == false, "full source already latest");

		// Restoration keeps comparisons exactly and refuses foreign revisions.
		expect(show_comparison(tabs, since_last_review_request{ }, pair(cycle, 2, 4)).Changed, "since my last review");
		const auto record   = persisted(tabs);
		auto       restored = cycle_tabs_state{ };
		expect(restore(restored, cycle, record).Reason == document_tab_reason::none && persisted(restored) == record, "comparisons restored");
		auto forged                = record;
		forged.Views[0].Comparison = cycle_tab_comparison{ .Request = all_cycle_request{ }, .Left = id(9) };
		auto refused               = cycle_tabs_state{ };
		expect(restore(refused, cycle, forged).Reason == document_tab_reason::invalid_restoration, "an unallocated left side is refused");
		forged.Views[0].Comparison = cycle_tab_comparison{ .Request = chosen_pair_request{ id(1), id(8) }, .Left = id(1) };
		expect(restore(refused, cycle, forged).Reason == document_tab_reason::invalid_restoration, "an unallocated request is refused");
		forged.Views[0].Comparison = cycle_tab_comparison{ .Request = this_round_request{ .Round = cycle_round_id{ 5 } }, .Left = id(1) };
		expect(restore(refused, cycle, forged).Reason == document_tab_reason::invalid_restoration, "a round never frozen is refused");
	}

	auto source(const review_cycle_state& aCycle, const std::uint64_t aRevision, const std::optional<std::string>& aReference, const std::uint8_t aCommit, const bool aRepository = true)
		-> cycle_revision_source
	{
		auto revision = cycle_source_revision{ .Id = ref(aCycle, aRevision), .Capture = { .Material = identity<cycle_material_digest>(static_cast<std::uint8_t>(aRevision)), .Coverage = cycle_capture_coverage::complete } };
		if (aRevision > 1U) {
			revision.Parent = id(aRevision - 1U);
		}
		auto value = cycle_revision_source{ .Revision = revision, .HeadReference = aReference, .Repository = aRepository };
		if (aCommit != 0U) {
			auto bytes       = std::array<std::uint8_t, 32>{ };
			bytes[0]         = aCommit;
			value.HeadCommit = git_commit_oid{ git_hash_algorithm::sha1, bytes };
		}
		return value;
	}

	void keeps_chronologies_apart()
	{
		const auto cycle = make_cycle();
		auto       list  = std::vector<cycle_revision_source>{
			source(cycle, 1, "refs/heads/main", 0xA1),
			source(cycle, 2, "refs/heads/main", 0xA1),
			source(cycle, 3, "refs/heads/main", 0xB2),
			source(cycle, 4, "refs/heads/feature", 0xC3),
			source(cycle, 5, std::nullopt, 0xC3),
			source(cycle, 6, std::nullopt, 0xC3),
			source(cycle, 8, std::nullopt, 0, false),
		};
		list[1].Revision.Capture.Material = list[0].Revision.Capture.Material;
		list[6].Revision.Parent           = id(5);
		const auto imported               = std::vector<cycle_imported_revision>{ { .ForeignCycle = "RC-9", .ForeignRound = "F1", .ForeignRevision = "R4", .Local = id(3) } };
		const auto history                = describe_history(list, imported);
		expect(history.Entries.size() == 7U && !history.ProviderChronology, "no provider chronology is inferred");
		expect(history.Entries[0].Step == cycle_revision_step::initial, "R0");
		expect(history.Entries[1].Step == cycle_revision_step::same_head && history.Entries[1].SameMaterial, "same head, same bytes: a deliberate observation");
		expect(history.Entries[2].Step == cycle_revision_step::head_moved && !history.Entries[2].SameMaterial, "head moved: commits between were not captured");
		expect(history.Entries[3].Step == cycle_revision_step::branch_changed, "another branch: divergence is explicit");
		expect(history.Entries[4].Step == cycle_revision_step::branch_changed, "detaching is a change of line");
		expect(history.Entries[5].Step == cycle_revision_step::same_head, "the same detached commit");
		expect(history.Unlisted == 1U && history.Entries[6].Step == cycle_revision_step::no_repository && history.Entries[6].ParentGap, "a missed capture and a lineage gap are both explicit");
		expect(history.Entries[2].Imported.size() == 1U && history.Entries[2].Imported[0].ForeignRevision == "R4", "the sender's revision stays the sender's name");
		const auto page = describe_history(std::span{ list }.subspan(2), { });
		expect(page.Entries[0].Step == cycle_revision_step::previous_unlisted, "a page does not guess the capture before it");
	}

	void rows_follow_the_alignment()
	{
		const auto cycle = captured(captured(make_cycle(), 1), 2);
		const auto key   = cycle_file_comparison(cycle.Subject, pair(cycle, 1, 2), "a.txt", identity<cycle_material_digest>(1), identity<cycle_material_digest>(2), { });
		const auto bytes = [](const std::string_view aText) { return std::vector<std::uint8_t>(aText.begin(), aText.end()); };
		const auto diff  = bpr::tests::product_material_diff({ .Key = *key, .LeftBytes = bytes("one\ntwo\r\nthree\n"), .RightBytes = bytes("one\n2\nthree\nfour\n") });
		expect(diff.published(), "diff computed");
		const auto rows = diff_rows(*diff.Result);
		expect(rows.size() == 5U && rows[3].Kind == cycle_diff_row_kind::context && rows[3].Text == "three", "five rows");
		expect(rows[0].Kind == cycle_diff_row_kind::context && rows[0].Text == "one" && rows[0].LeftLine == 1U && rows[0].RightLine == 1U, "context");
		expect(rows[1].Kind == cycle_diff_row_kind::removed && rows[1].Text == "two" && rows[1].LeftLine == 2U && !rows[1].RightLine, "removed without its terminator");
		expect(rows[2].Kind == cycle_diff_row_kind::added && rows[2].Text == "2" && rows[2].RightLine == 2U, "added");
		expect(rows[4].Kind == cycle_diff_row_kind::added && rows[4].Text == "four" && rows[4].RightLine == 4U, "trailing addition");
	}
}

auto main() -> int
{
	resolves_the_manual_course();
	tabs_keep_their_views();
	keeps_chronologies_apart();
	rows_follow_the_alignment();
	std::cout << "review-cycle-views-tests:pass\n";
	return EXIT_SUCCESS;
}
