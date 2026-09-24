/// \file
/// \brief Which X11 events the activation host keeps from Qt.
///
/// \details [[BPR-1024]]. On the xcb platform Qt takes its own
/// `_NET_ACTIVE_WINDOW` request as focus-in on the requested window, even when
/// the window manager refuses it, and then sends keys to that window while the
/// X server sends them to another. These checks give the host's decision the
/// events Qt's xcb platform delivers, built as the X server sends them: the
/// request, which is kept from Qt unless it names the modal window, and every
/// other event, which reaches Qt unchanged. They also check that the host does
/// nothing on the offscreen platform the other courses run on. What the window
/// manager and Qt then do is the native course's to show on WSLg.

#include "../../src/app/src/xcb_activation_host.hpp"

#include <QApplication>
#include <QByteArray>

#include <xcb/xcb.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	constexpr auto active_window = std::uint32_t{ 377U };
	constexpr auto protocols     = std::uint32_t{ 312U };
	constexpr auto first         = std::uint32_t{ 0x600012U };
	constexpr auto second        = std::uint32_t{ 0x60002fU };

	void expect(const bool aCondition, const std::string_view aWhat)
	{
		if (!aCondition) {
			std::cerr << "xcb-activation-host-tests:failed:" << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief A client message as Qt's xcb platform receives it.
	[[nodiscard]] auto client_message(const std::uint32_t aType, const std::uint32_t aWindow, const std::uint8_t aFormat = 32U) -> xcb_client_message_event_t
	{
		auto message          = xcb_client_message_event_t{ };
		message.response_type = XCB_CLIENT_MESSAGE | 0x80U; // sent by a client
		message.format        = aFormat;
		message.window        = aWindow;
		message.type          = aType;
		// What Qt's requestActivateWindow sends: source 1, a time, the current
		// focus window.
		message.data.data32[0] = 1U;
		message.data.data32[1] = 4'242U;
		message.data.data32[2] = second;
		return message;
	}

	void test_the_decision()
	{
		using bpr::app::withholds_activation_request;
		const auto request = client_message(active_window, first);
		expect(withholds_activation_request(&request, active_window, 0U), "Qt's own activation request is kept from Qt");
		auto fromServer          = request;
		fromServer.response_type = XCB_CLIENT_MESSAGE;
		expect(withholds_activation_request(&fromServer, active_window, 0U), "so is one the X server did not mark as sent");
		expect(withholds_activation_request(&request, active_window, second), "and one for another window while a modal window is shown");
		expect(!withholds_activation_request(&request, active_window, first), "a request for the modal window reaches Qt");
		expect(!withholds_activation_request(&request, XCB_ATOM_NONE, 0U), "without the atom, nothing is kept from Qt");

		const auto protocolMessage = client_message(protocols, first);
		expect(!withholds_activation_request(&protocolMessage, active_window, 0U), "another client message reaches Qt");
		const auto narrow = client_message(active_window, first, 8U);
		expect(!withholds_activation_request(&narrow, active_window, 0U), "a message in another format is not an EWMH request");

		auto focusIn          = xcb_focus_in_event_t{ };
		focusIn.response_type = XCB_FOCUS_IN;
		focusIn.detail        = XCB_NOTIFY_DETAIL_NONLINEAR;
		focusIn.event         = first;
		expect(!withholds_activation_request(&focusIn, active_window, 0U), "the X server's FocusIn reaches Qt");
		// A FocusIn whose bytes read, as a client message, like the request:
		// only the event code tells them apart.
		auto lookalike          = request;
		lookalike.response_type = XCB_FOCUS_IN;
		expect(!withholds_activation_request(&lookalike, active_window, 0U), "only a client message is ever kept from Qt");
	}

	void test_the_host_off_xcb(int aArgc, char** aArgv)
	{
		const QApplication application{ aArgc, aArgv };
		expect(QApplication::platformName() != QStringLiteral("xcb"), "the course runs off the xcb platform");
		bpr::app::xcb_activation_host host;
		expect(!host.active(), "the host does nothing on another platform");
		auto request = client_message(active_window, first);
		expect(!host.nativeEventFilter(QByteArrayLiteral("xcb_generic_event_t"), &request, nullptr), "and keeps nothing from Qt");
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	test_the_decision();
	test_the_host_off_xcb(aArgc, aArgv);
	std::cout << "xcb-activation-host-tests:pass\n";
	return EXIT_SUCCESS;
}
