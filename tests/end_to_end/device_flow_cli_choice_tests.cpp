// [[BPR-0803]]. Choosing GitHub CLI while a device sign-in waits for its code
// keeps the code on screen beside the GitHub CLI choice until the reviewer
// ends one of them (owner decision dbae079d, 2026-09-24). Three complete
// compositions over the recorded GitHub.com fixture, one per ending:
//
// - one discovered account, which Use GitHub CLI connects at once behind its
//   read opt-in, with the code still in view behind the opt-in;
// - two discovered accounts, the choice standing on the card beside the code,
//   and the GitHub CLI way in taken: its read opt-in is answered once, the
//   session is the discovered GitHub CLI profile's, and the device flow is
//   ended visibly rather than left polling;
// - two discovered accounts and the device authorization finishing first:
//   sign-in closes, which withdraws the GitHub CLI choice, no read opt-in is
//   asked, and the session is the native profile's.
//
// The device fixture answers `authorization_pending` until the course lets
// the browser finish. No live GitHub, credential or device-flow request.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;

	using bpr::tests::expect;
	void wait_for(const std::function<bool()>& aCondition, const std::string_view aMessage, const qint64 aTimeoutMs = 15000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	void settle(const qint64 aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	enum class ending : std::uint8_t {
		/// One discovered account: Use GitHub CLI connects it at once, behind
		/// its read opt-in (BPR-0909), and that way in finishes first.
		cli_connects_at_once,
		/// Two discovered accounts: the choice stands on the card beside the
		/// code, and the reviewer takes the GitHub CLI way in.
		cli_chosen_first,
		/// Two discovered accounts, and the browser finishes the device flow
		/// while the choice is on screen.
		device_first,
	};

	// Rendered evidence, written only when a directory is named: the shared
	// sign-in dialog with the code and the GitHub CLI choice side by side.
	void capture(QWidget& aSurface, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_DEVICE_CLI_CHOICE_CAPTURE_DIR");
		if (directory.isEmpty()) {
			return;
		}
		expect(aSurface.grab().save(QStringLiteral("%1/%2.png").arg(directory, aName), "PNG"), "the capture is written");
	}

	// [[BPR-1571]]. Exercise the actual discovery completion and projection,
	// not only the Qt message setter, with the inaccessible-Keychain shape.
	void inaccessible_cli_inventory_stays_signed_out()
	{
		QTemporaryDir temporary;
		auto          options              = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.RegisteredDeviceCourse     = true;
		options.PublicPreviewEnabledCourse = true;
		options.IndependentWindowReads     = true;
		options.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
			{ .Hostname = "github.com", .Login = "fixture-user", .SignedIn = false, .KeychainHeld = false }
		};
		std::unique_ptr<app::github_application> application;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "ineligible-inventory application starts");
		expect(application != nullptr, "ineligible-inventory composition is available");
		auto& window = application->window();
		window.show();
		window.use_github_cli_button()->click();
		wait_for([&] { return window.github_cli_discovery_status()->text().contains(QStringLiteral("available from secure storage")); },
				 "completed but ineligible inventory reaches its own actionable message");
		expect(window.session_panel()->status() == core::github_status::no_profile,
			   "inaccessible account is not promoted to an authenticated session");
		expect(window.use_github_cli_button()->isEnabled(), "discovery can be retried after environment recovery");
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "ineligible-inventory hosts quiesce");
	}

	void run(const ending aEnding)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "private temporary application root");
		auto options                       = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.RegisteredDeviceCourse     = true;
		options.PublicPreviewEnabledCourse = true;
		options.IndependentWindowReads     = true;
		options.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
			{ .Hostname = "github.com", .Login = "octo-reviewer" }
		};
		if (aEnding != ending::cli_connects_at_once) {
			options.GithubCliDiscoveryAccounts->push_back({ .Hostname = "ghe.example.invalid", .Login = "octo-enterprise" });
		}
		const auto pending                 = std::make_shared<std::atomic_bool>(true);
		options.DeviceAuthorizationPending = pending;
		expect(options.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");

		std::unique_ptr<app::github_application> application;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(application != nullptr, "application startup succeeds");
		auto& window = application->window();
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		qt::server_card* github = nullptr;
		for (auto* const card : window.server_cards()) {
			if (card->offers_device_sign_in()) {
				github = card;
			}
		}
		expect(github != nullptr && github->native_option() != nullptr, "GitHub.com's card offers the device sign-in");
		const auto  native = github->native_option()->Profile;
		auto* const panel  = window.device_authorization();

		github->sign_in_button()->click();
		wait_for([&] { return panel->signing_in() && !panel->user_code_field()->text().isEmpty(); }, "Sign in begins the device flow and shows its code");
		auto* const dialog = window.sign_in_dialog();
		expect(dialog != nullptr && dialog->isVisible() && github->isVisibleTo(dialog), "the code is shown in the shared sign-in dialog");
		const auto code       = panel->user_code_field()->text();
		const auto codeInView = [&] {
			return dialog->isVisible() && github->isVisibleTo(dialog) && panel->isVisibleTo(dialog) && panel->user_code_field()->isVisible()
				&& panel->user_code_field()->text() == code && panel->address_field()->isVisible() && panel->signing_in();
		};

		// The read opt-in answers yes, and each time it is asked is counted,
		// with whether the code was still in view behind it.
		auto   optIns          = 0U;
		auto   codeBehindOptIn = false;
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			// [[BPR-1121]]. The event loop calls this, and a throw may not cross it, so a failure here exits.
			const bpr::tests::exit_on_failure inTheEventLoop;
			if (auto* const prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				++optIns;
				codeBehindOptIn = codeInView();
				capture(*dialog, aEnding == ending::cli_connects_at_once ? QStringLiteral("code-behind-read-opt-in-at-once") : QStringLiteral("code-behind-read-opt-in-chosen"));
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);

		window.use_github_cli_button()->click();
		if (aEnding == ending::cli_connects_at_once) {
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "one discovered account connects at once and signs in");
			expect(optIns == 1U && codeBehindOptIn, "its read opt-in is asked once, with the code and address still in view behind it");
		} else {
			wait_for([&] { return github->cli_option() != nullptr && github->cli_offer_button() != nullptr && github->cli_offer_button()->isVisibleTo(dialog); },
					 "Use GitHub CLI during the flow offers the github.com account on the same card");
			expect(codeInView(), "the code and the address stay in view beside the GitHub CLI choice, and the flow is still running");
			expect(optIns == 0U && window.session_panel()->status() == core::github_status::no_profile,
				   "choosing GitHub CLI reads nothing and signs nothing in by itself");
			capture(*dialog, aEnding == ending::cli_chosen_first ? QStringLiteral("code-beside-cli-choice") : QStringLiteral("code-beside-cli-choice-then-browser"));
		}
		const auto cli = github->cli_option()->Profile;

		if (aEnding == ending::device_first) {
			pending->store(false);
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "the browser finishing the device flow signs in", 20000);
			expect(window.session_panel()->active_profile() == std::optional{ native } && github->signed_in() && github->profile() == native,
				   "GitHub.com ends signed in through its native HTTPS profile");
			wait_for([&] { return !dialog->isVisible(); }, "sign-in closes, withdrawing the GitHub CLI choice");
			settle(500);
			const auto evidence = github_application_fixture_access::evidence(*application);
			expect(optIns == 0U && !panel->signing_in() && evidence.VaultStores == 1U,
				   "the GitHub CLI choice was never taken: no read opt-in, no second flow, and one device grant stored");
			std::cout << "DEVICE_CLI_CHOICE:device-first:profile=" << native.value() << ":login=" << github->login_label()->text().toStdString() << '\n';
		} else {
			if (aEnding == ending::cli_chosen_first) {
				github->cli_offer_button()->click();
				wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "the GitHub CLI way in signs in");
				expect(optIns == 1U && codeBehindOptIn, "its read opt-in is answered once, before any gh read, with the code still in view behind it");
			}
			expect(window.session_panel()->active_profile() == std::optional{ cli } && github->signed_in() && github->profile() == cli,
				   "GitHub.com ends signed in through the discovered GitHub CLI profile");
			wait_for([&] { return !panel->signing_in(); }, "the device flow still waiting beside it is ended");
			wait_for([&] { return !dialog->isVisible(); }, "sign-in closes");
			// Long enough for the device flow to have polled again had it kept
			// running; letting the browser finish now must sign nothing in.
			pending->store(false);
			settle(7000);
			const auto evidence = github_application_fixture_access::evidence(*application);
			expect(window.session_panel()->active_profile() == std::optional{ cli } && evidence.VaultStores == 0U && evidence.LastEnrollmentDigest.empty()
					   && application->status().DeviceAuthorizationStage != core::device_authorization_stage::authorized,
				   "no device flow is left behind: finishing in the browser now enrolls nothing and the GitHub CLI session stays");
			std::cout << "DEVICE_CLI_CHOICE:" << (aEnding == ending::cli_chosen_first ? "cli-chosen-first" : "cli-connects-at-once") << ":profile=" << cli.value()
					  << ":login=" << github->login_label()->text().toStdString() << '\n';
		}
		consent.stop();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "all composed hosts quiesce");
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication qt{ aArgc, aArgv };
	inaccessible_cli_inventory_stays_signed_out();
	run(ending::cli_connects_at_once);
	run(ending::cli_chosen_first);
	run(ending::device_first);
	std::cout << "device-flow-cli-choice-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
