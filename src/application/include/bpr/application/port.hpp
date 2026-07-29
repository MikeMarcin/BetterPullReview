#pragma once

#include "bpr/core/model.hpp"

#include <functional>

namespace bpr::application
{
using completion_callback = std::function<void(core::application_event)>;

class foundation_observation_port
{
public:
    virtual ~foundation_observation_port() = default;

    [[nodiscard]] virtual auto observe() const
        -> core::foundation_observation = 0;
};

class foundation_effect_port
{
public:
    virtual ~foundation_effect_port() = default;

    virtual void submit(
        const core::refresh_foundation_request& request,
        completion_callback completion) = 0;
    virtual void cancel(core::request_id request) = 0;
};

class decision_executor
{
public:
    virtual ~decision_executor() = default;

    virtual void post(std::function<void()> task) = 0;
};

class projection_port
{
public:
    virtual ~projection_port() = default;

    virtual void publish(const core::transition_output& output) = 0;
};
}
