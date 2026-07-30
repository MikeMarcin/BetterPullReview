#pragma once

#include "bpr/core/github_model.hpp"

namespace bpr::core
{
[[nodiscard]] auto github_commands(const github_state& state)
    -> std::vector<github_command_projection>;

[[nodiscard]] auto transition(
    github_state& state,
    const github_event& event) -> github_transition_output;
}
