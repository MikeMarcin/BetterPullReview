// RFC-BPR-0032 amendment r1. Home's in-window account setup over the real
// composition: the recorded GitHub.com fixture, its registered device flow
// (pending until the course lets the browser finish) and two synthetic gh
// accounts. No live GitHub, gh, credential or device-flow request.
//
// - The GitHub CLI way: Home shows the setup in place of the first-launch
//   card; gh's accounts are listed; Connect as octo-reviewer signs in with no
//   dialog and no modal question; the Ready step names the account; Start
//   reviewing leaves Home signed in.
// - The browser way: the code is shown in the setup; Cancel ends the flow;
//   a new code, once the browser finishes, signs in and reaches Ready.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/account_setup_view.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
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

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using bpr::tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const std::string_view aMessage, const qint64 aTimeoutMs = 20000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	[[nodiscard]] auto button(const QWidget& aView, const QString& aName) -> QPushButton*
	{
		for (auto* const candidate : aView.findChildren<QPushButton*>(aName)) {
			if (candidate->isVisibleTo(&aView)) {
				return candidate;
			}
		}
		return nullptr;
	}

	void press(const QWidget& aView, const QString& aName)
	{
		auto* const found = button(aView, aName);
		expect(found != nullptr && found->isEnabled(), "the named setup button is shown and enabled");
		found->click();
		QApplication::processEvents();
	}

	/// Every modal or top-level window other than the main window, counted
	/// while the course runs: the setup must open none.
	struct window_watch
	{
		QTimer         Timer;
		int            Popups = 0;
		const QWidget* Main   = nullptr;
		explicit window_watch(const QWidget& aMain)
			: Main{ &aMain }
		{
			QObject::connect(&Timer, &QTimer::timeout, [this] {
				const bpr::tests::exit_on_failure inTheEventLoop;
				if (auto* const modal = QApplication::activeModalWidget(); modal != nullptr) {
					++Popups;
					modal->close();
				}
				for (auto* const widget : QApplication::topLevelWidgets()) {
					if (widget != Main && widget->isVisible() && (qobject_cast<QDialog*>(widget) != nullptr)) {
						++Popups;
						widget->hide();
					}
				}
			});
			Timer.start(5);
		}
	};

	struct composed
	{
		QTemporaryDir                            Temporary;
		std::shared_ptr<std::atomic_bool>        Pending = std::make_shared<std::atomic_bool>(true);
		std::unique_ptr<app::github_application> Application;

		composed()
		{
			auto options                       = app::detail::github_public_path_fixture_options{ };
			options.ApplicationDataRoot        = e2e::private_data_root(Temporary);
			options.RegisteredDeviceCourse     = true;
			options.IndependentWindowReads     = true;
			options.AccountSetupInWindow       = true;
			options.DeviceAuthorizationPending = Pending;
			auto first                         = app::detail::github_public_path_fixture_options::github_cli_discovery_account{ };
			first.Hostname                     = "github.com";
			first.Login                        = "octo-reviewer";
			auto second                        = app::detail::github_public_path_fixture_options::github_cli_discovery_account{ };
			second.Hostname                    = "github.com";
			second.Login                       = "octo-second";
			second.Active                      = false;
			options.GithubCliDiscoveryAccounts = std::vector{ first, second };
			auto terminal                      = false;
			auto startup                       = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
				terminal = true;
				if (aResult) {
					Application = std::move(*aResult);
				}
			});
			wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
			expect(Application != nullptr, "application startup succeeds");
		}

		void stop()
		{
			Application->begin_shutdown();
			wait_for([&] { return Application->status().ShutdownComplete; }, "the hosts quiesce");
		}
	};

	void github_cli_way()
	{
		auto  app    = composed{ };
		auto& window = app.Application->window();
		window.show();
		auto* const setup = window.account_setup();
		wait_for([&] { return setup != nullptr && setup->isVisible() && setup->projection().State.Phase == core::account_setup_phase::choosing_way; },
				 "signed out, Home shows the account setup at its first step");
		expect(!window.home_landing()->isVisible(), "the first-launch card gives way to the setup");
		auto watch = window_watch{ window };

		expect(setup->projection().GithubCliOffered, "the composition offers the GitHub CLI way");
		auto* const cliCard = setup->findChild<QWidget*>(QStringLiteral("accountSetupWayCli"));
		expect(cliCard != nullptr, "the CLI card is shown");
		auto event = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
		QApplication::sendEvent(cliCard, &event);
		QApplication::processEvents();
		press(*setup, QStringLiteral("accountSetupContinue"));
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::choosing_account; }, "gh's accounts are read through the composed discovery");
		expect(setup->projection().State.Accounts.size() == 2U && !setup->projection().State.Chosen.has_value(), "both stubbed accounts are offered and none is chosen for the reviewer");

		const auto cards = setup->findChildren<QWidget*>(QStringLiteral("accountSetupAccount"));
		expect(cards.size() == 2, "each account is a card");
		auto space = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
		QApplication::sendEvent(cards.front(), &space);
		QApplication::processEvents();
		expect(button(*setup, QStringLiteral("accountSetupConnect")) != nullptr && button(*setup, QStringLiteral("accountSetupConnect"))->text() == QStringLiteral("Connect as octo-reviewer"),
			   "Connect names the chosen account");
		press(*setup, QStringLiteral("accountSetupConnect"));
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::connected; }, "Connect signs in through the composed activation", 30000);
		expect(window.session_panel()->status() == core::github_status::session_ready, "the window's session is ready");
		expect(setup->projection().State.Login == "octo-reviewer", "the Ready step names the connected account");
		expect(watch.Popups == 0, "no dialog or modal question opened at any step");
		expect(window.sign_in_dialog() == nullptr || !window.sign_in_dialog()->isVisible(), "the shared sign-in dialog was not used");
		std::cout << "ACCOUNT_SETUP:cli:login=" << setup->projection().State.Login << '\n';

		press(*setup, QStringLiteral("accountSetupFinish"));
		wait_for([&] { return !setup->isVisible(); }, "Start reviewing leaves the setup for a signed-in Home");
		app.stop();
	}

	void browser_way()
	{
		auto  app    = composed{ };
		auto& window = app.Application->window();
		window.show();
		auto* const setup = window.account_setup();
		wait_for([&] { return setup != nullptr && setup->isVisible(); }, "Home shows the account setup");
		auto watch = window_watch{ window };

		press(*setup, QStringLiteral("accountSetupContinue"));
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::awaiting_browser && !setup->projection().Device.UserCode.empty(); },
				 "the composed device flow's code is shown in the setup");
		auto* const code = setup->findChild<QLabel*>(QStringLiteral("accountSetupCode"));
		expect(code != nullptr && code->text().toStdString() == setup->projection().Device.UserCode, "the code label is the device host's code");

		press(*setup, QStringLiteral("accountSetupBack"));
		wait_for([&] { return !window.device_authorization()->signing_in(); }, "Cancel ends the device flow");
		expect(setup->projection().State.Phase == core::account_setup_phase::choosing_way, "and returns to the way choice");

		press(*setup, QStringLiteral("accountSetupContinue"));
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::awaiting_browser && !setup->projection().Device.UserCode.empty(); },
				 "a new code is shown");
		app.Pending->store(false);
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::connected; }, "the browser finishing signs in", 30000);
		expect(window.session_panel()->status() == core::github_status::session_ready && !setup->projection().State.Login.empty(), "the Ready step names the account");
		expect(watch.Popups == 0, "no dialog or modal question opened at any step");
		std::cout << "ACCOUNT_SETUP:browser:login=" << setup->projection().State.Login << '\n';
		app.stop();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	github_cli_way();
	browser_way();
	std::cout << "account-setup-public-path-tests:pass\n";
	return EXIT_SUCCESS;
}
