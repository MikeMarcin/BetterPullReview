/// \file
/// \brief The Windows title row on the real `windows` platform.
///
/// \details [[BPR-0653]]. The strip is the title row: the host removes the
/// system title bar and answers the platform's hit test for the strip. These
/// checks send the window the same messages the platform sends it and read what
/// the platform does with the answers: the codes, the frame, maximise and
/// restore by double-click and by the caption buttons, minimise and close, a
/// window minimised while maximised coming back maximised, the menu control
/// standing in for the hidden menu bar, and the Windows half of RFC-BPR-0028
/// AC-01 at 1280x852 and 1920x1080: no title bar, the strip first with the
/// caption buttons in it, the client area directly beneath it and the epoch
/// line across the window. The snap layout flyout and a dragged window are the
/// platform's own responses to `HTMAXBUTTON` and `HTCAPTION`; they are the
/// attended capture's to show.

#include "../../src/app/src/windows_title_row_host.hpp"

#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/windows_caption_buttons.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractEventDispatcher>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QScreen>
#include <QToolBar>
#include <QToolButton>
#include <QWindow>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
	namespace application = bpr::application;
	namespace core        = bpr::core;
	namespace qt          = bpr::qt;

	void expect(const bool aCondition, const std::string_view aWhat)
	{
		if (!aCondition) {
			std::cerr << "windows-title-row-host-tests:failed:" << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// Processes events until the condition holds or five seconds pass.
	auto settle(const std::function<bool()>& aCondition) -> bool
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition()) {
			if (timer.elapsed() > 5'000) {
				return false;
			}
			QApplication::processEvents(QEventLoop::AllEvents, 50);
		}
		QApplication::processEvents();
		return true;
	}

	/// The platform window of a top-level widget: on the `windows` platform a
	/// window id is its `HWND`.
	auto native_handle(const QWidget& aWindow) -> HWND
	{
		return reinterpret_cast<HWND>(aWindow.winId()); // NOLINT(performance-no-int-to-ptr)
	}

	/// The screen point, in physical pixels, of a point in a widget.
	auto screen_point(const qt::workbench_window& aWindow, const QWidget& aWidget, const QPoint aLocal) -> POINT
	{
		const auto inWindow = aWidget.mapTo(&aWindow, aLocal);
		const auto ratio    = aWindow.devicePixelRatio();
		auto       point    = POINT{ static_cast<LONG>(inWindow.x() * ratio), static_cast<LONG>(inWindow.y() * ratio) };
		static_cast<void>(::ClientToScreen(native_handle(aWindow), &point));
		return point;
	}

	auto packed(const POINT aPoint) -> LPARAM
	{
		return MAKELPARAM(aPoint.x, aPoint.y);
	}

	/// Queues a non-client mouse message as the platform does and lets the
	/// event loop deliver it. Qt runs native event filters on input messages as
	/// its dispatcher takes them from the queue, not on input sent straight to
	/// the window procedure (QTBUG-67095).
	void post_input(const HWND aHandle, const UINT aMessage, const WPARAM aCode, const LPARAM aPoint)
	{
		static_cast<void>(::PostMessageW(aHandle, aMessage, aCode, aPoint));
		for (auto turn = 0; turn < 5; ++turn) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
	}

	auto hit(const HWND aHandle, const POINT aPoint) -> LRESULT
	{
		return ::SendMessageW(aHandle, WM_NCHITTEST, 0, packed(aPoint));
	}

	auto window_rect(const HWND aHandle) -> RECT
	{
		auto rect = RECT{ };
		static_cast<void>(::GetWindowRect(aHandle, &rect));
		return rect;
	}

	auto same(const RECT& aLeft, const RECT& aRight) -> bool
	{
		return aLeft.left == aRight.left && aLeft.top == aRight.top && aLeft.right == aRight.right
			&& aLeft.bottom == aRight.bottom;
	}

	auto client_origin(const HWND aHandle) -> POINT
	{
		auto origin = POINT{ 0, 0 };
		static_cast<void>(::ClientToScreen(aHandle, &origin));
		return origin;
	}

	auto work_area(const HWND aHandle) -> RECT
	{
		auto monitor = MONITORINFO{ .cbSize = sizeof(MONITORINFO) };
		static_cast<void>(::GetMonitorInfoW(::MonitorFromWindow(aHandle, MONITOR_DEFAULTTONEAREST), &monitor));
		return monitor.rcWork;
	}

	/// A file identity for the review the course opens, as the host derives one.
	auto workbench_file(const std::uint8_t aSeed) -> core::workbench_file_id
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return core::workbench_file_id{ bytes };
	}

	class close_counter final : public QObject
	{
	public:
		int Closes = 0;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::Close) {
				++Closes;
			}
			return QObject::eventFilter(aWatched, aEvent);
		}
	};
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	expect(QGuiApplication::platformName() == QStringLiteral("windows"),
		   "the course runs on the real windows platform");

	qt::workbench_window             window;
	bpr::app::windows_title_row_host host{ window };
	close_counter                    counter;
	window.installEventFilter(&counter);
	expect(host.active(), "on the windows platform the host replaces the system title bar");

	window.resize(1'280, 852);
	window.show();
	expect(settle([&window] { return window.windowHandle() != nullptr && window.windowHandle()->isExposed(); }),
		   "the window is exposed");
	const auto handle  = native_handle(window);
	auto*      strip   = window.primary_toolbar();
	auto*      buttons = window.caption_buttons();
	expect(strip != nullptr && buttons != nullptr && window.strip_menu_button() != nullptr,
		   "the strip carries the menu control and the caption buttons");
	auto* const minimize = buttons->button(qt::title_row_part::minimize);
	auto* const maximize = buttons->button(qt::title_row_part::maximize);
	auto* const close    = buttons->button(qt::title_row_part::close);
	const auto  centre   = [&window](const qt::windows_caption_button* aButton) {
		return screen_point(window, *aButton, aButton->rect().center());
	};

	// Criterion 1. No system title bar: the window's top edge is the client's,
	// and the strip is the client's first row. Home is a view, not a pull
	// request's: the title row keeps its menu control and caption buttons.
	const auto restored = window_rect(handle);
	expect(restored.top == client_origin(handle).y, "the system title bar is gone: the window's top edge is the client's");
	expect(strip->mapTo(&window, QPoint{ 0, 0 }).y() == 0 && strip->isVisible(),
		   "the strip is the first row of the window");
	expect(client_origin(handle).x > restored.left, "the side resize borders stay outside the client");
	expect(!window.menuBar()->isVisible(), "no menu bar row: the menu control stands in for it");
	expect(window.showing_home() && buttons->isVisible() && window.strip_menu_button()->isVisible(),
		   "on Home the strip keeps the menu control and the caption buttons");

	// Open a review, so the chrome above the comparison is the review's: two
	// files and every command the core offers an open workbench.
	{
		auto review  = application::workbench_projection{ };
		review.Files = std::vector<application::workbench_file_projection>{
			{ .File = workbench_file(1U), .Path = "src/a.cpp" },
			{ .File = workbench_file(2U), .Path = "src/b.cpp" },
		};
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = core::workbench_command_projection{
				.Id             = static_cast<core::workbench_command_id>(index),
				.Enabled        = true,
				.DisabledReason = core::workbench_reason_code::none,
			};
		}
		review.Commands = commands;
		window.publish(review);
	}
	expect(settle([&window] { return !window.showing_home() && window.review_bar()->isVisible(); }),
		   "a published review replaces Home with the review surface");
	expect(!window.updates_line_open() && !window.epoch_selectors()->isVisible(),
		   "a new review starts with its Updates line folded");
	// [[BPR-1130]]. The geometry course below measures the open line. Opening
	// it requires the core's projection; publishing files never unfolds it.
	auto openLine        = application::workbench_projection{ };
	openLine.UpdatesLine = core::workbench_updates_line_delta{ .State = core::workbench_updates_line::open };
	window.publish(openLine);
	expect(settle([&window] { return window.updates_line_open() && window.epoch_selectors()->isVisible(); }),
		   "the decided open Updates line is visible for native geometry checks");

	// The first point of bare strip surface along the row's middle.
	const auto barePoint = [&window, strip] {
		const auto row = strip->height() / 2;
		for (auto x = 0; x < strip->width(); ++x) {
			if (window.title_row_part_at(strip->mapTo(&window, QPoint{ x, row })) == qt::title_row_part::caption) {
				return QPoint{ x, row };
			}
		}
		return QPoint{ -1, -1 };
	};

	// Criterion 1 and the Windows half of RFC-BPR-0028 AC-01 at one window
	// size: no title bar, the strip is the window's first row and as wide as
	// it, the client area begins directly beneath it with no menu bar row, the
	// epoch line spans the window above the panes, and the strip is the row the
	// caption buttons sit in, with the platform's codes. Whether anything else
	// stands between the strip and the epoch line is the portable epoch-row
	// course's claim (qt_workbench_window, [[BPR-0940]]); the rows found there
	// are reported here, not judged.
	const auto titleRowHolds = [&](const std::string& aSize) {
		auto* const epochs     = window.epoch_selectors();
		auto* const client     = window.centralWidget();
		const auto  row        = QRect{ epochs->mapTo(&window, QPoint{ 0, 0 }), epochs->size() };
		const auto  area       = QRect{ client->mapTo(&window, QPoint{ 0, 0 }), client->size() };
		const auto  panes      = window.central_splitter()->mapTo(&window, QPoint{ 0, 0 });
		const auto  stripAt    = strip->mapTo(&window, QPoint{ 0, 0 });
		const auto  stripBelow = strip->mapTo(&window, QPoint{ 0, strip->height() }).y();
		const auto  text       = [](const QRect& aRect) {
			return std::to_string(aRect.x()) + ',' + std::to_string(aRect.y()) + ' ' + std::to_string(aRect.width())
				+ 'x' + std::to_string(aRect.height());
		};
		const auto where = " [window " + text(window.rect()) + ", strip " + text(QRect{ stripAt, strip->size() })
			+ ", client " + text(area) + ", epoch line " + text(row) + ", panes at " + std::to_string(panes.y()) + ']';
		expect(window_rect(handle).top == client_origin(handle).y,
			   "at " + aSize + " the system title bar is gone");
		expect(stripAt == QPoint{ 0, 0 } && strip->width() == window.width()
				   && strip->height() == qt::workbench_strip::height_pixels,
			   "at " + aSize + " the strip is the window's first row, as wide as the window" + where);
		expect(area.top() == stripBelow && area.left() == 0 && area.width() == window.width(),
			   "at " + aSize + " the client area begins directly beneath the strip" + where);
		expect(epochs->isVisible() && row.top() >= area.top() && row.left() == area.left()
				   && row.width() == area.width() && row.bottom() < panes.y(),
			   "at " + aSize + " the epoch line spans the window above the panes" + where);
		auto between = std::string{ };
		for (auto* const widget : epochs->parentWidget()->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
			const auto top = widget->mapTo(&window, QPoint{ 0, 0 }).y();
			if (widget != epochs && widget->isVisible() && top >= area.top() && top < row.top()) {
				between += ' ' + widget->objectName().toStdString() + '@' + std::to_string(top) + '+'
					+ std::to_string(widget->height());
			}
		}
		std::cout << "windows-title-row-host-tests:between-strip-and-epoch-line:" << aSize << ':'
				  << (between.empty() ? std::string{ " none" } : between) << '\n';
		expect(buttons->isVisible() && buttons->parentWidget() == strip
				   && close->mapTo(strip, QPoint{ close->width(), 0 }).x() == strip->width()
				   && minimize->height() == strip->height() && maximize->height() == strip->height()
				   && close->height() == strip->height()
				   && maximize->width() == qt::windows_caption_button::width_pixels,
			   "at " + aSize + " the caption buttons sit in the strip, 46 pixels wide, the row's height, close ending it");
		const auto bare = barePoint();
		expect(bare.x() >= 0, "at " + aSize + " the strip has bare surface");
		expect(hit(handle, screen_point(window, *strip, bare)) == HTCAPTION,
			   "at " + aSize + " bare strip surface answers HTCAPTION");
		expect(hit(handle, centre(minimize)) == HTMINBUTTON && hit(handle, centre(maximize)) == HTMAXBUTTON
				   && hit(handle, centre(close)) == HTCLOSE,
			   "at " + aSize + " the caption buttons answer HTMINBUTTON, HTMAXBUTTON and HTCLOSE");
		std::cout << "windows-title-row-host-tests:title-row:" << aSize << ":ok\n";
	};
	titleRowHolds("1280x852");

	// Criterion 2. The hit test: bare strip surface is the caption, the three
	// caption buttons are the platform's codes, controls are client, and the
	// top edge keeps a resize band.
	const auto bare = barePoint();
	expect(bare.x() >= 0, "the strip has bare surface");
	const auto caption = screen_point(window, *strip, bare);
	expect(hit(handle, caption) == HTCAPTION, "bare strip surface answers HTCAPTION");
	auto* const home = strip->widgetForAction(window.home_action());
	expect(home != nullptr && hit(handle, screen_point(window, *home, home->rect().center())) == HTCLIENT,
		   "a strip control answers HTCLIENT");
	auto* const menu     = window.strip_menu_button();
	const auto  menuCode = hit(handle, screen_point(window, *menu, menu->rect().center()));
	expect(menuCode == HTCLIENT, "the menu control answers HTCLIENT");
	expect(hit(handle, centre(minimize)) == HTMINBUTTON, "minimise answers HTMINBUTTON");
	expect(hit(handle, centre(maximize)) == HTMAXBUTTON, "maximise answers HTMAXBUTTON, which offers the snap layouts");
	expect(hit(handle, centre(close)) == HTCLOSE, "close answers HTCLOSE");
	expect(maximize->width() == qt::windows_caption_button::width_pixels
			   && close->mapTo(strip, QPoint{ close->width(), 0 }).x() == strip->width(),
		   "the caption buttons are 46 pixels wide and close ends the row");
	expect(hit(handle, POINT{ caption.x, client_origin(handle).y }) == HTTOP, "the strip's top edge keeps a resize band");
	expect(hit(handle, POINT{ restored.left + 2, (restored.top + restored.bottom) / 2 }) == HTLEFT,
		   "the left resize border answers HTLEFT");

	{
		const auto corner = screen_point(window, *strip, QPoint{ strip->width() - 1, strip->height() - 1 });
		expect(hit(handle, corner) == HTCLOSE && close->height() == strip->height(),
			   "close is the row's full height and reaches the window's corner");
	}

	// Non-client hover is reported back for the buttons to draw. The move is
	// handed to the native event filters exactly as the event dispatcher hands a
	// queued one, and checked before the loop turns: the host asks the platform
	// to report the leave, and with the cursor elsewhere the platform queues it
	// at once, which the next turn delivers.
	{
		auto move   = MSG{ .hwnd = handle, .message = WM_NCMOUSEMOVE, .wParam = HTMAXBUTTON, .lParam = packed(centre(maximize)) };
		auto result = qintptr{ 0 };
		expect(QAbstractEventDispatcher::instance()->filterNativeEvent(QByteArrayLiteral("windows_generic_MSG"), &move, &result),
			   "the host takes a non-client move over a caption button");
		expect(maximize->native_hot() && !minimize->native_hot() && !close->native_hot(), "hovering maximise shows it hot");
		expect(settle([maximize] { return !maximize->native_hot(); }), "leaving the caption buttons clears the hover");
	}

	// Maximised, the strip's top row is at the work area's top, not cut off, the
	// whole client lies within the work area, and the maximise button is
	// Restore.
	const auto maximizedInWorkArea = [&](const std::string_view aWhen) {
		const auto work = work_area(handle);
		expect(client_origin(handle).y == work.top,
			   std::string{ aWhen } + ", the strip's top row is at the work area's top, not cut off");
		auto client = RECT{ };
		static_cast<void>(::GetClientRect(handle, &client));
		auto bottomRight = POINT{ client.right, client.bottom };
		static_cast<void>(::ClientToScreen(handle, &bottomRight));
		expect(bottomRight.y <= work.bottom && client_origin(handle).x >= work.left && bottomRight.x <= work.right,
			   std::string{ aWhen } + ", the whole client lies within the work area");
		expect(settle([maximize] { return maximize->accessibleName() == QStringLiteral("Restore"); }),
			   std::string{ aWhen } + ", the maximise button is Restore");
	};

	// Double-click the caption maximises; double-click again restores the same
	// rectangle.
	post_input(handle, WM_NCLBUTTONDBLCLK, HTCAPTION, packed(caption));
	expect(settle([handle] { return ::IsZoomed(handle) != 0; }), "double-clicking the caption maximises");
	maximizedInWorkArea("maximised");
	const auto maximizedCaption = screen_point(window, *strip, barePoint());
	expect(hit(handle, maximizedCaption) == HTCAPTION && hit(handle, centre(maximize)) == HTMAXBUTTON,
		   "maximised, the caption and the restore button keep their codes");
	post_input(handle, WM_NCLBUTTONDBLCLK, HTCAPTION, packed(maximizedCaption));
	expect(settle([handle] { return ::IsZoomed(handle) == 0; }), "double-clicking the caption again restores");
	expect(settle([handle, restored] { return same(window_rect(handle), restored); }),
		   "restoring returns the window to the same rectangle");
	expect(window_rect(handle).top == client_origin(handle).y, "restored, the title bar stays gone");

	// A press released on the maximise button maximises; the platform draws
	// nothing of its own, so the press is the strip's.
	post_input(handle, WM_NCLBUTTONDOWN, HTMAXBUTTON, packed(centre(maximize)));
	QApplication::processEvents();
	post_input(handle, WM_NCLBUTTONUP, HTMAXBUTTON, packed(centre(maximize)));
	expect(settle([handle] { return ::IsZoomed(handle) != 0; }), "pressing maximise maximises");

	// Minimised while maximised: iconic, the window is not zoomed, so the host
	// takes the restored frame while it is away. Restored from the taskbar it
	// comes back maximised, with the strip's top row at the work area's top.
	minimize->click();
	expect(settle([handle] { return ::IsIconic(handle) != 0; }), "minimising the maximised window minimises it");
	static_cast<void>(::PostMessageW(handle, WM_SYSCOMMAND, SC_RESTORE, 0));
	expect(settle([handle] { return ::IsIconic(handle) == 0 && ::IsZoomed(handle) != 0; }),
		   "restored from the taskbar, a window minimised while maximised returns maximised");
	maximizedInWorkArea("back from the taskbar");

	// Keyboard activation of the same button restores, to the original
	// rectangle.
	maximize->click();
	expect(settle([handle] { return ::IsZoomed(handle) == 0; }), "activating Restore from the keyboard restores");
	expect(settle([handle, restored] { return same(window_rect(handle), restored); }),
		   "restored after a minimise from maximised, the window returns to the same rectangle");
	expect(window_rect(handle).top == client_origin(handle).y, "restored again, the title bar stays gone");

	// A press that leaves the button before release does nothing.
	post_input(handle, WM_NCLBUTTONDOWN, HTCLOSE, packed(centre(close)));
	post_input(handle, WM_NCLBUTTONUP, HTCAPTION, packed(caption));
	QApplication::processEvents(QEventLoop::AllEvents, 200);
	expect(counter.Closes == 0 && window.isVisible(), "a close press released elsewhere does not close");

	minimize->click();
	expect(settle([handle] { return ::IsIconic(handle) != 0; }), "minimise minimises");
	::ShowWindow(handle, SW_RESTORE);
	expect(settle([handle] { return ::IsIconic(handle) == 0; }), "the minimised window restores");
	expect(settle([handle, restored] { return same(window_rect(handle), restored); }),
		   "a minimised restored window returns to the same rectangle");

	// The menu control stands in for the menu bar's row: it opens the bar's
	// menus, and with the bar hidden every menu action that carries a shortcut
	// is also the window's, so the shortcut stays live.
	{
		auto expected = QList<QAction*>{ };
		for (auto* const action : window.menuBar()->actions()) {
			if (action->isVisible()) {
				expected.push_back(action);
			}
		}
		// [[BPR-1028]]. The menu opens with popup() and nothing waits for it:
		// the press returns with the menu showing, and closing it deletes it.
		window.strip_menu_button()->click();
		auto* popup = static_cast<QMenu*>(nullptr);
		expect(settle([&popup] {
				   popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
				   return popup != nullptr;
			   }),
			   "the menu control opens a menu");
		const auto shown  = popup->actions();
		const auto opened = QPointer<QMenu>{ popup };
		popup->close();
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		expect(!expected.isEmpty() && shown == expected,
			   "the menu control opens the menu bar's menus, in the bar's order");
		expect(settle([&opened] { return opened.isNull(); }), "the closed menu deletes itself");
		for (auto* const top : window.menuBar()->actions()) {
			expect(top->menu() != nullptr, "every entry of the menu control is one of the bar's menus");
			expect(window.strip_menu_button()->accessibleDescription().contains(top->menu()->accessibleName()),
				   "the menu control's description names each of the bar's menus");
			for (auto* const action : top->menu()->actions()) {
				expect(action->shortcut().isEmpty() || window.actions().contains(action),
					   "with the menu bar hidden, every menu shortcut stays live on the window");
			}
		}
	}

	// RFC-BPR-0028 AC-01 at 1920x1080, a window placed wholly inside the work
	// area when the screen has room for it.
	{
		const auto available = window.screen()->availableGeometry();
		const auto large     = QRect{ available.topLeft() + QPoint{ 16, 16 }, QSize{ 1'920, 1'080 } };
		if (available.contains(large)) {
			window.setGeometry(large);
			expect(settle([&window] { return window.size() == QSize{ 1'920, 1'080 }; }), "the window takes 1920x1080");
			titleRowHolds("1920x1080");
		} else {
			std::cout << "windows-title-row-host-tests:title-row:1920x1080:no-room:" << available.width() << 'x'
					  << available.height() << '\n';
		}
	}

	post_input(handle, WM_NCLBUTTONDOWN, HTCLOSE, packed(centre(close)));
	post_input(handle, WM_NCLBUTTONUP, HTCLOSE, packed(centre(close)));
	expect(settle([&counter] { return counter.Closes > 0; }), "pressing close closes the window as the system menu would");

	std::cout << "windows-title-row-host-tests:pass\n";
	return EXIT_SUCCESS;
}
