// RFC-BPR-0084. Images in the Markdown reader: which references it sends,
// the loading box, reflow on arrival, sizing, HiDPI drawing, animation,
// fallback text, links, copy and stale answers.
#include "../../src/adapters/qt/src/markdown_images.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMimeData>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	constexpr auto attachment = "https://github.com/user-attachments/assets/25addea2-357a-42c0-a1c8-fba0163382cc";

	auto body_text() -> std::string
	{
		return std::string{ "# Screenshots\n\n" }
		+ "Before the change:\n\n![Before the change](" + attachment + ")\n\n"
			+ "<img width=\"400\" height=\"120\" alt=\"Sized upload\" src=\"https://github.com/user-attachments/assets/00000000-357a-42c0-a1c8-fba0163382cc\" />\n\n"
			+ "A badge [![build](https://img.shields.io/badge/build-passing-green.png)](https://ci.example/run) inline, and `<img src=\"code.png\">` in code.\n\n"
			+ "- A list item with <a href=\"https://example.org/full\"><img src=\"https://e.example/thumb.png\" alt=\"Thumbnail\" width=\"50%\"></a>\n\n"
			+ "| Before | After |\n|---|---|\n| ![left](https://e.example/left.png) | ![right](https://e.example/right.png) |\n\n"
			+ "<picture><source media=\"(prefers-color-scheme: dark)\" srcset=\"https://e.example/dark.png\"><img src=\"https://e.example/light.png\" alt=\"Themed\"></picture>\n\n"
			+ "![Spinner](https://e.example/spinner.gif) ![Diagram](https://e.example/diagram.svg) ![Inline data](data:image/png;base64,AAAA)\n\n"
			+ "The final paragraph.\n";
	}

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	void capture(qt::markdown_body_view& aView, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MARKDOWN_RENDER_DIR");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "create capture directory");
			settle();
			expect(aView.grab().save(root + QStringLiteral("/images-") + aName + QStringLiteral(".png")), "save rendered capture");
		}
	}

	auto decoded(const int aWidth, const int aHeight, const std::vector<QColor>& aFrames, const std::uint32_t aDelay = 0U) -> std::shared_ptr<const application::markdown_decoded_image>
	{
		auto image    = std::make_shared<application::markdown_decoded_image>();
		image->Width  = static_cast<std::uint32_t>(aWidth);
		image->Height = static_cast<std::uint32_t>(aHeight);
		for (const auto& colour : aFrames) {
			auto frame = QImage{ aWidth, aHeight, QImage::Format_ARGB32_Premultiplied };
			frame.fill(colour);
			auto pixels = application::markdown_image_frame{ };
			pixels.Pixels.resize(static_cast<std::size_t>(aWidth) * static_cast<std::size_t>(aHeight) * 4U);
			std::memcpy(pixels.Pixels.data(), frame.constBits(), pixels.Pixels.size());
			pixels.DelayMs = aFrames.size() > 1U ? aDelay : 0U;
			image->Frames.push_back(std::move(pixels));
		}
		return image;
	}

	auto answer(const core::markdown_image_reference& aReference, const core::markdown_image_state aState,
				std::shared_ptr<const application::markdown_decoded_image> aImage = nullptr) -> application::markdown_image_projection
	{
		return { .Context = { }, .Body = { }, .Ordinal = aReference.Ordinal, .Source = aReference.Source, .State = aState, .Image = std::move(aImage) };
	}

	auto object_rect(qt::markdown_body_view& aView, const std::uint32_t aOrdinal) -> QRectF
	{
		auto*      document = aView.rendered_view()->document();
		const auto position = qt::detail::markdown_image_position(*document, aOrdinal);
		expect(position.has_value(), "image object present");
		const auto block  = document->findBlock(*position);
		const auto layout = block.layout();
		const auto line   = layout->lineForTextPosition(*position - block.position());
		const auto origin = document->documentLayout()->blockBoundingRect(block).topLeft();
		const auto x      = line.cursorToX(*position - block.position());
		const auto next   = line.cursorToX(*position - block.position() + 1);
		return { origin.x() + x, origin.y() + line.y(), next - x, line.height() };
	}

	void check_references_and_lifecycle()
	{
		qt::markdown_body_view view;
		view.resize(720, 1400);
		view.show();
		settle();
		std::vector<std::vector<core::markdown_image_reference>> asked;
		view.set_image_dispatch([&asked](std::vector<core::markdown_image_reference> aReferences) { asked.push_back(std::move(aReferences)); });
		view.set_body(body_text(), "description");
		settle();
		expect(asked.size() == 1U, "one request for the body's images");
		const auto               references = asked.front();
		std::vector<std::string> sources;
		for (const auto& reference : references) {
			sources.push_back(reference.Source);
		}
		const auto expected = std::vector<std::string>{
			attachment,
			"https://github.com/user-attachments/assets/00000000-357a-42c0-a1c8-fba0163382cc",
			"https://img.shields.io/badge/build-passing-green.png",
			"https://e.example/thumb.png",
			"https://e.example/left.png",
			"https://e.example/right.png",
			view.palette().color(QPalette::Base).lightness() < 128 ? "https://e.example/dark.png" : "https://e.example/light.png",
			"https://e.example/spinner.gif",
			"https://e.example/diagram.svg",
			"data:image/png;base64,AAAA",
		};
		expect(sources == expected, "Markdown, HTML, linked, list, table and picture images in document order; code is not an image");
		for (std::size_t index = 0; index < references.size(); ++index) {
			expect(references[index].Ordinal == index, "ordinals follow document order");
		}
		const auto plain = view.rendered_view()->toPlainText();
		expect(plain.contains(QStringLiteral("`<img src=\"code.png\">`")) || plain.contains(QStringLiteral("<img src=\"code.png\">")), "an <img> in code stays text");
		expect(!plain.contains(QStringLiteral("<img")) || plain.count(QStringLiteral("<img")) == 1, "only the code <img> remains as text");

		// The sized upload reserves its box before any pixels; others are one line.
		const auto reserved = object_rect(view, 1U);
		expect(qAbs(reserved.width() - 400.0) < 1.0 && qAbs(reserved.height() - 120.0) < 1.0, "width and height reserve the box while loading");
		capture(view, QStringLiteral("loading"));

		const auto finalTop = [&view] {
			auto* document = view.rendered_view()->document();
			return document->documentLayout()->blockBoundingRect(document->lastBlock()).top();
		};
		const auto beforeHeight = finalTop();
		// The first image arrives: 1252 x 625, wider than the reader.
		view.apply_image(answer(references[0], core::markdown_image_state::loaded, decoded(1252, 625, { QColor{ 30, 120, 220 } })));
		settle();
		const auto first     = object_rect(view, 0U);
		const auto available = view.rendered_view()->document()->textWidth() - 2.0 * view.rendered_view()->document()->documentMargin();
		expect(first.width() <= available + 0.5 && first.width() > available - 2.0, "an image wider than the reader fits its width");
		expect(qAbs(first.height() - first.width() * 625.0 / 1252.0) <= 1.0, "aspect ratio kept");
		expect(finalTop() > beforeHeight + 200.0, "the body reflows once the image arrives");

		view.apply_image(answer(references[1], core::markdown_image_state::loaded, decoded(800, 240, { QColor{ 200, 60, 60 } })));
		view.apply_image(answer(references[2], core::markdown_image_state::loaded, decoded(90, 20, { QColor{ 60, 160, 60 } })));
		view.apply_image(answer(references[3], core::markdown_image_state::loaded, decoded(300, 300, { QColor{ 160, 60, 160 } })));
		view.apply_image(answer(references[4], core::markdown_image_state::loaded, decoded(40, 40, { Qt::darkCyan })));
		view.apply_image(answer(references[5], core::markdown_image_state::loaded, decoded(40, 40, { Qt::darkYellow })));
		view.apply_image(answer(references[6], core::markdown_image_state::loaded, decoded(120, 60, { Qt::gray })));
		view.apply_image(answer(references[7], core::markdown_image_state::loaded, decoded(24, 24, { Qt::black, Qt::white }, 50U)));
		view.apply_image(answer(references[8], core::markdown_image_state::unsupported));
		view.apply_image(answer(references[9], core::markdown_image_state::blocked));
		settle();
		expect(qAbs(object_rect(view, 1U).width() - 400.0) < 1.0 && qAbs(object_rect(view, 1U).height() - 120.0) < 1.0, "width and height attributes are honoured once loaded");
		expect(qAbs(object_rect(view, 2U).width() - 90.0) < 1.0, "a small image keeps its natural size");
		expect(object_rect(view, 3U).width() > 0.40 * available && object_rect(view, 3U).width() < 0.55 * available, "a percentage width is a share of the reader");
		const auto text = view.rendered_view()->toPlainText();
		expect(text.contains(QStringLiteral("[Image: Diagram (format not shown) — open image link]")), "an unsupported image keeps its placeholder with the reason");
		expect(text.contains(QStringLiteral("[Image: Inline data — open image link]")), "a blocked image keeps the original placeholder");

		// Drawn pixels: the first image's centre is its colour at any scale.
		const auto grab   = view.rendered_view()->viewport()->grab().toImage();
		const auto centre = object_rect(view, 0U).center() - QPointF{ 0.0, static_cast<qreal>(view.rendered_view()->verticalScrollBar()->value()) };
		const auto ratio  = grab.devicePixelRatio();
		const auto colour = grab.pixelColor(QPoint{ static_cast<int>(centre.x() * ratio), static_cast<int>(centre.y() * ratio) });
		expect(qAbs(colour.red() - 30) <= 3 && qAbs(colour.green() - 120) <= 3 && qAbs(colour.blue() - 220) <= 3, "the decoded pixels are drawn where the image is");
		capture(view, QStringLiteral("loaded"));

		// Links: a linked image opens its link, an unlinked one its own source.
		const auto* store = view.image_store();
		expect(store->images()[2].Link == QStringLiteral("https://ci.example/run") && store->images()[3].Link == QStringLiteral("https://example.org/full"), "linked images keep their links");
		auto*      document = view.rendered_view()->document();
		const auto anchorOf = [document](const std::uint32_t aOrdinal) {
			auto cursor = QTextCursor{ document };
			cursor.setPosition(*qt::detail::markdown_image_position(*document, aOrdinal) + 1);
			return cursor.charFormat().anchorHref();
		};
		expect(anchorOf(0U) == QString::fromUtf8(attachment) && anchorOf(2U) == QStringLiteral("https://ci.example/run"), "image objects carry their link destination");

		// Animation moves frames while visible.
		const auto    frameBefore = store->images()[7].Frame;
		QElapsedTimer wait;
		wait.start();
		while (store->images()[7].Frame == frameBefore && wait.elapsed() < 2000) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		expect(store->images()[7].Frame != frameBefore, "an animated GIF animates");

		// Stale and duplicate answers change nothing.
		auto stale              = answer(references[0], core::markdown_image_state::failed);
		stale.Source            = "https://e.example/other.png";
		const auto heightBefore = document->size().height();
		view.apply_image(stale);
		view.apply_image(answer(references[0], core::markdown_image_state::failed));
		expect(document->size().height() == heightBefore && store->images()[0].State == core::markdown_image_state::loaded, "a stale or repeated answer is ignored");
		view.request_images();
		expect(asked.size() == 1U, "nothing is asked again once every image has an answer");
		view.set_body(body_text(), "description");
		expect(asked.size() == 1U, "an unchanged body asks nothing");

		// Copy of a partial selection writes the images back as Markdown.
		auto cursor = QTextCursor{ document };
		cursor.setPosition(3);
		cursor.setPosition(*qt::detail::markdown_image_position(*document, 2U) + 1, QTextCursor::KeepAnchor);
		view.rendered_view()->setTextCursor(cursor);
		view.rendered_view()->copy();
		const auto copied = QApplication::clipboard()->text();
		expect(copied.contains(QStringLiteral("![Before the change](") + QString::fromUtf8(attachment) + QLatin1Char{ ')' }) && copied.contains(QStringLiteral("[![build](https://img.shields.io/badge/build-passing-green.png)](https://ci.example/run)")),
			   "a partial copy keeps images as Markdown");
		expect(!copied.contains(QChar::ObjectReplacementCharacter), "no object character reaches the clipboard");
	}

	void check_without_dispatch_and_scroll()
	{
		// Without a holder to ask, the reader shows the placeholder and asks nothing.
		qt::markdown_body_view plain;
		plain.resize(600, 400);
		plain.show();
		plain.set_body("![Alt](https://e.example/a.png)\n", "plain");
		settle();
		expect(plain.rendered_view()->toPlainText().contains(QStringLiteral("[Image: Alt — open image link]")), "no dispatch keeps the placeholder");

		// A holder bound afterwards renders the body again so its images load.
		std::vector<core::markdown_image_reference> late;
		plain.set_image_dispatch([&late](std::vector<core::markdown_image_reference> aReferences) { late = std::move(aReferences); });
		expect(late.size() == 1U && late.front().Source == "https://e.example/a.png", "a late holder is asked once");

		// An image above the reading position does not move what is being read.
		qt::markdown_body_view view;
		view.resize(600, 300);
		view.show();
		std::vector<core::markdown_image_reference> asked;
		view.set_image_dispatch([&asked](std::vector<core::markdown_image_reference> aReferences) { asked = std::move(aReferences); });
		std::string longBody = "![Top](https://e.example/top.png)\n\n";
		for (int index = 0; index < 60; ++index) {
			longBody += "Paragraph " + std::to_string(index) + " of the long body.\n\n";
		}
		view.set_body(longBody, "long");
		settle();
		auto* scroll = view.rendered_view()->verticalScrollBar();
		scroll->setValue(scroll->maximum() / 2);
		const auto cursor     = view.rendered_view()->cursorForPosition(QPoint{ 10, 10 });
		const auto readBefore = cursor.position();
		view.apply_image(answer(asked.front(), core::markdown_image_state::loaded, decoded(500, 400, { Qt::blue })));
		settle();
		const auto readAfter = view.rendered_view()->cursorForPosition(QPoint{ 10, 10 }).position();
		expect(qAbs(readAfter - readBefore) <= 40, "the paragraph being read stays in view when an image above it arrives");
	}

	void check_in_place_height()
	{
		qt::markdown_body_view post;
		post.set_in_place(true);
		std::vector<core::markdown_image_reference> asked;
		post.set_image_dispatch([&asked](std::vector<core::markdown_image_reference> aReferences) { asked = std::move(aReferences); });
		post.set_body("A post with an image:\n\n![Shot](https://e.example/shot.png)\n", "post");
		const auto before = post.heightForWidth(300);
		auto       grew   = 0;
		post.set_height_observer([&grew] { ++grew; });
		post.apply_image(answer(asked.front(), core::markdown_image_state::loaded, decoded(600, 300, { Qt::red })));
		const auto after = post.heightForWidth(300);
		expect(grew > 0 && after > before + 100, "in place, an arriving image grows the post's height for its width");
		expect(post.heightForWidth(150) < after, "a narrower post scales its image down");
	}
} // namespace

int main(int aArgc, char** aArgv)
try {
	QApplication app{ aArgc, aArgv };
	check_references_and_lifecycle();
	check_without_dispatch_and_scroll();
	check_in_place_height();
	std::cout << "markdown-image-view-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
