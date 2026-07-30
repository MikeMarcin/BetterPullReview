#include "github_provider.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace bpr;
using namespace bpr::adapters::github;
using namespace bpr::adapters::github::detail;

[[noreturn]] void fail(const std::string_view message)
{
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(const bool condition, const std::string_view message)
{
    if(!condition)
    {
        fail(message);
    }
}

template <typename Alternative, typename Completion>
[[nodiscard]] auto require_one(const std::vector<Completion>& completions,
                               const std::string_view message)
    -> const Alternative&
{
    if(completions.size() != 1U)
    {
        fail(message);
    }
    const auto* value = std::get_if<Alternative>(&completions.front());
    if(value == nullptr)
    {
        fail(message);
    }
    return *value;
}

class controlled_executor final : public application::effect_executor
{
public:
    void post(std::function<void()> task) override
    {
        ++PostCalls;
        if(ThrowOnPost)
        {
            if(RetainTaskOnThrow)
            {
                Tasks.push_back(std::move(task));
            }
            throw std::runtime_error{"post failure"};
        }
        Tasks.push_back(std::move(task));
    }

    [[nodiscard]] auto pending() const noexcept -> std::size_t
    {
        return Tasks.size();
    }

    void run_next()
    {
        expect(!Tasks.empty(), "executor has a pending task");
        auto task = std::move(Tasks.front());
        Tasks.pop_front();
        task();
    }

    bool ThrowOnPost = false;
    bool RetainTaskOnThrow = false;
    std::size_t PostCalls = 0U;

private:
    std::deque<std::function<void()>> Tasks;
};

class fake_wire_transport final : public github_wire_transport_port
{
public:
    using read_handler = std::function<github_wire_completion(
        const github_wire_request&, std::stop_token)>;
    using capability_handler =
        std::function<github_capability_probe_wire_completion(
            const github_capability_probe_wire_request&, std::stop_token)>;

    [[nodiscard]] auto perform(const github_wire_request& request,
                               const std::stop_token stop)
        -> github_wire_completion override
    {
        ReadRequests.push_back(request);
        ReadStopRequested.push_back(stop.stop_requested());
        if(ReadHandler)
        {
            return ReadHandler(request, stop);
        }
        return github_wire_error{
            .Correlation = request.Correlation,
            .Error =
                {
                    .Failure =
                        application::github_provider_failure::unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    }

    [[nodiscard]] auto perform_capability_probe(
        const github_capability_probe_wire_request& request,
        const std::stop_token stop)
        -> github_capability_probe_wire_completion override
    {
        CapabilityRequests.push_back(request);
        CapabilityStopRequested.push_back(stop.stop_requested());
        if(CapabilityHandler)
        {
            return CapabilityHandler(request, stop);
        }
        return github_capability_probe_wire_error{
            .Correlation = request.Correlation,
            .Error =
                {
                    .Failure =
                        application::github_provider_failure::unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    }

    read_handler ReadHandler;
    capability_handler CapabilityHandler;
    std::vector<github_wire_request> ReadRequests;
    std::vector<bool> ReadStopRequested;
    std::vector<github_capability_probe_wire_request> CapabilityRequests;
    std::vector<bool> CapabilityStopRequested;
};

class fake_transport_factory final : public github_wire_transport_factory_port
{
public:
    [[nodiscard]] auto
    make_bootstrap(const validated_github_host_profile& profile,
                   const core::github_session_request_context context,
                   application::github_credential_material credential) noexcept
        -> github_wire_transport_factory_result override
    {
        ++BootstrapCalls;
        LastBootstrapProfile = profile.profile();
        LastBootstrapContext = context;
        LastBootstrapCredentialBytes = credential.size();
        if(BootstrapTransport == nullptr)
        {
            return std::unexpected{application::github_provider_error{
                .Failure = application::github_provider_failure::unavailable,
                .RetryAfterSeconds = std::nullopt,
                .RateLimits = {},
                .RateLimitCause = std::nullopt,
            }};
        }
        return BootstrapTransport;
    }

    [[nodiscard]] auto make_account(
        const validated_github_host_profile& profile,
        application::github_credential_retrieve_success credential) noexcept
        -> github_wire_transport_factory_result override
    {
        ++AccountCalls;
        LastAccountProfile = profile.profile();
        LastAccountContext = credential.Context;
        LastAccountCredentialBytes = credential.Credential.size();
        if(AccountTransport == nullptr)
        {
            return std::unexpected{application::github_provider_error{
                .Failure = application::github_provider_failure::unavailable,
                .RetryAfterSeconds = std::nullopt,
                .RateLimits = {},
                .RateLimitCause = std::nullopt,
            }};
        }
        return AccountTransport;
    }

    std::shared_ptr<github_wire_transport_port> BootstrapTransport;
    std::shared_ptr<github_wire_transport_port> AccountTransport;
    std::size_t BootstrapCalls = 0U;
    std::size_t AccountCalls = 0U;
    std::optional<core::host_profile_id> LastBootstrapProfile;
    std::optional<core::github_session_request_context> LastBootstrapContext;
    std::size_t LastBootstrapCredentialBytes = 0U;
    std::optional<core::host_profile_id> LastAccountProfile;
    std::optional<application::github_credential_operation_context>
        LastAccountContext;
    std::size_t LastAccountCredentialBytes = 0U;
};

[[nodiscard]] auto
test_profile(const application::github_authentication_transport authentication)
    -> validated_github_host_profile
{
    const auto profile =
        validate_github_host_profile(github_host_profile_candidate{
            .Profile = core::host_profile_id{11U},
            .Host = core::git_host_id{22U},
            .Revision = application::github_profile_revision{3U},
            .Server =
                application::github_server_contract::github_com_2022_11_28,
            .Authentication = authentication,
            .Proxy = application::github_proxy_mode::system,
            .Trust = application::github_trust_mode::system,
            .Provenance = github_profile_provenance::explicit_user,
            .Hostname = "github.com",
            .RestRoot = "https://api.github.com",
            .GraphqlRoot = "https://api.github.com/graphql",
        });
    expect(profile.has_value(), "provider test profile validates");
    return *profile;
}

[[nodiscard]] auto
test_enterprise_profile(
    const application::github_authentication_transport authentication,
    const core::host_profile_id profile_identity =
        core::host_profile_id{12U})
    -> validated_github_host_profile
{
    const auto profile =
        validate_github_host_profile(github_host_profile_candidate{
            .Profile = profile_identity,
            .Host = core::git_host_id{23U},
            .Revision = application::github_profile_revision{4U},
            .Server =
                application::github_server_contract::ghes_3_18_2022_11_28,
            .Authentication = authentication,
            .Proxy = application::github_proxy_mode::system,
            .Trust = application::github_trust_mode::system,
            .Provenance = github_profile_provenance::managed,
            .Hostname = "ghe.example.invalid",
            .RestRoot = "https://ghe.example.invalid/api/v3",
            .GraphqlRoot = "https://ghe.example.invalid/api/graphql",
        });
    expect(profile.has_value(), "enterprise provider test profile validates");
    return *profile;
}

[[nodiscard]] auto
profile_authority_for(const validated_github_host_profile& profile)
    -> application::github_profile_authority
{
    return {
        .Profile = profile.profile(),
        .ProfileRevision = profile.revision(),
        .Host = profile.host(),
        .Hostname =
            application::github_canonical_hostname{profile.hostname()},
        .ServerContract = profile.server_contract(),
        .Authentication = profile.authentication_transport(),
        .ProbeContractRevision =
            application::github_probe_contract_revision{1U},
    };
}

[[nodiscard]] auto session_context(const validated_github_host_profile& profile)
    -> core::github_session_request_context
{
    return {
        .Request = core::request_id{101U},
        .ApplicationSession = core::application_session_id{102U},
        .Profile = profile.profile(),
        .IdentityGeneration = core::github_identity_generation{103U},
    };
}

[[nodiscard]] auto viewer_record() -> application::github_viewer_record
{
    return {
        .Account = core::github_account_id{1001U},
        .Node = application::github_actor_id{"U_viewer"},
        .Login = "octo-reviewer",
        .DisplayName = "Octo Reviewer",
    };
}

[[nodiscard]] auto
viewer_request_for(const validated_github_host_profile& profile)
    -> application::github_viewer_request
{
    return {
        .Context = session_context(profile),
        .Authority = profile_authority_for(profile),
        .Attempt = application::github_provider_attempt{1U},
    };
}

[[nodiscard]] auto credential_material()
    -> application::github_credential_material
{
    constexpr std::string_view value = "BPR0027ProviderFixtureValue";
    const auto characters = std::span<const char>{value.data(), value.size()};
    auto material = application::github_credential_material::create(
        std::as_bytes(characters));
    expect(material.has_value(), "provider test credential material");
    return std::move(*material);
}

[[nodiscard]] auto
credential_context_for(const validated_github_host_profile& profile,
                       const core::github_session_request_context& context,
                       const core::github_account_id account)
    -> application::github_credential_operation_context
{
    return {
        .Request = context.Request,
        .ApplicationSession = context.ApplicationSession,
        .Authority =
            {
                .Profile = profile.profile(),
                .ProfileRevision = profile.revision(),
                .Host = profile.host(),
                .Hostname =
                    application::github_canonical_hostname{
                        profile.hostname()},
                .Account = account,
                .ServerContract = profile.server_contract(),
            },
        .IdentityGeneration = context.IdentityGeneration,
    };
}

[[nodiscard]] auto response_headers() -> std::vector<http_header>
{
    return {
        {
            .Name = "Content-Type",
            .Value = "application/json; charset=utf-8",
        },
        {
            .Name = "X-RateLimit-Limit",
            .Value = "5000",
        },
        {
            .Name = "X-RateLimit-Remaining",
            .Value = "4999",
        },
        {
            .Name = "X-RateLimit-Reset",
            .Value = "2000000000",
        },
        {
            .Name = "X-RateLimit-Resource",
            .Value = "core",
        },
    };
}

[[nodiscard]] auto viewer_response() -> wire_response
{
    return {
        .Status = 200,
        .Headers = response_headers(),
        .Body =
            R"({"id":1001,"login":"octo-reviewer","name":"Octo Reviewer","node_id":"U_viewer"})",
        .Completeness = page_completeness::complete,
    };
}

[[nodiscard]] auto capability_fixture_root() -> std::filesystem::path
{
#if defined(BPR_GITHUB_CAPABILITY_FIXTURE_ROOT)
    return BPR_GITHUB_CAPABILITY_FIXTURE_ROOT;
#else
    return std::filesystem::path{__FILE__}.parent_path().parent_path()
           / "fixtures/github/capabilities/v1";
#endif
}

[[nodiscard]] auto read_file(const std::filesystem::path& path) -> std::string
{
    std::ifstream input{path, std::ios::binary};
    expect(input.good(), "provider capability fixture opens");
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

[[nodiscard]] auto supported_capability_response() -> capability_probe_response
{
    auto response = capability_probe_response{
        .Status = 200,
        .Authenticated = true,
        .ContentType = "application/json",
        .Body =
            read_file(capability_fixture_root() / "github-com/supported.json"),
        .RateLimitHeaders = {},
        .RateLimitHeaderCount = 4U,
    };
    response.RateLimitHeaders[0U] = {
        .Name = "X-RateLimit-Limit",
        .Value = "5000",
    };
    response.RateLimitHeaders[1U] = {
        .Name = "X-RateLimit-Remaining",
        .Value = "4999",
    };
    response.RateLimitHeaders[2U] = {
        .Name = "X-RateLimit-Reset",
        .Value = "2000000000",
    };
    response.RateLimitHeaders[3U] = {
        .Name = "X-RateLimit-Resource",
        .Value = "graphql",
    };
    return response;
}

struct provider_fixture
{
    explicit provider_fixture(
        const application::github_authentication_transport authentication)
        : Profile{test_profile(authentication)}
        , Transport{std::make_shared<fake_wire_transport>()}
        , Factory{std::make_shared<fake_transport_factory>()}
    {
        Factory->BootstrapTransport = Transport;
        Factory->AccountTransport = Transport;
        auto provider = make_github_provider(
            std::span<const validated_github_host_profile>{&Profile, 1U},
            Effects,
            Factory);
        expect(provider.has_value(), "concrete provider constructs");
        Provider = std::move(*provider);
    }

    validated_github_host_profile Profile;
    controlled_executor Effects;
    std::shared_ptr<fake_wire_transport> Transport;
    std::shared_ptr<fake_transport_factory> Factory;
    std::unique_ptr<github_provider> Provider;
};

[[nodiscard]] auto run_viewer(provider_fixture& fixture,
                              application::github_viewer_submission submission)
    -> application::github_viewer_completion
{
    auto completions = std::vector<application::github_viewer_completion>{};
    fixture.Provider->observe_viewer(
        std::move(submission),
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    expect(completions.empty() && fixture.Effects.pending() == 1U,
           "viewer work is queued once");
    fixture.Effects.run_next();
    expect(completions.size() == 1U && fixture.Effects.pending() == 0U,
           "viewer work completes exactly once");
    return std::move(completions.front());
}

[[nodiscard]] auto
run_capability(provider_fixture& fixture,
               application::github_capability_request request)
    -> application::github_capability_completion
{
    auto completions = std::vector<application::github_capability_completion>{};
    fixture.Provider->probe_capabilities(
        std::move(request),
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    expect(completions.empty() && fixture.Effects.pending() == 1U,
           "capability work is queued once");
    fixture.Effects.run_next();
    expect(completions.size() == 1U && fixture.Effects.pending() == 0U,
           "capability work completes exactly once");
    return std::move(completions.front());
}

[[nodiscard]] auto account_lease(provider_fixture& fixture)
    -> application::github_account_transport
{
    const auto request = viewer_request_for(fixture.Profile);
    const auto context = credential_context_for(
        fixture.Profile, request.Context, viewer_record().Account);
    fixture.Transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        return github_wire_success{
            .Correlation = wire.Correlation,
            .Response = viewer_response(),
        };
    };
    auto completion =
        run_viewer(fixture,
                   application::github_viewer_submission{
                       .Request = request,
                       .Authentication =
                           application::github_credential_retrieve_success{
                               .Context = context,
                               .Credential = credential_material(),
                           },
                   });
    const auto* success =
        std::get_if<application::github_viewer_success>(&completion);
    expect(success != nullptr && success->Request == request
               && success->Viewer == viewer_record()
               && success->AccountTransport != nullptr,
           "retrieved-account viewer returns an exact account lease");
    expect(fixture.Factory->AccountCalls == 1U
               && fixture.Factory->LastAccountProfile
                      == std::optional{fixture.Profile.profile()}
               && fixture.Factory->LastAccountContext == std::optional{context}
               && fixture.Factory->LastAccountCredentialBytes != 0U,
           "account factory receives the exact credential authority");
    return success->AccountTransport;
}

[[nodiscard]] auto
capability_request_for(const validated_github_host_profile& profile,
                       application::github_account_transport lease)
    -> application::github_capability_request
{
    return {
        .Context = session_context(profile),
        .ProfileAuthority = profile_authority_for(profile),
        .Viewer = viewer_record(),
        .AccountTransport = std::move(lease),
        .Attempt = application::github_provider_attempt{1U},
    };
}

[[nodiscard]] auto
capability_request_for(const provider_fixture& fixture,
                       application::github_account_transport lease)
    -> application::github_capability_request
{
    return capability_request_for(fixture.Profile, std::move(lease));
}

[[nodiscard]] auto
conclusive_observation(const validated_github_host_profile& profile)
    -> application::github_capability_probe_observation
{
    auto observation = application::github_capability_probe_observation{
        .Authority =
            {
                .Profile = profile.profile(),
                .ProfileRevision = profile.revision(),
                .Host = profile.host(),
                .Account = viewer_record().Account,
                .ServerContract = profile.server_contract(),
                .ProbeContractRevision =
                    application::github_probe_contract_revision{1U},
            },
        .ObservedAt = application::github_freshness_epoch{100U},
        .Facts = {},
        .SchemaFacts = {},
    };
    for(std::size_t index = 0U; index < observation.Facts.size(); ++index)
    {
        observation.Facts[index] = {
            .Capability = application::canonical_github_capabilities[index],
            .State = core::github_capability_state::unsupported,
            .Source = application::github_capability_evidence_source::
                observed_contract,
        };
    }
    for(std::size_t index = 0U; index < observation.SchemaFacts.size(); ++index)
    {
        observation.SchemaFacts[index] = {
            .Capability =
                application::canonical_github_schema_capabilities[index],
            .State = core::github_capability_state::unsupported,
            .Source = application::github_capability_evidence_source::
                observed_contract,
        };
    }
    expect(application::valid(observation),
           "baseline capability observation is valid");
    return observation;
}

[[nodiscard]] auto cache_entry_for(const validated_github_host_profile& profile)
    -> application::github_capability_cache_entry
{
    const auto observation = conclusive_observation(profile);
    return {
        .Authority = observation.Authority,
        .Facts = observation.Facts,
        .SchemaFacts = observation.SchemaFacts,
        .Capabilities = application::projected_capabilities(observation),
        .ReadShape = application::projected_read_shape(observation),
        .Revision = core::capability_revision{44U},
        .ObservedAt = observation.ObservedAt,
        .ExpiresBefore = application::github_freshness_epoch{400U},
    };
}

[[nodiscard]] auto
pull_request_for(const validated_github_host_profile& profile,
                 application::github_account_transport lease)
    -> application::github_pull_request_provider_request
{
    const auto session = session_context(profile);
    return {
        .Context =
            {
                .Request = core::request_id{201U},
                .ApplicationSession = session.ApplicationSession,
                .Profile = profile.profile(),
                .Host = profile.host(),
                .Account = viewer_record().Account,
                .Repository =
                    {
                        .Owner = "octo-org",
                        .Name = "review-repository",
                    },
                .Number = core::pull_request_number{17U},
                .ExpectedRepository = std::nullopt,
                .ExpectedPullRequest = std::nullopt,
                .Capabilities = core::capability_revision{44U},
                .IdentityGeneration = session.IdentityGeneration,
                .Generation = core::request_generation{9U},
            },
        .ProfileAuthority = profile_authority_for(profile),
        .Viewer = viewer_record(),
        .CapabilityEvidence = cache_entry_for(profile),
        .AccountTransport = std::move(lease),
        .Attempt = application::github_provider_attempt{1U},
    };
}

[[nodiscard]] auto pull_request_for(const provider_fixture& fixture,
                                    application::github_account_transport lease)
    -> application::github_pull_request_provider_request
{
    return pull_request_for(fixture.Profile, std::move(lease));
}

void test_profile_set_validation()
{
    auto effects = controlled_executor{};
    auto factory = std::make_shared<fake_transport_factory>();
    const auto empty = make_github_provider(
        std::span<const validated_github_host_profile>{}, effects, factory);
    expect(!empty.has_value()
               && empty.error()
                      == github_provider_configuration_failure::no_profiles,
           "provider rejects an empty profile set");

    const auto github = test_profile(
        application::github_authentication_transport::native_https);
    const auto enterprise_duplicate = test_enterprise_profile(
        application::github_authentication_transport::native_https,
        github.profile());
    const std::array<validated_github_host_profile, 2U> duplicate_profiles{
        github,
        enterprise_duplicate,
    };
    const auto duplicate =
        make_github_provider(duplicate_profiles, effects, factory);
    expect(
        !duplicate.has_value()
            && duplicate.error()
                   == github_provider_configuration_failure::duplicate_profile
            && effects.PostCalls == 0U && factory->BootstrapCalls == 0U
            && factory->AccountCalls == 0U,
        "provider rejects duplicate profile identities before any effect");
}

void test_multi_profile_routing_and_authority_rejection()
{
    const auto github = test_profile(
        application::github_authentication_transport::native_https);
    const auto enterprise = test_enterprise_profile(
        application::github_authentication_transport::native_https);
    std::array<validated_github_host_profile, 2U> profiles{
        github,
        enterprise,
    };
    auto effects = controlled_executor{};
    auto transport = std::make_shared<fake_wire_transport>();
    auto factory = std::make_shared<fake_transport_factory>();
    factory->BootstrapTransport = transport;
    factory->AccountTransport = transport;
    auto provider = make_github_provider(profiles, effects, factory);
    expect(provider.has_value(), "multi-profile provider constructs");
    const auto replacement = test_enterprise_profile(
        application::github_authentication_transport::native_https,
        github.profile());
    profiles.front() = replacement;

    transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        return github_wire_success{
            .Correlation = wire.Correlation,
            .Response = viewer_response(),
        };
    };
    const auto verify_viewer_route =
        [&](const validated_github_host_profile& profile,
            const server_contract server)
    {
        const auto request = viewer_request_for(profile);
        auto completions =
            std::vector<application::github_viewer_completion>{};
        (*provider)->observe_viewer(
            application::github_viewer_submission{
                .Request = request,
                .Authentication =
                    application::github_bootstrap_credential{
                        .Credential = credential_material(),
                    },
            },
            [&completions](auto completion)
            { completions.push_back(std::move(completion)); });
        expect(completions.empty() && effects.pending() == 1U,
               "selected viewer route is queued");
        effects.run_next();
        const auto& success =
            require_one<application::github_viewer_success>(
                completions, "selected viewer route succeeds once");
        const auto expected = compile_read_request(semantic_read_request{
            .Server = server,
            .Operation = read_operation::get_viewer_identity,
            .Owner = {},
            .Repository = {},
            .PullRequestNumber = 0U,
            .HeadOid = {},
            .SearchTerms = {},
            .Page = 1U,
            .GraphqlPage = std::nullopt,
        });
        expect(expected.has_value(), "selected viewer route compiles");
        const auto& wire = transport->ReadRequests.back();
        expect(
            success.Request == request && success.Viewer == viewer_record()
                && factory->LastBootstrapProfile
                       == std::optional{profile.profile()}
                && wire.Wire == *expected
                && wire.Correlation.ProfileRevision == profile.revision()
                && wire.Correlation.Host == profile.host(),
            "viewer job captures the exact selected immutable profile");
    };
    verify_viewer_route(github, server_contract::github_com);
    verify_viewer_route(enterprise, server_contract::ghes_3_18);

    const auto account_request = viewer_request_for(enterprise);
    const auto account_context = credential_context_for(
        enterprise, account_request.Context, viewer_record().Account);
    auto account_completions =
        std::vector<application::github_viewer_completion>{};
    (*provider)->observe_viewer(
        application::github_viewer_submission{
            .Request = account_request,
            .Authentication =
                application::github_credential_retrieve_success{
                    .Context = account_context,
                    .Credential = credential_material(),
                },
        },
        [&account_completions](auto completion)
        { account_completions.push_back(std::move(completion)); });
    expect(account_completions.empty() && effects.pending() == 1U,
           "enterprise account route is queued");
    effects.run_next();
    const auto& account_success =
        require_one<application::github_viewer_success>(
            account_completions, "enterprise account route succeeds once");
    expect(
        account_success.AccountTransport != nullptr
            && factory->LastAccountProfile
                   == std::optional{enterprise.profile()},
        "account job captures the exact enterprise profile");
    const auto enterprise_lease = account_success.AccountTransport;

    auto capability_response = supported_capability_response();
    capability_response.Body = read_file(
        capability_fixture_root() / "ghes-3.18/supported.json");
    transport->CapabilityHandler =
        [capability_response](
            const github_capability_probe_wire_request& wire,
            std::stop_token)
    {
        return github_capability_probe_wire_success{
            .Correlation = wire.Correlation,
            .Response = capability_response,
        };
    };
    const auto capability_request =
        capability_request_for(enterprise, enterprise_lease);
    auto capability_completions =
        std::vector<application::github_capability_completion>{};
    (*provider)->probe_capabilities(
        capability_request,
        [&capability_completions](auto completion)
        { capability_completions.push_back(std::move(completion)); });
    expect(capability_completions.empty() && effects.pending() == 1U,
           "enterprise capability route is queued");
    effects.run_next();
    const auto& capability_success =
        require_one<application::github_capability_success>(
            capability_completions,
            "enterprise capability route succeeds once");
    const auto expected_capability =
        compile_capability_probe_request(enterprise);
    expect(
        expected_capability.has_value()
            && capability_success.Request == capability_request
            && transport->CapabilityRequests.back().Wire
                   == *expected_capability
            && transport->CapabilityRequests.back().Correlation.Host
                   == enterprise.host(),
        "capability job captures the exact enterprise profile");

    const auto pull_request =
        pull_request_for(enterprise, enterprise_lease);
    transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        return github_wire_cancelled{
            .Correlation = wire.Correlation,
        };
    };
    const auto reads_before_pull = transport->ReadRequests.size();
    auto pull_completions =
        std::vector<application::github_pull_request_completion>{};
    (*provider)->fetch_pull_request(
        pull_request,
        [&pull_completions](auto completion)
        { pull_completions.push_back(std::move(completion)); });
    expect(pull_completions.empty() && effects.pending() == 1U,
           "enterprise pull-request route is queued");
    effects.run_next();
    static_cast<void>(
        require_one<application::github_pull_request_cancelled>(
            pull_completions,
            "enterprise pull-request transport cancellation is final"));
    const auto expected_repository = compile_read_request(
        semantic_read_request{
            .Server = server_contract::ghes_3_18,
            .Operation = read_operation::get_repository,
            .Owner = pull_request.Context.Repository.Owner,
            .Repository = pull_request.Context.Repository.Name,
            .PullRequestNumber = 0U,
            .HeadOid = {},
            .SearchTerms = {},
            .Page = 1U,
            .GraphqlPage = std::nullopt,
        });
    expect(
        expected_repository.has_value()
            && transport->ReadRequests.size() == reads_before_pull + 1U
            && transport->ReadRequests.back().Wire == *expected_repository
            && transport->ReadRequests.back().Correlation.Host
                   == enterprise.host(),
        "pull-request job captures the exact enterprise profile");

    const auto effects_before_rejection = effects.PostCalls;
    const auto reads_before_rejection = transport->ReadRequests.size();
    const auto factory_before_rejection = factory->BootstrapCalls;
    auto unknown = viewer_request_for(github);
    unknown.Context.Profile = core::host_profile_id{999U};
    unknown.Authority.Profile = unknown.Context.Profile;
    auto completions = std::vector<application::github_viewer_completion>{};
    (*provider)->observe_viewer(
        application::github_viewer_submission{
            .Request = unknown,
            .Authentication =
                application::github_bootstrap_credential{
                    .Credential = credential_material(),
                },
        },
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    const auto& unknown_error = require_one<application::github_viewer_error>(
        completions, "unknown profile authority is rejected once");
    expect(
        unknown_error.Error.Failure
                == application::github_provider_failure::unavailable
            && effects.PostCalls == effects_before_rejection
            && transport->ReadRequests.size() == reads_before_rejection
            && factory->BootstrapCalls == factory_before_rejection,
        "unknown profile authority cannot reach transport selection");

    auto mismatched = viewer_request_for(enterprise);
    mismatched.Authority.ProfileRevision = github.revision();
    completions.clear();
    (*provider)->observe_viewer(
        application::github_viewer_submission{
            .Request = mismatched,
            .Authentication =
                application::github_bootstrap_credential{
                    .Credential = credential_material(),
                },
        },
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    const auto& mismatch_error = require_one<application::github_viewer_error>(
        completions, "mismatched profile authority is rejected once");
    expect(
        mismatch_error.Error.Failure
                == application::github_provider_failure::unavailable
            && effects.PostCalls == effects_before_rejection
            && transport->ReadRequests.size() == reads_before_rejection
            && factory->BootstrapCalls == factory_before_rejection,
        "authority drift cannot cross-route to another configured profile");

    auto hostname_mismatch = viewer_request_for(enterprise);
    hostname_mismatch.Authority.Hostname =
        application::github_canonical_hostname{"ghe.other.invalid"};
    completions.clear();
    (*provider)->observe_viewer(
        application::github_viewer_submission{
            .Request = hostname_mismatch,
            .Authentication =
                application::github_bootstrap_credential{
                    .Credential = credential_material(),
                },
        },
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    const auto& hostname_error =
        require_one<application::github_viewer_error>(
            completions, "mismatched hostname authority is rejected once");
    expect(
        hostname_error.Error.Failure
                == application::github_provider_failure::unavailable
            && effects.PostCalls == effects_before_rejection
            && transport->ReadRequests.size() == reads_before_rejection
            && factory->BootstrapCalls == factory_before_rejection,
        "hostname authority drift cannot reach transport selection");
}

void test_multi_profile_cancellation_is_profile_scoped()
{
    const auto github = test_profile(
        application::github_authentication_transport::native_https);
    const auto enterprise = test_enterprise_profile(
        application::github_authentication_transport::native_https);
    const std::array<validated_github_host_profile, 2U> profiles{
        github,
        enterprise,
    };
    auto effects = controlled_executor{};
    auto factory = std::make_shared<fake_transport_factory>();
    auto provider = make_github_provider(profiles, effects, factory);
    expect(provider.has_value(), "cancellation multi-profile provider constructs");

    const auto github_request = application::github_profile_request{
        .Context = session_context(github),
        .Attempt = application::github_provider_attempt{1U},
    };
    const auto enterprise_request = application::github_profile_request{
        .Context = session_context(enterprise),
        .Attempt = application::github_provider_attempt{1U},
    };
    auto github_completions =
        std::vector<application::github_profile_completion>{};
    auto enterprise_completions =
        std::vector<application::github_profile_completion>{};
    (*provider)->resolve_profile(
        github_request,
        [&github_completions](auto completion)
        { github_completions.push_back(std::move(completion)); });
    (*provider)->resolve_profile(
        enterprise_request,
        [&enterprise_completions](auto completion)
        { enterprise_completions.push_back(std::move(completion)); });
    expect(effects.pending() == 2U, "both profile jobs are queued");

    (*provider)->cancel_profile(github_request);
    effects.run_next();
    effects.run_next();
    const auto& cancelled =
        require_one<application::github_profile_cancelled>(
            github_completions, "selected profile cancellation is final once");
    const auto& success = require_one<application::github_profile_success>(
        enterprise_completions, "other profile remains routable");
    expect(
        cancelled.Request == github_request
            && success.Request == enterprise_request
            && success.Authority == profile_authority_for(enterprise)
            && effects.pending() == 0U,
        "cancellation matches the complete profile request context");
}

void test_profile_resolution()
{
    provider_fixture fixture{
        application::github_authentication_transport::native_https};
    const auto request = application::github_profile_request{
        .Context = session_context(fixture.Profile),
        .Attempt = application::github_provider_attempt{1U},
    };
    auto completions = std::vector<application::github_profile_completion>{};
    fixture.Provider->resolve_profile(
        request,
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    expect(completions.empty() && fixture.Effects.pending() == 1U,
           "profile resolution is queued");
    fixture.Effects.run_next();
    const auto& success = require_one<application::github_profile_success>(
        completions, "exact profile resolves once");
    expect(success.Request == request
               && success.Authority == profile_authority_for(fixture.Profile),
           "profile resolution returns exact immutable authority");

    auto mismatch = request;
    mismatch.Context.Profile = core::host_profile_id{999U};
    completions.clear();
    fixture.Provider->resolve_profile(
        mismatch,
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    expect(fixture.Effects.pending() == 1U, "mismatch is still serialized");
    fixture.Effects.run_next();
    const auto& error = require_one<application::github_profile_error>(
        completions, "mismatched profile fails once");
    expect(error.Request == mismatch
               && error.Error.Failure
                      == application::github_provider_failure::unavailable,
           "profile mismatch cannot resolve another authority");
}

void test_bootstrap_viewer_has_no_account_lease()
{
    provider_fixture fixture{
        application::github_authentication_transport::native_https};
    const auto request = viewer_request_for(fixture.Profile);
    fixture.Transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        return github_wire_success{
            .Correlation = wire.Correlation,
            .Response = viewer_response(),
        };
    };
    auto completion =
        run_viewer(fixture,
                   application::github_viewer_submission{
                       .Request = request,
                       .Authentication =
                           application::github_bootstrap_credential{
                               .Credential = credential_material(),
                           },
                   });
    const auto* success =
        std::get_if<application::github_viewer_success>(&completion);
    expect(success != nullptr && success->Request == request
               && success->Viewer == viewer_record()
               && success->AccountTransport == nullptr,
           "bootstrap /user success cannot mint an account lease");
    expect(fixture.Factory->BootstrapCalls == 1U
               && fixture.Factory->AccountCalls == 0U
               && fixture.Factory->LastBootstrapProfile
                      == std::optional{fixture.Profile.profile()}
               && fixture.Factory->LastBootstrapContext
                      == std::optional{request.Context}
               && fixture.Factory->LastBootstrapCredentialBytes != 0U,
           "bootstrap factory sees only the exact session submission");
    expect(fixture.Transport->ReadRequests.size() == 1U
               && !fixture.Transport->ReadStopRequested.front(),
           "bootstrap performs exactly one non-cancelled /user request");
    const auto expected = compile_read_request(semantic_read_request{
        .Server = server_contract::github_com,
        .Operation = read_operation::get_viewer_identity,
        .Owner = {},
        .Repository = {},
        .PullRequestNumber = 0U,
        .HeadOid = {},
        .SearchTerms = {},
        .Page = 1U,
        .GraphqlPage = std::nullopt,
    });
    expect(expected.has_value(), "canonical viewer request compiles");
    const auto& wire = fixture.Transport->ReadRequests.front();
    expect(wire.Wire == *expected &&
             wire.Correlation ==
                 github_wire_correlation{
                     .Parent = request.Context,
                     .ProfileRevision = request.Authority.ProfileRevision,
                     .Host = request.Authority.Host,
                     .Operation = read_operation::get_viewer_identity,
                     .Page = 1U,
                 },
         "bootstrap /user request is exact and typed");
}

void test_retrieved_account_must_match_observed_viewer()
{
    provider_fixture fixture{
        application::github_authentication_transport::native_https};
    const auto request = viewer_request_for(fixture.Profile);
    const auto context = credential_context_for(
        fixture.Profile, request.Context, viewer_record().Account);
    fixture.Transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        auto response = viewer_response();
        response.Body =
            R"({"id":1002,"login":"other-user","name":"Other User","node_id":"U_other"})";
        return github_wire_success{
            .Correlation = wire.Correlation,
            .Response = std::move(response),
        };
    };
    auto completion =
        run_viewer(fixture,
                   application::github_viewer_submission{
                       .Request = request,
                       .Authentication =
                           application::github_credential_retrieve_success{
                               .Context = context,
                               .Credential = credential_material(),
                           },
                   });
    const auto* error =
        std::get_if<application::github_viewer_error>(&completion);
    expect(error != nullptr && error->Request == request
               && error->Error.Failure
                      == application::github_provider_failure::authentication
               && fixture.Factory->AccountCalls == 1U
               && fixture.Transport->ReadRequests.size() == 1U,
           "retrieved authority cannot lease a differently observed account");
}

void test_viewer_terminal_correlation()
{
    provider_fixture fixture{
        application::github_authentication_transport::native_https};
    const auto request = viewer_request_for(fixture.Profile);
    const auto submission = [&request]
    {
        return application::github_viewer_submission{
            .Request = request,
            .Authentication =
                application::github_bootstrap_credential{
                    .Credential = credential_material(),
                },
        };
    };

    fixture.Transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        auto correlation = wire.Correlation;
        correlation.Host = core::git_host_id{999U};
        return github_wire_error{
            .Correlation = correlation,
            .Error =
                {
                    .Failure = application::github_provider_failure::network,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    };
    auto completion = run_viewer(fixture, submission());
    const auto* wrong_error =
        std::get_if<application::github_viewer_error>(&completion);
    expect(
        wrong_error != nullptr
            && wrong_error->Error.Failure
                   == application::github_provider_failure::malformed_response,
        "wrong viewer error correlation is rejected");

    fixture.Transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        auto correlation = wire.Correlation;
        correlation.Page = 2U;
        return github_wire_cancelled{
            .Correlation = correlation,
        };
    };
    completion = run_viewer(fixture, submission());
    const auto* wrong_cancel =
        std::get_if<application::github_viewer_error>(&completion);
    expect(
        wrong_cancel != nullptr
            && wrong_cancel->Error.Failure
                   == application::github_provider_failure::malformed_response,
        "wrong viewer cancellation correlation is rejected");

    fixture.Transport->ReadHandler =
        [](const github_wire_request& wire, std::stop_token)
    {
        return github_wire_cancelled{
            .Correlation = wire.Correlation,
        };
    };
    completion = run_viewer(fixture, submission());
    expect(std::holds_alternative<application::github_viewer_cancelled>(
               completion),
           "exact viewer cancellation is terminal");
}

void verify_capability_terminal_contracts(
    provider_fixture& fixture,
    const application::github_capability_request& request)
{
    auto request_drift = request;
    request_drift.Context.Request = core::request_id{999U};
    auto rejected = std::vector<application::github_capability_completion>{};
    const auto posts_before = fixture.Effects.PostCalls;
    fixture.Provider->probe_capabilities(
        request_drift,
        [&rejected](auto completion)
        { rejected.push_back(std::move(completion)); });
    const auto& binding_error =
        require_one<application::github_capability_error>(
            rejected, "capability lease request drift is rejected once");
    expect(binding_error.Error.Failure
                   == application::github_provider_failure::unavailable
               && fixture.Effects.PostCalls == posts_before
               && fixture.Transport->CapabilityRequests.empty(),
           "capability lease is bound to the exact credential request");

    const auto response = supported_capability_response();
    fixture.Transport->CapabilityHandler =
        [response](const github_capability_probe_wire_request& wire,
                   std::stop_token)
    {
        return github_capability_probe_wire_success{
            .Correlation = wire.Correlation,
            .Response = response,
        };
    };
    auto completion = run_capability(fixture, request);
    const auto* success =
        std::get_if<application::github_capability_success>(&completion);
    expect(success != nullptr && success->Request == request
               && success->RateLimits.Graphql.has_value()
               && success->RateLimits.Graphql->Limit == 5000U
               && success->RateLimits.Graphql->Remaining == 4999U
               && success->RateLimits.Graphql->ResetUnixSeconds == 2000000000U
               && std::ranges::all_of(
                   success->Facts,
                   [](const auto& fact)
                   {
                       return fact.State
                              == core::github_capability_state::supported;
                   })
               && std::ranges::all_of(
                   success->SchemaFacts,
                   [](const auto& fact)
                   {
                       return fact.State
                              == core::github_capability_state::supported;
                   }),
           "exact capability success projects observed fixture evidence");
    expect(fixture.Transport->CapabilityRequests.size() == 1U
               && !fixture.Transport->CapabilityStopRequested.front(),
           "capability success performs one transport call");
    const auto expected = compile_capability_probe_request(fixture.Profile);
    expect(expected.has_value(), "canonical provider probe compiles");
    expect(fixture.Transport->CapabilityRequests.front() ==
             github_capability_probe_wire_request{
                 .Correlation =
                     {
                         .Session = request.Context,
                         .ProfileRevision =
                             request.ProfileAuthority.ProfileRevision,
                         .Host = request.ProfileAuthority.Host,
                         .ObservedAccount = request.Viewer.Account,
                     },
                 .Wire = *expected,
             },
         "provider emits the exact capability wire request");

    auto structured_limit = response;
    structured_limit.Status = 403;
    structured_limit.Body =
        R"({"errors":[{"extensions":{"code":"RATE_LIMITED"}}]})";
    fixture.Transport->CapabilityHandler =
        [structured_limit](const github_capability_probe_wire_request& wire,
                           std::stop_token)
    {
        return github_capability_probe_wire_success{
            .Correlation = wire.Correlation,
            .Response = structured_limit,
        };
    };
    completion = run_capability(fixture, request);
    const auto* limited =
        std::get_if<application::github_capability_error>(&completion);
    expect(limited != nullptr
               && limited->Error.Failure
                      == application::github_provider_failure::rate_limit
               && limited->Error.RetryAfterSeconds
                      == std::optional<std::uint32_t>{60U}
               && limited->Error.RateLimitCause.has_value()
               && limited->Error.RateLimitCause->Resource
                      == application::github_rate_limit_resource::graphql
               && limited->Error.RateLimitCause->SecondaryLimited
               && limited->Error.RateLimits.Graphql
                      == limited->Error.RateLimitCause,
           "structured capability throttling preserves exact GraphQL cause");

    fixture.Transport->CapabilityHandler =
        [](const github_capability_probe_wire_request& wire, std::stop_token)
    {
        return github_capability_probe_wire_error{
            .Correlation = wire.Correlation,
            .Error =
                {
                    .Failure = application::github_provider_failure::permission,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    };
    completion = run_capability(fixture, request);
    const auto* exact_error =
        std::get_if<application::github_capability_error>(&completion);
    expect(exact_error != nullptr
               && exact_error->Error.Failure
                      == application::github_provider_failure::permission,
           "correctly correlated capability error is preserved");

    fixture.Transport->CapabilityHandler =
        [](const github_capability_probe_wire_request& wire, std::stop_token)
    {
        auto correlation = wire.Correlation;
        correlation.Host = core::git_host_id{999U};
        return github_capability_probe_wire_error{
            .Correlation = correlation,
            .Error =
                {
                    .Failure = application::github_provider_failure::permission,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    };
    completion = run_capability(fixture, request);
    const auto* wrong_error =
        std::get_if<application::github_capability_error>(&completion);
    expect(
        wrong_error != nullptr
            && wrong_error->Error.Failure
                   == application::github_provider_failure::malformed_response,
        "wrong capability error correlation is rejected");

    fixture.Transport->CapabilityHandler =
        [](const github_capability_probe_wire_request& wire, std::stop_token)
    {
        auto correlation = wire.Correlation;
        correlation.Session.Request = core::request_id{999U};
        return github_capability_probe_wire_cancelled{
            .Correlation = correlation,
        };
    };
    completion = run_capability(fixture, request);
    const auto* wrong_cancel =
        std::get_if<application::github_capability_error>(&completion);
    expect(
        wrong_cancel != nullptr
            && wrong_cancel->Error.Failure
                   == application::github_provider_failure::malformed_response,
        "wrong capability cancellation correlation is rejected");

    fixture.Transport->CapabilityHandler =
        [](const github_capability_probe_wire_request& wire, std::stop_token)
    {
        return github_capability_probe_wire_cancelled{
            .Correlation = wire.Correlation,
        };
    };
    completion = run_capability(fixture, request);
    expect(std::holds_alternative<application::github_capability_cancelled>(
               completion),
           "exact capability cancellation is terminal");
}

void verify_fabricated_cache_rejection(
    provider_fixture& fixture,
    const application::github_account_transport& lease)
{
    const auto read_calls_before = fixture.Transport->ReadRequests.size();
    const auto posts_before = fixture.Effects.PostCalls;
    auto fabricated_capabilities = pull_request_for(fixture, lease);
    fabricated_capabilities.CapabilityEvidence.Capabilities.ReviewThreads =
        core::github_capability_state::supported;
    auto pull_completions =
        std::vector<application::github_pull_request_completion>{};
    fixture.Provider->fetch_pull_request(
        fabricated_capabilities,
        [&pull_completions](auto value)
        { pull_completions.push_back(std::move(value)); });
    const auto& fabricated_capability_error =
        require_one<application::github_pull_request_error>(
            pull_completions,
            "fabricated aggregate capability is rejected once");
    expect(fabricated_capability_error.Error.Failure
                   == application::github_provider_failure::unavailable
               && fixture.Effects.PostCalls == posts_before
               && fixture.Transport->ReadRequests.size() == read_calls_before,
           "fabricated capability evidence causes no provider effect");

    auto fabricated_shape = pull_request_for(fixture, lease);
    fabricated_shape.CapabilityEvidence.ReadShape.ReviewThreads = true;
    pull_completions.clear();
    fixture.Provider->fetch_pull_request(
        fabricated_shape,
        [&pull_completions](auto value)
        { pull_completions.push_back(std::move(value)); });
    const auto& fabricated_shape_error =
        require_one<application::github_pull_request_error>(
            pull_completions, "fabricated read shape is rejected once");
    expect(fabricated_shape_error.Error.Failure
                   == application::github_provider_failure::unavailable
               && fixture.Effects.PostCalls == posts_before
               && fixture.Transport->ReadRequests.size() == read_calls_before,
           "fabricated read shape causes no provider effect");
}

void verify_no_internal_retry(
    provider_fixture& fixture,
    const application::github_capability_request& request)
{
    fixture.Transport->CapabilityHandler =
        [](const github_capability_probe_wire_request& wire, std::stop_token)
    {
        return github_capability_probe_wire_error{
            .Correlation = wire.Correlation,
            .Error =
                {
                    .Failure = application::github_provider_failure::network,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    };
    const auto calls_before_retry =
        fixture.Transport->CapabilityRequests.size();
    auto first_attempt = request;
    first_attempt.Attempt = application::github_provider_attempt{1U};
    auto completion = run_capability(fixture, first_attempt);
    const auto* first_error =
        std::get_if<application::github_capability_error>(&completion);
    expect(first_error != nullptr
               && first_error->Error.Failure
                      == application::github_provider_failure::network
               && fixture.Transport->CapabilityRequests.size()
                      == calls_before_retry + 1U
               && fixture.Effects.pending() == 0U,
           "first submitted attempt performs exactly one wire call");

    auto second_attempt = request;
    second_attempt.Attempt = application::github_provider_attempt{2U};
    completion = run_capability(fixture, second_attempt);
    const auto* second_error =
        std::get_if<application::github_capability_error>(&completion);
    expect(second_error != nullptr
               && second_error->Error.Failure
                      == application::github_provider_failure::network
               && fixture.Transport->CapabilityRequests.size()
                      == calls_before_retry + 2U
               && fixture.Effects.pending() == 0U,
           "provider never retries beyond explicitly submitted attempts");
}

void test_account_capability_cache_and_retry_contracts()
{
    provider_fixture fixture{
        application::github_authentication_transport::native_https};
    const auto lease = account_lease(fixture);
    const auto request = capability_request_for(fixture, lease);
    verify_capability_terminal_contracts(fixture, request);
    verify_fabricated_cache_rejection(fixture, lease);
    verify_no_internal_retry(fixture, request);
}

void test_cancellation_finality()
{
    provider_fixture fixture{
        application::github_authentication_transport::native_https};
    const auto lease = account_lease(fixture);
    auto request = capability_request_for(fixture, lease);
    request.Attempt = application::github_provider_attempt{3U};
    const auto calls_before = fixture.Transport->CapabilityRequests.size();
    auto completions = std::vector<application::github_capability_completion>{};
    fixture.Provider->probe_capabilities(
        request,
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    fixture.Provider->cancel_capabilities(request);
    fixture.Provider->cancel_capabilities(request);
    fixture.Effects.run_next();
    const auto& cancelled =
        require_one<application::github_capability_cancelled>(
            completions, "pre-execution cancellation is final exactly once");
    fixture.Provider->cancel_capabilities(request);
    expect(cancelled.Request == request && completions.size() == 1U
               && fixture.Transport->CapabilityRequests.size() == calls_before
               && fixture.Effects.pending() == 0U,
           "cancelled work cannot perform transport effects or redeliver");

    auto late_request = request;
    late_request.Attempt = application::github_provider_attempt{2U};
    bool stop_observed = false;
    fixture.Transport->CapabilityHandler =
        [&fixture, late_request, &stop_observed](
            const github_capability_probe_wire_request& wire,
            const std::stop_token stop)
    {
        fixture.Provider->cancel_capabilities(late_request);
        stop_observed = stop.stop_requested();
        return github_capability_probe_wire_error{
            .Correlation = wire.Correlation,
            .Error =
                {
                    .Failure = application::github_provider_failure::network,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                },
        };
    };
    auto completion = run_capability(fixture, late_request);
    expect(
        stop_observed
            && std::holds_alternative<application::github_capability_cancelled>(
                completion),
        "cancellation won during transport remains the only terminal");
}

void test_exception_containment_is_exactly_once()
{
    {
        provider_fixture fixture{
            application::github_authentication_transport::native_https};
        fixture.Effects.ThrowOnPost = true;
        fixture.Effects.RetainTaskOnThrow = true;
        const auto request = application::github_profile_request{
            .Context = session_context(fixture.Profile),
            .Attempt = application::github_provider_attempt{1U},
        };
        auto completions =
            std::vector<application::github_profile_completion>{};
        fixture.Provider->resolve_profile(
            request,
            [&completions](auto completion)
            { completions.push_back(std::move(completion)); });
        const auto error = require_one<application::github_profile_error>(
            completions, "post throw is contained exactly once");
        expect(fixture.Effects.pending() == 1U,
               "throwing post retained its worker task");
        fixture.Effects.run_next();
        fixture.Provider->cancel_profile(request);
        fixture.Provider->shutdown();
        expect(
            error.Request == request
                && error.Error.Failure
                       == application::github_provider_failure::response_limit
                && fixture.Effects.PostCalls == 1U
                && fixture.Effects.pending() == 0U && completions.size() == 1U,
            "post failure preserves the original completion delivery");
    }

    {
        provider_fixture fixture{
            application::github_authentication_transport::native_https};
        fixture.Effects.ThrowOnPost = true;
        fixture.Effects.RetainTaskOnThrow = true;
        const auto request = viewer_request_for(fixture.Profile);
        auto completions = std::vector<application::github_viewer_completion>{};
        fixture.Provider->observe_viewer(
            application::github_viewer_submission{
                .Request = request,
                .Authentication =
                    application::github_bootstrap_credential{
                        .Credential = credential_material(),
                    },
            },
            [&completions](auto completion)
            { completions.push_back(std::move(completion)); });
        const auto& error = require_one<application::github_viewer_error>(
            completions, "viewer post throw is contained exactly once");
        expect(
            fixture.Effects.pending() == 1U
                && fixture.Factory->BootstrapCalls == 0U
                && fixture.Factory->AccountCalls == 0U
                && fixture.Transport->ReadRequests.empty(),
            "retained viewer task is stopped before any credential transport");
        fixture.Effects.run_next();
        expect(
            error.Request == request
                && error.Error.Failure
                       == application::github_provider_failure::response_limit
                && fixture.Effects.pending() == 0U && completions.size() == 1U
                && fixture.Factory->BootstrapCalls == 0U
                && fixture.Transport->ReadRequests.empty(),
            "retained viewer task cannot perform effects or redeliver");
    }

    {
        provider_fixture fixture{
            application::github_authentication_transport::native_https};
        fixture.Transport->ReadHandler =
            [](const github_wire_request&,
               std::stop_token) -> github_wire_completion
        { throw std::runtime_error{"worker failure"}; };
        const auto request = viewer_request_for(fixture.Profile);
        auto completions = std::vector<application::github_viewer_completion>{};
        fixture.Provider->observe_viewer(
            application::github_viewer_submission{
                .Request = request,
                .Authentication =
                    application::github_bootstrap_credential{
                        .Credential = credential_material(),
                    },
            },
            [&completions](auto completion)
            { completions.push_back(std::move(completion)); });
        expect(completions.empty() && fixture.Effects.pending() == 1U,
               "throwing worker is queued");
        bool escaped = false;
        try
        {
            fixture.Effects.run_next();
        }
        catch(...)
        {
            escaped = true;
        }
        const auto& error = require_one<application::github_viewer_error>(
            completions, "worker throw is contained exactly once");
        fixture.Provider->cancel_viewer(request);
        fixture.Provider->shutdown();
        expect(
            !escaped && error.Request == request
                && error.Error.Failure
                       == application::github_provider_failure::response_limit
                && fixture.Transport->ReadRequests.size() == 1U
                && fixture.Effects.pending() == 0U && completions.size() == 1U,
            "worker exception cannot escape or redeliver");
    }
}

void test_cli_is_categorically_unavailable()
{
    provider_fixture fixture{
        application::github_authentication_transport::github_cli};
    const auto request = viewer_request_for(fixture.Profile);
    auto completions = std::vector<application::github_viewer_completion>{};
    fixture.Provider->observe_viewer(
        application::github_viewer_submission{
            .Request = request,
            .Authentication = std::monostate{},
        },
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    const auto& error = require_one<application::github_viewer_error>(
        completions, "CLI viewer is unavailable exactly once");
    expect(error.Request == request
               && error.Error.Failure
                      == application::github_provider_failure::cli_unavailable
               && fixture.Factory->BootstrapCalls == 0U
               && fixture.Factory->AccountCalls == 0U
               && fixture.Transport->ReadRequests.empty()
               && fixture.Transport->CapabilityRequests.empty()
               && fixture.Effects.PostCalls == 0U
               && fixture.Effects.pending() == 0U,
           "CLI path cannot start a process or credential transport");

    completions.clear();
    fixture.Provider->observe_viewer(
        application::github_viewer_submission{
            .Request = request,
            .Authentication =
                application::github_bootstrap_credential{
                    .Credential = credential_material(),
                },
        },
        [&completions](auto completion)
        { completions.push_back(std::move(completion)); });
    const auto& credential_error =
        require_one<application::github_viewer_error>(
            completions, "CLI credential submission is rejected once");
    expect(credential_error.Error.Failure
                   == application::github_provider_failure::unavailable
               && fixture.Factory->BootstrapCalls == 0U
               && fixture.Factory->AccountCalls == 0U
               && fixture.Effects.PostCalls == 0U,
           "CLI unavailability cannot be bypassed with credential material");
}
}

int main()
{
    test_profile_set_validation();
    test_multi_profile_routing_and_authority_rejection();
    test_multi_profile_cancellation_is_profile_scoped();
    test_profile_resolution();
    test_bootstrap_viewer_has_no_account_lease();
    test_retrieved_account_must_match_observed_viewer();
    test_viewer_terminal_correlation();
    test_account_capability_cache_and_retry_contracts();
    test_cancellation_finality();
    test_exception_containment_is_exactly_once();
    test_cli_is_categorically_unavailable();
    return EXIT_SUCCESS;
}
