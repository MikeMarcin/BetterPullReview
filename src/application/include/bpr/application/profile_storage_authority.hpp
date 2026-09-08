#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_STORAGE_AUTHORITY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_STORAGE_AUTHORITY_HPP

#include "bpr/application/profile_registry.hpp"

namespace bpr::application {
	enum class profile_storage_authority_failure : std::uint8_t { invalid_registry,
																  unknown_profile };

	/// \brief Immutable root selection admitted from an entire validated registry snapshot.
	/// \details This contains no filesystem path or label. Revision identifies the admitted
	/// observation, not a runtime lease or proof that the registry has not changed since.
	class profile_storage_authority final
	{
	public:
		/// \brief Admits one exact profile only after validating every registry entry.
		[[nodiscard]] static auto select(const profile_registry_snapshot& aRegistry, core::bpr_profile_id aProfile)
			-> std::expected<profile_storage_authority, profile_storage_authority_failure>;
		[[nodiscard]] auto profile() const noexcept -> core::bpr_profile_id;
		[[nodiscard]] auto root_mode() const noexcept -> profile_root_mode;
		[[nodiscard]] auto root_identity() const noexcept -> std::string_view;
		[[nodiscard]] auto registry_revision() const noexcept -> std::uint64_t;

	private:
		profile_storage_authority(core::bpr_profile_id aProfile, profile_root_mode aMode, std::string aIdentity, std::uint64_t aRevision);
		core::bpr_profile_id Profile;
		profile_root_mode    Mode;
		std::string          Identity;
		std::uint64_t        Revision;
	};
}
#endif
