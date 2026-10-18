#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/logging/quill_log.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/settings_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
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
	/// A retired desktop releases the owner lease its startup scratch holds when its
	/// implementation is destroyed, which is the poll tick after it first reports
	/// quiescence. Only a course starts another desktop in the same process, so it is
	/// the course that has to let that tick happen; otherwise the next startup is
	/// correctly refused as owner_busy.
	void settle(const int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	auto item(qt::profile_management_window& aManagement, const QString& aLabel) -> QListWidgetItem*
	{
		for (auto index = 0; index < aManagement.profiles()->count(); ++index) {
			auto* candidate = aManagement.profiles()->item(index);
			if (candidate->text() == aLabel) {
				return candidate;
			}
		}
		return nullptr;
	}
	auto create(qt::profile_management_window& aManagement, const QString& aLabel) -> core::bpr_profile_id
	{
		// [[BPR-0778]]. New profile appends a row already in its editor, so the name is
		// typed into that row and saved; there is no permanent name field to fill first.
		expect(aManagement.create_button()->isEnabled(), "New profile is reachable");
		aManagement.create_button()->click();
		aManagement.name_editor()->setText(aLabel);
		aManagement.save_button()->click();
		wait_for([&] { return item(aManagement, aLabel) != nullptr; }, "created profile is durably projected");
		auto* created = item(aManagement, aLabel);
		aManagement.profiles()->setCurrentItem(created);
		return core::bpr_profile_id{ created->data(Qt::UserRole).toULongLong() };
	}
	auto open(app::profile_desktop& aDesktop, qt::profile_management_window& aManagement, const core::bpr_profile_id aProfile) -> app::github_application*
	{
		expect(aManagement.open_button()->isEnabled(), "Open Profile is reachable");
		aManagement.open_button()->click();
		wait_for([&] { return aDesktop.runtime(aProfile) != nullptr; }, "selected profile runtime opens");
		return aDesktop.runtime(aProfile);
	}
	void capture(qt::profile_management_window& aManagement)
	{
		const auto path = qEnvironmentVariable("BPR_PROFILE_MANAGEMENT_ARTIFACT_ROOT");
		if (path.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(path), "management artifact directory");
		QApplication::processEvents();
		expect(aManagement.grab().save(path + QStringLiteral("/profile-management.png")), "management raster");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication qtApplication{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private desktop test scratch");
	// [[BPR-0817]], [[BPR-1050]]. The data root is a folder inside the private
	// scratch, as an installed data root is a folder inside a private data
	// folder. The agent workspace neighbourhood sits beside the root, so its
	// parent is the scratch, not the host's temporary directory, which on stock
	// Linux is a shared 1777 `/tmp` the neighbourhood correctly refuses.
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop test root under the canonical storage policy");
	{
		auto store = adapters::sqlite::make_appearance_settings_store({ .StorageRoot = root });
		expect(store.has_value(), "legacy appearance store");
		auto settings                        = application::appearance_settings{ };
		settings.Mode                        = core::appearance_mode::dark;
		settings.Typography.Ui.Font.Family   = "Sans Serif";
		settings.Typography.Code.Font.Family = "Monospace";
		expect((*store)->store(settings, { }).has_value(), "tuned Default fixture");
	}
	app::github_application* initial  = nullptr;
	auto                     terminal = false;
	auto                     options  = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = root;
	auto desktop                      = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
			}
		});
	expect(!terminal, "desktop startup remains queued");
	wait_for([&] { return terminal; }, "desktop initial startup");
	expect(initial != nullptr, "initial runtime admitted");
	initial->show();
	wait_for([&] { return initial->status().ProfileMode == core::appearance_mode::dark; }, "Default preserves tuned preferences");
	expect(initial->window().dif() != nullptr
			   && initial->status().DifCatalogLoadAttempts == 1U
			   && initial->status().DifCatalogDecodedClips == 10U,
		   "the desktop composes one complete accepted Dif catalog");
	auto* manage = initial->window().findChild<QAction*>(QStringLiteral("manageProfilesAction"));
	expect(manage != nullptr, "profile menu exposes management");
	wait_for([&] { return manage->isEnabled(); }, "profile menu becomes available");
	manage->trigger();
	wait_for([&] { return desktop->management_window() != nullptr && desktop->management_window()->isVisible(); }, "modeless management opens from real menu");
	auto& management = *desktop->management_window();
	wait_for([&] { return management.create_button()->isEnabled(); }, "registry is ready for management");
	const auto workId = create(management, QStringLiteral("Work"));
	expect(workId != core::default_bpr_profile_id, "new profile has distinct durable identity");
	auto* work = open(*desktop, management, workId);
	wait_for([&] { return work->status().AppearancePersistenceAvailable; }, "Work profile settings composed");
	expect(work->window().dif() != nullptr
			   && work->window().dif()->cache_identity() == initial->window().dif()->cache_identity()
			   && work->status().DifCatalogLoadAttempts == 1U
			   && work->status().DifCatalogDecodedClips == 10U,
		   "a second profile shares the same once-decoded catalog");
	expect(work->status().ProfileMode == core::appearance_mode::system, "new profile begins with standard preferences");
	expect(initial->status().ProfileMode == core::appearance_mode::dark, "new profile does not replace Default");
	const auto stableRow = QPersistentModelIndex{ management.profiles()->model()->index(management.profiles()->currentRow(), 0) };
	management.rename_button()->click();
	management.name_editor()->setText(QStringLiteral("Work renamed"));
	management.save_button()->click();
	wait_for([&] { return work->window().windowTitle().contains(QStringLiteral("Work renamed")); }, "accepted rename reaches open chrome");
	expect(stableRow.isValid() && stableRow.data().toString() == QStringLiteral("Work renamed"), "rename preserves semantic model index");
	// [[BPR-0778]]. Return on the row activates an open profile; the footer's filled
	// action says New window for one, because that is what that gesture does.
	expect(management.open_button()->text() == QStringLiteral("New window"),
		   "the filled action offers a new window for the open profile");
	auto openRow = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(management.profiles(), &openRow);
	QApplication::processEvents();
	expect(desktop->runtime(workId) == work, "reopening the live profile activates its existing runtime");
	expect(desktop->windows(workId).size() == 1U, "and does not add a window");
	capture(management);

	const auto writes = work->status().AppearanceDurableWrites;
	work->window().open_settings(qt::settings_page::appearance);
	// [[BPR-0790]]. The theme tiles are the control; choosing one is this gesture.
	work->window().appearance_settings()->choose_mode(
		core::appearance_mode::high_contrast_dark);
	auto* motion = work->window().appearance_settings()->motion_selector();
	motion->setCurrentIndex(motion->findData(static_cast<int>(core::motion_mode::full)));
	wait_for([&] {
		return work->status().AppearanceDurableWrites > writes
			&& work->status().DifResolution.Preferences.Motion == core::motion_mode::full;
	},
			 "Work preferences persist through real Settings");
	expect(initial->status().DifResolution.Preferences.Motion == core::motion_mode::system,
		   "a Work motion gesture does not change Default");
	initial->window().close();
	wait_for([&] { return desktop->runtime(core::default_bpr_profile_id) == nullptr; }, "closed initial profile drains and retires");
	expect(!work->status().Closing, "closing initial profile leaves Work usable");
	management.present();
	const auto personalId = create(management, QStringLiteral("Personal"));
	auto*      personal   = open(*desktop, management, personalId);
	expect(personal != work, "desktop owner remains usable after initial profile closes");
	expect(personal->window().dif() != nullptr
			   && personal->window().dif()->cache_identity() == work->window().dif()->cache_identity()
			   && personal->status().DifCatalogLoadAttempts == 1U
			   && personal->status().DifCatalogDecodedClips == 10U
			   && personal->status().DifResolution.Preferences.Motion == core::motion_mode::system
			   && work->status().DifResolution.Preferences.Motion == core::motion_mode::full,
		   "a third profile shares decoded pixels while keeping its own motion preference");
	work->window().close();
	wait_for([&] { return desktop->runtime(workId) == nullptr; }, "Work closes before owner reuse");
	management.profiles()->setCurrentItem(item(management, QStringLiteral("Work renamed")));
	work = open(*desktop, management, workId);
	wait_for([&] {
		return work->status().ProfileMode == core::appearance_mode::high_contrast_dark
			&& work->status().DifResolution.Preferences.Motion == core::motion_mode::full;
	},
			 "renamed Work restores its private appearance and motion preferences");
	expect(work->window().dif()->cache_identity() == personal->window().dif()->cache_identity()
			   && work->status().DifCatalogLoadAttempts == 1U,
		   "profile restart reuses the process catalog without a second load");
	auto* quit = work->window().findChild<QAction*>(QStringLiteral("quitProfilesAction"));
	expect(quit != nullptr, "native Quit action is composed");
	quit->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "desktop Quit drains every profile and startup");
	desktop.reset();
	// M4 supplies the same owner-private sink on every shipped platform.
	const auto sinkStatus = [&] {
		expect(adapters::sqlite::create_storage_root_owner_private(root / "diagnostic-sink-probe"), "private sink probe root");
		adapters::logging::quill_log probe{ root / "diagnostic-sink-probe" };
		const auto                   status = probe.status();
		probe.close();
		return status;
	}();
	expect(sinkStatus == application::diagnostic_log_status::available, "this platform reports its diagnostics file sink");
	for (const auto& logRoot : { root, root / "profiles" / ("profile-" + std::to_string(workId.value())), root / "profiles" / ("profile-" + std::to_string(personalId.value())) }) {
		if (sinkStatus != application::diagnostic_log_status::available) {
			expect(bpr::tests::diagnostic_files(logRoot).empty(),
				   "no concurrent profile writes a diagnostics file where the platform has no file sink");
			continue;
		}
		const auto text = bpr::tests::read_diagnostic_logs(logRoot);
		expect(text.find("\"op\":\"application\",\"outcome\":\"started\"") != std::string::npos
				   && text.find("\"op\":\"application\",\"outcome\":\"shutdown\"") != std::string::npos,
			   "each concurrent profile retains its own startup and shutdown diagnostics");
	}

	// [[BPR-0777]] RFC-BPR-0033 B1. This platform's own answer for an empty desktop,
	// read from the one place the composition names it. The other platform's answer is
	// driven below through the injected policy; its native behaviour is target work.
#if defined(__APPLE__)
	expect(app::native_empty_desktop_policy()() == app::empty_desktop_outcome::stay_resident,
		   "macOS stays resident when its last window closes");
#else
	expect(app::native_empty_desktop_policy()() == app::empty_desktop_outcome::quit,
		   "a platform where no window means gone quits when its last window closes");
#endif

	// B1 and B2 under the resident policy: closing the last window presents nothing,
	// the application stays alive with no window, and each of the three reopening
	// gestures opens the most recently used profile exactly once.
	settle(300);
	initial                            = nullptr;
	terminal                           = false;
	auto residentOptions               = options;
	residentOptions.EmptyDesktopPolicy = [] { return app::empty_desktop_outcome::stay_resident; };
	desktop                            = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, residentOptions, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "resident desktop startup");
	expect(initial != nullptr, "the resident desktop admits Default");
	initial->show();
	wait_for([&] { return desktop->management_window() != nullptr; }, "desktop management composed");
	auto& resident = *desktop->management_window();
	wait_for([&] { return resident.rename_button()->isEnabled(); }, "the registry is ready for management");
	const auto closedTheLastWindow = [&] {
		wait_for([&] { return desktop->runtime(core::default_bpr_profile_id) == nullptr; },
				 "the last window closes and its profile retires");
		expect(!resident.isVisible(), "closing the last window presents no chooser");
		expect(desktop->last_empty_desktop_outcome() == app::empty_desktop_outcome::stay_resident,
			   "the injected policy decided to stay resident");
		expect(!desktop->is_quiescent(), "so the application is still running with no window");
	};
	const auto reopenedExactlyOnce = [&](const char* aMessage) {
		wait_for([&] { return desktop->runtime(core::default_bpr_profile_id) != nullptr; }, aMessage);
		auto* const runtime = desktop->runtime(core::default_bpr_profile_id);
		QApplication::processEvents();
		expect(desktop->windows(core::default_bpr_profile_id).size() == 1U,
			   "exactly one window, under one runtime owner");
		expect(!resident.isVisible(), "and still no chooser");
		expect(runtime->window().showing_home(), "the reopened window arrives at Home");
		// [[BPR-0796]]. The application's own menu bar belongs to a desktop with no
		// window. Qt hands a parentless menu bar to any focused window that carries
		// none of its own, so one left alive here would take the menu bar from the
		// reopened window the moment its Settings window was focused.
		wait_for([&] { return desktop->desktop_menu() == nullptr; },
				 "and the desktop's own menu bar steps aside once that window is active");
		return runtime;
	};
	// Fetched again after each close rather than held, because the desktop builds it
	// when it empties and retires it when a window becomes active.
	const auto residentMenuBar = [&] {
		auto* const menu = desktop->desktop_menu();
		expect(menu != nullptr && menu->parentWidget() == nullptr,
			   "the resident desktop keeps the application's own menu bar");
		return menu;
	};
	// [[BPR-1304]]. The window's View, read before the close rather than counted
	// here: [[BPR-1295]] and [[BPR-1293]] each added an item to it.
	const auto viewItems = [](const QMenu* const aMenu) {
		auto items = QStringList{ };
		for (const auto* const action : aMenu->actions()) {
			items.push_back(action->isSeparator() ? QStringLiteral("-") : action->text());
		}
		return items;
	};
	expect(initial->window().view_menu() != nullptr, "the window has its View menu");
	const auto windowView = viewItems(initial->window().view_menu());
	initial->window().close();
	closedTheLastWindow();
	auto* desktopMenu = residentMenuBar();
	expect(desktopMenu->profiles_menu()->actions().size() == 5,
		   "which lists the three profiles, New Window and management");
	expect(desktopMenu->view_menu() != nullptr && viewItems(desktopMenu->view_menu()) == windowView
			   && std::ranges::all_of(desktopMenu->view_menu()->actions(), [](const auto* aAction) {
					  return aAction->isSeparator() || !aAction->isEnabled();
				  }),
		   "and carries the window menu's View, offered and disabled, so the close takes no menu off the bar");

	// B3 with no window to carry a refusal: the chooser is presented for it, which is
	// the startup case RFC-BPR-0024 already describes.
	resident.rename_button()->click();
	resident.name_editor()->setText(QString{ });
	resident.save_button()->click();
	wait_for([&] { return resident.isVisible(); }, "a refusal with no window presents the chooser");
	expect(resident.status_text().contains(QStringLiteral("nonblank profile name")), "and says what was refused");
	desktopMenu->manage_profiles_action()->trigger();
	wait_for([&] { return resident.rename_button()->isEnabled(); }, "management from the menu bar reloads profiles");
	resident.hide();

	desktopMenu->new_window_action()->trigger();
	auto* const byMenuBar = reopenedExactlyOnce("New Window on the application's menu bar reopens the most recently used profile");

	// B3 with a live window: the refusal is reported there and the chooser stays away.
	resident.rename_button()->click();
	resident.name_editor()->setText(QString{ });
	resident.save_button()->click();
	wait_for([&] { return byMenuBar->window().status_text().contains(QStringLiteral("nonblank profile name")); },
			 "a refusal with a live window is reported in that window");
	expect(!resident.isVisible(), "and the chooser is not presented to carry it");
	auto* const windowStatus = byMenuBar->window().findChild<QAction*>(QStringLiteral("profileManagementStatusAction"));
	expect(windowStatus != nullptr && windowStatus->isVisible(), "the window's own Profiles menu carries it too");
	byMenuBar->window().findChild<QAction*>(QStringLiteral("manageProfilesAction"))->trigger();
	wait_for([&] { return resident.rename_button()->isEnabled(); }, "management from the window reloads profiles");
	resident.hide();

	byMenuBar->window().close();
	closedTheLastWindow();
	static_cast<void>(residentMenuBar());
	expect(QMetaObject::invokeMethod(qApp, "applicationStateChanged", Qt::DirectConnection,
									 Q_ARG(Qt::ApplicationState, Qt::ApplicationActive)),
		   "the platform's own activation signal is deliverable");
	auto* const byActivation = reopenedExactlyOnce("activating the resident application reopens the most recently used profile");

	byActivation->window().close();
	closedTheLastWindow();
	desktopMenu            = residentMenuBar();
	auto* const defaultRow = desktopMenu->findChild<QAction*>(QStringLiteral("desktopOpenBprProfile_1"));
	expect(defaultRow != nullptr && defaultRow->isEnabled(), "the Profiles menu offers the profile by name");
	defaultRow->trigger();
	static_cast<void>(reopenedExactlyOnce("choosing the profile from the Profiles menu reopens it"));
	desktop->quit(0);
	wait_for([&] { return desktop->is_quiescent(); }, "the resident desktop still quits when asked");
	desktop.reset();
	settle(300);

	// B1 under the quit policy: the same close ends the application through the
	// existing drain and Quit path, with no chooser presented on the way out.
	initial                        = nullptr;
	terminal                       = false;
	auto quitOptions               = options;
	quitOptions.EmptyDesktopPolicy = [] { return app::empty_desktop_outcome::quit; };
	desktop                        = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, quitOptions, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "quitting-policy desktop startup");
	expect(initial != nullptr, "it admits Default");
	initial->show();
	wait_for([&] { return desktop->management_window() != nullptr; }, "its management is composed");
	const auto quitWindow = desktop->windows(core::default_bpr_profile_id).front().Window;
	initial->window().close();
	wait_for([&] { return desktop->is_quiescent(); }, "closing the last window quits the application");
	expect(desktop->last_empty_desktop_outcome() == app::empty_desktop_outcome::quit,
		   "through the injected policy rather than a platform test in the desktop");
	expect(!desktop->management_window()->isVisible(), "with no chooser presented on the way out");
	expect(desktop->exit_code() == 0, "and an orderly exit code");
	desktop.reset();
	settle(300);

	// RFC-BPR-0024's restore records still tell this close from a Quit: an explicitly
	// closed window is not restored, so the next launch opens a new window identity.
	initial  = nullptr;
	terminal = false;
	desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, residentOptions, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "relaunch after the quitting close");
	expect(initial != nullptr, "the relaunch admits Default");
	initial->show();
	QApplication::processEvents();
	const auto relaunched = desktop->windows(core::default_bpr_profile_id);
	expect(relaunched.size() == 1U, "one window opens");
	expect(relaunched.front().Window != quitWindow,
		   "and it is a new window rather than the explicitly closed one restored");
	desktop->quit(0);
	wait_for([&] { return desktop->is_quiescent(); }, "the relaunched desktop quits");
	desktop.reset();

	// [[BPR-1045]]. The entry point refuses a start the desktop already accepted when
	// its own run setup throws. It does that from inside the startup completion, with
	// the window open, and the management host refuses a Quit made while it delivers,
	// which is where that completion runs. The failure quit still ends the desktop,
	// with its exit code, under the resident policy that would otherwise keep it.
	settle(300);
	initial  = nullptr;
	terminal = false;
	desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, residentOptions, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				initial = *aResult;
				desktop->quit(EXIT_FAILURE);
			}
		});
	wait_for([&] { return terminal; }, "a start the composition goes on to refuse");
	expect(initial != nullptr, "the desktop accepted it first");
	wait_for([&] { return desktop->is_quiescent(); }, "a failure quit made in the startup completion ends the desktop");
	expect(desktop->exit_code() == EXIT_FAILURE, "with the failure's exit code");
	desktop.reset();
#if !defined(_WIN32)
	// [[BPR-0817]] RFC-BPR-0033's owner-delegated amendment. The data root sits in a
	// folder that existed before the install and is group-writable. An earlier
	// install under umask 0002 leaves `~/.local/share/BetterPullReview` like that.
	// Recovery still refuses to put agent workspaces beside the root and repairs
	// nothing. The window says so in its status bar at Home, and closing the last
	// window under the quit policy is still an orderly exit.
	{
		settle(300);
		const auto sharedParent  = root.parent_path() / "shared-parent";
		const auto sharedRoot    = sharedParent / "BetterPullReview";
		const auto neighbourhood = sharedParent / "BetterPullReview-agent-workspaces-v1";
		const auto groupWritable = static_cast<std::filesystem::perms>(0775);
		expect(std::filesystem::create_directory(sharedParent), "a data folder's parent that existed before the install");
		std::filesystem::permissions(sharedParent, groupWritable, std::filesystem::perm_options::replace);
		expect(std::filesystem::status(sharedParent).permissions() == groupWritable, "that parent is group-writable");
		expect(adapters::sqlite::create_storage_root_owner_private(sharedRoot), "the data root itself is created private");
		initial                           = nullptr;
		terminal                          = false;
		auto sharedOptions                = quitOptions;
		sharedOptions.ApplicationDataRoot = sharedRoot;
		desktop                           = app::detail::github_application_fixture_access::start_desktop(
			app::detail::github_public_path_fixture::github_com, sharedOptions, [&](auto aResult) {
				terminal = true;
				if (aResult) {
					initial = *aResult;
				}
			});
		wait_for([&] { return terminal; }, "a desktop on the group-writable parent starts");
		expect(initial != nullptr, "and admits Default, because only the agent workspaces are refused");
		initial->show();
		wait_for([&] { return initial->window().showing_home()
						   && initial->window().status_text().contains(QStringLiteral("Disposable agent workspaces are unavailable")); },
				 "the window's status bar says at Home that agent workspaces are unavailable");
		expect(!std::filesystem::exists(neighbourhood), "no workspace neighbourhood is created under the shared parent");
		// A window opened after the refusal says it too, and closing it leaves the
		// first window, so the course still ends on a last-window close.
		const auto first     = desktop->windows(core::default_bpr_profile_id).front();
		auto*      newWindow = initial->window().findChild<QAction*>(QStringLiteral("newBprWindowAction"));
		expect(newWindow != nullptr, "New Window is composed");
		wait_for([&] { return newWindow->isEnabled(); }, "New Window becomes available");
		newWindow->trigger();
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 2U; }, "New Window opens a second window of the profile");
		const auto contexts = desktop->windows(core::default_bpr_profile_id);
		auto*      later    = desktop->window(contexts.front() == first ? contexts.back() : contexts.front());
		expect(later != nullptr && later != &initial->window(), "the second window is a distinct window");
		wait_for([&] { return later->showing_home()
						   && later->status_text().contains(QStringLiteral("Disposable agent workspaces are unavailable")); },
				 "a window opened after the refusal says it at Home too");
		later->close();
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 1U; }, "closing the second window leaves the first");
		initial->window().close();
		wait_for([&] { return desktop->is_quiescent(); }, "closing its last window quits the application");
		expect(desktop->last_empty_desktop_outcome() == app::empty_desktop_outcome::quit, "through the quit policy");
		expect(desktop->exit_code() == 0, "and the refused recovery does not make the orderly quit exit 1");
		expect(std::filesystem::status(sharedParent).permissions() == groupWritable, "nothing repairs a folder the application did not create");
		expect(!std::filesystem::exists(neighbourhood), "and still no workspace neighbourhood exists under it");
		desktop.reset();
	}
#endif
	std::cout << "profile-management-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
