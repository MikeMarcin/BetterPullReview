#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_FEEDBACK_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_FEEDBACK_HPP

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

/// \file
/// \brief Feedback targets and suggestions checked against retained material
/// (RFC-BPR-0055, BPR-1153, BPR-1155).
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

	/// \brief A suggestion's before and after, from its retained original.
	struct cycle_suggestion_preview_observation
	{
		review_material_status Status = review_material_status::failure;
		/// The whole original file.
		std::vector<std::uint8_t>                     Before;
		std::optional<core::cycle_suggestion_preview> Preview;
	};
	/// \brief Previews a suggestion in memory. Nothing is written.
	[[nodiscard]] auto preview_cycle_suggestion(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_suggestion& aSuggestion,
												std::stop_token aStop) -> cycle_suggestion_preview_observation;

	/// \brief The whole-set decision against one revision's retained file.
	struct cycle_suggestion_preflight_observation
	{
		review_material_status Status = review_material_status::failure;
		/// The revision whose file was checked.
		core::cycle_source_revision_id                  Against;
		std::optional<core::cycle_suggestion_preflight> Preflight;
	};
	/// \brief Preflights suggestions to one file against `aAgainst`'s bytes
	/// at that path, or against their own source revision. Any result stays
	/// in memory; nothing is applied to a workspace.
	[[nodiscard]] auto preflight_cycle_suggestions(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, std::span<const core::cycle_suggestion> aSuggestions,
												   std::optional<core::cycle_source_revision_id> aAgainst, std::stop_token aStop) -> cycle_suggestion_preflight_observation;
	/// \brief Confirms from retained bytes that `aResult` holds exactly the
	/// suggested result before it may be recorded as already applied. Any
	/// other file, however similar, is refused as `invalid_target`.
	[[nodiscard]] auto confirm_suggestion_applied(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_suggestion& aSuggestion,
												  core::cycle_source_revision_id aResult, std::stop_token aStop) -> cycle_store_result<void>;
}

#endif
