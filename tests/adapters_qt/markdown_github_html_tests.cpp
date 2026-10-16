// [[BPR-1544]]. GitHub parity for the Markdown reader's HTML: `<details>`,
// GitHub's sanitised HTML subset and emoji shortcodes. Each fixture under
// tests/fixtures/markdown/github-parity-v1 has GitHub's own rendering beside
// it (`*.github.html`, the Markdown API in gfm mode), and the courses compare
// the reader's document with it. The hostile courses hold RFC-BPR-0036 AC-03:
// nothing loads, nothing runs, and no link reaches past application admission.

#include "../support/test_expectation.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextList>
#include <QTextTable>
#include <QUrl>

#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#ifndef BPR_MARKDOWN_PARITY_FIXTURES
	#error "BPR_MARKDOWN_PARITY_FIXTURES must name the GitHub parity fixture directory"
#endif

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	// The private property ids markdown_github.hpp gives presentation.
	constexpr auto internal_property = QTextFormat::UserProperty + 0x0B54;
	constexpr auto keyboard_property = QTextFormat::UserProperty + 0x0B55;
	constexpr auto details_property  = QTextFormat::UserProperty + 0x0B56;
	constexpr auto open_property     = QTextFormat::UserProperty + 0x0B57;
	constexpr auto line_separator    = QChar{ QChar::LineSeparator };

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	void key(QWidget& aWidget, const int aKey)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &release);
	}

	auto read_file(const QString& aName) -> QByteArray
	{
		QFile file{ QStringLiteral(BPR_MARKDOWN_PARITY_FIXTURES "/") + aName };
		expect(file.open(QIODevice::ReadOnly), "fixture opens: " + aName.toStdString());
		return file.readAll();
	}

	auto fixture(const QString& aName) -> std::string { return read_file(aName + QStringLiteral(".md")).toStdString(); }
	auto reference(const QString& aName) -> QString { return QString::fromUtf8(read_file(aName + QStringLiteral(".github.html"))); }

	auto matches(const QString& aText, const QString& aPattern) -> QStringList
	{
		QStringList found;
		for (auto it = QRegularExpression{ aPattern, QRegularExpression::DotMatchesEverythingOption }.globalMatch(aText); it.hasNext();) {
			const auto match = it.next();
			found.push_back(match.lastCapturedIndex() >= 1 ? match.captured(1) : match.captured(0));
		}
		return found;
	}

	/// GitHub's visible words: tags dropped, entities read, whitespace folded.
	auto visible(const QString& aHtml) -> QString
	{
		auto text = aHtml;
		text.remove(QRegularExpression{ QStringLiteral("<svg.*?</svg>"), QRegularExpression::DotMatchesEverythingOption });
		text.replace(QRegularExpression{ QStringLiteral("<[^>]*>") }, QStringLiteral(" "));
		text.replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">")).replace(QStringLiteral("&quot;"), QStringLiteral("\"")).replace(QStringLiteral("&amp;"), QStringLiteral("&"));
		return text.simplified();
	}

	auto document_of(qt::markdown_body_view& aView) -> QTextDocument& { return *aView.rendered_view()->document(); }

	auto folded(QTextDocument& aDocument) -> QString
	{
		return aDocument.toRawText().replace(line_separator, QLatin1Char{ ' ' }).replace(QChar{ QChar::ParagraphSeparator }, QLatin1Char{ ' ' }).simplified();
	}

	auto block_containing(QTextDocument& aDocument, const QString& aText) -> QTextBlock
	{
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			if (block.text().contains(aText)) {
				return block;
			}
		}
		return { };
	}

	auto format_of(QTextDocument& aDocument, const QString& aText) -> QTextCharFormat
	{
		const auto found = aDocument.find(aText, 0, QTextDocument::FindCaseSensitively);
		auto       probe = QTextCursor{ &aDocument };
		probe.setPosition(found.selectionStart() + 1);
		return probe.charFormat();
	}

	auto reserved_marks(QTextDocument& aDocument) -> int
	{
		auto count = 0;
		for (const auto ch : aDocument.toRawText()) {
			count += (ch.unicode() >= 0xFDE0 && ch.unicode() <= 0xFDEF) || ch.unicode() == 0x2008 ? 1 : 0;
		}
		return count;
	}

	void show(qt::markdown_body_view& aView, const std::string& aBody, const std::string& aIdentity)
	{
		aView.set_body(aBody, aIdentity);
		settle();
		expect(aView.rendered_available() && !aView.source_visible(), "body renders: " + aIdentity);
	}

	struct link_fact
	{
		QString Text;
		QString Href;
		QString Internal;
	};

	auto links_of(QTextDocument& aDocument) -> std::vector<link_fact>
	{
		std::vector<link_fact> links;
		auto                   end = -1;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto format = it.fragment().charFormat();
				if (!format.isAnchor() || format.anchorHref().isEmpty()) {
					continue;
				}
				if (!links.empty() && end == it.fragment().position() && links.back().Href == format.anchorHref()) {
					links.back().Text += it.fragment().text();
				} else {
					links.push_back({ it.fragment().text(), format.anchorHref(), format.stringProperty(internal_property) });
				}
				end = it.fragment().position() + it.fragment().length();
			}
		}
		return links;
	}

	struct summary_fact
	{
		QString Text;
		bool    Open = false;
		int     Id   = 0;
	};

	auto summaries_of(QTextDocument& aDocument) -> std::vector<summary_fact>
	{
		std::vector<summary_fact> found;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().hasProperty(details_property)) {
				found.push_back({ block.text().mid(2), block.blockFormat().boolProperty(open_property), block.blockFormat().intProperty(details_property) });
			}
		}
		return found;
	}

	auto visible_block(QTextDocument& aDocument, const QString& aText) -> bool
	{
		const auto block = block_containing(aDocument, aText);
		expect(block.isValid(), "block exists: " + aText.toStdString());
		return block.isVisible();
	}

	void check_details(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("details")), "details:1");
		auto&      document  = document_of(aView);
		const auto github    = reference(QStringLiteral("details"));
		const auto summaries = summaries_of(document);
		auto       texts     = QStringList{ };
		auto       open      = QStringList{ };
		for (const auto& summary : summaries) {
			texts.push_back(summary.Text.simplified());
			open.push_back(summary.Open ? QStringLiteral("open") : QStringLiteral("closed"));
		}
		auto githubTexts = QStringList{ };
		for (const auto& summary : matches(github, QStringLiteral("<summary>(.*?)</summary>"))) {
			githubTexts.push_back(visible(summary));
		}
		std::cout << "details: reader=" << texts.join(QLatin1Char{ '|' }).toStdString() << " open=" << open.join(QLatin1Char{ ',' }).toStdString()
				  << " github=" << matches(github, QStringLiteral("<details(?: open=\"\")?>")).join(QLatin1Char{ ',' }).toStdString() << '\n';
		expect(texts == githubTexts, "the summaries are GitHub's, in order, \"Details\" where none is written");
		expect(open == QStringList{ "closed", "open", "closed", "closed", "closed", "closed" } && matches(github, QStringLiteral("<details(?: open=\"\")?>")) == QStringList{ "<details>", "<details open=\"\">", "<details>", "<details>", "<details>", "<details>" },
			   "only the section GitHub opens is open");
		expect(!visible_block(document, QStringLiteral("[1/3] Compiling")) && !visible_block(document, QStringLiteral("trimmed")), "a closed section hides its code and text");
		expect(visible_block(document, QStringLiteral("item one")) && visible_block(document, QStringLiteral("After the details.")), "an open section and what follows show");
		expect(!visible_block(document, QStringLiteral("Inner")) && !visible_block(document, QStringLiteral("Nested content.")), "a closed outer section hides an inner one");
		expect(block_containing(document, QStringLiteral("Build log")).text().startsWith(QStringLiteral("▸ ")) && block_containing(document, QStringLiteral("Open by default")).text().startsWith(QStringLiteral("▾ ")), "the triangle says closed or open");
		expect(format_of(document, QStringLiteral("Bold")).fontWeight() >= QFont::Bold && format_of(document, QStringLiteral("code")).fontFixedPitch(), "a summary's own HTML formatting reads");
		expect(reserved_marks(document) == 0, "no mark reaches the reader");

		// The keyboard and the pointer open and close a section in place;
		// nothing reaches the application.
		auto gestures = 0;
		aView.set_link_dispatch([&gestures](const std::string&) { ++gestures; });
		auto*      reader = aView.rendered_view();
		const auto height = document.size().height();
		reader->setFocus();
		key(*reader, Qt::Key_Tab);
		expect(reader->textCursor().selectedText().contains(QStringLiteral("Build log")), "Tab reaches the first summary");
		expect(reader->accessibleDescription().startsWith(QStringLiteral("Collapsed section")), "a closed summary is announced as collapsed");
		key(*reader, Qt::Key_Return);
		expect(visible_block(document_of(aView), QStringLiteral("[1/3] Compiling")) && document_of(aView).size().height() > height, "Enter opens the section and the body grows");
		expect(reader->accessibleDescription().startsWith(QStringLiteral("Expanded section")) && reader->textCursor().selectedText().contains(QStringLiteral("Build log")), "the summary keeps the focus and says it is open");
		key(*reader, Qt::Key_Return);
		expect(!visible_block(document_of(aView), QStringLiteral("[1/3] Compiling")), "Enter closes it again");
		const auto outer = block_containing(document_of(aView), QStringLiteral("Outer"));
		const auto point = reader->cursorRect(QTextCursor{ outer }).center() + QPoint{ 12, 0 };
		auto       press = QMouseEvent{ QEvent::MouseButtonPress, point, reader->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(reader->viewport(), &press);
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, point, reader->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(reader->viewport(), &release);
		expect(visible_block(document_of(aView), QStringLiteral("Inner")) && !visible_block(document_of(aView), QStringLiteral("Nested content.")), "a click opens the outer section; the inner one stays closed");
		expect(gestures == 0, "a summary never reaches the application");

		// A link inside a closed section is skipped by Tab; a table inside one
		// gives up its borders until it opens.
		show(aView, "<details>\n<summary>Hidden</summary>\n\n[inside](https://example.test/inside)\n\n| a | b |\n| --- | --- |\n| 1 | 2 |\n</details>\n\n[after](https://example.test/after)", "details:skip");
		reader->setFocus();
		key(*reader, Qt::Key_Tab);
		key(*reader, Qt::Key_Tab);
		expect(reader->textCursor().selectedText() == QStringLiteral("after"), "Tab passes a closed section's link");
		auto* table = qobject_cast<QTextTable*>(document_of(aView).rootFrame()->childFrames().value(0));
		expect(table != nullptr && table->format().border() == 0.0, "a closed section's table draws no border");
		key(*reader, Qt::Key_Tab);
		key(*reader, Qt::Key_Tab);
		key(*reader, Qt::Key_Return);
		table = qobject_cast<QTextTable*>(document_of(aView).rootFrame()->childFrames().value(0));
		expect(table != nullptr && table->format().border() > 0.0 && visible_block(document_of(aView), QStringLiteral("inside")), "opening it shows the table's borders and the link");
		aView.set_link_dispatch({ });
	}

	void check_html(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("html")), "html:1");
		auto&      document = document_of(aView);
		const auto github   = visible(reference(QStringLiteral("html")));
		const auto text     = folded(document);
		for (const auto& phrase : { QStringLiteral("H 2 O and E = mc 2 ; press Ctrl + C ."), QStringLiteral("Centered paragraph"), QStringLiteral("Right-aligned div"), QStringLiteral("An HTML link and a script link and a fragment ."),
									QStringLiteral("An HTML quote"), QStringLiteral("HTML list item one"), QStringLiteral("An HTML heading"), QStringLiteral("attributes stripped"), QStringLiteral("font tag"),
									QStringLiteral("center tag"), QStringLiteral("custom element"), QStringLiteral("Send"), QStringLiteral("<script>alert('x')</script>"), QStringLiteral("<style>body { color: red }</style>"),
									QStringLiteral("Text with a literal < sign and 3 > 2.") }) {
			expect(github.contains(phrase), "GitHub shows: " + phrase.toStdString());
		}
		for (const auto& phrase : { QStringLiteral("Centered paragraph"), QStringLiteral("Right-aligned div"), QStringLiteral("An HTML quote"), QStringLiteral("HTML list item one"), QStringLiteral("An HTML heading"),
									QStringLiteral("attributes stripped"), QStringLiteral("font tag"), QStringLiteral("center tag"), QStringLiteral("custom element"), QStringLiteral("Send"),
									QStringLiteral("<script>alert('x')</script>"), QStringLiteral("<style>body { color: red }</style>"), QStringLiteral("Text with a literal < sign and 3 > 2.") }) {
			expect(text.contains(phrase), "the reader shows: " + phrase.toStdString());
		}
		expect(!text.contains(QStringLiteral("<b>")) && !text.contains(QStringLiteral("<form")) && !text.contains(QStringLiteral("<span")) && !text.contains(QStringLiteral("<font")), "allowed and dropped tags leave no markup");
		expect(block_containing(document, QStringLiteral("Line one")).text() == QStringLiteral("Line one") + line_separator + QStringLiteral("line two") + line_separator + QStringLiteral("line three."), "<br> is a line break");
		const auto sub = document.find(QStringLiteral("H2O"));
		auto       at  = QTextCursor{ &document };
		at.setPosition(sub.selectionStart() + 2);
		expect(at.charFormat().verticalAlignment() == QTextCharFormat::AlignSubScript, "<sub> lowers its text");
		const auto squared = document.find(QStringLiteral("mc2"));
		at.setPosition(squared.selectionStart() + 3);
		expect(at.charFormat().verticalAlignment() == QTextCharFormat::AlignSuperScript, "<sup> raises its text");
		expect(format_of(document, QStringLiteral("Ctrl")).boolProperty(keyboard_property) && format_of(document, QStringLiteral("Ctrl")).fontFixedPitch(), "<kbd> is a key");
		expect(format_of(document, QStringLiteral("bold")).fontWeight() >= QFont::Bold && format_of(document, QStringLiteral("strong")).fontWeight() >= QFont::Bold, "<b> and <strong> are bold");
		expect(format_of(document, QStringLiteral("italic")).fontItalic() && format_of(document, QStringLiteral("em ")).fontItalic(), "<i> and <em> are italic");
		expect(format_of(document, QStringLiteral("struck")).fontStrikeOut() && format_of(document, QStringLiteral("deleted")).fontStrikeOut(), "<s> and <del> strike through");
		expect(format_of(document, QStringLiteral("inserted")).fontUnderline(), "<ins> underlines, as GitHub's browser does");
		expect(!format_of(document, QStringLiteral("under")).fontUnderline() && !github.contains(QStringLiteral("<u>")), "<u> is plain text, as GitHub strips it");
		expect(format_of(document, QStringLiteral("code ")).fontFixedPitch(), "<code> is code");
		expect(block_containing(document, QStringLiteral("Centered paragraph")).blockFormat().alignment().testFlag(Qt::AlignHCenter), "<p align=center> centers");
		expect(block_containing(document, QStringLiteral("Right-aligned div")).blockFormat().alignment().testFlag(Qt::AlignRight), "<div align=right> aligns right");
		expect(block_containing(document, QStringLiteral("An HTML heading")).blockFormat().headingLevel() == 3, "<h3> is a heading");
		expect(block_containing(document, QStringLiteral("HTML list item one")).textList() != nullptr && block_containing(document, QStringLiteral("second")).textList() != nullptr, "HTML lists are lists");
		auto tables = 0;
		for (auto* frame : document.rootFrame()->childFrames()) {
			if (auto* table = qobject_cast<QTextTable*>(frame)) {
				++tables;
				expect(table->rows() == 2 && table->columns() == 2 && table->cellAt(1, 1).firstCursorPosition().block().text() == QStringLiteral("1"), "an HTML table is a table");
			}
		}
		expect(tables == 1, "one table");
		expect(block_containing(document, QStringLiteral("text")).text() == QStringLiteral("  text") || text.contains(QStringLiteral("preformatted")), "<pre> keeps its lines");
		auto links = QStringList{ };
		for (const auto& link : links_of(document)) {
			links.push_back(link.Href);
		}
		std::cout << "html-links: " << links.join(QLatin1Char{ ' ' }).toStdString() << '\n';
		expect(links == QStringList{ QStringLiteral("https://example.invalid/docs"), QStringLiteral("#section") }, "an HTML link keeps its destination; a script link is only its text, as on GitHub");
		expect(reserved_marks(document) == 0, "no mark reaches the reader");
	}

	void check_hostile(qt::markdown_body_view& aView)
	{
		auto loads = 0;
		QTextDocument::setDefaultResourceProvider([&loads](const QUrl&) -> QVariant { ++loads; return QByteArray{ "forbidden" }; });
		auto gestures     = 0;
		auto destinations = std::vector<std::string>{ };
		aView.set_link_dispatch([&](const std::string& aTarget) { ++gestures; destinations.push_back(aTarget); });
		show(aView, fixture(QStringLiteral("hostile-html")), "hostile:1");
		auto& document = document_of(aView);
		aView.resize(500, 700);
		settle();
		static_cast<void>(aView.grab());
		const auto text = folded(document);
		expect(loads == 0, "rendering, resizing and painting hostile HTML loads nothing");
		auto hrefs = QStringList{ };
		for (const auto& link : links_of(document)) {
			if (link.Internal.isEmpty()) {
				hrefs.push_back(link.Href);
			}
		}
		std::cout << "hostile-links: " << hrefs.join(QLatin1Char{ ' ' }).toStdString() << '\n';
		// Plain URLs in the tag filter's visible text are autolinked as any
		// URL in text is; they are HTTPS and still go through admission.
		expect(hrefs.contains(QStringLiteral("https://example.test/ok")) && hrefs.contains(QStringLiteral("#top")), "admissible HTML links are links");
		for (const auto& href : hrefs) {
			const auto lower = href.toLower();
			expect(href == QStringLiteral("#top") || (lower.startsWith(QStringLiteral("https://example.test/")) && !lower.contains(QLatin1Char{ '@' })), "no script, data, file or credential destination is a link: " + href.toStdString());
		}
		for (const auto& word : { QStringLiteral("script"), QStringLiteral("mixed case"), QStringLiteral("entity tab"), QStringLiteral("data"), QStringLiteral("file"), QStringLiteral("vbscript"), QStringLiteral("credentials") }) {
			expect(text.contains(word), "a refused link keeps its text: " + word.toStdString());
		}
		auto images = 0;
		for (auto block = document.begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				images += it.fragment().charFormat().isImageFormat() ? 1 : 0;
			}
		}
		expect(images == 0, "no HTML becomes an image or object");
		// The exported document's real elements (escaped literal text starts
		// with &lt;) carry no event handler, resource, script, frame or form.
		// Qt's export writes its own head (a qrichtext meta); the body is ours.
		const auto exported = document.toHtml();
		const auto html     = exported.mid(exported.indexOf(QStringLiteral("<body")));
		for (const auto& forbidden : { QStringLiteral(R"(<[^>]*\son[a-z]+\s*=)"), QStringLiteral(R"(<[^>]*url\()"), QStringLiteral(R"(<[^>]*background=)"), QStringLiteral(R"(<[^>]*\ssrc=)"), QStringLiteral(R"(<(script|iframe|form|input|object|embed|svg|link|meta|base)\b)"),
									   QStringLiteral(R"(<[^>]*class="x")") }) {
			expect(!QRegularExpression{ forbidden, QRegularExpression::CaseInsensitiveOption }.match(html).hasMatch(), "the document has no element matching " + forbidden.toStdString());
		}
		expect(text.contains(QStringLiteral("<script>fetch(")) && text.contains(QStringLiteral("<SCRIPT>alert('upper')</SCRIPT>")) && text.contains(QStringLiteral("<iframe src=")) && text.contains(QStringLiteral("<textarea>typed text</textarea>")), "the tag filter's tags stay visible text, as on GitHub");
		expect(text.contains(QStringLiteral("Send")) && !text.contains(QStringLiteral("<button")) && !text.contains(QStringLiteral("<object")) && !text.contains(QStringLiteral("<svg")), "forms, objects and SVG are dropped, their text kept");
		expect(block_containing(document, QStringLiteral("A styled paragraph")).blockFormat().alignment().testFlag(Qt::AlignHCenter), "an allowed attribute survives beside forbidden ones");
		expect(format_of(document, QStringLiteral("bold with a handler")).fontWeight() >= QFont::Bold && format_of(document, QStringLiteral("K")).boolProperty(keyboard_property), "formatting keeps its meaning without its handlers or style");
		expect(text.contains(QStringLiteral("<script>alert('encoded')</script>")), "encoded markup stays text");
		expect(format_of(document, QStringLiteral("an unclosed bold tag")).fontWeight() >= QFont::Bold && format_of(document, QStringLiteral("The paragraph after it.")).fontWeight() < QFont::Bold, "an unclosed tag ends with its paragraph");
		const auto summaries = summaries_of(document);
		expect(summaries.size() == 1 && summaries[0].Text == QStringLiteral("Hostile summary") && summaries[0].Open, "a hostile <details> is an ordinary open section");
		// Every link the keyboard reaches is one the application admits or refuses.
		auto* reader = aView.rendered_view();
		reader->setFocus();
		for (auto step = 0; step < 6; ++step) {
			key(*reader, Qt::Key_Tab);
			key(*reader, Qt::Key_Return);
		}
		for (const auto& destination : destinations) {
			expect(destination == "#top" || (destination.starts_with("https://example.test/") && destination.find('@') == std::string::npos), "only an admissible destination reaches the application: " + destination);
		}
		expect(reserved_marks(document) == 0, "no mark reaches the reader");

		// Generated hostile bodies: random tags, attributes and nesting, from a
		// fixed seed so a failure reproduces.
		auto       random     = std::mt19937{ 1544 }; // NOLINT(bugprone-random-generator-seed)
		const auto tags       = std::array{ "script", "style", "iframe", "a", "b", "kbd", "sub", "details", "summary", "p", "div", "table", "td", "img", "svg", "form", "input", "object", "embed", "link", "meta", "base", "pre", "br", "hr", "math", "video", "audio", "x-custom" };
		const auto attributes = std::array{ " href=\"javascript:alert(1)\"", " href=\"https://example.test/r\"", " href=\"data:text/html,x\"", " src=\"https://example.test/i.png\"", " onerror=\"alert(1)\"", " style=\"background:url(https://example.test/s)\"",
											" align=\"center\"", " open", " background=\"https://example.test/b\"", " href=\"file:///etc/passwd\"", "" };
		for (auto body = 0; body < 300; ++body) {
			auto source = std::string{ };
			for (auto part = 0; part < 24; ++part) {
				const auto* tag = tags[random() % tags.size()];
				source += random() % 4 == 0 ? std::string{ "</" } + tag + ">" : std::string{ "<" } + tag + attributes[random() % attributes.size()] + ">";
				source += random() % 3 == 0 ? "\n\n" : random() % 2 == 0 ? "\n"
																		 : " text ";
			}
			aView.set_body(source, "hostile-generated:" + std::to_string(body));
			for (const auto& link : links_of(document_of(aView))) {
				const auto lower = link.Href.toLower();
				expect(!lower.startsWith(QStringLiteral("javascript")) && !lower.startsWith(QStringLiteral("data:")) && !lower.startsWith(QStringLiteral("file:")), "no generated body makes a forbidden link");
			}
			expect(reserved_marks(document_of(aView)) == 0, "no generated body leaks a mark");
		}
		settle();
		expect(loads == 0, "no generated body loads anything");
		QTextDocument::setDefaultResourceProvider({ });
		aView.set_link_dispatch({ });
	}

	void check_emoji(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("emoji")), "emoji:1");
		auto&      document  = document_of(aView);
		const auto github    = reference(QStringLiteral("emoji"));
		const auto paragraph = visible(matches(github, QStringLiteral("<p dir=\"auto\">(.*?)</p>")).value(0));
		const auto first     = block_containing(document, QStringLiteral("Great work")).text().replace(line_separator, QLatin1Char{ ' ' }).simplified();
		std::cout << "emoji: reader=" << first.toStdString() << " github=" << paragraph.toStdString() << '\n';
		expect(first == paragraph, "every shortcode becomes GitHub's emoji");
		const auto text = folded(document);
		expect(text.contains(QStringLiteral(":notanemoji:")) && format_of(document, QStringLiteral(":+1:")).fontFixedPitch(), "an unknown or coded shortcode stays as written");
		expect(text.contains(QStringLiteral("Adjacent:smile:text")) && text.contains(QStringLiteral("10:30:45")) && text.contains(QStringLiteral("https://example.invalid/:smile:/x")), "a shortcode needs a boundary, and a URL keeps its text");
		expect(aView.original_body() == fixture(QStringLiteral("emoji")), "source keeps the shortcodes");
	}

	void check_template(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("pr-html")), "pr-html:1");
		auto&      document = document_of(aView);
		const auto github   = reference(QStringLiteral("pr-html"));
		auto       texts    = QStringList{ };
		for (const auto& summary : summaries_of(document)) {
			texts.push_back(summary.Text);
		}
		auto githubTexts = QStringList{ };
		for (const auto& summary : matches(github, QStringLiteral("<summary>(.*?)</summary>"))) {
			githubTexts.push_back(visible(summary));
		}
		expect(texts == githubTexts, "the template's sections are GitHub's");
		auto keys  = 0;
		auto press = block_containing(document, QStringLiteral("to rebuild the index"));
		auto inKey = false;
		for (auto it = press.begin(); !it.atEnd(); ++it) {
			const auto key = it.fragment().charFormat().boolProperty(keyboard_property);
			keys += key && !inKey ? 1 : 0;
			inKey = key;
		}
		expect(keys == matches(github, QStringLiteral("<kbd>")).size(), "the template's keys");
		expect(block_containing(document, QStringLiteral("Release 4.2")).blockFormat().alignment().testFlag(Qt::AlignHCenter), "the centered heading block");
		expect(folded(document).contains(QStringLiteral("Speeds up the cache writer \U0001F680 and fixes the crash on resume \U0001F41B.")), "emoji in prose");
		auto tables = 0;
		for (auto* frame : document.rootFrame()->childFrames()) {
			tables += qobject_cast<QTextTable*>(frame) != nullptr ? 1 : 0;
		}
		expect(tables == 2 && folded(document).contains(QStringLiteral("✅ passing")), "a Markdown table in a section and an HTML table with emoji");
		expect(!visible_block(document, QStringLiteral("cache_writer/serial")), "the benchmark section is closed");
		expect(reserved_marks(document) == 0, "no mark reaches the reader");
	}

	void check_bounds(qt::markdown_body_view& aView)
	{
		std::string body;
		auto        part  = 0;
		const auto  prose = std::string(90, 'w');
		while (body.size() < 62'000U) {
			const auto number = std::to_string(++part);
			for (const auto& piece : { std::string{ "<details>\n<summary>Section " }, number, std::string{ "</summary>\n\nText :+1: " }, prose, std::string{ " <kbd>K</kbd> <sub>2</sub>\n</details>\n\n<p align=\"center\"><b>" },
									   number, std::string{ "</b> " }, prose, std::string{ "</p>\n\n" } }) {
				body += piece;
			}
		}
		QElapsedTimer timer;
		timer.start();
		aView.set_body(body, "html-bounds:1");
		settle();
		const auto elapsed = timer.elapsed();
		std::cout << "github-html-62k-ms=" << elapsed << " rendered=" << aView.rendered_available() << " sections=" << part << '\n';
		expect(aView.rendered_available() && summaries_of(document_of(aView)).size() == static_cast<std::size_t>(part), "the maximal body renders every section");
		expect(elapsed < 1'500, "a maximal body of HTML renders within the local regression ceiling");
	}

	struct theme
	{
		core::appearance_mode Mode;
		const char*           Name;
	};
	constexpr auto themes = std::array{
		theme{ core::appearance_mode::light, "light" },
		theme{ core::appearance_mode::dark, "dark" },
		theme{ core::appearance_mode::high_contrast_light, "high-contrast-light" },
		theme{ core::appearance_mode::high_contrast_dark, "high-contrast-dark" },
	};

	auto palette_of(const core::appearance_mode aMode) -> QPalette
	{
		auto profile = qt::appearance_profile{ };
		profile.select_mode(aMode);
		return profile.derive(QApplication::palette());
	}

	/// Opens every section, for a capture that shows what they hold.
	void open_all(qt::markdown_body_view& aView)
	{
		auto* reader = aView.rendered_view();
		for (const auto& summary : summaries_of(document_of(aView))) {
			if (!summary.Open) {
				const auto block = block_containing(document_of(aView), summary.Text);
				const auto point = reader->cursorRect(QTextCursor{ block }).center() + QPoint{ 12, 0 };
				auto       press = QMouseEvent{ QEvent::MouseButtonPress, point, reader->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(reader->viewport(), &press);
				auto release = QMouseEvent{ QEvent::MouseButtonRelease, point, reader->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
				QApplication::sendEvent(reader->viewport(), &release);
				settle();
			}
		}
	}

	void capture(qt::markdown_body_view& aView)
	{
		const auto root = qEnvironmentVariable("BPR_MARKDOWN_RENDER_DIR");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "create capture directory");
		const auto inherited = aView.palette();
		const auto save      = [&](qt::markdown_body_view& aReader, QWidget& aHolder, const QString& aName, const bool aInPlace) {
			auto* const reader = aReader.rendered_view();
			if (aInPlace) {
				const auto height = aReader.heightForWidth(480);
				aHolder.resize(496, height + 16);
				aReader.setGeometry(8, 8, 480, height);
			} else {
				reader->document()->setTextWidth(reader->viewport()->width());
				aReader.resize(720, static_cast<int>(reader->document()->size().height()) + (aReader.height() - reader->viewport()->height()) + 8);
			}
			settle();
			expect(aHolder.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "save capture");
		};
		auto  holder = QWidget{ };
		auto* post   = new qt::markdown_body_view{ &holder };
		post->set_in_place(true);
		holder.setAutoFillBackground(true);
		holder.show();
		for (const auto& [mode, name] : themes) {
			const auto palette = palette_of(mode);
			aView.setPalette(palette);
			holder.setPalette(palette);
			for (const auto& page : { QStringLiteral("pr-html"), QStringLiteral("details"), QStringLiteral("html"), QStringLiteral("emoji") }) {
				aView.resize(720, 400);
				show(aView, fixture(page), (page + QLatin1Char{ ':' } + QString::fromLatin1(name)).toStdString());
				save(aView, aView, page + QLatin1Char{ '-' } + QString::fromLatin1(name), false);
				if (page == QStringLiteral("details") || page == QStringLiteral("pr-html")) {
					open_all(aView);
					aView.rendered_view()->setTextCursor(QTextCursor{ aView.rendered_view()->document() });
					save(aView, aView, page + QStringLiteral("-open-") + QString::fromLatin1(name), false);
				}
				post->set_body(fixture(page), (QStringLiteral("in-place:") + page + QLatin1Char{ ':' } + QString::fromLatin1(name)).toStdString());
				save(*post, holder, QStringLiteral("in-place-") + page + QLatin1Char{ '-' } + QString::fromLatin1(name), true);
			}
		}
		aView.setPalette(inherited);
		aView.resize(740, 900);
	}
} // namespace

int main(int aArgc, char** aArgv)
try {
	QApplication                app{ aArgc, aArgv };
	bpr::qt::markdown_body_view view;
	view.resize(740, 900);
	view.show();
	settle();
	check_details(view);
	check_html(view);
	check_hostile(view);
	check_emoji(view);
	check_template(view);
	check_bounds(view);
	capture(view);
	std::cout << "markdown-github-html-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
