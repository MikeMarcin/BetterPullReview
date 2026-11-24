#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_RECOVERY_DIALOG_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_RECOVERY_DIALOG_HPP
#include "bpr/application/release_update_recovery.hpp"
#include <QCoreApplication>
#include <QDialog>
#include <functional>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace bpr::qt {
	/// \brief Presents inert recovery text and emits confirmed local discard intents.
	class release_update_recovery_dialog final : public QDialog
	{
		Q_DECLARE_TR_FUNCTIONS(release_update_recovery_dialog)

	public:
		using discard_dispatch = std::function<void(std::vector<application::release_update_recovery_id>)>;
		explicit release_update_recovery_dialog(std::vector<application::release_update_recovery_row> aRows,
												discard_dispatch aDiscard = { }, QWidget* aParent = nullptr);
		/// Projects a committed discard while retaining selection where possible.
		void        remove_rows(std::span<const application::release_update_recovery_id> aIds);
		void        discard_failed();
		static auto action_label(std::size_t aCount) -> QString;

	private:
		void                                                  present_selection();
		void                                                  confirm_discard(bool aAll);
		std::vector<application::release_update_recovery_row> Rows;
		discard_dispatch                                      Discard;
		QListWidget*                                          List;
		QPlainTextEdit*                                       Body;
		QLabel*                                               Status;
		QPushButton*                                          Copy;
		QPushButton*                                          DiscardSelected;
		QPushButton*                                          DiscardAll;
		bool                                                  Busy = false;
	};
}
#endif
