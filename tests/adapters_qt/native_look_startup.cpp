// The owner, rejecting the tour stop editing look on 2026-11-10: "There's
// some kind of theme disparity going on between what I see and your
// offscreen renders." The renders came from the offscreen platform, which has
// no native style (Fusion), no system appearance (always Light) and its own
// font database; the owner's window is Cocoa with QMacStyle in Dark. A render
// made there can only ever be layout evidence.
//
// Linked into every Qt test executable on macOS. With BPR_NATIVE_LOOK=1 and
// QT_QPA_PLATFORM=cocoa (and QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1,
// so the process never becomes a foreground application), every window the
// test shows is laid out and painted by the native platform, style, palette
// and fonts, but marked Qt::WA_DontShowOnScreen before it is shown: nothing
// reaches the screen and nothing is captured from it. A widget's own grab()
// is then what the owner's window paints. BPR_NATIVE_LOOK_SCHEME=dark or
// light fixes the appearance for this process only (the default follows the
// system, as the application does). Without BPR_NATIVE_LOOK nothing changes.
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QStyleHints>
#include <QWidget>

#include <cstdio>
#include <cstdlib>

namespace {
	class hidden_native_windows final : public QObject
	{
	public:
		using QObject::QObject;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::Show || aEvent->type() == QEvent::Polish) {
				if (auto* const widget = qobject_cast<QWidget*>(aWatched); widget != nullptr && widget->isWindow()) {
					widget->setAttribute(Qt::WA_DontShowOnScreen);
				}
			}
			return false;
		}
	};

	void hide_native_windows()
	{
		if (qEnvironmentVariable("BPR_NATIVE_LOOK") != QStringLiteral("1")) {
			return;
		}
		if (QGuiApplication::platformName() != QStringLiteral("cocoa") || qobject_cast<QApplication*>(QCoreApplication::instance()) == nullptr) {
			std::fputs("NATIVE_LOOK_REFUSED: BPR_NATIVE_LOOK needs QT_QPA_PLATFORM=cocoa and a QApplication\n", stderr);
			std::exit(EXIT_FAILURE);
		}
		QCoreApplication::instance()->installEventFilter(new hidden_native_windows{ QCoreApplication::instance() });
		const auto scheme = qEnvironmentVariable("BPR_NATIVE_LOOK_SCHEME");
		if (scheme == QStringLiteral("dark")) {
			QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
		} else if (scheme == QStringLiteral("light")) {
			QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
		}
		std::fputs("NATIVE_LOOK: cocoa, windows kept off screen\n", stderr);
	}
} // namespace

Q_COREAPP_STARTUP_FUNCTION(hide_native_windows)
