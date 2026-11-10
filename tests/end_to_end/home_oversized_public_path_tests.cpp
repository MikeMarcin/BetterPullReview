// [[BPR-2048]] [[BPR-2049]]. One pull request too large to read, and reviewed
// pull requests back for new commits, through the composed application on
// the public-path fixture.
//
// The fixture serves four pull requests asking for my review: #17 with more
// review requests than one page (Home's own read of it is too large) and more
// changed files than GitHub lists (opening it is too large); #18 that I
// approved before commits were pushed; #19 whose approval GitHub dismissed
// when commits were pushed; #4001 ordinary. The course signs in, saves the
// repository and reads Home as a person would: every row is listed and the
// refresh is not failed; #17's row says it is too large; opening #17 shows the
// card that names what was too large, with Open on GitHub and Retry; #18
// offers Seen, which moves it to Following at once; #19 offers Approve again.
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
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
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

	/// The row's labelled action, as a pointer over the row shows it.
	[[nodiscard]] auto hovered_action(const found_row& aRow) -> QToolButton*
	{
		const auto point = QPointF{ aRow.View->visualRect(aRow.Index).center() };
		auto       move  = QMouseEvent{ QEvent::MouseMove, point, aRow.View->viewport()->mapToGlobal(point), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aRow.View->viewport(), &move);
		for (auto* const button : aRow.View->viewport()->findChildren<QToolButton*>(QStringLiteral("homeRowFollowupButton"))) {
			if (button->isVisible()) {
				return button;
			}
		}
		return nullptr;
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
	// `--approve-again`: the fixture's own recorded pull request (#17) is the
	// one whose approval GitHub dismissed, so Approve again can open it.
	const auto approveAgain = arguments.contains(QStringLiteral("--approve-again"));
	// `--attended`: the owner's look. Every check of the rows as listed runs;
	// the gestures (Seen, Approve again, opening #17) are left to the person.
	const auto attended   = arguments.contains(QStringLiteral("--attended"));
	auto       discovered = app::detail::github_personal_discovery_fixture{ };
	if (approveAgain) {
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

	if (approveAgain) {
		// ---- BPR-2049: Approve again opens the review with Approve ready ----
		const auto dismissed = row_of(*window, 17);
		expect(dismissed.Group == qt::home_group::needs_you && dismissed.Index.data(qt::home_row_role::row_action).toString() == QStringLiteral("Approve again"),
			   "a dismissed approval offers Approve again");
		auto* const again = hovered_action(dismissed);
		if (attended) {
			goto attend;
		}
		expect(again != nullptr && again->toolTip().contains(QStringLiteral("dismissed")), "Approve again shows on the row and says GitHub dismissed the approval");
		again->click();
		wait_for([&] { return !window->showing_home() && window->review_sheet_open(); }, "Approve again opens the pull request with its review");
		auto* const approve = window->review_submission_editor_view()->disposition_button(core::review_disposition::approve);
		wait_for([&] { return approve != nullptr && approve->isVisible(); }, "the review's draft is read and its Approve shows", 20'000);
		expect(approve != nullptr && approve->isVisible(), "the review shows its Approve, which submits the approval");
		if (renderAt >= 0) {
			window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/home-approve-again.png"));
		}
	} else {
		// ---- BPR-2048: one oversized pull request fails nothing ---------------
		expect(discovery().Status.State == application::queue_discovery_state::current, "the refresh with an oversized pull request in it is current, not failed");
		const auto oversized = row_of(*window, 17);
		expect(oversized.Group == qt::home_group::needs_you && oversized.Index.data(qt::home_row_role::why).toString().contains(QStringLiteral("too large to load here; open it on GitHub"), Qt::CaseInsensitive),
			   "the oversized row stays in Needs me and says it is too large: " + oversized.Index.data(qt::home_row_role::why).toString().toStdString());
		expect(!row_of(*window, 4001).Index.data(qt::home_row_role::why).toString().contains(QStringLiteral("too large"), Qt::CaseInsensitive), "the ordinary row loads as ever");

		// ---- BPR-2049: Seen and Approve again --------------------------------
		const auto approved = row_of(*window, 18);
		expect(approved.Group == qt::home_group::needs_you && approved.Index.data(qt::home_row_role::row_action).toString() == QStringLiteral("Seen"),
			   "my approval with commits after it is in Needs me and offers Seen");
		const auto dismissed = row_of(*window, 19);
		expect(dismissed.Group == qt::home_group::needs_you && dismissed.Index.data(qt::home_row_role::row_action).toString() == QStringLiteral("Approve again"),
			   "a dismissed approval offers Approve again");
		expect(oversized.Index.data(qt::home_row_role::row_action).toString().isEmpty(), "a row I have not reviewed offers neither");
		if (renderAt >= 0) {
			window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/home-oversized-seen.png"));
		}
		auto* const seen = hovered_action(approved);
		if (attended) {
			goto attend;
		}
		expect(seen != nullptr && seen->text() == QStringLiteral("Seen") && seen->toolTip().contains(QStringLiteral("Nothing changes on GitHub")), "Seen shows on the row under the pointer, saying it writes nothing");
		const auto reads = discovery().Statistics.Submitted;
		seen->click();
		wait_for([&] { return row_of(*window, 18).Group == qt::home_group::following; }, "Seen moves the row to Following at once", 5'000);
		expect(discovery().Statistics.Submitted == reads, "Seen asks GitHub for nothing");
		expect(row_of(*window, 18).Index.data(qt::home_row_role::row_action).toString().isEmpty(), "a seen row offers nothing more");

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
