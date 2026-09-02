#include "bpr/qt/device_authorization_panel.hpp"

#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		using application::device_authorization_handoff;
		using application::device_authorization_presentation;

		// Whether the core says this command may be issued right now. The panel
		// asks; it does not work the answer out from the stage, because the
		// stage is not the whole rule — a cancellation in flight forbids
		// beginning again, and a widget that reasons from "requested or
		// awaiting" offers a button the core would refuse. An unpublished
		// command is a disabled one, so a projection that arrives without this
		// command disables it rather than assuming.
		[[nodiscard]] auto enabled(
			const std::vector<core::device_authorization_command_projection>&
														aCommands,
			const core::device_authorization_command_id aId) noexcept -> bool
		{
			const auto match = std::ranges::find(
				aCommands,
				aId,
				&core::device_authorization_command_projection::Id);
			return match != aCommands.end() && match->Enabled;
		}

		[[nodiscard]] auto stage_text(
			const core::device_authorization_stage aStage) -> QString
		{
			switch (aStage) {
			case core::device_authorization_stage::idle:
				return QObject::tr("Not started");
			case core::device_authorization_stage::requested:
				return QObject::tr("Requesting a code");
			case core::device_authorization_stage::awaiting_authorization:
				return QObject::tr("Waiting for you to authorize");
			case core::device_authorization_stage::cancelling:
				return QObject::tr("Cancelling");
			case core::device_authorization_stage::authorized:
				return QObject::tr("Authorized");
			case core::device_authorization_stage::denied:
				return QObject::tr("Sign-in was denied");
			case core::device_authorization_stage::expired:
				return QObject::tr("The code expired before it was used");
			case core::device_authorization_stage::disabled:
				return QObject::tr("Device authorization is turned off for this "
								   "account");
			case core::device_authorization_stage::unavailable:
				return QObject::tr("Device authorization is unavailable here");
			case core::device_authorization_stage::cancelled:
				return QObject::tr("Cancelled");
			case core::device_authorization_stage::failed:
				return QObject::tr("Sign-in could not be completed");
			}
			return QObject::tr("Not started");
		}

		// Every one of these says what happened and what the reviewer can still
		// do, because every one of them leaves the flow live.
		[[nodiscard]] auto handoff_text(const device_authorization_handoff aHandoff)
			-> QString
		{
			switch (aHandoff) {
			case device_authorization_handoff::not_attempted:
				return { };
			case device_authorization_handoff::opened:
				return QObject::tr("Opened in your browser.");
			case device_authorization_handoff::refused:
				return QObject::tr(
					"That address is not this profile's sign-in address, so it "
					"was not opened. Type the address above yourself.");
			case device_authorization_handoff::failed:
				return QObject::tr(
					"A browser could not be opened. Enter the code at the "
					"address above on any device.");
			case device_authorization_handoff::unavailable:
				return QObject::tr(
					"This build cannot open a browser. Enter the code at the "
					"address above on any device.");
			}
			return { };
		}

		[[nodiscard]] auto validity_text(const std::uint32_t aSeconds) -> QString
		{
			if (aSeconds == 0U) {
				return { };
			}
			const auto minutes = aSeconds / 60U;
			const auto seconds = aSeconds % 60U;
			if (minutes == 0U) {
				return QObject::tr("Valid for %1 seconds").arg(seconds);
			}
			return QObject::tr("Valid for %1 min %2 s").arg(minutes).arg(seconds);
		}

		// A read-only line edit rather than a label: the reviewer has to be able
		// to select the text with the keyboard and read it with a screen reader,
		// and a label supports neither well.
		[[nodiscard]] auto make_readonly_field(
			const QString& aObjectName,
			const QString& aAccessibleName) -> QLineEdit*
		{
			auto* const field = new QLineEdit{ };
			field->setObjectName(aObjectName);
			field->setAccessibleName(aAccessibleName);
			field->setReadOnly(true);
			field->setFocusPolicy(Qt::StrongFocus);
			return field;
		}
	} // namespace

	device_authorization_panel::device_authorization_panel(
		const core::host_profile_id                        aProfile,
		application::device_authorization_gesture_dispatch aDispatch,
		QWidget* const                                     aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, Profile{ aProfile }
	{
		setObjectName(QStringLiteral("bpr-device-authorization-panel"));
		setAccessibleName(tr("Sign in with a device code"));

		HeadingLabel = new QLabel{ tr("Sign in with a device code") };
		HeadingLabel->setObjectName(
			QStringLiteral("bpr-device-authorization-heading"));

		UserCodeField = make_readonly_field(
			QStringLiteral("bpr-device-authorization-user-code"),
			tr("Device code to enter"));
		UserCodeField->setAccessibleDescription(
			tr("The code to type at the sign-in address. Empty until a code "
			   "has been asked for, and empty again at every terminal."));
		AddressField = make_readonly_field(
			QStringLiteral("bpr-device-authorization-address"),
			tr("Address to enter the code at"));
		AddressField->setAccessibleDescription(
			tr("The sign-in address this profile's code is entered at. It may "
			   "be read and typed on any device."));

		ValidityLabel = new QLabel{ };
		ValidityLabel->setObjectName(
			QStringLiteral("bpr-device-authorization-validity"));
		ValidityLabel->setAccessibleName(tr("Remaining validity"));

		HandoffLabel = new QLabel{ };
		HandoffLabel->setObjectName(
			QStringLiteral("bpr-device-authorization-handoff"));
		HandoffLabel->setAccessibleName(tr("Browser handoff"));
		HandoffLabel->setWordWrap(true);

		BeginButton  = new QPushButton{ tr("Get a code") };
		CopyButton   = new QPushButton{ tr("Copy code") };
		OpenButton   = new QPushButton{ tr("Open browser") };
		CancelButton = new QPushButton{ tr("Cancel") };
		BeginButton->setObjectName(
			QStringLiteral("bpr-device-authorization-begin"));
		CopyButton->setObjectName(QStringLiteral("bpr-device-authorization-copy"));
		OpenButton->setObjectName(QStringLiteral("bpr-device-authorization-open"));
		CancelButton->setObjectName(
			QStringLiteral("bpr-device-authorization-cancel"));
		// Named apart from the visible text. `BeginButton` relabels itself at a
		// terminal, and an accessible name that moved with it would rename a
		// stop under an assistive reader mid-flow.
		BeginButton->setAccessibleName(tr("Get a device sign-in code"));
		BeginButton->setAccessibleDescription(
			tr("Asks this host profile for a code to authorize in a browser."));
		CopyButton->setAccessibleName(tr("Copy the device sign-in code"));
		CopyButton->setAccessibleDescription(
			tr("Copies the code to the clipboard. The address is never copied "
			   "with it."));
		OpenButton->setAccessibleName(tr("Open the device sign-in address"));
		OpenButton->setAccessibleDescription(
			tr("Asks the host to open the sign-in address in a browser. The "
			   "flow stays live whether or not one opens."));
		CancelButton->setAccessibleName(tr("Cancel device sign-in"));
		CancelButton->setAccessibleDescription(
			tr("Ends the running sign-in without authorizing it."));

		auto* const fields = new QFormLayout{ };
		fields->addRow(tr("Code"), UserCodeField);
		fields->addRow(tr("Address"), AddressField);

		auto* const actions = new QHBoxLayout{ };
		actions->addWidget(BeginButton);
		actions->addWidget(CopyButton);
		actions->addWidget(OpenButton);
		actions->addWidget(CancelButton);
		actions->addStretch(1);

		auto* const layout = new QVBoxLayout{ this };
		layout->addWidget(HeadingLabel);
		layout->addLayout(fields);
		layout->addWidget(ValidityLabel);
		layout->addWidget(HandoffLabel);
		layout->addLayout(actions);

		// Reading order and tab order agree: code, address, then the actions
		// left to right, so a keyboard reviewer meets the code before the
		// button that acts on it.
		setTabOrder(UserCodeField, AddressField);
		setTabOrder(AddressField, BeginButton);
		setTabOrder(BeginButton, CopyButton);
		setTabOrder(CopyButton, OpenButton);
		setTabOrder(OpenButton, CancelButton);

		connect(BeginButton, &QPushButton::clicked, this, [this] {
			if (Dispatch) {
				Dispatch(core::begin_device_authorization_intent{
					.Profile = Profile,
				});
			}
		});
		connect(CopyButton, &QPushButton::clicked, this, [this] {
			static_cast<void>(copy_user_code());
		});
		connect(OpenButton, &QPushButton::clicked, this, [this] {
			if (Dispatch) {
				Dispatch(application::open_verification_address_gesture{ });
			}
		});
		connect(CancelButton, &QPushButton::clicked, this, [this] {
			if (Dispatch) {
				Dispatch(core::cancel_device_authorization_intent{ });
			}
		});

		apply_commands();
	}

	void device_authorization_panel::apply(
		const device_authorization_presentation& aPresentation)
	{
		++Statistics.Applications;
		if (aPresentation == Presentation) {
			// A republication that changed nothing rebuilds nothing and
			// announces nothing.
			return;
		}

		const auto previous = std::exchange(Presentation, aPresentation);
		++Statistics.Rebuilds;

		if (previous.UserCode != Presentation.UserCode) {
			UserCodeField->setText(QString::fromStdString(Presentation.UserCode));
		}
		if (previous.VerificationUri != Presentation.VerificationUri) {
			AddressField->setText(
				QString::fromStdString(Presentation.VerificationUri));
		}
		if (
			previous.RemainingValiditySeconds
			!= Presentation.RemainingValiditySeconds) {
			ValidityLabel->setText(
				validity_text(Presentation.RemainingValiditySeconds));
		}
		if (previous.Handoff != Presentation.Handoff) {
			HandoffLabel->setText(handoff_text(Presentation.Handoff));
		}

		apply_commands();
		announce(compose_status());
	}

	void device_authorization_panel::apply_commands()
	{
		// Beginning and cancelling are core commands, so their legality comes
		// from the core. Copying and opening are not: neither moves the flow,
		// and both are answerable from what is on screen.
		BeginButton->setEnabled(enabled(
			Presentation.Commands,
			core::device_authorization_command_id::begin_device_authorization));
		CancelButton->setEnabled(enabled(
			Presentation.Commands,
			core::device_authorization_command_id::cancel_device_authorization));
		CopyButton->setEnabled(!Presentation.UserCode.empty());
		OpenButton->setEnabled(!Presentation.VerificationUri.empty());

		// The one gesture that changes wording rather than only enablement: a
		// terminal flow is retried by asking for a new code, and calling that
		// "Get a code" after a denial reads as though nothing happened.
		BeginButton->setText(
			Presentation.Stage == core::device_authorization_stage::idle
				? tr("Get a code")
				: tr("Get a new code"));
	}

	void device_authorization_panel::bind_profile(
		const core::host_profile_id aProfile)
	{
		Profile = aProfile;
	}

	auto device_authorization_panel::bound_profile() const noexcept
		-> core::host_profile_id
	{
		return Profile;
	}

	auto device_authorization_panel::copy_user_code() -> bool
	{
		if (Presentation.UserCode.empty()) {
			return false;
		}
		auto* const clipboard = QApplication::clipboard();
		if (clipboard == nullptr) {
			return false;
		}
		clipboard->setText(QString::fromStdString(Presentation.UserCode));
		return true;
	}

	auto device_authorization_panel::compose_status() const -> QString
	{
		// One sentence, assembled from every part that changed, so a screen
		// reader hears the stage and the handoff together instead of twice.
		auto status = stage_text(Presentation.Stage);
		if (!Presentation.UserCode.empty()) {
			status = tr("%1. Code %2.")
						 .arg(status, QString::fromStdString(Presentation.UserCode));
		}
		const auto handoff = handoff_text(Presentation.Handoff);
		if (!handoff.isEmpty()) {
			status = QStringLiteral("%1 %2").arg(status, handoff);
		}
		return status;
	}

	void device_authorization_panel::announce(QString aText)
	{
		if (aText.isEmpty() || aText == LastAnnouncement) {
			return;
		}
		LastAnnouncement = std::move(aText);
		++Statistics.Announcements;
		const auto self  = QPointer<device_authorization_panel>{ this };
		auto       event = QAccessibleAnnouncementEvent{ this, LastAnnouncement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
		if (self == nullptr) {
			return;
		}
	}

	auto device_authorization_panel::statistics() const noexcept
		-> device_authorization_panel_statistics
	{
		return Statistics;
	}

	auto device_authorization_panel::accessible_status() const -> QString
	{
		return LastAnnouncement;
	}

	auto device_authorization_panel::user_code_field() const noexcept -> QLineEdit*
	{
		return UserCodeField;
	}

	auto device_authorization_panel::address_field() const noexcept -> QLineEdit*
	{
		return AddressField;
	}

	auto device_authorization_panel::validity_label() const noexcept -> QLabel*
	{
		return ValidityLabel;
	}

	auto device_authorization_panel::handoff_label() const noexcept -> QLabel*
	{
		return HandoffLabel;
	}

	auto device_authorization_panel::begin_button() const noexcept -> QPushButton*
	{
		return BeginButton;
	}

	auto device_authorization_panel::open_button() const noexcept -> QPushButton*
	{
		return OpenButton;
	}

	auto device_authorization_panel::copy_button() const noexcept -> QPushButton*
	{
		return CopyButton;
	}

	auto device_authorization_panel::cancel_button() const noexcept -> QPushButton*
	{
		return CancelButton;
	}
} // namespace bpr::qt
