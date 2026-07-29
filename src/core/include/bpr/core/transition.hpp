#pragma once

#include "bpr/core/model.hpp"

namespace bpr::core
{
[[nodiscard]] auto resolve_shared_revision(
    const foundation_observation& observation) noexcept -> std::uint64_t;

[[nodiscard]] auto transition(
    application_state& state,
    const application_event& event,
    const foundation_observation& observation) -> transition_output;
}
