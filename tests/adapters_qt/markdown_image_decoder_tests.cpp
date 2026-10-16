// RFC-BPR-0084. The body-image decoder: signature-chosen PNG, JPEG, GIF and
// WebP, size bounds before allocation, animation frames and delays; and, by
// owner amendment 1, admitted SVG rasterized statically at twice its size.
#include "bpr/qt/markdown_image_decoder.hpp"

#include <QBuffer>
#include <QGuiApplication>
#include <QImage>
#include <QImageWriter>

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto encoded(const QImage& aImage, const char* aFormat) -> std::string
	{
		QByteArray bytes;
		QBuffer    buffer{ &bytes };
		expect(buffer.open(QIODevice::WriteOnly), "buffer");
		QImageWriter writer{ &buffer, aFormat };
		expect(writer.write(aImage), std::string{ "write " } + aFormat);
		return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
	}

	auto solid(const int aWidth, const int aHeight, const QColor& aColour) -> QImage
	{
		auto image = QImage{ aWidth, aHeight, QImage::Format_ARGB32 };
		image.fill(aColour);
		return image;
	}

	auto pixel(const application::markdown_decoded_image& aImage, const std::size_t aFrame) -> std::uint32_t
	{
		const auto&   bytes = aImage.Frames[aFrame].Pixels;
		std::uint32_t value = 0U;
		std::memcpy(&value, bytes.data(), sizeof value);
		return value;
	}

	// A 1x1 GIF89a with two frames, black then white, delays 20 ms and none,
	// looping forever. Hand-assembled; Qt has no GIF writer.
	constexpr std::array<unsigned char, 70> animated_gif{
		'G',
		'I',
		'F',
		'8',
		'9',
		'a',
		0x01,
		0x00,
		0x01,
		0x00,
		0x80,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0xFF,
		0xFF,
		0xFF,
		0x21,
		0xFF,
		0x0B,
		'N',
		'E',
		'T',
		'S',
		'C',
		'A',
		'P',
		'E',
		'2',
		'.',
		'0',
		0x03,
		0x01,
		0x00,
		0x00,
		0x00,
		0x21,
		0xF9,
		0x04,
		0x00,
		0x02,
		0x00,
		0x00,
		0x00,
		0x2C,
		0x00,
		0x00,
		0x00,
		0x00,
		0x01,
		0x00,
		0x01,
		0x00,
		0x00,
		0x02,
		0x02,
		0x44,
		0x01,
		0x00,
		0x21,
		0xF9,
		0x04,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x2C,
	};
	constexpr std::array<unsigned char, 15> second_frame{ 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x4C, 0x01, 0x00, 0x3B };

	auto gif_bytes() -> std::string
	{
		auto bytes = std::string{ reinterpret_cast<const char*>(animated_gif.data()), animated_gif.size() };
		// The second frame's graphic control extension has no delay, shown as 100 ms.
		bytes.append(reinterpret_cast<const char*>(second_frame.data()), second_frame.size());
		return bytes;
	}

	// RFC-BPR-0084 Q2. An 8x6 lossless animated WebP: opaque red for 120 ms,
	// then half-transparent blue for 30 ms (img2webp -loop 0 -lossless).
	constexpr std::array<unsigned char, 148> animated_webp{
		0x52, 0x49, 0x46, 0x46, 0x8C, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x58,
		0x0A, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x05, 0x00, 0x00, 0x41, 0x4E,
		0x49, 0x4D, 0x06, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x41, 0x4E, 0x4D, 0x46,
		0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x05, 0x00, 0x00,
		0x78, 0x00, 0x00, 0x02, 0x56, 0x50, 0x38, 0x4C, 0x14, 0x00, 0x00, 0x00, 0x2F, 0x07, 0x40, 0x01,
		0x00, 0x07, 0x10, 0xFD, 0x8F, 0xFE, 0x07, 0x00, 0x82, 0xF0, 0xBF, 0x6D, 0x22, 0xA2, 0xFF, 0x21,
		0x41, 0x4E, 0x4D, 0x46, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
		0x00, 0x05, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x02, 0x56, 0x50, 0x38, 0x4C, 0x14, 0x00, 0x00, 0x00,
		0x2F, 0x07, 0x40, 0x01, 0x10, 0x07, 0x10, 0xD1, 0xFF, 0x02, 0x06, 0x00, 0x82, 0xF0, 0xBF, 0x6D,
		0x22, 0xA2, 0xFF, 0x21
	};

	// The same red 8x6 frame as a lossless still (cwebp -lossless).
	constexpr std::array<unsigned char, 40> still_webp{
		0x52, 0x49, 0x46, 0x46, 0x20, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x4C,
		0x14, 0x00, 0x00, 0x00, 0x2F, 0x07, 0x40, 0x01, 0x00, 0x07, 0x10, 0xFD, 0x8F, 0xFE, 0x07, 0x00,
		0x82, 0xF0, 0xBF, 0x6D, 0x22, 0xA2, 0xFF, 0x21
	};

	auto webp_bytes() -> std::string
	{
		return { reinterpret_cast<const char*>(animated_webp.data()), animated_webp.size() };
	}

	auto svg(const std::string_view aBody, const std::string_view aSize = R"svg(width="20" height="10")svg") -> std::string
	{
		return std::string{ R"svg(<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" )svg" } + std::string{ aSize } + ">" + std::string{ aBody } + "</svg>";
	}

	auto pixel_at(const application::markdown_decoded_image& aImage, const std::uint32_t aX, const std::uint32_t aY) -> std::uint32_t
	{
		const auto    row   = static_cast<std::size_t>(aImage.Width * aImage.Scale) * 4U;
		std::uint32_t value = 0U;
		std::memcpy(&value, aImage.Frames[0].Pixels.data() + row * aY + static_cast<std::size_t>(aX) * 4U, sizeof value);
		return value;
	}
} // namespace

int main(int argument_count, char** arguments)
{
	QGuiApplication application{ argument_count, arguments };
	const auto      decoder = qt::markdown_image_decoder{ };

	const auto png = decoder.decode(encoded(solid(3, 2, Qt::red), "png"));
	expect(png && png->Width == 3U && png->Height == 2U && png->Frames.size() == 1U, "PNG decodes to one frame");
	expect(png->Frames[0].Pixels.size() == 3U * 2U * 4U && pixel(*png, 0) == 0xFFFF0000U && png->Frames[0].DelayMs == 0U, "premultiplied ARGB, rows packed, still");

	const auto jpeg = decoder.decode(encoded(solid(16, 8, Qt::blue), "jpeg"));
	expect(jpeg && jpeg->Width == 16U && jpeg->Height == 8U && jpeg->Frames.size() == 1U, "JPEG decodes");

	const auto gif = decoder.decode(gif_bytes());
	expect(gif.has_value(), "animated GIF decodes");
	expect(gif->Frames.size() == 2U, "both GIF frames kept");
	expect(gif->Frames[0].DelayMs == 20U && gif->Frames[1].DelayMs == 100U, "frame delays as a browser shows them");
	expect(pixel(*gif, 0) == 0xFF000000U && pixel(*gif, 1) == 0xFFFFFFFFU, "frames in order");

	const auto wide = decoder.decode(encoded(solid(20'000, 1, Qt::green), "png"));
	expect(!wide && wide.error() == core::markdown_image_state::too_large, "a side over the bound is too large before decoding");

	auto mislabelled = encoded(solid(4, 4, Qt::blue), "jpeg");
	mislabelled.replace(0, 8, std::string{ "\x89PNG\r\n\x1a\n", 8 });
	const auto refused = decoder.decode(mislabelled);
	expect(!refused && refused.error() == core::markdown_image_state::failed, "a PNG signature on other bytes reaches only the PNG reader and fails");

	for (const std::string_view other : { std::string_view{ "BM\x3a\x00" }, std::string_view{ "II*\x00" }, std::string_view{ "" } }) {
		const auto result = decoder.decode(other);
		expect(!result && result.error() == core::markdown_image_state::unsupported, "BMP, TIFF and empty input are unsupported");
	}

	// RFC-BPR-0084 Q2. WebP through qwebp, still and animated, within the bounds.
	const auto still = decoder.decode(encoded(solid(5, 3, Qt::red), "webp"));
	expect(still && still->Width == 5U && still->Height == 3U && still->Frames.size() == 1U && still->Frames[0].DelayMs == 0U, "a still WebP decodes");
	const auto webp = decoder.decode(webp_bytes());
	expect(webp && webp->Width == 8U && webp->Height == 6U, "an animated WebP decodes at its canvas size");
	expect(webp->Frames.size() == 2U && webp->Frames[0].DelayMs == 120U && webp->Frames[1].DelayMs == 30U, "both WebP frames keep their delays");
	expect(pixel(*webp, 0) == 0xFFFF0000U, "the first WebP frame is opaque red");
	// A lossless still whose VP8L header claims 8000 x 8000: within the side
	// bound, over the pixel bound, refused from the header alone.
	auto huge = std::string{ reinterpret_cast<const char*>(still_webp.data()), still_webp.size() };
	expect(decoder.decode(huge).has_value(), "negative control: the unpatched still decodes");
	const auto header = std::uint32_t{ 7'999U } | (std::uint32_t{ 7'999U } << 14U);
	for (std::size_t index = 0; index < 4U; ++index) {
		huge[21U + index] = static_cast<char>((header >> (8U * index)) & 0xFFU);
	}
	const auto tooLarge = decoder.decode(huge);
	expect(!tooLarge && tooLarge.error() == core::markdown_image_state::too_large, "a WebP over the pixel bound is too large before decoding");
	auto riffOnly = webp_bytes();
	riffOnly.replace(8U, 4U, "WAVE");
	const auto wave = decoder.decode(riffOnly);
	expect(!wave && wave.error() == core::markdown_image_state::unsupported, "another RIFF payload is unsupported");

	// RFC-BPR-0084 Q1. An admitted SVG, rasterized once at twice its size.
	const auto badge = decoder.decode(svg(R"svg(<defs><linearGradient id="g"><stop offset="0" stop-color="#f00"/></linearGradient></defs><rect width="20" height="10" fill="url(#g)"/><use xlink:href="#g"/>)svg"));
	expect(badge.has_value(), "a self-contained SVG rasterizes");
	expect(badge->Width == 20U && badge->Height == 10U && badge->Scale == 2U && badge->Frames.size() == 1U, "at its own size, twice the density");
	expect(badge->Frames[0].Pixels.size() == 40U * 20U * 4U && pixel_at(*badge, 20U, 10U) == 0xFFFF0000U, "the gradient fill is drawn");
	const auto viewBox = decoder.decode(svg(R"svg(<rect width="4" height="2" fill="#00f"/>)svg", R"svg(viewBox="0 0 40 20")svg"));
	expect(viewBox && viewBox->Width == 40U && viewBox->Height == 20U, "a viewBox gives the size when width and height are absent");
	const auto refusedSvg = [&decoder](const std::string& aDocument) {
		const auto result = decoder.decode(aDocument);
		return result ? core::markdown_image_state::loaded : result.error();
	};
	expect(refusedSvg(svg(R"svg(<image xlink:href="https://example.invalid/a.png" width="1" height="1"/>)svg")) == core::markdown_image_state::blocked, "an external image is refused");
	expect(refusedSvg(svg(R"svg(<image href="file:///etc/hosts" width="1" height="1"/>)svg")) == core::markdown_image_state::blocked, "a local file is refused");
	expect(refusedSvg(svg(R"svg(<rect width="1" height="1" style="fill:url(https://example.invalid/p#x)"/>)svg")) == core::markdown_image_state::blocked, "an external url() is refused");
	expect(refusedSvg(svg(R"svg(<script>alert(1)</script>)svg")) == core::markdown_image_state::blocked, "a script is refused");
	expect(refusedSvg(R"svg(<?xml version="1.0"?><!DOCTYPE svg [<!ENTITY a "aaaa">]><svg xmlns="http://www.w3.org/2000/svg">&a;</svg>)svg") == core::markdown_image_state::blocked, "an entity is refused");
	expect(refusedSvg(svg("", R"svg(width="20000" height="10")svg")) == core::markdown_image_state::too_large, "an SVG over the side bound is too large");
	// Negative control: the same document with only a fragment reference draws.
	expect(refusedSvg(svg(R"svg(<rect id="r" width="1" height="1"/><use xlink:href="#r"/>)svg")) == core::markdown_image_state::loaded, "a fragment reference is admitted");
	auto truncated = encoded(solid(64, 64, Qt::red), "png");
	truncated.resize(40U);
	const auto broken = decoder.decode(truncated);
	expect(!broken && (broken.error() == core::markdown_image_state::failed), "a truncated PNG fails");
	std::cout << "markdown-image-decoder:pass\n";
	return EXIT_SUCCESS;
}
