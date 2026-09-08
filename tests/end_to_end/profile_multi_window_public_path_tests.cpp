#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>

namespace {
	using namespace bpr;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { }, const int aTimeoutMs = 15000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aDiagnostic) {
			aDiagnostic();
		}
		expect(aCondition(), aMessage);
	}
	auto action(qt::workbench_window& aWindow, const char* aName) -> QAction*
	{
		auto* result = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(result != nullptr, "public profile action exists");
		wait_for([&] { return result->isEnabled(); }, "public profile action becomes enabled");
		return result;
	}
	auto visible_settings() -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (dynamic_cast<qt::settings_window*>(widget) != nullptr && widget->isVisible()) {
				++count;
			}
		}
		return count;
	}
	void capture(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MULTI_WINDOW_ARTIFACT_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "multiwindow artifact directory");
			expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName), "multiwindow rendered artifact");
		}
	}
	void select_file(app::github_application& aOwner, qt::workbench_window& aWindow)
	{
		wait_for([&] {
			for (const auto context : aOwner.windows()) {
				if (aOwner.window(context) == &aWindow) {
					const auto status = aOwner.status(context);
					return status && status->EpochStatus == core::epoch_status::ready;
				}
			}
			return false; },
				 "exact window comparison epoch becomes ready", [&] {
            for (const auto context : aOwner.windows()) {
                if (aOwner.window(context) != &aWindow){ continue;
}
                const auto status = aOwner.status(context);
                const auto work = app::detail::github_application_fixture_access::diff_work_evidence(aOwner, context);
                std::cerr << "epoch=" << (status ? static_cast<int>(status->EpochStatus) : -1)
                          << " outstanding=" << work.EpochOutstandingJobs
                          << " navigation=" << work.NavigationReady
                          << " workbench=" << work.WorkbenchOutstandingRequests
                          << " status=" << aWindow.status_text().toStdString() << '\n';
            } }, 60000);
		aWindow.modality()->action(qt::review_modality::read)->trigger();
		auto* files = aWindow.files_drawer()->contents_view();
		wait_for([&] { return files->model()->rowCount() > 0; }, "accepted review lists files");
		files->selectionModel()->setCurrentIndex(files->model()->index(0, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait_for([&] {
			auto* surface = aWindow.comparison_surface();
			return surface->current_result() && surface->current_semantic_index() && !surface->geometry_pending() && surface->isVisible(); },
				 "selected file produces an actual comparison", [&] {
                auto* surface = aWindow.comparison_surface();
                std::cerr << "comparison status=" << aWindow.status_text().toStdString()
                          << " result=" << bool(surface->current_result())
                          << " semantic=" << bool(surface->current_semantic_index())
                          << " geometry-pending=" << surface->geometry_pending()
                          << " visible=" << surface->isVisible() << '\n';
                capture(aWindow, QStringLiteral("comparison-failure.png")); }, 60000);
	}
	void submit(qt::workbench_window& aWindow, const int aNumber)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		aWindow.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/%1").arg(aNumber));
		aWindow.home_open_action()->trigger();
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private multiwindow test root");
	const auto root                   = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
	auto       options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = root;
	options.RegisteredDeviceCourse    = true;
	options.IndependentWindowReads    = true;
	options.RepeatReadSessions        = true;
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			} else {
				std::cerr << "startup failure: " << static_cast<int>(aResult.error()) << '\n';
			}
		});
	wait_for([&] { return terminal; }, "profile owner startup completes");
	expect(owner != nullptr, "profile owner admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "first authoritative window admitted");
	const auto first   = owner->windows().front();
	auto*      windowA = owner->window(first);
	expect(windowA != nullptr && windowA->isVisible(), "first public window visible");
	submit(*windowA, 17);
	auto* signIn = windowA->findChild<QPushButton*>(QStringLiteral("homeSetupContinue"));
	expect(signIn != nullptr && signIn->isVisible(), "first review offers explicit sign in");
	signIn->click();
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::pull_request_ready; }, "first review opens through public setup", [&] {
		const auto evidence = app::detail::github_application_fixture_access::evidence(*owner);
		std::cerr << "status=" << windowA->session_panel()->status_text()
			<< " diagnostic=" << windowA->session_panel()->diagnostic_text()
			<< " notice=" << windowA->home_open_notice()->text().toStdString()
			<< " vault=" << evidence.VaultStores << " bootstrap=" << evidence.BootstrapTransportSelected
			<< " account=" << evidence.AccountTransportSelected << " reads=" << evidence.AccountReadCount
			<< " last-read=" << evidence.LastReadOperation << " wire-error=" << evidence.RecordingFailure << '\n'; });

	action(*windowA, "newBprWindowAction")->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "New Window admits a second session");
	const auto contexts = owner->windows();
	const auto second   = contexts.front() == first ? contexts.back() : contexts.front();
	expect(second != first && second.Profile == first.Profile, "distinct windows share exact profile identity");
	auto* windowB = owner->window(second);
	expect(windowB != nullptr && windowB != windowA && windowB->isVisible(), "second real Qt window visible");
	expect(windowB->showing_home(), "new window starts on Home without copying sibling review");
	expect(desktop->runtime(first.Profile) == owner && desktop->window(second) == windowB, "one profile owner routes both exact contexts");
	windowA->resize(1100, 700);
	windowB->resize(1400, 900);
	QApplication::processEvents();
	const auto sizeA = windowA->size();
	const auto sizeB = windowB->size();
	expect(sizeA != sizeB, "window geometry is independent");

	windowA->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "first Settings gesture opens one shell");
	windowB->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "second Settings gesture reuses profile shell");
	auto* mode = windowB->appearance_settings()->mode_selector();
	mode->setCurrentIndex(mode->findData(static_cast<int>(core::appearance_mode::high_contrast_dark)));
	wait_for([&] {
		const auto statusA = owner->status(first);
		const auto statusB = owner->status(second);
		return statusA && statusB && statusA->ProfileMode == core::appearance_mode::high_contrast_dark && statusB->ProfileMode == statusA->ProfileMode;
	},
			 "accepted profile preference reaches both windows");
	expect(windowA->size() == sizeA && windowB->size() == sizeB, "shared preferences preserve independent geometry");
	submit(*windowB, 18);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "second window opens distinct recorded PR using shared authentication");
	expect(windowA->session_panel()->status() == core::github_status::pull_request_ready, "second read preserves first review");

	expect(app::detail::github_application_fixture_access::evidence(*owner).VaultStores == 1U, "second window reuses accepted authentication without enrolling again");
	const auto snapshotA = app::detail::github_application_fixture_access::large_review_snapshot(*owner, first);
	const auto snapshotB = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
	expect(snapshotA && snapshotB && snapshotA->PullRequest.Number.value() == 17U && snapshotB->PullRequest.Number.value() == 18U,
		   "accepted read snapshots retain distinct PR identities");
	expect(snapshotA->Account == snapshotB->Account && snapshotA->Profile == snapshotB->Profile, "both reviews use shared account authority");
	select_file(*owner, *windowA);
	select_file(*owner, *windowB);
	const auto comparisonA = windowA->comparison_surface()->current_result();
	const auto comparisonB = windowB->comparison_surface()->current_result();
	expect(comparisonA->Alignment.Key.PullRequest == snapshotA->PullRequest.PullRequest && comparisonB->Alignment.Key.PullRequest == snapshotB->PullRequest.PullRequest, "each displayed comparison names its accepted PR");
	expect(comparisonA->Alignment.Key != comparisonB->Alignment.Key, "different PR windows retain distinct exact comparison keys");
	const auto selectionB = windowB->comparison_surface()->selection();
	expect(windowA->comparison_surface()->navigate_change(core::change_navigation_direction::next, core::change_navigation_policy::stop), "first window supports semantic navigation");
	expect(windowA->comparison_surface()->selection().has_value(), "first window receives semantic selection");
	expect(windowB->comparison_surface()->selection() == selectionB && windowB->comparison_surface()->current_result() == comparisonB,
		   "navigation in first window preserves second comparison and selection");
	expect(windowA->comparison_surface()->current_result() == comparisonA, "sibling file selection preserved first comparison");
	capture(*windowA, QStringLiteral("window-a.png"));
	capture(*windowB, QStringLiteral("window-b.png"));
	app::detail::github_application_fixture_access::hold_window_read_terminal(*owner, second, true);
	windowB->github_refresh_action()->trigger();
	wait_for([&] { return app::detail::github_application_fixture_access::held_window_read_terminals(*owner, second) > 0U; }, "surviving window has a real read completion pending");
	windowA->close();
	wait_for([&] { return owner->window(first) == nullptr && owner->windows().size() == 1U; }, "closing first window retires only its session");
	expect(desktop->runtime(first.Profile) == owner && owner->window(second) == windowB, "profile services survive first window close");
	expect(!windowA->isVisible(), "accepted close hides initial retained shell while sibling survives");
	app::detail::github_application_fixture_access::hold_window_read_terminal(*owner, second, false);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "surviving read completes after first window closes");
	const auto resumed = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
	expect(resumed && resumed->PullRequest.Number.value() == 18U, "surviving completion retains exact PR identity");
	select_file(*owner, *windowB);
	windowB->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "profile Settings remains usable after first window retirement");
	expect(!owner->status(first).has_value(), "retired exact context has no status");
	auto stale       = second;
	stale.Generation = core::bpr_window_generation{ second.Generation.value() + 1U };
	expect(owner->window(stale) == nullptr && !owner->status(stale), "foreign generation cannot address surviving window");
	action(*windowB, "manageProfilesAction")->trigger();
	wait_for([&] { return desktop->management_window() != nullptr; }, "management remains reachable from survivor");
	auto& management = *desktop->management_window();
	wait_for([&] { return management.open_button()->isEnabled(); }, "Open Profile available for current profile");
	management.open_button()->click();
	QApplication::processEvents();
	expect(owner->windows().size() == 1U && owner->window(second) == windowB, "Open Profile activates survivor without duplicate owner or session");
	action(*windowB, "newBprWindowAction")->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "New Window remains usable after initial retirement");
	qt::workbench_window* windowC = nullptr;
	for (const auto context : owner->windows()) {
		expect(context != first, "retired context is never reused");
		if (context != second) {
			expect(owner->window(context) != nullptr && owner->window(context)->showing_home(), "replacement is a fresh Home session");
			windowC = owner->window(context);
		}
	}
	expect(windowC != nullptr, "replacement window is reachable by its context");
	submit(*windowC, 18);
	wait_for([&] { return windowC->session_panel()->status() == core::github_status::pull_request_ready; }, "replacement opens sibling PR without another sign-in");
	select_file(*owner, *windowC);
	expect(windowB->comparison_surface()->navigate_change(core::change_navigation_direction::next, core::change_navigation_policy::stop)
			   && windowC->comparison_surface()->navigate_change(core::change_navigation_direction::next, core::change_navigation_policy::stop),
		   "both windows select the same review anchor");
	wait_for([&] { return windowB->review_editor()->shown_draft() && windowC->review_editor()->shown_draft(); }, "both actual editors receive a local draft");
	expect(windowB->review_editor()->shown_draft()->Anchor == windowC->review_editor()->shown_draft()->Anchor, "shared draft course uses identical comparison and line");
	windowB->review_editor()->body_editor()->setPlainText(QStringLiteral("Shared profile draft from surviving window"));
	wait_for([&] { return windowC->review_editor()->body_editor()->toPlainText() == QStringLiteral("Shared profile draft from surviving window"); }, "accepted draft edit reaches sibling editor");
	windowC->review_editor()->body_editor()->setPlainText(QStringLiteral("Shared profile draft edited in replacement"));
	wait_for([&] { return windowB->review_editor()->body_editor()->toPlainText() == QStringLiteral("Shared profile draft edited in replacement"); }, "replacement edit returns through shared owner");
	action(*windowB, "quitProfilesAction")->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "Quit drains all sessions and shared profile services");
	desktop.reset();
	std::ifstream     stream{ root / "logs/diagnostics.log" };
	const std::string log{ std::istreambuf_iterator<char>{ stream }, { } };
	const auto        started = log.find("op=application outcome=started");
	const auto        stopped = log.find("op=application outcome=shutdown");
	expect(started != std::string::npos && log.find("op=application outcome=started", started + 1U) == std::string::npos, "one profile services startup");
	expect(stopped != std::string::npos && log.find("op=application outcome=shutdown", stopped + 1U) == std::string::npos, "one profile services shutdown");
	std::cout << "profile-multi-window-public-path:pass\n";
	return EXIT_SUCCESS;
}
