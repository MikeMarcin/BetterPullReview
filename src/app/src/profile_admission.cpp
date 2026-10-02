#include "profile_admission.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include <algorithm>
namespace bpr::app {
	auto admit_profile(const std::filesystem::path& aBase, const std::string& aProfile) -> tour_cli_result<profile_admission>
	{
		const auto refuse = [](std::string aCode, std::string aMessage, const int aExit) { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); };
		if (!adapters::sqlite::create_storage_root_owner_private(aBase)) {
			return refuse("profile_unavailable", "The application root is unavailable or is not owner-private.", 7);
		}
		auto registry = adapters::sqlite::make_profile_registry_store(aBase);
		auto loaded   = registry->load({ });
		if (!loaded) {
			return refuse("profile_unavailable", "The private profile registry could not be opened.", 7);
		}
		const auto matches = [&](const auto& aEntry) { return aProfile.empty() ? aEntry.Profile.Id == core::default_bpr_profile_id : aProfile == std::to_string(aEntry.Profile.Id.value()) || aProfile == aEntry.Profile.Label; };
		if (std::ranges::count_if(loaded->Profiles, matches) > 1) {
			return refuse("profile_ambiguous", "The profile label is ambiguous; select its numeric ID.", 2);
		}
		const auto found = std::ranges::find_if(loaded->Profiles, matches);
		if (found == loaded->Profiles.end()) {
			return refuse("profile_unknown", "Select an existing profile by its exact name or ID.", 2);
		}
		auto admission = profile_admission{ .Profile = found->Profile.Id };
		auto authority = application::profile_storage_authority::select(*loaded, admission.Profile);
		if (!authority) {
			return refuse("profile_unavailable", "The profile root authority is invalid.", 7);
		}
		auto root = adapters::sqlite::resolve_profile_storage_root(aBase, *authority);
		if (!root) {
			return refuse("profile_unavailable", "The profile root is unavailable or is not owner-private.", 7);
		}
		admission.Root = *root;
		auto lease     = adapters::sqlite::acquire_profile_runtime_lease(admission.Root);
		if (!lease) {
			if (lease.error() != application::profile_runtime_lease_failure::busy) {
				return refuse("profile_unavailable", "The profile lease is unavailable.", 7);
			}
			return admission;
		}
		admission.Lease = std::move(*lease);
		return admission;
	}
}
