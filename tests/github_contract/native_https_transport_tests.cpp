#include "github_wire_request_validation.hpp"
#include "native_https_transport.hpp"

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>
#include <QtLogging>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

namespace
{
using namespace bpr;
using namespace bpr::adapters::github;
using namespace bpr::adapters::github::detail;

using namespace std::chrono_literals;

static_assert(
    std::is_base_of_v<github_wire_transport_port, native_https_transport>);
static_assert(!std::is_copy_constructible_v<native_https_transport>);
static_assert(!std::is_move_constructible_v<native_https_transport>);
static_assert(native_https_maximum_request_body_bytes == 256U * 1024U);
static_assert(native_https_maximum_response_body_bytes == 4U * 1024U * 1024U);
static_assert(native_https_maximum_error_response_body_bytes == 64U * 1024U);
static_assert(native_https_maximum_response_header_bytes == 64U * 1024U);
static_assert(native_https_maximum_redirects == 3U);

class test_failure final
{
public:
    explicit test_failure(const std::string_view message) noexcept
        : Message{message}
    {
    }

    [[nodiscard]] auto message() const noexcept -> std::string_view
    {
        return Message;
    }

private:
    std::string_view Message;
};

void expect(const bool condition, const std::string_view message)
{
    if(!condition)
    {
        throw test_failure{message};
    }
}

void discard_qt_message(
    QtMsgType,
    const QMessageLogContext&,
    const QString&)
{
}

void select_binary_standard_streams()
{
#if defined(_WIN32)
    expect(
        _setmode(_fileno(stdin), _O_BINARY) != -1,
        "binary-stdin");
    expect(
        _setmode(_fileno(stdout), _O_BINARY) != -1,
        "binary-stdout");
    expect(
        _setmode(_fileno(stderr), _O_BINARY) != -1,
        "binary-stderr");
#endif
}

[[nodiscard]] auto profile_candidate(
    const application::github_authentication_transport authentication)
    -> github_host_profile_candidate
{
    return {
        .Profile = core::host_profile_id{11},
        .Host = core::git_host_id{21},
        .Revision = application::github_profile_revision{31},
        .Server =
            application::github_server_contract::
                ghes_3_18_2022_11_28,
        .Authentication = authentication,
        .Proxy = application::github_proxy_mode::system,
        .Trust = application::github_trust_mode::system,
        .Provenance = github_profile_provenance::managed,
        .Hostname = "ghe.bpr.invalid",
        .RestRoot = "https://ghe.bpr.invalid/api/v3",
        .GraphqlRoot = "https://ghe.bpr.invalid/api/graphql",
    };
}

[[nodiscard]] auto session_context(
    const validated_github_host_profile& profile)
    -> core::github_session_request_context
{
    return {
        .Request = core::request_id{101},
        .ApplicationSession = core::application_session_id{102},
        .Profile = profile.profile(),
        .IdentityGeneration = core::github_identity_generation{103},
    };
}

[[nodiscard]] auto pull_request_context(
    const validated_github_host_profile& profile)
    -> core::pull_request_request_context
{
    return {
        .Request = core::request_id{201},
        .ApplicationSession = core::application_session_id{202},
        .Profile = profile.profile(),
        .Host = profile.host(),
        .Account = core::github_account_id{203},
        .Repository =
            core::repository_locator{
                .Owner = "octo-org",
                .Name = "review-repository",
            },
        .Number = core::pull_request_number{17},
        .ExpectedRepository = core::repository_id{204},
        .ExpectedPullRequest = core::pull_request_id{205},
        .Capabilities = core::capability_revision{206},
        .IdentityGeneration = core::github_identity_generation{207},
        .Generation = core::request_generation{208},
    };
}

[[nodiscard]] auto semantic_request(
    const read_operation operation) -> semantic_read_request
{
    semantic_read_request semantic{
        .Server = server_contract::ghes_3_18,
        .Operation = operation,
        .Owner = {},
        .Repository = {},
        .PullRequestNumber = 0U,
        .HeadOid = {},
        .SearchTerms = {},
        .Page = 1U,
        .GraphqlPage = std::nullopt,
    };
    if(operation != read_operation::get_viewer_identity)
    {
        semantic.Owner = "octo-org";
        semantic.Repository = "review-repository";
    }
    if(operation == read_operation::list_review_threads)
    {
        semantic.PullRequestNumber = 17U;
    }
    return semantic;
}

[[nodiscard]] auto wire_request_for(
    const validated_github_host_profile& profile,
    const read_operation operation) -> github_wire_request
{
    const auto wire = compile_read_request(semantic_request(operation));
    expect(wire.has_value(), "canonical-wire-request");
    return {
        .Correlation =
            github_wire_correlation{
                .Parent =
                    operation == read_operation::get_viewer_identity
                    ? github_wire_parent_context{session_context(profile)}
                    : github_wire_parent_context{
                          pull_request_context(profile)},
                .ProfileRevision = profile.revision(),
                .Host = profile.host(),
                .Operation = operation,
                .Page = 1U,
            },
        .Wire = *wire,
    };
}

[[nodiscard]] auto
capability_request_for(const validated_github_host_profile& profile,
                       const core::github_session_request_context& session,
                       const core::github_account_id observed_account)
    -> github_capability_probe_wire_request
{
    const auto wire = compile_capability_probe_request(profile);
    expect(wire.has_value(), "canonical-capability-request");
    return {
        .Correlation =
            github_capability_probe_wire_correlation{
                .Session = session,
                .ProfileRevision = profile.revision(),
                .Host = profile.host(),
                .ObservedAccount = observed_account,
            },
        .Wire = *wire,
    };
}

[[nodiscard]] auto credential_material(const std::string_view value)
    -> application::github_credential_material
{
    const auto characters = std::span<const char>{value.data(), value.size()};
    auto material = application::github_credential_material::create(
        std::as_bytes(characters));
    expect(material.has_value(), "credential-material");
    return std::move(*material);
}

[[nodiscard]] auto
account_operation_context(const validated_github_host_profile& profile,
                          const core::pull_request_request_context& request)
    -> application::github_credential_operation_context
{
    return {
        .Request = core::request_id{301},
        .ApplicationSession = request.ApplicationSession,
        .Authority =
            application::github_credential_authority_key{
                .Profile = profile.profile(),
                .ProfileRevision = profile.revision(),
                .Host = profile.host(),
                .Hostname =
                    application::github_canonical_hostname{
                        profile.hostname()},
                .Account = request.Account,
                .ServerContract = profile.server_contract(),
            },
        .IdentityGeneration = request.IdentityGeneration,
    };
}

[[nodiscard]] auto
account_operation_context(const validated_github_host_profile& profile,
                          const core::github_session_request_context& request,
                          const core::github_account_id account)
    -> application::github_credential_operation_context
{
    return {
        .Request = request.Request,
        .ApplicationSession = request.ApplicationSession,
        .Authority =
            application::github_credential_authority_key{
                .Profile = profile.profile(),
                .ProfileRevision = profile.revision(),
                .Host = profile.host(),
                .Hostname =
                    application::github_canonical_hostname{
                        profile.hostname()},
                .Account = account,
                .ServerContract = profile.server_contract(),
            },
        .IdentityGeneration = request.IdentityGeneration,
    };
}

[[nodiscard]] auto test_configuration() -> native_https_test_configuration
{
    return {
        .ProxyHost = "127.0.0.1",
        .ProxyPort = 9U,
        .FallbackProxyPort = 0U,
        .TrustedCaCertificatePem = {},
        .TotalTimeout = 1s,
        .ResponseBodyLimit = native_https_maximum_response_body_bytes,
    };
}

void expect_unavailable(const github_wire_completion& completion,
                        const github_wire_correlation& correlation,
                        const std::string_view message)
{
    const auto* error = std::get_if<github_wire_error>(&completion);
    expect(error != nullptr && error->Correlation == correlation
               && error->Error.Failure
                      == application::github_provider_failure::unavailable
               && !error->Error.RetryAfterSeconds.has_value(),
           message);
}

void expect_capability_error(
    const github_capability_probe_wire_completion& completion,
    const github_capability_probe_wire_correlation& correlation,
    const application::github_provider_failure error,
    const std::optional<std::uint32_t> retry_after,
    const std::string_view message)
{
    const auto* failure =
        std::get_if<github_capability_probe_wire_error>(&completion);
    expect(failure != nullptr && failure->Correlation == correlation
               && failure->Error.Failure == error
               && failure->Error.RetryAfterSeconds == retry_after,
           message);
}

void test_factory_contracts_and_preflight()
{
    const auto native = validate_github_host_profile(
        profile_candidate(
            application::github_authentication_transport::native_https));
    const auto cli = validate_github_host_profile(
        profile_candidate(
            application::github_authentication_transport::github_cli));
    expect(native.has_value() && cli.has_value(), "profile-validation");

    const auto invalid_runtime = observe_native_https_runtime(*cli);
    expect(
        invalid_runtime == native_https_runtime_facts{},
        "runtime-profile-policy");
    const auto runtime = observe_native_https_runtime(*native);
    if(runtime.TlsSupported)
    {
        expect(
            runtime.TlsBackend
                != native_https_tls_backend::unavailable,
            "runtime-tls-backend");
    }
    if(runtime.ProxyCandidateCount == 0U)
    {
        expect(
            runtime.Proxy
                == native_https_system_proxy_kind::unavailable,
            "runtime-empty-proxy");
    }

    const auto configuration = test_configuration();
    const auto cli_factory =
        make_native_https_bootstrap_transport_for_test(
            *cli,
            session_context(*cli),
            credential_material("BPR0026FixtureValue"),
            configuration);
    expect(
        !cli_factory.has_value()
            && cli_factory.error()
                == native_https_configuration_failure::
                    profile_transport_mismatch,
        "profile-transport-mismatch");

    auto invalid_configuration = configuration;
    invalid_configuration.ProxyHost = "proxy.bpr.invalid";
    const auto configuration_factory =
        make_native_https_bootstrap_transport_for_test(
            *native,
            session_context(*native),
            credential_material("BPR0026FixtureValue"),
            invalid_configuration);
    expect(
        !configuration_factory.has_value()
            && configuration_factory.error()
                == native_https_configuration_failure::
                    invalid_test_configuration,
        "invalid-test-configuration");

    auto duplicate_proxy_configuration = configuration;
    duplicate_proxy_configuration.FallbackProxyPort =
        duplicate_proxy_configuration.ProxyPort;
    const auto duplicate_proxy_factory =
        make_native_https_bootstrap_transport_for_test(
            *native,
            session_context(*native),
            credential_material("BPR0026FixtureValue"),
            duplicate_proxy_configuration);
    expect(
        !duplicate_proxy_factory.has_value()
            && duplicate_proxy_factory.error()
                == native_https_configuration_failure::
                    invalid_test_configuration,
        "duplicate-test-proxy");

    auto invalid_session = session_context(*native);
    invalid_session.Profile = core::host_profile_id{999};
    const auto context_factory =
        make_native_https_bootstrap_transport_for_test(
            *native,
            invalid_session,
            credential_material("BPR0026FixtureValue"),
            configuration);
    expect(
        !context_factory.has_value()
            && context_factory.error()
                == native_https_configuration_failure::
                    invalid_bootstrap_context,
        "invalid-bootstrap-context");

    const auto material_factory =
        make_native_https_bootstrap_transport_for_test(
            *native,
            session_context(*native),
            credential_material("invalid material"),
            configuration);
    expect(
        !material_factory.has_value()
            && material_factory.error()
                == native_https_configuration_failure::
                    invalid_credential_material,
        "invalid-credential-material");

    const auto pull_request = pull_request_context(*native);
    auto mismatched_operation =
        account_operation_context(*native, pull_request);
    mismatched_operation.Authority.Host = core::git_host_id{999};
    const auto authority_factory =
        make_native_https_account_transport_for_test(
            *native,
            application::github_credential_retrieve_success{
                .Context = mismatched_operation,
                .Credential =
                    credential_material("BPR0026FixtureValue"),
            },
            configuration);
    expect(
        !authority_factory.has_value()
            && authority_factory.error()
                == native_https_configuration_failure::
                    credential_authority_mismatch,
        "credential-authority-mismatch");

    auto mismatched_hostname =
        account_operation_context(*native, pull_request);
    mismatched_hostname.Authority.Hostname =
        application::github_canonical_hostname{"other.bpr.invalid"};
    const auto hostname_authority_factory =
        make_native_https_account_transport_for_test(
            *native,
            application::github_credential_retrieve_success{
                .Context = mismatched_hostname,
                .Credential =
                    credential_material("BPR0026FixtureValue"),
            },
            configuration);
    expect(
        !hostname_authority_factory.has_value()
            && hostname_authority_factory.error()
                == native_https_configuration_failure::
                    credential_authority_mismatch,
        "credential-hostname-authority-mismatch");

    auto bootstrap = make_native_https_bootstrap_transport_for_test(
        *native,
        session_context(*native),
        credential_material("BPR0026FixtureValue"),
        configuration);
    expect(bootstrap.has_value(), "bootstrap-transport");

    const auto viewer =
        wire_request_for(*native, read_operation::get_viewer_identity);
    std::stop_source stopped;
    expect(stopped.request_stop(), "pre-cancel-request");
    const auto cancelled =
        (*bootstrap)->perform(viewer, stopped.get_token());
    const auto* cancellation =
        std::get_if<github_wire_cancelled>(&cancelled);
    expect(
        cancellation != nullptr
            && cancellation->Correlation == viewer.Correlation,
        "pre-cancel-correlation");

    auto invalid_wire = viewer;
    invalid_wire.Correlation.ProfileRevision =
        application::github_profile_revision{999};
    const auto invalid_completion =
        (*bootstrap)->perform(invalid_wire, std::stop_token{});
    expect_unavailable(
        invalid_completion,
        invalid_wire.Correlation,
        "invalid-wire-preflight");

    const auto repository =
        wire_request_for(*native, read_operation::get_repository);
    const auto account_parent =
        std::get<core::pull_request_request_context>(
            repository.Correlation.Parent);
    auto account = make_native_https_account_transport_for_test(
        *native,
        application::github_credential_retrieve_success{
            .Context =
                account_operation_context(*native, account_parent),
            .Credential =
                credential_material("BPR0026FixtureValue"),
        },
        configuration);
    expect(account.has_value(), "account-transport");

    auto account_drift = repository;
    auto drift_parent =
        std::get<core::pull_request_request_context>(
            account_drift.Correlation.Parent);
    drift_parent.Account = core::github_account_id{999};
    account_drift.Correlation.Parent = drift_parent;
    const auto drift_completion =
        (*account)->perform(account_drift, std::stop_token{});
    expect_unavailable(
        drift_completion,
        account_drift.Correlation,
        "account-drift-preflight");

    const auto observed_account = core::github_account_id{203};
    const auto session = session_context(*native);
    const auto capability =
        capability_request_for(*native, session, observed_account);
    expect(validate_github_capability_probe_wire_request(*native, capability)
               .has_value(),
           "capability-wire-validation");

    auto mutation = capability;
    mutation.Wire.Body = R"({"query":"mutation { __typename }"})";
    const auto mutation_completion =
        (*account)->perform_capability_probe(mutation, std::stop_token{});
    expect_capability_error(mutation_completion, mutation.Correlation,
                            application::github_provider_failure::unavailable,
                            std::nullopt, "capability-mutation-preflight");

    const auto bootstrap_capability =
        (*bootstrap)->perform_capability_probe(capability, std::stop_token{});
    expect_capability_error(bootstrap_capability, capability.Correlation,
                            application::github_provider_failure::unavailable,
                            std::nullopt, "bootstrap-capability-denied");

    auto capability_account = make_native_https_account_transport_for_test(
        *native,
        application::github_credential_retrieve_success{
            .Context =
                account_operation_context(*native, session, observed_account),
            .Credential = credential_material("BPR0027FixtureValue"),
        },
        configuration);
    expect(capability_account.has_value(), "capability-account-transport");

    auto account_drift_capability = capability;
    account_drift_capability.Correlation.ObservedAccount =
        core::github_account_id{999};
    const auto account_drift_probe =
        (*capability_account)
            ->perform_capability_probe(account_drift_capability,
                                       std::stop_token{});
    expect_capability_error(account_drift_probe,
                            account_drift_capability.Correlation,
                            application::github_provider_failure::unavailable,
                            std::nullopt, "capability-account-drift");

    const auto expect_session_drift =
        [&capability_account](const github_capability_probe_wire_request& drift,
                              const std::string_view message) {
            const auto completion =
                (*capability_account)
                    ->perform_capability_probe(drift, std::stop_token{});
            expect_capability_error(
                completion, drift.Correlation,
                application::github_provider_failure::unavailable, std::nullopt,
                message);
        };
    auto request_drift = capability;
    request_drift.Correlation.Session.Request = core::request_id{999};
    expect_session_drift(request_drift, "capability-session-request-drift");
    auto application_session_drift = capability;
    application_session_drift.Correlation.Session.ApplicationSession =
        core::application_session_id{999};
    expect_session_drift(application_session_drift,
                         "capability-application-session-drift");
    auto identity_generation_drift = capability;
    identity_generation_drift.Correlation.Session.IdentityGeneration =
        core::github_identity_generation{999};
    expect_session_drift(identity_generation_drift,
                         "capability-identity-generation-drift");

    std::stop_source probe_stop;
    expect(probe_stop.request_stop(), "capability-pre-cancel-request");
    const auto capability_cancelled =
        (*capability_account)
            ->perform_capability_probe(capability, probe_stop.get_token());
    const auto* probe_cancellation =
        std::get_if<github_capability_probe_wire_cancelled>(
            &capability_cancelled);
    expect(probe_cancellation != nullptr
               && probe_cancellation->Correlation == capability.Correlation,
           "capability-pre-cancel-correlation");
}

[[nodiscard]] constexpr auto tls_backend_name(
    const native_https_tls_backend backend) noexcept -> std::string_view
{
    switch(backend)
    {
    case native_https_tls_backend::unavailable:
        return "unavailable";
    case native_https_tls_backend::secure_transport:
        return "secure-transport";
    case native_https_tls_backend::openssl:
        return "openssl";
    case native_https_tls_backend::schannel:
        return "schannel";
    case native_https_tls_backend::other:
        return "other";
    }
    return "invalid";
}

[[nodiscard]] constexpr auto proxy_kind_name(
    const native_https_system_proxy_kind proxy) noexcept -> std::string_view
{
    switch(proxy)
    {
    case native_https_system_proxy_kind::unavailable:
        return "unavailable";
    case native_https_system_proxy_kind::direct:
        return "direct";
    case native_https_system_proxy_kind::http:
        return "http";
    case native_https_system_proxy_kind::socks5:
        return "socks5";
    case native_https_system_proxy_kind::unsupported:
        return "unsupported";
    }
    return "invalid";
}

void print_runtime_facts()
{
    const auto profile = validate_github_host_profile(
        profile_candidate(
            application::github_authentication_transport::native_https));
    expect(profile.has_value(), "runtime-profile");
    const auto runtime = observe_native_https_runtime(*profile);
    std::cout
        << "native-https-runtime:tls="
        << tls_backend_name(runtime.TlsBackend)
        << ":proxy=" << proxy_kind_name(runtime.Proxy)
        << ":candidates=" << runtime.ProxyCandidateCount
        << ":supported=" << (runtime.TlsSupported ? "true" : "false")
        << '\n';
}

enum class fixture_scenario : std::uint8_t
{
    success,
    proxy_fallback,
    same_redirect,
    same_query_redirect,
    cross_redirect,
    graphql_redirect,
    untrusted_certificate,
    response_limit,
    header_limit,
    cancel,
    timeout,
    pre_cancel,
    http_401,
    http_403,
    http_403_secondary,
    http_403_secondary_retry,
    http_408,
    http_429,
    http_503_retry,
    http_504,
    graphql_403_rate,
    content_encoding,
    response_metadata,
    duplicate_rate_limit,
    invalid_rate_limit,
    wrong_rate_resource,
    invalid_link,
    graphql_link,
    capability_success,
    capability_redirect,
    capability_403_rate,
    capability_429,
};

[[nodiscard]] auto parse_scenario(const std::string_view value)
    -> std::optional<fixture_scenario>
{
    if(value == "success")
    {
        return fixture_scenario::success;
    }
    if(value == "proxy-fallback")
    {
        return fixture_scenario::proxy_fallback;
    }
    if(value == "same-redirect")
    {
        return fixture_scenario::same_redirect;
    }
    if(value == "same-query-redirect")
    {
        return fixture_scenario::same_query_redirect;
    }
    if(value == "cross-redirect")
    {
        return fixture_scenario::cross_redirect;
    }
    if(value == "graphql-redirect")
    {
        return fixture_scenario::graphql_redirect;
    }
    if(value == "untrusted-certificate")
    {
        return fixture_scenario::untrusted_certificate;
    }
    if(value == "response-limit")
    {
        return fixture_scenario::response_limit;
    }
    if(value == "header-limit")
    {
        return fixture_scenario::header_limit;
    }
    if(value == "cancel")
    {
        return fixture_scenario::cancel;
    }
    if(value == "timeout")
    {
        return fixture_scenario::timeout;
    }
    if(value == "pre-cancel")
    {
        return fixture_scenario::pre_cancel;
    }
    if(value == "http-401")
    {
        return fixture_scenario::http_401;
    }
    if(value == "http-403")
    {
        return fixture_scenario::http_403;
    }
    if(value == "http-403-secondary")
    {
        return fixture_scenario::http_403_secondary;
    }
    if(value == "http-403-secondary-retry")
    {
        return fixture_scenario::http_403_secondary_retry;
    }
    if(value == "http-408")
    {
        return fixture_scenario::http_408;
    }
    if(value == "http-429")
    {
        return fixture_scenario::http_429;
    }
    if(value == "http-503-retry")
    {
        return fixture_scenario::http_503_retry;
    }
    if(value == "http-504")
    {
        return fixture_scenario::http_504;
    }
    if(value == "graphql-403-rate")
    {
        return fixture_scenario::graphql_403_rate;
    }
    if(value == "content-encoding")
    {
        return fixture_scenario::content_encoding;
    }
    if(value == "response-metadata")
    {
        return fixture_scenario::response_metadata;
    }
    if(value == "duplicate-rate-limit")
    {
        return fixture_scenario::duplicate_rate_limit;
    }
    if(value == "invalid-rate-limit")
    {
        return fixture_scenario::invalid_rate_limit;
    }
    if(value == "wrong-rate-resource")
    {
        return fixture_scenario::wrong_rate_resource;
    }
    if(value == "invalid-link")
    {
        return fixture_scenario::invalid_link;
    }
    if(value == "graphql-link")
    {
        return fixture_scenario::graphql_link;
    }
    if(value == "capability-success")
    {
        return fixture_scenario::capability_success;
    }
    if(value == "capability-redirect")
    {
        return fixture_scenario::capability_redirect;
    }
    if(value == "capability-403-rate")
    {
        return fixture_scenario::capability_403_rate;
    }
    if(value == "capability-429")
    {
        return fixture_scenario::capability_429;
    }
    return std::nullopt;
}

[[nodiscard]] constexpr auto
scenario_name(const fixture_scenario scenario) noexcept -> std::string_view
{
    switch(scenario)
    {
    case fixture_scenario::success:
        return "success";
    case fixture_scenario::proxy_fallback:
        return "proxy-fallback";
    case fixture_scenario::same_redirect:
        return "same-redirect";
    case fixture_scenario::same_query_redirect:
        return "same-query-redirect";
    case fixture_scenario::cross_redirect:
        return "cross-redirect";
    case fixture_scenario::graphql_redirect:
        return "graphql-redirect";
    case fixture_scenario::untrusted_certificate:
        return "untrusted-certificate";
    case fixture_scenario::response_limit:
        return "response-limit";
    case fixture_scenario::header_limit:
        return "header-limit";
    case fixture_scenario::cancel:
        return "cancel";
    case fixture_scenario::timeout:
        return "timeout";
    case fixture_scenario::pre_cancel:
        return "pre-cancel";
    case fixture_scenario::http_401:
        return "http-401";
    case fixture_scenario::http_403:
        return "http-403";
    case fixture_scenario::http_403_secondary:
        return "http-403-secondary";
    case fixture_scenario::http_403_secondary_retry:
        return "http-403-secondary-retry";
    case fixture_scenario::http_408:
        return "http-408";
    case fixture_scenario::http_429:
        return "http-429";
    case fixture_scenario::http_503_retry:
        return "http-503-retry";
    case fixture_scenario::http_504:
        return "http-504";
    case fixture_scenario::graphql_403_rate:
        return "graphql-403-rate";
    case fixture_scenario::content_encoding:
        return "content-encoding";
    case fixture_scenario::response_metadata:
        return "response-metadata";
    case fixture_scenario::duplicate_rate_limit:
        return "duplicate-rate-limit";
    case fixture_scenario::invalid_rate_limit:
        return "invalid-rate-limit";
    case fixture_scenario::wrong_rate_resource:
        return "wrong-rate-resource";
    case fixture_scenario::invalid_link:
        return "invalid-link";
    case fixture_scenario::graphql_link:
        return "graphql-link";
    case fixture_scenario::capability_success:
        return "capability-success";
    case fixture_scenario::capability_redirect:
        return "capability-redirect";
    case fixture_scenario::capability_403_rate:
        return "capability-403-rate";
    case fixture_scenario::capability_429:
        return "capability-429";
    }
    return {};
}

[[nodiscard]] auto parse_port(
    const std::string_view value) noexcept -> std::optional<std::uint16_t>
{
    std::uint32_t parsed = 0U;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);
    if(
        result.ec != std::errc{}
        || result.ptr != value.data() + value.size()
        || parsed == 0U
        || parsed > std::numeric_limits<std::uint16_t>::max())
    {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(parsed);
}

[[nodiscard]] auto read_frame_length() -> std::uint32_t
{
    std::array<unsigned char, 4U> bytes{};
    std::cin.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    expect(
        std::cin.gcount()
            == static_cast<std::streamsize>(bytes.size()),
        "fixture-frame-length");
    return (static_cast<std::uint32_t>(bytes[0]) << 24U)
        | (static_cast<std::uint32_t>(bytes[1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[2]) << 8U)
        | static_cast<std::uint32_t>(bytes[3]);
}

[[nodiscard]] auto read_frame(
    const std::size_t maximum,
    const std::string_view limit_message) -> std::string
{
    const auto length = read_frame_length();
    expect(
        static_cast<std::size_t>(length) <= maximum,
        limit_message);
    std::string result(static_cast<std::size_t>(length), '\0');
    if(!result.empty())
    {
        std::cin.read(
            result.data(),
            static_cast<std::streamsize>(result.size()));
        expect(
            std::cin.gcount()
                == static_cast<std::streamsize>(result.size()),
            "fixture-frame-body");
    }
    return result;
}

void wipe(std::string& value) noexcept
{
    volatile char* cursor = value.data();
    for(std::size_t index = 0U; index < value.size(); ++index)
    {
        cursor[index] = '\0';
    }
    value.clear();
}

[[nodiscard]] auto fixture_transport(
    const validated_github_host_profile& profile,
    const github_wire_request& request,
    const std::string_view credential,
    const native_https_test_configuration& configuration)
    -> native_https_transport_factory_result
{
    if(
        request.Wire.Operation
        == read_operation::list_review_threads)
    {
        const auto parent =
            std::get<core::pull_request_request_context>(
                request.Correlation.Parent);
        return make_native_https_account_transport_for_test(
            profile,
            application::github_credential_retrieve_success{
                .Context = account_operation_context(profile, parent),
                .Credential = credential_material(credential),
            },
            configuration);
    }

    const auto parent =
        std::get<core::github_session_request_context>(
            request.Correlation.Parent);
    return make_native_https_bootstrap_transport_for_test(
        profile,
        parent,
        credential_material(credential),
        configuration);
}

[[nodiscard]] auto
fixture_transport(const validated_github_host_profile& profile,
                  const github_capability_probe_wire_request& request,
                  const std::string_view credential,
                  const native_https_test_configuration& configuration)
    -> native_https_transport_factory_result
{
    return make_native_https_account_transport_for_test(
        profile,
        application::github_credential_retrieve_success{
            .Context =
                account_operation_context(profile, request.Correlation.Session,
                                          request.Correlation.ObservedAccount),
            .Credential = credential_material(credential),
        },
        configuration);
}

[[nodiscard]] auto perform_fixture(native_https_transport& transport,
                                   const github_wire_request& request,
                                   const fixture_scenario scenario)
    -> github_wire_completion
{
    if(scenario == fixture_scenario::pre_cancel)
    {
        std::stop_source stop;
        expect(stop.request_stop(), "fixture-pre-cancel-request");
        return transport.perform(request, stop.get_token());
    }
    if(scenario != fixture_scenario::cancel)
    {
        return transport.perform(request, std::stop_token{});
    }

    std::stop_source stop;
    std::optional<github_wire_completion> completion;
    std::exception_ptr worker_failure;
    std::jthread worker{[&transport, &request, &stop, &completion,
                         &worker_failure] {
        try
        {
            completion.emplace(transport.perform(request, stop.get_token()));
        }
        catch(...)
        {
            worker_failure = std::current_exception();
        }
    }};
    std::this_thread::sleep_for(750ms);
    stop.request_stop();
    worker.join();
    expect(worker_failure == nullptr, "fixture-cancel-worker");
    expect(completion.has_value(), "fixture-cancel-completion");
    return std::move(*completion);
}

void expect_success(const github_wire_completion& completion,
                    const github_wire_correlation& correlation,
                    const fixture_scenario scenario)
{
    const auto* success = std::get_if<github_wire_success>(&completion);
    if(success == nullptr)
    {
        if(const auto* error =
               std::get_if<github_wire_error>(&completion);
           error != nullptr)
        {
            switch(error->Error.Failure)
            {
            case application::github_provider_failure::authentication:
                throw test_failure{"fixture-success-authentication"};
            case application::github_provider_failure::permission:
                throw test_failure{"fixture-success-permission"};
            case application::github_provider_failure::network:
                throw test_failure{"fixture-success-network"};
            case application::github_provider_failure::api_version:
                throw test_failure{"fixture-success-api-version"};
            case application::github_provider_failure::unsupported_capability:
                throw test_failure{"fixture-success-unsupported"};
            case application::github_provider_failure::rate_limit:
                throw test_failure{"fixture-success-rate-limit"};
            case application::github_provider_failure::certificate:
                throw test_failure{"fixture-success-certificate"};
            case application::github_provider_failure::redirect:
                throw test_failure{"fixture-success-redirect"};
            case application::github_provider_failure::timeout:
                throw test_failure{"fixture-success-timeout"};
            case application::github_provider_failure::process_startup:
                throw test_failure{"fixture-success-process-startup"};
            case application::github_provider_failure::cli_unavailable:
                throw test_failure{"fixture-success-cli-unavailable"};
            case application::github_provider_failure::vault:
                throw test_failure{"fixture-success-vault"};
            case application::github_provider_failure::malformed_response:
                throw test_failure{"fixture-success-malformed"};
            case application::github_provider_failure::response_limit:
                throw test_failure{"fixture-success-response-limit"};
            case application::github_provider_failure::unavailable:
                throw test_failure{"fixture-success-unavailable"};
            }
        }
        throw test_failure{"fixture-success-cancelled"};
    }
    expect(
        success->Correlation == correlation,
        "fixture-success-correlation");
    expect(
        success->Response.Status == 200,
        "fixture-success-status");
    const auto metadata_headers = std::array<http_header, 7U>{
        http_header{
            .Name = "Content-Type",
            .Value = "Application/JSON; Charset=UTF-8",
        },
        http_header{
            .Name = "Link",
            .Value =
                "<https://ghe.bpr.invalid/api/v3/user?page=2>; rel=\"next\"",
        },
        http_header{
            .Name = "X-RateLimit-Limit",
            .Value = "60",
        },
        http_header{
            .Name = "X-RateLimit-Remaining",
            .Value = "42",
        },
        http_header{
            .Name = "X-RateLimit-Reset",
            .Value = "1999999999",
        },
        http_header{
            .Name = "X-RateLimit-Resource",
            .Value = "core",
        },
        http_header{
            .Name = "Retry-After",
            .Value = "7",
        },
    };
    const auto standard_headers = std::array<http_header, 1U>{
        http_header{
            .Name = "Content-Type",
            .Value = "application/json",
        },
    };
    const auto headers_match =
        scenario == fixture_scenario::response_metadata
        ? std::ranges::equal(success->Response.Headers, metadata_headers)
        : std::ranges::equal(success->Response.Headers, standard_headers);
    expect(headers_match, "fixture-success-headers");
    expect(success->Response.Body == R"({"id":7,"login":"fixture-user"})",
           "fixture-success-body");
    expect(success->Response.Completeness
               == (scenario == fixture_scenario::response_metadata
                       ? page_completeness::has_next_page
                       : page_completeness::complete),
           "fixture-success-completeness");
}

void expect_error(const github_wire_completion& completion,
                  const github_wire_correlation& correlation,
                  const application::github_provider_failure failure,
                  const std::optional<std::uint32_t> retry_after = std::nullopt,
                  const std::optional<application::github_rate_limit_resource>
                      observed_resource = std::nullopt,
                  const bool expect_rate_cause = false,
                  const bool secondary = false)
{
    const auto* error = std::get_if<github_wire_error>(&completion);
    expect(error != nullptr && error->Correlation == correlation
               && error->Error.Failure == failure
               && error->Error.RetryAfterSeconds == retry_after
               && application::valid(error->Error),
           "fixture-error-completion");
    if(!observed_resource.has_value())
    {
        return;
    }
    const auto* observation =
        application::find(error->Error.RateLimits, *observed_resource);
    expect(
        observation != nullptr
            && error->Error.RateLimitCause.has_value() == expect_rate_cause
            && (!expect_rate_cause
                || (error->Error.RateLimitCause->Resource == *observed_resource
                    && error->Error.RateLimitCause->SecondaryLimited
                           == secondary
                    && *observation == *error->Error.RateLimitCause)),
        "fixture-error-rate-resource");
}

void expect_fixture_completion(const github_wire_completion& completion,
                               const github_wire_correlation& correlation,
                               const fixture_scenario scenario)
{
    switch(scenario)
    {
    case fixture_scenario::success:
    case fixture_scenario::proxy_fallback:
    case fixture_scenario::same_redirect:
    case fixture_scenario::response_metadata:
        expect_success(completion, correlation, scenario);
        return;
    case fixture_scenario::cross_redirect:
    case fixture_scenario::graphql_redirect:
    case fixture_scenario::same_query_redirect:
        expect_error(completion, correlation,
                     application::github_provider_failure::redirect);
        return;
    case fixture_scenario::untrusted_certificate:
        expect_error(completion, correlation,
                     application::github_provider_failure::certificate);
        return;
    case fixture_scenario::response_limit:
    case fixture_scenario::header_limit:
        expect_error(completion, correlation,
                     application::github_provider_failure::response_limit);
        return;
    case fixture_scenario::cancel:
    case fixture_scenario::pre_cancel: {
        const auto* cancelled = std::get_if<github_wire_cancelled>(&completion);
        expect(cancelled != nullptr && cancelled->Correlation == correlation,
               "fixture-cancelled-completion");
        return;
    }
    case fixture_scenario::timeout:
        expect_error(completion, correlation,
                     application::github_provider_failure::timeout);
        return;
    case fixture_scenario::http_401:
        expect_error(completion, correlation,
                     application::github_provider_failure::authentication);
        return;
    case fixture_scenario::http_403:
        expect_error(completion, correlation,
                     application::github_provider_failure::permission);
        return;
    case fixture_scenario::http_403_secondary:
        expect_error(completion, correlation,
                     application::github_provider_failure::rate_limit, 60U,
                     application::github_rate_limit_resource::core, true, true);
        return;
    case fixture_scenario::http_403_secondary_retry:
        expect_error(completion, correlation,
                     application::github_provider_failure::rate_limit, 17U,
                     application::github_rate_limit_resource::core, true, true);
        return;
    case fixture_scenario::http_408:
        expect_error(completion, correlation,
                     application::github_provider_failure::timeout);
        return;
    case fixture_scenario::http_429:
        expect_error(completion, correlation,
                     application::github_provider_failure::rate_limit, 7U,
                     application::github_rate_limit_resource::core, true, true);
        return;
    case fixture_scenario::http_503_retry:
        expect_error(completion, correlation,
                     application::github_provider_failure::unavailable, 9U,
                     application::github_rate_limit_resource::core);
        return;
    case fixture_scenario::http_504:
        expect_error(completion, correlation,
                     application::github_provider_failure::timeout);
        return;
    case fixture_scenario::graphql_403_rate:
        expect_error(completion, correlation,
                     application::github_provider_failure::rate_limit, 60U,
                     application::github_rate_limit_resource::graphql, true,
                     true);
        return;
    case fixture_scenario::content_encoding:
    case fixture_scenario::duplicate_rate_limit:
    case fixture_scenario::invalid_rate_limit:
    case fixture_scenario::wrong_rate_resource:
    case fixture_scenario::invalid_link:
    case fixture_scenario::graphql_link:
        expect_error(completion, correlation,
                     application::github_provider_failure::malformed_response);
        return;
    case fixture_scenario::capability_success:
    case fixture_scenario::capability_redirect:
    case fixture_scenario::capability_403_rate:
    case fixture_scenario::capability_429:
        throw test_failure{"capability-scenario-read-completion"};
    }
    throw test_failure{"fixture-scenario"};
}

void expect_capability_fixture_completion(
    const github_capability_probe_wire_completion& completion,
    const github_capability_probe_wire_correlation& correlation,
    const fixture_scenario scenario)
{
    if(scenario == fixture_scenario::capability_redirect)
    {
        expect_capability_error(completion, correlation,
                                application::github_provider_failure::redirect,
                                std::nullopt, "capability-fixture-redirect");
        return;
    }
    if(scenario == fixture_scenario::capability_403_rate
       || scenario == fixture_scenario::capability_429)
    {
        const auto* failure =
            std::get_if<github_capability_probe_wire_error>(&completion);
        expect(failure != nullptr && failure->Correlation == correlation
                   && failure->Error.Failure
                          == application::github_provider_failure::rate_limit
                   && failure->Error.RetryAfterSeconds
                          == std::optional<std::uint32_t>{60U}
                   && failure->Error.RateLimitCause.has_value()
                   && failure->Error.RateLimitCause->Resource
                          == application::github_rate_limit_resource::graphql
                   && failure->Error.RateLimitCause->SecondaryLimited
                   && failure->Error.RateLimits.Graphql
                          == failure->Error.RateLimitCause
                   && application::valid(failure->Error),
               "capability-fixture-rate-limit");
        return;
    }

    const auto* success =
        std::get_if<github_capability_probe_wire_success>(&completion);
    const auto expected_rate_limit_headers =
        std::array<capability_probe_header, 5U>{
            capability_probe_header{
                .Name = "X-RateLimit-Limit",
                .Value = "60",
            },
            capability_probe_header{
                .Name = "X-RateLimit-Remaining",
                .Value = "42",
            },
            capability_probe_header{
                .Name = "X-RateLimit-Reset",
                .Value = "1999999999",
            },
            capability_probe_header{
                .Name = "X-RateLimit-Resource",
                .Value = "graphql",
            },
            capability_probe_header{
                .Name = "Retry-After",
                .Value = "7",
            },
        };
    expect(
      success != nullptr && success->Correlation == correlation &&
          success->Response.Status == 200 && success->Response.Authenticated &&
          success->Response.ContentType == "Application/JSON; Charset=UTF-8" &&
          success->Response.Body == "{}" &&
          success->Response.RateLimitHeaderCount == 5U &&
          success->Response.RateLimitHeaders ==
              expected_rate_limit_headers,
      "capability-fixture-success");
}

void run_fixture(const fixture_scenario scenario,
                 const std::uint16_t proxy_port,
                 const std::uint16_t fallback_proxy_port)
{
    auto framed_credential =
        read_frame(application::maximum_github_credential_bytes,
                   "fixture-credential-limit");
    auto authority_certificate = read_frame(
        native_https_maximum_response_header_bytes, "fixture-authority-limit");
    expect(!framed_credential.empty(), "fixture-credential-empty");
    expect(!authority_certificate.empty(), "fixture-authority-empty");

    const auto profile = validate_github_host_profile(profile_candidate(
        application::github_authentication_transport::native_https));
    expect(profile.has_value(), "fixture-profile");

    auto configuration = native_https_test_configuration{
        .ProxyHost = "127.0.0.1",
        .ProxyPort = proxy_port,
        .FallbackProxyPort = fallback_proxy_port,
        .TrustedCaCertificatePem =
            scenario == fixture_scenario::untrusted_certificate
                ? std::string{}
                : std::move(authority_certificate),
        .TotalTimeout = scenario == fixture_scenario::timeout ? 1s : 5s,
        .ResponseBodyLimit = scenario == fixture_scenario::response_limit
                                 ? 1'024U
                                 : native_https_maximum_response_body_bytes,
    };

    if(scenario == fixture_scenario::capability_success
       || scenario == fixture_scenario::capability_redirect
       || scenario == fixture_scenario::capability_403_rate
       || scenario == fixture_scenario::capability_429)
    {
        const auto request = capability_request_for(
            *profile, session_context(*profile), core::github_account_id{203});
        auto transport = fixture_transport(*profile, request, framed_credential,
                                           configuration);
        wipe(framed_credential);
        expect(transport.has_value(), "capability-fixture-transport");
        const auto completion =
            (*transport)->perform_capability_probe(request, std::stop_token{});
        expect_capability_fixture_completion(completion, request.Correlation,
                                             scenario);
        return;
    }

    const auto operation =
        scenario == fixture_scenario::graphql_redirect
                || scenario == fixture_scenario::graphql_link
                || scenario == fixture_scenario::graphql_403_rate
            ? read_operation::list_review_threads
            : read_operation::get_viewer_identity;
    const auto request = wire_request_for(*profile, operation);
    auto transport =
        fixture_transport(*profile, request, framed_credential, configuration);
    wipe(framed_credential);
    expect(transport.has_value(), "fixture-transport");

    const auto started = std::chrono::steady_clock::now();
    const auto completion = perform_fixture(**transport, request, scenario);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    if(scenario == fixture_scenario::timeout)
    {
        expect(elapsed >= 500ms && elapsed < 4s, "fixture-timeout-window");
    }
    expect_fixture_completion(completion, request.Correlation, scenario);
    if(scenario != fixture_scenario::graphql_redirect
       && scenario != fixture_scenario::graphql_link
       && scenario != fixture_scenario::graphql_403_rate
       && scenario != fixture_scenario::pre_cancel)
    {
        const auto replay = (*transport)->perform(request, std::stop_token{});
        expect_unavailable(replay, request.Correlation,
                           "fixture-bootstrap-one-shot");
    }
}
} // namespace

auto main(int argument_count, char* arguments[]) -> int
{
    try
    {
        select_binary_standard_streams();
        qInstallMessageHandler(discard_qt_message);
        QCoreApplication application{argument_count, arguments};
        static_cast<void>(application);

        if(
            argument_count == 2
            && std::string_view{arguments[1]} == "--contract")
        {
            test_factory_contracts_and_preflight();
            std::cout << "native-https-transport:contract:pass\n";
            return EXIT_SUCCESS;
        }
        if(
            argument_count == 2
            && std::string_view{arguments[1]} == "--runtime")
        {
            print_runtime_facts();
            return EXIT_SUCCESS;
        }

        expect(
            argument_count == 5 || argument_count == 7,
            "arguments");
        expect(
            std::string_view{arguments[1]} == "--fixture"
                && std::string_view{arguments[3]} == "--proxy-port",
            "fixture-arguments");
        const auto scenario =
            parse_scenario(std::string_view{arguments[2]});
        const auto proxy_port =
            parse_port(std::string_view{arguments[4]});
        const auto fallback_proxy_port =
            argument_count == 7
            ? parse_port(std::string_view{arguments[6]})
            : std::optional<std::uint16_t>{};
        expect(
            scenario.has_value() && proxy_port.has_value(),
            "fixture-selection");
        expect(
            (
                *scenario == fixture_scenario::proxy_fallback
                && argument_count == 7
                && std::string_view{arguments[5]}
                    == "--fallback-proxy-port"
                && fallback_proxy_port.has_value())
            || (
                *scenario != fixture_scenario::proxy_fallback
                && argument_count == 5),
            "fixture-proxy-selection");
        run_fixture(
            *scenario,
            *proxy_port,
            fallback_proxy_port.value_or(0U));
        std::cout
            << "native-https-transport:"
            << scenario_name(*scenario)
            << ":pass\n";
        return EXIT_SUCCESS;
    }
    catch(const test_failure& failure)
    {
        std::cerr
            << "native-https-transport:fail:"
            << failure.message()
            << '\n';
        return EXIT_FAILURE;
    }
    catch(...)
    {
        std::cerr << "native-https-transport:fail:internal\n";
        return EXIT_FAILURE;
    }
}
