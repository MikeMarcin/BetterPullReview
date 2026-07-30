#pragma once

#include "github_capability_probe.hpp"
#include "github_read_contract.hpp"

#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/core/github_model.hpp"

#include <cstdint>
#include <stop_token>
#include <variant>

namespace bpr::adapters::github::detail
{
using github_wire_parent_context =
    std::variant<core::github_session_request_context,
                 core::pull_request_request_context>;

struct github_wire_correlation
{
    github_wire_parent_context Parent;
    application::github_profile_revision ProfileRevision;
    core::git_host_id Host;
    read_operation Operation = read_operation::get_viewer_identity;
    std::uint32_t Page = 1U;

    auto operator==(const github_wire_correlation&) const -> bool = default;
};

struct github_wire_request
{
    github_wire_correlation Correlation;
    wire_request Wire;

    auto operator==(const github_wire_request&) const -> bool = default;
};

struct github_wire_success
{
    github_wire_correlation Correlation;
    wire_response Response;

    auto operator==(const github_wire_success&) const -> bool = default;
};

struct github_wire_error
{
    github_wire_correlation Correlation;
    application::github_provider_error Error;

    auto operator==(const github_wire_error&) const -> bool = default;
};

struct github_wire_cancelled
{
    github_wire_correlation Correlation;

    auto operator==(const github_wire_cancelled&) const -> bool = default;
};

using github_wire_completion =
    std::variant<github_wire_success, github_wire_error, github_wire_cancelled>;

struct github_capability_probe_wire_correlation
{
    core::github_session_request_context Session;
    application::github_profile_revision ProfileRevision;
    core::git_host_id Host;
    core::github_account_id ObservedAccount;

    auto operator==(const github_capability_probe_wire_correlation&) const
        -> bool = default;
};

struct github_capability_probe_wire_request
{
    github_capability_probe_wire_correlation Correlation;
    capability_probe_request Wire;

    auto operator==(const github_capability_probe_wire_request&) const
        -> bool = default;
};

struct github_capability_probe_wire_success
{
    github_capability_probe_wire_correlation Correlation;
    capability_probe_response Response;

    auto operator==(const github_capability_probe_wire_success&) const
        -> bool = default;
};

struct github_capability_probe_wire_error
{
    github_capability_probe_wire_correlation Correlation;
    application::github_provider_error Error{
        .Failure = application::github_provider_failure::unavailable,
        .RetryAfterSeconds = std::nullopt,
        .RateLimits = {},
        .RateLimitCause = std::nullopt,
    };

    auto operator==(const github_capability_probe_wire_error&) const
        -> bool = default;
};

struct github_capability_probe_wire_cancelled
{
    github_capability_probe_wire_correlation Correlation;

    auto operator==(const github_capability_probe_wire_cancelled&) const
        -> bool = default;
};

using github_capability_probe_wire_completion =
    std::variant<github_capability_probe_wire_success,
                 github_capability_probe_wire_error,
                 github_capability_probe_wire_cancelled>;

class github_wire_transport_port
{
public:
    github_wire_transport_port() = default;
    github_wire_transport_port(const github_wire_transport_port&) = delete;
    auto operator=(const github_wire_transport_port&)
        -> github_wire_transport_port& = delete;
    github_wire_transport_port(github_wire_transport_port&&) = delete;
    auto operator=(github_wire_transport_port&&)
        -> github_wire_transport_port& = delete;
    virtual ~github_wire_transport_port() = default;

    [[nodiscard]] virtual auto perform(const github_wire_request& request,
                                       std::stop_token stop)
        -> github_wire_completion = 0;

    [[nodiscard]] virtual auto perform_capability_probe(
        const github_capability_probe_wire_request& request,
        std::stop_token stop) -> github_capability_probe_wire_completion
    {
        if(stop.stop_requested())
        {
            return github_capability_probe_wire_cancelled{
                .Correlation = request.Correlation,
            };
        }
        return github_capability_probe_wire_error{
            .Correlation = request.Correlation,
            .Error =
                {
                    .Failure =
                        application::github_provider_failure::unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    }
};
} // namespace bpr::adapters::github::detail
