#include "bpr/qt/server_card.hpp"

#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/usage_ring.hpp"

#include "github_presentation_text.hpp"
#include "this_machine_text.hpp"

#include <QApplication>
#include <QCheckBox>
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
#include <QSignalBlocker>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	using namespace detail;

	namespace {
		// A card's inset, a row's height and the gap between a label and what
		// it labels, on the 8-point grid RFC-BPR-0032 fixes and at the accepted
		// mockup's label column.
		constexpr auto card_inset   = 16;
		constexpr auto row_inset    = 12;
		constexpr auto row_height   = 44;
		constexpr auto column_gap   = 12;
		constexpr auto avatar_side  = 32;
		constexpr auto card_radius  = 8.0;
		constexpr auto label_column = 150;

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

		// "GitHub.com, native HTTPS". The transport is named once on a card,
		// here, so neither Details nor the warning repeats it.
		[[nodiscard]] auto kind_text(
			const application::github_profile_option& aOption) -> QString
		{
			const auto transport = command_line(aOption)
				? QStringLiteral("GitHub CLI")
				: QStringLiteral("native HTTPS");
			return QStringLiteral("%1, %2").arg(product_text(aOption.Product), transport);
		}

		// RFC-BPR-0015's offer, in the composition's own words: what its label
		// says after the product ("GitHub CLI (uses your gh sign-in)"), or the
		// transport alone for a label that says nothing more.
		[[nodiscard]] auto cli_offer_text(
			const application::github_profile_option& aOption) -> QString
		{
			const auto label     = QString::fromStdString(aOption.Label);
			const auto separator = QStringLiteral(" — ");
			const auto at        = label.indexOf(separator);
			const auto offer     = at < 0 ? QString{ } : label.mid(at + separator.size()).trimmed();
			return offer.startsWith(QStringLiteral("GitHub CLI")) ? offer : QStringLiteral("GitHub CLI");
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

		// RFC-BPR-0032's accepted green for Connected, the one colour on the page, at the
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
				// The trust warning beneath is this way in's consequence.
				return { };
			}
			if (device_capable(aOption)) {
				return QStringLiteral("Sign in starts a device code; the session is remembered on %1.")
					.arg(this_machine_text());
			}
			return QStringLiteral(
				"Signs in with a token for this session only; it is asked for again after a restart.");
		}

		// [[BPR-0644]]. What a failed session left on this machine, and how it
		// ends: the next launch, a new sign-in, or Sign out.
		[[nodiscard]] auto kept_grant_sentence(const kept_grant aKept) -> QString
		{
			switch (aKept) {
			case kept_grant::none:
				return { };
			case kept_grant::retried_next_launch:
				return QStringLiteral("Your sign-in is still remembered on %1 and is tried again at the next launch. Sign out forgets it.")
					.arg(this_machine_text());
			case kept_grant::needs_sign_in:
				return QStringLiteral("Your sign-in on %1 needs signing in again. It stays remembered until you sign in or sign out.")
					.arg(this_machine_text());
			case kept_grant::another_account:
				return QStringLiteral(
						   "Your remembered sign-in on %1 signs in to a different GitHub account than the one it was saved for. "
						   "Sign in again to replace it, or sign out to forget it.")
					.arg(this_machine_text());
			}
			return { };
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

		// One row of the card's rows: a label column, what it labels, and a
		// control at the right, one row high.
		[[nodiscard]] auto make_row(QWidget* const aParent, const QString& aObjectName)
			-> std::pair<QWidget*, QGridLayout*>
		{
			auto* const row    = new QWidget{ aParent };
			auto* const layout = new QGridLayout{ row };
			row->setObjectName(aObjectName);
			row->setMinimumHeight(row_height);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setHorizontalSpacing(card_inset);
			layout->setColumnMinimumWidth(0, label_column);
			layout->setColumnStretch(1, 1);
			return { row, layout };
		}
		// [[BPR-1373]]. Who uses a gh account, as the end of its sentence.
		[[nodiscard]] auto used_by_sentence(const std::vector<std::string>& aProfiles) -> QString
		{
			if (aProfiles.empty()) {
				return QStringLiteral("No profile uses it yet.");
			}
			auto names = QStringList{ };
			for (const auto& profile : aProfiles) {
				names.push_back(text(profile));
			}
			const auto last = names.takeLast();
			return QStringLiteral("Used by %1.").arg(names.isEmpty() ? last : names.join(QStringLiteral(", ")) + QStringLiteral(" and ") + last);
		}

		// [[BPR-1373]]. What a gh account is to this Mac, in one or two sentences.
		/// [[BPR-1373]]. How many listed accounts a profile can choose.
		[[nodiscard]] auto usable_cli_accounts(const std::vector<server_card_cli_account>& aAccounts) -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(aAccounts, [](const server_card_cli_account& aAccount) {
				return aAccount.State == server_card_cli_account_state::signed_in && aAccount.Profile.has_value();
			}));
		}

		[[nodiscard]] auto cli_account_sentence(const server_card_cli_account& aAccount) -> QString
		{
			switch (aAccount.State) {
			case server_card_cli_account_state::signed_in:
				return (aAccount.ActiveInGh ? QStringLiteral("Signed in with gh on %1, and gh's active account. ")
											: QStringLiteral("Signed in with gh on %1. "))
						   .arg(this_machine_text())
					+ used_by_sentence(aAccount.UsedBy);
			case server_card_cli_account_state::signed_out:
				return QStringLiteral("Signed out in gh. Sign it in with gh auth login, then check gh again.");
			case server_card_cli_account_state::file_token:
				return QStringLiteral("gh keeps this account's token in a file instead of the keychain, so a profile can't use it. "
									  "Sign it in again with gh auth login to move it to the keychain.");
			}
			return { };
		}
	} // namespace

	server_card::server_card(
		application::github_profile_option aOption,
		server_card_gestures               aGestures,
		QWidget* const                     aParent)
		: QWidget{ aParent }
		, Gestures{ std::move(aGestures) }
	{
		Options.push_back(std::move(aOption));
		const auto host = text(Options.front().Hostname);
		setObjectName(QStringLiteral("serverCard"));
		setProperty("hostname", host);
		setAccessibleName(host);

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
		KindLabel = make_quiet_label(header, QStringLiteral("serverCardKind"));
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

		// The account row: who, on what terms, and the ways in or out.
		Account       = new QWidget{ this };
		AccountLayout = new QGridLayout{ Account };
		Account->setObjectName(QStringLiteral("serverCardAccount"));
		AccountLayout->setContentsMargins(card_inset, row_inset, card_inset, row_inset);
		AccountLayout->setHorizontalSpacing(column_gap);
		AccountLayout->setVerticalSpacing(2);
		Avatar = make_label(Account, QStringLiteral("serverCardAvatar"));
		Avatar->setFixedSize(avatar_side, avatar_side);
		Avatar->setAlignment(Qt::AlignCenter);
		Avatar->setAutoFillBackground(true);
		Avatar->setBackgroundRole(QPalette::Button);
		Avatar->setTextInteractionFlags(Qt::NoTextInteraction);
		LoginLabel = make_label(Account, QStringLiteral("serverCardLogin"));
		LoginLabel->setAccessibleName(QStringLiteral("Account"));
		ConsequenceLabel = make_quiet_label(Account, QStringLiteral("serverCardConsequence"));
		ConsequenceLabel->setAccessibleName(QStringLiteral("Sign-in terms"));
		AccountLayout->addWidget(Avatar, 0, 0, 5, 1, Qt::AlignTop);
		AccountLayout->addWidget(LoginLabel, 0, 1);
		AccountLayout->addWidget(ConsequenceLabel, 1, 1);
		Actions                   = new QWidget{ Account };
		auto* const actions       = Actions;
		auto* const actionsLayout = new QHBoxLayout{ actions };
		actionsLayout->setContentsMargins(0, 0, 0, 0);
		SignInButton  = make_button(actions, QStringLiteral("serverCardSignIn"), QStringLiteral("Sign in"), QStringLiteral("Sign in to %1").arg(host));
		SignOutButton = make_button(actions, QStringLiteral("serverCardSignOut"), QStringLiteral("Sign out"), QStringLiteral("Sign out of %1").arg(host));
		SignOutButton->setToolTip(QStringLiteral("Forgets the sign-in on %1 and signs out").arg(this_machine_text()));
		SignOutButton->setAccessibleDescription(SignOutButton->toolTip());
		ConnectButton = make_button(actions, QStringLiteral("serverCardConnect"), QStringLiteral("Connect"), QStringLiteral("Connect %1").arg(host));
		actionsLayout->addWidget(SignInButton);
		actionsLayout->addWidget(SignOutButton);
		actionsLayout->addWidget(ConnectButton);
		AccountLayout->addWidget(actions, 0, 2, 2, 1, Qt::AlignRight | Qt::AlignTop);
		AccountLayout->setColumnStretch(1, 1);
		column->addWidget(Account);

		// [[BPR-1373]]. Every gh account on this host, once gh has been checked:
		// one row each, the way to give one to this profile, and Check gh again.
		CliAccounts           = new QWidget{ this };
		auto* const cliOuter  = new QVBoxLayout{ CliAccounts };
		auto* const cliBody   = new QWidget{ CliAccounts };
		auto* const cliColumn = new QVBoxLayout{ cliBody };
		CliAccounts->setObjectName(QStringLiteral("serverCardCliAccounts"));
		CliAccounts->setAccessibleName(QStringLiteral("GitHub CLI accounts on %1").arg(this_machine_text()));
		cliOuter->setContentsMargins(0, 0, 0, 0);
		cliOuter->setSpacing(0);
		cliOuter->addWidget(make_hairline(CliAccounts));
		cliOuter->addWidget(cliBody);
		cliColumn->setContentsMargins(card_inset, row_inset, card_inset, row_inset);
		cliColumn->setSpacing(row_inset);
		cliColumn->addWidget(make_label(cliBody, QStringLiteral("serverCardCliAccountsHeading"), QStringLiteral("GitHub CLI accounts on %1").arg(this_machine_text())));
		CliAccountList = new QWidget{ cliBody };
		CliAccountList->setObjectName(QStringLiteral("serverCardCliAccountList"));
		auto* const listColumn = new QVBoxLayout{ CliAccountList };
		listColumn->setContentsMargins(0, 0, 0, 0);
		listColumn->setSpacing(row_inset);
		cliColumn->addWidget(CliAccountList);
		CliCheckButton = make_button(cliBody, QStringLiteral("serverCardCliCheck"), QStringLiteral("Check gh again"), QStringLiteral("Check gh's accounts on %1 again").arg(host));
		CliCheckButton->setFlat(true);
		CliCheckButton->setAccessibleDescription(QStringLiteral("Asks the installed gh which accounts it holds. Tokens are never requested or shown."));
		CliCheckButton->setToolTip(CliCheckButton->accessibleDescription());
		cliColumn->addWidget(CliCheckButton, 0, Qt::AlignLeft);
		connect(CliCheckButton, &QPushButton::clicked, this, [this] {
			if (Gestures.CheckCliAccounts) {
				Gestures.CheckCliAccounts();
			}
		});
		CliAccounts->hide();
		column->addWidget(CliAccounts);

		// The rows: Access, Rate limit, an enterprise server's Remember and
		// Forget, and Details, one hairline between each.
		RowsSeparator = make_hairline(this);
		column->addWidget(RowsSeparator);
		Rows                   = new QWidget{ this };
		auto* const rowsColumn = new QVBoxLayout{ Rows };
		Rows->setObjectName(QStringLiteral("serverCardRows"));
		rowsColumn->setContentsMargins(card_inset, 0, card_inset, 0);
		rowsColumn->setSpacing(0);

		auto [accessRow, accessLayout] = make_row(Rows, QStringLiteral("serverCardAccessRow"));
		AccessRow                      = accessRow;
		auto* const accessName         = make_label(AccessRow, QStringLiteral("serverCardAccessName"), QStringLiteral("Access"));
		AccessLabel                    = make_quiet_label(AccessRow, QStringLiteral("serverCardAccess"));
		AccessLabel->setAccessibleName(QStringLiteral("Access"));
		accessLayout->addWidget(accessName, 0, 0, Qt::AlignVCenter);
		accessLayout->addWidget(AccessLabel, 0, 1, Qt::AlignVCenter);
		rowsColumn->addWidget(AccessRow);

		RowHairlines[0] = make_hairline(Rows);
		rowsColumn->addWidget(RowHairlines[0]);
		auto [rateRow, rateLayout] = make_row(Rows, QStringLiteral("serverCardRateLimitRow"));
		RateLimitRow               = rateRow;
		auto* const rateName       = make_label(RateLimitRow, QStringLiteral("serverCardRateLimitName"), QStringLiteral("Rate limit"));
		RateLimitLabel             = make_quiet_label(RateLimitRow, QStringLiteral("serverCardRateLimit"));
		RateLimitLabel->setAccessibleName(QStringLiteral("Rate limit"));
		UsageIndicator = new usage_ring{ RateLimitRow };
		UsageIndicator->hide();
		rateLayout->addWidget(rateName, 0, 0, Qt::AlignVCenter);
		rateLayout->addWidget(RateLimitLabel, 0, 1, Qt::AlignVCenter);
		rateLayout->addWidget(UsageIndicator, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
		rowsColumn->addWidget(RateLimitRow);

		// [[BPR-0774]]. An enterprise server's own rows: whether the queue keeps
		// it across launches, and the way to stop keeping it. Each row is label,
		// info glyph and control, and the consequence is the glyph's.
		RowHairlines[1] = make_hairline(Rows);
		rowsColumn->addWidget(RowHairlines[1]);
		auto [rememberRow, rememberLayout] = make_row(Rows, QStringLiteral("serverCardRememberRow"));
		RememberRow                        = rememberRow;
		auto* const rememberLabel          = new QWidget{ RememberRow };
		auto* const rememberLabelLayout    = new QHBoxLayout{ rememberLabel };
		rememberLabelLayout->setContentsMargins(0, 0, 0, 0);
		rememberLabelLayout->setSpacing(4);
		rememberLabelLayout->addWidget(make_label(rememberLabel, QStringLiteral("serverCardRememberName"), QStringLiteral("Remember this server")));
		RememberGlyph = new info_glyph{ QStringLiteral("Keeps the hostname and account name across launches. The token is still asked for."), rememberLabel };
		RememberGlyph->setAccessibleName(QStringLiteral("About remembering %1").arg(host));
		rememberLabelLayout->addWidget(RememberGlyph);
		rememberLabelLayout->addStretch(1);
		RememberSwitch = new QCheckBox{ RememberRow };
		RememberSwitch->setObjectName(QStringLiteral("serverCardRemember"));
		RememberSwitch->setAccessibleName(QStringLiteral("Remember %1").arg(host));
		RememberSwitch->setAccessibleDescription(RememberGlyph->consequence());
		RememberSwitch->setFocusPolicy(Qt::StrongFocus);
		rememberLayout->addWidget(rememberLabel, 0, 0, 1, 2, Qt::AlignVCenter);
		rememberLayout->addWidget(RememberSwitch, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
		rowsColumn->addWidget(RememberRow);

		RowHairlines[2] = make_hairline(Rows);
		rowsColumn->addWidget(RowHairlines[2]);
		auto [forgetRow, forgetLayout] = make_row(Rows, QStringLiteral("serverCardForgetRow"));
		ForgetRow                      = forgetRow;
		auto* const forgetControl      = new QWidget{ ForgetRow };
		auto* const forgetControlRow   = new QHBoxLayout{ forgetControl };
		forgetControlRow->setContentsMargins(0, 0, 0, 0);
		forgetControlRow->setSpacing(4);
		ForgetButton = make_button(forgetControl, QStringLiteral("serverCardForget"), QStringLiteral("Forget server"), QStringLiteral("Forget %1").arg(host));
		ForgetButton->setFlat(true);
		ForgetGlyph = new info_glyph{ QStringLiteral("Stops offering this server at the next start. Nothing on the server changes."), forgetControl };
		ForgetGlyph->setAccessibleName(QStringLiteral("About forgetting %1").arg(host));
		ForgetButton->setAccessibleDescription(ForgetGlyph->consequence());
		forgetControlRow->addWidget(ForgetButton);
		forgetControlRow->addWidget(ForgetGlyph);
		forgetControlRow->addStretch(1);
		forgetLayout->addWidget(forgetControl, 0, 0, 1, 3, Qt::AlignVCenter);
		rowsColumn->addWidget(ForgetRow);

		RowHairlines[3] = make_hairline(Rows);
		rowsColumn->addWidget(RowHairlines[3]);
		DetailsRow                   = new QWidget{ Rows };
		auto* const detailsRowColumn = new QVBoxLayout{ DetailsRow };
		DetailsRow->setObjectName(QStringLiteral("serverCardDetailsRow"));
		detailsRowColumn->setContentsMargins(0, 0, 0, 0);
		detailsRowColumn->setSpacing(0);
		auto [detailsLine, detailsLineLayout] = make_row(DetailsRow, QStringLiteral("serverCardDetailsLine"));
		DetailsButton                         = new QToolButton{ detailsLine };
		DetailsButton->setObjectName(QStringLiteral("serverCardDetailsButton"));
		DetailsButton->setText(QStringLiteral("Details"));
		DetailsButton->setCheckable(true);
		DetailsButton->setAutoRaise(true);
		DetailsButton->setArrowType(Qt::RightArrow);
		DetailsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		DetailsButton->setFocusPolicy(Qt::StrongFocus);
		DetailsButton->setAccessibleName(QStringLiteral("Show details for %1").arg(host));
		DetailsButton->setAccessibleDescription(QStringLiteral("The capabilities, the rate limits and the last problem on this host"));
		DetailsSummary = make_quiet_label(detailsLine, QStringLiteral("serverCardDetailsSummary"), QStringLiteral("Capabilities, rate limits and the last problem."));
		detailsLineLayout->addWidget(DetailsButton, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
		detailsLineLayout->addWidget(DetailsSummary, 0, 1, Qt::AlignVCenter);
		detailsRowColumn->addWidget(detailsLine);

		Details                   = new QWidget{ DetailsRow };
		auto* const detailsLayout = new QGridLayout{ Details };
		Details->setObjectName(QStringLiteral("serverCardDetails"));
		detailsLayout->setContentsMargins(0, 0, 0, row_inset);
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
		detailsRowColumn->addWidget(Details);
		rowsColumn->addWidget(DetailsRow);
		column->addWidget(Rows);

		connect(SignInButton, &QPushButton::clicked, this, [this] {
			if (Gestures.SignIn) {
				const auto* const native = native_option();
				Gestures.SignIn(native != nullptr ? native->Profile : profile());
			}
		});
		connect(SignOutButton, &QPushButton::clicked, this, [this] {
			if (Gestures.SignOut) {
				Gestures.SignOut();
			}
		});
		connect(RememberSwitch, &QCheckBox::toggled, this, [this](const bool aOn) {
			// [[BPR-0774]]. The switch shows the record the queue keeps, never a
			// wish: it returns to that at once, and the gesture asks the window,
			// which keeps the server or confirms forgetting it.
			const auto kept = Presentation.Remembered;
			{
				const QSignalBlocker blocker{ RememberSwitch };
				RememberSwitch->setChecked(kept.has_value());
			}
			if (aOn && !kept.has_value() && Gestures.Remember) {
				Gestures.Remember();
			} else if (!aOn && kept.has_value() && Gestures.Forget) {
				Gestures.Forget(*kept);
			}
		});
		connect(ForgetButton, &QPushButton::clicked, this, [this] {
			if (const auto kept = Presentation.Remembered; kept.has_value() && Gestures.Forget) {
				Gestures.Forget(*kept);
			}
		});
		connect(ConnectButton, &QPushButton::clicked, this, [this] {
			if (Gestures.Connect) {
				Gestures.Connect(profile());
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

		if (command_line(Options.front())) {
			ensure_cli_offer();
		}
		refresh_native_appearance();
		apply(server_card_presentation{ });
	}

	auto server_card::adopt_option(application::github_profile_option aOption) -> bool
	{
		if (aOption.Hostname != hostname() || holds(aOption.Profile)) {
			return false;
		}
		const auto cli = command_line(aOption);
		if (cli) {
			Options.push_back(std::move(aOption));
			ensure_cli_offer();
		} else {
			// Native HTTPS first: the card speaks for it until a host is chosen.
			const auto firstCli = std::ranges::find_if(Options, [](const application::github_profile_option& aHeld) {
				return command_line(aHeld);
			});
			Options.insert(firstCli, std::move(aOption));
		}
		// What the card offers changed even though the decision did not.
		Applied = false;
		apply(Presentation);
		return true;
	}

	void server_card::ensure_cli_offer()
	{
		if (CliOfferButton != nullptr) {
			return;
		}
		const auto* const cli  = cli_option();
		const auto        host = text(hostname());
		// [[BPR-0771]] D2, [[BPR-0772]]. RFC-BPR-0015: the account row offers
		// GitHub CLI for this exact host and names the configured-CLI trust
		// risks, before the opt-in and after it. The warning says "this
		// server" because the header already names it.
		CliOfferButton  = make_button(Account, QStringLiteral("serverCardCliOffer"), cli_offer_text(*cli), QStringLiteral("Connect %1 with GitHub CLI").arg(host));
		CliTrustWarning = make_quiet_label(Account, QStringLiteral("githubCliTrustWarning"), cli_trust_warning_text());
		CliTrustWarning->setAccessibleName(QStringLiteral("GitHub CLI trust warning"));
		CliOfferButton->setAccessibleDescription(CliTrustWarning->text());
		AccountLayout->addWidget(CliOfferButton, 2, 1, Qt::AlignLeft);
		AccountLayout->addWidget(CliTrustWarning, 3, 1);
		QWidget::setTabOrder(ConnectButton, CliOfferButton);
		connect(CliOfferButton, &QPushButton::clicked, this, [this] {
			if (const auto* const offered = cli_option(); offered != nullptr && Gestures.Connect) {
				Gestures.Connect(offered->Profile);
			}
		});
	}

	void server_card::set_setup_mode(const bool aSetup, const bool aCliOnly)
	{
		SetupMode    = aSetup;
		CliSetupOnly = aSetup && aCliOnly;
		Rows->setVisible(!aSetup);
		RowsSeparator->setVisible(!aSetup);
		apply_account_row();
		apply_cli_accounts();
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
		apply_cli_accounts();
		apply_rows();
		update();
	}

	void server_card::apply_cli_accounts()
	{
		const auto& accounts = Presentation.CliAccounts;
		CliAccounts->setVisible(SetupMode ? usable_cli_accounts(accounts) > 1U : !accounts.empty());
		CliCheckButton->setVisible(Presentation.CliCheckAdmitted && !SetupMode);
		// Rebuilt whole: a list of a few rows that changes only when gh is
		// checked or a profile starts or stops using an account.
		for (auto* const row : CliAccountList->findChildren<QWidget*>(QStringLiteral("serverCardCliAccount"), Qt::FindDirectChildrenOnly)) {
			row->hide();
			row->deleteLater();
		}
		auto* const list   = static_cast<QVBoxLayout*>(CliAccountList->layout());
		const auto  using_ = [this](const server_card_cli_account& aAccount) {
			return Presentation.State == server_card_state::connected && Presentation.Session.has_value() && aAccount.Profile.has_value()
				&& Presentation.Session->Profile == *aAccount.Profile;
		};
		for (const auto& account : accounts) {
			const auto  login  = text(account.Login);
			auto* const row    = new QWidget{ CliAccountList };
			auto* const layout = new QGridLayout{ row };
			row->setObjectName(QStringLiteral("serverCardCliAccount"));
			row->setProperty("login", login);
			row->setAccessibleName(login);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setHorizontalSpacing(column_gap);
			layout->setVerticalSpacing(2);
			auto* const avatar = make_label(row, QStringLiteral("serverCardCliAccountAvatar"), login.left(1).toUpper());
			avatar->setFixedSize(avatar_side, avatar_side);
			avatar->setAlignment(Qt::AlignCenter);
			avatar->setAutoFillBackground(true);
			avatar->setBackgroundRole(QPalette::Button);
			avatar->setTextInteractionFlags(Qt::NoTextInteraction);
			auto* const name     = make_label(row, QStringLiteral("serverCardCliAccountLogin"), login);
			auto* const sentence = make_quiet_label(row, QStringLiteral("serverCardCliAccountSentence"), cli_account_sentence(account));
			name->setAccessibleName(QStringLiteral("GitHub CLI account"));
			sentence->setAccessibleName(QStringLiteral("About %1").arg(login));
			if (account.State != server_card_cli_account_state::signed_in) {
				name->setForegroundRole(QPalette::PlaceholderText);
			}
			layout->addWidget(avatar, 0, 0, 2, 1, Qt::AlignTop);
			layout->addWidget(name, 0, 1);
			layout->addWidget(sentence, 1, 1);
			layout->setColumnStretch(1, 1);
			if (using_(account)) {
				auto* const here = make_label(row, QStringLiteral("serverCardCliAccountUsedHere"), QStringLiteral("Used here"));
				here->setAccessibleName(QStringLiteral("%1 is this profile's account").arg(login));
				layout->addWidget(here, 0, 2, Qt::AlignRight | Qt::AlignTop);
			} else if (account.State == server_card_cli_account_state::signed_in && account.Profile.has_value() && Presentation.ConnectAdmitted) {
				auto* const use = make_button(row, QStringLiteral("serverCardCliAccountUse"), QStringLiteral("Use for this profile"),
											  QStringLiteral("Use %1 for this profile").arg(login));
				use->setFlat(true);
				use->setAccessibleDescription(QStringLiteral("Connects this profile to %1 on %2 through gh. Asks first.").arg(login, text(hostname())));
				use->setToolTip(use->accessibleDescription());
				layout->addWidget(use, 0, 2, Qt::AlignRight | Qt::AlignTop);
				connect(use, &QPushButton::clicked, this, [this, profile = *account.Profile] {
					if (Gestures.Connect) {
						Gestures.Connect(profile);
					}
				});
			}
			list->addWidget(row);
		}
	}

	auto server_card::cli_accounts() const noexcept -> QWidget*
	{
		return CliAccounts;
	}

	auto server_card::cli_account_row(const QString& aLogin) const -> QWidget*
	{
		for (auto* const row : CliAccountList->findChildren<QWidget*>(QStringLiteral("serverCardCliAccount"), Qt::FindDirectChildrenOnly)) {
			if (row->isVisibleTo(CliAccountList) && row->property("login").toString() == aLogin) {
				return row;
			}
		}
		return nullptr;
	}

	auto server_card::cli_check_button() const noexcept -> QPushButton*
	{
		return CliCheckButton;
	}

	void server_card::set_public_preview_dispatch(
		application::public_preview_gesture_dispatch aDispatch)
	{
		UsageIndicator->set_dispatch(std::move(aDispatch));
	}

	void server_card::adopt_device_sign_in(device_authorization_panel* const aPanel)
	{
		if (aPanel == nullptr || aPanel == DevicePanel) {
			return;
		}
		release_device_sign_in();
		DevicePanel = aPanel;
		// Beneath who is signed in, across the terms and the ways in, so a
		// renewal that needs a new code can sit under a signed-in account and a
		// running flow can take the whole row.
		AccountLayout->addWidget(DevicePanel, 4, 1, 1, 2);
		DevicePanel->setVisible(Presentation.DeviceSignIn);
		Applied = false;
		apply(Presentation);
	}

	void server_card::release_device_sign_in()
	{
		if (DevicePanel == nullptr) {
			return;
		}
		AccountLayout->removeWidget(DevicePanel);
		DevicePanel->hide();
		DevicePanel = nullptr;
		Applied     = false;
		apply(Presentation);
	}

	auto server_card::device_sign_in() const noexcept -> device_authorization_panel*
	{
		return DevicePanel;
	}

	void server_card::apply_header()
	{
		const auto& presented = option();
		const auto  kind      = kind_text(presented);
		KindLabel->setText(kind);
		setAccessibleDescription(kind);
		setProperty("hostProfile", QVariant::fromValue<qulonglong>(presented.Profile.value()));
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
		const auto&       shown        = Presentation;
		const auto&       presented    = option();
		const auto* const native       = native_option();
		const auto        cliPresented = command_line(presented);
		const auto        signedIn     = this->signed_in();
		const auto        waiting      = shown.State == server_card_state::connecting
			|| shown.State == server_card_state::signing_in;
		const auto login = signedIn ? text(shown.Session->Login) : QString{ };
		Avatar->setText(login.isEmpty() ? QStringLiteral("?") : login.left(1).toUpper());
		LoginLabel->setText(!signedIn && CliSetupOnly && cli_option() ? cli_offer_text(*cli_option())
								: login.isEmpty()                     ? QStringLiteral("No account")
																	  : login);

		// [[BPR-0773]]. While signing in, the account row is the device sign-in:
		// the code, the browser and the way out replace who is signed in and the
		// ways in. A renewal that needs a new code sits beneath a signed-in
		// account instead of replacing it.
		const auto deviceRow = shown.DeviceSignIn && DevicePanel != nullptr;
		const auto flowRow   = deviceRow && !signedIn;
		const auto hadFocus  = SignInButton->hasFocus();
		if (DevicePanel != nullptr) {
			DevicePanel->setVisible(deviceRow);
		}
		LoginLabel->setVisible(!flowRow);
		Actions->setVisible(!flowRow);

		// [[BPR-0644]]. A failed session whose remembered grant was kept.
		const auto kept        = !signedIn && shown.KeptGrant != kept_grant::none;
		auto       consequence = QString{ };
		if (signedIn) {
			consequence = grant_sentence(*shown.Session);
		} else if (kept) {
			consequence = kept_grant_sentence(shown.KeptGrant);
		} else if (shown.ReadingPublic) {
			consequence = QStringLiteral("Reading public pull requests without an account.");
		} else {
			consequence = signed_out_sentence(presented);
		}
		ConsequenceLabel->setText(consequence);
		ConsequenceLabel->setVisible(!flowRow && !consequence.isEmpty());

		// The ways in or out, never a disabled one. The chosen host signs in
		// with its native profile, and that Sign in is the page's one filled
		// control while the card speaks for it. A host not chosen connects the
		// profile it speaks for; a chosen GitHub CLI profile's retry is its
		// Connect. The GitHub CLI offer stays beside them.
		SignOutButton->setVisible(signedIn || kept);
		const auto signIn = !CliSetupOnly && shown.Current && !signedIn && !waiting && native != nullptr && shown.SignInAdmitted;
		SignInButton->setVisible(signIn);
		SignInButton->setDefault(signIn && !cliPresented);
		const auto signInTerms = native != nullptr ? signed_out_sentence(*native) : QString{ };
		SignInButton->setAccessibleDescription(signInTerms);
		SignInButton->setToolTip(signInTerms);
		ConnectButton->setVisible(
			!signedIn && !waiting && shown.ConnectAdmitted
			&& (!shown.Current || cliPresented) && (!CliSetupOnly || cliPresented));
		ConnectButton->setAccessibleDescription(shown.ConnectDescription);
		ConnectButton->setToolTip(shown.ConnectDescription);
		if (CliOfferButton != nullptr) {
			// [[BPR-0803]]. Where sign-in asked for GitHub CLI and this host has
			// a discovered account, the offer stands beside a running device
			// flow's code rather than giving way to it: the reviewer ends one
			// or the other.
			// [[BPR-1373]]. On Accounts & hosts, once gh has been checked, its
			// account list is the way in and the single offer gives way to it.
			// Signing in keeps the single offer ([[BPR-0803]]) unless the host
			// has more than one account to choose between.
			const auto listed  = SetupMode ? usable_cli_accounts(shown.CliAccounts) > 1U : !shown.CliAccounts.empty();
			const auto offered = (!SetupMode || CliSetupOnly) && !cliPresented && shown.ConnectAdmitted
				&& (flowRow ? CliSetupOnly : !waiting) && !listed;
			CliOfferButton->setVisible(offered);
			CliTrustWarning->setVisible(!SetupMode && !flowRow && (offered || cliPresented || !shown.CliAccounts.empty()));
		}
		// The Sign in that began the flow is gone from under the keyboard; its
		// focus moves to the row's job rather than to whatever is next.
		if (flowRow && hadFocus) {
			DevicePanel->focus_primary();
		}
	}

	void server_card::apply_rows()
	{
		const auto& shown    = Presentation;
		const auto  signedIn = this->signed_in();

		AccessLabel->setText(access_sentence(shown.Capabilities));
		const auto access = signedIn;
		AccessRow->setVisible(access);

		auto rate = QString{ };
		if (shown.ReadingPublic && shown.PublicPreview.Budget.has_value()) {
			const auto& budget = *shown.PublicPreview.Budget;
			rate               = remaining_text(budget.Remaining, budget.Limit)
				+ QStringLiteral(" without an account") + reset_text(budget.ResetUnixSeconds) + QLatin1Char('.');
		} else if (signedIn && shown.RateLimits.Core.has_value()) {
			rate = rate_sentence(*shown.RateLimits.Core);
		}
		RateLimitLabel->setText(rate);
		RateLimitLabel->setVisible(!rate.isEmpty());
		UsageIndicator->apply(shown.PublicPreview, shown.PublicPreviewNow);
		UsageIndicator->setVisible(shown.ReadingPublic);
		const auto rateRow = !rate.isEmpty() || shown.ReadingPublic;
		RateLimitRow->setVisible(rateRow);

		// [[BPR-0774]]. Remember and Forget are an enterprise server's, while
		// the queue that keeps servers can be asked.
		const auto remember = enterprise() && shown.RememberAdmitted;
		const auto forget   = remember && shown.Remembered.has_value();
		RememberRow->setVisible(remember);
		{
			const QSignalBlocker blocker{ RememberSwitch };
			RememberSwitch->setChecked(shown.Remembered.has_value());
		}
		ForgetRow->setVisible(forget);

		const auto details = shown.Current || !shown.LastProblem.isEmpty();
		if (!details && DetailsButton->isChecked()) {
			DetailsButton->setChecked(false);
		}
		DetailsRow->setVisible(details);
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

		// One hairline between rows, none above the first.
		const std::array shownRows{ access, rateRow, remember, forget, details };
		auto             above = false;
		for (std::size_t index = 0U; index < shownRows.size(); ++index) {
			if (index > 0U) {
				RowHairlines[index - 1U]->setVisible(shownRows[index] && above);
			}
			above = above || shownRows[index];
		}
		Rows->setVisible(above);
		RowsSeparator->setVisible(above);
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

	auto server_card::find_option(const core::host_profile_id aProfile) const noexcept
		-> const application::github_profile_option*
	{
		const auto found = std::ranges::find_if(Options, [aProfile](const application::github_profile_option& aHeld) {
			return aHeld.Profile == aProfile;
		});
		return found == Options.end() ? nullptr : &*found;
	}

	auto server_card::hostname() const noexcept -> const std::string&
	{
		return Options.front().Hostname;
	}

	auto server_card::holds(const core::host_profile_id aProfile) const noexcept -> bool
	{
		return find_option(aProfile) != nullptr;
	}

	auto server_card::options() const noexcept
		-> const std::vector<application::github_profile_option>&
	{
		return Options;
	}

	auto server_card::native_option() const noexcept
		-> const application::github_profile_option*
	{
		const auto found = std::ranges::find_if(Options, [](const application::github_profile_option& aHeld) {
			return !command_line(aHeld);
		});
		return found == Options.end() ? nullptr : &*found;
	}

	auto server_card::cli_option() const noexcept
		-> const application::github_profile_option*
	{
		const auto found = std::ranges::find_if(Options, [](const application::github_profile_option& aHeld) {
			return command_line(aHeld);
		});
		return found == Options.end() ? nullptr : &*found;
	}

	auto server_card::profile() const noexcept -> core::host_profile_id
	{
		return option().Profile;
	}

	auto server_card::option() const noexcept -> const application::github_profile_option&
	{
		if (Presentation.Chosen.has_value()) {
			if (const auto* const chosen = find_option(*Presentation.Chosen); chosen != nullptr) {
				return *chosen;
			}
		}
		return Options.front();
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
		return std::ranges::any_of(Options, [](const application::github_profile_option& aHeld) {
			return device_capable(aHeld);
		});
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

	auto server_card::cli_offer_button() const noexcept -> QPushButton*
	{
		return CliOfferButton;
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

	auto server_card::enterprise() const noexcept -> bool
	{
		return std::ranges::any_of(Options, [](const application::github_profile_option& aHeld) {
			return aHeld.Product == application::github_profile_product::github_enterprise_server;
		});
	}

	auto server_card::remember_switch() const noexcept -> QCheckBox*
	{
		return RememberSwitch;
	}

	auto server_card::remember_glyph() const noexcept -> info_glyph*
	{
		return RememberGlyph;
	}

	auto server_card::forget_button() const noexcept -> QPushButton*
	{
		return ForgetButton;
	}

	auto server_card::forget_glyph() const noexcept -> info_glyph*
	{
		return ForgetGlyph;
	}
} // namespace bpr::qt
