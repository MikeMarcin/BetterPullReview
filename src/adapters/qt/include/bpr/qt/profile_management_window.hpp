#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILE_MANAGEMENT_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILE_MANAGEMENT_WINDOW_HPP

#include "bpr/application/profile_management_projection.hpp"

#include <QString>
#include <QWidget>

#include <cstdint>
#include <map>
#include <optional>

class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace bpr::qt {
	/// \brief One sentence per profile-management refusal.
	///
	/// \details [[BPR-0777]]. Shared with the windows, because RFC-BPR-0033 routes a
	/// refusal that has a live window to that window instead of presenting the
	/// chooser, and both surfaces must say the same thing about the same refusal.
	[[nodiscard]] auto profile_management_failure_text(application::profile_management_failure aFailure) -> QString;

	/// \brief [[BPR-0778]]. The Profiles sheet: one row per profile, edited in place.
	///
	/// \details RFC-BPR-0033. A letter avatar, the profile name and its state at the
	/// right; the current profile is selected when the sheet opens; Return and
	/// double-click open the selected profile, or focus it when it is already open,
	/// while the footer's one filled button says Open for a closed profile and New
	/// window for an open one. Rename and New profile edit in place with Save and
	/// Cancel in the footer's place; Escape cancels and keeps the name. A failure is
	/// one notice with Try again above the rows and is absent otherwise: there is no
	/// permanent name field, no permanent status sentence and no Reload button. Every
	/// durable change still leaves as one of the existing typed intents.
	class profile_management_window final : public QWidget
	{
	public:
		explicit profile_management_window(QWidget* aParent = nullptr);
		void set_dispatch(application::profile_management_dispatch aDispatch);
		/// \brief Incrementally mirrors accepted profiles without replacing rejected edit text.
		void adopt(const application::profile_management_projection& aProjection);
		/// \brief How many windows each open profile has.
		///
		/// \details The projection carries a runtime state and no count, and
		/// RFC-BPR-0033 changes neither the core nor the application host, so the
		/// composition that already owns the windows supplies the number a row says.
		/// Qt counts nothing itself.
		void set_window_counts(const std::map<core::bpr_profile_id, int>& aCounts);
		/// \brief The profile whose row is selected when the sheet opens.
		void set_current_profile(core::bpr_profile_id aProfile);
		void present();

		[[nodiscard]] auto profiles() const noexcept -> QListWidget*;
		/// \brief The in-place editor, which exists only while renaming or creating.
		[[nodiscard]] auto name_editor() const noexcept -> QLineEdit*;
		/// \brief "New profile": appends a row already in its editor.
		[[nodiscard]] auto create_button() const noexcept -> QPushButton*;
		/// \brief "Rename": turns the selected row's name into the editor.
		[[nodiscard]] auto rename_button() const noexcept -> QPushButton*;
		/// \brief The footer's filled action: Open, or New window for an open profile.
		[[nodiscard]] auto open_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto save_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton*;
		/// \brief The failure notice's retry; shown only while a failure is shown.
		[[nodiscard]] auto try_again_button() const noexcept -> QPushButton*;
		/// \brief The failure sentence, empty when no failure is shown.
		[[nodiscard]] auto status_text() const -> QString;
		/// \brief Whether a row is being renamed or created right now.
		[[nodiscard]] auto editing() const noexcept -> bool;

		static constexpr int row_height   = 44;
		static constexpr int sheet_width  = 440;
		static constexpr int sheet_height = 360;
		static constexpr int list_radius  = 8;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		bool eventFilter(QObject* aObject, QEvent* aEvent) override;

	private:
		enum class edit_mode : std::uint8_t {
			none,
			rename,
			create
		};
		void               select_profile();
		void               begin_rename();
		void               begin_create();
		void               commit_edit();
		void               cancel_edit();
		void               end_edit();
		void               open_selected();
		void               activate_footer();
		void               update_actions();
		void               place_editor();
		[[nodiscard]] auto pending_row() const -> QListWidgetItem*;
		[[nodiscard]] auto selected_profile() const -> core::bpr_profile_id;
		[[nodiscard]] auto selected_is_open() const -> bool;

		application::profile_management_dispatch                 Dispatch;
		QListWidget*                                             Profiles   = nullptr;
		QLineEdit*                                               Name       = nullptr;
		QWidget*                                                 Notice     = nullptr;
		QLabel*                                                  NoticeText = nullptr;
		QPushButton*                                             TryAgain   = nullptr;
		QPushButton*                                             Create     = nullptr;
		QPushButton*                                             Rename     = nullptr;
		QPushButton*                                             Open       = nullptr;
		QPushButton*                                             Save       = nullptr;
		QPushButton*                                             Cancel     = nullptr;
		std::map<core::bpr_profile_id, int>                      WindowCounts;
		core::bpr_profile_id                                     CurrentProfile;
		edit_mode                                                Mode              = edit_mode::none;
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
