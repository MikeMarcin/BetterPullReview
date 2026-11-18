// [[BPR-2048]] [[BPR-2049]] [[BPR-2217]]. One pull request too large to read,
// and reviewed pull requests back for new commits, through the composed
// application on the public-path fixture.
//
// The fixture serves four pull requests asking for my review: #17 with more
// review requests than one page (Home's own read of it is too large) and more
// changed files than GitHub lists (opening it is too large); #18 that I
// approved before commits were pushed; #19 whose approval GitHub dismissed
// when commits were pushed; #4001 ordinary. The course signs in, saves the
// repository and reads Home as a person would: every row is listed and the
// refresh is not failed; #17's row says it is too large; opening #17 shows the
// card that names what was too large, with Open on GitHub and Retry. #18 and
// #19 are in Needs me with their verdict chips and no row action: since
// [[BPR-2217]] Seen and Approve again are the review's (owner, 2026-11-16:
// "its dumb to put that on home, I'm going to look at the new commits from
// within the review"). `--seen` opens #17, approved before commits were
// pushed: its review offers Seen beside Approve, Seen asks GitHub for nothing
// and, recorded, leaves the review as a confirmed Approve does ([[BPR-2249]]):
// Home by default, the pull request with Approve unticked (`--seen-stays`); a
// Seen the store does not keep stays open and says so (`--seen-fails`). `--approve-again` opens #17 with its
// approval dismissed: its review's Approve reads Approve again.
// With `--attended` the window stays open for the owner's look.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_refusal.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <functional>
#include <iostream>
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
		QListView*     View = nullptr;
		QModelIndex    Index;
		qt::home_group Group = qt::home_group::needs_you;
	};

	[[nodiscard]] auto row_of(qt::workbench_window& aWindow, const int aNumber) -> found_row
	{
		for (const auto group : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
			auto* const view = aWindow.home_list()->state_view(group);
			for (auto row = 0; view != nullptr && view->model() != nullptr && row < view->model()->rowCount(); ++row) {
				const auto index = view->model()->index(row, 0);
				if (index.data(qt::home_row_role::identity).toString().endsWith(QStringLiteral("#%1").arg(aNumber))) {
					return { view, index, group };
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

	/// Every labelled action a pointer over the row shows.
	[[nodiscard]] auto hovered_actions(const found_row& aRow) -> QStringList
	{
		const auto point = QPointF{ aRow.View->visualRect(aRow.Index).center() };
		auto       move  = QMouseEvent{ QEvent::MouseMove, point, aRow.View->viewport()->mapToGlobal(point), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aRow.View->viewport(), &move);
		QApplication::processEvents();
		auto shown = QStringList{ };
		for (auto* const button : aRow.View->viewport()->findChildren<QToolButton*>()) {
			if (button->isVisible() && !button->text().isEmpty()) {
				shown << button->text();
			}
		}
		return shown;
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private oversized course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	const auto arguments           = QApplication::arguments();
	// `--approve-again`: the fixture's own recorded pull request (#17), the
	// one that opens, is the one whose approval GitHub dismissed. `--seen`:
	// it is the one I approved before commits were pushed.
	const auto approveAgain = arguments.contains(QStringLiteral("--approve-again"));
	// [[BPR-2249]]. `--seen-fails`: the first Seen is not kept; `--seen-stays`:
	// Approve's finish choice is unticked. Both are `--seen` courses.
	const auto seenFails  = arguments.contains(QStringLiteral("--seen-fails"));
	const auto seenStays  = arguments.contains(QStringLiteral("--seen-stays"));
	const auto seenCourse = arguments.contains(QStringLiteral("--seen")) || seenFails || seenStays;
	// `--attended`: the owner's look. Every check of the rows as listed runs;
	// the gestures (Seen, opening a review, opening #17) are left to the person.
	const auto attended   = arguments.contains(QStringLiteral("--attended"));
	auto       discovered = app::detail::github_personal_discovery_fixture{ };
	if (seenCourse) {
		discovered.OpenPullRequests = {
			{ .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" }, .ViewerVerdictBefore = "APPROVED" },
			{ .Number = 18U, .Author = "contributor-b", .RequestedUsers = { "octo-reviewer" }, .ViewerVerdictBefore = "DISMISSED" },
			{ .Number = 19U, .Author = "contributor-c", .RequestedUsers = { "octo-reviewer" } },
			{ .Number = 4001U, .Author = "contributor-d", .RequestedUsers = { "octo-reviewer" } },
		};
	} else if (approveAgain) {
		discovered.OpenPullRequests = {
			{ .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" }, .ViewerVerdictBefore = "DISMISSED" },
			{ .Number = 18U, .Author = "contributor-b", .RequestedUsers = { "octo-reviewer" }, .ViewerVerdictBefore = "APPROVED" },
			{ .Number = 19U, .Author = "contributor-c", .RequestedUsers = { "octo-reviewer" } },
			{ .Number = 4001U, .Author = "contributor-d", .RequestedUsers = { "octo-reviewer" } },
		};
	} else {
		discovered.OpenPullRequests = {
			{ .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" }, .OversizedRequests = true, .OversizedFiles = true },
			{ .Number = 18U, .Author = "contributor-b", .RequestedUsers = { "octo-reviewer" }, .ViewerVerdictBefore = "APPROVED" },
			{ .Number = 19U, .Author = "contributor-c", .RequestedUsers = { "octo-reviewer" }, .ViewerVerdictBefore = "DISMISSED" },
			{ .Number = 4001U, .Author = "contributor-d", .RequestedUsers = { "octo-reviewer" } },
		};
	}
	options.PersonalDiscovery = std::move(discovered);
	const auto renderAt       = arguments.indexOf(QStringLiteral("--render"));

	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
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
	window->resize(1280, 1000);

	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "Home sign-in reaches an account session");
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	window->settings()->hide();
	window->choose_home(true);
	const auto discovery = [&] { return access::personal_discovery_evidence(*owner); };
	const auto listed    = [&] {
		return row_of(*window, 17).View != nullptr && row_of(*window, 18).View != nullptr && row_of(*window, 19).View != nullptr && row_of(*window, 4001).View != nullptr;
	};
	wait_for(listed, "Home lists every pull request asking for my review");
	wait_for([&] { return discovery().Status.State != application::queue_discovery_state::refreshing && discovery().Quiescent; }, "Home's first refresh settles");
	// Every row read: a refresh reads each listed pull request on its own.
	window->queue_refresh_action()->trigger();
	wait_for([&] { return discovery().Status.State == application::queue_discovery_state::refreshing; }, "Refresh starts", 10'000);
	wait_for([&] { return discovery().Status.State != application::queue_discovery_state::refreshing && discovery().Quiescent; }, "the refresh settles");

	if (seenCourse || approveAgain) {
		// ---- BPR-2217: the review offers Seen, or reads Approve again -------
		const auto row = row_of(*window, 17);
		expect(row.Group == qt::home_group::needs_you && !row.Index.data(qt::home_row_role::verdict).toString().isEmpty(),
			   "my reviewed pull request back for new commits is in Needs me with its verdict chip");
		expect(!hovered_actions(row).contains(QStringLiteral("Seen")) && !hovered_actions(row).contains(QStringLiteral("Approve again")),
			   "its Home row offers neither Seen nor Approve again");
		double_click(row);
		wait_for([&] { return !window->showing_home() && window->session_panel()->status() == core::github_status::pull_request_ready; }, "the pull request opens");
		auto* const editor = window->review_submission_editor_view();
		if (attended) {
			goto attend;
		}
		window->compose_review_action()->trigger();
		wait_for([&] { return window->review_sheet_open(); }, "its review opens");
		auto* const approve = editor->disposition_button(core::review_disposition::approve);
		if (approve == nullptr || !approve->isVisible()) {
			const auto shown = [&] { return approve != nullptr && approve->isVisible(); };
			for (auto turn = 0; turn < 2000 && !shown(); ++turn) {
				QApplication::processEvents(QEventLoop::AllEvents, 10);
			}
			if (!shown()) {
				std::cerr << "home-oversized: approve=" << (approve != nullptr) << " draft=" << (editor->shown_draft() ? 1 : 0) << " submit_tip=" << editor->submit_action()->toolTip().toStdString()
						  << " status=" << editor->status_label()->text().toStdString() << '\n';
			}
		}
		wait_for([&] { return approve != nullptr && approve->isVisible(); }, "the review's draft is read and its Approve shows", 20'000);
		const auto settled = [&](const QString& aApprove) { return approve->text() == aApprove && editor->followup_label()->isVisible(); };
		if (approveAgain) {
			wait_for([&] { return settled(QStringLiteral("Approve again")); }, "a dismissed approval's review reads Approve again", 20'000);
			expect(editor->followup_label()->text().contains(QStringLiteral("dismissed")), "the review says GitHub dismissed my approval: " + editor->followup_label()->text().toStdString());
			expect(!editor->seen_button()->isVisible(), "a dismissed approval cannot be seen away");
			wait_for([&] { return approve->isEnabled(); }, "Approve again can be submitted once the review's comments are read", 20'000);
			expect(approve->toolTip().contains(QStringLiteral("new approval")) && approve->accessibleName() == QStringLiteral("Approve again"),
				   "Approve again says it submits a new approval: " + approve->toolTip().toStdString());
			if (renderAt >= 0) {
				window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/review-approve-again.png"));
			}
		} else {
			auto* const seen = editor->seen_button();
			wait_for([&] { return settled(QStringLiteral("Approve")) && seen->isVisible(); }, "a standing approval's review offers Seen", 20'000);
			expect(seen->text() == QStringLiteral("Seen") && seen->toolTip().contains(QStringLiteral("Nothing is submitted to GitHub")), "Seen says it submits nothing");
			expect(seen->geometry().top() == approve->geometry().top() && seen->geometry().left() > approve->geometry().right()
					   && seen->geometry().left() - approve->geometry().right() < 24,
				   "Seen sits beside Approve");
			wait_for([&] { return approve->isEnabled(); }, "the review's comments are read", 20'000);
			expect(editor->followup_label()->text().contains(QStringLiteral("still stands")), "the review says my approval stands: " + editor->followup_label()->text().toStdString());
			if (renderAt >= 0) {
				window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/review-seen.png"));
			}
			const auto reads = discovery().Statistics.Submitted;
			const auto said  = QStringLiteral("Marked the new commits on octo-org/review-repository#17 seen; your approval stands. Nothing changed on GitHub.");
			if (seenFails) {
				// ---- BPR-2249: a Seen the store does not keep stays open ----
				access::refuse_queue_store_writes(*owner, true);
				seen->click();
				wait_for([&] { return window->statusBar()->currentMessage().contains(QStringLiteral("not marked seen")); }, "a Seen the store refused says so", 10'000);
				QApplication::processEvents();
				expect(!window->finishing_at_home() && !window->showing_home() && window->review_sheet_open(), "the review stays open on its page");
				expect(window->statusBar()->currentMessage() == QStringLiteral("The new commits were not marked seen. The review stays open; try Seen again."),
					   "and says the commits were not marked seen: " + window->statusBar()->currentMessage().toStdString());
				// The queue reads unavailable until its store keeps a write again;
				// the unkept seen head is withdrawn, so the row has not moved.
				expect(row_of(*window, 17).Group == qt::home_group::needs_you, "Home still has it in Needs me");
				expect(discovery().Statistics.Submitted == reads, "Seen asks GitHub for nothing");
				access::refuse_queue_store_writes(*owner, false);
			} else if (seenStays) {
				// ---- BPR-2249: Approve unticked, Seen returns to the pull request
				auto* const box = window->finish_review_switch(core::review_disposition::approve);
				expect(box != nullptr && box->isChecked(), "Approve goes Home by default");
				box->click();
				wait_for([&] { return window->behaviour().FinishReview.Approve == core::review_finish_return::pull_request; }, "Approve's finish choice is accepted");
				window->settings()->hide();
				seen->click();
				wait_for([&] { return !window->review_sheet_open(); }, "a recorded Seen closes the review page", 10'000);
				QApplication::processEvents();
				expect(!window->finishing_at_home() && !window->showing_home() && window->statusBar()->currentMessage() == said,
					   "with Approve unticked, Seen returns to the pull request and says so: " + window->statusBar()->currentMessage().toStdString());
				expect(!window->open_pull_request_seen_head().empty(), "the head I said Seen on is recorded");
				expect(discovery().Statistics.Submitted == reads, "Seen asks GitHub for nothing");
				wait_for([&] { return row_of(*window, 17).Group == qt::home_group::following; }, "and Home has it in Following", 10'000);
			} else {
				seen->click();
				wait_for([&] { return window->finishing_at_home() || window->showing_home(); }, "a recorded Seen leaves the review as Approve does", 10'000);
				wait_for([&] { return !window->finishing_at_home(); }, "the close runs on the next turn");
				expect(window->showing_home() && !window->review_sheet_open()
						   && window->statusBar()->currentMessage() == said + QStringLiteral(". Closed the review of octo-org/review-repository#17."),
					   "Seen closes the review and shows Home, and says both: " + window->statusBar()->currentMessage().toStdString());
				wait_for([&] { return row_of(*window, 17).Group == qt::home_group::following; }, "and Home has it in Following", 10'000);
			}
			expect(!editor->shown_draft() || editor->shown_draft()->State == core::review_submission_state::drafting, "Seen submitted no review");
			if (renderAt >= 0) {
				window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/review-seen-up-to-date.png"));
			}
		}
	} else {
		// ---- BPR-2048: one oversized pull request fails nothing ---------------
		expect(discovery().Status.State == application::queue_discovery_state::current, "the refresh with an oversized pull request in it is current, not failed");
		const auto oversized = row_of(*window, 17);
		expect(oversized.Group == qt::home_group::needs_you && oversized.Index.data(qt::home_row_role::why).toString().contains(QStringLiteral("too large to load here; open it on GitHub"), Qt::CaseInsensitive),
			   "the oversized row stays in Needs me and says it is too large: " + oversized.Index.data(qt::home_row_role::why).toString().toStdString());
		expect(!row_of(*window, 4001).Index.data(qt::home_row_role::why).toString().contains(QStringLiteral("too large"), Qt::CaseInsensitive), "the ordinary row loads as ever");

		// ---- BPR-2217: Home keeps the verdict chip, not the action ----------
		for (const auto number : { 18, 19 }) {
			const auto reviewed = row_of(*window, number);
			expect(reviewed.Group == qt::home_group::needs_you && !reviewed.Index.data(qt::home_row_role::verdict).toString().isEmpty(),
				   "a reviewed row back for new commits is in Needs me with its verdict chip");
			expect(reviewed.Index.data(qt::home_row_role::row_action).toString().isEmpty(), "and offers no row action");
			const auto hovered = hovered_actions(reviewed);
			expect(!hovered.contains(QStringLiteral("Seen")) && !hovered.contains(QStringLiteral("Approve again")), "hovering it shows neither Seen nor Approve again");
		}
		if (renderAt >= 0) {
			window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/home-oversized.png"));
		}
		if (attended) {
			goto attend;
		}

		// ---- BPR-2048: opening the oversized pull request ---------------------
		double_click(row_of(*window, 17));
		auto* const card = window->home_refusal_card();
		wait_for([&] { return card != nullptr && card->isVisible(); }, "opening it shows the card");
		expect(card->heading_label()->text() == QStringLiteral("octo-org/review-repository#17 is too large to open here"), "the card names the pull request: " + card->heading_label()->text().toStdString());
		expect(card->explanation_label()->text().contains(QStringLiteral("3,000")) && card->explanation_label()->text().contains(QStringLiteral("files")),
			   "the card names what was too large: " + card->explanation_label()->text().toStdString());
		const auto offered = card->offered_buttons();
		expect(offered.size() == 2U && offered[0]->text() == QStringLiteral("Open on GitHub") && offered[1]->text() == QStringLiteral("Retry"),
			   "the card offers Open on GitHub, then Retry");
		expect(window->home_list()->row_delegate()->open().Phase == qt::home_row_open_phase::none, "the row does not say the failure again");
		{
			// The card's own controls, laid out: in the order the core gave, each
			// at its own width, Copy details after them and none over another.
			QElapsedTimer settle;
			settle.start();
			while (settle.elapsed() < 300) {
				QApplication::processEvents();
			}
			auto* const copy = card->copy_details_button();
			expect(offered[0]->geometry().right() < offered[1]->geometry().left() && offered[1]->geometry().right() < copy->geometry().left()
					   && offered[1]->width() < card->width() / 3,
				   "the card's steps sit side by side at their own widths, then Copy details");
		}
		if (renderAt >= 0) {
			window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/home-oversized-card.png"));
		}
		offered[1]->click();
		wait_for([&] { return window->session_panel()->status() == core::github_status::pull_request_failed && card->isVisible(); }, "Retry asks again and is refused the same way");
	}

attend:
	if (attended) {
		window->resize(1100, 900);
		window->show();
		window->raise();
		QApplication::setQuitOnLastWindowClosed(true);
		std::cout << "HOME_OVERSIZED_ATTENDED_READY: fixture account; no live GitHub reads\n"
				  << std::flush;
		QApplication::exec();
	}
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "home-oversized-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "home-oversized-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
