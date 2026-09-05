#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
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
	void shutdown(app::github_application& aApp)
	{
		aApp.begin_shutdown();
		wait_for([&aApp] { return aApp.status().ShutdownComplete; }, "all composed hosts quiesce");
	}
}
int main(int aArgc, char** aArgv)
{
	QApplication  qt{ aArgc, aArgv };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private temporary application root");
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot    = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
	options.RegisteredDeviceCourse = true;
	options.PersistDeviceBindings  = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	{
		auto first = github_application_fixture_access::make(app::detail::github_public_path_fixture::github_com, options);
		first->select_device_authorization();
		wait_for([&first] { return github_application_fixture_access::device_binding_durable(*first); }, "first lifetime persists a settled grant");
		expect(first->status().DeviceAuthorizationRefreshRotations == 0U, "first lifetime does not rotate early");
		shutdown(*first);
		const auto evidence = github_application_fixture_access::evidence(*first);
		expect(evidence.SecretRetained && evidence.RefreshRetained, "orderly shutdown retains accepted device pair");
	}
	// Simulates a later launch near expiry. Both hosts and the metadata connection are new;
	// only this test's runtime vault object survives. No token is serialized.
	options.DeviceWallOffsetSeconds = 28790;
	{
		auto second = github_application_fixture_access::make(app::detail::github_public_path_fixture::github_com, options);
		wait_for([&second] { return second->status().DeviceAuthorizationRefreshResumes == 1U; }, "fresh hosts resume durable exact binding");
		expect(second->status().DeviceAuthorizationRefreshRotations == 0U, "first refresh remains deferred after resume");
		expect(github_application_fixture_access::evidence(*second).DeviceTransportSubmissions == 0U, "resume sends no immediate token exchange");
		wait_for([&second] { return second->status().DeviceAuthorizationRefreshRotations == 1U && github_application_fixture_access::device_binding_durable(*second); }, "one deferred rotation settles and persists");
		expect(second->status().DeviceAuthorizationRefreshResumes == 1U, "rotation does not count as another startup resume");
		second->window().show();
		QApplication::processEvents();
		expect(!second->window().grab().isNull(), "rendered composed session panel");
		auto* signOut = second->window().findChild<QToolButton*>(QStringLiteral("signOutGithubButton"));
		expect(signOut != nullptr && signOut->isVisible() && signOut->text() == QStringLiteral("Sign out"), "visible Sign out action exists");
		signOut->click();
		wait_for([&second] {const auto evidence=github_application_fixture_access::evidence(*second);return !evidence.SecretRetained && !evidence.RefreshRetained; }, "Sign out exact-deletes both slots");
		shutdown(*second);
	}
	{
		auto third = github_application_fixture_access::make(app::detail::github_public_path_fixture::github_com, options);
		wait_for([&third] { return github_application_fixture_access::device_binding_store_idle(*third); }, "third lifetime completes startup metadata load");
		expect(third->window().session_panel()->status() == core::github_status::no_profile, "completed startup load leaves signed-out identity inactive");
		shutdown(*third);
		expect(third->status().DeviceAuthorizationRefreshResumes == 0U, "third lifetime cannot resume signed-out binding");
		expect(!github_application_fixture_access::evidence(*third).SecretRetained, "third lifetime retains no material");
	}
	{
		auto manual = github_application_fixture_access::make(app::detail::github_public_path_fixture::github_com, options);
		manual->select_device_authorization();
		wait_for([&manual] { return github_application_fixture_access::device_binding_durable(*manual); }, "replacement course earns a durable device grant");
		manual->window().session_panel()->connect_button()->click();
		wait_for([&manual] {const auto evidence=github_application_fixture_access::evidence(*manual);return evidence.SecretRetained && !evidence.RefreshRetained && evidence.VaultStoredDigest==evidence.LastEnrollmentDigest; }, "manual replacement removes device refresh and stores only pasted access");
		expect(!github_application_fixture_access::device_binding_durable(*manual), "manual replacement is session-only");
		shutdown(*manual);
		expect(!github_application_fixture_access::evidence(*manual).SecretRetained, "manual access is deleted on shutdown");
	}
	{
		auto last = github_application_fixture_access::make(app::detail::github_public_path_fixture::github_com, options);
		wait_for([&last] { return github_application_fixture_access::device_binding_store_idle(*last); }, "post-manual startup load completes");
		expect(last->window().session_panel()->status() == core::github_status::no_profile && last->status().DeviceAuthorizationRefreshResumes == 0U, "manual replacement cannot resume after restart");
		shutdown(*last);
	}
	options.DeviceWallOffsetSeconds = 0;
	{
		auto seed = github_application_fixture_access::make(app::detail::github_public_path_fixture::github_com, options);
		seed->select_device_authorization();
		wait_for([&seed] { return github_application_fixture_access::device_binding_durable(*seed); }, "preview regression seeds a remembered grant");
		shutdown(*seed);
	}
	options.DeviceWallOffsetSeconds = 28790;
	{
		auto preview = github_application_fixture_access::make(app::detail::github_public_path_fixture::github_com, options);
		wait_for([&preview] { return preview->status().DeviceAuthorizationRefreshResumes == 1U; }, "preview regression resumes near-expiry device grant");
		preview->activate_public_preview();
		wait_for([&preview] { return preview->window().session_panel()->public_preview_signed_out(); }, "ordinary Public preview selection becomes active");
		QElapsedTimer retiredRenewal;
		retiredRenewal.start();
		wait_for([&retiredRenewal] { return retiredRenewal.elapsed() >= 6000; }, "retired near-expiry renewal deadline passes");
		expect(preview->window().session_panel()->public_preview_signed_out(), "retired device renewal must not deactivate the newly selected Public preview");
		shutdown(*preview);
	}
	std::cout << "REFRESH_RESUMES:1\ndevice-binding-restart-tests:pass\n";
}
