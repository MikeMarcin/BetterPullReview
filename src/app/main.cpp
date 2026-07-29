#include "bpr/app/foundation_application.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
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

    if(has_argument(argc, argv, "--self-test"))
    {
        QTimer::singleShot(0, &application.window(), [&application] {
            application.window().refresh_action()->trigger();
            QTimer::singleShot(10, &application.window(), [&application] {
                const auto passed =
                    application.window().refresh_action()->isEnabled()
                    && application.window().status_text()
                        == "Foundation state is current"
                    && application.window().statistics().CommandUpdates == 2U
                    && application.window().statistics().StatusUpdates == 2U
                    && application.window().statistics().FullResets == 0U;

                QTextStream{stdout}
                    << (passed ? "PUBLIC_PATH_OK\n"
                               : "PUBLIC_PATH_FAILED\n");
                QCoreApplication::exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
            });
        });
    }

    return qt_application.exec();
}
