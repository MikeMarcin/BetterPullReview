#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_TRANSITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_TRANSITION_HPP

#include "bpr/core/model.hpp"

namespace bpr::core {
	/// \brief Computes the resolve shared revision result.
	[[nodiscard]] auto resolve_shared_revision(
		const foundation_observation& aObservation) noexcept -> std::uint64_t;

	/// \brief Applies an accepted event to the supplied state.
	[[nodiscard]] auto transition(
		application_state&            aState,
		const application_event&      aEvent,
		const foundation_observation& aObservation) -> transition_output;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_TRANSITION_HPP
