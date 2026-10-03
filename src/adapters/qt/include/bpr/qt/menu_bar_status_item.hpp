#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MENU_BAR_STATUS_ITEM_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MENU_BAR_STATUS_ITEM_HPP

#include <QIcon>

#include <memory>

class QSystemTrayIcon;

namespace bpr::qt {
	/// \brief Dif's head as a macOS template image, 18 points at 1x and 2x.
	///
	/// \details [[BPR-1250]]. The pixels are black with alpha
	/// (`assets/status-item/`, made by `scripts/dif_status_item_template.py`) and
	/// the icon is a mask, which the cocoa platform turns into an `NSImage`
	/// template, so the menu bar draws Dif in its own colour on a light, dark or
	/// highlighted bar. The application's colour icon would stay colour there.
	/// This header, its source and the resource are in macOS graphs only: Linux
	/// and Windows keep each profile's tray with the application icon.
	[[nodiscard]] auto menu_bar_status_icon() -> QIcon;

	/// \brief The application's own menu bar status item, shown, or null where the
	/// platform offers no tray.
	///
	/// \details One per process. A profile's notification sink submits through it
	/// rather than owning a tray, so the item is not a profile's: it is there from
	/// startup and stays while the application is resident with no window. It has
	/// no menu and performs no effect.
	[[nodiscard]] auto make_menu_bar_status_item() -> std::shared_ptr<QSystemTrayIcon>;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MENU_BAR_STATUS_ITEM_HPP
