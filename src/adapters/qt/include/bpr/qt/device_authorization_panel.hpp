#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEVICE_AUTHORIZATION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEVICE_AUTHORIZATION_PANEL_HPP

#include "bpr/application/device_authorization_projection.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>

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

		/// \brief Copies the presented user code to the system clipboard.
		///
		/// \returns Whether there was a code to copy. Nothing else about the
		/// flow changes, and the address is never copied implicitly.
		auto copy_user_code() -> bool;

		[[nodiscard]] auto statistics() const noexcept
			-> device_authorization_panel_statistics;
		[[nodiscard]] auto accessible_status() const -> QString;

		[[nodiscard]] auto user_code_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto address_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto validity_label() const noexcept -> QLabel*;
		[[nodiscard]] auto handoff_label() const noexcept -> QLabel*;
		[[nodiscard]] auto begin_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto open_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto copy_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton*;

	private:
		void               announce(QString aText);
		void               apply_commands();
		[[nodiscard]] auto compose_status() const -> QString;

		application::device_authorization_gesture_dispatch Dispatch;
		core::host_profile_id                              Profile;
		application::device_authorization_presentation     Presentation;
		device_authorization_panel_statistics              Statistics;
		QString                                            LastAnnouncement;
		QLabel*                                            HeadingLabel  = nullptr;
		QLineEdit*                                         UserCodeField = nullptr;
		QLineEdit*                                         AddressField  = nullptr;
		QLabel*                                            ValidityLabel = nullptr;
		QLabel*                                            HandoffLabel  = nullptr;
		QPushButton*                                       BeginButton   = nullptr;
		QPushButton*                                       CopyButton    = nullptr;
		QPushButton*                                       OpenButton    = nullptr;
		QPushButton*                                       CancelButton  = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DEVICE_AUTHORIZATION_PANEL_HPP
