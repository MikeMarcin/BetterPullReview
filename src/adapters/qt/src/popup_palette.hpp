#ifndef BPR_SRC_ADAPTERS_QT_SRC_POPUP_PALETTE_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_POPUP_PALETTE_HPP

/// \file
/// \brief [[BPR-2356]]. Popup menus wear the palette of the widget that opens
/// them, not the desktop's.
namespace bpr::qt::detail {
	/// \brief Makes every popup menu of the application take its opener's
	/// palette each time it is shown.
	///
	/// \details A popup is a window, and Qt resolves a window's palette from
	/// the application palette, which is the desktop's, rather than from its
	/// parent. The appearance profile sets its palette on the main window
	/// only, so with Dark chosen on a light desktop every menu opened light.
	/// One application event filter, installed on the first call and kept
	/// for the application's life, copies the parent's palette with every
	/// role set into each popup with a parent (a `QMenu`, a submenu) as it is
	/// shown. A popup whose owner set a palette of its own keeps
	/// it. Without an application this does nothing.
	void follow_opener_palette_in_popups();
} // namespace bpr::qt::detail

#endif
