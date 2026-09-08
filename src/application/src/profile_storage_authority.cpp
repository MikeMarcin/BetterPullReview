#include "bpr/application/profile_storage_authority.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	profile_storage_authority::profile_storage_authority(const core::bpr_profile_id aProfile, const profile_root_mode aMode,
														 std::string aIdentity, const std::uint64_t aRevision)
		: Profile{ aProfile }
		, Mode{ aMode }
		, Identity{ std::move(aIdentity) }
		, Revision{ aRevision }
	{ }

	auto profile_storage_authority::select(const profile_registry_snapshot& aRegistry, const core::bpr_profile_id aProfile)
		-> std::expected<profile_storage_authority, profile_storage_authority_failure>
	{
		if (!valid(aRegistry)) {
			return std::unexpected(profile_storage_authority_failure::invalid_registry);
		}
		const auto found = std::ranges::find_if(aRegistry.Profiles, [aProfile](const auto& aEntry) { return aEntry.Profile.Id == aProfile; });
		if (found == aRegistry.Profiles.end()) {
			return std::unexpected(profile_storage_authority_failure::unknown_profile);
		}
		return profile_storage_authority{ found->Profile.Id, found->RootMode, found->RootIdentity, aRegistry.Revision };
	}

	auto profile_storage_authority::profile() const noexcept -> core::bpr_profile_id { return Profile; }
	auto profile_storage_authority::root_mode() const noexcept -> profile_root_mode { return Mode; }
	auto profile_storage_authority::root_identity() const noexcept -> std::string_view { return Identity; }
	auto profile_storage_authority::registry_revision() const noexcept -> std::uint64_t { return Revision; }
}
