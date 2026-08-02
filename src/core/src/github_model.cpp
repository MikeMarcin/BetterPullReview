#include "bpr/core/github_model.hpp"

#include <string_view>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto valid_locator_character(const char aValue) noexcept
			-> bool
		{
			return (aValue >= 'a' && aValue <= 'z')
				|| (aValue >= 'A' && aValue <= 'Z')
				|| (aValue >= '0' && aValue <= '9') || aValue == '-'
				|| aValue == '_' || aValue == '.';
		}

		[[nodiscard]] auto valid_locator_part(
			const std::string_view aValue,
			const std::size_t      aMaximumLength) noexcept -> bool
		{
			if (aValue.empty() || aValue.size() > aMaximumLength) {
				return false;
			}
			if (aValue == "." || aValue == "..") {
				return false;
			}

			for (const auto character : aValue) {
				if (!valid_locator_character(character)) {
					return false;
				}
			}
			return true;
		}
	}

	auto valid(const repository_locator& aLocator) noexcept -> bool
	{
		return valid_locator_part(
				   aLocator.Owner,
				   maximum_repository_owner_length)
			&& valid_locator_part(
				   aLocator.Name,
				   maximum_repository_name_length);
	}

	auto capability_state(
		const github_capabilities& aCapabilities,
		const github_capability    aCapability) noexcept -> github_capability_state
	{
		switch (aCapability) {
		case github_capability::review_threads:
			return aCapabilities.ReviewThreads;
		case github_capability::viewed_file_mutations:
			return aCapabilities.ViewedFileMutations;
		case github_capability::resolve_thread:
			return aCapabilities.ResolveThread;
		case github_capability::unresolve_thread:
			return aCapabilities.UnresolveThread;
		}

		return github_capability_state::unknown;
	}

	auto capability_supported(
		const github_capabilities& aCapabilities,
		const github_capability    aCapability) noexcept -> bool
	{
		return capability_state(aCapabilities, aCapability)
			== github_capability_state::supported;
	}

	auto reason_for(const github_failure aFailure) noexcept -> github_reason_code
	{
		switch (aFailure) {
		case github_failure::authentication:
			return github_reason_code::authentication_failed;
		case github_failure::permission:
			return github_reason_code::permission_denied;
		case github_failure::network:
			return github_reason_code::network_unavailable;
		case github_failure::api_version:
			return github_reason_code::api_version_unsupported;
		case github_failure::capability:
			return github_reason_code::capability_unavailable;
		case github_failure::rate_limit:
			return github_reason_code::rate_limited;
		case github_failure::malformed_response:
			return github_reason_code::malformed_response;
		case github_failure::response_too_large:
			return github_reason_code::response_too_large;
		case github_failure::vault:
			return github_reason_code::vault_unavailable;
		case github_failure::certificate:
			return github_reason_code::certificate_rejected;
		case github_failure::redirect:
			return github_reason_code::redirect_rejected;
		case github_failure::timeout:
			return github_reason_code::timed_out;
		case github_failure::cli_unavailable:
			return github_reason_code::cli_unavailable;
		}

		return github_reason_code::network_unavailable;
	}
}
