#include "bpr/qt/server_card.hpp"

#include "bpr/qt/usage_ring.hpp"

#include "github_presentation_text.hpp"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QRectF>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include <utility>

namespace bpr::qt {
	using namespace detail;

	namespace {
		// A card's inset and the gap between a label and what it labels, on the
		// 8-point grid RFC-BPR-0032 fixes.
		constexpr auto card_inset   = 16;
		constexpr auto row_inset    = 12;
		constexpr auto column_gap   = 12;
		constexpr auto avatar_side  = 32;
		constexpr auto card_radius  = 8.0;
		constexpr auto label_column = 120;

		[[nodiscard]] auto this_machine_text() -> QString
		{
#if defined(__APPLE__)
			return QStringLiteral("this Mac");
#else
			return QStringLiteral("this computer");
#endif
		}

		[[nodiscard]] auto device_capable(
			const application::github_profile_option& aOption) noexcept -> bool
		{
			return aOption.Product == application::github_profile_product::github_com
				&& aOption.Transport == application::github_profile_transport::native_https;
		}

		[[nodiscard]] auto command_line(
			const application::github_profile_option& aOption) noexcept -> bool
		{
			return aOption.Transport == application::github_profile_transport::github_cli;
		}

		// "GitHub.com, native HTTPS". The transport is named once on the page,
		// here, so Details does not repeat it.
		[[nodiscard]] auto kind_text(
			const application::github_profile_option& aOption) -> QString
		{
			const auto transport = command_line(aOption)
				? QStringLiteral("GitHub CLI")
				: QStringLiteral("native HTTPS");
			return QStringLiteral("%1, %2").arg(product_text(aOption.Product), transport);
		}

		[[nodiscard]] auto state_text(const server_card_state aState) -> QString
		{
			switch (aState) {
			case server_card_state::signed_out:
				return QStringLiteral("Signed out");
			case server_card_state::connecting:
				return QStringLiteral("Connecting");
			case server_card_state::signing_in:
				return QStringLiteral("Signing in");
			case server_card_state::connected:
				return QStringLiteral("Connected");
			case server_card_state::refused:
				return QStringLiteral("Could not connect");
			}
			return QStringLiteral("Signed out");
		}

		// RFC-BPR-0028's accepted green, the one colour on the page, at the
		// strength that reads on the ground it sits on.
		[[nodiscard]] auto connected_colour(const QPalette& aPalette) -> QColor
		{
			const auto dark = aPalette.color(QPalette::Active, QPalette::Window).lightness() < 128;
			return dark ? QColor{ 0x4C, 0xC0, 0x6C } : QColor{ 0x1F, 0x8A, 0x3B };
		}

		[[nodiscard]] auto grant_sentence(
			const application::github_session_projection& aSession) -> QString
		{
			if (!aSession.Grant.has_value()) {
				return QStringLiteral("Signed in.");
			}
			const auto& grant = *aSession.Grant;
			if (grant.Kind == application::github_grant_kind_projection::token) {
				return QStringLiteral(
					"Signed in with a token for this session only; it is asked for again after a restart.");
			}
			auto sentence = grant.SignedInAt.has_value()
				? QStringLiteral("Signed in with a device grant on %1")
					  .arg(QDateTime::fromSecsSinceEpoch(*grant.SignedInAt).toLocalTime().date().toString(Qt::ISODate))
				: QStringLiteral("Signed in with a device grant");
			sentence += grant.Renews ? QStringLiteral("; renews itself") : QStringLiteral("; does not renew itself");
			sentence += grant.Remembered
				? QStringLiteral(" and is remembered on %1.").arg(this_machine_text())
				: QStringLiteral(" and is kept for this session only.");
			return sentence;
		}

		[[nodiscard]] auto signed_out_sentence(
			const application::github_profile_option& aOption) -> QString
		{
			if (command_line(aOption)) {
				// The trust warning beneath is this host's consequence.
				return { };
			}
			if (device_capable(aOption)) {
				return QStringLiteral("Sign in starts a device code; the session is remembered on %1.")
					.arg(this_machine_text());
			}
			return QStringLiteral(
				"Signs in with a token for this session only; it is asked for again after a restart.");
		}

		[[nodiscard]] auto access_sentence(const core::github_capabilities& aCapabilities)
			-> QString
		{
			using state        = core::github_capability_state;
			const auto resolve = aCapabilities.ResolveThread == state::supported;
			const auto reopen  = aCapabilities.UnresolveThread == state::supported;
			auto       parts   = QStringList{ };
			if (aCapabilities.ReviewThreads == state::supported) {
				parts.push_back(QStringLiteral("reads review threads"));
			}
			if (aCapabilities.ViewedFileMutations == state::supported) {
				parts.push_back(QStringLiteral("marks files viewed"));
			}
			if (resolve && reopen) {
				parts.push_back(QStringLiteral("resolves and reopens threads"));
			} else if (resolve) {
				parts.push_back(QStringLiteral("resolves threads"));
			} else if (reopen) {
				parts.push_back(QStringLiteral("reopens threads"));
			}
			if (parts.isEmpty()) {
				const auto unknown = aCapabilities.ReviewThreads == state::unknown
					|| aCapabilities.ViewedFileMutations == state::unknown
					|| aCapabilities.ResolveThread == state::unknown
					|| aCapabilities.UnresolveThread == state::unknown;
				return unknown ? QStringLiteral("Not yet known.")
							   : QStringLiteral("Reads pull requests only.");
			}
			auto sentence = parts.join(QStringLiteral("; "));
			sentence[0]   = sentence[0].toUpper();
			return sentence + QLatin1Char('.');
		}

		[[nodiscard]] auto remaining_text(std::uint64_t aRemaining, std::uint64_t aLimit)
			-> QString
		{
			const auto locale = QLocale{ };
			return QStringLiteral("%1 of %2 requests remaining")
				.arg(locale.toString(static_cast<qulonglong>(aRemaining)),
					 locale.toString(static_cast<qulonglong>(aLimit)));
		}

		[[nodiscard]] auto reset_text(std::uint64_t aResetUnixSeconds) -> QString
		{
			if (aResetUnixSeconds == 0U) {
				return { };
			}
			const auto reset = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(aResetUnixSeconds)).toLocalTime();
			return QStringLiteral("; resets at %1").arg(QLocale{ }.toString(reset.time(), QLocale::ShortFormat));
		}

		[[nodiscard]] auto rate_sentence(
			const application::github_rate_limit_projection& aLimit) -> QString
		{
			if (aLimit.SecondaryLimited) {
				return QStringLiteral("Temporarily limited.");
			}
			return remaining_text(aLimit.Remaining, aLimit.Limit)
				+ reset_text(aLimit.ResetUnixSeconds) + QLatin1Char('.');
		}

		[[nodiscard]] auto rate_value(
			const application::github_rate_limit_projection& aLimit) -> QString
		{
			return aLimit.SecondaryLimited ? QStringLiteral("Temporarily limited")
										   : remaining_text(aLimit.Remaining, aLimit.Limit);
		}

		[[nodiscard]] auto make_hairline(QWidget* const aParent) -> QFrame*
		{
			auto* const line = new QFrame{ aParent };
			line->setFrameShape(QFrame::HLine);
			line->setFrameShadow(QFrame::Plain);
			line->setForegroundRole(QPalette::Mid);
			line->setFixedHeight(1);
			return line;
		}

		[[nodiscard]] auto make_label(
			QWidget* const aParent,
			const QString& aObjectName,
			const QString& aText = { }) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aObjectName);
			set_plain_text_label(label);
			return label;
		}

		[[nodiscard]] auto make_quiet_label(
			QWidget* const aParent,
			const QString& aObjectName,
			const QString& aText = { }) -> QLabel*
		{
			auto* const label = make_label(aParent, aObjectName, aText);
			label->setWordWrap(true);
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		[[nodiscard]] auto make_button(
			QWidget* const aParent,
			const QString& aObjectName,
			const QString& aText,
			const QString& aAccessibleName) -> QPushButton*
		{
			auto* const button = new QPushButton{ aText, aParent };
			button->setObjectName(aObjectName);
			button->setAccessibleName(aAccessibleName);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setAutoDefault(false);
			return button;
		}
	} // namespace

	server_card::server_card(
		application::github_profile_option aOption,
		server_card_gestures               aGestures,
		QWidget* const                     aParent)
		: QWidget{ aParent }
		, Option{ std::move(aOption) }
		, Gestures{ std::move(aGestures) }
	{
		const auto host = text(Option.Hostname);
		setObjectName(QStringLiteral("serverCard"));
		setProperty("hostProfile", QVariant::fromValue<qulonglong>(Option.Profile.value()));
		setAccessibleName(host);
		setAccessibleDescription(kind_text(Option));

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);

		// The header: the host, its kind and transport, and the state.
		auto* const header       = new QWidget{ this };
		auto* const headerLayout = new QGridLayout{ header };
		header->setObjectName(QStringLiteral("serverCardHeader"));
		headerLayout->setContentsMargins(card_inset, row_inset, card_inset, row_inset);
		headerLayout->setHorizontalSpacing(column_gap);
		headerLayout->setVerticalSpacing(2);
		HostLabel = make_label(header, QStringLiteral("serverCardHost"), host);
		HostLabel->setAccessibleName(QStringLiteral("Host"));
		KindLabel = make_quiet_label(header, QStringLiteral("serverCardKind"), kind_text(Option));
		KindLabel->setAccessibleName(QStringLiteral("Server kind and transport"));
		StateLabel = make_label(header, QStringLiteral("serverCardState"));
		StateLabel->setAccessibleName(QStringLiteral("Connection state"));
		StateLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
		headerLayout->addWidget(HostLabel, 0, 0);
		headerLayout->addWidget(KindLabel, 1, 0);
		headerLayout->addWidget(StateLabel, 0, 1, 2, 1, Qt::AlignRight | Qt::AlignTop);
		headerLayout->setColumnStretch(0, 1);
		column->addWidget(header);
		column->addWidget(make_hairline(this));

		// The account row: who, on what terms, and the one way in or out.
		auto* const account       = new QWidget{ this };
		auto* const accountLayout = new QGridLayout{ account };
		account->setObjectName(QStringLiteral("serverCardAccount"));
		accountLayout->setContentsMargins(card_inset, row_inset, card_inset, row_inset);
		accountLayout->setHorizontalSpacing(column_gap);
		accountLayout->setVerticalSpacing(2);
		Avatar = make_label(account, QStringLiteral("serverCardAvatar"));
		Avatar->setFixedSize(avatar_side, avatar_side);
		Avatar->setAlignment(Qt::AlignCenter);
		Avatar->setAutoFillBackground(true);
		Avatar->setBackgroundRole(QPalette::Button);
		Avatar->setTextInteractionFlags(Qt::NoTextInteraction);
		LoginLabel = make_label(account, QStringLiteral("serverCardLogin"));
		LoginLabel->setAccessibleName(QStringLiteral("Account"));
		ConsequenceLabel = make_quiet_label(account, QStringLiteral("serverCardConsequence"));
		ConsequenceLabel->setAccessibleName(QStringLiteral("Sign-in terms"));
		accountLayout->addWidget(Avatar, 0, 0, 3, 1, Qt::AlignTop);
		accountLayout->addWidget(LoginLabel, 0, 1);
		accountLayout->addWidget(ConsequenceLabel, 1, 1);
		if (command_line(Option)) {
			// [[BPR-0771]] D2. RFC-BPR-0015: the opt-in account row names the
			// configured-CLI trust risks, before Connect and after it.
			CliTrustWarning = make_quiet_label(account, QStringLiteral("githubCliTrustWarning"), cli_trust_warning_text(host));
			CliTrustWarning->setAccessibleName(QStringLiteral("GitHub CLI trust warning"));
			accountLayout->addWidget(CliTrustWarning, 2, 1);
		}
		auto* const actions       = new QWidget{ account };
		auto* const actionsLayout = new QHBoxLayout{ actions };
		actionsLayout->setContentsMargins(0, 0, 0, 0);
		SignInButton = make_button(actions, QStringLiteral("serverCardSignIn"), QStringLiteral("Sign in"), QStringLiteral("Sign in to %1").arg(host));
		// The page's one filled control, and its job.
		SignInButton->setDefault(true);
		SignOutButton = make_button(actions, QStringLiteral("serverCardSignOut"), QStringLiteral("Sign out"), QStringLiteral("Sign out of %1").arg(host));
		SignOutButton->setToolTip(QStringLiteral("Forgets the sign-in on %1 and signs out").arg(this_machine_text()));
		SignOutButton->setAccessibleDescription(SignOutButton->toolTip());
		ConnectButton = make_button(actions, QStringLiteral("serverCardConnect"), QStringLiteral("Connect"), QStringLiteral("Connect %1").arg(host));
		actionsLayout->addWidget(SignInButton);
		actionsLayout->addWidget(SignOutButton);
		actionsLayout->addWidget(ConnectButton);
		accountLayout->addWidget(actions, 0, 2, 2, 1, Qt::AlignRight | Qt::AlignTop);
		accountLayout->setColumnStretch(1, 1);
		column->addWidget(account);

		// The rows: Access, Rate limit, and Details.
		RowsSeparator = make_hairline(this);
		column->addWidget(RowsSeparator);
		Rows                   = new QWidget{ this };
		auto* const rowsLayout = new QGridLayout{ Rows };
		Rows->setObjectName(QStringLiteral("serverCardRows"));
		rowsLayout->setContentsMargins(card_inset, row_inset, card_inset, row_inset);
		rowsLayout->setHorizontalSpacing(card_inset);
		rowsLayout->setVerticalSpacing(8);
		rowsLayout->setColumnMinimumWidth(0, label_column);
		AccessName  = make_label(Rows, QStringLiteral("serverCardAccessName"), QStringLiteral("Access"));
		AccessLabel = make_quiet_label(Rows, QStringLiteral("serverCardAccess"));
		AccessLabel->setAccessibleName(QStringLiteral("Access"));
		RateLimitName  = make_label(Rows, QStringLiteral("serverCardRateLimitName"), QStringLiteral("Rate limit"));
		RateLimitLabel = make_quiet_label(Rows, QStringLiteral("serverCardRateLimit"));
		RateLimitLabel->setAccessibleName(QStringLiteral("Rate limit"));
		UsageIndicator = new usage_ring{ Rows };
		UsageIndicator->hide();
		DetailsButton = new QToolButton{ Rows };
		DetailsButton->setObjectName(QStringLiteral("serverCardDetailsButton"));
		DetailsButton->setText(QStringLiteral("Details"));
		DetailsButton->setCheckable(true);
		DetailsButton->setAutoRaise(true);
		DetailsButton->setArrowType(Qt::RightArrow);
		DetailsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		DetailsButton->setFocusPolicy(Qt::StrongFocus);
		DetailsButton->setAccessibleName(QStringLiteral("Show details for %1").arg(host));
		DetailsButton->setAccessibleDescription(QStringLiteral("The capabilities, the rate limits and the last problem on this host"));

		Details                   = new QWidget{ Rows };
		auto* const detailsLayout = new QGridLayout{ Details };
		Details->setObjectName(QStringLiteral("serverCardDetails"));
		detailsLayout->setContentsMargins(0, 0, 0, 0);
		detailsLayout->setHorizontalSpacing(card_inset);
		detailsLayout->setColumnMinimumWidth(0, label_column);
		const std::array capabilityNames{
			QStringLiteral("Review threads"),
			QStringLiteral("Viewed files"),
			QStringLiteral("Resolving threads"),
			QStringLiteral("Reopening threads"),
		};
		auto detailRow = 0;
		for (std::size_t index = 0U; index < capabilityNames.size(); ++index, ++detailRow) {
			detailsLayout->addWidget(make_label(Details, QStringLiteral("serverCardCapabilityName"), capabilityNames[index]), detailRow, 0);
			CapabilityValues[index] = make_quiet_label(Details, QStringLiteral("serverCardCapability"));
			CapabilityValues[index]->setAccessibleName(capabilityNames[index]);
			detailsLayout->addWidget(CapabilityValues[index], detailRow, 1);
		}
		const std::array rateNames{
			QStringLiteral("API requests"),
			QStringLiteral("Search requests"),
			QStringLiteral("GraphQL requests"),
		};
		for (std::size_t index = 0U; index < rateNames.size(); ++index, ++detailRow) {
			RateLimitNames[index] = make_label(Details, QStringLiteral("serverCardRateLimitResourceName"), rateNames[index]);
			detailsLayout->addWidget(RateLimitNames[index], detailRow, 0);
			RateLimitValues[index] = make_quiet_label(Details, QStringLiteral("serverCardRateLimitResource"));
			RateLimitValues[index]->setAccessibleName(rateNames[index]);
			detailsLayout->addWidget(RateLimitValues[index], detailRow, 1);
		}
		RateLimitsUnreported = make_quiet_label(Details, QStringLiteral("serverCardRateLimitsUnreported"), QStringLiteral("No rate limits reported yet."));
		detailsLayout->addWidget(RateLimitsUnreported, detailRow++, 0, 1, 2);
		LastProblemName  = make_label(Details, QStringLiteral("serverCardLastProblemName"), QStringLiteral("Last problem"));
		LastProblemLabel = make_label(Details, QStringLiteral("serverCardLastProblem"));
		LastProblemLabel->setAccessibleName(QStringLiteral("Last problem"));
		LastProblemLabel->setWordWrap(true);
		LastProblemLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		CopyDetailsButton = make_button(Details, QStringLiteral("serverCardCopyDetails"), QStringLiteral("Copy details"), QStringLiteral("Copy the last problem on %1").arg(host));
		detailsLayout->addWidget(LastProblemName, detailRow, 0, Qt::AlignTop);
		detailsLayout->addWidget(LastProblemLabel, detailRow, 1);
		detailsLayout->addWidget(CopyDetailsButton, detailRow, 2, Qt::AlignTop);
		detailsLayout->setColumnStretch(1, 1);
		Details->hide();

		rowsLayout->addWidget(AccessName, 0, 0, Qt::AlignTop);
		rowsLayout->addWidget(AccessLabel, 0, 1);
		rowsLayout->addWidget(RateLimitName, 1, 0, Qt::AlignTop);
		rowsLayout->addWidget(RateLimitLabel, 1, 1);
		rowsLayout->addWidget(UsageIndicator, 1, 2, Qt::AlignRight | Qt::AlignTop);
		rowsLayout->addWidget(DetailsButton, 2, 0, 1, 3, Qt::AlignLeft);
		rowsLayout->addWidget(Details, 3, 0, 1, 3);
		rowsLayout->setColumnStretch(1, 1);
		column->addWidget(Rows);

		connect(SignInButton, &QPushButton::clicked, this, [this] {
			if (Gestures.SignIn) {
				Gestures.SignIn(Option.Profile);
			}
		});
		connect(SignOutButton, &QPushButton::clicked, this, [this] {
			if (Gestures.SignOut) {
				Gestures.SignOut();
			}
		});
		connect(ConnectButton, &QPushButton::clicked, this, [this] {
			if (Gestures.Connect) {
				Gestures.Connect(Option.Profile);
			}
		});
		connect(DetailsButton, &QToolButton::toggled, this, [this, host](const bool aOpen) {
			Details->setVisible(aOpen);
			DetailsButton->setArrowType(aOpen ? Qt::DownArrow : Qt::RightArrow);
			DetailsButton->setAccessibleName(
				aOpen ? QStringLiteral("Hide details for %1").arg(host)
					  : QStringLiteral("Show details for %1").arg(host));
		});
		connect(CopyDetailsButton, &QPushButton::clicked, this, [this] {
			if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr) {
				clipboard->setText(Presentation.LastProblem);
			}
		});

		refresh_native_appearance();
		apply(server_card_presentation{ });
	}

	void server_card::apply(const server_card_presentation& aPresentation)
	{
		++Statistics.Applications;
		if (Applied && aPresentation == Presentation) {
			// A decision that changed nothing rebuilds nothing.
			return;
		}
		Applied      = true;
		Presentation = aPresentation;
		++Statistics.Rebuilds;
		apply_header();
		apply_account_row();
		apply_rows();
		update();
	}

	void server_card::set_public_preview_dispatch(
		application::public_preview_gesture_dispatch aDispatch)
	{
		UsageIndicator->set_dispatch(std::move(aDispatch));
	}

	void server_card::apply_header()
	{
		StateLabel->setText(state_text(Presentation.State));
		auto colours = palette();
		colours.setColor(
			QPalette::WindowText,
			Presentation.State == server_card_state::connected
				? connected_colour(palette())
				: palette().color(QPalette::Active, QPalette::PlaceholderText));
		StateLabel->setPalette(colours);
		StateLabel->setForegroundRole(QPalette::WindowText);
	}

	void server_card::apply_account_row()
	{
		const auto& shown    = Presentation;
		const auto  signedIn = this->signed_in();
		const auto  waiting  = shown.State == server_card_state::connecting
			|| shown.State == server_card_state::signing_in;
		const auto login = signedIn ? text(shown.Session->Login) : QString{ };
		Avatar->setText(login.isEmpty() ? QStringLiteral("?") : login.left(1).toUpper());
		LoginLabel->setText(login.isEmpty() ? QStringLiteral("No account") : login);

		auto consequence = QString{ };
		if (signedIn) {
			consequence = grant_sentence(*shown.Session);
		} else if (shown.ReadingPublic) {
			consequence = QStringLiteral("Reading public pull requests without an account.");
		} else {
			consequence = signed_out_sentence(Option);
		}
		ConsequenceLabel->setText(consequence);
		ConsequenceLabel->setVisible(!consequence.isEmpty());

		// One way in or out, never a disabled one. The chosen host signs in;
		// a GitHub CLI host's way in is its Connect; any other host connects.
		SignOutButton->setVisible(signedIn);
		SignInButton->setVisible(shown.Current && !signedIn && !waiting && !command_line(Option));
		SignInButton->setAccessibleDescription(consequence);
		SignInButton->setToolTip(consequence);
		ConnectButton->setVisible(
			!signedIn && !waiting && shown.ConnectAdmitted
			&& (!shown.Current || command_line(Option)));
		ConnectButton->setAccessibleDescription(shown.ConnectDescription);
		ConnectButton->setToolTip(shown.ConnectDescription);
	}

	void server_card::apply_rows()
	{
		const auto& shown    = Presentation;
		const auto  signedIn = this->signed_in();

		AccessLabel->setText(access_sentence(shown.Capabilities));
		AccessName->setVisible(signedIn);
		AccessLabel->setVisible(signedIn);

		auto rate = QString{ };
		if (shown.ReadingPublic && shown.PublicPreview.Budget.has_value()) {
			const auto& budget = *shown.PublicPreview.Budget;
			rate               = remaining_text(budget.Remaining, budget.Limit)
				+ QStringLiteral(" without an account") + reset_text(budget.ResetUnixSeconds) + QLatin1Char('.');
		} else if (signedIn && shown.RateLimits.Core.has_value()) {
			rate = rate_sentence(*shown.RateLimits.Core);
		}
		RateLimitLabel->setText(rate);
		RateLimitName->setVisible(!rate.isEmpty() || shown.ReadingPublic);
		RateLimitLabel->setVisible(!rate.isEmpty());
		UsageIndicator->apply(shown.PublicPreview, shown.PublicPreviewNow);
		UsageIndicator->setVisible(shown.ReadingPublic);

		const auto details = shown.Current || !shown.LastProblem.isEmpty();
		if (!details && DetailsButton->isChecked()) {
			DetailsButton->setChecked(false);
		}
		DetailsButton->setVisible(details);
		const std::array capabilities{
			shown.Capabilities.ReviewThreads,
			shown.Capabilities.ViewedFileMutations,
			shown.Capabilities.ResolveThread,
			shown.Capabilities.UnresolveThread,
		};
		for (std::size_t index = 0U; index < capabilities.size(); ++index) {
			CapabilityValues[index]->setText(capability_text(capabilities[index]));
		}
		const std::array limits{
			&shown.RateLimits.Core,
			&shown.RateLimits.Search,
			&shown.RateLimits.Graphql,
		};
		auto reported = false;
		for (std::size_t index = 0U; index < limits.size(); ++index) {
			const auto present = limits[index]->has_value();
			reported           = reported || present;
			RateLimitValues[index]->setText(present ? rate_value(**limits[index]) : QString{ });
			RateLimitNames[index]->setVisible(present);
			RateLimitValues[index]->setVisible(present);
		}
		RateLimitsUnreported->setVisible(!reported);
		const auto problem = !shown.LastProblem.isEmpty();
		LastProblemLabel->setText(shown.LastProblem);
		LastProblemName->setVisible(problem);
		LastProblemLabel->setVisible(problem);
		CopyDetailsButton->setVisible(problem);

		const auto rows = signedIn || details || !rate.isEmpty() || shown.ReadingPublic;
		Rows->setVisible(rows);
		RowsSeparator->setVisible(rows);
	}

	void server_card::refresh_native_appearance()
	{
		auto emphasis = font();
		emphasis.setWeight(QFont::DemiBold);
		HostLabel->setFont(emphasis);
		LoginLabel->setFont(emphasis);
		Avatar->setFont(emphasis);
		if (Applied) {
			apply_header();
		}
	}

	void server_card::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::FontChange:
		case QEvent::PaletteChange:
		case QEvent::StyleChange:
			refresh_native_appearance();
			break;
		default:
			break;
		}
	}

	void server_card::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), card_radius, card_radius);
	}

	auto server_card::profile() const noexcept -> core::host_profile_id
	{
		return Option.Profile;
	}

	auto server_card::option() const noexcept -> const application::github_profile_option&
	{
		return Option;
	}

	auto server_card::presentation() const noexcept -> const server_card_presentation&
	{
		return Presentation;
	}

	auto server_card::statistics() const noexcept -> server_card_statistics
	{
		return Statistics;
	}

	auto server_card::offers_device_sign_in() const noexcept -> bool
	{
		return device_capable(Option);
	}

	auto server_card::signed_in() const noexcept -> bool
	{
		return Presentation.State == server_card_state::connected
			&& Presentation.Session.has_value() && !Presentation.ReadingPublic;
	}

	auto server_card::host_label() const noexcept -> QLabel*
	{
		return HostLabel;
	}

	auto server_card::kind_label() const noexcept -> QLabel*
	{
		return KindLabel;
	}

	auto server_card::state_label() const noexcept -> QLabel*
	{
		return StateLabel;
	}

	auto server_card::login_label() const noexcept -> QLabel*
	{
		return LoginLabel;
	}

	auto server_card::consequence_label() const noexcept -> QLabel*
	{
		return ConsequenceLabel;
	}

	auto server_card::cli_trust_warning() const noexcept -> QLabel*
	{
		return CliTrustWarning;
	}

	auto server_card::sign_in_button() const noexcept -> QPushButton*
	{
		return SignInButton;
	}

	auto server_card::sign_out_button() const noexcept -> QPushButton*
	{
		return SignOutButton;
	}

	auto server_card::connect_button() const noexcept -> QPushButton*
	{
		return ConnectButton;
	}

	auto server_card::access_label() const noexcept -> QLabel*
	{
		return AccessLabel;
	}

	auto server_card::rate_limit_label() const noexcept -> QLabel*
	{
		return RateLimitLabel;
	}

	auto server_card::usage_indicator() const noexcept -> usage_ring*
	{
		return UsageIndicator;
	}

	auto server_card::details_button() const noexcept -> QToolButton*
	{
		return DetailsButton;
	}

	auto server_card::details() const noexcept -> QWidget*
	{
		return Details;
	}

	auto server_card::last_problem_label() const noexcept -> QLabel*
	{
		return LastProblemLabel;
	}

	auto server_card::copy_details_button() const noexcept -> QPushButton*
	{
		return CopyDetailsButton;
	}
} // namespace bpr::qt
