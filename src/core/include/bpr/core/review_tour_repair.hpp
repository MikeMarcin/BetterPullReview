#ifndef BPR_CORE_REVIEW_TOUR_REPAIR_HPP
#define BPR_CORE_REVIEW_TOUR_REPAIR_HPP

#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/tour_agent_session.hpp"

namespace bpr::core {
	/// \brief Why this stop cannot join a pending review, independent of wording.
	enum class tour_destination_problem : std::uint8_t {
		none,
		historical_exact,
		historical_changed,
		historical_ambiguous,
		historical_missing,
		historical_unavailable,
		off_hunk,
		unchanged_file,
		operation_unavailable,
		incompatible_body,
		source_unavailable,
	};
	enum class tour_source_repair_kind : std::uint8_t { repin_head,
														nearest_hunk };

	/// \brief One stop's typed destination and available source edits. Ranges are
	/// zero-based, like the tour; provider hunks in tour_anchor_file are one-based.
	/// FileComment only reports an exactly qualified file operation; it grants no
	/// publication or staging authority. The host binds all observations to Head.
	struct tour_destination_repair
	{
		tour_stop_id                     Stop;
		std::size_t                      Number = 0U;
		std::string                      Title;
		tour_destination_plan            Destination;
		tour_destination_problem         Problem = tour_destination_problem::none;
		std::optional<tour_source_range> Repin;
		std::optional<tour_source_range> Relocation;
		bool                             FileComment                                              = false;
		auto                             operator==(const tour_destination_repair&) const -> bool = default;
	};

	/// \brief Observe a stop against the exact current-head provider hunks, then
	/// plan its destination and repairs. An observation for another source fails
	/// closed. Unknown patch evidence never supplies a relocation or an exact re-pin.
	[[nodiscard]] auto plan_tour_stop_repair(const review_tour& aTour, const tour_stop& aStop,
											 const tour_destination_observation& aObservation, std::span<const tour_anchor_file> aFiles) -> tour_destination_repair;
	/// \brief Report every stop that cannot join a native line/file pending
	/// review, in route order, including unresolved destinations. Observations
	/// correspond one-for-one to stops; absent/mismatched entries stay unavailable.
	[[nodiscard]] auto tour_destination_repairs(const review_tour&                            aTour,
												std::span<const tour_destination_observation> aObservations, std::span<const tour_anchor_file> aFiles) -> std::vector<tour_destination_repair>;
	/// \brief Prepare one ordinary revision-bound restore edit, suitable for the
	/// tour host's existing undo/autosave lane. Recomputes the repair from current
	/// evidence. It preserves prose, identity and remote discussion; line-number
	/// references to moved lines require reaffirmation. Published tours/subjects
	/// are refused; their maintenance uses its separate plan. No remote effect occurs.
	[[nodiscard]] auto prepare_tour_source_repair(const review_tour& aTour, tour_stop_id aStop,
												  tour_source_repair_kind aKind, const tour_destination_observation& aObservation,
												  std::span<const tour_anchor_file> aFiles) -> std::optional<tour_command>;
}
#endif
