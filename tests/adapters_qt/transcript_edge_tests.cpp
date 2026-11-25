/// \file
/// \brief A chat's transcript draws its top edge while turns are scrolled
/// under it, and only then.
///
/// \details Owner, attended review 2026-11-25, of a refused question in a
/// 280 pixel drawer: the first line of the question was "partly hidden under
/// the Native continuation line". Nothing lay over the turn: the transcript
/// keeps itself at its end, and one taller than its viewport cuts a turn at
/// the viewport's top, wherever that falls. With no line between the header's
/// words and the turns, the cut read as the header lying over the turn.
///
/// Checked on a bare scroll area that follows its end as the chats' do: no
/// edge on an empty transcript, on one that fits and on one at its start; an
/// edge, one pixel on the viewport's first row and over the turn scrolled
/// under it, the moment a turn is cut there, and none while only the blank
/// room over the first turn is scrolled away; the edge taking no room, no
/// pointer and no keyboard; and that the Agent tab's conversation and the
/// tour's side chat each have one.

#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/stick_to_bottom.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/transcript_edge.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QColor>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	/// The colour every turn of the bare transcript is filled with: nothing
	/// the palette's ground or the edge could be taken for.
	[[nodiscard]] auto turn_colour() -> QColor { return QColor{ 0x20, 0x90, 0x40 }; }

	/// \brief A transcript like the chats': a column of turns over a stretch,
	/// which follows its end.
	struct transcript
	{
		QWidget      Outer;
		QScrollArea* Area   = nullptr;
		QVBoxLayout* Column = nullptr;

		transcript()
		{
			auto* const outer = new QVBoxLayout{ &Outer };
			outer->setContentsMargins(0, 0, 0, 0);
			auto* const header = new QLabel{ QStringLiteral("The session's title"), &Outer };
			outer->addWidget(header);
			Area = new QScrollArea{ &Outer };
			Area->setWidgetResizable(true);
			Area->setFrameShape(QFrame::NoFrame);
			Area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			// A bar that takes room would narrow the viewport as it shows;
			// the range is the same with none drawn.
			Area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			auto* const content = new QWidget{ Area };
			Column              = new QVBoxLayout{ content };
			Column->setContentsMargins(0, 0, 0, 0);
			Column->setSpacing(0);
			Column->addStretch(1);
			Area->setWidget(content);
			outer->addWidget(Area, 1);
			qt::stick_to_bottom::attach(*Area);
			qt::transcript_edge::attach(*Area);
			Outer.resize(320, 240);
			Outer.show();
			settle();
		}

		auto append(const int aHeight) -> QWidget*
		{
			auto* const turn = new QWidget{ Area->widget() };
			turn->setFixedHeight(aHeight);
			turn->setAutoFillBackground(true);
			auto colours = turn->palette();
			colours.setColor(QPalette::Window, turn_colour());
			turn->setPalette(colours);
			Column->insertWidget(Column->count() - 1, turn);
			settle();
			return turn;
		}

		[[nodiscard]] auto edge() const -> qt::transcript_edge* { return qt::transcript_edge::of(*Area); }

		/// The share of the viewport's first row drawn in `aColour`.
		[[nodiscard]] auto first_row_in(const QColor& aColour) const -> double
		{
			const auto view  = Area->viewport()->geometry();
			const auto image = Area->grab(QRect{ view.left(), view.top(), view.width(), 1 }).toImage().convertToFormat(QImage::Format_ARGB32);
			auto       same  = 0;
			for (int x = 0; x < image.width(); ++x) {
				same += image.pixelColor(x, 0).rgb() == aColour.rgb() ? 1 : 0;
			}
			return image.width() > 0 ? static_cast<double>(same) / image.width() : 0.0;
		}
	};

	void test_the_edge_shows_exactly_while_turns_are_scrolled_under_it()
	{
		auto        chat = transcript{ };
		auto* const edge = chat.edge();
		auto* const bar  = chat.Area->verticalScrollBar();
		expect(edge != nullptr && &qt::transcript_edge::attach(*chat.Area) == edge, "a transcript has one edge, however often it is attached");
		const auto line = qt::transcript_edge::colour(chat.Outer.palette());
		expect(line != chat.Outer.palette().color(QPalette::Active, QPalette::Window) && line != turn_colour(), "the edge is a colour of its own on the drawer's ground");
		expect(!edge->isVisible(), "an empty transcript has no edge");

		chat.append(60);
		chat.append(60);
		expect(bar->maximum() == 0 && !edge->isVisible(), "a transcript that fits its viewport has no edge over its first turn");
		expect(chat.first_row_in(turn_colour()) == 1.0, "and its first turn is drawn to the viewport's top");
		const auto fits = chat.Area->viewport()->geometry();

		// A little taller than the viewport: at its end, the first turn is cut.
		chat.append(60);
		chat.append(60);
		expect(bar->maximum() > 0 && bar->value() == bar->maximum(), "a transcript taller than its viewport stays at its end");
		const auto view = chat.Area->viewport()->geometry();
		expect(edge->isVisible() && edge->geometry() == QRect{ view.left(), view.top(), view.width(), 1 },
			   "off its start the edge is one pixel on the viewport's first row, as wide as the viewport: " + std::to_string(edge->geometry().top()) + ", " + std::to_string(edge->geometry().width()) + "x"
				   + std::to_string(edge->geometry().height()));
		expect(chat.first_row_in(line) == 1.0, "and is drawn there, over the turn scrolled under it: " + std::to_string(chat.first_row_in(line)));
		expect(view == fits && chat.Area->widget()->height() == 4 * 60, "the edge takes no room: the viewport is where it was and the transcript as tall as its four turns");
		expect(chat.Area->children().indexOf(edge) > chat.Area->children().indexOf(chat.Area->viewport()), "the edge lies over the viewport");

		// No pointer and no keyboard.
		expect(chat.Area->childAt(edge->geometry().center()) != edge && edge->focusPolicy() == Qt::NoFocus, "a press on the edge reaches the turn under it, and Tab never stops on it");

		// At its start there is nothing under the edge.
		bar->setValue(0);
		settle();
		expect(!edge->isVisible() && chat.first_row_in(turn_colour()) == 1.0, "back at its start the transcript has no edge and its first turn is drawn to the top");
		bar->setValue(1);
		settle();
		expect(edge->isVisible() && chat.first_row_in(line) == 1.0, "one pixel off its start it has");

		// The viewport's own size is followed.
		chat.Outer.resize(400, 200);
		settle();
		const auto wider = chat.Area->viewport()->geometry();
		expect(edge->isVisible() && edge->geometry() == QRect{ wider.left(), wider.top(), wider.width(), 1 } && wider.width() > view.width(), "a wider drawer has a wider edge, on the viewport's first row");

		// Tall enough for every turn: nothing is scrolled under it.
		chat.Outer.resize(400, 600);
		settle();
		expect(bar->maximum() == 0 && !edge->isVisible(), "a viewport that holds every turn has no edge");
	}

	void test_blank_room_over_the_first_turn_is_not_a_turn()
	{
		// A transcript a few pixels taller than its viewport, at its end, has
		// only the room its layout keeps over the first turn scrolled away.
		auto chat = transcript{ };
		chat.Column->setContentsMargins(0, 8, 0, 0);
		for (int turn = 0; turn < 5; ++turn) {
			chat.append(60);
		}
		auto* const edge = chat.edge();
		auto* const bar  = chat.Area->verticalScrollBar();
		expect(bar->maximum() > 9 && edge->isVisible(), "at its end, with turns scrolled away, the transcript draws its edge");
		bar->setValue(0);
		settle();
		expect(!edge->isVisible(), "at its start it draws none");
		bar->setValue(8);
		settle();
		expect(!edge->isVisible(), "off its start by the room over its first turn it cuts nothing and draws none");
		bar->setValue(9);
		settle();
		expect(edge->isVisible() && chat.first_row_in(qt::transcript_edge::colour(chat.Outer.palette())) == 1.0, "one pixel of the first turn under the top and the edge is drawn over it");
	}

	void test_both_chats_draw_the_edge()
	{
		qt::workbench_window window;
		auto* const          agent = window.agent_tab()->findChild<QScrollArea*>(QStringLiteral("agentConversationScroll"));
		expect(agent != nullptr && qt::transcript_edge::of(*agent) != nullptr && qt::stick_to_bottom::of(*agent) != nullptr, "the Agent tab's conversation, which follows its end, draws its top edge");
		auto* const tour = window.tour_editor()->side_chat()->findChild<QScrollArea*>(QStringLiteral("tourSideChatScroll"));
		expect(tour != nullptr && qt::transcript_edge::of(*tour) != nullptr && qt::stick_to_bottom::of(*tour) != nullptr, "the tour's side chat, which follows its end, draws its top edge");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	test_the_edge_shows_exactly_while_turns_are_scrolled_under_it();
	test_blank_room_over_the_first_turn_is_not_a_turn();
	test_both_chats_draw_the_edge();
	std::cout << "transcript-edge:pass\n";
	return EXIT_SUCCESS;
}
