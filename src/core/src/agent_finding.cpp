#include "bpr/core/agent_finding.hpp"

#include <algorithm>
#include <ranges>

namespace bpr::core {
	namespace {
		template <typename Value>
		[[nodiscard]] constexpr auto enumerator_at_most(
			const Value aValue, const Value aMaximum) noexcept -> bool
		{
			return static_cast<std::uint64_t>(aValue)
				<= static_cast<std::uint64_t>(aMaximum);
		}

		/// \brief Builds a verdict that always carries the finding's own revisions.
		[[nodiscard]] auto verdict_for(
			const agent_finding&             aFinding,
			const agent_finding_anchor_state aState,
			const agent_finding_refusal      aRefusal) noexcept
			-> agent_finding_verdict
		{
			return {
				.State           = aState,
				.Refusal         = aRefusal,
				.AnalyzedBaseOid = aFinding.AnalyzedBaseOid,
				.AnalyzedHeadOid = aFinding.AnalyzedHeadOid,
			};
		}

		/// \brief The side the anchor names. Requires exactly one position.
		[[nodiscard]] auto anchor_side(
			const agent_finding_anchor& aAnchor) noexcept -> diff_side
		{
			return aAnchor.Line ? aAnchor.Line->Side : aAnchor.Boundary->Side;
		}

		/// \brief The comparison the anchor names. Requires exactly one position.
		[[nodiscard]] auto anchor_comparison(
			const agent_finding_anchor& aAnchor) noexcept
			-> file_comparison_digest
		{
			return aAnchor.Line ? aAnchor.Line->Comparison
								: aAnchor.Boundary->Comparison;
		}

		/// \brief Whether the finding attributes itself to a usable classification.
		///
		/// \details Confidence is deliberately absent from this list. It is the
		/// one stated field no verdict may turn on, so it is not read here even
		/// to reject it.
		[[nodiscard]] auto well_formed_attribution(
			const agent_finding& aFinding) noexcept -> bool
		{
			return aFinding.Provider.value() != 0U
				&& enumerator_at_most(aFinding.Severity, defect_severity::critical)
				&& enumerator_at_most(aFinding.Category, defect_category::other);
		}

		/// \brief Whether every stated string is present when required and bounded.
		[[nodiscard]] auto well_formed_text(const agent_finding& aFinding) noexcept
			-> bool
		{
			// An oversize path is malformed rather than unknown: it is beyond
			// the length any file identity can name, so it could not be the
			// path of a file whatever the snapshot holds.
			return !aFinding.Path.empty()
				&& aFinding.Path.size() <= maximum_identified_path_bytes
				&& !aFinding.Claim.empty()
				&& aFinding.Claim.size() <= maximum_finding_claim_bytes
				&& aFinding.Evidence.size() <= maximum_finding_evidence_bytes
				&& aFinding.Consequence.size() <= maximum_finding_consequence_bytes
				&& (!aFinding.SuggestedComment
					|| aFinding.SuggestedComment->size()
						<= maximum_finding_comment_bytes)
				&& aFinding.RequiredContext.size()
				<= maximum_finding_required_context_bytes;
		}

		/// \brief Whether the analyzed revisions are usable and bounded.
		[[nodiscard]] auto well_formed_revisions(
			const agent_finding& aFinding) noexcept -> bool
		{
			return aFinding.AnalyzedBaseOid.valid()
				&& aFinding.AnalyzedHeadOid.valid()
				&& aFinding.AnalyzedCommits.size() <= maximum_finding_analyzed_oids
				&& std::ranges::all_of(
					   aFinding.AnalyzedCommits, [](const git_commit_oid& aOid) {
						   return aOid.valid();
					   });
		}

		/// \brief Whether the anchor names exactly one place, inside its own range.
		///
		/// \details The claimed range must contain the anchored position. A
		/// finding whose range and anchor disagree names two different places,
		/// and every later reader would have to pick one.
		[[nodiscard]] auto well_formed_position(
			const agent_finding& aFinding) noexcept -> bool
		{
			const auto& anchor = aFinding.Anchor;
			if (
				anchor.Line.has_value() == anchor.Boundary.has_value()
				|| aFinding.Lines.Begin > aFinding.Lines.End) {
				return false;
			}
			if (anchor.Line) {
				return anchor.Line->Comparison.valid()
					&& enumerator_at_most(anchor.Line->Side, diff_side::right)
					&& anchor.Line->Line >= aFinding.Lines.Begin
					&& anchor.Line->Line < aFinding.Lines.End;
			}
			// A boundary is a seam rather than a line, so it may sit at either
			// end of the range and an empty range is a legitimate insertion
			// point rather than a contradiction.
			return anchor.Boundary->Comparison.valid()
				&& enumerator_at_most(anchor.Boundary->Side, diff_side::right)
				&& anchor.Boundary->Boundary >= aFinding.Lines.Begin
				&& anchor.Boundary->Boundary <= aFinding.Lines.End;
		}

		/// \brief Whether every stated semantic identity could be an identity.
		[[nodiscard]] auto well_formed_identities(
			const agent_finding_anchor& aAnchor) noexcept -> bool
		{
			return (!aAnchor.Segment || aAnchor.Segment->valid())
				&& (!aAnchor.Group || aAnchor.Group->valid())
				&& (!aAnchor.Move || aAnchor.Move->valid());
		}

		/// \brief The frozen record for one exact path, or nothing.
		[[nodiscard]] auto file_for_path(
			const agent_finding_snapshot& aSnapshot,
			const std::string&            aPath) noexcept
			-> const agent_finding_snapshot_file*
		{
			const auto match = std::ranges::find(
				aSnapshot.Files, aPath, &agent_finding_snapshot_file::Path);
			return match == aSnapshot.Files.end() ? nullptr : &*match;
		}

		template <typename Identity>
		[[nodiscard]] auto identity_frozen(
			const std::optional<Identity>& aIdentity,
			const std::vector<Identity>&   aFrozen) noexcept -> bool
		{
			return !aIdentity
				|| std::ranges::find(aFrozen, *aIdentity) != aFrozen.end();
		}

		/// \brief Whether the frozen alignment contains what the anchor names.
		[[nodiscard]] auto anchor_matches_frozen_file(
			const agent_finding_anchor&        aAnchor,
			const agent_finding_snapshot_file& aFile) noexcept -> bool
		{
			return anchor_comparison(aAnchor) == aFile.Comparison
				&& identity_frozen(aAnchor.Segment, aFile.Segments)
				&& identity_frozen(aAnchor.Group, aFile.Groups)
				&& identity_frozen(aAnchor.Move, aFile.Moves);
		}

		/// \brief Whether the claimed range fits inside the frozen file.
		///
		/// \details The side the anchor names selects which count applies, so
		/// the same range can hold on the right of a file and run past the end
		/// of its shorter left.
		///
		/// Only the range is measured. A well-formed anchor already lies inside
		/// its own range, so measuring the position again could refuse nothing
		/// the range does not, and a branch no input can reach is a branch no
		/// test can hold to account.
		[[nodiscard]] auto within_frozen_line_counts(
			const agent_finding&               aFinding,
			const agent_finding_snapshot_file& aFile) noexcept -> bool
		{
			const auto lineCount = anchor_side(aFinding.Anchor) == diff_side::left
				? aFile.LeftLineCount
				: aFile.RightLineCount;
			return valid(aFinding.Lines, lineCount);
		}
	} // namespace

	auto validate_agent_finding(
		const agent_finding&          aFinding,
		const agent_finding_snapshot& aSnapshot) noexcept
		-> agent_finding_verdict
	{
		// Nothing below reads aFinding.Confidence. An agent that says it is
		// certain gets the same verdict as one that says nothing, because the
		// snapshot decides whether an anchor holds and the agent does not.
		if (
			!well_formed_attribution(aFinding) || !well_formed_text(aFinding)
			|| !well_formed_revisions(aFinding) || !well_formed_position(aFinding)
			|| !well_formed_identities(aFinding.Anchor)) {
			return verdict_for(
				aFinding, agent_finding_anchor_state::unanchored,
				agent_finding_refusal::malformed);
		}
		// Before any line count is consulted, and deliberately so.
		if (
			aFinding.AnalyzedBaseOid != aSnapshot.Binding.BaseOid
			|| aFinding.AnalyzedHeadOid != aSnapshot.Binding.HeadOid) {
			return verdict_for(
				aFinding, agent_finding_anchor_state::stale,
				agent_finding_refusal::none);
		}
		const auto* file = file_for_path(aSnapshot, aFinding.Path);
		if (file == nullptr) {
			return verdict_for(
				aFinding, agent_finding_anchor_state::unanchored,
				agent_finding_refusal::unknown_path);
		}
		if (!anchor_matches_frozen_file(aFinding.Anchor, *file)) {
			return verdict_for(
				aFinding, agent_finding_anchor_state::unanchored,
				agent_finding_refusal::malformed);
		}
		if (!within_frozen_line_counts(aFinding, *file)) {
			return verdict_for(
				aFinding, agent_finding_anchor_state::unanchored,
				agent_finding_refusal::span_past_end);
		}
		return verdict_for(
			aFinding, agent_finding_anchor_state::anchored,
			agent_finding_refusal::none);
	}
} // namespace bpr::core
