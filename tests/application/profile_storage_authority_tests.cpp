#include "bpr/application/profile_storage_authority.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {
	using namespace bpr;
	using namespace bpr::application;
	static_assert(!std::is_default_constructible_v<profile_storage_authority>);
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}

int main()
{
	auto registry          = profile_registry_snapshot{ };
	registry.LastProfileId = 2U;
	registry.Profiles.push_back({ { core::bpr_profile_id{ 2U }, "../Work" }, profile_root_mode::isolated, "profile-2" });
	const auto original = profile_storage_authority::select(registry, core::default_bpr_profile_id);
	const auto work     = profile_storage_authority::select(registry, core::bpr_profile_id{ 2U });
	expect(original && original->root_mode() == profile_root_mode::legacy && original->root_identity() == "legacy", "Default alone selects the existing environment in place");
	expect(work && work->profile() == core::bpr_profile_id{ 2U } && work->root_identity() == "profile-2"
			   && work->root_mode() == profile_root_mode::isolated && work->registry_revision() == registry.Revision,
		   "exact admitted ID determines root authority and records its registry revision");
	registry.Profiles.back().Profile.Label = "Renamed";
	++registry.Revision;
	const auto renamed = profile_storage_authority::select(registry, core::bpr_profile_id{ 2U });
	expect(renamed && renamed->root_identity() == work->root_identity() && renamed->profile() == work->profile(), "rename cannot change root selection");
	const auto unknown = profile_storage_authority::select(registry, core::bpr_profile_id{ 99U });
	expect(!unknown && unknown.error() == profile_storage_authority_failure::unknown_profile, "unknown identity never falls back to Default");
	for (const auto token : { "legacy", "../profile-2", "/tmp/profile-2", "profile-3", "profiles/profile-2" }) {
		auto invalid                         = registry;
		invalid.Profiles.back().RootIdentity = token;
		const auto refused                   = profile_storage_authority::select(invalid, core::default_bpr_profile_id);
		expect(!refused && refused.error() == profile_storage_authority_failure::invalid_registry, "malformed root tokens elsewhere invalidate the entire authority observation");
	}
	registry.Profiles.back().RootMode = profile_root_mode::legacy;
	expect(!profile_storage_authority::select(registry, core::bpr_profile_id{ 2U }), "another profile cannot adopt the legacy root");
	registry                = profile_registry_snapshot{ };
	registry.DefaultAdopted = false;
	expect(!profile_storage_authority::select(registry, core::default_bpr_profile_id), "incomplete adoption cannot select a storage root");
	std::cout << "profile-storage-authority-tests:pass\n";
	return EXIT_SUCCESS;
}
