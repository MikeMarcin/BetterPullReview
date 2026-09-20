#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <utility>

#if defined(Q_OS_MACOS)
	#include <sys/stat.h>
#endif

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
	void cancel_setup(qt::workbench_window& aWindow)
	{
		expect(aWindow.sign_in_dialog() && aWindow.sign_in_dialog()->isVisible(), "shared sign-in dialog offers cancellation");
		aWindow.sign_in_dialog()->reject();
	}
	void connect_native(qt::workbench_window& aWindow, const std::string& aHostname = "ghe.example.invalid")
	{
		for (auto* card : aWindow.server_cards()) {
			if (card->hostname() == aHostname) {
				auto* action = card->sign_in_button()->isVisible() ? card->sign_in_button() : card->connect_button();
				expect(action->isVisible() && action->isEnabled(), "native connection is offered in the shared dialog");
				action->click();
				return;
			}
		}
		expect(false, "requested native server card exists");
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
		auto* surface = aWindow.sign_in_dialog() && aWindow.sign_in_dialog()->isVisible() ? static_cast<QWidget*>(aWindow.sign_in_dialog()) : &aWindow;
		expect(surface->grab().save(root + QLatin1Char{ '/' } + aName), "public fixture window capture");
	}

	void verify_composed_code_metrics(app::github_application& aApplication, const std::uint32_t aWidth)
	{
		auto* const workbench = aApplication.window().comparison_surface();
		for (auto* const pane : { workbench->left_pane(), workbench->right_pane(), workbench->detail_pane(),
								  aApplication.window().findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource")) }) {
			expect(pane != nullptr, "every composed source pane exists");
			const auto expected = QFontMetricsF{ pane->font() }.horizontalAdvance(QLatin1Char{ ' ' }) * aWidth;
			expect(qAbs(pane->tabStopDistance() - expected) < 0.01
					   && qAbs(pane->document()->defaultTextOption().tabStopDistance() - expected) < 0.01,
				   "composed source layout uses the selected code-font tab width, not Qt's default");
		}
	}

	void verify_composed_file_operation_ink(app::github_application& aApplication, const core::appearance_mode aMode)
	{
		auto* const view            = aApplication.window().files_tree();
		auto        expectedProfile = qt::appearance_profile{ };
		static_cast<void>(expectedProfile.select_mode(aMode));
		// Feed independent safe model rows to the actual composed delegate.
		// Do not manually apply a profile to it: this catches a lost app handoff.
		auto model = qt::files_tree_model{ };
		for (const auto operation : { application::overview_file_operation::added, application::overview_file_operation::removed }) {
			const auto path = std::string{ "synthetic.cpp" };
			const auto id   = core::file_identity_of(path);
			model.apply({ { .Identity = QString::fromStdString(path), .Title = QString::fromStdString(path), .File = id } });
			const auto row = application::overview_file_row{ .Path = path, .Name = path, .Operation = operation, .Additions = 1U, .Deletions = 1U };
			model.apply_overview(std::span{ &row, 1U });
			const auto ink    = expectedProfile.color(operation == application::overview_file_operation::added
														  ? core::appearance_role::state_accepted
														  : core::appearance_role::state_defect);
			auto       raster = QImage{ 420, 32, QImage::Format_ARGB32 };
			raster.fill(view->palette().color(QPalette::Base));
			auto option = QStyleOptionViewItem{ };
			option.initFrom(view);
			option.widget = view;
			option.rect   = raster.rect();
			option.state  = QStyle::State_Enabled;
			{
				auto painter = QPainter{ &raster };
				view->delegate()->paint(&painter, option, model.index_for_file(id));
			}
			auto matching = 0;
			for (auto y = 0; y < raster.height(); ++y) {
				for (auto x = 0; x < raster.width(); ++x) {
					matching += raster.pixelColor(x, y) == ink ? 1 : 0;
				}
			}
			expect(matching > 8, "actual composed drawer delegate receives the selected semantic operation ink");
		}
	}

	void run_course(const bool aEnterprise)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated application store");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = app::native_filesystem_path(QFileInfo{ temporary.path() }.canonicalFilePath());
		expect(adapters::sqlite::make_storage_root_owner_private(*options.ApplicationDataRoot), "private desktop test root under the canonical storage policy");
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
			expect(!window.home_landing()->isHidden() && window.home_landing()->state() == qt::landing_state::sign_in && !window.home_list()->queue_shown(), "the signed-out Home has no queue");
			expect(application->windows().size() == 1U, "public opening owns one window");
			const auto context = application->windows().front();
			github_application_fixture_access::hold_window_read_terminal(*application, context, true);
			submit(window, url);
			wait_for([&] { return github_application_fixture_access::held_window_read_terminals(*application, context) != 0U; },
					 "the real provider read reaches its held decision lane");
			auto* readiness = window.findChild<QLabel*>(QStringLiteral("reviewReadinessText"));
			expect(!window.showing_home() && window.overview_page()->isVisible()
					   && window.overview_page()->width() > window.width() * 3 / 4
					   && readiness != nullptr && readiness->isVisible() && readiness->text().contains(QStringLiteral("Loading"))
					   && window.session_panel()->status() == core::github_status::loading_pull_request
					   && !window.overview_page()->presented(),
				   "a real pending setup opening immediately paints the unaccepted overview and loading readiness");
			capture(window, QStringLiteral("setup-loading-overview.png"));
			github_application_fixture_access::hold_window_read_terminal(*application, context, false);
			expect(button(window, "homeSetupContinue")->isHidden(), "a public host with no account offers no sign-in stop");
			wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "a pasted public link opens signed out with no sign-in step");
			auto evidence = github_application_fixture_access::evidence(*application);
			expect(evidence.VaultStores == 0U && evidence.VaultRetrieveAttempts == 0U && !evidence.AccountTransportSelected && !evidence.CredentialStored && !evidence.CredentialRetrieved, "the signed-out open sends no credential and consults no vault");
			expect(application->status().PublicPreviewSignedOut, "the accepted session is public preview");
			expect(window.home_open_bar()->text().isEmpty(), "accepted opening clears input");
			expect(window.session_panel()->active_host_text().find("github.com") != std::string::npos, "accepted review belongs to exact requested host");
			submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
			cancel_setup(window);
			expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 0U, "cancelled unknown target preserves current review");
			application->begin_shutdown();
			wait_for([&] { return application->status().ShutdownComplete; }, "setup and review work quiesce");
			return;
		}
		// [[BPR-0772]]. RFC-BPR-0032 A1: one card per configured server. The
		// composition configures GitHub.com over native HTTPS and over GitHub
		// CLI, and both are ways in to the one github.com card.
		{
			const auto cards = window.server_cards();
			expect(cards.size() == 1U && cards.front()->hostname() == "github.com" && cards.front()->options().size() == 2U
					   && cards.front()->native_option() != nullptr && cards.front()->cli_option() != nullptr
					   && cards.front()->cli_offer_button() != nullptr && cards.front()->host_label()->text() == QStringLiteral("github.com"),
				   "the composed GitHub.com host profiles share one server card");
		}
		submit(window, url);
		capture(window, QStringLiteral("setup-unknown.png"));
		expect(window.home_open_bar()->text() == url, "pending setup retains the exact pasted target");
		expect(window.sign_in_dialog()->isVisible() && !window.settings()->isVisible() && button(window, "homeSetupContinue")->isHidden(), "pasted PR uses the shared dialog without a continuation button or Settings");
		expect(window.enterprise_server_hostname()->text() == QStringLiteral("ghe.example.invalid"), "pasted host prefills the server editor");
		auto evidence = github_application_fixture_access::evidence(*application);
		expect(evidence.VaultStores == 0U && !evidence.BootstrapTransportSelected && !evidence.AccountTransportSelected, "parsing sends no credentials or provider reads");
		cancel_setup(window);
		expect(window.home_open_bar()->text() == url && button(window, "homeSetupContinue")->isHidden(), "cancel retains editable input and dismisses setup");
		submit(window, url);
		if (aEnterprise) {
			window.enterprise_server_add()->click();
			wait_for([&] { return window.server_cards().size() == 2U; }, "durable Add server makes the exact host available without restart");
			evidence = github_application_fixture_access::evidence(*application);
			expect(evidence.VaultStores == 0U && !evidence.BootstrapTransportSelected, "configuration alone never signs in");
			// [[BPR-0772]]. The added server is a second card; it joins no other.
			const auto cards = window.server_cards();
			expect(cards.size() == 2U && cards.back()->hostname() == "ghe.example.invalid" && cards.back()->options().size() == 1U
					   && cards.front()->options().size() == 2U,
				   "an added enterprise server appends its own card");
		}

		if (aEnterprise) {
			connect_native(window);
			cancel_setup(window);
			submit(window, url);
			connect_native(window);
			submit(window, QStringLiteral("https://replacement.example.invalid/o/r/pull/99"));
			expect(button(window, "homeSetupContinue")->text() == QStringLiteral("Add server and continue"), "a new target replaces queued authentication without reusing its authority");
			submit(window, url);
		}
		connect_native(window);
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
		cancel_setup(window);
		expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 1U, "cancelled unknown target preserves current review and credential authority");

		if (aEnterprise) {
			submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
			window.enterprise_server_add()->click();
			wait_for([&] { return window.server_cards().size() == 3U; }, "second explicit server configured");
			expect(window.server_cards().size() == 3U && window.server_cards().back()->hostname() == "other.example.invalid",
				   "a second explicit server is a third card");
			connect_native(window, "other.example.invalid");
			wait_for([&] { return window.home_open_notice()->text().contains(QStringLiteral("Sign-in did not complete")); }, "recorded transport refuses unsupported authority before commitment");
			expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 1U, "failed staged setup preserves accepted review and existing vault material");
			// [[BPR-0774]]. RFC-BPR-0032 A3 through the shipped composition. The
			// enterprise server added above is kept, so its card remembers that
			// record; Forget confirms, names it, and removes exactly it through
			// the queue host; Remember keeps it again. No credential moves.
			window.open_settings(qt::settings_page::accounts_and_hosts);
			QApplication::processEvents();
			qt::server_card* kept = nullptr;
			for (auto* const candidate : window.server_cards()) {
				if (candidate->hostname() == "ghe.example.invalid") {
					kept = candidate;
				}
			}
			expect(kept != nullptr && kept->enterprise(), "the added enterprise server has a card");
			wait_for([&] { return kept->remember_switch()->isVisibleTo(kept) && kept->remember_switch()->isChecked() && kept->forget_button()->isVisibleTo(kept); }, "the kept enterprise server's card remembers it and offers Forget server");
			const auto record = kept->presentation().Remembered;
			expect(record.has_value() && record->Hostname == "ghe.example.invalid" && std::ranges::find(window.kept_servers(), *record) != window.kept_servers().end(), "the card remembers exactly the record the queue keeps");
			const auto vaultStores = github_application_fixture_access::evidence(*application).VaultStores;
			kept->forget_button()->click();
			QApplication::processEvents();
			auto* confirmation = window.forget_server_confirmation();
			expect(confirmation != nullptr && confirmation->isVisible() && confirmation->parentWidget() == window.settings() && (confirmation->text() + QLatin1Char{ ' ' } + confirmation->informativeText()).contains(QStringLiteral("ghe.example.invalid")), "Forget server confirms first and names the server it removes");
			confirmation->findChild<QPushButton*>(QStringLiteral("forgetServerCancel"))->click();
			QApplication::processEvents();
			expect(window.forget_server_confirmation() == nullptr || !window.forget_server_confirmation()->isVisible(), "Cancel closes the confirmation");
			expect(kept->remember_switch()->isChecked() && std::ranges::find(window.kept_servers(), *record) != window.kept_servers().end(), "Cancel performs no effect");
			kept->forget_button()->click();
			QApplication::processEvents();
			confirmation = window.forget_server_confirmation();
			expect(confirmation != nullptr && confirmation->isVisible(), "Forget server confirms again");
			confirmation->findChild<QPushButton*>(QStringLiteral("forgetServerConfirm"))->click();
			wait_for([&] { return std::ranges::find(window.kept_servers(), *record) == window.kept_servers().end() && !kept->remember_switch()->isChecked() && !kept->forget_button()->isVisibleTo(kept); }, "Forget server removes exactly the kept record through the queue host");
			kept->remember_switch()->click();
			wait_for([&] { return kept->remember_switch()->isChecked() && std::ranges::any_of(window.kept_servers(), [](const application::saved_server& aServer) { return aServer.Hostname == "ghe.example.invalid"; }); }, "Remember this server keeps it again through the queue host");
			expect(github_application_fixture_access::evidence(*application).VaultStores == vaultStores, "keeping and forgetting a server enrols no credential");
			window.settings()->hide();
		}
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "setup and review work quiesce");
	}

#if defined(__APPLE__)
	void check_denied_read_retains_session(app::github_application& aApplication, const QString& aHostname)
	{
		auto&      window  = aApplication.window();
		const auto session = window.session_panel()->session();
		const auto before  = github_application_fixture_access::evidence(aApplication);
		expect(session.has_value() && !aApplication.status().PublicPreviewSignedOut, "the refusal course begins with an accepted account");
		const auto sameAccount = [&session](const auto& aSession) {
			return aSession && aSession->Profile == session->Profile && aSession->Host == session->Host
				&& aSession->Account == session->Account && aSession->Login == session->Login;
		};
		submit(window, QStringLiteral("https://%1/octo-org/inaccessible-repository/pull/17").arg(aHostname));
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "the inaccessible repository reaches a read refusal");
		expect(window.home_open_notice()->text().contains(QStringLiteral("cannot read this pull request")), "the composed refusal is a permission denial");
		expect(sameAccount(window.session_panel()->session()) && !aApplication.status().PublicPreviewSignedOut
				   && window.home_landing()->state() == qt::landing_state::choose_pull_request,
			   "a denied read preserves the accepted account and signed-in Home");
		const auto cards = window.server_cards();
		const auto card  = std::ranges::find_if(cards, [&](const auto* aCard) { return aCard->hostname() == aHostname.toStdString(); });
		expect(card != cards.end() && (*card)->presentation().State == qt::server_card_state::connected
				   && sameAccount((*card)->presentation().Session),
			   "Accounts & hosts retains the same accepted account after the denial");
		const auto after = github_application_fixture_access::evidence(aApplication);
		expect(after.VaultStoreAttempts == before.VaultStoreAttempts && after.VaultRetrieveAttempts == before.VaultRetrieveAttempts
				   && after.VaultDeleteAttempts == before.VaultDeleteAttempts && after.RefreshStoreAttempts == before.RefreshStoreAttempts
				   && after.RefreshRetrieveAttempts == before.RefreshRetrieveAttempts && after.RefreshDeleteAttempts == before.RefreshDeleteAttempts
				   && after.DeviceTransportSubmissions == before.DeviceTransportSubmissions && after.AccountFactoryCalls == before.AccountFactoryCalls,
			   "a denied read neither touches credential slots nor starts another device or account session");
		capture(window, QStringLiteral("setup-denied-%1.png").arg(session->Grant ? QStringLiteral("native") : QStringLiteral("cli")));
		submit(window, QStringLiteral("https://%1/octo-org/review-repository/pull/17").arg(aHostname));
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "the same session opens an accessible review after the refusal");
		expect(sameAccount(window.session_panel()->session()), "retrying an accessible target preserves the account");
		expect(!window.sign_in_dialog()->isVisible(), "retrying an accessible target requires no sign-in");
	}

	void run_native_read_refusal_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated native refusal root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = app::native_filesystem_path(QFileInfo{ temporary.path() }.canonicalFilePath());
		expect(adapters::sqlite::make_storage_root_owner_private(*options.ApplicationDataRoot), "private native refusal root");
		options.RegisteredDeviceCourse     = true;
		options.PersistDeviceBindings      = true;
		options.IndependentWindowReads     = true;
		options.PublicPreviewEnabledCourse = true;
		std::unique_ptr<app::github_application> application;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "native refusal startup completes");
		expect(application != nullptr, "native refusal startup succeeds");
		auto& window = application->window();
		window.show();
		window.home_action()->trigger();
		QApplication::processEvents();
		expect(window.home_landing()->primary_button()->isVisible() && window.home_landing()->primary_button()->isEnabled(), "native sign-in is offered before any PR read");
		window.home_landing()->primary_button()->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "synthetic device account is accepted before any PR read");
		if (window.sign_in_dialog()->isVisible()) {
			cancel_setup(window);
		}
		check_denied_read_retains_session(*application, QStringLiteral("github.com"));
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "native refusal course quiesces");
	}

	void run_connected_cli_destination_course(const bool aPasteFirst, const bool aUnknownHost = false, const bool aGithubCom = false)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated connected CLI root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = app::native_filesystem_path(QFileInfo{ temporary.path() }.canonicalFilePath());
		expect(adapters::sqlite::make_storage_root_owner_private(*options.ApplicationDataRoot), "private connected CLI root");
		options.SetupUnconfiguredEnterprise = aUnknownHost;
		options.IndependentWindowReads      = true;
		options.PublicPreviewEnabledCourse  = aGithubCom;
		const auto hostname                 = aGithubCom ? "github.com" : "ghe.example.invalid";
		options.GithubCliDiscoveryAccounts  = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
			{ .Hostname = hostname, .Login = "octo-reviewer" }
		};
		std::unique_ptr<bpr::app::github_application> application;
		auto                                          terminal = false;
		auto                                          startup  = github_application_fixture_access::start(aGithubCom ? app::detail::github_public_path_fixture::github_com : app::detail::github_public_path_fixture::ghes_3_18, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "connected CLI startup completes");
		expect(application != nullptr, "connected CLI startup succeeds");
		auto& window = application->window();
		window.show();
		window.home_action()->trigger();
		QApplication::processEvents();
		expect(window.home_landing()->secondary_button()->isVisible() && window.home_landing()->secondary_button()->isEnabled(), "CLI is reachable before any device connection");
		// A canceled inventory must not present choices or connect on late completion.
		window.home_landing()->secondary_button()->click();
		cancel_setup(window);
		auto   confirmations = 0U;
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				++confirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		const auto url = QStringLiteral("https://%1/octo-org/review-repository/pull/17").arg(QString::fromLatin1(hostname));
		if (aPasteFirst) {
			submit(window, url);
			window.use_github_cli_button()->click();
		} else {
			window.home_landing()->secondary_button()->click();
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "one Home CLI press connects after one confirmation");
			submit(window, url);
		}

		expect(button(window, "homeSetupContinue")->isHidden(), "connected CLI paste does not offer native sign-in");
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "pasted Enterprise link reaches the PR through the accepted CLI session");
		const auto cards = window.server_cards();
		auto*      card  = *std::ranges::find_if(cards, [&](const auto* aCard) { return aCard->hostname() == hostname; });
		expect(card->cli_option() != nullptr, "discovered CLI profile is installed");
		const auto cliProfile = card->cli_option()->Profile;
		consent.stop();
		expect(confirmations == 1U && !window.settings()->isVisible() && !window.sign_in_dialog()->isVisible(), "CLI setup needs one confirmation and closes the focused dialog");
		const auto evidence = github_application_fixture_access::evidence(*application);
		expect(window.session_panel()->selected_profile() == cliProfile && evidence.AccountTransportSelected, "exact CLI profile serves the recorded PR");
		expect(evidence.VaultStores == 0U && evidence.VaultRetrieveAttempts == 0U && !evidence.BootstrapTransportSelected, "CLI link opens without native credential access");
		expect(evidence.DeviceTransportSubmissions == 0U && !application->status().PublicPreviewSignedOut, "CLI opens through the accepted account without device sign-in or anonymous fallback");
		capture(window, QStringLiteral("setup-connected-cli.png"));
		if (aGithubCom) {
			check_denied_read_retains_session(*application, QString::fromLatin1(hostname));
		}
		submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
		expect(button(window, "homeSetupContinue")->text() == QStringLiteral("Add server and continue"), "another host cannot reuse CLI authentication");
		cancel_setup(window);
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "connected CLI work quiesces");
	}

	/// \brief Reads the recorded PR-wide discussion without selecting code.
	void read_cli_source_update_discussion(qt::workbench_window& aWindow)
	{
		aWindow.resize(1440, 900);
		aWindow.modality()->action(qt::review_modality::discuss)->trigger();
		auto* const panel = aWindow.conversation_panel();
		wait_for([&] { return panel->isVisible() && panel->discuss(); },
				 "Discuss opens independently of a comparison");
		expect(panel->scope_whole_pull_request_button()->isChecked(),
			   "Discuss defaults to the whole PR");
		const auto expectConversation = [&](const std::string_view                      aId,
											const application::github_conversation_kind aKind,
											const QString& aPath, const QString& aBody) {
			const auto index = panel->model()->index_for_id(aId);
			expect(index.isValid()
					   && index.data(qt::github_conversation_model::kind_role).toInt() == static_cast<int>(aKind)
					   && index.data(qt::github_conversation_model::path_role).toString() == aPath
					   && index.data(qt::github_conversation_model::body_role).toString() == aBody,
				   "Discuss admits the exact recorded conversation identity, kind, path and body");
			panel->view()->setCurrentIndex(index);
			panel->view()->expandAll();
			panel->view()->scrollTo(index);
			QApplication::processEvents();
			expect(panel->view()->isVisible() && !panel->view()->isRowHidden(index.row(), index.parent())
					   && panel->view()->viewport()->rect().intersects(panel->view()->visualRect(index)),
				   "recorded conversation body is reachable in the visible selected detail");
		};
		expectConversation("issue-comment:IC_fixture_1", application::github_conversation_kind::issue_comment,
						   { }, QStringLiteral("Fixture follow-up: validation is complete."));
		expectConversation("review:PRR_fixture_1", application::github_conversation_kind::review,
						   { }, QStringLiteral("Please preserve snapshot identity across refreshes."));
		expectConversation("review-comment:PRRC_fixture_reading_review.cpp", application::github_conversation_kind::review_comment,
						   QStringLiteral("review.cpp"), QStringLiteral("Check the exact retained reading position."));
	}

	void run_cli_source_update_course()
	{
		auto reviewServer = std::make_shared<app::detail::review_submission_fixture_state>();
		auto temporary    = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated CLI source/update root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = app::native_filesystem_path(
			QFileInfo{ temporary.path() }.canonicalFilePath());
		expect(
			adapters::sqlite::make_storage_root_owner_private(
				*options.ApplicationDataRoot),
			"private CLI source/update root");
		options.EpochPhase                = app::detail::github_epoch_fixture_phase::single;
		options.GhesCliSourceUpdateCourse = true;
		options.ReviewSubmissionServer    = reviewServer;
		options.RefuseFirstSmartAcquire   = true;
		options.IndependentWindowReads    = true;
		struct catalogue_gate
		{
			std::mutex                  Mutex;
			std::condition_variable_any Changed;
			std::atomic_bool            Reached{ false };
			bool                        Released = false;
		};
		auto gate               = std::make_shared<catalogue_gate>();
		options.BeforeFilesPage = [gate](const std::stop_token stop) {
			gate->Reached = true;
			std::unique_lock lock{ gate->Mutex };
			static_cast<void>(gate->Changed.wait_for(lock, stop, std::chrono::seconds{ 20 }, [&] { return gate->Released; }));
		};
		options.GithubCliDiscoveryAccounts
			= std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
				  { .Hostname = "ghe.example.invalid", .Login = "octo-reviewer" },
			  };

		std::unique_ptr<bpr::app::github_application> application;
		auto                                          terminal = false;
		auto                                          startup  = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::ghes_3_18,
			options,
			[&](auto aResult) {
				terminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return terminal && startup && startup->is_quiescent(); },
			"CLI source/update startup completes");
		expect(application != nullptr, "CLI source/update startup succeeds");
		auto& window = application->window();
		window.show();
		QApplication::processEvents();
		verify_composed_code_metrics(*application, 4U);
		for (const auto width : { 2U, 8U, 4U }) {
			expect(github_application_fixture_access::apply_appearance_gesture(
					   *application, core::appearance_mode::dark, width),
				   "the composed Settings control accepts each offered tab width");
			QApplication::processEvents();
			verify_composed_code_metrics(*application, width);
		}
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::high_contrast_light,
								 core::appearance_mode::high_contrast_dark, core::appearance_mode::dark }) {
			expect(github_application_fixture_access::apply_appearance_gesture(*application, mode, 4U),
				   "Settings applies each drawer appearance through the concrete application");
			QApplication::processEvents();
			verify_composed_file_operation_ink(*application, mode);
		}

		// Discard the first setup surface while the CLI inventory settles. The
		// second gesture must use the discovered, explicitly confirmed profile.
		window.home_landing()->secondary_button()->click();
		cancel_setup(window);
		auto   confirmations = 0U;
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget())) {
				++confirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		window.home_landing()->secondary_button()->click();
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (window.session_panel()->status()
				   != core::github_status::session_ready
			   && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (window.session_panel()->status()
			!= core::github_status::session_ready) {
			const auto evidence = github_application_fixture_access::evidence(
				*application);
			std::cerr << "CLI source/update connect status="
					  << static_cast<int>(window.session_panel()->status())
					  << " text=" << window.session_panel()->status_text()
					  << " diagnostic="
					  << window.session_panel()->diagnostic_text()
					  << " confirmations=" << confirmations
					  << " account-factory=" << evidence.AccountFactoryCalls
					  << " reads=" << evidence.AccountReadCount
					  << " recording-failure=" << evidence.RecordingFailure
					  << '\n';
		}
		expect(
			window.session_panel()->status()
				== core::github_status::session_ready,
			"CLI source/update profile connects");
		const auto pullRequestUrl = QStringLiteral(
			"https://ghe.example.invalid/octo-org/review-repository/pull/17");
		auto loadElapsed = QElapsedTimer{ };
		loadElapsed.start();
		submit(window, pullRequestUrl);
		wait_for([&] {
			const auto* summary = window.findChild<QLabel*>(QStringLiteral("overviewSummary"));
			return gate->Reached && summary != nullptr && summary->text().contains(QStringLiteral("Provisional metadata"));
		},
				 "composed CLI course paints useful metadata while the first file page is still held");
		capture(window, QStringLiteral("setup-cli-progressive-overview.png"));
		const auto firstUsefulMs = loadElapsed.elapsed();
		expect(!window.overview_page()->presented() && !window.overview_page()->entry_button()->isEnabled()
				   && window.overview_page()->file_model()->rowCount() == 0
				   && window.session_panel()->status() == core::github_status::loading_pull_request
				   && github_application_fixture_access::evidence(*application).SmartAcquireCount == 0U,
			   "provisional paint supplies no file selection, snapshot or Git acquisition authority");
		{
			std::lock_guard lock{ gate->Mutex };
			gate->Released = true;
		}
		gate->Changed.notify_all();
		wait_for(
			[&] {
				return window.session_panel()->status()
					== core::github_status::pull_request_ready;
			},
			"CLI source/update PR opens");
		std::cout << "PROGRESSIVE_OVERVIEW first_useful_ms=" << firstUsefulMs
				  << " terminal_ms=" << loadElapsed.elapsed() << " course=recorded_cli_first_file_page_gate\n";
		consent.stop();
		expect(confirmations == 1U, "CLI source/update profile asks for one confirmation");

		const auto  cards = window.server_cards();
		auto* const card  = *std::ranges::find_if(
			cards,
			[](const auto* const aCard) {
				return aCard->hostname() == "ghe.example.invalid";
			});
		expect(card->cli_option() != nullptr, "CLI source/update profile remains explicit");
		const auto cliProfile = card->cli_option()->Profile;
		expect(
			window.session_panel()->selected_profile() == cliProfile,
			"CLI source/update PR belongs to the discovered CLI profile");
		wait_for(
			[&] {
				const auto status = application->status().EpochStatus;
				return status == core::epoch_status::unavailable
					|| status == core::epoch_status::failed;
			},
			"one-shot smart-Git refusal reaches a settled update state");
		expect(!window.files_drawer()->contents_view()->currentIndex().isValid()
				   && window.comparison_surface()->current_result() == nullptr,
			   "refused PR has neither a selected file nor comparison before Discuss");
		read_cli_source_update_discussion(window);
		window.modality()->action(qt::review_modality::read)->trigger();
		auto* const files  = window.files_drawer()->contents_view();
		const auto  review = window.files_drawer()->contents()->index_for_file(
			core::file_identity_of("review.cpp"));
		const auto secondary = window.files_drawer()->contents()->index_for_file(
			core::file_identity_of("secondary.cpp"));
		expect(
			review.isValid() && secondary.isValid(),
			"refused comparison still presents both exact recorded file identities");
		files->selectionModel()->setCurrentIndex(
			review,
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait_for(
			[&] {
				return window.status_text().contains(
					QStringLiteral("No exact comparison is ready"));
			},
			"missing comparison presents actionable recovery");
		auto* const surface         = window.comparison_surface();
		auto        initialEvidence = github_application_fixture_access::evidence(*application);
		expect(
			initialEvidence.SmartAcquireRefusalCount == 1
				&& initialEvidence.SmartAcquireCount == 0
				&& initialEvidence.AccountTransportSelected
				&& !initialEvidence.BootstrapTransportSelected
				&& initialEvidence.VaultStores == 0U
				&& !window.status_text().contains(
					QStringLiteral("selected file changed"),
					Qt::CaseInsensitive)
				&& surface->current_result() == nullptr
				&& surface->left_pane()->toPlainText().isEmpty()
				&& surface->right_pane()->toPlainText().isEmpty(),
			"CLI refusal is unavailable, not stale, and retains no source");
		expect(window.epoch_selectors()->line()->empty_status() == window.epoch_selectors()->status_text()
				   && window.epoch_selectors()->line()->empty_status() != QStringLiteral("No updates loaded."),
			   "the compact public updates line exposes the actual comparison refusal");

		expect(
			window.github_refresh_action()->isEnabled(),
			"unavailable update offers explicit recovery");
		application->begin_shutdown();
		wait_for(
			[&] { return application->status().ShutdownComplete; },
			"refused CLI source/update work quiesces");
		application.reset();
		startup.reset();

	#if defined(Q_OS_MACOS)
		// Reproduce the attended failure through the installed cache and real
		// public composition, not a fake cache result. The refused acquisition
		// above left a pristine repository and no accepted epoch.
		auto manifestCount = 0U;
		for (const auto& entry : std::filesystem::directory_iterator{ *options.ApplicationDataRoot / "repositories/v1" }) {
			const auto manifestPath = entry.path() / "betterpullreview-cache-manifest-v1";
			if (!std::filesystem::is_regular_file(manifestPath)) {
				continue;
			}
			++manifestCount;
			auto input = std::ifstream{ manifestPath, std::ios::binary };
			auto bytes = std::vector<char>{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
			expect(bytes.size() == 141U && bytes[11U] == 2, "public CLI seed has the durable Mac cache manifest");
			input.close();
			bytes[11U] = 1;
			bytes.resize(bytes.size() - 24U);
			struct stat lock{ };
			const auto  lockPath = *options.ApplicationDataRoot / "locks/v1" / (entry.path().stem().string() + ".lock");
			expect(::lstat(lockPath.c_str(), &lock) == 0, "public CLI fixture observes its exact lock");
			for (const auto value : { static_cast<std::uint64_t>(lock.st_dev) ^ 1U, std::uint64_t{ 0 }, static_cast<std::uint64_t>(lock.st_ino) }) {
				for (int shift = 56; shift >= 0; shift -= 8) {
					bytes.push_back(static_cast<char>(value >> shift));
				}
			}
			auto output = std::ofstream{ manifestPath, std::ios::binary | std::ios::trunc };
			output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			expect(static_cast<bool>(output), "public CLI fixture records a legacy remounted empty cache");
		}
		expect(manifestCount == 1U, "public CLI remount course changes only its one empty cache");
	#endif

		options.EpochPhase              = app::detail::github_epoch_fixture_phase::single;
		options.RefuseFirstSmartAcquire = false;
		terminal                        = false;
		startup                         = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::ghes_3_18,
			options,
			[&](auto aResult) {
				terminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return terminal && startup && startup->is_quiescent(); },
			"CLI source/update recovery startup completes");
		expect(application != nullptr, "CLI source/update recovery startup succeeds");
		auto& recoveredWindow = application->window();
		recoveredWindow.show();
		QApplication::processEvents();
		verify_composed_code_metrics(*application, 4U);
		recoveredWindow.home_landing()->secondary_button()->click();
		cancel_setup(recoveredWindow);
		auto   recoveredConfirmations = 0U;
		QTimer recoveredConsent;
		QObject::connect(&recoveredConsent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget())) {
				++recoveredConfirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		recoveredConsent.start(1);
		recoveredWindow.home_landing()->secondary_button()->click();
		wait_for(
			[&] {
				return recoveredWindow.session_panel()->status()
					== core::github_status::session_ready;
			},
			"CLI source/update recovery profile connects");
		submit(recoveredWindow, pullRequestUrl);
		wait_for(
			[&] {
				return recoveredWindow.session_panel()->status()
					== core::github_status::pull_request_ready;
			},
			"CLI source/update recovery PR opens");
		recoveredConsent.stop();
		expect(
			recoveredConfirmations == 1U,
			"CLI source/update recovery profile asks for one confirmation");
		wait_for(
			[&] {
				return application->status().EpochStatus == core::epoch_status::ready
					&& recoveredWindow.epoch_selectors()
						   ->history_model()
						   ->epoch_count()
					== 1U;
			},
			"supported CLI source course retains its initial revision");
		const auto seedEvidence = github_application_fixture_access::evidence(
			*application);
		expect(
			seedEvidence.SmartAcquireRefusalCount == 0
				&& seedEvidence.SmartAcquireCount == 1
				&& seedEvidence.SmartFetchCount == 1,
			"supported CLI source course acquires its initial exact comparison");
		application->begin_shutdown();
		wait_for(
			[&] { return application->status().ShutdownComplete; },
			"supported CLI source seed work quiesces");
		application.reset();
		startup.reset();

		options.EpochPhase = app::detail::github_epoch_fixture_phase::reuse;
		terminal           = false;
		startup            = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::ghes_3_18,
			options,
			[&](auto aResult) {
				terminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return terminal && startup && startup->is_quiescent(); },
			"CLI source/update retained-history startup completes");
		expect(
			application != nullptr,
			"CLI source/update retained-history startup succeeds");
		auto& updatedWindow = application->window();
		updatedWindow.show();
		QApplication::processEvents();
		updatedWindow.home_landing()->secondary_button()->click();
		cancel_setup(updatedWindow);
		auto   updatedConfirmations = 0U;
		QTimer updatedConsent;
		QObject::connect(&updatedConsent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget())) {
				++updatedConfirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		updatedConsent.start(1);
		updatedWindow.home_landing()->secondary_button()->click();
		wait_for(
			[&] {
				return updatedWindow.session_panel()->status()
					== core::github_status::session_ready;
			},
			"CLI source/update retained-history profile connects");
		submit(updatedWindow, pullRequestUrl);
		wait_for(
			[&] {
				return updatedWindow.session_panel()->status()
					== core::github_status::pull_request_ready;
			},
			"CLI source/update retained-history PR opens");
		updatedConsent.stop();
		expect(
			updatedConfirmations == 1U,
			"CLI source/update retained-history profile asks for one confirmation");
		wait_for(
			[&] {
				return application->status().EpochStatus == core::epoch_status::ready
					&& updatedWindow.epoch_selectors()
						   ->history_model()
						   ->epoch_count()
					>= 2U;
			},
			"relaunch installs the recorded GHES update history");
		auto recoveredEvidence = github_application_fixture_access::evidence(*application);
		expect(
			recoveredEvidence.SmartAcquireRefusalCount == 0
				&& recoveredEvidence.SmartAcquireCount == 1
				&& recoveredEvidence.SmartFetchCount == 1
				&& updatedWindow.epoch_selectors()
						->right_oid_editor()
						->text()
						.toStdString()
					== recoveredEvidence.HeadTwoOid,
			"relaunch recovers the second exact update through the same CLI authority");

		expect(!updatedWindow.files_drawer()->contents_view()->currentIndex().isValid()
				   && updatedWindow.comparison_surface()->current_result() == nullptr,
			   "recovered PR opens without a source selection");
		read_cli_source_update_discussion(updatedWindow);
		expect(!updatedWindow.files_drawer()->contents_view()->currentIndex().isValid()
				   && updatedWindow.comparison_surface()->current_result() == nullptr,
			   "reading PR-wide discussion does not require or select code");
		updatedWindow.modality()->action(qt::review_modality::overview)->trigger();
		expect(updatedWindow.overview_page()->entry_button()->isEnabled()
				   && updatedWindow.overview_page()->entry_button()->text() == QStringLiteral("Start review"),
			   "Overview offers Start review for the first visit");
		updatedWindow.overview_page()->entry_button()->click();
		expect(updatedWindow.current_modality() == qt::review_modality::review,
			   "Overview entry reaches Review");
		auto* const recoveredFiles     = updatedWindow.files_drawer()->contents_view();
		const auto  recoveredReview    = updatedWindow.files_drawer()
											 ->contents()
											 ->index_for_file(core::file_identity_of("review.cpp"));
		const auto  recoveredSecondary = updatedWindow.files_drawer()
											 ->contents()
											 ->index_for_file(core::file_identity_of("secondary.cpp"));
		expect(
			recoveredReview.isValid() && recoveredSecondary.isValid(),
			"recovered history presents both exact recorded file identities");
		auto* const recoveredSurface = updatedWindow.comparison_surface();
		wait_for([&] {
			return recoveredSurface->current_context().has_value()
				&& recoveredSurface->current_context()->Key.File == core::file_identity_of("review.cpp")
				&& recoveredSurface->left_pane()->document()->firstBlock().text() == QStringLiteral("int review_line_1 = 11;")
				&& recoveredSurface->right_pane()->document()->firstBlock().text() == QStringLiteral("int review_line_1 = 31;");
		},
				 "Overview entry loads the exact recorded review source without a file-row gesture");

		const auto selectExact = [&](const QModelIndex&     aIndex,
									 const std::string_view aPath,
									 const QString&         aLeftFirst,
									 const QString&         aRightFirst) {
			recoveredFiles->selectionModel()->setCurrentIndex(
				aIndex,
				QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			wait_for(
				[&] {
					return recoveredSurface->current_result() != nullptr
						&& recoveredSurface->current_context().has_value()
						&& recoveredSurface->current_context()->Key.File
						== core::file_identity_of(aPath)
						&& recoveredSurface->left_pane()->document()->firstBlock().text()
						== aLeftFirst
						&& recoveredSurface->right_pane()->document()->firstBlock().text()
						== aRightFirst
						&& !recoveredSurface->geometry_pending();
				},
				"installed Git presents exact recovered file content");
		};
		selectExact(
			recoveredReview,
			"review.cpp",
			QStringLiteral("int review_line_1 = 11;"),
			QStringLiteral("int review_line_1 = 31;"));
		selectExact(
			recoveredSecondary,
			"secondary.cpp",
			QStringLiteral("int secondary_line_1 = 111;"),
			QStringLiteral("int secondary_line_1 = 131;"));
		capture(updatedWindow, QStringLiteral("setup-cli-remount-readable.png"));
		auto* const compose = updatedWindow.findChild<QAction*>(QStringLiteral("composeReviewAction"));
		expect(compose && compose->isEnabled(), "dedicated Compose review action is reachable");
		compose->trigger();
		auto* const editor = updatedWindow.review_submission_editor_view();
		wait_for([&] { return editor->isVisible() && editor->shown_draft().has_value(); },
				 "Compose review presents a local draft");
		const auto draftBody = QStringLiteral("Combined GHES course local review draft.");
		editor->body_editor()->setPlainText(draftBody);
		const auto draftId = editor->shown_draft()->Id;
		button(updatedWindow, "composeReviewPageBack")->click();
		read_cli_source_update_discussion(updatedWindow);
		updatedWindow.modality()->action(qt::review_modality::review)->trigger();
		expect(recoveredSurface->current_context().has_value()
				   && recoveredSurface->current_context()->Key.File == core::file_identity_of("secondary.cpp")
				   && recoveredSurface->right_pane()->document()->firstBlock().text() == QStringLiteral("int secondary_line_1 = 131;"),
			   "Compose and Discuss navigation retain the exact secondary source");
		selectExact(recoveredReview, "review.cpp", QStringLiteral("int review_line_1 = 11;"), QStringLiteral("int review_line_1 = 31;"));
		selectExact(recoveredSecondary, "secondary.cpp", QStringLiteral("int secondary_line_1 = 111;"), QStringLiteral("int secondary_line_1 = 131;"));
		compose->trigger();
		expect(editor->isVisible() && editor->shown_draft()->Id == draftId
				   && editor->shown_draft()->Body == draftBody.toStdString()
				   && editor->body_editor()->toPlainText() == draftBody,
			   "local review draft survives modality and file navigation");
		recoveredEvidence = github_application_fixture_access::evidence(*application);
		expect(
			recoveredEvidence.SmartAcquireRefusalCount == 0
				&& recoveredEvidence.SmartAcquireCount == 1
				&& recoveredEvidence.SmartFetchCount == 1,
			"both files reuse the one recovered exact comparison without another fetch");

		application->begin_shutdown();
		wait_for(
			[&] { return application->status().ShutdownComplete; },
			"CLI source/update work quiesces");
		const auto mutation = github_application_fixture_access::review_mutation_evidence(*application);
		const auto lock     = std::lock_guard{ reviewServer->Mutex };
		expect(reviewServer->Submissions == 0U && reviewServer->Reviews.empty()
				   && mutation.Submissions == 0U && mutation.StoredComments == 0U
				   && mutation.Replies == 0U && mutation.Resolutions == 0U,
			   "combined navigation and local drafting submit and publish nothing");
	}

	void run_github_cli_discovery_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated CLI discovery application store");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = app::native_filesystem_path(
			QFileInfo{ temporary.path() }.canonicalFilePath());
		expect(adapters::sqlite::make_storage_root_owner_private(
				   *options.ApplicationDataRoot),
			   "private CLI discovery test root under the canonical storage policy");
		options.GithubCliDiscoveryAccounts
			= std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
				  { .Hostname = "ghe.discovered.example", .Login = "enterprise-reviewer" },
				  { .Hostname = "second.example.invalid", .Login = "another-reviewer" },
			  };

		std::unique_ptr<bpr::app::github_application> application;
		auto                                          startupTerminal = false;
		auto                                          startup         = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::github_com,
			options,
			[&](auto aResult) {
				startupTerminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return startupTerminal && startup && startup->is_quiescent(); },
			"CLI discovery application startup reaches a terminal");
		expect(application != nullptr, "CLI discovery application startup succeeds");
		auto& window = application->window();
		window.show();
		QApplication::processEvents();
		expect(!window.home_landing()->secondary_button()->isHidden()
				   && window.home_landing()->secondary_button()->text()
					   == QStringLiteral("Use GitHub CLI"),
			   "the shipped composition exposes CLI discovery as its own signed-out path");
		window.home_landing()->secondary_button()->click();
		wait_for(
			[&] {
				return window.github_cli_discovery_status()->text().contains(
					QStringLiteral("Found 2"));
			},
			"the composed discovery reaches its ready terminal");
		expect(window.sign_in_dialog()->isVisible() && !window.settings()->isVisible(), "CLI discovery presents choices without opening Settings");

		capture(window, QStringLiteral("setup-cli-choices.png"));
		qt::server_card* discovered = nullptr;
		for (auto* const card : window.server_cards()) {
			if (card->hostname() == "ghe.discovered.example") {
				discovered = card;
			}
		}
		expect(discovered != nullptr && discovered->options().size() == 1U
				   && discovered->native_option() == nullptr
				   && discovered->cli_option() != nullptr
				   && discovered->cli_offer_button() != nullptr
				   && discovered->cli_offer_button()->text().contains(
					   QStringLiteral("enterprise-reviewer")),
			   "the discovered GHES identity is a CLI-only card naming the observed login");

		const auto profile = discovered->cli_option()->Profile;
		QTimer::singleShot(0, [] {
			if (auto* const prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget());
				prompt != nullptr) {
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		discovered->cli_offer_button()->click();
		QApplication::processEvents();
		expect(window.session_panel()->selected_profile() == profile
				   && window.session_panel()->profile_option(profile) != nullptr
				   && window.session_panel()->profile_option(profile)->Hostname
					   == "ghe.discovered.example",
			   "the CLI-only GHES card reaches the existing exact-host connection path");

		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; },
				 "CLI discovery work quiesces");
	}
#endif
}
int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	run_course(false);
	run_course(true);
#if defined(__APPLE__)
	run_github_cli_discovery_course();
	run_connected_cli_destination_course(false);
	run_connected_cli_destination_course(true);
	run_connected_cli_destination_course(true, true);
	run_connected_cli_destination_course(false, false, true);
	run_native_read_refusal_course();
	run_cli_source_update_course();
#endif
	std::cout << "setup-destination-public-path:pass\n";
	return EXIT_SUCCESS;
}
