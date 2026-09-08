#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILE_MANAGEMENT_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILE_MANAGEMENT_WINDOW_HPP

#include "bpr/application/profile_management_projection.hpp"

#include <QString>
#include <QWidget>

#include <optional>

class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace bpr::qt {
	/// \brief Modeless profile management; all durable changes leave as typed intents.
	class profile_management_window final : public QWidget
	{
	public:
		explicit profile_management_window(QWidget* aParent = nullptr);
		void set_dispatch(application::profile_management_dispatch aDispatch);
		/// \brief Incrementally mirrors accepted profiles without replacing rejected edit text.
		void               adopt(const application::profile_management_projection& aProjection);
		void               present();
		[[nodiscard]] auto profiles() const noexcept -> QListWidget*;
		[[nodiscard]] auto name_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto create_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto rename_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto open_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto status_text() const -> QString;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		bool eventFilter(QObject* aObject, QEvent* aEvent) override;

	private:
		void               select_profile();
		void               submit_edit(bool aCreate);
		void               open_selected();
		void               update_actions();
		[[nodiscard]] auto selected_profile() const -> core::bpr_profile_id;

		application::profile_management_dispatch                 Dispatch;
		QListWidget*                                             Profiles          = nullptr;
		QLineEdit*                                               Name              = nullptr;
		QLabel*                                                  Status            = nullptr;
		QPushButton*                                             Create            = nullptr;
		QPushButton*                                             Rename            = nullptr;
		QPushButton*                                             Open              = nullptr;
		bool                                                     Ready             = false;
		bool                                                     Busy              = false;
		bool                                                     Closing           = false;
		bool                                                     PreserveCandidate = false;
		std::optional<application::profile_management_operation> PendingEdit;
		core::bpr_profile_id                                     PendingProfile;
		QString                                                  SubmittedLabel;
	};
}
#endif
