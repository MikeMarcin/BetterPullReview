#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_WORKSHOP_CANDIDATE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_WORKSHOP_CANDIDATE_HPP

#include "bpr/core/feedback_convention.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1091]]. What a comment-workshop candidate changes besides its
/// wording (RFC-BPR-0051 R37).
/// \details The workshop shows the reviewer's original beside the agent's
/// candidate. The words are compared by reading them; three things are easy to
/// miss there and are named instead: the Conventional Comments label and its
/// decorations, whether the comment still asks a question, and the bytes of
/// any suggested change. The label is read by the shared recognizer
/// (`recognize_feedback_prefix`) on both bodies, so the reviewer sees the same
/// semantics that would be published, and a candidate whose header it cannot
/// read says so rather than being treated as plain. Nothing here edits either
/// body, refuses a candidate or performs an effect: accepting the candidate
/// stays the reviewer's one local edit.
namespace bpr::core {
	/// \brief The label facts of one body, as the shared recognizer read them.
	struct workshop_label_facts
	{
		feedback_prefix_status        Status      = feedback_prefix_status::plain;
		std::optional<feedback_label> Label       = std::nullopt;
		feedback_expectation          Expectation = feedback_expectation::unspecified;
		/// The label token exactly as written; empty when none was read.
		std::string RawLabel;
		/// Each decoration exactly as written, in order.
		std::vector<std::string> Decorations;

		auto operator<=>(const workshop_label_facts&) const = default;
	};

	enum class workshop_label_change : std::uint8_t {
		/// Neither body has a header.
		none,
		/// Both have the same header semantics.
		kept,
		/// The original had none; the candidate has one.
		added,
		/// The original had one; the candidate has none.
		removed,
		/// Label, decorations or readability differ.
		changed,
	};

	enum class workshop_question_change : std::uint8_t {
		/// Neither body asks a question.
		none,
		kept,
		added,
		/// The candidate states what the original asked.
		removed,
	};

	enum class workshop_patch_change : std::uint8_t {
		/// Neither body has a suggested change.
		none,
		/// Every suggested-change block is byte-identical.
		kept,
		added,
		removed,
		/// Both have suggested changes and their bytes differ.
		changed,
	};

	[[nodiscard]] auto stable_name(workshop_label_change aChange) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(workshop_question_change aChange) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(workshop_patch_change aChange) noexcept -> std::string_view;

	/// \brief What one candidate changes against the original, besides wording.
	struct workshop_candidate_change
	{
		workshop_label_facts     Before;
		workshop_label_facts     After;
		workshop_label_change    Label    = workshop_label_change::none;
		workshop_question_change Question = workshop_question_change::none;
		workshop_patch_change    Patch    = workshop_patch_change::none;

		/// \brief The candidate keeps the label, the question and the patch.
		[[nodiscard]] auto prose_only() const noexcept -> bool
		{
			return (Label == workshop_label_change::none || Label == workshop_label_change::kept)
				&& (Question == workshop_question_change::none || Question == workshop_question_change::kept)
				&& (Patch == workshop_patch_change::none || Patch == workshop_patch_change::kept);
		}
		/// \brief The shared recognizer reads the candidate's header, or it has none.
		[[nodiscard]] auto candidate_readable() const noexcept -> bool
		{
			return After.Status == feedback_prefix_status::recognized || After.Status == feedback_prefix_status::plain;
		}

		auto operator<=>(const workshop_candidate_change&) const = default;
	};

	/// \brief The label facts of one ordinary body.
	[[nodiscard]] auto workshop_label_facts_of(std::string_view aBody) -> workshop_label_facts;

	/// \brief Whether the body's prose asks a question.
	/// \details A `?` ends a sentence when it is followed by the end of the
	/// body, a space, a line break or closing punctuation. Fenced blocks and
	/// inline code are not prose, so `a ? b : c` in code and `?page=2` in a link
	/// ask nothing.
	[[nodiscard]] auto workshop_asks_question(std::string_view aBody) -> bool;

	/// \brief Compares a candidate with the original it would replace.
	[[nodiscard]] auto compare_workshop_candidate(std::string_view aOriginal, std::string_view aCandidate) -> workshop_candidate_change;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_WORKSHOP_CANDIDATE_HPP
