#pragma once

#include "bpr/core/github_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bpr::application
{
template<typename Tag>
class github_revision
{
public:
    constexpr github_revision() = default;
    explicit constexpr github_revision(const std::uint64_t value) noexcept
        : Value{value}
    {
    }

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t
    {
        return Value;
    }

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        return Value != 0U;
    }

    auto operator<=>(const github_revision&) const = default;

private:
    std::uint64_t Value = 0U;
};

struct github_profile_revision_tag;
struct github_probe_contract_revision_tag;

using github_profile_revision =
    github_revision<github_profile_revision_tag>;
using github_probe_contract_revision =
    github_revision<github_probe_contract_revision_tag>;

inline constexpr std::size_t maximum_github_hostname_bytes = 253U;
inline constexpr std::size_t maximum_github_hostname_label_bytes = 63U;

[[nodiscard]] constexpr auto valid_github_canonical_hostname(
    const std::string_view hostname) noexcept -> bool
{
    if(
        hostname.empty() || hostname.size() > maximum_github_hostname_bytes
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
            label.empty() || label.size() > maximum_github_hostname_label_bytes
            || label.front() == '-' || label.back() == '-'
            || label.starts_with("xn--"))
        {
            return false;
        }

        bool label_has_letter = false;
        for(const auto character : label)
        {
            if(character >= 'a' && character <= 'z')
            {
                label_has_letter = true;
                continue;
            }
            if(
                (character < '0' || character > '9')
                && character != '-')
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

class github_canonical_hostname final
{
public:
    constexpr github_canonical_hostname() = default;

    explicit constexpr github_canonical_hostname(
        const std::string_view hostname) noexcept
    {
        if(!valid_github_canonical_hostname(hostname))
        {
            return;
        }
        for(std::size_t index = 0U; index < hostname.size(); ++index)
        {
            Bytes[index] = hostname[index];
        }
        Size = static_cast<std::uint8_t>(hostname.size());
    }

    [[nodiscard]] constexpr auto value() const noexcept -> std::string_view
    {
        return {Bytes.data(), Size};
    }

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        return Size != 0U;
    }

    auto operator<=>(const github_canonical_hostname&) const = default;

private:
    std::array<char, maximum_github_hostname_bytes> Bytes{};
    std::uint8_t Size = 0U;
};

class github_freshness_epoch
{
public:
    constexpr github_freshness_epoch() = default;
    explicit constexpr github_freshness_epoch(
        const std::uint64_t value) noexcept
        : Value{value}
    {
    }

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t
    {
        return Value;
    }

    auto operator<=>(const github_freshness_epoch&) const = default;

private:
    std::uint64_t Value = 0U;
};

enum class github_server_contract : std::uint8_t
{
    github_com_2022_11_28,
    ghes_3_18_2022_11_28,
};

enum class github_product_kind : std::uint8_t
{
    unknown,
    github_com,
    github_enterprise_server,
};

enum class github_authentication_transport : std::uint8_t
{
    github_cli,
    native_https,
};

enum class github_proxy_mode : std::uint8_t
{
    system,
};

enum class github_trust_mode : std::uint8_t
{
    system,
};

struct github_profile_authority
{
    core::host_profile_id Profile;
    github_profile_revision ProfileRevision;
    core::git_host_id Host;
    github_canonical_hostname Hostname;
    github_server_contract ServerContract =
        github_server_contract::github_com_2022_11_28;
    github_authentication_transport Authentication =
        github_authentication_transport::native_https;
    github_probe_contract_revision ProbeContractRevision;

    auto operator<=>(const github_profile_authority&) const = default;
};

struct github_read_shape
{
    bool ReviewThreads = false;
    bool ViewedFileState = false;
    bool ResolvePermission = false;
    bool UnresolvePermission = false;

    auto operator<=>(const github_read_shape&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const github_server_contract contract) noexcept -> bool
{
    switch(contract)
    {
    case github_server_contract::github_com_2022_11_28:
    case github_server_contract::ghes_3_18_2022_11_28:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_github_authority_hostname(
    const github_server_contract contract,
    const github_canonical_hostname& hostname) noexcept -> bool
{
    if(!valid(contract) || !hostname.valid())
    {
        return false;
    }
    constexpr std::string_view github_hostname = "github.com";
    switch(contract)
    {
    case github_server_contract::github_com_2022_11_28:
        return hostname.value() == github_hostname;
    case github_server_contract::ghes_3_18_2022_11_28:
        return hostname.value() != github_hostname
            && !hostname.value().ends_with(
                std::string_view{".github.com"});
    }
    return false;
}

[[nodiscard]] constexpr auto product_kind(
    const github_server_contract contract) noexcept
    -> github_product_kind
{
    switch(contract)
    {
    case github_server_contract::github_com_2022_11_28:
        return github_product_kind::github_com;
    case github_server_contract::ghes_3_18_2022_11_28:
        return github_product_kind::github_enterprise_server;
    }
    return github_product_kind::unknown;
}

[[nodiscard]] constexpr auto valid(
    const github_product_kind product) noexcept -> bool
{
    return product == github_product_kind::github_com
        || product == github_product_kind::github_enterprise_server;
}

[[nodiscard]] constexpr auto rest_api_version(
    const github_server_contract contract) noexcept -> std::string_view
{
    return valid(contract) ? "2022-11-28" : std::string_view{};
}

[[nodiscard]] constexpr auto valid(
    const github_authentication_transport transport) noexcept -> bool
{
    switch(transport)
    {
    case github_authentication_transport::github_cli:
    case github_authentication_transport::native_https:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid(
    const github_proxy_mode mode) noexcept -> bool
{
    return mode == github_proxy_mode::system;
}

[[nodiscard]] constexpr auto valid(
    const github_trust_mode mode) noexcept -> bool
{
    return mode == github_trust_mode::system;
}

[[nodiscard]] constexpr auto valid(
    const github_profile_authority& authority) noexcept -> bool
{
    return authority.Profile.value() != 0U
        && authority.ProfileRevision.valid()
        && authority.Host.value() != 0U
        && valid_github_authority_hostname(
            authority.ServerContract, authority.Hostname)
        && valid(authority.Authentication)
        && authority.ProbeContractRevision.valid();
}
}
