// [[BPR-2279]] (owner, 2026-11-19: "momentum scrolling that reaches the end
// of a nested scroll window won't scroll the outer window without an
// additional gesture. In fact that appears to be exactly how it works in the
// chrome browser"). Phased wheel events go in through QWindowSystemInterface,
// spontaneous as a trackpad's are, over a field nested in a scroll area and a
// code line that only scrolls sideways.
//
// [[BPR-2292]] (owner, 2026-11-19: "I have a agent tab review with a couple
// suggestions. If my mouse is over a suggestion that has horizontal scroll I
// can't scroll the outer area down"; and "Vertical scroll areas that are at
// their limit should let you initiate a scroll of the outer too if scrolling
// in the direction that's limited"). A scroller that declines the gesture's
// axis is passed over whatever range its hidden bar reports, a field at
// either end hands a new gesture that way to the outer area, and an Agent
// tab report's suggestions pass a vertical gesture to the page.
#include "bpr/qt/nested_scroll_latch.hpp"

#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <qpa/qwindowsysteminterface.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief One wheel event at a fixed place, as the platform hands it in:
	/// a trackpad's pixels and the angle Qt derives from them.
	void wheel(QWidget& aWindow, const QPoint aGlobal, const Qt::ScrollPhase aPhase, const QPoint aPixels)
	{
		auto* const handle = aWindow.windowHandle();
		QWindowSystemInterface::handleWheelEvent(handle, handle->mapFromGlobal(aGlobal), aGlobal, aPixels, aPhase == Qt::NoScrollPhase ? aPixels * 3 : aPixels, Qt::NoModifier, aPhase);
		QWindowSystemInterface::flushWindowSystemEvents();
		QApplication::processEvents();
	}

	/// \brief A gesture: its moves, then its momentum, each `aStep` pixels.
	void gesture(QWidget& aWindow, const QPoint aGlobal, const QPoint aStep, const int aMoves, const int aMomentum, const bool aEnd = true)
	{
		wheel(aWindow, aGlobal, Qt::ScrollBegin, { });
		for (auto move = 0; move < aMoves; ++move) {
			wheel(aWindow, aGlobal, Qt::ScrollUpdate, aStep);
		}
		for (auto coast = 0; coast < aMomentum; ++coast) {
			wheel(aWindow, aGlobal, Qt::ScrollMomentum, aStep);
		}
		if (aEnd) {
			wheel(aWindow, aGlobal, Qt::ScrollEnd, { });
		}
	}

	/// \brief What an in-place reader is to a wheel: sideways scrolling its
	/// own, every vertical movement its holder's, though its hidden vertical
	/// bar has a range.
	class declining_reader final : public QTextEdit
	{
	public:
		using QTextEdit::QTextEdit;

	protected:
		void wheelEvent(QWheelEvent* aEvent) override
		{
			const auto delta = aEvent->pixelDelta().isNull() ? aEvent->angleDelta() : aEvent->pixelDelta();
			if (std::abs(delta.y()) >= std::abs(delta.x())) {
				aEvent->ignore();
				return;
			}
			QTextEdit::wheelEvent(aEvent);
		}
	};

	/// \brief The visible text page of a reader, which takes its wheel.
	[[nodiscard]] auto reader_page(const QWidget& aReader) -> QTextEdit*
	{
		const auto pages = aReader.findChildren<QTextEdit*>();
		const auto found = std::ranges::find_if(pages, [](const QTextEdit* aPage) { return aPage->isVisible(); });
		return found != pages.end() ? *found : nullptr;
	}

	/// \brief [[BPR-2292]]. An Agent tab report with two suggestions wider
	/// than the page: a vertical gesture over either moves the page, both
	/// ways, and a sideways one moves the suggestion.
	void check_report_suggestions()
	{
		auto report = bpr::qt::agent_report_view{ };
		report.resize(560, 420);
		report.show();
		auto markdown = QStringLiteral("# Review of #9\n\n**Verdict:** Needs changes\n\nTwo fixes.\n\n## Must fix\n\n");
		for (const auto item : { 1, 2 }) {
			markdown += QStringLiteral("### issue: Fix %1\n\n`src/a.cpp:%1`\n\nWhy it matters.\n\n```suggestion\n"
									   "const auto value = compute_a_rather_long_name(first_argument, second_argument, third_argument, fourth_argument_%1);\n```\n\n")
							.arg(item);
		}
		markdown += QStringLiteral("More words.\n\n").repeated(30);
		report.present({ .Turn = 1U, .Title = QStringLiteral("Review of #9"), .Markdown = markdown, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
		QApplication::processEvents();
		auto* const scroll = report.findChild<QScrollArea*>(QStringLiteral("agentReportScroll"));
		expect(scroll != nullptr && report.code_views().size() == 2U, "the report draws both suggestions in its page");
		auto* const page = scroll->verticalScrollBar();
		for (auto* const box : report.code_views()) {
			// The suggestion in the middle of the pane, with room both ways.
			const auto down = box->mapToGlobal(QPoint{ }).y() - scroll->widget()->mapToGlobal(QPoint{ }).y();
			page->setValue(std::clamp(down - 150, 40, page->maximum() - 40));
			QApplication::processEvents();
			expect(scroll->viewport()->rect().contains(scroll->viewport()->mapFromGlobal(box->mapToGlobal(box->rect().center()))), "the suggestion is in view, the page free to move both ways");
			auto* const reader = reader_page(*box);
			expect(reader != nullptr && reader->horizontalScrollBar()->maximum() > 0, "each suggestion scrolls sideways in its box");
			// Under the macOS style the in-place reader's hidden vertical
			// bar keeps a range (0..227 under cocoa) its box does not have:
			// the latch gave it the gesture, and it passed every vertical
			// movement on, to nowhere.
			reader->verticalScrollBar()->setRange(0, 227);
			const auto over = [reader] { return reader->viewport()->mapToGlobal(reader->viewport()->rect().center()); };
			for (const auto step : { QPoint{ 0, -30 }, QPoint{ 0, 30 } }) {
				const auto before = page->value();
				gesture(report, over(), step, 2, 2);
				std::cout << "BPR-2292 report suggestion " << step.y() << ": page " << before << " -> " << page->value() << '\n';
				expect(step.y() < 0 ? page->value() > before : page->value() < before, "a vertical gesture over a suggestion moves the report, both ways");
				expect(bpr::qt::nested_scroll_latch::owner() == scroll, "the report's page owns it");
			}
			const auto top = page->value();
			wheel(report, over(), Qt::NoScrollPhase, QPoint{ 0, -40 });
			expect(page->value() > top, "a mouse wheel over a suggestion moves the report");
			const auto settled = page->value();
			gesture(report, over(), QPoint{ -30, 0 }, 2, 0);
			expect(reader->horizontalScrollBar()->value() > 0 && page->value() == settled, "a sideways gesture moves the suggestion, not the report");
			reader->horizontalScrollBar()->setValue(0);
		}
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	bpr::qt::nested_scroll_latch::install();
	bpr::qt::nested_scroll_latch::install();

	auto window = QWidget{ };
	window.resize(420, 320);
	auto* const column = new QVBoxLayout{ &window };
	column->setContentsMargins(0, 0, 0, 0);
	auto* const outer = new QScrollArea{ &window };
	outer->setWidgetResizable(true);
	column->addWidget(outer);
	auto* const page   = new QWidget{ };
	auto* const layout = new QVBoxLayout{ page };
	layout->setContentsMargins(10, 10, 10, 10);
	auto* const field = new QPlainTextEdit{ page };
	field->setFixedHeight(140);
	auto text = QString{ };
	for (auto line = 0; line < 200; ++line) {
		text += QStringLiteral("A long draft's line %1\n").arg(line);
	}
	field->setPlainText(text);
	layout->addWidget(field);
	// A code line: a sideways bar and no vertical range.
	auto* const code = new QTextEdit{ page };
	code->setLineWrapMode(QTextEdit::NoWrap);
	code->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	code->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	code->setPlainText(QString{ 400, QLatin1Char{ 'x' } });
	code->setFixedHeight(code->fontMetrics().lineSpacing() * 3 + code->horizontalScrollBar()->sizeHint().height());
	layout->addWidget(code);
	auto* const filler = new QWidget{ page };
	filler->setFixedHeight(1400);
	layout->addWidget(filler);
	outer->setWidget(page);
	window.show();
	QApplication::processEvents();

	auto* const outerBar = outer->verticalScrollBar();
	auto* const fieldBar = field->verticalScrollBar();
	auto* const codeBar  = code->horizontalScrollBar();
	expect(outerBar->maximum() > 0 && fieldBar->maximum() > 0 && codeBar->maximum() > 0 && code->verticalScrollBar()->maximum() == 0, "the outer area, the field and the code line each have their range");
	const auto overField = field->viewport()->mapToGlobal(field->viewport()->rect().center());
	const auto down      = QPoint{ 0, -30 };

	// A gesture that begins in the field stays the field's: reaching its end,
	// neither the rest of the gesture nor its momentum moves the outer area.
	fieldBar->setValue(fieldBar->maximum() - 3);
	gesture(window, overField, down, 6, 12, false);
	std::cout << "BPR-2279 latched: field " << fieldBar->value() << "/" << fieldBar->maximum() << ", outer " << outerBar->value() << '\n';
	expect(fieldBar->value() == fieldBar->maximum(), "the gesture scrolls the field to its end");
	expect(outerBar->value() == 0, "the rest of the gesture and its momentum stay with the field");
	expect(bpr::qt::nested_scroll_latch::owner() == field, "the field owns the gesture");
	wheel(window, overField, Qt::ScrollEnd, { });
	wheel(window, overField, Qt::ScrollMomentum, down);
	expect(outerBar->value() == 0, "momentum after the gesture's end is still the field's");

	// A new gesture at the field's end scrolls the outer area, and stays with
	// it even when it turns back the way the field could go.
	gesture(window, overField, down, 3, 3);
	const auto moved = outerBar->value();
	std::cout << "BPR-2279 next gesture: outer " << moved << '\n';
	expect(moved > 0 && fieldBar->value() == fieldBar->maximum(), "the next gesture at the field's end scrolls the outer area");
	expect(bpr::qt::nested_scroll_latch::owner() == outer, "the outer area owns it");
	const auto beforeBack = outerBar->value();
	wheel(window, overField, Qt::ScrollBegin, { });
	wheel(window, overField, Qt::ScrollUpdate, down);
	wheel(window, overField, Qt::ScrollUpdate, -down);
	wheel(window, overField, Qt::ScrollUpdate, -down);
	wheel(window, overField, Qt::ScrollEnd, { });
	expect(fieldBar->value() == fieldBar->maximum() && outerBar->value() < beforeBack + 30, "turning back, the gesture keeps moving the outer area, not the field");

	// A gesture with room in the field scrolls the field from where it is.
	outerBar->setValue(0);
	QApplication::processEvents();
	fieldBar->setValue(20);
	gesture(window, overField, -down, 2, 2);
	expect(fieldBar->value() < 20 && outerBar->value() == 0, "a gesture the field can take moves the field alone");

	// A code line that only scrolls sideways passes a vertical gesture on, and
	// keeps a sideways one, past its end too.
	const auto overCode = code->viewport()->mapToGlobal(code->viewport()->rect().center());
	gesture(window, overCode, down, 3, 2);
	expect(outerBar->value() > 0 && codeBar->value() == 0, "a vertical gesture over a sideways-only code line scrolls the outer area");
	const auto outerAfter  = outerBar->value();
	const auto overCodeNow = code->viewport()->mapToGlobal(code->viewport()->rect().center());
	codeBar->setValue(codeBar->maximum() - 5);
	gesture(window, overCodeNow, QPoint{ -30, 0 }, 3, 6);
	expect(codeBar->value() == codeBar->maximum() && outerBar->value() == outerAfter, "a sideways gesture is the code line's, to its end");

	// A mouse wheel's unphased notches are Qt's own: at the field's end they
	// pass on.
	outerBar->setValue(0);
	QApplication::processEvents();
	fieldBar->setValue(fieldBar->maximum());
	const auto overFieldNow = field->viewport()->mapToGlobal(field->viewport()->rect().center());
	wheel(window, overFieldNow, Qt::NoScrollPhase, QPoint{ 0, -40 });
	expect(outerBar->value() > 0, "a mouse wheel at the field's end still scrolls the outer area");

	// [[BPR-2292]]. At its top a field hands a new upward gesture to the
	// outer area, and still takes a downward one.
	outerBar->setValue(60);
	QApplication::processEvents();
	fieldBar->setValue(0);
	const auto overFieldTop = field->viewport()->mapToGlobal(field->viewport()->rect().center());
	gesture(window, overFieldTop, -down, 2, 2);
	std::cout << "BPR-2292 up at the field's top: outer 60 -> " << outerBar->value() << '\n';
	expect(outerBar->value() < 60 && fieldBar->value() == 0 && bpr::qt::nested_scroll_latch::owner() == outer, "a new gesture up at the field's top moves the outer area");
	const auto outerAtTop     = outerBar->value();
	const auto overFieldAgain = field->viewport()->mapToGlobal(field->viewport()->rect().center());
	gesture(window, overFieldAgain, down, 2, 2);
	expect(fieldBar->value() > 0 && outerBar->value() == outerAtTop, "a gesture down from the field's top moves the field alone");

	// A reader that declines vertical movement, though its hidden bar has a
	// range, passes a vertical gesture to the outer area either way.
	auto* const reader = new declining_reader{ page };
	reader->setLineWrapMode(QTextEdit::NoWrap);
	reader->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	reader->setPlainText(QString{ 400, QLatin1Char{ 'y' } } + QStringLiteral("\n").repeated(40));
	reader->setFixedHeight(reader->fontMetrics().lineSpacing() * 3);
	layout->insertWidget(1, reader);
	outerBar->setValue(0);
	QApplication::processEvents();
	reader->verticalScrollBar()->setValue(reader->verticalScrollBar()->maximum() / 2);
	expect(reader->verticalScrollBar()->maximum() > 0 && reader->horizontalScrollBar()->maximum() > 0, "the reader has both ranges");
	const auto overReader = [reader] { return reader->viewport()->mapToGlobal(reader->viewport()->rect().center()); };
	gesture(window, overReader(), down, 3, 2);
	const auto readerDown = outerBar->value();
	std::cout << "BPR-2292 declining reader down: outer " << readerDown << '\n';
	expect(readerDown > 0 && bpr::qt::nested_scroll_latch::owner() == outer, "a vertical gesture down over a reader that declines it moves the outer area");
	gesture(window, overReader(), -down, 2, 0);
	expect(outerBar->value() < readerDown, "and up");
	gesture(window, overReader(), QPoint{ -30, 0 }, 2, 0);
	expect(reader->horizontalScrollBar()->value() > 0 && bpr::qt::nested_scroll_latch::owner() == reader, "a sideways gesture is the reader's");

	// A reader that is no post in place keeps a wheel it can take, and at
	// its end hands a mouse wheel's notch that way to the outer area.
	auto* const body = new bpr::qt::markdown_body_view{ page };
	body->setFixedHeight(160);
	auto lines = std::string{ };
	for (auto line = 0; line < 80; ++line) {
		lines += "Line " + std::to_string(line) + "\n\n";
	}
	body->set_body(lines, "bpr-2292-body");
	layout->insertWidget(1, body);
	outerBar->setValue(100);
	QApplication::processEvents();
	auto* const bodyPage = reader_page(*body);
	expect(bodyPage != nullptr && bodyPage->verticalScrollBar()->maximum() > 0, "the body scrolls");
	const auto overBody = [bodyPage] { return bodyPage->viewport()->mapToGlobal(bodyPage->viewport()->rect().center()); };
	bodyPage->verticalScrollBar()->setValue(bodyPage->verticalScrollBar()->maximum());
	wheel(window, overBody(), Qt::NoScrollPhase, QPoint{ 0, -40 });
	expect(outerBar->value() > 100, "a mouse wheel down at the body's end moves the outer area");
	bodyPage->verticalScrollBar()->setValue(0);
	const auto beforeUp = outerBar->value();
	wheel(window, overBody(), Qt::NoScrollPhase, QPoint{ 0, 40 });
	expect(outerBar->value() < beforeUp, "a mouse wheel up at its top moves the outer area");
	const auto beforeInside = outerBar->value();
	wheel(window, overBody(), Qt::NoScrollPhase, QPoint{ 0, -40 });
	expect(bodyPage->verticalScrollBar()->value() > 0 && outerBar->value() == beforeInside, "a mouse wheel it can take moves the body alone");

	check_report_suggestions();

	std::cout << "nested-scroll-latch:pass\n";
	return EXIT_SUCCESS;
}
