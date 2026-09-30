#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DESKTOP_MENU_BAR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DESKTOP_MENU_BAR_HPP

#include "bpr/application/profile_management_projection.hpp"

#include <QMenuBar>
#include <QString>

#include <functional>
#include <map>

class QAction;
class QMenu;

namespace bpr::qt {
	/// \brief The application's own menu bar for a desktop with no window.
	///
	/// \details [[BPR-0777]]. macOS uses a parentless menu bar as the application's
	/// default one, so Profiles, New Window and Quit stay reachable after the last
	/// window closes and the application stays resident (RFC-BPR-0033 B1, B2). It
	/// holds no window state and performs no effect: a gesture leaves as one of the
	/// existing typed profile intents or as the desktop's own management and quit
	/// closures. Platforms whose empty desktop quits never construct one.
	///
	/// [[BPR-0796]]. It carries the *same menus* a window's bar carries, because
	/// RFC-BPR-0033 B1 promises "the menu bar remains (File, Profiles, View and the
	/// rest)" and a reviewer reads that promise as titles across the top of the
	/// screen. The window's own presentation commands are therefore offered here
	/// and disabled, as macOS applications offer them with no document open: the
	/// close subtracts nothing from the bar. Offering them is all this bar does
	/// with them - a disabled action is connected to nothing, so the rule that this
	/// bar holds no window state and performs no effect still holds.
	class desktop_menu_bar final : public QMenuBar
	{
	public:
		desktop_menu_bar();
		/// \brief Routes profile gestures and desktop management/quit to their composition owner.
		void set_dispatch(application::profile_management_dispatch aDispatch,
						  std::function<void()> aManage, std::function<void()> aQuit);
		/// \brief Mirrors accepted profiles by identity and marks the most recently used one.
		///
		/// \details Rows keep their `QAction` identity across a rename, as the
		/// window's own Profiles menu does, so a projection cannot rebuild the menu
		/// under a reviewer who has it open.
		void               adopt(const application::profile_management_projection& aProjection,
								 core::bpr_profile_id                              aCurrentProfile);
		[[nodiscard]] auto profiles_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto new_window_action() const noexcept -> QAction*;
		[[nodiscard]] auto manage_profiles_action() const noexcept -> QAction*;
		[[nodiscard]] auto quit_action() const noexcept -> QAction*;
		/// \brief The window's presentation menu, offered with no window and disabled.
		[[nodiscard]] auto view_menu() const noexcept -> QMenu*;
		/// \brief [[BPR-1035]]. The window's Pull Request menu, offered with no window and disabled.
		[[nodiscard]] auto pull_request_menu() const noexcept -> QMenu*;
		/// \brief [[BPR-1250]]. The window's Help menu, its diagnostic commands
		/// offered with no window and disabled.
		[[nodiscard]] auto help_menu() const noexcept -> QMenu*;

	private:
		QMenu*                                   ProfilesMenu         = nullptr;
		QMenu*                                   PullRequestMenu      = nullptr;
		QMenu*                                   ViewMenu             = nullptr;
		QMenu*                                   HelpMenu             = nullptr;
		QAction*                                 NewBprWindowAction   = nullptr;
		QAction*                                 ManageProfilesAction = nullptr;
		QAction*                                 QuitProfilesAction   = nullptr;
		std::map<core::bpr_profile_id, QAction*> ProfileActions;
		core::bpr_profile_id                     CurrentBprProfile;
		application::profile_management_dispatch ProfileManagementDispatch;
		std::function<void()>                    ManageProfiles;
		std::function<void()>                    QuitProfiles;
	};
}
#endif
