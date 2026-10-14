#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_COMMENT_RANGE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_COMMENT_RANGE_HPP

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/// \file
/// \brief Where a new pull-request comment may go when the reviewer selected
/// one line or several ([[BPR-1212]]).
/// \details A comment on several lines is GitHub's multi-line review comment:
/// `start_line` and `line` on one side. GitHub refuses a range that is not on
/// one side or whose two ends are not in one hunk of the pull request's diff,
/// so those are refused here, from the accepted diff, before any draft carries
/// the range and before anything is published. A single line on the right
/// keeps today's rules: a line outside every hunk is decided at Publish by
/// `plan_review_publication`. A comment on the left (base) side is GitHub's
/// `side: LEFT`: its lines count the merge base's version of the file, so it
/// is admitted only with the host's evidence that this comparison's base side
/// is numbered as that version, and only inside a hunk, because a base line
/// has no file-comment fallback. The suggestion block a selection starts is
/// `suggestion_draft.hpp`'s.
namespace bpr::core {
	/// \brief Why a selection cannot carry a new comment.
	enum class comment_range_refusal : std::uint8_t {
		none,
		/// The two ends name different comparisons.
		mixed_comparison,
		/// The two ends are on different sides.
		mixed_side,
		/// The left (base) side is not known to be numbered as GitHub's base.
		base_side,
		/// An end is past the side's last line, or the side has no text.
		outside_document,
		/// Several lines, none of them in a hunk of the diff.
		outside_hunk,
		/// Several lines whose two ends are in different hunks.
		crosses_hunk,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(comment_range_refusal aRefusal) noexcept -> std::string_view;

	/// \brief What a selection becomes as a draft's anchor.
	struct comment_range_decision
	{
		comment_range_refusal Refusal = comment_range_refusal::none;
		/// The last line, which the draft anchors to (GitHub's `line`).
		source_line_ref Anchor;
		/// The first line when there are several (GitHub's `start_line`).
		std::optional<std::uint32_t> StartLine;

		auto operator<=>(const comment_range_decision&) const = default;
	};

	/// \brief Lines of context GitHub's pull-request diff keeps around a change.
	inline constexpr std::uint32_t github_hunk_context_lines = 3U;

	/// \brief Unchanged lines GitHub's pull-request file list lets separate two
	/// hunks it still joins into one (`git diff --inter-hunk-context=1`).
	/// \details The raw `.diff` joins only touching hunks; the file list, whose
	/// hunks a review comment must stay inside, joins one line further
	/// ([[BPR-1482]], from [[RFC-BPR-0082]]'s recorded oracle).
	inline constexpr std::uint32_t github_inter_hunk_context_lines = 1U;

	/// \brief The hunks of one side as GitHub's file list draws them: each
	/// change group's lines with three lines of context either side, hunks
	/// that overlap, touch or lie at most one unchanged line apart merged, in
	/// line order.
	/// \details A deletion contributes its context on the right. The diff
	/// options and the comparison are the accepted ones; where they differ
	/// from GitHub's own diff, GitHub's refusal at Publish still stands.
	[[nodiscard]] auto github_review_hunks(const file_alignment& aAlignment, diff_side aSide) -> std::vector<line_span>;

	/// \brief Decides a new comment's anchor from a selection's two ends, in
	/// either order.
	/// \details One line on the right is the anchor itself and is refused only
	/// outside the document. Several lines must be on one side and inside one
	/// hunk of `github_review_hunks`. Lines on the left need
	/// `aBaseNumberedAsGitHub`, the host's evidence that the comparison's left
	/// document is the merge base's version GitHub numbers `LEFT` lines in;
	/// without it they are `base_side`. A single left line must also be in a
	/// hunk.
	[[nodiscard]] auto decide_comment_range(
		const file_alignment& aAlignment,
		source_line_ref       aFirst,
		source_line_ref       aLast,
		bool                  aBaseNumberedAsGitHub = false) -> comment_range_decision;

} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_COMMENT_RANGE_HPP
