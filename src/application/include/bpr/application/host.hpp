#pragma once

#include "bpr/application/port.hpp"
#include "bpr/core/transition.hpp"

#include <atomic>
#include <memory>

namespace bpr::application
{
class application_host final
{
public:
    application_host(
        core::application_state initial_state,
        foundation_observation_port& observations,
        foundation_effect_port& foundation,
        decision_executor& decision_lane,
        projection_port& projections);
    ~application_host();

    application_host(const application_host&) = delete;
    auto operator=(const application_host&) -> application_host& = delete;
    application_host(application_host&&) = delete;
    auto operator=(application_host&&) -> application_host& = delete;

    void dispatch(const core::application_event& event);
    void shutdown();

    [[nodiscard]] auto state() const noexcept
        -> const core::application_state&;
    [[nodiscard]] auto is_closing() const noexcept -> bool;

private:
    struct completion_gate
    {
        std::atomic<application_host*> Host = nullptr;
    };

    void execute(const core::host_request& request);

    core::application_state State;
    foundation_observation_port& Observations;
    foundation_effect_port& Foundation;
    decision_executor& DecisionLane;
    projection_port& Projections;
    std::shared_ptr<completion_gate> CompletionGate;
    bool Closing = false;
};
}
