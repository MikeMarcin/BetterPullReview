#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_RELEASE_UPDATE_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_RELEASE_UPDATE_HPP

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/release_update_port.hpp"

namespace bpr::adapters::github::detail {
	/// \brief Revocable release-only native lease; no general provider operations.
	class native_release_update_access : public application::release_update_access_port
	{
	public:
		virtual void revoke() noexcept = 0;
	};
	[[nodiscard]] auto make_native_release_update_access(application::github_credential_retrieve_success aCredential,
														 core::release_update_access_id aIdentity, std::shared_ptr<application::release_update_archive_store> aStore)
		-> std::shared_ptr<native_release_update_access>;
	/// \brief Credential-free lease on the same pinned public release routes.
	[[nodiscard]] auto make_public_release_update_access(core::release_update_access_id aIdentity, std::shared_ptr<application::release_update_archive_store> aStore)
		-> std::shared_ptr<native_release_update_access>;
	[[nodiscard]] auto parse_release_update_metadata(std::string_view aBody) -> application::release_update_check_result;
	/// \brief Builds a browser address only from the fixed repository and validated tag.
	[[nodiscard]] auto release_update_browser_address(std::string_view aTag) -> std::string;

	struct release_update_wire_result
	{
		core::release_update_reason Failure           = core::release_update_reason::none;
		std::uint64_t               Bytes             = 0;
		std::uint64_t               RetryAfterSeconds = 0;
	};
	using release_update_consumer = std::function<bool(std::span<const std::byte>)>;
	/// \brief Runs only on an effect worker; native HTTPS alone owns network objects.
	[[nodiscard]] auto perform_release_update_https(std::span<const std::byte> aCredential,
													std::uint64_t aAsset, std::uint64_t aLimit, std::stop_token aStop,
													const release_update_consumer& aConsume) -> release_update_wire_result;

#if defined(BPR_NATIVE_HTTPS_TESTING)
	struct native_https_test_configuration;
	[[nodiscard]] auto perform_release_update_https_for_test(std::span<const std::byte> aCredential,
															 std::uint64_t aAsset, std::uint64_t aLimit, std::stop_token aStop,
															 const release_update_consumer& aConsume, const native_https_test_configuration& aConfiguration) -> release_update_wire_result;
#endif
}
#endif
