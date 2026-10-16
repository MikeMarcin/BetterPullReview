#include "bpr/qt/markdown_image_decoder.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColorSpace>
#include <QImage>
#include <QImageReader>

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
	} // namespace

	auto markdown_image_decoder::decode(const std::string_view aBytes) const
		-> std::expected<application::markdown_decoded_image, core::markdown_image_state>
	{
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
