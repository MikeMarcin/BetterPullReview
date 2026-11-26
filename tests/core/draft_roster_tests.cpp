// [[BPR-2092]] [[BPR-2093]] [[BPR-2094]]. Every unsent draft of mine: where it
// is, where it stands with the review, the per-file counts, the drafts Finish
// your review notes, and what Add marks.

#include "bpr/core/draft_roster.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "draft roster failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto digest(const std::uint8_t aSeed) -> file_comparison_digest
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = 0x13U;
		return file_comparison_digest{ bytes };
	}

	// NOLINTBEGIN(bugprone-throwing-static-initialization): fixture values; a failure to build them ends the test.
	const auto head    = *parse_git_commit_oid("1111111111111111111111111111111111111111");
	const auto earlier = *parse_git_commit_oid("2222222222222222222222222222222222222222");
	// NOLINTEND(bugprone-throwing-static-initialization)

	[[nodiscard]] auto saved(const std::uint64_t aId, const std::uint8_t aComparison, const std::uint32_t aLine, std::string aBody) -> revisioned_review_draft
	{
		return { .Draft = { .Id = review_draft_id{ aId }, .Anchor = { .Comparison = digest(aComparison), .Side = diff_side::right, .Line = aLine }, .Body = std::move(aBody) }, .Revision = 4U, .Scope = review_draft_scope_id{ 3 } };
	}

	[[nodiscard]] auto placed(revisioned_review_draft aDraft, std::string aPath, const bool aReviewable) -> revisioned_review_draft
	{
		aDraft.Draft.Place = review_draft_place{ .Path = std::move(aPath), .ReviewHead = aReviewable ? std::optional{ head } : std::nullopt };
		return aDraft;
	}

	[[nodiscard]] auto pending(revisioned_review_draft aDraft, std::string aPath) -> revisioned_review_draft
	{
		aDraft.Draft.InReview = review_pending_mark{ .Path = std::move(aPath), .Head = head, .Submission = { } };
		return aDraft;
	}

	// NOLINTBEGIN(bugprone-throwing-static-initialization): fixture values; a failure to build them ends the test.
	const auto shown   = std::vector<std::pair<file_comparison_digest, std::string>>{ { digest(9), "src/old.cpp" } };
	const auto threads = std::vector<std::pair<std::string, std::string>>{ { "T1", "src/reply.cpp" } };
	const auto facts   = draft_roster_facts{ .Head = head, .Shown = shown, .Threads = threads };
	// NOLINTEND(bugprone-throwing-static-initialization)

	void test_standing_place_and_order()
	{
		auto reply                      = saved(5, 7, 3, "Agreed.");
		reply.Draft.ReplyToThread       = "T1";
		auto published                  = placed(saved(6, 1, 1, "sent"), "src/a.cpp", true);
		published.Draft.State           = review_draft_state::published;
		auto held                       = pending(saved(8, 1, 30, "held"), "src/a.cpp");
		held.Draft.State                = review_draft_state::publishing;
		held.Draft.InReview->Submission = "key-1";
		const auto drafts               = std::vector{
			placed(saved(1, 1, 20, "not in review, reviewable"), "src/b.cpp", true),
			pending(placed(saved(2, 1, 10, "in review"), "src/a.cpp", true), "src/a.cpp"),
			placed(saved(3, 2, 4, "written on a filtered pair"), "src/a.cpp", false),
			saved(4, 9, 2, "kept before places were recorded"),
			saved(10, 8, 2, "file never shown"),
			reply,
			published,
			held,
			placed(saved(11, 1, 12, "  \n"), "src/a.cpp", true),
		};
		const auto roster = project_draft_roster(drafts, facts);
		expect(roster.Entries.size() == 7U, "published and blank drafts are not listed");
		expect(roster.InReview == 1U && roster.NotInReview == 4U && roster.Held == 1U && roster.Replies == 1U, "counts by standing");
		auto order = std::vector<std::uint64_t>{ };
		for (const auto& entry : roster.Entries) {
			order.push_back(entry.Id.value());
		}
		expect(order == std::vector<std::uint64_t>{ 3U, 2U, 8U, 1U, 4U, 5U, 10U }, "file order, then line; unknown files last");
		expect(roster.Entries[4].Path == "src/old.cpp", "a draft without a place takes its shown comparison's file");
		expect(roster.Entries[5].Path == "src/reply.cpp", "a reply takes its thread's file");
		expect(roster.Entries[6].Path.empty(), "a draft whose file is not known has no path");
	}

	void test_add_refusals_and_marks()
	{
		auto reply                = placed(saved(5, 7, 3, "Agreed."), "src/reply.cpp", false);
		reply.Draft.ReplyToThread = "T1";
		const auto drafts         = std::vector{
			placed(saved(1, 1, 20, "reviewable"), "src/b.cpp", true),
			placed(saved(3, 2, 4, "filtered"), "src/a.cpp", false),
			saved(4, 9, 2, "old"),
			reply,
		};
		const auto roster  = project_draft_roster(drafts, facts);
		const auto outside = drafts_outside_review(roster);
		expect(outside.size() == 4U, "every draft the review does not carry is noted");
		const auto entryOf = [&outside](const std::uint64_t aId) -> draft_roster_entry { return *std::ranges::find(outside, review_draft_id{ aId }, &draft_roster_entry::Id); };
		expect(entryOf(1).Refusal == draft_add_refusal::none && entryOf(1).Mark == std::optional{ review_pending_mark{ .Path = "src/b.cpp", .Head = head, .Submission = { } } }, "a reviewable draft is marked at its place and the head");
		expect(entryOf(3).Refusal == draft_add_refusal::not_at_head && !entryOf(3).Mark, "a draft no review can take at its line is not marked");
		expect(entryOf(4).Refusal == draft_add_refusal::place_unknown, "a draft without a place must be opened");
		expect(entryOf(5).Refusal == draft_add_refusal::reply, "a reply is never added");

		auto moved = facts;
		moved.Head = earlier;
		expect(drafts_outside_review(project_draft_roster(drafts, moved))[1].Refusal == draft_add_refusal::not_at_head, "a place recorded at another head is not marked");

		const auto all      = std::vector{ review_draft_id{ 1 }, review_draft_id{ 3 }, review_draft_id{ 4 }, review_draft_id{ 5 } };
		const auto commands = plan_add_to_review(roster, all);
		expect(commands.size() == 1U, "Add all adds only what a review can take");
		expect(commands[0].Command.Kind == review_draft_command_kind::edit && commands[0].ChangePending && commands[0].ExpectedRevision == 4U && commands[0].Command.Body == "reviewable", "Add is an edit at the listed revision setting the mark");
		const auto accepted = apply(drafts[0], commands[0]);
		expect(accepted.Refusal == revisioned_draft_refusal::none && pending_in_review(accepted.Accepted.Draft) && accepted.Accepted.Draft.Body == "reviewable", "the command is one the draft policy accepts");
		expect(plan_add_to_review(roster, std::vector{ review_draft_id{ 3 } }).empty(), "a chosen draft a review cannot take adds nothing");
	}

	void test_counts_by_file()
	{
		const auto drafts = std::vector{
			placed(saved(1, 1, 20, "b one"), "src/b.cpp", true),
			pending(placed(saved(2, 1, 10, "a in"), "src/a.cpp", true), "src/a.cpp"),
			placed(saved(3, 1, 11, "a out"), "src/a.cpp", true),
			pending(placed(saved(6, 1, 12, "a in 2"), "src/a.cpp", true), "src/a.cpp"),
			saved(10, 8, 2, "unknown"),
		};
		const auto counts = draft_counts_by_file(project_draft_roster(drafts, facts));
		expect(counts == std::vector<draft_file_count>{ { .Path = "src/a.cpp", .InReview = 2U, .NotInReview = 1U }, { .Path = "src/b.cpp", .InReview = 0U, .NotInReview = 1U } }, "per file, in and not in the review; unknown files uncounted");
	}

	void test_place_is_recorded_only_by_an_edit()
	{
		const auto draft = saved(1, 1, 20, "body");
		auto       edit  = revisioned_review_draft_command{ .Command = { .Id = review_draft_id{ 1 }, .Kind = review_draft_command_kind::edit, .Body = "body" }, .ExpectedRevision = 4U, .Scope = review_draft_scope_id{ 3 } };
		edit.ChangePlace = true;
		edit.Place       = review_draft_place{ .Path = "src/b.cpp", .ReviewHead = head };
		const auto done  = apply(draft, edit);
		expect(done.Refusal == revisioned_draft_refusal::none && done.Changed && done.Accepted.Draft.Place == edit.Place && done.Accepted.Revision == 5U, "an edit records the place in a new revision");
		expect(!apply(done.Accepted, [&] { auto again = edit; again.ExpectedRevision = 5U; return again; }()).Changed, "the same place again changes nothing");
		auto discard         = edit;
		discard.Command.Kind = review_draft_command_kind::discard;
		expect(apply(draft, discard).Refusal == revisioned_draft_refusal::invalid_command, "only an edit records a place");
		auto stray        = edit;
		stray.ChangePlace = false;
		expect(apply(draft, stray).Refusal == revisioned_draft_refusal::invalid_command, "a place without ChangePlace is refused");
		auto empty = edit;
		empty.Place->Path.clear();
		expect(apply(draft, empty).Refusal == revisioned_draft_refusal::invalid_command, "an empty path is refused");
		auto huge        = edit;
		huge.Place->Path = std::string(maximum_pending_mark_path_bytes + 1U, 'a');
		expect(apply(draft, huge).Refusal == revisioned_draft_refusal::invalid_command, "an unbounded path is refused");
	}
} // namespace

int main()
{
	test_standing_place_and_order();
	test_add_refusals_and_marks();
	test_counts_by_file();
	test_place_is_recorded_only_by_an_edit();
	return EXIT_SUCCESS;
}
