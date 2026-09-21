#ifndef BPR_APPLICATION_RELEASE_PROFILE_CLEANUP_HPP
#define BPR_APPLICATION_RELEASE_PROFILE_CLEANUP_HPP

#include "bpr/application/device_binding_store.hpp"
#include "bpr/application/release_maintenance_application_host.hpp"

namespace bpr::application {
	/// \brief Bounded identity of resources with an existing cleanup owner outside native files.
	struct release_profile_resource_inventory
	{
		std::array<std::uint8_t, 32> Digest{ };
		std::uint32_t                Entries                                                             = 0U;
		auto                         operator==(const release_profile_resource_inventory&) const -> bool = default;
	};
	/// \brief Retained exact credential/workspace owner; never a path supplied by presentation.
	class release_profile_cleanup_port
	{
	public:
		virtual ~release_profile_cleanup_port()                                                                                            = default;
		virtual auto inspect(std::stop_token aStop) -> std::expected<release_profile_resource_inventory, core::release_maintenance_reason> = 0;
		/// \brief Idempotent cleanup of the exact inspected set, after durable purge authorization.
		virtual auto erase(const release_profile_resource_inventory& aInventory) -> release_maintenance_result = 0;
	};
	/// \brief Deletes both slots of remembered exact profile bindings through the existing vault.
	/// \details Runs on an effect lane, with a live independent vault executor. Metadata remains
	/// intact until native cleanup succeeds, so a crash can retry the same keys. No token is read.
	class release_profile_credentials final : public release_profile_cleanup_port
	{
	public:
		release_profile_credentials(core::bpr_profile_id aProfile, core::application_session_id aSession,
									std::function<std::unique_ptr<device_binding_store_port>()> aBindings, github_credential_vault_port& aVault);
		auto inspect(std::stop_token aStop) -> std::expected<release_profile_resource_inventory, core::release_maintenance_reason> override;
		auto erase(const release_profile_resource_inventory& aInventory) -> release_maintenance_result override;

	private:
		core::bpr_profile_id                                        Profile;
		core::application_session_id                                Session;
		std::function<std::unique_ptr<device_binding_store_port>()> Bindings;
		github_credential_vault_port&                               Vault;
		std::uint64_t                                               NextRequest = 1U;
	};
}
#endif
