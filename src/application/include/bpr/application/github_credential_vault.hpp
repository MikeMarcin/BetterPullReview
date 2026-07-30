#pragma once

#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <variant>

namespace bpr::application
{
inline constexpr std::size_t maximum_github_credential_bytes = 2'560U;

enum class github_credential_material_error : std::uint8_t
{
    none,
    empty,
    too_large,
    allocation_failure,
};

[[nodiscard]] constexpr auto valid(
    const github_credential_material_error error) noexcept -> bool
{
    switch(error)
    {
    case github_credential_material_error::empty:
    case github_credential_material_error::too_large:
    case github_credential_material_error::allocation_failure:
        return true;
    case github_credential_material_error::none:
        return false;
    }
    return false;
}

class github_credential_material final
{
public:
    github_credential_material() = delete;
    ~github_credential_material() noexcept;

    github_credential_material(const github_credential_material&) = delete;
    auto operator=(const github_credential_material&)
        -> github_credential_material& = delete;
    github_credential_material(github_credential_material&& other) noexcept;
    auto operator=(github_credential_material&& other) noexcept
        -> github_credential_material&;

    [[nodiscard]] static auto create(
        std::span<const std::byte> bytes) noexcept
        -> std::expected<
            github_credential_material,
            github_credential_material_error>;

    [[nodiscard]] auto bytes() const noexcept
        -> std::span<const std::byte>;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    void reset() noexcept;

private:
    github_credential_material(
        std::unique_ptr<std::byte[]> bytes,
        std::size_t size) noexcept;

    std::unique_ptr<std::byte[]> Bytes;
    std::size_t Size = 0U;
};

[[nodiscard]] auto valid(
    const github_credential_material& material) noexcept -> bool;

struct github_credential_authority_key
{
    core::host_profile_id Profile;
    github_profile_revision ProfileRevision;
    core::git_host_id Host;
    core::github_account_id Account;
    github_server_contract ServerContract =
        github_server_contract::github_com_2022_11_28;

    auto operator<=>(const github_credential_authority_key&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const github_credential_authority_key& authority) noexcept -> bool
{
    return authority.Profile.value() != 0U
        && authority.ProfileRevision.valid()
        && authority.Host.value() != 0U
        && authority.Account.value() != 0U
        && valid(authority.ServerContract);
}

struct github_credential_operation_context
{
    core::request_id Request;
    core::application_session_id ApplicationSession;
    github_credential_authority_key Authority;
    core::github_identity_generation IdentityGeneration;

    auto operator<=>(const github_credential_operation_context&) const =
        default;
};

[[nodiscard]] constexpr auto valid(
    const github_credential_operation_context& context) noexcept -> bool
{
    return context.Request.value() != 0U
        && context.ApplicationSession.value() != 0U
        && valid(context.Authority)
        && context.IdentityGeneration.value() != 0U;
}

struct github_credential_enrollment
{
    github_credential_operation_context Context;
    github_credential_material Credential;
};

[[nodiscard]] auto valid(
    const github_credential_enrollment& enrollment) noexcept -> bool;

enum class github_credential_vault_failure : std::uint8_t
{
    none,
    invalid_request,
    unavailable,
    locked,
    access_denied,
    corrupt,
    operation_failed,
    allocation_failure,
};

[[nodiscard]] constexpr auto valid(
    const github_credential_vault_failure failure) noexcept -> bool
{
    switch(failure)
    {
    case github_credential_vault_failure::invalid_request:
    case github_credential_vault_failure::unavailable:
    case github_credential_vault_failure::locked:
    case github_credential_vault_failure::access_denied:
    case github_credential_vault_failure::corrupt:
    case github_credential_vault_failure::operation_failed:
    case github_credential_vault_failure::allocation_failure:
        return true;
    case github_credential_vault_failure::none:
        return false;
    }
    return false;
}

enum class github_credential_store_disposition : std::uint8_t
{
    uninitialized,
    stored,
};

[[nodiscard]] constexpr auto valid(
    const github_credential_store_disposition disposition) noexcept -> bool
{
    return disposition == github_credential_store_disposition::stored;
}

struct github_credential_store_success
{
    github_credential_operation_context Context;
    github_credential_store_disposition Disposition =
        github_credential_store_disposition::uninitialized;
};

struct github_credential_store_error
{
    github_credential_operation_context Context;
    github_credential_vault_failure Failure =
        github_credential_vault_failure::none;
};

struct github_credential_store_cancelled
{
    github_credential_operation_context Context;
};

using github_credential_store_completion = std::variant<
    github_credential_store_success,
    github_credential_store_error,
    github_credential_store_cancelled>;

[[nodiscard]] auto valid(
    const github_credential_store_success& success) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_store_error& error) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_store_cancelled& cancelled) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_store_completion& completion) noexcept -> bool;

struct github_credential_retrieve_success
{
    github_credential_operation_context Context;
    github_credential_material Credential;
};

struct github_credential_retrieve_absent
{
    github_credential_operation_context Context;
};

struct github_credential_retrieve_error
{
    github_credential_operation_context Context;
    github_credential_vault_failure Failure =
        github_credential_vault_failure::none;
};

struct github_credential_retrieve_cancelled
{
    github_credential_operation_context Context;
};

using github_credential_retrieve_completion = std::variant<
    github_credential_retrieve_success,
    github_credential_retrieve_absent,
    github_credential_retrieve_error,
    github_credential_retrieve_cancelled>;

[[nodiscard]] auto valid(
    const github_credential_retrieve_success& success) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_retrieve_absent& absent) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_retrieve_error& error) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_retrieve_cancelled& cancelled) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_retrieve_completion& completion) noexcept
    -> bool;

enum class github_credential_delete_disposition : std::uint8_t
{
    uninitialized,
    deleted,
    absent,
};

[[nodiscard]] constexpr auto valid(
    const github_credential_delete_disposition disposition) noexcept -> bool
{
    return disposition == github_credential_delete_disposition::deleted
        || disposition == github_credential_delete_disposition::absent;
}

struct github_credential_delete_success
{
    github_credential_operation_context Context;
    github_credential_delete_disposition Disposition =
        github_credential_delete_disposition::uninitialized;
};

struct github_credential_delete_error
{
    github_credential_operation_context Context;
    github_credential_vault_failure Failure =
        github_credential_vault_failure::none;
};

struct github_credential_delete_cancelled
{
    github_credential_operation_context Context;
};

using github_credential_delete_completion = std::variant<
    github_credential_delete_success,
    github_credential_delete_error,
    github_credential_delete_cancelled>;

[[nodiscard]] auto valid(
    const github_credential_delete_success& success) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_delete_error& error) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_delete_cancelled& cancelled) noexcept -> bool;
[[nodiscard]] auto valid(
    const github_credential_delete_completion& completion) noexcept -> bool;

using github_credential_store_completion_handler =
    std::function<void(github_credential_store_completion)>;
using github_credential_retrieve_completion_handler =
    std::function<void(github_credential_retrieve_completion)>;
using github_credential_delete_completion_handler =
    std::function<void(github_credential_delete_completion)>;

// Vault submissions and cancellation return without waiting for a worker.
// A submission that returns normally has retained the operation and invokes
// its supplied completion exactly once; a submission that throws has retained
// nothing and never invokes it. Cancellation is advisory and does not suppress
// that one terminal. The application host serializes the terminal later on its
// decision executor.
class github_credential_vault_port
{
public:
    github_credential_vault_port() = default;
    virtual ~github_credential_vault_port() = default;

    github_credential_vault_port(const github_credential_vault_port&) = delete;
    auto operator=(const github_credential_vault_port&)
        -> github_credential_vault_port& = delete;
    github_credential_vault_port(github_credential_vault_port&&) = delete;
    auto operator=(github_credential_vault_port&&)
        -> github_credential_vault_port& = delete;

    virtual void store(
        github_credential_enrollment enrollment,
        github_credential_store_completion_handler completion) = 0;
    virtual void retrieve(
        github_credential_operation_context context,
        github_credential_retrieve_completion_handler completion) = 0;
    virtual void delete_credential(
        github_credential_operation_context context,
        github_credential_delete_completion_handler completion) = 0;
    virtual void cancel(
        github_credential_operation_context context) = 0;
};
}
