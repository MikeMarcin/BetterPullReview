#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	void submit(qt::workbench_window& aWindow, const QString& aText)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		expect(aWindow.home_open_bar()->isVisible(), "Home action exposes the real locator before typing");
		aWindow.home_open_bar()->setText(aText);
		aWindow.home_open_action()->trigger();
	}
	auto button(qt::workbench_window& aWindow, const char* aName) -> QPushButton*
	{
		auto* result = aWindow.findChild<QPushButton*>(QString::fromUtf8(aName));
		expect(result != nullptr, "public setup button is composed");
		return result;
	}
	void capture(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_SETUP_ARTIFACT_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "review artifact directory");
		aWindow.show();
		QApplication::processEvents();
		expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName), "public fixture window capture");
	}

	void run_course(const bool aEnterprise)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated application store");
		auto options                        = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot         = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
		options.SetupUnconfiguredEnterprise = aEnterprise;
		options.RegisteredDeviceCourse      = !aEnterprise;
		// [[BPR-0732]]. The GitHub.com course composes the anonymous
		// authority: a pasted public link with no account opens signed out.
		options.PublicPreviewEnabledCourse = !aEnterprise;
		options.RepeatReadSessions         = true;
		std::unique_ptr<bpr::app::github_application> application;
		auto                                          startupTerminal = false;
		auto                                          startup         = github_application_fixture_access::start(aEnterprise ? app::detail::github_public_path_fixture::ghes_3_18 : app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			startupTerminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return startupTerminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(application != nullptr, "application startup succeeds");
		auto& window = application->window();
		window.show();
		QApplication::processEvents();
		const auto url = aEnterprise ? QStringLiteral("https://ghe.example.invalid/octo-org/review-repository/pull/17/files#diff-abcd") : QStringLiteral("https://github.com/octo-org/review-repository/pull/17");
		if (!aEnterprise) {
			// [[BPR-0732]]. One gesture, signed out: no sign-in stop, no
			// continuation press, no credential request, no vault write.
			expect(!window.home_landing()->isHidden() && window.home_landing()->state() == qt::landing_state::sign_in && !window.queue_view()->isVisible(), "the signed-out Home has no queue");
			submit(window, url);
			expect(button(window, "homeSetupContinue")->isHidden(), "a public host with no account offers no sign-in stop");
			expect(window.home_open_notice()->text().contains(QStringLiteral("without signing in")) || window.session_panel()->status() == core::github_status::pull_request_ready, "the notice names the signed-out open");
			wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "a pasted public link opens signed out with no sign-in step");
			auto evidence = github_application_fixture_access::evidence(*application);
			expect(evidence.VaultStores == 0U && evidence.VaultRetrieveAttempts == 0U && !evidence.AccountTransportSelected && !evidence.CredentialStored && !evidence.CredentialRetrieved, "the signed-out open sends no credential and consults no vault");
			expect(application->status().PublicPreviewSignedOut, "the accepted session is public preview");
			expect(window.home_open_bar()->text().isEmpty(), "accepted opening clears input");
			expect(window.session_panel()->active_host_text().find("github.com") != std::string::npos, "accepted review belongs to exact requested host");
			submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
			expect(button(window, "homeSetupCancel")->isVisible(), "cancel action visible before gesture");
			button(window, "homeSetupCancel")->click();
			expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 0U, "cancelled unknown target preserves current review");
			application->begin_shutdown();
			wait_for([&] { return application->status().ShutdownComplete; }, "setup and review work quiesce");
			return;
		}
		submit(window, url);
		capture(window, QStringLiteral("setup-unknown.png"));
		expect(window.home_open_bar()->text() == url, "pending setup retains the exact pasted target");
		expect(button(window, "homeSetupContinue")->isVisible() && button(window, "homeSetupCancel")->isVisible(), "pending setup actions are visible on Home");
		expect(button(window, "homeSetupContinue")->text() == QStringLiteral("Add server and continue"), "correct explicit setup action");
		auto evidence = github_application_fixture_access::evidence(*application);
		expect(evidence.VaultStores == 0U && !evidence.BootstrapTransportSelected && !evidence.AccountTransportSelected, "parsing sends no credentials or provider reads");
		expect(button(window, "homeSetupCancel")->isVisible(), "cancel action visible before gesture");
		button(window, "homeSetupCancel")->click();
		expect(window.home_open_bar()->text() == url && button(window, "homeSetupContinue")->isHidden(), "cancel retains editable input and dismisses setup");
		submit(window, url);
		if (aEnterprise) {
			expect(button(window, "homeSetupContinue")->isVisible(), "continue action visible before gesture");
			button(window, "homeSetupContinue")->click();
			wait_for([&] { return button(window, "homeSetupContinue")->text() == QStringLiteral("Sign in and continue") && button(window, "homeSetupContinue")->isVisible(); }, "durable Add server makes the exact host available without restart");
			evidence = github_application_fixture_access::evidence(*application);
			expect(evidence.VaultStores == 0U && !evidence.BootstrapTransportSelected, "configuration alone never signs in");
		}

		if (aEnterprise) {
			expect(button(window, "homeSetupContinue")->isVisible(), "continue action visible before gesture");
			button(window, "homeSetupContinue")->click();
			expect(button(window, "homeSetupCancel")->isVisible(), "cancel action visible before gesture");
			button(window, "homeSetupCancel")->click();
			submit(window, url);
			expect(button(window, "homeSetupContinue")->isVisible(), "continue action visible before gesture");
			button(window, "homeSetupContinue")->click();
			submit(window, QStringLiteral("https://replacement.example.invalid/o/r/pull/99"));
			expect(button(window, "homeSetupContinue")->text() == QStringLiteral("Add server and continue"), "a new target replaces queued authentication without reusing its authority");
			submit(window, url);
		}
		expect(button(window, "homeSetupContinue")->isVisible(), "continue action visible before gesture");
		button(window, "homeSetupContinue")->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "accepted exact setup opens the recorded pull request");
		evidence = github_application_fixture_access::evidence(*application);
		expect(evidence.VaultStores == 1U && evidence.BootstrapTransportSelected && evidence.AccountTransportSelected && evidence.RecordingConsumed, "public setup traverses one exact recorded authenticated read course");
		expect(window.home_open_bar()->text().isEmpty(), "accepted opening clears input");
		expect(window.session_panel()->active_host_text().find(aEnterprise ? "ghe.example.invalid" : "github.com") != std::string::npos, "accepted review belongs to exact requested host");

		if (aEnterprise) {
			capture(window, QStringLiteral("setup-accepted.png"));
		}
		if (aEnterprise) {
			expect(window.home_open_notice()->text().contains(QStringLiteral("location is unavailable")) && window.statusBar()->currentMessage().contains(QStringLiteral("location is unavailable")), "unmapped exact fragment has an explanation on the accepted review");
		}
		submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
		expect(button(window, "homeSetupCancel")->isVisible(), "cancel action visible before gesture");
		button(window, "homeSetupCancel")->click();
		expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 1U, "cancelled unknown target preserves current review and credential authority");

		if (aEnterprise) {
			submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
			expect(button(window, "homeSetupContinue")->isVisible(), "continue action visible before gesture");
			button(window, "homeSetupContinue")->click();
			wait_for([&] { return button(window, "homeSetupContinue")->text() == QStringLiteral("Sign in and continue"); }, "second explicit server configured");
			expect(button(window, "homeSetupContinue")->isVisible(), "continue action visible before gesture");
			button(window, "homeSetupContinue")->click();
			wait_for([&] { return window.home_open_notice()->text().contains(QStringLiteral("Sign-in did not complete")); }, "recorded transport refuses unsupported authority before commitment");
			expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 1U, "failed staged setup preserves accepted review and existing vault material");
		}
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "setup and review work quiesce");
	}
}
int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	run_course(false);
	run_course(true);
	std::cout << "setup-destination-public-path:pass\n";
	return EXIT_SUCCESS;
}
