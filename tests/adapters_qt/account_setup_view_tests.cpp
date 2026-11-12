/// \file
/// \brief RFC-BPR-0032 amendment r1. The account setup, offscreen: the view
/// wired to the real setup host and core, with a stubbed gh, device flow and
/// Enterprise server. Walks the server step, both GitHub.com ways, an
/// Enterprise server with a token, and Settings' Add account while signed
/// in, including Back (cancel), failures and retry; proves every press is an
/// intent and no window opens. With `BPR_ACCOUNT_SETUP_RENDER_DIR` set, each
/// state is written there as a light and a dark PNG.

#include "bpr/application/account_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/account_setup_view.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QStyleFactory>
#include <QToolButton>
#include <QWidget>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using core::account_setup_phase;
	using core::account_setup_problem;
	using core::account_setup_server;
	using core::account_setup_step;
	using core::account_setup_way;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto dark_palette() -> QPalette
	{
		auto palette = QPalette{ };
		palette.setColor(QPalette::Window, QColor{ 0x1F, 0x23, 0x28 });
		palette.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Base, QColor{ 0x16, 0x1B, 0x22 });
		palette.setColor(QPalette::Text, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Button, QColor{ 0x2D, 0x33, 0x3B });
		palette.setColor(QPalette::ButtonText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Highlight, QColor{ 0x2F, 0x81, 0xF7 });
		palette.setColor(QPalette::HighlightedText, QColor{ 0xFF, 0xFF, 0xFF });
		palette.setColor(QPalette::PlaceholderText, QColor{ 0x8B, 0x94, 0x9E });
		palette.setColor(QPalette::Link, QColor{ 0x58, 0xA6, 0xFF });
		return palette;
	}

	/// A gh, a device flow and a server list that only record what they are asked.
	struct stub_effects final : application::account_setup_effect_port
	{
		std::vector<std::pair<std::uint64_t, std::string>> Inspections;
		std::vector<std::pair<std::uint64_t, std::string>> Servers;
		std::vector<core::host_profile_id>                 Connections;
		std::vector<core::host_profile_id>                 Tokens;
		int                                                Begins  = 0;
		int                                                Cancels = 0;
		int                                                Opens   = 0;

		void prepare_enterprise_server(const std::uint64_t aRequest, const std::string& aHostname) override { Servers.emplace_back(aRequest, aHostname); }
		void inspect_github_cli(const std::uint64_t aRequest, const std::string& aHostname) override { Inspections.emplace_back(aRequest, aHostname); }
		void begin_browser_sign_in() override { ++Begins; }
		void cancel_browser_sign_in() override { ++Cancels; }
		void connect_github_cli(const core::host_profile_id aProfile) override { Connections.push_back(aProfile); }
		void enroll_token(const core::host_profile_id aProfile) override { Tokens.push_back(aProfile); }
		void open_verification_address() override { ++Opens; }
	};

	struct harness
	{
		stub_effects                    Effects;
		application::manual_executor    Lane;
		qt::account_setup_view          View;
		application::account_setup_host Host;
		std::vector<std::size_t>        TokenSizes;

		explicit harness(const bool aCli = true)
			: Host{ Effects, Lane, [this](const application::account_setup_projection& aProjection) { View.present(aProjection); }, aCli }
		{
			auto intents                    = qt::account_setup_intents{ };
			intents.Dispatch                = [this](core::account_setup_event aEvent) { Host.dispatch(std::move(aEvent)); };
			intents.OpenVerificationAddress = [this] { Host.open_verification_address(); };
			intents.SubmitToken             = [this](qt::sensitive_bytes aBytes) { TokenSizes.push_back(aBytes.size()); };
			intents.MaximumTokenBytes       = 1024U;
			View.set_intents(std::move(intents));
			View.resize(720, 700);
			View.show();
		}
	};

	void render(QWidget& aWidget, const QString& aName)
	{
		const auto folder = qEnvironmentVariable("BPR_ACCOUNT_SETUP_RENDER_DIR");
		if (folder.isEmpty()) {
			return;
		}
		QDir{ }.mkpath(folder);
		const auto light = aWidget.palette();
		for (const auto& [suffix, palette] : { std::pair{ QStringLiteral("light"), light }, std::pair{ QStringLiteral("dark"), dark_palette() } }) {
			aWidget.setPalette(palette);
			aWidget.resize(720, 700);
			QApplication::processEvents();
			static_cast<void>(aWidget.grab().save(folder + QStringLiteral("/") + aName + QStringLiteral("-") + suffix + QStringLiteral(".png")));
		}
		aWidget.setPalette(light);
		QApplication::processEvents();
	}

	template <typename TWidget>
	[[nodiscard]] auto find(const QWidget& aView, const QString& aName) -> TWidget*
	{
		for (auto* const candidate : aView.findChildren<TWidget*>(aName)) {
			if (!candidate->isHidden() && candidate->isVisibleTo(&aView)) {
				return candidate;
			}
		}
		return nullptr;
	}

	void press(const QWidget& aView, const QString& aName)
	{
		QApplication::processEvents();
		auto* const button = find<QPushButton>(aView, aName);
		expect(button != nullptr && button->isEnabled(), "the named button is shown and enabled");
		button->click();
		QApplication::processEvents();
	}

	void click(QWidget& aWidget)
	{
		const auto centre  = QPointF{ aWidget.rect().center() };
		auto       pressed = QMouseEvent{ QEvent::MouseButtonPress, centre, aWidget.mapToGlobal(centre), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, centre, aWidget.mapToGlobal(centre), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &pressed);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}

	[[nodiscard]] auto shows(const qt::account_setup_view& aView, const QString& aText) -> bool { return aView.step_text().contains(aText); }

	[[nodiscard]] auto device(const core::device_authorization_stage aStage, const std::string& aCode = { }, const std::uint32_t aSeconds = 0U,
							  const application::device_authorization_handoff aHandoff = application::device_authorization_handoff::not_attempted)
		-> application::device_authorization_presentation
	{
		auto result                     = application::device_authorization_presentation{ };
		result.Stage                    = aStage;
		result.UserCode                 = aCode;
		result.VerificationUri          = aCode.empty() ? std::string{ } : std::string{ "github.com/login/device" };
		result.RemainingValiditySeconds = aSeconds;
		result.Handoff                  = aHandoff;
		return result;
	}

	[[nodiscard]] auto account(const std::uint64_t aProfile, std::string aLogin, const bool aActive, std::string aHost = "github.com") -> core::account_setup_account
	{
		auto result       = core::account_setup_account{ };
		result.Profile    = core::host_profile_id{ aProfile };
		result.Login      = std::move(aLogin);
		result.Hostname   = std::move(aHost);
		result.ActiveInGh = aActive;
		return result;
	}

	[[nodiscard]] auto session(const core::account_setup_session aOutcome, std::string aLogin = { }, std::string aHost = "github.com") -> core::account_setup_session_observed
	{
		auto result     = core::account_setup_session_observed{ };
		result.Outcome  = aOutcome;
		result.Login    = std::move(aLogin);
		result.Hostname = std::move(aHost);
		return result;
	}

	void answer_gh(harness& aSetup, const core::github_cli_inspection aOutcome, std::vector<core::account_setup_account> aAccounts)
	{
		auto answer     = core::github_cli_inspected{ };
		answer.Request  = aSetup.Effects.Inspections.back().first;
		answer.Outcome  = aOutcome;
		answer.Accounts = std::move(aAccounts);
		aSetup.Host.complete_github_cli(std::move(answer));
		aSetup.Lane.drain();
		QApplication::processEvents();
	}

	void answer_server(harness& aSetup, const std::optional<std::uint64_t> aProfile)
	{
		auto answer    = core::enterprise_server_prepared{ };
		answer.Request = aSetup.Effects.Servers.back().first;
		if (aProfile) {
			answer.Profile = core::host_profile_id{ *aProfile };
		}
		aSetup.Host.complete_enterprise_server(answer);
		aSetup.Lane.drain();
		QApplication::processEvents();
	}

	[[nodiscard]] auto top_level_windows() -> int
	{
		auto count = 0;
		for (const auto* const widget : QApplication::topLevelWidgets()) {
			count += widget->isVisible() ? 1 : 0;
		}
		return count;
	}

	[[nodiscard]] auto begin() -> core::begin_account_setup { return core::begin_account_setup{ }; }

	void test_server_then_browser()
	{
		auto       setup   = harness{ };
		const auto windows = top_level_windows();
		setup.Host.dispatch(begin());
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("Server")) && shows(setup.View, QStringLiteral("GitHub.com")) && shows(setup.View, QStringLiteral("GitHub Enterprise Server"))
				   && shows(setup.View, QStringLiteral("How to connect")) && shows(setup.View, QStringLiteral("Confirm the account")) && shows(setup.View, QStringLiteral("Ready")),
			   "the server is the first of five steps, all in view");
		expect(find<QPushButton>(setup.View, QStringLiteral("accountSetupClose")) == nullptr, "a signed-out window's setup offers no Close");
		render(setup.View, QStringLiteral("01-server"));

		press(setup.View, QStringLiteral("accountSetupServerContinue"));
		expect(setup.Host.projection().State.Phase == account_setup_phase::choosing_way && setup.Effects.Servers.empty(), "GitHub.com goes straight to its ways");
		expect(find<QWidget>(setup.View, QStringLiteral("accountSetupWayBrowser")) != nullptr && find<QWidget>(setup.View, QStringLiteral("accountSetupWayCli")) != nullptr
				   && find<QWidget>(setup.View, QStringLiteral("accountSetupWayToken")) == nullptr,
			   "GitHub.com offers the browser and gh, not a token");
		render(setup.View, QStringLiteral("02-way-github"));

		press(setup.View, QStringLiteral("accountSetupContinue"));
		expect(setup.Effects.Begins == 1 && setup.Host.projection().State.Phase == account_setup_phase::requesting_code, "Get a sign-in code asks the host");
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "WDJB-MJHT", 893U));
		QApplication::processEvents();
		auto* const code = find<QLabel>(setup.View, QStringLiteral("accountSetupCode"));
		expect(code != nullptr && code->text() == QStringLiteral("WDJB-MJHT"), "the code is shown in the setup");
		expect(shows(setup.View, QStringLiteral("github.com/login/device")) && shows(setup.View, QStringLiteral("14:53")), "the address and time left are said");
		expect(setup.View.status_text().contains(QStringLiteral("WDJB-MJHT")) && setup.View.announcements() > 0U, "the code is announced politely");
		const auto said = setup.View.announcements();
		setup.View.present(setup.Host.projection());
		QApplication::processEvents();
		expect(setup.View.announcements() == said, "an unchanged status is not announced again");
		render(setup.View, QStringLiteral("03-browser-code"));
		press(setup.View, QStringLiteral("accountSetupCopyCode"));
		expect(QApplication::clipboard()->text() == QStringLiteral("WDJB-MJHT"), "Copy code copies only the code");
		press(setup.View, QStringLiteral("accountSetupOpenGithub"));
		expect(setup.Effects.Opens == 1, "Open GitHub is a request to the host");

		press(setup.View, QStringLiteral("accountSetupBack"));
		expect(setup.Effects.Cancels == 1 && setup.Host.projection().State.Phase == account_setup_phase::choosing_way, "Cancel ends the code");
		press(setup.View, QStringLiteral("accountSetupChangeServer"));
		expect(setup.Host.projection().State.Phase == account_setup_phase::choosing_server, "Change on the server step goes back to it");
		press(setup.View, QStringLiteral("accountSetupServerContinue"));
		press(setup.View, QStringLiteral("accountSetupContinue"));
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "ABCD-EFGH", 899U));
		setup.Host.observe_device(device(core::device_authorization_stage::expired));
		QApplication::processEvents();
		expect(shows(setup.View, qt::account_setup_view::problem_words(account_setup_problem::code_expired)), "an expired code says what to do");
		render(setup.View, QStringLiteral("04-browser-expired"));
		press(setup.View, QStringLiteral("accountSetupRetry"));
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "IJKL-MNOP", 899U));
		setup.Host.observe_device(device(core::device_authorization_stage::authorized));
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("GitHub accepted the code")) && find<QPushButton>(setup.View, QStringLiteral("accountSetupChange")) == nullptr,
			   "an authorized code confirms the account, and the way can no longer change");
		render(setup.View, QStringLiteral("05-browser-confirm"));
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-reviewer"));
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("Connected as octo-reviewer on github.com.")), "Ready names the account");
		render(setup.View, QStringLiteral("06-ready"));
		press(setup.View, QStringLiteral("accountSetupFinish"));
		expect(!setup.Host.active() && top_level_windows() == windows, "Start reviewing leaves the setup, and no window opened");
	}

	void test_github_cli()
	{
		auto setup = harness{ };
		setup.Host.dispatch(begin());
		press(setup.View, QStringLiteral("accountSetupServerContinue"));
		click(*find<QWidget>(setup.View, QStringLiteral("accountSetupWayCli")));
		expect(setup.View.picked_way() == account_setup_way::github_cli && setup.Effects.Inspections.empty(), "picking a card starts nothing");
		press(setup.View, QStringLiteral("accountSetupContinue"));
		expect(setup.Effects.Inspections.size() == 1U && setup.Effects.Inspections.back().second == "github.com", "Check the GitHub CLI asks for github.com's accounts");
		render(setup.View, QStringLiteral("07-cli-checking"));
		answer_gh(setup, core::github_cli_inspection::missing, { });
		expect(shows(setup.View, QStringLiteral("The GitHub CLI isn't installed")), "a missing gh says how to install it");
		render(setup.View, QStringLiteral("08-cli-missing"));
		press(setup.View, QStringLiteral("accountSetupRetry"));
		answer_gh(setup, core::github_cli_inspection::read, { });
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupCommand")) != nullptr, "a signed-out gh shows the command to run");
		render(setup.View, QStringLiteral("09-cli-signed-out"));
		press(setup.View, QStringLiteral("accountSetupCopyCommand"));
		expect(QApplication::clipboard()->text() == QStringLiteral("gh auth login"), "the command can be copied");
		press(setup.View, QStringLiteral("accountSetupRetry"));
		answer_gh(setup, core::github_cli_inspection::read, { account(7U, "octo-reviewer", true), account(8U, "octo-test", false) });
		expect(setup.Host.projection().State.Chosen == core::host_profile_id{ 7U } && shows(setup.View, QStringLiteral("github.com, active in gh")),
			   "gh's active account is chosen first and says so (owner, 2026-11-12)");
		expect(find<QPushButton>(setup.View, QStringLiteral("accountSetupConnect"))->text() == QStringLiteral("Connect as octo-reviewer"), "Connect names it");
		render(setup.View, QStringLiteral("10-cli-accounts"));
		const auto cards = setup.View.findChildren<QWidget*>(QStringLiteral("accountSetupAccount"));
		expect(cards.size() == 2, "both accounts are cards");
		click(*cards.at(1));
		expect(setup.Host.projection().State.Chosen == core::host_profile_id{ 8U } && shows(setup.View, QStringLiteral("octo-test's existing gh sign-in")),
			   "another may be chosen, and the consent follows it");
		auto* const details = find<QToolButton>(setup.View, QStringLiteral("accountSetupConsentDetails"));
		expect(details != nullptr, "the full consent is behind a disclosure");
		details->click();
		QApplication::processEvents();
		render(setup.View, QStringLiteral("11-cli-consent"));
		press(setup.View, QStringLiteral("accountSetupConnect"));
		expect(setup.Effects.Connections == std::vector{ core::host_profile_id{ 8U } } && shows(setup.View, QStringLiteral("Checking octo-test with GitHub")),
			   "Connect as octo-test connects it and confirms");
		setup.Host.observe_session(session(core::account_setup_session::failed));
		QApplication::processEvents();
		expect(shows(setup.View, qt::account_setup_view::problem_words(account_setup_problem::connection_failed)), "a refused account is said under the list");
		render(setup.View, QStringLiteral("12-cli-refused"));
	}

	void test_enterprise_token()
	{
		auto setup = harness{ };
		setup.Host.dispatch(begin());
		QApplication::processEvents();
		click(*find<QWidget>(setup.View, QStringLiteral("accountSetupServerEnterprise")));
		auto* host = find<QLineEdit>(setup.View, QStringLiteral("accountSetupHostname"));
		expect(host != nullptr, "picking Enterprise asks for its hostname in place");
		host->setText(QStringLiteral("https://ghe.example.com"));
		emit host->textEdited(host->text());
		press(setup.View, QStringLiteral("accountSetupServerContinue"));
		expect(shows(setup.View, qt::account_setup_view::problem_words(account_setup_problem::invalid_hostname)) && setup.Effects.Servers.empty(),
			   "a hostname with a scheme is refused in place");
		host = find<QLineEdit>(setup.View, QStringLiteral("accountSetupHostname"));
		expect(host != nullptr && host->text() == QStringLiteral("https://ghe.example.com"), "what was typed stays to be corrected");
		host->setText(QStringLiteral("GHE.example.com"));
		emit host->textEdited(host->text());
		QApplication::processEvents();
		expect(find<QWidget>(setup.View, QStringLiteral("accountSetupProblem")) == nullptr, "editing the host clears the refusal");
		render(setup.View, QStringLiteral("13-server-enterprise"));
		press(setup.View, QStringLiteral("accountSetupServerContinue"));
		expect(setup.Effects.Servers.size() == 1U && setup.Effects.Servers.back().second == "ghe.example.com", "the server is added under its lowercase host");
		expect(shows(setup.View, QStringLiteral("Adding ghe.example.com")), "adding it is said");
		answer_server(setup, std::nullopt);
		expect(shows(setup.View, qt::account_setup_view::problem_words(account_setup_problem::server_unavailable)), "a server that could not be added is said");
		press(setup.View, QStringLiteral("accountSetupServerContinue"));
		answer_server(setup, 21U);
		expect(setup.Host.projection().State.Phase == account_setup_phase::choosing_way && find<QWidget>(setup.View, QStringLiteral("accountSetupWayBrowser")) == nullptr
				   && find<QWidget>(setup.View, QStringLiteral("accountSetupWayToken")) != nullptr && find<QWidget>(setup.View, QStringLiteral("accountSetupWayCli")) != nullptr,
			   "Enterprise offers a token and gh, not the browser");
		expect(setup.View.picked_way() == account_setup_way::token, "the token is picked first for Enterprise");
		render(setup.View, QStringLiteral("14-way-enterprise"));
		press(setup.View, QStringLiteral("accountSetupContinue"));
		auto* const token = find<QLineEdit>(setup.View, QStringLiteral("accountSetupToken"));
		expect(token != nullptr && token->echoMode() == QLineEdit::Password, "the token is entered in a password field in the setup");
		render(setup.View, QStringLiteral("15-token"));
		press(setup.View, QStringLiteral("accountSetupSubmitToken"));
		expect(setup.TokenSizes.empty() && setup.Effects.Tokens.empty() && shows(setup.View, QStringLiteral("Paste a token first")), "an empty token is refused in place");
		find<QLineEdit>(setup.View, QStringLiteral("accountSetupToken"))->setText(QStringLiteral("ghp_synthetic_fixture_token"));
		press(setup.View, QStringLiteral("accountSetupSubmitToken"));
		expect(setup.TokenSizes == std::vector<std::size_t>{ 27U } && setup.Effects.Tokens == std::vector{ core::host_profile_id{ 21U } },
			   "Sign in hands the token over as bytes, then enrolls it for the server's profile");
		expect(shows(setup.View, QStringLiteral("Checking the token with ghe.example.com")), "confirming says which server");
		setup.Host.observe_session(session(core::account_setup_session::failed));
		QApplication::processEvents();
		auto* const again = find<QLineEdit>(setup.View, QStringLiteral("accountSetupToken"));
		expect(again != nullptr && again->text().isEmpty() && shows(setup.View, qt::account_setup_view::problem_words(account_setup_problem::connection_failed)),
			   "a refused token returns to an empty field with the reason");
		render(setup.View, QStringLiteral("16-token-refused"));
		find<QLineEdit>(setup.View, QStringLiteral("accountSetupToken"))->setText(QStringLiteral("ghp_synthetic_fixture_token"));
		press(setup.View, QStringLiteral("accountSetupSubmitToken"));
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-ghe", "ghe.example.com"));
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("Connected as octo-ghe on ghe.example.com.")), "the Enterprise account is ready");
	}

	void test_settings_add_account()
	{
		// Settings' Add account, with an account already: the same setup,
		// which can be closed without adding one.
		auto setup = harness{ };
		setup.Host.observe_signed_in(true);
		setup.Host.dispatch(begin());
		QApplication::processEvents();
		expect(find<QPushButton>(setup.View, QStringLiteral("accountSetupClose")) != nullptr, "a signed-in window's setup offers Close");
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupTitle"))->text() == QStringLiteral("Add a GitHub account"), "with an account already, the setup adds one");
		render(setup.View, QStringLiteral("17-settings-add-account"));
		press(setup.View, QStringLiteral("accountSetupClose"));
		expect(!setup.Host.active(), "Close leaves the setup with nothing changed");

		auto noCli = harness{ false };
		noCli.Host.dispatch(begin());
		press(noCli.View, QStringLiteral("accountSetupServerContinue"));
		click(*find<QWidget>(noCli.View, QStringLiteral("accountSetupWayCli")));
		expect(noCli.View.picked_way() == account_setup_way::browser && shows(noCli.View, QStringLiteral("Not available in this build")),
			   "an unavailable way cannot be picked, and says why");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
	test_server_then_browser();
	test_github_cli();
	test_enterprise_token();
	test_settings_add_account();
	std::cout << "account-setup-view-tests:pass\n";
	return EXIT_SUCCESS;
}
