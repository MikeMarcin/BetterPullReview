#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_WINDOWS_CREDENTIAL_MANAGER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_WINDOWS_CREDENTIAL_MANAGER_HPP

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string_view>
#include <variant>

namespace bpr::adapters::github {
	inline constexpr std::size_t
		windows_credential_manager_authority_account_chars = (4U * 16U) + 2U + 2U
		+ (2U * application::maximum_github_hostname_bytes);

	using windows_credential_manager_authority_account = std::array<
		wchar_t,
		windows_credential_manager_authority_account_chars>;

	// The refresh scopes name a second Credential Manager target under the same
	// authority account, so one slot can never answer for the other (BPR-0543).
	enum class windows_credential_manager_target_scope : std::uint8_t {
		production,
		integration_test,
		production_refresh,
		integration_test_refresh,
	};

	[[nodiscard]] constexpr auto valid(
		const windows_credential_manager_target_scope scope) noexcept -> bool
	{
		return scope == windows_credential_manager_target_scope::production
			|| scope
			== windows_credential_manager_target_scope::integration_test
			|| scope
			== windows_credential_manager_target_scope::production_refresh
			|| scope
			== windows_credential_manager_target_scope::
				integration_test_refresh;
	}

	[[nodiscard]] auto windows_credential_manager_target_prefix(
		windows_credential_manager_target_scope scope) noexcept
		-> std::wstring_view;

	[[nodiscard]] auto encode_windows_credential_manager_authority_account(
		const application::github_credential_authority_key& authority) noexcept
		-> std::expected<
			windows_credential_manager_authority_account,
			application::github_credential_vault_failure>;

	enum class windows_credential_manager_status_kind : std::uint8_t {
		success,
		item_not_found,
		failure,
	};

	struct windows_credential_manager_status_mapping
	{
		windows_credential_manager_status_kind       Kind    = windows_credential_manager_status_kind::failure;
		application::github_credential_vault_failure Failure = application::github_credential_vault_failure::operation_failed;
	};

	[[nodiscard]] auto classify_windows_credential_manager_status(
		std::uint32_t status) noexcept
		-> windows_credential_manager_status_mapping;

	struct windows_credential_manager_credential_absent
	{
	};

	using windows_credential_manager_backend_store_result = std::expected<
		application::github_credential_store_disposition,
		application::github_credential_vault_failure>;
	using windows_credential_manager_backend_retrieve_result = std::variant<
		application::github_credential_material,
		windows_credential_manager_credential_absent,
		application::github_credential_vault_failure>;
	using windows_credential_manager_backend_delete_result = std::expected<
		application::github_credential_delete_disposition,
		application::github_credential_vault_failure>;

	class windows_credential_manager_backend
	{
	public:
		windows_credential_manager_backend()          = default;
		virtual ~windows_credential_manager_backend() = default;

		windows_credential_manager_backend(
			const windows_credential_manager_backend&) = delete;
		auto operator=(const windows_credential_manager_backend&)
			-> windows_credential_manager_backend& = delete;
		windows_credential_manager_backend(
			windows_credential_manager_backend&&) = delete;
		auto operator=(windows_credential_manager_backend&&)
			-> windows_credential_manager_backend& = delete;

		[[nodiscard]] virtual auto store(
			const application::github_credential_authority_key& authority,
			const application::github_credential_material&      credential)
			-> windows_credential_manager_backend_store_result = 0;
		[[nodiscard]] virtual auto retrieve(
			const application::github_credential_authority_key& authority)
			-> windows_credential_manager_backend_retrieve_result = 0;
		[[nodiscard]] virtual auto delete_credential(
			const application::github_credential_authority_key& authority)
			-> windows_credential_manager_backend_delete_result = 0;
	};

	using windows_credential_manager_vault_factory_result = std::expected<
		std::unique_ptr<application::github_credential_vault_port>,
		application::github_credential_vault_failure>;

	[[nodiscard]] auto make_windows_credential_manager_vault(
		application::effect_executor& effect_lane)
		-> windows_credential_manager_vault_factory_result;

	[[nodiscard]] auto
	make_windows_credential_manager_integration_test_vault(
		application::effect_executor& effect_lane)
		-> windows_credential_manager_vault_factory_result;

	// A vault built without a refresh backend answers every refresh-slot
	// operation with the typed unavailable failure. That is the useful default
	// for the existing contract tests, which have no refresh material.
	[[nodiscard]] auto make_windows_credential_manager_vault_with_backend(
		application::effect_executor&                       effect_lane,
		std::shared_ptr<windows_credential_manager_backend> backend,
		std::shared_ptr<windows_credential_manager_backend> aRefreshBackend
		= nullptr) noexcept -> windows_credential_manager_vault_factory_result;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_WINDOWS_CREDENTIAL_MANAGER_HPP
