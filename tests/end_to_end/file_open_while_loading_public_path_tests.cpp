// [[BPR-1871]]. A file opened while its pull request is still loading says it
// is loading that file, and opens by itself once it can (the owner,
// 2026-11-05: "if I try to open a file while it's still loading the pr the tab
// shows the "no file opened" screen instead it should indicate were loading
// that file"). Through the composed application on a fixture whose Git fetch
// is held as a large repository's clone is: the review is accepted and the
// Files drawer lists its files while the clone is still on its way. A file
// chosen then names itself on the comparison's loading plate, never "No file
// open"; a second choice replaces the first; once the clone lands the second
// file shows with no further gesture. The course prints how long after the
// clone answered the file showed.
//
// `--clone-fails` makes the held clone fail instead: the chosen file then says
// why it cannot open in its place, never "No file open" and never loading
// forever. `--measure-only` prints the timing and skips the loading-state
// checks, so the same course measures a build without this card.
// `--render-directory DIR` saves the window while the file loads (after the
// plate's reveal, and past its slow mark when the clone is held that long) and
// when it fails.
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
#include <QDir>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

	/// The drawer's file rows, under whatever folders hold them.
	void collect_files(QAbstractItemModel& aModel, const QModelIndex& aParent, std::vector<QModelIndex>& aFiles)
	{
		for (auto row = 0; row < aModel.rowCount(aParent); ++row) {
			const auto index = aModel.index(row, 0, aParent);
			if (aModel.hasChildren(index)) {
				collect_files(aModel, index, aFiles);
			} else {
				aFiles.push_back(index);
			}
		}
	}

	[[nodiscard]] auto steady_ms(const std::chrono::steady_clock::time_point::rep aTicks) -> qint64
	{
		const auto point = std::chrono::steady_clock::time_point{ std::chrono::steady_clock::duration{ aTicks } };
		return std::chrono::duration_cast<std::chrono::milliseconds>(point.time_since_epoch()).count();
	}

	[[nodiscard]] auto now_ms() -> qint64
	{
		return steady_ms(std::chrono::steady_clock::now().time_since_epoch().count());
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    arguments   = QApplication::arguments();
	const auto    measureOnly = arguments.contains(QStringLiteral("--measure-only"));
	const auto    cloneFails  = arguments.contains(QStringLiteral("--clone-fails"));
	const auto    renderIndex = arguments.indexOf(QStringLiteral("--render-directory"));
	const auto    renders     = renderIndex >= 0 ? arguments.value(renderIndex + 1) : QString{ };
	const auto    holdIndex   = arguments.indexOf(QStringLiteral("--clone-ms"));
	const auto    hold        = holdIndex >= 0 ? arguments.value(holdIndex + 1).toUInt() : 2500U;
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private file-open course root");
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
		},
	};
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = access::start_desktop(
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
	const auto identity = QStringLiteral("octo-org/review-repository#17");
	wait_for([&] { return row_of(*window, identity).View != nullptr; }, "the pull request is listed");

	// The clone is held; GitHub's own reads are not.
	delay->GitFetchMilliseconds = hold;
	delay->GitFetchFails        = cloneFails;
	const auto opened           = now_ms();
	double_click(row_of(*window, identity));
	auto* const overview = window->overview_page();
	wait_for([&] { return overview->projection().has_value(); }, "the review is accepted while the clone is held");
	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*owner, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
	expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and the Files drawer");
	auto* const drawer = window->files_drawer()->contents_view();
	wait_for([&] { return drawer->model() != nullptr && drawer->model()->rowCount() > 0; }, "the Files drawer lists the files");
	auto files = std::vector<QModelIndex>{ };
	collect_files(*drawer->model(), { }, files);
	expect(files.size() >= 2U, "the drawer lists two files to choose between");
	const auto cloneHeld = delay->GitFetchReleasedAt.load() == 0 && owner->status().EpochStatus != core::epoch_status::ready;
	std::cout << "file-open-while-loading:clone-held " << (cloneHeld ? "yes" : "no") << " at " << (now_ms() - opened) << "ms\n";
	expect(measureOnly || cloneHeld, "the drawer lists its files while the clone is still held");

	auto* const surface = window->comparison_surface();
	auto* const veil    = surface->comparison_veil();
	const auto  choose  = [&](const QModelIndex& aFile) {
		drawer->selectionModel()->setCurrentIndex(aFile, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		for (auto turn = 0; turn < 20; ++turn) {
			QApplication::processEvents();
		}
	};
	const auto name_of = [&](const QModelIndex& aFile) { return aFile.data(Qt::DisplayRole).toString(); };
	const auto waiting = [&](const QModelIndex& aFile) {
		return !surface->idle() && veil->busy() && veil->words().Loading.contains(name_of(aFile));
	};

	// The first choice names itself.
	choose(files[0]);
	const auto firstNamed = waiting(files[0]);
	std::cout << "file-open-while-loading:first-choice idle=" << surface->idle() << " loading=\"" << veil->words().Loading.toStdString() << "\"\n";
	expect(measureOnly || firstNamed, "the first file chosen during the load says it is loading that file, not No file open");
	expect(measureOnly || delay->GitFetchReleasedAt.load() == 0, "the clone is still held when the file says it is loading");

	// A second choice replaces it.
	choose(files[1]);
	const auto secondNamed = waiting(files[1]) && !veil->words().Loading.contains(name_of(files[0]));
	std::cout << "file-open-while-loading:second-choice idle=" << surface->idle() << " loading=\"" << veil->words().Loading.toStdString() << "\"\n";
	expect(measureOnly || secondNamed, "a second choice replaces the first on the loading plate");
	const auto chosen = now_ms();
	const auto render = [&](const QString& aName, const int aAfterMs) {
		if (renders.isEmpty()) {
			return;
		}
		QElapsedTimer settle;
		settle.start();
		while (settle.elapsed() < aAfterMs && delay->GitFetchReleasedAt.load() == 0) {
			QApplication::processEvents();
			QThread::msleep(5);
		}
		QDir{ }.mkpath(renders);
		expect(window->grab().save(QDir{ renders }.filePath(aName)), "the render is saved");
		std::cout << "file-open-while-loading:render " << aName.toStdString() << " shown=\"" << veil->shown_text().toStdString() << "\"\n";
	};
	render(QStringLiteral("loading.png"), 1000);
	render(QStringLiteral("loading-slow.png"), 5000);

	if (cloneFails) {
		// The clone fails: the chosen file says why in its place.
		wait_for([&] { return veil->look() == core::pending_look::failed || surface->idle(); }, "the chosen file stops loading once the clone fails", 60000);
		const auto failure = veil->words().Failure;
		render(QStringLiteral("failure.png"), 0);
		std::cout << "file-open-while-loading:failure idle=" << surface->idle() << " sentence=\"" << failure.toStdString() << "\"\n";
		expect(!surface->idle(), "a file that cannot open says why, not No file open");
		expect(failure.startsWith(QStringLiteral("No exact comparison is ready for this file.")) && failure != QStringLiteral("No exact comparison is ready for this file."),
			   "the failure names its reason");
		expect(surface->waiting_path().endsWith(name_of(files[1])), "the failure stands in the chosen file's place");
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "the course removes its private root");
		std::cout << "file-open-while-loading-public-path:pass\n";
		return EXIT_SUCCESS;
	}

	// The second file shows once the clone lands, with no further gesture.
	auto neverIdle = true;
	auto ready     = std::optional<qint64>{ };
	auto requested = std::optional<qint64>{ };
	wait_for([&] {
		neverIdle = neverIdle && !surface->idle();
		if (!ready && owner->status().EpochStatus == core::epoch_status::ready) {
			ready = now_ms();
		}
		if (!requested && surface->current_context().has_value()) {
			requested = now_ms();
		}
		return surface->current_result() != nullptr && surface->current_context().has_value() && !surface->geometry_pending();
	},
			 "the chosen file shows once the clone has landed", 60000);
	const auto shown    = now_ms();
	const auto released = delay->GitFetchReleasedAt.load() == 0 ? shown : steady_ms(delay->GitFetchReleasedAt.load());
	const auto evidence = access::epoch_refresh_identity(*owner);
	expect(evidence.PublishedFile.has_value() && evidence.PublishedFile == evidence.SelectedFile, "the file shown is the one chosen");
	expect(measureOnly || neverIdle, "No file open never stood for the chosen file while it loaded");
	// The chosen file's reads went first: no preparation round of other files
	// started while it waited for its comparison.
	const auto work = access::diff_work_evidence(*owner);
	std::cout << "file-open-while-loading:priority rounds-before-opened=" << work.PreparationsStartedBeforeOpenedFile
			  << " preempted=" << work.PreparationsPreempted << '\n';
	expect(measureOnly || work.PreparationsStartedBeforeOpenedFile == 0U, "no preparation round went ahead of the chosen file");
	expect(measureOnly || !veil->busy() || veil->words().Loading != QStringLiteral("Loading %1…").arg(name_of(files[1])), "nothing waits once it shows");
	std::cout << "file-open-while-loading:timing clone-ms=" << hold << " open-to-shown=" << (shown - opened)
			  << " chosen-to-shown=" << (shown - chosen) << " clone-answered-to-shown=" << (shown - released)
			  << " clone-answered-to-epoch-ready=" << (ready.value_or(shown) - released)
			  << " clone-answered-to-diff-requested=" << (requested.value_or(shown) - released)
			  << " fetches=" << delay->GitFetchesHeld.load() << '\n';
	delay->GitFetchMilliseconds = 0U;

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << (measureOnly ? "file-open-while-loading-public-path:measured\n" : "file-open-while-loading-public-path:pass\n");
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "file-open-while-loading-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
