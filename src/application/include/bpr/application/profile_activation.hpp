#ifndef BPR_APPLICATION_PROFILE_ACTIVATION_HPP
#define BPR_APPLICATION_PROFILE_ACTIVATION_HPP

#include "bpr/core/profile_window.hpp"

#include <memory>
#include <optional>

namespace bpr::application {
	enum class profile_activation_outcome { queued,
											refused,
											unavailable,
											indeterminate };
	/// \brief A single terminal acknowledgement, completed on the effect lane.
	/// \details Only explicit queued/refused decisions acknowledge delivery. Dropping
	/// an unanswered request is indeterminate: it never promises the gesture was refused.
	class profile_activation_reply
	{
	public:
		virtual ~profile_activation_reply()                                 = default;
		virtual void complete(profile_activation_outcome aOutcome) noexcept = 0;
	};
	/// \brief One authenticated, profile-bound launch gesture.
	struct profile_activation_request
	{
		core::profile_launch_intent               Intent;
		std::shared_ptr<profile_activation_reply> Reply;
	};
	/// \brief Bounded effect-lane polling; retain through profile shutdown.
	/// \details Serialize receive and destruction. At most one delivered request
	/// remains pending until its reply is completed or released.
	class profile_activation_server
	{
	public:
		virtual ~profile_activation_server()                                              = default;
		[[nodiscard]] virtual auto receive() -> std::optional<profile_activation_request> = 0;
	};
}
#endif
