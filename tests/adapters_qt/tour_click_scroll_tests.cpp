// [[BPR-1914]] v004, owner look v003: "Clicking things in the tour tab is
// disorienting ... a single left-click to activate (1) but it scrolls almost
// all the way off the top of the screen." The Tour tab sits in a scroll area,
// as the drawer holds it; a single click on a stop, a step or a control keeps
// the scroll position, within a few pixels.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/review_tour_editor.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void settle()
	{
		for (int round = 0; round < 8; ++round) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents();
		}
	}
	void QTest_wait(const int aMilliseconds)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < aMilliseconds) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
	}
	/// One left click: press and release, as a person's.
	void click(QWidget* aWidget, const QPoint& aAt)
	{
		auto press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aAt }, aWidget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ aAt }, aWidget->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
		settle();
		QTest_wait(150);
		settle();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };

	auto draft = tests::review_tour_fixture();
	auto stop  = draft.Stops.front();
	draft.Stops.clear();
	for (std::uint64_t index = 1U; index <= 6U; ++index) {
		stop.Id    = core::tour_stop_id{ index };
		stop.Title = "Stop number " + std::to_string(index);
		stop.Body  = "Read these lines.\n\nThen these, and notice `clear()` before `draw()`.";
		draft.Stops.push_back(stop);
	}
	draft.LastStopId = 6U;
	// [[BPR-1991]]. The route scrolls inside the tab, between its head and
	// its agent bar: the drawer's scroll area is the editor's own.
	auto        tab    = qt::review_tour_editor{ };
	auto* const editor = &tab;
	editor->resize(380, 700);
	// As the window answers: arriving at a stop (navigate_stop) brings the stops
	// step forward and the stop's lines into the code, and the host's
	// presentation comes back with the stop selected.
	editor->set_dispatch([](core::tour_command) { }, [editor](const qt::tour_editor_action aAction) {
		// As the owner saw it on macOS, the drawer then sent to its end by
		// what the window does next.
		if (aAction == qt::tour_editor_action::navigate_stop) {
			editor->show_stops();
			QTimer::singleShot(20, editor, [editor] {
				auto* const area = editor->route_scroll();
				area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
			});
		} });
	editor->show();
	editor->present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor->set_agent_readiness(qt::agent_setup_readiness::ready);
	editor->show_stops();
	settle();
	auto* const outline = editor->findChild<QListWidget*>(QStringLiteral("tourOutline"));
	expect(outline != nullptr && outline->count() == 6, "six stops on the route");
	auto* const bar = editor->route_scroll()->verticalScrollBar();
	expect(bar->maximum() > 0, "the route is taller than the drawer");

	// Each stop, clicked where it is shown: the drawer keeps its place.
	for (int row = 0; row < outline->count(); ++row) {
		// Bring the row into the middle of the drawer, as a person scrolls to it.
		const auto rowTop = outline->mapTo(editor->route_scroll()->widget(), outline->visualItemRect(outline->item(row)).topLeft()).y();
		bar->setValue(std::clamp(rowTop - 200, 0, bar->maximum()));
		settle();
		const auto before = bar->value();
		const auto rect   = outline->visualItemRect(outline->item(row));
		click(outline->viewport(), QPoint{ rect.left() + 60, rect.top() + 12 });
		const auto after = bar->value();
		expect(outline->currentRow() == row, "the click opens stop " + std::to_string(row + 1));
		expect(std::abs(after - before) <= 4, "a click on stop " + std::to_string(row + 1) + " keeps the drawer where it was: " + std::to_string(before) + " then " + std::to_string(after));
	}
	// The open stop's own control: the pencil. [[BPR-2013]]: Show in code
	// is gone; arriving at a stop shows its code.
	// Settled past the hold, a scroll the person makes stands.
	QTest_wait(700);
	bar->setValue(0);
	settle();
	expect(bar->value() == 0, "after the hold the person's own scroll stands");
	bar->setValue(std::min(120, bar->maximum()));
	settle();
	for (const auto* name : { "tourStopEdit" }) {
		auto* const control = editor->findChild<QWidget*>(QString::fromLatin1(name));
		expect(control != nullptr && control->isVisible(), std::string{ name } + " is shown");
		const auto before = bar->value();
		click(control, control->rect().center());
		expect(std::abs(bar->value() - before) <= 4, std::string{ "a click on " } + name + " keeps the drawer where it was: " + std::to_string(before) + " then " + std::to_string(bar->value()) + " of " + std::to_string(bar->maximum()));
	}
	std::cout << "tour-click-scroll:pass\n";
	return EXIT_SUCCESS;
}
