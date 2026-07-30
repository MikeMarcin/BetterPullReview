#pragma once

#include "github_read_contract.hpp"

#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/core/github_model.hpp"

#include <cstdint>
#include <stop_token>
#include <variant>

namespace bpr::adapters::github::detail
{
using github_wire_parent_context = std::variant<
    core::github_session_request_context,
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

using github_wire_completion = std::variant<
    github_wire_success,
    github_wire_error,
    github_wire_cancelled>;

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

    [[nodiscard]] virtual auto perform(
        const github_wire_request& request,
        std::stop_token stop) -> github_wire_completion = 0;
};
}
