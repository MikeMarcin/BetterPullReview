#ifndef BPR_SRC_APP_SRC_XCB_ACTIVATION_HOST_HPP
#define BPR_SRC_APP_SRC_XCB_ACTIVATION_HOST_HPP

#include <QAbstractNativeEventFilter>

#include <cstdint>

namespace bpr::app {
	/// \brief Whether an X11 event is an activation request that Qt must not
	/// take as a focus change.
	///
	/// \details [[BPR-1024]]. Qt's xcb platform asks the window manager to
	/// activate a window by sending an EWMH `_NET_ACTIVE_WINDOW` client message
	/// to the root. Qt selects StructureNotify on the root, so it receives that
	/// message too, and it treats the message as focus-in on the requested
	/// window at once. If the window manager refuses the request, Qt delivers
	/// every key to the requested window while the X server sends the keys to
	/// the window it focused. WSLg's window manager refuses every such request.
	///
	/// A request that this answers `true` for is kept from Qt. Qt then changes
	/// its focus window only on the X server's FocusIn, which is what happens
	/// anyway when the window manager grants the request. A request for the
	/// application's modal window still reaches Qt. The modal window blocks
	/// input to the windows it covers, so Qt's keys go to it whichever window
	/// holds X11 focus.
	///
	/// \param aEvent An `xcb_generic_event_t` as Qt's xcb platform delivers it.
	/// \param aActiveWindow The `_NET_ACTIVE_WINDOW` atom, or 0 when there is
	/// none, in which case nothing is kept from Qt.
	/// \param aModalWindow The X11 window of the application's modal window, or
	/// 0 when no modal window is shown.
	[[nodiscard]] auto withholds_activation_request(const void* aEvent, std::uint32_t aActiveWindow, std::uint32_t aModalWindow) noexcept -> bool;

	/// \brief Keeps Qt's focus window on the window the X server focused.
	///
	/// \details [[BPR-1024]]. The host performs. It is created once, after
	/// `QApplication`, and filters Qt's native events with
	/// `withholds_activation_request`. Only the `xcb` platform has such requests.
	/// On any other platform plugin (the offscreen tests, Windows, macOS) the
	/// host does nothing.
	class xcb_activation_host final : public QAbstractNativeEventFilter
	{
	public:
		xcb_activation_host();
		~xcb_activation_host() override;
		xcb_activation_host(const xcb_activation_host&)                    = delete;
		auto operator=(const xcb_activation_host&) -> xcb_activation_host& = delete;
		xcb_activation_host(xcb_activation_host&&)                         = delete;
		auto operator=(xcb_activation_host&&) -> xcb_activation_host&      = delete;

		/// \brief Whether the host withholds activation requests here.
		[[nodiscard]] auto active() const noexcept -> bool;

		auto nativeEventFilter(const QByteArray& aEventType, void* aMessage, qintptr* aResult) -> bool override;

	private:
		std::uint32_t ActiveWindow = 0U;
	};
} // namespace bpr::app

#endif
