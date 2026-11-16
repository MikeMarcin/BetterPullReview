// [[BPR-2198]] (owner, 2026-11-15). Export Profile Settings… never writes a
// credential. Over the real composition and the recorded GitHub.com fixture,
// the course signs in through Home's browser way (the composed device flow,
// whose fixture hands out an access and a refresh token), then exports from
// Settings › Profiles with its sections ticked, Identity included, and reads
// the file back: the signed-in login is there, and no token, device code,
// keychain or `gh` material is. No live GitHub, gh or credential.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/agent_configuration.hpp"
#include "bpr/qt/account_setup_view.hpp"
#include "bpr/qt/agent_configuration_dialogs.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/profiles_settings_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidgetItem>

#include <array>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using bpr::tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const std::string_view aMessage, const qint64 aTimeoutMs = 30000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	void press(const QWidget& aView, const QString& aName)
	{
		for (auto* const candidate : aView.findChildren<QPushButton*>(aName)) {
			if (candidate->isVisibleTo(&aView) && candidate->isEnabled()) {
				candidate->click();
				QApplication::processEvents();
				return;
			}
		}
		expect(false, "the named setup button is shown and enabled");
	}

	[[nodiscard]] auto lower(std::string aText) -> std::string
	{
		for (auto& byte : aText) {
			byte = (byte >= 'A' && byte <= 'Z') ? static_cast<char>(byte - 'A' + 'a') : byte;
		}
		return aText;
	}

	void export_with_a_live_sign_in()
	{
		QTemporaryDir temporary;
		auto          pending              = std::make_shared<std::atomic_bool>(true);
		auto          options              = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot        = e2e::private_data_root(temporary);
		options.RegisteredDeviceCourse     = true;
		options.IndependentWindowReads     = true;
		options.DeviceAuthorizationPending = pending;
		auto application                   = std::unique_ptr<app::github_application>{ };
		auto terminal                      = false;
		auto startup                       = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(application != nullptr, "application startup succeeds");
		auto& window = application->window();
		window.show();

		// Signed in through the browser way: the composed device flow.
		auto* const setup = window.account_setup();
		wait_for([&] { return setup != nullptr && setup->isVisible(); }, "Home shows the account setup");
		press(*setup, QStringLiteral("accountSetupServerContinue"));
		press(*setup, QStringLiteral("accountSetupContinue"));
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::awaiting_browser; }, "the device flow's code is shown");
		pending->store(false);
		wait_for([&] { return setup->projection().State.Phase == core::account_setup_phase::connected; }, "the browser finishing signs in");
		const auto login = setup->projection().State.Login;
		expect(window.session_panel()->status() == core::github_status::session_ready && !login.empty(), "the session is live under a login");
		press(*setup, QStringLiteral("accountSetupFinish"));

		// Settings › Profiles › Export Profile Settings…, every section ticked.
		const auto file = temporary.filePath(QStringLiteral("profile-settings.bpr-config.json"));
		qputenv("BPR_ATTENDED_CONFIGURATION_FILE", file.toUtf8());
		window.present_profiles_settings();
		expect(window.settings()->current_page() == qt::settings_page::profiles, "Manage Profiles… opens Settings on Profiles");
		auto* const share = window.profiles_settings();
		expect(share->export_button()->isVisible() && share->export_button()->text() == QStringLiteral("Export Profile Settings…"), "Export Profile Settings… is on the page");
		share->export_button()->click();
		auto* exporter = static_cast<qt::agent_configuration_export_dialog*>(nullptr);
		wait_for([&] {
			for (auto* const widget : QApplication::topLevelWidgets()) {
				if (auto* const found = dynamic_cast<qt::agent_configuration_export_dialog*>(widget); found != nullptr && found->isVisible()) {
					exporter = found;
				}
			}
			return exporter != nullptr;
		},
				 "the export dialog opens");
		// Every section ticked but Prompts, which keeps its own start (edited
		// prompts only): a shipped prompt's text is ours, not the profile's,
		// and may say anything, "token" included.
		for (std::size_t section = 0U; section < core::profile_settings_section_count; ++section) {
			if (static_cast<core::profile_settings_section>(section) == core::profile_settings_section::prompts) {
				continue;
			}
			if (auto* const row = exporter->section(static_cast<core::profile_settings_section>(section)); row != nullptr && row->flags().testFlag(Qt::ItemIsUserCheckable)) {
				row->setCheckState(0, Qt::Checked);
			}
		}
		expect(exporter->identity_chosen(), "Identity is ticked for this export");
		exporter->export_button()->click();
		wait_for([&] { return share->exchange_note()->text().startsWith(QStringLiteral("Exported")); }, "the page says the file was written");

		auto read = QFile{ file };
		expect(read.open(QIODevice::ReadOnly), "the exported file reads back");
		const auto bytes = read.readAll().toStdString();
		std::cout << "PROFILE_SETTINGS_EXPORT:bytes=" << bytes.size() << '\n';
		expect(bytes.find("\"identity\"") != std::string::npos && bytes.find("\"login\": \"" + login + "\"") != std::string::npos && bytes.find("\"host\": \"github.com\"") != std::string::npos,
			   "Identity names the signed-in login and its host");
		// No secret of any kind. The fixture's grant is
		// fixture-access-token-0276 and fixture-refresh-token-0276, from the
		// device code fixture-device-code-0276; a GitHub token starts gh?_ or
		// github_pat_; no key in the format says token, secret or password.
		const auto text = lower(bytes);
		for (const auto* const secret : std::array{ "fixture-access-token", "fixture-refresh-token", "fixture-device-code", "token", "secret", "password", "bearer", "cookie", "keychain",
													"credential", "gho_", "ghp_", "ghu_", "ghs_", "ghr_", "github_pat_", "oauth", "authorization", "hosts.yml" }) {
			if (text.find(secret) != std::string::npos) {
				std::cerr << "secret material in the export: " << secret << '\n';
			}
			expect(text.find(secret) == std::string::npos, "no credential is written into Profile Settings");
		}
		// And it reads back as the one format.
		expect(core::decode_agent_configuration(bytes, { }).has_value(), "the file is Profile Settings");

		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "the hosts quiesce");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	export_with_a_live_sign_in();
	std::cout << "profile-settings-export-public-path-tests:pass\n";
	return EXIT_SUCCESS;
}
