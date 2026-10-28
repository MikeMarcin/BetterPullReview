// [[BPR-1543]]. GitHub parity for the Markdown reader: hidden HTML comments,
// single newlines as line breaks, alerts and footnotes. Each fixture under
// tests/fixtures/markdown/github-parity-v1 has GitHub's own rendering beside
// it (`*.github.html`, from the Markdown API in gfm mode), and the courses
// compare the reader's document with that structure.

#include "../support/test_expectation.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
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

#include <array>
#include <cstdlib>
#include <iostream>
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
	constexpr auto alert_property    = QTextFormat::UserProperty + 0x0B50;
	constexpr auto ordinal_property  = QTextFormat::UserProperty + 0x0B51;
	constexpr auto title_property    = QTextFormat::UserProperty + 0x0B52;
	constexpr auto footnote_property = QTextFormat::UserProperty + 0x0B53;
	constexpr auto internal_property = QTextFormat::UserProperty + 0x0B54;
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
		for (auto it = QRegularExpression{ aPattern }.globalMatch(aText); it.hasNext();) {
			const auto match = it.next();
			found.push_back(match.lastCapturedIndex() >= 1 ? match.captured(1) : match.captured(0));
		}
		return found;
	}

	/// GitHub draws a footnote reference's number in brackets.
	auto bracketed(QStringList aNumbers) -> QStringList
	{
		for (auto& number : aNumbers) {
			number = QLatin1Char{ '[' } + number + QLatin1Char{ ']' };
		}
		return aNumbers;
	}

	/// GitHub's visible words: tags dropped, entities read, whitespace folded.
	auto reference_text(const QString& aHtml) -> QString
	{
		auto text = aHtml;
		text.remove(QRegularExpression{ QStringLiteral("<svg.*?</svg>"), QRegularExpression::DotMatchesEverythingOption });
		text.replace(QRegularExpression{ QStringLiteral("<[^>]*>") }, QStringLiteral(" "));
		text.replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">")).replace(QStringLiteral("&quot;"), QStringLiteral("\"")).replace(QStringLiteral("&amp;"), QStringLiteral("&"));
		return text.simplified();
	}

	auto document_of(qt::markdown_body_view& aView) -> QTextDocument& { return *aView.rendered_view()->document(); }

	auto folded_text(QTextDocument& aDocument) -> QString
	{
		return aDocument.toRawText().replace(line_separator, QLatin1Char{ ' ' }).simplified();
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
		bool    Raised = false;
		int     End    = 0;
	};

	auto links_of(QTextDocument& aDocument) -> std::vector<link_fact>
	{
		std::vector<link_fact> links;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto format = it.fragment().charFormat();
				if (!format.isAnchor() || format.anchorHref().isEmpty()) {
					continue;
				}
				// A raised "2" after a back reference's arrow is the same link.
				if (!links.empty() && links.back().End == it.fragment().position() && links.back().Href == format.anchorHref()) {
					links.back().Text += it.fragment().text();
				} else {
					links.push_back({ it.fragment().text(), format.anchorHref(), format.stringProperty(internal_property), format.verticalAlignment() == QTextCharFormat::AlignSuperScript });
				}
				links.back().End = it.fragment().position() + it.fragment().length();
			}
		}
		return links;
	}

	void check_comments(qt::markdown_body_view& aView)
	{
		const auto body = fixture(QStringLiteral("comments"));
		show(aView, body, "comments:1");
		auto&      document = document_of(aView);
		const auto text     = folded_text(document);
		const auto github   = reference_text(reference(QStringLiteral("comments")));
		for (const auto& hidden : { QStringLiteral("a single-line comment that GitHub hides"), QStringLiteral("A multi-line template comment"), QStringLiteral("It spans a blank line"), QStringLiteral("hidden words") }) {
			expect(!github.contains(hidden), "GitHub hides the comment: " + hidden.toStdString());
			expect(!text.contains(hidden), "the reader hides the comment: " + hidden.toStdString());
		}
		for (const auto& kept : { QStringLiteral("Visible before the comment."), QStringLiteral("Visible after the comment."), QStringLiteral("<!-- literal in code -->"), QStringLiteral("<!-- literal in a fenced block -->"),
								  QStringLiteral("<!-- literal in an indented block -->"), QStringLiteral("An unclosed inline <!-- stays as written"), QStringLiteral("text keeps its neighbours."), QStringLiteral("Final visible line.") }) {
			expect(github.contains(kept), "GitHub keeps: " + kept.toStdString());
			expect(text.contains(kept), "the reader keeps: " + kept.toStdString());
		}
		expect(block_containing(document, QStringLiteral("Visible before")) != block_containing(document, QStringLiteral("Visible after")), "a comment line between two lines parts them, as GitHub's HTML block does");
		expect(block_containing(document, QStringLiteral("literal in a fenced block")).blockFormat().hasProperty(QTextFormat::BlockCodeFence), "a comment in a fence stays code");
		expect(aView.original_body() == body && aView.source_view()->toPlainText().contains(QStringLiteral("a single-line comment that GitHub hides")), "source shows the comment exactly as written");
		// Negative control: Qt's own import shows every comment.
		auto control = QTextDocument{ };
		control.setMarkdown(QString::fromStdString(body), QTextDocument::MarkdownFeatures{ QTextDocument::MarkdownDialectGitHub } | QTextDocument::MarkdownNoHTML);
		expect(control.toPlainText().contains(QStringLiteral("hidden words")), "negative control: without the preparation the comment is visible");

		show(aView, fixture(QStringLiteral("comments-unclosed")), "comments:unclosed");
		const auto unclosed = folded_text(document_of(aView));
		const auto closedOn = reference_text(reference(QStringLiteral("comments-unclosed")));
		expect(closedOn.contains(QStringLiteral("Before the comment.")) && !closedOn.contains(QStringLiteral("After the comment.")), "GitHub hides everything after an unclosed comment block");
		expect(unclosed.contains(QStringLiteral("Before the comment.")) && !unclosed.contains(QStringLiteral("After the comment.")), "so does the reader");
	}

	void check_breaks(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("breaks")), "breaks:1");
		auto&      document     = document_of(aView);
		const auto breaks       = document.toRawText().count(line_separator);
		const auto githubBreaks = matches(reference(QStringLiteral("breaks")), QStringLiteral("<br>")).size();
		std::cout << "breaks: reader=" << breaks << " github=" << githubBreaks << '\n';
		expect(breaks == githubBreaks, "one line break for each of GitHub's <br>");
		const auto first = block_containing(document, QStringLiteral("First line of a paragraph"));
		expect(first.text() == QStringLiteral("First line of a paragraph") + line_separator + QStringLiteral("second line of the same paragraph") + line_separator + QStringLiteral("emphasis that ends a line") + line_separator + QStringLiteral("third line ends with strong") + line_separator + QStringLiteral("and the paragraph ends here."),
			   "a paragraph's lines stay one block with a line separator between each");
		auto italic = false, strong = false;
		for (auto it = first.begin(); !it.atEnd(); ++it) {
			// GitHub reads `_text_` as emphasis; Qt's dialect read it as underline.
			italic |= it.fragment().text() == QStringLiteral("emphasis that ends a line") && it.fragment().charFormat().fontItalic() && !it.fragment().charFormat().fontUnderline();
			strong |= it.fragment().text() == QStringLiteral("strong") && it.fragment().charFormat().fontWeight() >= QFont::Bold;
		}
		expect(italic && strong, "emphasis that ends a line still closes");
		const auto item = block_containing(document, QStringLiteral("A list item"));
		expect(item.textList() != nullptr && item.text() == QStringLiteral("A list item") + line_separator + QStringLiteral("that wraps onto a second line"), "a list item's lines break inside the one item");
		expect(block_containing(document, QStringLiteral("Another item")).textList() == item.textList(), "the list keeps its two items");
		expect(block_containing(document, QStringLiteral("A quoted line")).text().contains(line_separator), "a quote's lines break");
		auto tables = 0;
		for (auto* frame : document.rootFrame()->childFrames()) {
			if (auto* table = qobject_cast<QTextTable*>(frame)) {
				++tables;
				expect(table->rows() == 3 && table->columns() == 2 && table->cellAt(2, 1).firstCursorPosition().block().text() == QStringLiteral("2"), "the table keeps its rows and cells");
			}
		}
		expect(tables == 1, "the table stays a table");
		const auto code = block_containing(document, QStringLiteral("code line one"));
		expect(code.text() == QStringLiteral("code line one") && code.next().text() == QStringLiteral("code line two") && code.blockFormat().hasProperty(QTextFormat::BlockCodeFence), "code lines are untouched");
		const auto span = block_containing(document, QStringLiteral("that crosses"));
		expect(!span.text().contains(line_separator) && span.text().contains(QStringLiteral("that crosses a line")), "a code span over two lines stays one span, as on GitHub");
		expect(block_containing(document, QStringLiteral("Two trailing spaces")).text() == QStringLiteral("Two trailing spaces") + line_separator + QStringLiteral("also break here."), "a written hard break is the same line separator in the same block");
		expect(block_containing(document, QStringLiteral("Heading")).blockFormat().headingLevel() == 2, "a setext heading stays a heading");
		expect(reserved_marks(document) == 0, "no mark reaches the reader");
		// An authored U+2008 turns the transformation off rather than being misread.
		const auto authored = std::string{ "one\xE2\x80\x88\ntwo" };
		show(aView, authored, "breaks:authored-mark");
		expect(document_of(aView).toRawText().contains(QChar{ 0x2008 }) && !document_of(aView).toRawText().contains(line_separator), "a body holding the break mark keeps its soft break");
		// Tab-bearing fences still repair exactly beside the new breaks.
		show(aView, "```\n\tindented\n```\nline one\nline two", "breaks:tabs");
		expect(block_containing(document_of(aView), QStringLiteral("indented")).text() == QStringLiteral("\tindented") && block_containing(document_of(aView), QStringLiteral("line one")).text().contains(line_separator), "code tabs are restored and prose breaks");
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

	auto contrast(const QColor& aFirst, const QColor& aSecond) -> double
	{
		return qt::contrast_ratio(qt::to_appearance_color(aFirst), qt::to_appearance_color(aSecond));
	}

	auto title_colour(QTextDocument& aDocument, const QString& aTitle) -> QColor
	{
		const auto block = block_containing(aDocument, aTitle);
		return block.begin().fragment().charFormat().foreground().color();
	}

	void check_alerts(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("alerts")), "alerts:1");
		auto& document = document_of(aView);
		auto  kinds    = QStringList{ };
		auto  titles   = QStringList{ };
		for (auto block = document.begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().boolProperty(title_property)) {
				static const auto names = QStringList{ QString{ }, QStringLiteral("note"), QStringLiteral("tip"), QStringLiteral("important"), QStringLiteral("warning"), QStringLiteral("caution") };
				kinds.push_back(names.value(block.blockFormat().intProperty(alert_property)));
				titles.push_back(block.text());
			}
		}
		const auto github = reference(QStringLiteral("alerts"));
		std::cout << "alerts: reader=" << kinds.join(QLatin1Char{ ',' }).toStdString() << '\n';
		expect(kinds == matches(github, QStringLiteral(R"(markdown-alert markdown-alert-(\w+))")), "the reader's alerts are GitHub's, in order");
		expect(titles == matches(github, QStringLiteral(R"(</svg>(\w+)</p>)")), "each title is GitHub's word");
		const auto text = folded_text(document);
		expect(!text.contains(QStringLiteral("[!NOTE] Useful")) && !document.toRawText().contains(QStringLiteral("[!TIP] Helpful")), "an alert's marker is not shown");
		expect(text.contains(QStringLiteral("[!NOTE] Text on the marker line is not an alert.")) && text.contains(QStringLiteral("[!TIP]")), "a marker with text after it, or inside a list, stays a plain quote");
		const auto inside = [&document](const QString& aText) { return block_containing(document, aText).blockFormat().intProperty(alert_property); };
		expect(inside(QStringLiteral("a list item")) == 1 && inside(QStringLiteral("make check")) == 1, "a list and code block stay inside their alert");
		expect(inside(QStringLiteral("A plain quote after")) == 0 && inside(QStringLiteral("Text on the marker line")) == 0, "a plain quote after an alert is not drawn as one");
		expect(block_containing(document, QStringLiteral("Useful information")).blockFormat().intProperty(ordinal_property) != block_containing(document, QStringLiteral("Helpful advice")).blockFormat().intProperty(ordinal_property), "two alerts in a row are two alerts");
		expect(reserved_marks(document) == 0, "no mark reaches the reader");

		show(aView, "> [!IMPORTANT]\n> # Inside heading\n>\n> | a | b |\n> | --- | --- |\n> | c | d |\n\nAfter the alert.", "alerts:structure");
		auto& structure = document_of(aView);
		expect(block_containing(structure, QStringLiteral("Inside heading")).blockFormat().intProperty(alert_property) == 3 && block_containing(structure, QStringLiteral("d")).blockFormat().intProperty(alert_property) == 3,
			   "a heading and a table, which Qt does not mark as quoted, stay inside the alert");
		expect(block_containing(structure, QStringLiteral("After the alert.")).blockFormat().intProperty(alert_property) == 0, "the alert ends with its quote");

		show(aView, "> [!NOTE]\n> Authored \xEF\xB7\xA1 mark.", "alerts:authored-mark");
		expect(block_containing(document_of(aView), QStringLiteral("[!NOTE]")).isValid() && block_containing(document_of(aView), QStringLiteral("Authored")).blockFormat().intProperty(alert_property) == 0
				   && document_of(aView).toRawText().contains(QChar{ 0xFDE1 }),
			   "a body holding a reserved mark is read without alerts and keeps its character");

		// Each theme: a title reads at 4.5:1 on the reader's base, 7:1 in high
		// contrast; the five kinds differ; the bar is drawn in the title's colour.
		const auto inherited = aView.palette();
		for (const auto& [mode, name] : themes) {
			const auto palette = palette_of(mode);
			aView.setPalette(palette);
			show(aView, fixture(QStringLiteral("alerts")), std::string{ "alerts:" } + name);
			auto&      themed = document_of(aView);
			const auto base   = palette.color(QPalette::Active, QPalette::Base);
			const auto high   = mode == core::appearance_mode::high_contrast_light || mode == core::appearance_mode::high_contrast_dark;
			auto       seen   = std::vector<QRgb>{ };
			for (const auto& title : { QStringLiteral("Note"), QStringLiteral("Tip"), QStringLiteral("Important"), QStringLiteral("Warning"), QStringLiteral("Caution") }) {
				const auto colour = title_colour(themed, title);
				const auto ratio  = contrast(colour, base);
				std::cout << "alert-colour: " << name << ' ' << title.toStdString() << ' ' << colour.name().toStdString() << " ratio=" << ratio << '\n';
				expect(ratio >= (high ? 7.0 : 4.5), std::string{ "the alert title reads on its base in " } + name);
				seen.push_back(colour.rgb());
			}
			std::ranges::sort(seen);
			expect(std::ranges::adjacent_find(seen) == seen.end() || high, std::string{ "the five kinds have five colours in " } + name);
			const auto image    = aView.rendered_view()->viewport()->grab().toImage();
			const auto block    = block_containing(themed, QStringLiteral("Useful information"));
			const auto rect     = themed.documentLayout()->blockBoundingRect(block);
			const auto margin   = themed.documentMargin();
			const auto pixel    = image.pixelColor(QPointF{ margin + 2.0, rect.center().y() }.toPoint() * image.devicePixelRatio());
			const auto expected = title_colour(themed, QStringLiteral("Note"));
			expect(std::abs(pixel.red() - expected.red()) <= 3 && std::abs(pixel.green() - expected.green()) <= 3 && std::abs(pixel.blue() - expected.blue()) <= 3, std::string{ "the Note bar is drawn in its colour in " } + name);
			const auto plain = image.pixelColor(QPointF{ margin + 2.0, themed.documentLayout()->blockBoundingRect(block_containing(themed, QStringLiteral("A plain quote after"))).center().y() }.toPoint() * image.devicePixelRatio());
			expect(plain != pixel, std::string{ "a plain quote has no bar in " } + name);
		}
		aView.setPalette(inherited);
	}

	void check_footnotes(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("footnotes")), "footnotes:1");
		auto&       document = document_of(aView);
		const auto  github   = reference(QStringLiteral("footnotes"));
		const auto  links    = links_of(document);
		QStringList references, backs;
		for (const auto& link : links) {
			if (link.Href.startsWith(QStringLiteral("#fn-"))) {
				expect(link.Raised && link.Internal.startsWith(QStringLiteral("fnback-")), "a reference is a raised in-body link");
				references.push_back(link.Text);
			} else if (link.Href.startsWith(QStringLiteral("#fnref-"))) {
				backs.push_back(link.Text);
			}
		}
		std::cout << "footnotes: references=" << references.join(QLatin1Char{ ',' }).toStdString() << " backs=" << backs.join(QLatin1Char{ ',' }).toStdString() << '\n';
		expect(references == bracketed(matches(github, QStringLiteral(R"(data-footnote-ref="" [^>]*>(\d+)</a>)"))), "the references are numbered as GitHub numbers them, bracketed as its stylesheet draws them");
		expect(backs.size() == matches(github, QStringLiteral("data-footnote-backref=\"\" ")).size() && backs.value(1) == QStringLiteral("↩2"), "one back reference for each reference, the second numbered");
		auto items = std::vector<QTextBlock>{ };
		for (auto block = document.begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().boolProperty(footnote_property) && block.textList() != nullptr) {
				items.push_back(block);
			}
		}
		expect(static_cast<qsizetype>(items.size()) == matches(github, QStringLiteral("<li id=\"user-content-fn-")).size(), "the footnotes section lists GitHub's footnotes");
		expect(items.size() == 2 && items[0].text().startsWith(QStringLiteral("My reference.")) && items[1].text().startsWith(QStringLiteral("Every new line should be prefixed with 2 spaces.") + line_separator + QStringLiteral("This allows")),
			   "each footnote reads as written, its lines broken as GitHub breaks them");
		expect(items[0].blockNumber() > block_containing(document, QStringLiteral("A reference with no definition")).blockNumber(), "the footnotes come at the end");
		const auto text = folded_text(document);
		expect(text.contains(QStringLiteral("[^missing]")) && text.contains(QStringLiteral("[^1]")) && !text.contains(QStringLiteral("Nothing refers")), "a missing or coded reference stays literal and an unused definition is dropped");
		expect(reserved_marks(document) == 0, "no mark reaches the reader");

		// The reported bug: a one-word definition made a link to that word.
		show(aView, "Text x[^1].\n\n[^1]: word", "footnotes:bug");
		for (const auto& link : links_of(document_of(aView))) {
			expect(link.Href != QStringLiteral("word"), "a footnote never becomes a link to its own text");
		}
		expect(block_containing(document_of(aView), QStringLiteral("word")).blockFormat().boolProperty(footnote_property), "the definition is shown in the footnotes");
		// Negative control: Qt's own import makes that link and drops the definition.
		auto control = QTextDocument{ };
		control.setMarkdown(QStringLiteral("Text x[^1].\n\n[^1]: word"), QTextDocument::MarkdownFeatures{ QTextDocument::MarkdownDialectGitHub } | QTextDocument::MarkdownNoHTML);
		expect(control.find(QStringLiteral("^1")).charFormat().anchorHref() == QStringLiteral("word"), "negative control: without the preparation the reference links to its text");

		// Keyboard and pointer: a reference moves to its back reference and back,
		// within the body; no gesture reaches the application.
		auto gestures = 0;
		aView.set_link_dispatch([&gestures](const std::string&) { ++gestures; });
		show(aView, fixture(QStringLiteral("footnotes")), "footnotes:navigate");
		auto* reader = aView.rendered_view();
		reader->setFocus();
		key(*reader, Qt::Key_Tab);
		const auto first    = reader->textCursor().selectionStart();
		const auto firstEnd = reader->textCursor().selectionEnd();
		expect(reader->textCursor().selectedText() == QStringLiteral("[1]"), "Tab reaches the first reference");
		key(*reader, Qt::Key_Return);
		expect(reader->textCursor().selectedText() == QStringLiteral("↩") && reader->textCursor().block().blockFormat().boolProperty(footnote_property), "Enter moves to the footnote's back reference");
		key(*reader, Qt::Key_Return);
		expect(reader->textCursor().selectionStart() == first && reader->textCursor().selectedText() == QStringLiteral("[1]"), "Enter on the back reference returns to the reference");
		key(*reader, Qt::Key_Tab);
		key(*reader, Qt::Key_Tab);
		expect(reader->textCursor().selectedText() == QStringLiteral("[1]"), "Tab reaches the second reference to footnote 1");
		key(*reader, Qt::Key_Return);
		expect(reader->textCursor().selectedText() == QStringLiteral("↩2"), "the second reference moves to the second back reference");
		// The pointer: press and release on the first reference.
		auto cursor = QTextCursor{ reader->document() };
		cursor.setPosition(first);
		reader->setTextCursor(cursor);
		reader->verticalScrollBar()->setValue(0);
		settle();
		auto probe = QTextCursor{ reader->document() };
		probe.setPosition(first + 1);
		const auto inside = reader->cursorRect(probe).center();
		probe.setPosition(first);
		const auto left = reader->cursorRect(probe).center().x();
		probe.setPosition(firstEnd);
		const auto right = reader->cursorRect(probe).center().x();
		const auto point = QPoint{ left + (right - left) / 2, inside.y() };
		expect(!reader->anchorAt(point).isEmpty(), "the pointer targets the interior of the first reference");
		auto press = QMouseEvent{ QEvent::MouseButtonPress, point, reader->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(reader->viewport(), &press);
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, point, reader->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(reader->viewport(), &release);
		expect(reader->textCursor().selectedText() == QStringLiteral("↩"), "a click on a reference moves to its footnote");
		expect(gestures == 0, "in-body footnote links never reach the application");
		show(aView, "[Ordinary](#fn-1) link.", "footnotes:authored");
		reader->setFocus();
		key(*reader, Qt::Key_Tab);
		key(*reader, Qt::Key_Return);
		expect(gestures == 1, "an authored link that looks like a footnote link is still an application gesture");
		aView.set_link_dispatch({ });
	}

	void check_template(qt::markdown_body_view& aView)
	{
		show(aView, fixture(QStringLiteral("pr-template")), "template:1");
		auto&      document = document_of(aView);
		const auto github   = reference(QStringLiteral("pr-template"));
		const auto text     = folded_text(document);
		expect(!text.contains(QStringLiteral("Thanks for opening a pull request")) && !text.contains(QStringLiteral("Reviewers: the interesting part")), "the template's comments are hidden");
		expect(text.contains(QStringLiteral("<!-- this comment is part of the code and stays -->")) && text.contains(QStringLiteral("<!-- contributing -->")), "comments in code stay");
		expect(document.toRawText().count(line_separator) == matches(github, QStringLiteral("<br>")).size(), "the template breaks its lines where GitHub does");
		auto headings = 0, tasks = 0, alerts = 0;
		for (auto block = document.begin(); block.isValid(); block = block.next()) {
			headings += block.blockFormat().headingLevel() == 2 ? 1 : 0;
			tasks += block.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker ? 1 : 0;
			alerts += block.blockFormat().boolProperty(title_property) ? 1 : 0;
		}
		expect(headings == matches(github, QStringLiteral("<h2 dir=")).size() && tasks == matches(github, QStringLiteral("task-list-item-checkbox")).size(), "headings and tasks as on GitHub");
		expect(alerts == matches(github, QStringLiteral("markdown-alert-title")).size(), "the template's alerts");
		auto references = QStringList{ };
		for (const auto& link : links_of(document)) {
			if (link.Href.startsWith(QStringLiteral("#fn-"))) {
				references.push_back(link.Text);
			}
		}
		expect(references == bracketed(matches(github, QStringLiteral(R"(data-footnote-ref="" [^>]*>(\d+)</a>)"))), "the template's footnotes");
		expect(reserved_marks(document) == 0, "no mark reaches the reader");
	}

	void check_bounds(qt::markdown_body_view& aView)
	{
		// Within every preview budget (2,048 lines, 8,192 formatting marks)
		// and near the 65,536-byte ceiling, with every construct repeated.
		const auto  prose = std::string(110, 'w');
		std::string body;
		auto        note = 0;
		while (body.size() < 60'000U) {
			++note;
			const auto number = std::to_string(note);
			for (const auto& part : { std::string{ "<!-- hidden " }, number, std::string{ " " }, prose, std::string{ " -->\nA line with a reference[^n" }, number, std::string{ "] " }, prose,
									  std::string{ "\nand its next line " }, prose, std::string{ "\n\n> [!TIP]\n> An alert " }, prose, std::string{ "\n> and a second line " }, prose, std::string{ "\n\n" } }) {
				body += part;
			}
		}
		for (auto index = 1; index <= note; ++index) {
			body += "[^n" + std::to_string(index) + "]: Footnote.\n";
		}
		QElapsedTimer timer;
		timer.start();
		aView.set_body(body, "bounds:1");
		settle();
		const auto elapsed = timer.elapsed();
		std::cout << "github-parity-65536-ms=" << elapsed << " rendered=" << aView.rendered_available() << '\n';
		expect(aView.rendered_available() && body.size() <= 65'536U, "the maximal body is inside the preview budgets");
		auto alerts = 0;
		for (auto block = document_of(aView).begin(); block.isValid(); block = block.next()) {
			alerts += block.blockFormat().boolProperty(title_property) ? 1 : 0;
		}
		expect(alerts == note, "every alert of the maximal body is presented");
		expect(elapsed < 1'000, "a maximal body with every construct renders within the local one-second regression ceiling");
		expect(aView.original_body() == body, "the source is kept");
	}

	/// The character format inside the first occurrence of `aText`.
	auto format_inside(QTextDocument& aDocument, const QString& aText) -> QTextCharFormat
	{
		auto cursor = aDocument.find(aText);
		expect(!cursor.isNull(), "the reader shows: " + aText.toStdString());
		cursor.setPosition(cursor.selectionStart() + 1);
		return cursor.charFormat();
	}

	// [[BPR-1792]]. GitHub renders `$…$` with MathJax, so a template's
	// `$\color{gray}\text{…}$` reads as grey words; the reader shows those
	// words in that colour, and leaves any other formula as written.
	void check_math(qt::markdown_body_view& aView)
	{
		const auto body = fixture(QStringLiteral("math"));
		show(aView, body, "math:1");
		auto&      document = document_of(aView);
		const auto text     = folded_text(document);
		// GitHub's own page: which spans are math.
		const auto formulas = matches(reference(QStringLiteral("math")), QStringLiteral("<math-renderer[^>]*>\\$(.*?)\\$</math-renderer>"));
		expect(formulas.size() == 6 && formulas.contains(QStringLiteral("\\frac{a}{b}")), "GitHub takes six spans for math, the code span and a price not among them");
		const auto note = QStringLiteral("A tidy pull request template. Ping @template-keeper with questions.");
		expect(text.contains(note) && !text.contains(QStringLiteral("\\color{gray}")) && !text.contains(QStringLiteral("@{}")) && !text.contains(QStringLiteral("\\text{notes")), "a coloured text formula reads as its words, the empty group gone");
		expect(format_inside(document, note).foreground().color() == QColor{ QStringLiteral("gray") }, "in the colour it names");
		expect(format_inside(document, QStringLiteral("notes for tools")).foreground().color() == QColor{ QStringLiteral("silver") }, "a summary's formula keeps its colour too");
		const auto boldRed = format_inside(document, QStringLiteral("bold red"));
		expect(boldRed.foreground().color() == QColor{ QStringLiteral("#cc3300") } && boldRed.fontWeight() >= QFont::Bold, "\\textcolor and \\textbf: bold, in the hex colour");
		expect(format_inside(document, QStringLiteral("backtick form")).foreground().color() == QColor{ QStringLiteral("teal") }, "the dollar-backtick form reads the same");
		expect(text.contains(QStringLiteral("Math mode x+1 reads italic")) && format_inside(document, QStringLiteral("x+1")).fontItalic(), "math mode drops spaces and sets a letter italic, as TeX does");
		expect(text.contains(QStringLiteral("$\\frac{a}{b}$ stays as written")), "a formula the reader does not draw stays as written");
		expect(text.contains(QStringLiteral("$\\color{red}\\text{code}$")) && format_inside(document, QStringLiteral("color{red}")).fontFixedPitch(), "math in code stays code");
		expect(text.contains(QStringLiteral("It costs $5 and that is all.")), "a price is not math");
		expect(reserved_marks(document) == 0, "no mark is left in the document");
		expect(aView.original_body() == body, "the source is kept");
		// Negative control: Qt's own import shows the TeX.
		auto control = QTextDocument{ };
		control.setMarkdown(QString::fromStdString(body), QTextDocument::MarkdownFeatures{ QTextDocument::MarkdownDialectGitHub } | QTextDocument::MarkdownNoHTML);
		expect(control.toPlainText().contains(QStringLiteral("\\color{gray}")), "negative control: without the preparation the TeX shows");
	}

	/// Renders each fixture in each theme, for review beside GitHub's page.
	void capture(qt::markdown_body_view& aView)
	{
		const auto root = qEnvironmentVariable("BPR_MARKDOWN_RENDER_DIR");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "create capture directory");
		const auto inherited = aView.palette();
		for (const auto& [mode, name] : themes) {
			aView.setPalette(palette_of(mode));
			for (const auto& page : { QStringLiteral("pr-template"), QStringLiteral("comments"), QStringLiteral("breaks"), QStringLiteral("alerts"), QStringLiteral("footnotes"), QStringLiteral("math") }) {
				aView.resize(720, 400);
				show(aView, fixture(page), (page + QLatin1Char{ ':' } + QString::fromLatin1(name)).toStdString());
				auto* const reader = aView.rendered_view();
				reader->document()->setTextWidth(reader->viewport()->width());
				aView.resize(720, static_cast<int>(reader->document()->size().height()) + (aView.height() - reader->viewport()->height()) + 8);
				settle();
				expect(aView.grab().save(root + QLatin1Char{ '/' } + page + QLatin1Char{ '-' } + QString::fromLatin1(name) + QStringLiteral(".png")), "save capture");
			}
		}
		aView.setPalette(inherited);
		aView.resize(740, 900);
		// In place, as a post in the Comments drawer reads.
		auto        holder = QWidget{ };
		auto* const post   = new qt::markdown_body_view{ &holder };
		post->set_in_place(true);
		holder.show();
		for (const auto& [mode, name] : themes) {
			holder.setPalette(palette_of(mode));
			holder.setAutoFillBackground(true);
			for (const auto& page : { QStringLiteral("comments"), QStringLiteral("breaks"), QStringLiteral("alerts"), QStringLiteral("footnotes") }) {
				post->set_body(fixture(page), (QStringLiteral("in-place:") + page + QLatin1Char{ ':' } + QString::fromLatin1(name)).toStdString());
				const auto height = post->heightForWidth(480);
				holder.resize(496, height + 16);
				post->setGeometry(8, 8, 480, height);
				settle();
				expect(holder.grab().save(root + QStringLiteral("/in-place-") + page + QLatin1Char{ '-' } + QString::fromLatin1(name) + QStringLiteral(".png")), "save in-place capture");
			}
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
	check_comments(view);
	check_breaks(view);
	check_alerts(view);
	check_footnotes(view);
	check_template(view);
	check_math(view);
	check_bounds(view);
	capture(view);
	std::cout << "markdown-github-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
