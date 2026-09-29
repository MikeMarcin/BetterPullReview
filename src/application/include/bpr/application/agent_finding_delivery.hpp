#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_DELIVERY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_DELIVERY_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_finding_promotion.hpp"
#include "bpr/core/agent_session.hpp"

#include <compare>
#include <expected>
#include <optional>
#include <vector>

namespace bpr::application {
	/// \brief One returned finding and the verdict the core reached about it.
	///
	/// \details The two travel together because neither is usable alone: a
	/// finding without its verdict invites a reader to decide for itself
	/// whether the anchor holds, and a verdict without its finding names
	/// nothing. **Nothing here is dropped.** A finding whose anchor the frozen
	/// snapshot does not contain arrives with an `unanchored` verdict, because
	/// a reviewer who cannot see what the agent got wrong cannot correct it.
	struct agent_delivered_finding
	{
		core::agent_finding         Finding;
		core::agent_finding_verdict Verdict;

		auto operator<=>(const agent_delivered_finding&) const = default;
	};

	/// \brief One run's validated findings, bound to what the session asked about.
	///
	/// \details `Binding` is the session's own `agent_revision_binding` —
	/// always, and there is no field the binding current at delivery could
	/// occupy. That is how "never the head current at delivery" is a property
	/// of the type rather than of a branch somebody could invert.
	///
	/// `Staleness` says how the revision under review has moved *since*, which
	/// is a different question from whether any one anchor holds. A head that
	/// moved between dispatch and delivery makes the whole delivery stale
	/// without changing a single finding's analyzed revisions.
	struct agent_finding_delivery
	{
		core::agent_session_id        Session;
		core::agent_revision_binding  Binding;
		core::agent_session_staleness Staleness
			= core::agent_session_staleness::current;
		std::vector<agent_delivered_finding> Findings;

		auto operator<=>(const agent_finding_delivery&) const = default;
	};

	/// \brief Validates one decoded payload against the session's frozen snapshot.
	///
	/// \details This is the seam between decoding agent bytes and re-entering
	/// the host: every decoded finding passes `core::validate_agent_finding`
	/// here, and there is no other way into `agent_finding_delivery`. Pure —
	/// no store, no clock, no live alignment — so the verdict a reviewer sees
	/// can be replayed from the values that produced it.
	///
	/// The snapshot is supplied rather than looked up, for the reason
	/// `agent_finding_snapshot` exists: a validator that could reach a live
	/// alignment could be handed a newer one, and would then silently
	/// re-anchor an answer about an older revision.
	///
	/// \param aObserved The revision under review now, when the host still has
	/// one. Absent means the reviewer has moved away from this review
	/// entirely, which is reported as `different_review` rather than guessed at
	/// as `current`.
	///
	/// \returns A delivery carrying the session's own binding, the staleness of
	/// that binding against what is observed now, and one verdict per finding
	/// in the order the payload stated them. A payload naming a different
	/// session carries no findings: pairing the wrong payload with a session is
	/// a host mistake, and answering it with somebody else's findings is worse
	/// than answering it with none.
	[[nodiscard]] auto deliver_agent_findings(
		const agent_finding_payload&                       aPayload,
		const core::agent_session&                         aSession,
		const core::agent_finding_snapshot&                aSnapshot,
		const std::optional<core::agent_revision_binding>& aObserved)
		-> agent_finding_delivery;

	/// \brief Why one promoted finding did not become a durable private note.
	///
	/// \details Three separable causes, in the two-vocabulary shape
	/// `receipt_failure_publication` already uses: the core refused the
	/// promotion, the request named an artifact this path does not write, or
	/// the store refused the write.
	struct agent_note_promotion_failure
	{
		core::agent_finding_promotion_refusal Refusal
			= core::agent_finding_promotion_refusal::none;
		/// The request promoted into a draft or a defect. Those are applied
		/// through the draft and defect paths that already own them; this one
		/// writes notes and nothing else.
		bool                                WrongArtifact = false;
		std::optional<ledger_store_failure> Store;

		auto operator<=>(const agent_note_promotion_failure&) const = default;
	};

	/// \brief Builds the private note one promoted finding becomes.
	///
	/// \details The note's text is whatever `core::promote_agent_finding`
	/// produced — the claim, byte for byte. A record the durable authority
	/// would refuse is refused here, before any store sees it.
	[[nodiscard]] auto promoted_note(
		local_note_id                                aNote,
		std::optional<local_note_attachment>         aAttachment,
		const core::agent_finding_promotion_request& aRequest)
		-> std::expected<local_note_record, agent_note_promotion_failure>;

	/// \brief Stores one promoted finding as a private note, and nowhere else.
	///
	/// \details The single authority agent output may reach. The note's text is
	/// whatever `core::promote_agent_finding` produced — the claim, byte for
	/// byte — and this function composes nothing of its own. It touches exactly
	/// one port method, `review_ledger_store_port::store_note`; there is no
	/// branch here that could publish, open a defect, move a review head, or
	/// admit a session.
	///
	/// \returns The record as it was committed, or the exact cause. A record
	/// the durable authority would refuse never reaches the store.
	[[nodiscard]] auto store_promoted_finding(
		review_ledger_store_port&                    aStore,
		local_note_id                                aNote,
		std::optional<local_note_attachment>         aAttachment,
		const core::agent_finding_promotion_request& aRequest)
		-> std::expected<local_note_record, agent_note_promotion_failure>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_DELIVERY_HPP
