// [[BPR-1576]]. Panes say when their data is not ready, through the composed
// application on a slow fixture network: Home's list while discovery first
// finds my pull requests, then the Overview opening a pull request section by
// section (the owner: "This is especially true of the overview page when
// opening a PR"), and again for another pull request in the same window. Each
// section waits in its own place, says what it waits on once slow, shows as
// its part arrives, and the Overview says once when it is ready. Nothing
// reaches a live provider.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const int aTimeoutMs = 30000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	/// Lets the event loop run for a while, as a person waiting would.
	void pause(const int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
	}

	struct found_row
	{
		QListView*  View = nullptr;
		QModelIndex Index;
	};

	[[nodiscard]] auto row_of(qt::workbench_window& aWindow, const QString& aIdentity) -> found_row
	{
		for (const auto group : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
			auto* const view = aWindow.home_list()->state_view(group);
			for (auto row = 0; view != nullptr && view->model() != nullptr && row < view->model()->rowCount(); ++row) {
				const auto index = view->model()->index(row, 0);
				if (index.data(qt::home_row_role::identity).toString() == aIdentity) {
					return { view, index };
				}
			}
		}
		return { };
	}

	void double_click(const found_row& aRow)
	{
		const auto point   = QPointF{ aRow.View->visualRect(aRow.Index).center() };
		const auto global  = aRow.View->viewport()->mapToGlobal(point);
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		auto       doubled = QMouseEvent{ QEvent::MouseButtonDblClick, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aRow.View->viewport(), &press);
		QApplication::sendEvent(aRow.View->viewport(), &release);
		QApplication::sendEvent(aRow.View->viewport(), &doubled);
		QApplication::sendEvent(aRow.View->viewport(), &release);
	}

	std::vector<QString>* Announcements = nullptr;

	void capture(QAccessibleEvent* const aEvent)
	{
		if (Announcements != nullptr && aEvent != nullptr && aEvent->type() == QAccessible::Announcement) {
			Announcements->push_back(static_cast<const QAccessibleAnnouncementEvent*>(aEvent)->message());
		}
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    renderArgument  = QApplication::arguments().indexOf(QStringLiteral("--render-directory"));
	const auto    renderDirectory = renderArgument >= 0 ? QApplication::arguments().value(renderArgument + 1) : QString{ };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private pending-states course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto delay                     = std::make_shared<app::detail::github_read_delay_fixture>();
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	options.ReadDelay              = delay;
	options.PersonalDiscovery      = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = {
			{ .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" } },
			{ .Number = 18U, .Author = "contributor-b", .RequestedUsers = { "octo-reviewer" } },
		},
	};
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "profile owner startup completes");
	expect(owner != nullptr, "profile owner admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "the window is admitted");
	auto* const window = owner->window(owner->windows().front());
	window->resize(1280, 800);
	const auto renders = [&](const QString& aName) {
		if (renderDirectory.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(renderDirectory), "render directory exists");
		const auto mode = QString::fromUtf8(core::stable_name(owner->status().ProfileMode));
		expect(window->grab().save(renderDirectory + QStringLiteral("/%1-%2.png").arg(mode, aName)), "render saved");
	};
	std::vector<QString> heard;
	Announcements       = &heard;
	const auto previous = QAccessible::installUpdateHandler(capture);

	// ---- Home's list while discovery first finds my pull requests -------------
	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "Home sign-in reaches an account session");
	delay->DiscoveryMilliseconds = 2'500U;
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	window->settings()->hide();
	window->choose_home(true);
	auto* const homeVeil = window->pending_veils().HomeList;
	wait_for([&] { return homeVeil->look() == core::pending_look::loading || homeVeil->look() == core::pending_look::loading_slow; },
			 "with nothing listed yet, Home's list says it is finding my pull requests");
	expect(homeVeil->shown_text().startsWith(QStringLiteral("Finding my pull requests in 1 saved repository")), "in words naming what it searches");
	expect(homeVeil->full_motion() == window->pending_full_motion(), "the list turns only if the system allows motion");
	renders(QStringLiteral("home-first-find"));
	const auto first  = QStringLiteral("octo-org/review-repository#17");
	const auto second = QStringLiteral("octo-org/review-repository#18");
	wait_for([&] { return row_of(*window, first).View != nullptr && row_of(*window, second).View != nullptr; }, "the pull requests are listed");
	wait_for([&] { return homeVeil->look() == core::pending_look::ready; }, "and the list is ready");
	delay->DiscoveryMilliseconds = 0U;

	// ---- the Overview opening a pull request, section by section --------------
	const auto opens = [&](const QString& aIdentity, const QString& aName, const bool aAssert) {
		window->choose_home(true);
		pause(100);
		heard.clear();
		delay->Milliseconds     = 1'500U;
		delay->PageMilliseconds = 1'200U;
		double_click(row_of(*window, aIdentity));
		wait_for([&] { return !window->showing_home(); }, "the Overview shows once GitHub names the pull request");
		const auto veils = window->overview_page()->pending_veils();
		if (aAssert) {
			expect(veils.Files->busy() && veils.Side->busy(), "the file table and the side column wait in their own places");
		}
		renders(aName + QStringLiteral("-first-answer"));
		wait_for([&] { return !veils.Header->busy() && !veils.Description->busy(); }, "the title and description show as soon as GitHub names them");
		if (aAssert) {
			expect(veils.Files->busy() && veils.Files->words().Loading.contains(QStringLiteral("reported")), "the file table says how many files GitHub reported");
		}
		wait_for([&] { return veils.Files->look() == core::pending_look::loading_slow || !veils.Files->busy(); }, "the files wait on", 30'000);
		if (aAssert && veils.Files->busy()) {
			expect(veils.Files->shown_text().startsWith(QStringLiteral("Still waiting for GitHub")), "a slow wait says what the read is on");
			renders(aName + QStringLiteral("-slow"));
		}
		wait_for([&] { return window->overview_page()->projection().has_value() && !veils.Files->busy() && !veils.Side->busy(); }, "the accepted review makes every section ready", 60'000);
		renders(aName + QStringLiteral("-ready"));
		if (aAssert) {
			expect(std::ranges::count(heard, QStringLiteral("Overview ready: files, reviewers and checks are shown.")) == 1, "the Overview says once that it is ready");
		}
		delay->Milliseconds     = 0U;
		delay->PageMilliseconds = 0U;
	};
	auto asserted = false;
	for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark }) {
		window->appearance_settings()->choose_mode(mode);
		wait_for([&] { return owner->status().ProfileMode == mode; }, "the rendered appearance is accepted");
		window->settings()->hide();
		opens(first, QStringLiteral("overview-open"), !asserted);
		// Another pull request in the same window: nothing of the first shows
		// while the second's sections wait.
		opens(second, QStringLiteral("overview-switch"), !asserted);
		asserted = true;
	}

	static_cast<void>(QAccessible::installUpdateHandler(previous));
	Announcements = nullptr;
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "pending-states-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "pending-states-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
