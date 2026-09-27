#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/diagnostic_settings_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>
#include <filesystem>
#include <mutex>
#include <sqlite3.h>
using namespace bpr;
using tests::expect;
class folder_receiver final : public QObject
{
	Q_OBJECT
public:
	std::mutex Mutex;
	QUrl       Address;
public slots:
	void open(const QUrl& aAddress)
	{
		std::lock_guard lock{ Mutex };
		Address = aAddress;
	}
};
namespace {
	void until(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	struct running
	{
		std::unique_ptr<app::profile_desktop> Desktop;
		app::github_application*              Owner    = nullptr;
		qt::workbench_window*                 Window   = nullptr;
		qt::settings_window*                  Settings = nullptr;
		qt::diagnostic_settings_panel*        Panel    = nullptr;
		QCheckBox*                            Enabled  = nullptr;
		QComboBox*                            Detail   = nullptr;
		QComboBox*                            Privacy  = nullptr;
		QLabel*                               Status   = nullptr;
		explicit running(const std::filesystem::path& aRoot)
		{
			app::detail::github_public_path_fixture_options options;
			options.ApplicationDataRoot    = aRoot;
			options.RegisteredDeviceCourse = true;
			options.IndependentWindowReads = true;
			options.RepeatReadSessions     = true;
			Desktop                        = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1,
																										   options, [this](auto aResult) { if (aResult) { Owner = *aResult; } });
			until([this] { return Owner && !Owner->windows().empty(); }, "real profile desktop starts");
			Window = Owner->window(Owner->windows().front());
			expect(Window, "real window exists");
			Window->show();
			auto* action = Window->findChild<QAction*>(QStringLiteral("openDiagnosticsAction"));
			expect(action && action->isEnabled(), "Help Diagnostics available while signed out");
			action->trigger();
			Settings = Window->settings();
			expect(Settings && Settings->current_page() == qt::settings_page::storage_and_privacy, "Help raises profile Storage and privacy");
			Panel = dynamic_cast<qt::diagnostic_settings_panel*>(Settings->findChild<QWidget*>(QStringLiteral("diagnosticSettingsPanel")));
			expect(Panel && Panel->isVisible(), "real diagnostics panel rendered");
			Enabled = Panel->findChild<QCheckBox*>(QStringLiteral("diagnosticEnabled"));
			Detail  = Panel->findChild<QComboBox*>(QStringLiteral("diagnosticDetail"));
			Privacy = Panel->findChild<QComboBox*>(QStringLiteral("diagnosticPrivacy"));
			Status  = Panel->findChild<QLabel*>(QStringLiteral("diagnosticStatus"));
			until([this] { return Enabled->isEnabled(); }, "settings persistence admitted independently of sign-in");
			expect(!Settings->grab().isNull(), "full Settings paints with controls");
		}
		void stop()
		{
			Desktop->quit();
			until([this] { return Desktop->is_quiescent(); }, "controls and profile effects quiesce before destruction");
			expect(Desktop->exit_code() == 0, "orderly shutdown succeeds");
			Desktop.reset();
		}
	};
}
auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private disposable parent");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private data root");
	{
		running app{ root };
		expect(app.Enabled->isChecked() && app.Detail->currentIndex() == 0 && app.Privacy->currentIndex() == 0, "public default On Standard Anonymized");
		app.Detail->setCurrentIndex(1);
		until([&] { return app.Enabled->isEnabled() && app.Detail->currentIndex() == 1; }, "public Verbose accepted");
		expect(app.Privacy->currentIndex() == 0, "Verbose does not increase identity disclosure");
		folder_receiver receiver;
		QDesktopServices::setUrlHandler(QStringLiteral("file"), &receiver, "open");
		app.Panel->findChild<QPushButton*>(QStringLiteral("diagnosticOpen"))->click();
		until([&] { return app.Status->text().contains(QStringLiteral("Folder opening requested")); }, "host-selected folder handoff acknowledged");
		{
			std::lock_guard lock{ receiver.Mutex };
			expect(receiver.Address.toLocalFile() == QString::fromStdU16String((root / "logs").u16string()), "Open targets exact admitted profile directory");
		}
		QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
		app.Window->home_landing()->primary_button()->click();
		until([&] { return app.Window->session_panel()->status() == core::github_status::session_ready; }, "credential-free fixture establishes a session");
		app.Window->home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/17"));
		app.Window->home_open_action()->trigger();
		until([&] { return app.Window->session_panel()->status() == core::github_status::pull_request_ready; }, "real provider fixture completes requested PR");
		until([&] { return tests::read_diagnostic_logs(root).find("\"event_detail\":\"verbose\"") != std::string::npos; }, "Verbose intermediate producer reaches real file");
		app.Window->findChild<QAction*>(QStringLiteral("openDiagnosticsAction"))->trigger();
		until([&] { return !app.Status->text().contains(QStringLiteral("Recent context: 0 events")); }, "public status exposes bounded recent producer context");
		app.Window->open_settings(qt::settings_page::accounts_and_hosts);
		auto* signedInCard = app.Window->server_card_for(*app.Window->session_panel()->active_profile());
		auto* signout      = signedInCard ? signedInCard->sign_out_button() : nullptr;
		expect(signout && signout->isEnabled() && signout->isVisible(), "fixture account has explicit Sign out");
		signout->click();
		until([&] { return !signedInCard->signed_in(); }, "real Sign out retires accepted account");
		app.Window->findChild<QAction*>(QStringLiteral("openDiagnosticsAction"))->trigger();
		until([&] { return app.Status->text().contains(QStringLiteral("Recent context: 0 events")); }, "real Sign out discards profile transient context without waiting for logger I/O");
		until([&] { return app.Enabled->isEnabled(); }, "diagnostic controls remain reachable signed out");
		app.Privacy->setCurrentIndex(1);
		auto* consent = app.Panel->findChild<QMessageBox*>(QStringLiteral("diagnosticPrivacyConfirmation"));
		expect(consent && consent->isVisible() && app.Privacy->currentIndex() == 0, "identity choice waits for disclosure and accepted state");
		consent->done(QMessageBox::Cancel);
		expect(app.Privacy->currentIndex() == 0, "cancel retains anonymization");
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		app.Privacy->setCurrentIndex(1);
		consent = app.Panel->findChild<QMessageBox*>(QStringLiteral("diagnosticPrivacyConfirmation"));
		expect(consent && consent->isVisible(), "explicit second privacy choice");
		consent->done(QMessageBox::Yes);
		until([&] { return app.Enabled->isEnabled() && app.Privacy->currentIndex() == 1; }, "include locally accepted after confirmation");
		app.Enabled->click();
		until([&] { return app.Enabled->isEnabled() && !app.Enabled->isChecked(); }, "Off acknowledged");
		auto* siblingAction = app.Window->findChild<QAction*>(QStringLiteral("newBprWindowAction"));
		siblingAction->trigger();
		until([&] { return app.Owner->windows().size() == 2U; }, "another window shares the profile");
		for (const auto context : app.Owner->windows()) {
			auto* window = app.Owner->window(context);
			window->open_settings(qt::settings_page::storage_and_privacy);
			expect(window->settings() == app.Settings, "windows share accepted settings host");
		}
		expect(app.Settings->findChildren<QWidget*>(QStringLiteral("diagnosticSettingsPanel")).size() == 1, "no duplicate profile diagnostics panel");
		until([&] { return app.Enabled->isEnabled(); }, "shared panel refresh completes");
		app.Panel->findChild<QPushButton*>(QStringLiteral("diagnosticClear"))->click();
		auto* clear = app.Panel->findChild<QMessageBox*>(QStringLiteral("diagnosticClearConfirmation"));
		expect(clear && clear->isVisible(), "Clear asks about exact profile scope");
		clear->done(QMessageBox::Yes);
		until([&] { return app.Status->text().contains(QStringLiteral("cleared")); }, "Clear reaches acknowledged terminal");
		expect(tests::read_diagnostic_logs(root).empty(), "real files cleared while recording Off");
		app.stop();
	}
	{
		running app{ root };
		expect(!app.Enabled->isChecked() && app.Detail->currentIndex() == 1 && app.Privacy->currentIndex() == 1, "restart preserves accepted Off and independent choices");
		expect(tests::read_diagnostic_logs(root).empty(), "Off at restart emits no startup or hidden context");
		app.stop();
	}
	sqlite3*   db   = nullptr;
	const auto path = (root / "state/v1/diagnostic-preferences.sqlite3").string();
	expect(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "open fixture preferences for malformed-value test");
	expect(sqlite3_exec(db, "UPDATE diagnostic_preferences SET version=99, detail=99, privacy=99", nullptr, nullptr, nullptr) == SQLITE_OK, "inject unknown persisted values");
	sqlite3_close(db);
	{
		running app{ root };
		expect(!app.Enabled->isChecked() && app.Detail->currentIndex() == 0 && app.Privacy->currentIndex() == 0, "unknown schema safely preserves Off and defaults detail/privacy");
		expect(app.Status->text().contains(QStringLiteral("safe defaults")), "migration refusal visible on public UI");
		app.Enabled->click();
		until([&] { return app.Enabled->isEnabled() && app.Enabled->isChecked(); }, "explicit choice repairs compatible invalid preferences");
		app.stop();
	}
	expect(adapters::sqlite::load_diagnostic_preferences(root).Preferences == application::diagnostic_preferences{ }, "explicit repaired preference persists");
	return 0;
}

catch (...) {
	return tests::failing_exit();
}

#include "diagnostic_controls_public_path_tests.moc"
