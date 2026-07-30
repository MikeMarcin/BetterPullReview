#pragma once

#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include <cstdint>
#include <expected>

namespace bpr::adapters::github::detail
{
enum class github_wire_request_validation_failure : std::uint8_t
{
    profile_correlation_mismatch,
    server_contract_mismatch,
    invalid_wire_request,
};

[[nodiscard]] auto validate_github_wire_request(
    const validated_github_host_profile& profile,
    const github_wire_request& request)
    -> std::expected<void, github_wire_request_validation_failure>;

[[nodiscard]] auto validate_github_capability_probe_wire_request(
    const validated_github_host_profile& profile,
    const github_capability_probe_wire_request& request)
    -> std::expected<void, github_wire_request_validation_failure>;
}
