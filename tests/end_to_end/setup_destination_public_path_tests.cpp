#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
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
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QThread>
#include <QTimer>

#include <algorithm>
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
	void run_connected_cli_destination_course(const bool aPasteFirst, const bool aUnknownHost = false)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated connected CLI root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = app::native_filesystem_path(QFileInfo{ temporary.path() }.canonicalFilePath());
		expect(adapters::sqlite::make_storage_root_owner_private(*options.ApplicationDataRoot), "private connected CLI root");
		options.SetupUnconfiguredEnterprise = aUnknownHost;
		options.IndependentWindowReads      = true;
		options.GithubCliDiscoveryAccounts  = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
			{ .Hostname = "ghe.example.invalid", .Login = "octo-reviewer" }
		};
		std::unique_ptr<bpr::app::github_application> application;
		auto                                          terminal = false;
		auto                                          startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::ghes_3_18, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "connected CLI startup completes");
		expect(application != nullptr, "connected CLI startup succeeds");
		auto& window = application->window();
		window.show();
		QApplication::processEvents();
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
		const auto url = QStringLiteral("https://ghe.example.invalid/octo-org/review-repository/pull/17");
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
		auto*      card  = *std::ranges::find_if(cards, [](const auto* aCard) { return aCard->hostname() == "ghe.example.invalid"; });
		expect(card->cli_option() != nullptr, "discovered CLI profile is installed");
		const auto cliProfile = card->cli_option()->Profile;
		consent.stop();
		expect(confirmations == 1U && !window.settings()->isVisible() && !window.sign_in_dialog()->isVisible(), "CLI setup needs one confirmation and closes the focused dialog");
		const auto evidence = github_application_fixture_access::evidence(*application);
		expect(window.session_panel()->selected_profile() == cliProfile && evidence.AccountTransportSelected, "exact CLI profile serves the recorded PR");
		expect(evidence.VaultStores == 0U && evidence.VaultRetrieveAttempts == 0U && !evidence.BootstrapTransportSelected, "CLI link opens without native credential access");
		capture(window, QStringLiteral("setup-connected-cli.png"));
		submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
		expect(button(window, "homeSetupContinue")->text() == QStringLiteral("Add server and continue"), "another host cannot reuse CLI authentication");
		cancel_setup(window);
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "connected CLI work quiesces");
	}

	void run_cli_source_update_course()
	{
		auto temporary = QTemporaryDir{ };
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
		options.RefuseFirstSmartAcquire   = true;
		options.IndependentWindowReads    = true;
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
		submit(window, pullRequestUrl);
		wait_for(
			[&] {
				return window.session_panel()->status()
					== core::github_status::pull_request_ready;
			},
			"CLI source/update PR opens");
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

		expect(
			window.github_refresh_action()->isEnabled(),
			"unavailable update offers explicit recovery");
		application->begin_shutdown();
		wait_for(
			[&] { return application->status().ShutdownComplete; },
			"refused CLI source/update work quiesces");
		application.reset();
		startup.reset();

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

		updatedWindow.modality()->action(qt::review_modality::read)->trigger();
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
	run_cli_source_update_course();
#endif
	std::cout << "setup-destination-public-path:pass\n";
	return EXIT_SUCCESS;
}
