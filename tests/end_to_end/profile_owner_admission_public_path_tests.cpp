#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QListWidget>
#include <QPushButton>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage = "bounded public course condition")
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	expect(aArgc == 5, "mode root profile signal arguments");
	const auto mode    = std::string{ aArgv[1] };
	const auto root    = std::filesystem::path{ aArgv[2] };
	const auto profile = core::bpr_profile_id{ std::stoull(aArgv[3]) };
	const auto signal  = std::filesystem::path{ aArgv[4] };
	if (mode == "seed") {
		auto store = adapters::sqlite::make_profile_registry_store(root);
		auto state = store->load({ });
		// [[BPR-0877]]. Name the refusal rather than only reporting that the
		// seed failed: this store's own failure value says which.
		if (!state) {
			std::cerr << "SEED_REGISTRY_REFUSAL:" << static_cast<int>(state.error()) << '\n'
					  << std::flush;
		}
		expect(state.has_value(), "seed registry");
		const auto revision  = state->Revision++;
		state->LastProfileId = 2;
		state->Profiles.push_back({ { core::bpr_profile_id{ 2 }, "Work" }, application::profile_root_mode::isolated, "profile-2" });
		expect(store->save(*state, revision, { }).has_value(), "seed second profile");
		return EXIT_SUCCESS;
	}
	auto options                                                    = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot                                     = root;
	options.Profile                                                 = profile;
	bool                                                   terminal = false;
	std::optional<app::github_application_startup_failure> failure;
	unsigned                                               completions = 0;
	auto                                                   desktop     = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
		terminal = true;
		++completions;
		if (!aResult) {
			failure = aResult.error();
		}
	});
	wait_for([&] { return terminal; }, "startup reaches a terminal");
	if (mode == "contender") {
		// [[BPR-0907]]. Every platform hands a second launch to the owner since
		// [[BPR-0979]] gave Windows the activation POSIX had. The Windows-only
		// branches this replaced waited for an owner_busy refusal and its
		// "already open" notice, which Windows no longer reaches.
		const auto expectedFailure = app::github_application_startup_failure::activation_queued;
		if (failure != expectedFailure) {
			std::cerr << "unexpected startup result " << (failure ? static_cast<int>(*failure) : -1) << '\n';
		}
		expect(failure == expectedFailure, "same-profile startup routes or reports unavailable transport");
		wait_for([&] { return desktop->is_quiescent(); }, "desktop goes quiescent");
		expect(completions == 1, "refusal delivers one startup terminal through shutdown");
		expect(desktop->windows(profile).empty(), "contender never publishes a window");
		std::cout << "OWNER_HANDOFF_OR_BUSY\n";
		return EXIT_SUCCESS;
	}
	expect(!failure, "owner startup succeeds");
	wait_for([&] { return desktop->windows(profile).size() == 1; }, "the profile has exactly one window");
	if (mode == "other") {
		auto* management = desktop->management_window();
		expect(management != nullptr, "management composed");
		wait_for([&] { return management->open_button()->isEnabled(); }, "the management window offers open");
		management->profiles()->setCurrentRow(0);
		management->open_button()->click();
		// [[BPR-0777]]. Create's enablement is the registry-settled fact the retired
		// Reload Profiles button carried.
		auto* settled = management->create_button();
		expect(settled != nullptr, "registry settlement indicator");
		wait_for([&] { return settled->isEnabled(); }, "the settled control is enabled");
		expect(desktop->windows(core::default_bpr_profile_id).empty(), "Open Profile contention creates no foreign window");
		expect(desktop->windows(profile).size() == 1, "other profile remains usable after refusal");
		std::cout << "OTHER_REFUSED_DEFAULT\n"
				  << std::flush;
	}
	std::cout << "OWNER_READY\n"
			  << std::flush;
	wait_for([&] { return std::filesystem::exists(signal); }, "owner waits for orderly quit signal");
	// Another profile can advance the global allocator while this process is open.
	// Reload before Quit exercises the existing explicit CAS recovery course.
	auto* management = desktop->management_window();
	auto* idle       = management->create_button();
	expect(idle != nullptr, "the chooser's registry-idle indicator");
	wait_for([&] { return idle->isEnabled(); }, "registry idle before reload");
	// [[BPR-0777]]. Manage Profiles is the reload gesture now that the desktop appends
	// no Reload Profiles button; the explicit CAS recovery it exercises is unchanged.
	auto* manage = desktop->window(desktop->windows(profile).front())
					   ->findChild<QAction*>(QStringLiteral("manageProfilesAction"));
	expect(manage != nullptr, "registry reload reaches the host through Manage Profiles");
	manage->trigger();
	wait_for([&] { return idle->isEnabled(); }, "registry reload completes");
	desktop->quit();
	wait_for([&] { return desktop->is_quiescent(); }, "owner and all services drain after quit");
	expect(desktop->exit_code() == 0, "orderly owner quit");
	std::cout << "OWNER_RELEASED\n";
	return EXIT_SUCCESS;
}
