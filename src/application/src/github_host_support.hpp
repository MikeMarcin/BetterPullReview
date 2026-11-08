#ifndef BPR_SRC_APPLICATION_SRC_GITHUB_HOST_SUPPORT_HPP
#define BPR_SRC_APPLICATION_SRC_GITHUB_HOST_SUPPORT_HPP
#include "bpr/application/github_application_host.hpp"
#include "github_projection_mapping.hpp"

#include <optional>

// Helpers the GitHub application host's sources share.
namespace bpr::application {
	[[nodiscard]] constexpr auto
	retryable(const github_provider_error& aError) noexcept -> bool
	{
		if (!valid(aError)
			|| (aError.RetryAfterSeconds.has_value()
				&& *aError.RetryAfterSeconds != 0U)
			|| is_limited(aError.RateLimits)) {
			return false;
		}
		return aError.Failure == github_provider_failure::network
			|| aError.Failure == github_provider_failure::timeout
			|| aError.Failure == github_provider_failure::unavailable;
	}

	[[nodiscard]] constexpr auto
	mapped_failure(const github_provider_failure aFailure) noexcept
		-> core::github_failure
	{
		switch (aFailure) {
		case github_provider_failure::authentication:
			return core::github_failure::authentication;
		case github_provider_failure::permission:
			return core::github_failure::permission;
		case github_provider_failure::network:
		case github_provider_failure::process_startup:
		case github_provider_failure::unavailable:
			return core::github_failure::network;
		case github_provider_failure::api_version:
			return core::github_failure::api_version;
		case github_provider_failure::unsupported_capability:
			return core::github_failure::capability;
		case github_provider_failure::rate_limit:
			return core::github_failure::rate_limit;
		case github_provider_failure::certificate:
			return core::github_failure::certificate;
		case github_provider_failure::redirect:
			return core::github_failure::redirect;
		case github_provider_failure::timeout:
			return core::github_failure::timeout;
		case github_provider_failure::cli_unavailable:
			return core::github_failure::cli_unavailable;
		case github_provider_failure::vault:
			return core::github_failure::vault;
		case github_provider_failure::malformed_response:
			return core::github_failure::malformed_response;
		case github_provider_failure::response_limit:
			return core::github_failure::response_too_large;
		}
		return core::github_failure::malformed_response;
	}

	[[nodiscard]] constexpr auto
	valid_rate_limit_resources(const github_rate_limit_state& aState,
							   const bool                     aAllowCore,
							   const bool                     aAllowSearch,
							   const bool                     aAllowGraphql) noexcept -> bool
	{
		return valid(aState) && (aAllowCore || !aState.Core.has_value())
			&& (aAllowSearch || !aState.Search.has_value())
			&& (aAllowGraphql || !aState.Graphql.has_value());
	}

	[[nodiscard]] constexpr auto
	requires_rate_limit_observations(const github_server_contract aContract) noexcept
		-> bool
	{
		return aContract == github_server_contract::github_com_2022_11_28;
	}

	[[nodiscard]] auto
	valid_viewer(const github_profile_authority&             aAuthority,
				 const core::github_session_request_context& aContext,
				 const github_viewer_record&                 aViewer,
				 const github_rate_limit_state&              aRateLimits) noexcept -> bool;

	template <typename Completion>
	[[nodiscard]] auto completion_context(const Completion& aCompletion)
	{
		return std::visit([](const auto& aTerminal) { return aTerminal.Context; },
						  aCompletion);
	}

	template <typename Completion>
	[[nodiscard]] decltype(auto)
	provider_completion_request(const Completion& aCompletion)
	{
		return std::visit([](const auto& aTerminal) -> const auto& { return aTerminal.Request; },
						  aCompletion);
	}

	[[nodiscard]] auto create_host_capability_cache() -> github_capability_cache;
}
#endif
