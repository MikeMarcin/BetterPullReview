#include "github_host_profile.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
using namespace bpr;
using namespace bpr::adapters::github;

static_assert(
    !std::is_default_constructible_v<validated_github_host_profile>);
static_assert(
    !std::is_constructible_v<
        validated_github_host_profile,
        github_host_profile_candidate>);
static_assert(
    std::is_copy_constructible_v<validated_github_host_profile>);
static_assert(
    !std::is_move_constructible_v<validated_github_host_profile>);
static_assert(
    !std::is_move_assignable_v<validated_github_host_profile>);
static_assert(
    !std::is_default_constructible_v<approved_github_redirect_target>);
static_assert(
    !std::is_same_v<
        approved_github_redirect_target,
        validated_github_host_profile>);
static_assert(
    std::is_copy_constructible_v<approved_github_redirect_target>);
static_assert(
    !std::is_move_constructible_v<approved_github_redirect_target>);
static_assert(
    !std::is_move_assignable_v<approved_github_redirect_target>);

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

[[nodiscard]] auto github_com_candidate() -> github_host_profile_candidate
{
    return {
        .Profile = core::host_profile_id{11},
        .Host = core::git_host_id{21},
        .Revision = application::github_profile_revision{31},
        .Server =
            application::github_server_contract::github_com_2022_11_28,
        .Authentication =
            application::github_authentication_transport::github_cli,
        .Proxy = application::github_proxy_mode::system,
        .Trust = application::github_trust_mode::system,
        .Provenance = github_profile_provenance::builtin,
        .Hostname = "github.com",
        .RestRoot = "https://api.github.com",
        .GraphqlRoot = "https://api.github.com/graphql",
    };
}

[[nodiscard]] auto enterprise_candidate(
    std::string hostname = "ghe.example.invalid")
    -> github_host_profile_candidate
{
    return {
        .Profile = core::host_profile_id{12},
        .Host = core::git_host_id{22},
        .Revision = application::github_profile_revision{32},
        .Server =
            application::github_server_contract::ghes_3_18_2022_11_28,
        .Authentication =
            application::github_authentication_transport::native_https,
        .Proxy = application::github_proxy_mode::system,
        .Trust = application::github_trust_mode::system,
        .Provenance = github_profile_provenance::explicit_user,
        .Hostname = hostname,
        .RestRoot = "https://" + hostname + "/api/v3",
        .GraphqlRoot = "https://" + hostname + "/api/graphql",
    };
}

auto expect_failure(
    const github_host_profile_candidate& candidate,
    const github_profile_failure failure,
    const std::string_view message) -> void
{
    const auto result = validate_github_host_profile(candidate);
    expect(!result.has_value() && result.error() == failure, message);
}

auto test_valid_profiles_bind_authority_and_routes() -> void
{
    for(const auto provenance : {
            github_profile_provenance::explicit_user,
            github_profile_provenance::builtin,
            github_profile_provenance::managed,
        })
    {
        auto candidate = github_com_candidate();
        candidate.Provenance = provenance;
        const auto profile = validate_github_host_profile(candidate);
        expect(profile.has_value(), "trusted GitHub.com provenance validates");
        expect(
            profile->profile() == candidate.Profile
                && profile->host() == candidate.Host
                && profile->revision() == candidate.Revision,
            "validated profile retains opaque authority identity");
        expect(
            profile->server_contract() == candidate.Server
                && profile->product_kind()
                    == application::github_product_kind::github_com
                && profile->rest_api_version() == "2022-11-28",
            "GitHub.com contract binds product and REST version");
        expect(
            profile->hostname() == "github.com"
                && profile->rest_authority() == "api.github.com"
                && profile->rest_root() == "https://api.github.com"
                && profile->graphql_root()
                    == "https://api.github.com/graphql"
                && profile->rest_route_prefix().empty()
                && profile->graphql_route() == "/graphql",
            "GitHub.com contract binds exact roots and routes");
        expect(
            profile->authentication_transport()
                    == candidate.Authentication
                && profile->proxy_mode() == candidate.Proxy
                && profile->trust_mode() == candidate.Trust
                && profile->provenance() == provenance,
            "validated profile retains non-secret operational modes");
    }

    const auto candidate = enterprise_candidate();
    const auto profile = validate_github_host_profile(candidate);
    expect(profile.has_value(), "canonical GHES profile validates");
    expect(
        profile->product_kind()
                == application::github_product_kind::
                    github_enterprise_server
            && profile->rest_authority() == "ghe.example.invalid"
            && profile->rest_route_prefix() == "/api/v3"
            && profile->graphql_route() == "/api/graphql"
            && profile->rest_api_version() == "2022-11-28",
        "GHES contract binds exact server version and route namespace");
}

auto test_identity_enum_and_provenance_fail_closed() -> void
{
    expect(
        application::product_kind(
            static_cast<application::github_server_contract>(255U))
            == application::github_product_kind::unknown,
        "malformed server contract has no product kind");
    expect(
        !application::valid(
            application::github_product_kind::unknown),
        "unknown product kind is not usable");

    auto candidate = github_com_candidate();
    candidate.Profile = {};
    expect_failure(
        candidate,
        github_profile_failure::invalid_profile_identity,
        "zero profile identity is rejected");

    candidate = github_com_candidate();
    candidate.Host = {};
    expect_failure(
        candidate,
        github_profile_failure::invalid_host_identity,
        "zero host identity is rejected");

    candidate = github_com_candidate();
    candidate.Revision = {};
    expect_failure(
        candidate,
        github_profile_failure::invalid_profile_revision,
        "zero profile revision is rejected");

    candidate = github_com_candidate();
    candidate.Server =
        static_cast<application::github_server_contract>(255U);
    expect_failure(
        candidate,
        github_profile_failure::invalid_server_contract,
        "malformed server contract is rejected");

    candidate = github_com_candidate();
    candidate.Authentication =
        static_cast<application::github_authentication_transport>(255U);
    expect_failure(
        candidate,
        github_profile_failure::invalid_authentication_transport,
        "malformed authentication transport is rejected");

    candidate = github_com_candidate();
    candidate.Proxy = static_cast<application::github_proxy_mode>(255U);
    expect_failure(
        candidate,
        github_profile_failure::invalid_proxy_mode,
        "malformed proxy mode is rejected");

    candidate = github_com_candidate();
    candidate.Trust = static_cast<application::github_trust_mode>(255U);
    expect_failure(
        candidate,
        github_profile_failure::invalid_trust_mode,
        "malformed trust mode is rejected");

    candidate = github_com_candidate();
    candidate.Provenance = static_cast<github_profile_provenance>(255U);
    expect_failure(
        candidate,
        github_profile_failure::invalid_provenance,
        "malformed profile provenance is rejected");

    for(const auto provenance : {
            github_profile_provenance::provider_content,
            github_profile_provenance::redirect,
        })
    {
        candidate = github_com_candidate();
        candidate.Provenance = provenance;
        const auto original = candidate;
        expect_failure(
            candidate,
            github_profile_failure::untrusted_provenance,
            "content-derived authority provenance is rejected");
        expect(
            candidate == original,
            "rejected provenance cannot mutate its profile candidate");
    }
}

[[nodiscard]] auto maximum_hostname() -> std::string
{
    return std::string(63U, 'a') + "." + std::string(63U, 'b') + "."
        + std::string(63U, 'c') + "." + std::string(61U, 'd');
}

auto test_hostname_bounds_and_aliases_fail_closed() -> void
{
    const auto maximum = maximum_hostname();
    expect(maximum.size() == 253U, "maximum hostname fixture is 253 bytes");
    expect(
        validate_github_host_profile(enterprise_candidate(maximum))
            .has_value(),
        "253-byte FQDN with 63-byte labels validates");

    auto overlong = maximum;
    overlong.push_back('e');
    expect_failure(
        enterprise_candidate(overlong),
        github_profile_failure::invalid_hostname,
        "254-byte hostname is rejected");

    const auto label_at_limit =
        std::string(63U, 'a') + ".example.invalid";
    expect(
        validate_github_host_profile(
            enterprise_candidate(label_at_limit))
            .has_value(),
        "63-byte DNS label validates");
    const auto label_over_limit =
        std::string(64U, 'a') + ".example.invalid";
    expect_failure(
        enterprise_candidate(label_over_limit),
        github_profile_failure::invalid_hostname,
        "64-byte DNS label is rejected");

    const std::vector<std::string> hostile_hostnames{
        "",
        "localhost",
        ".ghe.example.invalid",
        "ghe.example.invalid.",
        "ghe..example.invalid",
        "-ghe.example.invalid",
        "ghe-.example.invalid",
        "GHE.example.invalid",
        "ghe_example.invalid",
        "ghe.example.invalid:443",
        "https://ghe.example.invalid",
        "127.0.0.1",
        "999.999.999.999",
        "[::1]",
        "xn--bcher-kva.example",
        "bücher.example",
        "ghe.example.invalid/path",
        "ghe.example.invalid%2eevil",
        "ghe.example.invalid@evil.invalid",
        "ghe.example.invalid\n.evil",
    };
    for(const auto& hostname : hostile_hostnames)
    {
        expect_failure(
            enterprise_candidate(hostname),
            github_profile_failure::invalid_hostname,
            "hostile hostname alias is rejected");
    }

    for(const auto& reserved : {"github.com", "api.github.com"})
    {
        expect_failure(
            enterprise_candidate(reserved),
            github_profile_failure::invalid_hostname,
            "cloud authority cannot masquerade as GHES");
    }
}

auto test_exact_product_roots_fail_closed() -> void
{
    const std::vector<std::string> invalid_cloud_hosts{
        "api.github.com",
        "www.github.com",
        "github.com.evil.invalid",
        "evil-github.com",
    };
    for(const auto& hostname : invalid_cloud_hosts)
    {
        auto candidate = github_com_candidate();
        candidate.Hostname = hostname;
        expect_failure(
            candidate,
            github_profile_failure::invalid_hostname,
            "GitHub.com contract requires exact hostname");
    }

    const std::vector<std::string> invalid_cloud_rest_roots{
        "http://api.github.com",
        "https://api.github.com/",
        "https://api.github.com:443",
        "https://github.com/api/v3",
        "https://api.github.com.evil.invalid",
        "https://api.github.com@evil.invalid",
        "//api.github.com",
    };
    for(const auto& root : invalid_cloud_rest_roots)
    {
        auto candidate = github_com_candidate();
        candidate.RestRoot = root;
        expect_failure(
            candidate,
            github_profile_failure::invalid_rest_root,
            "GitHub.com REST root alias is rejected");
    }

    const std::vector<std::string> invalid_cloud_graphql_roots{
        "http://api.github.com/graphql",
        "https://api.github.com/graphql/",
        "https://api.github.com:443/graphql",
        "https://api.github.com/api/graphql",
        "https://api.github.com/graphql?query=x",
        "https://api.github.com/graphql#fragment",
        "https://api.github.com.evil.invalid/graphql",
    };
    for(const auto& root : invalid_cloud_graphql_roots)
    {
        auto candidate = github_com_candidate();
        candidate.GraphqlRoot = root;
        expect_failure(
            candidate,
            github_profile_failure::invalid_graphql_root,
            "GitHub.com GraphQL root alias is rejected");
    }

    const std::vector<std::string> invalid_enterprise_rest_roots{
        "http://ghe.example.invalid/api/v3",
        "https://ghe.example.invalid/api/v3/",
        "https://ghe.example.invalid:443/api/v3",
        "https://ghe.example.invalid/api",
        "https://ghe.example.invalid/api/v4",
        "https://api.ghe.example.invalid/api/v3",
        "https://ghe.example.invalid.evil/api/v3",
        "https://ghe.example.invalid@evil.invalid/api/v3",
        "https://ghe.example.invalid/api/../api/v3",
        "https://ghe.example.invalid/api%2fv3",
    };
    for(const auto& root : invalid_enterprise_rest_roots)
    {
        auto candidate = enterprise_candidate();
        candidate.RestRoot = root;
        expect_failure(
            candidate,
            github_profile_failure::invalid_rest_root,
            "GHES REST root alias is rejected");
    }

    const std::vector<std::string> invalid_enterprise_graphql_roots{
        "http://ghe.example.invalid/api/graphql",
        "https://ghe.example.invalid/api/graphql/",
        "https://ghe.example.invalid:443/api/graphql",
        "https://ghe.example.invalid/graphql",
        "https://ghe.example.invalid/api/v3/graphql",
        "https://other.example.invalid/api/graphql",
        "https://ghe.example.invalid/api%2fgraphql",
    };
    for(const auto& root : invalid_enterprise_graphql_roots)
    {
        auto candidate = enterprise_candidate();
        candidate.GraphqlRoot = root;
        expect_failure(
            candidate,
            github_profile_failure::invalid_graphql_root,
            "GHES GraphQL root alias is rejected");
    }

    auto crossed = enterprise_candidate();
    crossed.RestRoot = "https://api.github.com";
    crossed.GraphqlRoot = "https://api.github.com/graphql";
    expect_failure(
        crossed,
        github_profile_failure::invalid_rest_root,
        "cloud roots cannot cross into GHES profile");
}

auto expect_redirect_failure(
    const validated_github_host_profile& profile,
    const github_request_surface surface,
    const std::string_view target,
    const github_redirect_failure failure,
    const std::string_view message) -> void
{
    const auto result =
        validate_github_redirect_target(profile, surface, target);
    expect(!result.has_value() && result.error() == failure, message);
}

auto test_redirect_policy_is_bounded_and_non_mutating() -> void
{
    const auto cloud_result =
        validate_github_host_profile(github_com_candidate());
    const auto enterprise_result =
        validate_github_host_profile(enterprise_candidate());
    expect(
        cloud_result.has_value() && enterprise_result.has_value(),
        "redirect profiles validate");
    const auto cloud = *cloud_result;
    const auto enterprise = *enterprise_result;

    for(const auto target : {
            "https://api.github.com",
            "https://api.github.com/",
            "https://api.github.com/repos/octo/repo/pulls/17",
            "https://api.github.com/search/issues",
        })
    {
        const auto approved = validate_github_redirect_target(
            cloud,
            github_request_surface::rest,
            target);
        expect(
            approved.has_value() && approved->url() == target,
            "same-authority cloud REST redirect is approved exactly");
        const auto copied = *approved;
        expect(
            copied.url() == target && approved->url() == target,
            "approved redirect copies retain the validated target");
    }

    for(const auto target : {
            "https://ghe.example.invalid/api/v3",
            "https://ghe.example.invalid/api/v3/",
            "https://ghe.example.invalid/api/v3/repos/octo/repo",
        })
    {
        const auto approved = validate_github_redirect_target(
            enterprise,
            github_request_surface::rest,
            target);
        expect(
            approved.has_value() && approved->url() == target,
            "same-authority GHES REST redirect stays in namespace");
    }

    expect_redirect_failure(
        cloud,
        static_cast<github_request_surface>(255U),
        "https://api.github.com/user",
        github_redirect_failure::invalid_surface,
        "malformed redirect surface is rejected");
    expect_redirect_failure(
        cloud,
        github_request_surface::graphql,
        "https://api.github.com/graphql",
        github_redirect_failure::redirects_not_permitted,
        "cloud GraphQL redirects are rejected");
    expect_redirect_failure(
        enterprise,
        github_request_surface::graphql,
        "https://ghe.example.invalid/api/graphql",
        github_redirect_failure::redirects_not_permitted,
        "GHES GraphQL redirects are rejected");

    const std::vector<std::pair<std::string, github_redirect_failure>>
        hostile_cloud_targets{
            {"http://api.github.com/user",
             github_redirect_failure::invalid_target},
            {"//api.github.com/user",
             github_redirect_failure::invalid_target},
            {"https://github.com/user",
             github_redirect_failure::authority_mismatch},
            {"https://api.github.com.evil.invalid/user",
             github_redirect_failure::authority_mismatch},
            {"https://evil-api.github.com/user",
             github_redirect_failure::authority_mismatch},
            {"https://api.github.com@evil.invalid/user",
             github_redirect_failure::invalid_target},
            {"https://api.github.com:443/user",
             github_redirect_failure::invalid_target},
            {"https://API.github.com/user",
             github_redirect_failure::authority_mismatch},
            {"https://api.github.com/user?next=x",
             github_redirect_failure::invalid_target},
            {"https://api.github.com/user#fragment",
             github_redirect_failure::invalid_target},
            {"https://api.github.com/%2e%2e/user",
             github_redirect_failure::invalid_target},
            {"https://api.github.com/a/../user",
             github_redirect_failure::invalid_target},
            {"https://api.github.com//user",
             github_redirect_failure::invalid_target},
            {"https://api.github.com/a\\b",
             github_redirect_failure::invalid_target},
        };
    for(const auto& [target, failure] : hostile_cloud_targets)
    {
        expect_redirect_failure(
            cloud,
            github_request_surface::rest,
            target,
            failure,
            "hostile cloud redirect target is rejected");
    }

    for(const auto target : {
            "https://ghe.example.invalid/",
            "https://ghe.example.invalid/api",
            "https://ghe.example.invalid/api/v30/user",
            "https://ghe.example.invalid/graphql",
        })
    {
        expect_redirect_failure(
            enterprise,
            github_request_surface::rest,
            target,
            github_redirect_failure::outside_api_namespace,
            "GHES redirect outside /api/v3 is rejected");
    }
    expect_redirect_failure(
        enterprise,
        github_request_surface::rest,
        "https://other.example.invalid/api/v3/user",
        github_redirect_failure::authority_mismatch,
        "GHES cross-authority redirect is rejected");

    std::string at_limit = "https://api.github.com/";
    at_limit.append(2U * 1024U - at_limit.size(), 'a');
    const auto approved_limit = validate_github_redirect_target(
        cloud,
        github_request_surface::rest,
        at_limit);
    expect(
        approved_limit.has_value()
            && approved_limit->url().size() == 2U * 1024U,
        "redirect target at the byte ceiling validates");

    auto overlong = at_limit;
    overlong.push_back('a');
    expect_redirect_failure(
        cloud,
        github_request_surface::rest,
        overlong,
        github_redirect_failure::target_too_long,
        "redirect target N+1 bound is rejected");

    const auto original_profile = cloud.profile();
    const auto original_host = cloud.host();
    const auto original_root = std::string{cloud.rest_root()};
    static_cast<void>(validate_github_redirect_target(
        cloud,
        github_request_surface::rest,
        "https://evil.invalid/user"));
    expect(
        cloud.profile() == original_profile && cloud.host() == original_host
            && cloud.rest_root() == original_root,
        "redirect rejection cannot mutate validated profile authority");
}
}

auto main() -> int
{
    test_valid_profiles_bind_authority_and_routes();
    test_identity_enum_and_provenance_fail_closed();
    test_hostname_bounds_and_aliases_fail_closed();
    test_exact_product_roots_fail_closed();
    test_redirect_policy_is_bounded_and_non_mutating();

    std::cout << "All GitHub host-profile contract tests passed\n";
    return EXIT_SUCCESS;
}
