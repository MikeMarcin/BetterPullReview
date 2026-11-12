/// \file
/// \brief RFC-BPR-0032 amendment r1. Home's account setup, offscreen: the
/// view wired to the real setup host and core, with a stubbed gh and device
/// flow. Walks both ways in through every step, including Back (cancel),
/// failures and retry, and proves every press is an intent, no window is
/// opened, and the code and consent are shown in the view. With
/// `BPR_ACCOUNT_SETUP_RENDER_DIR` set, each state is written there as a
/// light and a dark PNG.

#include "bpr/application/account_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/account_setup_view.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
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
#include <vector>

namespace {
	using namespace bpr;
	using core::account_setup_phase;
	using core::account_setup_problem;
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

	/// A gh and a device flow that only record what they are asked.
	struct stub_effects final : application::account_setup_effect_port
	{
		std::vector<std::uint64_t>         Inspections;
		std::vector<core::host_profile_id> Connections;
		int                                Begins  = 0;
		int                                Cancels = 0;
		int                                Opens   = 0;

		void inspect_github_cli(const std::uint64_t aRequest) override { Inspections.push_back(aRequest); }
		void begin_browser_sign_in() override { ++Begins; }
		void cancel_browser_sign_in() override { ++Cancels; }
		void connect_github_cli(const core::host_profile_id aProfile) override { Connections.push_back(aProfile); }
		void open_verification_address() override { ++Opens; }
	};

	struct harness
	{
		stub_effects                    Effects;
		application::manual_executor    Lane;
		qt::account_setup_view          View;
		application::account_setup_host Host;
		int                             Enterprise = 0;

		explicit harness(const bool aCli = true)
			: Host{ Effects, Lane, [this](const application::account_setup_projection& aProjection) { View.present(aProjection); }, aCli }
		{
			View.set_intents({ .Dispatch                = [this](core::account_setup_event aEvent) { Host.dispatch(std::move(aEvent)); },
							   .OpenVerificationAddress = [this] { Host.open_verification_address(); },
							   .AddEnterpriseServer     = [this] { ++Enterprise; } });
			View.resize(720, 640);
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
			aWidget.resize(720, 640);
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

	[[nodiscard]] auto account(const std::uint64_t aProfile, std::string aLogin, const bool aActive) -> core::account_setup_account
	{
		auto result       = core::account_setup_account{ };
		result.Profile    = core::host_profile_id{ aProfile };
		result.Login      = std::move(aLogin);
		result.Hostname   = "github.com";
		result.ActiveInGh = aActive;
		return result;
	}

	[[nodiscard]] auto session(const core::account_setup_session aOutcome, std::string aLogin = { }) -> core::account_setup_session_observed
	{
		auto result     = core::account_setup_session_observed{ };
		result.Outcome  = aOutcome;
		result.Login    = std::move(aLogin);
		result.Hostname = "github.com";
		return result;
	}

	void answer_gh(harness& aSetup, const core::github_cli_inspection aOutcome, std::vector<core::account_setup_account> aAccounts)
	{
		auto answer     = core::github_cli_inspected{ };
		answer.Request  = aSetup.Effects.Inspections.back();
		answer.Outcome  = aOutcome;
		answer.Accounts = std::move(aAccounts);
		aSetup.Host.complete_github_cli(std::move(answer));
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

	void test_browser_progression()
	{
		auto       setup   = harness{ };
		const auto windows = top_level_windows();
		setup.Host.dispatch(core::begin_account_setup{ });
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("How to connect")) && shows(setup.View, QStringLiteral("Sign in with your browser"))
				   && shows(setup.View, QStringLiteral("Use the GitHub CLI")) && shows(setup.View, QStringLiteral("Confirm the account"))
				   && shows(setup.View, QStringLiteral("Ready")),
			   "all four steps are in view from the start, with both ways");
		expect(setup.View.picked_way() == account_setup_way::browser, "the browser way is picked first");
		render(setup.View, QStringLiteral("01-way"));

		press(setup.View, QStringLiteral("accountSetupContinue"));
		expect(setup.Effects.Begins == 1 && setup.Host.projection().State.Phase == account_setup_phase::requesting_code, "Continue asks the host for a code");
		expect(shows(setup.View, QStringLiteral("Getting a sign-in code")), "asking for a code says so");
		render(setup.View, QStringLiteral("02-browser-requesting"));

		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "WDJB-MJHT", 893U));
		QApplication::processEvents();
		auto* const code = find<QLabel>(setup.View, QStringLiteral("accountSetupCode"));
		expect(code != nullptr && code->text() == QStringLiteral("WDJB-MJHT"), "the code is shown in the setup itself");
		expect(shows(setup.View, QStringLiteral("github.com/login/device")) && shows(setup.View, QStringLiteral("14:53")), "the address and the time left are said");
		expect(shows(setup.View, QStringLiteral("Waiting for you to approve on GitHub")), "the setup says it is waiting");
		expect(shows(setup.View, QStringLiteral("Browser sign-in")), "the way step folds to its choice");
		render(setup.View, QStringLiteral("03-browser-code"));
		expect(setup.View.status_text().contains(QStringLiteral("WDJB-MJHT")) && setup.View.announcements() > 0U, "the code is announced politely");
		const auto said = setup.View.announcements();
		setup.View.present(setup.Host.projection());
		QApplication::processEvents();
		expect(setup.View.announcements() == said, "an unchanged status is not announced again");
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupCode")) != nullptr, "presenting the same projection again keeps the code in place");
		setup.View.setPalette(dark_palette());
		QApplication::processEvents();
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupCode")) != nullptr, "a palette change rebuilds the step with the code still shown");
		setup.View.setPalette(QPalette{ });
		QApplication::processEvents();

		press(setup.View, QStringLiteral("accountSetupCopyCode"));
		expect(QApplication::clipboard()->text() == QStringLiteral("WDJB-MJHT"), "Copy code copies only the code");
		press(setup.View, QStringLiteral("accountSetupOpenGithub"));
		expect(setup.Effects.Opens == 1, "Open GitHub is a request to the host");

		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "WDJB-MJHT", 880U, application::device_authorization_handoff::failed));
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("The browser didn't open")), "a browser that did not open is said beside the code");

		// Cancel: Back ends the code and returns to the way choice.
		press(setup.View, QStringLiteral("accountSetupBack"));
		expect(setup.Effects.Cancels == 1 && setup.Host.projection().State.Phase == account_setup_phase::choosing_way, "Cancel ends the code");
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupCode")) == nullptr, "no code remains after cancel");

		// Failure, then retry.
		press(setup.View, QStringLiteral("accountSetupContinue"));
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "ABCD-EFGH", 899U));
		setup.Host.observe_device(device(core::device_authorization_stage::expired));
		QApplication::processEvents();
		expect(shows(setup.View, qt::account_setup_view::problem_words(account_setup_problem::code_expired)), "an expired code says what happened and what to do");
		render(setup.View, QStringLiteral("04-browser-expired"));
		press(setup.View, QStringLiteral("accountSetupRetry"));
		expect(setup.Effects.Begins == 3 && setup.Host.projection().State.Phase == account_setup_phase::requesting_code, "Get a new code asks again");

		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "IJKL-MNOP", 899U));
		setup.Host.observe_device(device(core::device_authorization_stage::authorized));
		QApplication::processEvents();
		expect(setup.Host.projection().Step == account_setup_step::confirm && shows(setup.View, QStringLiteral("GitHub accepted the code")),
			   "an authorized code moves to confirming the account");
		expect(find<QPushButton>(setup.View, QStringLiteral("accountSetupChange")) == nullptr, "the way cannot be changed while the account is checked");
		render(setup.View, QStringLiteral("05-browser-confirm"));

		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-reviewer"));
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("Connected as octo-reviewer on github.com.")), "the ready step names the account");
		render(setup.View, QStringLiteral("06-ready"));
		press(setup.View, QStringLiteral("accountSetupFinish"));
		expect(!setup.Host.active(), "Start reviewing leaves the setup");
		expect(top_level_windows() == windows, "no window was opened at any step");
	}

	void test_github_cli_progression()
	{
		auto setup = harness{ };
		setup.Host.dispatch(core::begin_account_setup{ });
		QApplication::processEvents();
		auto* const cliCard = find<QWidget>(setup.View, QStringLiteral("accountSetupWayCli"));
		expect(cliCard != nullptr, "the CLI card is shown");
		click(*cliCard);
		expect(setup.View.picked_way() == account_setup_way::github_cli && setup.Effects.Inspections.empty(), "picking a card starts nothing");
		expect(find<QPushButton>(setup.View, QStringLiteral("accountSetupContinue"))->text() == QStringLiteral("Check the GitHub CLI"), "Continue says what it will do");
		press(setup.View, QStringLiteral("accountSetupContinue"));
		expect(setup.Effects.Inspections.size() == 1U && shows(setup.View, QStringLiteral("Looking for the GitHub CLI")), "Continue checks gh");
		render(setup.View, QStringLiteral("07-cli-checking"));

		// gh missing, then signed out with its command, then two accounts.
		answer_gh(setup, core::github_cli_inspection::missing, { });
		expect(shows(setup.View, QStringLiteral("The GitHub CLI isn't installed")), "a missing gh says how to install it");
		render(setup.View, QStringLiteral("08-cli-missing"));
		press(setup.View, QStringLiteral("accountSetupRetry"));
		answer_gh(setup, core::github_cli_inspection::outdated, { });
		expect(shows(setup.View, QStringLiteral("too old")), "an old gh says to upgrade");
		press(setup.View, QStringLiteral("accountSetupRetry"));
		answer_gh(setup, core::github_cli_inspection::read, { });
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupCommand")) != nullptr, "a signed-out gh shows the command to run");
		render(setup.View, QStringLiteral("09-cli-signed-out"));
		press(setup.View, QStringLiteral("accountSetupCopyCommand"));
		expect(QApplication::clipboard()->text() == QStringLiteral("gh auth login"), "the command can be copied");
		press(setup.View, QStringLiteral("accountSetupRetry"));
		answer_gh(setup, core::github_cli_inspection::read, { account(7U, "octo-reviewer", true), account(8U, "octo-test", false) });

		auto* const connectButton = find<QPushButton>(setup.View, QStringLiteral("accountSetupConnect"));
		expect(connectButton != nullptr && !connectButton->isEnabled(), "with several accounts, Connect waits for a choice");
		expect(shows(setup.View, QStringLiteral("github.com, active in gh")), "gh's active account is said, not chosen");
		render(setup.View, QStringLiteral("10-cli-accounts"));
		const auto cards = setup.View.findChildren<QWidget*>(QStringLiteral("accountSetupAccount"));
		expect(cards.size() == 2, "both accounts are cards");
		click(*cards.at(1));
		expect(setup.Host.projection().State.Chosen == core::host_profile_id{ 8U }, "clicking a card chooses that account");
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupConsent")) != nullptr && shows(setup.View, QStringLiteral("octo-test's existing gh sign-in")),
			   "the consent is said in the setup, for the chosen account");
		auto* const details = find<QToolButton>(setup.View, QStringLiteral("accountSetupConsentDetails"));
		expect(details != nullptr, "the full consent text is behind a disclosure");
		details->click();
		QApplication::processEvents();
		expect(find<QLabel>(setup.View, QStringLiteral("accountSetupConsentDetailsText")) != nullptr, "the disclosure opens in place");
		render(setup.View, QStringLiteral("11-cli-consent"));

		press(setup.View, QStringLiteral("accountSetupConnect"));
		expect(setup.Effects.Connections == std::vector{ core::host_profile_id{ 8U } }, "Connect as octo-test connects that account");
		expect(shows(setup.View, QStringLiteral("Checking octo-test with GitHub")), "the confirm step names the account");
		render(setup.View, QStringLiteral("12-cli-confirm"));

		setup.Host.observe_session(session(core::account_setup_session::failed));
		QApplication::processEvents();
		expect(shows(setup.View, qt::account_setup_view::problem_words(account_setup_problem::connection_failed)) && setup.Host.projection().State.Chosen == core::host_profile_id{ 8U },
			   "a refused account is said under the list, with the choice kept");
		render(setup.View, QStringLiteral("13-cli-refused"));
		press(setup.View, QStringLiteral("accountSetupConnect"));
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-test"));
		QApplication::processEvents();
		expect(shows(setup.View, QStringLiteral("Connected as octo-test")), "the connected gh account is ready");
		render(setup.View, QStringLiteral("14-cli-ready"));
	}

	void test_change_and_enterprise_and_no_cli()
	{
		auto setup = harness{ };
		setup.Host.dispatch(core::begin_account_setup{ .Way = account_setup_way::github_cli });
		answer_gh(setup, core::github_cli_inspection::read, { account(7U, "octo-reviewer", true) });
		expect(setup.Host.projection().State.Chosen == core::host_profile_id{ 7U } && find<QPushButton>(setup.View, QStringLiteral("accountSetupConnect"))->isEnabled(),
			   "one account is chosen and Connect is offered, not pressed");
		expect(setup.Effects.Connections.empty(), "nothing connects without Connect");
		press(setup.View, QStringLiteral("accountSetupChange"));
		expect(setup.Host.projection().State.Phase == account_setup_phase::choosing_way, "Change returns to the way choice");
		press(setup.View, QStringLiteral("accountSetupEnterprise"));
		expect(setup.Enterprise == 1, "the enterprise link asks the window");

		auto noCli = harness{ false };
		noCli.Host.dispatch(core::begin_account_setup{ });
		QApplication::processEvents();
		auto* const card = find<QWidget>(noCli.View, QStringLiteral("accountSetupWayCli"));
		click(*card);
		expect(noCli.View.picked_way() == account_setup_way::browser, "an unavailable way cannot be picked");
		expect(shows(noCli.View, QStringLiteral("Not available in this build")), "and says why");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
	test_browser_progression();
	test_github_cli_progression();
	test_change_and_enterprise_and_no_cli();
	std::cout << "account-setup-view-tests:pass\n";
	return EXIT_SUCCESS;
}
