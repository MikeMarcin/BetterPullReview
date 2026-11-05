// [[BPR-1865]]. Home's filter through the composed application, on the
// public-path fixture with eighteen discovered pull requests (the owner: "I
// want to be able to fuzzy filter the home screen to quickly find a PR").
// The course signs in, saves the repository, lets discovery list Home, then
// filters it as a reviewer would: Command-F, a number, a word from a title,
// an author, nothing at all, Escape twice, and finally "#18" and Return,
// which opens that pull request. Coming back to Home finds it whole. No
// filtering reads anything: discovery's requests are counted before and
// after.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

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

	void settle(const int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
	}

	void send_key(QWidget& aWidget, const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier, const QString& aText = { })
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers, aText };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers, aText };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}

	constexpr auto sections = std::array{ qt::home_group::needs_you, qt::home_group::my_pull_requests, qt::home_group::waiting_on_others, qt::home_group::following };

	/// Every queue row Home shows, in reading order.
	[[nodiscard]] auto shown_rows(qt::workbench_window& aWindow) -> QStringList
	{
		auto rows = QStringList{ };
		for (const auto group : sections) {
			auto* const panel = aWindow.home_list()->state_panel(group);
			auto* const view  = aWindow.home_list()->state_view(group);
			for (auto row = 0; panel->isVisible() && view != nullptr && row < view->model()->rowCount(); ++row) {
				rows.push_back(view->model()->index(row, 0).data(qt::home_row_role::identity).toString());
			}
		}
		return rows;
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private home-filter course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	// No delay: its log is what proves filtering reads nothing.
	auto delay                = std::make_shared<app::detail::github_read_delay_fixture>();
	options.ReadDelay         = delay;
	options.PersonalDiscovery = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = {
			{ .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" } },
			{ .Number = 18U, .Author = "contributor-b", .RequestedUsers = { "octo-reviewer" } },
		},
	};
	for (auto& row : app::detail::home_filter_fixture_pull_requests("octo-reviewer")) {
		options.PersonalDiscovery->OpenPullRequests.push_back(std::move(row));
	}
	const auto listed = static_cast<int>(options.PersonalDiscovery->OpenPullRequests.size());
	// `--render DIR` writes what the window shows, for a person to look at.
	const auto arguments = QApplication::arguments();
	const auto renderAt  = arguments.indexOf(QStringLiteral("--render"));
	const auto render    = [&](QWidget& aWindow, const QString& aName) {
		if (renderAt >= 0) {
			aWindow.grab().save(arguments.value(renderAt + 1) + QStringLiteral("/") + aName + QStringLiteral(".png"));
		}
	};

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
	window->resize(1280, 900);
	window->activateWindow();

	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "Home sign-in reaches an account session");
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	window->settings()->hide();
	window->choose_home(true);
	wait_for([&] { return shown_rows(*window).size() == listed; }, "Home lists every discovered pull request");
	const auto discovery = [&] { return access::personal_discovery_evidence(*owner); };
	wait_for([&] { return discovery().Status.State != application::queue_discovery_state::refreshing && discovery().Quiescent; }, "Home's first refresh settles");

	auto* const filter = window->home_filter_box();
	auto* const field  = filter->line_edit();
	auto* const list   = window->home_list();
	expect(filter->isVisible() && filter->y() > window->home_open_bar()->mapTo(filter->parentWidget(), QPoint{ }).y(), "the filter stands under the Open bar");
	// From here every account read is logged, Home's included.
	delay->EveryReadEverywhere = true;
	const auto logged          = [&delay] {
		const std::scoped_lock lock{ delay->ReadLogMutex };
		return delay->ReadLog.size();
	};
	const auto reads = logged();

	render(*window, QStringLiteral("home-rest"));
	// Command-F: the window's Find key is the filter's on Home.
	window->activateWindow();
	wait_for([&] { return QApplication::activeWindow() == window; }, "the window is active again once Settings is closed");
	expect(window->home_filter_action()->shortcuts() == QList{ QKeySequence{ QKeySequence::Find } }, "on Home the Find key is the filter's");
	window->home_filter_action()->trigger();
	wait_for([&] { return field->hasFocus(); }, "Command-F takes the keyboard to the filter", 5'000);

	field->setText(QStringLiteral("18"));
	settle(30);
	expect(list->filter_result().data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/review-repository#18"), "a number's exact row is the current result");
	expect(shown_rows(*window).contains(QStringLiteral("octo-org/review-repository#18")) && !shown_rows(*window).contains(QStringLiteral("octo-org/review-repository#17")), "the rows that do not match are gone");
	render(*window, QStringLiteral("home-number"));

	field->setText(QStringLiteral("cache"));
	settle(30);
	render(*window, QStringLiteral("home-cache"));
	const auto cache = shown_rows(*window);
	expect(cache.size() == 2 && cache.contains(QStringLiteral("octo-org/review-repository#4011")) && cache.contains(QStringLiteral("octo-org/review-repository#4022")),
		   "a word finds the titles that hold it: " + cache.join(QLatin1Char{ ',' }).toStdString());
	expect(list->filter_result().data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/review-repository#4022"), "a title that starts with the word ranks first");
	expect(!list->filter_result().data(qt::home_row_role::title_marks).toList().isEmpty(), "and its matched characters are marked");
	for (const auto group : sections) {
		auto* const panel = list->state_panel(group);
		expect(panel->isVisible() == (list->state_model(group)->rowCount() > 0), "a section with no match is not shown");
	}
	expect(filter->count_label()->text() == QStringLiteral("2 matches"), "the field says how many it found");

	field->setText(QStringLiteral("fix"));
	settle(30);
	render(*window, QStringLiteral("home-fix"));
	field->setText(QStringLiteral("dmitri"));
	settle(30);
	expect(shown_rows(*window) == QStringList{ QStringLiteral("octo-org/review-repository#4016") }, "an author finds their pull request");
	field->setText(QStringLiteral("zebra"));
	settle(30);
	expect(shown_rows(*window).isEmpty() && list->filter_sentence()->isVisible(), "nothing matching says so in one sentence");
	render(*window, QStringLiteral("home-nothing"));

	send_key(*field, Qt::Key_Escape);
	expect(field->text().isEmpty() && shown_rows(*window).size() == listed && field->hasFocus(), "Escape clears the filter and every row is back");
	send_key(*field, Qt::Key_Escape);
	expect(qobject_cast<QListView*>(QApplication::focusWidget()) != nullptr && list->isAncestorOf(QApplication::focusWidget()), "Escape again leaves the field for the list");

	// Typing in the list starts the filter.
	send_key(*QApplication::focusWidget(), Qt::Key_NumberSign, Qt::ShiftModifier, QStringLiteral("#"));
	expect(field->hasFocus() && field->text() == QStringLiteral("#"), "a character typed in the list goes to the filter");
	field->setText(QStringLiteral("#18"));
	settle(30);
	expect(logged() == reads, "filtering reads nothing from GitHub");

	auto* const overview = window->overview_page();
	send_key(*field, Qt::Key_Return);
	wait_for([&] { return !window->showing_home() && overview->projection().has_value(); }, "Return opens the current result", 120'000);
	expect(window->identity_label()->text().contains(QStringLiteral("18")), "the pull request opened is #18: " + window->identity_label()->text().toStdString());
	window->choose_home(true);
	settle(50);
	expect(field->text().isEmpty() && shown_rows(*window).size() == listed, "coming back, Home is whole again");

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "home-filter-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "home-filter-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
