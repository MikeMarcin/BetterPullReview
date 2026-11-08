#include "github_host_support.hpp"

#include <expected>
#include <new>
#include <utility>

namespace bpr::application {
	auto
	valid_viewer(const github_profile_authority&             aAuthority,
				 const core::github_session_request_context& aContext,
				 const github_viewer_record&                 aViewer,
				 const github_rate_limit_state&              aRateLimits) noexcept -> bool
	{
		if (!valid_rate_limit_resources(aRateLimits, true, false, false)
			|| is_secondary_limited(aRateLimits)
			|| (requires_rate_limit_observations(aAuthority.ServerContract)
				&& !aRateLimits.Core.has_value())) {
			return false;
		}
		const github_session_read_model model{
			.Profile            = aAuthority.Profile,
			.Host               = aAuthority.Host,
			.Viewer             = aViewer,
			.CapabilityEvidence = {
				.Capabilities    = { },
				.Revision        = core::capability_revision{ 1U },
				.ReadShape       = { },
				.ContractVersion = 1U,
			},
			.RateLimits = aRateLimits,
		};
		return validate(model, aContext).valid();
	}

	auto create_host_capability_cache() -> github_capability_cache
	{
		auto cache = github_capability_cache::create({ });
		if (!cache.has_value()) {
			throw std::bad_alloc{ };
		}
		return std::move(*cache);
	}
}
