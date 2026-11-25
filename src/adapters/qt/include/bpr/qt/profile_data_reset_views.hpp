#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILE_DATA_RESET_VIEWS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROFILE_DATA_RESET_VIEWS_HPP

#include "bpr/core/profile_data_reset.hpp"

#include <QDialog>
#include <QGroupBox>
#include <QString>
#include <QStringList>

#include <array>
#include <functional>
#include <optional>

class QAction;
class QCheckBox;
class QLabel;
class QPushButton;

/// \file
/// \brief [[BPR-1602]]. Settings › Storage & privacy's Clear local data, the
/// confirmation it asks, and the dialog a profile that cannot open shows.
/// Presentation only: core decides what each category removes, what a
/// selection implies and whether a request is admitted; the desktop
/// performs it. [[BPR-2377]]: one checklist and one Clear replace a button
/// per reset (owner, 2026-11-24). The checklist is a table of one line per
/// category under a header row whose box checks or unchecks them all (owner,
/// 2026-11-24: "Checkmark, label, and description should all fit a single
/// line per option").
namespace bpr::qt {
	/// \brief The words one confirmation says.
	struct profile_data_reset_wording
	{
		QString     Title;
		QString     Lead;
		QStringList Removes;
		QStringList Keeps;
		/// The quiet line under the lists, or empty.
		QString Note;
		/// The confirming button's words.
		QString Confirm;
	};

	/// \brief [[BPR-2377]]. One category's row in the Clear list.
	struct profile_data_reset_category_words
	{
		QString Name;
		/// Its row's one line: what it clears, short enough to sit beside
		/// its name.
		QString Brief;
		/// Exactly what it clears and what stays: the row's tooltip and its
		/// box's accessible description.
		QString Line;
		/// What its confirmation lists as removed.
		QStringList Removes;
	};

	/// \brief [[BPR-2377]]. The words for one category of the Clear list.
	[[nodiscard]] auto profile_data_reset_category_words_for(core::profile_data_reset aReset) -> profile_data_reset_category_words;

	/// \brief The confirmation's words for `aReset` (with what it implies) on
	/// the profile `aProfile`.
	[[nodiscard]] auto profile_data_reset_wording_for(core::profile_data_reset_selection aReset, const QString& aProfile) -> profile_data_reset_wording;

	/// \brief [[BPR-2377]]. The acknowledgement a destructive selection asks,
	/// naming what only this computer holds; empty for one that is not.
	[[nodiscard]] auto profile_data_reset_acknowledgement(core::profile_data_reset_selection aReset) -> QString;

	/// \brief The status line a reopened window shows for what a clear did,
	/// naming each category that did not clear.
	/// \param aCopyFolder The copy folder's own name, or empty.
	[[nodiscard]] auto profile_data_reset_notice(const core::profile_data_reset_outcome& aOutcome, const QString& aCopyFolder) -> QString;

	/// \brief Asks before one clear, listing what it removes and keeps.
	/// \details Window modal, deleted on close, Cancel the default and Esc.
	/// A destructive selection adds the acknowledgement, and "Save a copy…"
	/// (on) when it removes store files; its button is enabled only once the
	/// acknowledgement is ticked. Children are named `profileResetConfirm`,
	/// `profileResetCancel`, `profileResetCopy` and `profileResetAcknowledge`.
	class profile_reset_confirmation final : public QDialog
	{
	public:
		profile_reset_confirmation(QWidget* aParent, core::profile_data_reset_selection aReset, const QString& aProfile);
		/// \brief The request this dialog's state confirms.
		[[nodiscard]] auto request() const -> core::profile_data_reset_request;
		[[nodiscard]] auto reset() const noexcept -> core::profile_data_reset_selection;
		[[nodiscard]] auto wording() const noexcept -> const profile_data_reset_wording&;
		[[nodiscard]] auto confirm_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton*;
		/// \brief Null for a selection with no store files to copy.
		[[nodiscard]] auto copy_box() const noexcept -> QCheckBox*;
		/// \brief Null for a selection that is not destructive.
		[[nodiscard]] auto acknowledge_box() const noexcept -> QCheckBox*;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void refresh_colours();
		void refresh_confirm();

		core::profile_data_reset_selection Reset;
		profile_data_reset_wording         Words;
		QLabel*                            RemovesHeading = nullptr;
		QLabel*                            KeepsHeading   = nullptr;
		QList<QLabel*>                     RemovesMarks;
		QList<QLabel*>                     KeepsMarks;
		QWidget*                           Ledger      = nullptr;
		QCheckBox*                         Copy        = nullptr;
		QCheckBox*                         Acknowledge = nullptr;
		QPushButton*                       Confirm     = nullptr;
		QPushButton*                       Cancel      = nullptr;
	};

	/// \brief Clear local data on Settings › Storage & privacy.
	/// \details A table of one line per category in core's order: its box,
	/// its name, a short line saying what it clears and its size where one is
	/// known; none checked at first. A line too long for its cell ends in "…";
	/// the row's tooltip and the box's accessible description say exactly
	/// what the category clears. The header row's box checks every category,
	/// or unchecks them when all are checked, and shows a dash when only some
	/// are. A category the checked ones imply shows checked and locked, with
	/// why first in its tooltip and description. A click anywhere on a row
	/// is a click on its box. One Clear…, enabled once something is checked,
	/// asks one confirmation for the whole selection; a confirmed request goes
	/// to the dispatch. Nothing here removes anything. Children are named
	/// `profileReset<Category>` (the box), `…Name`, `…Line`, `…Size`,
	/// `profileResetTable`, `profileResetClear` and `profileResetSelectAll`.
	class profile_reset_panel final : public QGroupBox
	{
	public:
		using dispatch = std::function<void(core::profile_data_reset_request)>;
		explicit profile_reset_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		void set_saved_update_drafts_action(QAction& aAction);
		void set_profile_label(const QString& aProfile);
		/// \brief Says why a clear did not start, or clears with empty text.
		void present_status(const QString& aStatus);
		/// \brief [[BPR-2377]]. Shows a category's measured size ("3, 17.2 GB"),
		/// or hides it with empty text.
		void               present_size(core::profile_data_reset aCategory, const QString& aSize);
		[[nodiscard]] auto check_box(core::profile_data_reset aCategory) const noexcept -> QCheckBox*;
		[[nodiscard]] auto size_label(core::profile_data_reset aCategory) const noexcept -> QLabel*;
		[[nodiscard]] auto clear_button() const noexcept -> QPushButton*;
		/// \brief The header row's box: checked when every category is,
		/// partially checked when only some are.
		[[nodiscard]] auto select_all_box() const noexcept -> QCheckBox*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief What is checked, implied categories included.
		[[nodiscard]] auto checked() const -> core::profile_data_reset_selection;
		/// \brief Checks exactly `aSelection` (and what it implies).
		void set_checked(core::profile_data_reset_selection aSelection);
		/// \brief Checks `aReset` and opens its confirmation, as Clear… does,
		/// and returns it; none for an empty selection. An entry point that
		/// names one category opens here with it checked.
		auto ask(core::profile_data_reset_selection aReset) -> profile_reset_confirmation*;

	private:
		[[nodiscard]] auto chosen() const -> core::profile_data_reset_selection;
		void               refresh();

		static constexpr auto category_count = core::profile_data_reset_categories.size();
		dispatch              Dispatch;
		QString               Profile;
		/// What the person checked, before what that implies.
		std::array<bool, category_count>       Own{ };
		std::array<QCheckBox*, category_count> Boxes{ };
		std::array<QLabel*, category_count>    Names{ };
		std::array<QLabel*, category_count>    Lines{ };
		std::array<QLabel*, category_count>    Sizes{ };
		QCheckBox*                             SelectAll = nullptr;
		QPushButton*                           Clear     = nullptr;
		QLabel*                                Status    = nullptr;
	};

	/// \brief What the person chose when a profile could not open.
	enum class profile_recovery_choice : std::uint8_t {
		quit,
		clear_caches,
		reset_everything,
	};

	/// \brief Shown when a profile cannot open, or when the application is
	/// started with `--reset-profile-data`.
	/// \details Offers Clear caches and try again (the default), Reset
	/// everything… (which asks its confirmation first) and Quit, with the
	/// cause and the start-up flag under Details.
	class profile_recovery_dialog final : public QDialog
	{
	public:
		/// \param aCause The cause's words for Details, or empty when the
		/// person asked for the dialog with the flag.
		profile_recovery_dialog(const QString& aProfile, const QString& aCause, QWidget* aParent = nullptr);
		[[nodiscard]] auto choice() const noexcept -> profile_recovery_choice;
		/// \brief The confirmed request, once a reset was chosen.
		[[nodiscard]] auto request() const -> std::optional<core::profile_data_reset_request>;
		[[nodiscard]] auto clear_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto everything_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto quit_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto details_label() const noexcept -> QLabel*;
		/// \brief Chooses as a click would; Reset everything confirms with
		/// `aEverything` (acknowledged, with or without its copy) instead of
		/// asking. For courses and automation.
		void choose(profile_recovery_choice aChoice, std::optional<core::profile_data_reset_request> aEverything = std::nullopt);

	private:
		profile_recovery_choice                         Choice = profile_recovery_choice::quit;
		std::optional<core::profile_data_reset_request> Request;
		QPushButton*                                    Clear      = nullptr;
		QPushButton*                                    Everything = nullptr;
		QPushButton*                                    Quit       = nullptr;
		QLabel*                                         Details    = nullptr;
		QString                                         Profile;
	};
}

#endif
