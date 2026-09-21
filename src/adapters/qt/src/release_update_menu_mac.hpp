#ifndef BPR_SRC_ADAPTERS_QT_SRC_RELEASE_UPDATE_MENU_MAC_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_RELEASE_UPDATE_MENU_MAC_HPP

#include <functional>
#include <memory>

class QObject;

namespace bpr::qt {
	/// \brief Owns a single native application-menu item; returns null off Cocoa.
	[[nodiscard]] auto make_release_update_native_menu(std::function<void()> aCheck) -> std::unique_ptr<QObject>;
}
#endif
