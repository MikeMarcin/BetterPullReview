#include "bpr/qt/device_authorization_panel.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include "this_machine_text.hpp"

#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		using application::device_authorization_handoff;
		using application::device_authorization_presentation;
		using application::device_authorization_refresh_observation;
		using application::device_authorization_refresh_state;

		// RFC-BPR-0032: the user code in the code font at 1.6 times the row's
		// size, and a "Copied" said in place for as long as it takes to read.
		constexpr auto code_scale         = 1.6;
		constexpr auto copied_duration_ms = 1500;

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

		// One sentence per typed stage, in the words the account row says.
		[[nodiscard]] auto stage_text(
			const core::device_authorization_stage aStage) -> QString
		{
			switch (aStage) {
			case core::device_authorization_stage::idle:
				return QObject::tr("Not started.");
			case core::device_authorization_stage::requested:
				return QObject::tr("Asking for a code.");
			case core::device_authorization_stage::awaiting_authorization:
				return QObject::tr("Waiting for the browser to authorize %1.")
					.arg(detail::this_machine_text());
			case core::device_authorization_stage::cancelling:
				return QObject::tr("Cancelling.");
			case core::device_authorization_stage::authorized:
				return QObject::tr("Authorized.");
			case core::device_authorization_stage::denied:
				return QObject::tr("Sign-in was denied.");
			case core::device_authorization_stage::expired:
				return QObject::tr("The code expired before it was used.");
			case core::device_authorization_stage::disabled:
				return QObject::tr("Device sign-in is turned off for this account.");
			case core::device_authorization_stage::unavailable:
				return QObject::tr("Device sign-in is unavailable here.");
			case core::device_authorization_stage::cancelled:
				return QObject::tr("Sign-in was cancelled.");
			case core::device_authorization_stage::failed:
				return QObject::tr("Sign-in could not be completed.");
			}
			return QObject::tr("Not started.");
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
				// [[BPR-0797]]. What this knows is that the launch was handed
				// to the desktop and accepted. It does not know that a browser
				// drew anything: the host's desktop open call returns when the
				// request is dispatched, and the window it asks for is a window
				// on somebody's screen, which this process cannot see. The
				// sentence used to say "Opened in your browser." and the owner
				// read it while nothing opened, so it now says what happened
				// and leaves the same way out the other three leave -- the code
				// and the address are already on the screen above it.
				return QObject::tr(
					"Your browser was asked to open the page. If it did not, "
					"enter the code at the address above on any device.");
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

		// Three of the four states say nothing. Renewal is a promise the
		// application keeps on its own, and a panel that narrated it would be
		// asking the reviewer to supervise a token they are never shown.
		[[nodiscard]] auto renewal_text(
			const device_authorization_refresh_state aState) -> QString
		{
			switch (aState) {
			case device_authorization_refresh_state::idle:
			case device_authorization_refresh_state::scheduled:
			case device_authorization_refresh_state::refreshing:
				return { };
			case device_authorization_refresh_state::reenrollment_required:
				return QObject::tr(
					"This sign-in can no longer renew itself. Get a code to "
					"sign in again.");
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
		// and a label supports neither well. Frameless, because in the account
		// row it reads as text, not as a field to fill in.
		[[nodiscard]] auto make_readonly_field(
			const QString& aObjectName,
			const QString& aAccessibleName) -> QLineEdit*
		{
			auto* const field = new QLineEdit{ };
			field->setObjectName(aObjectName);
			field->setAccessibleName(aAccessibleName);
			field->setReadOnly(true);
			field->setFrame(false);
			field->setFocusPolicy(Qt::StrongFocus);
			return field;
		}

		// No field truncates what it holds: each is exactly as wide as its text
		// in its own font, with the line edit's own margins on both sides.
		void fit_to_text(QLineEdit* const aField)
		{
			const auto margins = aField->textMargins();
			const auto advance = QFontMetrics{ aField->font() }.horizontalAdvance(aField->text());
			aField->setFixedWidth(std::max(1, advance + margins.left() + margins.right() + 8));
		}

		[[nodiscard]] auto make_line(QWidget* const aParent) -> std::pair<QWidget*, QHBoxLayout*>
		{
			auto* const line   = new QWidget{ aParent };
			auto* const layout = new QHBoxLayout{ line };
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(8);
			return { line, layout };
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

		AddressSentence = new QLabel{ tr("Enter this code at") };
		AddressSentence->setObjectName(
			QStringLiteral("bpr-device-authorization-address-sentence"));
		AddressSentence->setTextFormat(Qt::PlainText);

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
		ValidityLabel->setForegroundRole(QPalette::PlaceholderText);

		StatusLabel = new QLabel{ };
		StatusLabel->setObjectName(
			QStringLiteral("bpr-device-authorization-status"));
		StatusLabel->setAccessibleName(tr("Sign-in status"));
		StatusLabel->setTextFormat(Qt::PlainText);
		StatusLabel->setWordWrap(true);
		StatusLabel->setForegroundRole(QPalette::PlaceholderText);

		HandoffLabel = new QLabel{ };
		HandoffLabel->setObjectName(
			QStringLiteral("bpr-device-authorization-handoff"));
		HandoffLabel->setAccessibleName(tr("Browser handoff"));
		HandoffLabel->setWordWrap(true);
		HandoffLabel->setForegroundRole(QPalette::PlaceholderText);

		RenewalLabel = new QLabel{ };
		RenewalLabel->setObjectName(
			QStringLiteral("bpr-device-authorization-renewal"));
		RenewalLabel->setAccessibleName(tr("Sign-in renewal"));
		RenewalLabel->setWordWrap(true);

		BeginButton  = new QPushButton{ tr("Sign in") };
		CopyButton   = new QPushButton{ };
		OpenButton   = new quiet_push_button{ tr("Open browser") };
		CancelButton = new quiet_push_button{ tr("Cancel") };
		TokenButton  = new quiet_push_button{ tr("Sign in with a token") };
		BeginButton->setObjectName(
			QStringLiteral("bpr-device-authorization-begin"));
		CopyButton->setObjectName(QStringLiteral("bpr-device-authorization-copy"));
		OpenButton->setObjectName(QStringLiteral("bpr-device-authorization-open"));
		CancelButton->setObjectName(
			QStringLiteral("bpr-device-authorization-cancel"));
		TokenButton->setObjectName(
			QStringLiteral("bpr-device-authorization-token"));
		for (auto* const button : { BeginButton, CopyButton, OpenButton, CancelButton, TokenButton }) {
			button->setAutoDefault(false);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		// The row's job while a code is live, and so its one filled control.
		OpenButton->setDefault(true);
		CancelButton->setFlat(true);
		CopyButton->setFlat(true);
		// Named apart from the visible text. `BeginButton` relabels itself at a
		// terminal, and an accessible name that moved with it would rename a
		// stop under an assistive reader mid-flow.
		BeginButton->setAccessibleName(tr("Get a device sign-in code"));
		BeginButton->setAccessibleDescription(
			tr("Asks this host profile for a code to authorize in a browser."));
		// A glyph is never the only label: the name, the tooltip and the
		// "Copied" said in place all say what it does.
		CopyButton->setAccessibleName(tr("Copy code"));
		CopyButton->setToolTip(tr("Copy code"));
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
		TokenButton->setAccessibleDescription(
			tr("Signs in with a token instead, through the token prompt this "
			   "host already offers."));
		CopyIcon = QIcon::fromTheme(QIcon::ThemeIcon::EditCopy);

		auto [addressLine, addressLayout] = make_line(this);
		AddressLine                       = addressLine;
		addressLayout->setSpacing(4);
		addressLayout->addWidget(AddressSentence);
		addressLayout->addWidget(AddressField);
		addressLayout->addStretch(1);

		auto [codeLine, codeLayout] = make_line(this);
		CodeLine                    = codeLine;
		codeLayout->addWidget(UserCodeField);
		codeLayout->addWidget(CopyButton);
		codeLayout->addStretch(1);

		auto [actionLine, actionLayout] = make_line(this);
		ActionLine                      = actionLine;
		actionLayout->addWidget(OpenButton);
		actionLayout->addStretch(1);
		actionLayout->addWidget(ValidityLabel);

		auto [retryLine, retryLayout] = make_line(this);
		RetryLine                     = retryLine;
		retryLayout->addWidget(BeginButton);
		retryLayout->addWidget(TokenButton);
		retryLayout->addStretch(1);

		auto* const layout = new QGridLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setHorizontalSpacing(12);
		layout->setVerticalSpacing(6);
		layout->addWidget(AddressLine, 0, 0);
		layout->addWidget(CancelButton, 0, 1, Qt::AlignRight | Qt::AlignTop);
		layout->addWidget(CodeLine, 1, 0, 1, 2);
		layout->addWidget(ActionLine, 2, 0, 1, 2);
		layout->addWidget(StatusLabel, 3, 0, 1, 2);
		layout->addWidget(HandoffLabel, 4, 0, 1, 2);
		layout->addWidget(RenewalLabel, 5, 0, 1, 2);
		layout->addWidget(RetryLine, 6, 0, 1, 2);
		layout->setColumnStretch(0, 1);

		// Reading order and tab order agree: the address, the code and its
		// copy, the browser, then the row's way out and its retries, so a
		// keyboard reviewer meets the code before the button that acts on it.
		setTabOrder(AddressField, UserCodeField);
		setTabOrder(UserCodeField, CopyButton);
		setTabOrder(CopyButton, OpenButton);
		setTabOrder(OpenButton, CancelButton);
		setTabOrder(CancelButton, BeginButton);
		setTabOrder(BeginButton, TokenButton);

		connect(BeginButton, &QPushButton::clicked, this, [this] {
			if (Dispatch) {
				Dispatch(core::begin_device_authorization_intent{
					.Profile = Profile,
				});
			}
		});
		connect(CopyButton, &QPushButton::clicked, this, [this] {
			if (!copy_user_code()) {
				return;
			}
			CopyButton->setIcon(QIcon{ });
			CopyButton->setText(tr("Copied"));
			QTimer::singleShot(copied_duration_ms, this, [this] { show_copy_glyph(); });
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
		connect(TokenButton, &QPushButton::clicked, this, [this] {
			if (TokenDispatch) {
				TokenDispatch();
			}
		});

		apply_fonts();
		show_copy_glyph();
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
			fit_to_text(UserCodeField);
		}
		if (previous.VerificationUri != Presentation.VerificationUri) {
			AddressField->setText(
				QString::fromStdString(Presentation.VerificationUri));
			fit_to_text(AddressField);
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
		notify_observer();
	}

	void device_authorization_panel::apply_refresh(
		const device_authorization_refresh_observation& aObservation)
	{
		++Statistics.Applications;
		if (aObservation == Refresh) {
			return;
		}

		const auto previous = std::exchange(Refresh, aObservation);
		if (
			previous.State == Refresh.State
			&& previous.Failure == Refresh.Failure) {
			// A rotation moved the count and nothing else. Renewal working is
			// not news, and repainting for it would be the panel telling the
			// reviewer about a token they never see.
			return;
		}

		++Statistics.Rebuilds;
		RenewalLabel->setText(renewal_text(Refresh.State));
		apply_commands();
		if (Refresh.State
			== device_authorization_refresh_state::reenrollment_required) {
			announce(renewal_text(Refresh.State));
		}
		notify_observer();
	}

	void device_authorization_panel::apply_commands()
	{
		// Beginning and cancelling are core commands, so their legality comes
		// from the core. Copying and opening are not: neither moves the flow,
		// and both are answerable from what is on screen. A command that is not
		// allowed is also not shown: RFC-BPR-0032 shows no disabled button.
		const auto begin = enabled(
			Presentation.Commands,
			core::device_authorization_command_id::begin_device_authorization);
		const auto cancel = enabled(
			Presentation.Commands,
			core::device_authorization_command_id::cancel_device_authorization);
		const auto code    = !Presentation.UserCode.empty();
		const auto address = !Presentation.VerificationUri.empty();
		const auto idle    = Presentation.Stage == core::device_authorization_stage::idle;
		BeginButton->setEnabled(begin);
		BeginButton->setVisible(begin);
		CancelButton->setEnabled(cancel);
		CancelButton->setVisible(cancel);
		CopyButton->setEnabled(code);
		OpenButton->setEnabled(address);
		// While a code is live, Open browser is the row's one filled control,
		// said again here because a dialog holding the row (the shared sign-in,
		// [[BPR-0915]]) picks its own default when it is shown and clears this
		// one ([[BPR-0940]]).
		if (address) {
			OpenButton->setDefault(true);
		}
		CodeLine->setVisible(code);
		AddressLine->setVisible(address);
		ActionLine->setVisible(address);
		ValidityLabel->setVisible(!ValidityLabel->text().isEmpty());
		HandoffLabel->setVisible(!HandoffLabel->text().isEmpty());
		RenewalLabel->setVisible(!RenewalLabel->text().isEmpty());

		// One status sentence from the typed stage, once a flow has begun.
		StatusLabel->setText(idle ? QString{ } : stage_text(Presentation.Stage));
		StatusLabel->setVisible(!idle);

		// Where this host has no device flow, the token prompt it already
		// offers is the way in, in the same row.
		const auto token = static_cast<bool>(TokenDispatch)
			&& (Presentation.Stage == core::device_authorization_stage::unavailable
				|| Presentation.Stage == core::device_authorization_stage::disabled);
		TokenButton->setEnabled(token);
		TokenButton->setVisible(token);
		RetryLine->setVisible(begin || token);

		// The one gesture that changes wording rather than only enablement: a
		// terminal flow is retried by asking for a new code, and calling that
		// "Get a code" after a denial reads as though nothing happened.
		BeginButton->setText(idle ? tr("Sign in") : tr("Sign in again"));
	}

	void device_authorization_panel::apply_fonts()
	{
		auto emphasis = font();
		emphasis.setWeight(QFont::DemiBold);
		AddressSentence->setFont(emphasis);
		AddressField->setFont(emphasis);
		// The platform's code font where it names one; a platform that does
		// not still gets a fixed-pitch face rather than the interface font.
		auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		code.setStyleHint(QFont::Monospace);
		code.setFixedPitch(true);
		code.setPointSizeF(font().pointSizeF() * code_scale);
		// [[BPR-0869]]. The hint is only a request. A font database with no
		// alias for the platform's name matches it to a proportional face: Qt's
		// offscreen platform on Windows names the fixed font "monospace" and gets
		// Arial. So a face that does not resolve fixed-pitch falls back to
		// fixed-pitch families the platforms ship, in order.
		if (!QFontInfo{ code }.fixedPitch()) {
			code.setFamilies({
				code.family(),
				QStringLiteral("Cascadia Mono"),
				QStringLiteral("Consolas"),
				QStringLiteral("Courier New"),
				QStringLiteral("Menlo"),
				QStringLiteral("DejaVu Sans Mono"),
				QStringLiteral("Liberation Mono"),
			});
		}
		UserCodeField->setFont(code);
		fit_to_text(UserCodeField);
		fit_to_text(AddressField);
	}

	void device_authorization_panel::show_copy_glyph()
	{
		if (!CopyIcon.isNull()) {
			CopyButton->setIcon(CopyIcon);
			CopyButton->setText({ });
			return;
		}
		// U+29C9 TWO JOINED SQUARES, where the font has it; otherwise the word.
		const auto glyph = QChar{ 0x29C9 };
		CopyButton->setText(QFontMetrics{ CopyButton->font() }.inFont(glyph) ? QString{ glyph } : tr("Copy"));
	}

	void device_authorization_panel::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange) {
			apply_fonts();
		}
	}

	void device_authorization_panel::notify_observer()
	{
		if (Observer) {
			Observer();
		}
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

	void device_authorization_panel::set_presentation_observer(std::function<void()> aObserver)
	{
		Observer = std::move(aObserver);
	}

	void device_authorization_panel::set_manual_token_dispatch(std::function<void()> aDispatch)
	{
		TokenDispatch = std::move(aDispatch);
		apply_commands();
	}

	auto device_authorization_panel::presentation() const noexcept
		-> const device_authorization_presentation&
	{
		return Presentation;
	}

	auto device_authorization_panel::refresh_observation() const noexcept
		-> const device_authorization_refresh_observation&
	{
		return Refresh;
	}

	auto device_authorization_panel::signing_in() const noexcept -> bool
	{
		return Presentation.Stage == core::device_authorization_stage::requested
			|| Presentation.Stage == core::device_authorization_stage::awaiting_authorization
			|| Presentation.Stage == core::device_authorization_stage::cancelling;
	}

	auto device_authorization_panel::shows_flow() const noexcept -> bool
	{
		const auto flow = Presentation.Stage != core::device_authorization_stage::idle
			&& Presentation.Stage != core::device_authorization_stage::authorized;
		return flow || Refresh.State == device_authorization_refresh_state::reenrollment_required;
	}

	void device_authorization_panel::focus_primary()
	{
		if (OpenButton->isVisibleTo(this) && OpenButton->isEnabled()) {
			OpenButton->setFocus(Qt::OtherFocusReason);
		} else if (BeginButton->isVisibleTo(this) && BeginButton->isEnabled()) {
			BeginButton->setFocus(Qt::OtherFocusReason);
		} else if (CodeLine->isVisibleTo(this)) {
			UserCodeField->setFocus(Qt::OtherFocusReason);
		}
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
			status = tr("%1 Code %2.")
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

	auto device_authorization_panel::address_sentence_label() const noexcept -> QLabel*
	{
		return AddressSentence;
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

	auto device_authorization_panel::status_label() const noexcept -> QLabel*
	{
		return StatusLabel;
	}

	auto device_authorization_panel::renewal_label() const noexcept -> QLabel*
	{
		return RenewalLabel;
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

	auto device_authorization_panel::token_button() const noexcept -> QPushButton*
	{
		return TokenButton;
	}
} // namespace bpr::qt
