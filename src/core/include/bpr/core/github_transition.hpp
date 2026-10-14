#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_TRANSITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_TRANSITION_HPP

#include "bpr/core/github_model.hpp"

namespace bpr::core {
	/// \brief Whether another user connection setup may begin before validation finishes.
	[[nodiscard]] auto github_connection_setup_available(const github_state& aState) noexcept -> bool;

	/// \brief Computes the github commands result.
	[[nodiscard]] auto github_commands(const github_state& aState)
		-> std::vector<github_command_projection>;

	/// \brief Applies an accepted event to the supplied state.
	[[nodiscard]] auto transition(
		github_state&       aState,
		const github_event& aEvent) -> github_transition_output;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_TRANSITION_HPP
