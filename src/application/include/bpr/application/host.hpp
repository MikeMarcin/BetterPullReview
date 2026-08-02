#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/transition.hpp"

#include <atomic>
#include <memory>

namespace bpr::application {
	/// \brief Provides the application host contract.
	class application_host final
	{
	public:
		application_host(
			core::application_state      aInitialState,
			foundation_observation_port& aObservations,
			foundation_effect_port&      aFoundation,
			decision_executor&           aDecisionLane,
			projection_port&             aProjections);
		~application_host();

		application_host(const application_host&)                    = delete;
		auto operator=(const application_host&) -> application_host& = delete;
		application_host(application_host&&)                         = delete;
		auto operator=(application_host&&) -> application_host&      = delete;

		void dispatch(const core::application_event& aEvent);
		void shutdown();

		[[nodiscard]] auto state() const noexcept
			-> const core::application_state&;
		[[nodiscard]] auto is_closing() const noexcept -> bool;

	private:
		/// \brief Describes the completion gate record.
		struct completion_gate
		{
			std::atomic<application_host*> Host = nullptr;
		};

		void execute(const core::host_request& aRequest);

		core::application_state          State;
		foundation_observation_port&     Observations;
		foundation_effect_port&          Foundation;
		decision_executor&               DecisionLane;
		projection_port&                 Projections;
		std::shared_ptr<completion_gate> CompletionGate;
		bool                             Closing = false;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOST_HPP
