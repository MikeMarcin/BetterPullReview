// RFC-BPR-0127 ([[BPR-2157]]). One comment in three states: a draft is never
// Local, Keep local moves a draft's text, line or range and reply target out
// of the review and is refused while a review holds it or once it is
// published, and Make draft moves a Local comment back to the same kind at
// the same place, or only to a selected line, refused where my draft stands.
#include "bpr/core/local_comment.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto blob(const std::uint8_t aSeed) -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto draft(const std::uint32_t aLine, const std::string_view aBody) -> core::review_comment_draft
	{
		auto result   = core::review_comment_draft{ };
		result.Id     = core::review_draft_id{ 7U };
		result.Anchor = core::source_line_ref{ .Comparison = identity<core::file_comparison_digest>(2U), .Side = core::diff_side::right, .Line = aLine };
		result.Body   = std::string{ aBody };
		return result;
	}

	void a_draft_is_never_local()
	{
		auto written = draft(10U, "words");
		expect(core::state_of(written) == core::comment_state::draft, "a draft being written is Draft");
		written.State = core::review_draft_state::publishing;
		expect(core::state_of(written) == core::comment_state::draft, "a draft being published is still Draft");
		written.State = core::review_draft_state::published;
		expect(core::state_of(written) == core::comment_state::published, "a published draft is Published");
		expect(core::name(core::comment_state::local) == "local" && core::name(core::keep_local_refusal::held_by_review) == "held_by_review" && core::name(core::make_draft_refusal::draft_stands) == "draft_stands", "stable names");
	}

	void keep_local_keeps_the_line_the_range_and_the_reply()
	{
		const auto file = identity<core::file_identity>(1U);
		// A range in the review: out of it, the same range, the blob recorded.
		auto ranged      = draft(14U, "a range");
		ranged.StartLine = 11U;
		ranged.InReview  = core::review_pending_mark{ .Path = "src/a.cpp", .Head = core::git_commit_oid{ core::git_hash_algorithm::sha1, identity<core::file_identity>(9U).bytes() }, .Submission = { } };
		const auto kept  = core::decide_keep_local({ .Draft = ranged, .File = file, .ShownBlob = blob(40U) });
		expect(kept && kept->Place.File == file && kept->Place.Comparison == ranged.Anchor.Comparison && kept->Place.Line == core::note_line_anchor{ .Side = core::diff_side::right, .Line = 14U, .Blob = blob(40U) }
				   && kept->StartLine == std::optional{ 11U } && !kept->ReplyToThread && core::valid(*kept),
			   "Keep local keeps a range's lines and records the shown blob");
		// A reply on a line keeps its thread and its line.
		auto reply          = draft(20U, "a reply");
		reply.ReplyToThread = "PRRT_kwDOthread";
		const auto replied  = core::decide_keep_local({ .Draft = reply, .File = file, .ShownBlob = std::nullopt });
		expect(replied && replied->ReplyToThread == reply.ReplyToThread && replied->Place.Line && !replied->Place.Line->Blob && !replied->StartLine && core::valid(*replied),
			   "Keep local keeps a reply's thread; with its comparison not shown, no blob");
		// A reply bound to its thread alone has no line.
		auto bound          = draft(0U, "on an outdated thread");
		bound.Anchor        = core::thread_reply_anchor(identity<core::file_comparison_digest>(2U));
		bound.ReplyToThread = "PRRT_outdated";
		const auto unbound  = core::decide_keep_local({ .Draft = bound, .File = file, .ShownBlob = blob(40U) });
		expect(unbound && !unbound->Place.Line && unbound->ReplyToThread == bound.ReplyToThread && core::valid(*unbound), "a thread-bound reply kept local has no line of its own");
	}

	void keep_local_is_refused_while_a_review_holds_it_and_once_published()
	{
		const auto file = identity<core::file_identity>(1U);
		auto       held = draft(3U, "held");
		held.State      = core::review_draft_state::publishing;
		held.InReview   = core::review_pending_mark{ .Path = "src/a.cpp", .Head = core::git_commit_oid{ core::git_hash_algorithm::sha1, identity<core::file_identity>(9U).bytes() }, .Submission = "submission-1" };
		expect(core::decide_keep_local({ .Draft = held, .File = file }).error() == core::keep_local_refusal::held_by_review, "a draft a submitted review holds is refused");
		auto publishing  = draft(3U, "sending");
		publishing.State = core::review_draft_state::publishing;
		expect(core::decide_keep_local({ .Draft = publishing, .File = file }).error() == core::keep_local_refusal::publishing, "a draft being published on its own is refused");
		auto published  = draft(3U, "sent");
		published.State = core::review_draft_state::published;
		expect(core::decide_keep_local({ .Draft = published, .File = file }).error() == core::keep_local_refusal::published, "a published comment is GitHub's");
		auto discarded  = draft(3U, "gone");
		discarded.State = core::review_draft_state::discarded;
		expect(core::decide_keep_local({ .Draft = discarded, .File = file }).error() == core::keep_local_refusal::discarded, "a discarded draft is refused");
	}

	void make_draft_returns_the_same_kind_at_the_same_place()
	{
		const auto file   = identity<core::file_identity>(1U);
		auto       ranged = draft(14U, "a range");
		ranged.StartLine  = 11U;
		const auto kept   = *core::decide_keep_local({ .Draft = ranged, .File = file, .ShownBlob = blob(40U) });
		const auto placed = core::note_placement{ .Kind = core::note_placement_kind::placed, .Reason = core::note_outdated_reason::none, .At = ranged.Anchor };
		const auto none   = std::vector<core::review_comment_draft>{ };
		const auto back   = core::decide_make_draft({ .Anchor = kept, .Placement = placed, .Shown = ranged.Anchor.Comparison, .Selected = std::nullopt, .Drafts = none });
		expect(back && back->Anchor == ranged.Anchor && back->StartLine == ranged.StartLine && !back->ReplyToThread && !back->Reanchored, "Make draft on a kept range returns the same range");
		// Placed by the same blob in a later comparison, the range moves with its last line.
		auto later      = placed;
		later.At        = core::source_line_ref{ .Comparison = identity<core::file_comparison_digest>(5U), .Side = core::diff_side::right, .Line = 14U };
		const auto next = core::decide_make_draft({ .Anchor = kept, .Placement = later, .Shown = later.At->Comparison, .Drafts = none });
		expect(next && next->Anchor == *later.At && next->StartLine == std::optional{ 11U }, "in another comparison showing the same text, at the same lines");
		// A Local reply becomes a reply to its thread, bound to it.
		auto reply          = draft(20U, "a reply");
		reply.ReplyToThread = "PRRT_kwDOthread";
		const auto local    = *core::decide_keep_local({ .Draft = reply, .File = file });
		const auto answer   = core::decide_make_draft({ .Anchor = local, .Placement = { }, .Shown = std::nullopt, .Drafts = none });
		expect(answer && answer->ReplyToThread == reply.ReplyToThread && core::thread_bound_anchor(answer->Anchor) && answer->Anchor.Comparison == reply.Anchor.Comparison, "a Local reply becomes a reply to its thread");
	}

	void make_draft_off_its_line_asks_for_a_line_and_never_merges()
	{
		auto anchor             = core::local_comment_anchor{ };
		anchor.Place.File       = identity<core::file_identity>(1U);
		anchor.Place.Comparison = identity<core::file_comparison_digest>(2U);
		anchor.Place.Line       = core::note_line_anchor{ .Side = core::diff_side::right, .Line = 30U, .Blob = blob(40U) };
		const auto outdated     = core::note_placement{ .Kind = core::note_placement_kind::outdated, .Reason = core::note_outdated_reason::content_changed, .At = std::nullopt };
		const auto none         = std::vector<core::review_comment_draft>{ };
		expect(core::decide_make_draft({ .Anchor = anchor, .Placement = outdated, .Drafts = none }).error() == core::make_draft_refusal::select_a_line, "an outdated Local comment asks for a line");
		const auto selected = core::source_line_ref{ .Comparison = identity<core::file_comparison_digest>(5U), .Side = core::diff_side::right, .Line = 8U };
		const auto onLine   = core::decide_make_draft({ .Anchor = anchor, .Placement = outdated, .Shown = selected.Comparison, .Selected = selected, .Drafts = none });
		expect(onLine && onLine->Anchor == selected && onLine->Reanchored && !onLine->StartLine, "with a line selected it is Make draft on line N");
		auto whole = core::local_comment_anchor{ };
		expect(core::decide_make_draft({ .Anchor = whole, .Placement = { .Kind = core::note_placement_kind::pull_request }, .Drafts = none }).error() == core::make_draft_refusal::select_a_line, "a comment on the pull request asks for a line");
		// My draft with words at the line refuses it; an empty one does not.
		auto standing   = draft(8U, "mine");
		standing.Anchor = selected;
		auto stands     = std::vector{ standing };
		expect(core::decide_make_draft({ .Anchor = anchor, .Placement = outdated, .Shown = selected.Comparison, .Selected = selected, .Drafts = stands }).error() == core::make_draft_refusal::draft_stands, "nothing is merged into my draft");
		stands.front().Body.clear();
		expect(core::decide_make_draft({ .Anchor = anchor, .Placement = outdated, .Shown = selected.Comparison, .Selected = selected, .Drafts = stands }).has_value(), "an empty placeholder does not stand");
		auto reply             = core::local_comment_anchor{ .Place = anchor.Place, .StartLine = std::nullopt, .ReplyToThread = std::string{ "PRRT_x" } };
		auto replying          = draft(9U, "my reply");
		replying.ReplyToThread = "PRRT_x";
		expect(core::decide_make_draft({ .Anchor = reply, .Placement = outdated, .Drafts = std::vector{ replying } }).error() == core::make_draft_refusal::draft_stands, "a thread with my draft reply refuses another");
	}

	void a_place_is_checked_whole()
	{
		auto place = core::local_comment_anchor{ };
		expect(core::valid(place), "the pull request is a place");
		place.Place.Line = core::note_line_anchor{ };
		expect(!core::valid(place), "a line needs a file");
		place.Place.File       = identity<core::file_identity>(1U);
		place.Place.Comparison = identity<core::file_comparison_digest>(2U);
		place.Place.Line->Line = 5U;
		place.StartLine        = 5U;
		expect(!core::valid(place), "a range starts above its line");
		place.StartLine     = 2U;
		place.ReplyToThread = "PRRT";
		expect(!core::valid(place), "a reply has no range");
		place.StartLine.reset();
		place.ReplyToThread = std::string(core::maximum_local_reply_thread_bytes + 1U, 'x');
		expect(!core::valid(place), "a reply's thread is bounded");
	}

	// RFC-BPR-0127 ([[BPR-2160]]). The one keep action: a draft where a
	// thread or a line takes one, a Local comment on the file or the pull
	// request where none does.
	void the_keep_action_drafts_where_a_line_takes_one()
	{
		using core::keep_kind;
		using core::keep_local_place;
		expect(core::decide_keep({ .ThreadReply = true }).Kind == keep_kind::draft, "a thread reply is a draft");
		expect(core::decide_keep({ .LineTakesDraft = true, .FileInPullRequest = true }).Kind == keep_kind::draft, "a line that takes a draft makes one");
		expect(core::decide_keep({ .LineTakesDraft = true }).Kind == keep_kind::draft, "a line is enough");
		expect((core::decide_keep({ .FileInPullRequest = true }) == core::keep_choice{ .Kind = keep_kind::local, .Local = keep_local_place::file }), "no line: Local on the file");
		expect((core::decide_keep({ }) == core::keep_choice{ .Kind = keep_kind::local, .Local = keep_local_place::pull_request }), "no file: Local on the pull request");
		expect(core::name(keep_kind::draft) == "draft" && core::name(keep_kind::local) == "local", "kind names");
		expect(core::name(keep_local_place::file) == "file" && core::name(keep_local_place::pull_request) == "pull_request", "place names");
	}
} // namespace

int main()
{
	a_draft_is_never_local();
	keep_local_keeps_the_line_the_range_and_the_reply();
	keep_local_is_refused_while_a_review_holds_it_and_once_published();
	make_draft_returns_the_same_kind_at_the_same_place();
	make_draft_off_its_line_asks_for_a_line_and_never_merges();
	a_place_is_checked_whole();
	the_keep_action_drafts_where_a_line_takes_one();
	std::cout << "LOCAL_COMMENT_OK\n";
	return EXIT_SUCCESS;
}
