#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	void trigger(qt::workbench_window& aWindow, const char* aName)
	{
		auto* action = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(action != nullptr, "public action exists");
		wait_for([&] { return action->isEnabled(); }, "public action enabled");
		action->trigger();
	}
	void submit(qt::workbench_window& aWindow, int aNumber)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		aWindow.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/%1").arg(aNumber));
		aWindow.home_open_action()->trigger();
	}
	void accepted(app::github_application& aOwner, core::bpr_window_context aContext, unsigned aNumber)
	{
		wait_for([&] { const auto snapshot = github_application_fixture_access::large_review_snapshot(aOwner, aContext);
            return snapshot && snapshot->PullRequest.Number.value() == aNumber; }, "exact window accepts expected PR");
		expect(!aOwner.window(aContext)->showing_home(), "accepted review is visible");
	}
	auto read_vault() -> std::string
	{
		std::array<char, 2049U> bytes{ };
		std::cin.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		const auto size = std::cin.gcount();
		expect(size <= 2048, "vault input bounded");
		return { bytes.data(), static_cast<std::size_t>(size) };
	}
	void check_vault_protocol(std::string_view aBytes)
	{
		expect(app::detail::export_github_fixture_vault(nullptr).empty(), "absent fixture vault exports no material");
		expect(!app::detail::import_github_fixture_vault(std::string(2049U, '0')), "oversized vault refused");
		if (aBytes.empty()) {
			return;
		}
		expect(!app::detail::import_github_fixture_vault(aBytes.substr(0U, aBytes.size() - 2U)), "truncated slot refused");
		auto hostile = std::string{ aBytes };
		// Seven uint64 fields precede the first bounded slot length.
		hostile.replace(112U, 8U, "7fffffff");
		expect(!app::detail::import_github_fixture_vault(hostile), "embedded oversized slot length refused before allocation");
	}
	void capture(qt::workbench_window& aWindow, const std::string& aName)
	{
		const auto root = qEnvironmentVariable("BPR_REVIEW_RESTORATION_ARTIFACT_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "artifact directory available");
			expect(aWindow.grab().save(root + QLatin1Char{ '/' } + QString::fromStdString(aName) + QStringLiteral(".png")), "full-window restoration raster saved");
		}
	}
	void expect_no_vault_attempts(const app::detail::github_public_path_fixture_evidence& aEvidence)
	{
		expect(aEvidence.VaultStoreAttempts == 0U && aEvidence.VaultRetrieveAttempts == 0U && aEvidence.VaultDeleteAttempts == 0U
				   && aEvidence.RefreshStoreAttempts == 0U && aEvidence.RefreshRetrieveAttempts == 0U && aEvidence.RefreshDeleteAttempts == 0U,
			   "no vault request reaches any admission boundary, including failed retrieval");
	}
	void expect_no_account_attempts(const app::detail::github_public_path_fixture_evidence& aEvidence)
	{
		expect(aEvidence.AccountFactoryAttempts == 0U && aEvidence.BootstrapFactoryAttempts == 0U
				   && aEvidence.AccountFactoryCalls == 0U && aEvidence.BootstrapFactoryCalls == 0U && aEvidence.AccountReadCount == 0,
			   "zero account/bootstrap factory attempts and accepted calls");
	}
	void expect_anonymous(app::github_application& aOwner, bool aRead)
	{
		const auto status   = aOwner.status();
		const auto evidence = github_application_fixture_access::evidence(aOwner);
		expect_no_vault_attempts(evidence);
		expect_no_account_attempts(evidence);
		expect((!aRead || status.PublicPreviewRequests > 0U) && status.PublicPreviewVaultEffects == 0U && status.PublicPreviewHeaderEffects == 0U, "anonymous path has zero vault/header effects");
		expect(!evidence.AccountTransportSelected && !evidence.BootstrapTransportSelected && evidence.VaultStores == 0U && evidence.DeviceTransportSubmissions == 0U, "anonymous path selects no account/bootstrap/enrollment transport");
	}
	void seed_auth(app::github_application& aOwner, core::bpr_window_context aFirst)
	{
		auto* first = aOwner.window(aFirst);
		submit(*first, 17);
		auto* signIn = first->findChild<QPushButton*>(QStringLiteral("homeSetupContinue"));
		expect(signIn && signIn->isVisible(), "fresh destination offers sign-in");
		signIn->click();
		accepted(aOwner, aFirst, 17);
		wait_for([&] { return github_application_fixture_access::device_binding_durable(aOwner); }, "remembered grant settles before Quit");
		trigger(*first, "newBprWindowAction");
		wait_for([&] { return aOwner.windows().size() == 2U; }, "second stable window created");
		const auto windows = aOwner.windows();
		const auto second  = windows.front() == aFirst ? windows.back() : windows.front();
		expect(aOwner.window(second)->showing_home() && aOwner.window(second)->home_open_notice()->text().isEmpty(), "fresh sibling has no fabricated destination");
		submit(*aOwner.window(second), 18);
		accepted(aOwner, second, 18);
		aOwner.window(second)->home_action()->trigger();
		expect(aOwner.window(second)->showing_home(), "Home usable over accepted destination");
		expect(github_application_fixture_access::evidence(aOwner).VaultStores == 1U, "two windows share one enrollment");
	}
	void check_contexts(app::github_application& aOwner, std::uint64_t aGeneration)
	{
		for (const auto context : aOwner.windows()) {
			expect(context.Generation.value() == aGeneration, "restart advances exact stable generation");
			if (aGeneration > 1U) {
				auto stale       = context;
				stale.Generation = core::bpr_window_generation{ aGeneration - 1U };
				expect(aOwner.window(stale) == nullptr, "previous process generation refused");
			}
		}
	}
}
auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 3, "phase and root required");
	const auto phase      = std::string{ aArgv[1] };
	const auto root       = std::filesystem::canonical(aArgv[2]);
	const auto anonymous  = phase.starts_with("anonymous") || phase == "capability";
	const auto seed       = phase.ends_with("seed");
	const auto failure    = phase == "missing-grant" || phase == "missing-server" || phase == "corrupt" || phase == "future" || phase == "capability" || phase == "failed-read";
	const auto vaultBytes = read_vault();
	check_vault_protocol(vaultBytes);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot        = root;
	options.PublicPreviewEnabledCourse = anonymous && phase != "capability";
	options.IndependentWindowReads     = true;
	options.RepeatReadSessions         = true;
	options.RegisteredDeviceCourse     = !anonymous;
	options.PersistDeviceBindings      = true;
	options.SharedDeviceVault          = vaultBytes.empty() ? app::detail::make_shared_github_fixture_vault() : app::detail::import_github_fixture_vault(vaultBytes);
	expect(options.SharedDeviceVault != nullptr, "retained synthetic vault decodes");
	app::github_application* owner    = nullptr;
	bool                     terminal = false;
	auto                     desktop  = github_application_fixture_access::start_desktop(phase == "capability" ? app::detail::github_public_path_fixture::ghes_3_18 : app::detail::github_public_path_fixture::github_com, options,
																						 [&](auto aResult) { terminal = true; if(aResult) { owner = *aResult; } });
	wait_for([&] { return terminal; }, "desktop startup finishes");
	expect(owner != nullptr, "desktop admitted");
	if (phase == "auth-stale" || phase == "auth-home") {
		github_application_fixture_access::hold_window_read_terminal(*owner, { core::default_bpr_profile_id, core::bpr_window_id{ 1U }, core::bpr_window_generation{ 2U } }, true);
	}
	const auto expectedCount = anonymous || phase == "auth-verify" || seed ? 1U : 2U;
	wait_for([&] { return owner->windows().size() == expectedCount; }, "exact retained window count");
	const auto generation = seed ? 1U : phase == "auth-close" ? 3U
		: phase == "auth-verify"                              ? 4U
															  : 2U;
	check_contexts(*owner, generation);
	const auto firstContext = core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ phase == "auth-verify" ? 2U : 1U }, core::bpr_window_generation{ generation } };
	auto*      first        = owner->window(firstContext);
	expect(first != nullptr, "stable first identity restored");
	if (seed) {
		expect(first->showing_home() && first->home_open_notice()->text().isEmpty(), "fresh window is Home without resume target");
		if (anonymous) {
			owner->activate_public_preview();
			wait_for([&] { return owner->status().PublicPreviewSignedOut; }, "anonymous authority accepted");
			first->session_panel()->owner_editor()->setText(QStringLiteral("octo-org"));
			first->session_panel()->repository_editor()->setText(QStringLiteral("review-repository"));
			first->session_panel()->pull_request_editor()->setValue(17);
			wait_for([&] { return first->github_open_action()->isEnabled(); }, "anonymous open enabled");
			first->github_open_action()->trigger();
			accepted(*owner, firstContext, 17);
		} else {
			seed_auth(*owner, firstContext);
		}
	} else if (failure) {
		wait_for([&] { const auto text = first->home_open_notice()->text(); return !text.isEmpty() && !text.startsWith(QStringLiteral("Restoring")); }, "restoration failure explicitly visible");
		std::cerr << "restoration-error=" << first->home_open_notice()->text().toStdString() << '\n';
		expect(first->showing_home() && first->home_open_bar()->isEnabled(), "failed restore leaves usable Home");
		expect(!github_application_fixture_access::large_review_snapshot(*owner, firstContext), "failed restore fabricates no content");
	} else if (phase == "auth-stale" || phase == "auth-home") {
		wait_for([&] { return github_application_fixture_access::held_window_read_terminals(*owner, firstContext) > 0U; }, "restored read terminal is held before acceptance");
		expect(first->showing_home(), "pending restoration leaves Home available for user navigation");
		if (phase == "auth-home") {
			first->home_action()->trigger();
			std::cerr << "home-after-gesture=" << first->showing_home() << '\n';
			github_application_fixture_access::hold_window_read_terminal(*owner, firstContext, false);
			// The read lane posts released terminals as queued MetaCalls on this
			// thread. A later queued barrier observes their actual delivery.
			bool delivered = false;
			QMetaObject::invokeMethod(&application, [&] { delivered = true; }, Qt::QueuedConnection);
			wait_for([&] { return delivered; }, "released read terminal reaches decision lane after Home gesture");
			const auto snapshot = github_application_fixture_access::large_review_snapshot(*owner, firstContext);
			std::cerr << "home-after-terminal=" << first->showing_home() << " accepted-snapshot=" << bool(snapshot) << '\n';
			expect(first->showing_home() && !snapshot, "Home cancels pending restoration and refuses its late content");
		} else {
			submit(*first, 18);
			github_application_fixture_access::hold_window_read_terminal(*owner, firstContext, false);
			accepted(*owner, firstContext, 18);
			expect(first->home_open_notice()->text().isEmpty(), "late restoration completion does not replace user navigation or notice");
		}
	} else if (anonymous) {
		accepted(*owner, firstContext, 17);
	} else {
		accepted(*owner, firstContext, phase == "auth-restore" ? 17 : 18);
		if (phase == "auth-restore") {
			capture(*first, "restored-auth-pr17");
		}
		if (phase != "auth-verify") {
			const auto second = core::bpr_window_context{ firstContext.Profile, core::bpr_window_id{ 2U }, firstContext.Generation };
			accepted(*owner, second, 18);
			if (phase == "auth-restore") {
				submit(*first, 18);
				accepted(*owner, firstContext, 18);
				submit(*first, 99);
				wait_for([&] { return first->session_panel()->status() == core::github_status::pull_request_failed; }, "failed attempted navigation terminates");
			} else {
				first->close();
				wait_for([&] { return owner->windows().size() == 1U; }, "Close retires exact first window");
				expect(owner->window(firstContext) == nullptr, "closed context retired");
				first = owner->window(second);
			}
		} else {
			trigger(*first, "newBprWindowAction");
			wait_for([&] { return owner->windows().size() == 2U; }, "fresh window after restart");
			const auto fresh = core::bpr_window_context{ firstContext.Profile, core::bpr_window_id{ 3U }, core::bpr_window_generation{ 1U } };
			expect(owner->window(fresh) && owner->window(fresh)->showing_home() && owner->window(fresh)->home_open_notice()->text().isEmpty(), "monotonic new identity stays Home without destination");
		}
	}
	if (anonymous) {
		expect_anonymous(*owner, !failure);
	}
	if (!seed) {
		const auto evidence = github_application_fixture_access::evidence(*owner);
		expect(evidence.VaultStores == 0U && evidence.DeviceTransportSubmissions == 0U, "restart never enrolls or exchanges tokens");
		if (failure && phase != "failed-read") {
			expect_no_account_attempts(evidence);
			if (phase != "missing-grant") {
				expect_no_vault_attempts(evidence);
			} else {
				expect(evidence.VaultRetrieveAttempts > 0U && !evidence.CredentialRetrieved,
					   "negative control: empty-vault failed lookup increments raw admission attempts");
			}
		}
		if (!anonymous && !failure) {
			expect(evidence.CredentialRetrieved && evidence.AccountTransportSelected, "remembered binding revalidated through retained vault/account");
		}
	}
	capture(*first, phase);
	trigger(*first, "quitProfilesAction");
	wait_for([&] { return desktop->is_quiescent(); }, "Quit flushes latest destinations and drains services");
	desktop.reset();
	// stdout is a dedicated binary pipe to parent memory, never a diagnostic log.
	const auto exported = app::detail::export_github_fixture_vault(options.SharedDeviceVault);
	expect(exported.size() <= 2048U, "vault output bounded");
	if (!anonymous && !failure) {
		expect(!exported.empty(), "orderly Quit retained synthetic grant");
	}
	std::cout.write(exported.data(), static_cast<std::streamsize>(exported.size()));
	std::cerr << "profile-window-review-restoration:" << phase << ":pass\n";
	return EXIT_SUCCESS;
}
