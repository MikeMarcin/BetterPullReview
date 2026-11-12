// [[BPR-0803]], RFC-BPR-0032 amendment r1. A device sign-in and the GitHub
// CLI way in, in Home's account setup. The setup shows one way at a time, so
// choosing the GitHub CLI while a code waits ends that code: the device flow
// is cancelled visibly rather than left polling, and finishing in the browser
// afterwards enrolls nothing. Three complete compositions over the recorded
// GitHub.com fixture:
//
// - gh holds accounts but none usable: the setup says so and nothing signs in;
// - a code waits, then the reviewer changes to the GitHub CLI and connects
//   gh's active account: the code is ended and the session is gh's;
// - a code waits and the browser finishes first: the native profile signs in,
//   and no gh consent was asked.
//
// The device fixture answers `authorization_pending` until the course lets
// the browser finish. No live GitHub, credential or device-flow request.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "account_setup_course.hpp"
#include "bpr/qt/account_setup_view.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPushButton>
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

	void press(const qt::account_setup_view& aSetup, const QString& aName)
	{
		QApplication::processEvents();
		for (auto* const button : aSetup.findChildren<QPushButton*>(aName)) {
			if (button->isVisibleTo(&aSetup) && button->isEnabled()) {
				button->click();
				QApplication::processEvents();
				return;
			}
		}
		expect(false, "the named setup button is shown and enabled");
	}

	/// No read question opens in any ending; consent is the setup's Connect.
	void refuse_questions(QTimer& aTimer)
	{
		QObject::connect(&aTimer, &QTimer::timeout, [] {
			if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				const tests::exit_on_failure inTheEventLoop;
				expect(false, "no read-trust question opens; consent is the account setup's Connect");
				prompt->close();
			}
		});
		aTimer.start(1);
	}

	// [[BPR-1571]]. Exercise the actual discovery completion and projection
	// with the inaccessible-Keychain shape.
	void inaccessible_cli_inventory_stays_signed_out()
	{
		QTemporaryDir temporary;
		auto          options              = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.RegisteredDeviceCourse     = true;
		options.PublicPreviewEnabledCourse = true;
		options.IndependentWindowReads     = true;
		auto account                       = app::detail::github_public_path_fixture_options::github_cli_discovery_account{ };
		account.Hostname                   = "github.com";
		account.Login                      = "fixture-user";
		account.SignedIn                   = false;
		account.KeychainHeld               = false;
		options.GithubCliDiscoveryAccounts = std::vector{ account };
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
		window.begin_account_setup(core::account_setup_server::github_com, "github.com", core::account_setup_way::github_cli);
		auto* const setup = window.account_setup();
		wait_for([&] { return setup->projection().State.Problem == core::account_setup_problem::gh_no_usable_account; },
				 "completed but ineligible inventory reaches its own actionable problem in the setup");
		expect(setup->projection().State.Accounts.empty() && window.session_panel()->status() == core::github_status::no_profile,
			   "inaccessible account is not promoted to an authenticated session");
		press(*setup, QStringLiteral("accountSetupRetry"));
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::choosing_account; }, "Check again can be retried after environment recovery");
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "ineligible-inventory hosts quiesce");
	}

	enum class ending : std::uint8_t {
		/// A code waits; the reviewer changes to the GitHub CLI and connects.
		cli_chosen,
		/// A code waits and the browser finishes first.
		device_first,
	};

	void run(const ending aEnding)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "private temporary application root");
		auto options                       = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.RegisteredDeviceCourse     = true;
		options.PublicPreviewEnabledCourse = true;
		options.IndependentWindowReads     = true;
		auto account                       = app::detail::github_public_path_fixture_options::github_cli_discovery_account{ };
		account.Hostname                   = "github.com";
		account.Login                      = "octo-reviewer";
		options.GithubCliDiscoveryAccounts = std::vector{ account };
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
		QTimer questions;
		refuse_questions(questions);
		auto* const setup = window.account_setup();
		auto* const panel = window.device_authorization();

		window.begin_account_setup(core::account_setup_server::github_com, "github.com", core::account_setup_way::browser);
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::awaiting_browser && panel->signing_in(); },
				 "the browser way begins the device flow and shows its code in the setup");

		if (aEnding == ending::device_first) {
			pending->store(false);
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "the browser finishing the device flow signs in", 20000);
			wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::connected; }, "the setup reaches Ready");
			settle(500);
			const auto        evidence = github_application_fixture_access::evidence(*application);
			const auto* const active   = window.session_panel()->active_profile() ? window.session_panel()->profile_option(*window.session_panel()->active_profile()) : nullptr;
			expect(!panel->signing_in() && evidence.VaultStores == 1U && active != nullptr && active->Transport == application::github_profile_transport::native_https,
				   "the GitHub CLI was never asked for: no consent, the native profile's session, and one device grant stored");
			std::cout << "DEVICE_CLI_CHOICE:device-first:login=" << setup->projection().State.Login << '\n';
		} else {
			// Change the way: Back ends the code, then the GitHub CLI connects
			// gh's active account with Connect.
			press(*setup, QStringLiteral("accountSetupChange"));
			wait_for([&] { return !panel->signing_in(); }, "changing the way ends the waiting device flow visibly");
			auto driver = e2e::account_setup_driver{ window, e2e::account_setup_driver::plan{ } };
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready && !setup->isVisible(); }, "the GitHub CLI way in signs in");
			expect(driver.connects() == 1U, "Connect, the consent, is pressed once");
			// Long enough for the device flow to have polled again had it kept
			// running; letting the browser finish now must sign nothing in.
			pending->store(false);
			settle(7000);
			const auto evidence = github_application_fixture_access::evidence(*application);
			expect(evidence.VaultStores == 0U && evidence.LastEnrollmentDigest.empty()
					   && application->status().DeviceAuthorizationStage != core::device_authorization_stage::authorized,
				   "no device flow is left behind: finishing in the browser now enrolls nothing and the GitHub CLI session stays");
			std::cout << "DEVICE_CLI_CHOICE:cli-chosen:login=" << window.session_panel()->session()->Login << '\n';
		}
		questions.stop();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "all composed hosts quiesce");
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication qt{ aArgc, aArgv };
	inaccessible_cli_inventory_stays_signed_out();
	run(ending::cli_chosen);
	run(ending::device_first);
	std::cout << "device-flow-cli-choice-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
