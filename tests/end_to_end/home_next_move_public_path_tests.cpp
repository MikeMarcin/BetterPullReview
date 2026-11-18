// [[RFC-BPR-0098]]. Home says whose move each of my pull requests is,
// through the composed application on the public-path fixture. The fixture
// serves ten pull requests of mine, each with the facts of one next move
// (`home_next_move_fixture_rows`), and one of another author's asking for my
// review. The course signs in, saves the repository, lets discovery list Home
// and refresh it once more, then reads Home as a person would: which section
// each row is in, in what order, which word leads it, in which colour, and
// what its sentence and the section header say. A refresh is logged: the
// conversations ride the one batched query Home already makes (AC05).
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "account_setup_course.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <functional>
#include <iostream>
#include <mutex>
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

	struct shown_row
	{
		QString Identity, Word, Why;
		int     Tone = -1;
	};

	[[nodiscard]] auto rows_of(qt::workbench_window& aWindow, const qt::home_group aGroup) -> std::vector<shown_row>
	{
		auto        rows  = std::vector<shown_row>{ };
		auto* const panel = aWindow.home_list()->state_panel(aGroup);
		auto* const view  = aWindow.home_list()->state_view(aGroup);
		for (auto row = 0; panel->isVisible() && view != nullptr && row < view->model()->rowCount(); ++row) {
			const auto index = view->model()->index(row, 0);
			const auto tone  = index.data(qt::home_row_role::tone);
			rows.push_back({ .Identity = index.data(qt::home_row_role::identity).toString(),
							 .Word     = index.data(qt::home_row_role::reason_word).toString(),
							 .Why      = index.data(qt::home_row_role::why).toString(),
							 .Tone     = tone.isValid() ? tone.toInt() : -1 });
		}
		return rows;
	}

	[[nodiscard]] auto row(const std::vector<shown_row>& aRows, const int aNumber) -> shown_row
	{
		const auto found = std::ranges::find_if(aRows, [aNumber](const shown_row& aRow) { return aRow.Identity.endsWith(QStringLiteral("#%1").arg(aNumber)); });
		expect(found != aRows.end(), "row #" + std::to_string(aNumber) + " is in its section");
		return *found;
	}

	[[nodiscard]] auto described(const std::vector<shown_row>& aRows) -> std::string
	{
		auto text = std::string{ };
		for (const auto& shown : aRows) {
			text += (shown.Identity + QLatin1Char{ ' ' } + shown.Word + QStringLiteral(": ") + shown.Why + QLatin1Char{ '\n' }).toStdString();
		}
		return text;
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private home-next-move course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	auto delay                     = std::make_shared<app::detail::github_read_delay_fixture>();
	options.ReadDelay              = delay;
	options.PersonalDiscovery      = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = { { .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" } } },
	};
	for (auto& pull : app::detail::home_next_move_fixture_pull_requests("octo-reviewer")) {
		options.PersonalDiscovery->OpenPullRequests.push_back(std::move(pull));
	}
	const auto listed    = static_cast<int>(options.PersonalDiscovery->OpenPullRequests.size());
	const auto arguments = QApplication::arguments();
	const auto renderAt  = arguments.indexOf(QStringLiteral("--render"));

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

	// RFC-BPR-0032 amendment r1 (f0dfd707c), missed here ([[BPR-2215]]):
	// signed in from Home's account setup, the reviewer leaves its Ready step
	// with Start reviewing. Left on it, Home's lists never show.
	auto ready = e2e::account_setup_driver{ *window, e2e::account_setup_driver::plan{ .Way = core::account_setup_way::browser } };
	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready && !window->account_setup()->isVisible(); },
			 "Home sign-in reaches an account session");
	ready.stop();
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	window->settings()->hide();
	window->choose_home(true);
	const auto discovery = [&] { return access::personal_discovery_evidence(*owner); };
	const auto shown     = [&] {
		auto count = std::size_t{ 0U };
		for (const auto group : { qt::home_group::needs_you, qt::home_group::my_pull_requests, qt::home_group::waiting_on_others, qt::home_group::following }) {
			count += rows_of(*window, group).size();
		}
		return static_cast<int>(count);
	};
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (shown() != listed && elapsed.elapsed() < 60000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (shown() != listed) {
			std::cout << "HOME_NEXT_MOVE_LISTED shown=" << shown() << " listed=" << listed << " state=" << static_cast<int>(discovery().Status.State)
					  << " quiescent=" << discovery().Quiescent << " elapsed_ms=" << elapsed.elapsed() << '\n';
		}
	}
	wait_for([&] { return shown() == listed; }, "Home lists every discovered pull request");
	wait_for([&] { return discovery().Status.State != application::queue_discovery_state::refreshing && discovery().Quiescent; }, "Home's first refresh settles");

	// The search that found them read titles and requests, not reviews,
	// decisions or checks: nothing is claimed Quiet before they are read.
	for (const auto& first : rows_of(*window, qt::home_group::my_pull_requests)) {
		expect(first.Word != QStringLiteral("Quiet") && (first.Word != QStringLiteral("Unknown") || first.Why == QStringLiteral("Its reviews and checks have not been read yet.")),
			   "a row whose reviews are unread is not called Quiet:\n" + described(rows_of(*window, qt::home_group::my_pull_requests)));
	}
	// A refresh reads every listed pull request again, mine in the batch
	// that carries their conversations. It is logged.
	auto before = std::size_t{ 0U };
	{
		const std::scoped_lock lock{ delay->ReadLogMutex };
		before = delay->ReadLog.size();
	}
	delay->EveryReadEverywhere = true;
	window->queue_refresh_action()->trigger();
	wait_for([&] { return discovery().Status.State == application::queue_discovery_state::refreshing; }, "Refresh starts", 10'000);
	wait_for([&] { return discovery().Status.State != application::queue_discovery_state::refreshing && discovery().Quiescent; }, "the refresh settles");
	delay->EveryReadEverywhere = false;
	auto operations            = std::vector<std::string>{ };
	{
		const std::scoped_lock lock{ delay->ReadLogMutex };
		for (auto index = before; index < delay->ReadLog.size(); ++index) {
			operations.push_back(delay->ReadLog[index].Operation);
		}
	}
	auto log = std::string{ };
	for (const auto& operation : operations) {
		log += operation + ' ';
	}
	// Eleven pull requests are two batches of at most ten, as before; the
	// searches are the refresh's own. Nothing else is read.
	const auto counted = [&operations](const std::string_view aName) { return std::ranges::count_if(operations, [aName](const std::string& aOperation) { return aOperation.ends_with(aName); }); };
	expect(counted("ListHomePullRequests") == 2 && counted("SearchPersonalPullRequests") == 2 && operations.size() == 4U,
		   "a refresh is its two searches and two batched queries, no more: " + log);

	if (renderAt >= 0) {
		window->grab().save(arguments.value(renderAt + 1) + QStringLiteral("/home-next-move.png"));
	}

	// AC02. Placement.
	const auto needs   = rows_of(*window, qt::home_group::needs_you);
	const auto mine    = rows_of(*window, qt::home_group::my_pull_requests);
	const auto waiting = rows_of(*window, qt::home_group::waiting_on_others);
	expect(needs.size() == 1U && needs.front().Identity.endsWith(QStringLiteral("#17")), "Needs me holds only the review I was asked for:\n" + described(needs));
	expect(waiting.size() == 1U && waiting.front().Identity.endsWith(QStringLiteral("#4037")) && waiting.front().Word == QStringLiteral("Waiting"),
		   "Waiting on others holds the one pull request waiting on reviewers:\n" + described(waiting));
	expect(mine.size() == 9U, "every other pull request of mine is in My pull requests:\n" + described(mine));
	// The table's order: Returned, Failing, Blocked, Ready, Unassigned, Draft,
	// Checking, Quiet; within a move, the existing order.
	auto words = QStringList{ };
	for (const auto& shownRow : mine) {
		words.push_back(shownRow.Word);
	}
	expect(words == QStringList{ "Returned", "Returned", "Failing", "Blocked", "Ready", "Unassigned", "Draft", "Checking", "Quiet" },
		   "My pull requests is in next-move order:\n" + described(mine));

	// AC03. Words and sentences, as the design round frames them.
	const auto expectRow = [&](const int aNumber, const QString& aWord, const QString& aWhy, const qt::home_row_tone aTone) {
		const auto shownRow = row(mine, aNumber);
		expect(shownRow.Word == aWord && shownRow.Why == aWhy && shownRow.Tone == static_cast<int>(aTone),
			   "#" + std::to_string(aNumber) + " reads \"" + shownRow.Word.toStdString() + " " + shownRow.Why.toStdString() + "\" in tone " + std::to_string(shownRow.Tone));
	};
	expectRow(4030, QStringLiteral("Returned"), QStringLiteral("alice requested changes; 3 conversations await my reply."), qt::home_row_tone::needs_me);
	expectRow(4031, QStringLiteral("Returned"), QStringLiteral("2 conversations await my reply."), qt::home_row_tone::needs_me);
	expectRow(4032, QStringLiteral("Ready"), QStringLiteral("Approved by alice and bo; checks passed. Ready to merge."), qt::home_row_tone::confirmed);
	expectRow(4033, QStringLiteral("Failing"), QStringLiteral("build-linux failed; approved by alice."), qt::home_row_tone::error);
	expectRow(4034, QStringLiteral("Blocked"), QStringLiteral("It conflicts with the base branch."), qt::home_row_tone::error);
	expectRow(4035, QStringLiteral("Unassigned"), QStringLiteral("No reviewer is requested."), qt::home_row_tone::my_pull_requests);
	expectRow(4036, QStringLiteral("Draft"), QStringLiteral("Still a draft."), qt::home_row_tone::my_pull_requests);
	expectRow(4038, QStringLiteral("Checking"), QStringLiteral("Approved by bo; 2 checks are running."), qt::home_row_tone::my_pull_requests);
	expectRow(4039, QStringLiteral("Quiet"), QStringLiteral("Nothing here is waiting on anyone."), qt::home_row_tone::my_pull_requests);
	for (const auto& shownRow : mine) {
		expect(!shownRow.Why.contains(QStringLiteral("My pull request")), "the section says it is mine; no sentence does");
	}

	// The header counts what came back and what can merge.
	const auto summary = window->home_list()->state_panel(qt::home_group::my_pull_requests)->summary();
	expect(summary == QStringLiteral("2 returned, 1 ready to merge"), "My pull requests' header: " + summary.toStdString());

	// `--attended`: the owner's look (RFC-BPR-0098 AC06). The same composed
	// window, every assertion above already passed, stays open on Home until
	// the person closes it.
	if (arguments.contains(QStringLiteral("--attended"))) {
		window->resize(1100, 900);
		window->show();
		window->raise();
		QApplication::setQuitOnLastWindowClosed(true);
		std::cout << "HOME_NEXT_MOVE_ATTENDED_READY: fixture account; no live GitHub reads\n"
				  << std::flush;
		QApplication::exec();
	}
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "home-next-move-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "home-next-move-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
