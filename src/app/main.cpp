#include "bpr/app/github_application.hpp"

#if defined(BPR_BUILD_TESTING)
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

#if defined(BPR_BUILD_TESTING)
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QStyle>
#include <QStyleHints>
#include <QTreeView>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#if defined(BPR_BUILD_TESTING)
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

#if defined(BPR_BUILD_TESTING)
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
#if defined(BPR_BUILD_TESTING)
        epoch_history,
        epoch_refresh,
        epoch_whole_pr,
        epoch_previous_upload,
        epoch_last_reviewed,
        epoch_arbitrary_left,
        epoch_arbitrary,
        epoch_reverse_left,
        epoch_reverse,
        epoch_identical_left,
        epoch_identical,
        epoch_maintenance_history,
        epoch_maintenance_select_right,
        epoch_maintenance_prune,
        epoch_maintenance_prune_result,
        epoch_maintenance_delete_result,
        appearance,
#endif
        shutdown,
    };

    struct cli_unavailable_probe_state
    {
        bool Enabled = false;
        std::optional<std::string> EnterpriseHostname;
        int ActiveRow = -1;
        std::size_t Expected = 0U;
        std::size_t Observed = 0U;
        std::size_t IdentityUpdatesBeforeConnect = 0U;
        std::size_t DiagnosticUpdatesBeforeConnect = 0U;
        std::string ExpectedActiveHostText;
    };

    stage Stage = stage::start;
    std::size_t Attempts = 0U;
    bool PresentationPassed = false;
    bool RenderSaved = true;
    bool VerifyNativePlatformVault = false;
    QString LastEpochStatus;
    QString LastEpochComparison;
    QString LastEpochLeft;
    QString LastEpochRight;
    int LastEpochCoreStatus = -1;
    int LastEpochLeftRow = -1;
    int LastEpochRightRow = -1;
    std::size_t LastEpochCount = 0U;
    bool LastEpochHistoryEnabled = false;
    int FailedStage = -1;
    cli_unavailable_probe_state Cli;
#if defined(BPR_BUILD_TESTING)
    struct native_appearance_probe_state
    {
        bool Enabled = false;
        bool WaitingForOwnerFocusReported = false;
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
        bool InitialWindowActive = false;
        QWidget* InitialFocus = nullptr;
        std::size_t InitialModelResets = 0U;
        QString ExpectedEpochLeftOid;
        QString ExpectedEpochRightOid;
        QPersistentModelIndex EpochLeft;
        QPersistentModelIndex EpochRight;
        QString EpochLeftIdentity;
        QString EpochRightIdentity;
        QPalette InitialEpochPalette;
        std::size_t InitialEpochModelResets = 0U;
        std::size_t InitialEpochAppearanceUpdates = 0U;
        QImage InitialRender;
        QImage ChangedRender;
        native_appearance_evidence_paths EvidencePaths;
    };

    bool Fixture = false;
    std::string FixtureName;
    std::string ExpectedActiveHostText;
    bpr::app::detail::github_epoch_fixture_phase EpochPhase =
        bpr::app::detail::github_epoch_fixture_phase::single;
    enum class epoch_maintenance_probe
    {
        none,
        prune,
        verify_pruned_and_delete,
        verify_deleted,
    };
    epoch_maintenance_probe MaintenanceProbe = epoch_maintenance_probe::none;
    bpr::core::review_epoch_id MaintenanceTarget;
    int MaintenanceLeftRow = -1;
    int MaintenanceRightRow = -1;
    int MaintenanceAccountReads = 0;
    int MaintenanceSmartAcquires = 0;
    std::size_t MaintenanceModelResets = 0U;
    int EpochLeftRow = -1;
    int EpochRightRow = -1;
    native_appearance_probe_state Appearance;
#endif
    std::optional<std::string_view> RenderPath;
};

#if defined(BPR_BUILD_TESTING)
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
    const auto* const control = window.epoch_control();
    const auto* const epoch_model = control->history_model();
    const auto* const left_selector = control->left_selector();
    const auto* const right_selector = control->right_selector();
    const auto* const left_oid = control->left_oid_editor();
    const auto* const right_oid = control->right_oid_editor();

    const auto epoch_context_preserved =
        run.Appearance.EpochLeft.isValid()
        && run.Appearance.EpochRight.isValid()
        && left_selector->currentIndex()
               == run.Appearance.EpochLeft.row()
        && right_selector->currentIndex()
               == run.Appearance.EpochRight.row()
        && epoch_model->index(left_selector->currentIndex(), 0)
               == run.Appearance.EpochLeft
        && epoch_model->index(right_selector->currentIndex(), 0)
               == run.Appearance.EpochRight
        && run.Appearance.EpochLeft.data(
               bpr::qt::epoch_history_model::identity_role)
               .toString()
               == run.Appearance.EpochLeftIdentity
        && run.Appearance.EpochRight.data(
               bpr::qt::epoch_history_model::identity_role)
               .toString()
               == run.Appearance.EpochRightIdentity
        && left_oid->text() == run.Appearance.ExpectedEpochLeftOid
        && right_oid->text() == run.Appearance.ExpectedEpochRightOid
        && left_oid->isReadOnly() && right_oid->isReadOnly()
        && control->whole_pr_action()->isChecked()
        && epoch_model->statistics().ModelResets
               == run.Appearance.InitialEpochModelResets
        && !left_selector->accessibleName().isEmpty()
        && !left_selector->accessibleDescription().isEmpty()
        && !right_selector->accessibleName().isEmpty()
        && !right_selector->accessibleDescription().isEmpty()
        && !left_oid->accessibleName().isEmpty()
        && !left_oid->accessibleDescription().isEmpty()
        && !right_oid->accessibleName().isEmpty()
        && !right_oid->accessibleDescription().isEmpty()
        && !control->testAttribute(Qt::WA_SetPalette)
        && !control->testAttribute(Qt::WA_SetStyle)
        && !control->testAttribute(Qt::WA_SetFont)
        && !left_selector->testAttribute(Qt::WA_SetPalette)
        && !right_selector->testAttribute(Qt::WA_SetPalette)
        && !left_oid->testAttribute(Qt::WA_SetPalette)
        && !right_oid->testAttribute(Qt::WA_SetPalette)
        && control->styleSheet().isEmpty()
        && left_selector->styleSheet().isEmpty()
        && right_selector->styleSheet().isEmpty()
        && left_oid->styleSheet().isEmpty()
        && right_oid->styleSheet().isEmpty();

    return run.Appearance.Thread.isValid()
        && QGuiApplication::platformName()
               == run.Appearance.InitialPlatformName
        && QString::fromLatin1(
               QApplication::style()->metaObject()->className())
               == run.Appearance.InitialStyleClass
        && view->currentIndex() == run.Appearance.Thread
        && view->selectionModel()->isSelected(run.Appearance.Thread)
        && !view->isExpanded(run.Appearance.Thread)
        && run.Appearance.InitialWindowActive
        && run.Appearance.InitialFocus != nullptr
        && run.Appearance.InitialFocus->window() == &window
        && window.isActiveWindow()
        && QApplication::activeWindow() == &window
        && QApplication::focusWidget() == run.Appearance.InitialFocus
        && epoch_context_preserved
        && model->statistics().ModelResets
               == run.Appearance.InitialModelResets
        && window.statistics().FullResets == 0U
        && !window.testAttribute(Qt::WA_SetPalette)
        && !window.testAttribute(Qt::WA_SetStyle)
        && !window.testAttribute(Qt::WA_SetFont)
        && window.styleSheet().isEmpty();
}

[[nodiscard]] auto native_palette_readable(
    const QPalette& palette) noexcept -> bool
{
    const auto contrast = [&palette](
        const QPalette::ColorRole background,
        const QPalette::ColorRole foreground)
    {
        return std::abs(
                   palette.color(background).lightness()
                   - palette.color(foreground).lightness())
            >= 80;
    };
    return contrast(QPalette::Window, QPalette::WindowText)
        && contrast(QPalette::Base, QPalette::Text);
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
        && window.epoch_control()->palette()
               == run.Appearance.InitialEpochPalette
        && native_palette_readable(window.epoch_control()->palette())
        && window.epoch_control()->statistics().AppearanceUpdates
               == run.Appearance.InitialEpochAppearanceUpdates
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
    const auto initial_epoch = run.Appearance.InitialEpochPalette;
    const auto final_epoch = window.epoch_control()->palette();
    const auto initial_epoch_window =
        initial_epoch.color(QPalette::Window).lightness();
    const auto initial_epoch_base =
        initial_epoch.color(QPalette::Base).lightness();
    const auto initial_epoch_text =
        initial_epoch.color(QPalette::Text).lightness();
    const auto final_epoch_window =
        final_epoch.color(QPalette::Window).lightness();
    const auto final_epoch_base =
        final_epoch.color(QPalette::Base).lightness();
    const auto final_epoch_text =
        final_epoch.color(QPalette::Text).lightness();

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
        && final_epoch_window < initial_epoch_window
        && final_epoch_base < initial_epoch_base
        && final_epoch_text > initial_epoch_text
        && final_epoch_window < final_epoch_text
        && final_epoch_base < final_epoch_text
        && native_palette_readable(final_epoch)
        && window.epoch_control()->statistics().AppearanceUpdates
               > run.Appearance.InitialEpochAppearanceUpdates
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
    auto* const control = window.epoch_control();
    auto* const epoch_model = control->history_model();
    const auto thread =
        model->index_for_id("review-thread:PRRT_fixture_1");
    const auto left_epoch = epoch_model->index(
        control->left_selector()->currentIndex(), 0);
    const auto right_epoch = epoch_model->index(
        control->right_selector()->currentIndex(), 0);
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
        || !light_palette || !thread.isValid()
        || !left_epoch.isValid() || !right_epoch.isValid()
        || run.Appearance.ExpectedEpochLeftOid.isEmpty()
        || run.Appearance.ExpectedEpochRightOid.isEmpty()
        || control->left_oid_editor()->text()
               != run.Appearance.ExpectedEpochLeftOid
        || control->right_oid_editor()->text()
               != run.Appearance.ExpectedEpochRightOid
        || !control->whole_pr_action()->isChecked()
        || !run.RenderPath.has_value()
        || run.Appearance.EvidencePaths.Initial.isEmpty()
        || run.Appearance.EvidencePaths.Changed.isEmpty())
    {
        QTextStream{stdout}
            << "NATIVE_APPEARANCE_PRECONDITION:"
            << (hints != nullptr) << ':'
            << (run.Appearance.InitialPlatformName == "cocoa") << ':'
            << (run.Appearance.InitialStyleClass == "QMacStyle") << ':'
            << (hints != nullptr
                && hints->colorScheme() == Qt::ColorScheme::Light) << ':'
            << (run.Appearance.InitialGlobalColorScheme
                == Qt::ColorScheme::Light) << ':'
            << light_palette << ':' << thread.isValid() << ':'
            << left_epoch.isValid() << ':' << right_epoch.isValid() << ':'
            << (!run.Appearance.ExpectedEpochLeftOid.isEmpty()) << ':'
            << (!run.Appearance.ExpectedEpochRightOid.isEmpty()) << ':'
            << (control->left_oid_editor()->text()
                == run.Appearance.ExpectedEpochLeftOid) << ':'
            << (control->right_oid_editor()->text()
                == run.Appearance.ExpectedEpochRightOid) << ':'
            << control->whole_pr_action()->isChecked() << ':'
            << run.RenderPath.has_value() << ':'
            << (!run.Appearance.EvidencePaths.Initial.isEmpty()) << ':'
            << (!run.Appearance.EvidencePaths.Changed.isEmpty()) << '\n';
        return false;
    }

    run.Appearance.Thread = QPersistentModelIndex{thread};
    run.Appearance.InitialModelResets = model->statistics().ModelResets;
    run.Appearance.EpochLeft = QPersistentModelIndex{left_epoch};
    run.Appearance.EpochRight = QPersistentModelIndex{right_epoch};
    run.Appearance.EpochLeftIdentity = left_epoch.data(
        bpr::qt::epoch_history_model::identity_role).toString();
    run.Appearance.EpochRightIdentity = right_epoch.data(
        bpr::qt::epoch_history_model::identity_role).toString();
    run.Appearance.InitialEpochModelResets =
        epoch_model->statistics().ModelResets;
    run.Appearance.InitialEpochPalette = control->palette();
    run.Appearance.InitialEpochAppearanceUpdates =
        control->statistics().AppearanceUpdates;
    view->setCurrentIndex(thread);
    view->selectionModel()->select(
        thread,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    view->collapse(thread);
    QApplication::processEvents();
    run.Appearance.InitialWindowActive = window.isActiveWindow();
    run.Appearance.InitialFocus = QApplication::focusWidget();
    if(!native_appearance_light_state_passed(window, run))
    {
        const auto* const focus = QApplication::focusWidget();
        QTextStream{stdout}
            << "NATIVE_APPEARANCE_EPOCH_CONTEXT:"
            << (QApplication::focusWidget()
                == run.Appearance.InitialFocus) << ':'
            << (control->palette()
                == run.Appearance.InitialEpochPalette) << ':'
            << native_palette_readable(control->palette()) << ':'
            << (control->statistics().AppearanceUpdates
                == run.Appearance.InitialEpochAppearanceUpdates) << ':'
            << native_appearance_context_passed(window, run) << '\n'
            << "NATIVE_APPEARANCE_FOCUS:"
            << window.isActiveWindow() << ':' << view->isVisible() << ':'
            << control->isVisible() << ':'
            << control->right_oid_editor()->isVisible() << ':'
            << (focus != nullptr
                    ? QString::fromLatin1(
                          focus->metaObject()->className())
                    : QStringLiteral("none"))
            << ':'
            << (focus != nullptr
                    ? focus->objectName()
                    : QStringLiteral("none"))
            << ':' << static_cast<int>(view->focusPolicy()) << '\n';
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

#if defined(BPR_BUILD_TESTING)
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

[[nodiscard]] auto fixture_epoch_presentation_passed(
    const bpr::qt::github_window& window,
    const std::size_t minimum_epochs = 1U) -> bool
{
    const auto* const control = window.epoch_control();
    return control->status_text().startsWith(
               QStringLiteral("Review history is ready."))
        && control->comparison_text().endsWith(
            QStringLiteral("Ready for diff."))
        && control->history_model()->epoch_count() >= minimum_epochs
        && !control->left_oid_editor()->text().isEmpty()
        && !control->right_oid_editor()->text().isEmpty();
}

[[nodiscard]] auto fixture_epoch_pair_passed(
    const bpr::qt::github_window& window,
    const std::string& left,
    const std::string& right,
    const std::size_t minimum_epochs = 2U) -> bool
{
    const auto* const control = window.epoch_control();
    return fixture_epoch_presentation_passed(window, minimum_epochs)
        && control->left_oid_editor()->text()
               == QString::fromStdString(left)
        && control->right_oid_editor()->text()
               == QString::fromStdString(right);
}

[[nodiscard]] auto fixture_anchor_row(
    const bpr::qt::epoch_history_model& model,
    const std::uint64_t ordinal,
    const bpr::core::comparison_anchor_kind kind) -> int
{
    for(auto row = 0; row < model.rowCount(); ++row)
    {
        const auto index = model.index(row, 0);
        if(index.data(bpr::qt::epoch_history_model::epoch_ordinal_role)
                   .toULongLong()
               == ordinal
           && index.data(bpr::qt::epoch_history_model::anchor_kind_role)
                      .toInt()
               == static_cast<int>(kind))
        {
            return row;
        }
    }
    return -1;
}

[[nodiscard]] auto fixture_epoch_cause_passed(
    const bpr::qt::epoch_history_model& model,
    const std::uint64_t ordinal,
    const bpr::core::epoch_primary_cause expected) -> bool
{
    const auto row = fixture_anchor_row(
        model,
        ordinal,
        bpr::core::comparison_anchor_kind::epoch_head);
    return row >= 0
        && model.index(row, 0)
                   .data(bpr::qt::epoch_history_model::primary_cause_role)
                   .toInt()
            == static_cast<int>(expected);
}

[[nodiscard]] auto fixture_epoch_id(
    const bpr::qt::epoch_history_model& model,
    const std::uint64_t ordinal) -> std::optional<bpr::core::review_epoch_id>
{
    const auto row = fixture_anchor_row(
        model, ordinal, bpr::core::comparison_anchor_kind::epoch_base);
    if(row < 0)
    {
        return std::nullopt;
    }
    const auto anchor = model.anchor_at(model.index(row, 0));
    if(!anchor)
    {
        return std::nullopt;
    }
    return bpr::core::review_epoch_id{
        .Authority = anchor->Identity.Authority,
        .Incarnation = anchor->Identity.Incarnation,
        .Ordinal = anchor->Identity.Epoch,
    };
}

[[nodiscard]] auto fixture_epoch_availability_is(
    const bpr::qt::epoch_history_model& model,
    const std::uint64_t ordinal,
    const bpr::core::anchor_availability expected) -> bool
{
    const auto base = fixture_anchor_row(
        model, ordinal, bpr::core::comparison_anchor_kind::epoch_base);
    const auto head = fixture_anchor_row(
        model, ordinal, bpr::core::comparison_anchor_kind::epoch_head);
    return base >= 0 && head >= 0
        && model.index(base, 0)
                   .data(bpr::qt::epoch_history_model::availability_role)
                   .toInt()
            == static_cast<int>(expected)
        && model.index(head, 0)
                   .data(bpr::qt::epoch_history_model::availability_role)
                   .toInt()
            == static_cast<int>(expected);
}

[[nodiscard]] auto fixture_all_epoch_availability_is(
    const bpr::qt::epoch_history_model& model,
    const bpr::core::anchor_availability expected) -> bool
{
    if(model.rowCount() == 0)
    {
        return false;
    }
    for(auto row = 0; row < model.rowCount(); ++row)
    {
        if(model.index(row, 0)
                   .data(bpr::qt::epoch_history_model::availability_role)
                   .toInt()
           != static_cast<int>(expected))
        {
            return false;
        }
    }
    return true;
}

void accept_maintenance_confirmation(const QString& object_name)
{
    auto attempts = std::make_shared<std::size_t>(0U);
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [object_name, attempts, poll]
    {
        auto* const dialog = qobject_cast<QMessageBox*>(
            QApplication::activeModalWidget());
        if(dialog != nullptr && dialog->objectName() == object_name)
        {
            auto* const confirm = dialog->findChild<QPushButton*>(
                object_name + QStringLiteral("ConfirmButton"));
            if(confirm != nullptr)
            {
                confirm->click();
                return;
            }
        }
        if(++*attempts <= 100U)
        {
            QTimer::singleShot(1, qApp, *poll);
        }
    };
    QTimer::singleShot(0, qApp, *poll);
}

void advance_fixture_stage(
    automated_run& run, automated_run::stage next) noexcept;

[[nodiscard]] auto begin_epoch_maintenance_probe(
    bpr::app::github_application& application,
    automated_run& run) -> bool
{
    auto* const control = application.window().epoch_control();
    auto* const model = control->history_model();
    if(run.MaintenanceProbe
       == automated_run::epoch_maintenance_probe::verify_deleted)
    {
        const auto provider = bpr::app::detail::
            github_application_fixture_access::evidence(application);
        const auto maintenance = bpr::app::detail::
            github_application_fixture_access::maintenance_evidence(
                application, {});
        return provider.SmartAcquireCount == 1
            && provider.SmartDiscoveryCount == 1
            && provider.SmartLsRefsCount == 1
            && provider.SmartFetchCount == 1
            && !maintenance.RetiredRepositoryPresent
            && maintenance.RetiredCacheDeletionMarkerPresent
            && maintenance.PruneIntentDispatchCount == 0U
            && maintenance.DeleteIntentDispatchCount == 0U;
    }
    if(model->epoch_count() < 2U)
    {
        if(control->return_to_newest_action()->isEnabled())
        {
            control->return_to_newest_action()->trigger();
        }
        else if(control->load_history_action()->isEnabled())
        {
            control->load_history_action()->trigger();
        }
        else
        {
            return false;
        }
        advance_fixture_stage(
            run, automated_run::stage::epoch_maintenance_history);
        return true;
    }

    const auto target = fixture_epoch_id(*model, 1U);
    if(!target)
    {
        return false;
    }
    run.MaintenanceTarget = *target;
    const auto provider = bpr::app::detail::
        github_application_fixture_access::evidence(application);
    const auto maintenance = bpr::app::detail::
        github_application_fixture_access::maintenance_evidence(
            application, *target);

    switch(run.MaintenanceProbe)
    {
    case automated_run::epoch_maintenance_probe::prune:
        run.MaintenanceLeftRow = fixture_anchor_row(
            *model, 1U, bpr::core::comparison_anchor_kind::epoch_base);
        run.MaintenanceRightRow = fixture_anchor_row(
            *model, 1U, bpr::core::comparison_anchor_kind::epoch_head);
        if(run.MaintenanceLeftRow < 0 || run.MaintenanceRightRow < 0
           || !maintenance.TargetBaseRefPresent
           || !maintenance.TargetHeadRefPresent
           || !maintenance.NonTargetBaseRefPresent
           || !maintenance.NonTargetHeadRefPresent
           || !maintenance.RepositoryPresent
           || maintenance.CacheDeletionMarkerPresent)
        {
            return false;
        }
        run.MaintenanceAccountReads = provider.AccountReadCount;
        run.MaintenanceSmartAcquires = provider.SmartAcquireCount;
        run.MaintenanceModelResets = model->statistics().ModelResets;
        control->left_selector()->setCurrentIndex(run.MaintenanceLeftRow);
        advance_fixture_stage(
            run, automated_run::stage::epoch_maintenance_select_right);
        return true;

    case automated_run::epoch_maintenance_probe::verify_pruned_and_delete:
        if(!fixture_epoch_availability_is(*model,
               1U,
               bpr::core::anchor_availability::pruned)
           || maintenance.TargetBaseRefPresent
           || maintenance.TargetHeadRefPresent
           || !maintenance.NonTargetBaseRefPresent
           || !maintenance.NonTargetHeadRefPresent
           || !maintenance.RepositoryPresent
           || maintenance.CacheDeletionMarkerPresent
           || maintenance.PruneIntentDispatchCount != 0U
           || maintenance.DeleteIntentDispatchCount != 0U
           || !control->delete_cache_action()->isEnabled())
        {
            return false;
        }
        run.MaintenanceAccountReads = provider.AccountReadCount;
        run.MaintenanceSmartAcquires = provider.SmartAcquireCount;
        run.MaintenanceModelResets = model->statistics().ModelResets;
        accept_maintenance_confirmation(
            QStringLiteral("deleteRepositoryCacheConfirmationDialog"));
        control->delete_cache_action()->trigger();
        advance_fixture_stage(
            run, automated_run::stage::epoch_maintenance_delete_result);
        return true;

    case automated_run::epoch_maintenance_probe::verify_deleted:
        break;

    case automated_run::epoch_maintenance_probe::none:
        break;
    }
    return false;
}

[[nodiscard]] constexpr auto expected_fixture_smart_calls(
    const bpr::app::detail::github_epoch_fixture_phase phase) noexcept
    -> int
{
    switch(phase)
    {
    case bpr::app::detail::github_epoch_fixture_phase::single:
        return 1U;
    case bpr::app::detail::github_epoch_fixture_phase::seed:
        return 2U;
    case bpr::app::detail::github_epoch_fixture_phase::reuse:
        return 0U;
    }
    return 0U;
}

void advance_fixture_stage(
    automated_run& run, const automated_run::stage next) noexcept
{
    run.Stage = next;
    run.Attempts = 0U;
}
#endif

[[nodiscard]] auto cli_fail_closed_presentation_passed(
    const bpr::qt::github_window& window,
    const automated_run& run) -> bool
{
    const auto statistics = window.statistics();
    const auto& cli = run.Cli;
    return window.profile_selector()->currentIndex()
               == cli.ActiveRow
        && window.active_host_text() == cli.ExpectedActiveHostText
        && statistics.IdentityUpdates
               == cli.IdentityUpdatesBeforeConnect + 1U
        && statistics.DiagnosticUpdates
               == cli.DiagnosticUpdatesBeforeConnect + 1U
        && window.status_text() == "The GitHub connection failed."
        && window.diagnostic_text()
               == "The selected GitHub CLI connection is unavailable."
        && window.active_account_text() == "Not authenticated"
        && !window.open_action()->isEnabled()
        && statistics.FullResets == 0U;
}

[[nodiscard]] auto is_cli_unavailable_profile(
    const QComboBox& selector,
    const int row) -> bool
{
    return selector.itemText(row).endsWith(
        QStringLiteral("GitHub CLI (unavailable)"));
}

[[nodiscard]] auto cli_unavailable_profile_count(
    const QComboBox& selector) -> std::size_t
{
    auto count = std::size_t{0U};
    for(auto row = 0; row < selector.count(); ++row)
    {
        if(is_cli_unavailable_profile(selector, row))
        {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] auto next_cli_unavailable_profile(
    const QComboBox& selector,
    const int after_row) -> int
{
    for(auto row = after_row + 1; row < selector.count(); ++row)
    {
        if(is_cli_unavailable_profile(selector, row))
        {
            return row;
        }
    }
    return -1;
}

[[nodiscard]] auto expected_cli_active_host_text(
    const QComboBox& selector,
    const int row,
    const std::optional<std::string>& enterprise_hostname)
    -> std::optional<std::string>
{
    const auto label = selector.itemText(row);
    if(label == QStringLiteral("GitHub.com — GitHub CLI (unavailable)"))
    {
        return std::string{"Active host: github.com (GitHub.com)"};
    }
    if(
        label == QStringLiteral("GHES 3.18 — GitHub CLI (unavailable)")
        && enterprise_hostname.has_value())
    {
        return std::string{"Active host: "} + *enterprise_hostname
            + " (GitHub Enterprise Server)";
    }
    return std::nullopt;
}

[[nodiscard]] auto activate_cli_unavailable_profile(
    bpr::qt::github_window& window,
    automated_run& run,
    const int row) -> bool
{
    auto& cli = run.Cli;
    auto expected_host = expected_cli_active_host_text(
        *window.profile_selector(), row, cli.EnterpriseHostname);
    if(!expected_host.has_value())
    {
        return false;
    }

    window.profile_selector()->setCurrentIndex(row);
    if(
        window.profile_selector()->currentIndex() != row
        || !window.connect_action()->isEnabled())
    {
        return false;
    }

    const auto statistics = window.statistics();
    cli.ActiveRow = row;
    cli.IdentityUpdatesBeforeConnect = statistics.IdentityUpdates;
    cli.DiagnosticUpdatesBeforeConnect =
        statistics.DiagnosticUpdates;
    cli.ExpectedActiveHostText = std::move(*expected_host);
    window.connect_action()->trigger();
    return true;
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
    const auto cli_unavailable_evidence_passed =
        !run.Cli.Enabled
        || (
            run.Cli.Expected > 0U
            && run.Cli.Observed == run.Cli.Expected);
    auto passed = run.PresentationPassed && status.ShutdownComplete
        && !status.ShutdownCleanupFailed
        && !status.EpochProjectionDeliveryFailed
        && !status.EpochHandoffDeliveryFailed
        && native_vault_evidence_passed
        && cli_unavailable_evidence_passed;
    auto render_saved = run.RenderSaved;

#if defined(BPR_BUILD_TESTING)
    const auto fixture_evidence =
        run.Fixture
        ? bpr::app::detail::github_application_fixture_access::evidence(
              application)
        : bpr::app::detail::github_public_path_fixture_evidence{};
    if(run.Fixture)
    {
        const auto expected_smart_calls =
            run.MaintenanceProbe
                    == automated_run::epoch_maintenance_probe::verify_deleted
                ? 1
                : expected_fixture_smart_calls(run.EpochPhase);
        passed = passed && fixture_evidence.RecordingConsumed
            && fixture_evidence.CapabilityProbeConsumed
            && fixture_evidence.BootstrapTransportSelected
            && fixture_evidence.AccountTransportSelected
            && fixture_evidence.CredentialStored
            && fixture_evidence.CredentialRetrieved
            && fixture_evidence.CredentialDeleted
            && !fixture_evidence.SecretRetained
            && fixture_evidence.SmartAcquireCount
                == expected_smart_calls
            && fixture_evidence.SmartDiscoveryCount
                == expected_smart_calls
            && fixture_evidence.SmartLsRefsCount
                == expected_smart_calls
            && fixture_evidence.SmartFetchCount
                == expected_smart_calls;
        if(run.MaintenanceProbe
           == automated_run::epoch_maintenance_probe::verify_deleted)
        {
            const auto maintenance = bpr::app::detail::
                github_application_fixture_access::maintenance_evidence(
                    application, {});
            passed = passed && maintenance.TargetBaseCacheDeleted
                && maintenance.TargetHeadCacheDeleted
                && !maintenance.RetiredRepositoryPresent
                && maintenance.RetiredCacheDeletionMarkerPresent
                && maintenance.PruneIntentDispatchCount == 0U
                && maintenance.DeleteIntentDispatchCount == 0U;
        }
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
#if defined(BPR_BUILD_TESTING)
    if(run.Fixture)
    {
        stream << "GITHUB_FIXTURE_QT_PLATFORM:"
               << QGuiApplication::platformName() << '\n'
               << "GITHUB_FIXTURE_QT_STYLE:"
               << application.window().style()->metaObject()->className()
               << '\n';
    }
#endif
    stream << (passed ? "PUBLIC_PATH_OK\n" : "PUBLIC_PATH_FAILED\n");
    if(run.VerifyNativePlatformVault)
    {
        stream << "NATIVE_PLATFORM_VAULT_SELECTED:"
               << (status.NativePlatformVaultSelected ? "true" : "false")
               << '\n';
    }
    if(run.Cli.Enabled)
    {
        stream << (passed ? "CLI_UNAVAILABLE_OK\n"
                          : "CLI_UNAVAILABLE_FAILED\n")
                << "CLI_UNAVAILABLE_COUNT:"
                << static_cast<unsigned long long>(
                       run.Cli.Observed)
                << '\n';
    }
#if defined(BPR_BUILD_TESTING)
    if(run.Fixture
       && run.EpochPhase
              != bpr::app::detail::github_epoch_fixture_phase::single)
    {
        const auto phase = run.EpochPhase
                == bpr::app::detail::github_epoch_fixture_phase::seed
            ? QStringLiteral("SEED")
            : QStringLiteral("REUSE");
        stream << "EPOCH_FIXTURE_" << phase << (passed ? "_OK\n" : "_FAILED\n");
    }
    if(run.Fixture
       && run.MaintenanceProbe
              != automated_run::epoch_maintenance_probe::none)
    {
        const auto marker = run.MaintenanceProbe
                == automated_run::epoch_maintenance_probe::prune
            ? QStringLiteral("EPOCH_MAINTENANCE_PRUNE")
            : run.MaintenanceProbe
                    == automated_run::epoch_maintenance_probe::
                        verify_pruned_and_delete
                ? QStringLiteral("EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE")
                : QStringLiteral("EPOCH_MAINTENANCE_DELETED_RESTART");
        stream << marker << (passed ? "_OK\n" : "_FAILED\n");
    }
#endif
    if(!passed)
    {
        const auto& window = application.window();
        stream << "PUBLIC_PATH_STAGE:"
               << (run.FailedStage >= 0 ? run.FailedStage
                                        : static_cast<int>(run.Stage))
               << '\n'
               << "PUBLIC_PATH_STATUS:"
               << QString::fromStdString(window.status_text()) << '\n'
               << "PUBLIC_PATH_DIAGNOSTIC:"
               << QString::fromStdString(window.diagnostic_text()) << '\n'
               << "PUBLIC_PATH_ACCOUNT:"
               << QString::fromStdString(window.active_account_text()) << '\n'
               << "PUBLIC_PATH_EPOCH_STATUS:"
               << (run.LastEpochStatus.isEmpty()
                       ? window.epoch_control()->status_text()
                       : run.LastEpochStatus)
               << '\n'
               << "PUBLIC_PATH_EPOCH_COMPARISON:"
               << (run.LastEpochComparison.isEmpty()
                       ? window.epoch_control()->comparison_text()
                       : run.LastEpochComparison)
               << '\n'
               << "PUBLIC_PATH_EPOCH_PAIR:"
               << (run.LastEpochLeft.isEmpty()
                       ? window.epoch_control()->left_oid_editor()->text()
                       : run.LastEpochLeft)
               << ':'
               << (run.LastEpochRight.isEmpty()
                       ? window.epoch_control()->right_oid_editor()->text()
                       : run.LastEpochRight)
               << ':'
               << (run.LastEpochLeftRow >= 0
                       ? run.LastEpochLeftRow
                       : window.epoch_control()->left_selector()->currentIndex())
               << ':'
               << (run.LastEpochRightRow >= 0
                       ? run.LastEpochRightRow
                       : window.epoch_control()->right_selector()->currentIndex())
               << ':'
               << static_cast<unsigned long long>(
                      run.LastEpochCount != 0U
                          ? run.LastEpochCount
                          : window.epoch_control()
                                ->history_model()
                                ->epoch_count())
               << ':'
               << (run.LastEpochHistoryEnabled
                           || window.epoch_control()
                                  ->load_history_action()
                                  ->isEnabled()
                       ? 1
                       : 0)
               << '\n'
               << "PUBLIC_PATH_EPOCH_CORE_STATUS:"
               << run.LastEpochCoreStatus << '\n';
#if defined(BPR_BUILD_TESTING)
        if(run.MaintenanceProbe
           != automated_run::epoch_maintenance_probe::none)
        {
            const auto maintenance = bpr::app::detail::
                github_application_fixture_access::maintenance_evidence(
                    application, run.MaintenanceTarget);
            stream << "PUBLIC_PATH_MAINTENANCE_EVIDENCE:"
                   << maintenance.PruneIntentDispatchCount << ':'
                   << maintenance.DeleteIntentDispatchCount << ':'
                   << maintenance.CatalogRevision << ':'
                   << maintenance.TargetBaseRefPresent << ':'
                   << maintenance.TargetHeadRefPresent << ':'
                   << maintenance.NonTargetBaseRefPresent << ':'
                   << maintenance.NonTargetHeadRefPresent << ':'
                   << maintenance.RepositoryPresent << ':'
                   << maintenance.CacheDeletionMarkerPresent << ':'
                   << maintenance.TargetBaseCacheDeleted << ':'
                   << maintenance.TargetHeadCacheDeleted << ':'
                   << maintenance.RetiredRepositoryPresent << ':'
                   << maintenance.RetiredCacheDeletionMarkerPresent << ':'
                   << maintenance.TargetBaseAvailability << ':'
                   << maintenance.TargetHeadAvailability << '\n';
        }
#endif
    }
#if defined(BPR_BUILD_TESTING)
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
            stream << "GITHUB_FIXTURE_SMART_EVIDENCE:"
                   << fixture_evidence.SmartAcquireCount << ':'
                   << fixture_evidence.SmartDiscoveryCount << ':'
                   << fixture_evidence.SmartLsRefsCount << ':'
                   << fixture_evidence.SmartFetchCount << '\n';
        }
    }
#endif
    if(run.RenderPath.has_value())
    {
        stream << (render_saved ? "RENDER_OK\n" : "RENDER_FAILED\n");
    }
#if defined(BPR_BUILD_TESTING)
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
               << (run.Appearance.InitialWindowActive
                           && run.Appearance.InitialFocus != nullptr
                           && run.Appearance.StatePassed
                       ? "NATIVE_APPEARANCE_ACTIVE_FOCUS_OK\n"
                       : "NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED\n")
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

void fail_automated_run(
    bpr::app::github_application& application,
    automated_run& run)
{
    run.PresentationPassed = false;
    transition_to_shutdown(application, run);
}

void capture_automated_timeout_evidence(
    bpr::app::github_application& application,
    automated_run& run)
{
    auto& window = application.window();
    run.PresentationPassed = false;
    run.FailedStage = static_cast<int>(run.Stage);
    run.LastEpochStatus = window.epoch_control()->status_text();
    run.LastEpochComparison = window.epoch_control()->comparison_text();
    run.LastEpochLeft = window.epoch_control()->left_oid_editor()->text();
    run.LastEpochRight = window.epoch_control()->right_oid_editor()->text();
    run.LastEpochLeftRow =
        window.epoch_control()->left_selector()->currentIndex();
    run.LastEpochRightRow =
        window.epoch_control()->right_selector()->currentIndex();
    run.LastEpochCount =
        window.epoch_control()->history_model()->epoch_count();
    run.LastEpochHistoryEnabled =
        window.epoch_control()->load_history_action()->isEnabled();
    run.LastEpochCoreStatus =
        static_cast<int>(application.status().EpochStatus);
#if defined(BPR_BUILD_TESTING)
    if(!run.Appearance.Enabled)
#endif
    {
        run.RenderSaved = save_render_evidence(window, run.RenderPath);
    }
    transition_to_shutdown(application, run);
}

void schedule_automated_run(
    bpr::app::github_application& application,
    std::shared_ptr<automated_run> run)
{
#if defined(BPR_BUILD_TESTING)
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
#if defined(BPR_BUILD_TESTING)
            run->Fixture
                && (run->Stage == automated_run::stage::start
                    || run->Stage == automated_run::stage::connect
                    || run->Stage == automated_run::stage::open
                    || run->Stage == automated_run::stage::epoch_history
                    || run->Stage == automated_run::stage::epoch_refresh
                    || run->Stage
                        == automated_run::stage::epoch_maintenance_history
                    || run->Stage
                        == automated_run::stage::epoch_maintenance_prune_result
                    || run->Stage
                        == automated_run::stage::epoch_maintenance_delete_result
                    || run->Stage == automated_run::stage::appearance)
                ? 6'000U
                :
#endif
                                      1'000U;
        if(run->Stage != automated_run::stage::shutdown
           && ++run->Attempts > maximum_attempts)
        {
            capture_automated_timeout_evidence(application, *run);
        }

        switch(run->Stage)
        {
        case automated_run::stage::start:
#if defined(BPR_BUILD_TESTING)
            if(run->Fixture)
            {
                window.connect_action()->trigger();
                advance_fixture_stage(
                    *run, automated_run::stage::connect);
            }
            else
#endif
            {
                const auto visible_cli_profiles =
                    cli_unavailable_profile_count(
                        *window.profile_selector());
                const auto expected_cli_profiles =
                    run->Cli.EnterpriseHostname.has_value() ? 2U : 1U;
                if(
                    run->Cli.Enabled
                    && (
                        visible_cli_profiles != expected_cli_profiles
                        || window.profile_selector()->count()
                               != static_cast<int>(
                                   expected_cli_profiles * 2U)))
                {
                    fail_automated_run(application, *run);
                    break;
                }
                run->Cli.Expected =
                    run->Cli.Enabled
                    ? expected_cli_profiles
                    : 1U;
                const auto first_cli_profile =
                    next_cli_unavailable_profile(
                        *window.profile_selector(), -1);
                if(
                    first_cli_profile < 0
                    || !activate_cli_unavailable_profile(
                        window, *run, first_cli_profile))
                {
                    fail_automated_run(application, *run);
                    break;
                }
                run->Stage = automated_run::stage::connect;
            }
            break;

        case automated_run::stage::connect:
#if defined(BPR_BUILD_TESTING)
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
                advance_fixture_stage(*run, automated_run::stage::open);
            }
            else if(cli_fail_closed_presentation_passed(window, *run))
#else
            if(cli_fail_closed_presentation_passed(window, *run))
#endif
            {
                ++run->Cli.Observed;
                const auto next_cli_profile =
                    run->Cli.Enabled
                    ? next_cli_unavailable_profile(
                          *window.profile_selector(),
                          run->Cli.ActiveRow)
                    : -1;
                if(next_cli_profile >= 0)
                {
                    if(!activate_cli_unavailable_profile(
                           window, *run, next_cli_profile))
                    {
                        fail_automated_run(application, *run);
                    }
                }
                else
                {
                    run->PresentationPassed =
                        run->Cli.Observed == run->Cli.Expected;
                    run->RenderSaved =
                        save_render_evidence(window, run->RenderPath);
                    transition_to_shutdown(application, *run);
                }
            }
            break;

        case automated_run::stage::open:
#if defined(BPR_BUILD_TESTING)
            if(run->MaintenanceProbe
               == automated_run::epoch_maintenance_probe::verify_deleted)
            {
                if(begin_epoch_maintenance_probe(application, *run))
                {
                    run->PresentationPassed = true;
                    transition_to_shutdown(application, *run);
                }
                break;
            }
            if(fixture_presentation_passed(
                   window, run->ExpectedActiveHostText))
            {
                const auto evidence = bpr::app::detail::
                    github_application_fixture_access::evidence(application);
                const auto persistent_phase =
                    run->EpochPhase
                    != bpr::app::detail::
                        github_epoch_fixture_phase::single;
                const auto initial_head =
                    run->EpochPhase
                            == bpr::app::detail::
                                github_epoch_fixture_phase::reuse
                        ? evidence.HeadTwoOid
                        : evidence.HeadOneOid;
                const auto initial_epochs =
                    run->EpochPhase
                            == bpr::app::detail::
                                github_epoch_fixture_phase::reuse
                        ? 0U
                        : 1U;
                if(run->MaintenanceProbe
                   != automated_run::epoch_maintenance_probe::none)
                {
                    if(begin_epoch_maintenance_probe(application, *run))
                    {
                    }
                    break;
                }
                if(!fixture_epoch_pair_passed(
                       window,
                       evidence.BaseOid,
                       initial_head,
                       initial_epochs))
                {
                    break;
                }
                if(run->EpochPhase
                   == bpr::app::detail::github_epoch_fixture_phase::seed)
                {
                    window.refresh_action()->trigger();
                    advance_fixture_stage(
                        *run, automated_run::stage::epoch_refresh);
                }
                else if(persistent_phase)
                {
                    auto* const control = window.epoch_control();
                    if(control->history_model()->epoch_count() < 2U)
                    {
                        if(control->return_to_newest_action()->isEnabled())
                        {
                            control->return_to_newest_action()->trigger();
                        }
                        else if(control->load_history_action()->isEnabled())
                        {
                            control->load_history_action()->trigger();
                        }
                        else
                        {
                            break;
                        }
                        advance_fixture_stage(
                            *run, automated_run::stage::epoch_history);
                    }
                    else
                    {
                        control->whole_pr_action()->trigger();
                        advance_fixture_stage(
                            *run, automated_run::stage::epoch_whole_pr);
                    }
                }
                else if(run->Appearance.Enabled)
                {
                    run->Appearance.ExpectedEpochLeftOid =
                        QString::fromStdString(evidence.BaseOid);
                    run->Appearance.ExpectedEpochRightOid =
                        QString::fromStdString(evidence.HeadOneOid);
                    const auto waiting_for_owner_focus =
                        QGuiApplication::platformName() == "cocoa"
                        && (
                            !window.isActiveWindow()
                            || QApplication::focusWidget() == nullptr);
                    if(waiting_for_owner_focus)
                    {
                        if(!run->Appearance.WaitingForOwnerFocusReported)
                        {
                            QTextStream stream{stdout};
                            stream
                                << "NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS\n";
                            stream.flush();
                            run->Appearance.WaitingForOwnerFocusReported =
                                true;
                        }
                        break;
                    }
                    if(prepare_native_appearance_probe(window, *run))
                    {
                        run->Stage = automated_run::stage::appearance;
                    }
                    else
                    {
                        fail_automated_run(application, *run);
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
            fail_automated_run(application, *run);
#endif
            break;

#if defined(BPR_BUILD_TESTING)
        case automated_run::stage::epoch_history:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            if(fixture_epoch_pair_passed(
                   window, evidence.BaseOid, evidence.HeadTwoOid)
               && fixture_epoch_cause_passed(
                   *window.epoch_control()->history_model(),
                   2U,
                   bpr::core::epoch_primary_cause::force_push_rebase))
            {
                window.epoch_control()->whole_pr_action()->trigger();
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_whole_pr);
            }
            break;
        }

        case automated_run::stage::epoch_refresh:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            if(fixture_presentation_passed(
                   window, run->ExpectedActiveHostText)
               && fixture_epoch_pair_passed(
                   window,
                   evidence.BaseOid,
                   evidence.HeadTwoOid)
               && fixture_epoch_cause_passed(
                   *window.epoch_control()->history_model(),
                   2U,
                   bpr::core::epoch_primary_cause::force_push_rebase))
            {
                window.epoch_control()->whole_pr_action()->trigger();
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_whole_pr);
            }
            break;
        }

        case automated_run::stage::epoch_whole_pr:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.BaseOid, evidence.HeadTwoOid)
               && control->whole_pr_action()->isChecked())
            {
                control->previous_upload_action()->trigger();
                advance_fixture_stage(
                    *run,
                    automated_run::stage::epoch_previous_upload);
            }
            break;
        }

        case automated_run::stage::epoch_previous_upload:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.HeadOneOid, evidence.HeadTwoOid)
               && control->previous_upload_action()->isChecked())
            {
                control->last_reviewed_action()->trigger();
                advance_fixture_stage(
                    *run,
                    automated_run::stage::epoch_last_reviewed);
            }
            break;
        }

        case automated_run::stage::epoch_last_reviewed:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.HeadOneOid, evidence.HeadTwoOid)
               && control->last_reviewed_action()->isChecked())
            {
                run->EpochLeftRow = fixture_anchor_row(
                    *control->history_model(),
                    1U,
                    bpr::core::comparison_anchor_kind::epoch_head);
                run->EpochRightRow = fixture_anchor_row(
                    *control->history_model(),
                    2U,
                    bpr::core::comparison_anchor_kind::epoch_base);
                if(run->EpochLeftRow < 0 || run->EpochRightRow < 0)
                {
                    fail_automated_run(application, *run);
                    break;
                }
                control->left_selector()->setCurrentIndex(
                    run->EpochLeftRow);
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_arbitrary_left);
            }
            break;
        }

        case automated_run::stage::epoch_arbitrary_left:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.HeadOneOid, evidence.HeadTwoOid)
               && control->left_selector()->currentIndex()
                      == run->EpochLeftRow
               && !control->whole_pr_action()->isChecked()
               && !control->previous_upload_action()->isChecked()
               && !control->last_reviewed_action()->isChecked())
            {
                control->right_selector()->setCurrentIndex(
                    run->EpochRightRow);
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_arbitrary);
            }
            break;
        }

        case automated_run::stage::epoch_arbitrary:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.HeadOneOid, evidence.BaseOid)
               && control->left_selector()->currentIndex()
                      == run->EpochLeftRow
               && control->right_selector()->currentIndex()
                      == run->EpochRightRow
               && !control->whole_pr_action()->isChecked()
               && !control->previous_upload_action()->isChecked()
               && !control->last_reviewed_action()->isChecked())
            {
                std::swap(run->EpochLeftRow, run->EpochRightRow);
                control->left_selector()->setCurrentIndex(
                    run->EpochLeftRow);
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_reverse_left);
            }
            break;
        }

        case automated_run::stage::epoch_reverse_left:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.BaseOid, evidence.BaseOid)
               && control->left_selector()->currentIndex()
                      == run->EpochLeftRow
               && control->right_selector()->currentIndex()
                      == run->EpochLeftRow)
            {
                control->right_selector()->setCurrentIndex(
                    run->EpochRightRow);
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_reverse);
            }
            break;
        }

        case automated_run::stage::epoch_reverse:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.BaseOid, evidence.HeadOneOid)
               && control->left_selector()->currentIndex()
                      == run->EpochLeftRow
               && control->right_selector()->currentIndex()
                      == run->EpochRightRow)
            {
                const auto identical = fixture_anchor_row(
                    *control->history_model(),
                    2U,
                    bpr::core::comparison_anchor_kind::epoch_head);
                if(identical < 0)
                {
                    fail_automated_run(application, *run);
                    break;
                }
                run->EpochLeftRow = identical;
                run->EpochRightRow = identical;
                control->left_selector()->setCurrentIndex(identical);
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_identical_left);
            }
            break;
        }

        case automated_run::stage::epoch_identical_left:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.HeadTwoOid, evidence.HeadOneOid)
               && control->left_selector()->currentIndex()
                      == run->EpochLeftRow)
            {
                control->right_selector()->setCurrentIndex(
                    run->EpochRightRow);
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_identical);
            }
            break;
        }

        case automated_run::stage::epoch_identical:
        {
            const auto evidence = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            auto* const control = window.epoch_control();
            if(fixture_epoch_pair_passed(
                   window, evidence.HeadTwoOid, evidence.HeadTwoOid)
               && control->left_selector()->currentIndex()
                      == run->EpochLeftRow
               && control->right_selector()->currentIndex()
                      == run->EpochRightRow)
            {
                run->PresentationPassed = true;
                run->RenderSaved =
                    save_render_evidence(window, run->RenderPath);
                transition_to_shutdown(application, *run);
            }
            break;
        }

        case automated_run::stage::epoch_maintenance_history:
            if(window.epoch_control()->history_model()->epoch_count() >= 2U)
            {
                if(!begin_epoch_maintenance_probe(application, *run))
                {
                    fail_automated_run(application, *run);
                }
                else if(run->MaintenanceProbe
                        == automated_run::epoch_maintenance_probe::
                            verify_deleted
                        && run->Stage
                            == automated_run::stage::epoch_maintenance_history)
                {
                    run->PresentationPassed = true;
                    transition_to_shutdown(application, *run);
                }
            }
            break;

        case automated_run::stage::epoch_maintenance_select_right:
        {
            auto* const control = window.epoch_control();
            if(control->left_selector()->currentIndex()
               == run->MaintenanceLeftRow)
            {
                control->right_selector()->setCurrentIndex(
                    run->MaintenanceRightRow);
                advance_fixture_stage(
                    *run, automated_run::stage::epoch_maintenance_prune);
            }
            break;
        }

        case automated_run::stage::epoch_maintenance_prune:
        {
            auto* const control = window.epoch_control();
            if(control->left_selector()->currentIndex()
                       == run->MaintenanceLeftRow
               && control->right_selector()->currentIndex()
                       == run->MaintenanceRightRow
               && control->prune_action()->isEnabled())
            {
                accept_maintenance_confirmation(
                    QStringLiteral("pruneSelectedEpochsConfirmationDialog"));
                control->prune_action()->trigger();
                advance_fixture_stage(
                    *run,
                    automated_run::stage::epoch_maintenance_prune_result);
            }
            break;
        }

        case automated_run::stage::epoch_maintenance_prune_result:
        {
            auto* const control = window.epoch_control();
            const auto provider = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            const auto maintenance = bpr::app::detail::
                github_application_fixture_access::maintenance_evidence(
                    application, run->MaintenanceTarget);
            if(fixture_epoch_availability_is(*control->history_model(),
                   1U,
                   bpr::core::anchor_availability::pruned)
               && maintenance.PruneIntentDispatchCount == 1U
               && maintenance.DeleteIntentDispatchCount == 0U
               && maintenance.CatalogRevision >= 3
               && !maintenance.TargetBaseRefPresent
               && !maintenance.TargetHeadRefPresent
               && maintenance.NonTargetBaseRefPresent
               && maintenance.NonTargetHeadRefPresent
               && maintenance.RepositoryPresent
               && !maintenance.CacheDeletionMarkerPresent
               && provider.AccountReadCount == run->MaintenanceAccountReads
               && provider.SmartAcquireCount
                   == run->MaintenanceSmartAcquires
               && control->history_model()->statistics().ModelResets
                   == run->MaintenanceModelResets)
            {
                run->PresentationPassed = true;
                transition_to_shutdown(application, *run);
            }
            break;
        }

        case automated_run::stage::epoch_maintenance_delete_result:
        {
            auto* const control = window.epoch_control();
            const auto provider = bpr::app::detail::
                github_application_fixture_access::evidence(application);
            const auto maintenance = bpr::app::detail::
                github_application_fixture_access::maintenance_evidence(
                    application, run->MaintenanceTarget);
            if(fixture_all_epoch_availability_is(*control->history_model(),
                   bpr::core::anchor_availability::cache_deleted)
               && maintenance.PruneIntentDispatchCount == 0U
               && maintenance.DeleteIntentDispatchCount == 1U
               && !maintenance.TargetBaseRefPresent
               && !maintenance.TargetHeadRefPresent
               && !maintenance.NonTargetBaseRefPresent
               && !maintenance.NonTargetHeadRefPresent
               && !maintenance.RepositoryPresent
               && maintenance.CacheDeletionMarkerPresent
               && maintenance.TargetBaseCacheDeleted
               && maintenance.TargetHeadCacheDeleted
               && provider.AccountReadCount == run->MaintenanceAccountReads
               && provider.SmartAcquireCount
                   == run->MaintenanceSmartAcquires
               && control->history_model()->statistics().ModelResets
                   == run->MaintenanceModelResets)
            {
                run->PresentationPassed = true;
                transition_to_shutdown(application, *run);
            }
            break;
        }

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
        auto enterprise_hostname = config.EnterpriseHostname;

#if defined(BPR_BUILD_TESTING)
        auto fixture_name = std::string{};
        auto expected_active_host_text = std::string{};
        auto selected_fixture = std::optional<
            bpr::app::detail::github_public_path_fixture>{};
        auto fixture_options =
            bpr::app::detail::github_public_path_fixture_options{};
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
        const auto epoch_fixture_root =
            argument_value(argc, argv, "--epoch-fixture-root");
        const auto epoch_fixture_phase =
            argument_value(argc, argv, "--epoch-fixture-phase");
        const auto epoch_maintenance_probe =
            argument_value(argc, argv, "--epoch-maintenance-probe");
        if(epoch_fixture_root.has_value() != epoch_fixture_phase.has_value()
           || (epoch_fixture_root.has_value()
               && !selected_fixture.has_value()))
        {
            QTextStream{stdout}
                << "EPOCH_FIXTURE_CONFIGURATION_FAILED\n";
            return EXIT_FAILURE;
        }
        if(epoch_fixture_root)
        {
            fixture_options.ApplicationDataRoot =
                std::filesystem::path{std::string{*epoch_fixture_root}};
            if(*epoch_fixture_phase == "seed")
            {
                fixture_options.EpochPhase =
                    bpr::app::detail::github_epoch_fixture_phase::seed;
            }
            else if(*epoch_fixture_phase == "reuse")
            {
                fixture_options.EpochPhase =
                    bpr::app::detail::github_epoch_fixture_phase::reuse;
            }
            else
            {
                QTextStream{stdout}
                    << "EPOCH_FIXTURE_CONFIGURATION_FAILED\n";
                return EXIT_FAILURE;
            }
        }
        auto selected_maintenance_probe =
            automated_run::epoch_maintenance_probe::none;
        if(epoch_maintenance_probe)
        {
            if(!epoch_fixture_root
               || fixture_options.EpochPhase
                   != bpr::app::detail::github_epoch_fixture_phase::reuse)
            {
                QTextStream{stdout}
                    << "EPOCH_MAINTENANCE_CONFIGURATION_FAILED\n";
                return EXIT_FAILURE;
            }
            if(*epoch_maintenance_probe == "prune")
            {
                selected_maintenance_probe =
                    automated_run::epoch_maintenance_probe::prune;
            }
            else if(*epoch_maintenance_probe == "verify-pruned-delete")
            {
                selected_maintenance_probe = automated_run::
                    epoch_maintenance_probe::verify_pruned_and_delete;
            }
            else if(*epoch_maintenance_probe == "verify-deleted")
            {
                selected_maintenance_probe =
                    automated_run::epoch_maintenance_probe::verify_deleted;
            }
            else
            {
                QTextStream{stdout}
                    << "EPOCH_MAINTENANCE_CONFIGURATION_FAILED\n";
                return EXIT_FAILURE;
            }
        }
#endif

        const auto self_test = has_argument(argc, argv, "--self-test");
        const auto render_path =
            argument_value(argc, argv, "--render-evidence");
#if defined(BPR_BUILD_TESTING)
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
        if(native_appearance_probe
           && fixture_options.EpochPhase
                  != bpr::app::detail::github_epoch_fixture_phase::single)
        {
            QTextStream{stdout}
                << "EPOCH_FIXTURE_CONFIGURATION_FAILED\n";
            return EXIT_FAILURE;
        }
#endif

        auto application =
#if defined(BPR_BUILD_TESTING)
            selected_fixture.has_value()
            ? bpr::app::detail::github_application_fixture_access::make(
                  *selected_fixture, fixture_options)
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
#if defined(BPR_BUILD_TESTING)
        verify_native_platform_vault =
            verify_native_platform_vault && fixture_name.empty();
        verify_cli_unavailable =
            verify_cli_unavailable && fixture_name.empty();
#endif
        if(self_test || render_path.has_value()
#if defined(BPR_BUILD_TESTING)
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
                        .Cli =
                            automated_run::cli_unavailable_probe_state{
                                .Enabled = verify_cli_unavailable,
                                .EnterpriseHostname =
                                    std::move(enterprise_hostname),
                            },
#if defined(BPR_BUILD_TESTING)
                        .Fixture = !fixture_name.empty(),
                        .FixtureName = std::move(fixture_name),
                        .ExpectedActiveHostText =
                            std::move(expected_active_host_text),
                        .EpochPhase = fixture_options.EpochPhase,
                        .MaintenanceProbe = selected_maintenance_probe,
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
