#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_FEEDBACK_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_FEEDBACK_HPP

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_feedback.hpp"

#include <optional>
#include <span>
#include <stop_token>

/// \file
/// \brief Feedback targets checked against retained material (RFC-BPR-0055,
/// BPR-1153).
/// \details A note is attached to the exact bytes a revision retained, never
/// to the workspace: targets are verified by reading the pinned object, and a
/// target's place in newer material is observed from that revision's
/// retained bytes. Runs on the effect lane.
namespace bpr::application {
	/// \brief Verifies every file and anchor target against its revision's
	/// retained bytes. A target whose bytes differ is refused as
	/// `invalid_target`; missing, corrupt or cancelled reads are reported as
	/// such and nothing is substituted.
	[[nodiscard]] auto verify_feedback_targets(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject,
											   std::span<const core::cycle_feedback_target* const> aTargets, std::stop_token aStop) -> cycle_store_result<void>;

	/// \brief Where one target stands in one revision.
	struct cycle_correspondence_observation
	{
		review_material_status Status = review_material_status::failure;
		/// Absent for whole-cycle and module targets, and when the revision's
		/// material could not be read at all.
		std::optional<core::cycle_target_correspondence> Correspondence;
	};
	/// \brief Observes a target against `aAgainst` from retained material only.
	[[nodiscard]] auto observe_feedback_target(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_feedback_target& aTarget,
											   core::cycle_source_revision_id aAgainst, std::stop_token aStop) -> cycle_correspondence_observation;
}

#endif
