#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <iostream>
namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using tests::expect;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnose = { })
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aDiagnose) {
			aDiagnose();
		}
		expect(aCondition(), aMessage);
	}
	auto start(app::detail::github_public_path_fixture_options aOptions, app::detail::github_public_path_fixture aFixture = app::detail::github_public_path_fixture::github_com) -> std::unique_ptr<app::github_application>
	{
		std::unique_ptr<app::github_application> result;
		bool                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(aFixture, std::move(aOptions), [&](auto aResult) { terminal=true; if (aResult) { result=std::move(*aResult); } });
		wait_for([&] { return terminal && startup->is_quiescent(); }, "startup terminates");
		expect(result != nullptr, "application starts");
		return result;
	}
	void stop(std::unique_ptr<app::github_application>& aApplication)
	{
		aApplication->begin_shutdown();
		wait_for([&] { return aApplication->status().ShutdownComplete; }, "orderly Quit drains metadata and all hosts");
		aApplication.reset();
	}
	void run()
	{
		QTemporaryDir temporary;
		auto          options              = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.PublicPreviewEnabledCourse = true;
		options.IndependentWindowReads     = true;
		expect(options.ApplicationDataRoot.has_value(), "private root");
		options.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{ { .Hostname = "github.com", .Login = "octo-reviewer" } };
		options.GithubCliInventoryChecks   = std::make_shared<std::atomic_size_t>(0U);
		unsigned prompts                   = 0;
		QTimer   consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				const tests::exit_on_failure inTheEventLoop;
				expect(prompt->text() == QStringLiteral("Use octo-reviewer’s GitHub CLI sign-in on github.com?"), "read consent names the chosen account and server");
				expect(prompt->informativeText().contains(QStringLiteral("existing sign-in without saving its token")), "read consent explains existing sign-in without token storage");
				expect(prompt->informativeText().contains(QStringLiteral("reused until you sign out or replace it")), "read consent explains durable reuse");
				expect(prompt->informativeText().contains(QStringLiteral("Connecting for reads does not authorize publishing.")), "read consent grants no publication authority");
				expect(prompt->detailedText().contains(QStringLiteral("Remembered metadata saves no token and is checked again before use.")), "read consent retains revalidation and token-free metadata disclosure");
				const auto capture = qEnvironmentVariable("BPR_REMEMBERED_CONNECTION_CONSENT_CAPTURE");
				if (!capture.isEmpty()) {
					expect(prompt->grab().save(capture, "PNG"), "offscreen consent capture is written");
				}
				++prompts;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		auto  application = start(options);
		auto& window      = application->window();
		window.show();
		window.home_action()->trigger();
		QApplication::processEvents();
		expect(window.home_landing()->secondary_button()->text() == QStringLiteral("Use GitHub CLI"), "Home offers the explicit CLI action");
		window.home_landing()->secondary_button()->click();
		expect(window.sign_in_dialog() && window.sign_in_dialog()->isVisible(), "CLI discovery opens the public sign-in surface");
		window.sign_in_dialog()->reject();
		window.home_landing()->secondary_button()->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "explicit CLI selection accepts authenticated viewer");
		expect(prompts == 1, "first CLI selection asks read trust once");
		const auto profile = window.session_panel()->active_profile();
		auto       store   = adapters::sqlite::make_remembered_connection_store(*options.ApplicationDataRoot, core::default_bpr_profile_id);
		wait_for([&] { auto saved=store->load({}); return saved && core::select_remembered_connection(*saved).has_value(); }, "successful selection persists before layout or Quit");
		auto       saved = store->load({ });
		const auto row   = *core::select_remembered_connection(*saved);
		expect(row.Profile == profile && row.Login == "octo-reviewer" && row.Account.value() != 0, "saved selection binds accepted stable profile and numeric account");
		stop(application);
		// Empty discovery inventory proves restoration does not need it.
		options.GithubCliDiscoveryAccounts->clear();
		application = start(options);
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_ready; }, "reopen revalidates exact remembered CLI connection");
		expect(application->window().session_panel()->active_profile() == profile && prompts == 1, "reopen preserves identity without another selector or trust prompt");
		stop(application);
		const auto checksBeforeFailure    = options.GithubCliInventoryChecks->load();
		options.ViewerNamesAnotherAccount = true;
		application                       = start(options);
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_failed; }, "changed authenticated viewer refuses the remembered authority");
		stop(application);
		expect(options.GithubCliInventoryChecks->load() == checksBeforeFailure, "failed remembered startup drains without ambient account inventory network checks");
		saved               = store->load({ });
		const auto retained = core::select_remembered_connection(*saved);
		expect(retained && retained->Profile == row.Profile && retained->Account == row.Account, "validation refusal preserves the exact durable choice");
		options.ViewerNamesAnotherAccount = false;
		application                       = start(options);
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_ready; }, "later launch retries the retained exact authority successfully");
		expect(application->window().session_panel()->active_profile() == profile && prompts == 1, "retry keeps stable identity and read consent");
		stop(application);
		// Keep the login unchanged while changing only the persisted numeric binding.
		// Removing the provider's durable numeric check must make this course fail.
		saved              = store->load({ });
		auto mismatched    = row;
		mismatched.Profile = core::host_profile_id{ saved->LastProfileId + 1U };
		mismatched.Account = core::github_account_id{ row.Account.value() + 1U };
		auto changed       = core::remember_connection(*saved, saved->Revision, mismatched);
		expect(changed && store->save(*changed, saved->Revision, { }).has_value(), "fixture writes an exact same-login mismatched numeric binding");
		application = start(options);
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_failed; }, "numeric account mismatch refuses restored session");
		expect(!application->window().session_panel()->active_profile().has_value() || application->window().session_panel()->status() != core::github_status::session_ready, "mismatch exposes no accepted session");
		stop(application);
		saved            = store->load({ });
		auto repaired    = row;
		repaired.Profile = core::host_profile_id{ saved->LastProfileId + 1U };
		changed          = core::remember_connection(*saved, saved->Revision, repaired);
		expect(changed && store->save(*changed, saved->Revision, { }).has_value(), "fixture restores the authenticated account binding for retry");
		application = start(options);
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_ready; }, "a corrected numeric binding accepts the matching authenticated viewer");
		application->window().open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		const auto cards = application->window().server_cards();
		auto*      card  = *std::ranges::find_if(cards, [](const auto* aCard) { return aCard->hostname() == "github.com"; });
		expect(card->sign_out_button()->isEnabled(), "restored CLI session offers the public Sign out action");
		card->sign_out_button()->click();
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::no_profile; }, "Sign out retires the shared accepted identity");
		wait_for([&] { auto disabled=store->load({}); return disabled && !core::select_remembered_connection(*disabled); }, "Sign out saves disabled reuse before Quit");
		stop(application);
		application = start(options);
		expect(application->window().session_panel()->status() == core::github_status::no_profile && prompts == 1, "restart after Sign out stays signed out without discovery or trust");
		stop(application);
		consent.stop();
	}
	void failed_cli_connect()
	{
		QTemporaryDir temporary;
		auto          options              = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.PublicPreviewEnabledCourse = true;
		options.ViewerNamesAnotherAccount  = true;
		options.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{ { .Hostname = "github.com", .Login = "octo-reviewer" } };
		options.GithubCliInventoryChecks   = std::make_shared<std::atomic_size_t>(0U);
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		auto  application = start(options);
		auto& window      = application->window();
		window.show();
		window.home_action()->trigger();
		window.home_landing()->secondary_button()->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_failed; }, "failed Connect reaches its refusal through the public CLI gesture");
		stop(application);
		expect(options.GithubCliInventoryChecks->load() == 1U, "failed Connect drains with only the explicitly requested discovery and no automatic inventory network check");
		consent.stop();
	}

	void forget_server()
	{
		QTemporaryDir temporary;
		auto          options              = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.IndependentWindowReads     = true;
		options.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{ { .Hostname = "ghe.example.invalid", .Login = "octo-reviewer" } };
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [] { if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) { prompt->button(QMessageBox::Yes)->click(); } });
		consent.start(1);
		auto  application = start(options, app::detail::github_public_path_fixture::ghes_3_18);
		auto& window      = application->window();
		window.show();
		window.home_action()->trigger();
		QApplication::processEvents();
		window.home_landing()->secondary_button()->click();
		window.sign_in_dialog()->reject();
		window.home_landing()->secondary_button()->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "enterprise CLI connection accepts before Forget");
		consent.stop();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		const auto cards = window.server_cards();
		auto*      card  = *std::ranges::find_if(cards, [](const auto* aCard) { return aCard->hostname() == "ghe.example.invalid"; });
		if (!card->remember_switch()->isChecked()) {
			card->remember_switch()->click();
		}
		wait_for([&] { return card->forget_button()->isVisibleTo(card); }, "the retained enterprise server offers Forget");
		card->forget_button()->click();
		QApplication::processEvents();
		auto* confirmation = window.forget_server_confirmation();
		expect(confirmation != nullptr && confirmation->isVisible(), "Forget asks the existing server confirmation");
		confirmation->findChild<QPushButton*>(QStringLiteral("forgetServerConfirm"))->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::no_profile; }, "Forget retires accepted enterprise authority");
		stop(application);
		auto       store = adapters::sqlite::make_remembered_connection_store(*options.ApplicationDataRoot, core::default_bpr_profile_id);
		const auto saved = store->load({ });
		expect(saved && saved->Connections.empty() && saved->LastProfileId != 0, "Forget removes remembered selection while retaining identity allocation");
		options.GithubCliDiscoveryAccounts->clear();
		application = start(options, app::detail::github_public_path_fixture::ghes_3_18);
		expect(application->window().session_panel()->status() == core::github_status::no_profile, "Forget leaves the next launch signed out");
		stop(application);
	}
	void native_replacement_and_legacy()
	{
		QTemporaryDir temporary;
		auto          options              = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.PublicPreviewEnabledCourse = true;
		options.IndependentWindowReads     = true;
		options.RepeatReadSessions         = true;
		options.RegisteredDeviceCourse     = true;
		options.DeviceRefreshCourse        = true;
		options.PersistDeviceBindings      = true;
		options.SharedDeviceVault          = app::detail::make_shared_github_fixture_vault();
		options.Offline                    = std::make_shared<app::detail::github_offline_fixture>();
		options.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{ { .Hostname = "github.com", .Login = "octo-reviewer" } };
		options.DeviceAuthorizationPending = std::make_shared<std::atomic_bool>(false);
		auto store                         = adapters::sqlite::make_remembered_connection_store(*options.ApplicationDataRoot, core::default_bpr_profile_id);
		auto application                   = start(options);
		auto showCard                      = [&]() -> qt::server_card& {
			application->window().show();
			application->window().open_settings(qt::settings_page::accounts_and_hosts);
			QApplication::processEvents();
			const auto cards = application->window().server_cards();
			return **std::ranges::find_if(cards, [](const auto* aCard) { return aCard->hostname() == "github.com"; });
		};
		showCard().sign_in_button()->click();
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_ready; }, "initial device grant is accepted");
		wait_for([&] { return github_application_fixture_access::device_binding_durable(*application); }, "older native grant is durably resumable before replacement");
		wait_for([&] { auto saved=store->load({}); const auto row=saved ? core::select_remembered_connection(*saved) : std::nullopt; return row && row->Transport == core::remembered_connection_transport::native_device; }, "initial device choice is durable");
		const auto native = *core::select_remembered_connection(*store->load({ }));
		// Begin another native attempt, then explicitly choose CLI beside it.
		options.DeviceAuthorizationPending->store(true);
		showCard().sign_in_button()->click();
		wait_for([&] { return application->window().device_authorization()->signing_in(); }, "replacement device flow waits");
		unsigned asks = 0;
		QTimer   consent;
		QObject::connect(&consent, &QTimer::timeout, [&] { if (auto* prompt=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) { ++asks; prompt->button(QMessageBox::Yes)->click(); } });
		consent.start(1);
		application->window().use_github_cli_button()->click();
		wait_for([&] { auto saved=store->load({}); const auto row=saved ? core::select_remembered_connection(*saved) : std::nullopt; return row && row->Transport == core::remembered_connection_transport::github_cli; }, "explicit CLI replacement is durable", [&] { const auto evidence=github_application_fixture_access::evidence(*application); std::cerr << "native-cli:status=" << application->window().session_panel()->status_text() << ":diagnostic=" << application->window().session_panel()->diagnostic_text() << ":asks=" << asks << ":factories=" << evidence.AccountFactoryCalls << ":wire=" << evidence.RecordingFailure << '\n'; });
		const auto cli = *core::select_remembered_connection(*store->load({ }));
		expect(asks == 1 && cli.Profile != native.Profile && cli.Host == native.Host, "CLI replacement has a distinct stable connection on the same server");
		options.DeviceAuthorizationPending->store(false);
		const auto priorVaultAttempts = github_application_fixture_access::evidence(*application).VaultRetrieveAttempts;
		stop(application);
		options.GithubCliDiscoveryAccounts->clear();
		application = start(options);
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_ready; }, "CLI restores while the older device grant remains");
		expect(application->window().session_panel()->active_profile() == cli.Profile && github_application_fixture_access::evidence(*application).VaultRetrieveAttempts == priorVaultAttempts, "older device material cannot replace the saved CLI connection");
		options.DeviceAuthorizationPending->store(true);
		showCard().sign_in_button()->click();
		wait_for([&] { return application->window().device_authorization()->signing_in() && !application->window().device_authorization()->user_code_field()->text().isEmpty(); }, "native attempt is pending beside the restored CLI authority");
		const auto storesBeforeSignOut = github_application_fixture_access::evidence(*application).VaultStores;
		const auto cardsBeforeSignOut  = application->window().server_cards();
		(*std::ranges::find_if(cardsBeforeSignOut, [](const auto* aCard) { return aCard->hostname() == "github.com"; }))->sign_out_button()->click();
		wait_for([&] { return !core::select_remembered_connection(*store->load({ })); }, "CLI Sign out disables reuse beside the older device grant");
		options.DeviceAuthorizationPending->store(false);
		QElapsedTimer late;
		late.start();
		while (late.elapsed() < 7000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(application->window().session_panel()->status() == core::github_status::no_profile && !core::select_remembered_connection(*store->load({ })) && github_application_fixture_access::evidence(*application).VaultStores == storesBeforeSignOut, "late device authorization cannot undo Sign out or persist a replacement");
		stop(application);
		application = start(options);
		expect(application->window().session_panel()->status() == core::github_status::no_profile && github_application_fixture_access::evidence(*application).VaultRetrieveAttempts == priorVaultAttempts, "CLI Sign out cannot resurrect the prior device grant on restart");
		options.GithubCliDiscoveryAccounts->push_back({ .Hostname = "github.com", .Login = "octo-reviewer" });
		stop(application);
		application = start(options);
		application->window().show();
		application->window().home_action()->trigger();
		application->window().home_landing()->secondary_button()->click();
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_ready; }, "explicit CLI selection reconnects after disabled reuse");
		wait_for([&] { auto saved=store->load({}); return saved && core::select_remembered_connection(*saved).has_value(); }, "explicit retry has durable metadata");
		const auto retriedCli = *core::select_remembered_connection(*store->load({ }));
		options.DeviceAuthorizationPending->store(true);
		showCard().sign_in_button()->click();
		wait_for([&] { return application->window().device_authorization()->signing_in() && !application->window().device_authorization()->user_code_field()->text().isEmpty(); }, "native replacement offers its code", [&] { std::cerr << "native-begin:status=" << application->window().session_panel()->status_text() << ":stage=" << static_cast<int>(application->status().DeviceAuthorizationStage) << ":enabled=" << showCard().sign_in_button()->isEnabled() << ":submissions=" << github_application_fixture_access::evidence(*application).DeviceTransportSubmissions << '\n'; });
		application->window().sign_in_dialog()->reject();
		wait_for([&] { return !application->window().device_authorization()->signing_in(); }, "native replacement cancels");
		expect(*core::select_remembered_connection(*store->load({ })) == retriedCli, "cancelled replacement preserves the complete durable CLI choice");
		options.DeviceAuthorizationPending->store(false);
		options.Offline->Offline.store(true);
		showCard().sign_in_button()->click();
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_failed; }, "offline native replacement fails validation visibly");
		expect(*core::select_remembered_connection(*store->load({ })) == retriedCli, "unsuccessful native replacement preserves durable CLI authority");
		options.Offline->Offline.store(false);
		showCard().sign_in_button()->click();
		wait_for([&] { auto saved=store->load({}); const auto row=saved ? core::select_remembered_connection(*saved) : std::nullopt; return row && row->Transport == core::remembered_connection_transport::native_device; }, "successful native sign-in atomically replaces CLI");
		const auto replacement = *core::select_remembered_connection(*store->load({ }));
		expect(replacement.Profile != cli.Profile && replacement.Profile.value() > cli.Profile.value(), "successful native replacement receives fresh identity");
		wait_for([&] { return github_application_fixture_access::device_binding_durable(*application); }, "native replacement retains established device-grant durability", [&] { const auto evidence=github_application_fixture_access::evidence(*application); std::cerr << "native-durable:status=" << application->window().session_panel()->status_text() << ":reason=" << application->window().session_panel()->diagnostic_text() << ":refresh=" << static_cast<int>(application->status().DeviceAuthorizationRefreshState) << ":stores=" << evidence.VaultStores << ":attempts=" << evidence.VaultStoreAttempts << '\n'; });
		stop(application);
		application = start(options);
		wait_for([&] { return application->window().session_panel()->status() == core::github_status::session_ready; }, "native replacement is the next launch's choice", [&] { const auto evidence=github_application_fixture_access::evidence(*application); std::cerr << "native-restore:status=" << application->window().session_panel()->status_text() << ":reason=" << application->window().session_panel()->diagnostic_text() << ":vault=" << evidence.VaultRetrieveAttempts << ":wire=" << evidence.RecordingFailure << '\n'; });
		expect(application->window().session_panel()->active_profile() == replacement.Profile && asks == 2, "restart retains native identity without CLI trust");
		showCard().sign_out_button()->click();
		wait_for([&] { return !core::select_remembered_connection(*store->load({ })); }, "native sign-out durably disables reuse");
		const auto signedOutVaultAttempts = github_application_fixture_access::evidence(*application).VaultRetrieveAttempts;
		stop(application);
		application = start(options);
		expect(application->window().session_panel()->status() == core::github_status::no_profile && github_application_fixture_access::evidence(*application).VaultRetrieveAttempts == signedOutVaultAttempts, "disabled reuse does not resurrect any older device binding");
		stop(application);
		consent.stop();
	}

}
int main(int aArgc, char** aArgv)
try {
	QApplication qt{ aArgc, aArgv };
	run();
	failed_cli_connect();
	forget_server();
	native_replacement_and_legacy();
	std::cout << "remembered-connection-public-path-tests:pass\n";
} catch (const std::exception& aError) {
	std::cerr << aError.what() << '\n';
	return 1;
}
