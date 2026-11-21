#include "bpr/qt/account_setup_view.hpp"

#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "bpr/qt/sensitive_text_input.hpp"

#include "setup_step_parts.hpp"
#include "this_machine_text.hpp"

#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::qt {
	namespace {
		using namespace setup_parts;
		using core::account_setup_phase;
		using core::account_setup_problem;
		using core::account_setup_step;
		using core::account_setup_way;

		[[nodiscard]] auto text_of(const std::string& aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		/// \brief The device code, large and spaced, on a plate of its own:
		/// the one thing in the setup the reviewer has to carry elsewhere.
		class code_plate final : public QWidget
		{
		public:
			explicit code_plate(QWidget* aParent)
				: QWidget{ aParent }
			{ }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto base   = palette().color(QPalette::Active, QPalette::Base);
				const auto accent = palette().color(QPalette::Active, QPalette::Highlight);
				painter.setPen(QPen{ mixed(base, accent, 0.55), 1.5 });
				painter.setBrush(mixed(base, accent, 0.05));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.75, 0.75, -0.75, -0.75), 10.0, 10.0);
			}
		};

		/// \brief One line of the setup's checks: its mark, then its words.
		[[nodiscard]] auto check_row(const core::setup_check_state aState, const QString& aText, QWidget* aParent, const QString& aName) -> QWidget*
		{
			auto* const row    = new QWidget{ aParent };
			auto* const layout = new QHBoxLayout{ row };
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(8);
			const auto [glyph, colour] = setup_check_mark(aState, aParent->palette());
			auto* const mark           = new QLabel{ glyph, row };
			auto        markPalette    = mark->palette();
			markPalette.setColor(QPalette::WindowText, colour);
			mark->setPalette(markPalette);
			mark->setAccessibleName(aState == core::setup_check_state::passed        ? QObject::tr("Done")
										: aState == core::setup_check_state::failed  ? QObject::tr("Problem")
										: aState == core::setup_check_state::running ? QObject::tr("In progress")
																					 : QObject::tr("Not started"));
			layout->addWidget(mark, 0, Qt::AlignTop);
			auto* const label = words(aText, row);
			label->setObjectName(aName);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			layout->addWidget(label, 1);
			return row;
		}

		[[nodiscard]] auto expiry_text(const std::uint32_t aSeconds) -> QString
		{
			return QObject::tr("The code expires in %1:%2.").arg(aSeconds / 60U).arg(aSeconds % 60U, 2, 10, QLatin1Char{ '0' });
		}

		[[nodiscard]] auto action_row(QWidget* aParent) -> std::pair<QWidget*, QHBoxLayout*>
		{
			auto* const row    = new QWidget{ aParent };
			auto* const layout = new QHBoxLayout{ row };
			layout->setContentsMargins(0, 4, 0, 0);
			layout->setSpacing(8);
			return { row, layout };
		}
	} // namespace

	account_setup_view::account_setup_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("accountSetup"));
		setAccessibleName(tr("Connect your GitHub account"));
		auto* const outer = new QVBoxLayout{ this };
		outer->setContentsMargins(0, 0, 0, 0);
		auto* const scroll = new QScrollArea{ this };
		scroll->setObjectName(QStringLiteral("accountSetupScroll"));
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const content = new QWidget{ scroll };
		auto* const column  = new QVBoxLayout{ content };
		column->setContentsMargins(24, 18, 24, 16);
		column->setSpacing(10);
		auto* const title = new QLabel{ tr("Connect your GitHub account"), content };
		Title             = title;
		title->setObjectName(QStringLiteral("accountSetupTitle"));
		auto heading = title->font();
		heading.setPointSizeF(heading.pointSizeF() * 1.35);
		heading.setWeight(QFont::DemiBold);
		title->setFont(heading);
		auto* const titleRow = new QHBoxLayout{ };
		titleRow->setContentsMargins(0, 0, 0, 0);
		titleRow->addWidget(title);
		titleRow->addStretch(1);
		// Closing is offered once the window has an account: Settings' Add
		// account and Use another account can be left without adding one.
		Close = link_button(tr("Close"), content, QStringLiteral("accountSetupClose"));
		Close->setAccessibleDescription(tr("Leaves the setup. The account already connected stays connected."));
		Close->hide();
		connect(Close, &QPushButton::clicked, this, [this] { dispatch(core::finish_account_setup{ }); });
		titleRow->addWidget(Close);
		column->addLayout(titleRow);
		Intro = muted(tr("BetterPullReview reads pull requests as you, on %1. Nothing is published until you choose to publish it. "
						 "To read a public pull request without an account, paste its link in the bar above.")
						  .arg(detail::this_machine_text()),
					  content);
		Intro->setObjectName(QStringLiteral("accountSetupIntro"));
		column->addWidget(Intro);
		column->addSpacing(4);
		Steps = new QVBoxLayout{ };
		Steps->setSpacing(10);
		column->addLayout(Steps);
		column->addStretch(1);
		scroll->setWidget(content);
		outer->addWidget(scroll, 1);
		rebuild();
	}

	void account_setup_view::set_intents(account_setup_intents aIntents) { Intents = std::move(aIntents); }

	void account_setup_view::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		// Marks, the code's font and the plates are derived from the palette
		// and font when built, so a theme or font change builds them again.
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			rebuild();
		}
	}

	auto account_setup_view::status_text() const -> QString
	{
		const auto& state = Projection.State;
		if (state.Problem != account_setup_problem::none) {
			return problem_words(state.Problem);
		}
		switch (state.Phase) {
		case account_setup_phase::idle:             return { };
		case account_setup_phase::choosing_server:  return tr("Choose the server your account is on.");
		case account_setup_phase::preparing_server: return tr("Adding %1.").arg(text_of(state.Hostname));
		case account_setup_phase::choosing_way:     return tr("Choose how to connect your account.");
		case account_setup_phase::entering_token:   return tr("Enter a personal access token for %1.").arg(text_of(state.Hostname));
		case account_setup_phase::requesting_code:  return tr("Getting a sign-in code from GitHub.");
		case account_setup_phase::awaiting_browser: return Projection.Device.UserCode.empty() ? QString{ } : tr("Your sign-in code is %1. Enter it on GitHub.").arg(text_of(Projection.Device.UserCode));
		case account_setup_phase::browser_stopped:  return { };
		case account_setup_phase::inspecting_cli:   return tr("Looking for the GitHub CLI and its accounts.");
		case account_setup_phase::choosing_account: return tr("Choose a GitHub CLI account.");
		case account_setup_phase::connecting:       return tr("Checking the account with GitHub.");
		case account_setup_phase::connected:        return tr("Connected as %1.").arg(text_of(state.Login));
		}
		return { };
	}

	void account_setup_view::announce()
	{
		const auto text = status_text();
		if (text.isEmpty() || text == LastAnnouncement || !isVisible()) {
			return;
		}
		LastAnnouncement = text;
		++Announcements;
		auto event = QAccessibleAnnouncementEvent{ this, LastAnnouncement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	void account_setup_view::present(const application::account_setup_projection& aProjection)
	{
		const auto wayChanged    = aProjection.State.Way != Projection.State.Way;
		const auto serverChanged = aProjection.State.Server != Projection.State.Server || aProjection.State.Hostname != Projection.State.Hostname;
		Projection               = aProjection;
		const auto& state        = Projection.State;
		if (serverChanged && state.Server.has_value()) {
			PickedServer = *state.Server;
			if (*state.Server == core::account_setup_server::enterprise) {
				EnterpriseHost = text_of(state.Hostname);
			}
		}
		const auto server = state.Server.value_or(PickedServer);
		if (wayChanged && state.Way.has_value()) {
			Picked = *state.Way;
		} else if (serverChanged || !core::account_setup_way_offered(server, Picked)) {
			Picked = server == core::account_setup_server::enterprise ? account_setup_way::token : account_setup_way::browser;
		}
		if (!Projection.GithubCliOffered && Picked == account_setup_way::github_cli) {
			Picked = server == core::account_setup_server::enterprise ? account_setup_way::token : account_setup_way::browser;
		}
		if (state.Phase != account_setup_phase::entering_token) {
			TokenNotice.clear();
		}
		if (Projection.State.Phase != account_setup_phase::choosing_account) {
			DetailsOpen = false;
		}
		rebuild();
		announce();
	}

	void account_setup_view::dispatch(core::account_setup_event aEvent)
	{
		if (Intents.Dispatch) {
			Intents.Dispatch(std::move(aEvent));
		}
	}

	auto account_setup_view::chosen_login() const -> QString
	{
		const auto& state = Projection.State;
		for (const auto& account : state.Accounts) {
			if (state.Chosen == account.Profile) {
				return text_of(account.Login);
			}
		}
		return { };
	}

	auto account_setup_view::problem_words(const core::account_setup_problem aProblem) -> QString
	{
		switch (aProblem) {
		case account_setup_problem::none:               break;
		case account_setup_problem::invalid_hostname:   return tr("Enter the server's hostname, such as github.example.com, with no scheme or path.");
		case account_setup_problem::server_unavailable: return tr("The server couldn't be added. Check the hostname, then try again.");
#if defined(_WIN32)
		case account_setup_problem::gh_missing: return tr("The GitHub CLI isn't installed. Install gh 2.96 or newer (for example, winget install GitHub.cli), then check again.");
#elif defined(__APPLE__)
		case account_setup_problem::gh_missing: return tr("The GitHub CLI isn't installed. Install gh 2.96 or newer in PATH or /usr/local/bin (for example, brew install gh), then check again.");
#else
		case account_setup_problem::gh_missing: return tr("The GitHub CLI isn't installed. Install gh 2.96 or newer in PATH or /usr/bin, then check again.");
#endif
		case account_setup_problem::gh_outdated:          return tr("This GitHub CLI is too old to list its accounts. Upgrade to gh 2.96 or newer, then check again.");
		case account_setup_problem::gh_failed:            return tr("The GitHub CLI couldn't be run or read. Check that gh auth status works in a terminal, then check again.");
		case account_setup_problem::gh_signed_out:        return tr("The GitHub CLI has no signed-in account. Sign in from a terminal, then check again:");
		case account_setup_problem::gh_no_usable_account: return tr("The GitHub CLI has accounts, but none is signed in with secure storage. Run gh auth status in a terminal to see why, or choose another way to connect.");
		case account_setup_problem::code_expired:         return tr("The code expired before it was entered. Get a new code to try again.");
		case account_setup_problem::code_denied:          return tr("The code was declined on GitHub. Get a new code if that wasn't you.");
		case account_setup_problem::browser_failed:       return tr("GitHub couldn't be reached to finish signing in. Check the connection, then get a new code.");
		case account_setup_problem::browser_unavailable:  return tr("Browser sign-in isn't available here. Choose another way to connect.");
		case account_setup_problem::connection_failed:    return tr("GitHub didn't accept this account or token. Try again, or choose another way to connect.");
		}
		return { };
	}

	auto account_setup_view::step_text() const -> QString
	{
		auto       text    = QString{ };
		const auto collect = [&text](const QObject* aObject, const auto& aSelf) -> void {
			for (const auto* const child : aObject->children()) {
				if (const auto* const widget = qobject_cast<const QWidget*>(child); widget != nullptr && widget->isHidden()) {
					continue;
				}
				if (const auto* const label = qobject_cast<const QLabel*>(child); label != nullptr && !label->text().isEmpty()) {
					text += label->text() + QLatin1Char{ '\n' };
				}
				if (const auto* const button = qobject_cast<const QPushButton*>(child); button != nullptr && !button->text().isEmpty()) {
					text += QStringLiteral("[") + button->text() + QStringLiteral("]\n");
				}
				aSelf(child, aSelf);
			}
		};
		collect(this, collect);
		return text;
	}

	void account_setup_view::rebuild()
	{
		while (auto* const item = Steps->takeAt(0)) {
			if (auto* const widget = item->widget()) {
				// Out of the view at once; deleted once any press inside it returned.
				widget->hide();
				widget->setParent(nullptr);
				widget->deleteLater();
			}
			delete item;
		}
		const auto& state   = Projection.State;
		const auto  current = Projection.Step;
		const auto  way     = state.Way.value_or(Picked);
		struct step
		{
			account_setup_step Step;
			QString            Title;
			QString            Summary;
		};
		const auto serverName = state.Server == core::account_setup_server::enterprise ? text_of(state.Hostname) : tr("GitHub.com");
		const auto wayName    = way == account_setup_way::browser ? tr("Browser sign-in") : way == account_setup_way::github_cli ? tr("GitHub CLI")
																																 : tr("Personal access token");
		const auto signInName = !state.Way.has_value() || way == account_setup_way::browser ? tr("Sign in")
			: way == account_setup_way::github_cli                                          ? tr("Choose an account")
																							: tr("Enter a token");
		const auto signedName = way == account_setup_way::browser ? tr("Code entered") : way == account_setup_way::github_cli ? chosen_login()
																															  : tr("Token entered");
		const auto steps      = std::array{
			step{ account_setup_step::server, tr("Server"), state.Server.has_value() ? serverName : QString{ } },
			step{ account_setup_step::way, tr("How to connect"), state.Way.has_value() ? wayName : QString{ } },
			step{ account_setup_step::sign_in, signInName, signedName },
			step{ account_setup_step::confirm, tr("Confirm the account"), text_of(state.Login) },
			step{ account_setup_step::ready, tr("Ready"), QString{ } },
		};
		const auto settled = current == account_setup_step::confirm || current == account_setup_step::ready;
		Close->setVisible(Projection.SignedIn && state.Phase != account_setup_phase::idle && !settled);
		Title->setText(Projection.SignedIn ? tr("Add a GitHub account") : tr("Connect your GitHub account"));
		for (auto index = std::size_t{ 0U }; index < steps.size(); ++index) {
			const auto& item  = steps[index];
			const auto  order = static_cast<int>(item.Step);
			const auto  open  = item.Step == current;
			const auto  done  = order < static_cast<int>(current);
			auto* const block = new QWidget{ this };
			block->setObjectName(QStringLiteral("accountSetupStep%1").arg(index + 1U));
			auto* const blockColumn = new QVBoxLayout{ block };
			blockColumn->setContentsMargins(0, 0, 0, 0);
			blockColumn->setSpacing(6);
			auto* const headerRow = new step_header{ block };
			headerRow->setObjectName(QStringLiteral("accountSetupStepHeader"));
			auto* const header = new QHBoxLayout{ headerRow };
			header->setContentsMargins(0, 2, 0, 2);
			header->setSpacing(10);
			header->addWidget(new step_marker{ order + 1, done ? step_marker::state::done : open ? step_marker::state::open
																								 : step_marker::state::ahead,
											   block });
			auto* const name = new QLabel{ item.Title, block };
			auto        bold = name->font();
			bold.setWeight(QFont::DemiBold);
			name->setFont(bold);
			if (!open && !done) {
				name->setForegroundRole(QPalette::PlaceholderText);
			}
			header->addWidget(name);
			if (done && !item.Summary.isEmpty()) {
				auto* const summary = muted(item.Summary, block);
				summary->setWordWrap(false);
				summary->setObjectName(QStringLiteral("accountSetupSummary"));
				summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
				header->addWidget(summary, 1);
			} else {
				header->addStretch(1);
			}
			// The server and the way can be changed until the account is being
			// confirmed: Change goes back, and ends a code still shown.
			if (done && !settled && (item.Step == account_setup_step::server || item.Step == account_setup_step::way)) {
				const auto  toServer = item.Step == account_setup_step::server;
				auto* const change   = link_button(tr("Change"), block, toServer ? QStringLiteral("accountSetupChangeServer") : QStringLiteral("accountSetupChange"));
				change->setAccessibleName(toServer ? tr("Change the server") : tr("Change how to connect"));
				const auto back = [this, toServer] {
					if (toServer) {
						dispatch(core::return_to_account_setup_server{ });
					} else {
						dispatch(core::return_to_account_setup_way{ });
					}
				};
				connect(change, &QPushButton::clicked, this, back);
				header->addWidget(change);
				headerRow->Activate = back;
				headerRow->setToolTip(toServer ? tr("Choose another server") : tr("Choose another way to connect"));
			}
			headerRow->settle();
			blockColumn->addWidget(headerRow);
			if (open && state.Phase != account_setup_phase::idle) {
				auto* const body       = new QWidget{ block };
				auto* const bodyColumn = new QVBoxLayout{ body };
				bodyColumn->setContentsMargins(34, 0, 0, 4);
				bodyColumn->setSpacing(8);
				switch (item.Step) {
				case account_setup_step::server: build_server(*bodyColumn); break;
				case account_setup_step::way:    build_way(*bodyColumn); break;
				case account_setup_step::sign_in:
					if (way == account_setup_way::browser) {
						build_browser(*bodyColumn);
					} else if (way == account_setup_way::github_cli) {
						build_github_cli(*bodyColumn);
					} else {
						build_token(*bodyColumn);
					}
					break;
				case account_setup_step::confirm: build_confirm(*bodyColumn); break;
				case account_setup_step::ready:   build_ready(*bodyColumn); break;
				}
				blockColumn->addWidget(body);
			}
			Steps->addWidget(block);
			// Shown now rather than on the layout's queued show, so what the
			// setup shows is what it was last given.
			block->show();
		}
		setAccessibleDescription(step_text().simplified());
	}

	void account_setup_view::build_way(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		struct way_card
		{
			account_setup_way Way;
			QString           Title;
			QString           Detail;
			QString           Name;
			bool              Offered;
		};
		const auto server = Projection.State.Server.value_or(core::account_setup_server::github_com);
		const auto cards  = std::array{
			way_card{ account_setup_way::browser, tr("Sign in with your browser"),
					  tr("GitHub shows a sign-in page; you enter a one-time code from here. Works with any GitHub.com account."), QStringLiteral("accountSetupWayBrowser"), true },
			way_card{ account_setup_way::github_cli, tr("Use the GitHub CLI"),
					  Projection.GithubCliOffered ? tr("Connect an account gh already holds for %1 on %2. Its token is never saved here.").arg(text_of(Projection.State.Hostname), detail::this_machine_text())
												  : tr("Not available in this build of BetterPullReview."),
					  QStringLiteral("accountSetupWayCli"), Projection.GithubCliOffered },
			way_card{ account_setup_way::token, tr("Use a personal access token"),
					  tr("Create a token on %1 and paste it here. It is kept in the system's secure storage, never in settings or logs.").arg(text_of(Projection.State.Hostname)),
					  QStringLiteral("accountSetupWayToken"), true },
		};
		for (const auto& item : cards) {
			if (!core::account_setup_way_offered(server, item.Way)) {
				continue;
			}
			auto* const card = new choice_card{ parent };
			card->setObjectName(item.Name);
			card->setAccessibleName(item.Title);
			card->setAccessibleDescription(item.Detail);
			card->Chosen    = item.Offered && Picked == item.Way;
			card->Disabled  = !item.Offered;
			auto* const row = new QHBoxLayout{ card };
			row->setContentsMargins(12, 10, 12, 10);
			row->setSpacing(10);
			auto* const radio = new QRadioButton{ card };
			radio->setChecked(card->Chosen);
			radio->setEnabled(item.Offered);
			radio->setAccessibleName(item.Title);
			row->addWidget(radio, 0, Qt::AlignTop);
			auto* const text = new QVBoxLayout{ };
			text->setSpacing(2);
			auto* const name = words(item.Title, card);
			auto        bold = name->font();
			bold.setWeight(QFont::DemiBold);
			name->setFont(bold);
			if (!item.Offered) {
				name->setForegroundRole(QPalette::PlaceholderText);
			}
			text->addWidget(name);
			text->addWidget(muted(item.Detail, card));
			row->addLayout(text, 1);
			const auto way = item.Way;
			card->Activate = [this, way] {
				Picked = way;
				rebuild();
			};
			card->settle();
			aBody.addWidget(card);
		}
		auto [actions, actionRow] = action_row(parent);
		const auto  label         = Picked == account_setup_way::browser ? tr("Get a sign-in code") : Picked == account_setup_way::github_cli ? tr("Check the GitHub CLI")
																																			  : tr("Continue");
		auto* const next          = primary_button(label, actions, QStringLiteral("accountSetupContinue"));
		connect(next, &QPushButton::clicked, this, [this] { dispatch(core::choose_account_setup_way{ .Way = Picked }); });
		actionRow->addWidget(next);
		actionRow->addStretch(1);
		aBody.addWidget(actions);
	}

	void account_setup_view::build_server(QVBoxLayout& aBody)
	{
		auto* const parent    = aBody.parentWidget();
		const auto& state     = Projection.State;
		const auto  preparing = state.Phase == account_setup_phase::preparing_server;
		struct server_card
		{
			core::account_setup_server Server;
			QString                    Title;
			QString                    Detail;
			QString                    Name;
		};
		const auto cards = std::array{
			server_card{ core::account_setup_server::github_com, tr("GitHub.com"), tr("Your account on github.com."), QStringLiteral("accountSetupServerGithub") },
			server_card{ core::account_setup_server::enterprise, tr("GitHub Enterprise Server"), tr("Your organization's own GitHub, at its own address."),
						 QStringLiteral("accountSetupServerEnterprise") },
		};
		QLineEdit* host = nullptr;
		for (const auto& item : cards) {
			auto* const card = new choice_card{ parent };
			card->setObjectName(item.Name);
			card->setAccessibleName(item.Title);
			card->setAccessibleDescription(item.Detail);
			card->Chosen    = PickedServer == item.Server;
			card->Disabled  = preparing;
			auto* const row = new QHBoxLayout{ card };
			row->setContentsMargins(12, 10, 12, 10);
			row->setSpacing(10);
			auto* const radio = new QRadioButton{ card };
			radio->setChecked(card->Chosen);
			radio->setAccessibleName(item.Title);
			row->addWidget(radio, 0, Qt::AlignTop);
			auto* const text = new QVBoxLayout{ };
			text->setSpacing(4);
			auto* const name = words(item.Title, card);
			auto        bold = name->font();
			bold.setWeight(QFont::DemiBold);
			name->setFont(bold);
			text->addWidget(name);
			text->addWidget(muted(item.Detail, card));
			// The host is typed where the choice is made.
			if (item.Server == core::account_setup_server::enterprise && card->Chosen) {
				host = new QLineEdit{ card };
				host->setObjectName(QStringLiteral("accountSetupHostname"));
				host->setAccessibleName(tr("Server hostname"));
				host->setPlaceholderText(tr("github.example.com"));
				host->setText(EnterpriseHost);
				host->setEnabled(!preparing);
				connect(host, &QLineEdit::textEdited, this, [this](const QString& aText) { EnterpriseHost = aText; });
				text->addWidget(host);
			}
			row->addLayout(text, 1);
			const auto server = item.Server;
			card->Activate    = [this, server] {
				PickedServer = server;
				rebuild();
				if (auto* const field = findChild<QLineEdit*>(QStringLiteral("accountSetupHostname")); field != nullptr && field->isVisibleTo(this)) {
					field->setFocus(Qt::OtherFocusReason);
				}
			};
			card->settle();
			aBody.addWidget(card);
		}
		if (preparing) {
			aBody.addWidget(check_row(core::setup_check_state::running, tr("Adding %1…").arg(text_of(state.Hostname)), parent, QStringLiteral("accountSetupStatus")));
			return;
		}
		if (state.Problem == account_setup_problem::invalid_hostname || state.Problem == account_setup_problem::server_unavailable) {
			auto* const problem = check_row(core::setup_check_state::failed, problem_words(state.Problem), parent, QStringLiteral("accountSetupProblem"));
			aBody.addWidget(problem);
			// Editing the host answers the refusal; it is not said over the new text.
			if (host != nullptr) {
				connect(host, &QLineEdit::textEdited, problem, &QWidget::hide);
			}
		}
		auto [actions, actionRow] = action_row(parent);
		auto* const next          = primary_button(tr("Continue"), actions, QStringLiteral("accountSetupServerContinue"));
		const auto  choose        = [this] {
			auto event   = core::choose_account_setup_server{ };
			event.Server = PickedServer;
			if (PickedServer == core::account_setup_server::enterprise) {
				event.Hostname = EnterpriseHost.trimmed().toLower().toStdString();
			}
			dispatch(std::move(event));
		};
		connect(next, &QPushButton::clicked, this, choose);
		if (host != nullptr) {
			connect(host, &QLineEdit::returnPressed, this, choose);
		}
		actionRow->addWidget(next);
		actionRow->addStretch(1);
		aBody.addWidget(actions);
	}

	void account_setup_view::build_token(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		const auto& state  = Projection.State;
		const auto  host   = text_of(state.Hostname);
		aBody.addWidget(words(tr("Create a personal access token on %1 (Settings, Developer settings, Personal access tokens) with the repo and read:org scopes, then paste it here.").arg(host), parent));
		auto* const field = new QLineEdit{ parent };
		field->setObjectName(QStringLiteral("accountSetupToken"));
		field->setAccessibleName(tr("Personal access token"));
		field->setEchoMode(QLineEdit::Password);
		field->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhNoPredictiveText | Qt::ImhSensitiveData);
		if (Intents.MaximumTokenBytes != 0U) {
			field->setMaxLength(static_cast<int>(std::min<std::size_t>(Intents.MaximumTokenBytes, 65536U)));
		}
		aBody.addWidget(field);
		aBody.addWidget(muted(tr("It is kept in the system's secure storage, never in settings or logs."), parent));
		if (state.Problem == account_setup_problem::connection_failed) {
			aBody.addWidget(check_row(core::setup_check_state::failed, problem_words(state.Problem), parent, QStringLiteral("accountSetupProblem")));
		}
		if (!TokenNotice.isEmpty()) {
			aBody.addWidget(check_row(core::setup_check_state::failed, TokenNotice, parent, QStringLiteral("accountSetupProblem")));
		}
		auto [actions, actionRow] = action_row(parent);
		auto* const signIn        = primary_button(tr("Sign in"), actions, QStringLiteral("accountSetupSubmitToken"));
		// The token goes to the host as erasable bytes, and the field is
		// emptied at once; only then is Sign in a setup event.
		const auto submit = [this, field] {
			auto bytes = take_sensitive_text(*field, Intents.MaximumTokenBytes);
			if (!bytes.has_value()) {
				TokenNotice = tr("Paste a token first. It must be shorter than %1 bytes.").arg(Intents.MaximumTokenBytes);
				rebuild();
				return;
			}
			TokenNotice.clear();
			if (Intents.SubmitToken) {
				Intents.SubmitToken(std::move(*bytes));
			}
			dispatch(core::submit_account_setup_token{ });
		};
		connect(signIn, &QPushButton::clicked, this, submit);
		connect(field, &QLineEdit::returnPressed, this, submit);
		actionRow->addWidget(signIn);
		auto* const back = link_button(tr("Choose another way"), actions, QStringLiteral("accountSetupBack"));
		connect(back, &QPushButton::clicked, this, [this] { dispatch(core::return_to_account_setup_way{ }); });
		actionRow->addWidget(back);
		actionRow->addStretch(1);
		aBody.addWidget(actions);
	}

	void account_setup_view::build_browser(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		const auto& state  = Projection.State;
		const auto& device = Projection.Device;
		if (state.Phase == account_setup_phase::browser_stopped) {
			aBody.addWidget(check_row(core::setup_check_state::failed, problem_words(state.Problem), parent, QStringLiteral("accountSetupProblem")));
			auto [actions, actionRow] = action_row(parent);
			auto* const retry         = primary_button(tr("Get a new code"), actions, QStringLiteral("accountSetupRetry"));
			connect(retry, &QPushButton::clicked, this, [this] { dispatch(core::retry_account_setup{ }); });
			actionRow->addWidget(retry);
			auto* const back = link_button(tr("Choose another way"), actions, QStringLiteral("accountSetupBack"));
			connect(back, &QPushButton::clicked, this, [this] { dispatch(core::return_to_account_setup_way{ }); });
			actionRow->addWidget(back);
			actionRow->addStretch(1);
			aBody.addWidget(actions);
			return;
		}
		if (state.Phase == account_setup_phase::requesting_code || device.UserCode.empty()) {
			aBody.addWidget(check_row(core::setup_check_state::running, tr("Getting a sign-in code from GitHub…"), parent, QStringLiteral("accountSetupStatus")));
		} else {
			aBody.addWidget(words(tr("Enter this code on GitHub's sign-in page:"), parent));
			auto* const plate = new code_plate{ parent };
			plate->setObjectName(QStringLiteral("accountSetupCodePlate"));
			auto* const plateRow = new QHBoxLayout{ plate };
			plateRow->setContentsMargins(18, 12, 12, 12);
			plateRow->setSpacing(12);
			auto* const code = new QLabel{ text_of(device.UserCode), plate };
			code->setObjectName(QStringLiteral("accountSetupCode"));
			code->setAccessibleName(tr("Sign-in code"));
			code->setTextFormat(Qt::PlainText);
			code->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			auto large = code_font(font());
			large.setPointSizeF(font().pointSizeF() * 2.0);
			large.setWeight(QFont::DemiBold);
			large.setLetterSpacing(QFont::PercentageSpacing, 112.0);
			code->setFont(large);
			plateRow->addWidget(code);
			plateRow->addStretch(1);
			auto* const copy = new QPushButton{ tr("Copy code"), plate };
			copy->setObjectName(QStringLiteral("accountSetupCopyCode"));
			copy->setAutoDefault(false);
			connect(copy, &QPushButton::clicked, this, [copy, codeText = text_of(device.UserCode)] {
				QApplication::clipboard()->setText(codeText);
				copy->setText(tr("Copied"));
				QTimer::singleShot(1600, copy, [copy] { copy->setText(tr("Copy code")); });
			});
			plateRow->addWidget(copy, 0, Qt::AlignVCenter);
			aBody.addWidget(plate);
			auto* const where = muted(tr("The page is %1. You can open it on any device.").arg(text_of(device.VerificationUri)), parent);
			where->setObjectName(QStringLiteral("accountSetupAddress"));
			where->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			aBody.addWidget(where);
			auto [actions, actionRow] = action_row(parent);
			auto* const open          = primary_button(tr("Open GitHub"), actions, QStringLiteral("accountSetupOpenGithub"));
			open->setAccessibleDescription(tr("Opens GitHub's sign-in page in your browser. The code stays here."));
			connect(open, &QPushButton::clicked, this, [this] {
				if (Intents.OpenVerificationAddress) {
					Intents.OpenVerificationAddress();
				}
			});
			actionRow->addWidget(open);
			actionRow->addStretch(1);
			auto* const expiry = muted(expiry_text(device.RemainingValiditySeconds), actions);
			expiry->setObjectName(QStringLiteral("accountSetupExpiry"));
			expiry->setWordWrap(false);
			actionRow->addWidget(expiry);
			aBody.addWidget(actions);
			switch (device.Handoff) {
			case application::device_authorization_handoff::not_attempted:
			case application::device_authorization_handoff::opened:        break;
			case application::device_authorization_handoff::refused:
			case application::device_authorization_handoff::failed:
			case application::device_authorization_handoff::unavailable:
				aBody.addWidget(muted(tr("The browser didn't open. Go to the page above on any device and enter the code."), parent));
				break;
			}
			aBody.addWidget(check_row(core::setup_check_state::running, tr("Waiting for you to approve on GitHub…"), parent, QStringLiteral("accountSetupStatus")));
		}
		auto* const back = link_button(tr("Cancel and choose another way"), parent, QStringLiteral("accountSetupBack"));
		back->setAccessibleDescription(tr("Ends this code. It can no longer be used."));
		connect(back, &QPushButton::clicked, this, [this] { dispatch(core::return_to_account_setup_way{ }); });
		aBody.addWidget(back, 0, Qt::AlignLeft);
	}

	void account_setup_view::build_github_cli(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		const auto& state  = Projection.State;
		const auto  back   = [this, parent](QHBoxLayout& aRow) {
			auto* const button = link_button(tr("Choose another way"), parent, QStringLiteral("accountSetupBack"));
			connect(button, &QPushButton::clicked, this, [this] { dispatch(core::return_to_account_setup_way{ }); });
			aRow.addWidget(button);
		};
		if (state.Phase == account_setup_phase::inspecting_cli) {
			aBody.addWidget(check_row(core::setup_check_state::running, tr("Looking for the GitHub CLI and its accounts…"), parent, QStringLiteral("accountSetupStatus")));
			auto [actions, actionRow] = action_row(parent);
			back(*actionRow);
			actionRow->addStretch(1);
			aBody.addWidget(actions);
			return;
		}
		const auto ghProblem = state.Problem != account_setup_problem::none && state.Problem != account_setup_problem::connection_failed;
		if (ghProblem) {
			aBody.addWidget(check_row(core::setup_check_state::failed, problem_words(state.Problem), parent, QStringLiteral("accountSetupProblem")));
			if (state.Problem == account_setup_problem::gh_signed_out) {
				auto* const plate = new code_plate{ parent };
				plate->setObjectName(QStringLiteral("accountSetupCommandPlate"));
				auto* const plateRow = new QHBoxLayout{ plate };
				plateRow->setContentsMargins(14, 8, 8, 8);
				auto* const command = new QLabel{ QStringLiteral("gh auth login"), plate };
				command->setObjectName(QStringLiteral("accountSetupCommand"));
				command->setFont(code_font(font()));
				command->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
				plateRow->addWidget(command);
				plateRow->addStretch(1);
				auto* const copy = new QPushButton{ tr("Copy"), plate };
				copy->setObjectName(QStringLiteral("accountSetupCopyCommand"));
				copy->setAutoDefault(false);
				connect(copy, &QPushButton::clicked, this, [copy] {
					QApplication::clipboard()->setText(QStringLiteral("gh auth login"));
					copy->setText(tr("Copied"));
					QTimer::singleShot(1600, copy, [copy] { copy->setText(tr("Copy")); });
				});
				plateRow->addWidget(copy);
				aBody.addWidget(plate);
			}
			auto [actions, actionRow] = action_row(parent);
			auto* const again         = primary_button(tr("Check again"), actions, QStringLiteral("accountSetupRetry"));
			connect(again, &QPushButton::clicked, this, [this] { dispatch(core::retry_account_setup{ }); });
			actionRow->addWidget(again);
			back(*actionRow);
			actionRow->addStretch(1);
			aBody.addWidget(actions);
			return;
		}
		const auto count = static_cast<int>(state.Accounts.size());
		aBody.addWidget(check_row(core::setup_check_state::passed,
								  count == 1 ? tr("The GitHub CLI has one account on %1.").arg(text_of(state.Accounts.front().Hostname))
											 : tr("The GitHub CLI has %1 accounts. Choose the one to review as.").arg(count),
								  parent, QStringLiteral("accountSetupStatus")));
		for (const auto& account : state.Accounts) {
			auto* const card = new choice_card{ parent };
			card->setObjectName(QStringLiteral("accountSetupAccount"));
			const auto login = text_of(account.Login);
			card->setAccessibleName(login);
			card->Chosen    = state.Chosen == account.Profile;
			auto* const row = new QHBoxLayout{ card };
			row->setContentsMargins(12, 8, 12, 8);
			row->setSpacing(10);
			auto* const radio = new QRadioButton{ card };
			radio->setChecked(card->Chosen);
			radio->setAccessibleName(login);
			row->addWidget(radio);
			auto* const name = words(login, card);
			auto        bold = name->font();
			bold.setWeight(QFont::DemiBold);
			name->setFont(bold);
			row->addWidget(name);
			auto* const where = muted(account.ActiveInGh ? tr("%1, active in gh").arg(text_of(account.Hostname)) : text_of(account.Hostname), card);
			where->setWordWrap(false);
			row->addWidget(where, 1);
			const auto profile = account.Profile;
			card->Activate     = [this, profile] { dispatch(core::choose_account_setup_account{ .Profile = profile }); };
			card->settle();
			aBody.addWidget(card);
		}
		if (state.Problem == account_setup_problem::connection_failed) {
			aBody.addWidget(check_row(core::setup_check_state::failed, problem_words(state.Problem), parent, QStringLiteral("accountSetupProblem")));
		}
		const auto login = chosen_login();
		if (!login.isEmpty()) {
			// The consent, said where the choice is made: the short decision,
			// then RFC-BPR-0015's full residual-risk text behind a disclosure.
			// [[BPR-2319]]. One sentence; reuse and the publishing boundary are
			// in What connecting allows.
			auto* const consent = words(tr("BetterPullReview uses %1's existing gh sign-in and doesn't save its token.").arg(login), parent);
			consent->setObjectName(QStringLiteral("accountSetupConsent"));
			aBody.addWidget(consent);
		}
		auto [actions, actionRow] = action_row(parent);
		auto* const connectButton = primary_button(login.isEmpty() ? tr("Connect") : tr("Connect as %1").arg(login), actions, QStringLiteral("accountSetupConnect"));
		connectButton->setEnabled(!login.isEmpty());
		if (login.isEmpty()) {
			connectButton->setToolTip(tr("Choose an account first."));
		}
		connect(connectButton, &QPushButton::clicked, this, [this] { dispatch(core::connect_account_setup{ }); });
		actionRow->addWidget(connectButton);
		auto* const again = link_button(tr("Check again"), actions, QStringLiteral("accountSetupCheckAgain"));
		again->setToolTip(tr("Ask the GitHub CLI for its accounts again."));
		connect(again, &QPushButton::clicked, this, [this] { dispatch(core::retry_account_setup{ }); });
		actionRow->addWidget(again);
		back(*actionRow);
		actionRow->addStretch(1);
		aBody.addWidget(actions);
		if (!login.isEmpty()) {
			// The full text stays beneath Connect, so opening it never moves
			// the button out of view.
			auto* const toggle = new disclosure_button{ parent };
			toggle->setObjectName(QStringLiteral("accountSetupConsentDetails"));
			toggle->setText(tr("What connecting allows"));
			toggle->setAccessibleName(toggle->text());
			toggle->setChecked(DetailsOpen);
			aBody.addWidget(toggle);
			auto* const details = muted(github_cli_read_consent_details(), parent);
			details->setObjectName(QStringLiteral("accountSetupConsentDetailsText"));
			details->setVisible(DetailsOpen);
			aBody.addWidget(details);
			connect(toggle, &QToolButton::toggled, this, [this, details](const bool aChecked) {
				DetailsOpen = aChecked;
				details->setVisible(aChecked);
			});
		}
	}

	void account_setup_view::build_confirm(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		const auto  login  = chosen_login();
		if (Projection.State.Way == account_setup_way::browser) {
			aBody.addWidget(check_row(core::setup_check_state::passed, tr("GitHub accepted the code."), parent, QStringLiteral("accountSetupAccepted")));
			aBody.addWidget(check_row(core::setup_check_state::running, tr("Checking which account signed in…"), parent, QStringLiteral("accountSetupStatus")));
		} else if (Projection.State.Way == account_setup_way::token) {
			aBody.addWidget(check_row(core::setup_check_state::running, tr("Checking the token with %1…").arg(text_of(Projection.State.Hostname)), parent,
									  QStringLiteral("accountSetupStatus")));
		} else {
			aBody.addWidget(check_row(core::setup_check_state::running, tr("Checking %1 with GitHub…").arg(login), parent, QStringLiteral("accountSetupStatus")));
		}
	}

	void account_setup_view::build_ready(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		const auto& state  = Projection.State;
		aBody.addWidget(check_row(core::setup_check_state::passed, tr("Connected as %1 on %2.").arg(text_of(state.Login), text_of(state.Hostname)), parent,
								  QStringLiteral("accountSetupConnected")));
		aBody.addWidget(muted(tr("Paste a pull request link in the bar above, or choose one from Home. Accounts and hosts in Settings manages this connection."), parent));
		auto [actions, actionRow] = action_row(parent);
		auto* const start         = primary_button(tr("Start reviewing"), actions, QStringLiteral("accountSetupFinish"));
		connect(start, &QPushButton::clicked, this, [this] { dispatch(core::finish_account_setup{ }); });
		actionRow->addWidget(start);
		actionRow->addStretch(1);
		aBody.addWidget(actions);
	}
} // namespace bpr::qt
