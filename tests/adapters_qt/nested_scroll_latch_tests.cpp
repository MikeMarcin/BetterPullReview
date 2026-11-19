// [[BPR-2279]] (owner, 2026-11-19: "momentum scrolling that reaches the end
// of a nested scroll window won't scroll the outer window without an
// additional gesture. In fact that appears to be exactly how it works in the
// chrome browser"). Phased wheel events go in through QWindowSystemInterface,
// spontaneous as a trackpad's are, over a field nested in a scroll area and a
// code line that only scrolls sideways.
#include "bpr/qt/nested_scroll_latch.hpp"

#include <QApplication>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <qpa/qwindowsysteminterface.h>

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

	std::cout << "nested-scroll-latch:pass\n";
	return EXIT_SUCCESS;
}
