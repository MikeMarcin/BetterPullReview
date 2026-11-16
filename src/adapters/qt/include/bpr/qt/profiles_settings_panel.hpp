#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILES_SETTINGS_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILES_SETTINGS_PANEL_HPP

#include "bpr/application/profile_management_projection.hpp"

#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
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
	/// refusal that has a live window to that window, and every surface must say
	/// the same thing about the same refusal.
	[[nodiscard]] auto profile_management_failure_text(application::profile_management_failure aFailure) -> QString;

	/// \brief [[BPR-2198]]. Settings › Profiles: every profile, edited in place,
	/// and the profile's settings shared as a file.
	///
	/// \details The owner, 2026-11-15: profiles are managed in Settings, and the
	/// Manage Profiles window goes away. The page keeps that window's model and
	/// actions (RFC-BPR-0033): one row per profile with a letter avatar, the name
	/// and its state; the current profile is selected; Return and double-click
	/// open the selected profile or focus it when it is open; the filled button
	/// says Open for a closed profile and New window for an open one. Rename and
	/// New profile edit in place with Save and Cancel; Escape cancels and keeps
	/// the name. A failure is one notice with Try again, absent otherwise. Every
	/// durable change still leaves as one of the existing typed intents.
	///
	/// Below the list, Share profile settings carries Import Profile Settings…
	/// and Export Profile Settings… when the composition gives them a handler; a
	/// Settings window with no profile runtime behind it (the desktop's, shown
	/// when no window is open) has no profile to import into, and leaves the
	/// section out.
	class profiles_settings_panel final : public QWidget
	{
	public:
		explicit profiles_settings_panel(QWidget* aParent = nullptr);
		void set_dispatch(application::profile_management_dispatch aDispatch);
		/// \brief What Import Profile Settings… and Export Profile Settings… do.
		///
		/// \details Empty handlers hide Share profile settings.
		void set_share_handlers(std::function<void()> aImport, std::function<void()> aExport);
		/// \brief Says what the last import or export did, under the buttons.
		void present_exchange(const QString& aSaid);
		/// \brief Incrementally mirrors accepted profiles without replacing rejected edit text.
		void adopt(const application::profile_management_projection& aProjection);
		/// \brief How many windows each open profile has.
		///
		/// \details The projection carries a runtime state and no count, so the
		/// composition that owns the windows supplies the number a row says.
		void set_window_counts(const std::map<core::bpr_profile_id, int>& aCounts);
		/// \brief The profile this Settings window belongs to: its row is
		/// selected when nothing else is, and its name is set heavier.
		void set_current_profile(core::bpr_profile_id aProfile);
		/// \brief Selects the current profile's row and takes focus, as
		/// opening the page from Manage Profiles… does.
		void focus_profiles();

		[[nodiscard]] auto profiles() const noexcept -> QListWidget* { return Profiles; }
		/// \brief The in-place editor, which exists only while renaming or creating.
		[[nodiscard]] auto name_editor() const noexcept -> QLineEdit* { return Name; }
		/// \brief "New profile": appends a row already in its editor.
		[[nodiscard]] auto create_button() const noexcept -> QPushButton* { return Create; }
		/// \brief "Rename": turns the selected row's name into the editor.
		[[nodiscard]] auto rename_button() const noexcept -> QPushButton* { return Rename; }
		/// \brief The filled action: Open, or New window for an open profile.
		[[nodiscard]] auto open_button() const noexcept -> QPushButton* { return Open; }
		[[nodiscard]] auto save_button() const noexcept -> QPushButton* { return Save; }
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton* { return Cancel; }
		/// \brief The failure notice's retry; shown only while a failure is shown.
		[[nodiscard]] auto try_again_button() const noexcept -> QPushButton* { return TryAgain; }
		[[nodiscard]] auto import_button() const noexcept -> QPushButton* { return ImportButton; }
		[[nodiscard]] auto export_button() const noexcept -> QPushButton* { return ExportButton; }
		[[nodiscard]] auto exchange_note() const noexcept -> QLabel* { return ExchangeNote; }
		/// \brief The failure sentence, empty when no failure is shown.
		[[nodiscard]] auto status_text() const -> QString;
		/// \brief Whether a row is being renamed or created right now.
		[[nodiscard]] auto editing() const noexcept -> bool { return Mode != edit_mode::none; }

		static constexpr int row_height  = 44;
		static constexpr int list_radius = 8;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		bool eventFilter(QObject* aObject, QEvent* aEvent) override;

	private:
		enum class edit_mode : std::uint8_t {
			none,
			rename,
			create
		};
		void               select_current();
		void               refresh_states();
		void               begin_rename();
		void               begin_create();
		void               commit_edit();
		void               cancel_edit();
		void               end_edit();
		void               open_selected();
		void               activate_open();
		void               update_actions();
		void               place_editor();
		void               fit_list();
		[[nodiscard]] auto pending_row() const -> QListWidgetItem*;
		[[nodiscard]] auto selected_profile() const -> core::bpr_profile_id;
		[[nodiscard]] auto selected_is_open() const -> bool;

		application::profile_management_dispatch                 Dispatch;
		std::function<void()>                                    Import;
		std::function<void()>                                    Export;
		QListWidget*                                             Profiles     = nullptr;
		QLineEdit*                                               Name         = nullptr;
		QWidget*                                                 Notice       = nullptr;
		QLabel*                                                  NoticeText   = nullptr;
		QPushButton*                                             TryAgain     = nullptr;
		QPushButton*                                             Create       = nullptr;
		QPushButton*                                             Rename       = nullptr;
		QPushButton*                                             Open         = nullptr;
		QPushButton*                                             Save         = nullptr;
		QPushButton*                                             Cancel       = nullptr;
		QWidget*                                                 Share        = nullptr;
		QPushButton*                                             ImportButton = nullptr;
		QPushButton*                                             ExportButton = nullptr;
		QLabel*                                                  ExchangeNote = nullptr;
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
