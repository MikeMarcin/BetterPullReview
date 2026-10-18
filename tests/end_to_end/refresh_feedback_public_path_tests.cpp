// [[BPR-1575]]. Refresh made visible, through the composed application on the
// fixture network: the reviewer's Refresh of the open pull request answered
// slowly with a new reply, with nothing new, offline and then retried, and
// rate limited; an update the window makes by itself; and Home's discovery
// refreshed slowly and then rate limited. Each says it started in the
// gesture's turn, what it found or why it failed, ages in the status bar and
// is announced once. Nothing reaches a live provider.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/refresh_note.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	using outcome = app::detail::github_read_delay_fixture::refresh_outcome;

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

	[[nodiscard]] auto button_of(QAction* const aAction) -> QToolButton*
	{
		for (auto* const object : aAction->associatedObjects()) {
			if (auto* const button = qobject_cast<QToolButton*>(object); button != nullptr) {
				return button;
			}
		}
		return nullptr;
	}

	void script(app::detail::github_read_delay_fixture& aDelay, std::vector<outcome> aScript)
	{
		const std::scoped_lock lock{ aDelay.ScriptMutex };
		aDelay.RefreshScript = std::move(aScript);
		aDelay.ScriptNext    = 0U;
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    renderArgument  = QApplication::arguments().indexOf(QStringLiteral("--render-directory"));
	const auto    renderDirectory = renderArgument >= 0 ? QApplication::arguments().value(renderArgument + 1) : QString{ };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private refresh course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto delay                     = std::make_shared<app::detail::github_read_delay_fixture>();
	auto limits                    = std::make_shared<app::detail::github_rate_limit_fixture>();
	limits->RetryAfterSeconds      = 45U;
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	options.ReadDelay              = delay;
	options.RateLimit              = limits;
	options.PersonalDiscovery      = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = { { .Number = 17U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" } } },
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
	const auto served = QStringLiteral("octo-org/review-repository#17");
	wait_for([&] { return row_of(*window, served).View != nullptr; }, "the saved repository's pull request is on Home");
	window->settings()->hide();
	window->choose_home(true);
	double_click(row_of(*window, served));
	wait_for([&] { return !window->showing_home() && window->github_refresh_action()->isEnabled() && window->pull_request_refresh().UpdatedAt.has_value(); },
			 "the pull request opens, up to date");

	std::vector<QString> heard;
	Announcements       = &heard;
	const auto previous = QAccessible::installUpdateHandler(capture);
	const auto renders  = [&](const QString& aName) {
		if (renderDirectory.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(renderDirectory), "render directory exists");
		const auto mode = QString::fromUtf8(core::stable_name(owner->status().ProfileMode));
		expect(window->grab().save(renderDirectory + QStringLiteral("/%1-%2.png").arg(mode, aName)), "render saved");
	};
	const auto heardOnce = [&](const QString& aSaid) { return std::ranges::count(heard, aSaid) == 1; };
	auto*      refresh   = window->github_refresh_action();
	auto*      note      = window->refresh_note_widget();
	const auto settlePr  = [&](const char* aMessage) {
		wait_for([&] { return window->pull_request_refresh().Phase != core::refresh_phase::refreshing && refresh->isEnabled(); }, aMessage);
	};

	for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark }) {
		window->appearance_settings()->choose_mode(mode);
		wait_for([&] { return owner->status().ProfileMode == mode; }, "the rendered appearance is accepted");
		window->settings()->hide();
		window->choose_home(false);
		pause(100);

		// ---- the reviewer's Refresh, slow, bringing a new reply ------------
		heard.clear();
		script(*delay, { outcome::new_reply });
		delay->RefreshMilliseconds = 2'500U;
		refresh->trigger();
		expect(window->pull_request_refresh().Phase == core::refresh_phase::refreshing && window->pull_request_refresh().Trigger == core::refresh_trigger::reviewer,
			   "in the gesture's turn the refresh is under way, the reviewer's");
		expect(button_of(refresh)->property(qt::strip_style::busy_property).toBool(), "the button reads as engaged in the same turn");
		expect(note->isVisible() && note->text() == QStringLiteral("Refreshing the pull request…"), "the note says it at once");
		expect(heardOnce(QStringLiteral("Refreshing the pull request.")), "a screen reader hears it start");
		renders(QStringLiteral("pr-refresh-started"));
		pause(800);
		expect(window->pending_veils().Content->look() == core::pending_look::refreshing, "after the reveal the review is veiled with the rule");
		expect(window->refresh_freshness_text() == QStringLiteral("Refreshing…"), "the status bar says it is refreshing");
		renders(QStringLiteral("pr-refresh-under-way"));
		settlePr("the slow refresh settles");
		expect(window->pull_request_refresh().Phase == core::refresh_phase::updated, "the refresh answered");
		expect(window->pull_request_refresh().Changes.NewComments == 1U, "it found the new reply");
		expect(note->text() == QStringLiteral("Updated: 1 new comment."), "and says so under the button");
		expect(heardOnce(note->text()), "and to a screen reader");
		expect(window->pending_veils().Content->look() == core::pending_look::ready, "the veil lifts");
		expect(window->refresh_freshness_text() == QStringLiteral("Updated just now"), "the status bar says how fresh");
		renders(QStringLiteral("pr-refresh-updated"));

		// ---- nothing new --------------------------------------------------
		script(*delay, { outcome::serve });
		delay->RefreshMilliseconds = 300U;
		refresh->trigger();
		settlePr("the quick refresh settles");
		expect(note->text() == QStringLiteral("Up to date: nothing new."), "a refresh that found nothing says so");
		renders(QStringLiteral("pr-refresh-nothing-new"));

		// ---- offline, then Retry ------------------------------------------
		script(*delay, { outcome::offline, outcome::serve });
		heard.clear();
		refresh->trigger();
		settlePr("the offline refresh settles");
		expect(window->pull_request_refresh().Phase == core::refresh_phase::failed && window->pull_request_refresh().Failure == core::refresh_failure::offline,
			   "an offline refresh fails as offline");
		expect(note->tone() == qt::refresh_note_tone::failed && note->retry_button()->isVisible(), "the note says why and offers Retry");
		expect(button_of(refresh)->property(qt::strip_style::alert_property).value<QColor>().isValid(), "the button keeps a failure dot");
		expect(std::ranges::any_of(heard, [](const QString& aSaid) { return aSaid.endsWith(QStringLiteral("Retry is available.")); }), "the failure is announced");
		expect(window->overview_page()->projection().has_value(), "the review is kept");
		renders(QStringLiteral("pr-refresh-failed"));
		note->retry_button()->click();
		settlePr("Retry settles");
		expect(window->pull_request_refresh().Phase == core::refresh_phase::updated, "Retry refreshes");

		// ---- an update the window makes by itself -------------------------
		heard.clear();
		script(*delay, { outcome::new_reply });
		window->run_automatic_refresh([refresh] { refresh->trigger(); });
		expect(window->pull_request_refresh().Trigger == core::refresh_trigger::automatic, "the window's own read is automatic");
		expect(!(note->isVisible() && note->tone() == qt::refresh_note_tone::busy), "it starts without a note");
		settlePr("the automatic refresh settles");
		expect(note->text() == QStringLiteral("Updated automatically: 1 new comment."), "it speaks because it brought news");
		expect(!heardOnce(QStringLiteral("Refreshing the pull request.")), "its start is not announced");
		renders(QStringLiteral("pr-refresh-automatic"));

		// ---- Home's discovery ---------------------------------------------
		window->choose_home(true);
		pause(100);
		delay->DiscoveryMilliseconds = 2'500U;
		wait_for([&] { return window->queue_refresh_action()->isEnabled(); }, "Home's refresh is available");
		window->queue_refresh_action()->trigger();
		wait_for([&] { return window->home_refresh().Phase == core::refresh_phase::refreshing; }, "Home's refresh starts");
		expect(window->home_refresh().Trigger == core::refresh_trigger::reviewer, "it is the reviewer's");
		expect(note->text() == QStringLiteral("Finding my pull requests…"), "Home's note says it");
		renders(QStringLiteral("home-refresh-started"));
		pause(800);
		expect(window->pending_veils().HomeList->look() == core::pending_look::refreshing, "Home's list is veiled while it is refreshed");
		renders(QStringLiteral("home-refresh-under-way"));
		wait_for([&] { return window->home_refresh().Phase != core::refresh_phase::refreshing; }, "Home's refresh settles");
		expect(note->text() == QStringLiteral("Up to date: no new pull requests."), "Home's note says what it found");
		renders(QStringLiteral("home-refresh-updated"));
		delay->DiscoveryMilliseconds = 0U;
	}

	// ---- rate limits, last: the window then holds reads for GitHub's wait --
	window->choose_home(false);
	pause(100);
	script(*delay, { outcome::rate_limited });
	delay->RefreshMilliseconds = 0U;
	refresh->trigger();
	settlePr("the rate-limited refresh settles");
	expect(window->pull_request_refresh().Failure == core::refresh_failure::rate_limited, "a rate limit is told apart");
	expect(note->text().contains(QStringLiteral("rate limit")), "and said");
	renders(QStringLiteral("pr-refresh-rate-limited"));
	window->choose_home(true);
	pause(100);
	limits->Mode = 1U;
	wait_for([&] { return window->queue_refresh_action()->isEnabled(); }, "Home's refresh is available again");
	window->queue_refresh_action()->trigger();
	wait_for([&] { return window->home_refresh().Phase == core::refresh_phase::failed; }, "Home's refresh meets the rate limit");
	expect(window->home_refresh().Failure == core::refresh_failure::rate_limited, "Home's discovery fails as rate limited");
	expect(note->tone() == qt::refresh_note_tone::failed, "and says so under its button");
	renders(QStringLiteral("home-refresh-rate-limited"));
	limits->Mode = 0U;

	static_cast<void>(QAccessible::installUpdateHandler(previous));
	Announcements = nullptr;
	script(*delay, { });
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "refresh-feedback-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "refresh-feedback-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
} catch (...) {
	return bpr::tests::failing_exit();
}
