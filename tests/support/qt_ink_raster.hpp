#ifndef BPR_TESTS_SUPPORT_QT_INK_RASTER_HPP
#define BPR_TESTS_SUPPORT_QT_INK_RASTER_HPP

#include <QImage>
#include <QRectF>
#include <QWidget>
#include <QtMath>

#include <algorithm>

namespace bpr::tests {
	/// \brief Paints the actual widget with enough device pixels to resolve literal ink.
	/// \details A nine-point glyph at DPR1 can contain only blended LCD pixels.
	/// This changes the assessment target, never the widget's font, layout or DPR.
	[[nodiscard]] inline auto ink_raster(QWidget& aWidget) -> QImage
	{
		const auto scale = std::max(2.0, aWidget.devicePixelRatioF());
		auto       image = QImage{ QSize{ qCeil(aWidget.width() * scale), qCeil(aWidget.height() * scale) }, QImage::Format_ARGB32_Premultiplied };
		image.setDevicePixelRatio(scale);
		image.fill(Qt::transparent);
		aWidget.render(&image);
		return image;
	}

	/// \brief Crops the assessment raster using the widget's logical coordinates.
	[[nodiscard]] inline auto ink_raster(QWidget& aWidget, const QRect& aArea) -> QImage
	{
		const auto image   = ink_raster(aWidget);
		const auto scale   = image.devicePixelRatio();
		const auto pixels  = QRectF{ aArea.x() * scale, aArea.y() * scale, aArea.width() * scale, aArea.height() * scale }.toAlignedRect().intersected(image.rect());
		auto       cropped = image.copy(pixels);
		cropped.setDevicePixelRatio(scale);
		return cropped;
	}
}

#endif
