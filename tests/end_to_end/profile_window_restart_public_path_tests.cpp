#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/profile_management_window.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QThread>

#include <sqlite3.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
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
		auto* selected = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(selected != nullptr, "profile action exists");
		wait_for([&] { return selected->isEnabled(); }, "profile action enabled");
		selected->trigger();
	}
	void reject_locked_lifecycle(app::profile_desktop& aDesktop, qt::workbench_window& aWindow,
								 const std::filesystem::path& aRoot, bool aQuit)
	{
		auto* management = aDesktop.management_window();
		expect(management != nullptr, "management available for recovery");
		management->present();
		// [[BPR-0777]]. The appended recovery notice and Reload Profiles button are gone.
		// RFC-BPR-0033 B3 reports a refusal in a window that exists, so recovery is
		// observed there: the window's Profiles menu carries the refusal, and Manage
		// Profiles performs the reload that clears it. The chooser's Create button is
		// the registry-idle indicator while the registry is accepted; a refused save
		// leaves it unready, which is why only the window's item is waited on below.
		auto* failure = aWindow.findChild<QAction*>(QStringLiteral("profileManagementStatusAction"));
		auto* idle    = management->create_button();
		expect(failure != nullptr && idle != nullptr, "the window's refusal item and the chooser's idle indicator exist");
		wait_for([&] { return idle->isEnabled(); }, "registry idle before fault");
		sqlite3* raw = nullptr;
		expect(sqlite3_open_v2((aRoot / "state/v1/profile-registry.sqlite3").string().c_str(), &raw,
							   SQLITE_OPEN_READWRITE, nullptr)
				   == SQLITE_OK,
			   "fixture opens existing registry");
		const auto connection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>{ raw, sqlite3_close };
		expect(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture blocks registry writes");
		if (aQuit) {
			trigger(aWindow, "quitProfilesAction");
		} else {
			aWindow.close();
		}
		wait_for([&] { return failure->isVisible(); }, "the failed save is reported in the window that stays usable");
		expect(aWindow.isVisible() && aDesktop.windows(core::default_bpr_profile_id).size() == 2U && !aDesktop.is_quiescent(),
			   "failed lifecycle save keeps both real windows usable");
		expect(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture releases write lock");
		trigger(aWindow, "manageProfilesAction");
		wait_for([&] { return !failure->isVisible() && idle->isEnabled(); }, "an explicit reload recovers the accepted registry");
		expect(aWindow.isVisible() && aDesktop.windows(core::default_bpr_profile_id).size() == 2U,
			   "reload never retries a destructive action automatically");
	}
	auto selected_window(app::profile_desktop& aDesktop, core::bpr_profile_id aProfile,
						 core::bpr_window_id aId, std::uint64_t aGeneration) -> qt::workbench_window*
	{
		const auto context = core::bpr_window_context{ aProfile, aId, core::bpr_window_generation{ aGeneration } };
		auto*      result  = aDesktop.window(context);
		expect(result != nullptr && result->isVisible() && result->showing_home(), "exact restored Home window visible");
		// This identity-only course has no saved review destination; Home is the
		// expected content, with no synthetic unavailable-review notice required.
		if (aGeneration > 1U) {
			expect(aDesktop.window({ aProfile, aId, core::bpr_window_generation{ aGeneration - 1U } }) == nullptr,
				   "previous process generation is stale");
		}
		return result;
	}
	auto profile_item(qt::profile_management_window& aManagement, core::bpr_profile_id aProfile) -> QListWidgetItem*
	{
		for (auto index = 0; index < aManagement.profiles()->count(); ++index) {
			auto* row = aManagement.profiles()->item(index);
			if (row->data(Qt::UserRole).toULongLong() == aProfile.value()) {
				return row;
			}
		}
		return nullptr;
	}
	void open_profile(app::profile_desktop& aDesktop, core::bpr_profile_id aProfile)
	{
		auto* management = aDesktop.management_window();
		expect(management != nullptr, "profile management exists");
		management->present();
		auto* row = profile_item(*management, aProfile);
		expect(row != nullptr, "durable profile remains listed");
		management->profiles()->setCurrentItem(row);
		wait_for([&] { return management->open_button()->isEnabled(); }, "Open Profile enabled");
		management->open_button()->click();
		wait_for([&] { return !aDesktop.windows(aProfile).empty(); }, "profile windows open");
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 3, "phase and private root required");
	const auto   phase = std::string{ aArgv[1] };
	const auto   root  = std::filesystem::canonical(aArgv[2]);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	std::array<std::uint64_t, 4U> saved{ }; // Default A/B IDs, Work profile/window IDs.
	if (phase != "seed") {
		std::ifstream input{ root / "restart-expectations.txt" };
		for (auto& value : saved) {
			input >> value;
			expect(input.good() && value != 0U, "previous-process expectation is readable");
		}
	}
	auto options                      = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = root;
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "desktop startup completes");
	expect(owner != nullptr, "desktop admitted after durable window allocation");
	const auto            defaultId  = core::default_bpr_profile_id;
	qt::workbench_window* quitWindow = nullptr;
	if (phase == "seed") {
		wait_for([&] { return desktop->windows(defaultId).size() == 1U; }, "fresh Default has one window");
		const auto first = desktop->windows(defaultId).front();
		auto*      a     = desktop->window(first);
		expect(a != nullptr && a->showing_home() && a->home_open_notice()->text().isEmpty(), "fresh window has no restoration warning");
		trigger(*a, "newBprWindowAction");
		wait_for([&] { return desktop->windows(defaultId).size() == 2U; }, "second window opens");
		const auto windows = desktop->windows(defaultId);
		const auto second  = windows.front() == first ? windows.back() : windows.front();
		saved[0]           = first.Window.value();
		saved[1]           = second.Window.value();
		reject_locked_lifecycle(*desktop, *a, root, false);
		reject_locked_lifecycle(*desktop, *a, root, true);
		trigger(*a, "manageProfilesAction");
		auto* management = desktop->management_window();
		wait_for([&] { return management && management->create_button()->isEnabled(); }, "profile creation available");
		management->name_editor()->setText(QStringLiteral("Work"));
		management->create_button()->click();
		wait_for([&] { return management->profiles()->count() == 2; }, "Work profile saved");
		for (auto index = 0; index < management->profiles()->count(); ++index) {
			const auto id = management->profiles()->item(index)->data(Qt::UserRole).toULongLong();
			if (id != defaultId.value()) {
				saved[2] = id;
			}
		}
		open_profile(*desktop, core::bpr_profile_id{ saved[2] });
		saved[3] = desktop->windows(core::bpr_profile_id{ saved[2] }).front().Window.value();
		expect(saved[3] > saved[1] && saved[1] > saved[0], "global allocation crosses profiles monotonically");
		a->raise();
		a->activateWindow();
		wait_for([&] { return QApplication::activeWindow() == a; }, "native focus returns to first window");
		std::ofstream output{ root / "restart-expectations.txt" };
		for (const auto value : saved) {
			output << value << '\n';
		}
		output.close();
		expect(output.good(), "expected identities recorded outside application state");
		quitWindow = a;
	} else {
		const auto expectedCount = phase == "verify" ? 1U : 2U;
		wait_for([&] { return desktop->windows(defaultId).size() == expectedCount; }, "retained Default window set restored");
		const auto generation = phase == "restore" ? 2U : phase == "close" ? 3U
																		   : 4U;
		auto*      b          = selected_window(*desktop, defaultId, core::bpr_window_id{ saved[1] }, generation);
		quitWindow            = b;
		if (phase != "verify") {
			auto* a = selected_window(*desktop, defaultId, core::bpr_window_id{ saved[0] }, generation);
			if (phase == "close") {
				a->close();
				wait_for([&] { return desktop->windows(defaultId).size() == 1U; }, "explicit close removes only one window");
				expect(desktop->window({ defaultId, core::bpr_window_id{ saved[0] }, core::bpr_window_generation{ generation } }) == nullptr, "closed context retired");
			} else {
				wait_for([&] { return QApplication::activeWindow() == a; }, "last-focused retained window reactivates");
				const auto artifactRoot = qEnvironmentVariable("BPR_PROFILE_RESTART_ARTIFACT_ROOT");
				if (!artifactRoot.isEmpty()) {
					expect(QDir{ }.mkpath(artifactRoot), "restart artifact directory");
					expect(a->grab().save(artifactRoot + QStringLiteral("/restored-home.png")), "restored Home raster");
				}
			}
		} else {
			for (const auto context : desktop->windows(defaultId)) {
				expect(context.Window.value() != saved[0], "explicitly closed ID not restored");
			}
			trigger(*b, "newBprWindowAction");
			wait_for([&] { return desktop->windows(defaultId).size() == 2U; }, "new window after restart");
			for (const auto context : desktop->windows(defaultId)) {
				if (context.Window.value() != saved[1]) {
					expect(context.Window.value() > saved[3] && context.Generation.value() == 1U, "new identity exceeds durable global high-water");
					expect(desktop->window(context)->home_open_notice()->text().isEmpty(), "new window is not falsely restored");
				}
			}
		}
		if (phase != "close") {
			const auto work = core::bpr_profile_id{ saved[2] };
			open_profile(*desktop, work);
			expect(desktop->windows(work).size() == 1U, "other profile retained set remains independent");
			static_cast<void>(selected_window(*desktop, work, core::bpr_window_id{ saved[3] }, phase == "restore" ? 2U : 3U));
			auto* before = desktop->runtime(defaultId);
			open_profile(*desktop, defaultId);
			expect(desktop->runtime(defaultId) == before && desktop->windows(defaultId).size() == 2U, "ordinary Open activates without duplication");
		}
	}
	expect(quitWindow != nullptr, "Quit has an exact live context");
	trigger(*quitWindow, "quitProfilesAction");
	wait_for([&] { return desktop->is_quiescent(); }, "durable Quit drains every profile");
	desktop.reset();
	std::cout << "profile-window-restart:" << phase << ":pass\n";
	return EXIT_SUCCESS;
}
