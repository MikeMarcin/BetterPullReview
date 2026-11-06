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
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
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
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidget>
#include <atomic>
#include <sqlite3.h>
#include <thread>

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
		std::optional<qint64> Title, Identity, OverviewFiles, Checks, Merge, People, Accepted, DrawerFiles, Diff;
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
	const auto    measureOnly       = QApplication::arguments().contains(QStringLiteral("--measure"));
	const auto    renderIndex       = QApplication::arguments().indexOf(QStringLiteral("--render-directory"));
	const auto    renderDirectory   = renderIndex >= 0 ? QApplication::arguments().value(renderIndex + 1) : QString{ };
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
	std::atomic<bool> holdStatus{ false }, statusHeld{ false }, holdPeople{ false }, peopleHeld{ false }, holdRules{ false }, rulesHeld{ false };
	const auto        hold = [](std::atomic<bool>& aHold, std::atomic<bool>& aHeld, const std::stop_token aStop) {
		if (aHold.load()) {
			aHeld = true;
			while (aHold.load() && !aStop.stop_requested()) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
			}
		}
	};
	options.BeforeMergeAssessment = [&](const std::stop_token aStop) { hold(holdStatus, statusHeld, aStop); };
	options.BeforeReviewRequests  = [&](const std::stop_token aStop) { hold(holdPeople, peopleHeld, aStop); };
	options.BeforeBranchRules     = [&](const std::stop_token aStop) { hold(holdRules, rulesHeld, aStop); };
	options.PersonalDiscovery     = app::detail::github_personal_discovery_fixture{
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
		auto preparedWhileLoading    = false;
		auto statusShown             = false;
		statusHeld                   = false;
		peopleHeld                   = false;
		rulesHeld                    = false;
		holdStatus                   = !measureOnly && roundTrip != 0U;
		holdPeople                   = holdStatus.load();
		holdRules                    = holdStatus.load();
		auto        statusReleased   = !holdStatus.load();
		auto        selectedCheck    = QString{ };
		auto* const checks           = overview->findChild<QTreeWidget*>(QStringLiteral("overviewCheckList"));
		expect(checks != nullptr, "Overview has its check-result list");
		const auto releaseLedger = !measureOnly && roundTrip != 0U ? access::hold_review_ledger_effects(*owner) : std::function<void()>{ };
		const auto releaseOnExit = qScopeGuard([&] { if (releaseLedger) { releaseLedger(); } });
		clock.start();
		double_click(row_of(*window, aIdentity));
		while (!times.Accepted && clock.elapsed() < 120'000) {
			QApplication::processEvents();
			const auto accepted = overview->projection().has_value();
			if (aIdentity == first && !accepted && !preparedWhileLoading) {
				preparedWhileLoading = access::diff_work_evidence(*owner).EpochOutstandingJobs != 0U;
			}
			const auto shown = identity != nullptr && identity->isVisibleTo(window) && identity->text().contains(aNumber);
			staleName        = staleName || (identity != nullptr && identity->isVisibleTo(window) && identity->text().contains(aOther));
			mark(times.Title, !overview->pending_veils().Header->busy() && !window->showing_home(), clock);
			mark(times.Identity, shown, clock);
			mark(times.OverviewFiles, overview->file_model()->rowCount() > 0, clock);
			mark(times.DrawerFiles, drawer->model() != nullptr && drawer->model()->rowCount() > 0, clock);
			mark(times.Accepted, accepted, clock);
			if (!window->showing_home() && !statusShown) {
				overview->status_drawer()->show_tab(qt::overview_drawer_tab::status);
				statusShown = true;
			}
			const auto checksVisible = checks->topLevelItemCount() > 0 && overview->status_section(qt::overview_status_section::checks)->isVisibleTo(window);
			mark(times.Checks, checksVisible, clock);
			const auto* merge = overview->findChild<QLabel*>(QStringLiteral("overviewMergeState"));
			mark(times.Merge, merge != nullptr && !merge->text().isEmpty() && overview->status_section(qt::overview_status_section::attention)->isVisibleTo(window), clock);
			mark(times.People, overview->status_section(qt::overview_status_section::people)->isVisibleTo(window), clock);
			if (!statusReleased && statusHeld && (checksVisible || clock.elapsed() > 15'000)) {
				const auto partial = overview->pending_veils().Side->look() == core::pending_look::partial;
				if (!checksVisible || !partial) {
					holdStatus = false;
				}
				expect(checksVisible && partial && !accepted, "check results and remaining-loading plate are visible while merge assessment is held");
				expect(overview->pending_veils().Side->shown_text().contains(QStringLiteral("Loading reviewers")), "Status names its remaining loading");
				auto* const header = overview->status_section_header(qt::overview_status_section::checks);
				if (!header->isChecked()) {
					header->click();
				}
				QApplication::processEvents();
				auto* const item = checks->topLevelItem(checks->topLevelItemCount() - 1);
				checks->scrollToItem(item);
				const auto point   = QPointF{ checks->visualItemRect(item).center() };
				const auto global  = checks->viewport()->mapToGlobal(point);
				auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
				auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
				QApplication::sendEvent(checks->viewport(), &press);
				QApplication::sendEvent(checks->viewport(), &release);
				selectedCheck      = item->data(0, Qt::UserRole).toString();
				auto* const detail = overview->findChild<QLabel*>(QStringLiteral("overviewSelectedCheck"));
				expect(checks->currentItem() == item && detail != nullptr && detail->isVisibleTo(window) && !detail->text().isEmpty(), "an actual click reads the selected check while remaining status is held");
				expect(!overview->findChild<QPushButton*>(QStringLiteral("overviewOpenCheck"))->isEnabled(), "check links wait for accepted review authority");
				if (!renderDirectory.isEmpty() && aIdentity == first) {
					expect(window->grab().save(renderDirectory + QStringLiteral("/status-checks-before-details.png")), "the early Status is rendered offscreen");
				}
				holdStatus     = false;
				statusReleased = true;
			}
			if (holdPeople && peopleHeld && (times.Merge || clock.elapsed() > 15'000)) {
				holdPeople = false;
				expect(times.Merge && !times.People && !accepted && overview->pending_veils().Side->look() == core::pending_look::partial,
					   "merge assessment is readable while reviewers remain explicitly loading");
				auto* header = overview->status_section_header(qt::overview_status_section::attention);
				if (!header->isChecked()) {
					header->click();
				}
				expect(overview->findChild<QLabel*>(QStringLiteral("overviewRequirementDetails"))->text().contains(QStringLiteral("still loading")), "early assessment identifies unfinished merge rules");
			}
			if (holdRules && rulesHeld && (times.People || clock.elapsed() > 15'000)) {
				holdRules = false;
				expect(times.Merge && times.People && !accepted && !overview->participant_rows().empty(), "people and merge are readable before held rules finish");
				expect(overview->pending_veils().Side->shown_text().contains(QStringLiteral("Loading merge rules and review progress")), "completed reviewers leave the remaining-loading plate");
				auto* header = overview->status_section_header(qt::overview_status_section::people);
				if (!header->isChecked()) {
					header->click();
				}
				QApplication::processEvents();
				expect(checks->currentItem() != nullptr && checks->currentItem()->data(0, Qt::UserRole).toString() == selectedCheck,
					   "independent people and merge updates preserve check selection");
				if (!renderDirectory.isEmpty() && aIdentity == first) {
					expect(window->grab().save(renderDirectory + QStringLiteral("/status-people-before-rules.png")), "independent Status sections render in the composed app");
				}
			}
			QThread::msleep(1);
		}
		expect(times.Accepted.has_value(), "the review is accepted");
		expect(statusReleased, "the independent status read is released");
		if (!selectedCheck.isEmpty()) {
			expect(checks->currentItem() != nullptr && checks->currentItem()->data(0, Qt::UserRole).toString() == selectedCheck
					   && overview->status_section_header(qt::overview_status_section::checks)->isChecked(),
				   "final acceptance retains the selected check and disclosure");
		}
		if (releaseLedger) {
			using readiness    = application::overview_readiness;
			const auto context = owner->windows().front();
			wait_for([&] { return access::hold_review_ledger_terminals(*owner, context, true); }, "the accepted review owns its local store");
			const auto releaseAnswers = qScopeGuard([&] { static_cast<void>(access::hold_review_ledger_terminals(*owner, context, false)); });
			expect(overview->projection()->Progress.FilesReadiness == readiness::loading && overview->projection()->Progress.NotesReadiness == readiness::loading
					   && overview->value_label(qt::overview_value::files_read)->text() == QStringLiteral("Loading…")
					   && !overview->pending_veils().Side->busy(),
				   "network Status is complete while local progress explicitly loads");
			auto* reviewHeader = overview->status_section_header(qt::overview_status_section::my_review);
			if (!reviewHeader->isChecked()) {
				reviewHeader->click();
			}
			if (overview->status_section_header(qt::overview_status_section::people)->isChecked()) {
				overview->status_section_header(qt::overview_status_section::people)->click();
			}
			QApplication::processEvents();
			if (!renderDirectory.isEmpty() && aIdentity == first) {
				expect(window->grab().save(renderDirectory + QStringLiteral("/status-progress-loading.png")), "local loading is rendered with usable network Status");
			}
			releaseLedger();
			// Deliver the opening and receipt answers individually; later
			// private-record completions stay held on the same decision gate.
			for (auto step = 0; overview->projection()->Progress.FilesReadiness != readiness::ready && step < 16; ++step) {
				wait_for([&] { return access::held_review_ledger_terminals(*owner, context) != 0U; }, "one local completion is available");
				expect(access::release_review_ledger_terminal(*owner, context), "release one local completion");
				QApplication::processEvents();
			}
			expect(overview->projection()->Progress.FilesReadiness == readiness::ready && overview->projection()->Progress.NotesReadiness == readiness::loading,
				   "file progress becomes ready before independent private records");
			expect(overview->value_label(qt::overview_value::files_read)->text() == QStringLiteral("0 of 2")
					   && overview->value_label(qt::overview_value::private_notes)->text() == QStringLiteral("Loading…"),
				   "only the completed local input presents its value");
			if (!renderDirectory.isEmpty() && aIdentity == first) {
				QApplication::processEvents();
				expect(window->grab().save(renderDirectory + QStringLiteral("/status-files-before-notes.png")), "file progress renders before notes complete");
			}
			expect(access::hold_review_ledger_terminals(*owner, context, false), "release remaining local completions");
			wait_for([&] { return overview->projection()->Progress.NotesReadiness == readiness::ready; }, "private records complete independently");
			const auto ledger = access::scaling_evidence(*owner, context).Ledger;
			expect(ledger.ReceiptLoads == 2U && ledger.PrivateLoads == 1U, "independent completion reads each file and private records once");
			expect(overview->projection()->Progress.PrivateNotes == 0U && reviewHeader->isChecked()
					   && checks->currentItem() != nullptr && checks->currentItem()->data(0, Qt::UserRole).toString() == selectedCheck,
				   "local completion preserves final values, disclosure and check selection");
			std::cout << "progressive-open:local-progress " << aIdentity.toStdString() << " network-ready-before-ledger=1 files-ready-before-private=1 receipt-loads=" << ledger.ReceiptLoads << " private-loads=" << ledger.PrivateLoads << '\n';
		}
		expect(!staleName, "while it loads, the title row never names the pull request the window showed before");
		if (aIdentity == first && roundTrip != 0U) {
			expect(preparedWhileLoading, "the composed first open prepares Git while the PR is still loading");
		}
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
		if (releaseLedger && aIdentity == first) {
			wait_for([&] { return window->read_action()->isEnabled(); }, "the first file offers its durable read gesture");
			window->read_action()->trigger();
			wait_for([&] { return overview->projection()->Progress.FilesRead == 1U; }, "a nonzero durable receipt is recorded for the first PR");
		}
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
				  << " checks=" << text(times.Checks) << " merge=" << text(times.Merge) << " people=" << text(times.People) << " accepted=" << text(times.Accepted) << " first-diff=" << text(times.Diff) << " reads=" << times.Reads << '\n';
		return times;
	};

	const auto opened = opens(first, QStringLiteral("#17"), QStringLiteral("#18"));
	// With no round trip the whole read lands within one turn of the loop, so
	// only the measurement is printed.
	if (roundTrip == 0U || measureOnly) {
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
	wait_for([&] { return overview->projection()->Progress.FilesReadiness == application::overview_readiness::ready; }, "reopened progress finishes loading");
	expect(overview->projection()->Progress.FilesRead == 1U, "the first PR retains its own durable progress across replacement");

	{
		// A real read failure in this course's private SQLite file must not
		// turn unknown progress into zero or disable completed network Status.
		using readiness    = application::overview_readiness;
		const auto context = owner->windows().front();
		window->choose_home(true);
		wait_for([&] { return window->showing_home(); }, "Home precedes the local failure opening");
		const auto releaseOpening = access::hold_review_ledger_effects(*owner);
		const auto openingGuard   = qScopeGuard(releaseOpening);
		double_click(row_of(*window, second));
		wait_for([&] { return overview->projection().has_value(); }, "network Status completes before the failed local read");
		expect(access::hold_review_ledger_terminals(*owner, context, true), "hold this review's store completion");
		const auto answerGuard = qScopeGuard([&] { static_cast<void>(access::hold_review_ledger_terminals(*owner, context, false)); });
		expect(overview->projection()->Progress.FilesReadiness == readiness::loading, "replacement awaits its own local progress");
		releaseOpening();
		wait_for([&] { return access::held_review_ledger_terminals(*owner, context) != 0U; }, "the store has opened on its effect lane");
		const auto releaseReads = access::hold_review_ledger_effects(*owner);
		const auto readsGuard   = qScopeGuard(releaseReads);
		expect(access::release_review_ledger_terminal(*owner, context), "deliver the store opening before faulting reads");
		wait_for([&] { return access::review_ledger_available(*owner) == true; }, "the scoped store opened successfully");
		sqlite3* ledgerRaw = nullptr;
		expect(sqlite3_open_v2((root / "state/v1/review-ledger.sqlite3").string().c_str(), &ledgerRaw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
			   "fixture opens only its own ledger for a bounded read fault");
		const auto ledgerConnection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>{ ledgerRaw, sqlite3_close };
		expect(sqlite3_exec(ledgerRaw, "ALTER TABLE file_receipt RENAME TO fixture_hidden_receipt", nullptr, nullptr, nullptr) == SQLITE_OK,
			   "fixture hides its receipt table before the queued read");
		overview->status_drawer()->show_tab(qt::overview_drawer_tab::status);
		auto* checks = overview->findChild<QTreeWidget*>(QStringLiteral("overviewCheckList"));
		expect(checks->topLevelItemCount() > 0, "network checks are available during the local fault");
		checks->setCurrentItem(checks->topLevelItem(0));
		const auto selectedCheck = checks->currentItem()->data(0, Qt::UserRole).toString();
		auto*      reviewHeader  = overview->status_section_header(qt::overview_status_section::my_review);
		if (!reviewHeader->isChecked()) {
			reviewHeader->click();
		}
		releaseReads();
		expect(access::hold_review_ledger_terminals(*owner, context, false), "deliver the failed receipt read and independent private records");
		wait_for([&] { return overview->projection()->Progress.FilesReadiness == readiness::unavailable && overview->projection()->Progress.NotesReadiness == readiness::ready; },
				 "failed receipts and successful private records reach separate terminal states");
		expect(access::scaling_evidence(*owner, context).Ledger.StoreFailures > 0U
				   && overview->value_label(qt::overview_value::files_read)->text() == QStringLiteral("Unavailable")
				   && !overview->pending_veils().Side->busy() && reviewHeader->isChecked()
				   && checks->currentItem()->data(0, Qt::UserRole).toString() == selectedCheck,
			   "an actual SQLite failure preserves usable Status, selection and explicit unavailable progress");
		if (!renderDirectory.isEmpty()) {
			QApplication::processEvents();
			expect(window->grab().save(renderDirectory + QStringLiteral("/status-progress-unavailable.png")), "unavailable progress renders with completed network sections");
		}
		std::cout << "progressive-open:local-progress-failure receipt-unavailable=1 private-ready=1 network-usable=1\n";
	}

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
