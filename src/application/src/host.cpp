#include "bpr/application/host.hpp"

#include <type_traits>
#include <utility>

namespace bpr::application
{
application_host::application_host(
    core::application_state initial_state,
    foundation_observation_port& observations,
    foundation_effect_port& foundation,
    decision_executor& decision_lane,
    projection_port& projections)
    : State{std::move(initial_state)}
    , Observations{observations}
    , Foundation{foundation}
    , DecisionLane{decision_lane}
    , Projections{projections}
    , CompletionGate{std::make_shared<completion_gate>()}
{
    CompletionGate->Host.store(this, std::memory_order_release);
}

application_host::~application_host()
{
    shutdown();
    CompletionGate->Host.store(nullptr, std::memory_order_release);
}

void application_host::dispatch(const core::application_event& event)
{
    if(Closing)
    {
        return;
    }

    const auto observation = Observations.observe();
    const auto output = core::transition(State, event, observation);
    Projections.publish(output);

    for(const auto& request : output.Requests)
    {
        execute(request);
    }
}

void application_host::shutdown()
{
    if(Closing)
    {
        return;
    }

    Closing = true;
    if(State.PendingRefresh.has_value())
    {
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

void application_host::execute(const core::host_request& request)
{
    std::visit(
        [this](const auto& typed_request) {
            using request_type = std::decay_t<decltype(typed_request)>;
            if constexpr(
                std::is_same_v<
                    request_type,
                    core::refresh_foundation_request>)
            {
                const auto gate = std::weak_ptr<completion_gate>{
                    CompletionGate};
                auto* const lane = &DecisionLane;
                Foundation.submit(
                    typed_request,
                    [gate, lane](core::application_event event) {
                        lane->post(
                            [gate, event = std::move(event)]() mutable {
                                const auto locked_gate = gate.lock();
                                if(!locked_gate)
                                {
                                    return;
                                }
                                auto* const host = locked_gate->Host.load(
                                    std::memory_order_acquire);
                                if(host != nullptr)
                                {
                                    host->dispatch(event);
                                }
                            });
                    });
            }
        },
        request);
}
}
