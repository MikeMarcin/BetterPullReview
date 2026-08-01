#pragma once

#include "bpr/core/epoch_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <vector>

namespace bpr::application
{
inline constexpr std::size_t maximum_git_base_branch_ref_bytes = 1024U;
inline constexpr std::size_t maximum_git_smart_request_bytes = 64U * 1024U;
inline constexpr std::size_t maximum_git_smart_control_response_bytes =
    64U * 1024U;
inline constexpr std::uint64_t maximum_git_smart_fetch_response_bytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL + 64ULL * 1024ULL * 1024ULL;

class git_base_branch_ref
{
public:
    constexpr git_base_branch_ref() = default;

    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view
    {
        return {Characters.data(), Length};
    }

    auto operator<=>(const git_base_branch_ref&) const = default;

private:
    friend auto parse_git_base_branch_ref(std::string_view) noexcept
        -> std::optional<git_base_branch_ref>;

    std::array<char, maximum_git_base_branch_ref_bytes> Characters{};
    std::uint16_t Length = 0U;
};

[[nodiscard]] auto parse_git_base_branch_ref(std::string_view value) noexcept
    -> std::optional<git_base_branch_ref>;
[[nodiscard]] constexpr auto valid(
    const git_base_branch_ref& ref) noexcept -> bool
{
    return !ref.view().empty();
}

struct git_smart_request_context
{
    core::application_session_id ApplicationSession;
    core::pull_request_authority Authority;
    core::accepted_pull_request_revision Revision;
    core::repository_locator Repository;
    git_base_branch_ref BaseRef;

    auto operator<=>(const git_smart_request_context&) const = default;
};

[[nodiscard]] auto valid(
    const git_smart_request_context& context) noexcept -> bool;

enum class git_smart_transport_failure : std::uint8_t
{
    invalid_request,
    unavailable,
    authentication,
    permission,
    network,
    certificate,
    proxy,
    redirect,
    timeout,
    cancelled,
    request_limit,
    response_limit,
    malformed_response,
    consumer_rejected,
};

struct git_smart_transport_error
{
    git_smart_transport_failure Failure =
        git_smart_transport_failure::unavailable;

    auto operator<=>(const git_smart_transport_error&) const = default;
};

template<typename Value>
using git_smart_transport_result =
    std::expected<Value, git_smart_transport_error>;

struct git_smart_discovery_request
{
    git_smart_request_context Context;
};

struct git_smart_discovery_response
{
    std::vector<std::byte> Body;
};

struct git_smart_ls_refs_request
{
    git_smart_request_context Context;
    std::span<const std::byte> Body;
};

struct git_smart_ls_refs_response
{
    std::vector<std::byte> Body;
};

struct git_smart_fetch_request
{
    git_smart_request_context Context;
    std::span<const std::byte> Body;
    std::uint64_t MaximumResponseBytes = 0U;
};

[[nodiscard]] auto valid(
    const git_smart_discovery_request& request) noexcept -> bool;
[[nodiscard]] auto valid(
    const git_smart_ls_refs_request& request) noexcept -> bool;
[[nodiscard]] auto valid(
    const git_smart_fetch_request& request) noexcept -> bool;

class git_smart_wire_consumer
{
public:
    virtual ~git_smart_wire_consumer() = default;

    [[nodiscard]] virtual auto consume(
        std::span<const std::byte> bytes) noexcept -> bool = 0;
};

class git_smart_transport_port
{
public:
    virtual ~git_smart_transport_port() = default;

    git_smart_transport_port() = default;
    git_smart_transport_port(const git_smart_transport_port&) = delete;
    auto operator=(const git_smart_transport_port&)
        -> git_smart_transport_port& = delete;
    git_smart_transport_port(git_smart_transport_port&&) = delete;
    auto operator=(git_smart_transport_port&&)
        -> git_smart_transport_port& = delete;

    virtual auto revoke() noexcept -> void = 0;
    [[nodiscard]] virtual auto revoked() const noexcept -> bool = 0;

    [[nodiscard]] virtual auto discover(
        const git_smart_discovery_request& request,
        std::stop_token stop)
        -> git_smart_transport_result<git_smart_discovery_response> = 0;
    [[nodiscard]] virtual auto ls_refs(
        const git_smart_ls_refs_request& request,
        std::stop_token stop)
        -> git_smart_transport_result<git_smart_ls_refs_response> = 0;
    [[nodiscard]] virtual auto fetch(
        const git_smart_fetch_request& request,
        git_smart_wire_consumer& consumer,
        std::stop_token stop) -> git_smart_transport_result<void> = 0;
};

// The authority retains only weak ownership so it can revoke every issued
// transport without forwarding protocol rounds or extending a job lifetime.
using git_smart_transport = std::shared_ptr<git_smart_transport_port>;

class git_smart_transport_factory_port
{
public:
    virtual ~git_smart_transport_factory_port() = default;

    git_smart_transport_factory_port() = default;
    git_smart_transport_factory_port(
        const git_smart_transport_factory_port&) = delete;
    auto operator=(const git_smart_transport_factory_port&)
        -> git_smart_transport_factory_port& = delete;
    git_smart_transport_factory_port(
        git_smart_transport_factory_port&&) = delete;
    auto operator=(git_smart_transport_factory_port&&)
        -> git_smart_transport_factory_port& = delete;

    [[nodiscard]] virtual auto acquire(
        const git_smart_request_context& context)
        -> git_smart_transport_result<git_smart_transport> = 0;
};
}
