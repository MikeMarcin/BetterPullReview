#include "bpr/app/github_application.hpp"

#if defined(BPR_BUILD_TESTS)
#include "github_application_fixture_access.hpp"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif
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

#if defined(BPR_BUILD_TESTS)
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QItemSelectionModel>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QStyle>
#include <QStyleHints>
#include <QTreeView>
#endif

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#if defined(BPR_BUILD_TESTS)
#include <cstring>
#endif

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

#if defined(BPR_BUILD_TESTS)
struct native_appearance_evidence_paths
{
    QString Initial;
    QString Changed;
};

[[nodiscard]] auto resolved_fresh_render_path(
    const std::string_view raw_path) -> std::optional<QString>
{
    if(raw_path.empty())
    {
        return std::nullopt;
    }

    const auto path = QString::fromUtf8(
        raw_path.data(),
        static_cast<qsizetype>(raw_path.size()));
    const auto supplied = QFileInfo{path};
    if(supplied.fileName().isEmpty())
    {
        return std::nullopt;
    }

    const auto parent = supplied.dir().canonicalPath();
    if(parent.isEmpty())
    {
        return std::nullopt;
    }

    const auto resolved =
        QDir::cleanPath(QDir{parent}.filePath(supplied.fileName()));
    const auto destination = QFileInfo{resolved};
    if(destination.exists() || destination.isSymbolicLink())
    {
        return std::nullopt;
    }
    return resolved;
}

[[nodiscard]] auto resolve_native_appearance_evidence_paths(
    const std::optional<std::string_view>& initial,
    const std::optional<std::string_view>& changed)
    -> std::optional<native_appearance_evidence_paths>
{
    if(!initial.has_value() || !changed.has_value())
    {
        return std::nullopt;
    }

    const auto resolved_initial =
        resolved_fresh_render_path(*initial);
    const auto resolved_changed =
        resolved_fresh_render_path(*changed);
    if(
        !resolved_initial.has_value() || !resolved_changed.has_value()
        || *resolved_initial == *resolved_changed)
    {
        return std::nullopt;
    }

    return native_appearance_evidence_paths{
        .Initial = *resolved_initial,
        .Changed = *resolved_changed,
    };
}

[[nodiscard]] auto capture_normalized_render(
    bpr::qt::github_window& window) -> QImage
{
    auto image = window.grab().toImage();
    if(image.isNull())
    {
        return {};
    }

    const auto device_pixel_ratio = image.devicePixelRatio();
    image = image.convertToFormat(QImage::Format_RGBA8888);
    image.setDevicePixelRatio(device_pixel_ratio);
    return image;
}

[[nodiscard]] auto same_render_pixels(
    const QImage& left,
    const QImage& right) -> bool
{
    if(left.isNull() || right.isNull() || left.size() != right.size())
    {
        return false;
    }

    const auto normalized_left =
        left.convertToFormat(QImage::Format_RGBA8888);
    const auto normalized_right =
        right.convertToFormat(QImage::Format_RGBA8888);
    const auto row_size =
        static_cast<std::size_t>(normalized_left.width()) * 4U;
    for(auto row = 0; row < normalized_left.height(); ++row)
    {
        if(
            std::memcmp(
                normalized_left.constScanLine(row),
                normalized_right.constScanLine(row),
                row_size)
            != 0)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto save_new_png(
    const QImage& image,
    const QString& path) -> bool
{
    QFile output{path};
    if(
        image.isNull()
        || !output.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    {
        return false;
    }

    const auto saved = image.save(&output, "PNG") && output.flush();
    output.close();
    if(!saved)
    {
        output.remove();
    }
    return saved;
}

[[nodiscard]] auto persist_native_appearance_evidence(
    const QImage& initial,
    const QImage& changed,
    const native_appearance_evidence_paths& paths) -> bool
{
    if(!save_new_png(initial, paths.Initial))
    {
        return false;
    }
    if(!save_new_png(changed, paths.Changed))
    {
        QFile::remove(paths.Initial);
        return false;
    }

    const auto persisted_initial = QImage{paths.Initial, "PNG"};
    const auto persisted_changed = QImage{paths.Changed, "PNG"};
    const auto passed = same_render_pixels(persisted_initial, initial)
        && same_render_pixels(persisted_changed, changed)
        && !same_render_pixels(persisted_initial, persisted_changed);
    if(!passed)
    {
        QFile::remove(paths.Initial);
        QFile::remove(paths.Changed);
    }
    return passed;
}

[[nodiscard]] auto macos_global_color_scheme() noexcept
    -> Qt::ColorScheme
{
#if defined(__APPLE__)
    if(!CFPreferencesSynchronize(
           kCFPreferencesAnyApplication,
           kCFPreferencesCurrentUser,
           kCFPreferencesAnyHost))
    {
        return Qt::ColorScheme::Unknown;
    }

    // Qt's effective appearance can be overridden per application, so the
    // test gate independently reads macOS's global appearance domain.
    const auto value = CFPreferencesCopyValue(
        CFSTR("AppleInterfaceStyle"),
        kCFPreferencesAnyApplication,
        kCFPreferencesCurrentUser,
        kCFPreferencesAnyHost);
    if(value == nullptr)
    {
        return Qt::ColorScheme::Light;
    }

    auto scheme = Qt::ColorScheme::Unknown;
    if(
        CFGetTypeID(value) == CFStringGetTypeID()
        && CFEqual(value, CFSTR("Dark")))
    {
        scheme = Qt::ColorScheme::Dark;
    }
    CFRelease(value);
    return scheme;
#else
    return Qt::ColorScheme::Unknown;
#endif
}
#endif

struct automated_run
{
    enum class stage
    {
        start,
        connect,
        open,
#if defined(BPR_BUILD_TESTS)
        appearance,
#endif
        shutdown,
    };

    stage Stage = stage::start;
    std::size_t Attempts = 0U;
    bool PresentationPassed = false;
    bool RenderSaved = true;
    bool VerifyNativePlatformVault = false;
    bool VerifyCliUnavailable = false;
#if defined(BPR_BUILD_TESTS)
    struct native_appearance_probe_state
    {
        bool Enabled = false;
        bool SignalObserved = false;
        bool StatePassed = false;
        bool PixelsDiffer = false;
        std::size_t SettleAttempts = 0U;
        std::size_t GlobalPollTicks = 0U;
        Qt::ColorScheme InitialColorScheme = Qt::ColorScheme::Unknown;
        Qt::ColorScheme ObservedColorScheme = Qt::ColorScheme::Unknown;
        Qt::ColorScheme InitialGlobalColorScheme =
            Qt::ColorScheme::Unknown;
        Qt::ColorScheme ObservedGlobalColorScheme =
            Qt::ColorScheme::Unknown;
        QPalette InitialPalette;
        QString InitialPlatformName;
        QString InitialStyleClass;
        QPersistentModelIndex Thread;
        std::size_t InitialModelResets = 0U;
        QImage InitialRender;
        QImage ChangedRender;
        native_appearance_evidence_paths EvidencePaths;
    };

    bool Fixture = false;
    std::string FixtureName;
    std::string ExpectedActiveHostText;
    native_appearance_probe_state Appearance;
#endif
    std::optional<std::string_view> RenderPath;
};

#if defined(BPR_BUILD_TESTS)
[[nodiscard]] auto color_scheme_name(
    const Qt::ColorScheme scheme) noexcept -> const char*
{
    switch(scheme)
    {
    case Qt::ColorScheme::Unknown:
        return "Unknown";
    case Qt::ColorScheme::Light:
        return "Light";
    case Qt::ColorScheme::Dark:
        return "Dark";
    }
    return "Unknown";
}

[[nodiscard]] auto native_appearance_context_passed(
    const bpr::qt::github_window& window,
    const automated_run& run) -> bool
{
    const auto* const view = window.conversation_view();
    const auto* const model = window.conversation_model();

    return run.Appearance.Thread.isValid()
        && QGuiApplication::platformName()
               == run.Appearance.InitialPlatformName
        && QString::fromLatin1(
               QApplication::style()->metaObject()->className())
               == run.Appearance.InitialStyleClass
        && view->currentIndex() == run.Appearance.Thread
        && view->selectionModel()->isSelected(run.Appearance.Thread)
        && !view->isExpanded(run.Appearance.Thread)
        && QApplication::focusWidget() == view
        && model->statistics().ModelResets
               == run.Appearance.InitialModelResets
        && window.statistics().FullResets == 0U
        && !window.testAttribute(Qt::WA_SetPalette)
        && !window.testAttribute(Qt::WA_SetStyle)
        && !window.testAttribute(Qt::WA_SetFont)
        && window.styleSheet().isEmpty();
}

[[nodiscard]] auto native_appearance_light_state_passed(
    const bpr::qt::github_window& window,
    const automated_run& run) -> bool
{
    const auto* const hints = QGuiApplication::styleHints();
    return hints != nullptr && !run.Appearance.SignalObserved
        && run.Appearance.InitialColorScheme == Qt::ColorScheme::Light
        && run.Appearance.ObservedColorScheme == Qt::ColorScheme::Light
        && run.Appearance.InitialGlobalColorScheme
               == Qt::ColorScheme::Light
        && macos_global_color_scheme() == Qt::ColorScheme::Light
        && hints->colorScheme() == Qt::ColorScheme::Light
        && window.palette() == run.Appearance.InitialPalette
        && native_appearance_context_passed(window, run);
}

[[nodiscard]] auto native_appearance_dark_state_passed(
    const bpr::qt::github_window& window,
    const automated_run& run) -> bool
{
    const auto final_palette = window.palette();
    const auto initial_window =
        run.Appearance.InitialPalette.color(QPalette::Window).lightness();
    const auto initial_base =
        run.Appearance.InitialPalette.color(QPalette::Base).lightness();
    const auto initial_text =
        run.Appearance.InitialPalette.color(QPalette::Text).lightness();
    const auto final_window =
        final_palette.color(QPalette::Window).lightness();
    const auto final_base =
        final_palette.color(QPalette::Base).lightness();
    const auto final_text =
        final_palette.color(QPalette::Text).lightness();

    return run.Appearance.SignalObserved
        && run.Appearance.InitialColorScheme == Qt::ColorScheme::Light
        && run.Appearance.ObservedColorScheme == Qt::ColorScheme::Dark
        && run.Appearance.InitialGlobalColorScheme
               == Qt::ColorScheme::Light
        && run.Appearance.ObservedGlobalColorScheme
               == Qt::ColorScheme::Dark
        && macos_global_color_scheme() == Qt::ColorScheme::Dark
        && QGuiApplication::styleHints()->colorScheme()
               == Qt::ColorScheme::Dark
        && final_window < initial_window && final_base < initial_base
        && final_text > initial_text && final_window < final_text
        && final_base < final_text
        && native_appearance_context_passed(window, run);
}

[[nodiscard]] auto prepare_native_appearance_probe(
    bpr::qt::github_window& window,
    automated_run& run) -> bool
{
    run.RenderSaved = false;
    auto* const hints = QGuiApplication::styleHints();
    auto* const view = window.conversation_view();
    auto* const model = window.conversation_model();
    const auto thread =
        model->index_for_id("review-thread:PRRT_fixture_1");
    if(hints != nullptr)
    {
        run.Appearance.InitialColorScheme = hints->colorScheme();
        run.Appearance.ObservedColorScheme = hints->colorScheme();
    }
    run.Appearance.InitialGlobalColorScheme =
        macos_global_color_scheme();
    run.Appearance.ObservedGlobalColorScheme =
        run.Appearance.InitialGlobalColorScheme;
    run.Appearance.InitialPalette = window.palette();
    run.Appearance.InitialPlatformName = QGuiApplication::platformName();
    run.Appearance.InitialStyleClass = QString::fromLatin1(
        QApplication::style()->metaObject()->className());
    const auto initial_text =
        run.Appearance.InitialPalette.color(QPalette::Text).lightness();
    const auto light_palette =
        run.Appearance.InitialPalette.color(QPalette::Window).lightness()
            > initial_text
        && run.Appearance.InitialPalette.color(QPalette::Base).lightness()
               > initial_text;
    if(
        hints == nullptr || run.Appearance.InitialPlatformName != "cocoa"
        || run.Appearance.InitialStyleClass != "QMacStyle"
        || hints->colorScheme() != Qt::ColorScheme::Light
        || run.Appearance.InitialGlobalColorScheme
               != Qt::ColorScheme::Light
        || !light_palette || !thread.isValid() || !run.RenderPath.has_value()
        || run.Appearance.EvidencePaths.Initial.isEmpty()
        || run.Appearance.EvidencePaths.Changed.isEmpty())
    {
        return false;
    }

    run.Appearance.Thread = QPersistentModelIndex{thread};
    run.Appearance.InitialModelResets = model->statistics().ModelResets;
    view->setCurrentIndex(thread);
    view->selectionModel()->select(
        thread,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    view->collapse(thread);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    if(!native_appearance_light_state_passed(window, run))
    {
        return false;
    }

    auto initial_render = capture_normalized_render(window);
    if(
        initial_render.isNull()
        || !native_appearance_light_state_passed(window, run))
    {
        return false;
    }
    run.Appearance.InitialRender = std::move(initial_render);

    QTextStream stream{stdout};
    stream << "NATIVE_APPEARANCE_READY:"
           << color_scheme_name(run.Appearance.InitialColorScheme) << '\n'
           << "NATIVE_APPEARANCE_GLOBAL_READY:"
           << color_scheme_name(
                  run.Appearance.InitialGlobalColorScheme)
           << '\n';
    stream.flush();
    return true;
}
#endif

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
    auto passed = run.PresentationPassed && status.ShutdownComplete
        && !status.ShutdownCleanupFailed
        && native_vault_evidence_passed;
    auto render_saved = run.RenderSaved;

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
    if(run.Appearance.Enabled)
    {
        const auto appearance_passed =
            run.Appearance.SignalObserved
            && run.Appearance.StatePassed
            && run.Appearance.PixelsDiffer;
        render_saved =
            passed && appearance_passed
            && persist_native_appearance_evidence(
                run.Appearance.InitialRender,
                run.Appearance.ChangedRender,
                run.Appearance.EvidencePaths);
        passed = passed && appearance_passed && render_saved;
    }
    else
    {
        passed = passed && render_saved;
    }
#else
    passed = passed && render_saved;
#endif

    QTextStream stream{stdout};
    stream << (passed ? "PUBLIC_PATH_OK\n" : "PUBLIC_PATH_FAILED\n");
    if(run.VerifyNativePlatformVault)
    {
        stream << "NATIVE_PLATFORM_VAULT_SELECTED:"
               << (status.NativePlatformVaultSelected ? "true" : "false")
               << '\n';
    }
    if(run.VerifyCliUnavailable)
    {
        stream << (passed ? "CLI_UNAVAILABLE_OK\n"
                          : "CLI_UNAVAILABLE_FAILED\n");
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
        stream << (render_saved ? "RENDER_OK\n" : "RENDER_FAILED\n");
    }
#if defined(BPR_BUILD_TESTS)
    if(run.Appearance.Enabled)
    {
        stream << "NATIVE_APPEARANCE_TRANSITION:"
               << color_scheme_name(
                      run.Appearance.InitialColorScheme)
               << "->"
               << color_scheme_name(
                      run.Appearance.ObservedColorScheme)
               << '\n'
               << "NATIVE_APPEARANCE_GLOBAL_TRANSITION:"
               << color_scheme_name(
                      run.Appearance.InitialGlobalColorScheme)
               << "->"
               << color_scheme_name(
                      run.Appearance.ObservedGlobalColorScheme)
               << '\n'
               << "NATIVE_APPEARANCE_PLATFORM:"
               << run.Appearance.InitialPlatformName << '\n'
               << "NATIVE_APPEARANCE_STYLE:"
               << run.Appearance.InitialStyleClass << '\n'
               << (run.Appearance.SignalObserved
                       ? "NATIVE_APPEARANCE_SIGNAL_OK\n"
                       : "NATIVE_APPEARANCE_SIGNAL_FAILED\n")
               << (run.Appearance.StatePassed
                       ? "NATIVE_APPEARANCE_STATE_OK\n"
                       : "NATIVE_APPEARANCE_STATE_FAILED\n")
               << (run.Appearance.PixelsDiffer
                       ? "NATIVE_APPEARANCE_PIXELS_DIFFER_OK\n"
                       : "NATIVE_APPEARANCE_PIXELS_DIFFER_FAILED\n")
               << (render_saved
                       ? "NATIVE_APPEARANCE_CHANGED_RENDER_OK\n"
                       : "NATIVE_APPEARANCE_CHANGED_RENDER_FAILED\n");
    }
#endif
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
#if defined(BPR_BUILD_TESTS)
    if(run->Appearance.Enabled)
    {
        run->RenderSaved = false;
        QObject::connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            &application.window(),
            [run](const Qt::ColorScheme scheme)
            {
                run->Appearance.SignalObserved = true;
                run->Appearance.ObservedColorScheme = scheme;
                run->Appearance.SettleAttempts = 0U;
                run->Appearance.GlobalPollTicks = 0U;
            });
    }
#endif

    auto tick = std::make_shared<std::function<void()>>();
    *tick = [&application, run, tick]
    {
        auto& window = application.window();
        const auto maximum_attempts =
#if defined(BPR_BUILD_TESTS)
            run->Appearance.Enabled ? 6'000U :
#endif
                                      1'000U;
        if(++run->Attempts > maximum_attempts)
        {
            run->PresentationPassed = false;
#if defined(BPR_BUILD_TESTS)
            if(!run->Appearance.Enabled)
#endif
            {
                run->RenderSaved =
                    save_render_evidence(window, run->RenderPath);
            }
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
                if(run->Appearance.Enabled)
                {
                    if(prepare_native_appearance_probe(window, *run))
                    {
                        run->Stage = automated_run::stage::appearance;
                    }
                    else
                    {
                        run->PresentationPassed = false;
                        transition_to_shutdown(application, *run);
                    }
                }
                else
                {
                    run->PresentationPassed = true;
                    run->RenderSaved =
                        save_render_evidence(window, run->RenderPath);
                    transition_to_shutdown(application, *run);
                }
            }
#else
            run->PresentationPassed = false;
            transition_to_shutdown(application, *run);
#endif
            break;

#if defined(BPR_BUILD_TESTS)
        case automated_run::stage::appearance:
            if(
                run->Appearance.SignalObserved
                && run->Appearance.ObservedColorScheme
                       == Qt::ColorScheme::Dark
                && ++run->Appearance.GlobalPollTicks >= 10U)
            {
                run->Appearance.GlobalPollTicks = 0U;
                const auto global_scheme =
                    macos_global_color_scheme();
                if(
                    global_scheme
                    != run->Appearance.ObservedGlobalColorScheme)
                {
                    run->Appearance.SettleAttempts = 0U;
                }
                run->Appearance.ObservedGlobalColorScheme =
                    global_scheme;
            }
            if(
                run->Appearance.SignalObserved
                && run->Appearance.ObservedColorScheme
                       == Qt::ColorScheme::Dark
                && run->Appearance.ObservedGlobalColorScheme
                       == Qt::ColorScheme::Dark
                && QGuiApplication::styleHints()->colorScheme()
                       == Qt::ColorScheme::Dark
                && ++run->Appearance.SettleAttempts >= 50U)
            {
                const auto state_before_render =
                    native_appearance_dark_state_passed(window, *run);
                const auto changed_render =
                    state_before_render
                    ? capture_normalized_render(window)
                    : QImage{};
                const auto state_after_render =
                    state_before_render && !changed_render.isNull()
                    && native_appearance_dark_state_passed(window, *run);
                run->Appearance.StatePassed = state_after_render;
                run->Appearance.PixelsDiffer =
                    run->Appearance.StatePassed
                    && run->Appearance.InitialRender.size()
                           == changed_render.size()
                    && run->Appearance.InitialRender.devicePixelRatio()
                           == changed_render.devicePixelRatio()
                    && !same_render_pixels(
                        run->Appearance.InitialRender,
                        changed_render);
                if(run->Appearance.PixelsDiffer)
                {
                    run->Appearance.ChangedRender = changed_render;
                }
                run->PresentationPassed =
                    run->Appearance.StatePassed
                    && run->Appearance.PixelsDiffer;
                transition_to_shutdown(application, *run);
            }
            break;
#endif

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

        const auto self_test = has_argument(argc, argv, "--self-test");
        const auto render_path =
            argument_value(argc, argv, "--render-evidence");
#if defined(BPR_BUILD_TESTS)
        const auto native_appearance_probe =
            has_argument(argc, argv, "--native-appearance-probe");
        const auto changed_render_path =
            argument_value(argc, argv, "--changed-render-evidence");
        auto native_appearance_paths =
            std::optional<native_appearance_evidence_paths>{};
        if(native_appearance_probe)
        {
            native_appearance_paths =
                resolve_native_appearance_evidence_paths(
                    render_path,
                    changed_render_path);
        }
        if(
            native_appearance_probe
            && (
                !selected_fixture.has_value()
                || *selected_fixture
                       != bpr::app::detail::
                           github_public_path_fixture::github_com
                || !native_appearance_paths.has_value()))
        {
            QTextStream{stdout}
                << "NATIVE_APPEARANCE_CONFIGURATION_FAILED\n";
            return EXIT_FAILURE;
        }
        if(!native_appearance_probe && changed_render_path.has_value())
        {
            QTextStream{stdout}
                << "NATIVE_APPEARANCE_CONFIGURATION_FAILED\n";
            return EXIT_FAILURE;
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

        auto verify_native_platform_vault = self_test;
        auto verify_cli_unavailable = self_test;
#if defined(BPR_BUILD_TESTS)
        verify_native_platform_vault =
            verify_native_platform_vault && fixture_name.empty();
        verify_cli_unavailable =
            verify_cli_unavailable && fixture_name.empty();
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
                        .VerifyCliUnavailable = verify_cli_unavailable,
#if defined(BPR_BUILD_TESTS)
                        .Fixture = !fixture_name.empty(),
                        .FixtureName = std::move(fixture_name),
                        .ExpectedActiveHostText =
                            std::move(expected_active_host_text),
                        .Appearance =
                            automated_run::
                                native_appearance_probe_state{
                                    .Enabled =
                                        native_appearance_probe,
                                    .EvidencePaths =
                                        native_appearance_paths.value_or(
                                            native_appearance_evidence_paths{}),
                                },
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
