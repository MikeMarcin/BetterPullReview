#pragma once

#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_read_model.hpp"

namespace bpr::application
{
struct github_host_diagnostic
{
    core::github_reason_code Reason = core::github_reason_code::none;
    std::optional<std::uint32_t> RetryAfterSeconds;
    github_rate_limit_state RateLimits;
    std::optional<github_rate_limit_record> RateLimitCause;

    auto operator<=>(const github_host_diagnostic&) const = default;
};

[[nodiscard]] auto project_github_transition(
    const core::github_transition_output& output)
    -> github_transition_projection;
[[nodiscard]] auto project_github_session(
    const github_session_read_model& session) -> github_session_projection;
[[nodiscard]] auto project_github_pull_request(
    const github_pull_request_snapshot& snapshot)
    -> github_pull_request_projection;
[[nodiscard]] auto project_github_host_diagnostic(
    const github_host_diagnostic& diagnostic)
    -> github_host_diagnostic_projection;
}
