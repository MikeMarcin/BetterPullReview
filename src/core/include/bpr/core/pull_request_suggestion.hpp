#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_SUGGESTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_SUGGESTION_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/filtered_comment.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/suggestion_text_editing.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief [[RFC-BPR-0090]]. Suggest change edits the selected head lines in
/// place in pull request review.
/// \details Only the selected lines are editable (owner, Q1 rejected,
/// 2026-10-21: "Only the selected lines are editable in place"), and each
/// Suggest change is one GitHub suggestion comment on those lines. These are
/// the decisions the core makes about it, as pure functions of values the
/// host already holds: whether a selection can be suggested on at all, what
/// the edited lines become on GitHub, how a draft body holds the reviewer's
/// words and the block, how a kept draft opens again in the code, what a
/// suggestion GitHub cannot apply becomes as text, and whether an edit can be
/// carried to a newer head. The editing model's span is
/// `suggestion_text_editor::set_editable_lines`. Nothing here reads a file,
/// asks a provider or publishes.
namespace bpr::core {
	/// \brief Why a selection cannot be suggested on.
	enum class suggestion_range_refusal : std::uint8_t {
		none,
		/// `decide_comment_range` refused the selection; its own reason is kept.
		comment_range,
		/// A base line: GitHub applies suggestions to the head only.
		base_side,
		/// One line outside every hunk of `github_review_hunks`. GitHub takes
		/// no line comment there ([[BPR-1715]]), and a suggestion has no file
		/// comment form.
		outside_changed_region,
		/// Several lines on a filtered diff whose right side is not the head:
		/// `place_filtered_comment` publishes a range only where no line moves.
		filtered_range,
	};

	[[nodiscard]] auto stable_name(suggestion_range_refusal aRefusal) noexcept -> std::string_view;

	struct suggestion_range_decision
	{
		suggestion_range_refusal Refusal = suggestion_range_refusal::none;
		/// `decide_comment_range`'s reason when `Refusal` is `comment_range`.
		comment_range_refusal CommentRefusal = comment_range_refusal::none;
		/// The anchor and first line when accepted; `Range.Anchor` is the last line.
		comment_range_decision Range;

		[[nodiscard]] auto accepted() const noexcept -> bool
		{
			return Refusal == suggestion_range_refusal::none;
		}
		/// The zero-based first selected line.
		[[nodiscard]] auto first() const noexcept -> std::uint32_t
		{
			return Range.StartLine.value_or(Range.Anchor.Line);
		}

		auto operator<=>(const suggestion_range_decision&) const = default;
	};

	/// \brief Whether Suggest change can open on a selection, from its two
	/// ends in either order.
	/// \details The comment rules first (`decide_comment_range`, base lines
	/// refused), then the head side, then one changed region for a single
	/// line too, then no range when `aRightIsNotHead` (a commit-filtered diff
	/// whose right side is an earlier commit).
	[[nodiscard]] auto decide_suggestion_range(const file_alignment& aAlignment, source_line_ref aFirst, source_line_ref aLast, bool aRightIsNotHead)
		-> suggestion_range_decision;

	/// \brief Why a planned suggestion is not GitHub's applicable suggestion.
	enum class suggestion_placement_reason : std::uint8_t {
		none,
		/// On a filtered diff, the line changed after the diff's commit; GitHub
		/// would show the comment outdated on that commit, where no suggestion
		/// applies.
		changed_after_commit,
		/// On a filtered diff, GitHub resolves the line on no commit; only
		/// [[BPR-1706]]'s file comment is left, which carries no suggestion.
		unresolvable_line,
		/// The filtered placement refused the line, or is not known yet.
		not_placed,
	};

	[[nodiscard]] auto stable_name(suggestion_placement_reason aReason) noexcept -> std::string_view;

	/// \brief What one in-pane edit is, for planning.
	struct pull_request_suggestion_input
	{
		/// The zero-based first and last selected head lines.
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;
		/// The selected lines as the head holds them, with their endings.
		std::span<const text_line> Original;
		/// The editable range's lines now, with their endings.
		std::span<const text_line> Edited;
		/// The reviewer's words, which are kept byte for byte.
		std::string_view Prose;
		/// On a filtered diff whose right side is not the head: where
		/// `place_filtered_comment` puts the anchor line. None otherwise.
		std::optional<filtered_comment_target> Filtered;
	};

	/// \brief What GitHub receives for one in-pane edit.
	struct pull_request_suggestion_plan
	{
		/// The block's lines differ in content from the original lines. An
		/// edit that changed only line endings, or was typed back, does not.
		bool Changed = false;
		/// The block is empty: the lines are deleted.
		bool Deletion = false;
		/// GitHub's `start_line` (zero-based; none for one line) and `line`.
		std::optional<std::uint32_t> StartLine;
		std::uint32_t                Line = 0U;
		/// The block's lines, without their endings.
		std::vector<std::string> Block;
		/// The draft's body: the prose, then the block, as
		/// `compose_suggestion_body` writes them; the prose alone when unchanged.
		std::string Body;
		/// What the block changes, as "What it changes" shows it.
		suggested_change Change;
		/// GitHub will offer to apply it, or why not.
		suggestion_placement_reason Reason = suggestion_placement_reason::none;

		[[nodiscard]] auto placeable() const noexcept -> bool
		{
			return Reason == suggestion_placement_reason::none;
		}

		auto operator<=>(const pull_request_suggestion_plan&) const = default;
	};

	/// \brief Plans one in-pane edit.
	/// \details The edited range is always on the selected lines, side RIGHT,
	/// however many lines it holds. A range left as one empty line where the
	/// original was not is a deletion: an empty block.
	[[nodiscard]] auto plan_pull_request_suggestion(const pull_request_suggestion_input& aInput) -> pull_request_suggestion_plan;

	/// \brief A draft body holding the reviewer's words and one block: the
	/// prose exactly, a blank line, then `suggestion_fence_body(aBlock)`; the
	/// fence alone when the prose is empty.
	[[nodiscard]] auto compose_suggestion_body(std::string_view aProse, std::span<const std::string> aBlock) -> std::string;

	/// \brief A body read back into its prose and its block.
	struct split_suggestion
	{
		std::string              Prose;
		std::vector<std::string> Block;

		auto operator<=>(const split_suggestion&) const = default;
	};

	/// \brief The prose and block of a body `compose_suggestion_body` wrote,
	/// exactly; none for any other body.
	/// \details The body must end with a suggestion block that composing the
	/// prose and its lines again gives back byte for byte, so a body typed
	/// another way stays a text draft.
	[[nodiscard]] auto split_suggestion_body(std::string_view aBody) -> std::optional<split_suggestion>;

	/// \brief Why a kept draft does not open in the code.
	enum class suggestion_rebuild_refusal : std::uint8_t {
		none,
		/// The body does not end with a block `compose_suggestion_body` wrote.
		not_composed,
		/// The prose holds a suggestion block of its own.
		second_block,
		/// The draft is on another comparison than the one shown.
		other_comparison,
		/// The original lines are not known.
		no_original,
	};

	[[nodiscard]] auto stable_name(suggestion_rebuild_refusal aRefusal) noexcept -> std::string_view;

	struct suggestion_rebuild
	{
		suggestion_rebuild_refusal Refusal = suggestion_rebuild_refusal::none;
		std::string                Prose;
		/// The editable range's lines: the block's lines, the last with the
		/// original last line's ending and the others with the ending of the
		/// original line at their place, or the last line's past its end.
		std::vector<text_line> Lines;

		auto operator<=>(const suggestion_rebuild&) const = default;
	};

	/// \brief Opens a kept draft in the code again.
	/// \details Exact in content for every block an edit wrote; line endings
	/// follow the original lines. A deletion is one empty line.
	[[nodiscard]] auto rebuild_suggestion_span(std::string_view aBody, std::span<const text_line> aOriginal, bool aSameComparison) -> suggestion_rebuild;

	/// \brief A suggestion as text: the prose exactly, a blank line,
	/// "Suggested change to line a:" (or "lines a–b"), and the whole change as
	/// a ```` ```diff ```` block of the original lines removed and the block's
	/// lines added.
	/// \details What Convert to file comment writes (owner, Q3). The result is
	/// a portable body (`classify_review_body`), on the same lines.
	[[nodiscard]] auto convert_suggestion_to_text(std::string_view aProse, std::uint32_t aFirst, std::uint32_t aLast, std::span<const std::string> aOriginal,
												  std::span<const std::string> aBlock) -> std::string;

	/// \brief Where a suggestion's lines are at a newer head, when every one of
	/// them is unchanged there and they stay together; none otherwise.
	/// \details `aOldToNew`'s left document is the head the edit was made on and
	/// its right one the newer head.
	[[nodiscard]] auto decide_suggestion_carry(const line_correspondence& aOldToNew, std::uint32_t aFirst, std::uint32_t aLast)
		-> std::optional<std::pair<std::uint32_t, std::uint32_t>>;
} // namespace bpr::core

#endif
