#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DEVICE_AUTHORIZATION_TRANSITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DEVICE_AUTHORIZATION_TRANSITION_HPP

#include "bpr/core/device_authorization_model.hpp"

namespace bpr::core {
	/// \brief Computes the device authorization commands result.
	[[nodiscard]] auto device_authorization_commands(
		const device_authorization_state& aState)
		-> std::vector<device_authorization_command_projection>;

	/// \brief Applies an accepted event to the supplied state.
	[[nodiscard]] auto transition(
		device_authorization_state&       aState,
		const device_authorization_event& aEvent)
		-> device_authorization_transition_output;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DEVICE_AUTHORIZATION_TRANSITION_HPP
