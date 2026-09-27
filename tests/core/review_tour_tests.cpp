#include "bpr/core/review_tour.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
	using namespace bpr::core;
	void expect(bool aCondition, const char* aReason)
	{
		if (!aCondition) {
			std::cerr << aReason << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	template <typename T>
	auto identity(unsigned char aSeed) -> T
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0] = aSeed;
		return T{ bytes };
	}
	auto source(std::uint32_t aFirst = 3, std::uint32_t aLast = 5) -> tour_source_range
	{
		const auto comparison = identity<file_comparison_digest>(2);
		return { { comparison, diff_side::right, aFirst }, { comparison, diff_side::right, aLast }, *parse_git_commit_oid("1111111111111111111111111111111111111111"), { }, "src/item.cpp" };
	}
	auto draft() -> review_tour
	{
		return { .Id = tour_draft_id{ 1 }, .Owner = { bpr_profile_id{ 1 }, git_host_id{ 2 }, identity<host_authority_fingerprint>(3), github_account_id{ 4 }, repository_id{ 5 }, pull_request_id{ 6 } }, .Base = *parse_git_commit_oid("2222222222222222222222222222222222222222"), .Head = source().Commit };
	}
	auto command(const review_tour& aDraft, tour_command_kind aKind) -> tour_command
	{
		return { .Draft = aDraft.Id, .Owner = aDraft.Owner, .ExpectedRevision = aDraft.Revision, .Kind = aKind };
	}
	auto apply_ok(review_tour& aDraft, tour_command aCommand) -> tour_outcome
	{
		auto result = apply(aDraft, aCommand);
		expect(result.Refusal == tour_refusal::none, "command unexpectedly refused");
		aDraft = result.Accepted;
		return result;
	}
	void check_commands()
	{
		auto tour = draft();
		expect(validate(tour) == tour_refusal::none, "empty draft is saved");
		auto brief = command(tour, tour_command_kind::problem);
		brief.Text = "Crash when reading data that's been concurrently deleted.";
		apply_ok(tour, brief);
		expect(apply(tour, brief).Refusal == tour_refusal::conflict, "stale writer refused");
		brief      = command(tour, tour_command_kind::solution);
		brief.Text = "Retain the snapshot.";
		apply_ok(tour, brief);
		expect(tour.Problem != tour.Solution, "brief fields independent");
		auto add            = command(tour, tour_command_kind::insert);
		add.Value           = { .Title = "Why", .Body = "first\nsecond", .Original = source(), .Mapping = tour_mapping::exact_original, .Related = { source(10, 20) } };
		auto       result   = apply_ok(tour, add);
		const auto original = result.Selected;
		auto       split    = command(tour, tour_command_kind::split);
		split.Stop          = original;
		split.SplitByte     = 6;
		const auto before   = tour;
		result              = apply_ok(tour, split);
		expect(tour.Stops[0].Body + tour.Stops[1].Body == before.Stops[0].Body && tour.Stops[1].Related == before.Stops[0].Related, "split retains exact text and links");
		expect(result.Selected != original, "split fresh ID");
		auto merge = command(tour, tour_command_kind::merge_next);
		merge.Stop = original;
		apply_ok(tour, merge);
		expect(tour.Stops[0].Body == before.Stops[0].Body, "merge exact split prose");
		auto duplicate    = command(tour, tour_command_kind::duplicate);
		duplicate.Stop    = original;
		result            = apply_ok(tour, duplicate);
		const auto second = result.Selected;
		expect(tour.Stops[0].Title == tour.Stops[1].Title && original != second, "titles are not identities");
		auto move     = command(tour, tour_command_kind::move);
		move.Stop     = second;
		move.Position = 0;
		apply_ok(tour, move);
		expect(tour.Stops[0].Id == second, "reordered independently of identity");
		move          = command(tour, tour_command_kind::move);
		move.Stop     = second;
		move.Position = 1;
		apply_ok(tour, move);
		expect(tour.Stops[0].Id == original, "reorder inverse");
		auto complete = command(tour, tour_command_kind::complete);
		complete.Stop = second;
		apply_ok(tour, complete);
		expect(tour.Stops[1].Preparation == tour_preparation::ready, "manual completion");
		auto head = command(tour, tour_command_kind::observe_head);
		head.Head = tour.Base;
		apply_ok(tour, head);
		expect(tour.Stops[1].Preparation == tour_preparation::needs_attention && tour.Head == before.Head && tour.Stops[1].Original == before.Stops[0].Original, "head change preserves original binding");
		complete      = command(tour, tour_command_kind::complete);
		complete.Stop = second;
		expect(apply(tour, complete).Refusal == tour_refusal::incomplete, "unresolved source cannot be ready");
		auto restore    = command(tour, tour_command_kind::restore);
		restore.Restore = before;
		apply_ok(tour, restore);
		expect(tour.Stops == before.Stops && tour.LastStopId == second.value(), "undo retains high water and source text");
		auto remove = command(tour, tour_command_kind::remove);
		remove.Stop = original;
		apply_ok(tour, remove);
		expect(tour.Stops.empty(), "remove");
		add       = command(tour, tour_command_kind::insert);
		add.Value = before.Stops[0];
		apply_ok(tour, add);
		expect(tour.Stops[0].Id.value() > second.value(), "removed ID never reused");
		auto wrong          = command(tour, tour_command_kind::problem);
		wrong.Owner.Profile = bpr_profile_id{ 12 };
		wrong.Text          = "overwrite";
		expect(apply(tour, wrong).Refusal == tour_refusal::identity && apply(tour, wrong).Accepted == tour, "profile replacement refused");
		wrong.Owner         = tour.Owner;
		wrong.Owner.Account = github_account_id{ 12 };
		expect(apply(tour, wrong).Refusal == tour_refusal::identity, "account replacement refused");
	}
	void check_ranges_and_bounds()
	{
		auto range = source();
		expect(valid(range), "inclusive source range");
		range.Last.Side = diff_side::left;
		expect(!valid(range), "cross-side range refused");
		range                 = source();
		range.Last.Comparison = identity<file_comparison_digest>(7);
		expect(!valid(range), "cross-comparison refused");
		range = source(5, 3);
		expect(!valid(range), "inverted source refused");
		range = source(0, 3);
		expect(!valid(range), "zero source refused");
		range            = source();
		range.First.Side = range.Last.Side = diff_side::left;
		expect(valid(range), "historical supporting source remains representable");
		auto tour = draft();
		auto edit = command(tour, tour_command_kind::problem);
		edit.Text = std::string(maximum_tour_field_bytes, 'x');
		apply_ok(tour, edit);
		edit      = command(tour, tour_command_kind::problem);
		edit.Text = std::string(maximum_tour_field_bytes + 1, 'x');
		expect(apply(tour, edit).Refusal == tour_refusal::bounds, "field limit refuses without truncating");
		for (std::size_t index = 0; index < maximum_tour_stops; ++index) {
			auto add     = command(tour, tour_command_kind::insert);
			add.Position = index;
			add.Value    = { .Title = "Why", .Body = "Ordinary explanation", .Original = source(), .Mapping = tour_mapping::exact_original };
			apply_ok(tour, add);
		}
		auto add     = command(tour, tour_command_kind::insert);
		add.Position = tour.Stops.size();
		expect(apply(tour, add).Refusal == tour_refusal::bounds, "257th stop refused");
		auto full       = draft();
		full.Stops      = { tour.Stops.front() };
		full.LastStopId = 1;
		full.Problem    = std::string(maximum_tour_field_bytes, 'p');
		full.Solution   = full.Problem;
		while (retained_tour_bytes(full) + maximum_tour_field_bytes + 1024U <= maximum_tour_bytes) {
			auto stop = full.Stops.front();
			stop.Id   = tour_stop_id{ ++full.LastStopId };
			stop.Body = std::string(maximum_tour_field_bytes, 'b');
			full.Stops.push_back(stop);
		}
		expect(validate(full) == tour_refusal::none, "large admitted tour");
		while (retained_tour_bytes(full) <= maximum_tour_bytes) {
			auto stop = full.Stops.front();
			stop.Id   = tour_stop_id{ ++full.LastStopId };
			stop.Body = std::string(maximum_tour_field_bytes, 'b');
			full.Stops.push_back(stop);
		}
		expect(validate(full) == tour_refusal::bounds, "aggregate includes overhead");
		tour                = draft();
		auto       skeleton = command(tour, tour_command_kind::insert);
		const auto inserted = apply_ok(tour, skeleton);
		auto       complete = command(tour, tour_command_kind::complete);
		complete.Stop       = inserted.Selected;
		expect(apply(tour, complete).Refusal == tour_refusal::incomplete, "skeleton saved but not completed");
		tour.Revision = std::numeric_limits<std::uint64_t>::max();
		expect(apply(tour, command(tour, tour_command_kind::problem)).Refusal == tour_refusal::exhausted, "generation overflow refused");
	}
}
int main()
{
	check_commands();
	check_ranges_and_bounds();
}
