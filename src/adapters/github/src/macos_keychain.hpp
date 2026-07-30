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
inline constexpr std::size_t macos_keychain_authority_account_bytes =
    (4U * 16U) + 2U + 2U
    + (2U * application::maximum_github_hostname_bytes);

using macos_keychain_authority_account =
    std::array<char, macos_keychain_authority_account_bytes>;

enum class macos_keychain_service_scope : std::uint8_t
{
    production,
    integration_test,
};

[[nodiscard]] constexpr auto valid(
    const macos_keychain_service_scope scope) noexcept -> bool
{
    return scope == macos_keychain_service_scope::production
        || scope == macos_keychain_service_scope::integration_test;
}

[[nodiscard]] auto macos_keychain_service_identifier(
    macos_keychain_service_scope scope) noexcept -> std::string_view;

[[nodiscard]] auto encode_macos_keychain_authority_account(
    const application::github_credential_authority_key& authority) noexcept
    -> std::expected<
        macos_keychain_authority_account,
        application::github_credential_vault_failure>;

enum class macos_keychain_status_kind : std::uint8_t
{
    success,
    item_not_found,
    duplicate_item,
    failure,
};

struct macos_keychain_status_mapping
{
    macos_keychain_status_kind Kind =
        macos_keychain_status_kind::failure;
    application::github_credential_vault_failure Failure =
        application::github_credential_vault_failure::operation_failed;
};

[[nodiscard]] auto classify_macos_keychain_status(
    std::int32_t status) noexcept -> macos_keychain_status_mapping;

struct macos_keychain_credential_absent
{
};

using macos_keychain_backend_store_result = std::expected<
    application::github_credential_store_disposition,
    application::github_credential_vault_failure>;
using macos_keychain_backend_retrieve_result = std::variant<
    application::github_credential_material,
    macos_keychain_credential_absent,
    application::github_credential_vault_failure>;
using macos_keychain_backend_delete_result = std::expected<
    application::github_credential_delete_disposition,
    application::github_credential_vault_failure>;

class macos_keychain_backend
{
public:
    macos_keychain_backend() = default;
    virtual ~macos_keychain_backend() = default;

    macos_keychain_backend(const macos_keychain_backend&) = delete;
    auto operator=(const macos_keychain_backend&)
        -> macos_keychain_backend& = delete;
    macos_keychain_backend(macos_keychain_backend&&) = delete;
    auto operator=(macos_keychain_backend&&)
        -> macos_keychain_backend& = delete;

    [[nodiscard]] virtual auto store(
        const application::github_credential_authority_key& authority,
        const application::github_credential_material& credential)
        -> macos_keychain_backend_store_result = 0;
    [[nodiscard]] virtual auto retrieve(
        const application::github_credential_authority_key& authority)
        -> macos_keychain_backend_retrieve_result = 0;
    [[nodiscard]] virtual auto delete_credential(
        const application::github_credential_authority_key& authority)
        -> macos_keychain_backend_delete_result = 0;
};

using macos_keychain_vault_factory_result = std::expected<
    std::unique_ptr<application::github_credential_vault_port>,
    application::github_credential_vault_failure>;

[[nodiscard]] auto make_macos_keychain_vault(
    application::effect_executor& effect_lane)
    -> macos_keychain_vault_factory_result;

[[nodiscard]] auto make_macos_keychain_integration_test_vault(
    application::effect_executor& effect_lane)
    -> macos_keychain_vault_factory_result;

[[nodiscard]] auto make_macos_keychain_vault_with_backend(
    application::effect_executor& effect_lane,
    std::shared_ptr<macos_keychain_backend> backend) noexcept
    -> macos_keychain_vault_factory_result;
}
