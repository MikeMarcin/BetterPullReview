/// \file
/// \brief The Windows title row on the real `windows` platform.
///
/// \details [[BPR-0653]]. The strip is the title row: the host removes the
/// system title bar and answers the platform's hit test for the strip. These
/// checks send the window the same messages the platform sends it and read what
/// the platform does with the answers: the codes, the frame, maximise and
/// restore by double-click and by the caption buttons, minimise and close. The
/// snap layout flyout and a dragged window are the platform's own responses to
/// `HTMAXBUTTON` and `HTCAPTION`; they are the attended capture's to show.

#include "../../src/app/src/windows_title_row_host.hpp"

#include "bpr/qt/windows_caption_buttons.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractEventDispatcher>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QToolBar>
#include <QToolButton>
#include <QWindow>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
	namespace qt = bpr::qt;

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

	/// The screen point, in physical pixels, of a point in a widget.
	auto screen_point(const qt::workbench_window& aWindow, const QWidget& aWidget, const QPoint aLocal) -> POINT
	{
		const auto inWindow = aWidget.mapTo(&aWindow, aLocal);
		const auto ratio    = aWindow.devicePixelRatio();
		auto       point    = POINT{ static_cast<LONG>(inWindow.x() * ratio), static_cast<LONG>(inWindow.y() * ratio) };
		static_cast<void>(::ClientToScreen(reinterpret_cast<HWND>(aWindow.winId()), &point));
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
	const auto handle  = reinterpret_cast<HWND>(window.winId());
	auto*      strip   = window.primary_toolbar();
	auto*      buttons = window.caption_buttons();
	expect(strip != nullptr && buttons != nullptr && window.strip_menu_button() != nullptr,
		   "the strip carries the menu control and the caption buttons");

	// Criterion 1. No system title bar: the window's top edge is the client's,
	// and the strip is the client's first row.
	const auto restored = window_rect(handle);
	expect(restored.top == client_origin(handle).y, "the system title bar is gone: the window's top edge is the client's");
	expect(strip->mapTo(&window, QPoint{ 0, 0 }).y() == 0 && strip->isVisible(),
		   "the strip is the first row of the window");
	expect(client_origin(handle).x > restored.left, "the side resize borders stay outside the client");

	// Criterion 2. The hit test: bare strip surface is the caption, the three
	// caption buttons are the platform's codes, controls are client, and the
	// top edge keeps a resize band.
	const auto row  = strip->height() / 2;
	auto       bare = QPoint{ -1, -1 };
	for (auto x = 0; x < strip->width(); ++x) {
		if (window.title_row_part_at(strip->mapTo(&window, QPoint{ x, row })) == qt::title_row_part::caption) {
			bare = QPoint{ x, row };
			break;
		}
	}
	expect(bare.x() >= 0, "the strip has bare surface");
	const auto caption = screen_point(window, *strip, bare);
	expect(hit(handle, caption) == HTCAPTION, "bare strip surface answers HTCAPTION");
	auto* const home = strip->widgetForAction(window.home_action());
	expect(home != nullptr && hit(handle, screen_point(window, *home, home->rect().center())) == HTCLIENT,
		   "a strip control answers HTCLIENT");
	auto* const menu     = window.strip_menu_button();
	const auto  menuCode = hit(handle, screen_point(window, *menu, menu->rect().center()));
	expect(menuCode == HTCLIENT, "the menu control answers HTCLIENT");
	const auto centre = [&window](const qt::windows_caption_button* aButton) {
		return screen_point(window, *aButton, aButton->rect().center());
	};
	auto* const minimize = buttons->button(qt::title_row_part::minimize);
	auto* const maximize = buttons->button(qt::title_row_part::maximize);
	auto* const close    = buttons->button(qt::title_row_part::close);
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

	// Double-click the caption maximises; the strip's top row stays in the work
	// area; double-click again restores the same rectangle.
	post_input(handle, WM_NCLBUTTONDBLCLK, HTCAPTION, packed(caption));
	expect(settle([handle] { return ::IsZoomed(handle) != 0; }), "double-clicking the caption maximises");
	auto monitor = MONITORINFO{ .cbSize = sizeof(MONITORINFO) };
	static_cast<void>(::GetMonitorInfoW(::MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor));
	expect(client_origin(handle).y == monitor.rcWork.top, "maximised, the strip's top row is at the work area's top, not cut off");
	{
		auto client = RECT{ };
		static_cast<void>(::GetClientRect(handle, &client));
		auto bottomRight = POINT{ client.right, client.bottom };
		static_cast<void>(::ClientToScreen(handle, &bottomRight));
		expect(bottomRight.y <= monitor.rcWork.bottom && client_origin(handle).x >= monitor.rcWork.left
				   && bottomRight.x <= monitor.rcWork.right,
			   "maximised, the whole client lies within the work area");
	}
	expect(settle([maximize] { return maximize->accessibleName() == QStringLiteral("Restore"); }),
		   "maximised, the maximise button is Restore");
	const auto maximizedCaption = screen_point(window, *strip, bare);
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
	// Keyboard activation of the same button restores.
	maximize->click();
	expect(settle([handle] { return ::IsZoomed(handle) == 0; }), "activating Restore from the keyboard restores");

	// A press that leaves the button before release does nothing.
	post_input(handle, WM_NCLBUTTONDOWN, HTCLOSE, packed(centre(close)));
	post_input(handle, WM_NCLBUTTONUP, HTCAPTION, packed(caption));
	QApplication::processEvents(QEventLoop::AllEvents, 200);
	expect(counter.Closes == 0 && window.isVisible(), "a close press released elsewhere does not close");

	minimize->click();
	expect(settle([handle] { return ::IsIconic(handle) != 0; }), "minimise minimises");
	::ShowWindow(handle, SW_RESTORE);
	expect(settle([handle] { return ::IsIconic(handle) == 0; }), "the minimised window restores");

	post_input(handle, WM_NCLBUTTONDOWN, HTCLOSE, packed(centre(close)));
	post_input(handle, WM_NCLBUTTONUP, HTCLOSE, packed(centre(close)));
	expect(settle([&counter] { return counter.Closes > 0; }), "pressing close closes the window as the system menu would");

	std::cout << "windows-title-row-host-tests:pass\n";
	return EXIT_SUCCESS;
}
