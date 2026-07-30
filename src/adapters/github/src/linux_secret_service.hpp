#pragma once

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string_view>
#include <variant>

namespace bpr::adapters::github
{
inline constexpr std::size_t linux_secret_service_authority_account_bytes = 66U;

using linux_secret_service_authority_account =
    std::array<char, linux_secret_service_authority_account_bytes>;

enum class linux_secret_service_schema_scope : std::uint8_t
{
    production,
    integration_test,
};

[[nodiscard]] constexpr auto valid(
    const linux_secret_service_schema_scope scope) noexcept -> bool
{
    return scope == linux_secret_service_schema_scope::production
        || scope == linux_secret_service_schema_scope::integration_test;
}

[[nodiscard]] auto linux_secret_service_schema_identifier(
    linux_secret_service_schema_scope scope) noexcept -> std::string_view;

[[nodiscard]] auto encode_linux_secret_service_authority_account(
    const application::github_credential_authority_key& authority) noexcept
    -> std::expected<linux_secret_service_authority_account,
        application::github_credential_vault_failure>;

enum class linux_secret_service_status_domain : std::uint8_t
{
    none,
    secret_service,
    dbus,
    io,
    other,
};

struct linux_secret_service_status
{
    linux_secret_service_status_domain Domain =
        linux_secret_service_status_domain::none;
    std::int32_t Code = 0;
};

[[nodiscard]] auto classify_linux_secret_service_status(
    linux_secret_service_status status) noexcept
    -> application::github_credential_vault_failure;

struct linux_secret_service_credential_absent
{
};

using linux_secret_service_backend_store_result =
    std::expected<application::github_credential_store_disposition,
        application::github_credential_vault_failure>;
using linux_secret_service_backend_retrieve_result =
    std::variant<application::github_credential_material,
        linux_secret_service_credential_absent,
        application::github_credential_vault_failure>;
using linux_secret_service_backend_delete_result =
    std::expected<application::github_credential_delete_disposition,
        application::github_credential_vault_failure>;

class linux_secret_service_backend
{
public:
    linux_secret_service_backend() = default;
    virtual ~linux_secret_service_backend() = default;

    linux_secret_service_backend(const linux_secret_service_backend&) = delete;
    auto operator=(const linux_secret_service_backend&)
        -> linux_secret_service_backend& = delete;
    linux_secret_service_backend(linux_secret_service_backend&&) = delete;
    auto operator=(linux_secret_service_backend&&)
        -> linux_secret_service_backend& = delete;

    [[nodiscard]] virtual auto store(
        const application::github_credential_authority_key& authority,
        const application::github_credential_material& credential)
        -> linux_secret_service_backend_store_result = 0;
    [[nodiscard]] virtual auto retrieve(
        const application::github_credential_authority_key& authority)
        -> linux_secret_service_backend_retrieve_result = 0;
    [[nodiscard]] virtual auto delete_credential(
        const application::github_credential_authority_key& authority)
        -> linux_secret_service_backend_delete_result = 0;
};

using linux_secret_service_vault_factory_result =
    std::expected<std::unique_ptr<application::github_credential_vault_port>,
        application::github_credential_vault_failure>;

[[nodiscard]] auto make_linux_secret_service_vault(
    application::effect_executor& effect_lane)
    -> linux_secret_service_vault_factory_result;

[[nodiscard]] auto make_linux_secret_service_integration_test_vault(
    application::effect_executor& effect_lane)
    -> linux_secret_service_vault_factory_result;

[[nodiscard]] auto make_linux_secret_service_vault_with_backend(
    application::effect_executor& effect_lane,
    std::shared_ptr<linux_secret_service_backend> backend) noexcept
    -> linux_secret_service_vault_factory_result;
}
