// [[RFC-BPR-0105]]. A long session through the composed desktop: two pull
// requests opened from Home in one window, Approve after leaving one whose
// work is still running. The course prints what the review page shows at
// each step (`dependable-session:observe`), so a run on a revision before
// the fix records where a submission waited and what the reviewer saw.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	using access = app::detail::github_application_fixture_access;

	void pump_until(const std::function<bool()>& aCondition, const int aTimeoutMs)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const int aTimeoutMs = 60000)
	{
		pump_until(aCondition, aTimeoutMs);
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

	[[nodiscard]] auto approve_button(qt::workbench_window& aWindow) -> QToolButton*
	{
		return aWindow.review_submission_editor_view()->findChild<QToolButton*>(QStringLiteral("wholeReviewApprove"));
	}

	/// What the review page shows now, in one line.
	void observe(qt::workbench_window& aWindow, const char* aStep, const qint64 aAt)
	{
		auto* const editor  = aWindow.review_submission_editor_view();
		const auto& draft   = editor->shown_draft();
		auto* const approve = approve_button(aWindow);
		const auto  state   = !draft                                    ? std::string{ "none" }
			: draft->State == core::review_submission_state::drafting   ? std::string{ "drafting" }
			: draft->State == core::review_submission_state::submitting ? std::string{ "submitting" }
			: draft->State == core::review_submission_state::submitted  ? std::string{ "submitted" }
																		: std::string{ "discarded" };
		std::cout << "dependable-session:observe step=" << aStep << " at_ms=" << aAt << " draft=" << state
				  << " approve_enabled=" << (approve != nullptr && approve->isEnabled())
				  << " status_visible=" << editor->status_label()->isVisible()
				  << " status=\"" << editor->status_label()->text().toStdString() << '"'
				  << " waiting_visible=" << editor->waiting_panel()->isVisible()
				  << " waiting=\"" << editor->waiting_title_label()->text().toStdString() << '"'
				  << " approve_tip=\"" << (approve != nullptr ? approve->toolTip().toStdString() : std::string{ }) << "\"\n";
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private dependable-session course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto server                    = std::make_shared<app::detail::review_submission_fixture_state>();
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	options.ReviewSubmissionServer = server;
	options.PersonalDiscovery      = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = {
			{ .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" } },
			{ .Number = 18U, .Author = "contributor-b", .RequestedUsers = { "octo-reviewer" } },
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
	const auto first  = QStringLiteral("octo-org/review-repository#17");
	const auto second = QStringLiteral("octo-org/review-repository#18");
	wait_for([&] { return row_of(*window, first).View != nullptr && row_of(*window, second).View != nullptr; }, "the pull requests are listed");

	auto* const overview = window->overview_page();
	const auto  open     = [&](const QString& aIdentity) {
		window->choose_home(true);
		wait_for([&] { return window->showing_home() && row_of(*window, aIdentity).View != nullptr; }, "Home lists the pull request before it is opened");
		double_click(row_of(*window, aIdentity));
		wait_for([&] { return !window->showing_home() && overview->projection().has_value(); }, "the review is accepted", 120'000);
		window->choose_modality(qt::review_modality::review);
		QApplication::processEvents();
		window->open_review_composition();
		QApplication::processEvents();
	};

	auto* const editor    = window->review_submission_editor_view();
	const auto  submitted = [&] {
		const auto lock = std::scoped_lock{ server->Mutex };
		return server->Submissions;
	};
	const auto drafting = [&] { return editor->shown_draft() && editor->shown_draft()->State == core::review_submission_state::drafting; };
	const auto ready    = [&](const QString& aIdentity, const char* aWhat) {
		open(aIdentity);
		wait_for([&] { return approve_button(*window) != nullptr && approve_button(*window)->isEnabled(); }, aWhat);
	};
	QElapsedTimer clock;

	// 1. AC02: work on the shared publication lane (a tour publication or a
	// reply that has not answered) does not hold another review's Approve.
	ready(first, "#17's review page offers Approve");
	observe(*window, "first-ready", 0);
	auto release = access::hold_review_submission_effects(*owner);
	open(second);
	clock.start();
	observe(*window, "second-opened", clock.elapsed());
	pump_until([&] { return approve_button(*window)->isEnabled(); }, 10'000);
	observe(*window, "second-after-wait", clock.elapsed());
	const auto offered = approve_button(*window)->isEnabled();
	auto       before  = submitted();
	if (offered) {
		approve_button(*window)->click();
		observe(*window, "second-approve-pressed", clock.elapsed());
		pump_until([&] { return submitted() > before; }, 10'000);
		observe(*window, "second-after-approve", clock.elapsed());
	}
	const auto sentWhileShared = submitted() > before;
	release();
	expect(offered, "Approve is offered on #18 while the shared publication lane is held");
	expect(sentWhileShared, "Approve on #18 publishes while the shared publication lane is held");

	// 2. AC02: #17's own submission still waiting on its lane when the
	// reviewer leaves it for #18. #18's Approve does not wait for it.
	ready(first, "#17's review page offers Approve again");
	release = access::hold_review_submission_lane(*owner);
	approve_button(*window)->click();
	observe(*window, "first-held-approve", 0);
	expect(editor->waiting_panel()->isVisible() && editor->waiting_title_label()->text() == QStringLiteral("Waiting to send your review"),
		   "a submission that cannot start says it is waiting");
	ready(second, "#18's Approve is offered while #17's submission is held");
	before = submitted();
	approve_button(*window)->click();
	pump_until([&] { return submitted() > before; }, 10'000);
	observe(*window, "second-while-first-held", 0);
	expect(submitted() == before + 1U, "#18 publishes while #17's own submission is held on its lane");
	release();

	// 2b. RFC-BPR-0114 AC06 ([[BPR-2065]]): a push in flight. The author's
	// own Git runs on a lane of its own; held as a push waiting on its hooks
	// or the network holds it, a review submission still completes.
	release = access::hold_author_git_lane(*owner);
	ready(second, "#18's Approve is offered while a push is in flight");
	before = submitted();
	approve_button(*window)->click();
	pump_until([&] { return submitted() > before; }, 10'000);
	observe(*window, "second-while-push-held", 0);
	expect(submitted() == before + 1U, "RFC-BPR-0114 AC06: #18 publishes while a push holds the author's Git lane");
	release();

	// 2c. RFC-BPR-0111 AC07 ([[BPR-2029]]): a draft pull request's creation
	// in flight. Create Draft PR runs on a lane of its own; held as a
	// creation waiting on GitHub holds it, a review submission for another
	// pull request still completes.
	release = access::hold_local_draft_publication_lane(*owner);
	expect(static_cast<bool>(release), "Create Draft PR's lane is composed");
	ready(second, "#18's Approve is offered while a draft pull request's creation is in flight");
	before = submitted();
	approve_button(*window)->click();
	pump_until([&] { return submitted() > before; }, 10'000);
	observe(*window, "second-while-creation-held", 0);
	expect(submitted() == before + 1U, "RFC-BPR-0111 AC07: #18 publishes while a draft pull request's creation holds its lane");
	release();

	// 3. AC03: #18's own submission cannot start. It waits, says for what,
	// and at the bound ends not published with Retry; the summary is kept,
	// and Retry sends it once the obstruction is gone.
	access::set_review_submission_wait_bound(*owner, std::chrono::milliseconds{ 1500 });
	ready(second, "#18's Approve is offered for the bounded course");
	editor->body_editor()->setPlainText(QStringLiteral("Kept across the wait."));
	wait_for([&] { return editor->shown_draft() && editor->shown_draft()->Body == "Kept across the wait."; }, "the summary is in the draft");
	release = access::hold_review_submission_lane(*owner);
	before  = submitted();
	clock.restart();
	approve_button(*window)->click();
	observe(*window, "bounded-pressed", clock.elapsed());
	expect(editor->waiting_panel()->isVisible() && editor->waiting_title_label()->text() == QStringLiteral("Waiting to send your review")
			   && editor->findChild<QLabel*>(QStringLiteral("wholeReviewWaitingFallback"))->text().startsWith(QStringLiteral("Waiting for ")),
		   "the waiting page names what it waits for");
	wait_for([&] { return drafting(); }, "the bounded wait ends the submission", 10'000);
	observe(*window, "bounded-expired", clock.elapsed());
	expect(clock.elapsed() >= 1400, "the submission waited for the bound before ending");
	expect(editor->status_label()->isVisible() && editor->status_label()->text().startsWith(QStringLiteral("Not published. Still waiting for ")),
		   "it ends as not published, saying what it was still waiting for");
	expect(editor->retry_button()->isVisible(), "Retry is offered");
	expect(editor->shown_draft()->Body == "Kept across the wait." && editor->shown_draft()->Disposition == core::review_disposition::approve, "the draft is intact");
	expect(submitted() == before, "nothing was sent");
	release();
	QApplication::processEvents();
	editor->retry_button()->click();
	pump_until([&] { return submitted() > before; }, 10'000);
	observe(*window, "bounded-retried", clock.elapsed());
	expect(submitted() == before + 1U, "Retry submits once the obstruction is gone");
	access::set_review_submission_wait_bound(*owner, std::chrono::milliseconds{ 20'000 });

	// 4. AC04: a submission superseded while it waits (the window's
	// authority changed) ends when its answer arrives, never left saying
	// it is being submitted.
	ready(second, "#18's Approve is offered for the superseded course");
	release = access::hold_review_submission_lane(*owner);
	approve_button(*window)->click();
	expect(access::invalidate_current_authority(*owner), "the window's authority changes");
	observe(*window, "superseded-invalidated", 0);
	release();
	wait_for([&] { return drafting(); }, "the superseded submission's answer ends it", 10'000);
	observe(*window, "superseded-ended", 0);
	expect(!editor->waiting_panel()->isVisible(), "the page no longer says the review is being submitted");

	// 6. AC06, AC07: a review left on the Tour tab with its side chat open
	// does not leak into the next one opened, another or the same again;
	// the remembered filters and the drawer widths are kept.
	const auto leaveOnTour = [&](const QString& aIdentity) {
		open(aIdentity);
		window->tray_tabs()->setCurrentIndex(2);
		window->tour_editor()->side_chat()->open(core::tour_thread{ }, std::nullopt, false);
		QApplication::processEvents();
		observe(*window, "left-on-tour", 0);
		std::cout << "dependable-session:observe tray=" << window->tray_tabs()->currentIndex() << " chat_open=" << window->tour_editor()->side_chat()->column_open() << '\n';
		expect(window->tray_tabs()->currentIndex() == 2 && window->tour_editor()->side_chat()->column_open(), "the review is left on the Tour tab with its side chat open");
	};
	const auto fresh = [&](const char* aWhat) {
		std::cout << "dependable-session:observe opened tray=" << window->tray_tabs()->currentIndex() << " chat_open=" << window->tour_editor()->side_chat()->column_open() << " tour_page=" << window->tour_page_open() << '\n';
		expect(window->tray_tabs()->currentIndex() == 0 && !window->tour_editor()->side_chat()->column_open() && !window->tour_page_open(), aWhat);
	};
	open(first);
	QApplication::processEvents();
	const auto filters = window->view_filters();
	const auto width   = window->conversations_drawer()->width();
	leaveOnTour(first);
	open(second);
	QApplication::processEvents();
	fresh("another pull request opens on Comments with no tour side chat");
	expect(window->view_filters() == filters, "the remembered filters are kept");
	std::cout << "dependable-session:observe drawer_width before=" << width << " after=" << window->conversations_drawer()->width() << '\n';
	expect(window->conversations_drawer()->width() == width, "the drawer's width is kept");
	leaveOnTour(second);
	open(second);
	QApplication::processEvents();
	fresh("the same pull request reopened from Home opens on Comments with no tour side chat");

	// 5. AC05: the log says where each submission waited and how it ended.
	const auto logs = tests::read_diagnostic_logs(root);
	for (const auto* const word : { "action=review_submit_request", "action=review_submit_wait", "submit_subject=review_lane", "action=review_submit_send",
									"action=review_submit_drop", "submit_subject=retired", "action=review_submit_end", "submit_subject=published", "submit_subject=wait_expired" }) {
		if (logs.find(word) == std::string::npos) {
			std::cerr << "dependable-session: log lacks " << word << '\n';
		}
		expect(logs.find(word) != std::string::npos, "the diagnostic log records each request, wait, drop and ending");
	}

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "dependable-session-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "dependable-session-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
