#include "github_provider.hpp"

#include "github_paged_read.hpp"
#include "native_https_transport.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <ranges>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail
{
namespace
{
enum class provider_phase : std::uint8_t
{
    profile,
    viewer,
    capability,
    pull_request,
};

using provider_parent_context =
    std::variant<core::github_session_request_context,
                 core::pull_request_request_context>;

struct provider_job
{
    provider_phase Phase = provider_phase::profile;
    provider_parent_context Context;
    application::github_provider_attempt Attempt;
    std::stop_source Stop;
};

struct provider_state
{
    application::effect_executor* Effects = nullptr;
    std::shared_ptr<github_wire_transport_factory_port> TransportFactory;
    std::vector<std::shared_ptr<const validated_github_host_profile>> Profiles;
    std::mutex Mutex;
    std::vector<std::shared_ptr<provider_job>> Jobs;
    std::atomic<std::uint64_t> NextSnapshot{1U};
    bool Closing = false;
};

enum class make_job_failure : std::uint8_t
{
    closing,
    exhausted,
};

using make_job_result =
    std::expected<std::shared_ptr<provider_job>, make_job_failure>;

template <typename Handler> class provider_completion_delivery final
{
public:
    explicit provider_completion_delivery(Handler handler) noexcept
        : HandlerValue{std::move(handler)}
    {
    }

    template <typename Completion> void deliver(Completion completion) noexcept
    {
        if(Delivered.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }
        try
        {
            HandlerValue(std::move(completion));
        }
        catch(...)
        {
        }
    }

private:
    Handler HandlerValue;
    std::atomic_bool Delivered = false;
};

template <typename Handler>
[[nodiscard]] auto make_delivery(Handler&& handler) -> std::shared_ptr<
    provider_completion_delivery<std::remove_cvref_t<Handler>>>
{
    return std::make_shared<
        provider_completion_delivery<std::remove_cvref_t<Handler>>>(
        std::forward<Handler>(handler));
}

class github_wire_account_transport final
    : public application::github_account_transport_lease
{
public:
    github_wire_account_transport(
        application::github_profile_authority profile,
        application::github_credential_operation_context credential_context,
        std::shared_ptr<github_wire_transport_port> transport) noexcept
        : Profile{profile}
        , CredentialContext{credential_context}
        , Transport{std::move(transport)}
    {
    }

    application::github_profile_authority Profile;
    application::github_credential_operation_context CredentialContext;
    std::shared_ptr<github_wire_transport_port> Transport;
};

[[nodiscard]] auto
provider_error(const application::github_provider_failure failure) noexcept
    -> application::github_provider_error
{
    return {
        .Failure = failure,
        .RetryAfterSeconds = std::nullopt,
        .RateLimits = {},
        .RateLimitCause = std::nullopt,
    };
}

[[nodiscard]] auto rate_limit_state(
    const std::optional<application::github_rate_limit_record>& record) noexcept
    -> application::github_rate_limit_state
{
    auto state = application::github_rate_limit_state{};
    if(record.has_value())
    {
        static_cast<void>(application::observe(state, *record));
    }
    return state;
}

[[nodiscard]] constexpr auto rate_limits_are_empty_or_exact(
    const application::github_rate_limit_state& state,
    const application::github_rate_limit_resource expected) noexcept -> bool
{
    if(!application::valid(state) || !application::has_observations(state))
    {
        return application::valid(state);
    }
    const auto count = static_cast<std::size_t>(state.Core.has_value())
                       + static_cast<std::size_t>(state.Search.has_value())
                       + static_cast<std::size_t>(state.Graphql.has_value());
    return count == 1U && application::find(state, expected) != nullptr;
}

[[nodiscard]] auto
configuration_error(const native_https_configuration_failure failure) noexcept
    -> application::github_provider_error
{
    switch(failure)
    {
    case native_https_configuration_failure::invalid_credential_material:
    case native_https_configuration_failure::invalid_account_credential:
        return provider_error(
            application::github_provider_failure::authentication);
    case native_https_configuration_failure::allocation_failure:
        return provider_error(
            application::github_provider_failure::response_limit);
    case native_https_configuration_failure::profile_transport_mismatch:
    case native_https_configuration_failure::profile_policy_mismatch:
    case native_https_configuration_failure::invalid_bootstrap_context:
    case native_https_configuration_failure::credential_authority_mismatch:
    case native_https_configuration_failure::invalid_test_configuration:
        return provider_error(
            application::github_provider_failure::unavailable);
    }
    return provider_error(application::github_provider_failure::unavailable);
}

[[nodiscard]] auto
profile_authority(const validated_github_host_profile& profile) noexcept
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

[[nodiscard]] auto
exact_profile(const application::github_profile_authority& authority,
              const validated_github_host_profile& profile) noexcept -> bool
{
    return application::valid(authority)
           && authority == profile_authority(profile);
}

[[nodiscard]] auto
find_profile(const provider_state& state,
             const core::host_profile_id identity) noexcept
    -> std::shared_ptr<const validated_github_host_profile>
{
    const auto match = std::ranges::find_if(
        state.Profiles,
        [identity](const auto& profile)
        { return profile->profile() == identity; });
    return match == state.Profiles.end() ? nullptr : *match;
}

[[nodiscard]] auto
find_exact_profile(const provider_state& state,
                   const application::github_profile_authority& authority)
    noexcept -> std::shared_ptr<const validated_github_host_profile>
{
    auto profile = find_profile(state, authority.Profile);
    if(profile == nullptr || !exact_profile(authority, *profile))
    {
        return {};
    }
    return profile;
}

[[nodiscard]] auto structurally_valid(
    const application::github_capability_cache_entry& evidence) noexcept -> bool
{
    const application::github_capability_probe_observation observation{
        .Authority = evidence.Authority,
        .ObservedAt = evidence.ObservedAt,
        .Facts = evidence.Facts,
        .SchemaFacts = evidence.SchemaFacts,
    };
    return application::valid(observation) && evidence.Revision.value() != 0U
           && evidence.ExpiresBefore > evidence.ObservedAt
           && (evidence.ExpiresBefore.value() - evidence.ObservedAt.value()
               <= application::maximum_github_capability_freshness_window)
           && evidence.Capabilities
                  == application::projected_capabilities(observation)
           && evidence.ReadShape
                  == application::projected_read_shape(observation);
}

[[nodiscard]] auto
valid_request(const application::github_profile_request& request) noexcept
    -> bool
{
    return application::detail::valid_context(request.Context)
           && request.Attempt.valid();
}

[[nodiscard]] auto
valid_request(const application::github_viewer_request& request) noexcept
    -> bool
{
    return application::detail::valid_context(request.Context)
           && application::valid(request.Authority)
           && request.Context.Profile == request.Authority.Profile
           && request.Attempt.valid();
}

[[nodiscard]] auto
valid_request(const application::github_capability_request& request) noexcept
    -> bool
{
    return application::detail::valid_context(request.Context)
           && application::valid(request.ProfileAuthority)
           && request.Context.Profile == request.ProfileAuthority.Profile
           && request.Viewer.Account.value() != 0U
           && request.AccountTransport != nullptr && request.Attempt.valid();
}

[[nodiscard]] auto valid_request(
    const application::github_pull_request_provider_request& request) noexcept
    -> bool
{
    return application::detail::valid_context(request.Context)
           && application::valid(request.ProfileAuthority)
           && request.Context.Profile == request.ProfileAuthority.Profile
           && request.Context.Host == request.ProfileAuthority.Host
           && request.Viewer.Account == request.Context.Account
           && request.CapabilityEvidence.Authority.Profile
                  == request.Context.Profile
           && request.CapabilityEvidence.Authority.Host == request.Context.Host
           && request.CapabilityEvidence.Authority.Account
                  == request.Context.Account
           && request.CapabilityEvidence.Authority.ProfileRevision
                  == request.ProfileAuthority.ProfileRevision
           && request.CapabilityEvidence.Authority.ServerContract
                  == request.ProfileAuthority.ServerContract
           && request.CapabilityEvidence.Authority.ProbeContractRevision
                  == request.ProfileAuthority.ProbeContractRevision
           && request.CapabilityEvidence.Revision
                  == request.Context.Capabilities
           && structurally_valid(request.CapabilityEvidence)
           && request.AccountTransport != nullptr && request.Attempt.valid();
}

[[nodiscard]] auto make_job(const std::shared_ptr<provider_state>& state,
                            const provider_phase phase,
                            provider_parent_context context,
                            const application::github_provider_attempt attempt)
    -> make_job_result
{
    try
    {
        auto job = std::make_shared<provider_job>(provider_job{
            .Phase = phase,
            .Context = std::move(context),
            .Attempt = attempt,
            .Stop = {},
        });
        const std::scoped_lock lock{state->Mutex};
        if(state->Closing)
        {
            return std::unexpected{make_job_failure::closing};
        }
        state->Jobs.push_back(job);
        return job;
    }
    catch(...)
    {
        return std::unexpected{make_job_failure::exhausted};
    }
}

auto finish_job(const std::shared_ptr<provider_state>& state,
                const std::shared_ptr<provider_job>& job) noexcept -> bool
{
    const std::scoped_lock lock{state->Mutex};
    const auto cancelled = job->Stop.stop_requested();
    const auto match = std::ranges::find(state->Jobs, job);
    if(match != state->Jobs.end())
    {
        state->Jobs.erase(match);
    }
    return cancelled;
}

[[nodiscard]] auto
allocate_snapshot(const std::shared_ptr<provider_state>& state) noexcept
    -> std::optional<core::provider_snapshot_id>
{
    auto current = state->NextSnapshot.load(std::memory_order_relaxed);
    while(current != 0U && current != std::numeric_limits<std::uint64_t>::max())
    {
        if(state->NextSnapshot.compare_exchange_weak(current,
                                                     current + 1U,
                                                     std::memory_order_relaxed,
                                                     std::memory_order_relaxed))
        {
            return core::provider_snapshot_id{current};
        }
    }
    return std::nullopt;
}

[[nodiscard]] auto
stop_after_failed_post(const std::shared_ptr<provider_state>& state,
                       const std::shared_ptr<provider_job>& job) noexcept
    -> bool
{
    const auto was_cancelled = job->Stop.stop_requested();
    job->Stop.request_stop();
    static_cast<void>(finish_job(state, job));
    return was_cancelled;
}

template <typename Request>
void cancel_jobs(const std::shared_ptr<provider_state>& state,
                 const provider_phase phase,
                 const Request& request) noexcept
{
    const provider_parent_context context{request.Context};
    const std::scoped_lock lock{state->Mutex};
    for(const auto& job : state->Jobs)
    {
        if(job->Phase == phase && job->Context == context
           && job->Attempt == request.Attempt)
        {
            job->Stop.request_stop();
        }
    }
}

[[nodiscard]] auto
expected_server(const validated_github_host_profile& profile) noexcept
    -> std::optional<server_contract>
{
    switch(profile.server_contract())
    {
    case application::github_server_contract::github_com_2022_11_28:
        return server_contract::github_com;
    case application::github_server_contract::ghes_3_18_2022_11_28:
        return server_contract::ghes_3_18;
    }
    return std::nullopt;
}

[[nodiscard]] auto
viewer_wire_request(const validated_github_host_profile& profile,
                    const application::github_viewer_request& request)
    -> std::expected<github_wire_request, application::github_provider_error>
{
    const auto server = expected_server(profile);
    if(!server.has_value())
    {
        return std::unexpected{
            provider_error(application::github_provider_failure::api_version)};
    }
    semantic_read_request semantic{
        .Server = *server,
        .Operation = read_operation::get_viewer_identity,
        .Owner = {},
        .Repository = {},
        .PullRequestNumber = 0U,
        .HeadOid = {},
        .SearchTerms = {},
        .Page = 1U,
        .GraphqlPage = std::nullopt,
    };
    const auto wire = compile_read_request(semantic);
    if(!wire.has_value())
    {
        return std::unexpected{provider_error(
            application::github_provider_failure::malformed_response)};
    }
    return github_wire_request{
        .Correlation =
            github_wire_correlation{
                .Parent = request.Context,
                .ProfileRevision = request.Authority.ProfileRevision,
                .Host = request.Authority.Host,
                .Operation = read_operation::get_viewer_identity,
                .Page = 1U,
            },
        .Wire = *wire,
    };
}

[[nodiscard]] auto
provider_failure(const capability_probe_unavailability failure) noexcept
    -> application::github_provider_failure
{
    switch(failure)
    {
    case capability_probe_unavailability::authentication:
        return application::github_provider_failure::authentication;
    case capability_probe_unavailability::permission:
        return application::github_provider_failure::permission;
    case capability_probe_unavailability::network:
        return application::github_provider_failure::network;
    case capability_probe_unavailability::api_version:
        return application::github_provider_failure::api_version;
    case capability_probe_unavailability::rate_limit:
        return application::github_provider_failure::rate_limit;
    case capability_probe_unavailability::certificate:
        return application::github_provider_failure::certificate;
    case capability_probe_unavailability::redirect:
        return application::github_provider_failure::redirect;
    case capability_probe_unavailability::timeout:
        return application::github_provider_failure::timeout;
    case capability_probe_unavailability::response_limit:
    case capability_probe_unavailability::allocation_failure:
        return application::github_provider_failure::response_limit;
    case capability_probe_unavailability::invalid_contract:
    case capability_probe_unavailability::invalid_limits:
    case capability_probe_unavailability::invalid_status:
    case capability_probe_unavailability::invalid_content_type:
    case capability_probe_unavailability::partial_response:
    case capability_probe_unavailability::malformed_response:
        return application::github_provider_failure::malformed_response;
    case capability_probe_unavailability::cancelled:
        return application::github_provider_failure::unavailable;
    case capability_probe_unavailability::none:
        return application::github_provider_failure::unavailable;
    }
    return application::github_provider_failure::unavailable;
}

[[nodiscard]] auto
schema_state(const capability_schema_fact_state state) noexcept
    -> core::github_capability_state
{
    switch(state)
    {
    case capability_schema_fact_state::matched:
        return core::github_capability_state::supported;
    case capability_schema_fact_state::missing:
    case capability_schema_fact_state::incompatible:
        return core::github_capability_state::unsupported;
    case capability_schema_fact_state::unknown:
        return core::github_capability_state::unknown;
    }
    return core::github_capability_state::unknown;
}

[[nodiscard]] auto
capability_facts(const core::github_capabilities& capabilities)
    -> std::array<application::github_capability_fact,
                  application::github_capability_fact_count>
{
    std::array<application::github_capability_fact,
               application::github_capability_fact_count>
        facts{};
    for(std::size_t index = 0U; index < facts.size(); ++index)
    {
        const auto capability =
            application::canonical_github_capabilities[index];
        facts[index] = {
            .Capability = capability,
            .State = core::capability_state(capabilities, capability),
            .Source = application::github_capability_evidence_source::
                observed_contract,
        };
    }
    return facts;
}

[[nodiscard]] auto schema_facts(const capability_probe_observation& observation)
    -> std::array<application::github_schema_capability_fact,
                  application::github_schema_capability_fact_count>
{
    std::array<application::github_schema_capability_fact,
               application::github_schema_capability_fact_count>
        facts{};
    for(std::size_t index = 0U; index < facts.size(); ++index)
    {
        facts[index] = {
            .Capability =
                application::canonical_github_schema_capabilities[index],
            .State = schema_state(observation.Facts[index]),
            .Source = application::github_capability_evidence_source::
                observed_contract,
        };
    }
    return facts;
}

[[nodiscard]] auto exact_account_transport(
    const application::github_account_transport& opaque,
    const application::github_profile_authority& profile,
    const core::application_session_id session,
    const core::github_identity_generation generation,
    const core::github_account_id account,
    const std::optional<core::request_id> credential_request = std::nullopt)
    -> std::shared_ptr<github_wire_account_transport>
{
    const auto transport =
        std::dynamic_pointer_cast<github_wire_account_transport>(opaque);
    if(transport == nullptr || transport->Transport == nullptr
       || transport->Profile != profile
       || transport->CredentialContext.ApplicationSession != session
       || transport->CredentialContext.IdentityGeneration != generation
       || transport->CredentialContext.Authority.Profile != profile.Profile
       || transport->CredentialContext.Authority.ProfileRevision
              != profile.ProfileRevision
       || transport->CredentialContext.Authority.Host != profile.Host
       || transport->CredentialContext.Authority.Hostname != profile.Hostname
       || transport->CredentialContext.Authority.Account != account
       || transport->CredentialContext.Authority.ServerContract
              != profile.ServerContract)
    {
        return {};
    }
    if(credential_request.has_value()
       && transport->CredentialContext.Request != *credential_request)
    {
        return {};
    }
    return transport;
}
}

class github_provider::implementation final
{
public:
    explicit implementation(std::shared_ptr<provider_state> state) noexcept
        : State{std::move(state)}
    {
    }

    std::shared_ptr<provider_state> State;
};

auto native_github_wire_transport_factory::make_bootstrap(
    const validated_github_host_profile& profile,
    const core::github_session_request_context context,
    application::github_credential_material credential) noexcept
    -> github_wire_transport_factory_result
{
    auto transport = make_native_https_bootstrap_transport(
        profile, context, std::move(credential));
    if(!transport.has_value())
    {
        return std::unexpected{configuration_error(transport.error())};
    }
    return std::shared_ptr<github_wire_transport_port>{std::move(*transport)};
}

auto native_github_wire_transport_factory::make_account(
    const validated_github_host_profile& profile,
    application::github_credential_retrieve_success credential) noexcept
    -> github_wire_transport_factory_result
{
    auto transport =
        make_native_https_account_transport(profile, std::move(credential));
    if(!transport.has_value())
    {
        return std::unexpected{configuration_error(transport.error())};
    }
    return std::shared_ptr<github_wire_transport_port>{std::move(*transport)};
}

github_provider::github_provider(
    std::unique_ptr<implementation> implementation) noexcept
    : Implementation{std::move(implementation)}
{
}

github_provider::~github_provider()
{
    shutdown();
}

void github_provider::resolve_profile(
    application::github_profile_request request,
    application::github_profile_completion_handler completion)
{
    if(Implementation == nullptr || !valid_request(request))
    {
        completion(application::github_profile_error{
            .Request = request,
            .Error = provider_error(
                application::github_provider_failure::unavailable),
        });
        return;
    }
    const auto state = Implementation->State;
    const auto profile = find_profile(*state, request.Context.Profile);
    std::shared_ptr<provider_completion_delivery<
        application::github_profile_completion_handler>>
        delivery;
    try
    {
        delivery = make_delivery(std::move(completion));
    }
    catch(...)
    {
        completion(application::github_profile_error{
            .Request = request,
            .Error = provider_error(
                application::github_provider_failure::response_limit),
        });
        return;
    }
    const auto job_result = make_job(
        state, provider_phase::profile, request.Context, request.Attempt);
    if(!job_result.has_value())
    {
        if(job_result.error() == make_job_failure::closing)
        {
            delivery->deliver(
                application::github_profile_cancelled{.Request = request});
        }
        else
        {
            delivery->deliver(application::github_profile_error{
                .Request = request,
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
        return;
    }
    const auto job = *job_result;
    try
    {
        state->Effects->post(
            [state, job, profile, request, delivery]
            {
                try
                {
                    if(finish_job(state, job))
                    {
                        delivery->deliver(application::github_profile_cancelled{
                            .Request = request,
                        });
                    }
                    else if(profile == nullptr)
                    {
                        delivery->deliver(application::github_profile_error{
                            .Request = request,
                            .Error = provider_error(
                                application::github_provider_failure::
                                    unavailable),
                        });
                    }
                    else
                    {
                        delivery->deliver(application::github_profile_success{
                            .Request = request,
                            .Authority = profile_authority(*profile),
                        });
                    }
                }
                catch(...)
                {
                    const auto cancelled = finish_job(state, job);
                    if(cancelled)
                    {
                        delivery->deliver(application::github_profile_cancelled{
                            .Request = request,
                        });
                    }
                    else
                    {
                        delivery->deliver(application::github_profile_error{
                            .Request = request,
                            .Error = provider_error(
                                application::github_provider_failure::
                                    response_limit),
                        });
                    }
                }
            });
    }
    catch(...)
    {
        const auto cancelled = stop_after_failed_post(state, job);
        if(cancelled)
        {
            delivery->deliver(application::github_profile_cancelled{
                .Request = request,
            });
        }
        else
        {
            delivery->deliver(application::github_profile_error{
                .Request = request,
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
    }
}

void github_provider::cancel_profile(
    const application::github_profile_request request)
{
    if(Implementation != nullptr)
    {
        cancel_jobs(Implementation->State, provider_phase::profile, request);
    }
}

void github_provider::observe_viewer(
    application::github_viewer_submission submission,
    application::github_viewer_completion_handler completion)
{
    auto request = submission.Request;
    if(Implementation == nullptr || !valid_request(request))
    {
        completion(application::github_viewer_error{
            .Request = request,
            .Error = provider_error(
                application::github_provider_failure::unavailable),
        });
        return;
    }
    const auto state = Implementation->State;
    const auto profile = find_exact_profile(*state, request.Authority);
    if(profile == nullptr
       || (request.Authority.Authentication
               == application::github_authentication_transport::github_cli
           && !std::holds_alternative<std::monostate>(
               submission.Authentication)))
    {
        completion(application::github_viewer_error{
            .Request = request,
            .Error = provider_error(
                application::github_provider_failure::unavailable),
        });
        return;
    }
    if(request.Authority.Authentication
       == application::github_authentication_transport::github_cli)
    {
        completion(application::github_viewer_error{
            .Request = request,
            .Error = provider_error(
                application::github_provider_failure::cli_unavailable),
        });
        return;
    }

    std::shared_ptr<provider_completion_delivery<
        application::github_viewer_completion_handler>>
        delivery;
    try
    {
        delivery = make_delivery(std::move(completion));
    }
    catch(...)
    {
        completion(application::github_viewer_error{
            .Request = request,
            .Error = provider_error(
                application::github_provider_failure::response_limit),
        });
        return;
    }
    const auto job_result = make_job(
        state, provider_phase::viewer, request.Context, request.Attempt);
    if(!job_result.has_value())
    {
        if(job_result.error() == make_job_failure::closing)
        {
            delivery->deliver(
                application::github_viewer_cancelled{.Request = request});
        }
        else
        {
            delivery->deliver(application::github_viewer_error{
                .Request = request,
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
        return;
    }
    const auto job = *job_result;
    std::shared_ptr<application::github_viewer_authentication> authentication;
    try
    {
        authentication =
            std::make_shared<application::github_viewer_authentication>(
                std::move(submission.Authentication));
        state->Effects->post(
            [state,
             job,
             profile,
             request,
             authentication = std::move(authentication),
             delivery]() mutable
            {
                const auto cancelled = [&]
                {
                    finish_job(state, job);
                    delivery->deliver(application::github_viewer_cancelled{
                        .Request = request,
                    });
                };
                const auto failed =
                    [&](application::github_provider_error error)
                {
                    if(finish_job(state, job))
                    {
                        delivery->deliver(application::github_viewer_cancelled{
                            .Request = request,
                        });
                        return;
                    }
                    delivery->deliver(application::github_viewer_error{
                        .Request = request,
                        .Error = std::move(error),
                    });
                };
                try
                {
                    if(job->Stop.stop_requested())
                    {
                        cancelled();
                        return;
                    }
                    if(authentication->valueless_by_exception())
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                malformed_response));
                        return;
                    }

                    std::optional<
                        application::github_credential_operation_context>
                        account_context;
                    github_wire_transport_factory_result transport =
                        std::unexpected{provider_error(
                            application::github_provider_failure::unavailable)};
                    if(auto* bootstrap = std::get_if<
                           application::github_bootstrap_credential>(
                           authentication.get());
                       bootstrap != nullptr)
                    {
                        if(!application::valid(bootstrap->Credential))
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    authentication));
                            return;
                        }
                        transport = state->TransportFactory->make_bootstrap(
                            *profile,
                            request.Context,
                            std::move(bootstrap->Credential));
                    }
                    else if(auto* account = std::get_if<
                                application::
                                    github_credential_retrieve_success>(
                                authentication.get());
                            account != nullptr)
                    {
                        if(!application::valid(*account)
                           || account->Context.Request
                                  != request.Context.Request
                           || account->Context.ApplicationSession
                                  != request.Context.ApplicationSession
                           || account->Context.Authority.Profile
                                  != request.Context.Profile
                           || account->Context.Authority.ProfileRevision
                                  != request.Authority.ProfileRevision
                           || account->Context.Authority.Host
                                  != request.Authority.Host
                           || account->Context.Authority.Hostname
                                  != request.Authority.Hostname
                           || account->Context.Authority.ServerContract
                                  != request.Authority.ServerContract
                           || account->Context.IdentityGeneration
                                  != request.Context.IdentityGeneration)
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    authentication));
                            return;
                        }
                        account_context = account->Context;
                        transport = state->TransportFactory->make_account(
                            *profile, std::move(*account));
                    }
                    if(!transport.has_value())
                    {
                        failed(transport.error());
                        return;
                    }
                    const auto wire =
                        viewer_wire_request(*profile, request);
                    if(!wire.has_value())
                    {
                        failed(wire.error());
                        return;
                    }
                    const auto terminal =
                        (*transport)->perform(*wire, job->Stop.get_token());
                    if(terminal.valueless_by_exception())
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                malformed_response));
                        return;
                    }
                    if(const auto* wire_cancelled =
                           std::get_if<github_wire_cancelled>(&terminal);
                       wire_cancelled != nullptr)
                    {
                        if(wire_cancelled->Correlation != wire->Correlation)
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    malformed_response));
                        }
                        else
                        {
                            cancelled();
                        }
                        return;
                    }
                    if(const auto* error =
                           std::get_if<github_wire_error>(&terminal);
                       error != nullptr)
                    {
                        if(error->Correlation != wire->Correlation
                           || !application::valid(error->Error)
                           || !rate_limits_are_empty_or_exact(
                               error->Error.RateLimits,
                               application::github_rate_limit_resource::core))
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    malformed_response));
                        }
                        else
                        {
                            failed(error->Error);
                        }
                        return;
                    }
                    const auto* success =
                        std::get_if<github_wire_success>(&terminal);
                    if(success == nullptr
                       || success->Correlation != wire->Correlation)
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                malformed_response));
                        return;
                    }
                    const auto server = expected_server(*profile);
                    if(!server.has_value())
                    {
                        failed(provider_error(
                            application::github_provider_failure::api_version));
                        return;
                    }
                    const auto observed = normalize_observed_github_viewer(
                        success->Response, *server);
                    if(!observed.has_value()
                       || (account_context.has_value()
                           && observed->Viewer.Account
                                  != account_context->Authority.Account))
                    {
                        failed(observed.has_value()
                                   ? provider_error(
                                         application::github_provider_failure::
                                             authentication)
                                   : observed.error());
                        return;
                    }

                    application::github_account_transport account_transport;
                    if(account_context.has_value())
                    {
                        account_transport =
                            std::make_shared<github_wire_account_transport>(
                                request.Authority,
                                *account_context,
                                *transport);
                    }
                    if(finish_job(state, job))
                    {
                        delivery->deliver(application::github_viewer_cancelled{
                            .Request = request,
                        });
                        return;
                    }
                    delivery->deliver(application::github_viewer_success{
                        .Request = request,
                        .Viewer = observed->Viewer,
                        .RateLimits = observed->RateLimits,
                        .AccountTransport = std::move(account_transport),
                    });
                }
                catch(...)
                {
                    failed(provider_error(
                        application::github_provider_failure::response_limit));
                }
            });
    }
    catch(...)
    {
        const auto was_cancelled = stop_after_failed_post(state, job);
        if(was_cancelled)
        {
            delivery->deliver(application::github_viewer_cancelled{
                .Request = request,
            });
        }
        else
        {
            delivery->deliver(application::github_viewer_error{
                .Request = request,
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
    }
}

void github_provider::cancel_viewer(
    const application::github_viewer_request request)
{
    if(Implementation != nullptr)
    {
        cancel_jobs(Implementation->State, provider_phase::viewer, request);
    }
}

void github_provider::probe_capabilities(
    application::github_capability_request request,
    application::github_capability_completion_handler completion)
{
    if(Implementation == nullptr || !valid_request(request))
    {
        completion(application::github_capability_error{
            .Request = std::move(request),
            .Error = provider_error(
                application::github_provider_failure::unavailable),
        });
        return;
    }
    const auto state = Implementation->State;
    const auto profile =
        find_exact_profile(*state, request.ProfileAuthority);
    const auto account_transport =
        exact_account_transport(request.AccountTransport,
                                request.ProfileAuthority,
                                request.Context.ApplicationSession,
                                request.Context.IdentityGeneration,
                                request.Viewer.Account,
                                request.Context.Request);
    if(profile == nullptr || account_transport == nullptr)
    {
        completion(application::github_capability_error{
            .Request = std::move(request),
            .Error = provider_error(
                application::github_provider_failure::unavailable),
        });
        return;
    }
    std::shared_ptr<provider_completion_delivery<
        application::github_capability_completion_handler>>
        delivery;
    try
    {
        delivery = make_delivery(std::move(completion));
    }
    catch(...)
    {
        completion(application::github_capability_error{
            .Request = std::move(request),
            .Error = provider_error(
                application::github_provider_failure::response_limit),
        });
        return;
    }
    const auto job_result = make_job(
        state, provider_phase::capability, request.Context, request.Attempt);
    if(!job_result.has_value())
    {
        if(job_result.error() == make_job_failure::closing)
        {
            delivery->deliver(application::github_capability_cancelled{
                .Request = std::move(request),
            });
        }
        else
        {
            delivery->deliver(application::github_capability_error{
                .Request = std::move(request),
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
        return;
    }
    const auto job = *job_result;
    try
    {
        state->Effects->post(
            [state,
             job,
             profile,
             account_transport,
             request,
             delivery]() mutable
            {
                const auto cancelled = [&]
                {
                    finish_job(state, job);
                    delivery->deliver(application::github_capability_cancelled{
                        .Request = request,
                    });
                };
                const auto failed =
                    [&](application::github_provider_error error)
                {
                    if(finish_job(state, job))
                    {
                        delivery->deliver(
                            application::github_capability_cancelled{
                                .Request = request,
                            });
                        return;
                    }
                    delivery->deliver(application::github_capability_error{
                        .Request = request,
                        .Error = std::move(error),
                    });
                };
                try
                {
                    if(job->Stop.stop_requested())
                    {
                        cancelled();
                        return;
                    }
                    const auto compiled =
                        compile_capability_probe_request(*profile);
                    if(!compiled.has_value())
                    {
                        failed(
                            provider_error(provider_failure(compiled.error())));
                        return;
                    }
                    const github_capability_probe_wire_request wire{
                        .Correlation =
                            github_capability_probe_wire_correlation{
                                .Session = request.Context,
                                .ProfileRevision =
                                    request.ProfileAuthority.ProfileRevision,
                                .Host = request.ProfileAuthority.Host,
                                .ObservedAccount = request.Viewer.Account,
                            },
                        .Wire = *compiled,
                    };
                    const auto terminal =
                        account_transport->Transport->perform_capability_probe(
                            wire, job->Stop.get_token());
                    if(terminal.valueless_by_exception())
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                malformed_response));
                        return;
                    }
                    if(const auto* wire_cancelled =
                           std::get_if<github_capability_probe_wire_cancelled>(
                               &terminal);
                       wire_cancelled != nullptr)
                    {
                        if(wire_cancelled->Correlation != wire.Correlation)
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    malformed_response));
                        }
                        else
                        {
                            cancelled();
                        }
                        return;
                    }
                    if(const auto* error =
                           std::get_if<github_capability_probe_wire_error>(
                               &terminal);
                       error != nullptr)
                    {
                        if(error->Correlation != wire.Correlation
                           || !application::valid(error->Error)
                           || !rate_limits_are_empty_or_exact(
                               error->Error.RateLimits,
                               application::github_rate_limit_resource::
                                   graphql))
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    malformed_response));
                        }
                        else
                        {
                            failed(error->Error);
                        }
                        return;
                    }
                    const auto* success =
                        std::get_if<github_capability_probe_wire_success>(
                            &terminal);
                    if(success == nullptr
                       || success->Correlation != wire.Correlation
                       || success->Response.RateLimitHeaderCount
                              > success->Response.RateLimitHeaders.size())
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                malformed_response));
                        return;
                    }

                    wire_response metadata_response{
                        .Status = success->Response.Status,
                        .Headers =
                            {
                                http_header{
                                    .Name = "Content-Type",
                                    .Value = success->Response.ContentType,
                                },
                            },
                        .Body = {},
                        .Completeness = page_completeness::complete,
                    };
                    metadata_response.Headers.reserve(
                        success->Response.RateLimitHeaderCount + 1U);
                    for(std::size_t index = 0U;
                        index < success->Response.RateLimitHeaderCount;
                        ++index)
                    {
                        const auto& header =
                            success->Response.RateLimitHeaders[index];
                        metadata_response.Headers.push_back(http_header{
                            .Name = header.Name,
                            .Value = header.Value,
                        });
                    }
                    const auto metadata =
                        parse_github_response_metadata(metadata_response);
                    const auto server = expected_server(*profile);
                    if(!metadata.has_value())
                    {
                        failed(metadata.error());
                        return;
                    }
                    if(!server.has_value()
                       || (!metadata->RateLimit.has_value()
                           && *server == server_contract::github_com)
                       || (metadata->RateLimit.has_value()
                           && metadata->RateLimit->Resource
                                  != application::github_rate_limit_resource::
                                      graphql))
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                malformed_response));
                        return;
                    }
                    auto rate_limits = rate_limit_state(metadata->RateLimit);
                    const auto observation = normalize_capability_probe(
                        capability_probe_transport_result{success->Response});
                    if(!observation.Complete || !valid(observation))
                    {
                        auto failure = provider_error(
                            valid(observation)
                                ? provider_failure(observation.Unavailability)
                                : application::github_provider_failure::
                                      malformed_response);
                        failure.RateLimits = rate_limits;
                        if(failure.Failure
                           == application::github_provider_failure::rate_limit)
                        {
                            if(!metadata->RateLimit.has_value())
                            {
                                failed(provider_error(
                                    application::github_provider_failure::
                                        malformed_response));
                                return;
                            }
                            auto cause = *metadata->RateLimit;
                            cause.SecondaryLimited = cause.Remaining != 0U;
                            static_cast<void>(application::observe(
                                failure.RateLimits, cause));
                            failure.RateLimitCause = cause;
                            failure.RetryAfterSeconds =
                                metadata->RetryAfterSeconds.has_value()
                                    ? metadata->RetryAfterSeconds
                                    : (cause.SecondaryLimited
                                           ? std::optional<std::uint32_t>{60U}
                                           : std::nullopt);
                        }
                        else if(metadata->RetryAfterSeconds.has_value())
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    malformed_response));
                            return;
                        }
                        failed(application::valid(failure)
                                   ? failure
                                   : provider_error(
                                         application::github_provider_failure::
                                             malformed_response));
                        return;
                    }
                    if(metadata->RetryAfterSeconds.has_value())
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                malformed_response));
                        return;
                    }
                    auto facts = capability_facts(observation.Capabilities);
                    auto detailed = schema_facts(observation);
                    if(finish_job(state, job))
                    {
                        delivery->deliver(
                            application::github_capability_cancelled{
                                .Request = request,
                            });
                        return;
                    }
                    delivery->deliver(application::github_capability_success{
                        .Request = request,
                        .Facts = std::move(facts),
                        .SchemaFacts = std::move(detailed),
                        .RateLimits = std::move(rate_limits),
                    });
                }
                catch(...)
                {
                    failed(provider_error(
                        application::github_provider_failure::response_limit));
                }
            });
    }
    catch(...)
    {
        const auto was_cancelled = stop_after_failed_post(state, job);
        if(was_cancelled)
        {
            delivery->deliver(application::github_capability_cancelled{
                .Request = std::move(request),
            });
        }
        else
        {
            delivery->deliver(application::github_capability_error{
                .Request = std::move(request),
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
    }
}

void github_provider::cancel_capabilities(
    const application::github_capability_request request)
{
    if(Implementation != nullptr)
    {
        cancel_jobs(Implementation->State, provider_phase::capability, request);
    }
}

void github_provider::fetch_pull_request(
    application::github_pull_request_provider_request request,
    application::github_pull_request_completion_handler completion)
{
    if(Implementation == nullptr || !valid_request(request))
    {
        completion(application::github_pull_request_error{
            .Request = std::move(request),
            .Error = provider_error(
                application::github_provider_failure::unavailable),
        });
        return;
    }
    const auto state = Implementation->State;
    const auto profile =
        find_exact_profile(*state, request.ProfileAuthority);
    const auto account_transport =
        exact_account_transport(request.AccountTransport,
                                request.ProfileAuthority,
                                request.Context.ApplicationSession,
                                request.Context.IdentityGeneration,
                                request.Context.Account);
    if(profile == nullptr || account_transport == nullptr)
    {
        completion(application::github_pull_request_error{
            .Request = std::move(request),
            .Error = provider_error(
                application::github_provider_failure::unavailable),
        });
        return;
    }
    std::shared_ptr<provider_completion_delivery<
        application::github_pull_request_completion_handler>>
        delivery;
    try
    {
        delivery = make_delivery(std::move(completion));
    }
    catch(...)
    {
        completion(application::github_pull_request_error{
            .Request = std::move(request),
            .Error = provider_error(
                application::github_provider_failure::response_limit),
        });
        return;
    }
    const auto job_result = make_job(
        state, provider_phase::pull_request, request.Context, request.Attempt);
    if(!job_result.has_value())
    {
        if(job_result.error() == make_job_failure::closing)
        {
            delivery->deliver(application::github_pull_request_cancelled{
                .Request = std::move(request),
            });
        }
        else
        {
            delivery->deliver(application::github_pull_request_error{
                .Request = std::move(request),
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
        return;
    }
    const auto job = *job_result;
    try
    {
        state->Effects->post(
            [state,
             job,
             profile,
             account_transport,
             request,
             delivery]() mutable
            {
                const auto cancelled = [&]
                {
                    finish_job(state, job);
                    delivery->deliver(
                        application::github_pull_request_cancelled{
                            .Request = request,
                        });
                };
                const auto failed =
                    [&](application::github_provider_error error)
                {
                    if(finish_job(state, job))
                    {
                        delivery->deliver(
                            application::github_pull_request_cancelled{
                                .Request = request,
                            });
                        return;
                    }
                    delivery->deliver(application::github_pull_request_error{
                        .Request = request,
                        .Error = std::move(error),
                    });
                };
                try
                {
                    if(job->Stop.stop_requested())
                    {
                        cancelled();
                        return;
                    }
                    const auto snapshot = allocate_snapshot(state);
                    if(!snapshot.has_value())
                    {
                        failed(provider_error(
                            application::github_provider_failure::
                                response_limit));
                        return;
                    }
                    auto result =
                        perform_paged_github_read(*account_transport->Transport,
                                                  *profile,
                                                  request,
                                                  *snapshot,
                                                  job->Stop.get_token());
                    if(!result.has_value())
                    {
                        auto& error = result.error();
                        if(error.valueless_by_exception())
                        {
                            failed(provider_error(
                                application::github_provider_failure::
                                    malformed_response));
                        }
                        else if(std::holds_alternative<
                                    github_paged_read_cancelled>(error))
                        {
                            cancelled();
                        }
                        else
                        {
                            const auto& provider =
                                std::get<application::github_provider_error>(
                                    error);
                            failed(
                                application::valid(provider)
                                    ? provider
                                    : provider_error(
                                          application::github_provider_failure::
                                              malformed_response));
                        }
                        return;
                    }
                    if(finish_job(state, job))
                    {
                        delivery->deliver(
                            application::github_pull_request_cancelled{
                                .Request = request,
                            });
                        return;
                    }
                    delivery->deliver(application::github_pull_request_success{
                        .Request = request,
                        .Snapshot = std::move(*result),
                    });
                }
                catch(...)
                {
                    failed(provider_error(
                        application::github_provider_failure::response_limit));
                }
            });
    }
    catch(...)
    {
        const auto was_cancelled = stop_after_failed_post(state, job);
        if(was_cancelled)
        {
            delivery->deliver(application::github_pull_request_cancelled{
                .Request = std::move(request),
            });
        }
        else
        {
            delivery->deliver(application::github_pull_request_error{
                .Request = std::move(request),
                .Error = provider_error(
                    application::github_provider_failure::response_limit),
            });
        }
    }
}

void github_provider::cancel_pull_request(
    const application::github_pull_request_provider_request request)
{
    if(Implementation != nullptr)
    {
        cancel_jobs(
            Implementation->State, provider_phase::pull_request, request);
    }
}

void github_provider::shutdown() noexcept
{
    if(Implementation == nullptr)
    {
        return;
    }
    const auto state = Implementation->State;
    const std::scoped_lock lock{state->Mutex};
    if(state->Closing)
    {
        return;
    }
    state->Closing = true;
    for(const auto& job : state->Jobs)
    {
        job->Stop.request_stop();
    }
}

namespace
{
auto make_github_provider_from_pointers(
    const std::span<const validated_github_host_profile* const> profiles,
    application::effect_executor& effects,
    std::shared_ptr<github_wire_transport_factory_port> transport_factory)
    noexcept
    -> github_provider_factory_result
{
    if(profiles.empty())
    {
        return std::unexpected{
            github_provider_configuration_failure::no_profiles};
    }
    if(transport_factory == nullptr)
    {
        return std::unexpected{
            github_provider_configuration_failure::invalid_profile};
    }
    for(std::size_t index = 0U; index < profiles.size(); ++index)
    {
        if(profiles[index] == nullptr
           || !application::valid(profile_authority(*profiles[index])))
        {
            return std::unexpected{
                github_provider_configuration_failure::invalid_profile};
        }
        const auto previous_profiles = profiles.first(index);
        const auto duplicate = std::ranges::find_if(
            previous_profiles,
            [&](const auto& profile)
            {
                return profile != nullptr
                    && profile->profile() == profiles[index]->profile();
            });
        if(duplicate != previous_profiles.end())
        {
            return std::unexpected{
                github_provider_configuration_failure::duplicate_profile};
        }
    }
    try
    {
        auto state = std::make_shared<provider_state>();
        state->Effects = &effects;
        state->TransportFactory = std::move(transport_factory);
        state->Profiles.reserve(profiles.size());
        for(const auto* const profile : profiles)
        {
            state->Profiles.push_back(
                std::make_shared<const validated_github_host_profile>(
                    *profile));
        }
        auto implementation =
            std::make_unique<github_provider::implementation>(std::move(state));
        return std::make_unique<github_provider>(std::move(implementation));
    }
    catch(...)
    {
        return std::unexpected{
            github_provider_configuration_failure::allocation_failure};
    }
}
}

auto make_github_provider(
    const std::span<const validated_github_host_profile> profiles,
    application::effect_executor& effects,
    std::shared_ptr<github_wire_transport_factory_port> transport_factory)
    noexcept
    -> github_provider_factory_result
{
    try
    {
        auto profile_pointers =
            std::vector<const validated_github_host_profile*>{};
        profile_pointers.reserve(profiles.size());
        for(const auto& profile : profiles)
        {
            profile_pointers.push_back(&profile);
        }
        return make_github_provider_from_pointers(
            profile_pointers, effects, std::move(transport_factory));
    }
    catch(...)
    {
        return std::unexpected{
            github_provider_configuration_failure::allocation_failure};
    }
}

auto make_github_provider(
    const std::span<const validated_github_host_profile* const> profiles,
    application::effect_executor& effects,
    std::shared_ptr<github_wire_transport_factory_port> transport_factory)
    noexcept
    -> github_provider_factory_result
{
    return make_github_provider_from_pointers(
        profiles, effects, std::move(transport_factory));
}
}
