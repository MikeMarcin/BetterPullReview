// A render made with BPR_NATIVE_LOOK is the owner's look: Cocoa, QMacStyle,
// the asked appearance, and no window on screen (native_look_startup.cpp).
#include <QApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>
#include <QWindow>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	const QApplication application{ argc, argv };
	expect(QGuiApplication::platformName() == QStringLiteral("cocoa"), "the native look runs on Cocoa");
	expect(QString::fromLatin1(QApplication::style()->metaObject()->className()) == QStringLiteral("QMacStyle"), "with the native style");
	const auto dark = qEnvironmentVariable("BPR_NATIVE_LOOK_SCHEME") == QStringLiteral("dark");
	expect(QGuiApplication::styleHints()->colorScheme() == (dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light), "in the asked appearance");

	QWidget     window;
	auto* const layout = new QHBoxLayout{ &window };
	auto* const cancel = new QPushButton{ QStringLiteral("Cancel") };
	auto* const accept = new QPushButton{ QStringLiteral("Accept") };
	accept->setDefault(true);
	layout->addWidget(cancel);
	layout->addWidget(accept);
	window.show();
	QApplication::processEvents();
	expect(window.isVisible() && window.testAttribute(Qt::WA_DontShowOnScreen), "the window is shown to Qt and kept off screen");
	expect(window.windowHandle() == nullptr || !window.windowHandle()->isVisible(), "no platform window is visible");
	const auto image = window.grab().toImage();
	expect(!image.isNull() && image.width() > 0, "the window still paints for a render");
	const auto ground = image.pixelColor(2, 2).lightness();
	expect(dark ? ground < 128 : ground > 128, "its ground follows the appearance");
	std::cout << "native-look:pass\n";
	return EXIT_SUCCESS;
}
