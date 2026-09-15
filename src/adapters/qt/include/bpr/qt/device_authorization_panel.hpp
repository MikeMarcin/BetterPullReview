#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEVICE_AUTHORIZATION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEVICE_AUTHORIZATION_PANEL_HPP

#include "bpr/application/device_authorization_projection.hpp"

#include <QIcon>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <functional>

class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;

namespace bpr::qt {
	/// \brief Records directly observable device-authorization panel activity.
	///
	/// \details `Applications` counts every projection handed to the panel and
	/// `Rebuilds` counts the ones that changed a field, so a test can prove the
	/// panel applies incrementally rather than repainting itself on every
	/// publication.
	struct device_authorization_panel_statistics
	{
		std::size_t Applications  = 0;
		std::size_t Rebuilds      = 0;
		std::size_t Announcements = 0;
	};

	/// \brief Presents the device-authorization course and its browser handoff.
	///
	/// \details The second way into a profile, beside the token prompt that
	/// remains the first. This panel shows a user code to read aloud, the
	/// address to read it into, how long that pair stays good, and what became
	/// of the last attempt to open a browser. It performs no effect of its own:
	/// beginning, cancelling, and asking for a browser all leave through the
	/// dispatch closure, and everything it draws arrives through
	/// `application::device_authorization_presentation`.
	///
	/// [[BPR-0773]], RFC-BPR-0032. It is drawn as a server card's account row
	/// while signing in: "Enter this code at" and the address, the code in the
	/// code font with its copy glyph, Open browser as the filled control, the
	/// validity, one status sentence, and Cancel at the right. A command the
	/// core does not allow is hidden rather than shown disabled; a terminal
	/// keeps its sentence and offers one Sign in again, and where the device
	/// flow is unavailable the manual-token way in sits in the same row.
	///
	/// What it never sees is the point. There is no access token, no refresh
	/// token, no device code, no client identifier, no endpoint, and no poll
	/// interval in the projection it is given, so there is none in the widget
	/// tree it builds and none in the accessibility text it announces. The
	/// address it displays was validated twice before it arrived — once by the
	/// wire parser against the server authority, once by the host against the
	/// compiled profile authority — and this panel opens nothing itself, which
	/// is why it can be handed an address at all.
	///
	/// A failed handoff is presented, never escalated. `refused`, `failed`, and
	/// `unavailable` each leave the code and the address exactly where they
	/// were, because the reviewer finishing on their phone is a success the
	/// panel must not stand in the way of.
	class device_authorization_panel final : public QWidget
	{
	public:
		device_authorization_panel(
			core::host_profile_id                              aProfile,
			application::device_authorization_gesture_dispatch aDispatch,
			QWidget*                                           aParent = nullptr);

		/// \brief Applies one published presentation incrementally.
		void apply(
			const application::device_authorization_presentation& aPresentation);

		/// \brief Applies one published renewal observation.
		///
		/// \details Renewal is silent while it works. The reviewer authorized
		/// once and should not be told about a scheduled timer, an exchange in
		/// flight, or a rotation that succeeded: none of those is anything for
		/// them to do. The one state that is theirs is
		/// `reenrollment_required`, which says the renewal chain is gone and
		/// only a new code can restore it -- so it is presented as a typed line
		/// beside the "Get a code" button that answers it, and nothing else
		/// here changes.
		void apply_refresh(
			const application::device_authorization_refresh_observation&
				aObservation);

		/// \brief Rebinds sign-in to the profile the composing window now names.
		///
		/// \details The panel is given a profile rather than choosing one, and
		/// a window that offers a host picker changes which one it means. The
		/// presentation is left exactly where it was, because the flow belongs
		/// to the host and not to a profile: one is running or none is, and a
		/// panel that cleared a live code because the picker moved would throw
		/// away the only copy of a code the reviewer is mid-way through typing.
		/// What changes is the profile a *new* request would name, and the core
		/// forbids beginning one while a flow is live.
		void bind_profile(core::host_profile_id aProfile);

		[[nodiscard]] auto bound_profile() const noexcept -> core::host_profile_id;

		/// \brief [[BPR-0773]]. Adopts what runs after a publication changed
		/// what the panel shows, so the window that mounts it can decide where
		/// it is drawn and what its card's header says. The observer performs
		/// no effect through the panel.
		void set_presentation_observer(std::function<void()> aObserver);
		/// \brief [[BPR-0773]]. Adopts the way in a reviewer takes when the
		/// device flow is unavailable here: the window's own Connect, which
		/// opens the composition's token prompt. Without one, no token way in
		/// is offered.
		void set_manual_token_dispatch(std::function<void()> aDispatch);

		[[nodiscard]] auto presentation() const noexcept
			-> const application::device_authorization_presentation&;
		[[nodiscard]] auto refresh_observation() const noexcept
			-> const application::device_authorization_refresh_observation&;
		/// \brief Whether a flow is being asked for, awaited or cancelled.
		[[nodiscard]] auto signing_in() const noexcept -> bool;
		/// \brief Whether the panel has anything to show: a flow that began and
		/// has not handed back to a session, or a renewal that needs a new code.
		[[nodiscard]] auto shows_flow() const noexcept -> bool;
		/// \brief Moves keyboard focus to the row's job: Open browser while
		/// there is an address, otherwise the one retry, otherwise the code.
		void focus_primary();

		/// \brief Copies the presented user code to the system clipboard.
		///
		/// \returns Whether there was a code to copy. Nothing else about the
		/// flow changes, and the address is never copied implicitly.
		auto copy_user_code() -> bool;

		[[nodiscard]] auto statistics() const noexcept
			-> device_authorization_panel_statistics;
		[[nodiscard]] auto accessible_status() const -> QString;

		[[nodiscard]] auto address_sentence_label() const noexcept -> QLabel*;
		[[nodiscard]] auto user_code_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto address_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto validity_label() const noexcept -> QLabel*;
		/// \brief The one status sentence, from the typed stage.
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		[[nodiscard]] auto handoff_label() const noexcept -> QLabel*;
		[[nodiscard]] auto renewal_label() const noexcept -> QLabel*;
		[[nodiscard]] auto begin_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto open_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto copy_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton*;
		/// \brief "Sign in with a token", shown only where the device flow is
		/// unavailable or turned off and a manual-token dispatch is adopted.
		[[nodiscard]] auto token_button() const noexcept -> QPushButton*;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void               announce(QString aText);
		void               apply_commands();
		void               apply_fonts();
		void               show_copy_glyph();
		void               notify_observer();
		[[nodiscard]] auto compose_status() const -> QString;

		application::device_authorization_gesture_dispatch    Dispatch;
		std::function<void()>                                 Observer;
		std::function<void()>                                 TokenDispatch;
		core::host_profile_id                                 Profile;
		application::device_authorization_presentation        Presentation;
		application::device_authorization_refresh_observation Refresh;
		device_authorization_panel_statistics                 Statistics;
		QString                                               LastAnnouncement;
		QIcon                                                 CopyIcon;
		QWidget*                                              AddressLine     = nullptr;
		QWidget*                                              CodeLine        = nullptr;
		QWidget*                                              ActionLine      = nullptr;
		QWidget*                                              RetryLine       = nullptr;
		QLabel*                                               AddressSentence = nullptr;
		QLineEdit*                                            UserCodeField   = nullptr;
		QLineEdit*                                            AddressField    = nullptr;
		QLabel*                                               ValidityLabel   = nullptr;
		QLabel*                                               StatusLabel     = nullptr;
		QLabel*                                               HandoffLabel    = nullptr;
		QLabel*                                               RenewalLabel    = nullptr;
		QPushButton*                                          BeginButton     = nullptr;
		QPushButton*                                          CopyButton      = nullptr;
		QPushButton*                                          OpenButton      = nullptr;
		QPushButton*                                          CancelButton    = nullptr;
		QPushButton*                                          TokenButton     = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEVICE_AUTHORIZATION_PANEL_HPP
