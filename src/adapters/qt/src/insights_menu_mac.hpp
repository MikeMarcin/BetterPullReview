#ifndef BPR_SRC_ADAPTERS_QT_SRC_INSIGHTS_MENU_MAC_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_INSIGHTS_MENU_MAC_HPP

class QAction;
class QMenuBar;

namespace bpr::qt {
	/// \brief Registers the bar's existing command with one application-owned Cocoa item.
	/// \details Returns false off Cocoa, where the ordinary QAction remains visible.
	[[nodiscard]] auto use_native_insights_menu(QAction& aAction, QMenuBar& aBar) -> bool;
}
#endif
