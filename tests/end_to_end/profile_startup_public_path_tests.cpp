#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/settings_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
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
	void expect_refusal(const std::filesystem::path& aRoot, const core::bpr_profile_id aProfile, const bool aCancel)
	{
		QApplication::processEvents();
		const auto windowCount      = QApplication::topLevelWidgets().size();
		auto       terminal         = false;
		auto       failed           = false;
		auto       calls            = 0U;
		auto       options          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = aRoot;
		options.Profile             = aProfile;
		auto startup                = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com,
																			   options, [&](auto aResult) { terminal = true; failed = !aResult; ++calls; });
		expect(!terminal, "refusal is queued on decision lane");
		if (aCancel) {
			startup->cancel();
		}
		wait_for([&] { return terminal && startup->is_quiescent(); }, "refused startup settles");
		QApplication::processEvents();
		expect(failed && calls == 1U, "one failure terminal, no fallback runtime");
		expect(QApplication::topLevelWidgets().size() == windowCount, "refused admission constructs no window");
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication qtApplication{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private registry root");
	const auto root = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
#if !defined(_WIN32)
	const auto unsafeRoot = root / "unsafe-existing";
	expect(std::filesystem::create_directory(unsafeRoot), "create designated unsafe fixture root");
	const auto unsafePermissions = std::filesystem::perms::owner_all | std::filesystem::perms::group_read;
	std::filesystem::permissions(unsafeRoot, unsafePermissions);
	expect_refusal(unsafeRoot, core::default_bpr_profile_id, false);
	expect(std::filesystem::status(unsafeRoot).permissions() == unsafePermissions,
		   "startup refuses existing shared root without permission repair");
	expect(!std::filesystem::exists(unsafeRoot / "state"), "unsafe root creates no registry or domain store");
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

	auto original = await_profile(root, core::default_bpr_profile_id);
	wait_for([&] { return original.Runtime->status().ProfileMode == core::appearance_mode::dark; }, "Default retains original settings");
	const auto workWrites = work.Runtime->status().AppearanceDurableWrites;
	auto*      mode       = work.Runtime->window().appearance_settings()->mode_selector();
	mode->setCurrentIndex(mode->findData(static_cast<int>(core::appearance_mode::high_contrast_light)));
	wait_for([&] { return work.Runtime->status().AppearanceDurableWrites > workWrites; }, "Work setting persists through owning host");
	expect(original.Runtime->status().ProfileMode == core::appearance_mode::dark, "Work edit leaves Default appearance unchanged");
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
}
