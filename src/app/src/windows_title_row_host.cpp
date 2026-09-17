#include "windows_title_row_host.hpp"

#include "bpr/qt/windows_caption_buttons.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMargins>
#include <QPointF>
#include <QWindow>
#include <QtGui/qpa/qplatformwindow_p.h>

#include <algorithm>
#include <cmath>

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

namespace bpr::app {
	namespace {
		[[nodiscard]] auto hit_code(const qt::title_row_part aPart) -> LRESULT
		{
			switch (aPart) {
			case qt::title_row_part::caption:
				return HTCAPTION;
			case qt::title_row_part::minimize:
				return HTMINBUTTON;
			case qt::title_row_part::maximize:
				return HTMAXBUTTON;
			case qt::title_row_part::close:
				return HTCLOSE;
			case qt::title_row_part::client:
				break;
			}
			return HTCLIENT;
		}

		/// \brief The caption button a non-client code names, or `client`.
		[[nodiscard]] auto button_part(const WPARAM aCode) -> qt::title_row_part
		{
			switch (aCode) {
			case HTMINBUTTON:
				return qt::title_row_part::minimize;
			case HTMAXBUTTON:
				return qt::title_row_part::maximize;
			case HTCLOSE:
				return qt::title_row_part::close;
			default:
				return qt::title_row_part::client;
			}
		}
	} // namespace

	windows_title_row_host::windows_title_row_host(qt::workbench_window& aWindow)
		: Window{ &aWindow }
		, Active{ QGuiApplication::platformName() == QStringLiteral("windows")
				  && aWindow.caption_buttons() != nullptr }
	{
		if (!Active) {
			return;
		}
		aWindow.caption_buttons()->set_dispatch([this](const qt::title_row_part aPart) {
			perform(aPart);
		});
		aWindow.installEventFilter(this);
		QCoreApplication::instance()->installNativeEventFilter(this);
		if (aWindow.windowHandle() != nullptr) {
			attach();
		}
	}

	windows_title_row_host::~windows_title_row_host()
	{
		if (!Active) {
			return;
		}
		if (auto* const application = QCoreApplication::instance(); application != nullptr) {
			application->removeNativeEventFilter(this);
		}
		if (Window != nullptr) {
			Window->removeEventFilter(this);
			if (Window->caption_buttons() != nullptr) {
				Window->caption_buttons()->set_dispatch({ });
			}
		}
	}

	auto windows_title_row_host::active() const noexcept -> bool
	{
		return Active;
	}

	auto windows_title_row_host::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == Window) {
			switch (aEvent->type()) {
			case QEvent::WinIdChange:
			case QEvent::Show:
				attach();
				break;
			case QEvent::WindowStateChange:
				apply_frame();
				if (Window->caption_buttons() != nullptr) {
					Window->caption_buttons()->set_maximized(Window->isMaximized());
				}
				break;
			default:
				break;
			}
		}
		return QObject::eventFilter(aWatched, aEvent);
	}

	void windows_title_row_host::attach()
	{
		if (Window == nullptr || Window->windowHandle() == nullptr) {
			return;
		}
		// The handle is read here, never inside the native filter: asking a
		// window for its handle while the platform is still creating it
		// re-enters creation.
		Handle = static_cast<quintptr>(Window->winId());
		apply_frame();
		if (Window->caption_buttons() != nullptr) {
			Window->caption_buttons()->set_maximized(Window->isMaximized());
		}
	}

	void windows_title_row_host::apply_frame()
	{
		if (Window == nullptr || Handle == 0U || Window->windowHandle() == nullptr) {
			return;
		}
		auto* const native = Window->windowHandle()->nativeInterface<QNativeInterface::Private::QWindowsWindow>();
		if (native == nullptr) {
			return;
		}
		// Measured on the Windows station: removing the caption and the top
		// sizing frame puts the window's top edge on the client's. A maximised
		// window already hangs its frame outside the work area, so there only the
		// caption comes off; taking the frame too would cut the strip's top rows.
		const auto hwnd    = reinterpret_cast<HWND>(Handle);
		const auto dpi     = ::GetDpiForWindow(hwnd);
		const auto frame   = ::GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
		const auto caption = ::GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
		const auto top     = ::IsZoomed(hwnd) != 0 ? caption : caption + frame;
		if (top == AppliedTop) {
			return;
		}
		AppliedTop = top;
		// Qt keeps the client's size when the margins change and moves the
		// frame instead, so every maximise and restore would take the frame's
		// difference off the window's height. The platform has already placed
		// the window where it belongs; it is put back there.
		auto       placed = RECT{ };
		const auto known  = ::GetWindowRect(hwnd, &placed) != 0;
		native->setCustomMargins(QMargins{ 0, -top, 0, 0 });
		auto now = RECT{ };
		if (known && ::GetWindowRect(hwnd, &now) != 0
			&& (now.left != placed.left || now.top != placed.top || now.right != placed.right || now.bottom != placed.bottom)) {
			static_cast<void>(::SetWindowPos(
				hwnd,
				nullptr,
				placed.left,
				placed.top,
				placed.right - placed.left,
				placed.bottom - placed.top,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED));
		}
	}

	void windows_title_row_host::present()
	{
		if (Window != nullptr && Window->caption_buttons() != nullptr) {
			Window->caption_buttons()->set_native_state(Hot, Pressed);
		}
	}

	void windows_title_row_host::perform(const qt::title_row_part aPart)
	{
		if (Handle == 0U) {
			return;
		}
		// As the system menu asks: posted, so the window changes after the
		// press that asked for it has returned.
		const auto hwnd = reinterpret_cast<HWND>(Handle);
		switch (aPart) {
		case qt::title_row_part::minimize:
			::PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
			break;
		case qt::title_row_part::maximize:
			::PostMessageW(hwnd, WM_SYSCOMMAND, ::IsZoomed(hwnd) != 0 ? SC_RESTORE : SC_MAXIMIZE, 0);
			break;
		case qt::title_row_part::close:
			::PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
			break;
		case qt::title_row_part::caption:
		case qt::title_row_part::client:
			break;
		}
	}

	auto windows_title_row_host::nativeEventFilter(const QByteArray&, void* const aMessage, qintptr* const aResult)
		-> bool
	{
		auto* const message = static_cast<MSG*>(aMessage);
		if (Window == nullptr || Handle == 0U || message == nullptr
			|| message->hwnd != reinterpret_cast<HWND>(Handle)) {
			return false;
		}
		const auto answer = [aResult](const LRESULT aValue) {
			if (aResult != nullptr) {
				*aResult = static_cast<qintptr>(aValue);
			}
			return true;
		};
		switch (message->message) {
		case WM_NCHITTEST: {
			if (!Window->isVisible()) {
				return false;
			}
			auto point = POINT{ GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam) };
			if (::ScreenToClient(message->hwnd, &point) == 0) {
				return false;
			}
			auto client = RECT{ };
			if (::GetClientRect(message->hwnd, &client) == 0 || point.x < client.left || point.y < client.top
				|| point.x >= client.right || point.y >= client.bottom) {
				// The side and bottom resize borders lie outside the client;
				// the platform answers for them.
				return false;
			}
			const auto ratio = Window->devicePixelRatio();
			// The top sizing frame is gone, so the strip's top edge keeps a
			// one-pixel band that resizes as that frame did.
			if (::IsZoomed(message->hwnd) == 0 && point.y < std::max(1, static_cast<int>(std::lround(ratio)))) {
				return answer(HTTOP);
			}
			const auto logical = QPointF{ point.x / ratio, point.y / ratio }.toPoint();
			const auto part    = Window->title_row_part_at(logical);
			if (part == qt::title_row_part::client) {
				return false;
			}
			return answer(hit_code(part));
		}
		case WM_NCMOUSEMOVE: {
			const auto part = button_part(message->wParam);
			if (part == qt::title_row_part::client) {
				if (Hot != qt::title_row_part::client) {
					Hot = qt::title_row_part::client;
					present();
				}
				return false;
			}
			if (Hot != part) {
				Hot = part;
				present();
				auto track = TRACKMOUSEEVENT{ .cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE | TME_NONCLIENT, .hwndTrack = message->hwnd, .dwHoverTime = 0U };
				static_cast<void>(::TrackMouseEvent(&track));
			}
			// The buttons are drawn by the strip; the platform's own caption
			// buttons must not track or paint over them.
			return answer(0);
		}
		case WM_NCMOUSELEAVE:
		case WM_MOUSELEAVE:
			if (Hot != qt::title_row_part::client || Pressed != qt::title_row_part::client) {
				Hot     = qt::title_row_part::client;
				Pressed = qt::title_row_part::client;
				present();
			}
			return false;
		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONDBLCLK: {
			const auto part = button_part(message->wParam);
			if (part == qt::title_row_part::client) {
				return false;
			}
			Pressed = part;
			Hot     = part;
			present();
			return answer(0);
		}
		case WM_NCLBUTTONUP: {
			const auto part   = button_part(message->wParam);
			const auto chosen = part != qt::title_row_part::client && part == Pressed;
			if (Pressed != qt::title_row_part::client) {
				Pressed = qt::title_row_part::client;
				present();
			}
			if (part == qt::title_row_part::client) {
				return false;
			}
			if (chosen) {
				perform(part);
			}
			return answer(0);
		}
		case WM_DPICHANGED:
			// The frame's metrics follow the window's DPI; they are re-read once
			// the platform has moved the window to its new size.
			QMetaObject::invokeMethod(
				this,
				[this] {
					AppliedTop = 0;
					apply_frame();
				},
				Qt::QueuedConnection);
			return false;
		default:
			return false;
		}
	}
} // namespace bpr::app
