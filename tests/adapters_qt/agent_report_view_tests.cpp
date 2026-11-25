// RFC-BPR-0128, review of 2026-11-14. A report's dressed page never loads a
// resource: agent-written text with an image, raw HTML and resource links is
// drawn with no image, the page's document denies a load even when asked
// directly for a file that exists, and a link is followed as its text.
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/qt/agent_report_view.hpp"

#include "bpr/core/agent_report.hpp"
#include "bpr/core/agent_report_keep.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/report_page.hpp"

#include "bpr/application/markdown_code_syntax.hpp"

#include <QAbstractScrollArea>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QTextTable>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
	using bpr::tests::expect;

	/// Where on the page's viewport the anchor `aHref` is drawn; none when
	/// the page has no such link.
	[[nodiscard]] auto anchor_point(QTextEdit& aPage, const QString& aHref) -> std::optional<QPoint>
	{
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				if (fragment.fragment().charFormat().anchorHref() == aHref) {
					auto cursor = QTextCursor{ aPage.document() };
					cursor.setPosition(fragment.fragment().position() + 1);
					return aPage.cursorRect(cursor).center();
				}
			}
		}
		return std::nullopt;
	}

	/// The hrefs of every link on the page, in order.
	[[nodiscard]] auto anchors(QTextEdit& aPage) -> QStringList
	{
		auto found = QStringList{ };
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				if (const auto href = fragment.fragment().charFormat().anchorHref(); !href.isEmpty() && !found.contains(href)) {
					found << href;
				}
			}
		}
		return found;
	}

	/// [[BPR-2238]]. How far each code box stands inside the left and the
	/// right of its finding's box, as the page laid that box out.
	struct inset
	{
		int Left  = 0;
		int Right = 0;
	};

	[[nodiscard]] auto code_insets(bpr::qt::agent_report_view& aView) -> std::vector<inset>
	{
		auto  found = std::vector<inset>{ };
		auto* page  = aView.dressed_view();
		for (auto* const code : aView.code_views()) {
			if (!code->isVisible()) {
				continue;
			}
			const auto  box    = code->geometry();
			const auto  cursor = page->cursorForPosition(box.topLeft() + QPoint{ 2, 2 });
			auto* const table  = cursor.currentTable();
			if (table == nullptr) {
				found.push_back({ .Left = -1000, .Right = -1000 });
				continue;
			}
			const auto frame = page->document()->documentLayout()->frameBoundingRect(table).translated(-page->horizontalScrollBar()->value(), -page->verticalScrollBar()->value());
			found.push_back({ .Left = box.left() - static_cast<int>(frame.left()), .Right = static_cast<int>(frame.right()) - (box.left() + box.width()) });
		}
		return found;
	}

	/// The page's layout formats of the block holding `aText`, and where in
	/// that block it starts.
	[[nodiscard]] auto formats_at(QTextEdit& aPage, const QString& aText) -> std::pair<QList<QTextLayout::FormatRange>, int>
	{
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			if (const auto at = block.text().indexOf(aText); at >= 0) {
				return { block.layout() != nullptr ? block.layout()->formats() : QList<QTextLayout::FormatRange>{ }, static_cast<int>(at) };
			}
		}
		return { { }, -1 };
	}

	/// [[BPR-2266]] (owner, 2026-11-18: "Seems like we lost syntax
	/// highlights for inline code on reports"). The page's inline code is
	/// coloured as the Agent tab's answers colour theirs: asked for as the
	/// pull request's language through the same dispatch, coloured when the
	/// spans arrive; a place stays a link.
	void check_inline_code()
	{
		auto view = bpr::qt::agent_report_view{ };
		view.resize(820, 700);
		view.show();
		auto asked = std::vector<bpr::application::markdown_code_syntax_request>{ };
		view.set_dresser([&asked](bpr::qt::markdown_body_view& aView) {
			aView.set_inline_code_selector("code.cpp");
			aView.set_code_syntax_dispatch([&asked](std::vector<bpr::application::markdown_code_syntax_request> aRequests) { asked.insert(asked.end(), aRequests.begin(), aRequests.end()); });
		});
		const auto markdown = QStringLiteral("# Review of #4\n\n**Verdict:** Needs changes\n\nThe summary names `resize_target(width)`.\n\n## Must fix\n\n### issue: A zero height\n\n"
											 "`src/engine/renderer.cpp:12`\n\nThe height from `window_extent()` is used unchecked.\n");
		view.present({ .Turn = 1U, .Title = QStringLiteral("Review of #4"), .Markdown = markdown, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
		QApplication::processEvents();
		expect(view.dressed_shown(), "the report is a page");
		const auto wanted = [&asked](const std::string& aText) {
			return std::ranges::any_of(asked, [&aText](const bpr::application::markdown_code_syntax_request& aRequest) { return aRequest.Selector == "code.cpp" && aRequest.Text && *aRequest.Text == aText; });
		};
		expect(wanted("resize_target(width)") && wanted("window_extent()"), "the report's inline code is asked for as the pull request's language");
		auto* const page = view.dressed_view();
		expect(formats_at(*page, QStringLiteral("window_extent()")).first.isEmpty(), "plain until the spans arrive");
		const auto keyword = [](const std::string& aText, const std::uint32_t aLength) {
			return bpr::application::markdown_code_syntax_publication{
				.Selector = "code.cpp",
				.Text     = std::make_shared<const std::string>(aText),
				.Spans    = std::make_shared<const bpr::application::token_span_list>(bpr::application::token_span_list{ { .Line = 0U, .Column = 0U, .Length = aLength, .Role = bpr::core::appearance_role::syntax_keyword } }),
			};
		};
		view.apply_code_syntax(keyword("window_extent()", 13U));
		view.apply_code_syntax(keyword("resize_target(width)", 13U));
		for (const auto& text : { QStringLiteral("window_extent()"), QStringLiteral("resize_target(width)") }) {
			const auto [formats, at] = formats_at(*page, text);
			expect(at >= 0 && !formats.isEmpty() && std::ranges::all_of(formats, [&](const QTextLayout::FormatRange& aRange) { return aRange.start >= at && aRange.start + aRange.length <= at + text.size(); })
					   && std::ranges::any_of(formats, [&](const QTextLayout::FormatRange& aRange) { return aRange.start == at && aRange.length == 13 && aRange.format.foreground().style() != Qt::NoBrush; }),
				   "the page colours its inline code, and only inside the span: " + text.toStdString());
		}
		expect(formats_at(*page, QStringLiteral("src/engine/renderer.cpp:12")).first.isEmpty(), "a place stays a link, not code to colour");
		// A render again (a keep's state, a width) keeps the colours.
		view.resize(600, 700);
		view.refresh_keeps();
		QApplication::processEvents();
		expect(!formats_at(*view.dressed_view(), QStringLiteral("window_extent()")).first.isEmpty(), "a page drawn again keeps its colours");
	}

	/// [[BPR-2268]] (owner, 2026-11-18: "Draft comment button in a report
	/// doesn't look good when the description text to its right line
	/// wraps"). At a narrow width, with the words beside it on several
	/// lines, the chip keeps the height it has beside one line, its words on
	/// one line, its text level with the words' first line.
	void check_keep_chip()
	{
		auto view = bpr::qt::agent_report_view{ };
		view.show();
		view.set_draft_dispatch([](const bpr::qt::report_draft&, bpr::qt::report_draft_done) { });
		const auto markdown = QStringLiteral("# Review of #5\n\nOne fix.\n\n## Must fix\n\n### issue: A zero height reaches the swap chain\n\n`src/a.cpp:12`\n\nClamp it.\n");
		// The chip's box and its words' line, and the words beside it, as the
		// page laid them out.
		struct measured
		{
			QRectF Box;
			QRectF Chip;
			QRectF Words;
			qreal  Line = 0.0;
		};
		const auto measure = [&view](const int aWidth) {
			view.resize(aWidth, 900);
			QApplication::processEvents();
			auto* const document = view.dressed_view()->document();
			auto* const layout   = document->documentLayout();
			const auto  link     = document->find(QStringLiteral("Draft comment"));
			const auto  words    = document->find(QStringLiteral("at a.cpp:12"), link);
			expect(!link.isNull() && !words.isNull() && link.currentTable() != nullptr, "the finding offers Draft comment beside its words");
			const auto chip = layout->blockBoundingRect(link.block());
			return measured{ .Box   = layout->frameBoundingRect(link.currentTable()),
							 .Chip  = chip,
							 .Words = layout->blockBoundingRect(words.block()),
							 .Line  = link.block().layout()->lineCount() > 0 ? link.block().layout()->lineAt(0).height() : 0.0 };
		};
		view.present({ .Turn = 1U, .Title = QStringLiteral("Review of #5"), .Markdown = markdown, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
		const auto wide = measure(1400);
		expect(wide.Words.height() < 2.0 * wide.Line, "wide, the words take one line");
		// Narrower, the words wrap beside the chip, in two lines at most; and
		// where they would need a third, they stand under it at the row's
		// width. The chip is the same chip throughout.
		auto wrapped = 0;
		auto under   = 0;
		for (auto width = 300; width <= 560; width += 10) {
			const auto narrow = measure(width);
			const auto beside = std::abs(narrow.Chip.top() - narrow.Words.top()) <= 1.0;
			std::cout << "BPR-2268 width " << width << ": chip box " << narrow.Box.height() << " (one line beside: " << wide.Box.height() << "), chip text top " << narrow.Chip.top() << ", words top "
					  << narrow.Words.top() << ", words " << narrow.Words.height() << " high, " << (beside ? "beside" : "under") << "\n";
			expect(narrow.Chip.height() < 1.5 * narrow.Line, "Draft comment stays on one line at " + std::to_string(width));
			expect(narrow.Box.height() == wide.Box.height() && narrow.Box.height() <= narrow.Chip.height() + 12.0, "the chip keeps its natural height, its one line and padding, at " + std::to_string(width));
			if (beside) {
				// Level with the first line beside it, however many it takes.
				expect(narrow.Words.height() < 3.0 * narrow.Line, "beside the chip the words take two lines at most at " + std::to_string(width));
				wrapped += narrow.Words.height() >= 2.0 * narrow.Line ? 1 : 0;
			} else {
				// The chip's padding and border, and room, under its words.
				expect(narrow.Words.top() >= narrow.Chip.bottom() + 5.0 + 4.0, "the words stand clear under the chip at " + std::to_string(width));
				++under;
			}
		}
		expect(wrapped > 0, "at some width the words wrap beside the chip, the chip keeping its height");
		expect(under > 0, "narrower still, the words stand under the chip");
	}

	/// [[BPR-2299]] (owner, 2026-11-19: "The current report width feels too
	/// narrow by default ... Or perhaps just drive it off of the existing
	/// reading width setting"). The page's column stops at the reading
	/// width, not at 572 pixels; a new width reaches the report shown at
	/// once; its code boxes and suggestion take the new column and a long
	/// line still scrolls inside its box, never widening the page.
	void check_reading_width()
	{
		namespace core = bpr::core;
		auto view      = bpr::qt::agent_report_view{ };
		view.resize(3'200, 900);
		view.show();
		const auto longLine = QString{ 3'000, QLatin1Char{ 'w' } };
		const auto markdown = QStringLiteral("# Review of #6\n\n**Verdict:** Needs changes\n\nOne fix.\n\n## Must fix\n\n### issue: A zero height\n\n`src/a.cpp:12`\n\nClamp it.\n\n"
											 "```cpp\nconst auto %1 = 1;\n```\n\n```suggestion\nconst auto %1 = 2;\n```\n")
								  .arg(longLine);
		view.present({ .Turn = 1U, .Title = QStringLiteral("Review of #6"), .Markdown = markdown, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
		const auto settle = [] {
			for (auto pass = 0; pass < 4; ++pass) {
				QApplication::processEvents();
			}
		};
		settle();
		expect(view.dressed_shown() && view.code_views().size() == 2U, "the report is a page with a code box and a suggestion");
		auto* const column = view.column();
		// The room the pane gives the column: the page's width less its
		// margins (24 left, the menu button's 24 + 32 + 14 right).
		const auto room       = view.findChild<QScrollArea*>()->viewport()->width() - 24 - (24 + 32 + 14);
		const auto checkWidth = [&](const std::optional<std::uint32_t> aLimit, const std::string& aName) {
			const auto expected = aLimit.has_value() ? std::min(room, static_cast<int>(*aLimit)) : room;
			const auto at       = std::string{ " at " } + aName;
			std::cout << "BPR-2299" << at << ": column " << column->width() << ", room " << room << ", expected " << expected << "\n";
			expect(view.reading_limit() == aLimit, "the view holds the width it was given" + at);
			expect(column->width() == expected, "the column is the reading width, or the pane's room when that is less" + at + ": " + std::to_string(column->width()));
			expect(std::abs(column->x() + column->width() / 2 - (24 + room / 2)) <= 2, "the column stays centred" + at);
			auto* const page = view.dressed_view();
			expect(page->width() == column->width() && page->horizontalScrollBar()->maximum() == 0, "the page takes the column and never scrolls sideways" + at);
			for (auto* const code : view.code_views()) {
				// A code block scrolls in its box's text, a suggestion's diff in
				// the box's reader: either way inside the box, never the page.
				const auto scrolls = std::ranges::any_of(code->findChildren<QAbstractScrollArea*>(), [&](const QAbstractScrollArea* aArea) {
					return aArea->isVisible() && aArea->horizontalScrollBar()->maximum() > 0 && aArea->width() <= code->width();
				});
				expect(scrolls, "the long line scrolls sideways inside its box" + at);
			}
		};
		// The default is Wide (1100); at 3200 pixels the cap shows.
		expect(view.reading_limit() == core::reading_width_limit(core::default_reading_width), "the view starts at the setting's default");
		expect(room > 1'100, "the pane is wider than Wide's cap");
		checkWidth(core::reading_width_limit(core::default_reading_width), "the default");
		expect(column->width() > 572, "negative control: the old 572-pixel column is gone");
		for (const auto& [limit, name] : { std::pair{ core::reading_width_limit(core::reading_width::narrow), "Narrow" }, std::pair{ core::reading_width_limit(core::reading_width::medium), "Medium" },
										   std::pair{ std::optional<std::uint32_t>{ 1'480U }, "a width of its own" }, std::pair{ std::optional<std::uint32_t>{ }, "Full width" },
										   std::pair{ core::reading_width_limit(core::reading_width::wide), "Wide" } }) {
			// Live: the report already shown, no new present.
			view.set_reading_limit(limit);
			settle();
			checkWidth(limit, name);
		}
	}

	/// The colour the document draws the link reading `aWords` in; none when
	/// the page has no such link.
	[[nodiscard]] auto anchor_colour(QTextEdit& aPage, const QString& aWords) -> std::optional<QColor>
	{
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				if (const auto format = fragment.fragment().charFormat(); format.isAnchor() && !format.anchorHref().isEmpty() && fragment.fragment().text() == aWords) {
					return format.foreground().color();
				}
			}
		}
		return std::nullopt;
	}

	/// What a widget draws, as its pixels' colours.
	[[nodiscard]] auto drawn(QWidget& aWidget) -> QImage { return aWidget.grab().toImage().convertToFormat(QImage::Format_ARGB32); }

	/// Whether any pixel leans to the stand-in platform link (a magenta):
	/// green well under both red and blue, which no blend of a blue and a
	/// ground is.
	[[nodiscard]] auto draws_platform_link(const QImage& aImage) -> bool
	{
		for (auto y = 0; y < aImage.height(); ++y) {
			for (auto x = 0; x < aImage.width(); ++x) {
				const auto pixel = aImage.pixelColor(x, y);
				if (pixel.alpha() > 0 && pixel.red() - pixel.green() > 40 && pixel.blue() - pixel.green() > 40) {
					return true;
				}
			}
		}
		return false;
	}

	/// How near the nearest opaque pixel of `aImage` comes to `aColour`: the
	/// largest difference of a channel.
	[[nodiscard]] auto nearest(const QImage& aImage, const QColor& aColour) -> int
	{
		auto best = 255;
		for (auto y = 0; y < aImage.height(); ++y) {
			for (auto x = 0; x < aImage.width(); ++x) {
				const auto pixel = aImage.pixelColor(x, y);
				if (pixel.alpha() == 255) {
					best = std::min(best, std::max({ std::abs(pixel.red() - aColour.red()), std::abs(pixel.green() - aColour.green()), std::abs(pixel.blue() - aColour.blue()) }));
				}
			}
		}
		return best;
	}

	/// The owner, 2026-11-24: "Why is "Review this pull request" at the top
	/// in the dark blue and the other links are in the more pastel blue? Can
	/// we have the softer blue for the question link too on reports." The
	/// question read the palette's link role, which no theme owns (the
	/// platform's blue); the page writes its places in a blue of its own. In
	/// the built-in Light and Dark, under a platform link role unlike
	/// either, the question, its bubble, the menu's bar and a report drawn
	/// as Markdown wear the colour the page's places wear.
	void check_question_link()
	{
		namespace core = bpr::core;
		namespace qt   = bpr::qt;
		// The role a platform hands the application, as no page would derive.
		const auto platform  = QColor{ 0xFF, 0x00, 0xC8 };
		const auto inherited = QApplication::palette();
		auto       handed    = inherited;
		handed.setColor(QPalette::Link, platform);
		QApplication::setPalette(handed);
		const auto settle = [] {
			for (auto pass = 0; pass < 4; ++pass) {
				QApplication::processEvents();
			}
		};
		{
			// The control: a label left to the palette draws the platform's
			// role, which is what the question did.
			QLabel plain{ QStringLiteral("<a href=\"conversation\" style=\"text-decoration:none\">Review this pull request</a>") };
			plain.setTextFormat(Qt::RichText);
			plain.show();
			settle();
			expect(draws_platform_link(drawn(plain)), "a link left to the palette is drawn in the platform's link role");
		}
		auto view = qt::agent_report_view{ };
		view.resize(820, 700);
		view.show();
		const auto report = QStringLiteral("# Review of #7\n\n**Verdict:** Needs changes\n\nOne fix, and [the tests](tests/a_tests.cpp:4) miss it.\n\n## Must fix\n\n### issue: A zero height\n\n"
										   "`src/engine/renderer.cpp:12`\n\nClamp it.\n");
		view.present({ .Turn = 1U, .Title = QStringLiteral("Review of #7"), .Markdown = report, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
		auto       profile = qt::appearance_profile{ };
		auto       seen    = std::vector<QColor>{ };
		const auto place   = QStringLiteral("src/engine/renderer.cpp:12");
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, std::string{ "Light" } }, std::pair{ core::appearance_mode::dark, std::string{ "Dark" } } }) {
			// The theme arrives as a palette alone: the page shown is drawn
			// again in it, with no new report and no tone told.
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(view);
			settle();
			const auto wanted = qt::to_presentation_color(core::derive_report_shades(profile.palette()).Link);
			const auto ground = view.palette().color(QPalette::Base);
			expect(view.dressed_shown() && (ground.lightness() < 128) == (mode == core::appearance_mode::dark), name + ": the page is shown in the theme");
			expect(wanted != platform && qt::report_link_colour({ .Paper = ground, .Ink = view.palette().color(QPalette::Text) }) == wanted, name + ": the page's link is the core's, off the theme's ground and ink");
			auto* const page = view.dressed_view();
			expect(anchor_colour(*page, place) == std::optional{ wanted }, name + ": a place is written in the page's link colour");
			expect(anchor_colour(*page, QStringLiteral("the tests")) == std::optional{ wanted }, name + ": and so is a link in the summary");
			auto* const byline = view.findChild<QLabel*>(QStringLiteral("agentReportByline"));
			auto* const bubble = view.findChild<QLabel*>(QStringLiteral("agentReportQuestionIcon"));
			auto* const menu   = view.findChild<QWidget*>(QStringLiteral("agentReportOutline"));
			expect(byline != nullptr && bubble != nullptr && menu != nullptr, name + ": the question, its bubble and the menu");
			expect(byline->text().contains(QStringLiteral("Review this pull request")) && byline->text().contains(QStringLiteral("color:") + wanted.name()), name + ": the question is written in the page's link colour");
			const auto question = drawn(*byline);
			const auto near     = nearest(question, wanted);
			std::cout << "report-question-link " << name << ": page link " << wanted.name().toStdString() << ", nearest pixel of the question drawn within "
					  << near << ", platform role " << platform.name().toStdString() << "\n";
			expect(!draws_platform_link(question), name + ": no pixel of the question is the platform's link role");
			expect(near <= 24, name + ": the question is drawn in the page's link colour, within " + std::to_string(near));
			const auto glyph = bubble->pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
			expect(!glyph.isNull() && nearest(glyph, wanted) <= 2 && !draws_platform_link(glyph), name + ": the bubble before it is the same colour");
			expect(menu->palette().color(QPalette::Active, QPalette::Link) == wanted, name + ": the menu marks the current section in it");
			seen.push_back(wanted);
		}
		expect(seen.size() == 2U && seen[0] != seen[1], "Light and Dark each have their own");
		// A report that does not keep to the template is drawn as Markdown:
		// its links and the question still agree, in the theme left on (Dark).
		view.present({ .Turn = 2U, .Title = QStringLiteral("Notes"), .Markdown = QStringLiteral("# Notes\n\nSee [the clamp](src/engine/renderer.cpp:12) first.\n"), .Question = QStringLiteral("Review this pull request"), .Byline = { } });
		settle();
		auto* const body = dynamic_cast<qt::markdown_body_view*>(view.findChild<QWidget*>(QStringLiteral("agentReportBody")));
		expect(!view.dressed_shown() && body != nullptr && body->rendered_view() != nullptr, "a report with no sections is drawn as Markdown");
		expect(anchor_colour(*body->rendered_view(), QStringLiteral("the clamp")) == std::optional{ seen.back() }, "its link is the page's link colour");
		expect(view.findChild<QLabel*>(QStringLiteral("agentReportByline"))->text().contains(QStringLiteral("color:") + seen.back().name()) && nearest(drawn(*view.findChild<QLabel*>(QStringLiteral("agentReportByline"))), seen.back()) <= 24, "and so is the question over it");
		QApplication::setPalette(inherited);
	}

	void click(QTextEdit& aPage, const QPoint aAt)
	{
		QMouseEvent press{ QEvent::MouseButtonPress, QPointF{ aAt }, aPage.viewport()->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aPage.viewport(), &press);
		QMouseEvent release{ QEvent::MouseButtonRelease, QPointF{ aAt }, aPage.viewport()->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aPage.viewport(), &release);
		QApplication::processEvents();
	}

	/// The middle of the link whose words are `aWords` (the `aNth` such
	/// link), on the page's viewport; none when the page has no such link.
	/// `aLeft`: where its words start instead.
	[[nodiscard]] auto link_point(QTextEdit& aPage, const QString& aWords, const int aNth = 0, const bool aLeft = false) -> std::optional<QPoint>
	{
		auto seen = 0;
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				const auto part = fragment.fragment();
				// A chip's words are kept on one line with no-break spaces.
				if (part.charFormat().anchorHref().isEmpty() || part.text().replace(QChar::Nbsp, QLatin1Char{ ' ' }) != aWords || seen++ != aNth) {
					continue;
				}
				auto cursor = QTextCursor{ aPage.document() };
				cursor.setPosition(part.position());
				const auto first = aPage.cursorRect(cursor);
				cursor.setPosition(part.position() + part.length());
				return QPoint{ aLeft ? first.left() : (first.left() + aPage.cursorRect(cursor).left()) / 2, first.center().y() };
			}
		}
		return std::nullopt;
	}

	/// The first pixel from `aFrom` along `aStep` that is not the colour at
	/// `aFrom`: the edge of the face `aFrom` stands on. In device pixels.
	[[nodiscard]] auto face_edge(const QImage& aImage, const QPoint aFrom, const QPoint aStep) -> QPoint
	{
		auto at = aFrom;
		while (aImage.rect().contains(at + aStep) && aImage.pixel(at) == aImage.pixel(aFrom)) {
			at += aStep;
		}
		return at;
	}

	/// How far a pixel is from a colour, over its three channels.
	[[nodiscard]] auto colour_gap(const QColor& aOne, const QColor& aOther) -> int
	{
		return std::abs(aOne.red() - aOther.red()) + std::abs(aOne.green() - aOther.green()) + std::abs(aOne.blue() - aOther.blue());
	}

	/// The owner, 2026-11-25, of a report finding: "When I click draft
	/// comment it sends me to go to draft with this button text selected and
	/// then I cant actually click go to draft properly until I deselect it
	/// and then click it again." Clicks and keys go in as the window system
	/// reports them, so the page takes focus as it does under a hand.
	/// A link takes one click whatever the page has selected; the keyboard's
	/// link is a focus ring, never a selection; and only a keep action made
	/// with Return hands the keyboard on.
	void check_a_link_takes_one_click()
	{
		auto view = bpr::qt::agent_report_view{ };
		view.resize(900, 700);
		view.show();
		// Only an active window's focus widget has focus, and the native look
		// keeps its windows off screen, where none is made active.
		QT_WARNING_PUSH
		QT_WARNING_DISABLE_DEPRECATED
		QApplication::setActiveWindow(&view);
		QT_WARNING_POP
		QApplication::processEvents();
		auto drafts = 0;
		auto gone   = 0;
		auto asked  = 0;
		auto places = QStringList{ };
		view.set_draft_dispatch([&drafts](const bpr::qt::report_draft&, const bpr::qt::report_draft_done& aDone) {
			++drafts;
			aDone(true, { });
		});
		view.set_go_dispatch([&gone](const bpr::qt::report_draft&) { ++gone; });
		view.set_follow_up_dispatch([&asked](const bpr::qt::report_draft&) { ++asked; });
		view.set_link_dispatch([&places](const QString& aLink) { places << aLink; });
		view.present({ .Turn     = 1U,
					   .Title    = QStringLiteral("Review of #5"),
					   .Markdown = QStringLiteral("# Review of #5\n\nTwo fixes.\n\n## Must fix\n\n### issue: A zero height reaches the swap chain\n\n`src/a.cpp:12`\n\nClamp it.\n\n"
												  "### issue: The width is read twice\n\n`src/b.cpp:3`\n\nRead it once.\n\n### nitpick: The README is stale\n\n`README.md`\n\nIt names the old folder.\n"),
					   .Question = QStringLiteral("Review this pull request"),
					   .Byline   = { } });
		QApplication::processEvents();
		auto* const page = view.dressed_view();
		expect(view.findings().size() == 3U && view.findings()[2].Keep == bpr::core::report_keep::keep_local, "the report has two findings to draft and one to keep local");
		const auto first  = view.findings()[0].Outline;
		const auto second = view.findings()[1].Outline;
		const auto press  = [page](const QString& aWords) {
			const auto at = link_point(*page, aWords);
			expect(at.has_value(), "the page offers " + aWords.toStdString());
			bpr::tests::window_system_clicks(*page->viewport(), QPointF{ *at }, 1);
		};
		const auto selected = [page] { return page->textCursor().selectedText().toStdString(); };

		// The pointer. One click on Draft comment drafts; Go to draft stands
		// in its place, not selected and not the keyboard's; one click goes.
		press(QStringLiteral("Draft comment"));
		expect(drafts == 1 && view.keep_state(first).Phase == bpr::qt::report_keep_phase::kept, "one click on Draft comment drafts the finding");
		expect(page->hasFocus(), "the click leaves the keyboard with the page");
		expect(!page->textCursor().hasSelection(), "after Draft comment nothing on the page is selected; \"" + selected() + "\" is");
		expect(view.keyboard_link().isEmpty(), "a keep action made with the pointer hands the keyboard to no link; it is on " + view.keyboard_link().toStdString());
		const auto ringless = page->viewport()->grab().toImage();
		press(QStringLiteral("Go to draft"));
		expect(gone == 1, "one click on Go to draft goes to the draft: went " + std::to_string(gone) + " times");
		expect(!page->textCursor().hasSelection() && view.keyboard_link().isEmpty(), "the click selects nothing");

		// The owner's screen, as the page used to make it: Go to draft's own
		// words selected. The owner: "It's worse than that actually, clicking
		// on it does nothing, you had to click away first before you were
		// allowed to click." The first click on it goes, with no click
		// anywhere else before it.
		for (auto block = page->document()->begin(); block.isValid(); block = block.next()) {
			for (auto run = block.begin(); !run.atEnd(); ++run) {
				if (const auto part = run.fragment(); !part.charFormat().anchorHref().isEmpty() && part.text().replace(QChar::Nbsp, QLatin1Char{ ' ' }) == QStringLiteral("Go to draft")) {
					auto words = QTextCursor{ page->document() };
					words.setPosition(part.position());
					words.setPosition(part.position() + part.length(), QTextCursor::KeepAnchor);
					page->setTextCursor(words);
				}
			}
		}
		expect(page->textCursor().selectedText().replace(QChar::Nbsp, QLatin1Char{ ' ' }) == QStringLiteral("Go to draft"), "Go to draft's words are selected");
		press(QStringLiteral("Go to draft"));
		expect(gone == 2, "with its own words selected, the first click on Go to draft goes: went " + std::to_string(gone) + " times");
		press(QStringLiteral("Go to draft"));
		expect(gone == 3, "and so does the next: went " + std::to_string(gone) + " times");
		// Whatever the page has selected, a link takes one click, and the
		// click leaves the selection as it was.
		page->selectAll();
		const auto whole = selected();
		expect(!whole.empty(), "the whole page is selected");
		press(QStringLiteral("Go to draft"));
		expect(gone == 4, "with the page selected, one click on Go to draft still goes: went " + std::to_string(gone) + " times");
		press(QStringLiteral("Ask follow-up"));
		expect(asked == 1, "and one click on Ask follow-up asks");
		press(QStringLiteral("src/a.cpp:12"));
		expect(places.size() == 1, "and one click on a place follows it");
		expect(selected() == whole, "a click on a link changes no selection");

		// A press that leaves its link follows nothing and selects nothing.
		auto caret = page->textCursor();
		caret.movePosition(QTextCursor::Start);
		page->setTextCursor(caret);
		const auto chip = *link_point(*page, QStringLiteral("Go to draft"));
		bpr::tests::window_system_mouse(*page->viewport(), QPointF{ chip }, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
		bpr::tests::window_system_mouse(*page->viewport(), QPointF{ chip } + QPointF{ 0.0, -60.0 }, QEvent::MouseMove, Qt::LeftButton);
		bpr::tests::window_system_mouse(*page->viewport(), QPointF{ chip } + QPointF{ 0.0, -60.0 }, QEvent::MouseButtonRelease, Qt::NoButton);
		expect(gone == 4, "a press dragged off Go to draft does not go");
		expect(!page->textCursor().hasSelection(), "and it drags no selection out of the chip; \"" + selected() + "\" is selected");

		// The keyboard. Tab reaches the second finding's Draft comment: it
		// wears the window's focus ring and nothing is selected.
		for (auto presses = 0; presses < 24 && (view.keyboard_link() != QStringLiteral("Draft comment")); ++presses) {
			bpr::tests::window_system_key(*page, Qt::Key_Tab);
			expect(!page->textCursor().hasSelection(), "Tab selects no text; \"" + selected() + "\" is selected");
		}
		expect(view.keyboard_link() == QStringLiteral("Draft comment") && page->hasFocus(), "Tab reaches the second finding's Draft comment; the keyboard is on \"" + view.keyboard_link().toStdString() + "\"");
		expect(view.last_announcement() == QStringLiteral("Draft comment"), "a screen reader hears the link Tab reached: " + view.last_announcement().toStdString());
		// Return on it drafts, and the keyboard goes on to Go to draft, in
		// its ring: the chip's edge is the ring's colour, which it was not
		// after the pointer's Draft comment.
		bpr::tests::window_system_key(*page, Qt::Key_Return);
		expect(drafts == 2 && view.keep_state(second).Phase == bpr::qt::report_keep_phase::kept, "Return on Draft comment drafts the second finding");
		expect(view.keyboard_link() == QStringLiteral("Go to draft") && page->hasFocus(), "made with Return, the keyboard goes on to Go to draft; it is on \"" + view.keyboard_link().toStdString() + "\"");
		expect(!page->textCursor().hasSelection(), "and Go to draft is not selected; \"" + selected() + "\" is");
		const auto ringed = page->viewport()->grab().toImage();
		// A render (a keep's state read back) keeps the keyboard where it is.
		view.refresh_keeps();
		QApplication::processEvents();
		expect(view.keyboard_link() == QStringLiteral("Go to draft") && !page->textCursor().hasSelection(), "a render keeps the keyboard on its link");
		// The ring is the page's while it holds the keyboard only.
		page->clearFocus();
		QApplication::processEvents();
		const auto plain = page->viewport()->grab().toImage();
		page->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		// The ring is the chip's own edge. The chip's face (its wash, in
		// the room before its words) ends at its border on every side; with
		// the keyboard on the chip that border is the ring's colour, and the
		// pixel beyond it is as it was: the chip keeps its footprint. The
		// first finding's Go to draft, which the keyboard is not on, and
		// either chip without the keyboard wear none.
		const auto ring  = bpr::qt::focus_ring_colour(page->palette(), QPalette::Base);
		const auto scale = plain.devicePixelRatio();
		const auto face  = [page, scale](const int aNth) { return (QPointF{ *link_point(*page, QStringLiteral("Go to draft"), aNth, true) } * scale).toPoint() - QPoint{ qRound(3.0 * scale), 0 }; };
		for (const auto& step : { QPoint{ -1, 0 }, QPoint{ 1, 0 }, QPoint{ 0, -1 }, QPoint{ 0, 1 } }) {
			// Rightwards the face is crossed by the words: start past them.
			const auto from = step == QPoint{ 1, 0 } ? face(1) + QPoint{ 2 * ((QPointF{ *link_point(*page, QStringLiteral("Go to draft"), 1) } * scale).toPoint().x() - face(1).x()), 0 } : face(1);
			const auto edge = face_edge(plain, from, step);
			const auto said = " (step " + std::to_string(step.x()) + "," + std::to_string(step.y()) + ", edge " + std::to_string(edge.x()) + "," + std::to_string(edge.y()) + ", plain " + plain.pixelColor(edge).name().toStdString()
				+ ", ringed " + ringed.pixelColor(edge).name().toStdString() + ", ring " + ring.name().toStdString() + ")";
			std::cout << "report-link-ring:" << said << "\n";
			expect(plain.pixel(from) == plain.pixel(face(1)) && colour_gap(plain.pixelColor(edge), ring) > 60, "without the keyboard the chip's border is its own" + said);
			expect(colour_gap(ringed.pixelColor(edge), ring) <= 12, "the keyboard's Go to draft wears the window's focus ring on its border" + said);
			// The border is one logical pixel: a whole one beyond its inner
			// edge is outside the chip at any scale, a fractional one too.
			const auto beyond = edge + step * static_cast<int>(std::ceil(scale));
			expect(ringed.pixel(beyond) == plain.pixel(beyond), "and the ring stays inside the chip's edge" + said);
			const auto other = face_edge(ringed, face(0), step == QPoint{ 1, 0 } ? QPoint{ -1, 0 } : step);
			expect(colour_gap(ringed.pixelColor(other), ring) > 60 && colour_gap(ringless.pixelColor(other), ring) > 60, "a chip the keyboard is not on, and Go to draft after the pointer's Draft comment, wear none" + said);
		}
		// Return follows the keyboard's link, and one click on it follows it
		// too, ring or no ring; the pointer's press ends the ring.
		bpr::tests::window_system_key(*page, Qt::Key_Return);
		expect(gone == 5, "Return on Go to draft goes: went " + std::to_string(gone) + " times");
		bpr::tests::window_system_clicks(*page->viewport(), QPointF{ *link_point(*page, QStringLiteral("Go to draft"), 1) }, 1);
		expect(gone == 6, "one click on the keyboard's Go to draft goes: went " + std::to_string(gone) + " times");
		expect(view.keyboard_link().isEmpty() && !page->textCursor().hasSelection(), "a press of the pointer takes the keyboard off its link and selects nothing");

		// The page's other actions are the same links. Keep local leaves Go
		// to comment unselected, and one click on it goes; Update draft
		// takes one click with the page selected.
		press(QStringLiteral("Keep local"));
		expect(drafts == 3 && view.keep_state(view.findings()[2].Outline).Phase == bpr::qt::report_keep_phase::kept, "one click on Keep local keeps the finding");
		expect(!page->textCursor().hasSelection() && view.keyboard_link().isEmpty(), "after Keep local nothing on the page is selected; \"" + selected() + "\" is");
		press(QStringLiteral("Go to comment"));
		expect(gone == 7, "one click on Go to comment goes: went " + std::to_string(gone) + " times");
		auto updated = 0;
		view.set_revision_source([](const bpr::qt::report_draft& aDraft) { return aDraft.Path == QStringLiteral("src/a.cpp") ? std::optional{ QStringLiteral("issue: clamp both.") } : std::nullopt; });
		view.set_draft_update([](const bpr::qt::report_draft&) { return true; }, [&updated](const bpr::qt::report_draft&) { ++updated; });
		QApplication::processEvents();
		page->selectAll();
		press(QStringLiteral("Update draft"));
		expect(updated == 1, "with the page selected, one click on Update draft updates: " + std::to_string(updated) + " times");
	}

	/// The owner's report, narrowed by its side chat: the keep row's
	/// sentence ("at renderer.cpp:11. Nothing is published") stood a letter
	/// to a line beside its three chips. The page arranges each row for the
	/// width it is drawn at. At every width a report can be dragged to: no
	/// row is wider than the page, every chip keeps its words on one line,
	/// and every sentence breaks only between words. Wide, a row is one line
	/// as it was; narrow, its sentence stands under its chips and the chips
	/// take the lines they need.
	void check_keep_rows_fit_every_width()
	{
		auto view = bpr::qt::agent_report_view{ };
		view.set_reading_limit(std::nullopt);
		view.resize(1'300, 900);
		view.show();
		auto records = std::map<std::pair<bpr::core::agent_findings_digest, std::uint32_t>, bpr::core::agent_finding_card_record>{ };
		view.set_keep_records([&records](const bpr::core::report_finding_key& aKey) {
			const auto found = records.find({ aKey.Findings, aKey.Ordinal });
			return found == records.end() ? bpr::core::agent_finding_card_record{ } : found->second;
		});
		view.set_draft_dispatch([&records](const bpr::qt::report_draft& aDraft, const bpr::qt::report_draft_done& aDone) {
			auto record                                          = bpr::core::agent_finding_card_record{ };
			record.Findings                                      = aDraft.Key.Findings;
			record.Ordinal                                       = aDraft.Key.Ordinal;
			record.State                                         = aDraft.Keep == bpr::core::report_keep::draft_comment ? bpr::core::agent_finding_card_disposition::drafted : bpr::core::agent_finding_card_disposition::noted;
			record.Note                                          = 7U;
			records[{ aDraft.Key.Findings, aDraft.Key.Ordinal }] = record;
			aDone(true, { });
		});
		view.set_go_dispatch([](const bpr::qt::report_draft&) { });
		view.set_follow_up_dispatch([](const bpr::qt::report_draft&) { });
		view.set_dismiss_dispatch([&records](const bpr::qt::report_draft& aFinding, const bool aDismiss) {
			auto record                                              = bpr::core::agent_finding_card_record{ };
			record.Findings                                          = aFinding.Key.Findings;
			record.Ordinal                                           = aFinding.Key.Ordinal;
			record.State                                             = aDismiss ? bpr::core::agent_finding_card_disposition::dismissed : bpr::core::agent_finding_card_disposition::open;
			records[{ aFinding.Key.Findings, aFinding.Key.Ordinal }] = record;
		});
		view.set_draft_all_dispatch([](std::vector<bpr::qt::report_draft>, const bpr::core::draft_all_outcome&, bpr::qt::report_draft_all_done) { });
		view.set_placeable([](const bpr::qt::report_draft&) { return true; });
		const auto differs = QStringLiteral("src/engine/shaders/post/tonemap.frag");
		view.set_revision_source([&differs](const bpr::qt::report_draft& aDraft) { return aDraft.Path == differs ? std::optional{ QStringLiteral("issue: read the exposure.") } : std::nullopt; });
		view.set_draft_update([&differs](const bpr::qt::report_draft& aDraft) { return aDraft.Path == differs; }, [](const bpr::qt::report_draft&) { });
		view.present({ .Turn     = 1U,
					   .Title    = QStringLiteral("Review of #5"),
					   .Markdown = QStringLiteral("# Review of #5\n\n**Verdict:** Needs changes\n\nThe move is clean. One thing must be fixed before this merges.\n\n## Must fix\n\n"
												  "### issue (blocking): A zero height still asserts\n\n`src/engine/renderer.cpp:12-13`\n\nClamp it.\n\n"
												  "### issue: The width is read twice\n\n`src/engine/renderer.cpp:11`\n\nRead it once.\n\n"
												  "### issue: The pass ignores exposure\n\n`src/engine/shaders/post/tonemap.frag:5`\n\nIt writes the scene straight through.\n\n"
												  "### nitpick: The README is stale\n\n`README.md`\n\nIt names the old folder.\n\n"
												  "### question: Is the bracket right?\n\n`src/engine/renderer.cpp:3`\n\nBegin, draw, present.\n"),
					   .Question = QStringLiteral("Review this pull request"),
					   .Byline   = { } });
		QApplication::processEvents();
		expect(view.findings().size() == 5U && view.findings()[3].Keep == bpr::core::report_keep::keep_local, "the report has four findings to draft and one to keep local");
		// One row of each kind: offered, drafted, drafted with a draft that
		// differs, kept local and dismissed; and the summary's and Draft
		// all's.
		expect(view.press(view.findings()[1].Outline, bpr::qt::report_link::keep) && view.press(view.findings()[2].Outline, bpr::qt::report_link::keep) && view.press(view.findings()[3].Outline, bpr::qt::report_link::keep)
				   && view.press(view.findings()[4].Outline, bpr::qt::report_link::dismiss),
			   "the findings take their states");
		QApplication::processEvents();
		auto* const page     = view.dressed_view();
		const auto  expected = QStringList{
			QStringLiteral("as a local comment on the pull request. Nothing is published"),
			QStringLiteral("Each finding not yet drafted, at its lines. Nothing is published"),
			QStringLiteral("at renderer.cpp:12-13. Nothing is published"),
			QStringLiteral("Nothing is published"),
			QStringLiteral("Drafted. The draft differs from the updated suggested comment. Nothing is published"),
			QStringLiteral("Nothing was made"),
		};
		const auto chips = QStringList{ QStringLiteral("Keep summary"), QStringLiteral("Draft all (1)"), QStringLiteral("Draft comment"), QStringLiteral("Dismiss"), QStringLiteral("Ask follow-up"),
										QStringLiteral("Go to draft"), QStringLiteral("Update draft"), QStringLiteral("Go to comment"), QStringLiteral("Undo") };
		// How the first finding's row is arranged: where its sentence and
		// its chips stand.
		struct arrangement
		{
			int  ChipLines     = 0;
			int  SentenceLines = 0;
			bool Beside        = false;
		};
		const auto judge = [&](const int aWidth) {
			view.resize(aWidth, 900);
			for (auto pass = 0; pass < 4; ++pass) {
				QApplication::processEvents();
			}
			const auto  at       = " at " + std::to_string(aWidth) + " (the page " + std::to_string(view.column()->width()) + ")";
			auto* const document = page->document();
			auto* const layout   = document->documentLayout();
			const auto  width    = static_cast<qreal>(view.column()->width());
			expect(page->width() == view.column()->width() && page->horizontalScrollBar()->maximum() == 0, "the page takes its column and has no sideways range" + at);
			// Every sentence of a row breaks between words, never in one.
			auto found = QStringList{ };
			auto lines = std::map<QString, int>{ };
			for (auto block = document->begin(); block.isValid(); block = block.next()) {
				const auto text = block.text();
				if (!expected.contains(text)) {
					continue;
				}
				found << text;
				const auto* const laid = block.layout();
				lines[text]            = laid->lineCount();
				for (auto index = 0; index + 1 < laid->lineCount(); ++index) {
					const auto line = laid->lineAt(index);
					const auto last = text.at(line.textStart() + line.textLength() - 1);
					expect(last.isSpace(), "\"" + text.toStdString() + "\" breaks between words" + at + ": line " + std::to_string(index + 1) + " is \"" + text.mid(line.textStart(), line.textLength()).toStdString() + "\"");
				}
				expect(layout->blockBoundingRect(block).right() <= width + 0.5, "the sentence stays in the page" + at);
			}
			for (const auto& sentence : expected) {
				expect(found.contains(sentence), "the page says \"" + sentence.toStdString() + "\"" + at + "; it says:\n" + page->toPlainText().toStdString());
			}
			// Every chip keeps its words on one line, inside the page.
			auto seen = QStringList{ };
			for (auto block = document->begin(); block.isValid(); block = block.next()) {
				for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
					const auto part  = fragment.fragment();
					const auto words = part.text().replace(QChar::Nbsp, QLatin1Char{ ' ' });
					if (part.charFormat().anchorHref().isEmpty() || !chips.contains(words)) {
						continue;
					}
					seen << words;
					expect(block.layout()->lineCount() == 1 && block.text().replace(QChar::Nbsp, QLatin1Char{ ' ' }) == words, words.toStdString() + " keeps its words on one line" + at);
					// Its border and padding stand outside its words.
					expect(layout->blockBoundingRect(block).right() + 11.0 <= width + 0.5, words.toStdString() + " is inside the page" + at + ": it reaches " + std::to_string(layout->blockBoundingRect(block).right() + 11.0));
				}
			}
			for (const auto& chip : chips) {
				expect(seen.contains(chip), "the page offers " + chip.toStdString() + at);
			}
			// The first finding's row: Draft comment, Dismiss, Ask follow-up
			// and where it goes.
			auto       shape    = arrangement{ };
			const auto first    = document->find(QStringLiteral("Draft comment"));
			const auto sentence = document->find(QStringLiteral("at renderer.cpp:12-13"), first);
			expect(!first.isNull() && !sentence.isNull(), "the first finding offers Draft comment and says where" + at);
			const auto draftTop = layout->blockBoundingRect(first.block()).top();
			const auto saidTop  = layout->blockBoundingRect(sentence.block()).top();
			auto       tops     = std::vector<qreal>{ draftTop };
			for (const auto& words : { QStringLiteral("Dismiss"), QStringLiteral("Ask follow-up") }) {
				const auto chip = document->find(words, first);
				expect(!chip.isNull() && chip.position() < sentence.position(), "its chips keep their order before its sentence" + at);
				if (const auto top = layout->blockBoundingRect(chip.block()).top(); std::abs(top - tops.back()) > 1.0) {
					tops.push_back(top);
				}
			}
			shape.ChipLines     = static_cast<int>(tops.size());
			shape.SentenceLines = sentence.block().layout()->lineCount();
			shape.Beside        = std::abs(saidTop - tops.back()) <= 1.0;
			expect(shape.Beside || saidTop > tops.back() + 12.0, "its sentence is level with its chips or clear under them" + at);
			std::cout << "keep row" << at << ": chips on " << shape.ChipLines << (shape.ChipLines == 1 ? " line" : " lines") << ", the sentence " << (shape.Beside ? "beside" : "under") << " on " << shape.SentenceLines
					  << (shape.SentenceLines == 1 ? " line" : " lines") << "\n";
			return shape;
		};
		// BPR_REPORT_KEEP_ROW_RENDER_ROOT: the page as it is drawn at four
		// widths, saved there before it is judged.
		if (const auto renders = qEnvironmentVariable("BPR_REPORT_KEEP_ROW_RENDER_ROOT"); !renders.isEmpty()) {
			for (const auto width : { 1'100, 660, 520, 420, 330 }) {
				view.resize(width, 900);
				for (auto pass = 0; pass < 4; ++pass) {
					QApplication::processEvents();
				}
				static_cast<void>(page->grab().save(QStringLiteral("%1/keep-rows-%2.png").arg(renders).arg(view.column()->width())));
			}
		}
		const auto wide = judge(1'300);
		expect(wide.ChipLines == 1 && wide.Beside && wide.SentenceLines == 1, "wide, a row is one line: its chips and its sentence beside them");
		const auto plain = page->toPlainText();
		auto       under = false;
		auto       stack = false;
		for (const auto width : { 1'100, 980, 900, 840, 780, 720, 680, 640, 600, 570, 540, 510, 480, 450, 420, 400, 380, 360, 340, 320, 300, 280 }) {
			const auto shape = judge(width);
			// Beside, a sentence takes two lines at most: a third makes a
			// thin column taller than the chips it explains.
			expect(!shape.Beside || shape.SentenceLines <= 2, "beside its chips a sentence takes two lines at most at " + std::to_string(width));
			expect(shape.Beside || shape.SentenceLines <= 2 || width < 300, "under its chips the sentence has the row's width at " + std::to_string(width));
			under = under || !shape.Beside;
			stack = stack || shape.ChipLines > 1;
		}
		expect(under && stack, "narrow, the sentence stands under its chips and the chips take a second line");
		// Wide again, the row is the one line it was.
		const auto again = judge(1'300);
		expect(again.ChipLines == 1 && again.Beside && again.SentenceLines == 1 && page->toPlainText() == plain, "wide again, the page is as it was");
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication application{ aArgc, aArgv };
	// A folder made without a mode inherits the temporary folder's entries:
	// QTemporaryDir's owner-only mode gives a Windows AppContainer a folder
	// it may not write in ("Access is denied").
	struct scratch_folder
	{
		QString Path = QDir::temp().filePath(QStringLiteral("bpr-agent-report-view-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
		scratch_folder() { static_cast<void>(QDir{ }.mkpath(Path)); }
		~scratch_folder() { static_cast<void>(QDir{ Path }.removeRecursively()); }
		[[nodiscard]] auto isValid() const -> bool { return QDir{ Path }.exists(); }
		[[nodiscard]] auto path() const -> const QString& { return Path; }
		[[nodiscard]] auto filePath(const QString& aName) const -> QString { return QDir{ Path }.filePath(aName); }
	} folder;
	expect(folder.isValid(), "a scratch folder");
	const auto picture = folder.filePath(QStringLiteral("secret.png"));
	{
		QFile      file{ picture };
		const auto written = file.open(QIODevice::WriteOnly) && file.write("PICTURE-BYTES") == 13;
		if (!written) {
			std::cerr << "agent-report-view: " << folder.path().toStdString() << " valid=" << folder.isValid() << " error=" << file.errorString().toStdString() << '\n';
		}
		expect(written, "a file a resource could name");
	}
	const auto address = QUrl::fromLocalFile(picture).toString();
	auto       view    = bpr::qt::agent_report_view{ };
	view.resize(900, 700);
	view.show();
	// [[BPR-2219]]. With BPR_REPORT_RENDER_MARKDOWN and BPR_REPORT_RENDER_OUT
	// set, this only draws that report (a saved answer) as the page shows it
	// and saves the whole page: the renders an owner's look carries.
	if (const auto source = qEnvironmentVariable("BPR_REPORT_RENDER_MARKDOWN"); !source.isEmpty()) {
		QFile file{ source };
		expect(file.open(QIODevice::ReadOnly), "the report to draw opens");
		// [[BPR-2268]]: BPR_REPORT_RENDER_WIDTH draws it at another width.
		view.resize(qEnvironmentVariableIntValue("BPR_REPORT_RENDER_WIDTH") > 0 ? qEnvironmentVariableIntValue("BPR_REPORT_RENDER_WIDTH") : 820, 900);
		// The design round's renders: which look, and the dark theme's
		// verdict colours when the palette is dark.
		const auto look = qEnvironmentVariable("BPR_REPORT_RENDER_LOOK");
		view.set_look(look == QStringLiteral("band") ? bpr::qt::report_look::band : look == QStringLiteral("plain") ? bpr::qt::report_look::plain
																													: bpr::qt::report_look::cards);
		// BPR_REPORT_RENDER_SCHEME (light or dark) draws it in that built-in
		// theme, as the application applies it. The offscreen platform has
		// no theme of its own, so BPR_REPORT_RENDER_PLATFORM_LINK stands in
		// for the link role a platform hands the application, which no
		// theme owns (macOS: #0068DA light, #419CFF dark).
		if (const auto inherited = QColor{ qEnvironmentVariable("BPR_REPORT_RENDER_PLATFORM_LINK") }; inherited.isValid()) {
			auto platform = QApplication::palette();
			platform.setColor(QPalette::Link, inherited);
			QApplication::setPalette(platform);
		}
		auto profile = bpr::qt::appearance_profile{ };
		if (const auto scheme = qEnvironmentVariable("BPR_REPORT_RENDER_SCHEME"); !scheme.isEmpty()) {
			static_cast<void>(profile.select_mode(scheme == QStringLiteral("dark") ? bpr::core::appearance_mode::dark : bpr::core::appearance_mode::light));
			profile.apply_to(view);
			view.set_tone_colours(bpr::qt::tone_colours_of(profile));
		} else if (view.palette().color(QPalette::Base).lightness() < 128) {
			view.set_tone_colours(bpr::qt::builtin_tone_colours(true));
		}
		const auto markdown = QString::fromUtf8(file.readAll());
		view.present({ .Turn     = 1U,
					   .Title    = QString::fromStdString(bpr::core::report_card_of(bpr::core::read_report_answer(markdown.toStdString()).Report).Title),
					   .Markdown = markdown,
					   .Question = QStringLiteral("Review this pull request"),
					   .Byline   = { .Ending = qEnvironmentVariable("BPR_REPORT_RENDER_BYLINE"), .When = { }, .Head = { } } });
		QElapsedTimer settled;
		settled.start();
		while (settled.elapsed() < 1500) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		auto* const page = view.findChild<QScrollArea*>();
		expect(page != nullptr && page->widget() != nullptr, "the page to save");
		// The page without the room kept under a short report.
		auto       drawn  = page->widget()->grab();
		const auto bottom = view.dressed_shown() ? view.dressed_view()->geometry().bottom() + 40 : drawn.height();
		expect(drawn.copy(0, 0, drawn.width(), std::min(drawn.height(), static_cast<int>(bottom * drawn.devicePixelRatio()))).save(qEnvironmentVariable("BPR_REPORT_RENDER_OUT")), "the render is saved");
		std::cout << "AGENT_REPORT_VIEW_RENDERED\n";
		return EXIT_SUCCESS;
	}
	auto followed = QStringList{ };
	view.set_link_dispatch([&followed](const QString& aLink) { followed << aLink; });
	const auto markdown = QStringLiteral("# Was this addressed?\n\nOne thread. ![diagram](%1) <img src=\"%1\"> [file](%1)\n\n## Partly\n\n### The clamp\n\n`src/engine/renderer.cpp:12`\n\n"
										 "The width is clamped. ![inline](%1)\n\n<img src=\"%1\" width=\"40\">\n\n- `src/a.cpp:3` A row with ![x](%1).\n")
							  .arg(address);
	view.present({ .Turn = 1U, .Title = QStringLiteral("Was this addressed?"), .Markdown = markdown, .Question = QStringLiteral("Was this addressed?"), .Byline = { .Ending = QStringLiteral("Claude answered in 12s"), .When = QStringLiteral("just now"), .Head = QStringLiteral("abc1234") } });
	QApplication::processEvents();
	// [[BPR-2184]] (review N5): the byline is data, joined by the view.
	expect(view.findChild<QLabel*>(QStringLiteral("agentReportByline"))->text().contains(QStringLiteral("Claude answered in 12s · just now · at abc1234")), "the byline is said from its parts");
	expect(view.dressed_shown(), "the report is drawn as a page");
	auto* const page   = view.dressed_view();
	auto        images = 0;
	for (auto block = page->document()->begin(); block.isValid(); block = block.next()) {
		for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
			images += fragment.fragment().charFormat().isImageFormat() ? 1 : 0;
		}
	}
	expect(images == 0, "no image is drawn from the report's text");
	expect(page->toPlainText().contains(QStringLiteral("<img")), "raw HTML in the report is shown as text, not read as markup");
	for (const auto kind : { QTextDocument::ImageResource, QTextDocument::StyleSheetResource, QTextDocument::HtmlResource }) {
		const auto loaded = page->document()->resource(kind, QUrl{ address });
		expect(!loaded.toByteArray().contains("PICTURE-BYTES"), "the page's document loads nothing, even a file that exists");
	}
	// N1, N8: an action is never an href the report's text can write. A
	// report whose own links name the old action scheme and an action-like
	// token, beside a real suggestion, drafts only from the link the view
	// minted, and only on its own item.
	auto drafts = std::vector<bpr::qt::report_draft>{ };
	view.set_draft_dispatch([&drafts](const bpr::qt::report_draft& aDraft, bpr::qt::report_draft_done aDone) {
		drafts.push_back(aDraft);
		aDone(true, { });
	});
	const auto hostile = QStringLiteral("# Review of #21\n\nOne fix. [Add as draft comment](bpr-draft:2) and [this](report-action-00000000000000000000000000000000).\n\n"
										"## Must fix\n\n### issue (blocking): A zero height\n\n`src/engine/renderer.cpp:12-13`\n\nClamp it. [again](bpr-draft:2)\n\n"
										"```cpp\nconst auto width = 1;\nTarget.resize(width);\n```\n\n```suggestion\nconst auto width = 1;\nTarget.resize(width, 1);\n```\n");
	view.present({ .Turn = 2U, .Title = QStringLiteral("Review of #21"), .Markdown = hostile, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
	QApplication::processEvents();
	auto* const reviewed = view.dressed_view();
	followed.clear();
	for (const auto& href : { QStringLiteral("bpr-draft:2"), QStringLiteral("report-action-00000000000000000000000000000000") }) {
		const auto at = anchor_point(*reviewed, href);
		expect(at.has_value(), "the report's own link is on the page");
		click(*reviewed, *at);
	}
	expect(drafts.empty(), "a link the report's text wrote drafts nothing, whatever it is written as");
	expect(followed.contains(QStringLiteral("bpr-draft:2")), "it is followed as an ordinary link, by its text");
	// The page's own keep actions are links this render minted: Keep summary
	// first, then the finding's Draft comment ([[BPR-2219]]).
	auto minted = QString{ };
	for (const auto& href : anchors(*reviewed)) {
		if (href.startsWith(QStringLiteral("report-action-")) && href != QStringLiteral("report-action-00000000000000000000000000000000")) {
			minted = href;
		}
	}
	expect(!minted.isEmpty() && view.findings().size() == 1U && view.summary_offered(), "the finding offers Draft comment and the summary Keep summary");
	expect(reviewed->toPlainText().contains(QStringLiteral("Draft comment")) && reviewed->toPlainText().contains(QStringLiteral("Keep summary")), "both are on the page");
	click(*reviewed, *anchor_point(*reviewed, minted));
	expect(drafts.size() == 1U && drafts.front().Path == QStringLiteral("src/engine/renderer.cpp") && drafts.front().First == 12U, "the minted link drafts its own item");
	// [[BPR-2219]]. What is drafted is the core's: the label and the finding,
	// why, and the suggestion; the finding then reads Drafted and Go to
	// draft, and is not drafted twice.
	expect(drafts.front().Keep == bpr::core::report_keep::draft_comment && drafts.front().Comment.startsWith(QStringLiteral("issue (blocking): A zero height\n\nClamp it."))
			   && drafts.front().Comment.contains(QStringLiteral("```suggestion\nconst auto width = 1;\nTarget.resize(width, 1);\n```")),
		   "the draft starts from the finding's label, reasons and suggestion: " + drafts.front().Comment.toStdString());
	expect(view.keep_state(drafts.front().Outline).Phase == bpr::qt::report_keep_phase::kept && reviewed->toPlainText().contains(QStringLiteral("Drafted")) && reviewed->toPlainText().contains(QStringLiteral("Go to draft")),
		   "a drafted finding reads Drafted and Go to draft");
	expect(!view.press(drafts.front().Outline, bpr::qt::report_link::keep) && drafts.size() == 1U, "and is not drafted twice");
	auto gone = std::vector<bpr::qt::report_draft>{ };
	view.set_go_dispatch([&gone](const bpr::qt::report_draft& aDraft) { gone.push_back(aDraft); });
	expect(view.press(drafts.front().Outline, bpr::qt::report_link::go) && gone.size() == 1U && gone.front().First == 12U, "Go to draft goes to the finding's lines");
	// A refusal is said at the finding, which still offers its action.
	view.set_draft_dispatch([](const bpr::qt::report_draft&, bpr::qt::report_draft_done aDone) { aDone(false, QStringLiteral("Not kept: no store.")); });
	expect(view.press(0U, bpr::qt::report_link::keep) && view.keep_state(0U).Phase == bpr::qt::report_keep_phase::refused && reviewed->toPlainText().contains(QStringLiteral("Not kept: no store."))
			   && reviewed->toPlainText().contains(QStringLiteral("Keep summary")),
		   "a refused keep says why and stays offered");
	// A verdict stands over the summary; what the agent said while it worked
	// is not on the page; a finding with no suggestion drafts too.
	view.set_draft_dispatch([&drafts](const bpr::qt::report_draft& aDraft, bpr::qt::report_draft_done aDone) {
		drafts.push_back(aDraft);
		aDone(true, { });
	});
	view.present({ .Turn      = 3U,
				   .Title     = QStringLiteral("Review of #21"),
				   .Markdown  = QStringLiteral("Let me read the diff first.\n\nNow the callers.\n\n# Review of #21\n\n**Verdict:** Needs changes\n\nOne fix.\n\n## Must fix\n\n### issue (blocking): A zero height\n\n"
											   "`src/engine/renderer.cpp:12-13`\n\nClamp it.\n\n## Worth a look\n\n### nitpick: The README is stale\n\n`README.md`\n\nIt names the old folder.\n"),
				   .Question  = QStringLiteral("Review this pull request"),
				   .Byline    = { },
				   .Suggested = { { .Path = "src/engine/renderer.cpp", .First = 12U, .Last = 13U, .Comment = "issue: clamp the height too." } } });
	QApplication::processEvents();
	const auto pageText = view.dressed_view()->toPlainText();
	expect(view.dressed_shown() && view.dressed().Verdict == "Needs changes" && pageText.contains(QStringLiteral("Verdict")) && pageText.contains(QStringLiteral("Needs changes")), "the verdict stands over the summary");
	expect(!pageText.contains(QStringLiteral("Let me read the diff")) && !pageText.contains(QStringLiteral("Now the callers")), "what the agent said while it worked is not in the report");
	expect(view.findings().size() == 2U && view.findings()[0].Keep == bpr::core::report_keep::draft_comment && view.findings()[1].Keep == bpr::core::report_keep::keep_local
			   && pageText.count(QStringLiteral("Draft comment")) == 1 && pageText.count(QStringLiteral("Keep local")) == 1,
		   "a finding without a suggestion offers Draft comment, one that names only a file Keep local");
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::offered, "another report's findings start offered");
	drafts.clear();
	expect(view.press(view.findings()[0].Outline, bpr::qt::report_link::keep) && drafts.size() == 1U && drafts.front().Comment == QStringLiteral("issue: clamp the height too."),
		   "the agent's own suggested comment for the finding's place is what is drafted");
	view.present({ .Turn = 2U, .Title = QStringLiteral("Review of #21"), .Markdown = hostile, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
	QApplication::processEvents();
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::kept && view.dressed_view()->toPlainText().contains(QStringLiteral("Go to draft")),
		   "a report opened again says what was already drafted");
	// [[BPR-2238]]. In the cards look a finding's code box and suggested
	// change stand inside the finding's box, its padding at both sides, at
	// every pane width.
	for (const auto width : { 360, 520, 700, 900, 1400 }) {
		view.resize(width, 700);
		QApplication::processEvents();
		const auto insets = code_insets(view);
		expect(insets.size() == 2U, "the finding's code and its suggested change are shown at " + std::to_string(width));
		for (const auto& at : insets) {
			std::cout << "BPR-2238 width " << width << ": left " << at.Left << ", right " << at.Right << "\n";
			expect(at.Right >= 14 && at.Right <= 16 && at.Left >= 3 + 14 && at.Left <= 3 + 16, "a code box keeps the box's padding at both sides at " + std::to_string(width) + ": left " + std::to_string(at.Left) + ", right " + std::to_string(at.Right));
		}
	}
	view.resize(900, 700);
	QApplication::processEvents();
	// Report pages render with no action at all where no view mints one.
	const auto bare = bpr::qt::report_page_html(bpr::core::dress_report(hostile.toStdString()), { });
	expect(!bare.contains(QStringLiteral("report-action-0000000000000000000000000000000a")) && bare.contains(QStringLiteral("bpr-draft:2")) && !bare.contains(QStringLiteral("Draft comment")) && !bare.contains(QStringLiteral("Keep summary")),
		   "a page written with no minted links offers no action; the report's own links stay links");
	// N10: Tab reaches the page's links, and Return on the one reached
	// follows it; a click follows a place by its text.
	reviewed->setFocus();
	reviewed->moveCursor(QTextCursor::Start);
	followed.clear();
	auto reached = QString{ };
	for (auto press = 0; press < 12 && reached.isEmpty(); ++press) {
		QKeyEvent tab{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
		QApplication::sendEvent(reviewed, &tab);
		reached = view.keyboard_link();
	}
	expect(!reached.isEmpty() && !reviewed->textCursor().hasSelection(), "Tab reaches a link on the page, and selects no text");
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(reviewed, &enter);
	expect(!followed.isEmpty() || drafts.size() > 1U, "Return on the link Tab reached follows it");
	followed.clear();
	click(*reviewed, *anchor_point(*reviewed, QStringLiteral("src/engine/renderer.cpp#L12-L13")));
	expect(followed == QStringList{ QStringLiteral("src/engine/renderer.cpp#L12-L13") }, "a click on a place follows it by its text");

	// [[BPR-2231]]. With the host's records, a finding's state is the turn's
	// one state: a finding at a card's place is that card (its key and
	// delivery go with the keep), one with no card is kept under the
	// report's own digest; what the card did reads on the report.
	auto records = std::map<std::pair<bpr::core::agent_findings_digest, std::uint32_t>, bpr::core::agent_finding_card_record>{ };
	view.set_keep_records([&records](const bpr::core::report_finding_key& aKey) {
		const auto found = records.find({ aKey.Findings, aKey.Ordinal });
		return found == records.end() ? bpr::core::agent_finding_card_record{ } : found->second;
	});
	auto       batches = std::vector<std::vector<bpr::qt::report_draft>>{ };
	auto       finish  = bpr::qt::report_draft_all_done{ };
	const auto block   = bpr::core::digest_agent_findings("[]");
	view.set_draft_all_dispatch([&batches, &finish](std::vector<bpr::qt::report_draft> aDrafts, const bpr::core::draft_all_outcome&, bpr::qt::report_draft_all_done aDone) {
		batches.push_back(std::move(aDrafts));
		finish = std::move(aDone);
	});
	view.set_placeable([](const bpr::qt::report_draft& aDraft) { return aDraft.Path != QStringLiteral("src/gone.cpp"); });
	const auto three  = QStringLiteral("# Review of #30\n\n**Verdict:** Needs changes\n\nThree things.\n\n## Must fix\n\n### issue: A\n\n`src/a.cpp:3`\n\nWhy A.\n\n### issue: B\n\n`src/b.cpp:7`\n\nWhy B.\n\n"
									   "### issue: C\n\n`src/c.cpp:2`\n\nWhy C.\n\n### issue: D\n\n`src/gone.cpp:4`\n\nWhy D.\n");
	auto       card   = bpr::qt::agent_report_card{ .Session = 77U, .Ordinal = 1U, .Finding = { }, .Key = { .Findings = block, .Ordinal = 1U } };
	card.Finding.Path = "src/b.cpp";
	view.present({ .Turn      = 9U,
				   .Title     = QStringLiteral("Review of #30"),
				   .Markdown  = three,
				   .Question  = QStringLiteral("Review this pull request"),
				   .Byline    = { },
				   .Suggested = { { .Path = "src/b.cpp", .First = 7U, .Last = 7U, .Comment = "" } },
				   .Cards     = { card },
				   .Answer    = three });
	QApplication::processEvents();
	expect(view.findings().size() == 4U && view.findings()[1].Card == std::optional<std::size_t>{ 0U } && !view.findings()[0].Card, "B is its card; A, C and D are no card's");
	const auto own         = bpr::core::digest_report_findings(three.toStdString());
	records[{ block, 1U }] = { .Findings = block, .Ordinal = 1U, .State = bpr::core::agent_finding_card_disposition::drafted };
	view.settle_keep({ .Findings = block, .Ordinal = 1U, .Card = true });
	expect(view.keep_state(view.findings()[1].Outline).Phase == bpr::qt::report_keep_phase::kept, "drafted on its card, the finding reads Drafted on the report");
	// [[BPR-2232]]. Draft all: the findings not yet drafted, in the report's
	// order (B is drafted; D is not in the diff and is counted so).
	expect(view.draft_all_offered() == 2U && view.dressed_view()->toPlainText().contains(QStringLiteral("Draft all (2)")), "Draft all offers the two left to draft: " + std::to_string(view.draft_all_offered()));
	expect(view.press_draft_all() && batches.size() == 1U && batches[0].size() == 2U && batches[0][0].Path == QStringLiteral("src/a.cpp") && batches[0][1].Path == QStringLiteral("src/c.cpp")
			   && !batches[0][0].Card && batches[0][0].Key == bpr::core::report_finding_key{ .Findings = own, .Ordinal = view.findings()[0].Outline, .Card = false },
		   "Draft all drafts A then C, each under the report's own key");
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::working && view.draft_all_said().contains(QStringLiteral("Drafting 2 findings")) && !view.press_draft_all(),
		   "under way: each says so, and Draft all is not pressed twice");
	records[{ own, view.findings()[0].Outline }] = { .Findings = own, .Ordinal = view.findings()[0].Outline, .State = bpr::core::agent_finding_card_disposition::drafted };
	view.settle_keep({ .Findings = own, .Ordinal = view.findings()[0].Outline, .Card = false });
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::kept && view.keep_state(view.findings()[2].Outline).Phase == bpr::qt::report_keep_phase::working,
		   "each finding settles as its draft is made");
	records[{ own, view.findings()[2].Outline }] = { .Findings = own, .Ordinal = view.findings()[2].Outline, .State = bpr::core::agent_finding_card_disposition::refused, .Why = QStringLiteral("Not drafted: resolve the draft open at that line first.").toStdString() };
	finish({ .Drafted = 1U, .AlreadyDrafted = 1U, .NotInDiff = 1U, .Refused = 1U });
	QApplication::processEvents();
	const auto said = view.dressed_view()->toPlainText();
	expect(view.draft_all_said() == QStringLiteral("Drafted 1 private draft. Skipped 3: 1 already drafted, 1 no longer in the diff, 1 could not be drafted; its finding says why.") && said.contains(view.draft_all_said())
			   && view.keep_state(view.findings()[2].Outline).Phase == bpr::qt::report_keep_phase::refused && said.contains(QStringLiteral("resolve the draft open")),
		   "it says how many were drafted and which were not, and the refused one says why: " + view.draft_all_said().toStdString());
	expect(view.draft_all_offered() == 1U && said.contains(QStringLiteral("Draft all (1)")), "the refused one is still offered");
	// [[BPR-2325]]. B's draft was deleted: the host forgets its record, and
	// the finding offers Draft comment again; A's, still there, keeps Drafted.
	records.erase({ block, 1U });
	view.settle_keep({ .Findings = block, .Ordinal = 1U, .Card = true });
	QApplication::processEvents();
	const auto reopened = view.dressed_view()->toPlainText();
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::kept, "a finding whose draft is still there keeps Drafted");
	expect(view.keep_state(view.findings()[1].Outline).Phase == bpr::qt::report_keep_phase::offered && view.draft_all_offered() == 2U && reopened.contains(QStringLiteral("Draft all (2)")),
		   "the finding whose draft was deleted offers Draft comment again: " + reopened.toStdString());
	// [[BPR-2331]]. The Agent tab no longer shows a finding card the report
	// holds, so what the card offered is on the report: Dismiss and its Undo,
	// the suggested comment, what a draft said of its lines, and Undo of
	// Draft all.
	{
		auto dismissals = std::vector<std::pair<QString, bool>>{ };
		view.set_dismiss_dispatch([&records, &dismissals](const bpr::qt::report_draft& aFinding, const bool aDismiss) {
			dismissals.emplace_back(aFinding.Path, aDismiss);
			auto record                                              = bpr::core::agent_finding_card_record{ };
			record.Findings                                          = aFinding.Key.Findings;
			record.Ordinal                                           = aFinding.Key.Ordinal;
			record.State                                             = aDismiss ? bpr::core::agent_finding_card_disposition::dismissed : bpr::core::agent_finding_card_disposition::open;
			records[{ aFinding.Key.Findings, aFinding.Key.Ordinal }] = record;
		});
		const auto cardBlock               = bpr::core::digest_agent_findings("[{}]");
		auto       commented               = bpr::qt::agent_report_card{ .Session = 78U, .Ordinal = 0U, .Finding = { }, .Key = { .Findings = cardBlock, .Ordinal = 0U } };
		commented.Finding.Path             = "src/a.cpp";
		commented.Finding.SuggestedComment = "issue: say why A matters.";
		view.present({ .Turn      = 10U,
					   .Title     = QStringLiteral("Review of #30"),
					   .Markdown  = three,
					   .Question  = QStringLiteral("Review this pull request"),
					   .Byline    = { },
					   .Suggested = { { .Path = "src/a.cpp", .First = 3U, .Last = 3U, .Comment = "issue: say why A matters." } },
					   .Cards     = { commented },
					   .Answer    = three });
		QApplication::processEvents();
		const auto a = view.findings().at(0U).Outline;
		const auto b = view.findings().at(1U).Outline;
		expect(view.findings()[0].Card == std::optional<std::size_t>{ 0U } && !view.findings()[1].Card, "A is its card, B no card's");
		auto shownText = view.dressed_view()->toPlainText();
		expect(shownText.contains(QStringLiteral("Suggested comment:")) && shownText.contains(QStringLiteral("issue: say why A matters.")), "the suggested comment shows on its finding, as its card showed it");
		expect(shownText.count(QStringLiteral("Dismiss")) >= 2, "Dismiss is offered beside the findings' keep actions");
		expect(view.press(a, bpr::qt::report_link::dismiss) && dismissals.size() == 1U && dismissals.back() == std::pair{ QStringLiteral("src/a.cpp"), true }
				   && view.keep_state(a).Phase == bpr::qt::report_keep_phase::dismissed,
			   "Dismiss dismisses the card's finding through its card's record");
		shownText = view.dressed_view()->toPlainText();
		expect(shownText.contains(QStringLiteral("Dismissed")) && shownText.contains(QStringLiteral("Undo")) && !view.press(a, bpr::qt::report_link::keep) && !view.press(a, bpr::qt::report_link::dismiss),
			   "a dismissed finding says so with Undo, and offers no keep action until it is undone");
		expect(view.press(a, bpr::qt::report_link::undo_dismiss) && dismissals.back() == std::pair{ QStringLiteral("src/a.cpp"), false } && view.keep_state(a).Phase == bpr::qt::report_keep_phase::offered,
			   "Undo takes the dismissal back");
		expect(view.press(b, bpr::qt::report_link::dismiss) && view.keep_state(b).Phase == bpr::qt::report_keep_phase::dismissed, "a finding no card carries is dismissed under the report's own record");
		expect(!view.press(0U, bpr::qt::report_link::dismiss), "the summary is not dismissed");
		// What its draft said of its lines ([[BPR-2298]]) stands beside Drafted.
		records[{ cardBlock, 0U }] = { .Findings = cardBlock, .Ordinal = 0U, .State = bpr::core::agent_finding_card_disposition::drafted };
		view.say_drafted({ .Findings = cardBlock, .Ordinal = 0U, .Card = true }, QStringLiteral("Drafted on lines 3 to 5, widened to its suggestion."));
		view.settle_keep({ .Findings = cardBlock, .Ordinal = 0U, .Card = true });
		QApplication::processEvents();
		expect(view.keep_state(a).Phase == bpr::qt::report_keep_phase::kept && view.keep_state(a).Why == QStringLiteral("Drafted on lines 3 to 5, widened to its suggestion.")
				   && view.dressed_view()->toPlainText().contains(QStringLiteral("widened to its suggestion")) && view.dressed_view()->toPlainText().contains(QStringLiteral("Go to draft")),
			   "Drafted says what the draft said of its lines");
		// Undo of the last Draft all.
		auto undone = false;
		view.offer_draft_all_undo(10U, [&undone](bpr::qt::report_draft_all_undo_done aDone) {
			undone = true;
			aDone(2U, 1U);
		});
		QApplication::processEvents();
		expect(view.dressed_view()->toPlainText().count(QStringLiteral("Undo")) >= 1, "Draft all's Undo is offered");
		expect(view.undo_draft_all() && undone && view.draft_all_said() == QStringLiteral("Removed 2 drafts. Kept 1 draft you changed since.") && view.dressed_view()->toPlainText().contains(view.draft_all_said()),
			   "Undo removes the batch's drafts and says what it removed and kept: " + view.draft_all_said().toStdString());
		expect(!view.undo_draft_all(), "Undo is offered once");
		view.set_dismiss_dispatch({ });
	}
	// [[BPR-2352]] (owner, 2026-11-21: "The code block didn't appear in
	// the suggested comment"). A finding whose fix is a block in the file's
	// language, and whose card's comment is only words: the suggested
	// comment shows the fix as a code box, and Draft comment drafts it.
	{
		const auto review                = QStringLiteral("# Review of #31\n\n**Verdict:** Needs changes\n\nOne thing.\n\n## Must fix\n\n### issue (blocking): A zero height still asserts\n\n`src/r.cpp:12-13`\n\n"
														  "The height is not clamped.\n\n```cpp\nTarget.resize(extent{ width, aSize.Height });\n```\n\nClamp it as well:\n\n"
														  "```cpp\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```\n");
		const auto fixBlock              = bpr::core::digest_agent_findings("[{\"fix\":1}]");
		auto       fixCard               = bpr::qt::agent_report_card{ .Session = 79U, .Ordinal = 0U, .Finding = { }, .Key = { .Findings = fixBlock, .Ordinal = 0U } };
		fixCard.Finding.Path             = "src/r.cpp";
		fixCard.Finding.SuggestedComment = "issue (blocking): the height is not clamped. Clamp both.";
		view.present({ .Turn      = 11U,
					   .Title     = QStringLiteral("Review of #31"),
					   .Markdown  = review,
					   .Question  = QStringLiteral("Review this pull request"),
					   .Byline    = { },
					   .Suggested = { { .Path = "src/r.cpp", .First = 12U, .Last = 13U, .Comment = "issue (blocking): the height is not clamped. Clamp both." } },
					   .Cards     = { fixCard },
					   .Answer    = review });
		QApplication::processEvents();
		const auto& finding = view.findings().at(0U);
		expect(finding.Comment.ends_with("```cpp\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```"), "the finding's comment carries the fix: " + finding.Comment);
		const auto shown = view.dressed_view()->toPlainText();
		const auto at    = shown.indexOf(QStringLiteral("Suggested comment:"));
		expect(at >= 0 && shown.indexOf(QStringLiteral("Clamp both."), at) > at, "the suggested comment shows its words");
		// The fix is the page's own plain code box, as the owner accepted it
		// (session 772f868fd02ed420c8889a2d055f5fab), not a code view with
		// Copy and Wrap: the item's two blocks are the page's only views.
		expect(view.code_views().size() == 2U, "the suggested comment's fix is not a code view of its own: " + std::to_string(view.code_views().size()));
		expect(shown.indexOf(QStringLiteral("const auto height = std::max(aSize.Height, 1);"), at) > at, "the suggested comment shows the fix in its own box");
		expect(!shown.mid(at).contains(QStringLiteral("```")), "and no fence shows as text");
	}
	check_inline_code();
	check_keep_rows_fit_every_width();
	check_keep_chip();
	check_a_link_takes_one_click();
	check_reading_width();
	check_question_link();
	std::cout << "AGENT_REPORT_VIEW_OK\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << aError.what() << '\n';
	return EXIT_FAILURE;
}
