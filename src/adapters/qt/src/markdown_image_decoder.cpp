#include "bpr/qt/markdown_image_decoder.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColorSpace>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSvgRenderer>
#include <QtSvg/qtsvgglobal.h>

#include <cstring>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The one reader a payload may reach, by its leading bytes.
		auto format_of(const std::string_view aBytes) -> QByteArray
		{
			if (aBytes.starts_with(std::string_view{ "\x89PNG\r\n\x1a\n", 8 })) {
				return QByteArrayLiteral("png");
			}
			if (aBytes.starts_with(std::string_view{ "\xFF\xD8\xFF", 3 })) {
				return QByteArrayLiteral("jpeg");
			}
			if (aBytes.starts_with("GIF87a") || aBytes.starts_with("GIF89a")) {
				return QByteArrayLiteral("gif");
			}
			// RFC-BPR-0084 Q2: RIFF, a little-endian length, then WEBP.
			if (aBytes.size() >= 12U && aBytes.starts_with("RIFF") && aBytes.substr(8U, 4U) == "WEBP") {
				return QByteArrayLiteral("webp");
			}
			return { };
		}

		/// Browsers show a delay of 10 ms or less as 100 ms.
		auto frame_delay(const int aDelay) -> std::uint32_t
		{
			return aDelay <= 10 ? 100U : static_cast<std::uint32_t>(aDelay);
		}

		auto frame_of(QImage aImage, const std::uint32_t aDelay) -> application::markdown_image_frame
		{
			if (aImage.colorSpace().isValid() && aImage.colorSpace() != QColorSpace{ QColorSpace::SRgb }) {
				aImage.convertToColorSpace(QColorSpace{ QColorSpace::SRgb });
			}
			aImage           = std::move(aImage).convertToFormat(QImage::Format_ARGB32_Premultiplied);
			auto       frame = application::markdown_image_frame{ };
			const auto row   = static_cast<std::size_t>(aImage.width()) * 4U;
			frame.Pixels.resize(row * static_cast<std::size_t>(aImage.height()));
			for (int line = 0; line < aImage.height(); ++line) {
				std::memcpy(frame.Pixels.data() + row * static_cast<std::size_t>(line), aImage.constScanLine(line), row);
			}
			frame.DelayMs = aDelay;
			return frame;
		}

		/// RFC-BPR-0084 Q1. An admitted SVG, rendered once, statically, at twice
		/// its natural size so it stays sharp on high-density screens.
		auto rasterize_svg(const std::string_view aBytes) -> std::expected<application::markdown_decoded_image, core::markdown_image_state>
		{
			if (const auto admitted = core::admit_markdown_svg(aBytes); !admitted) {
				return std::unexpected{ admitted.error() };
			}
			QSvgRenderer renderer;
			// Untrusted-source parsing (no AssumeTrustedSource) and no animation.
			renderer.setOptions(QtSvg::DisableAnimations);
			if (!renderer.load(QByteArray{ aBytes.data(), static_cast<qsizetype>(aBytes.size()) }) || !renderer.isValid()) {
				return std::unexpected{ core::markdown_image_state::failed };
			}
			auto natural = renderer.defaultSize();
			if (natural.isEmpty()) {
				// A browser's size for an image with neither size nor viewBox.
				natural = QSize{ 300, 150 };
			}
			const auto size = core::markdown_image_size{ static_cast<std::uint32_t>(natural.width()), static_cast<std::uint32_t>(natural.height()) };
			if (!core::markdown_image_dimensions_admitted(size)) {
				return std::unexpected{ core::markdown_image_state::too_large };
			}
			const auto scale  = core::markdown_image_dimensions_admitted({ size.Width * 2U, size.Height * 2U }) ? 2U : 1U;
			auto       pixels = QImage{ natural * static_cast<int>(scale), QImage::Format_ARGB32_Premultiplied };
			if (pixels.isNull()) {
				return std::unexpected{ core::markdown_image_state::too_large };
			}
			pixels.fill(Qt::transparent);
			{
				QPainter painter{ &pixels };
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setRenderHint(QPainter::SmoothPixmapTransform);
				renderer.render(&painter, QRectF{ QPointF{ }, QSizeF{ pixels.size() } });
			}
			auto image   = application::markdown_decoded_image{ };
			image.Width  = size.Width;
			image.Height = size.Height;
			image.Scale  = scale;
			image.Frames.push_back(frame_of(std::move(pixels), 0U));
			return image;
		}
	} // namespace

	auto markdown_image_decoder::decode(const std::string_view aBytes) const
		-> std::expected<application::markdown_decoded_image, core::markdown_image_state>
	{
		if (core::markdown_svg_signature(aBytes)) {
			return rasterize_svg(aBytes);
		}
		const auto format = format_of(aBytes);
		if (format.isEmpty()) {
			return std::unexpected{ core::markdown_image_state::unsupported };
		}
		auto bytes  = QByteArray::fromRawData(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		auto buffer = QBuffer{ &bytes };
		if (!buffer.open(QIODevice::ReadOnly)) {
			return std::unexpected{ core::markdown_image_state::failed };
		}
		auto reader = QImageReader{ &buffer, format };
		reader.setAutoDetectImageFormat(false);
		reader.setDecideFormatFromContent(false);
		reader.setAutoTransform(true);
		if (!reader.canRead()) {
			return std::unexpected{ core::markdown_image_state::failed };
		}
		const auto declared = reader.size();
		if (!declared.isValid()
			|| !core::markdown_image_dimensions_admitted({ static_cast<std::uint32_t>(declared.width()), static_cast<std::uint32_t>(declared.height()) })) {
			return std::unexpected{ declared.isValid() ? core::markdown_image_state::too_large : core::markdown_image_state::failed };
		}
		auto image    = application::markdown_decoded_image{ };
		auto animated = reader.supportsAnimation() && reader.imageCount() != 1;
		auto total    = std::size_t{ 0 };
		for (;;) {
			auto frame = reader.read();
			if (frame.isNull()) {
				break;
			}
			// How long this frame shows, known once it has been read.
			const auto delay = reader.nextImageDelay();
			const auto size  = core::markdown_image_size{ static_cast<std::uint32_t>(frame.width()), static_cast<std::uint32_t>(frame.height()) };
			if (!core::markdown_image_dimensions_admitted(size)) {
				return std::unexpected{ core::markdown_image_state::too_large };
			}
			if (image.Frames.empty()) {
				image.Width  = size.Width;
				image.Height = size.Height;
			} else if (size.Width != image.Width || size.Height != image.Height) {
				break;
			}
			total += static_cast<std::size_t>(size.Width) * size.Height * 4U;
			if (!core::markdown_image_animation_admitted(image.Frames.size() + 1U, total)) {
				// Over the animation bounds: the first frame, still.
				image.Frames.resize(1U);
				break;
			}
			image.Frames.push_back(frame_of(std::move(frame), animated ? frame_delay(delay) : 0U));
			if (!animated) {
				break;
			}
		}
		if (image.Frames.empty()) {
			return std::unexpected{ core::markdown_image_state::failed };
		}
		if (image.Frames.size() == 1U) {
			image.Frames.front().DelayMs = 0U;
		}
		return image;
	}
} // namespace bpr::qt
