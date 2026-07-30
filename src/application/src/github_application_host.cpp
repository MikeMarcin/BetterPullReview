#include "bpr/application/github_application_host.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <variant>

namespace bpr::application
{
namespace
{
[[nodiscard]] constexpr auto
retryable(const github_provider_error& error) noexcept -> bool
{
    if(!valid(error)
       || (error.RetryAfterSeconds.has_value()
           && *error.RetryAfterSeconds != 0U)
       || is_limited(error.RateLimits))
    {
        return false;
    }
    return error.Failure == github_provider_failure::network
           || error.Failure == github_provider_failure::timeout
           || error.Failure == github_provider_failure::unavailable;
}

[[nodiscard]] constexpr auto
mapped_failure(const github_provider_failure failure) noexcept
    -> core::github_failure
{
    switch(failure)
    {
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

[[nodiscard]] auto capability_key(const github_profile_authority& profile,
                                  const core::github_account_id account)
    -> github_capability_authority_key
{
    return {
        .Profile = profile.Profile,
        .ProfileRevision = profile.ProfileRevision,
        .Host = profile.Host,
        .Account = account,
        .ServerContract = profile.ServerContract,
        .ProbeContractRevision = profile.ProbeContractRevision,
    };
}

[[nodiscard]] auto credential_key(const github_profile_authority& profile,
                                  const core::github_account_id account)
    -> github_credential_authority_key
{
    return {
        .Profile = profile.Profile,
        .ProfileRevision = profile.ProfileRevision,
        .Host = profile.Host,
        .Account = account,
        .ServerContract = profile.ServerContract,
    };
}

[[nodiscard]] constexpr auto
valid_rate_limit_resources(const github_rate_limit_state& state,
                           const bool allow_core,
                           const bool allow_search,
                           const bool allow_graphql) noexcept -> bool
{
    return valid(state) && (allow_core || !state.Core.has_value())
           && (allow_search || !state.Search.has_value())
           && (allow_graphql || !state.Graphql.has_value());
}

[[nodiscard]] constexpr auto
requires_rate_limit_observations(const github_server_contract contract) noexcept
    -> bool
{
    return contract == github_server_contract::github_com_2022_11_28;
}

[[nodiscard]] auto
valid_viewer(const github_profile_authority& authority,
             const core::github_session_request_context& context,
             const github_viewer_record& viewer,
             const github_rate_limit_state& rate_limits) noexcept -> bool
{
    if(!valid_rate_limit_resources(rate_limits, true, false, false)
       || is_secondary_limited(rate_limits)
       || (requires_rate_limit_observations(authority.ServerContract)
           && !rate_limits.Core.has_value()))
    {
        return false;
    }
    const github_session_read_model model{
        .Profile = authority.Profile,
        .Host = authority.Host,
        .Viewer = viewer,
        .CapabilityEvidence =
            {
                .Capabilities = {},
                .Revision = core::capability_revision{1U},
                .ReadShape = {},
                .ContractVersion = 1U,
            },
        .RateLimits = rate_limits,
    };
    return validate(model, context).valid();
}

[[nodiscard]] constexpr auto valid_capability_rate_limits(
    const github_profile_authority& authority,
    const github_rate_limit_state& rate_limits) noexcept -> bool
{
    return valid_rate_limit_resources(rate_limits, false, false, true)
           && !is_secondary_limited(rate_limits)
           && (!requires_rate_limit_observations(authority.ServerContract)
               || rate_limits.Graphql.has_value());
}

[[nodiscard]] constexpr auto valid_snapshot_rate_limits(
    const github_profile_authority& authority,
    const github_pull_request_snapshot& snapshot) noexcept -> bool
{
    if(!valid(snapshot.RateLimits) || is_secondary_limited(snapshot.RateLimits))
    {
        return false;
    }
    const auto uses_graphql =
        snapshot.CapabilityEvidence.ReadShape.ReviewThreads
        || snapshot.CapabilityEvidence.ReadShape.ViewedFileState;
    if(snapshot.RateLimits.Graphql.has_value() && !uses_graphql)
    {
        return false;
    }
    if(!requires_rate_limit_observations(authority.ServerContract))
    {
        return true;
    }
    return snapshot.RateLimits.Core.has_value()
           && snapshot.RateLimits.Search.has_value()
           && snapshot.RateLimits.Graphql.has_value() == uses_graphql;
}

[[nodiscard]] constexpr auto
valid_provider_error_resources(const github_provider_error& error,
                               const bool allow_core,
                               const bool allow_search,
                               const bool allow_graphql) noexcept -> bool
{
    return valid(error)
           && valid_rate_limit_resources(
               error.RateLimits, allow_core, allow_search, allow_graphql);
}

[[nodiscard]] auto projected_capability_evidence(
    const github_capability_cache_entry& entry) noexcept
    -> github_capability_evidence
{
    return {
        .Capabilities = entry.Capabilities,
        .Revision = entry.Revision,
        .ReadShape = entry.ReadShape,
        .ContractVersion = 1U,
    };
}

template <typename Completion>
[[nodiscard]] auto completion_context(const Completion& completion)
{
    return std::visit([](const auto& terminal) { return terminal.Context; },
                      completion);
}

template <typename Completion>
[[nodiscard]] decltype(auto)
provider_completion_request(const Completion& completion)
{
    return std::visit([](const auto& terminal) -> const auto&
                      { return terminal.Request; },
                      completion);
}

[[nodiscard]] auto create_host_capability_cache() -> github_capability_cache
{
    auto cache = github_capability_cache::create({});
    if(!cache.has_value())
    {
        throw std::bad_alloc{};
    }
    return std::move(*cache);
}

}

github_credential_binding_catalog::~github_credential_binding_catalog() =
    default;

auto github_credential_binding_catalog::find(
    const core::host_profile_id profile) const noexcept
    -> const github_credential_authority_key*
{
    if(profile.value() == 0U)
    {
        return nullptr;
    }
    for(const auto& entry : Entries)
    {
        if(entry.has_value() && entry->Profile == profile)
        {
            return &*entry;
        }
    }
    return nullptr;
}

auto github_credential_binding_catalog::remember(
    const github_credential_authority_key authority) noexcept -> bool
{
    if(!valid(authority))
    {
        return false;
    }
    for(const auto& entry : Entries)
    {
        if(entry.has_value() && entry->Profile == authority.Profile)
        {
            return *entry == authority;
        }
    }
    for(auto& entry : Entries)
    {
        if(!entry.has_value())
        {
            entry = authority;
            return true;
        }
    }
    return false;
}

void github_credential_binding_catalog::forget(
    const github_credential_authority_key& authority) noexcept
{
    if(!valid(authority))
    {
        return;
    }
    for(auto& entry : Entries)
    {
        if(entry == authority)
        {
            entry.reset();
            return;
        }
    }
}

auto github_credential_binding_catalog::size() const noexcept -> std::size_t
{
    std::size_t result = 0U;
    for(const auto& entry : Entries)
    {
        if(entry.has_value())
        {
            ++result;
        }
    }
    return result;
}

struct github_application_host::implementation final
{
    enum class viewer_mode : std::uint8_t
    {
        none,
        cli,
        bootstrap,
        account,
    };

    enum class vault_operation : std::uint8_t
    {
        none,
        store,
        retrieve,
        delete_credential,
    };

    enum class credential_lifecycle : std::uint8_t
    {
        none,
        provisional,
        persistent,
    };

    struct deferred_session_terminal
    {
        core::github_session_request_context Context;
        std::optional<core::github_failure> Failure;
    };

    struct session_candidate
    {
        core::github_session_request_context Context;
        github_profile_authority ProfileAuthority;
        github_viewer_record Viewer;
        github_capability_cache_entry CapabilityEvidence;
        github_account_transport AccountTransport;
        github_rate_limit_state RateLimits;
        std::optional<github_capability_cache_stage> CacheStage;
    };

    struct deferred_dispatch
    {
        core::github_event Event;
    };

    struct deferred_enrollment
    {
        core::host_profile_id Profile;
        github_credential_material Credential;
    };

    struct deferred_resume
    {
        core::host_profile_id Profile;
    };

    struct deferred_shutdown
    {
    };

    using deferred_ingress = std::variant<deferred_dispatch,
                                          deferred_enrollment,
                                          deferred_resume,
                                          deferred_shutdown>;

    using pending_provider_request =
        std::variant<std::monostate,
                     github_profile_request,
                     github_viewer_request,
                     github_capability_request,
                     github_pull_request_provider_request>;

    struct completion_gate
    {
        std::recursive_mutex Barrier;
        std::atomic<implementation*> Host = nullptr;
    };

    implementation(core::github_state initial_state,
                   github_profile_port& profiles,
                   github_viewer_port& viewers,
                   github_capability_port& capabilities,
                   github_pull_request_port& pull_requests,
                   github_credential_vault_port& credentials,
                   github_credential_binding_catalog& credential_bindings,
                   decision_executor& decision_lane,
                   github_projection_port& projections)
        : State{std::move(initial_state)}
        , Profiles{profiles}
        , Viewers{viewers}
        , Capabilities{capabilities}
        , PullRequests{pull_requests}
        , Credentials{credentials}
        , CredentialBindings{credential_bindings}
        , CapabilityCache{create_host_capability_cache()}
        , DecisionLane{decision_lane}
        , Projections{projections}
        , CompletionGate{std::make_shared<completion_gate>()}
    {
        CompletionGate->Host.store(this, std::memory_order_release);
    }

    ~implementation()
    {
        const std::lock_guard barrier{CompletionGate->Barrier};
        CompletionGate->Host.store(nullptr, std::memory_order_release);
    }

    template <typename Action> void run_turn(Action&& action)
    {
        TurnActive = true;
        try
        {
            std::forward<Action>(action)();
            const auto deferred_count = DeferredIngress.size();
            for(std::size_t index = 0U; index < deferred_count; ++index)
            {
                auto deferred = std::move(DeferredIngress.front());
                DeferredIngress.pop_front();
                std::visit(
                    [this](auto&& ingress)
                    {
                        using ingress_type = std::decay_t<decltype(ingress)>;
                        if constexpr(std::is_same_v<ingress_type,
                                                    deferred_dispatch>)
                        {
                            dispatch_now(ingress.Event);
                        }
                        else if constexpr(std::is_same_v<ingress_type,
                                                         deferred_enrollment>)
                        {
                            enroll_profile_now(ingress.Profile,
                                               std::move(ingress.Credential));
                        }
                        else if constexpr(std::is_same_v<ingress_type,
                                                         deferred_resume>)
                        {
                            resume_profile_now(ingress.Profile);
                        }
                        else
                        {
                            shutdown_now();
                        }
                    },
                    std::move(deferred));
            }
            TurnActive = false;
            schedule_deferred_ingress();
        }
        catch(...)
        {
            DeferredIngress.clear();
            TurnActive = false;
            throw;
        }
    }

    void schedule_deferred_ingress() noexcept
    {
        if(DeferredIngress.empty() || DeferredIngressScheduled)
        {
            return;
        }
        DeferredIngressScheduled = true;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        try
        {
            DecisionLane.post(
                [gate]
                {
                    const auto locked = gate.lock();
                    if(!locked)
                    {
                        return;
                    }
                    const std::lock_guard completion_barrier{locked->Barrier};
                    auto* const host =
                        locked->Host.load(std::memory_order_acquire);
                    if(host == nullptr)
                    {
                        return;
                    }
                    host->DeferredIngressScheduled = false;
                    host->run_turn([] {});
                });
        }
        catch(...)
        {
            DeferredIngressScheduled = false;
            DeferredIngress.clear();
            ProjectionDeliveryFailed = true;
        }
    }

    void dispatch(const core::github_event& event)
    {
        if(Closing)
        {
            return;
        }
        if(TurnActive)
        {
            DeferredIngress.emplace_back(deferred_dispatch{event});
            return;
        }
        if(DeferredIngressScheduled || !DeferredIngress.empty())
        {
            DeferredIngress.emplace_back(deferred_dispatch{event});
            schedule_deferred_ingress();
            return;
        }
        run_turn([this, &event] { dispatch_now(event); });
    }

    void dispatch_now(const core::github_event& event)
    {
        if(Closing)
        {
            return;
        }
        const auto intent =
            std::holds_alternative<core::activate_host_profile_intent>(event)
            || std::holds_alternative<core::deactivate_github_identity_intent>(
                event)
            || std::holds_alternative<core::open_pull_request_intent>(event)
            || std::holds_alternative<core::refresh_pull_request_intent>(event);
        if(!intent)
        {
            return;
        }
        if(std::holds_alternative<core::activate_host_profile_intent>(event))
        {
            NextEnrollment.reset();
            NextResumeProfile.reset();
        }
        dispatch_core(event);
    }

    void enroll_profile(const core::host_profile_id profile,
                        github_credential_material credential)
    {
        if(Closing)
        {
            return;
        }
        if(TurnActive)
        {
            DeferredIngress.emplace_back(deferred_enrollment{
                .Profile = profile,
                .Credential = std::move(credential),
            });
            return;
        }
        if(DeferredIngressScheduled || !DeferredIngress.empty())
        {
            DeferredIngress.emplace_back(deferred_enrollment{
                .Profile = profile,
                .Credential = std::move(credential),
            });
            schedule_deferred_ingress();
            return;
        }
        run_turn([this, profile, credential = std::move(credential)]() mutable
                 { enroll_profile_now(profile, std::move(credential)); });
    }

    void enroll_profile_now(const core::host_profile_id profile,
                            github_credential_material credential)
    {
        if(Closing)
        {
            return;
        }
        NextEnrollment.reset();
        NextResumeProfile.reset();
        if(valid(credential))
        {
            NextEnrollment.emplace(std::move(credential));
        }
        try
        {
            dispatch_core(
                core::activate_host_profile_intent{.Profile = profile});
        }
        catch(...)
        {
            NextEnrollment.reset();
            throw;
        }
        // A valid activation synchronously moved this material into the
        // session scratch state. An invalid activation emitted no request, so
        // retaining it here would let a later activation consume the wrong
        // enrollment secret.
        NextEnrollment.reset();
    }

    void resume_profile(const core::host_profile_id profile)
    {
        if(Closing)
        {
            return;
        }
        if(TurnActive)
        {
            DeferredIngress.emplace_back(deferred_resume{
                .Profile = profile,
            });
            return;
        }
        if(DeferredIngressScheduled || !DeferredIngress.empty())
        {
            DeferredIngress.emplace_back(deferred_resume{
                .Profile = profile,
            });
            schedule_deferred_ingress();
            return;
        }
        run_turn([this, profile] { resume_profile_now(profile); });
    }

    void resume_profile_now(const core::host_profile_id profile)
    {
        if(Closing || profile.value() == 0U)
        {
            return;
        }
        NextEnrollment.reset();
        NextResumeProfile = profile;
        try
        {
            dispatch_core(core::activate_host_profile_intent{
                .Profile = profile,
            });
        }
        catch(...)
        {
            NextResumeProfile.reset();
            throw;
        }
        NextResumeProfile.reset();
    }

    void dispatch_core(const core::github_event& event)
    {
        auto output = core::transition(State, event);
        for(const auto& request : output.Requests)
        {
            const auto* fetch =
                std::get_if<core::fetch_pull_request_request>(&request);
            if(fetch != nullptr
               && !fetch->Context.ExpectedRepository.has_value()
               && !fetch->Context.ExpectedPullRequest.has_value())
            {
                StagedSnapshot.reset();
                AcceptedSnapshot.reset();
            }
        }
        publish_and_execute(output);
    }

    void publish_and_execute(const core::github_transition_output& output)
    {
        if(Closing)
        {
            return;
        }
        publish_transition(output);
        for(const auto& request : output.Requests)
        {
            execute(request);
        }
    }

    template <typename Delivery>
    void deliver_projection(Delivery&& delivery) noexcept
    {
        try
        {
            std::forward<Delivery>(delivery)();
        }
        catch(...)
        {
            ProjectionDeliveryFailed = true;
        }
    }

    void
    publish_transition(const core::github_transition_output& output) noexcept
    {
        deliver_projection([this, &output] { Projections.publish(output); });
    }

    void publish_session(const github_session_read_model& session) noexcept
    {
        deliver_projection([this, &session]
                           { Projections.publish_session(session); });
    }

    void publish_snapshot(const github_pull_request_snapshot& snapshot) noexcept
    {
        deliver_projection([this, &snapshot]
                           { Projections.publish_snapshot(snapshot); });
    }

    void publish_diagnostic(const github_host_diagnostic& diagnostic) noexcept
    {
        deliver_projection([this, &diagnostic]
                           { Projections.publish_diagnostic(diagnostic); });
    }

    void observe_provider_error(const github_provider_error& error)
    {
        if(valid(error))
        {
            static_cast<void>(merge(RateLimits, error.RateLimits));
        }
    }

    void publish_error_advisory(const github_provider_error& error)
    {
        if(valid(error)
           && (error.RetryAfterSeconds.has_value()
               || has_observations(error.RateLimits)))
        {
            publish_diagnostic(github_host_diagnostic{
                .Reason = core::reason_for(mapped_failure(error.Failure)),
                .RetryAfterSeconds = error.RetryAfterSeconds,
                .RateLimits = RateLimits,
                .RateLimitCause = error.RateLimitCause,
            });
        }
    }

    void execute(const core::github_host_request& request)
    {
        std::visit(
            [this](const auto& typed_request)
            {
                using request_type = std::decay_t<decltype(typed_request)>;
                if constexpr(std::is_same_v<
                                 request_type,
                                 core::invalidate_github_identity_request>)
                {
                    invalidate(typed_request);
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      core::establish_github_session_request>)
                {
                    DeferredSession = typed_request.Context;
                    SessionEnrollment = std::move(NextEnrollment);
                    NextEnrollment.reset();
                    SessionResumeProfile = NextResumeProfile;
                    NextResumeProfile.reset();
                    if(ActiveVaultOperation == vault_operation::none)
                    {
                        start_deferred_session();
                    }
                }
                else
                {
                    start_pull_request(typed_request.Context);
                }
            },
            request);
    }

    void invalidate(const core::invalidate_github_identity_request& request)
    {
        cancel_provider();
        const auto* resume_binding =
            NextResumeProfile.has_value()
                ? CredentialBindings.find(*NextResumeProfile)
                : nullptr;
        const auto preserve_for_exact_resume =
            CredentialLifecycle == credential_lifecycle::persistent
            && CredentialAuthority.has_value()
            && CredentialSessionContext.has_value() && resume_binding != nullptr
            && *resume_binding == *CredentialAuthority;
        const auto vault_operation_running =
            ActiveVaultOperation == vault_operation::store
            || ActiveVaultOperation == vault_operation::retrieve;
        if(vault_operation_running)
        {
            if(ActiveVaultContext.has_value())
            {
                try
                {
                    Credentials.cancel(*ActiveVaultContext);
                }
                catch(...)
                {
                }
            }
            DeferredInvalidationGeneration = request.NewGeneration;
            DeferredInvalidationPreservesCredential = preserve_for_exact_resume;
        }
        else
        {
            cancel_vault();
            DeferredInvalidationGeneration.reset();
            DeferredInvalidationPreservesCredential = false;
        }
        StagedSession.reset();
        StagedSnapshot.reset();
        AcceptedSnapshot.reset();
        RateLimits = {};
        AccountTransport.reset();
        CurrentProfile.reset();
        BootstrapViewer.reset();
        SessionEnrollment.reset();
        SessionResumeProfile.reset();
        ViewerMode = viewer_mode::none;
        DeferredTerminal.reset();
        DeferredSession.reset();

        if(request.Previous.Profile.has_value()
           && request.Previous.Host.has_value()
           && request.Previous.Account.has_value())
        {
            static_cast<void>(
                CapabilityCache.invalidate_identity(*request.Previous.Profile,
                                                    *request.Previous.Host,
                                                    *request.Previous.Account));
        }
        else if(request.Previous.Profile.has_value())
        {
            static_cast<void>(
                CapabilityCache.invalidate_profile(*request.Previous.Profile));
        }

        AcceptedSession.reset();
        AcceptedProfileAuthority.reset();
        AcceptedCapabilityEvidence.reset();
        if(CredentialAuthority.has_value()
           && CredentialSessionContext.has_value())
        {
            if(!vault_operation_running && !preserve_for_exact_resume)
            {
                start_delete(request.NewGeneration, std::nullopt);
            }
        }
    }

    void select_persistent_credential(
        const github_credential_authority_key& authority,
        const core::github_session_request_context& context)
    {
        CredentialAuthority = authority;
        CredentialSessionContext = context;
        CredentialLifecycle = credential_lifecycle::persistent;
        CredentialBindingRemembered = true;
    }

    void start_deferred_session()
    {
        if(Closing || !DeferredSession.has_value()
           || ActiveVaultOperation != vault_operation::none)
        {
            return;
        }

        const auto context = *DeferredSession;
        DeferredSession.reset();
        if(SessionResumeProfile.has_value())
        {
            if(const auto* retained =
                   CredentialBindings.find(*SessionResumeProfile);
               retained != nullptr)
            {
                select_persistent_credential(*retained, context);
            }
        }
        const github_profile_request request{
            .Context = context,
            .Attempt = github_provider_attempt{1U},
        };
        submit_profile(request);
    }

    void submit_profile(const github_profile_request& request)
    {
        PendingProvider = request;
        const auto token = ++ProviderOperationToken;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const lane = &DecisionLane;
        const auto callback = [gate, lane, token, request](
                                  github_profile_completion completion) noexcept
        {
            const auto callback_target = gate.lock();
            if(!callback_target)
            {
                return;
            }
            const std::lock_guard callback_barrier{callback_target->Barrier};
            if(callback_target->Host.load(std::memory_order_acquire) == nullptr)
            {
                return;
            }
            try
            {
                auto payload = std::make_shared<github_profile_completion>(
                    std::move(completion));
                lane->post(
                    [gate, payload, token]() mutable
                    {
                        const auto locked = gate.lock();
                        if(!locked)
                        {
                            return;
                        }
                        const std::lock_guard completion_barrier{
                            locked->Barrier};
                        auto* const host =
                            locked->Host.load(std::memory_order_acquire);
                        if(host != nullptr)
                        {
                            host->handle_profile(token, std::move(*payload));
                        }
                    });
            }
            catch(...)
            {
                try
                {
                    lane->post(
                        [gate, token, request]() mutable
                        {
                            const auto locked = gate.lock();
                            if(!locked)
                            {
                                return;
                            }
                            const std::lock_guard completion_barrier{
                                locked->Barrier};
                            auto* const host =
                                locked->Host.load(std::memory_order_acquire);
                            if(host != nullptr)
                            {
                                host->handle_profile(
                                    token,
                                    github_profile_error{
                                        .Request = request,
                                        .Error =
                                            {
                                                .Failure =
                                                    github_provider_failure::
                                                        response_limit,
                                                .RetryAfterSeconds =
                                                    std::nullopt,
                                                .RateLimits = {},
                                                .RateLimitCause = std::nullopt,
                                            },
                                    });
                            }
                        });
                }
                catch(...)
                {
                }
            }
        };
        try
        {
            Profiles.resolve_profile(request, callback);
        }
        catch(...)
        {
            callback(github_profile_error{
                .Request = request,
                .Error =
                    {
                        .Failure = github_provider_failure::response_limit,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        }
    }

    void handle_profile(const std::uint64_t token,
                        github_profile_completion completion)
    {
        run_turn([this, token, completion = std::move(completion)]() mutable
                 { handle_profile_now(token, std::move(completion)); });
    }

    void handle_profile_now(const std::uint64_t token,
                            github_profile_completion completion)
    {
        if(Closing || token != ProviderOperationToken)
        {
            return;
        }
        const auto* pending =
            std::get_if<github_profile_request>(&PendingProvider);
        if(pending == nullptr)
        {
            return;
        }
        const auto current = *pending;
        if(completion.valueless_by_exception()
           || provider_completion_request(completion) != current)
        {
            PendingProvider = std::monostate{};
            fail_session(current.Context,
                         core::github_failure::malformed_response);
            return;
        }

        if(auto* success = std::get_if<github_profile_success>(&completion))
        {
            const auto context = success->Request.Context;
            PendingProvider = std::monostate{};
            if(!valid(success->Authority)
               || success->Authority.Profile != context.Profile)
            {
                fail_session(context, core::github_failure::malformed_response);
                return;
            }
            CurrentProfile = success->Authority;

            if(success->Authority.Authentication
               == github_authentication_transport::github_cli)
            {
                const auto has_native_material =
                    SessionEnrollment.has_value()
                    || SessionResumeProfile.has_value();
                if(const auto* retained =
                       CredentialBindings.find(context.Profile);
                   retained != nullptr)
                {
                    select_persistent_credential(*retained, context);
                    if(has_native_material)
                    {
                        SessionEnrollment.reset();
                        SessionResumeProfile.reset();
                        fail_session(context,
                                     core::github_failure::malformed_response,
                                     true);
                    }
                    else
                    {
                        DeferredSession = context;
                        start_delete(context.IdentityGeneration, std::nullopt);
                    }
                    return;
                }
                if(has_native_material)
                {
                    SessionEnrollment.reset();
                    SessionResumeProfile.reset();
                    fail_session(context,
                                 core::github_failure::malformed_response);
                    return;
                }
                ViewerMode = viewer_mode::cli;
                submit_viewer(
                    github_viewer_request{
                        .Context = context,
                        .Authority = success->Authority,
                        .Attempt = github_provider_attempt{1U},
                    },
                    std::monostate{});
                return;
            }

            if(SessionEnrollment.has_value()
               && SessionResumeProfile.has_value())
            {
                fail_session(context, core::github_failure::malformed_response);
                return;
            }
            if(SessionEnrollment.has_value())
            {
                if(const auto* retained =
                       CredentialBindings.find(context.Profile);
                   retained != nullptr)
                {
                    select_persistent_credential(*retained, context);
                    DeferredSession = context;
                    start_delete(context.IdentityGeneration, std::nullopt);
                    return;
                }
                ViewerMode = viewer_mode::bootstrap;
                submit_bootstrap_viewer(github_viewer_request{
                    .Context = context,
                    .Authority = success->Authority,
                    .Attempt = github_provider_attempt{1U},
                });
                return;
            }
            if(!SessionResumeProfile.has_value()
               || *SessionResumeProfile != context.Profile)
            {
                fail_session(context, core::github_failure::vault);
                return;
            }

            const auto* retained = CredentialBindings.find(context.Profile);
            if(retained == nullptr)
            {
                fail_session(context, core::github_failure::vault);
                return;
            }
            const auto resume_authority = *retained;
            select_persistent_credential(resume_authority, context);
            SessionResumeProfile.reset();
            if(resume_authority.Profile != success->Authority.Profile
               || resume_authority.ProfileRevision
                      != success->Authority.ProfileRevision
               || resume_authority.Host != success->Authority.Host
               || resume_authority.ServerContract
                      != success->Authority.ServerContract)
            {
                fail_session(
                    context, core::github_failure::malformed_response, true);
                return;
            }
            ViewerMode = viewer_mode::account;
            start_retrieve(github_provider_attempt{1U});
            return;
        }

        if(auto* error = std::get_if<github_profile_error>(&completion))
        {
            const auto error_valid =
                valid(error->Error)
                && !has_observations(error->Error.RateLimits);
            if(error_valid && retryable(error->Error)
               && error->Request.Attempt.value()
                      < maximum_github_provider_attempts)
            {
                auto retry = error->Request;
                retry.Attempt = github_provider_attempt{
                    static_cast<std::uint8_t>(retry.Attempt.value() + 1U)};
                submit_profile(retry);
                return;
            }
            PendingProvider = std::monostate{};
            if(error_valid)
            {
                publish_error_advisory(error->Error);
            }
            fail_session(error->Request.Context,
                         error_valid
                             ? mapped_failure(error->Error.Failure)
                             : core::github_failure::malformed_response);
            return;
        }

        const auto& cancelled = std::get<github_profile_cancelled>(completion);
        PendingProvider = std::monostate{};
        cancel_session(cancelled.Request.Context);
    }

    void submit_bootstrap_viewer(const github_viewer_request& request)
    {
        if(!SessionEnrollment.has_value())
        {
            fail_session(request.Context, core::github_failure::vault);
            return;
        }
        auto copy =
            github_credential_material::create(SessionEnrollment->bytes());
        if(!copy.has_value())
        {
            fail_session(request.Context, core::github_failure::vault);
            return;
        }
        submit_viewer(request,
                      github_bootstrap_credential{
                          .Credential = std::move(*copy),
                      });
    }

    void submit_viewer(const github_viewer_request& request,
                       github_viewer_authentication authentication)
    {
        PendingProvider = request;
        const auto token = ++ProviderOperationToken;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const lane = &DecisionLane;
        const auto callback = [gate, lane, token, request](
                                  github_viewer_completion completion) noexcept
        {
            const auto callback_target = gate.lock();
            if(!callback_target)
            {
                return;
            }
            const std::lock_guard callback_barrier{callback_target->Barrier};
            if(callback_target->Host.load(std::memory_order_acquire) == nullptr)
            {
                return;
            }
            try
            {
                auto payload = std::make_shared<github_viewer_completion>(
                    std::move(completion));
                lane->post(
                    [gate, payload, token]() mutable
                    {
                        const auto locked = gate.lock();
                        if(!locked)
                        {
                            return;
                        }
                        const std::lock_guard completion_barrier{
                            locked->Barrier};
                        auto* const host =
                            locked->Host.load(std::memory_order_acquire);
                        if(host != nullptr)
                        {
                            host->handle_viewer(token, std::move(*payload));
                        }
                    });
            }
            catch(...)
            {
                try
                {
                    lane->post(
                        [gate, token, request]() mutable
                        {
                            const auto locked = gate.lock();
                            if(!locked)
                            {
                                return;
                            }
                            const std::lock_guard completion_barrier{
                                locked->Barrier};
                            auto* const host =
                                locked->Host.load(std::memory_order_acquire);
                            if(host != nullptr)
                            {
                                host->handle_viewer(
                                    token,
                                    github_viewer_error{
                                        .Request = request,
                                        .Error =
                                            {
                                                .Failure =
                                                    github_provider_failure::
                                                        response_limit,
                                                .RetryAfterSeconds =
                                                    std::nullopt,
                                                .RateLimits = {},
                                                .RateLimitCause = std::nullopt,
                                            },
                                    });
                            }
                        });
                }
                catch(...)
                {
                }
            }
        };
        try
        {
            Viewers.observe_viewer(
                github_viewer_submission{
                    .Request = request,
                    .Authentication = std::move(authentication),
                },
                callback);
        }
        catch(...)
        {
            callback(github_viewer_error{
                .Request = request,
                .Error =
                    {
                        .Failure = github_provider_failure::response_limit,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        }
    }

    void handle_viewer(const std::uint64_t token,
                       github_viewer_completion completion)
    {
        run_turn([this, token, completion = std::move(completion)]() mutable
                 { handle_viewer_now(token, std::move(completion)); });
    }

    void handle_viewer_now(const std::uint64_t token,
                           github_viewer_completion completion)
    {
        if(Closing || token != ProviderOperationToken)
        {
            return;
        }
        const auto* pending =
            std::get_if<github_viewer_request>(&PendingProvider);
        if(pending == nullptr)
        {
            return;
        }
        const auto current = *pending;
        if(completion.valueless_by_exception()
           || provider_completion_request(completion) != current)
        {
            PendingProvider = std::monostate{};
            fail_session(current.Context,
                         core::github_failure::malformed_response);
            return;
        }

        if(auto* success = std::get_if<github_viewer_success>(&completion))
        {
            const auto context = success->Request.Context;
            const auto authority = success->Request.Authority;
            PendingProvider = std::monostate{};
            if(!valid_viewer(
                   authority, context, success->Viewer, success->RateLimits))
            {
                fail_session(context, core::github_failure::malformed_response);
                return;
            }

            if(ViewerMode == viewer_mode::bootstrap)
            {
                if(success->AccountTransport != nullptr)
                {
                    fail_session(context,
                                 core::github_failure::malformed_response);
                    return;
                }
                static_cast<void>(merge(RateLimits, success->RateLimits));
                BootstrapViewer = success->Viewer;
                CredentialAuthority =
                    credential_key(authority, success->Viewer.Account);
                CredentialSessionContext = context;
                CredentialLifecycle = credential_lifecycle::provisional;
                if(!CredentialBindings.remember(*CredentialAuthority))
                {
                    fail_session(context, core::github_failure::vault);
                    return;
                }
                CredentialBindingRemembered = true;
                start_store(context);
                return;
            }

            if(success->AccountTransport == nullptr)
            {
                fail_session(context, core::github_failure::malformed_response);
                return;
            }
            if(ViewerMode == viewer_mode::account)
            {
                if(!CredentialAuthority.has_value()
                   || success->Viewer.Account != CredentialAuthority->Account)
                {
                    fail_session(context,
                                 core::github_failure::malformed_response,
                                 true);
                    return;
                }
                if(BootstrapViewer.has_value()
                   && success->Viewer != *BootstrapViewer)
                {
                    fail_session(context,
                                 core::github_failure::malformed_response);
                    return;
                }
            }

            static_cast<void>(merge(RateLimits, success->RateLimits));
            AccountTransport = success->AccountTransport;
            begin_capabilities(
                context, authority, success->Viewer, success->AccountTransport);
            return;
        }

        if(auto* error = std::get_if<github_viewer_error>(&completion))
        {
            const auto error_valid = valid_provider_error_resources(
                error->Error, true, false, false);
            if(error_valid)
            {
                observe_provider_error(error->Error);
            }
            if(error_valid && retryable(error->Error)
               && error->Request.Attempt.value()
                      < maximum_github_provider_attempts)
            {
                auto retry = error->Request;
                retry.Attempt = github_provider_attempt{
                    static_cast<std::uint8_t>(retry.Attempt.value() + 1U)};
                if(ViewerMode == viewer_mode::bootstrap)
                {
                    submit_bootstrap_viewer(retry);
                }
                else if(ViewerMode == viewer_mode::account)
                {
                    PendingProvider = std::monostate{};
                    start_retrieve(retry.Attempt);
                }
                else
                {
                    submit_viewer(retry, std::monostate{});
                }
                return;
            }
            PendingProvider = std::monostate{};
            if(error_valid)
            {
                publish_error_advisory(error->Error);
            }
            fail_session(error->Request.Context,
                         error_valid
                             ? mapped_failure(error->Error.Failure)
                             : core::github_failure::malformed_response);
            return;
        }

        const auto& cancelled = std::get<github_viewer_cancelled>(completion);
        PendingProvider = std::monostate{};
        cancel_session(cancelled.Request.Context);
    }

    [[nodiscard]] auto next_epoch() -> std::optional<github_freshness_epoch>
    {
        if(LogicalEpoch == std::numeric_limits<std::uint64_t>::max())
        {
            return std::nullopt;
        }
        ++LogicalEpoch;
        return github_freshness_epoch{LogicalEpoch};
    }

    void begin_capabilities(const core::github_session_request_context& context,
                            const github_profile_authority& profile,
                            const github_viewer_record& viewer,
                            const github_account_transport& transport)
    {
        const auto epoch = next_epoch();
        if(!epoch.has_value())
        {
            fail_session(context, core::github_failure::malformed_response);
            return;
        }
        const auto key = capability_key(profile, viewer.Account);
        const auto lookup = CapabilityCache.lookup(key, *epoch);
        if(lookup.hit())
        {
            accept_session(session_candidate{
                .Context = context,
                .ProfileAuthority = profile,
                .Viewer = viewer,
                .CapabilityEvidence = *lookup.Entry,
                .AccountTransport = transport,
                .RateLimits = RateLimits,
                .CacheStage = std::nullopt,
            });
            return;
        }
        if(lookup.Status != github_capability_cache_lookup_status::miss
           && lookup.Status != github_capability_cache_lookup_status::expired)
        {
            fail_session(context, core::github_failure::malformed_response);
            return;
        }

        submit_capability(github_capability_request{
            .Context = context,
            .ProfileAuthority = profile,
            .Viewer = viewer,
            .AccountTransport = transport,
            .Attempt = github_provider_attempt{1U},
        });
    }

    void submit_capability(const github_capability_request& request)
    {
        PendingProvider = request;
        const auto token = ++ProviderOperationToken;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const lane = &DecisionLane;
        const auto callback =
            [gate, lane, token, request](
                github_capability_completion completion) noexcept
        {
            const auto callback_target = gate.lock();
            if(!callback_target)
            {
                return;
            }
            const std::lock_guard callback_barrier{callback_target->Barrier};
            if(callback_target->Host.load(std::memory_order_acquire) == nullptr)
            {
                return;
            }
            try
            {
                auto payload = std::make_shared<github_capability_completion>(
                    std::move(completion));
                lane->post(
                    [gate, payload, token]() mutable
                    {
                        const auto locked = gate.lock();
                        if(!locked)
                        {
                            return;
                        }
                        const std::lock_guard completion_barrier{
                            locked->Barrier};
                        auto* const host =
                            locked->Host.load(std::memory_order_acquire);
                        if(host != nullptr)
                        {
                            host->handle_capability(token, std::move(*payload));
                        }
                    });
            }
            catch(...)
            {
                try
                {
                    lane->post(
                        [gate, token, request]() mutable
                        {
                            const auto locked = gate.lock();
                            if(!locked)
                            {
                                return;
                            }
                            const std::lock_guard completion_barrier{
                                locked->Barrier};
                            auto* const host =
                                locked->Host.load(std::memory_order_acquire);
                            if(host != nullptr)
                            {
                                host->handle_capability(
                                    token,
                                    github_capability_error{
                                        .Request = request,
                                        .Error =
                                            {
                                                .Failure =
                                                    github_provider_failure::
                                                        response_limit,
                                                .RetryAfterSeconds =
                                                    std::nullopt,
                                                .RateLimits = {},
                                                .RateLimitCause = std::nullopt,
                                            },
                                    });
                            }
                        });
                }
                catch(...)
                {
                }
            }
        };
        try
        {
            Capabilities.probe_capabilities(request, callback);
        }
        catch(...)
        {
            callback(github_capability_error{
                .Request = request,
                .Error =
                    {
                        .Failure = github_provider_failure::response_limit,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        }
    }

    void handle_capability(const std::uint64_t token,
                           github_capability_completion completion)
    {
        run_turn([this, token, completion = std::move(completion)]() mutable
                 { handle_capability_now(token, std::move(completion)); });
    }

    void handle_capability_now(const std::uint64_t token,
                               github_capability_completion completion)
    {
        if(Closing || token != ProviderOperationToken)
        {
            return;
        }
        const auto* pending =
            std::get_if<github_capability_request>(&PendingProvider);
        if(pending == nullptr)
        {
            return;
        }
        const auto current = *pending;
        if(completion.valueless_by_exception()
           || provider_completion_request(completion) != current)
        {
            PendingProvider = std::monostate{};
            fail_session(current.Context,
                         core::github_failure::malformed_response);
            return;
        }

        if(auto* success = std::get_if<github_capability_success>(&completion))
        {
            PendingProvider = std::monostate{};
            const auto epoch = next_epoch();
            if(!epoch.has_value()
               || !valid_capability_rate_limits(
                   success->Request.ProfileAuthority, success->RateLimits))
            {
                fail_session(success->Request.Context,
                             core::github_failure::malformed_response);
                return;
            }
            const github_capability_probe_observation observation{
                .Authority = capability_key(success->Request.ProfileAuthority,
                                            success->Request.Viewer.Account),
                .ObservedAt = *epoch,
                .Facts = success->Facts,
                .SchemaFacts = success->SchemaFacts,
            };
            const auto staged = CapabilityCache.stage(
                github_capability_probe_result{observation});
            if(!staged.staged())
            {
                fail_session(
                    success->Request.Context,
                    staged.Error
                            == github_capability_cache_error::capacity_exhausted
                        ? core::github_failure::response_too_large
                        : core::github_failure::malformed_response);
                return;
            }
            static_cast<void>(merge(RateLimits, success->RateLimits));
            accept_session(session_candidate{
                .Context = success->Request.Context,
                .ProfileAuthority = success->Request.ProfileAuthority,
                .Viewer = success->Request.Viewer,
                .CapabilityEvidence = staged.Stage->entry(),
                .AccountTransport = success->Request.AccountTransport,
                .RateLimits = RateLimits,
                .CacheStage = *staged.Stage,
            });
            return;
        }

        if(auto* error = std::get_if<github_capability_error>(&completion))
        {
            const auto error_valid = valid_provider_error_resources(
                error->Error, false, false, true);
            if(error_valid)
            {
                observe_provider_error(error->Error);
            }
            if(error_valid && retryable(error->Error)
               && error->Request.Attempt.value()
                      < maximum_github_provider_attempts)
            {
                auto retry = error->Request;
                retry.Attempt = github_provider_attempt{
                    static_cast<std::uint8_t>(retry.Attempt.value() + 1U)};
                submit_capability(retry);
                return;
            }
            PendingProvider = std::monostate{};
            if(error_valid)
            {
                publish_error_advisory(error->Error);
            }
            fail_session(error->Request.Context,
                         error_valid
                             ? mapped_failure(error->Error.Failure)
                             : core::github_failure::malformed_response);
            return;
        }

        const auto& cancelled =
            std::get<github_capability_cancelled>(completion);
        PendingProvider = std::monostate{};
        cancel_session(cancelled.Request.Context);
    }

    void accept_session(session_candidate candidate)
    {
        const github_session_read_model model{
            .Profile = candidate.ProfileAuthority.Profile,
            .Host = candidate.ProfileAuthority.Host,
            .Viewer = candidate.Viewer,
            .CapabilityEvidence =
                {
                    .Capabilities = candidate.CapabilityEvidence.Capabilities,
                    .Revision = candidate.CapabilityEvidence.Revision,
                    .ReadShape = candidate.CapabilityEvidence.ReadShape,
                    .ContractVersion = 1U,
                },
            .RateLimits = candidate.RateLimits,
        };
        if(!validate(model, candidate.Context).valid())
        {
            fail_session(candidate.Context,
                         core::github_failure::malformed_response);
            return;
        }
        if(CredentialAuthority.has_value() && !CredentialBindingRemembered)
        {
            if(!CredentialBindings.remember(*CredentialAuthority))
            {
                SessionEnrollment.reset();
                fail_session(candidate.Context, core::github_failure::vault);
                return;
            }
            CredentialBindingRemembered = true;
        }

        StagedSession = candidate;
        auto output = core::transition(
            State,
            core::github_session_ready_event{
                .Context = candidate.Context,
                .Host = candidate.ProfileAuthority.Host,
                .Account = candidate.Viewer.Account,
                .Capabilities = candidate.CapabilityEvidence.Capabilities,
                .CapabilityRevision = candidate.CapabilityEvidence.Revision,
            });
        const core::accepted_session_delta expected{
            .Context = candidate.Context,
            .Host = candidate.ProfileAuthority.Host,
            .Account = candidate.Viewer.Account,
            .Capabilities = candidate.CapabilityEvidence.Capabilities,
            .CapabilityRevision = candidate.CapabilityEvidence.Revision,
        };
        if(!output.AcceptedSession.has_value()
           || *output.AcceptedSession != expected)
        {
            StagedSession.reset();
            publish_and_execute(output);
            if(CredentialLifecycle == credential_lifecycle::provisional
               && CredentialAuthority.has_value()
               && CredentialSessionContext.has_value()
               && ActiveVaultOperation == vault_operation::none)
            {
                start_delete(candidate.Context.IdentityGeneration,
                             std::nullopt);
            }
            return;
        }

        if(candidate.CacheStage.has_value())
        {
            const auto committed =
                CapabilityCache.commit(*candidate.CacheStage);
            if(!committed.stored()
               || committed.Entry != candidate.CapabilityEvidence)
            {
                StagedSession.reset();
                publish_diagnostic(github_host_diagnostic{
                    .Reason = core::github_reason_code::malformed_response,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                });
                auto rollback = core::transition(
                    State, core::deactivate_github_identity_intent{});
                publish_and_execute(rollback);
                return;
            }
        }

        AcceptedSession = model;
        AcceptedProfileAuthority = candidate.ProfileAuthority;
        AcceptedCapabilityEvidence = candidate.CapabilityEvidence;
        AccountTransport = candidate.AccountTransport;
        RateLimits = candidate.RateLimits;
        StagedSession.reset();
        SessionEnrollment.reset();
        SessionResumeProfile.reset();
        BootstrapViewer.reset();
        ViewerMode = viewer_mode::none;
        if(CredentialAuthority.has_value())
        {
            CredentialLifecycle = credential_lifecycle::persistent;
        }
        publish_transition(output);
        publish_session(*AcceptedSession);
    }

    void start_store(const core::github_session_request_context& context)
    {
        if(!CredentialAuthority.has_value() || !SessionEnrollment.has_value())
        {
            fail_session(context, core::github_failure::vault);
            return;
        }
        const github_credential_operation_context operation{
            .Request = context.Request,
            .ApplicationSession = context.ApplicationSession,
            .Authority = *CredentialAuthority,
            .IdentityGeneration = context.IdentityGeneration,
        };
        ActiveVaultOperation = vault_operation::store;
        ActiveVaultContext = operation;
        const auto token = ++VaultOperationToken;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const lane = &DecisionLane;
        const auto callback =
            [gate, lane, token, operation](
                github_credential_store_completion completion) noexcept
        {
            const auto callback_target = gate.lock();
            if(!callback_target)
            {
                return;
            }
            const std::lock_guard callback_barrier{callback_target->Barrier};
            if(callback_target->Host.load(std::memory_order_acquire) == nullptr)
            {
                return;
            }
            try
            {
                auto payload =
                    std::make_shared<github_credential_store_completion>(
                        std::move(completion));
                lane->post(
                    [gate, payload, token, operation]() mutable
                    {
                        const auto locked = gate.lock();
                        if(!locked)
                        {
                            return;
                        }
                        const std::lock_guard completion_barrier{
                            locked->Barrier};
                        auto* const host =
                            locked->Host.load(std::memory_order_acquire);
                        if(host != nullptr)
                        {
                            host->handle_store(
                                token, operation, std::move(*payload));
                        }
                    });
            }
            catch(...)
            {
                try
                {
                    lane->post(
                        [gate, token, operation]() mutable
                        {
                            const auto locked = gate.lock();
                            if(!locked)
                            {
                                return;
                            }
                            const std::lock_guard completion_barrier{
                                locked->Barrier};
                            auto* const host =
                                locked->Host.load(std::memory_order_acquire);
                            if(host != nullptr)
                            {
                                host->handle_store(
                                    token,
                                    operation,
                                    github_credential_store_error{
                                        .Context = operation,
                                        .Failure =
                                            github_credential_vault_failure::
                                                allocation_failure,
                                    });
                            }
                        });
                }
                catch(...)
                {
                }
            }
        };
        try
        {
            Credentials.store(
                github_credential_enrollment{
                    .Context = operation,
                    .Credential = std::move(*SessionEnrollment),
                },
                callback);
        }
        catch(...)
        {
            callback(github_credential_store_error{
                .Context = operation,
                .Failure = github_credential_vault_failure::operation_failed,
            });
        }
        SessionEnrollment.reset();
    }

    void
    handle_store(const std::uint64_t token,
                 const github_credential_operation_context& expected_operation,
                 github_credential_store_completion completion)
    {
        run_turn(
            [this,
             token,
             expected_operation,
             completion = std::move(completion)]() mutable
            {
                handle_store_now(
                    token, expected_operation, std::move(completion));
            });
    }

    void handle_store_now(
        const std::uint64_t token,
        const github_credential_operation_context& expected_operation,
        github_credential_store_completion completion)
    {
        if(token != VaultOperationToken)
        {
            return;
        }
        ++VaultOperationToken;
        if(Closing)
        {
            if(ActiveVaultOperation == vault_operation::store
               && ActiveVaultContext == expected_operation)
            {
                ActiveVaultOperation = vault_operation::none;
                ActiveVaultContext.reset();
                DeferredInvalidationGeneration.reset();
                if(CredentialLifecycle == credential_lifecycle::provisional)
                {
                    start_delete(expected_operation.IdentityGeneration,
                                 std::nullopt);
                }
            }
            else
            {
                ActiveVaultOperation = vault_operation::none;
                ActiveVaultContext.reset();
                ShutdownCleanupFailed = true;
                publish_diagnostic(github_host_diagnostic{
                    .Reason = core::github_reason_code::vault_unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                });
            }
            return;
        }
        if(DeferredInvalidationGeneration.has_value())
        {
            const auto generation = *DeferredInvalidationGeneration;
            DeferredInvalidationGeneration.reset();
            const auto preserve =
                std::exchange(DeferredInvalidationPreservesCredential, false);
            ActiveVaultOperation = vault_operation::none;
            ActiveVaultContext.reset();
            if(preserve)
            {
                start_deferred_session();
            }
            else
            {
                start_delete(generation, std::nullopt);
            }
            return;
        }
        if(ActiveVaultOperation != vault_operation::store
           || !ActiveVaultContext.has_value()
           || *ActiveVaultContext != expected_operation
           || !CredentialSessionContext.has_value())
        {
            ActiveVaultOperation = vault_operation::none;
            ActiveVaultContext.reset();
            publish_diagnostic(github_host_diagnostic{
                .Reason = core::github_reason_code::vault_unavailable,
                .RetryAfterSeconds = std::nullopt,
                .RateLimits = {},
                .RateLimitCause = std::nullopt,
            });
            if(CredentialSessionContext.has_value())
            {
                fail_session(*CredentialSessionContext,
                             core::github_failure::vault);
            }
            return;
        }
        if(!valid(completion)
           || completion_context(completion) != expected_operation)
        {
            ActiveVaultOperation = vault_operation::none;
            ActiveVaultContext.reset();
            if(CredentialSessionContext.has_value())
            {
                fail_session(*CredentialSessionContext,
                             core::github_failure::vault);
            }
            else
            {
                publish_diagnostic(github_host_diagnostic{
                    .Reason = core::github_reason_code::vault_unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                });
            }
            return;
        }
        const auto session = *CredentialSessionContext;
        ActiveVaultOperation = vault_operation::none;
        ActiveVaultContext.reset();

        if(std::holds_alternative<github_credential_store_success>(completion))
        {
            ViewerMode = viewer_mode::account;
            start_retrieve(github_provider_attempt{1U});
            return;
        }
        if(std::holds_alternative<github_credential_store_cancelled>(
               completion))
        {
            cancel_session(session);
            return;
        }
        fail_session(session, core::github_failure::vault);
    }

    void start_retrieve(const github_provider_attempt attempt)
    {
        if(!CredentialAuthority.has_value()
           || !CredentialSessionContext.has_value())
        {
            return;
        }
        const auto& session = *CredentialSessionContext;
        const github_credential_operation_context operation{
            .Request = session.Request,
            .ApplicationSession = session.ApplicationSession,
            .Authority = *CredentialAuthority,
            .IdentityGeneration = session.IdentityGeneration,
        };
        PendingRetrieveAttempt = attempt;
        ActiveVaultOperation = vault_operation::retrieve;
        ActiveVaultContext = operation;
        const auto token = ++VaultOperationToken;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const lane = &DecisionLane;
        const auto callback =
            [gate, lane, token, operation](
                github_credential_retrieve_completion completion) noexcept
        {
            const auto callback_target = gate.lock();
            if(!callback_target)
            {
                return;
            }
            const std::lock_guard callback_barrier{callback_target->Barrier};
            if(callback_target->Host.load(std::memory_order_acquire) == nullptr)
            {
                return;
            }
            try
            {
                auto payload =
                    std::make_shared<github_credential_retrieve_completion>(
                        std::move(completion));
                lane->post(
                    [gate, payload, token, operation]() mutable
                    {
                        const auto locked = gate.lock();
                        if(!locked)
                        {
                            return;
                        }
                        const std::lock_guard completion_barrier{
                            locked->Barrier};
                        auto* const host =
                            locked->Host.load(std::memory_order_acquire);
                        if(host != nullptr)
                        {
                            host->handle_retrieve(
                                token, operation, std::move(*payload));
                        }
                    });
            }
            catch(...)
            {
                try
                {
                    lane->post(
                        [gate, token, operation]() mutable
                        {
                            const auto locked = gate.lock();
                            if(!locked)
                            {
                                return;
                            }
                            const std::lock_guard completion_barrier{
                                locked->Barrier};
                            auto* const host =
                                locked->Host.load(std::memory_order_acquire);
                            if(host != nullptr)
                            {
                                host->handle_retrieve(
                                    token,
                                    operation,
                                    github_credential_retrieve_error{
                                        .Context = operation,
                                        .Failure =
                                            github_credential_vault_failure::
                                                allocation_failure,
                                    });
                            }
                        });
                }
                catch(...)
                {
                }
            }
        };
        try
        {
            Credentials.retrieve(operation, callback);
        }
        catch(...)
        {
            callback(github_credential_retrieve_error{
                .Context = operation,
                .Failure = github_credential_vault_failure::operation_failed,
            });
        }
    }

    void handle_retrieve(
        const std::uint64_t token,
        const github_credential_operation_context& expected_operation,
        github_credential_retrieve_completion completion)
    {
        run_turn(
            [this,
             token,
             expected_operation,
             completion = std::move(completion)]() mutable
            {
                handle_retrieve_now(
                    token, expected_operation, std::move(completion));
            });
    }

    void handle_retrieve_now(
        const std::uint64_t token,
        const github_credential_operation_context& expected_operation,
        github_credential_retrieve_completion completion)
    {
        if(token != VaultOperationToken)
        {
            return;
        }
        ++VaultOperationToken;
        if(Closing)
        {
            if(ActiveVaultOperation == vault_operation::retrieve
               && ActiveVaultContext == expected_operation)
            {
                ActiveVaultOperation = vault_operation::none;
                ActiveVaultContext.reset();
                DeferredInvalidationGeneration.reset();
                if(CredentialLifecycle == credential_lifecycle::provisional)
                {
                    start_delete(expected_operation.IdentityGeneration,
                                 std::nullopt);
                }
            }
            else
            {
                ActiveVaultOperation = vault_operation::none;
                ActiveVaultContext.reset();
                ShutdownCleanupFailed = true;
                publish_diagnostic(github_host_diagnostic{
                    .Reason = core::github_reason_code::vault_unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                });
            }
            return;
        }
        if(DeferredInvalidationGeneration.has_value())
        {
            const auto generation = *DeferredInvalidationGeneration;
            DeferredInvalidationGeneration.reset();
            const auto preserve =
                std::exchange(DeferredInvalidationPreservesCredential, false);
            ActiveVaultOperation = vault_operation::none;
            ActiveVaultContext.reset();
            if(preserve)
            {
                start_deferred_session();
            }
            else
            {
                start_delete(generation, std::nullopt);
            }
            return;
        }
        if(ActiveVaultOperation != vault_operation::retrieve
           || !ActiveVaultContext.has_value()
           || *ActiveVaultContext != expected_operation
           || !CurrentProfile.has_value()
           || !CredentialSessionContext.has_value())
        {
            ActiveVaultOperation = vault_operation::none;
            ActiveVaultContext.reset();
            publish_diagnostic(github_host_diagnostic{
                .Reason = core::github_reason_code::vault_unavailable,
                .RetryAfterSeconds = std::nullopt,
                .RateLimits = {},
                .RateLimitCause = std::nullopt,
            });
            if(CredentialSessionContext.has_value())
            {
                fail_session(*CredentialSessionContext,
                             core::github_failure::vault);
            }
            return;
        }
        if(!valid(completion)
           || completion_context(completion) != expected_operation)
        {
            const auto context = *CredentialSessionContext;
            ActiveVaultOperation = vault_operation::none;
            ActiveVaultContext.reset();
            fail_session(context, core::github_failure::vault);
            return;
        }
        ActiveVaultOperation = vault_operation::none;
        ActiveVaultContext.reset();
        const auto context = *CredentialSessionContext;

        if(auto* success =
               std::get_if<github_credential_retrieve_success>(&completion))
        {
            submit_viewer(
                github_viewer_request{
                    .Context = context,
                    .Authority = *CurrentProfile,
                    .Attempt = PendingRetrieveAttempt,
                },
                std::move(*success));
            return;
        }
        if(std::holds_alternative<github_credential_retrieve_absent>(
               completion))
        {
            if(CredentialBindingRemembered)
            {
                CredentialBindings.forget(expected_operation.Authority);
            }
            CredentialAuthority.reset();
            CredentialSessionContext.reset();
            CredentialLifecycle = credential_lifecycle::none;
            CredentialBindingRemembered = false;
            fail_session_direct(context, core::github_failure::vault);
            return;
        }
        if(std::holds_alternative<github_credential_retrieve_cancelled>(
               completion))
        {
            cancel_session(context);
            return;
        }
        fail_session(context, core::github_failure::vault);
    }

    void start_delete(const core::github_identity_generation generation,
                      std::optional<deferred_session_terminal> terminal)
    {
        if(!CredentialAuthority.has_value()
           || !CredentialSessionContext.has_value())
        {
            if(terminal.has_value())
            {
                finish_deferred_terminal(*terminal);
            }
            return;
        }
        DeferredTerminal = std::move(terminal);
        const auto& session = *CredentialSessionContext;
        const github_credential_operation_context operation{
            .Request = session.Request,
            .ApplicationSession = session.ApplicationSession,
            .Authority = *CredentialAuthority,
            .IdentityGeneration = generation,
        };
        ActiveVaultOperation = vault_operation::delete_credential;
        ActiveVaultContext = operation;
        const auto token = ++VaultOperationToken;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const lane = &DecisionLane;
        const auto callback =
            [gate, lane, token, operation](
                github_credential_delete_completion completion) noexcept
        {
            const auto callback_target = gate.lock();
            if(!callback_target)
            {
                return;
            }
            const std::lock_guard callback_barrier{callback_target->Barrier};
            if(callback_target->Host.load(std::memory_order_acquire) == nullptr)
            {
                return;
            }
            try
            {
                auto payload =
                    std::make_shared<github_credential_delete_completion>(
                        std::move(completion));
                lane->post(
                    [gate, payload, token, operation]() mutable
                    {
                        const auto locked = gate.lock();
                        if(!locked)
                        {
                            return;
                        }
                        const std::lock_guard completion_barrier{
                            locked->Barrier};
                        auto* const host =
                            locked->Host.load(std::memory_order_acquire);
                        if(host != nullptr)
                        {
                            host->handle_delete(
                                token, operation, std::move(*payload));
                        }
                    });
            }
            catch(...)
            {
                try
                {
                    lane->post(
                        [gate, token, operation]() mutable
                        {
                            const auto locked = gate.lock();
                            if(!locked)
                            {
                                return;
                            }
                            const std::lock_guard completion_barrier{
                                locked->Barrier};
                            auto* const host =
                                locked->Host.load(std::memory_order_acquire);
                            if(host != nullptr)
                            {
                                host->handle_delete(
                                    token,
                                    operation,
                                    github_credential_delete_error{
                                        .Context = operation,
                                        .Failure =
                                            github_credential_vault_failure::
                                                allocation_failure,
                                    });
                            }
                        });
                }
                catch(...)
                {
                }
            }
        };
        try
        {
            Credentials.delete_credential(operation, callback);
        }
        catch(...)
        {
            callback(github_credential_delete_error{
                .Context = operation,
                .Failure = github_credential_vault_failure::operation_failed,
            });
        }
    }

    void
    handle_delete(const std::uint64_t token,
                  const github_credential_operation_context& expected_operation,
                  github_credential_delete_completion completion)
    {
        run_turn(
            [this,
             token,
             expected_operation,
             completion = std::move(completion)]() mutable
            {
                handle_delete_now(
                    token, expected_operation, std::move(completion));
            });
    }

    void handle_delete_now(
        const std::uint64_t token,
        const github_credential_operation_context& expected_operation,
        github_credential_delete_completion completion)
    {
        if(token != VaultOperationToken)
        {
            return;
        }
        ++VaultOperationToken;
        if(Closing)
        {
            if(ActiveVaultOperation != vault_operation::delete_credential
               || !ActiveVaultContext.has_value()
               || *ActiveVaultContext != expected_operation)
            {
                ActiveVaultOperation = vault_operation::none;
                ActiveVaultContext.reset();
                DeferredTerminal.reset();
                ShutdownCleanupFailed = true;
                publish_diagnostic(github_host_diagnostic{
                    .Reason = core::github_reason_code::vault_unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                });
                return;
            }

            if(!valid(completion)
               || completion_context(completion) != expected_operation)
            {
                ActiveVaultOperation = vault_operation::none;
                ActiveVaultContext.reset();
                DeferredTerminal.reset();
                ShutdownCleanupFailed = true;
                publish_diagnostic(github_host_diagnostic{
                    .Reason = core::github_reason_code::vault_unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                });
                return;
            }

            ActiveVaultOperation = vault_operation::none;
            ActiveVaultContext.reset();
            DeferredTerminal.reset();
            if(std::holds_alternative<github_credential_delete_success>(
                   completion))
            {
                if(CredentialBindingRemembered)
                {
                    CredentialBindings.forget(expected_operation.Authority);
                }
                CredentialAuthority.reset();
                CredentialSessionContext.reset();
                CredentialLifecycle = credential_lifecycle::none;
                CredentialBindingRemembered = false;
            }
            else
            {
                ShutdownCleanupFailed = true;
                publish_diagnostic(github_host_diagnostic{
                    .Reason = core::github_reason_code::vault_unavailable,
                    .RetryAfterSeconds = std::nullopt,
                    .RateLimits = {},
                    .RateLimitCause = std::nullopt,
                });
            }
            return;
        }
        if(ActiveVaultOperation != vault_operation::delete_credential
           || !ActiveVaultContext.has_value()
           || *ActiveVaultContext != expected_operation || !valid(completion)
           || completion_context(completion) != expected_operation)
        {
            fail_delete_operation();
            return;
        }
        if(!std::holds_alternative<github_credential_delete_success>(
               completion))
        {
            fail_delete_operation();
            return;
        }

        const auto terminal = DeferredTerminal;
        DeferredTerminal.reset();
        ActiveVaultOperation = vault_operation::none;
        ActiveVaultContext.reset();
        if(CredentialBindingRemembered)
        {
            CredentialBindings.forget(expected_operation.Authority);
        }
        CredentialAuthority.reset();
        CredentialSessionContext.reset();
        CredentialLifecycle = credential_lifecycle::none;
        CredentialBindingRemembered = false;
        if(terminal.has_value())
        {
            finish_deferred_terminal(*terminal);
        }
        else
        {
            start_deferred_session();
        }
    }

    void fail_delete_operation()
    {
        const auto terminal = DeferredTerminal;
        DeferredTerminal.reset();
        ActiveVaultOperation = vault_operation::none;
        ActiveVaultContext.reset();
        publish_diagnostic(github_host_diagnostic{
            .Reason = core::github_reason_code::vault_unavailable,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = {},
            .RateLimitCause = std::nullopt,
        });
        if(terminal.has_value())
        {
            finish_deferred_terminal(deferred_session_terminal{
                .Context = terminal->Context,
                .Failure = core::github_failure::vault,
            });
        }
        else if(DeferredSession.has_value())
        {
            const auto context = *DeferredSession;
            DeferredSession.reset();
            fail_session_direct(context, core::github_failure::vault);
        }
    }

    void fail_session(const core::github_session_request_context& context,
                      const core::github_failure failure,
                      const bool delete_persistent_credential = false)
    {
        cancel_provider();
        if(CredentialAuthority.has_value()
           && CredentialSessionContext.has_value()
           && !AcceptedSession.has_value()
           && ActiveVaultOperation == vault_operation::none
           && (CredentialLifecycle == credential_lifecycle::provisional
               || delete_persistent_credential))
        {
            start_delete(context.IdentityGeneration,
                         deferred_session_terminal{
                             .Context = context,
                             .Failure = failure,
                         });
            return;
        }
        fail_session_direct(context, failure);
    }

    void
    fail_session_direct(const core::github_session_request_context& context,
                        const core::github_failure failure)
    {
        reset_session_scratch();
        dispatch_core(core::github_session_failed_event{
            .Context = context,
            .Failure = failure,
        });
    }

    void cancel_session(const core::github_session_request_context& context)
    {
        cancel_provider();
        if(CredentialAuthority.has_value()
           && CredentialSessionContext.has_value()
           && !AcceptedSession.has_value()
           && ActiveVaultOperation == vault_operation::none
           && CredentialLifecycle == credential_lifecycle::provisional)
        {
            start_delete(context.IdentityGeneration,
                         deferred_session_terminal{
                             .Context = context,
                             .Failure = std::nullopt,
                         });
            return;
        }
        reset_session_scratch();
        dispatch_core(core::github_session_cancelled_event{
            .Context = context,
        });
    }

    void finish_deferred_terminal(const deferred_session_terminal& terminal)
    {
        if(terminal.Failure.has_value())
        {
            fail_session_direct(terminal.Context, *terminal.Failure);
        }
        else
        {
            reset_session_scratch();
            dispatch_core(core::github_session_cancelled_event{
                .Context = terminal.Context,
            });
        }
    }

    void reset_session_scratch()
    {
        PendingProvider = std::monostate{};
        StagedSession.reset();
        CurrentProfile.reset();
        BootstrapViewer.reset();
        AccountTransport.reset();
        SessionEnrollment.reset();
        SessionResumeProfile.reset();
        ViewerMode = viewer_mode::none;
    }

    void start_pull_request(const core::pull_request_request_context& context)
    {
        if(!AcceptedSession.has_value() || !AcceptedProfileAuthority.has_value()
           || !AcceptedCapabilityEvidence.has_value()
           || AccountTransport == nullptr
           || context.Profile != AcceptedProfileAuthority->Profile
           || context.Host != AcceptedProfileAuthority->Host
           || context.Account != AcceptedSession->Viewer.Account
           || context.Capabilities != AcceptedCapabilityEvidence->Revision
           || AcceptedCapabilityEvidence->Authority.Account != context.Account)
        {
            dispatch_core(core::pull_request_load_failed_event{
                .Context = context,
                .Failure = core::github_failure::malformed_response,
            });
            return;
        }
        submit_pull_request(github_pull_request_provider_request{
            .Context = context,
            .ProfileAuthority = *AcceptedProfileAuthority,
            .Viewer = AcceptedSession->Viewer,
            .CapabilityEvidence = *AcceptedCapabilityEvidence,
            .AccountTransport = AccountTransport,
            .Attempt = github_provider_attempt{1U},
        });
    }

    void
    submit_pull_request(const github_pull_request_provider_request& request)
    {
        PendingProvider = request;
        const auto token = ++ProviderOperationToken;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const lane = &DecisionLane;
        const auto callback =
            [gate, lane, token, request](
                github_pull_request_completion completion) noexcept
        {
            const auto callback_target = gate.lock();
            if(!callback_target)
            {
                return;
            }
            const std::lock_guard callback_barrier{callback_target->Barrier};
            if(callback_target->Host.load(std::memory_order_acquire) == nullptr)
            {
                return;
            }
            try
            {
                auto payload = std::make_shared<github_pull_request_completion>(
                    std::move(completion));
                lane->post(
                    [gate, payload, token]() mutable
                    {
                        const auto locked = gate.lock();
                        if(!locked)
                        {
                            return;
                        }
                        const std::lock_guard completion_barrier{
                            locked->Barrier};
                        auto* const host =
                            locked->Host.load(std::memory_order_acquire);
                        if(host != nullptr)
                        {
                            host->handle_pull_request(token,
                                                      std::move(*payload));
                        }
                    });
            }
            catch(...)
            {
                try
                {
                    lane->post(
                        [gate, token, request]() mutable
                        {
                            const auto locked = gate.lock();
                            if(!locked)
                            {
                                return;
                            }
                            const std::lock_guard completion_barrier{
                                locked->Barrier};
                            auto* const host =
                                locked->Host.load(std::memory_order_acquire);
                            if(host != nullptr)
                            {
                                host->handle_pull_request(
                                    token,
                                    github_pull_request_error{
                                        .Request = request,
                                        .Error =
                                            {
                                                .Failure =
                                                    github_provider_failure::
                                                        response_limit,
                                                .RetryAfterSeconds =
                                                    std::nullopt,
                                                .RateLimits = {},
                                                .RateLimitCause = std::nullopt,
                                            },
                                    });
                            }
                        });
                }
                catch(...)
                {
                }
            }
        };
        try
        {
            PullRequests.fetch_pull_request(request, callback);
        }
        catch(...)
        {
            callback(github_pull_request_error{
                .Request = request,
                .Error =
                    {
                        .Failure = github_provider_failure::response_limit,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            });
        }
    }

    void handle_pull_request(const std::uint64_t token,
                             github_pull_request_completion completion)
    {
        run_turn([this, token, completion = std::move(completion)]() mutable
                 { handle_pull_request_now(token, std::move(completion)); });
    }

    void handle_pull_request_now(const std::uint64_t token,
                                 github_pull_request_completion completion)
    {
        if(Closing || token != ProviderOperationToken)
        {
            return;
        }
        const auto* pending =
            std::get_if<github_pull_request_provider_request>(&PendingProvider);
        if(pending == nullptr)
        {
            return;
        }
        const auto current = *pending;
        if(completion.valueless_by_exception()
           || provider_completion_request(completion) != current)
        {
            PendingProvider = std::monostate{};
            dispatch_core(core::pull_request_load_failed_event{
                .Context = current.Context,
                .Failure = core::github_failure::malformed_response,
            });
            return;
        }

        if(auto* success =
               std::get_if<github_pull_request_success>(&completion))
        {
            PendingProvider = std::monostate{};
            const github_actor_record expected_viewer{
                .Actor = success->Request.Viewer.Node,
                .Login = success->Request.Viewer.Login,
            };
            if(success->Snapshot.Viewer != expected_viewer
               || success->Snapshot.CapabilityEvidence
                      != projected_capability_evidence(
                          success->Request.CapabilityEvidence)
               || !AcceptedProfileAuthority.has_value()
               || !valid_snapshot_rate_limits(*AcceptedProfileAuthority,
                                              success->Snapshot)
               || !validate(success->Snapshot, success->Request.Context)
                       .valid())
            {
                dispatch_core(core::pull_request_load_failed_event{
                    .Context = success->Request.Context,
                    .Failure = core::github_failure::malformed_response,
                });
                return;
            }
            StagedSnapshot = success->Snapshot;
            auto output = core::transition(
                State,
                core::pull_request_loaded_event{
                    .Context = success->Request.Context,
                    .Repository = success->Snapshot.Repository.Repository,
                    .PullRequest = success->Snapshot.PullRequest.PullRequest,
                    .Snapshot = success->Snapshot.Snapshot,
                });
            const core::accepted_snapshot_delta expected{
                .Context = success->Request.Context,
                .Repository = success->Snapshot.Repository.Repository,
                .PullRequest = success->Snapshot.PullRequest.PullRequest,
                .Snapshot = success->Snapshot.Snapshot,
            };
            if(output.AcceptedSnapshot.has_value()
               && *output.AcceptedSnapshot == expected)
            {
                AcceptedSnapshot = std::move(StagedSnapshot);
                StagedSnapshot.reset();
                static_cast<void>(
                    merge(RateLimits, AcceptedSnapshot->RateLimits));
                publish_transition(output);
                publish_snapshot(*AcceptedSnapshot);
            }
            else
            {
                StagedSnapshot.reset();
                publish_and_execute(output);
            }
            return;
        }

        if(auto* error = std::get_if<github_pull_request_error>(&completion))
        {
            const auto uses_graphql =
                error->Request.CapabilityEvidence.ReadShape.ReviewThreads
                || error->Request.CapabilityEvidence.ReadShape.ViewedFileState;
            const auto error_valid = valid_provider_error_resources(
                error->Error, true, true, uses_graphql);
            if(error_valid)
            {
                observe_provider_error(error->Error);
            }
            if(error_valid && retryable(error->Error)
               && error->Request.Attempt.value()
                      < maximum_github_provider_attempts)
            {
                auto retry = error->Request;
                retry.Attempt = github_provider_attempt{
                    static_cast<std::uint8_t>(retry.Attempt.value() + 1U)};
                submit_pull_request(retry);
                return;
            }
            PendingProvider = std::monostate{};
            if(error_valid)
            {
                publish_error_advisory(error->Error);
            }
            dispatch_core(core::pull_request_load_failed_event{
                .Context = error->Request.Context,
                .Failure = error_valid
                               ? mapped_failure(error->Error.Failure)
                               : core::github_failure::malformed_response,
            });
            return;
        }

        const auto& cancelled =
            std::get<github_pull_request_cancelled>(completion);
        PendingProvider = std::monostate{};
        dispatch_core(core::pull_request_load_cancelled_event{
            .Context = cancelled.Request.Context,
        });
    }

    void cancel_provider()
    {
        std::visit(
            [this](const auto& request)
            {
                using request_type = std::decay_t<decltype(request)>;
                if constexpr(std::is_same_v<request_type,
                                            github_profile_request>)
                {
                    try
                    {
                        Profiles.cancel_profile(request);
                    }
                    catch(...)
                    {
                    }
                }
                else if constexpr(std::is_same_v<request_type,
                                                 github_viewer_request>)
                {
                    try
                    {
                        Viewers.cancel_viewer(request);
                    }
                    catch(...)
                    {
                    }
                }
                else if constexpr(std::is_same_v<request_type,
                                                 github_capability_request>)
                {
                    try
                    {
                        Capabilities.cancel_capabilities(request);
                    }
                    catch(...)
                    {
                    }
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      github_pull_request_provider_request>)
                {
                    try
                    {
                        PullRequests.cancel_pull_request(request);
                    }
                    catch(...)
                    {
                    }
                }
            },
            PendingProvider);
        PendingProvider = std::monostate{};
        ++ProviderOperationToken;
    }

    void cancel_vault()
    {
        if(ActiveVaultContext.has_value())
        {
            try
            {
                Credentials.cancel(*ActiveVaultContext);
            }
            catch(...)
            {
            }
        }
        ++VaultOperationToken;
        ActiveVaultOperation = vault_operation::none;
        ActiveVaultContext.reset();
    }

    void shutdown()
    {
        if(TurnActive)
        {
            DeferredIngress.emplace_back(deferred_shutdown{});
            return;
        }
        run_turn([this] { shutdown_now(); });
    }

    void shutdown_now()
    {
        {
            const std::lock_guard barrier{CompletionGate->Barrier};
            if(Closing)
            {
                return;
            }

            if(State.ActiveProfile.has_value()
               || State.PendingSession.has_value()
               || State.PendingPullRequest.has_value())
            {
                static_cast<void>(core::transition(
                    State, core::deactivate_github_identity_intent{}));
            }

            Closing = true;
        }

        // Wipe raw host-owned material before an external cancellation can
        // block or re-enter. PendingProvider remains intact until its exact
        // cancellation has been submitted.
        NextEnrollment.reset();
        SessionEnrollment.reset();
        NextResumeProfile.reset();
        SessionResumeProfile.reset();

        // Never call an external port while holding the completion gate.
        // A cancellation implementation may wait for a worker callback that
        // itself needs the gate in order to enqueue its terminal.
        cancel_provider();
        if((ActiveVaultOperation == vault_operation::store
            || ActiveVaultOperation == vault_operation::retrieve)
           && ActiveVaultContext.has_value())
        {
            try
            {
                Credentials.cancel(*ActiveVaultContext);
            }
            catch(...)
            {
            }
        }
        else if(ActiveVaultOperation == vault_operation::delete_credential
                && ActiveVaultContext.has_value())
        {
            // An exact cleanup is already in flight. Its terminal remains the
            // only authority for shutdown quiescence.
        }
        else if(CredentialAuthority.has_value()
                && CredentialSessionContext.has_value()
                && !AcceptedSession.has_value()
                && CredentialLifecycle == credential_lifecycle::provisional)
        {
            start_delete(CredentialSessionContext->IdentityGeneration,
                         std::nullopt);
        }
        else
        {
            cancel_vault();
        }
        static_cast<void>(CapabilityCache.clear());
        DeferredSession.reset();
        reset_session_scratch();
        StagedSnapshot.reset();
        AcceptedSession.reset();
        AcceptedSnapshot.reset();
        AcceptedProfileAuthority.reset();
        AcceptedCapabilityEvidence.reset();
    }

    core::github_state State;
    github_profile_port& Profiles;
    github_viewer_port& Viewers;
    github_capability_port& Capabilities;
    github_pull_request_port& PullRequests;
    github_credential_vault_port& Credentials;
    github_credential_binding_catalog& CredentialBindings;
    github_capability_cache CapabilityCache;
    decision_executor& DecisionLane;
    github_projection_port& Projections;
    std::shared_ptr<completion_gate> CompletionGate;
    std::deque<deferred_ingress> DeferredIngress;
    bool TurnActive = false;
    bool DeferredIngressScheduled = false;

    pending_provider_request PendingProvider;
    std::uint64_t ProviderOperationToken = 0U;
    std::optional<github_credential_material> NextEnrollment;
    std::optional<github_credential_material> SessionEnrollment;
    std::optional<core::host_profile_id> NextResumeProfile;
    std::optional<core::host_profile_id> SessionResumeProfile;
    std::optional<core::github_session_request_context> DeferredSession;
    std::optional<github_profile_authority> CurrentProfile;
    std::optional<github_viewer_record> BootstrapViewer;
    viewer_mode ViewerMode = viewer_mode::none;
    github_account_transport AccountTransport;

    vault_operation ActiveVaultOperation = vault_operation::none;
    std::optional<github_credential_operation_context> ActiveVaultContext;
    std::uint64_t VaultOperationToken = 0U;
    github_provider_attempt PendingRetrieveAttempt{1U};
    std::optional<github_credential_authority_key> CredentialAuthority;
    std::optional<core::github_session_request_context>
        CredentialSessionContext;
    credential_lifecycle CredentialLifecycle = credential_lifecycle::none;
    bool CredentialBindingRemembered = false;
    std::optional<deferred_session_terminal> DeferredTerminal;
    std::optional<core::github_identity_generation>
        DeferredInvalidationGeneration;
    bool DeferredInvalidationPreservesCredential = false;

    std::optional<session_candidate> StagedSession;
    std::optional<github_pull_request_snapshot> StagedSnapshot;
    std::optional<github_session_read_model> AcceptedSession;
    std::optional<github_profile_authority> AcceptedProfileAuthority;
    std::optional<github_capability_cache_entry> AcceptedCapabilityEvidence;
    std::optional<github_pull_request_snapshot> AcceptedSnapshot;
    github_rate_limit_state RateLimits;
    std::uint64_t LogicalEpoch = 0U;
    bool Closing = false;
    bool ShutdownCleanupFailed = false;
    bool ProjectionDeliveryFailed = false;
};

github_application_host::github_application_host(
    core::github_state initial_state,
    github_profile_port& profiles,
    github_viewer_port& viewers,
    github_capability_port& capabilities,
    github_pull_request_port& pull_requests,
    github_credential_vault_port& credentials,
    github_credential_binding_catalog& credential_bindings,
    decision_executor& decision_lane,
    github_projection_port& projections)
    : Implementation{std::make_unique<implementation>(std::move(initial_state),
                                                      profiles,
                                                      viewers,
                                                      capabilities,
                                                      pull_requests,
                                                      credentials,
                                                      credential_bindings,
                                                      decision_lane,
                                                      projections)}
{
}

github_application_host::~github_application_host()
{
    shutdown();
}

void github_application_host::dispatch(const core::github_event& event)
{
    Implementation->dispatch(event);
}

void github_application_host::enroll_profile(
    const core::host_profile_id profile, github_credential_material credential)
{
    Implementation->enroll_profile(profile, std::move(credential));
}

void github_application_host::resume_profile(
    const core::host_profile_id profile)
{
    Implementation->resume_profile(profile);
}

void github_application_host::shutdown()
{
    Implementation->shutdown();
}

auto github_application_host::state() const noexcept
    -> const core::github_state&
{
    return Implementation->State;
}

auto github_application_host::accepted_session() const noexcept
    -> const std::optional<github_session_read_model>&
{
    return Implementation->AcceptedSession;
}

auto github_application_host::accepted_snapshot() const noexcept
    -> const std::optional<github_pull_request_snapshot>&
{
    return Implementation->AcceptedSnapshot;
}

auto github_application_host::rate_limits() const noexcept
    -> const github_rate_limit_state&
{
    return Implementation->RateLimits;
}

auto github_application_host::capability_cache_size() const noexcept
    -> std::size_t
{
    return Implementation->CapabilityCache.size();
}

auto github_application_host::is_closing() const noexcept -> bool
{
    return Implementation->Closing;
}

auto github_application_host::is_quiescent() const noexcept -> bool
{
    return Implementation->Closing
           && std::holds_alternative<std::monostate>(
               Implementation->PendingProvider)
           && Implementation->ActiveVaultOperation
                  == implementation::vault_operation::none;
}

auto github_application_host::shutdown_cleanup_failed() const noexcept -> bool
{
    return Implementation->ShutdownCleanupFailed;
}

auto github_application_host::projection_delivery_failed() const noexcept
    -> bool
{
    return Implementation->ProjectionDeliveryFailed;
}
}
