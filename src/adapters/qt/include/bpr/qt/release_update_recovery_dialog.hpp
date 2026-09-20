#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_RECOVERY_DIALOG_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_RECOVERY_DIALOG_HPP
#include "bpr/application/release_update_recovery.hpp"
#include <QDialog>
namespace bpr::qt {
	/// \brief Read-only recovery text, deliberately without publication commands.
	class release_update_recovery_dialog final : public QDialog
	{
	public:
		explicit release_update_recovery_dialog(std::vector<application::release_update_saved_draft> aDrafts, QWidget* aParent = nullptr);
	};
}
#endif
