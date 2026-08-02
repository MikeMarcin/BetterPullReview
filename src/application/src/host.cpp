#include "bpr/application/host.hpp"

#include <type_traits>
#include <utility>

namespace bpr::application {
	application_host::application_host(
		core::application_state      aInitialState,
		foundation_observation_port& aObservations,
		foundation_effect_port&      aFoundation,
		decision_executor&           aDecisionLane,
		projection_port&             aProjections)
		: State{ std::move(aInitialState) }
		, Observations{ aObservations }
		, Foundation{ aFoundation }
		, DecisionLane{ aDecisionLane }
		, Projections{ aProjections }
		, CompletionGate{ std::make_shared<completion_gate>() }
	{
		CompletionGate->Host.store(this, std::memory_order_release);
	}

	application_host::~application_host()
	{
		shutdown();
		CompletionGate->Host.store(nullptr, std::memory_order_release);
	}

	void application_host::dispatch(const core::application_event& aEvent)
	{
		if (Closing) {
			return;
		}

		const auto observation = Observations.observe();
		const auto output      = core::transition(State, aEvent, observation);
		Projections.publish(output);

		for (const auto& request : output.Requests) {
			execute(request);
		}
	}

	void application_host::shutdown()
	{
		if (Closing) {
			return;
		}

		Closing = true;
		if (State.PendingRefresh.has_value()) {
			Foundation.cancel(*State.PendingRefresh);
		}
	}

	auto application_host::state() const noexcept
		-> const core::application_state&
	{
		return State;
	}

	auto application_host::is_closing() const noexcept -> bool
	{
		return Closing;
	}

	void application_host::execute(const core::host_request& aRequest)
	{
		std::visit(
			[this](const auto& aTypedRequest) {
				using request_type = std::decay_t<decltype(aTypedRequest)>;
				if constexpr (
					std::is_same_v<
						request_type,
						core::refresh_foundation_request>) {
					const auto gate = std::weak_ptr<completion_gate>{
						CompletionGate
					};
					auto* const lane = &DecisionLane;
					Foundation.submit(
						aTypedRequest,
						[gate, lane](core::application_event aEvent) {
							lane->post(
								[gate, event = std::move(aEvent)]() mutable {
									const auto lockedGate = gate.lock();
									if (!lockedGate) {
										return;
									}
									auto* const host = lockedGate->Host.load(
										std::memory_order_acquire);
									if (host != nullptr) {
										host->dispatch(event);
									}
								});
						});
				}
			},
			aRequest);
	}
}
