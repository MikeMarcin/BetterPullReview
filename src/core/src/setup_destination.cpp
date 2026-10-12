#include "bpr/core/setup_destination.hpp"

#include <algorithm>

namespace bpr::core {
	auto resolve_setup_destination(pending_setup_destination aPending,
								   const setup_owner_id aOwner, const std::span<const setup_profile_observation> aProfiles)
		-> std::optional<pending_setup_destination>
	{
		if (aPending.Owner != aOwner) {
			return std::nullopt;
		}
		auto found = std::ranges::find_if(aProfiles, [&aPending](const auto& aProfile) {
			return aProfile.Hostname == aPending.Destination.Hostname && aProfile.Authenticated;
		});
		if (found == aProfiles.end()) {
			found = std::ranges::find_if(aProfiles, [&aPending](const auto& aProfile) {
				return aProfile.Hostname == aPending.Destination.Hostname && aProfile.RevalidationAllowed;
			});
		}
		if (found == aProfiles.end()) {
			found = std::ranges::find(aProfiles, aPending.Destination.Hostname, &setup_profile_observation::Hostname);
		}
		aPending.Profile.reset();
		aPending.Stage = setup_destination_stage::add_server;
		if (found != aProfiles.end()) {
			aPending.Profile = found->Profile;
			aPending.Stage   = found->Authenticated                                                            ? setup_destination_stage::ready
				: found->RevalidationAllowed                                                                   ? setup_destination_stage::authenticating
				: found->PreviewAvailable && aPending.Destination.Kind == setup_destination_kind::pull_request ? setup_destination_stage::public_preview
																											   : setup_destination_stage::sign_in;
		}
		return aPending;
	}

	auto accept_setup_destination(const std::optional<pending_setup_destination>& aPending,
								  const setup_request_id aRequest, const setup_owner_id aOwner,
								  const host_profile_id aProfile, const std::string_view aHostname) -> std::optional<setup_destination>
	{
		if (!aPending || aPending->Request != aRequest || aPending->Owner != aOwner
			|| aPending->Profile != aProfile || aPending->Destination.Hostname != aHostname
			|| (aPending->Stage != setup_destination_stage::authenticating && aPending->Stage != setup_destination_stage::opening && aPending->Stage != setup_destination_stage::ready
				&& aPending->Stage != setup_destination_stage::public_preview)) {
			return std::nullopt;
		}
		return aPending->Destination;
	}
}
