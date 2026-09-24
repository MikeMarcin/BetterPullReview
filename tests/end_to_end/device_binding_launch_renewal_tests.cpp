// [[BPR-0644]]. A stored device grant whose access lifetime elapsed while the
// application was closed is renewed at launch with its refresh material. Under
// the owner's rule of 2026-09-24 nothing the launch meets removes the grant: an
// offline launch, a refresh-expired grant and a refused one each fail visibly,
// keep both slots and a live row, and offer Begin beside Sign out. The next
// launch tries again; Sign out or a new sign-in is what removes it. Every
// lifetime below is a complete composition over the same application data
// root; the runtime fixture vault object is the only thing that survives, and
// no token is serialized anywhere.
//
// [[BPR-1116]]. The same rule while the application runs: a grant signed in
// during a lifetime whose own renewal gives up, offline or refused, keeps its
// grant the same way, and the next launch restores it. Those lifetimes use a
// data root and a vault of their own.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QString>
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
	void wait_for(const std::function<bool()>& aCondition, std::string_view aMessage, const qint64 aTimeoutMs = 15000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
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
		// [[BPR-0877]]. A refusal here used to say only that startup failed. The
		// profile refusal that actually fired is named beside it, so a seed that
		// cannot start says why in its own output.
		if (application == nullptr) {
			std::cerr << "SEED_STARTUP_PROFILE_REFUSAL:" << bpr::app::profile_startup_refusal_name() << std::endl;
			// [[BPR-0882]]. And which composition arm refused, with what it caught.
			std::cerr << "SEED_STARTUP_FAILURE_SOURCE:" << bpr::app::startup_failure_source() << '\n';
		}
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
	auto github_card(app::github_application& aApp) -> qt::server_card*
	{
		aApp.window().open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		qt::server_card* github = nullptr;
		for (auto* const card : aApp.window().server_cards()) {
			if (card->offers_device_sign_in()) {
				github = card;
			}
		}
		return github;
	}
	void expect_device_sign_in_offered(app::github_application& aApp, std::string_view aMessage)
	{
		auto* const github = github_card(aApp);
		expect(github != nullptr && !github->signed_in(), aMessage);
		auto* const panel = aApp.window().device_authorization();
		expect(panel != nullptr && panel->begin_button() != nullptr && panel->begin_button()->isEnabled(), aMessage);
	}
	// [[BPR-0644]]. The failed launch kept the grant: both slots, no erasure
	// attempted since `aBefore` (the shared fixture vault counts across
	// lifetimes), the card's sentence naming how it ends, and a Sign out
	// beside Begin.
	void expect_grant_kept(app::github_application& aApp, const app::detail::github_public_path_fixture_evidence& aBefore, const qt::kept_grant aKept, std::string_view aMessage)
	{
		const auto evidence = github_application_fixture_access::evidence(aApp);
		expect(evidence.SecretRetained && evidence.RefreshRetained && evidence.VaultDeleteAttempts == aBefore.VaultDeleteAttempts && evidence.RefreshDeleteAttempts == aBefore.RefreshDeleteAttempts, aMessage);
		expect(aApp.window().session_panel()->status() == core::github_status::session_failed, aMessage);
		expect_device_sign_in_offered(aApp, aMessage);
		auto* const github = github_card(aApp);
		expect(github->presentation().KeptGrant == aKept && github->sign_out_button()->isVisibleTo(github) && !github->consequence_label()->text().isEmpty(), aMessage);
		std::cout << "KEPT_GRANT_SENTENCE:" << github->consequence_label()->text().toStdString() << '\n';
		// Rendered evidence, written only when a directory is named, as the
		// other capture courses do: the card as the composed Settings page
		// draws it.
		const auto directory = qEnvironmentVariable("BPR_KEPT_GRANT_CAPTURE_DIR");
		if (directory.isEmpty()) {
			return;
		}
		static auto captures = 0;
		const auto  path     = QStringLiteral("%1/kept-grant-%2.png").arg(directory).arg(++captures);
		expect(github->grab().save(path, "PNG"), "the kept-grant capture is written");
	}
	// [[BPR-1116]]. One lifetime that signs in and whose own renewal then
	// gives up. The renewal course's clock makes the renewal of a grant bound
	// in this lifetime due at once, and `aOptions` says how the token endpoint
	// answers it. Returns the application, still running, once the session has
	// ended and the metadata work is done.
	auto sign_in_until_the_renewal_gives_up(app::detail::github_public_path_fixture_options aOptions, app::detail::github_public_path_fixture_evidence& aBefore) -> std::unique_ptr<app::github_application>
	{
		aOptions.DeviceRefreshCourse     = true;
		aOptions.DeviceWallOffsetSeconds = 0;
		auto running                     = launch(aOptions);
		aBefore                          = github_application_fixture_access::evidence(*running);
		running->select_device_authorization();
		wait_for([&running] { return running->status().DeviceAuthorizationStage == core::device_authorization_stage::authorized; }, "the device sign-in in this lifetime is authorized");
		// The device host retries a network failure three times, five seconds
		// apart, before it gives up; a refusal ends it at once.
		wait_for([&running] { return running->window().session_panel()->status() == core::github_status::session_failed; }, "the renewal gives up and the session ends visibly", 45000);
		wait_for([&running] { return github_application_fixture_access::device_binding_store_idle(*running); }, "the ended session completes its metadata work");
		settle(500);
		expect(running->status().DeviceAuthorizationRefreshRotations == 0U, "the renewal that gave up rotated nothing");
		return running;
	}
	void in_process_renewals(app::detail::github_public_path_fixture_options aOptions)
	{
		aOptions.RejectDeviceRefresh     = false;
		aOptions.DeviceWallOffsetSeconds = 0;
		// An in-process renewal that gives up offline keeps the grant, and the
		// next launch restores it.
		{
			QTemporaryDir root;
			expect(root.isValid(), "private temporary application root for the offline in-process renewal");
			aOptions.ApplicationDataRoot = e2e::private_data_root(root);
			expect(aOptions.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");
			aOptions.SharedDeviceVault   = app::detail::make_shared_github_fixture_vault();
			auto offline                 = aOptions;
			offline.OfflineDeviceRefresh = true;
			auto before                  = app::detail::github_public_path_fixture_evidence{ };
			{
				auto running = sign_in_until_the_renewal_gives_up(offline, before);
				expect_grant_kept(*running, before, qt::kept_grant::retried_next_launch, "an in-process renewal that gives up offline keeps the grant and says the next launch tries again");
				shutdown(*running);
				const auto closed = github_application_fixture_access::evidence(*running);
				expect(closed.SecretRetained && closed.RefreshRetained && closed.VaultDeleteAttempts == before.VaultDeleteAttempts && closed.RefreshDeleteAttempts == before.RefreshDeleteAttempts,
					   "an orderly quit after the in-process renewal gave up keeps the grant");
			}
			auto next = launch(aOptions);
			wait_for([&next] { return next->window().session_panel()->status() == core::github_status::session_ready; }, "the next launch restores the kept grant");
			wait_for([&next] { return next->status().DeviceAuthorizationRefreshResumes == 1U; }, "and arms its renewal from the kept refresh slot");
			const auto evidence = github_application_fixture_access::evidence(*next);
			expect(evidence.DeviceTransportSubmissions == 0U && evidence.LastEnrollmentDigest.empty() && next->status().DeviceAuthorizationStage == core::device_authorization_stage::idle,
				   "the restore took no code, no pasted token and nothing from the token endpoint");
			shutdown(*next);
		}
		// A refused in-process renewal keeps the grant, says it needs signing
		// in again, and offers Begin and Sign out; Sign out removes it.
		{
			QTemporaryDir root;
			expect(root.isValid(), "private temporary application root for the refused in-process renewal");
			aOptions.ApplicationDataRoot = e2e::private_data_root(root);
			expect(aOptions.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");
			aOptions.SharedDeviceVault  = app::detail::make_shared_github_fixture_vault();
			auto refused                = aOptions;
			refused.RejectDeviceRefresh = true;
			auto before                 = app::detail::github_public_path_fixture_evidence{ };
			{
				auto running = sign_in_until_the_renewal_gives_up(refused, before);
				expect_grant_kept(*running, before, qt::kept_grant::needs_sign_in, "a refused in-process renewal keeps the grant, asks for signing in again, and offers Begin and Sign out");
				github_card(*running)->sign_out_button()->click();
				wait_for([&running] { const auto evidence = github_application_fixture_access::evidence(*running); return !evidence.SecretRetained && !evidence.RefreshRetained; },
						 "Sign out on the card erases both slots of the kept grant");
				wait_for([&running] { return github_application_fixture_access::device_binding_store_idle(*running); }, "the sign-out completes its metadata work");
				shutdown(*running);
			}
			auto removed = launch(aOptions);
			wait_for([&removed] { return github_application_fixture_access::device_binding_store_idle(*removed); }, "the launch after Sign out completes its metadata work");
			settle(500);
			expect(removed->window().session_panel()->status() == core::github_status::no_profile, "after Sign out the grant is gone and the launch is signed out");
			expect_device_sign_in_offered(*removed, "and offers the device sign-in with Begin enabled");
			shutdown(*removed);
		}
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qt{ aArgc, aArgv };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private temporary application root");
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot = e2e::private_data_root(temporary);
	expect(options.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");
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
	// The shared fixture vault's counters run across lifetimes; each kept-grant
	// check compares with the last lifetime's closing count.
	auto ledger = app::detail::github_public_path_fixture_evidence{ };
	// The rotated binding carries the rotated lifetime: the same clock that
	// found the old access expired now finds eight fresh hours.
	{
		auto resumed = launch(options);
		wait_for([&resumed] { return resumed->status().DeviceAuthorizationRefreshResumes == 1U; }, "the rotated binding resumes on the next launch");
		expect(resumed->status().DeviceAuthorizationRefreshRotations == 0U, "a fresh rotated grant is not rotated again");
		expect(github_application_fixture_access::evidence(*resumed).DeviceTransportSubmissions == 0U, "the resume sends nothing to the token endpoint");
		shutdown(*resumed);
		ledger = github_application_fixture_access::evidence(*resumed);
	}

	// The policy criterion: an offline launch after the access token expired.
	// Every renewal attempt meets a network failure; the launch says so, keeps
	// the grant, and the next launch online renews it. Before the owner's rule
	// the renewal tombstoned the row before its first attempt, and this launch
	// lost the grant.
	options.DeviceWallOffsetSeconds = (2 * access_lifetime_seconds) + 1200;
	options.OfflineDeviceRefresh    = true;
	{
		auto offline = launch(options);
		// The device host retries a network failure three times, five seconds
		// apart, before it gives up.
		wait_for([&offline] { return offline->window().session_panel()->status() == core::github_status::session_failed; }, "an offline launch renewal fails the held session visibly", 30000);
		wait_for([&offline] { return github_application_fixture_access::device_binding_store_idle(*offline); }, "the offline launch completes its metadata work");
		settle(500);
		expect(github_application_fixture_access::evidence(*offline).DeviceTransportSubmissions >= 1U && offline->status().DeviceAuthorizationRefreshRotations == 0U, "the offline launch tried the renewal and rotated nothing");
		expect_grant_kept(*offline, ledger, qt::kept_grant::retried_next_launch, "an offline launch keeps the grant and says the next launch tries again");
		shutdown(*offline);
		const auto closed = github_application_fixture_access::evidence(*offline);
		expect(closed.SecretRetained && closed.RefreshRetained && closed.VaultDeleteAttempts == ledger.VaultDeleteAttempts, "an orderly quit after an offline launch keeps the grant");
	}
	options.OfflineDeviceRefresh = false;
	{
		auto online = launch(options);
		wait_for([&online] { return online->status().DeviceAuthorizationRefreshRotations == 1U && github_application_fixture_access::device_binding_durable(*online); },
				 "the next launch, online, renews the kept grant");
		wait_for([&online] { return online->window().session_panel()->status() == core::github_status::session_ready; }, "and resumes the session");
		expect(github_application_fixture_access::evidence(*online).LastEnrollmentDigest.empty(), "no pasted credential was taken");
		shutdown(*online);
		ledger = github_application_fixture_access::evidence(*online);
	}

	// Criterion 2, refresh-expired. The stored refresh lifetime has elapsed,
	// so nothing is spent. The grant is kept and the launch says it needs
	// signing in again; Begin and Sign out are both offered.
	options.DeviceWallOffsetSeconds = (3 * access_lifetime_seconds) + 1800 + refresh_lifetime_seconds;
	{
		auto expired = launch(options);
		wait_for([&expired] { return expired->window().session_panel()->status() == core::github_status::session_failed; }, "a refresh-expired grant fails its held session visibly");
		wait_for([&expired] { return github_application_fixture_access::device_binding_store_idle(*expired); }, "the refresh-expired launch completes its metadata work");
		settle(500);
		expect(github_application_fixture_access::evidence(*expired).DeviceTransportSubmissions == 0U, "refresh-expired material is never sent");
		expect(expired->status().DeviceAuthorizationRefreshResumes == 0U, "a refresh-expired grant does not resume");
		expect_grant_kept(*expired, ledger, qt::kept_grant::needs_sign_in, "a refresh-expired launch keeps the grant, asks for signing in again, and offers Begin");
		shutdown(*expired);
		ledger = github_application_fixture_access::evidence(*expired);
	}
	{
		// Still there at the next launch, still saying so; Sign out is the
		// person's way to remove it.
		auto after = launch(options);
		wait_for([&after] { return after->window().session_panel()->status() == core::github_status::session_failed; }, "the kept refresh-expired grant is met again at the next launch");
		settle(500);
		expect_grant_kept(*after, ledger, qt::kept_grant::needs_sign_in, "the next launch keeps it too");
		github_card(*after)->sign_out_button()->click();
		wait_for([&after] { const auto evidence = github_application_fixture_access::evidence(*after); return !evidence.SecretRetained && !evidence.RefreshRetained; },
				 "Sign out on the failed card erases both slots");
		wait_for([&after] { return github_application_fixture_access::device_binding_store_idle(*after); }, "the sign-out completes its metadata work");
		shutdown(*after);
	}
	{
		auto removed = launch(options);
		wait_for([&removed] { return github_application_fixture_access::device_binding_store_idle(*removed); }, "the launch after Sign out completes its metadata work");
		settle(500);
		expect(removed->window().session_panel()->status() == core::github_status::no_profile, "after Sign out the grant is gone and the launch is signed out");
		expect_device_sign_in_offered(*removed, "and offers the device sign-in with Begin enabled");
		shutdown(*removed);
	}

	// Criterion 2, server-rejected. A fresh grant, a launch after its access
	// expired, and a token endpoint that refuses the refresh material. The
	// grant is kept, and the refusal is met again at every launch until the
	// person signs in again or signs out.
	options.SharedDeviceVault       = app::detail::make_shared_github_fixture_vault();
	options.DeviceWallOffsetSeconds = 0;
	ledger                          = seed(options);
	options.DeviceWallOffsetSeconds = access_lifetime_seconds + 600;
	options.RejectDeviceRefresh     = true;
	{
		auto rejected = launch(options);
		wait_for([&rejected] { return github_application_fixture_access::evidence(*rejected).DeviceTransportSubmissions == 1U; }, "the launch spends the refresh material once");
		wait_for([&rejected] { return rejected->window().session_panel()->status() == core::github_status::session_failed; }, "a rejected renewal fails the held session instead of establishing it");
		wait_for([&rejected] { return github_application_fixture_access::device_binding_store_idle(*rejected); }, "the rejected launch completes its metadata work");
		settle(500);
		expect(rejected->status().DeviceAuthorizationRefreshRotations == 0U, "a rejected renewal rotates nothing");
		expect_grant_kept(*rejected, ledger, qt::kept_grant::needs_sign_in, "a rejected launch keeps the grant, asks for signing in again, and offers Begin");
		shutdown(*rejected);
		ledger = github_application_fixture_access::evidence(*rejected);
	}
	{
		auto after = launch(options);
		wait_for([&after] { return after->window().session_panel()->status() == core::github_status::session_failed; }, "the kept refused grant is met again at the next launch");
		wait_for([&after] { return github_application_fixture_access::device_binding_store_idle(*after); }, "the launch after a rejected renewal completes its metadata work");
		settle(500);
		expect_grant_kept(*after, ledger, qt::kept_grant::needs_sign_in, "the next launch keeps it and says so again");
		// Begin, not a pasted token: a new sign-in replaces the kept grant.
		after->window().device_authorization()->begin_button()->click();
		wait_for([&after] { return github_application_fixture_access::device_binding_durable(*after); }, "Begin reaches a fresh device grant that replaces the kept one");
		expect(github_application_fixture_access::evidence(*after).LastEnrollmentDigest.empty(), "Begin took no pasted credential");
		shutdown(*after);
	}

	options.RejectDeviceRefresh = false;
	in_process_renewals(options);
	std::cout << "device-binding-launch-renewal-tests:pass\n";
}
