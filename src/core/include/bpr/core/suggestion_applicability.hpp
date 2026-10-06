#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_APPLICABILITY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_APPLICABILITY_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/review_publication_policy.hpp"

#include <compare>
#include <cstdint>
#include <string_view>

/// \file
/// \brief Whether a comment's suggested change would be GitHub's native,
/// applicable suggestion where it is about to be published, or only text
/// ([[BPR-1084]]; RFC-BPR-0048 R13, RFC-BPR-0050 R32).
/// \details The decision is made before Publish from facts the host already
/// holds: the body, what the comment is attached to, and the placement and
/// capability evidence `plan_review_publication` already reads. It asks no
/// provider anything, never moves a comment to another line, and never claims
/// an apply affordance BPR does not have: "native" means the comment goes to
/// GitHub as an inline comment on its exact line, where GitHub itself offers
/// to apply the suggestion; BPR applies nothing.
namespace bpr::core {
	/// \brief What the comment is attached to on GitHub, as the accepted
	/// projection or the composition observed it.
	enum class suggestion_subject : std::uint8_t {
		/// One line of the pull request's current head: a new inline comment
		/// on the selected line, or a reply in a thread on such a line.
		current_line,
		/// A file comment, such as a thread GitHub stores as a file comment
		/// that its code link places at a line (RFC-BPR-0022).
		file,
		/// A thread whose line belongs to an older revision of the file.
		outdated_line,
		/// A thread with no line GitHub can name now.
		unavailable,
		/// The pull request's own conversation, which has no file or line.
		conversation,
	};

	/// \brief Bounded facts for one decision; the host validated where they came from.
	struct suggestion_placement_observation
	{
		suggestion_subject Subject = suggestion_subject::unavailable;
		/// The side of the line; only `current_line` reads it.
		diff_side Side = diff_side::right;
		/// Provider evidence about the line, as `plan_review_publication` reads
		/// it. `unknown` is the ordinary case: GitHub is asked for an inline
		/// comment and either takes it or refuses it, and a refused suggestion
		/// is never sent as a file comment instead.
		review_placement             Placement  = review_placement::unknown;
		off_hunk_creation_capability Capability = off_hunk_creation_capability::unknown;
		/// The capability evidence is current for this exact scope.
		bool CapabilityCurrent = false;

		auto operator==(const suggestion_placement_observation&) const -> bool = default;
	};

	enum class suggestion_applicability : std::uint8_t {
		/// The body has no suggested-change fence; there is nothing to apply.
		no_patch,
		/// The body has a fence and goes to GitHub as an inline comment on its
		/// exact line, where GitHub shows its own suggestion and apply action.
		native,
		/// The body has a fence, but GitHub cannot apply a change here. The
		/// text is kept exactly as written; nothing is moved to another line.
		textual_only,
	};

	enum class suggestion_text_reason : std::uint8_t {
		none,
		/// The line is on the base side; GitHub applies suggestions to the head.
		base_side,
		/// A file comment has no line to apply a change to.
		file_subject,
		/// The thread's line is outdated.
		outdated_line,
		/// The thread has no current line.
		unavailable_anchor,
		/// A pull request comment has no file or line.
		conversation,
		/// GitHub refused an inline comment on this line. The only placement
		/// left is a file comment with a link, which `plan_review_publication`
		/// refuses for a suggestion (`incompatible_body`).
		inline_comment_unsupported,
		/// The observation is out of range; nothing is claimed.
		invalid_observation,
	};

	struct suggestion_applicability_decision
	{
		suggestion_applicability Applicability = suggestion_applicability::no_patch;
		suggestion_text_reason   Reason        = suggestion_text_reason::none;

		auto operator<=>(const suggestion_applicability_decision&) const = default;
	};

	[[nodiscard]] auto stable_name(suggestion_applicability aApplicability) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(suggestion_text_reason aReason) noexcept -> std::string_view;

	/// \brief Decide what a body's suggested change would be at one placement.
	/// \details A body is a suggestion when `classify_review_body` says it
	/// needs a line subject, the same conservative recognizer the publication
	/// policy and the GitHub adapter use. A current head line is then planned
	/// with `plan_review_publication` from its initial state: a native attempt
	/// is `native`, and a refusal for `incompatible_body` (the line can only
	/// fall back to a file comment) is `textual_only`.
	[[nodiscard]] auto decide_suggestion_applicability(std::string_view aBody, const suggestion_placement_observation& aPlacement) noexcept -> suggestion_applicability_decision;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_APPLICABILITY_HPP
