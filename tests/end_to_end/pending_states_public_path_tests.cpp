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
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractItemView>
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

	void double_click(QAbstractItemView* const aView, const QModelIndex& aIndex)
	{
		const auto point   = QPointF{ aView->visualRect(aIndex).center() };
		const auto global  = aView->viewport()->mapToGlobal(point);
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		auto       doubled = QMouseEvent{ QEvent::MouseButtonDblClick, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aView->viewport(), &press);
		QApplication::sendEvent(aView->viewport(), &release);
		QApplication::sendEvent(aView->viewport(), &doubled);
		QApplication::sendEvent(aView->viewport(), &release);
	}

	/// \brief [[BPR-2180]]. Waits until the comparison shows a file's result,
	/// failing at once if the code veil lifts before then; true once it does.
	/// \details The result is the arrival: the veil may stay as the partial
	/// "Colouring syntax…" plate after it, which under load can outlast any
	/// wait and is not the file failing to arrive.
	[[nodiscard]] auto shows_file_without_gap(qt::diff_workbench& aComparison, const char* const aGap) -> bool
	{
		auto* const code  = aComparison.comparison_veil();
		auto        clock = QElapsedTimer{ };
		clock.start();
		while (clock.elapsed() < 90'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 10);
			if (aComparison.current_context().has_value() && aComparison.current_result() != nullptr) {
				return true;
			}
			if (!code->busy()) {
				std::cerr << "comparison at the gap, " << clock.elapsed() << " ms: waiting '" << aComparison.waiting_path().toStdString() << "' held '"
						  << aComparison.held_waiting().toStdString() << "' context " << aComparison.current_context().has_value() << " result "
						  << (aComparison.current_result() != nullptr) << " look " << static_cast<int>(code->look()) << '\n';
			}
			expect(code->busy(), aGap);
			QThread::msleep(5);
		}
		std::cerr << "comparison after 90 s: waiting '" << aComparison.waiting_path().toStdString() << "' held '" << aComparison.held_waiting().toStdString()
				  << "' context " << aComparison.current_context().has_value() << " result " << (aComparison.current_result() != nullptr) << " look "
				  << static_cast<int>(code->look()) << '\n';
		return false;
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
	// [[BPR-2195]]. A failure keeps the host's diagnostic log, which names the
	// stage that failed, after the root is removed.
	const auto keepLogs            = e2e::course_logs_on_failure{ temporary, "pending-states" };
	auto       delay               = std::make_shared<app::detail::github_read_delay_fixture>();
	auto       options             = app::detail::github_public_path_fixture_options{ };
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
	// [[BPR-2216]]. In the shape of its rows, with no words: the status bar
	// says Home is refreshing.
	expect(homeVeil->shown_text().isEmpty() && (window->refresh_freshness_label()->text().startsWith(QStringLiteral("Updating")) || window->refresh_freshness_label()->text().startsWith(QStringLiteral("Refreshing"))),
		   "and only the status bar says it is finding them");
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
		wait_for([&] { return !window->showing_home(); }, "the pull request leaves Home once GitHub names it");
		window->choose_modality(qt::review_modality::overview);
		wait_for([&] { return window->showing_overview() && window->overview_page()->isVisible(); }, "the course reads the pending sections in Overview");
		const auto veils = window->overview_page()->pending_veils();
		if (aAssert) {
			expect(veils.Files->busy() && veils.Side->busy(), "the file table and the side column wait in their own places");
			// [[BPR-2180]]. The Comments drawer and Threads wait as thread
			// cards, and the status bar's line turns the mark while it says
			// the read.
			const auto& panes = window->pending_veils();
			expect(panes.Conversations->busy() && panes.Conversations->shape() == qt::pending_shape::cards, "the Comments drawer waits as thread cards");
			expect(panes.Threads->busy() && panes.Threads->shape() == qt::pending_shape::cards, "Threads waits as thread cards");
			expect(panes.Status != nullptr && panes.Status->turning(), "the status line turns the mark while it says the read");
		}
		renders(aName + QStringLiteral("-first-answer"));
		wait_for([&] { return !veils.Header->busy() && !veils.Description->busy(); }, "the title and description show as soon as GitHub names them");
		if (aAssert) {
			// [[BPR-2216]]. The file table waits without a sentence.
			expect(veils.Files->busy() && veils.Files->shown_text().isEmpty(), "the file table waits without saying what loads");
		}
		wait_for([&] { return veils.Files->look() == core::pending_look::loading_slow || !veils.Files->busy(); }, "the files wait on", 30'000);
		if (aAssert && veils.Files->busy()) {
			// [[BPR-2216]]. The status bar says what the read is on; the
			// section does not say it again.
			expect(!veils.Files->shown_text().contains(QStringLiteral("Still waiting for GitHub")), "a slow wait leaves the read's step to the status bar");
			renders(aName + QStringLiteral("-slow"));
		}
		wait_for([&] { return window->overview_page()->projection().has_value() && window->overview_page()->projection()->Number.value() == aIdentity.section(QLatin1Char('#'), -1).toULongLong() && !veils.Files->busy() && !veils.Side->busy(); }, "the accepted review makes every section ready", 60'000);
		renders(aName + QStringLiteral("-ready"));
		if (aAssert) {
			wait_for([&] { return std::ranges::count(heard, QStringLiteral("Overview ready: files, reviewers and checks are shown.")) == 1; }, "the accepted review announces its readiness");
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

	// ---- [[BPR-2180]] Overview, then Review, while the review still loads -----
	// The owner, 2026-11-14: "going to review the file list should be loaded
	// becuase it's already loaded on overview but now we see loading there
	// instead of the file list." Once the read has reported the files, Review's
	// Files drawer lists them uncovered; a file clicked there is remembered and
	// opened once the review has loaded; and a read that never opens a review
	// leaves none of its files behind.
	{
		const auto covering = [](const qt::pending_veil& aVeil) {
			const auto look = aVeil.look();
			return look == core::pending_look::placeholder || look == core::pending_look::loading || look == core::pending_look::loading_slow;
		};
		const auto  reported = [&] { return window->overview_page()->file_model()->rowCount() > 0 && !window->overview_page()->projection().has_value(); };
		auto* const drawer   = window->files_drawer();
		const auto  open     = [&](const QString& aIdentity) {
			window->choose_modality(qt::review_modality::overview);
			window->choose_home(true);
			pause(100);
			double_click(row_of(*window, aIdentity));
			wait_for([&] { return !window->showing_home(); }, "the pull request leaves Home once GitHub names it");
			window->choose_modality(qt::review_modality::overview);
		};
		delay->Milliseconds     = 1'500U;
		delay->PageMilliseconds = 1'200U;
		open(first);
		const auto veils = window->overview_page()->pending_veils();
		// The side column waits section by section, each in its own place.
		wait_for([&] { return window->overview_page()->isVisible() && veils.MyReview->busy(); }, "the Overview shows its sections waiting");
		expect(veils.MyReview->isVisible() && veils.Merge->busy() && veils.Checks->busy() && veils.People->busy(), "each side section waits in its own place");
		wait_for(reported, "the Overview lists the files GitHub reported", 60'000);
		window->choose_modality(qt::review_modality::review);
		pause(200);
		auto* const files = window->pending_veils().Files;
		auto* const tree  = window->files_tree();
		expect(window->current_modality() == qt::review_modality::review && tree->isVisible(), "Review shows its Files drawer");
		expect(!covering(*files), "Review's Files drawer is not covered once the read has reported the files");
		expect(files->look() == core::pending_look::partial || files->look() == core::pending_look::quiet, "what is still coming is said at its foot");
		expect(tree->model()->rowCount() > 0 && drawer->contents()->rowCount() > 0, "the reported files are the drawer's own rows, in the list and the tree");
		renders(QStringLiteral("review-files-while-loading"));
		// A click on a reported file is remembered, said, and opened later.
		const auto selections  = window->statistics().FileSelections;
		const auto opensBefore = window->statistics().PendingOpensOpened;
		drawer->contents_view()->setCurrentIndex(drawer->contents()->index(0, 0));
		pause(50);
		expect(files->words().Coming.startsWith(QStringLiteral("Opening ")), "the drawer says it opens the clicked file once the review has loaded");
		expect(window->statistics().FileSelections == selections, "nothing is asked of a comparison that does not exist yet");
		// The owner, rejecting v002: "the main diff file pane, no longer shows
		// the loading." The comparison waits in the shape of code from the
		// click until the file arrives, with no gap between.
		auto* const comparison = window->comparison_surface();
		auto* const code       = comparison->comparison_veil();
		expect(code->busy() && code->shape() == qt::pending_shape::code && comparison->waiting_path() == drawer->contents()->index(0, 0).data().toString(),
			   "the comparison waits for the clicked file in the shape of code");
		expect(shows_file_without_gap(*comparison, "the code veil shows until the file arrives"), "the clicked file arrives");
		// [[BPR-2191]]. The host opened it, once the review that reported it
		// listed it; the window only showed what the host published.
		expect(window->overview_page()->projection().has_value() && window->statistics().PendingOpensOpened > opensBefore && window->statistics().FileSelections == selections,
			   "it opened once the review listed it");
		expect(!files->busy() && !files->isVisible(), "the accepted review leaves the drawer with no veil");
		// A read that is superseded and then fails opens no review: the files
		// it reported go, and the drawer lists the review still shown.
		const auto paths = [&] {
			auto listed = QStringList{ };
			for (int row = 0; row < drawer->contents()->rowCount(); ++row) {
				listed.push_back(drawer->contents()->index(row, 0).data().toString());
			}
			return listed;
		};
		delay->Milliseconds     = 0U;
		delay->PageMilliseconds = 0U;
		open(second);
		wait_for([&] { return window->overview_page()->projection().has_value() && window->overview_page()->projection()->Number.value() == 18U && drawer->contents()->rowCount() > 0; }, "the second pull request opens");
		const auto shown        = paths();
		delay->Milliseconds     = 1'500U;
		delay->PageMilliseconds = 1'200U;
		open(first);
		// [[BPR-2216]]. The drawer lists them again, waiting without a sentence.
		wait_for([&] { return window->pending_veils().Files->busy() && window->pending_veils().Files->look() == core::pending_look::partial && window->pending_veils().Files->shown_text().isEmpty(); }, "the first pull request's files are reported again", 60'000);
		const auto provisional = paths();
		std::cerr << "shown " << shown.join(QLatin1Char{ ',' }).toStdString() << " provisional " << provisional.join(QLatin1Char{ ',' }).toStdString() << '\n';
		delay->FailNumber = 18U;
		open(second);
		wait_for([&] { return window->session_panel()->status() != core::github_status::loading_pull_request; }, "the superseding read fails", 60'000);
		pause(300);
		expect(window->pending_veils().Files->words().Coming.isEmpty() || !window->pending_veils().Files->busy(), "nothing reported stays said");
		expect(paths() == shown, "a read that opened no review leaves none of its reported files behind");
		delay->FailNumber       = 0U;
		delay->Milliseconds     = 0U;
		delay->PageMilliseconds = 0U;
	}

	// ---- [[BPR-2180]] A double-click on the Overview while the review loads ---
	// The owner, 2026-11-15: "When I double click a file on overview I expect an
	// immediate transition to review with that loading view". Within one turn
	// of the event loop Review is current and the comparison waits for the
	// file in the shape of code, without a gap, until it arrives. And: "I
	// don't know why the respositories drawer suddenly pops open after its
	// done loading": no drawer opens by itself when the load ends.
	{
		window->choose_modality(qt::review_modality::overview);
		window->choose_home(true);
		pause(100);
		delay->Milliseconds     = 1'500U;
		delay->PageMilliseconds = 1'200U;
		double_click(row_of(*window, first));
		wait_for([&] { return !window->showing_home(); }, "the pull request leaves Home once GitHub names it");
		window->choose_modality(qt::review_modality::overview);
		auto* const overview = window->overview_page();
		wait_for([&] { return overview->file_model()->rowCount() > 0 && !overview->projection().has_value(); }, "the Overview lists the reported files", 60'000);
		auto* const table  = overview->files_view();
		auto        listed = QModelIndex{ };
		for (auto parent = QModelIndex{ }; !listed.isValid() && table->model()->rowCount(parent) > 0;) {
			const auto index = table->model()->index(0, 0, parent);
			if (table->model()->hasChildren(index)) {
				table->expand(index);
				parent = index;
			} else {
				listed = index;
			}
		}
		expect(listed.isValid(), "the Overview names a file");
		const auto path = listed.data(qt::overview_file_model::path_role).toString();
		double_click(table, listed);
		QApplication::processEvents();
		auto* const comparison = window->comparison_surface();
		auto* const code       = comparison->comparison_veil();
		expect(window->current_modality() == qt::review_modality::review && !window->showing_overview(), "the double-click goes to Review at once");
		expect(code->busy() && code->shape() == qt::pending_shape::code && comparison->waiting_path() == path, "where the comparison waits for the file in the shape of code");
		expect(!overview->projection().has_value(), "before the review has loaded");
		const auto repositoriesShown = window->repositories_drawer()->isVisibleTo(window);
		renders(QStringLiteral("overview-double-click-while-loading"));
		expect(shows_file_without_gap(*comparison, "the code veil shows until the double-clicked file arrives") && overview->projection().has_value(),
			   "the double-clicked file arrives once the review has loaded");
		pause(1'000);
		expect(window->repositories_drawer()->isVisibleTo(window) == repositoriesShown, "no drawer opens by itself when the load ends");
		delay->Milliseconds     = 0U;
		delay->PageMilliseconds = 0U;
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
