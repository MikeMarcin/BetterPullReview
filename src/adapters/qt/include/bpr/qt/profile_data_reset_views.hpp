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

class QCheckBox;
class QLabel;
class QPushButton;

/// \file
/// \brief [[BPR-1602]]. Settings › Storage & privacy › Troubleshooting, the
/// confirmation each reset asks, and the dialog a profile that cannot open
/// shows. Presentation only: core decides what a reset removes and whether a
/// request is admitted; the desktop performs it.
namespace bpr::qt {
	/// \brief The words one reset's confirmation says.
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

	/// \brief The confirmation's words for `aReset` on the profile `aProfile`.
	[[nodiscard]] auto profile_data_reset_wording_for(core::profile_data_reset aReset, const QString& aProfile) -> profile_data_reset_wording;

	/// \brief The status line a reopened window shows for what a reset did.
	/// \param aCopyFolder The copy folder's own name, or empty.
	[[nodiscard]] auto profile_data_reset_notice(const core::profile_data_reset_outcome& aOutcome, const QString& aCopyFolder) -> QString;

	/// \brief Asks before one reset, listing what it removes and keeps.
	/// \details Window modal, deleted on close, Cancel the default and Esc.
	/// A destructive reset adds "Save a copy…" (on) and the acknowledgement,
	/// and its button is enabled only once that is ticked. Children are named
	/// `profileResetConfirm`, `profileResetCancel`, `profileResetCopy` and
	/// `profileResetAcknowledge`.
	class profile_reset_confirmation final : public QDialog
	{
	public:
		profile_reset_confirmation(QWidget* aParent, core::profile_data_reset aReset, const QString& aProfile);
		/// \brief The request this dialog's state confirms.
		[[nodiscard]] auto request() const -> core::profile_data_reset_request;
		[[nodiscard]] auto reset() const noexcept -> core::profile_data_reset;
		[[nodiscard]] auto wording() const noexcept -> const profile_data_reset_wording&;
		[[nodiscard]] auto confirm_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton*;
		/// \brief Null for a reset that is not destructive.
		[[nodiscard]] auto copy_box() const noexcept -> QCheckBox*;
		[[nodiscard]] auto acknowledge_box() const noexcept -> QCheckBox*;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void refresh_colours();
		void refresh_confirm();

		core::profile_data_reset   Reset;
		profile_data_reset_wording Words;
		QLabel*                    RemovesHeading = nullptr;
		QLabel*                    KeepsHeading   = nullptr;
		QList<QLabel*>             RemovesMarks;
		QList<QLabel*>             KeepsMarks;
		QWidget*                   Ledger      = nullptr;
		QCheckBox*                 Copy        = nullptr;
		QCheckBox*                 Acknowledge = nullptr;
		QPushButton*               Confirm     = nullptr;
		QPushButton*               Cancel      = nullptr;
	};

	/// \brief The Troubleshooting group on Settings › Storage & privacy.
	/// \details Four rows, each a name, one short line and a button that asks
	/// its confirmation. A confirmed request goes to the dispatch; nothing
	/// here removes anything.
	class profile_reset_panel final : public QGroupBox
	{
	public:
		using dispatch = std::function<void(core::profile_data_reset_request)>;
		explicit profile_reset_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		void set_profile_label(const QString& aProfile);
		/// \brief Says why a reset did not start, or clears with empty text.
		void               present_status(const QString& aStatus);
		[[nodiscard]] auto button(core::profile_data_reset aReset) const noexcept -> QPushButton*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief Opens `aReset`'s confirmation, as its button does, and
		/// returns it.
		auto ask(core::profile_data_reset aReset) -> profile_reset_confirmation*;

	private:
		dispatch                    Dispatch;
		QString                     Profile;
		std::array<QPushButton*, 4> Buttons{ };
		QLabel*                     Status = nullptr;
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
