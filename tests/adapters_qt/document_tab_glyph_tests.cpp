// Owner, look report-keeps-v001 (2026-11-16): "the report glyph is
// unreadable on dark theme". Every glyph a document tab wears (a report's
// page; a file's tab wears none) is drawn in the tabs' ink, light and dark,
// and drawn again when the theme changes under an open tab.
#include "../support/test_expectation.hpp"
#include "bpr/qt/document_tab_strip.hpp"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPalette>
#include <QTabBar>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	using bpr::tests::expect;

	[[nodiscard]] auto palette_of(const QColor& aGround, const QColor& aInk) -> QPalette
	{
		auto palette = QPalette{ aGround };
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			palette.setColor(group, QPalette::Window, aGround);
			palette.setColor(group, QPalette::WindowText, aInk);
			palette.setColor(group, QPalette::Text, aInk);
			palette.setColor(group, QPalette::ButtonText, aInk);
		}
		return palette;
	}

	/// The glyph's ink: the mean lightness of its drawn pixels.
	[[nodiscard]] auto ink_lightness(const QIcon& aIcon) -> double
	{
		const auto image = aIcon.pixmap(QSize{ 16, 16 }, 2.0).toImage().convertToFormat(QImage::Format_ARGB32);
		auto       sum   = 0.0;
		auto       drawn = 0;
		for (auto y = 0; y < image.height(); ++y) {
			for (auto x = 0; x < image.width(); ++x) {
				const auto pixel = QColor::fromRgba(image.pixel(x, y));
				if (pixel.alpha() > 128) {
					sum += pixel.lightnessF();
					++drawn;
				}
			}
		}
		return drawn == 0 ? -1.0 : sum / drawn;
	}
} // namespace

int main(int aArgc, char** aArgv)
try {
	QApplication application{ aArgc, aArgv };
	const auto   light = palette_of(QColor{ 0xf6, 0xf6, 0xf6 }, QColor{ 0x1d, 0x1d, 0x1f });
	const auto   dark  = palette_of(QColor{ 0x2b, 0x2b, 0x2b }, QColor{ 0xf2, 0xf2, 0xf2 });
	QApplication::setPalette(light);
	auto strip = bpr::qt::document_tab_strip{ };
	strip.resize(900, bpr::qt::document_tab_strip::height_pixels);
	strip.present({ { .Id = bpr::core::document_tab_id{ 1U }, .Name = QStringLiteral("renderer.cpp"), .Path = QStringLiteral("src/engine/renderer.cpp") },
					{ .Id = bpr::core::document_tab_id{ 2U }, .Name = QStringLiteral("Review of #21"), .Active = true, .Report = true } });
	strip.show();
	QApplication::processEvents();
	auto* const bar = strip.tab_bar();
	expect(bar->count() == 2, "a file's tab and a report's");
	expect(bar->tabIcon(0).isNull(), "a file's tab wears no glyph");
	const auto onLight = ink_lightness(bar->tabIcon(1));
	expect(onLight >= 0.0 && onLight < 0.4, "on a light strip the report's page is dark: " + std::to_string(onLight));
	// The theme changes under the open tab.
	QApplication::setPalette(dark);
	QApplication::processEvents();
	const auto onDark = ink_lightness(bar->tabIcon(1));
	expect(onDark > 0.6, "on a dark strip the report's page is drawn light again: " + std::to_string(onDark));
	// A strip opened dark draws it light from the start.
	auto opened = bpr::qt::document_tab_strip{ };
	opened.present({ { .Id = bpr::core::document_tab_id{ 3U }, .Name = QStringLiteral("Review of #22"), .Active = true, .Report = true } });
	QApplication::processEvents();
	expect(ink_lightness(opened.tab_bar()->tabIcon(0)) > 0.6, "opened on a dark strip, it is light");
	QApplication::setPalette(light);
	QApplication::processEvents();
	expect(ink_lightness(bar->tabIcon(1)) < 0.4 && ink_lightness(opened.tab_bar()->tabIcon(0)) < 0.4, "and back to dark ink on light");
	std::cout << "DOCUMENT_TAB_GLYPH_OK\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
