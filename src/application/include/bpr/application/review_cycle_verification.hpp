#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_VERIFICATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_VERIFICATION_HPP

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_verification.hpp"

#include <optional>
#include <stop_token>
#include <vector>

/// \file
/// \brief Reviewer decisions, reconciliation against a revision and the next
/// round's draft, from retained records (RFC-BPR-0057, BPR-1165).
/// \details A decision records the note's current wording and the digest of
/// the material its locations held in the checked revision, read from that
/// revision's manifest. Reconciliation reads each note's correspondence, its
/// suggestions' exact results, recipients' statements and the reviewer's
/// decisions, and keeps them apart. Preparing the next round saves a draft
/// from the latest frozen round's assignment and the notes still open; it
/// freezes, copies and sends nothing. Runs on the effect lane.
namespace bpr::application {
	struct cycle_decision_request
	{
		core::cycle_feedback_item_ref                Item;
		std::optional<core::cycle_reviewer_decision> Decision;
		/// The revision the reviewer checked.
		core::cycle_source_revision_id Checked;
	};
	struct cycle_decision_result
	{
		std::optional<cycle_write_receipt>             Receipt;
		std::optional<core::cycle_refusal>             Refusal;
		std::optional<core::cycle_verification_record> Record;
	};
	/// \brief Records one decision about a note's current wording against the
	/// checked revision's retained material.
	[[nodiscard]] auto record_item_decision(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_decision_request& aRequest,
											const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_decision_result>;

	/// \brief Every note reconciled against `aAgainst`, in item order.
	[[nodiscard]] auto reconcile_cycle_items(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, core::cycle_source_revision_id aAgainst,
											 std::stop_token aStop) -> cycle_store_result<std::vector<core::cycle_item_reconciliation>>;

	/// \brief Every note of the cycle, in item order, as a round would carry
	/// it now: its current wording, the suggestions it still offers and its
	/// lifecycle (BPR-1161). Whether a round carried it, or a decision
	/// resolved it, is left unset.
	[[nodiscard]] auto current_note_items(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop)
		-> cycle_store_result<std::vector<core::cycle_next_round_item>>;

	struct cycle_next_round_result
	{
		/// The new draft, when a previous round existed to carry forward.
		std::optional<core::cycle_round_draft_ref> Draft;
		std::optional<core::cycle_round_id>        From;
		core::cycle_next_round_plan                Plan;
	};
	/// \brief Saves a new draft planned from the latest frozen round against
	/// the latest capture. With no frozen round there is nothing to carry and
	/// nothing is written.
	[[nodiscard]] auto prepare_next_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aBegin,
										  const cycle_operation_id& aSave, std::stop_token aStop) -> cycle_store_result<cycle_next_round_result>;
}

#endif
