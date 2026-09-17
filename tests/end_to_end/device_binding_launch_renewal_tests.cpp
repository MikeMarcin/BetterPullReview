// [[BPR-0644]]. A stored device grant whose access lifetime elapsed while the
// application was closed is renewed at launch with its refresh material, and
// only a refresh-expired or refused grant is tombstoned. Every lifetime below
// is a complete composition over the same application data root; the runtime
// fixture vault object is the only thing that survives, and no token is
// serialized anywhere.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;

	// The recorded grant: eight hours of access, 184 days of refresh.
	constexpr std::int64_t access_lifetime_seconds  = 28'800;
	constexpr std::int64_t refresh_lifetime_seconds = 15'897'600;

	void expect(bool aCondition, std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, std::string_view aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	void settle(int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	auto launch(const app::detail::github_public_path_fixture_options& aOptions) -> std::unique_ptr<app::github_application>
	{
		std::unique_ptr<app::github_application> application;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, aOptions, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(application != nullptr, "application startup succeeds");
		return application;
	}
	void shutdown(app::github_application& aApp)
	{
		aApp.begin_shutdown();
		wait_for([&aApp] { return aApp.status().ShutdownComplete; }, "all composed hosts quiesce");
	}
	auto seed(app::detail::github_public_path_fixture_options aOptions) -> app::detail::github_public_path_fixture_evidence
	{
		aOptions.DeviceWallOffsetSeconds = 0;
		auto first                       = launch(aOptions);
		first->select_device_authorization();
		wait_for([&first] { return github_application_fixture_access::device_binding_durable(*first); }, "the seeding lifetime persists a settled grant");
		shutdown(*first);
		const auto evidence = github_application_fixture_access::evidence(*first);
		expect(evidence.SecretRetained && evidence.RefreshRetained, "orderly shutdown retains the accepted device pair");
		return evidence;
	}
	// A signed-out launch offers the device flow, not a token prompt: the
	// GitHub.com card's sign-in is the device flow and the panel's Begin is
	// enabled.
	void expect_device_sign_in_offered(app::github_application& aApp, std::string_view aMessage)
	{
		aApp.window().open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		qt::server_card* github = nullptr;
		for (auto* const card : aApp.window().server_cards()) {
			if (card->offers_device_sign_in()) {
				github = card;
			}
		}
		expect(github != nullptr && !github->signed_in(), aMessage);
		auto* const panel = aApp.window().device_authorization();
		expect(panel != nullptr && panel->begin_button() != nullptr && panel->begin_button()->isEnabled(), aMessage);
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qt{ aArgc, aArgv };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private temporary application root");
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot = app::native_filesystem_path(QFileInfo{ temporary.path() }.canonicalFilePath());
	expect(adapters::sqlite::make_storage_root_owner_private(*options.ApplicationDataRoot), "private desktop test root under the canonical storage policy");
	options.RegisteredDeviceCourse = true;
	options.PersistDeviceBindings  = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();

	const auto seeded = seed(options);

	// Criterion 1. Ten minutes past the access expiry, months inside the
	// refresh lifetime: the launch spends the refresh material through the
	// renewal path, stores the rotated pair through the vault, and the session
	// resumes without a code, a pasted token, or any gesture.
	options.DeviceWallOffsetSeconds = access_lifetime_seconds + 600;
	{
		auto renewed = launch(options);
		wait_for([&renewed] { return renewed->status().DeviceAuthorizationRefreshRotations == 1U && github_application_fixture_access::device_binding_durable(*renewed); },
				 "an access-expired grant rotates at launch and its binding is durable again");
		wait_for([&renewed] { return renewed->window().session_panel()->status() == core::github_status::session_ready; }, "the rotated grant establishes the session");
		wait_for([&renewed] { return renewed->status().DeviceAuthorizationRefreshResumes == 1U; }, "renewal is armed for the rotated grant");
		const auto evidence = github_application_fixture_access::evidence(*renewed);
		expect(evidence.DeviceTransportSubmissions == 1U, "exactly one token-endpoint submission: the renewal, never a device code");
		expect(evidence.VaultStores == seeded.VaultStores + 1U && evidence.VaultStoredDigest != seeded.VaultStoredDigest, "the rotated access token is stored through the vault");
		expect(evidence.RefreshStores == seeded.RefreshStores + 1U && evidence.RefreshStoredDigest != seeded.RefreshStoredDigest, "the rotated refresh material replaces the spent material");
		expect(evidence.LastEnrollmentDigest.empty(), "no pasted credential was taken");
		expect(renewed->status().DeviceAuthorizationStage == core::device_authorization_stage::idle, "no device flow was started");
		shutdown(*renewed);
		const auto closed = github_application_fixture_access::evidence(*renewed);
		expect(closed.SecretRetained && closed.RefreshRetained, "the rotated pair is retained across an orderly quit");
	}
	// The rotated binding carries the rotated lifetime: the same clock that
	// found the old access expired now finds eight fresh hours.
	{
		auto resumed = launch(options);
		wait_for([&resumed] { return resumed->status().DeviceAuthorizationRefreshResumes == 1U; }, "the rotated binding resumes on the next launch");
		expect(resumed->status().DeviceAuthorizationRefreshRotations == 0U, "a fresh rotated grant is not rotated again");
		expect(github_application_fixture_access::evidence(*resumed).DeviceTransportSubmissions == 0U, "the resume sends nothing to the token endpoint");
		shutdown(*resumed);
	}

	// Criterion 2, refresh-expired. The stored refresh lifetime has elapsed,
	// so nothing is spent: the binding is tombstoned and the device flow is
	// offered.
	options.DeviceWallOffsetSeconds = access_lifetime_seconds + 600 + refresh_lifetime_seconds + 600;
	{
		auto expired = launch(options);
		wait_for([&expired] { return github_application_fixture_access::device_binding_store_idle(*expired); }, "the refresh-expired launch completes its metadata work");
		settle(500);
		expect(expired->window().session_panel()->status() == core::github_status::no_profile, "a refresh-expired grant does not establish a session");
		expect(github_application_fixture_access::evidence(*expired).DeviceTransportSubmissions == 0U, "refresh-expired material is never sent");
		expect(expired->status().DeviceAuthorizationRefreshResumes == 0U, "a refresh-expired grant does not resume");
		expect_device_sign_in_offered(*expired, "a refresh-expired launch offers the device sign-in with Begin enabled");
		shutdown(*expired);
	}
	{
		auto after = launch(options);
		wait_for([&after] { return github_application_fixture_access::device_binding_store_idle(*after); }, "the launch after a refresh-expired tombstone completes its metadata work");
		settle(500);
		expect(after->window().session_panel()->status() == core::github_status::no_profile && after->status().DeviceAuthorizationRefreshResumes == 0U,
			   "a tombstoned binding stays signed out");
		expect(github_application_fixture_access::evidence(*after).DeviceTransportSubmissions == 0U, "a tombstoned binding is never renewed");
		expect_device_sign_in_offered(*after, "a tombstoned binding offers the device sign-in with Begin enabled");
		shutdown(*after);
	}

	// Criterion 2, server-rejected. A fresh grant, a launch after its access
	// expired, and a token endpoint that refuses the refresh material.
	options.SharedDeviceVault = app::detail::make_shared_github_fixture_vault();
	static_cast<void>(seed(options));
	options.DeviceWallOffsetSeconds = access_lifetime_seconds + 600;
	options.RejectDeviceRefresh     = true;
	{
		auto rejected = launch(options);
		wait_for([&rejected] { return github_application_fixture_access::evidence(*rejected).DeviceTransportSubmissions == 1U; }, "the launch spends the refresh material once");
		wait_for([&rejected] { return github_application_fixture_access::device_binding_store_idle(*rejected); }, "the rejected launch completes its metadata work");
		settle(500);
		expect(rejected->window().session_panel()->status() == core::github_status::session_failed, "a rejected renewal fails the held session instead of establishing it");
		expect(rejected->status().DeviceAuthorizationRefreshRotations == 0U, "a rejected renewal rotates nothing");
		expect_device_sign_in_offered(*rejected, "a rejected launch offers the device sign-in with Begin enabled");
		shutdown(*rejected);
	}
	options.RejectDeviceRefresh = false;
	{
		auto after = launch(options);
		wait_for([&after] { return github_application_fixture_access::device_binding_store_idle(*after); }, "the launch after a rejected renewal completes its metadata work");
		settle(500);
		expect(github_application_fixture_access::evidence(*after).DeviceTransportSubmissions == 0U, "refused refresh material is never replayed on a later launch");
		expect(after->window().session_panel()->status() == core::github_status::no_profile, "the rejected binding stays tombstoned");
		// Begin, not a pasted token: the offered gesture reaches the device flow.
		expect_device_sign_in_offered(*after, "the launch after a rejected renewal offers the device sign-in");
		after->window().device_authorization()->begin_button()->click();
		wait_for([&after] { return github_application_fixture_access::device_binding_durable(*after); }, "Begin reaches a fresh device grant");
		expect(github_application_fixture_access::evidence(*after).LastEnrollmentDigest.empty(), "Begin took no pasted credential");
		shutdown(*after);
	}
	std::cout << "device-binding-launch-renewal-tests:pass\n";
}
