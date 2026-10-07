#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_LINUX_SECRET_SERVICE_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_LINUX_SECRET_SERVICE_HPP

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

// This file's identifier spellings are pinned, not merely conventional.
// cmake/VerifyArchitectureBoundaries.cmake requires the Linux Secret Service
// adapter to retain an exact, binary, noninteractive credential lifecycle, and
// it enforces that by matching source text -- including parameter names such
// as `prompt`, `actual_name` and `canonical_service_bus_name`. Renaming them to
// the project's `aPrompt` convention leaves the behaviour identical and fails
// `architecture_boundaries`, because the pinned patterns no longer match.
//
// The naming findings here are therefore accepted rather than resolved.
// Anything that is not an identifier name is still checked; if a pattern in
// that file is ever relaxed, drop the matching part of this suppression rather
// than widening it.
// NOLINTBEGIN(readability-identifier-naming)
namespace bpr::adapters::github {
	inline constexpr std::size_t linux_secret_service_authority_account_bytes = (4U * 16U) + 2U + 2U
		+ (2U * application::maximum_github_hostname_bytes);

	using linux_secret_service_authority_account = std::array<char, linux_secret_service_authority_account_bytes>;

	enum class linux_secret_service_schema_scope : std::uint8_t {
		production,
		integration_test,
	};

	[[nodiscard]] constexpr auto valid(
		const linux_secret_service_schema_scope scope) noexcept -> bool
	{
		return scope == linux_secret_service_schema_scope::production
			|| scope == linux_secret_service_schema_scope::integration_test;
	}

	/// @brief Which of the vault's two stores an operation belongs to.
	///
	/// The refresh slot is a second Secret Service schema, not a second
	/// attribute on the credential's. Sharing the schema and distinguishing the
	/// slots by an attribute would leave one lookup able to answer for the
	/// other whenever an attribute were dropped or mistyped, and the failure
	/// would be a refresh token handed out as an access token. Separate schemas
	/// make that impossible rather than merely incorrect: a query carries its
	/// schema, so the credential slot cannot see a refresh item at all.
	enum class linux_secret_service_slot : std::uint8_t {
		credential,
		refresh,
	};

	[[nodiscard]] constexpr auto valid(
		const linux_secret_service_slot slot) noexcept -> bool
	{
		return slot == linux_secret_service_slot::credential
			|| slot == linux_secret_service_slot::refresh;
	}

	[[nodiscard]] auto linux_secret_service_schema_identifier(
		linux_secret_service_schema_scope scope) noexcept -> std::string_view;

	/// @brief The refresh slot's schema identifier for @p scope.
	///
	/// Deliberately a separate entry point rather than a slot parameter on the
	/// one above: that function's two answers are pinned by the contract test
	/// as the credential schemas, and a caller that wants the refresh schema
	/// should have to say so.
	[[nodiscard]] auto linux_secret_service_refresh_schema_identifier(
		linux_secret_service_schema_scope scope) noexcept -> std::string_view;

	/// \brief Resolves the immutable profile namespace; Default preserves legacy bytes.
	[[nodiscard]] auto linux_secret_service_profile_schema_identifier(
		linux_secret_service_schema_scope scope,
		linux_secret_service_slot         slot,
		core::bpr_profile_id              aBprProfile) -> std::string;

	[[nodiscard]] auto encode_linux_secret_service_authority_account(
		const application::github_credential_authority_key& authority) noexcept
		-> std::expected<linux_secret_service_authority_account,
						 application::github_credential_vault_failure>;

	enum class linux_secret_service_status_domain : std::uint8_t {
		none,
		secret_service,
		dbus,
		io,
		other,
	};

	struct linux_secret_service_status
	{
		linux_secret_service_status_domain Domain = linux_secret_service_status_domain::none;
		std::int32_t                       Code   = 0;
	};

	[[nodiscard]] auto classify_linux_secret_service_status(
		linux_secret_service_status status) noexcept
		-> application::github_credential_vault_failure;

	struct linux_secret_service_credential_absent
	{
	};

	using linux_secret_service_backend_store_result    = std::expected<application::github_credential_store_disposition,
																	   application::github_credential_vault_failure>;
	using linux_secret_service_backend_retrieve_result = std::variant<application::github_credential_material,
																	  linux_secret_service_credential_absent,
																	  application::github_credential_vault_failure>;
	using linux_secret_service_backend_delete_result   = std::expected<application::github_credential_delete_disposition,
																	   application::github_credential_vault_failure>;

	class linux_secret_service_backend
	{
	public:
		linux_secret_service_backend()          = default;
		virtual ~linux_secret_service_backend() = default;

		linux_secret_service_backend(const linux_secret_service_backend&) = delete;
		auto operator=(const linux_secret_service_backend&)
			-> linux_secret_service_backend&                         = delete;
		linux_secret_service_backend(linux_secret_service_backend&&) = delete;
		auto operator=(linux_secret_service_backend&&)
			-> linux_secret_service_backend& = delete;

		[[nodiscard]] virtual auto store(
			const application::github_credential_authority_key& authority,
			const application::github_credential_material&      credential)
			-> linux_secret_service_backend_store_result = 0;
		[[nodiscard]] virtual auto retrieve(
			const application::github_credential_authority_key& authority)
			-> linux_secret_service_backend_retrieve_result = 0;
		[[nodiscard]] virtual auto delete_credential(
			const application::github_credential_authority_key& authority)
			-> linux_secret_service_backend_delete_result = 0;
	};

	using linux_secret_service_vault_factory_result = std::expected<std::unique_ptr<application::github_credential_vault_port>,
																	application::github_credential_vault_failure>;

	/// \brief Binds access and refresh namespaces and admission to one BPR profile.
	[[nodiscard]] auto make_linux_secret_service_vault(
		application::effect_executor& effect_lane,
		core::bpr_profile_id          aBprProfile = core::default_bpr_profile_id)
		-> linux_secret_service_vault_factory_result;

	/// @brief Whether a store would reach an unlocked default collection now.
	///
	/// [[BPR-1068]]. A caller about to ask a person to authorize a sign-in asks
	/// this first, so a session with no Secret Service, or a locked default
	/// collection, is refused before anything is requested from the provider.
	/// It runs the same verified-service, session and default-collection gate
	/// as `store`, and searches, reads, writes and unlocks nothing. A later
	/// store can still fail; this answers only for the moment it is asked.
	[[nodiscard]] auto linux_secret_service_store_readiness() noexcept
		-> std::expected<void, application::github_credential_vault_failure>;

	/// \brief Uses isolated integration namespaces for the immutable BPR profile.
	[[nodiscard]] auto make_linux_secret_service_integration_test_vault(
		application::effect_executor& effect_lane,
		core::bpr_profile_id          aBprProfile = core::default_bpr_profile_id)
		-> linux_secret_service_vault_factory_result;

	/// @brief Builds a vault over injected backends.
	///
	/// @p refresh_backend may be null, and a vault built that way answers
	/// `unavailable` to every refresh operation. That is the honest answer for
	/// a composition that has no second store -- it is not the answer for a
	/// slot that exists and was never implemented, which is what this adapter
	/// used to give ([[BPR-0542]]).
	[[nodiscard]] auto make_linux_secret_service_vault_with_backend(
		application::effect_executor&                 effect_lane,
		std::shared_ptr<linux_secret_service_backend> backend,
		std::shared_ptr<linux_secret_service_backend> refresh_backend
		= nullptr,
		core::bpr_profile_id aBprProfile = core::default_bpr_profile_id) noexcept -> linux_secret_service_vault_factory_result;
}

// NOLINTEND(readability-identifier-naming)
#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_LINUX_SECRET_SERVICE_HPP
