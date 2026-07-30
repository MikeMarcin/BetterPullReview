#pragma once

#include "bpr/application/github_read_model.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <variant>

namespace bpr::application
{
enum class github_provider_failure : std::uint8_t
{
    authentication,
    permission,
    network,
    api_version,
    unsupported_capability,
    rate_limit,
    certificate,
    redirect,
    timeout,
    process_startup,
    cli_unavailable,
    vault,
    malformed_response,
    response_limit,
    unavailable,
};

[[nodiscard]] constexpr auto valid(
    const github_provider_failure failure) noexcept -> bool
{
    switch(failure)
    {
    case github_provider_failure::authentication:
    case github_provider_failure::permission:
    case github_provider_failure::network:
    case github_provider_failure::api_version:
    case github_provider_failure::unsupported_capability:
    case github_provider_failure::rate_limit:
    case github_provider_failure::certificate:
    case github_provider_failure::redirect:
    case github_provider_failure::timeout:
    case github_provider_failure::process_startup:
    case github_provider_failure::cli_unavailable:
    case github_provider_failure::vault:
    case github_provider_failure::malformed_response:
    case github_provider_failure::response_limit:
    case github_provider_failure::unavailable:
        return true;
    }
    return false;
}

struct github_provider_error
{
    github_provider_failure Failure = github_provider_failure::network;
    std::optional<std::uint32_t> RetryAfterSeconds;

    auto operator<=>(const github_provider_error&) const = default;
};

struct github_session_success
{
    core::github_session_request_context Context;
    github_session_read_model Model;

    auto operator<=>(const github_session_success&) const = default;
};

struct github_session_error
{
    core::github_session_request_context Context;
    github_provider_error Error;

    auto operator<=>(const github_session_error&) const = default;
};

struct github_session_cancelled
{
    core::github_session_request_context Context;

    auto operator<=>(const github_session_cancelled&) const = default;
};

using github_session_completion = std::variant<
    github_session_success,
    github_session_error,
    github_session_cancelled>;

struct github_pull_request_success
{
    core::pull_request_request_context Context;
    github_pull_request_snapshot Snapshot;

    auto operator<=>(const github_pull_request_success&) const = default;
};

struct github_pull_request_error
{
    core::pull_request_request_context Context;
    github_provider_error Error;

    auto operator<=>(const github_pull_request_error&) const = default;
};

struct github_pull_request_cancelled
{
    core::pull_request_request_context Context;

    auto operator<=>(const github_pull_request_cancelled&) const = default;
};

using github_pull_request_completion = std::variant<
    github_pull_request_success,
    github_pull_request_error,
    github_pull_request_cancelled>;

using github_session_completion_handler =
    std::function<void(github_session_completion)>;
using github_pull_request_completion_handler =
    std::function<void(github_pull_request_completion)>;
using github_cancel_context = std::variant<
    core::github_session_request_context,
    core::pull_request_request_context>;

class github_provider_port
{
public:
    virtual ~github_provider_port() = default;

    virtual void establish_session(
        core::github_session_request_context context,
        github_session_completion_handler completion) = 0;

    virtual void fetch_pull_request(
        core::pull_request_request_context context,
        github_pull_request_completion_handler completion) = 0;

    virtual void cancel(github_cancel_context context) = 0;
};
}
