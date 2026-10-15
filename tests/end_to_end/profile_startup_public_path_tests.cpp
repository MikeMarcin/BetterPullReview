#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/settings_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>

#if !defined(_WIN32)
	#include <sys/stat.h>
	#include <sys/types.h>
#endif

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using bpr::tests::expect;
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
	void seed_appearance(const std::filesystem::path& aRoot, const core::appearance_mode aMode)
	{
		auto store = adapters::sqlite::make_appearance_settings_store({ .StorageRoot = aRoot });
		expect(store.has_value(), "seed appearance store");
		auto settings                        = application::appearance_settings{ };
		settings.Mode                        = aMode;
		settings.Typography.Ui.Font.Family   = "Sans Serif";
		settings.Typography.Code.Font.Family = "Monospace";
		expect((*store)->store(settings, { }).has_value(), "seed valid distinct appearance");
	}
	/// \brief [[BPR-1513]]. Switches the open Appearance page through its owning host.
	void test_appearance_banners(app::github_application& aRuntime)
	{
		auto* const panel            = aRuntime.window().appearance_settings();
		auto* const settings         = aRuntime.window().settings();
		const auto  captureDirectory = qEnvironmentVariable("BPR_APPEARANCE_BANNER_CAPTURE_DIR");
		for (const auto mode : { core::appearance_mode::dark, core::appearance_mode::light,
								 core::appearance_mode::high_contrast_dark, core::appearance_mode::high_contrast_light,
								 core::appearance_mode::system, core::appearance_mode::high_contrast_system }) {
			panel->choose_mode(mode);
			wait_for([&] { return aRuntime.status().ProfileMode == mode; }, "Appearance tile reaches the profile host");
			QApplication::processEvents();
			const auto surface  = settings->palette().color(QPalette::Window);
			const auto ink      = settings->palette().color(QPalette::WindowText);
			const auto expected = qt::builtin_appearance_palette(aRuntime.status().ProfileVariant);
			expect(surface == qt::to_presentation_color(expected[core::index_of(core::appearance_role::application_surface)])
					   && ink == qt::to_presentation_color(expected[core::index_of(core::appearance_role::application_text)]),
				   "the open Settings shell receives the selected profile's actual surface and text");
			for (const auto* section : { "Theme", "Type", "Colours", "Dif" }) {
				const auto  prefix = QStringLiteral("appearance%1Card").arg(QString::fromLatin1(section));
				auto* const header = panel->findChild<QWidget*>(prefix + QStringLiteral("Header"));
				auto* const title  = panel->findChild<QLabel*>(prefix + QStringLiteral("Title"));
				expect(header != nullptr && title != nullptr, "every Appearance section has its header and title");
				const auto band     = qt::settings_section_band(header->palette());
				const auto bandPage = qt::contrast_ratio(qt::to_appearance_color(band), qt::to_appearance_color(surface));
				expect(band.rgb() == qt::settings_section_band(settings->palette()).rgb(),
					   "Appearance banners derive their band from the open Settings shell's palette");
				expect(bandPage >= 1.1 && bandPage <= 1.35,
					   "the banner is a distinct band that stays on the surface's side, never a bright strip");
				expect(title->palette().color(title->foregroundRole()) == ink
						   && qt::contrast_ratio(qt::to_appearance_color(ink), qt::to_appearance_color(band)) >= 7.0,
					   "Appearance titles retain readable application text on the band after each live mode change");
				const auto raster = header->grab().toImage();
				expect(!raster.isNull() && raster.pixelColor(5, raster.height() / 2).rgb() == band.rgb(),
					   "the actual painted banner uses the restrained band");
			}
			if (!captureDirectory.isEmpty()) {
				const auto name = core::stable_name(mode);
				expect(panel->grab().save(QStringLiteral("%1/appearance-%2.png").arg(captureDirectory, QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())))),
					   "write the composed Appearance page capture");
			}
		}
		panel->choose_mode(core::appearance_mode::light);
		wait_for([&] { return aRuntime.status().ProfileMode == core::appearance_mode::light; }, "restore Work's light appearance");
	}
	struct running_profile
	{
		std::unique_ptr<app::github_application_startup> Startup;
		std::unique_ptr<app::github_application>         Runtime;
	};
	// Only the test harness waits; both production and fixture factories return immediately.
	auto await_profile(const std::filesystem::path& aRoot, const core::bpr_profile_id aProfile) -> running_profile
	{
		auto result                 = running_profile{ };
		auto terminal               = false;
		auto failed                 = false;
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = aRoot;
		options.Profile             = aProfile;
		result.Startup              = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com,
																			   options, [&](auto aResult) {
																	  terminal = true;
																	  failed   = !aResult.has_value();
																	  if (aResult) {
																		  result.Runtime = std::move(*aResult);
																	  }
																			   });
		expect(!terminal && !result.Runtime, "startup never returns an inline runtime");
		wait_for([&] { return terminal && result.Startup->is_quiescent(); }, "profile admission terminal");
		expect(!failed && result.Runtime != nullptr, "exact profile is admitted");
		result.Runtime->show();
		return result;
	}
	void stop(running_profile& aProfile)
	{
		aProfile.Runtime->begin_shutdown();
		wait_for([&] { return aProfile.Runtime->status().ShutdownComplete; }, "profile shutdown");
		aProfile.Runtime.reset();
		aProfile.Startup.reset();
	}
	void expect_refusal(const std::filesystem::path& aRoot, const core::bpr_profile_id aProfile, const bool aCancel,
						std::optional<app::github_application_startup_failure> aExpected = { })
	{
		QApplication::processEvents();
		const auto windowCount      = QApplication::topLevelWidgets().size();
		auto       terminal         = false;
		auto       failed           = false;
		auto       calls            = 0U;
		auto       reason           = std::optional<app::github_application_startup_failure>{ };
		auto       options          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = aRoot;
		options.Profile             = aProfile;
		auto startup                = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com,
																			   options, [&](auto aResult) { terminal = true; failed = !aResult; ++calls; if (!aResult) { reason = aResult.error(); } });
		expect(!terminal, "refusal is queued on decision lane");
		if (aCancel) {
			startup->cancel();
		}
		wait_for([&] { return terminal && startup->is_quiescent(); }, "refused startup settles");
		QApplication::processEvents();
		expect(failed && calls == 1U, "one failure terminal, no fallback runtime");
		expect(!aExpected || reason == aExpected, "exact admission failure");
		expect(QApplication::topLevelWidgets().size() == windowCount, "refused admission constructs no window");
	}
#if !defined(_WIN32)
	// The permission and special bits of the named entry itself; 0 when it is absent.
	auto mode_of(const std::filesystem::path& aPath) -> mode_t
	{
		struct stat status{ };
		return ::lstat(aPath.c_str(), &status) == 0 ? status.st_mode & static_cast<mode_t>(07777) : mode_t{ 0 };
	}
	// [[BPR-0817]]. A Linux desktop login with a user-private group runs under
	// umask 0002. A fresh install there must create its data folders owner-only,
	// so the agent workspace neighbourhood beside the root is admitted and the
	// profile closes with no cleanup failure.
	void expect_fresh_install_under_group_umask(const std::filesystem::path& aPrivateScratch)
	{
		const auto previousMask  = ::umask(0002);
		const auto installParent = aPrivateScratch / "fresh-install";
		const auto dataParent    = installParent / "BetterPullReview";
		const auto dataRoot      = dataParent / "BetterPullReview";
		const auto neighbourhood = dataParent / "BetterPullReview-agent-workspaces-v1";
		expect(mode_of(aPrivateScratch) == 0700 && mode_of(installParent) == 0, "fresh install starts below a private folder with no data folder");
		auto fresh = await_profile(dataRoot, core::default_bpr_profile_id);
		expect(mode_of(installParent) == 0700, "the outermost folder a fresh install creates is exactly 0700 under umask 0002");
		expect(mode_of(dataParent) == 0700, "the data root's created parent is exactly 0700 under umask 0002");
		expect(mode_of(dataRoot) == 0700, "the created data root is exactly 0700 under umask 0002");
		wait_for([&] { return mode_of(neighbourhood) != 0; }, "startup recovery creates the agent workspace neighbourhood beside a fresh root");
		struct stat neighbourhoodStatus{ };
		expect(::lstat(neighbourhood.c_str(), &neighbourhoodStatus) == 0 && S_ISDIR(neighbourhoodStatus.st_mode)
				   && (neighbourhoodStatus.st_mode & static_cast<mode_t>(07777)) == 0700,
			   "the agent workspace neighbourhood is an exactly 0700 directory");
		fresh.Runtime->begin_shutdown();
		wait_for([&] { return fresh.Runtime->status().ShutdownComplete; }, "fresh install profile shutdown");
		const auto closed = fresh.Runtime->status();
		expect(!closed.AgentWorkspaceRecoveryRefused, "agent workspace recovery succeeds for a fresh install");
		expect(!closed.AgentWorkspaceCleanupFailed, "and its workspace cleanup succeeds at the close");
		expect(!closed.ShutdownCleanupFailed, "a fresh install closes with no shutdown cleanup failure");
		fresh.Runtime.reset();
		fresh.Startup.reset();
		static_cast<void>(::umask(previousMask));
	}
#endif
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication qtApplication{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private registry root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop test root under the canonical storage policy");
#if !defined(_WIN32)
	const auto unsafeRoot = root / "unsafe-existing";
	expect(std::filesystem::create_directory(unsafeRoot), "create designated unsafe fixture root");
	const auto unsafePermissions = std::filesystem::perms::owner_all | std::filesystem::perms::group_read;
	std::filesystem::permissions(unsafeRoot, unsafePermissions);
	expect_refusal(unsafeRoot, core::default_bpr_profile_id, false);
	expect(std::filesystem::status(unsafeRoot).permissions() == unsafePermissions,
		   "startup refuses existing shared root without permission repair");
	expect(!std::filesystem::exists(unsafeRoot / "state"), "unsafe root creates no registry or domain store");
	expect_fresh_install_under_group_umask(root);
#endif
	auto registry = adapters::sqlite::make_profile_registry_store(root);
	auto snapshot = registry->load({ });
	expect(snapshot.has_value(), "Default registry adoption");
	const auto revision = snapshot->Revision;
	++snapshot->Revision;
	snapshot->LastProfileId = 2U;
	snapshot->Profiles.push_back({ { core::bpr_profile_id{ 2U }, "Work" }, application::profile_root_mode::isolated, "profile-2" });
	expect(registry->save(*snapshot, revision, { }).has_value(), "seed durable Work profile");
	auto authority = application::profile_storage_authority::select(*snapshot, core::bpr_profile_id{ 2U });
	expect(authority.has_value(), "exact Work root authority");
	auto workRoot = adapters::sqlite::resolve_profile_storage_root(root, *authority);
	expect(workRoot.has_value(), "private Work root");
	seed_appearance(root, core::appearance_mode::dark);
	seed_appearance(*workRoot, core::appearance_mode::light);

	expect_refusal(root, core::bpr_profile_id{ 99U }, false);
	expect_refusal(root, core::bpr_profile_id{ 2U }, true);
	expect(!std::filesystem::exists(root / "state/v1/review-queue.sqlite3"), "refused startup did not construct Default queue");
	auto work = await_profile(root, core::bpr_profile_id{ 2U });
	wait_for([&] { return work.Runtime->status().ProfileMode == core::appearance_mode::light; }, "Work restores its own appearance");
	expect(!std::filesystem::exists(root / "state/v1/review-queue.sqlite3"), "Work private services do not use registry root");
	expect(work.Runtime->window().windowTitle().contains(QStringLiteral("Work")), "workbench identifies admitted profile");
	work.Runtime->window().open_settings(qt::settings_page::appearance);
	expect(work.Runtime->window().settings()->windowTitle().contains(QStringLiteral("Work")), "Settings identifies admitted profile");
	test_appearance_banners(*work.Runtime);

	auto original = await_profile(root, core::default_bpr_profile_id);
	wait_for([&] { return original.Runtime->status().ProfileMode == core::appearance_mode::dark; }, "Default retains original settings");
	const auto workWrites = work.Runtime->status().AppearanceDurableWrites;
	// [[BPR-0790]]. The theme tiles are the control; choosing one is this gesture.
	work.Runtime->window().appearance_settings()->choose_mode(
		core::appearance_mode::high_contrast_light);
	wait_for([&] { return work.Runtime->status().AppearanceDurableWrites > workWrites; }, "Work setting persists through owning host");
	expect(original.Runtime->status().ProfileMode == core::appearance_mode::dark, "Work edit leaves Default appearance unchanged");
	// [[BPR-1336]]. View > Zoom In, twice, zooms Work's comparison through its
	// own appearance host and store; Settings shows the same zoom, Code size is
	// unchanged, and Default keeps its code at its own size.
	const auto zoomedBy = [](app::github_application& aRuntime, const double aFactor) {
		auto* const panel = aRuntime.window().appearance_settings();
		const auto  size  = panel->code_size_editor()->value() / 100.0;
		return std::abs(aRuntime.window().comparison_surface()->right_pane()->font().pointSizeF() - size * aFactor) < 0.01;
	};
	wait_for([&] { return zoomedBy(*work.Runtime, 1.0) && work.Runtime->window().zoom_in_action()->isEnabled(); }, "Work's comparison starts at its code size with Zoom In offered");
	const auto zoomWrites = work.Runtime->status().AppearanceDurableWrites;
	const auto workSize   = work.Runtime->window().appearance_settings()->code_size_editor()->value();
	work.Runtime->window().zoom_in_action()->trigger();
	work.Runtime->window().zoom_in_action()->trigger();
	wait_for([&] { return zoomedBy(*work.Runtime, 1.25) && work.Runtime->status().AppearanceDurableWrites > zoomWrites
					   && work.Runtime->window().appearance_settings()->code_zoom_selector()->currentData().toUInt() == 125U; },
			 "two Zoom Ins show Work's comparison at 125%, persisted and shown in Settings");
	expect(work.Runtime->window().appearance_settings()->code_size_editor()->value() == workSize && work.Runtime->window().actual_size_action()->isEnabled(),
		   "Code size is unchanged and Actual Size is offered");
	expect(zoomedBy(*original.Runtime, 1.0), "Default's comparison keeps its own code size");
	expect_refusal(root, core::bpr_profile_id{ 2U }, false, app::github_application_startup_failure::owner_busy);
	work.Runtime->begin_shutdown();
	wait_for([&] { return work.Runtime->status().ShutdownComplete; }, "Work drains before retained-lease test");
	expect_refusal(root, core::bpr_profile_id{ 2U }, false, app::github_application_startup_failure::owner_busy);
	stop(work);
	expect(!original.Runtime->status().Closing, "closing Work leaves Default usable");
	stop(original);

	snapshot = registry->load({ });
	expect(snapshot.has_value(), "reload registry before rename");
	const auto renameRevision = snapshot->Revision;
	++snapshot->Revision;
	snapshot->Profiles.at(1).Profile.Label = "Work / renamed";
	expect(registry->save(*snapshot, renameRevision, { }).has_value(), "rename keeps stable root identity");
	work = await_profile(root, core::bpr_profile_id{ 2U });
	wait_for([&] { return work.Runtime->status().ProfileMode == core::appearance_mode::high_contrast_light; }, "renamed Work restores its same private store");
	wait_for([&] { return zoomedBy(*work.Runtime, 1.25); }, "Work reopens with its comparison zoomed to 125%");
	expect(work.Runtime->window().windowTitle().contains(QStringLiteral("Work / renamed")), "accepted rename is visible");
	stop(work);
	original = await_profile(root, core::default_bpr_profile_id);
	wait_for([&] { return original.Runtime->status().ProfileMode == core::appearance_mode::dark; }, "Default remains unchanged after both reopen");
	stop(original);
	{
		std::ofstream damaged{ *workRoot / "state/v1/review-queue.sqlite3", std::ios::binary | std::ios::trunc };
		damaged << "invalid isolated queue database";
	}
	work = await_profile(root, core::bpr_profile_id{ 2U });
	wait_for([&] { return work.Runtime->window().queue_status_label()->text().contains(QStringLiteral("unavailable")); }, "damaged optional queue is reported at its own scope");
	wait_for([&] { return work.Runtime->status().ProfileMode == core::appearance_mode::high_contrast_light; }, "optional queue refusal preserves independent appearance capability");
	stop(work);
	std::cout << "profile-startup-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
