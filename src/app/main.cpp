#include "bpr/app/foundation_application.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QPixmap>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <optional>
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
        [expected](const char* const argument) {
            return std::string_view{argument} == expected;
        });
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

auto public_path_passed(
    const bpr::qt::foundation_window& window) -> bool
{
    return window.refresh_action()->isEnabled()
        && window.status_text() == "Foundation state is current"
        && window.statistics().CommandUpdates == 2U
        && window.statistics().StatusUpdates == 2U
        && window.statistics().FullResets == 0U;
}

auto save_render_evidence(
    bpr::qt::foundation_window& window,
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

void finish_automated_run(
    bpr::app::foundation_application& application,
    const std::optional<std::string_view>& render_path)
{
    const auto passed = public_path_passed(application.window());
    const auto evidence_saved =
        save_render_evidence(application.window(), render_path);

    QTextStream stream{stdout};
    stream << (passed ? "PUBLIC_PATH_OK\n" : "PUBLIC_PATH_FAILED\n");
    if(render_path.has_value())
    {
        stream << (evidence_saved ? "RENDER_OK\n" : "RENDER_FAILED\n");
    }

    QCoreApplication::exit(
        passed && evidence_saved ? EXIT_SUCCESS : EXIT_FAILURE);
}
}

auto main(int argc, char** argv) -> int
{
    QApplication qt_application{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
    QCoreApplication::setApplicationVersion(QStringLiteral(BPR_VERSION));
    QCoreApplication::setOrganizationName(
        QStringLiteral("BetterPullReview"));

    bpr::app::foundation_application application{
        bpr::qt::foundation_view_config{
            .Version = BPR_VERSION,
            .Platform = QSysInfo::prettyProductName().toStdString(),
            .Configuration = BPR_BUILD_CONFIGURATION,
        }};
    application.show();

    const auto self_test = has_argument(argc, argv, "--self-test");
    const auto render_path =
        argument_value(argc, argv, "--render-evidence");
    if(self_test || render_path.has_value())
    {
        QTimer::singleShot(
            0,
            &application.window(),
            [&application, render_path] {
                application.window().refresh_action()->trigger();
                QTimer::singleShot(
                    25,
                    &application.window(),
                    [&application, render_path] {
                        finish_automated_run(application, render_path);
                    });
            });
    }

    return qt_application.exec();
}
