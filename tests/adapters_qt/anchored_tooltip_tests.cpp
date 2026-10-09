/// \file
/// \brief [[BPR-1354]]. Every tooltip the application shows sits next to the
/// control it names: directly below the control's own rectangle, left edges
/// aligned and `anchored_tooltip_gap` apart, or directly above it where below
/// would leave the screen. The owner, on [[BPR-1335]]'s v002 look: "bug: Why
/// is this damned tooltip far away again. This is a consistent problem."
///
/// Each kind of control is plain Qt, built here with nothing of the
/// application's around it, so the placement is proven for any widget, not
/// only for the surfaces that once opted in: a button with its own tooltip,
/// an item in a list, a tab, a header section, a menu action, and a button at
/// the bottom of the screen.

#include "bpr/qt/anchored_tooltip.hpp"

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QTabBar>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	auto failures = 0;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			++failures;
		}
	}

	void settle()
	{
		for (auto round = 0; round < 4; ++round) {
			QApplication::processEvents();
		}
	}

	/// \brief Sends a tooltip event at `aPoint` of `aOwner`, as Qt does after
	/// the pointer rests there, and expects `aText` in a tooltip window right
	/// next to `aAnchor` (a rectangle of `aOwner`): directly below it, left
	/// edges aligned and `anchored_tooltip_gap` apart, or directly above it
	/// where below would leave the screen; moved left only to stay on it.
	/// Returns where it went, `below` or `above`, or empty.
	auto expect_tooltip_next_to(QWidget& aOwner, const QPoint aPoint, const QRect& aAnchor, const QString& aText, const std::string& aWhat) -> std::string
	{
		qt::hide_anchored_tooltip();
		QToolTip::hideText();
		settle();
		auto help = QHelpEvent{ QEvent::ToolTip, aPoint, aOwner.mapToGlobal(aPoint) };
		QApplication::sendEvent(&aOwner, &help);
		const auto* const tip    = qobject_cast<const QLabel*>(qt::anchored_tooltip());
		const auto        anchor = QRect{ aOwner.mapToGlobal(aAnchor.topLeft()), aAnchor.size() };
		const auto        box    = tip != nullptr ? tip->geometry() : QRect{ };
		const auto*       screen = QGuiApplication::screenAt(anchor.center()) != nullptr ? QGuiApplication::screenAt(anchor.center()) : aOwner.screen();
		const auto        area   = screen->availableGeometry();
		const auto        below  = box.top() == anchor.bottom() + 1 + qt::anchored_tooltip_gap;
		const auto        above  = box.bottom() + 1 + qt::anchored_tooltip_gap == anchor.top();
		const auto        left   = std::max(area.left(), std::min(anchor.left(), area.left() + area.width() - box.width()));
		std::cout << "bpr-1354 " << aWhat << ": anchored=" << (tip != nullptr) << " qt=" << QToolTip::isVisible() << " anchor=" << anchor.x() << ',' << anchor.y()
				  << ' ' << anchor.width() << 'x' << anchor.height() << " tip=" << box.x() << ',' << box.y() << ' ' << box.width() << 'x' << box.height()
				  << (below ? " below" : above ? " above"
											   : "")
				  << '\n';
		expect(tip != nullptr && !QToolTip::isVisible(), aWhat + ": the tooltip is the anchored one, not Qt's at the pointer");
		expect(tip != nullptr && tip->text() == aText, aWhat + ": the tooltip names the control under the pointer");
		expect(tip != nullptr && (below || above) && box.left() == left && box == qt::anchored_tooltip_geometry(anchor, box.size(), area),
			   aWhat + ": the tooltip sits directly below its control (or above it at the screen's edge), left edges aligned");
		qt::hide_anchored_tooltip();
		return tip == nullptr ? std::string{ } : below ? std::string{ "below" }
			: above                                    ? std::string{ "above" }
													   : std::string{ };
	}

	/// \brief A button with its own tooltip, in a plain window no surface of
	/// the application set up.
	void test_a_buttons_tooltip()
	{
		auto window = QWidget{ };
		window.setWindowTitle(QStringLiteral("anchored tooltips: button"));
		auto* const layout = new QHBoxLayout{ &window };
		auto* const button = new QPushButton{ QStringLiteral("Refresh"), &window };
		button->setToolTip(QStringLiteral("Refresh the pull request (⌘R)"));
		layout->addWidget(button);
		layout->addStretch();
		window.move(40, 40);
		window.resize(320, 80);
		window.show();
		settle();
		// The pointer rests off-centre, as a real pointer does: the tooltip
		// still goes under the button's own rectangle, not the pointer.
		const auto rest = QPoint{ button->width() - 4, button->height() - 3 };
		expect(expect_tooltip_next_to(*button, rest, button->rect(), button->toolTip(), "button") == "below", "button: below, with room under it");

		// A long tooltip wraps at about eighty characters instead of running
		// across the window, and still sits below its control.
		button->setToolTip(QStringLiteral("Your private notes, AI conversations and review feedback. Nothing here is published; promote an item to a draft in Comments to publish it."));
		expect(expect_tooltip_next_to(*button, rest, button->rect(), button->toolTip(), "long tooltip") == "below", "long tooltip: below the button");
		{
			auto help = QHelpEvent{ QEvent::ToolTip, rest, button->mapToGlobal(rest) };
			QApplication::sendEvent(button, &help);
			const auto* const tip = qobject_cast<const QLabel*>(qt::anchored_tooltip());
			const auto        one = tip != nullptr ? tip->fontMetrics().height() : 0;
			std::cout << "bpr-1354 long tooltip: " << (tip != nullptr ? tip->width() : 0) << 'x' << (tip != nullptr ? tip->height() : 0) << " line=" << one << '\n';
			expect(tip != nullptr && tip->wordWrap() && tip->height() >= 2 * one && tip->width() <= (tip->fontMetrics().averageCharWidth() * 80) + (2 * tip->margin()),
				   "long tooltip: it wraps at about eighty characters");
			qt::hide_anchored_tooltip();
		}

		// A child with no tooltip of its own shows its parent's, anchored to
		// the parent, as Qt hands the event up.
		auto* const holder = new QWidget{ &window };
		holder->setToolTip(QStringLiteral("Holder"));
		holder->setGeometry(200, 10, 100, 60);
		auto* const inner = new QWidget{ holder };
		inner->setGeometry(10, 10, 30, 30);
		holder->show();
		inner->show();
		settle();
		const auto holderInInner = QRect{ inner->mapFrom(holder, QPoint{ 0, 0 }), holder->size() };
		expect(expect_tooltip_next_to(*inner, QPoint{ 15, 15 }, holderInInner, holder->toolTip(), "child without a tooltip") == "below",
			   "child without a tooltip: below the parent whose tooltip it is");
	}

	/// \brief An item of a list, by its `Qt::ToolTipRole`, anchored to the
	/// item's own row.
	void test_an_items_tooltip()
	{
		auto list = QListWidget{ };
		list.setWindowTitle(QStringLiteral("anchored tooltips: list"));
		for (auto index = 1; index <= 6; ++index) {
			auto* const item = new QListWidgetItem{ QStringLiteral("src/file_%1.cpp").arg(index), &list };
			item->setToolTip(QStringLiteral("src/file_%1.cpp, 3 threads").arg(index));
		}
		list.move(40, 160);
		list.resize(260, 180);
		list.show();
		settle();
		auto* const third = list.item(2);
		const auto  row   = list.visualItemRect(third);
		const auto  rest  = QPoint{ row.left() + 120, row.center().y() };
		expect(expect_tooltip_next_to(*list.viewport(), rest, row, third->toolTip(), "list item") == "below", "list item: below its row");
	}

	/// \brief A tab, by its own tab tooltip, anchored to the tab.
	void test_a_tabs_tooltip()
	{
		auto bar = QTabBar{ };
		bar.setWindowTitle(QStringLiteral("anchored tooltips: tabs"));
		bar.addTab(QStringLiteral("Conversations"));
		bar.addTab(QStringLiteral("Notes"));
		bar.setTabToolTip(0, QStringLiteral("The pull request's conversations"));
		bar.setTabToolTip(1, QStringLiteral("Your private notes"));
		bar.move(40, 380);
		bar.show();
		settle();
		const auto tab  = bar.tabRect(1);
		const auto rest = QPoint{ tab.right() - 3, tab.center().y() };
		expect(expect_tooltip_next_to(bar, rest, tab, bar.tabToolTip(1), "tab") == "below", "tab: below the tab");
	}

	/// \brief A header section, by the model's `Qt::ToolTipRole` for it,
	/// anchored to the section.
	void test_a_header_sections_tooltip()
	{
		auto table = QTableWidget{ 3, 3 };
		table.setWindowTitle(QStringLiteral("anchored tooltips: header"));
		for (auto column = 0; column < 3; ++column) {
			auto* const item = new QTableWidgetItem{ QStringLiteral("Column %1").arg(column + 1) };
			item->setToolTip(QStringLiteral("What column %1 counts").arg(column + 1));
			table.setHorizontalHeaderItem(column, item);
		}
		table.move(360, 40);
		table.resize(360, 160);
		table.show();
		settle();
		auto* const header  = table.horizontalHeader();
		const auto  section = QRect{ header->sectionViewportPosition(1), 0, header->sectionSize(1), header->viewport()->height() };
		const auto  rest    = QPoint{ section.left() + 5, section.center().y() };
		expect(expect_tooltip_next_to(*header->viewport(), rest, section, table.horizontalHeaderItem(1)->toolTip(), "header section") == "below", "header section: below the section");
	}

	/// \brief A menu action's own tooltip, in a menu that shows them, anchored
	/// to the action's row.
	void test_a_menu_actions_tooltip()
	{
		auto menu = QMenu{ };
		menu.setToolTipsVisible(true);
		static_cast<void>(menu.addAction(QStringLiteral("Copy link")));
		auto* const resolve = menu.addAction(QStringLiteral("Resolve"));
		resolve->setToolTip(QStringLiteral("Resolving needs write access to the repository"));
		menu.popup(QPoint{ 400, 260 });
		settle();
		const auto row  = menu.actionGeometry(resolve);
		const auto rest = QPoint{ row.left() + 30, row.center().y() };
		expect(expect_tooltip_next_to(menu, rest, row, resolve->toolTip(), "menu action") == "below", "menu action: below its row");
		menu.close();
	}

	/// \brief A button at the bottom of the screen: the tooltip goes directly
	/// above it, left edges aligned.
	void test_at_the_screens_bottom_edge()
	{
		auto*      screen = QGuiApplication::primaryScreen();
		const auto area   = screen->availableGeometry();
		auto       window = QWidget{ nullptr, Qt::FramelessWindowHint };
		window.setWindowTitle(QStringLiteral("anchored tooltips: edge"));
		auto* const layout = new QVBoxLayout{ &window };
		layout->setContentsMargins(0, 0, 0, 0);
		auto* const button = new QPushButton{ QStringLiteral("Publish"), &window };
		button->setToolTip(QStringLiteral("Publish the review"));
		layout->addWidget(button);
		window.resize(160, 32);
		window.move(area.left() + 60, area.top() + area.height() - 32);
		window.show();
		settle();
		expect(expect_tooltip_next_to(*button, button->rect().center(), button->rect(), button->toolTip(), "button at the bottom edge") == "above",
			   "button at the bottom edge: above it, where below would leave the screen");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(QStringLiteral("BetterPullReviewAnchoredTooltipTests"));
	test_a_buttons_tooltip();
	test_an_items_tooltip();
	test_a_tabs_tooltip();
	test_a_header_sections_tooltip();
	test_a_menu_actions_tooltip();
	test_at_the_screens_bottom_edge();
	if (failures != 0) {
		std::cerr << "anchored-tooltip-tests: " << failures << " failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "anchored-tooltip-tests:pass\n";
	return EXIT_SUCCESS;
}
