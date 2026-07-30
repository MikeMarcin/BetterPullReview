#include "bpr/app/github_application.hpp"

#include "bpr/app/worker_executor.hpp"

#include "github_host_profile.hpp"
#include "github_provider.hpp"

#if defined(__APPLE__)
#include "macos_keychain.hpp"
#elif defined(_WIN32)
#include "windows_credential_manager.hpp"
#elif defined(__linux__)
#include "linux_secret_service.hpp"
#endif

#if defined(BPR_BUILD_TESTS)
#include "github_public_path_fixture.hpp"
#endif

#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/sensitive_text_input.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::app
{
namespace
{
using adapters::github::github_host_profile_candidate;
using adapters::github::github_profile_provenance;
using adapters::github::validated_github_host_profile;
using adapters::github::validate_github_host_profile;
using adapters::github::detail::github_provider;
using adapters::github::detail::github_wire_transport_factory_port;
using adapters::github::detail::make_github_provider;
using adapters::github::detail::native_github_wire_transport_factory;

class unavailable_credential_vault final
    : public application::github_credential_vault_port
{
public:
    explicit unavailable_credential_vault(
        application::effect_executor& effects)
        : Effects{effects}
    {
    }

    void store(
        application::github_credential_enrollment enrollment,
        application::github_credential_store_completion_handler completion)
        override
    {
        const auto context = enrollment.Context;
        auto pending = std::make_shared<
            application::github_credential_enrollment>(
            std::move(enrollment));
        Effects.post(
            [pending = std::move(pending),
             context,
             completion = std::move(completion)]() mutable
            {
                pending->Credential.reset();
                completion(application::github_credential_store_error{
                    .Context = context,
                    .Failure = application::github_credential_vault_failure::
                        unavailable,
                });
            });
    }

    void retrieve(
        const application::github_credential_operation_context context,
        application::github_credential_retrieve_completion_handler completion)
        override
    {
        Effects.post(
            [context, completion = std::move(completion)]() mutable
            {
                completion(application::github_credential_retrieve_error{
                    .Context = context,
                    .Failure = application::github_credential_vault_failure::
                        unavailable,
                });
            });
    }

    void delete_credential(
        const application::github_credential_operation_context context,
        application::github_credential_delete_completion_handler completion)
        override
    {
        Effects.post(
            [context, completion = std::move(completion)]() mutable
            {
                completion(application::github_credential_delete_error{
                    .Context = context,
                    .Failure = application::github_credential_vault_failure::
                        unavailable,
                });
            });
    }

    void cancel(
        const application::github_credential_operation_context) override
    {
    }

private:
    application::effect_executor& Effects;
};

[[nodiscard]] auto native_profile_candidate(
    const core::host_profile_id profile,
    const core::git_host_id host,
    const application::github_profile_revision revision,
    const application::github_server_contract server,
    const application::github_authentication_transport authentication,
    std::string hostname,
    const github_profile_provenance provenance)
    -> github_host_profile_candidate
{
    const auto cloud =
        server
        == application::github_server_contract::github_com_2022_11_28;
    auto rest_root = cloud ? std::string{"https://api.github.com"}
                           : std::string{"https://"} + hostname + "/api/v3";
    auto graphql_root =
        cloud ? std::string{"https://api.github.com/graphql"}
              : std::string{"https://"} + hostname + "/api/graphql";
    return {
        .Profile = profile,
        .Host = host,
        .Revision = revision,
        .Server = server,
        .Authentication = authentication,
        .Proxy = application::github_proxy_mode::system,
        .Trust = application::github_trust_mode::system,
        .Provenance = provenance,
        .Hostname = std::move(hostname),
        .RestRoot = std::move(rest_root),
        .GraphqlRoot = std::move(graphql_root),
    };
}

[[nodiscard]] auto profile_option(
    const validated_github_host_profile& profile,
    std::string label) -> application::github_profile_option
{
    return {
        .Profile = profile.profile(),
        .Label = std::move(label),
        .Hostname = std::string{profile.hostname()},
        .Product =
            profile.product_kind()
                    == application::github_product_kind::github_com
                ? application::github_profile_product::github_com
                : application::github_profile_product::
                    github_enterprise_server,
        .Transport =
            profile.authentication_transport()
                    == application::github_authentication_transport::
                        github_cli
                ? application::github_profile_transport::github_cli
                : application::github_profile_transport::native_https,
    };
}

void append_profile(
    std::deque<validated_github_host_profile>& profiles,
    std::vector<application::github_profile_option>& options,
    const github_host_profile_candidate& candidate,
    std::string label)
{
    const auto profile = validate_github_host_profile(candidate);
    if(!profile.has_value())
    {
        throw std::invalid_argument{"Invalid GitHub host profile"};
    }
    profiles.push_back(*profile);
    options.push_back(profile_option(*profile, std::move(label)));
}

[[nodiscard]] auto make_platform_vault(
    application::effect_executor& effects)
    -> std::unique_ptr<application::github_credential_vault_port>
{
#if defined(__APPLE__)
    auto vault = adapters::github::make_macos_keychain_vault(effects);
#elif defined(_WIN32)
    auto vault =
        adapters::github::make_windows_credential_manager_vault(effects);
#elif defined(__linux__)
    auto vault = adapters::github::make_linux_secret_service_vault(effects);
#else
    static_cast<void>(effects);
    return {};
#endif
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
    return vault.has_value() ? std::move(*vault) : nullptr;
#endif
}

[[nodiscard]] auto initial_github_state() -> core::github_state
{
    return {
        .ApplicationSession = core::application_session_id{1U},
        .NextRequest = core::request_id{1U},
        .ActiveProfile = std::nullopt,
        .IdentityGeneration = {},
        .PendingSession = std::nullopt,
        .Host = std::nullopt,
        .Account = std::nullopt,
        .Capabilities = {},
        .CapabilityRevision = {},
        .PullRequestGeneration = {},
        .PendingPullRequest = std::nullopt,
        .ActiveLocator = std::nullopt,
        .ActiveNumber = std::nullopt,
        .Repository = std::nullopt,
        .PullRequest = std::nullopt,
        .AcceptedSnapshot = std::nullopt,
    };
}

[[nodiscard]] auto prompt_for_credential(
    QWidget* parent) -> std::optional<application::github_credential_material>
{
    auto input = qt::prompt_for_sensitive_text(
        application::maximum_github_credential_bytes,
        parent);
    if(!input.has_value())
    {
        return std::nullopt;
    }

    auto material =
        application::github_credential_material::create(input->bytes());
    input->reset();
    if(!material.has_value())
    {
        return std::nullopt;
    }
    return std::move(*material);
}
}

class github_application::implementation final
{
public:
    explicit implementation(
        github_application_config config
#if defined(BPR_BUILD_TESTS)
        ,
        const std::optional<detail::github_public_path_fixture> fixture =
            std::nullopt
#endif
    )
    {
#if defined(BPR_BUILD_TESTS)
        if(fixture.has_value())
        {
            const auto enterprise =
                *fixture
                == detail::github_public_path_fixture::ghes_3_18;
            append_profile(
                Profiles,
                ProfileOptions,
                native_profile_candidate(
                    core::host_profile_id{enterprise ? 3U : 1U},
                    core::git_host_id{enterprise ? 2U : 1U},
                    application::github_profile_revision{1U},
                    enterprise
                        ? application::github_server_contract::
                            ghes_3_18_2022_11_28
                        : application::github_server_contract::
                            github_com_2022_11_28,
                    application::github_authentication_transport::native_https,
                    enterprise ? "ghe.example.invalid" : "github.com",
                    github_profile_provenance::builtin),
                enterprise ? "GHES 3.18 — Recorded native HTTPS"
                           : "GitHub.com — Recorded native HTTPS");
            Fixture = std::make_unique<
                detail::github_public_path_fixture_components>(
                *fixture, Profiles.front(), VaultEffects);
        }
        else
#endif
        {
            append_profile(
                Profiles,
                ProfileOptions,
                native_profile_candidate(
                    core::host_profile_id{1U},
                    core::git_host_id{1U},
                    application::github_profile_revision{1U},
                    application::github_server_contract::
                        github_com_2022_11_28,
                    application::github_authentication_transport::native_https,
                    "github.com",
                    github_profile_provenance::builtin),
                "GitHub.com — Native HTTPS");
            append_profile(
                Profiles,
                ProfileOptions,
                native_profile_candidate(
                    core::host_profile_id{2U},
                    core::git_host_id{1U},
                    application::github_profile_revision{1U},
                    application::github_server_contract::
                        github_com_2022_11_28,
                    application::github_authentication_transport::github_cli,
                    "github.com",
                    github_profile_provenance::builtin),
                "GitHub.com — GitHub CLI (unavailable)");
            if(config.EnterpriseHostname.has_value())
            {
                append_profile(
                    Profiles,
                    ProfileOptions,
                    native_profile_candidate(
                        core::host_profile_id{3U},
                        core::git_host_id{2U},
                        application::github_profile_revision{1U},
                        application::github_server_contract::
                            ghes_3_18_2022_11_28,
                        application::github_authentication_transport::
                            native_https,
                        *config.EnterpriseHostname,
                        github_profile_provenance::explicit_user),
                    "GHES 3.18 — Native HTTPS");
                append_profile(
                    Profiles,
                    ProfileOptions,
                    native_profile_candidate(
                        core::host_profile_id{4U},
                        core::git_host_id{2U},
                        application::github_profile_revision{1U},
                        application::github_server_contract::
                            ghes_3_18_2022_11_28,
                        application::github_authentication_transport::
                            github_cli,
                        *config.EnterpriseHostname,
                        github_profile_provenance::explicit_user),
                    "GHES 3.18 — GitHub CLI (unavailable)");
            }
        }

#if defined(BPR_BUILD_TESTS)
        if(Fixture != nullptr)
        {
            TransportFactory = Fixture->transport_factory();
        }
        else
#endif
        {
            TransportFactory =
                std::make_shared<native_github_wire_transport_factory>();
        }

        auto profile_pointers =
            std::vector<const validated_github_host_profile*>{};
        profile_pointers.reserve(Profiles.size());
        for(const auto& profile : Profiles)
        {
            profile_pointers.push_back(&profile);
        }
        auto provider = make_github_provider(
            std::span<const validated_github_host_profile* const>{
                profile_pointers},
            ProviderEffects,
            TransportFactory);
        if(!provider.has_value())
        {
            throw std::runtime_error{"GitHub provider composition failed"};
        }
        Provider = std::move(*provider);

#if defined(BPR_BUILD_TESTS)
        if(Fixture == nullptr)
#endif
        {
            OwnedVault = make_platform_vault(VaultEffects);
            NativePlatformVaultSelected = OwnedVault != nullptr;
            if(OwnedVault == nullptr)
            {
                OwnedVault =
                    std::make_unique<unavailable_credential_vault>(
                        VaultEffects);
            }
        }

        Window = std::make_unique<qt::github_window>(
            qt::github_view_config{.Profiles = ProfileOptions},
            [this](application::github_intent intent)
            { dispatch_intent(std::move(intent)); });

        Host = std::make_unique<application::github_application_host>(
            initial_github_state(),
            *Provider,
            *Provider,
            *Provider,
            *Provider,
            credential_vault(),
            CredentialBindings,
            DecisionLane,
            *Window,
            application::github_credential_shutdown_policy::
                delete_retained_on_shutdown);
    }

    ~implementation()
    {
        begin_shutdown();
        while(!ShutdownComplete)
        {
            poll_shutdown();
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::yield();
        }
    }

    [[nodiscard]] auto credential_vault() noexcept
        -> application::github_credential_vault_port&
    {
#if defined(BPR_BUILD_TESTS)
        if(Fixture != nullptr)
        {
            return Fixture->credential_vault();
        }
#endif
        return *OwnedVault;
    }

    [[nodiscard]] auto find_profile(const core::host_profile_id profile) const
        -> const validated_github_host_profile*
    {
        const auto found = std::ranges::find_if(
            Profiles,
            [profile](const validated_github_host_profile& candidate)
            { return candidate.profile() == profile; });
        return found == Profiles.end() ? nullptr : &*found;
    }

    void dispatch_intent(application::github_intent intent)
    {
        if(Closing || Host == nullptr)
        {
            return;
        }
        std::visit(
            [this](auto value)
            {
                using intent_type = decltype(value);
                if constexpr(std::is_same_v<
                                 intent_type,
                                 core::activate_host_profile_intent>)
                {
                    const auto* profile = find_profile(value.Profile);
                    if(profile == nullptr)
                    {
                        return;
                    }
                    if(profile->authentication_transport()
                       == application::github_authentication_transport::
                           github_cli)
                    {
                        Host->dispatch(core::github_event{value});
                        return;
                    }

                    auto credential = std::optional<
                        application::github_credential_material>{};
#if defined(BPR_BUILD_TESTS)
                    if(Fixture != nullptr)
                    {
                        credential.emplace(
                            Fixture->take_enrollment_credential());
                    }
                    else
#endif
                    {
                        credential = prompt_for_credential(Window.get());
                    }
                    if(credential.has_value())
                    {
                        Host->enroll_profile(
                            value.Profile, std::move(*credential));
                    }
                }
                else
                {
                    Host->dispatch(core::github_event{std::move(value)});
                }
            },
            std::move(intent));
    }

    void begin_shutdown()
    {
        if(Closing)
        {
            return;
        }
        Closing = true;
        Host->shutdown();
        schedule_shutdown_poll();
    }

    void schedule_shutdown_poll()
    {
        if(ShutdownComplete || ShutdownPollScheduled || Window == nullptr)
        {
            return;
        }
        ShutdownPollScheduled = true;
        QTimer::singleShot(
            0,
            Window.get(),
            [this]
            {
                ShutdownPollScheduled = false;
                poll_shutdown();
            });
    }

    void poll_shutdown()
    {
        if(!Closing || ShutdownComplete)
        {
            return;
        }
        if(!Host->is_quiescent())
        {
            schedule_shutdown_poll();
            return;
        }

        ShutdownCleanupFailed = Host->shutdown_cleanup_failed();
        Provider->shutdown();
        ProviderEffects.close_and_drain();
        VaultEffects.close_and_drain();
        ShutdownComplete = true;
    }

    std::deque<validated_github_host_profile> Profiles;
    std::vector<application::github_profile_option> ProfileOptions;
    worker_executor ProviderEffects;
    worker_executor VaultEffects;
#if defined(BPR_BUILD_TESTS)
    std::unique_ptr<detail::github_public_path_fixture_components> Fixture;
#endif
    std::unique_ptr<application::github_credential_vault_port> OwnedVault;
    std::shared_ptr<github_wire_transport_factory_port> TransportFactory;
    std::unique_ptr<github_provider> Provider;
    qt::event_loop_executor DecisionLane;
    std::unique_ptr<qt::github_window> Window;
    application::github_credential_binding_catalog CredentialBindings;
    std::unique_ptr<application::github_application_host> Host;
    bool Closing = false;
    bool ShutdownPollScheduled = false;
    bool ShutdownComplete = false;
    bool ShutdownCleanupFailed = false;
    bool NativePlatformVaultSelected = false;
};

github_application::github_application(github_application_config config)
    : Implementation{
          std::make_unique<implementation>(std::move(config))}
{
}

github_application::github_application(
    std::unique_ptr<implementation> implementation) noexcept
    : Implementation{std::move(implementation)}
{
}

github_application::~github_application() = default;

void github_application::show()
{
    Implementation->Window->show();
}

void github_application::begin_shutdown()
{
    Implementation->begin_shutdown();
}

auto github_application::window() noexcept -> qt::github_window&
{
    return *Implementation->Window;
}

auto github_application::status() const noexcept -> github_application_status
{
    return {
        .Closing = Implementation->Closing,
        .ShutdownComplete = Implementation->ShutdownComplete,
        .ShutdownCleanupFailed = Implementation->ShutdownCleanupFailed,
        .NativePlatformVaultSelected =
            Implementation->NativePlatformVaultSelected,
    };
}

#if defined(BPR_BUILD_TESTS)
auto detail::github_application_fixture_access::make(
    const detail::github_public_path_fixture fixture)
    -> std::unique_ptr<github_application>
{
    return std::unique_ptr<github_application>{new github_application{
        std::make_unique<github_application::implementation>(
            github_application_config{}, fixture)}};
}

auto detail::github_application_fixture_access::evidence(
    const github_application& application) noexcept
    -> detail::github_public_path_fixture_evidence
{
    return application.Implementation->Fixture != nullptr
        ? application.Implementation->Fixture->evidence()
        : detail::github_public_path_fixture_evidence{};
}
#endif
}
