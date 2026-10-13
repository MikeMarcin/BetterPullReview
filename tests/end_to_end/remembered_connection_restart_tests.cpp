// BPR-1410: every phase below is a new OS process using the public composition.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
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
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <sqlite3.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using options = app::detail::github_public_path_fixture_options;
	using tests::expect;
	void wait_for(const std::function<bool()>& aCondition, const std::string_view aMessage, const std::function<void()>& aDiagnose = { })
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
	auto load(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile = core::default_bpr_profile_id) -> core::remembered_connection_state
	{
		auto store = adapters::sqlite::make_remembered_connection_store(aRoot, aProfile);
		auto state = store->load({ });
		expect(state.has_value(), "real metadata store reopens");
		return *state;
	}
	auto selected(const std::filesystem::path& aRoot, const std::string_view aHostname) -> core::remembered_connection
	{
		const auto row = core::select_remembered_connection(load(aRoot), aHostname);
		expect(row.has_value(), "exact host has a durable enabled choice");
		return *row;
	}
	auto card(qt::workbench_window& aWindow, const std::string_view aHostname) -> qt::server_card&
	{
		const auto cards = aWindow.server_cards();
		const auto found = std::ranges::find_if(cards, [&](const auto* aCard) { return aCard->hostname() == aHostname; });
		expect(found != cards.end(), "exact server card is present");
		return **found;
	}
	void action(qt::workbench_window& aWindow, const char* aName)
	{
		auto* command = aWindow.findChild<QAction*>(QString::fromLatin1(aName));
		expect(command && command->isEnabled(), "public desktop action is enabled");
		command->trigger();
	}
	void connect_cli(qt::workbench_window& aWindow, const std::string_view aHostname, const std::string_view aLogin)
	{
		if (aWindow.session_panel()->status() == core::github_status::session_ready) {
			aWindow.open_settings(qt::settings_page::accounts_and_hosts);
			card(aWindow, aHostname).cli_check_button()->click();
		} else {
			aWindow.home_action()->trigger();
			QApplication::processEvents();
			aWindow.home_landing()->secondary_button()->click();
		}
		wait_for([&] {
			return std::ranges::any_of(aWindow.server_cards(), [&](const auto* aCard) {
				return std::ranges::any_of(aCard->options(), [&](const auto& aOption) { return aOption.Hostname == aHostname && aOption.Label.find(aLogin) != std::string::npos; });
			});
		},
				 "explicit discovery presents the named account");
		std::optional<core::host_profile_id> profile;
		for (const auto* server : aWindow.server_cards()) {
			for (const auto& option : server->options()) {
				if (option.Hostname == aHostname && option.Label.find(aLogin) != std::string::npos) {
					profile = option.Profile;
				}
			}
		}
		expect(profile.has_value(), "account selector resolves exact profile");
		aWindow.connect_github_profile(*profile);
		wait_for([&] { return aWindow.session_panel()->status() == core::github_status::session_ready; }, "explicit named CLI account connects");
	}
	class write_lock final
	{
	public:
		explicit write_lock(const std::filesystem::path& aRoot)
		{
			expect(sqlite3_open_v2((aRoot / "state/v1/remembered-connections.sqlite3").string().c_str(), &Database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "fixture opens the real metadata database");
			expect(sqlite3_exec(Database, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture holds a real rival write transaction");
		}
		~write_lock()
		{
			sqlite3_exec(Database, "ROLLBACK", nullptr, nullptr, nullptr);
			sqlite3_close(Database);
		}
		write_lock(const write_lock&)                    = delete;
		auto operator=(const write_lock&) -> write_lock& = delete;

	private:
		sqlite3* Database = nullptr;
	};
	void corrupt(const std::filesystem::path& aRoot, const char* aSql)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2((aRoot / "state/v1/remembered-connections.sqlite3").string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "fixture opens complete storage");
		const auto result = sqlite3_exec(database, aSql, nullptr, nullptr, nullptr);
		sqlite3_close(database);
		expect(result == SQLITE_OK, "controlled storage refusal is prepared");
	}
	auto database_bytes(const std::filesystem::path& aRoot) -> QByteArray
	{
		QFile database{ QString::fromStdString((aRoot / "state/v1/remembered-connections.sqlite3").string()) };
		expect(database.open(QIODevice::ReadOnly), "fixture can inspect its own metadata bytes");
		return database.readAll();
	}
	void open(qt::workbench_window& aWindow, const std::string_view aHostname)
	{
		aWindow.home_action()->trigger();
		aWindow.home_open_bar()->setText(QStringLiteral("https://%1/octo-org/review-repository/pull/17").arg(QString::fromUtf8(aHostname.data(), static_cast<qsizetype>(aHostname.size()))));
		aWindow.home_open_action()->trigger();
		wait_for([&] { return aWindow.session_panel()->status() == core::github_status::pull_request_ready; }, "exact-host link reaches the fixture PR", [&] { std::cerr << "open:status=" << aWindow.session_panel()->status_text() << ":diagnostic=" << aWindow.session_panel()->diagnostic_text() << ":notice=" << aWindow.home_open_notice()->text().toStdString() << '\n'; });
	}
	void no_private_content(app::github_application& aOwner, qt::workbench_window& aWindow)
	{
		expect(aWindow.showing_home() && aWindow.home_open_bar()->isEnabled(), "refusal leaves usable signed-out Home");
		expect(!aWindow.home_recent_resume_action()->isEnabled(), "refusal cannot resume private reading");
		expect(aWindow.home_recent_list()->count() == 0, "refusal exposes no private Recent titles");
		expect(!github_application_fixture_access::large_review_snapshot(aOwner, aOwner.windows().front()), "refusal exposes no accepted private snapshot");
		const auto evidence = github_application_fixture_access::evidence(aOwner);
		expect(evidence.VaultRetrieveAttempts == 0 && evidence.VaultStores == 0 && evidence.DeviceTransportSubmissions == 0 && !aOwner.status().PublicPreviewSignedOut,
			   "CLI refusal never substitutes native vault, device or public preview authority");
	}
	void close_all(app::profile_desktop& aDesktop)
	{
		while (!aDesktop.windows(core::default_bpr_profile_id).empty()) {
			const auto windows = aDesktop.windows(core::default_bpr_profile_id);
			static_cast<void>(aDesktop.window(windows.back())->close());
			wait_for([&] { return aDesktop.windows(core::default_bpr_profile_id).size() < windows.size(); }, "explicit Close removes this window");
		}
		wait_for([&] { return aDesktop.runtime(core::default_bpr_profile_id) == nullptr; }, "last-window closure drains the profile");
	}
	void child(const std::string& aPhase, const std::filesystem::path& aRoot)
	{
		QApplication::setQuitOnLastWindowClosed(false);
		const bool enterprise                     = aPhase.starts_with("enterprise");
		const auto host                           = enterprise ? "ghe.example.invalid" : "github.com";
		const bool seed                           = aPhase == "seed" || aPhase == "enterprise-seed" || aPhase == "save-refused" || aPhase == "cancel-first";
		auto       fixtureOptions                 = options{ };
		fixtureOptions.ApplicationDataRoot        = aRoot;
		fixtureOptions.PublicPreviewEnabledCourse = !enterprise;
		fixtureOptions.IndependentWindowReads     = true;
		fixtureOptions.RepeatReadSessions         = true;
		fixtureOptions.GithubCliDiscoveryAccounts = std::vector<options::github_cli_discovery_account>{ };
		fixtureOptions.RecentAccount              = std::make_shared<std::atomic<std::uint64_t>>(77U);
		if (seed || aPhase == "replace-account") {
			fixtureOptions.GithubCliDiscoveryAccounts->push_back({ .Hostname = host, .Login = "recent-reviewer-78", .Active = true });
			fixtureOptions.GithubCliDiscoveryAccounts->push_back({ .Hostname = host, .Login = "recent-reviewer-77", .Active = false });
		}
		if (aPhase == "mismatch" || aPhase.ends_with("-78")) {
			fixtureOptions.RecentAccount->store(78U);
		}
		if (aPhase == "unavailable") {
			fixtureOptions.GithubCliDiscoveryAccounts.reset();
		}
		unsigned readAsks = 0;
		QTimer   consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			const tests::exit_on_failure inTheEventLoop;
			if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				const auto login = aPhase == "replace-account" || aPhase == "cancel-first" ? QStringLiteral("recent-reviewer-78") : QStringLiteral("recent-reviewer-77");
				expect(prompt->text() == QStringLiteral("Use %1’s GitHub CLI sign-in on %2?").arg(login, QString::fromLatin1(host)), "only the exact chosen-account read trust is asked");
				expect(prompt->informativeText().contains(QStringLiteral("existing sign-in without saving its token")), "read trust explains existing sign-in without token storage");
				expect(prompt->informativeText().contains(QStringLiteral("reused until you sign out or replace it")), "read trust explains reuse across fresh processes");
				expect(prompt->informativeText().contains(QStringLiteral("Connecting for reads does not authorize publishing.")), "remembered read trust grants no publication authority");
				expect(prompt->detailedText().contains(QStringLiteral("Remembered metadata saves no token and is checked again before use.")), "read trust retains token-free metadata and current revalidation");
				++readAsks;
				prompt->button(aPhase == "cancel-first" ? QMessageBox::Cancel : QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		app::github_application* owner    = nullptr;
		bool                     terminal = false;
		auto                     desktop  = github_application_fixture_access::start_desktop(enterprise ? app::detail::github_public_path_fixture::ghes_3_18 : app::detail::github_public_path_fixture::github_com, fixtureOptions,
																							 [&](auto aResult) { terminal = true; if (aResult) { owner = *aResult; } });
		wait_for([&] { return terminal; }, "new-process desktop starts");
		expect(owner != nullptr, "profile runtime admitted");
		auto& window = owner->window();
		window.show();
		if (aPhase == "fresh" || aPhase == "signed-out" || aPhase == "corrupt" || aPhase == "cancel-first") {
			expect(window.session_panel()->status() == core::github_status::no_profile, "no saved consent means no implicit connection");
			if (aPhase == "cancel-first") {
				window.home_action()->trigger();
				window.home_landing()->secondary_button()->click();
				wait_for([&] { return card(window, host).cli_option() != nullptr; }, "first explicit inventory completes");
				window.connect_github_profile(card(window, host).cli_option()->Profile);
				expect(readAsks == 1U, "cancelled first selection asks once");
			}
			no_private_content(*owner, window);
			const auto evidence = github_application_fixture_access::evidence(*owner);
			expect(evidence.AccountFactoryCalls == 0U && evidence.BootstrapFactoryCalls == 0U, "no implicit CLI or native authenticated launch");
			if (aPhase == "corrupt") {
				wait_for([&] { return !window.home_open_notice()->text().isEmpty(); }, "refused metadata has a visible storage reason");
			}
		} else if (aPhase == "mismatch" || aPhase == "unavailable") {
			const auto before = selected(aRoot, host);
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_failed; }, "saved account or unavailable CLI is visibly refused");
			no_private_content(*owner, window);
			expect(!window.session_panel()->diagnostic_text().empty(), "failure has a presented diagnostic reason");
			expect(selected(aRoot, host) == before, "failed restore preserves the complete exact choice");
		} else {
			std::unique_ptr<write_lock> rival;
			if (aPhase == "save-refused") {
				rival = std::make_unique<write_lock>(aRoot);
			}
			if (seed) {
				connect_cli(window, host, "recent-reviewer-77");
				expect(readAsks == 1U, "only the explicitly selected account receives read consent");
			} else if (aPhase == "exact-host" || aPhase == "enterprise-forget" || aPhase == "after-forget") {
				wait_for([&] { return window.session_panel()->status() == (aPhase == "after-forget" ? core::github_status::no_profile : core::github_status::session_failed); }, "unavailable or forgotten startup host grants no authority");
				no_private_content(*owner, window);
				open(window, host);
				expect(readAsks == 0U, "exact-host navigation reuses remembered trust without inventory");
			} else {
				wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "new process revalidates exact remembered account");
				expect(readAsks == 0U && (!window.sign_in_dialog() || !window.sign_in_dialog()->isVisible()), "restoration performs no selection or read-trust prompt");
			}
			if (aPhase == "save-refused") {
				wait_for([&] { return window.home_open_notice()->text().contains(QStringLiteral("session-only")); }, "failed selection save is visibly session-only");
				expect(window.session_panel()->status() == core::github_status::session_ready && !core::select_remembered_connection(load(aRoot)), "refused save keeps current connection usable without durable consent");
				rival.reset();
				desktop->quit();
				wait_for([&] { return desktop->is_quiescent(); }, "Quit drains the refused save terminal");
				desktop.reset();
				std::cout << "remembered-restart:" << aPhase << ":pass\n";
				return;
			}
			wait_for([&] { return core::select_remembered_connection(load(aRoot), host).has_value(); }, "selection persists before layout or Quit");
			const auto row     = selected(aRoot, host);
			const auto account = aPhase.ends_with("-78") ? 78U : 77U;
			expect(row.Profile == window.session_panel()->active_profile() && row.Account == core::github_account_id{ account } && row.Login == "recent-reviewer-" + std::to_string(account), "stable host/login/numeric authority matches accepted session");
			const auto evidence = github_application_fixture_access::evidence(*owner);
			expect(evidence.AccountFactoryCalls == 1U && evidence.VaultRetrieveAttempts == 0U && evidence.VaultStores == 0U && evidence.BootstrapFactoryCalls == 0U, "restored CLI uses only its chosen account transport without native credential fallback");
			if (aPhase == "replace-account") {
				fixtureOptions.RecentAccount->store(78U);
				connect_cli(window, host, "recent-reviewer-78");
				wait_for([&] { return selected(aRoot, host).Account == core::github_account_id{ 78U }; }, "explicit second account atomically replaces remembered selection");
				const auto replacement = selected(aRoot, host);
				expect(readAsks == 1U && replacement.Profile != row.Profile && replacement.Host == row.Host && replacement.Login == "recent-reviewer-78", "second account receives distinct stable authority and explicit trust");
			} else if (aPhase == "close") {
				close_all(*desktop);
				expect(selected(aRoot, host) == row, "last-window closure retains the connection independently of layout");
			} else if (aPhase == "sign-out" || aPhase == "sign-out-78" || aPhase == "disable-refused") {
				window.open_settings(qt::settings_page::accounts_and_hosts);
				QApplication::processEvents();
				if (aPhase == "disable-refused") {
					rival = std::make_unique<write_lock>(aRoot);
				}
				card(window, host).sign_out_button()->click();
				if (rival) {
					wait_for([&] { return window.home_open_notice()->text().contains(QStringLiteral("Automatic sign-in may return")); }, "failed disable reports that restart may reconnect", [&] { std::cerr << "disable:status=" << window.session_panel()->status_text() << ":notice=" << window.home_open_notice()->text().toStdString() << ":enabled=" << bool(core::select_remembered_connection(load(aRoot), host)) << '\n'; });
					expect(window.session_panel()->status() == core::github_status::no_profile && core::select_remembered_connection(load(aRoot), host).has_value(), "failed disable signs this session out without lying about durable state");
					rival.reset();
				} else {
					wait_for([&] { return !core::select_remembered_connection(load(aRoot), host); }, "Sign out durably disables reuse");
				}
			} else if (aPhase == "enterprise-forget") {
				window.open_settings(qt::settings_page::accounts_and_hosts);
				if (!card(window, host).remember_switch()->isChecked()) {
					card(window, host).remember_switch()->click();
				}
				wait_for([&] { return card(window, host).forget_button()->isVisibleTo(&card(window, host)); }, "retained server offers Forget");
				// Start another validation, then retire it before its terminal.
				window.connect_github_profile(row.Profile);
				card(window, host).forget_button()->click();
				auto* confirmation = window.forget_server_confirmation();
				expect(confirmation && confirmation->isVisible(), "Forget uses the public confirmation");
				confirmation->findChild<QPushButton*>(QStringLiteral("forgetServerConfirm"))->click();
				wait_for([&] { return !core::select_remembered_connection(load(aRoot), host); }, "Forget removes metadata after a pending validation");
				for (const auto context : owner->windows()) {
					expect(owner->window(context)->session_panel()->status() == core::github_status::no_profile, "Forget retires authority in every sibling");
				}
			} else if (aPhase == "restore" || aPhase == "enterprise-restore") {
				open(window, host);
				const auto count = owner->windows().size();
				action(window, "newBprWindowAction");
				wait_for([&] { return owner->windows().size() == count + 1U; }, "sibling window opens");
				auto* sibling = owner->window(owner->windows().back());
				expect(sibling && sibling->session_panel()->active_profile() == row.Profile, "sibling shares the exact accepted authority");
				expect(github_application_fixture_access::evidence(*owner).AccountFactoryCalls == 1U && readAsks == 0U, "sibling does not repeat connection work or consent");
			}
		}
		consent.stop();
		desktop->quit();
		wait_for([&] { return desktop->is_quiescent(); }, "orderly Quit drains all profile effects");
		desktop.reset();
		std::cout << "remembered-restart:" << aPhase << ":pass\n";
	}
	void phase(const std::string& aPhase, const std::filesystem::path& aRoot)
	{
		QProcess process;
		process.setProgram(QFileInfo{ QCoreApplication::applicationFilePath() }.canonicalFilePath());
		process.setArguments({ QStringLiteral("--phase"), QString::fromStdString(aPhase), QString::fromStdString(aRoot.string()) });
		process.setWorkingDirectory(QString::fromStdString(aRoot.parent_path().string()));
		process.start();
		expect(process.waitForStarted(5000), "fresh fixture process starts");
		const auto finished = process.waitForFinished(90000);
		if (!finished) {
			process.kill();
			process.waitForFinished(5000);
		}
		const auto output = process.readAllStandardOutput();
		const auto errors = process.readAllStandardError();
		if (!finished || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
			std::cerr << aPhase << ":" << output.toStdString() << errors.toStdString();
		}
		expect(finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, "new-process qualification phase passes");
		expect(output.contains(QByteArray::fromStdString("remembered-restart:" + aPhase + ":pass")), "child reports its complete phase");
		expect(!errors.contains("AGENT_WORKSPACE_RECOVERY_REFUSED"), "workspace recovery remains usable");
		std::cout << "new-process:" << aPhase << ":pass\n";
	}
	void run()
	{
		QTemporaryDir temporary;
		const auto    root = e2e::private_data_root(temporary);
		expect(root.has_value(), "isolated private application data root");
		phase("fresh", *root);
		phase("seed", *root);
		const auto before   = selected(*root, "github.com");
		const auto metadata = *root / "state/v1/remembered-connections.sqlite3";
		const auto held     = *root / "state/v1/remembered-connections.saved";
		std::filesystem::rename(metadata, held);
		phase("fresh", *root);
		expect(std::filesystem::remove(metadata), "negative control removes only its newly created empty store");
		std::filesystem::rename(held, metadata);
		expect(selected(*root, "github.com") == before, "missing-metadata negative control restores its exact saved fixture");
		std::cout << "negative-control:selection-absent:pass\n";
		phase("restore", *root);
		expect(selected(*root, "github.com").Profile == before.Profile, "process restart preserves connection identity");
		phase("close", *root);
		phase("restore", *root);
		phase("mismatch", *root);
		phase("unavailable", *root);
		phase("restore", *root);
		phase("replace-account", *root);
		phase("restore-78", *root);
		const auto replacement = selected(*root, "github.com");
		expect(replacement.Account == core::github_account_id{ 78U } && replacement.Profile != before.Profile, "different named account stays selected across a new process");
		// Return the viewer fixture to the accepted account for removal courses.
		phase("sign-out-78", *root);
		phase("signed-out", *root);
		phase("seed", *root);
		phase("disable-refused", *root);
		phase("restore", *root);
		phase("sign-out", *root);
		phase("signed-out", *root);
		expect(e2e::remove_course_root(temporary), "course removes its own private root");
		QTemporaryDir refusing;
		const auto    refusedRoot = e2e::private_data_root(refusing);
		expect(refusedRoot.has_value(), "separate fault-course root");
		phase("save-refused", *refusedRoot);
		phase("fresh", *refusedRoot);
		phase("cancel-first", *refusedRoot);
		phase("fresh", *refusedRoot);
		phase("seed", *refusedRoot);
		corrupt(*refusedRoot, "PRAGMA user_version=99");
		const auto bytes = database_bytes(*refusedRoot);
		phase("corrupt", *refusedRoot);
		expect(database_bytes(*refusedRoot) == bytes, "newer metadata is preserved byte-for-byte");
		expect(e2e::remove_course_root(refusing), "fault course cleans its own root");
		QTemporaryDir multiple;
		const auto    multipleRoot = e2e::private_data_root(multiple);
		expect(multipleRoot.has_value(), "multiple-server course root");
		phase("seed", *multipleRoot);
		const auto github = selected(*multipleRoot, "github.com");
		phase("enterprise-seed", *multipleRoot);
		const auto enterprise = selected(*multipleRoot, "ghe.example.invalid");
		expect(github.Host != enterprise.Host && github.Profile != enterprise.Profile && load(*multipleRoot).Connections.size() == 2U, "two canonical servers retain distinct stable identities");
		phase("enterprise-restore", *multipleRoot);
		phase("exact-host", *multipleRoot);
		expect(selected(*multipleRoot, "github.com").Profile == github.Profile && selected(*multipleRoot, "ghe.example.invalid").Profile == enterprise.Profile, "exact-host navigation never aliases either durable identity");
		phase("enterprise-forget", *multipleRoot);
		expect(!core::select_remembered_connection(load(*multipleRoot), "ghe.example.invalid") && selected(*multipleRoot, "github.com").Profile == github.Profile, "late enterprise work cannot recreate the forgotten server or affect another server");
		phase("after-forget", *multipleRoot);
		expect(e2e::remove_course_root(multiple), "multiple-server course cleans its own root");
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication qt{ aArgc, aArgv };
	if (aArgc == 4 && std::string_view{ aArgv[1] } == "--phase") {
		child(aArgv[2], app::native_filesystem_path(QString::fromLocal8Bit(aArgv[3])));
	} else {
		run();
		std::cout << "remembered-connection-restart-tests:pass\n";
	}
} catch (const std::exception& aError) {
	std::cerr << aError.what() << '\n';
	return 1;
}
