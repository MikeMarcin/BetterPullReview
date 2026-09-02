#include "bpr/core/agent_finding_promotion.hpp"

namespace bpr::core {
	namespace {
		/// \brief A result carrying no command and the reason there is none.
		[[nodiscard]] auto refuse(const agent_finding_promotion_refusal aRefusal)
			-> agent_finding_promotion
		{
			return { .Refusal = aRefusal };
		}

		/// \brief Whether the choice is one this build declared.
		[[nodiscard]] auto declared(
			const agent_finding_promotion_choice aChoice) noexcept -> bool
		{
			return static_cast<std::size_t>(aChoice)
				< agent_finding_promotion_choice_count;
		}

		/// \brief The comparison the anchor belongs to, or an invalid identity.
		///
		/// \details Unlike the validator's own accessor this one is total: a
		/// promotion may be handed an anchor naming no position, and reading
		/// through an absent one to find that out is not a mistake worth
		/// risking.
		[[nodiscard]] auto anchor_comparison(
			const agent_finding_anchor& aAnchor) noexcept -> file_comparison_digest
		{
			if (aAnchor.Line) {
				return aAnchor.Line->Comparison;
			}
			if (aAnchor.Boundary) {
				return aAnchor.Boundary->Comparison;
			}
			return { };
		}

		/// \brief Whether the finding carries the facts a promotion copies.
		///
		/// \details Not a second validation. `validate_agent_finding` decides
		/// whether an anchor holds, and this decides only whether the values
		/// promotion is about to copy exist at all — a file identity, one
		/// position, its comparison, and a claim. A promotion that filled any
		/// of them from nothing would produce an artifact naming a place or
		/// saying a thing that no finding did.
		[[nodiscard]] auto promotable(const agent_finding& aFinding) noexcept -> bool
		{
			return aFinding.Anchor.Line.has_value()
				!= aFinding.Anchor.Boundary.has_value()
				&& file_identity_of(aFinding.Path).valid()
				&& anchor_comparison(aFinding.Anchor).valid()
				&& !aFinding.Claim.empty();
		}

		/// \brief Whether the verdict was reached about this exact finding.
		///
		/// \details Both values carry the revisions they were computed against,
		/// and a verdict whose revisions are not the finding's own is an answer
		/// to a different question. This is the seam between validation and
		/// promotion, and it is the one place a caller could pair the two up
		/// wrongly.
		[[nodiscard]] auto verdict_is_about(
			const agent_finding&         aFinding,
			const agent_finding_verdict& aVerdict) noexcept -> bool
		{
			return aVerdict.AnalyzedBaseOid == aFinding.AnalyzedBaseOid
				&& aVerdict.AnalyzedHeadOid == aFinding.AnalyzedHeadOid;
		}

		/// \brief The refusal the request earns before any target is consulted.
		///
		/// \details Returns `none` when the finding may be promoted into
		/// something. The order here is the policy the header describes.
		[[nodiscard]] auto refusal_before_target(
			const agent_finding_promotion_request& aRequest) noexcept
			-> agent_finding_promotion_refusal
		{
			if (!declared(aRequest.Choice)) {
				return agent_finding_promotion_refusal::unknown_choice;
			}
			if (aRequest.Standing == agent_finding_standing::dismissed) {
				return agent_finding_promotion_refusal::dismissed;
			}
			if (!verdict_is_about(aRequest.Finding, aRequest.Verdict)) {
				return agent_finding_promotion_refusal::foreign_verdict;
			}
			switch (aRequest.Verdict.State) {
			case agent_finding_anchor_state::anchored:
				break;
			case agent_finding_anchor_state::unanchored:
				return agent_finding_promotion_refusal::unanchored;
			case agent_finding_anchor_state::stale:
				return agent_finding_promotion_refusal::stale;
			}
			if (!promotable(aRequest.Finding)) {
				return agent_finding_promotion_refusal::malformed_finding;
			}
			return agent_finding_promotion_refusal::none;
		}

		/// \brief The refusal a line-anchored target earns, or `none`.
		///
		/// \details Shared by the draft and the defect because they are anchored
		/// by the same identity and must therefore be refused for the same
		/// reasons. Two copies of this rule could drift, and a defect quietly
		/// accepting an anchor a draft refused is exactly the drift that would
		/// matter.
		[[nodiscard]] auto refusal_for_line_target(
			const agent_finding&                  aFinding,
			const std::optional<source_line_ref>& aTargetAnchor) noexcept
			-> agent_finding_promotion_refusal
		{
			if (!aFinding.Anchor.Line) {
				return agent_finding_promotion_refusal::boundary_anchor;
			}
			if (!aTargetAnchor) {
				return agent_finding_promotion_refusal::unnamed_target;
			}
			if (*aTargetAnchor != *aFinding.Anchor.Line) {
				return agent_finding_promotion_refusal::anchor_elsewhere;
			}
			return agent_finding_promotion_refusal::none;
		}

		/// \brief Puts the agent's suggested comment into the reviewer's draft.
		///
		/// \details The body is the suggestion copied. It is not trimmed,
		/// re-wrapped, prefixed with the agent's name, or annotated as
		/// machine-written, because every one of those is this tool editing text
		/// the reviewer is about to be shown as their own.
		///
		/// The command is an `edit`. There is no branch here that could emit
		/// `begin_publish`, which is the only kind that would move a draft into
		/// the state `propose_publication` accepts.
		[[nodiscard]] auto promote_into_draft(
			const agent_finding_promotion_request& aRequest)
			-> agent_finding_promotion
		{
			const auto anchor = aRequest.Draft
				? std::optional<source_line_ref>{ aRequest.Draft->Anchor }
				: std::optional<source_line_ref>{ };
			const auto refusal
				= refusal_for_line_target(aRequest.Finding, anchor);
			if (refusal != agent_finding_promotion_refusal::none) {
				return refuse(refusal);
			}
			if (!aRequest.Finding.SuggestedComment) {
				return refuse(
					agent_finding_promotion_refusal::no_suggested_comment);
			}
			return {
				.Draft = review_draft_command{
					.Id   = aRequest.Draft->Id,
					.Kind = review_draft_command_kind::edit,
					.Body = *aRequest.Finding.SuggestedComment,
				},
				.Refusal = agent_finding_promotion_refusal::none,
			};
		}

		/// \brief Gives the reviewer's defect the classification the agent gave.
		[[nodiscard]] auto promote_into_defect(
			const agent_finding_promotion_request& aRequest)
			-> agent_finding_promotion
		{
			const auto anchor = aRequest.Defect
				? std::optional<source_line_ref>{ aRequest.Defect->Anchor }
				: std::optional<source_line_ref>{ };
			const auto refusal
				= refusal_for_line_target(aRequest.Finding, anchor);
			if (refusal != agent_finding_promotion_refusal::none) {
				return refuse(refusal);
			}
			return {
				.Defect = defect_command{
					.Id       = aRequest.Defect->Id,
					.Kind     = defect_command_kind::reclassify,
					.Severity = aRequest.Finding.Severity,
					.Category = aRequest.Finding.Category,
				},
				.Refusal = agent_finding_promotion_refusal::none,
			};
		}

		/// \brief Writes the claim down privately, attached to the exact file.
		///
		/// \details A seam is a place a note may legitimately be about, so this
		/// is the one choice a boundary-anchored finding can still take. The
		/// attachment is the file identity derived from the finding's path and
		/// the comparison its anchor belongs to, which is everything
		/// `application::local_note_attachment` needs and nothing else.
		[[nodiscard]] auto promote_into_note(
			const agent_finding_promotion_request& aRequest)
			-> agent_finding_promotion
		{
			return {
				.Note = supplement_note{
					.File       = file_identity_of(aRequest.Finding.Path),
					.Comparison = anchor_comparison(aRequest.Finding.Anchor),
					.Text       = aRequest.Finding.Claim,
				},
				.Refusal = agent_finding_promotion_refusal::none,
			};
		}
	} // namespace

	auto promote_agent_finding(const agent_finding_promotion_request& aRequest)
		-> agent_finding_promotion
	{
		const auto refusal = refusal_before_target(aRequest);
		if (refusal != agent_finding_promotion_refusal::none) {
			return refuse(refusal);
		}
		// Exactly one of these runs, and each returns at most one command. The
		// choice is a selection rather than a set of flags, so there is no
		// arrangement of inputs that produces a draft and a defect together.
		switch (aRequest.Choice) {
		case agent_finding_promotion_choice::draft_comment:
			return promote_into_draft(aRequest);
		case agent_finding_promotion_choice::defect:
			return promote_into_defect(aRequest);
		case agent_finding_promotion_choice::private_note:
			return promote_into_note(aRequest);
		}
		return refuse(agent_finding_promotion_refusal::unknown_choice);
	}
} // namespace bpr::core
