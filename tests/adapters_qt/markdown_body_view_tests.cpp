#include "../support/test_expectation.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextList>
#include <QTextTable>
#include <QUrl>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	using bpr::tests::expect;

	constexpr auto corpus = R"MD(# Native Markdown

Paragraph with *emphasis*, **strong**, ~~struck~~ and `inline code`.

> A quotation with a second line.
>
> Read this in the native reader.

1. First ordered item
2. Second ordered item
   - Nested bullet
     - Deeper bullet

- [x] Completed task
- [ ] Pending task

```cpp
    int value = 42;
	return value;
```

| Column | Result |
| --- | --- |
| native | **Readable** |
| source | Preserved |

---

[Public destination](https://example.test/review)
[Private-host fixture](https://ghe.example.test/owner/repo/blob/0123456789/README.md)
[Ambiguous relative](../guide.md)

![Architecture diagram](https://images.example.test/diagram.png)

<script>alert('inert')</script>
<img src="file:///tmp/markdown-must-not-open.png">

Final content: Ω 日本語 😀
)MD";

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	void key(QWidget& aWidget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(&aWidget, &press);
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers };
		QApplication::sendEvent(&aWidget, &release);
	}

	void check_corpus(qt::markdown_body_view& aView)
	{
		aView.set_body(corpus, "synthetic-pr:1:body");
		settle();
		expect(aView.rendered_available() && !aView.source_visible(), "corpus renders");
		auto* document = aView.rendered_view()->document();
		expect(document->begin().blockFormat().headingLevel() == 1, "heading is native structure");
		int  tasks = 0, lists = 0, quotes = 0, code = 0, images = 0, links = 0;
		bool strong = false, emphasis = false, strike = false, inlineCode = false, rule = false;
		for (auto block = document->begin(); block.isValid(); block = block.next()) {
			tasks += block.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker ? 1 : 0;
			lists += block.textList() != nullptr ? 1 : 0;
			quotes += block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel) > 0 ? 1 : 0;
			code += block.blockFormat().hasProperty(QTextFormat::BlockCodeFence) ? 1 : 0;
			rule |= block.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto format = it.fragment().charFormat();
				images += format.isImageFormat() ? 1 : 0;
				links += !format.anchorHref().isEmpty() ? 1 : 0;
				strong |= it.fragment().text() == QStringLiteral("strong") && format.fontWeight() >= QFont::Bold;
				emphasis |= it.fragment().text() == QStringLiteral("emphasis") && format.fontItalic();
				strike |= format.fontStrikeOut();
				inlineCode |= it.fragment().text() == QStringLiteral("inline code") && format.fontFixedPitch();
			}
		}
		std::cout << "corpus: tasks=" << tasks << " lists=" << lists << " quotes=" << quotes << " code=" << code << '\n';
		expect(tasks == 2 && lists >= 6 && quotes > 0 && code > 0, "tasks, nested lists, quotes and code have native structure");
		expect(strong && emphasis && strike && inlineCode && rule, "inline styles and thematic break render");
		expect(images == 0 && links >= 4, "all images replaced with linked accessible text");
		bool table = false;
		for (auto* frame : document->rootFrame()->childFrames()) {
			if (auto* found = qobject_cast<QTextTable*>(frame)) {
				table = found->rows() == 3 && found->columns() == 2;
				expect(found->cellAt(0, 0).firstCursorPosition().block().text() == QStringLiteral("Column"), "table header has no imported blank paragraph");
			}
		}
		expect(table, "table is a native table");
		const auto text = document->toPlainText();
		expect(text.contains(QStringLiteral("    int value = 42;\n\treturn value;")), "code indentation and tab survive");
		expect(text.contains(QStringLiteral("Image: Architecture diagram")), "image alt text remains readable");
		expect(text.contains(QStringLiteral("Final content: Ω 日本語 😀")), "final Unicode content survives");
		expect(text.contains(QStringLiteral("<script>")), "raw HTML is literal text");
		expect(!aView.rendered_view()->accessibleName().isEmpty() && !aView.source_view()->accessibleName().isEmpty(), "both modes have accessible names");
	}

	void check_source_and_refresh(qt::markdown_body_view& aView)
	{
		const std::string original = "# Title\r\n\r\nExact  spacing Ω\r\n";
		aView.set_body(original, "source:1");
		aView.copy_source();
		expect(QApplication::clipboard()->mimeData()->data(QStringLiteral("text/plain")).toStdString() == original, "copy preserves original UTF-8 and CRLF bytes");
		auto* toggle = aView.findChild<QPushButton*>(QStringLiteral("markdownSourceToggle"));
		expect(toggle != nullptr, "source control is discoverable");
		toggle->setFocus();
		key(*toggle, Qt::Key_Space);
		expect(aView.source_visible(), "keyboard source switch");
		key(*toggle, Qt::Key_Space);
		expect(!aView.source_visible(), "keyboard rendered switch");
		expect(aView.original_body() == original, "switching never reconstructs the source");
		std::string large;
		for (int i = 0; i < 400; ++i) {
			large += "Paragraph **bold** with readable text and a preserved reading position.\n\n";
		}
		aView.set_body(large, "refresh:1");
		settle();
		auto* reader   = aView.rendered_view();
		auto* document = reader->document();
		auto  cursor   = reader->textCursor();
		cursor.setPosition(300);
		cursor.setPosition(350, QTextCursor::KeepAnchor);
		reader->setTextCursor(cursor);
		reader->verticalScrollBar()->setValue(reader->verticalScrollBar()->maximum() / 2);
		const auto scroll = reader->verticalScrollBar()->value();
		aView.set_body(large, "refresh:1");
		expect(reader->document() == document && reader->textCursor().anchor() == 300 && reader->textCursor().position() == 350 && reader->verticalScrollBar()->value() == scroll, "unchanged refresh retains exact document, selection and scroll");
		aView.show_source(true);
		settle();
		expect(aView.source_view()->verticalScrollBar()->value() > 0, "first source switch retains approximate reading fraction");
		aView.show_source(false);
		expect(reader->textCursor().anchor() == 300 && reader->verticalScrollBar()->value() == scroll, "mode round trip retains rendered place and selection");
		aView.set_body("Replacement", "replacement:2");
		settle();
		expect(reader->toPlainText() == QStringLiteral("Replacement") && !reader->textCursor().hasSelection(), "identity replacement clears prior document selection");
		aView.clear();
		expect(aView.original_body().empty() && reader->toPlainText().isEmpty(), "clear invalidates both retained bodies");
	}

	void check_resources(qt::markdown_body_view& aView)
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "temporary resource directory");
		const auto path = directory.filePath(QStringLiteral("canary.png"));
		QImage     image{ 2, 2, QImage::Format_ARGB32 };
		image.fill(Qt::magenta);
		expect(image.save(path), "write designated canary image");
		const auto    local = QUrl::fromLocalFile(path);
		QTextDocument control;
		expect(!control.resource(QTextDocument::ImageResource, local).isNull(), "negative control: default Qt document reads the existing file");
		int loads = 0;
		QTextDocument::setDefaultResourceProvider([&loads](const QUrl&) -> QVariant { ++loads; return QByteArray{ "forbidden" }; });
		const auto hostile = std::string{ "![local](" } + local.toString().toStdString()
			+ ")\n![remote](https://example.test/pixel)\n![data](data:image/png;base64,AAAA)\n"
			+ "<iframe src='https://example.test/embed'></iframe>\n<style>@import 'https://example.test/style';</style>\n"
			+ "<form><input autofocus></form>\n[execute](javascript:alert) [local](file:///etc/passwd)";
		aView.set_body(hostile, "hostile:1");
		settle();
		for (const auto& target : { local, QUrl{ "https://example.test/pixel" }, QUrl{ "data:image/png;base64,AAAA" }, QUrl{ "qrc:/nonexistent" } }) {
			const auto resource = aView.rendered_view()->document()->resource(QTextDocument::ImageResource, target);
			expect(resource.isValid() && resource.toByteArray().isEmpty(), "resource denial returns valid empty bytes");
		}
		aView.resize(450, 450);
		settle();
		expect(loads == 0, "rendering, resize and resource lookup never call a global provider");
		QTextDocument::setDefaultResourceProvider({ });
		expect(aView.original_body() == hostile, "hostile body is not rewritten at the source");
	}

	void check_links(qt::markdown_body_view& aView)
	{
		std::string activated;
		int         gestures = 0;
		aView.set_link_dispatch([&](const std::string& aTarget) { activated = aTarget; ++gestures; });
		aView.set_body("[First](https://example.test/first) and [Untrusted](javascript:alert)", "links:1");
		settle();
		expect(gestures == 0, "display does not dispatch a link");
		auto* reader = aView.rendered_view();
		reader->setFocus();
		key(*reader, Qt::Key_Tab);
		expect(reader->textCursor().selectedText() == QStringLiteral("First"), "Tab selects the first link");
		key(*reader, Qt::Key_Return);
		expect(gestures == 1 && activated == "https://example.test/first", "Enter emits exactly one original destination gesture");
		key(*reader, Qt::Key_Tab);
		key(*reader, Qt::Key_Return);
		expect(gestures == 2 && activated == "javascript:alert", "untrusted target remains an application admission request, never a widget effect");
		key(*reader, Qt::Key_Tab);
		expect(!reader->hasFocus(), "Tab can leave the last link");
		auto empty = reader->textCursor();
		empty.clearSelection();
		reader->setTextCursor(empty);
		const auto selectAll = QKeySequence{ QKeySequence::SelectAll }[0];
		key(*reader, selectAll.key(), selectAll.keyboardModifiers());
		expect(!reader->textCursor().selectedText().isEmpty(), "keyboard text selection remains available");
		aView.set_link_dispatch({ });
	}

	void check_bounds(qt::markdown_body_view& aView)
	{
		for (const auto& source : { std::string(65'537, 'x'), std::string(4'097, 'x'), std::string(65, ' ') + "deep", std::string(9, '>') + "quote", std::string(17, '[') + "nested", std::string(33, '|'), std::string{ "ordinary\ttab" }, std::string{ "> ```\n> \tquoted code\n> ```" } }) {
			aView.set_body(source, "bound:" + source.substr(0, 10));
			expect(!aView.rendered_available() && aView.source_visible() && !aView.fallback_reason().isEmpty(), "pathological input selects explained full source fallback");
			expect(aView.original_body() == source && aView.source_view()->toPlainText().size() == static_cast<qsizetype>(source.size()), "fallback retains all source");
		}
		std::string maximum;
		while (maximum.size() < 65'536U) {
			maximum += "A long admitted prose paragraph whose native layout is bounded and readable.\n\n";
		}
		maximum.resize(65'536U);
		QElapsedTimer timer;
		timer.start();
		aView.set_body(maximum, "maximum:1");
		settle();
		const auto rendered = timer.elapsed();
		expect(aView.rendered_available(), "render-byte boundary is admitted");
		expect(rendered < 1'000, "bounded eager layout completes within the local one-second regression ceiling");
		maximum.resize(262'144U, 'x');
		timer.restart();
		aView.set_body(maximum, "maximum-source:1");
		settle();
		const auto fallback = timer.elapsed();
		expect(aView.source_visible() && aView.source_view()->toPlainText().size() == 262'144, "provider maximum remains complete in source");
		std::cout << "fallback-ms=" << fallback << '\n';
		expect(fallback < 1'000, "provider-size source fallback remains bounded locally");
		std::cout << "markdown-layout: qt=" << qVersion() << " scale=" << aView.devicePixelRatioF() << " render-65536-ms=" << rendered << " source-262144-ms=" << fallback << '\n';
		aView.set_body("Broken [link and **unclosed\n\nfinal text", "malformed:1");
		expect(aView.rendered_view()->toPlainText().contains(QStringLiteral("final text")), "malformed input never blanks the complete body");
	}

	void capture(qt::markdown_body_view& aView, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MARKDOWN_RENDER_DIR");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "create capture directory");
			settle();
			expect(aView.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "save rendered capture");
		}
	}

	void check_appearance_and_overflow(qt::markdown_body_view& aView)
	{
		aView.resize(740, 900);
		check_corpus(aView);
		capture(aView, QStringLiteral("native"));
		aView.set_body("```\n" + std::string(300, 'w') + "\n```\n\nFinal wide-block content", "wide:1");
		settle();
		expect(aView.width() == 740 && aView.rendered_view()->horizontalScrollBar()->maximum() > 0, "wide code scrolls without widening the reader");
		capture(aView, QStringLiteral("wide"));
		std::string table = "|";
		for (int column = 0; column < 8; ++column) {
			table += std::string(80, 'W') + "|";
		}
		table += "\n|---|---|---|---|---|---|---|---|\n|a|b|c|d|e|f|g|h|\n\nFinal table content";
		aView.set_body(table, "wide-table:1");
		settle();
		expect(aView.width() == 740 && aView.rendered_view()->horizontalScrollBar()->maximum() > 0, "wide table scrolls without widening the reader");
		expect(aView.rendered_view()->toPlainText().contains(QStringLiteral("Final table content")), "wide table retains final content");
		capture(aView, QStringLiteral("wide-table"));
		check_corpus(aView);
		const auto inherited = aView.palette();
		for (const auto& name : { QStringLiteral("dark"), QStringLiteral("high-contrast") }) {
			auto palette = inherited;
			palette.setColor(QPalette::Base, Qt::black);
			palette.setColor(QPalette::Window, Qt::black);
			palette.setColor(QPalette::Text, Qt::white);
			palette.setColor(QPalette::WindowText, Qt::white);
			palette.setColor(QPalette::AlternateBase, name == QStringLiteral("dark") ? QColor{ 35, 35, 35 } : Qt::black);
			palette.setColor(QPalette::Mid, Qt::white);
			palette.setColor(QPalette::Link, Qt::cyan);
			aView.setPalette(palette);
			auto font = aView.font();
			font.setPointSize(18);
			aView.setFont(font);
			settle();
			expect(aView.rendered_view()->document()->defaultFont().pointSize() == 18, "derived layout refreshes on font changes");
			capture(aView, name);
		}
		aView.setPalette(inherited);
	}

	/// Review Cycles notes (BPR-1154): every Conventional Comments label, Plain
	/// and malformed prefixes render as ordinary Markdown and keep their exact
	/// source bytes; the reader never interprets or rewrites a label.
	void check_labelled_bodies(qt::markdown_body_view& aView)
	{
		const auto bodies = std::vector<std::string>{
			"praise: Clear naming.",
			"nitpick: Trailing space.",
			"suggestion: Return early.",
			"issue (blocking): Unchecked input.",
			"todo: Add a test.",
			"question: Why two passes?",
			"thought: Could be lazy.",
			"chore: Update the notice.",
			"note: Mirrors the parser.",
			"typo: recieve",
			"polish: Tighten the wording.",
			"quibble (non-blocking, style): Prefer auto.",
			"Plain text with no label.",
			"suggestion(blocking) :spacing is odd",
			"> issue: quoted\r\n\r\n**nitpick**: bold prefix\r\n",
		};
		auto index = 0;
		for (const auto& body : bodies) {
			aView.set_body(body, "cycle-note:" + std::to_string(index++));
			settle();
			expect(aView.rendered_available() && aView.original_body() == body, "labelled note renders and keeps its bytes");
			aView.copy_source();
			expect(QApplication::clipboard()->mimeData()->data(QStringLiteral("text/plain")).toStdString() == body, "copy returns the authored bytes");
			const auto first = QString::fromUtf8(body.data(), static_cast<qsizetype>(body.size())).section(QLatin1Char(':'), 0, 0).remove(QLatin1Char('>')).remove(QLatin1Char('*')).trimmed();
			expect(aView.rendered_view()->document()->toPlainText().contains(first), "the label is shown as the author wrote it");
		}
	}
} // namespace

int main(int aArgc, char** aArgv)
try {
	QApplication                app{ aArgc, aArgv };
	bpr::qt::markdown_body_view view;
	view.resize(740, 900);
	view.show();
	settle();
	check_corpus(view);
	check_source_and_refresh(view);
	check_resources(view);
	check_links(view);
	check_bounds(view);
	check_appearance_and_overflow(view);
	check_labelled_bodies(view);
	std::cout << "markdown-body-view-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
