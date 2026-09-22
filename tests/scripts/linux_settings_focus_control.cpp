#include <QApplication>
#include <QWidget>

// Native control for the Settings observer: two ordinary Qt windows, with no
// BetterPullReview composition, profile, setting, provider or storage code.
auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	QWidget      first;
	first.setWindowTitle(QStringLiteral("BPR0018 native focus control A"));
	first.resize(900, 600);
	first.show();
	QWidget second;
	second.setWindowTitle(QStringLiteral("BPR0018 native focus control B"));
	second.resize(760, 480);
	second.show();
	second.raise();
	second.activateWindow();
	return application.exec();
}
