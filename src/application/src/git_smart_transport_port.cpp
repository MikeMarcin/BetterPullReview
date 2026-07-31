#include "bpr/application/git_smart_transport_port.hpp"

#include "bpr/core/epoch_transition.hpp"

#include <algorithm>
#include <string_view>

namespace bpr::application
{
namespace
{
[[nodiscard]] constexpr auto forbidden_ref_character(
    const char character) noexcept -> bool
{
    const auto byte = static_cast<unsigned char>(character);
    return byte <= 0x20U || byte == 0x7fU || character == '~'
        || character == '^' || character == ':' || character == '?'
        || character == '*' || character == '[' || character == '\\';
}

[[nodiscard]] auto valid_ref_component(
    const std::string_view component) noexcept -> bool
{
    return !component.empty() && component != "." && component != ".."
        && component.front() != '.' && component.back() != '.'
        && !component.ends_with(".lock");
}
}

auto parse_git_base_branch_ref(const std::string_view value) noexcept
    -> std::optional<git_base_branch_ref>
{
    constexpr std::string_view prefix = "refs/heads/";
    if(
        value.size() <= prefix.size()
        || value.size() > maximum_git_base_branch_ref_bytes
        || !value.starts_with(prefix) || value.ends_with('/')
        || value.ends_with('.') || value.find("..") != std::string_view::npos
        || value.find("@{") != std::string_view::npos
        || std::any_of(
            value.begin(),
            value.end(),
            forbidden_ref_character))
    {
        return std::nullopt;
    }

    auto component_start = prefix.size();
    while(component_start < value.size())
    {
        const auto separator = value.find('/', component_start);
        const auto component_end =
            separator == std::string_view::npos ? value.size() : separator;
        if(!valid_ref_component(
               value.substr(
                   component_start,
                   component_end - component_start)))
        {
            return std::nullopt;
        }
        if(separator == std::string_view::npos)
        {
            break;
        }
        component_start = separator + 1U;
    }

    git_base_branch_ref result;
    std::copy(value.begin(), value.end(), result.Characters.begin());
    result.Length = static_cast<std::uint16_t>(value.size());
    return result;
}

auto valid(const git_smart_request_context& context) noexcept -> bool
{
    return context.ApplicationSession.value() != 0U
        && core::valid(context.Authority)
        && core::valid(context.Revision)
        && core::valid(context.Repository) && valid(context.BaseRef);
}

auto valid(const git_smart_discovery_request& request) noexcept -> bool
{
    return valid(request.Context);
}

auto valid(const git_smart_ls_refs_request& request) noexcept -> bool
{
    return valid(request.Context) && !request.Body.empty()
        && request.Body.size() <= maximum_git_smart_request_bytes;
}

auto valid(const git_smart_fetch_request& request) noexcept -> bool
{
    return valid(request.Context) && !request.Body.empty()
        && request.Body.size() <= maximum_git_smart_request_bytes
        && request.MaximumResponseBytes != 0U
        && request.MaximumResponseBytes
            <= maximum_git_smart_fetch_response_bytes;
}
}
