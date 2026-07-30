#include "bpr/core/github_model.hpp"

#include <string_view>

namespace bpr::core
{
namespace
{
[[nodiscard]] constexpr auto valid_locator_character(const char value) noexcept
    -> bool
{
    return (value >= 'a' && value <= 'z')
        || (value >= 'A' && value <= 'Z')
        || (value >= '0' && value <= '9') || value == '-'
        || value == '_' || value == '.';
}

[[nodiscard]] auto valid_locator_part(
    const std::string_view value,
    const std::size_t maximum_length) noexcept -> bool
{
    if(value.empty() || value.size() > maximum_length)
    {
        return false;
    }
    if(value == "." || value == "..")
    {
        return false;
    }

    for(const auto character : value)
    {
        if(!valid_locator_character(character))
        {
            return false;
        }
    }
    return true;
}
}

auto valid(const repository_locator& locator) noexcept -> bool
{
    return valid_locator_part(
               locator.Owner,
               maximum_repository_owner_length)
        && valid_locator_part(
            locator.Name,
            maximum_repository_name_length);
}

auto capability_state(
    const github_capabilities& capabilities,
    const github_capability capability) noexcept -> github_capability_state
{
    switch(capability)
    {
    case github_capability::review_threads:
        return capabilities.ReviewThreads;
    case github_capability::viewed_file_mutations:
        return capabilities.ViewedFileMutations;
    case github_capability::resolve_thread:
        return capabilities.ResolveThread;
    case github_capability::unresolve_thread:
        return capabilities.UnresolveThread;
    }

    return github_capability_state::unknown;
}

auto capability_supported(
    const github_capabilities& capabilities,
    const github_capability capability) noexcept -> bool
{
    return capability_state(capabilities, capability)
        == github_capability_state::supported;
}

auto reason_for(const github_failure failure) noexcept -> github_reason_code
{
    switch(failure)
    {
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
