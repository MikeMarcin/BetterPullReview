#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/release_update_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/app/release_update_desktop.hpp"
#include "bpr/qt/release_update_panel.hpp"
#include "bpr/qt/settings_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr;
	using phase   = core::release_update_phase;
	using command = core::release_update_command;
	using reason  = core::release_update_reason;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void until(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < 12000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	struct clock final : application::release_update_clock_port
	{
		core::release_update_time Time{ 100000, 100U };
		auto                      now() noexcept -> core::release_update_time override { return Time; }
	};
	struct access final : application::release_update_access_port
	{
		std::atomic<unsigned> Checks = 0U, Downloads = 0U;
		std::atomic<bool>     Revoked = false;
		reason                Failure = reason::none;
		auto                  binding() const noexcept -> core::release_update_access_id override { return core::release_update_access_id{ Revoked ? 0U : 7U }; }
		auto                  kind() const noexcept -> core::release_update_access_kind override { return core::release_update_access_kind::native_github_com; }
		auto                  latest(std::stop_token) -> application::release_update_check_result override
		{
			++Checks;
			return { Failure, { core::release_update_source::better_pull_review_github_com, core::release_update_release_id{ 10U }, "v2.3.4", false, false, { { core::release_update_asset_id{ 20U }, "BetterPullReview-2.3.4-macos-universal.zip", 100U, { }, true } } }, 0U, "<b>Fixture private release notes</b>" };
		}
		auto download(const core::release_update_candidate&     aCandidate, std::stop_token,
					  const std::function<void(std::uint64_t)>& aProgress) -> application::release_update_download_result override
		{
			++Downloads;
			aProgress(50U);
			aProgress(100U);
			return { reason::none, std::make_shared<application::release_update_archive>(aCandidate), 100U, 0U };
		}
	};
	struct verifier final : application::release_update_verification_port
	{
		std::atomic<unsigned> Calls = 0U;
		auto                  verify(std::shared_ptr<const application::release_update_archive> aArchive, std::stop_token) -> application::release_update_verification_result override
		{
			++Calls;
			return { reason::none, std::make_shared<application::release_update_verified_bundle>(aArchive->candidate()) };
		}
	};
	auto root(QTemporaryDir& aTemporary) -> std::filesystem::path
	{
		expect(aTemporary.isValid(), "temporary root exists");
		const auto path = std::filesystem::path{ QFileInfo{ aTemporary.path() }.canonicalFilePath().toStdString() };
		expect(adapters::sqlite::make_storage_root_owner_private(path), "private fixture state root");
		return path;
	}
	void root_wiring()
	{
		QTemporaryDir                                   temporary;
		app::detail::github_public_path_fixture_options options;
		options.ApplicationDataRoot      = root(temporary);
		app::github_application* owner   = nullptr;
		auto                     desktop = app::detail::github_application_fixture_access::start_desktop(
			app::detail::github_public_path_fixture::profile_window_restoration_v1, options,
			[&](auto aResult) { if (aResult) { owner = *aResult; } });
		until([&] { return owner && !owner->windows().empty(); }, "production desktop starts");
		auto* window = owner->window(owner->windows().front());
		auto* action = window->findChild<QAction*>(QStringLiteral("checkForUpdatesAction"));
		expect(action, "production root composes the menu action");
		action->trigger();
		window->open_settings(qt::settings_page::general);
		until([&] { return window->settings() && window->settings()->findChild<QWidget*>(QStringLiteral("releaseUpdatePanel")); }, "production root composes Settings");
		auto* status = window->settings()->findChild<QLabel*>(QStringLiteral("releaseUpdateStatus"));
		until([&] { return status && !status->text().isEmpty(); }, "native unsupported/development projection arrives");
		desktop->quit();
		until([&] { return desktop->is_quiescent(); }, "root updater participates in orderly desktop drain");
	}
	void gestures_restart_and_staging()
	{
		QTemporaryDir                        temporary;
		const auto                           stateRoot = root(temporary);
		const auto                           store     = adapters::sqlite::make_release_update_store(stateRoot);
		auto                                 time      = std::make_shared<clock>();
		auto                                 wire      = std::make_shared<access>();
		auto                                 validator = std::make_shared<verifier>();
		application::release_update_settings initial;
		initial.Preferences.AutomaticChecks = false;
		expect(store->save(initial), "initial app-wide preferences saved");
		std::string          browser;
		auto                 start   = [&] { return app::release_update_desktop::start({ "2.3.3", core::release_update_capability::eligible, time, store, validator, { }, [&](std::string aAddress) { browser = std::move(aAddress); } }); };
		auto                 desktop = start();
		qt::workbench_window window;
		qt::settings_window  settings;
		desktop->attach(window);
		desktop->attach(settings);
		const auto                        binding = std::string(64U, 'a');
		const app::release_update_account account{ binding, "Fixture account", [wire](auto, auto, auto aDone) { aDone(wire); } };
		desktop->set_accounts({ account });
		until([&] { return desktop->projection().Installed.has_value(); }, "settings loaded before gestures");
		auto* panel = dynamic_cast<qt::release_update_panel*>(settings.findChild<QWidget*>(QStringLiteral("releaseUpdatePanel")));
		expect(panel, "Settings owns live update panel");
		auto* accounts = panel->findChild<QComboBox*>(QStringLiteral("releaseUpdateAccount"));
		expect(accounts && accounts->count() == 2, "explicit release-account choice");
		accounts->setCurrentIndex(1);
		accounts->activated(1);
		auto* menu = window.findChild<QAction*>(QStringLiteral("checkForUpdatesAction"));
		expect(menu, "public menu entry exists");
		menu->trigger();
		until([&] { return desktop->projection().Phase == phase::available; }, "menu reaches host and accepted offer");
		expect(wire->Checks == 1U && desktop->panel(), "one manual request and app-wide modeless offer");
		const auto screenshot = qEnvironmentVariable("BPR_UPDATE_SCREENSHOT_PATH");
		if (!screenshot.isEmpty()) {
			expect(desktop->panel()->grab().save(screenshot), "fixture-only offer capture");
		}
		expect(desktop->projection().Preferences.LastSuccess == time->Time.WallSeconds, "successful timestamp projected");
		const auto* notes = panel->findChild<QPlainTextEdit*>(QStringLiteral("releaseUpdateNotes"));
		expect(notes && notes->isReadOnly() && notes->toPlainText() == QStringLiteral("<b>Fixture private release notes</b>"), "release notes remain literal plain text");
		panel->action(command::later)->trigger();
		expect(!desktop->projection().OfferVisible, "Later hides current offer");
		panel->action(command::check)->trigger();
		until([&] { return desktop->projection().OfferVisible; }, "manual check reoffers deferred version");
		panel->action(command::skip)->trigger();
		auto* automatic = panel->findChild<QCheckBox*>(QStringLiteral("releaseUpdateAutomatic"));
		automatic->click();
		desktop->shutdown();
		until([&] { return desktop->quiescent(); }, "preferences flush before shutdown");
		desktop.reset();
		const auto persisted = store->load();
		expect(persisted && persisted->AccountBinding == binding && persisted->Preferences.SkippedVersion && persisted->Preferences.AutomaticChecks, "opaque account, skip and automatic preference persist");
		desktop = start();
		QMenuBar menuBar;
		desktop->attach(menuBar);
		desktop->set_accounts({ account });
		until([&] { return desktop->projection().Installed && desktop->quiescent(); }, "restart preferences restored");
		const auto previous = wire->Checks.load();
		time->Time.WallSeconds += 90000;
		time->Time.MonotonicSeconds += 90000U;
		until([&] { return wire->Checks > previous && desktop->quiescent(); }, "daily timer checks once after restart");
		expect(!desktop->projection().OfferVisible && desktop->projection().Reason == reason::skipped, "automatic check respects persisted skip");
		menuBar.findChild<QAction*>(QStringLiteral("checkForUpdatesAction"))->trigger();
		until([&] { return desktop->projection().OfferVisible; }, "manual override ignores skipped version");
		auto* offer = desktop->panel();
		offer->action(command::download)->trigger();
		until([&] { return desktop->projection().Phase == phase::ready; }, "download reaches staged verification and projection");
		expect(wire->Downloads == 1U && validator->Calls == 1U && !offer->action(command::install)->isEnabled(), "verified candidate retained without enabling unimplemented installer");
		wire->Revoked = true;
		until([&] { return !desktop->projection().Candidate; }, "revocation discards staged candidate");
		desktop->shutdown();
		until([&] { return desktop->quiescent(); }, "all fixture effects drained");
		desktop.reset();
	}
	void unavailable_installations()
	{
		const auto binding = std::string(64U, 'a');
		for (const auto capability : { core::release_update_capability::managed, core::release_update_capability::unsupported_platform,
									   core::release_update_capability::untrusted_installation, core::release_update_capability::read_only, core::release_update_capability::translocated, core::release_update_capability::mounted_image }) {
			QTemporaryDir                        temporary;
			const auto                           store = adapters::sqlite::make_release_update_store(root(temporary));
			application::release_update_settings initial;
			initial.AccountBinding = binding;
			expect(store->save(initial), "fallback settings fixture");
			auto                time     = std::make_shared<clock>();
			unsigned            acquired = 0U;
			std::string         browser;
			auto                desktop = app::release_update_desktop::start({ "2.3.3", capability, time, store, { }, { }, [&](std::string aAddress) { browser = std::move(aAddress); } });
			qt::settings_window settings;
			desktop->attach(settings);
			desktop->set_accounts({ { binding, "Fixture account", [&](auto, auto, auto aDone) { ++acquired; aDone({}); } } });
			until([&] { return desktop->projection().Installed.has_value(); }, "fallback projection settles");
			auto* panel = dynamic_cast<qt::release_update_panel*>(settings.findChild<QWidget*>(QStringLiteral("releaseUpdatePanel")));
			expect(panel && !panel->action(command::check)->isEnabled() && !desktop->projection().AutomaticAvailable && desktop->projection().Reason != reason::none, "truthful unavailable controls and reason");
			expect(acquired == 0U, "managed and unsupported installs never acquire credentials");
			panel->findChild<QPushButton*>(QStringLiteral("releaseUpdateViewRelease"))->click();
			expect(browser == "https://github.com/MikeMarcin/BetterPullReview/releases", "manual fallback uses only fixed release browser address");
			desktop->shutdown();
			until([&] { return desktop->quiescent(); }, "fallback service drains");
		}
	}
}
auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QApplication::setApplicationVersion(QStringLiteral("2.3.3"));
	root_wiring();
	gestures_restart_and_staging();
	unavailable_installations();
	std::cout << "release-update-public-path:pass\n";
}
