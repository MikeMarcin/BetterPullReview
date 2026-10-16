// RFC-BPR-0084. The body-image decoder: signature-chosen PNG, JPEG and GIF
// only, size bounds before allocation, animation frames and delays.
#include "bpr/qt/markdown_image_decoder.hpp"

#include <QBuffer>
#include <QCoreApplication>
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
} // namespace

int main(int argument_count, char** arguments)
{
	QCoreApplication application{ argument_count, arguments };
	const auto       decoder = qt::markdown_image_decoder{ };

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

	for (const std::string_view other : { std::string_view{ "<svg xmlns=\"http://www.w3.org/2000/svg\"/>" }, std::string_view{ "RIFF\x10\x00\x00\x00WEBPVP8 ", 16 }, std::string_view{ "BM\x3a\x00" }, std::string_view{ "" } }) {
		const auto result = decoder.decode(other);
		expect(!result && result.error() == core::markdown_image_state::unsupported, "SVG, WebP, BMP and empty input are unsupported");
	}
	auto truncated = encoded(solid(64, 64, Qt::red), "png");
	truncated.resize(40U);
	const auto broken = decoder.decode(truncated);
	expect(!broken && (broken.error() == core::markdown_image_state::failed), "a truncated PNG fails");
	std::cout << "markdown-image-decoder:pass\n";
	return EXIT_SUCCESS;
}
