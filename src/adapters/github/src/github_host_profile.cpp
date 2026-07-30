#include "github_host_profile.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace bpr::adapters::github
{
namespace
{
constexpr std::size_t maximum_hostname_bytes = 253U;
constexpr std::size_t maximum_hostname_label_bytes = 63U;
constexpr std::size_t maximum_redirect_target_bytes = 2U * 1024U;
constexpr std::string_view github_com_hostname = "github.com";
constexpr std::string_view github_com_rest_root = "https://api.github.com";
constexpr std::string_view github_com_graphql_root =
    "https://api.github.com/graphql";
constexpr std::string_view https_prefix = "https://";

[[nodiscard]] constexpr auto ascii_lower_letter(const char value) noexcept
    -> bool
{
    return value >= 'a' && value <= 'z';
}

[[nodiscard]] constexpr auto ascii_digit(const char value) noexcept -> bool
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] auto canonical_hostname(const std::string_view hostname) noexcept
    -> bool
{
    if(
        hostname.empty() || hostname.size() > maximum_hostname_bytes
        || hostname.front() == '.' || hostname.back() == '.')
    {
        return false;
    }

    bool saw_dot = false;
    std::size_t label_begin = 0U;
    while(label_begin < hostname.size())
    {
        const auto label_end = hostname.find('.', label_begin);
        const auto end =
            label_end == std::string_view::npos ? hostname.size() : label_end;
        const auto label = hostname.substr(label_begin, end - label_begin);
        if(
            label.empty() || label.size() > maximum_hostname_label_bytes
            || label.front() == '-' || label.back() == '-'
            || label.starts_with("xn--"))
        {
            return false;
        }

        bool label_has_letter = false;
        for(const auto character : label)
        {
            if(ascii_lower_letter(character))
            {
                label_has_letter = true;
                continue;
            }
            if(!ascii_digit(character) && character != '-')
            {
                return false;
            }
        }

        if(end == hostname.size())
        {
            return saw_dot && label_has_letter;
        }
        saw_dot = true;
        label_begin = end + 1U;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_provenance(
    const github_profile_provenance provenance) noexcept -> bool
{
    switch(provenance)
    {
    case github_profile_provenance::explicit_user:
    case github_profile_provenance::builtin:
    case github_profile_provenance::managed:
    case github_profile_provenance::provider_content:
    case github_profile_provenance::redirect:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto trusted_provenance(
    const github_profile_provenance provenance) noexcept -> bool
{
    return provenance == github_profile_provenance::explicit_user
        || provenance == github_profile_provenance::builtin
        || provenance == github_profile_provenance::managed;
}

[[nodiscard]] auto enterprise_rest_root(const std::string_view hostname)
    -> std::string
{
    return std::string{https_prefix} + std::string{hostname} + "/api/v3";
}

[[nodiscard]] auto enterprise_graphql_root(const std::string_view hostname)
    -> std::string
{
    return std::string{https_prefix} + std::string{hostname} + "/api/graphql";
}

[[nodiscard]] constexpr auto valid_request_surface(
    const github_request_surface surface) noexcept -> bool
{
    return surface == github_request_surface::rest
        || surface == github_request_surface::graphql;
}

[[nodiscard]] constexpr auto valid_path_character(const char value) noexcept
    -> bool
{
    return ascii_lower_letter(value)
        || (value >= 'A' && value <= 'Z') || ascii_digit(value)
        || value == '/' || value == '-' || value == '_'
        || value == '.' || value == '~';
}

[[nodiscard]] auto canonical_redirect_path(const std::string_view path) noexcept
    -> bool
{
    if(path.empty() || path.front() != '/' || path.starts_with("//"))
    {
        return false;
    }
    if(!std::ranges::all_of(path, valid_path_character))
    {
        return false;
    }

    std::size_t segment_begin = 1U;
    while(segment_begin <= path.size())
    {
        const auto segment_end = path.find('/', segment_begin);
        const auto end =
            segment_end == std::string_view::npos ? path.size() : segment_end;
        const auto segment = path.substr(segment_begin, end - segment_begin);
        if(segment == "." || segment == "..")
        {
            return false;
        }
        if(segment_end == std::string_view::npos)
        {
            break;
        }
        if(segment.empty())
        {
            return false;
        }
        segment_begin = segment_end + 1U;
    }
    return true;
}

[[nodiscard]] auto within_namespace(
    const std::string_view path,
    const std::string_view prefix) noexcept -> bool
{
    if(prefix.empty())
    {
        return true;
    }
    return path == prefix
        || (
            path.size() > prefix.size()
            && path.starts_with(prefix)
            && path[prefix.size()] == '/');
}
}

validated_github_host_profile::validated_github_host_profile(
    const github_host_profile_candidate& candidate)
    : Profile{candidate.Profile}
    , Host{candidate.Host}
    , Revision{candidate.Revision}
    , Server{candidate.Server}
    , Authentication{candidate.Authentication}
    , Proxy{candidate.Proxy}
    , Trust{candidate.Trust}
    , Provenance{candidate.Provenance}
    , Hostname{candidate.Hostname}
    , RestRoot{candidate.RestRoot}
    , GraphqlRoot{candidate.GraphqlRoot}
{
}

auto validated_github_host_profile::profile() const noexcept
    -> core::host_profile_id
{
    return Profile;
}

auto validated_github_host_profile::host() const noexcept -> core::git_host_id
{
    return Host;
}

auto validated_github_host_profile::revision() const noexcept
    -> application::github_profile_revision
{
    return Revision;
}

auto validated_github_host_profile::server_contract() const noexcept
    -> application::github_server_contract
{
    return Server;
}

auto validated_github_host_profile::product_kind() const noexcept
    -> application::github_product_kind
{
    return application::product_kind(Server);
}

auto validated_github_host_profile::authentication_transport() const noexcept
    -> application::github_authentication_transport
{
    return Authentication;
}

auto validated_github_host_profile::proxy_mode() const noexcept
    -> application::github_proxy_mode
{
    return Proxy;
}

auto validated_github_host_profile::trust_mode() const noexcept
    -> application::github_trust_mode
{
    return Trust;
}

auto validated_github_host_profile::provenance() const noexcept
    -> github_profile_provenance
{
    return Provenance;
}

auto validated_github_host_profile::hostname() const noexcept
    -> std::string_view
{
    return Hostname;
}

auto validated_github_host_profile::rest_root() const noexcept
    -> std::string_view
{
    return RestRoot;
}

auto validated_github_host_profile::graphql_root() const noexcept
    -> std::string_view
{
    return GraphqlRoot;
}

auto validated_github_host_profile::rest_authority() const noexcept
    -> std::string_view
{
    return Server
            == application::github_server_contract::github_com_2022_11_28
        ? std::string_view{"api.github.com"}
        : std::string_view{Hostname};
}

auto validated_github_host_profile::rest_route_prefix() const noexcept
    -> std::string_view
{
    return Server
            == application::github_server_contract::github_com_2022_11_28
        ? std::string_view{}
        : std::string_view{"/api/v3"};
}

auto validated_github_host_profile::graphql_route() const noexcept
    -> std::string_view
{
    return Server
            == application::github_server_contract::github_com_2022_11_28
        ? std::string_view{"/graphql"}
        : std::string_view{"/api/graphql"};
}

auto validated_github_host_profile::rest_api_version() const noexcept
    -> std::string_view
{
    return application::rest_api_version(Server);
}

auto validate_github_host_profile(
    const github_host_profile_candidate& candidate) -> github_profile_result
{
    if(candidate.Profile.value() == 0U)
    {
        return std::unexpected{
            github_profile_failure::invalid_profile_identity};
    }
    if(candidate.Host.value() == 0U)
    {
        return std::unexpected{
            github_profile_failure::invalid_host_identity};
    }
    if(!candidate.Revision.valid())
    {
        return std::unexpected{
            github_profile_failure::invalid_profile_revision};
    }
    if(!application::valid(candidate.Server))
    {
        return std::unexpected{
            github_profile_failure::invalid_server_contract};
    }
    if(!application::valid(candidate.Authentication))
    {
        return std::unexpected{
            github_profile_failure::invalid_authentication_transport};
    }
    if(!application::valid(candidate.Proxy))
    {
        return std::unexpected{github_profile_failure::invalid_proxy_mode};
    }
    if(!application::valid(candidate.Trust))
    {
        return std::unexpected{github_profile_failure::invalid_trust_mode};
    }
    if(!valid_provenance(candidate.Provenance))
    {
        return std::unexpected{
            github_profile_failure::invalid_provenance};
    }
    if(!trusted_provenance(candidate.Provenance))
    {
        return std::unexpected{
            github_profile_failure::untrusted_provenance};
    }
    if(!canonical_hostname(candidate.Hostname))
    {
        return std::unexpected{github_profile_failure::invalid_hostname};
    }

    std::string expected_rest_root;
    std::string expected_graphql_root;
    if(
        candidate.Server
        == application::github_server_contract::github_com_2022_11_28)
    {
        if(candidate.Hostname != github_com_hostname)
        {
            return std::unexpected{github_profile_failure::invalid_hostname};
        }
        expected_rest_root = github_com_rest_root;
        expected_graphql_root = github_com_graphql_root;
    }
    else
    {
        if(
            candidate.Hostname == github_com_hostname
            || candidate.Hostname == "api.github.com")
        {
            return std::unexpected{github_profile_failure::invalid_hostname};
        }
        expected_rest_root = enterprise_rest_root(candidate.Hostname);
        expected_graphql_root =
            enterprise_graphql_root(candidate.Hostname);
    }

    if(candidate.RestRoot != expected_rest_root)
    {
        return std::unexpected{github_profile_failure::invalid_rest_root};
    }
    if(candidate.GraphqlRoot != expected_graphql_root)
    {
        return std::unexpected{github_profile_failure::invalid_graphql_root};
    }
    const validated_github_host_profile profile{candidate};
    return github_profile_result{profile};
}

approved_github_redirect_target::approved_github_redirect_target(
    std::string url)
    : Url{std::move(url)}
{
}

auto approved_github_redirect_target::url() const noexcept -> std::string_view
{
    return Url;
}

auto validate_github_redirect_target(
    const validated_github_host_profile& profile,
    const github_request_surface surface,
    const std::string_view target) -> github_redirect_result
{
    if(!valid_request_surface(surface))
    {
        return std::unexpected{
            github_redirect_failure::invalid_surface};
    }
    if(surface == github_request_surface::graphql)
    {
        return std::unexpected{
            github_redirect_failure::redirects_not_permitted};
    }
    if(target.size() > maximum_redirect_target_bytes)
    {
        return std::unexpected{
            github_redirect_failure::target_too_long};
    }
    if(!target.starts_with(https_prefix))
    {
        return std::unexpected{github_redirect_failure::invalid_target};
    }

    const auto authority_begin = https_prefix.size();
    const auto path_begin = target.find('/', authority_begin);
    const auto authority = target.substr(
        authority_begin,
        path_begin == std::string_view::npos
            ? target.size() - authority_begin
            : path_begin - authority_begin);
    if(
        authority.empty() || authority.find('@') != std::string_view::npos
        || authority.find(':') != std::string_view::npos
        || authority.find('%') != std::string_view::npos
        || authority.find('[') != std::string_view::npos
        || authority.find(']') != std::string_view::npos)
    {
        return std::unexpected{github_redirect_failure::invalid_target};
    }
    if(authority != profile.rest_authority())
    {
        return std::unexpected{
            github_redirect_failure::authority_mismatch};
    }

    const auto path = path_begin == std::string_view::npos
        ? std::string_view{"/"}
        : target.substr(path_begin);
    if(!canonical_redirect_path(path))
    {
        return std::unexpected{github_redirect_failure::invalid_target};
    }
    if(!within_namespace(path, profile.rest_route_prefix()))
    {
        return std::unexpected{
            github_redirect_failure::outside_api_namespace};
    }
    const approved_github_redirect_target approved{std::string{target}};
    return github_redirect_result{approved};
}
}
