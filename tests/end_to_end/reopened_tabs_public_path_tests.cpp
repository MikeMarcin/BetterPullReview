#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const std::string& aMessage, const std::function<void()>& aReport = { })
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aReport) {
			aReport();
		}
		expect(aCondition(), aMessage);
	}

	void settle()
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 150) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	/// The strip's tabs, as the reviewer reads them: each file's name, and
	/// which one is active.
	[[nodiscard]] auto strip_names(const qt::workbench_window& aWindow) -> std::vector<std::string>
	{
		auto names = std::vector<std::string>{ };
		for (const auto& tab : aWindow.document_tabs()->tabs()) {
			names.push_back(tab.Name.toStdString() + (tab.Active ? "*" : ""));
		}
		return names;
	}

	void report(app::github_application& aApplication, const qt::workbench_window& aWindow, const char* aWhere)
	{
		const auto evidence = access::reopened_tabs(aApplication);
		std::cerr << "reopened-tabs:" << aWhere << " held=" << evidence.Held << " shown=" << evidence.ShownPath;
		if (evidence.Last) {
			std::cerr << " restored=" << evidence.Last->Restored << " dropped=" << evidence.Last->Dropped << " tabs=" << evidence.Last->Tabs.Tabs.size();
		}
		std::cerr << " strip=";
		for (const auto& name : strip_names(aWindow)) {
			std::cerr << name << ',';
		}
		std::cerr << '\n';
	}

	/// The profile's desktop, which keeps the window restoration store
	/// where a pull request's tabs are remembered, signed in on Home.
	struct session
	{
		std::unique_ptr<app::profile_desktop> Desktop;
		app::github_application*              Owner  = nullptr;
		qt::workbench_window*                 Window = nullptr;
	};

	[[nodiscard]] auto start(const std::filesystem::path& aRoot) -> session
	{
		auto options                   = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot    = aRoot;
		options.RegisteredDeviceCourse = true;
		options.IndependentWindowReads = true;
		options.RepeatReadSessions     = true;
		options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
		auto result                    = session{ };
		auto terminal                  = false;
		result.Desktop                 = access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				result.Owner = *aResult;
			}
		});
		wait_for([&] { return terminal; }, "reopened tabs course startup completes");
		expect(result.Owner != nullptr, "reopened tabs course profile admitted");
		result.Owner->show();
		wait_for([&] { return result.Owner->windows().size() == 1U; }, "the window is admitted");
		result.Window = result.Owner->window(result.Owner->windows().front());
		result.Window->resize(1280, 860);
		result.Window->home_landing()->primary_button()->click();
		wait_for([&] { return result.Window->session_panel()->status() == core::github_status::session_ready; }, "Home sign-in reaches an account session");
		return result;
	}

	/// Opens the pull request by the bar: not Resume, which brings its own tabs.
	void open(qt::workbench_window& aWindow)
	{
		aWindow.choose_home(true);
		aWindow.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
		aWindow.home_open_action()->trigger();
		wait_for([&] { return !aWindow.showing_home() && aWindow.next_file_action()->isEnabled(); }, "the pull request opens");
	}

	void stop(session& aSession)
	{
		aSession.Owner->begin_shutdown();
		wait_for([&] { return aSession.Owner->status().ShutdownComplete; }, "reopened tabs course drains all commands");
		// [[BPR-2215]]. The desktop is quit and drained before it goes, as an
		// application quits it: destroyed at once, it released the profile
		// after the next start asked for it, which refused the restart (2 of
		// 10 on macOS).
		aSession.Desktop->quit();
		wait_for([&] { return aSession.Desktop->is_quiescent(); }, "the reopened tabs course's desktop drains");
		aSession.Desktop.reset();
		aSession.Owner  = nullptr;
		aSession.Window = nullptr;
	}

	/// Back in Review, the remembered tabs are on the strip in their order
	/// with the remembered tab active, and the comparison shows its file.
	void expect_returned(app::github_application& aApplication, qt::workbench_window& aWindow, const std::vector<std::string>& aStrip, const char* aShown, const char* aWhen)
	{
		aWindow.choose_modality(qt::review_modality::review);
		wait_for([&] { return strip_names(aWindow) == aStrip && access::reopened_tabs(aApplication).ShownPath == aShown; },
				 std::string{ "in Review the tabs are back in order with the same tab active, and its file is shown: " } + aWhen,
				 [&] { report(aApplication, aWindow, aWhen); });
	}

	/// [[BPR-2324]]. A pull request opened again, by the bar, brings back the
	/// tabs it had: in the same session after Close review, and after a
	/// restart. A remembered tab whose file the pull request no longer has is
	/// dropped quietly and counted.
	void run_reopened_tabs_course()
	{
		QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-reopened-tabs-XXXXXX") };
		const auto    dataRoot = e2e::private_data_root(temporary);
		expect(dataRoot.has_value(), "private reopened tabs course root");

		auto  running     = start(*dataRoot);
		auto* application = running.Owner;
		auto* window      = running.Window;
		open(*window);
		expect(!access::reopened_tabs(*application).Last && !access::reopened_tabs(*application).Held, "a pull request seen for the first time has no tabs to bring back");

		// Three tabs: review.cpp, then secondary.cpp, then a second view of
		// secondary.cpp; the middle one is active when the review closes.
		wait_for([&] { return access::reveal_fixture_file(*application, "review.cpp") == app::detail::fixture_file_reveal_result::shown; }, "review.cpp is shown in Review");
		auto* strip = window->document_tabs();
		wait_for([&] { return strip->tabs().size() == 1U; }, "review.cpp has the first tab");
		strip->new_tab_button()->click();
		wait_for([&] { return strip->tabs().size() == 2U && strip->tabs()[1].Active; }, "the plus opens a second tab");
		wait_for([&] { return access::reveal_fixture_file(*application, "secondary.cpp") == app::detail::fixture_file_reveal_result::shown; }, "secondary.cpp is shown in the second tab");
		strip->new_tab_button()->click();
		wait_for([&] { return strip->tabs().size() == 3U && strip->tabs()[2].Active; }, "the plus opens a third tab");
		strip->tab_bar()->setCurrentIndex(1);
		const auto before = std::vector<std::string>{ "review.cpp", "secondary.cpp*", "secondary.cpp" };
		wait_for([&] { return strip_names(*window) == before && access::reopened_tabs(*application).ShownPath == "secondary.cpp"; }, "the middle tab is active",
				 [&] { report(*application, *window, "before"); });
		settle();
		expect(access::remember_missing_tab(*application, "gone-since.cpp"), "a fourth tab is remembered on a file the pull request does not have");

		// Close review, then the same pull request again by the bar.
		window->home_action()->trigger();
		settle();
		auto* const landing = window->home_landing();
		expect(landing != nullptr && landing->state() == qt::landing_state::back_to_review, "Home over the review offers Close review");
		landing->close_button()->click();
		wait_for([&] { return landing->state() != qt::landing_state::back_to_review && strip->tabs().empty(); }, "Close review releases the tabs");
		open(*window);
		wait_for([&] { const auto e = access::reopened_tabs(*application); return e.Last.has_value() && !e.Held; }, "the reopened pull request brings its tabs back once its files are known",
				 [&] { report(*application, *window, "reopen"); });
		const auto reopened = *access::reopened_tabs(*application).Last;
		expect(reopened.Restored == 3U && reopened.Dropped == 1U, "three tabs come back and the one whose file is gone is dropped");
		expect(window->current_modality() != qt::review_modality::review && strip->tabs().size() == 3U && strip->tabs()[1].Active,
			   "the pull request opens where the bar opens it, its tabs already on the strip");
		expect_returned(*application, *window, before, "secondary.cpp", "same session");
		settle();
		stop(running);

		// A restart: the tabs are kept with the pull request's local state.
		running     = start(*dataRoot);
		application = running.Owner;
		window      = running.Window;
		open(*window);
		wait_for([&] { const auto e = access::reopened_tabs(*application); return e.Last.has_value() && !e.Held; }, "after a restart the pull request brings its tabs back",
				 [&] { report(*application, *window, "restart"); });
		const auto restarted = *access::reopened_tabs(*application).Last;
		expect(restarted.Restored == 3U && restarted.Dropped == 0U, "the dropped tab stayed dropped, and the other three come back after a restart");
		expect_returned(*application, *window, before, "secondary.cpp", "after a restart");
		stop(running);
		expect(e2e::remove_course_root(temporary), "reopened tabs course root removed");
		std::cout << "reopened tabs course passed\n"
				  << std::flush;
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication qtApplication{ aArgc, aArgv };
	run_reopened_tabs_course();
	return EXIT_SUCCESS;
}
