#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_PANEL_HPP

#include "bpr/application/release_update_projection.hpp"

#include <QWidget>
#include <map>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QMenuBar;
class QProgressBar;
class QPlainTextEdit;

namespace bpr::qt {
	/// \brief Shared updater presentation for Settings and the standalone release offer.
	class release_update_panel final : public QWidget
	{
	public:
		explicit release_update_panel(QWidget* aParent = nullptr);
		void               set_dispatch(application::release_update_dispatch aDispatch);
		void               adopt(const application::release_update_projection& aProjection, bool aDurable);
		void               set_accounts(const std::vector<std::pair<std::string, std::string>>& aAccounts,
										const std::string&                                      aSelected);
		[[nodiscard]] auto action(core::release_update_command aCommand) const -> QAction*;

	private:
		application::release_update_dispatch             Dispatch;
		application::release_update_projection           Projection;
		std::map<core::release_update_command, QAction*> Actions;
		QCheckBox*                                       Automatic   = nullptr;
		QComboBox*                                       Accounts    = nullptr;
		QLabel*                                          Status      = nullptr;
		QLabel*                                          Versions    = nullptr;
		QLabel*                                          Checked     = nullptr;
		QPlainTextEdit*                                  Notes       = nullptr;
		QLabel*                                          Persistence = nullptr;
		QProgressBar*                                    Progress    = nullptr;
	};
	/// \brief Adds the same application-owned check gesture to a window or empty desktop.
	[[nodiscard]] auto add_release_update_action(QMenuBar& aMenu, std::function<void()> aCheck) -> QAction*;
}
#endif
