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
/// the range and before anything is published. A single line keeps today's
/// rules: a new comment goes on the right side, and a line outside every hunk
/// is decided at Publish by `plan_review_publication`. The suggestion block a
/// selection starts is `suggestion_draft.hpp`'s.
namespace bpr::core {
	/// \brief Why a selection cannot carry a new comment.
	enum class comment_range_refusal : std::uint8_t {
		none,
		/// The two ends name different comparisons.
		mixed_comparison,
		/// The two ends are on different sides.
		mixed_side,
		/// A new comment cannot go on the left (base) side.
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

	/// \brief The hunks of one side as GitHub's diff draws them: each change
	/// group's lines with three lines of context either side, touching or
	/// overlapping hunks merged, in line order.
	/// \details A deletion contributes its context on the right. The diff
	/// options and the comparison are the accepted ones; where they differ
	/// from GitHub's own diff, GitHub's refusal at Publish still stands.
	[[nodiscard]] auto github_review_hunks(const file_alignment& aAlignment, diff_side aSide) -> std::vector<line_span>;

	/// \brief Decides a new comment's anchor from a selection's two ends, in
	/// either order.
	/// \details One line is the anchor itself and is refused only on the left
	/// side or outside the document. Several lines must be on one side, the
	/// right, and inside one hunk of `github_review_hunks`.
	[[nodiscard]] auto decide_comment_range(
		const file_alignment& aAlignment,
		source_line_ref       aFirst,
		source_line_ref       aLast) -> comment_range_decision;

} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_COMMENT_RANGE_HPP
