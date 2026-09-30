#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_DISCLOSURE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_DISCLOSURE_HPP

#include "bpr/core/review_cycle_feedback.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// \file
/// \brief What one private cycle note may carry out of the cycle
/// (RFC-BPR-0055, BPR-1154).
/// \details A note is private until parts of it are deliberately selected.
/// By default only the note's current wording travels: the latest revision of
/// its first turn, which the reviewer wrote. Replies, pasted or observed
/// answers, superseded wordings, private references, lineage and
/// identity-sensitive author details (adapter, provider, model, effort) stay
/// in the cycle unless they are named. Bodies travel as their exact bytes;
/// nothing here reads a label, rewrites text or asks a model.
namespace bpr::core {
	/// \brief Additions to the default disclosure of one note.
	struct cycle_disclosure_selection
	{
		/// Further revisions to include, by identity: another turn, or an
		/// earlier wording.
		std::vector<private_review_turn_revision_id> Revisions;
		/// Private related references to include, by their 1-based position
		/// among the related references.
		std::vector<std::uint32_t> PrivateReferences;
		/// Include adapter, provider, model and effort for the included
		/// revisions that record them.
		bool AuthorDetails = false;
	};

	struct cycle_disclosed_revision
	{
		private_review_turn_revision_id Revision;
		authored_provenance             Provenance = authored_provenance::unknown;
		/// Only when `AuthorDetails` was selected.
		std::optional<cycle_feedback_author> Details;
		std::string                          Body;
		auto                                 operator<=>(const cycle_disclosed_revision&) const = default;
	};
	/// \brief One note as it may leave the cycle. No derivation or reply
	/// links, thread version or lifecycle travel with it.
	struct cycle_disclosed_feedback
	{
		cycle_feedback_item_ref               Item;
		cycle_feedback_target                 Primary;
		std::vector<cycle_related_reference>  Related;
		std::vector<cycle_disclosed_revision> Revisions;
		auto                                  operator<=>(const cycle_disclosed_feedback&) const = default;
	};

	/// \brief Discloses one note from its complete revision history, in
	/// revision order. Refuses an archived note, a history that is not this
	/// thread's, and a selection naming a revision or reference it lacks.
	[[nodiscard]] auto disclose_feedback(const cycle_feedback_thread& aThread, std::span<const cycle_feedback_revision> aHistory, const cycle_disclosure_selection& aSelection)
		-> std::expected<cycle_disclosed_feedback, cycle_refusal>;
}

#endif
