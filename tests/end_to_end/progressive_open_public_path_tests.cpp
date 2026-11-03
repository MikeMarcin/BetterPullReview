// [[BPR-1842]]. Opening a pull request becomes usable as its parts arrive,
// through the composed application on a fixture network that holds every read
// as long as `gh` takes for one (the owner: "the title bar doesn't fill in
// until the whole PR is loaded"). The title row names the pull request as soon
// as GitHub does, the Overview's file table lists the files once their page
// arrives, and both come before the accepted review; switching to another pull
// request mid-load never shows the first one's name or files. The course
// prints what each surface waited and how many reads the load made.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
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
#include <QApplication>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const int aTimeoutMs = 60000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
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

	/// When each surface of one opening first became usable, in milliseconds
	/// from the gesture.
	struct opening_times
	{
		std::optional<qint64> Title, Identity, OverviewFiles, Accepted, DrawerFiles, Diff;
		std::size_t           Reads = 0U;
	};

	void mark(std::optional<qint64>& aSlot, const bool aNow, const QElapsedTimer& aClock)
	{
		if (!aSlot && aNow) {
			aSlot = aClock.elapsed();
		}
	}

	auto text(const std::optional<qint64>& aSlot) -> std::string
	{
		return aSlot ? std::to_string(*aSlot) : std::string{ "-" };
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    roundTripArgument = QApplication::arguments().indexOf(QStringLiteral("--round-trip-ms"));
	const auto    roundTrip         = roundTripArgument >= 0 ? QApplication::arguments().value(roundTripArgument + 1).toUInt() : 120U;
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private progressive-open course root");
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

	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "Home sign-in reaches an account session");
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	window->settings()->hide();
	window->choose_home(true);
	const auto first  = QStringLiteral("octo-org/review-repository#17");
	const auto second = QStringLiteral("octo-org/review-repository#18");
	wait_for([&] { return row_of(*window, first).View != nullptr && row_of(*window, second).View != nullptr; }, "the pull requests are listed");

	auto* const overview = window->overview_page();
	auto* const identity = window->identity_label();
	auto* const drawer   = window->files_drawer()->contents_view();
	const auto  opens    = [&](const QString& aIdentity, const QString& aNumber, const QString& aOther) {
		window->choose_home(true);
		wait_for([&] { return window->showing_home(); }, "Home is shown before the opening");
		{
			const std::scoped_lock lock{ delay->ReadLogMutex };
			delay->ReadLog.clear();
		}
		delay->EveryReadMilliseconds = roundTrip;
		auto times                   = opening_times{ };
		auto clock                   = QElapsedTimer{ };
		auto staleName               = false;
		clock.start();
		double_click(row_of(*window, aIdentity));
		while (!times.Accepted && clock.elapsed() < 120'000) {
			QApplication::processEvents();
			const auto accepted = overview->projection().has_value();
			const auto shown    = identity != nullptr && identity->isVisibleTo(window) && identity->text().contains(aNumber);
			staleName           = staleName || (identity != nullptr && identity->isVisibleTo(window) && identity->text().contains(aOther));
			mark(times.Title, !overview->pending_veils().Header->busy() && !window->showing_home(), clock);
			mark(times.Identity, shown, clock);
			mark(times.OverviewFiles, overview->file_model()->rowCount() > 0, clock);
			mark(times.DrawerFiles, drawer->model() != nullptr && drawer->model()->rowCount() > 0, clock);
			mark(times.Accepted, accepted, clock);
			QThread::msleep(1);
		}
		expect(times.Accepted.has_value(), "the review is accepted");
		expect(!staleName, "while it loads, the title row never names the pull request the window showed before");
		// The first file a reviewer would open, read as soon as the drawer can
		// open it, in Review, which shows the comparison and the drawer.
		auto step  = app::detail::course_modality_step{ };
		step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
		wait_for([&] { return access::enter_course_modality(*owner, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
		expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and the Files drawer");
		wait_for([&] { return drawer->model() != nullptr && drawer->model()->rowCount() > 0; }, "the Files drawer lists the files");
		mark(times.DrawerFiles, true, clock);
		auto* const surface = window->comparison_surface();
		auto        file    = QModelIndex{ };
		for (auto row = 0; row < drawer->model()->rowCount() && !file.isValid(); ++row) {
			const auto index = drawer->model()->index(row, 0);
			if (!drawer->model()->hasChildren(index)) {
				file = index;
			}
		}
		expect(file.isValid(), "the drawer names a file");
		drawer->selectionModel()->setCurrentIndex(file, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait_for([&] { return surface->current_result() != nullptr && surface->current_context().has_value() && !surface->geometry_pending(); }, "the first file's comparison shows");
		mark(times.Diff, true, clock);
		{
			const std::scoped_lock lock{ delay->ReadLogMutex };
			times.Reads = delay->ReadLog.size();
			auto counts = std::map<std::string, int>{ };
			for (const auto& read : delay->ReadLog) {
				++counts[read.Operation];
			}
			std::cout << "progressive-open:reads " << aIdentity.toStdString() << ':';
			for (const auto& [name, count] : counts) {
				std::cout << ' ' << name << '=' << count;
			}
			std::cout << '\n';
		}
		delay->EveryReadMilliseconds = 0U;
		std::cout << "progressive-open:timing " << aIdentity.toStdString() << " round-trip-ms=" << roundTrip
				  << " title=" << text(times.Title) << " identity=" << text(times.Identity)
				  << " overview-files=" << text(times.OverviewFiles) << " drawer-files=" << text(times.DrawerFiles)
				  << " accepted=" << text(times.Accepted) << " first-diff=" << text(times.Diff) << " reads=" << times.Reads << '\n';
		return times;
	};

	const auto opened = opens(first, QStringLiteral("#17"), QStringLiteral("#18"));
	// With no round trip the whole read lands within one turn of the loop, so
	// only the measurement is printed.
	if (roundTrip == 0U) {
		static_cast<void>(opens(second, QStringLiteral("#18"), QStringLiteral("#17")));
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "the course removes its private root");
		std::cout << "progressive-open-public-path:measured\n";
		return EXIT_SUCCESS;
	}
	expect(opened.Identity && *opened.Identity < *opened.Accepted, "the title row names the pull request before the review is accepted");
	expect(opened.Title && *opened.Identity <= *opened.Title + 250, "the title row fills with the Overview's title, not after it");
	expect(opened.OverviewFiles && *opened.OverviewFiles < *opened.Accepted, "the Overview lists the files before the review is accepted");
	// Another pull request in the same window: the first one's name and
	// files never stand for the second while it loads.
	const auto switched = opens(second, QStringLiteral("#18"), QStringLiteral("#17"));
	expect(switched.Identity && *switched.Identity < *switched.Accepted, "switching, the title row names the new pull request before its review is accepted");
	expect(switched.OverviewFiles && *switched.OverviewFiles < *switched.Accepted, "switching, the Overview lists the new files before the review is accepted");

	// A file opened from the Overview's list while the review still loads
	// opens by itself once the review is accepted; the plate says so meanwhile.
	window->choose_home(true);
	wait_for([&] { return window->showing_home(); }, "Home is shown before the third opening");
	delay->EveryReadMilliseconds = roundTrip;
	double_click(row_of(*window, first));
	wait_for([&] { return overview->file_model()->rowCount() > 0 && !overview->projection().has_value(); }, "the Overview lists the files while the review loads");
	auto* const table  = overview->files_view();
	auto        listed = QModelIndex{ };
	// The first file in the tree, under whatever folders hold it.
	for (auto parent = QModelIndex{ }; !listed.isValid() && table->model()->rowCount(parent) > 0;) {
		const auto index = table->model()->index(0, 0, parent);
		if (table->model()->hasChildren(index)) {
			parent = index;
		} else {
			listed = index;
		}
	}
	expect(listed.isValid(), "the provisional list names a file");
	emit table->activated(listed);
	expect(!overview->pending_open().isEmpty() && overview->pending_veils().Files->busy()
			   && overview->pending_veils().Files->words().Coming.startsWith(QStringLiteral("Opening ")),
		   "the file waits for the review, and the plate says it will open");
	wait_for([&] { return overview->projection().has_value(); }, "the review is accepted");
	delay->EveryReadMilliseconds = 0U;
	auto* const surface          = window->comparison_surface();
	wait_for([&] { return !window->showing_overview() && surface->current_result() != nullptr && surface->current_context().has_value(); }, "the waiting file opens once the review is accepted");
	expect(overview->pending_open().isEmpty(), "nothing waits once it has opened");

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "progressive-open-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "progressive-open-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
