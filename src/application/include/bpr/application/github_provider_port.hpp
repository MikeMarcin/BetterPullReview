#pragma once

#include "bpr/application/github_capability_cache.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_read_model.hpp"

#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace bpr::application
{
inline constexpr std::uint8_t maximum_github_provider_attempts = 3U;

class github_provider_attempt
{
public:
    constexpr github_provider_attempt() = default;
    explicit constexpr github_provider_attempt(
        const std::uint8_t value) noexcept
        : Value{value}
    {
    }

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint8_t
    {
        return Value;
    }

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        return Value != 0U && Value <= maximum_github_provider_attempts;
    }

    auto operator<=>(const github_provider_attempt&) const = default;

private:
    std::uint8_t Value = 0U;
};

class github_account_transport_lease
{
public:
    virtual ~github_account_transport_lease() = default;
};

using github_account_transport =
    std::shared_ptr<github_account_transport_lease>;

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
    github_rate_limit_state RateLimits;
    std::optional<github_rate_limit_record> RateLimitCause;

    auto operator<=>(const github_provider_error&) const = default;
};

inline constexpr std::uint32_t maximum_github_retry_after_seconds = 86'400U;

[[nodiscard]] constexpr auto valid(
    const github_provider_error& error) noexcept -> bool
{
    if(!valid(error.Failure))
    {
        return false;
    }
    if(!valid(error.RateLimits))
    {
        return false;
    }
    if(
        (
            error.RateLimitCause.has_value()
            && (
                error.Failure != github_provider_failure::rate_limit
                || !valid(*error.RateLimitCause)
                || !is_limited(*error.RateLimitCause)
                || find(
                       error.RateLimits,
                       error.RateLimitCause->Resource)
                    == nullptr
                || *find(
                       error.RateLimits,
                       error.RateLimitCause->Resource)
                    != *error.RateLimitCause))
        || (
            error.Failure == github_provider_failure::rate_limit
            && !error.RateLimitCause.has_value()))
    {
        return false;
    }
    if(
        is_secondary_limited(error.RateLimits)
        && (
            !error.RateLimitCause.has_value()
            || !error.RateLimitCause->SecondaryLimited
            || (
                error.RateLimits.Core.has_value()
                && error.RateLimits.Core->SecondaryLimited
                && *error.RateLimits.Core != *error.RateLimitCause)
            || (
                error.RateLimits.Search.has_value()
                && error.RateLimits.Search->SecondaryLimited
                && *error.RateLimits.Search != *error.RateLimitCause)
            || (
                error.RateLimits.Graphql.has_value()
                && error.RateLimits.Graphql->SecondaryLimited
                && *error.RateLimits.Graphql != *error.RateLimitCause)))
    {
        return false;
    }
    return !error.RetryAfterSeconds.has_value()
        || (
            *error.RetryAfterSeconds
                <= maximum_github_retry_after_seconds
            && (
                error.Failure == github_provider_failure::rate_limit
                || error.Failure
                    == github_provider_failure::unavailable));
}

struct github_profile_request
{
    core::github_session_request_context Context;
    github_provider_attempt Attempt{1U};

    auto operator<=>(const github_profile_request&) const = default;
};

struct github_profile_success
{
    github_profile_request Request;
    github_profile_authority Authority;

    auto operator<=>(const github_profile_success&) const = default;
};

struct github_profile_error
{
    github_profile_request Request;
    github_provider_error Error;

    auto operator<=>(const github_profile_error&) const = default;
};

struct github_profile_cancelled
{
    github_profile_request Request;

    auto operator<=>(const github_profile_cancelled&) const = default;
};

using github_profile_completion = std::variant<
    github_profile_success,
    github_profile_error,
    github_profile_cancelled>;
using github_profile_completion_handler =
    std::function<void(github_profile_completion)>;

struct github_viewer_request
{
    core::github_session_request_context Context;
    github_profile_authority Authority;
    github_provider_attempt Attempt{1U};

    auto operator<=>(const github_viewer_request&) const = default;
};

struct github_viewer_success
{
    github_viewer_request Request;
    github_viewer_record Viewer;
    github_rate_limit_state RateLimits;
    github_account_transport AccountTransport;

    auto operator<=>(const github_viewer_success&) const = default;
};

struct github_viewer_error
{
    github_viewer_request Request;
    github_provider_error Error;

    auto operator<=>(const github_viewer_error&) const = default;
};

struct github_viewer_cancelled
{
    github_viewer_request Request;

    auto operator<=>(const github_viewer_cancelled&) const = default;
};

using github_viewer_completion = std::variant<
    github_viewer_success,
    github_viewer_error,
    github_viewer_cancelled>;
using github_viewer_completion_handler =
    std::function<void(github_viewer_completion)>;

struct github_bootstrap_credential
{
    github_credential_material Credential;
};

using github_viewer_authentication = std::variant<
    std::monostate,
    github_bootstrap_credential,
    github_credential_retrieve_success>;

struct github_viewer_submission
{
    github_viewer_request Request;
    github_viewer_authentication Authentication;
};

struct github_capability_request
{
    core::github_session_request_context Context;
    github_profile_authority ProfileAuthority;
    github_viewer_record Viewer;
    github_account_transport AccountTransport;
    github_provider_attempt Attempt{1U};

    auto operator<=>(const github_capability_request&) const = default;
};

struct github_capability_success
{
    github_capability_request Request;
    std::array<github_capability_fact, github_capability_fact_count> Facts;
    std::array<
        github_schema_capability_fact,
        github_schema_capability_fact_count>
        SchemaFacts;
    github_rate_limit_state RateLimits;

    auto operator<=>(const github_capability_success&) const = default;
};

struct github_capability_error
{
    github_capability_request Request;
    github_provider_error Error;

    auto operator<=>(const github_capability_error&) const = default;
};

struct github_capability_cancelled
{
    github_capability_request Request;

    auto operator<=>(const github_capability_cancelled&) const = default;
};

using github_capability_completion = std::variant<
    github_capability_success,
    github_capability_error,
    github_capability_cancelled>;
using github_capability_completion_handler =
    std::function<void(github_capability_completion)>;

struct github_pull_request_provider_request
{
    core::pull_request_request_context Context;
    github_profile_authority ProfileAuthority;
    github_viewer_record Viewer;
    github_capability_cache_entry CapabilityEvidence;
    github_account_transport AccountTransport;
    github_provider_attempt Attempt{1U};

    auto operator<=>(const github_pull_request_provider_request&) const =
        default;
};

struct github_pull_request_success
{
    github_pull_request_provider_request Request;
    github_pull_request_snapshot Snapshot;

    auto operator<=>(const github_pull_request_success&) const = default;
};

struct github_pull_request_error
{
    github_pull_request_provider_request Request;
    github_provider_error Error;

    auto operator<=>(const github_pull_request_error&) const = default;
};

struct github_pull_request_cancelled
{
    github_pull_request_provider_request Request;

    auto operator<=>(const github_pull_request_cancelled&) const = default;
};

using github_pull_request_completion = std::variant<
    github_pull_request_success,
    github_pull_request_error,
    github_pull_request_cancelled>;
using github_pull_request_completion_handler =
    std::function<void(github_pull_request_completion)>;

// Submission and cancellation return without waiting for a worker. A
// submission that returns normally has retained the operation and invokes its
// supplied completion exactly once; a submission that throws has retained
// nothing and never invokes it. Cancellation is advisory and does not suppress
// that one terminal. A completion may be invoked before return, but the
// decision executor still enqueues its application turn; an implementation
// must not join or otherwise wait for that turn.
class github_profile_port
{
public:
    virtual ~github_profile_port() = default;
    virtual void resolve_profile(
        github_profile_request request,
        github_profile_completion_handler completion) = 0;
    virtual void cancel_profile(github_profile_request request) = 0;
};

class github_viewer_port
{
public:
    virtual ~github_viewer_port() = default;
    virtual void observe_viewer(
        github_viewer_submission submission,
        github_viewer_completion_handler completion) = 0;
    virtual void cancel_viewer(github_viewer_request request) = 0;
};

class github_capability_port
{
public:
    virtual ~github_capability_port() = default;
    virtual void probe_capabilities(
        github_capability_request request,
        github_capability_completion_handler completion) = 0;
    virtual void cancel_capabilities(github_capability_request request) = 0;
};

class github_pull_request_port
{
public:
    virtual ~github_pull_request_port() = default;
    virtual void fetch_pull_request(
        github_pull_request_provider_request request,
        github_pull_request_completion_handler completion) = 0;
    virtual void cancel_pull_request(
        github_pull_request_provider_request request) = 0;
};

class github_provider_port
    : public github_profile_port
    , public github_viewer_port
    , public github_capability_port
    , public github_pull_request_port
{
public:
    ~github_provider_port() override = default;
};
}
