#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CREDENTIAL_VAULT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CREDENTIAL_VAULT_HPP

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

namespace bpr::application {
	inline constexpr std::size_t maximum_github_credential_bytes = 2'560U;

	enum class github_credential_material_error : std::uint8_t {
		none,
		empty,
		too_large,
		allocation_failure,
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_credential_material_error aError) noexcept -> bool
	{
		switch (aError) {
		case github_credential_material_error::empty:
		case github_credential_material_error::too_large:
		case github_credential_material_error::allocation_failure:
			return true;
		case github_credential_material_error::none:
			return false;
		}
		return false;
	}

	/// \brief Provides the github credential material contract.
	class github_credential_material final
	{
	public:
		github_credential_material() = delete;
		~github_credential_material() noexcept;

		github_credential_material(const github_credential_material&) = delete;
		auto operator=(const github_credential_material&)
			-> github_credential_material& = delete;
		github_credential_material(github_credential_material&& aOther) noexcept;
		auto operator=(github_credential_material&& aOther) noexcept
			-> github_credential_material&;

		[[nodiscard]] static auto create(
			std::span<const std::byte> aBytes) noexcept
			-> std::expected<
				github_credential_material,
				github_credential_material_error>;

		[[nodiscard]] auto bytes() const noexcept
			-> std::span<const std::byte>;
		[[nodiscard]] auto size() const noexcept -> std::size_t;
		[[nodiscard]] auto empty() const noexcept -> bool;
		void               reset() noexcept;

	private:
		github_credential_material(
			std::unique_ptr<std::byte[]> aBytes,
			std::size_t                  aSize) noexcept;

		std::unique_ptr<std::byte[]> Bytes;
		std::size_t                  Size = 0U;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_material& aMaterial) noexcept -> bool;

	/// \brief Describes the github credential authority key record.
	struct github_credential_authority_key
	{
		core::host_profile_id     Profile;
		github_profile_revision   ProfileRevision;
		core::git_host_id         Host;
		github_canonical_hostname Hostname;
		core::github_account_id   Account;
		github_server_contract    ServerContract = github_server_contract::github_com_2022_11_28;

		auto operator<=>(const github_credential_authority_key&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_credential_authority_key& aAuthority) noexcept -> bool
	{
		return aAuthority.Profile.value() != 0U
			&& aAuthority.ProfileRevision.valid()
			&& aAuthority.Host.value() != 0U
			&& aAuthority.Account.value() != 0U
			&& valid_github_authority_hostname(
				   aAuthority.ServerContract, aAuthority.Hostname);
	}

	/// \brief Describes the github credential operation context record.
	struct github_credential_operation_context
	{
		core::request_id                 Request;
		core::application_session_id     ApplicationSession;
		github_credential_authority_key  Authority;
		core::github_identity_generation IdentityGeneration;

		auto operator<=>(const github_credential_operation_context&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_credential_operation_context& aContext) noexcept -> bool
	{
		return aContext.Request.value() != 0U
			&& aContext.ApplicationSession.value() != 0U
			&& valid(aContext.Authority)
			&& aContext.IdentityGeneration.value() != 0U;
	}

	/// \brief Describes the github credential enrollment record.
	struct github_credential_enrollment
	{
		github_credential_operation_context Context;
		github_credential_material          Credential;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_enrollment& aEnrollment) noexcept -> bool;

	enum class github_credential_vault_failure : std::uint8_t {
		none,
		invalid_request,
		unavailable,
		locked,
		access_denied,
		corrupt,
		operation_failed,
		allocation_failure,
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_credential_vault_failure aFailure) noexcept -> bool
	{
		switch (aFailure) {
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

	enum class github_credential_store_disposition : std::uint8_t {
		uninitialized,
		stored,
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_credential_store_disposition aDisposition) noexcept -> bool
	{
		return aDisposition == github_credential_store_disposition::stored;
	}

	/// \brief Describes the github credential store success record.
	struct github_credential_store_success
	{
		github_credential_operation_context Context;
		github_credential_store_disposition Disposition = github_credential_store_disposition::uninitialized;
	};

	/// \brief Describes the github credential store error record.
	struct github_credential_store_error
	{
		github_credential_operation_context Context;
		github_credential_vault_failure     Failure = github_credential_vault_failure::none;
	};

	/// \brief Describes the github credential store cancelled record.
	struct github_credential_store_cancelled
	{
		github_credential_operation_context Context;
	};

	using github_credential_store_completion = std::variant<
		github_credential_store_success,
		github_credential_store_error,
		github_credential_store_cancelled>;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_store_success& aSuccess) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_store_error& aError) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_store_cancelled& aCancelled) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_store_completion& aCompletion) noexcept -> bool;

	/// \brief Describes the github credential retrieve success record.
	struct github_credential_retrieve_success
	{
		github_credential_operation_context Context;
		github_credential_material          Credential;
	};

	/// \brief Describes the github credential retrieve absent record.
	struct github_credential_retrieve_absent
	{
		github_credential_operation_context Context;
	};

	/// \brief Describes the github credential retrieve error record.
	struct github_credential_retrieve_error
	{
		github_credential_operation_context Context;
		github_credential_vault_failure     Failure = github_credential_vault_failure::none;
	};

	/// \brief Describes the github credential retrieve cancelled record.
	struct github_credential_retrieve_cancelled
	{
		github_credential_operation_context Context;
	};

	using github_credential_retrieve_completion = std::variant<
		github_credential_retrieve_success,
		github_credential_retrieve_absent,
		github_credential_retrieve_error,
		github_credential_retrieve_cancelled>;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_retrieve_success& aSuccess) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_retrieve_absent& aAbsent) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_retrieve_error& aError) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_retrieve_cancelled& aCancelled) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_retrieve_completion& aCompletion) noexcept
		-> bool;

	enum class github_credential_delete_disposition : std::uint8_t {
		uninitialized,
		deleted,
		absent,
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_credential_delete_disposition aDisposition) noexcept -> bool
	{
		return aDisposition == github_credential_delete_disposition::deleted
			|| aDisposition == github_credential_delete_disposition::absent;
	}

	/// \brief Describes the github credential delete success record.
	struct github_credential_delete_success
	{
		github_credential_operation_context  Context;
		github_credential_delete_disposition Disposition = github_credential_delete_disposition::uninitialized;
	};

	/// \brief Describes the github credential delete error record.
	struct github_credential_delete_error
	{
		github_credential_operation_context Context;
		github_credential_vault_failure     Failure = github_credential_vault_failure::none;
	};

	/// \brief Describes the github credential delete cancelled record.
	struct github_credential_delete_cancelled
	{
		github_credential_operation_context Context;
	};

	using github_credential_delete_completion = std::variant<
		github_credential_delete_success,
		github_credential_delete_error,
		github_credential_delete_cancelled>;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_delete_success& aSuccess) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_delete_error& aError) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_delete_cancelled& aCancelled) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_credential_delete_completion& aCompletion) noexcept -> bool;

	using github_credential_store_completion_handler    = std::function<void(github_credential_store_completion)>;
	using github_credential_retrieve_completion_handler = std::function<void(github_credential_retrieve_completion)>;
	using github_credential_delete_completion_handler   = std::function<void(github_credential_delete_completion)>;

	// Vault submissions and cancellation return without waiting for a worker.
	// A submission that returns normally has retained the operation and invokes
	// its supplied completion exactly once; a submission that throws has retained
	// nothing and never invokes it. Cancellation is advisory and does not suppress
	// that one terminal. The application host serializes the terminal later on its
	// decision executor.
	//
	// The refresh slot holds the material that renews an account credential
	// without the reviewer authorizing again. It is keyed by the same authority
	// as the credential itself and kept beside it rather than in place of it, so
	// neither can be retrieved under the other's name and deleting one never
	// silently removes the other. `cancel` covers both slots because an
	// operation is identified by its context. An implementation whose platform
	// store has no second slot reports the typed `unavailable` failure.
	//
	// It is a port of its own because the device-authorization host needs
	// exactly this much of the vault and no more. Holding the whole vault would
	// leave the base-token slot reachable from a host that has no business
	// naming it; holding this one makes that a compile error rather than a rule
	// somebody has to keep. The full vault below satisfies it, so composition
	// hands one platform object to both.
	/// \brief Provides the github credential refresh vault port contract.
	class github_credential_refresh_vault_port
	{
	public:
		github_credential_refresh_vault_port()          = default;
		virtual ~github_credential_refresh_vault_port() = default;

		github_credential_refresh_vault_port(
			const github_credential_refresh_vault_port&) = delete;
		auto operator=(const github_credential_refresh_vault_port&)
			-> github_credential_refresh_vault_port& = delete;
		github_credential_refresh_vault_port(
			github_credential_refresh_vault_port&&) = delete;
		auto operator=(github_credential_refresh_vault_port&&)
			-> github_credential_refresh_vault_port& = delete;

		virtual void store_refresh(
			github_credential_enrollment               aEnrollment,
			github_credential_store_completion_handler aCompletion) = 0;
		virtual void retrieve_refresh(
			github_credential_operation_context           aContext,
			github_credential_retrieve_completion_handler aCompletion) = 0;
		virtual void delete_refresh(
			github_credential_operation_context         aContext,
			github_credential_delete_completion_handler aCompletion) = 0;
		virtual void cancel(
			github_credential_operation_context aContext) = 0;
	};

	// The account credential itself, on the same submission and cancellation
	// contract as the refresh slot it extends.
	/// \brief Provides the github credential vault port contract.
	class github_credential_vault_port
		: public github_credential_refresh_vault_port
	{
	public:
		virtual void store(
			github_credential_enrollment               aEnrollment,
			github_credential_store_completion_handler aCompletion) = 0;
		virtual void retrieve(
			github_credential_operation_context           aContext,
			github_credential_retrieve_completion_handler aCompletion) = 0;
		virtual void delete_credential(
			github_credential_operation_context         aContext,
			github_credential_delete_completion_handler aCompletion) = 0;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CREDENTIAL_VAULT_HPP
