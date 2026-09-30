#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_VERIFICATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_VERIFICATION_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_manifest.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

/// \file
/// \brief Reconciling a note with newer material, the reviewer's explicit
/// verification, and the next round's carried-forward selection
/// (RFC-BPR-0057, BPR-1165).
/// \details A recipient saying "addressed" asks for verification; it never is
/// one. Neither is code that disappeared, moved or now reads like the
/// suggestion. Only the reviewer's explicit decision verifies, and it names
/// what was checked: the item's exact wording and the material its locations
/// held in the checked revision. When that material or wording later
/// changes, the decision stays in the history but is no longer current, and
/// the item asks to be checked again. The next round is prepared from an
/// explicit selection that carries the previous assignment and the items
/// still open; nothing is sent by preparing it.
namespace bpr::core {
	/// \brief One reviewer decision, as appended. `Decision` absent is an
	/// explicit clearing of the earlier decision.
	struct cycle_verification_record
	{
		cycle_verification_ref                 Id;
		cycle_feedback_item_id                 Item;
		private_review_turn_revision_id        Wording;
		cycle_source_revision_id               Checked;
		cycle_material_digest                  Material;
		std::optional<cycle_reviewer_decision> Decision;
		auto                                   operator<=>(const cycle_verification_record&) const = default;
	};
	[[nodiscard]] auto stable_name(cycle_reviewer_decision aValue) noexcept -> std::string_view;

	/// \brief The material a note's locations name in one revision, as one
	/// digest: each named file's exact blob (or its absence) at the path, or,
	/// for a note on a module or the whole cycle, the revision's whole
	/// manifest. Proximity plays no part.
	[[nodiscard]] auto verification_material(const cycle_feedback_thread& aThread, const cycle_manifest& aManifest, const cycle_material_digest& aManifestDigest)
		-> cycle_material_digest;

	/// \brief Checks a decision before it is recorded: the item and wording
	/// must be the note's current ones and the revision one this cycle
	/// captured. A reviewer verifies what is in front of them, not an older wording.
	[[nodiscard]] auto check_verification(const review_cycle_state& aCycle, const cycle_feedback_thread& aThread, private_review_turn_revision_id aCurrentWording,
										  const cycle_verification_record& aRecord) -> std::optional<cycle_refusal>;

	/// \brief Whether a decision still applies to the note as it is now.
	enum class cycle_verification_currency : std::uint8_t {
		current,
		/// The checked material changed since; the decision is history.
		material_changed,
		/// The note's wording was amended since; the decision is history.
		item_amended,
	};
	[[nodiscard]] auto stable_name(cycle_verification_currency aValue) noexcept -> std::string_view;
	[[nodiscard]] auto verification_currency(const cycle_verification_record& aRecord, private_review_turn_revision_id aCurrentWording, const cycle_material_digest& aCurrentMaterial)
		-> cycle_verification_currency;

	/// \brief Where a note stands, from separate facts.
	enum class cycle_item_status : std::uint8_t {
		/// Nobody said anything and the reviewer decided nothing.
		open,
		/// A recipient said something other than "addressed"; nothing decided.
		reported,
		/// A recipient says it was addressed: the reviewer's check is needed.
		needs_verification,
		/// Verified against the material it holds now.
		verified,
		/// Verified earlier, but the material or wording changed: check again.
		recheck_needed,
		needs_work,
		withdrawn,
		deferred,
	};
	[[nodiscard]] auto stable_name(cycle_item_status aValue) noexcept -> std::string_view;

	/// \brief What one suggestion's exact result looks like in the reconciled revision.
	struct cycle_suggestion_reconciliation
	{
		cycle_suggestion_id Suggestion;
		/// `already_applied` only for an exact replacement whose exact result
		/// is the file now; an illustrative suggestion is never "present".
		cycle_application_observation Observation                                               = cycle_application_observation::unknown;
		auto                          operator<=>(const cycle_suggestion_reconciliation&) const = default;
	};
	/// \brief Everything known about one note against one revision, kept apart.
	struct cycle_item_reconciliation
	{
		cycle_feedback_item_id   Item;
		cycle_source_revision_id Against;
		/// Where the original attachment stands; absent for module and whole-cycle notes.
		std::optional<cycle_target_correspondence>   Primary;
		std::vector<cycle_suggestion_reconciliation> Suggestions;
		/// The latest recipient claim, from any statement.
		std::optional<cycle_response_disposition>  Claim;
		std::optional<cycle_verification_record>   Decision;
		std::optional<cycle_verification_currency> Currency;
		cycle_item_status                          Status                                              = cycle_item_status::open;
		auto                                       operator<=>(const cycle_item_reconciliation&) const = default;
	};
	struct cycle_reconciliation_inputs
	{
		const cycle_feedback_thread*                     Thread = nullptr;
		private_review_turn_revision_id                  Wording;
		cycle_source_revision_id                         Against;
		cycle_material_digest                            Material;
		std::optional<cycle_target_correspondence>       Primary;
		std::span<const cycle_suggestion_reconciliation> Suggestions;
		std::span<const cycle_item_statement>            Statements;
		/// This note's decisions in append order; the last one counts.
		std::span<const cycle_verification_record> Decisions;
	};
	/// \brief Reconciles one note. Its status comes only from the reviewer's
	/// decision and its currency, or else from what recipients said; code
	/// that moved, disappeared or matches a suggestion never resolves it.
	[[nodiscard]] auto reconcile_item(const cycle_reconciliation_inputs& aInputs) -> cycle_item_reconciliation;
	/// \brief True when the reviewer's current decision closes the note for
	/// the next round: verified or withdrawn, and still current.
	[[nodiscard]] auto resolved(const cycle_item_reconciliation& aItem) noexcept -> bool;

	/// \brief The next round's starting selection and what it left out.
	struct cycle_next_round_plan
	{
		cycle_round_selection               Selection;
		std::vector<cycle_feedback_item_id> Carried;
		std::vector<cycle_feedback_item_id> Added;
		/// Previous items the reviewer's current decision resolved.
		std::vector<cycle_feedback_item_id> Resolved;
		/// Previous items archived since.
		std::vector<cycle_feedback_item_id> Archived;
	};
	/// \brief The assignment a frozen round of this cycle carried, read back
	/// from its payload: its revision, action, goal, constraints, write scope
	/// and each item's wording, action, relations and suggestion revisions.
	/// Nothing when the payload names anything that is not a local identity.
	[[nodiscard]] auto round_assignment(const cycle_round_payload& aRound) -> std::optional<cycle_round_selection>;

	/// \brief One open note as it stands now, for the plan.
	struct cycle_next_round_item
	{
		cycle_feedback_item_id          Item;
		private_review_turn_revision_id Wording;
		private_thread_lifecycle        Lifecycle = private_thread_lifecycle::open;
		/// The latest revision of each suggestion still offered, in order: a
		/// new note brings all of them, a carried note the ones it carried.
		std::vector<cycle_round_suggestion_pin> Suggestions;
		/// Whether any frozen round already carried it.
		bool InRound                                         = false;
		bool Resolved                                        = false;
		auto operator<=>(const cycle_next_round_item&) const = default;
	};
	/// \brief Plans the next round: the previous round's action, goal,
	/// constraints and write scope; its items that are still unresolved, at
	/// their current wording and suggestion revisions, in its order; then open
	/// notes no round carried yet, in item order; the given source revision.
	/// Replies, earlier wordings and context are not carried: the reviewer adds
	/// what the next assignment needs. Nothing is frozen or sent.
	[[nodiscard]] auto plan_next_round(const cycle_round_selection& aPrevious, std::span<const cycle_next_round_item> aItems, cycle_source_revision_id aSource)
		-> cycle_next_round_plan;
}

#endif
