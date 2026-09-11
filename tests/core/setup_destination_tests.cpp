#include "bpr/core/setup_destination.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}
int main()
{
	using namespace bpr::core;
	const auto destination = std::optional{ setup_destination{ .Hostname = "ghe.example", .Repository = { "owner", "repo" }, .Number = pull_request_number{ 42U }, .Location = "/files#diff-deadbeef" } };

	const auto owner    = setup_owner_id{ 7U };
	const auto request  = setup_request_id{ 11U };
	auto       pending  = pending_setup_destination{ .Request = request, .Owner = owner, .Destination = *destination, .Profile = std::nullopt };
	const auto profiles = std::array{ setup_profile_observation{ host_profile_id{ 2U }, "github.com", true }, setup_profile_observation{ host_profile_id{ 3U }, "ghe.example", false } };
	auto       resolved = resolve_setup_destination(pending, owner, profiles);
	expect(resolved && resolved->Profile == host_profile_id{ 3U } && resolved->Stage == setup_destination_stage::sign_in, "never route to active other host");
	expect(!accept_setup_destination(resolved, request, owner, host_profile_id{ 3U }, "ghe.example"), "sign in requires explicit continuation");
	resolved->Stage = setup_destination_stage::authenticating;
	expect(accept_setup_destination(resolved, request, owner, host_profile_id{ 3U }, "ghe.example") == destination, "exact accepted destination");
	expect(!accept_setup_destination(resolved, setup_request_id{ 12U }, owner, host_profile_id{ 3U }, "ghe.example"), "stale request refused");
	expect(!accept_setup_destination(resolved, request, setup_owner_id{ 8U }, host_profile_id{ 3U }, "ghe.example"), "replaced owner refused");
	expect(!accept_setup_destination(resolved, request, owner, host_profile_id{ 2U }, "ghe.example"), "wrong profile refused");
	expect(!accept_setup_destination(resolved, request, owner, host_profile_id{ 3U }, "github.com"), "wrong hostname refused");
	expect(!accept_setup_destination(std::nullopt, request, owner, host_profile_id{ 3U }, "ghe.example"), "cancelled request refused");
	pending.Destination.Hostname = "unknown.example";
	resolved                     = resolve_setup_destination(pending, owner, profiles);
	expect(resolved && !resolved->Profile && resolved->Stage == setup_destination_stage::add_server, "unknown authority requires explicit configuration");
	// [[BPR-0732]]. A known public host with no account opens signed out in
	// one gesture; a host that refuses preview keeps the sign-in stop; an
	// account still wins over preview.
	const auto anonymous         = std::array{ setup_profile_observation{ host_profile_id{ 2U }, "github.com", false, true }, setup_profile_observation{ host_profile_id{ 3U }, "ghe.example", false, false } };
	pending.Destination.Hostname = "github.com";
	resolved                     = resolve_setup_destination(pending, owner, anonymous);
	expect(resolved && resolved->Profile == host_profile_id{ 2U } && resolved->Stage == setup_destination_stage::public_preview, "public host with no account opens signed out");
	expect(accept_setup_destination(resolved, request, owner, host_profile_id{ 2U }, "github.com") == pending.Destination, "the signed-out stage is accepted without a continuation press");
	pending.Destination.Hostname = "ghe.example";
	resolved                     = resolve_setup_destination(pending, owner, anonymous);
	expect(resolved && resolved->Profile == host_profile_id{ 3U } && resolved->Stage == setup_destination_stage::sign_in, "a host that refuses preview keeps sign in");
	const auto both              = std::array{ setup_profile_observation{ host_profile_id{ 2U }, "github.com", true, true } };
	pending.Destination.Hostname = "github.com";
	resolved                     = resolve_setup_destination(pending, owner, both);
	expect(resolved && resolved->Stage == setup_destination_stage::ready, "an account wins over preview");
	return EXIT_SUCCESS;
}
