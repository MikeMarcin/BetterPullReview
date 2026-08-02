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
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/sensitive_text_input.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <filesystem>
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

#if defined(_WIN32)
#include <windows.h>
#endif

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

[[nodiscard]] auto native_filesystem_path(const QString& value)
    -> std::filesystem::path
{
#if defined(_WIN32)
    return std::filesystem::path{value.toStdWString()};
#else
    return std::filesystem::path{value.toStdString()};
#endif
}

[[nodiscard]] auto native_application_data_root(
    const github_application_config& config) -> std::filesystem::path
{
    if(config.ApplicationDataRoot)
    {
        return config.ApplicationDataRoot->lexically_normal();
    }
    return native_filesystem_path(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation));
}

[[nodiscard]] auto native_git_executable(
    const github_application_config& config) -> std::filesystem::path
{
    if(config.GitExecutable)
    {
        return config.GitExecutable->lexically_normal();
    }
    return native_filesystem_path(
        QFileInfo{QStandardPaths::findExecutable(QStringLiteral("git"))}
            .canonicalFilePath());
}

[[nodiscard]] auto prepare_application_data_root(
    const std::filesystem::path& root) -> std::filesystem::path
{
    if(root.empty() || !root.is_absolute())
    {
        return {};
    }
    std::error_code error;
    static_cast<void>(std::filesystem::create_directories(root, error));
    if(error)
    {
        return {};
    }
    std::filesystem::permissions(root,
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace,
                                 error);
    if(error)
    {
        return {};
    }
    const auto canonical = std::filesystem::canonical(root, error);
    return !error && canonical == root.lexically_normal() ? canonical
                                                          : std::filesystem::path{};
}

[[nodiscard]] auto windows_system_root()
    -> std::optional<std::filesystem::path>
{
#if defined(_WIN32)
    auto value = std::array<wchar_t, MAX_PATH + 1U>{};
    const auto length = GetWindowsDirectoryW(
        value.data(), static_cast<UINT>(value.size()));
    if(length == 0U || length >= value.size())
    {
        return std::nullopt;
    }
    return std::filesystem::path{value.data()};
#else
    return std::nullopt;
#endif
}

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
            std::nullopt,
        detail::github_public_path_fixture_options fixture_options = {}
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
                *fixture,
                Profiles.front(),
                VaultEffects,
                std::move(fixture_options));
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

        auto application_data_root = std::filesystem::path{};
        auto git_executable = std::filesystem::path{};
#if defined(BPR_BUILD_TESTS)
        if(Fixture != nullptr)
        {
            application_data_root = Fixture->application_data_root();
            git_executable = Fixture->git_executable();
        }
        else
#endif
        {
            application_data_root =
                prepare_application_data_root(
                    native_application_data_root(config));
            git_executable = native_git_executable(config);
        }
        if(application_data_root.empty() || git_executable.empty())
        {
            throw std::runtime_error{
                "Application data or installed Git is unavailable"};
        }

        auto cache = adapters::git::make_installed_git_cache({
            .GitExecutable = git_executable,
            .StorageRoot = application_data_root,
            .WindowsSystemRoot = windows_system_root(),
        });
        auto catalog = adapters::sqlite::make_epoch_catalog({
            .StorageRoot = application_data_root,
        });
        if(!cache || !catalog)
        {
            throw std::runtime_error{
                "Git cache or epoch catalog composition failed"};
        }
        GitCaches = std::move(*cache);
        EpochCatalogs = std::move(*catalog);
        EpochEntropy = application::make_system_epoch_entropy();
        EpochClock = application::make_system_epoch_clock();
        if(!EpochEntropy || !EpochClock)
        {
            throw std::runtime_error{"Epoch runtime ports are unavailable"};
        }

        Window = std::make_unique<qt::github_window>(
            qt::github_view_config{.Profiles = ProfileOptions},
            [this](application::github_intent intent)
            { dispatch_intent(std::move(intent)); });

        EpochHost = std::make_unique<application::epoch_application_host>(
            core::epoch_state{
                .ApplicationSession = core::application_session_id{1U},
            },
            *GitCaches,
            *EpochCatalogs,
            *EpochEntropy,
            *EpochClock,
            EpochEffects,
            DecisionLane,
            *Window->epoch_control());
        Window->epoch_control()->set_dispatch(
            [this](application::epoch_intent intent)
            { dispatch_epoch_intent(std::move(intent)); });

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
                delete_retained_on_shutdown,
            EpochHost.get());
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

    void dispatch_epoch_intent(application::epoch_intent intent)
    {
        if(Closing || EpochHost == nullptr)
        {
            return;
        }
#if defined(BPR_BUILD_TESTS)
        if(std::holds_alternative<core::prune_review_epochs_intent>(intent))
        {
            ++EpochPruneIntentDispatchCount;
        }
        else if(std::holds_alternative<core::delete_repository_cache_intent>(
                    intent))
        {
            ++EpochDeleteIntentDispatchCount;
        }
        else if(std::holds_alternative<core::cancel_epoch_maintenance_intent>(
                    intent))
        {
            ++EpochCancelIntentDispatchCount;
        }
#endif
        EpochHost->dispatch(std::move(intent));
    }

    void begin_shutdown()
    {
        if(Closing)
        {
            return;
        }
        Closing = true;
        EpochHost->shutdown();
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
        if(!EpochHost->is_quiescent() || !Host->is_quiescent())
        {
            schedule_shutdown_poll();
            return;
        }

        ShutdownCleanupFailed = Host->shutdown_cleanup_failed();
        EpochProjectionDeliveryFailed =
            EpochHost->projection_delivery_failed();
        EpochEffects.close_and_drain();
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
    std::size_t EpochPruneIntentDispatchCount = 0U;
    std::size_t EpochDeleteIntentDispatchCount = 0U;
    std::size_t EpochCancelIntentDispatchCount = 0U;
#endif
    std::unique_ptr<application::github_credential_vault_port> OwnedVault;
    std::shared_ptr<github_wire_transport_factory_port> TransportFactory;
    std::unique_ptr<github_provider> Provider;
    qt::event_loop_executor DecisionLane;
    std::unique_ptr<qt::github_window> Window;
    application::github_credential_binding_catalog CredentialBindings;
    std::unique_ptr<application::git_cache_port> GitCaches;
    std::unique_ptr<application::epoch_catalog_port> EpochCatalogs;
    std::unique_ptr<application::epoch_entropy_port> EpochEntropy;
    std::unique_ptr<application::epoch_clock_port> EpochClock;
    worker_executor EpochEffects;
    std::unique_ptr<application::epoch_application_host> EpochHost;
    std::unique_ptr<application::github_application_host> Host;
    bool Closing = false;
    bool ShutdownPollScheduled = false;
    bool ShutdownComplete = false;
    bool ShutdownCleanupFailed = false;
    bool NativePlatformVaultSelected = false;
    bool EpochProjectionDeliveryFailed = false;
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
        .EpochProjectionDeliveryFailed =
            Implementation->EpochProjectionDeliveryFailed,
        .EpochHandoffDeliveryFailed =
            Implementation->Host->epoch_handoff_delivery_failed(),
        .EpochStatus = Implementation->EpochHost->state().Status,
    };
}

#if defined(BPR_BUILD_TESTS)
auto detail::github_application_fixture_access::make(
    const detail::github_public_path_fixture fixture,
    detail::github_public_path_fixture_options options)
    -> std::unique_ptr<github_application>
{
    return std::unique_ptr<github_application>{new github_application{
        std::make_unique<github_application::implementation>(
            github_application_config{}, fixture, std::move(options))}};
}

auto detail::github_application_fixture_access::evidence(
    const github_application& application) noexcept
    -> detail::github_public_path_fixture_evidence
{
    return application.Implementation->Fixture != nullptr
        ? application.Implementation->Fixture->evidence()
        : detail::github_public_path_fixture_evidence{};
}

auto detail::github_application_fixture_access::maintenance_evidence(
    const github_application& application,
    core::review_epoch_id target) noexcept
    -> detail::github_epoch_maintenance_fixture_evidence
{
    const auto* const implementation = application.Implementation.get();
    if(implementation == nullptr || implementation->Fixture == nullptr
       || implementation->EpochHost == nullptr)
    {
        return {};
    }

    const auto& state = implementation->EpochHost->state();
    const auto root = implementation->Fixture->application_data_root();
    auto retired_repository_present = false;
    auto retired_marker_present = false;
    std::error_code directory_error;
    auto marker = std::filesystem::directory_iterator{
        root / "quarantine" / "v1", directory_error};
    while(!directory_error
          && marker != std::filesystem::directory_iterator{})
    {
        if(marker->path().extension() == ".deleted")
        {
            retired_marker_present = true;
            auto repository = marker->path().stem();
            repository += ".git";
            std::error_code exists_error;
            retired_repository_present = retired_repository_present
                || std::filesystem::exists(
                    root / "repositories" / "v1" / repository,
                    exists_error);
        }
        marker.increment(directory_error);
    }
    // Whole-cache deletion may leave a replacement authority projected while
    // shutdown is rejecting its unfinished rehydration.  An empty target asks
    // for durable evidence about the retired cache, so derive the stable
    // catalog binding from the captured smart authority instead of sampling
    // that transient projection.
    if(!state.Authority.has_value()
       || (!core::valid(target) && retired_marker_present))
    {
        auto catalog_revision = std::int64_t{0};
        auto base_cache_deleted = false;
        auto head_cache_deleted = false;
        auto base_availability = -1;
        auto head_availability = -1;
        const auto smart_authority =
            implementation->Fixture->last_smart_authority();
        const auto fixture_evidence = implementation->Fixture->evidence();
        const auto base =
            core::parse_git_commit_oid(fixture_evidence.BaseOid);
        if(smart_authority && base)
        {
            auto reopened = implementation->EpochCatalogs->open(
                application::epoch_catalog_open_request{
                    .ApplicationSession = state.ApplicationSession,
                    .Authority =
                        {
                            .Profile = smart_authority->Profile,
                            .ProfileRevision = smart_authority->ProfileRevision,
                            .Host = smart_authority->Host,
                            .HostAuthority = smart_authority->HostAuthority,
                            .Account = smart_authority->Account,
                            .Viewer = smart_authority->Viewer,
                            .Repository = smart_authority->Repository,
                            .PullRequest = smart_authority->PullRequest,
                            .Number = smart_authority->Number,
                            .ObjectFormat = base->algorithm(),
                        },
                },
                {});
            if(reopened && *reopened)
            {
                const auto& binding = (*reopened)->binding();
                catalog_revision = binding.Catalog.Revision.value();
                target = core::review_epoch_id{
                    .Authority = binding.Authority.Scope,
                    .Incarnation = binding.Catalog.Incarnation,
                    .Ordinal = core::epoch_ordinal{1},
                };
                const auto anchors = (*reopened)->exact_anchors(
                    application::exact_anchor_lookup_request{
                        .Catalog = binding.Catalog,
                        .Anchors =
                            {
                                core::comparison_anchor_identity{
                                    .Authority = target.Authority,
                                    .Incarnation = target.Incarnation,
                                    .Kind = core::comparison_anchor_kind::
                                        epoch_base,
                                    .Epoch = target.Ordinal,
                                },
                                core::comparison_anchor_identity{
                                    .Authority = target.Authority,
                                    .Incarnation = target.Incarnation,
                                    .Kind = core::comparison_anchor_kind::
                                        epoch_head,
                                    .Epoch = target.Ordinal,
                                },
                            },
                    },
                    {});
                if(anchors && anchors->Anchors.size() == 2U)
                {
                    base_availability = static_cast<int>(
                        anchors->Anchors[0].Availability);
                    head_availability = static_cast<int>(
                        anchors->Anchors[1].Availability);
                    base_cache_deleted = anchors->Anchors[0].Availability
                        == core::anchor_availability::cache_deleted;
                    head_cache_deleted = anchors->Anchors[1].Availability
                        == core::anchor_availability::cache_deleted;
                }
            }
        }
        return {
            .PruneIntentDispatchCount =
                implementation->EpochPruneIntentDispatchCount,
            .DeleteIntentDispatchCount =
                implementation->EpochDeleteIntentDispatchCount,
            .CancelIntentDispatchCount =
                implementation->EpochCancelIntentDispatchCount,
            .CatalogRevision = catalog_revision,
            .RepositoryPresent = retired_repository_present,
            .CacheDeletionMarkerPresent = retired_marker_present,
            .TargetBaseCacheDeleted = base_cache_deleted,
            .TargetHeadCacheDeleted = head_cache_deleted,
            .RetiredRepositoryPresent = retired_repository_present,
            .RetiredCacheDeletionMarkerPresent = retired_marker_present,
            .TargetBaseAvailability = base_availability,
            .TargetHeadAvailability = head_availability,
        };
    }

    const auto lower_hex = [](const std::span<const std::uint8_t> bytes)
    {
        static constexpr std::string_view digits = "0123456789abcdef";
        auto result = std::string{};
        result.reserve(bytes.size() * 2U);
        for(const auto byte : bytes)
        {
            result.push_back(digits[(byte >> 4U) & 0x0fU]);
            result.push_back(digits[byte & 0x0fU]);
        }
        return result;
    };

    const auto& authority = *state.Authority;
    if(!core::valid(target) && state.Catalog)
    {
        target = core::review_epoch_id{
            .Authority = authority.Scope,
            .Incarnation = state.Catalog->Incarnation,
            .Ordinal = core::epoch_ordinal{1},
        };
    }
    const auto cache = lower_hex(authority.Cache.bytes());
    const auto scope = lower_hex(target.Authority.bytes());
    const auto repository = root / "repositories" / "v1" / (cache + ".git");
    auto ordinal = std::to_string(target.Ordinal.value());
    if(ordinal.size() < 16U)
    {
        ordinal.insert(0U, 16U - ordinal.size(), '0');
    }
    const auto epoch_root = repository / "refs" / "betterpullreview" / "v1"
        / "scope" / scope / "epoch"
        / ordinal;
    auto non_target_ordinal = std::to_string(2U);
    non_target_ordinal.insert(
        0U, 16U - non_target_ordinal.size(), '0');
    const auto non_target_epoch_root = repository / "refs"
        / "betterpullreview" / "v1" / "scope" / scope / "epoch"
        / non_target_ordinal;
    const auto deletion_marker =
        root / "quarantine" / "v1" / (cache + ".deleted");
    std::error_code error;
    const auto present = [&error](const std::filesystem::path& path)
    {
        error.clear();
        const auto result = std::filesystem::exists(path, error);
        return !error && result;
    };

    auto base_cache_deleted = false;
    auto head_cache_deleted = false;
    auto base_availability = -1;
    auto head_availability = -1;
    const auto& accepted_snapshot = implementation->Host->accepted_snapshot();
    const auto accepted_base = accepted_snapshot
        ? core::parse_git_commit_oid(
              accepted_snapshot->PullRequest.BaseOid.value())
        : std::nullopt;
    if(accepted_base && !state.PendingMaintenance.has_value())
    {
        auto reopened = implementation->EpochCatalogs->open(
            application::epoch_catalog_open_request{
                .ApplicationSession = state.ApplicationSession,
                .Authority =
                    {
                        .Profile = authority.Profile,
                        .ProfileRevision = authority.ProfileRevision,
                        .Host = authority.Host,
                        .HostAuthority = authority.HostAuthority,
                        .Account = authority.Account,
                        .Viewer = authority.Viewer,
                        .Repository = authority.Repository,
                        .PullRequest = authority.PullRequest,
                        .Number = authority.Number,
                        .ObjectFormat = accepted_base->algorithm(),
                    },
            },
            {});
        if(reopened && *reopened)
        {
            const auto& binding = (*reopened)->binding();
            if(!core::valid(target))
            {
                target = core::review_epoch_id{
                    .Authority = binding.Authority.Scope,
                    .Incarnation = binding.Catalog.Incarnation,
                    .Ordinal = core::epoch_ordinal{1},
                };
            }
            const auto anchors = (*reopened)->exact_anchors(
                application::exact_anchor_lookup_request{
                    .Catalog = binding.Catalog,
                    .Anchors =
                        {
                            core::comparison_anchor_identity{
                                .Authority = target.Authority,
                                .Incarnation = target.Incarnation,
                                .Kind =
                                    core::comparison_anchor_kind::epoch_base,
                                .Epoch = target.Ordinal,
                            },
                            core::comparison_anchor_identity{
                                .Authority = target.Authority,
                                .Incarnation = target.Incarnation,
                                .Kind =
                                    core::comparison_anchor_kind::epoch_head,
                                .Epoch = target.Ordinal,
                            },
                        },
                },
                {});
            if(anchors && anchors->Anchors.size() == 2U)
            {
                base_availability = static_cast<int>(
                    anchors->Anchors[0].Availability);
                head_availability = static_cast<int>(
                    anchors->Anchors[1].Availability);
                base_cache_deleted = anchors->Anchors[0].Availability
                    == core::anchor_availability::cache_deleted;
                head_cache_deleted = anchors->Anchors[1].Availability
                    == core::anchor_availability::cache_deleted;
            }
        }
    }

    return {
        .PruneIntentDispatchCount =
            implementation->EpochPruneIntentDispatchCount,
        .DeleteIntentDispatchCount =
            implementation->EpochDeleteIntentDispatchCount,
        .CancelIntentDispatchCount =
            implementation->EpochCancelIntentDispatchCount,
        .CatalogRevision = state.Catalog
            ? state.Catalog->Revision.value()
            : 0,
        .TargetBaseRefPresent = present(epoch_root / "base"),
        .TargetHeadRefPresent = present(epoch_root / "head"),
        .NonTargetBaseRefPresent =
            present(non_target_epoch_root / "base"),
        .NonTargetHeadRefPresent =
            present(non_target_epoch_root / "head"),
        .RepositoryPresent = present(repository),
        .CacheDeletionMarkerPresent = present(deletion_marker),
        .TargetBaseCacheDeleted = base_cache_deleted,
        .TargetHeadCacheDeleted = head_cache_deleted,
        .RetiredRepositoryPresent = retired_repository_present,
        .RetiredCacheDeletionMarkerPresent = retired_marker_present,
        .TargetBaseAvailability = base_availability,
        .TargetHeadAvailability = head_availability,
    };
}
#endif
}
