#include "xcb_activation_host.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QString>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>

#include <cstdlib>
#include <memory>
#include <string_view>

namespace bpr::app {
	namespace {
		/// \brief `_NET_ACTIVE_WINDOW` on Qt's own X11 connection, or 0 off xcb.
		[[nodiscard]] auto active_window_atom() -> std::uint32_t
		{
#if QT_CONFIG(xcb)
			if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
				return XCB_ATOM_NONE;
			}
			const auto* const x11        = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
			auto* const       connection = x11 != nullptr ? x11->connection() : nullptr;
			if (connection == nullptr) {
				return XCB_ATOM_NONE;
			}
			constexpr auto name   = std::string_view{ "_NET_ACTIVE_WINDOW" };
			const auto     cookie = xcb_intern_atom(connection, 0U, static_cast<std::uint16_t>(name.size()), name.data());
			const auto     reply  = std::unique_ptr<xcb_intern_atom_reply_t, decltype(&std::free)>{
				xcb_intern_atom_reply(connection, cookie, nullptr), &std::free
			};
			return reply != nullptr ? reply->atom : std::uint32_t{ XCB_ATOM_NONE };
#else
			// A Qt without its xcb platform never sends the request.
			return XCB_ATOM_NONE;
#endif
		}

		/// \brief The X11 window of the application's modal window, or 0.
		[[nodiscard]] auto modal_window() -> std::uint32_t
		{
			const auto* const modal = QGuiApplication::modalWindow();
			return modal != nullptr ? static_cast<std::uint32_t>(modal->winId()) : 0U;
		}
	} // namespace

	auto withholds_activation_request(const void* const aEvent, const std::uint32_t aActiveWindow, const std::uint32_t aModalWindow) noexcept -> bool
	{
		const auto* const event = static_cast<const xcb_generic_event_t*>(aEvent);
		// The high bit only says that a client sent the event.
		if (aActiveWindow == XCB_ATOM_NONE || (event->response_type & 0x7fU) != XCB_CLIENT_MESSAGE) {
			return false;
		}
		const auto* const message = static_cast<const xcb_client_message_event_t*>(aEvent);
		return message->format == 32U && message->type == aActiveWindow && message->window != aModalWindow;
	}

	xcb_activation_host::xcb_activation_host()
		: ActiveWindow{ active_window_atom() }
	{
		if (active()) {
			QCoreApplication::instance()->installNativeEventFilter(this);
		}
	}

	xcb_activation_host::~xcb_activation_host()
	{
		if (auto* const application = QCoreApplication::instance(); active() && application != nullptr) {
			application->removeNativeEventFilter(this);
		}
	}

	auto xcb_activation_host::active() const noexcept -> bool
	{
		return ActiveWindow != XCB_ATOM_NONE;
	}

	auto xcb_activation_host::nativeEventFilter(const QByteArray& aEventType, void* const aMessage, qintptr* const /*aResult*/) -> bool
	{
		return active() && aEventType == QByteArrayLiteral("xcb_generic_event_t")
			&& withholds_activation_request(aMessage, ActiveWindow, modal_window());
	}
} // namespace bpr::app
