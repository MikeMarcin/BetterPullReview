#include "bpr/qt/menu_bar_status_item.hpp"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QSystemTrayIcon>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

// [[BPR-1250]]. The menu bar status item is Dif as a macOS template image: a mask
// icon whose pixels are black with alpha, at 18 points in 1x and 2x, so the menu
// bar draws it in its own colour. The application's colour icon stays colour in
// the menu bar, which is what the owner rejected.
namespace {
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	struct template_reading
	{
		bool Template      = false;
		int  Transparent   = 0;
		int  Solid         = 0;
		int  ColouredPixel = -1;
	};

	/// Whether every pixel is black with some alpha: the only thing a template
	/// image may carry, because AppKit reads its alpha and nothing else.
	[[nodiscard]] auto read_template(const QImage& aImage) -> template_reading
	{
		const auto image   = aImage.convertToFormat(QImage::Format_ARGB32);
		auto       reading = template_reading{ };
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				const auto pixel = image.pixel(x, y);
				if (qAlpha(pixel) != 0 && (qRed(pixel) != 0 || qGreen(pixel) != 0 || qBlue(pixel) != 0)) {
					reading.ColouredPixel = y * image.width() + x;
					return reading;
				}
				reading.Transparent += qAlpha(pixel) == 0 ? 1 : 0;
				reading.Solid += qAlpha(pixel) == 255 ? 1 : 0;
			}
		}
		const auto area  = image.width() * image.height();
		reading.Template = reading.Transparent > 0 && reading.Solid >= area / 10;
		return reading;
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };

	const auto icon = bpr::qt::menu_bar_status_icon();
	expect(!icon.isNull(), "the status item's icon is in this macOS graph's resources");
	expect(icon.isMask(), "and it is a mask, which the cocoa platform makes an NSImage template");

	const auto single = icon.pixmap(QSize{ 18, 18 }, 1.0);
	expect(single.size() == QSize(18, 18) && single.devicePixelRatio() == 1.0, "18 points at 1x is 18 pixels");
	const auto doubled = icon.pixmap(QSize{ 18, 18 }, 2.0);
	expect(doubled.size() == QSize(36, 36) && doubled.devicePixelRatio() == 2.0,
		   "18 points at 2x is the 36-pixel @2x image, not a scaled 1x");

	for (const auto& [name, pixmap] : { std::pair{ "1x", single }, std::pair{ "2x", doubled } }) {
		const auto reading = read_template(pixmap.toImage());
		expect(reading.ColouredPixel < 0,
			   (std::string{ name } + " is black with alpha: no colour for the menu bar to keep").c_str());
		expect(reading.Template, (std::string{ name } + " has a glyph and a background").c_str());
	}

	// Negative controls: the reading refuses what a template is not.
	auto colour = QImage{ 18, 18, QImage::Format_ARGB32 };
	colour.fill(Qt::transparent);
	for (int x = 0; x < 18; ++x) {
		for (int y = 4; y < 14; ++y) {
			colour.setPixelColor(x, y, QColor{ 120, 80, 200 });
		}
	}
	expect(read_template(colour).ColouredPixel >= 0, "control: a coloured glyph is refused");
	auto blank = QImage{ 18, 18, QImage::Format_ARGB32 };
	blank.fill(Qt::transparent);
	expect(!read_template(blank).Template, "control: an empty image is refused");
	auto plain = QIcon{ QPixmap::fromImage(colour) };
	expect(!plain.isMask(), "control: an icon is not a template unless marked");

	// The item itself: there wherever the platform offers a tray, and nowhere else.
	const auto item = bpr::qt::make_menu_bar_status_item();
	if (QSystemTrayIcon::isSystemTrayAvailable()) {
		expect(item != nullptr && item->isVisible(), "the status item is created and shown");
		expect(!item->icon().isNull() && item->icon().isMask(), "and it shows the template image");
		std::cout << "menu-bar-status-item: shown on " << QApplication::platformName().toStdString() << '\n';
	} else {
		expect(item == nullptr, "a platform with no tray gets no status item");
		std::cout << "menu-bar-status-item: no tray on " << QApplication::platformName().toStdString() << '\n';
	}

	std::cout << "menu-bar-status-item-tests:pass\n";
	return EXIT_SUCCESS;
}
