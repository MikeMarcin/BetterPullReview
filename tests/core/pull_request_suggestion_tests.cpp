// [[RFC-BPR-0090]] M1. Suggest change edits the selected head lines in place:
// which selections it opens on, the editable span the editing model enforces,
// what GitHub receives, how a body holds the words and the block, how a kept
// draft opens again, what a suggestion becomes as text, and whether it can be
// carried to a newer head.
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/pull_request_suggestion.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_text_editing.hpp"
#include "product_diff.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const diff_options& aOptions) -> file_comparison_key
	{
		const auto digest = options_digest(aOptions);
		expect(digest.has_value(), "test options are normalized");
		const auto scope       = opaque<pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
		const auto authority   = pull_request_authority{
			.Profile         = host_profile_id{ 10U },
			.ProfileRevision = github_profile_revision{ 20U },
			.Host            = git_host_id{ 30U },
			.HostAuthority   = opaque<host_authority_fingerprint>(13U),
			.Account         = github_account_id{ 40U },
			.Viewer          = github_viewer_actor_id{ 50U },
			.Repository      = repository_id{ 101U },
			.Cache           = opaque<repository_cache_scope_id>(14U),
			.PullRequest     = pull_request_id{ 202U },
			.Number          = pull_request_number{ 17U },
			.Scope           = scope,
		};
		return file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = comparison_anchor_identity{ .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_base, .Epoch = epoch_ordinal{ 1 } },
			.RightAnchor = comparison_anchor_identity{ .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_head, .Epoch = epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = opaque<file_identity>(31U),
			.Options     = *digest,
			.Algorithm   = diff_algorithm_revision{ 1U },
		};
	}

	[[nodiscard]] auto bytes_of(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	[[nodiscard]] auto diff(const std::string_view aLeft, const std::string_view aRight) -> computed_file_diff
	{
		const auto left    = bytes_of(aLeft);
		const auto right   = bytes_of(aRight);
		const auto options = diff_options{ };
		auto       input   = diff_compute_input{ };
		input.Key          = comparison_key(options);
		input.Options      = options;
		input.LeftBytes    = std::span<const std::uint8_t>{ left };
		input.RightBytes   = std::span<const std::uint8_t>{ right };
		auto outcome       = bpr::tests::product_file_diff(input);
		expect(outcome.Result.has_value(), "the fixture texts compute a diff");
		return std::move(*outcome.Result);
	}

	[[nodiscard]] auto numbered(const std::uint32_t aCount, const std::uint32_t aChanged = 0xFFFF'FFFFU) -> std::string
	{
		auto text = std::string{ };
		for (std::uint32_t line = 0U; line < aCount; ++line) {
			text += line == aChanged ? "changed " : "line ";
			text += std::to_string(line);
			text += '\n';
		}
		return text;
	}

	[[nodiscard]] auto at(const computed_file_diff& aDiff, const diff_side aSide, const std::uint32_t aLine) -> source_line_ref
	{
		return { .Comparison = aDiff.Alignment.Digest, .Side = aSide, .Line = aLine };
	}

	void test_the_range_must_pass_the_existing_rules()
	{
		// Forty lines; line 10 and line 30 changed: hunks 7..13 and 27..33.
		auto right = numbered(40U, 10U);
		right.replace(right.find("line 30\n"), 8U, "changed 30\n");
		const auto result = diff(numbered(40U), right);

		const auto one = decide_suggestion_range(result.Alignment, at(result, diff_side::right, 10U), at(result, diff_side::right, 10U), false);
		expect(one.accepted() && !one.Range.StartLine && one.Range.Anchor.Line == 10U && one.first() == 10U, "one changed head line is suggested on");
		const auto range = decide_suggestion_range(result.Alignment, at(result, diff_side::right, 12U), at(result, diff_side::right, 8U), false);
		expect(range.accepted() && range.Range.StartLine == 8U && range.Range.Anchor.Line == 12U && range.first() == 8U, "a range inside one hunk, in either order");

		const auto base = decide_suggestion_range(result.Alignment, at(result, diff_side::left, 10U), at(result, diff_side::left, 10U), false);
		expect(base.Refusal == suggestion_range_refusal::base_side, "a base line is head side only");
		const auto across = decide_suggestion_range(result.Alignment, at(result, diff_side::right, 12U), at(result, diff_side::right, 28U), false);
		expect(across.Refusal == suggestion_range_refusal::comment_range && across.CommentRefusal == comment_range_refusal::crosses_hunk,
			   "a range across two changed regions keeps the comment rule's reason");
		const auto mixed = decide_suggestion_range(result.Alignment, at(result, diff_side::left, 10U), at(result, diff_side::right, 10U), false);
		expect(mixed.Refusal == suggestion_range_refusal::comment_range && mixed.CommentRefusal == comment_range_refusal::mixed_side, "two sides are refused");
		const auto outside = decide_suggestion_range(result.Alignment, at(result, diff_side::right, 20U), at(result, diff_side::right, 20U), false);
		expect(outside.Refusal == suggestion_range_refusal::outside_changed_region, "one line outside every hunk is refused for a suggestion");
		expect(decide_suggestion_range(result.Alignment, at(result, diff_side::right, 13U), at(result, diff_side::right, 13U), false).accepted(),
			   "the hunk's last context line is inside it");
		expect(decide_suggestion_range(result.Alignment, at(result, diff_side::right, 14U), at(result, diff_side::right, 14U), false).Refusal
				   == suggestion_range_refusal::outside_changed_region,
			   "the line after the hunk is outside it");

		const auto filtered = decide_suggestion_range(result.Alignment, at(result, diff_side::right, 8U), at(result, diff_side::right, 12U), true);
		expect(filtered.Refusal == suggestion_range_refusal::filtered_range, "no range on a filtered diff whose right side is not the head");
		const auto filteredOne = decide_suggestion_range(result.Alignment, at(result, diff_side::right, 20U), at(result, diff_side::right, 20U), true);
		expect(filteredOne.accepted(), "one filtered line is left to the filtered placement");
		expect(stable_name(suggestion_range_refusal::outside_changed_region) == "outside_changed_region", "stable names");
	}

	/// Six lines, LF; the editable lines are 2 to 3.
	[[nodiscard]] auto span_editor() -> suggestion_text_editor
	{
		auto editor = suggestion_text_editor{ "zero\none\ntwo\nthree\nfour\nfive\n" };
		editor.set_editable_lines(2U, 3U);
		return editor;
	}

	void test_only_the_editable_lines_change()
	{
		auto editor = span_editor();
		expect(editor.editable_range() == std::pair<std::uint64_t, std::uint64_t>{ 2U, 3U }, "the range is lines 2 to 3");
		expect(!editor.can_undo(), "setting the lines starts the history again");

		// Typing inside the range.
		expect(editor.apply(place_caret{ .At = { 2U, 3U } }), "a caret may go into the range");
		expect(editor.apply(insert_text{ .Text = "X" }), "typing inside the range changes it");
		expect(editor.text() == "zero\none\ntwoX\nthree\nfour\nfive\n", "the typed character is in line 2");
		expect(editor.last_refusal() == suggestion_edit_refusal::none, "no refusal");

		// The caret may go anywhere, but typing there is refused whole.
		expect(editor.apply(place_caret{ .At = { 5U, 1U } }), "the caret goes outside to read");
		expect(!editor.apply(insert_text{ .Text = "Y" }), "typing on a fixed line is refused");
		expect(editor.last_refusal() == suggestion_edit_refusal::fixed_text, "the refusal names the fixed text");
		expect(editor.text() == "zero\none\ntwoX\nthree\nfour\nfive\n", "nothing changed");
		expect(editor.state().Carets.front().Head == text_point{ 5U, 1U }, "the caret is not moved into the range");
		expect(editor.apply(place_caret{ .At = { 0U, 0U } }) && editor.last_refusal() == suggestion_edit_refusal::none, "a movement is no refusal");
		expect(!editor.apply(delete_forward{ }), "a deletion on a fixed line is refused");
		expect(editor.last_refusal() == suggestion_edit_refusal::fixed_text, "refused for the fixed text");

		// Backspace at the span's start and Delete at its end would join a fixed line.
		expect(editor.apply(place_caret{ .At = { 2U, 0U } }), "caret at the span's start");
		expect(!editor.apply(delete_backward{ }) && editor.last_refusal() == suggestion_edit_refusal::fixed_text, "Backspace at the span's start is refused");
		expect(editor.apply(place_caret{ .At = { 3U, 5U } }), "caret at the span's end");
		expect(!editor.apply(delete_forward{ }) && editor.last_refusal() == suggestion_edit_refusal::fixed_text, "Delete at the span's end is refused");
		expect(editor.apply(delete_backward{ }), "Backspace inside the span works");
		expect(editor.text() == "zero\none\ntwoX\nthre\nfour\nfive\n", "one character went");

		// Undo goes back through the accepted edits only.
		expect(editor.apply(undo_edit{ }) && editor.text() == "zero\none\ntwoX\nthree\nfour\nfive\n", "undo restores the deletion");
		expect(editor.apply(undo_edit{ }) && editor.text() == "zero\none\ntwo\nthree\nfour\nfive\n", "undo restores the typing");
		expect(!editor.can_undo(), "the refused edits left no undo steps");
		expect(editor.apply(redo_edit{ }) && editor.text() == "zero\none\ntwoX\nthree\nfour\nfive\n", "redo replays the typing");
	}

	void test_a_selection_into_fixed_text_is_clipped()
	{
		// A triple-clicked last line selects its line break; typing replaces the line only.
		auto editor = span_editor();
		expect(editor.apply(select_line_at{ .At = { 3U, 0U } }), "triple-click line 3");
		expect(editor.apply(insert_text{ .Text = "3" }), "typing over it is accepted");
		expect(editor.text() == "zero\none\ntwo\n3\nfour\nfive\n", "line 3 is replaced and its line break kept");

		// A selection from a fixed line into the range is clipped to the span.
		auto other = span_editor();
		expect(other.apply(place_caret{ .At = { 1U, 1U } }) && other.apply(place_caret{ .At = { 2U, 2U }, .Extend = true }), "select from line 1 into line 2");
		expect(other.apply(insert_text{ .Text = "Z" }), "typing over it is accepted");
		expect(other.text() == "zero\none\nZo\nthree\nfour\nfive\n", "only the span's part is replaced");

		// Select All selects the span.
		auto all = span_editor();
		expect(all.apply(select_all{ }), "Select All");
		expect(all.state().Carets.front().Anchor == text_point{ 2U, 0U } && all.state().Carets.front().Head == text_point{ 3U, 5U }, "Select All is the span");
		expect(all.apply(insert_text{ .Text = "only" }) && all.text() == "zero\none\nonly\nfour\nfive\n", "typing over it replaces the range");
		expect(all.editable_range() == std::pair<std::uint64_t, std::uint64_t>{ 2U, 2U }, "two lines became one");

		// A second caret outside refuses the whole edit.
		auto carets = span_editor();
		expect(carets.apply(place_caret{ .At = { 2U, 0U } }) && carets.apply(add_caret{ .At = { 4U, 0U } }), "two carets, one outside");
		expect(!carets.apply(insert_text{ .Text = "-" }) && carets.last_refusal() == suggestion_edit_refusal::fixed_text, "refused whole");
		expect(carets.text() == "zero\none\ntwo\nthree\nfour\nfive\n", "neither caret typed");
	}

	void test_the_range_grows_and_shrinks()
	{
		auto editor = span_editor();
		expect(editor.apply(place_caret{ .At = { 3U, 5U } }), "end of the last line");
		expect(editor.apply(insert_line_break{ }), "Enter at the span's end");
		expect(editor.apply(insert_text{ .Text = "new" }), "typing on the new line");
		expect(editor.text() == "zero\none\ntwo\nthree\nnew\nfour\nfive\n", "a line is added inside the range");
		expect(editor.editable_range() == std::pair<std::uint64_t, std::uint64_t>{ 2U, 4U }, "the range grew by a line");

		expect(editor.apply(paste_text{ .Text = "a\nb\n" }), "pasting two lines");
		expect(editor.editable_range() == std::pair<std::uint64_t, std::uint64_t>{ 2U, 6U }, "the range grew by the pasted lines");
		expect(editor.text() == "zero\none\ntwo\nthree\nnewa\nb\n\nfour\nfive\n", "pasted inside the range");

		expect(editor.apply(undo_edit{ }) && editor.editable_range() == std::pair<std::uint64_t, std::uint64_t>{ 2U, 4U }, "undo takes the range back");

		// Emptying the span leaves one empty line; the fixed ending stays.
		expect(editor.apply(select_all{ }) && editor.apply(delete_backward{ }), "delete everything in the range");
		expect(editor.text() == "zero\none\n\nfour\nfive\n", "one empty line is left");
		expect(editor.editable_range() == std::pair<std::uint64_t, std::uint64_t>{ 2U, 2U }, "the range is one line");
		expect(!editor.apply(delete_backward{ }) && editor.last_refusal() == suggestion_edit_refusal::fixed_text, "the empty line cannot be joined to the line above");
		expect(editor.apply(insert_text{ .Text = "back" }) && editor.text() == "zero\none\nback\nfour\nfive\n", "typing in the empty range");

		// A range at the end of a file with no final newline.
		auto last = suggestion_text_editor{ "a\nb\nc" };
		last.set_editable_lines(2U, 2U);
		expect(last.apply(place_caret{ .At = { 2U, 1U } }) && last.apply(insert_line_break{ }) && last.apply(insert_text{ .Text = "d" }), "Enter on the file's last line");
		expect(last.text() == "a\nb\nc\nd", "the file still ends without a newline");
		expect(last.editable_range() == std::pair<std::uint64_t, std::uint64_t>{ 2U, 3U }, "the range grew to the end");

		// CRLF: a new line takes the file's own ending.
		auto crlf = suggestion_text_editor{ "a\r\nb\r\nc\r\n" };
		crlf.set_editable_lines(1U, 1U);
		expect(crlf.apply(place_caret{ .At = { 1U, 1U } }) && crlf.apply(insert_line_break{ }) && crlf.apply(insert_text{ .Text = "x" }), "Enter in a CRLF file");
		expect(crlf.text() == "a\r\nb\r\nx\r\nc\r\n", "the new line ends CRLF like the others");
	}

	[[nodiscard]] auto lines(const std::string_view aText) -> std::vector<text_line>
	{
		return split_text_lines(aText);
	}

	[[nodiscard]] auto range_of(const std::vector<text_line>& aLines) -> std::vector<text_line>
	{
		// split_text_lines adds an empty last line after a final newline.
		auto result = aLines;
		if (result.size() > 1U && result.back().Content.empty() && result.back().Ending == text_line_ending::none) {
			result.pop_back();
		}
		return result;
	}

	void test_the_plan_is_what_github_receives()
	{
		const auto original = range_of(lines("\tif (aScale < 0.5) {\n\t\treturn 0.5;\n\t}\n\treturn std::min(aScale, 4.0);\n"));

		// A replacement of four lines by one, with words.
		const auto replaced = range_of(lines("\treturn std::clamp(aScale, 0.5, 4.0);\n"));
		const auto plan     = plan_pull_request_suggestion({ .First = 11U, .Last = 14U, .Original = original, .Edited = replaced, .Prose = "clamp covers both bounds." });
		expect(plan.Changed && !plan.Deletion && plan.placeable(), "a placeable replacement");
		expect(plan.StartLine == 11U && plan.Line == 14U, "anchored to the selected lines, start_line and line");
		expect(plan.Block == std::vector<std::string>{ "\treturn std::clamp(aScale, 0.5, 4.0);" }, "the block is the edited lines without endings");
		expect(plan.Body == "clamp covers both bounds.\n\n```suggestion\n\treturn std::clamp(aScale, 0.5, 4.0);\n```", "the prose exactly, a blank line, the fence");
		expect(plan.Change.Removed.size() == 4U && plan.Change.Added.size() == 1U, "what it changes");
		expect(classify_review_body(plan.Body) == review_body_compatibility::requires_line_subject, "the body is a suggestion");

		// One line grown to three, without words: the fence alone.
		const auto one   = range_of(lines("\treturn std::min(aScale, 4.0);\n"));
		const auto grown = range_of(lines("\t// Both bounds.\n\treturn std::clamp(aScale, 0.5, 4.0);\n\t// Done.\n"));
		const auto three = plan_pull_request_suggestion({ .First = 14U, .Last = 14U, .Original = one, .Edited = grown });
		expect(three.Changed && !three.StartLine && three.Line == 14U, "one line keeps one anchor however many lines the block holds");
		expect(three.Body == "```suggestion\n\t// Both bounds.\n\treturn std::clamp(aScale, 0.5, 4.0);\n\t// Done.\n```", "the fence alone");

		// Emptying the lines is a deletion: an empty block.
		const auto empty    = std::vector<text_line>{ text_line{ .Content = "", .Ending = text_line_ending::lf } };
		const auto deletion = plan_pull_request_suggestion({ .First = 11U, .Last = 13U, .Original = std::span{ original }.first(3U), .Edited = empty, .Prose = "Dead code." });
		expect(deletion.Changed && deletion.Deletion && deletion.Block.empty(), "an emptied range deletes the lines");
		expect(deletion.Body == "Dead code.\n\n```suggestion\n```", "an empty block");
		const auto blank     = std::vector<text_line>{ text_line{ .Content = "", .Ending = text_line_ending::lf } };
		const auto blankKept = plan_pull_request_suggestion({ .First = 5U, .Last = 5U, .Original = blank, .Edited = blank });
		expect(!blankKept.Changed, "an empty line left empty is unchanged");

		// Typed back, or only the endings changed: nothing to suggest, the words stay.
		const auto back = plan_pull_request_suggestion({ .First = 11U, .Last = 14U, .Original = original, .Edited = original, .Prose = "Just a note." });
		expect(!back.Changed && back.Body == "Just a note." && back.Block.empty(), "typed back to the original is a plain comment");
		auto crlf = original;
		for (auto& line : crlf) {
			line.Ending = text_line_ending::crlf;
		}
		expect(!plan_pull_request_suggestion({ .First = 11U, .Last = 14U, .Original = original, .Edited = crlf }).Changed, "endings only is unchanged");

		// Code holding backticks gets a longer fence.
		const auto ticks     = range_of(lines("const auto fence = \"```\";\n"));
		const auto ticksPlan = plan_pull_request_suggestion({ .First = 2U, .Last = 2U, .Original = one, .Edited = ticks });
		expect(ticksPlan.Body == "````suggestion\nconst auto fence = \"```\";\n````", "the fence outruns the code's backticks");

		// A filtered diff: placeable only at the head.
		const auto placed = [&](const filtered_comment_placement aPlacement) {
			return plan_pull_request_suggestion({ .First = 14U, .Last = 14U, .Original = one, .Edited = grown, .Filtered = filtered_comment_target{ .Placement = aPlacement } }).Reason;
		};
		expect(placed(filtered_comment_placement::head) == suggestion_placement_reason::none, "a filtered line unchanged at head is placeable");
		expect(placed(filtered_comment_placement::earlier_commit) == suggestion_placement_reason::changed_after_commit, "a line changed after the commit");
		expect(placed(filtered_comment_placement::file_comment) == suggestion_placement_reason::unresolvable_line, "a line GitHub resolves nowhere");
		expect(placed(filtered_comment_placement::refused) == suggestion_placement_reason::not_placed, "a refused placement");
	}

	void test_a_body_splits_back_exactly()
	{
		const auto block = std::vector<std::string>{ "a", "", "```b```" };
		for (const auto* prose : { "", "words", "issue (blocking): words\n\nmore", "ends with a newline\n", "two\r\nlines" }) {
			const auto body  = compose_suggestion_body(prose, block);
			const auto split = split_suggestion_body(body);
			expect(split.has_value() && split->Prose == prose && split->Block == block, std::string{ "round trip for prose \"" } + prose + "\"");
		}
		const auto deleted = split_suggestion_body(compose_suggestion_body("Delete.", { }));
		expect(deleted.has_value() && deleted->Prose == "Delete." && deleted->Block.empty(), "an empty block splits back");

		// Bodies typed another way stay text drafts.
		expect(!split_suggestion_body("words\n\n```suggestion\nx\n```\n").has_value(), "a trailing newline is not a composed body");
		expect(!split_suggestion_body("words\n```suggestion\nx\n```").has_value(), "one newline before the fence is not a composed body");
		expect(!split_suggestion_body("```suggestion\nx\n```\n\nafter").has_value(), "text after the block is not a composed body");
		expect(!split_suggestion_body("plain words").has_value(), "words alone");
		expect(!split_suggestion_body("~~~suggestion\nx\n~~~").has_value(), "a tilde fence is not one the edit writes");
		expect(!split_suggestion_body("\n\n```suggestion\nx\n```").has_value(), "an empty prose before a separator is not composed");

		// Prose holding its own block still splits at the last one.
		const auto two   = compose_suggestion_body("see\n\n```suggestion\nold\n```", std::vector<std::string>{ "new" });
		const auto split = split_suggestion_body(two);
		expect(split.has_value() && split->Prose == "see\n\n```suggestion\nold\n```" && split->Block == std::vector<std::string>{ "new" }, "the last block is the edit's");
	}

	void test_a_kept_draft_opens_again()
	{
		const auto original = range_of(lines("one\r\ntwo\r\n"));
		const auto body     = compose_suggestion_body("Words.", std::vector<std::string>{ "uno", "dos", "tres" });
		const auto rebuilt  = rebuild_suggestion_span(body, original, true);
		expect(rebuilt.Refusal == suggestion_rebuild_refusal::none && rebuilt.Prose == "Words.", "a composed body opens in the code");
		expect(rebuilt.Lines.size() == 3U && rebuilt.Lines[0].Content == "uno" && rebuilt.Lines[2].Content == "tres", "the block's lines");
		expect(rebuilt.Lines[0].Ending == text_line_ending::crlf && rebuilt.Lines[1].Ending == text_line_ending::crlf && rebuilt.Lines[2].Ending == text_line_ending::crlf,
			   "the endings follow the original lines");

		// The rebuilt span plans back to the same body.
		const auto again = plan_pull_request_suggestion({ .First = 0U, .Last = 1U, .Original = original, .Edited = rebuilt.Lines, .Prose = rebuilt.Prose });
		expect(again.Body == body, "rebuilding and planning again gives the same body");

		const auto deletion = rebuild_suggestion_span(compose_suggestion_body("", { }), original, true);
		expect(deletion.Refusal == suggestion_rebuild_refusal::none && deletion.Lines.size() == 1U && deletion.Lines[0].Content.empty(), "a deletion is one empty line");
		expect(plan_pull_request_suggestion({ .First = 0U, .Last = 1U, .Original = original, .Edited = deletion.Lines }).Deletion, "and plans back to a deletion");

		const auto noFinal = range_of(lines("x"));
		const auto end     = rebuild_suggestion_span(compose_suggestion_body("", std::vector<std::string>{ "y", "z" }), noFinal, true);
		expect(end.Lines[0].Ending == text_line_ending::lf && end.Lines[1].Ending == text_line_ending::none, "the file's last line keeps no ending, inner lines take one");

		expect(rebuild_suggestion_span(body, original, false).Refusal == suggestion_rebuild_refusal::other_comparison, "another comparison");
		expect(rebuild_suggestion_span("plain words", original, true).Refusal == suggestion_rebuild_refusal::not_composed, "a text draft");
		expect(rebuild_suggestion_span(compose_suggestion_body("see\n\n```suggestion\nold\n```", std::vector<std::string>{ "new" }), original, true).Refusal
				   == suggestion_rebuild_refusal::second_block,
			   "a second block in the words");
		expect(rebuild_suggestion_span(body, { }, true).Refusal == suggestion_rebuild_refusal::no_original, "no original lines");
	}

	void test_convert_writes_the_change_as_text()
	{
		const auto text = convert_suggestion_to_text("clamp covers both bounds.", 14U, 14U, std::vector<std::string>{ "\treturn std::min(aScale, 4.0);" },
													 std::vector<std::string>{ "\treturn std::clamp(aScale, 0.5, 4.0);" });
		expect(text == "clamp covers both bounds.\n\nSuggested change to line 15:\n\n```diff\n-\treturn std::min(aScale, 4.0);\n+\treturn std::clamp(aScale, 0.5, 4.0);\n```",
			   "the words, the lines and a diff block");
		expect(classify_review_body(text) == review_body_compatibility::portable, "the converted body is portable text");

		const auto range = convert_suggestion_to_text("", 11U, 13U, std::vector<std::string>{ "a", "b", "c" }, { });
		expect(range == "Suggested change to lines 12\xE2\x80\x93"
						"14:\n\n```diff\n-a\n-b\n-c\n```",
			   "a deletion of a range, without words");
		const auto ticks = convert_suggestion_to_text("", 0U, 0U, std::vector<std::string>{ "x" }, std::vector<std::string>{ "````" });
		expect(ticks.find("`````diff\n") != std::string::npos && ticks.ends_with("\n`````"), "the fence outruns the code's backticks");
	}

	void test_carry_needs_every_line_unchanged()
	{
		// Old lines 0..9; the new head inserts two lines before line 4 and changes line 8.
		const auto lines = line_correspondence{
			.LeftLines  = 10U,
			.RightLines = 12U,
			.Unchanged  = { { .Left = 0U, .Right = 0U, .Length = 4U }, { .Left = 4U, .Right = 6U, .Length = 4U }, { .Left = 9U, .Right = 11U, .Length = 1U } },
		};
		expect(decide_suggestion_carry(lines, 1U, 2U) == std::pair<std::uint32_t, std::uint32_t>{ 1U, 2U }, "unmoved lines carry to themselves");
		expect(decide_suggestion_carry(lines, 4U, 6U) == std::pair<std::uint32_t, std::uint32_t>{ 6U, 8U }, "moved lines carry to where they are");
		expect(!decide_suggestion_carry(lines, 7U, 8U).has_value(), "a changed line is not carried");
		expect(!decide_suggestion_carry(lines, 3U, 4U).has_value(), "lines split apart by an insertion are not carried");
		expect(!decide_suggestion_carry(lines, 5U, 4U).has_value(), "an inverted range");
	}
	/// \brief [[BPR-1733]]. A kept suggestion from an earlier head is carried
	/// exactly when its lines are unchanged and together at the newer head;
	/// otherwise it offers its change to copy, and anything else is not a
	/// suggestion to carry.
	void test_a_suggestion_from_an_earlier_head_carries_or_copies()
	{
		const auto lines = line_correspondence{
			.LeftLines  = 10U,
			.RightLines = 12U,
			.Unchanged  = { { .Left = 0U, .Right = 0U, .Length = 4U }, { .Left = 4U, .Right = 6U, .Length = 4U }, { .Left = 9U, .Right = 11U, .Length = 1U } },
		};
		auto draft        = review_comment_draft{ };
		draft.Id          = review_draft_id{ 7U };
		draft.Anchor.Side = diff_side::right;
		draft.Anchor.Line = 6U;
		draft.StartLine   = 4U;
		const auto block  = std::vector<std::string>{ "int a = 1;", "int b = 2;" };
		draft.Body        = compose_suggestion_body("Tighter.", block);
		const auto moved  = decide_suggestion_head_move(draft, lines);
		expect(moved.has_value() && moved->Carry == std::pair<std::uint32_t, std::uint32_t>{ 6U, 8U } && moved->Refusal == suggestion_head_move_refusal::none,
			   "lines 5 to 7 unchanged at the newer head carry to lines 7 to 9");
		expect(moved->Change == "int a = 1;\nint b = 2;\n", "Copy change is the block's lines");

		draft.Anchor.Line  = 8U;
		draft.StartLine    = 7U;
		const auto changed = decide_suggestion_head_move(draft, lines);
		expect(changed.has_value() && !changed->Carry.has_value() && changed->Refusal == suggestion_head_move_refusal::changed_where_edited && changed->Change == moved->Change,
			   "a range with a changed line is not carried and offers its change");
		const auto unknown = decide_suggestion_head_move(draft, std::nullopt);
		expect(unknown.has_value() && !unknown->Carry.has_value() && unknown->Refusal == suggestion_head_move_refusal::lines_unknown, "with no comparison nothing is carried");

		auto deletion = draft;
		deletion.Body = compose_suggestion_body("", { });
		expect(decide_suggestion_head_move(deletion, lines)->Change.empty(), "a deletion has no change to copy");
		auto second = draft;
		second.Body = compose_suggestion_body(suggestion_fence_body(std::vector<std::string>{ "x" }), block);
		expect(decide_suggestion_head_move(second, lines)->Refusal == suggestion_head_move_refusal::second_block, "words holding a block of their own do not carry");

		auto text = draft;
		text.Body = "Just words.";
		expect(!decide_suggestion_head_move(text, lines).has_value(), "a text draft is not a suggestion to carry");
		auto reply          = draft;
		reply.ReplyToThread = "thread";
		expect(!decide_suggestion_head_move(reply, lines).has_value(), "nor is a reply");
		auto base        = draft;
		base.Anchor.Side = diff_side::left;
		expect(!decide_suggestion_head_move(base, lines).has_value(), "nor a base line's comment");
		auto publishing  = draft;
		publishing.State = review_draft_state::publishing;
		expect(!decide_suggestion_head_move(publishing, lines).has_value(), "nor a draft being published");
		expect(stable_name(suggestion_head_move_refusal::changed_where_edited) == "changed_where_edited", "the refusal has a stable name");
	}

	/// \brief A JSON string of exact bytes.
	[[nodiscard]] auto json(const std::string_view aText) -> std::string
	{
		auto out = std::string{ "\"" };
		for (const auto character : aText) {
			switch (character) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:   out += character; break;
			}
		}
		return out + "\"";
	}

	struct live_suggestion
	{
		std::string_view              Name;
		std::uint32_t                 First = 0U;
		std::uint32_t                 Last  = 0U;
		std::vector<std::string_view> Edited;
		std::string_view              Prose;
	};

	/// \brief [[RFC-BPR-0090]] M4. The plans the live course on BPR-Test
	/// publishes, made by the core from a fixed base and head: each file's
	/// texts, each suggestion's range decision, anchor and body, and the head
	/// file with every suggestion applied, its new lines ending as the file's
	/// own do. The course compares GitHub's read-back and its committed
	/// suggestions with these bytes.
	void print_live_plans()
	{
		const auto file = [](const std::string_view aEnding, const bool aHead, const std::uint32_t aChanged1, const std::uint32_t aChanged2) {
			auto text = std::string{ };
			for (std::uint32_t line = 1U; line <= 20U; ++line) {
				const auto changed = aHead && (line == aChanged1 || line == aChanged2);
				text += "int value_" + std::to_string(line) + " = " + (changed ? "compute(" + std::to_string(line) + ")" : std::to_string(line)) + ";";
				text += aEnding;
			}
			return text;
		};
		struct fixture
		{
			std::string_view             Path;
			std::string_view             Ending;
			std::uint32_t                Changed1 = 0U;
			std::uint32_t                Changed2 = 0U;
			std::vector<live_suggestion> Suggestions;
		};
		const auto fixtures = std::vector<fixture>{
			{ .Path        = "rfc-0090/lf.cpp",
			  .Ending      = "\n",
			  .Changed1    = 6U,
			  .Changed2    = 14U,
			  .Suggestions = { { .Name = "replace-two-with-one", .First = 3U, .Last = 4U, .Edited = { "int value_4_5 = 4 + 5;" }, .Prose = "issue: fold these two into one." },
							   { .Name = "delete-one", .First = 6U, .Last = 6U, .Edited = { "" } },
							   { .Name = "grow-one-to-three", .First = 8U, .Last = 8U, .Edited = { "// nine", "int value_9 = 9;", "// done" } },
							   { .Name = "backticks", .First = 11U, .Last = 11U, .Edited = { "const char* fence = \"```\";" } } } },
			{ .Path        = "rfc-0090/crlf.cpp",
			  .Ending      = "\r\n",
			  .Changed1    = 10U,
			  .Changed2    = 10U,
			  .Suggestions = { { .Name = "crlf-replace", .First = 9U, .Last = 9U, .Edited = { "int value_10 = 10; // crlf" } } } },
		};
		std::cout << "{\"files\":[";
		for (std::size_t index = 0U; index < fixtures.size(); ++index) {
			const auto& fixture = fixtures[index];
			const auto  base    = file(fixture.Ending, false, 0U, 0U);
			const auto  head    = file(fixture.Ending, true, fixture.Changed1, fixture.Changed2);
			const auto  result  = diff(base, head);
			auto        lines   = split_text_lines(head);
			auto        applied = lines;
			std::cout << (index == 0U ? "" : ",") << "{\"path\":" << json(fixture.Path) << ",\"base\":" << json(base) << ",\"head\":" << json(head) << ",\"suggestions\":[";
			for (std::size_t at = 0U; at < fixture.Suggestions.size(); ++at) {
				const auto& suggestion = fixture.Suggestions[at];
				const auto  range      = decide_suggestion_range(result.Alignment, ::at(result, diff_side::right, suggestion.First), ::at(result, diff_side::right, suggestion.Last), false);
				expect(range.accepted(), std::string{ "the live suggestion's lines are suggestible: " } + std::string{ suggestion.Name });
				const auto original = std::vector<text_line>{ lines.begin() + suggestion.First, lines.begin() + suggestion.Last + 1U };
				auto       edited   = std::vector<text_line>{ };
				for (const auto line : suggestion.Edited) {
					edited.push_back({ .Content = std::string{ line }, .Ending = original.back().Ending });
				}
				const auto plan = plan_pull_request_suggestion({ .First = suggestion.First, .Last = suggestion.Last, .Original = original, .Edited = edited, .Prose = suggestion.Prose });
				expect(plan.Changed && plan.placeable(), "the live suggestion is a placeable change");
				std::cout << (at == 0U ? "" : ",") << "{\"name\":" << json(suggestion.Name) << ",\"start_line\":"
						  << (plan.StartLine ? std::to_string(*plan.StartLine + 1U) : std::string{ "null" }) << ",\"line\":" << plan.Line + 1U << ",\"side\":\"RIGHT\",\"body\":" << json(plan.Body)
						  << "}";
			}
			// Applied from the bottom up, so earlier ranges keep their lines.
			for (auto suggestion = fixture.Suggestions.rbegin(); suggestion != fixture.Suggestions.rend(); ++suggestion) {
				const auto ending = applied[suggestion->First].Ending;
				auto       with   = std::vector<text_line>{ };
				const auto empty  = suggestion->Edited.size() == 1U && suggestion->Edited.front().empty();
				for (const auto line : suggestion->Edited) {
					if (!empty) {
						with.push_back({ .Content = std::string{ line }, .Ending = ending });
					}
				}
				applied.erase(applied.begin() + suggestion->First, applied.begin() + suggestion->Last + 1U);
				applied.insert(applied.begin() + suggestion->First, with.begin(), with.end());
			}
			std::cout << "],\"applied\":" << json(join_text_lines(applied)) << "}";
		}
		std::cout << "]}\n";
	}
} // namespace

auto main(const int aArgc, char** const aArgv) -> int
{
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--live-plans") {
		print_live_plans();
		return EXIT_SUCCESS;
	}
	test_the_range_must_pass_the_existing_rules();
	test_only_the_editable_lines_change();
	test_a_selection_into_fixed_text_is_clipped();
	test_the_range_grows_and_shrinks();
	test_the_plan_is_what_github_receives();
	test_a_body_splits_back_exactly();
	test_a_kept_draft_opens_again();
	test_convert_writes_the_change_as_text();
	test_carry_needs_every_line_unchanged();
	test_a_suggestion_from_an_earlier_head_carries_or_copies();
	std::cout << "pull request suggestion tests passed\n";
	return EXIT_SUCCESS;
}
