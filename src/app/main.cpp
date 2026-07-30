#include "bpr/app/github_application.hpp"

#if defined(BPR_BUILD_TESTS)
#include "github_application_fixture_access.hpp"
#endif

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QSpinBox>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace
{
auto has_argument(
    const int argc,
    char* const* const argv,
    const std::string_view expected) -> bool
{
    return std::any_of(
        argv + 1,
        argv + argc,
        [expected](const char* const argument)
        { return std::string_view{argument} == expected; });
}

auto argument_value(
    const int argc,
    char* const* const argv,
    const std::string_view expected) -> std::optional<std::string_view>
{
    for(auto index = 1; index + 1 < argc; ++index)
    {
        if(std::string_view{argv[index]} == expected)
        {
            return std::string_view{argv[index + 1]};
        }
    }
    return std::nullopt;
}

auto save_render_evidence(
    bpr::qt::github_window& window,
    const std::optional<std::string_view>& render_path) -> bool
{
    if(!render_path.has_value())
    {
        return true;
    }
    const auto path = QString::fromUtf8(
        render_path->data(),
        static_cast<qsizetype>(render_path->size()));
    return window.grab().save(path, "PNG");
}

struct automated_run
{
    enum class stage
    {
        start,
        connect,
        open,
        shutdown,
    };

    stage Stage = stage::start;
    std::size_t Attempts = 0U;
    bool PresentationPassed = false;
    bool RenderSaved = true;
    bool VerifyNativePlatformVault = false;
#if defined(BPR_BUILD_TESTS)
    bool Fixture = false;
    std::string FixtureName;
    std::string ExpectedActiveHostText;
#endif
    std::optional<std::string_view> RenderPath;
};

[[nodiscard]] constexpr auto native_platform_vault_expected() noexcept
    -> bool
{
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
    return true;
#else
    return false;
#endif
}

#if defined(BPR_BUILD_TESTS)
[[nodiscard]] auto fixture_presentation_passed(
    bpr::qt::github_window& window,
    const std::string_view expected_active_host_text) -> bool
{
    const auto* const title =
        window.findChild<QLabel*>(QStringLiteral("pullRequestTitle"));
    return window.status_text() == "Pull request ready."
        && window.active_account_text() == "Octo Reviewer (@octo-reviewer)"
        && window.active_host_text() == expected_active_host_text
        && window.open_action()->isEnabled()
        && window.refresh_action()->isEnabled()
        && title != nullptr
        && title->text()
               == QStringLiteral("Keep review snapshots deterministic")
        && window.conversation_model()->rowCount() == 3
        && window.conversation_model()
               ->index_for_id("review-thread:PRRT_fixture_1")
               .isValid()
        && window.statistics().FullResets == 0U;
}
#endif

[[nodiscard]] auto cli_fail_closed_presentation_passed(
    const bpr::qt::github_window& window) -> bool
{
    return window.status_text() == "The GitHub connection failed."
        && window.diagnostic_text().find("CLI connection is unavailable")
               != std::string::npos
        && !window.open_action()->isEnabled()
        && window.statistics().FullResets == 0U;
}

void finish_automated_run(
    bpr::app::github_application& application,
    const automated_run& run)
{
    const auto status = application.status();
    const auto native_vault_evidence_passed =
        !run.VerifyNativePlatformVault
        || status.NativePlatformVaultSelected
               == native_platform_vault_expected();
    auto passed = run.PresentationPassed && run.RenderSaved
        && status.ShutdownComplete && !status.ShutdownCleanupFailed
        && native_vault_evidence_passed;

#if defined(BPR_BUILD_TESTS)
    const auto fixture_evidence =
        run.Fixture
        ? bpr::app::detail::github_application_fixture_access::evidence(
              application)
        : bpr::app::detail::github_public_path_fixture_evidence{};
    if(run.Fixture)
    {
        passed = passed && fixture_evidence.RecordingConsumed
            && fixture_evidence.CapabilityProbeConsumed
            && fixture_evidence.BootstrapTransportSelected
            && fixture_evidence.AccountTransportSelected
            && fixture_evidence.CredentialStored
            && fixture_evidence.CredentialRetrieved
            && fixture_evidence.CredentialDeleted
            && !fixture_evidence.SecretRetained;
    }
#endif

    QTextStream stream{stdout};
    stream << (passed ? "PUBLIC_PATH_OK\n" : "PUBLIC_PATH_FAILED\n");
    if(run.VerifyNativePlatformVault)
    {
        stream << "NATIVE_PLATFORM_VAULT_SELECTED:"
               << (status.NativePlatformVaultSelected ? "true" : "false")
               << '\n';
    }
    if(!passed)
    {
        const auto& window = application.window();
        stream << "PUBLIC_PATH_STAGE:"
               << static_cast<int>(run.Stage) << '\n'
               << "PUBLIC_PATH_STATUS:"
               << QString::fromStdString(window.status_text()) << '\n'
               << "PUBLIC_PATH_DIAGNOSTIC:"
               << QString::fromStdString(window.diagnostic_text()) << '\n'
               << "PUBLIC_PATH_ACCOUNT:"
               << QString::fromStdString(window.active_account_text()) << '\n';
    }
#if defined(BPR_BUILD_TESTS)
    if(run.Fixture)
    {
        stream << (passed ? "GITHUB_FIXTURE_OK:" : "GITHUB_FIXTURE_FAILED:")
               << QString::fromStdString(run.FixtureName) << '\n';
        if(!passed)
        {
            stream << "GITHUB_FIXTURE_EVIDENCE:"
                   << fixture_evidence.RecordingConsumed
                   << fixture_evidence.CapabilityProbeConsumed
                   << fixture_evidence.BootstrapTransportSelected
                   << fixture_evidence.AccountTransportSelected
                   << fixture_evidence.CredentialStored
                   << fixture_evidence.CredentialRetrieved
                   << fixture_evidence.CredentialDeleted
                   << fixture_evidence.SecretRetained << ':'
                   << fixture_evidence.RecordingFailure << ':'
                   << fixture_evidence.AccountReadCount << ':'
                   << fixture_evidence.LastReadOperation << '\n';
        }
    }
#endif
    if(run.RenderPath.has_value())
    {
        stream << (run.RenderSaved ? "RENDER_OK\n" : "RENDER_FAILED\n");
    }
    QCoreApplication::exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
}

void transition_to_shutdown(
    bpr::app::github_application& application,
    automated_run& run)
{
    application.begin_shutdown();
    run.Stage = automated_run::stage::shutdown;
}

void schedule_automated_run(
    bpr::app::github_application& application,
    std::shared_ptr<automated_run> run)
{
    auto tick = std::make_shared<std::function<void()>>();
    *tick = [&application, run, tick]
    {
        auto& window = application.window();
        if(++run->Attempts > 1'000U)
        {
            run->PresentationPassed = false;
            run->RenderSaved =
                save_render_evidence(window, run->RenderPath);
            transition_to_shutdown(application, *run);
        }

        switch(run->Stage)
        {
        case automated_run::stage::start:
#if defined(BPR_BUILD_TESTS)
            if(run->Fixture)
            {
                window.connect_action()->trigger();
                run->Stage = automated_run::stage::connect;
            }
            else
#endif
            {
                if(window.profile_selector()->count() < 2)
                {
                    run->PresentationPassed = false;
                    transition_to_shutdown(application, *run);
                    break;
                }
                window.profile_selector()->setCurrentIndex(1);
                window.connect_action()->trigger();
                run->Stage = automated_run::stage::connect;
            }
            break;

        case automated_run::stage::connect:
#if defined(BPR_BUILD_TESTS)
            if(run->Fixture)
            {
                if(window.status_text() != "GitHub account connected."
                   || !window.open_action()->isEnabled())
                {
                    break;
                }
                window.owner_editor()->setText(QStringLiteral("octo-org"));
                window.repository_editor()->setText(
                    QStringLiteral("review-repository"));
                window.pull_request_editor()->setValue(17);
                window.open_action()->trigger();
                run->Stage = automated_run::stage::open;
            }
            else if(cli_fail_closed_presentation_passed(window))
#else
            if(cli_fail_closed_presentation_passed(window))
#endif
            {
                run->PresentationPassed = true;
                run->RenderSaved =
                    save_render_evidence(window, run->RenderPath);
                transition_to_shutdown(application, *run);
            }
            break;

        case automated_run::stage::open:
#if defined(BPR_BUILD_TESTS)
            if(fixture_presentation_passed(
                   window, run->ExpectedActiveHostText))
            {
                run->PresentationPassed = true;
                run->RenderSaved =
                    save_render_evidence(window, run->RenderPath);
                transition_to_shutdown(application, *run);
            }
#else
            run->PresentationPassed = false;
            transition_to_shutdown(application, *run);
#endif
            break;

        case automated_run::stage::shutdown:
            if(application.status().ShutdownComplete)
            {
                finish_automated_run(application, *run);
                return;
            }
            break;
        }
        QTimer::singleShot(10, &window, *tick);
    };
    QTimer::singleShot(0, &application.window(), *tick);
}

void schedule_interactive_shutdown(
    bpr::app::github_application& application)
{
    application.begin_shutdown();
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&application, poll]
    {
        const auto status = application.status();
        if(status.ShutdownComplete)
        {
            QCoreApplication::exit(
                status.ShutdownCleanupFailed ? EXIT_FAILURE : EXIT_SUCCESS);
            return;
        }
        QTimer::singleShot(10, &application.window(), *poll);
    };
    QTimer::singleShot(0, &application.window(), *poll);
}
}

auto main(int argc, char** argv) -> int
{
    QApplication qt_application{argc, argv};
    qt_application.setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
    QCoreApplication::setApplicationVersion(QStringLiteral(BPR_VERSION));
    QCoreApplication::setOrganizationName(
        QStringLiteral("BetterPullReview"));

    try
    {
        auto config = bpr::app::github_application_config{
            .EnterpriseHostname = std::nullopt,
        };
        if(const auto host = argument_value(argc, argv, "--ghes-host");
           host.has_value())
        {
            config.EnterpriseHostname = std::string{*host};
        }

#if defined(BPR_BUILD_TESTS)
        auto fixture_name = std::string{};
        auto expected_active_host_text = std::string{};
        auto selected_fixture = std::optional<
            bpr::app::detail::github_public_path_fixture>{};
        if(const auto fixture =
               argument_value(argc, argv, "--github-fixture");
           fixture.has_value())
        {
            if(*fixture == "github-com")
            {
                selected_fixture =
                    bpr::app::detail::github_public_path_fixture::github_com;
                fixture_name = "github.com";
                expected_active_host_text =
                    "Active host: github.com (GitHub.com)";
            }
            else if(*fixture == "ghes-3.18")
            {
                selected_fixture =
                    bpr::app::detail::github_public_path_fixture::ghes_3_18;
                fixture_name = "ghes-3.18";
                expected_active_host_text =
                    "Active host: ghe.example.invalid "
                    "(GitHub Enterprise Server)";
            }
            else
            {
                QTextStream{stdout} << "PUBLIC_PATH_FAILED\n";
                return EXIT_FAILURE;
            }
        }
#endif

        auto application =
#if defined(BPR_BUILD_TESTS)
            selected_fixture.has_value()
            ? bpr::app::detail::github_application_fixture_access::make(
                  *selected_fixture)
            :
#endif
            std::make_unique<bpr::app::github_application>(
                std::move(config));
        application->show();

        QObject::connect(
            &qt_application,
            &QGuiApplication::lastWindowClosed,
            &application->window(),
            [&application]
            { schedule_interactive_shutdown(*application); });

        const auto self_test = has_argument(argc, argv, "--self-test");
        const auto render_path =
            argument_value(argc, argv, "--render-evidence");
        auto verify_native_platform_vault = self_test;
#if defined(BPR_BUILD_TESTS)
        verify_native_platform_vault =
            verify_native_platform_vault && fixture_name.empty();
#endif
        if(self_test || render_path.has_value()
#if defined(BPR_BUILD_TESTS)
           || !fixture_name.empty()
#endif
        )
        {
            schedule_automated_run(
                *application,
                std::make_shared<automated_run>(
                    automated_run{
                        .Stage = automated_run::stage::start,
                        .Attempts = 0U,
                        .PresentationPassed = false,
                        .RenderSaved = true,
                        .VerifyNativePlatformVault =
                            verify_native_platform_vault,
#if defined(BPR_BUILD_TESTS)
                        .Fixture = !fixture_name.empty(),
                        .FixtureName = std::move(fixture_name),
                        .ExpectedActiveHostText =
                            std::move(expected_active_host_text),
#endif
                        .RenderPath = render_path,
                    }));
        }

        return qt_application.exec();
    }
    catch(...)
    {
        QTextStream{stdout} << "STARTUP_FAILED\n";
        return EXIT_FAILURE;
    }
}
