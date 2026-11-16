// [[BPR-2200]]. Linked into every Qt test executable of this directory on
// Windows; macOS and Linux build nothing from it.
//
// The Qt courses run on Qt's offscreen platform, which on Windows finds no
// fonts unless QT_QPA_FONTDIR names them: every font then resolves to an
// unnamed fallback about twice as wide as the system font, so rows yield at
// widths no desktop sees and anything that depends on font metrics is tested
// against no real font ([[BPR-2197]] measured the pull request's name at 1388
// pixels against 624). Before main, QT_QPA_FONTDIR names %WINDIR%/Fonts unless
// it is already set; once the application exists on the offscreen platform,
// its font is Segoe UI 9 pt, the font a Windows desktop gives. On another
// platform (windows itself, which has its own fonts) the font is left alone.
#if defined(_WIN32)
	#include <QCoreApplication>
	#include <QDir>
	#include <QFont>
	#include <QGuiApplication>
	#include <QString>
	#include <QtGlobal>

namespace {
	/// Before main: the platform reads the folder when the application starts.
	[[nodiscard]] auto name_windows_fonts_folder() -> bool
	{
		if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) {
			const auto windows = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows"));
			qputenv("QT_QPA_FONTDIR", QDir{ windows }.filePath(QStringLiteral("Fonts")).toLocal8Bit());
		}
		return true;
	}
	[[maybe_unused]] const auto windows_fonts_folder_named = name_windows_fonts_folder();

	void use_windows_desktop_font()
	{
		if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr || QGuiApplication::platformName() != QStringLiteral("offscreen")) {
			return;
		}
		QGuiApplication::setFont(QFont{ QStringLiteral("Segoe UI"), 9 });
	}
} // namespace

Q_COREAPP_STARTUP_FUNCTION(use_windows_desktop_font)
#endif
