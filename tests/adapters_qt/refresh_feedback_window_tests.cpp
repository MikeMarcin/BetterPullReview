// [[BPR-1575]]. Refresh, made visible on the pull request and on Home, from the
// read's own stages: the button turns (or, under reduced motion, holds a still
// mark) and reads as engaged from the gesture's turn; a note under it says
// "Refreshing…", then what the read brought or why it failed, with Retry; the
// status bar says how old the content is and ages on the window's clock; an
// update the application made by itself stays quiet unless it brought news;
// a screen reader hears the start, the result and any failure once. Without
// the core's read stage nothing is said: the negative control.
#include "../support/test_expectation.hpp"
#include "bpr/qt/refresh_note.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	void settle(const int aMilliseconds = 40)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		do {
			QApplication::processEvents(QEventLoop::AllEvents, 10);
			QThread::msleep(2);
		} while (timer.elapsed() < aMilliseconds);
	}

	std::vector<QString>* Announcements = nullptr;

	void capture(QAccessibleEvent* const aEvent)
	{
		if (Announcements != nullptr && aEvent != nullptr && aEvent->type() == QAccessible::Announcement) {
			Announcements->push_back(static_cast<const QAccessibleAnnouncementEvent*>(aEvent)->message());
		}
	}

	[[nodiscard]] auto conversation(std::string aId, const application::github_conversation_kind aKind) -> application::github_conversation_projection
	{
		return application::github_conversation_projection{ .Id = std::move(aId), .Kind = aKind, .AuthorLogin = "octo", .Body = "A post." };
	}

	[[nodiscard]] auto pull_request(const std::uint64_t aSnapshot, std::vector<application::github_conversation_projection> aConversations, std::string aHead = "aaaa")
		-> application::github_pull_request_projection
	{
		return application::github_pull_request_projection{
			.Snapshot       = core::provider_snapshot_id{ aSnapshot },
			.Repository     = core::repository_id{ 3U },
			.PullRequest    = core::pull_request_id{ 4U },
			.Owner          = "octo",
			.RepositoryName = "repo",
			.Number         = core::pull_request_number{ 7U },
			.Title          = "Seven",
			.HeadOid        = std::move(aHead),
			.CommitCount    = 1U,
			.Conversations  = std::move(aConversations),
		};
	}

	[[nodiscard]] auto read(const core::pull_request_read_stage aStage, const std::uint64_t aRequest, const bool aRefresh = true,
							const core::github_reason_code aReason = core::github_reason_code::none) -> application::github_presentation_update
	{
		auto status = core::github_status::loading_pull_request;
		switch (aStage) {
		case core::pull_request_read_stage::pending:   status = core::github_status::loading_pull_request; break;
		case core::pull_request_read_stage::loaded:    status = core::github_status::pull_request_ready; break;
		case core::pull_request_read_stage::failed:    status = core::github_status::pull_request_failed; break;
		case core::pull_request_read_stage::cancelled: status = core::github_status::pull_request_cancelled; break;
		}
		return application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = status },
				.Read   = core::pull_request_read_delta{ .Request = core::request_id{ aRequest }, .Repository = { .Owner = "octo", .Name = "repo" }, .Number = core::pull_request_number{ 7U }, .Stage = aStage, .Refresh = aRefresh, .Reason = aReason },
			},
		};
	}

	struct fixture
	{
		std::shared_ptr<std::int64_t> Now = std::make_shared<std::int64_t>(10'000);
		qt::workbench_window          Window;
		std::vector<QString>          Heard;
		QAccessible::UpdateHandler    Before = nullptr;

		fixture()
		{
			Window.set_home_clock([now = Now]() -> std::optional<std::int64_t> { return *now; });
			Window.resize(1'280, 820);
			Window.show();
			settle();
			// The first read opens the pull request: not a refresh.
			Window.publish(read(core::pull_request_read_stage::pending, 1U, false));
			auto loaded        = read(core::pull_request_read_stage::loaded, 1U, false);
			loaded.PullRequest = pull_request(1U, { conversation("IC_1", application::github_conversation_kind::issue_comment) });
			Window.publish(loaded);
			Window.choose_home(false);
			settle();
			Announcements = &Heard;
			Before        = QAccessible::installUpdateHandler(capture);
		}

		~fixture()
		{
			static_cast<void>(QAccessible::installUpdateHandler(Before));
			Announcements = nullptr;
		}

		fixture(const fixture&)                    = delete;
		auto operator=(const fixture&) -> fixture& = delete;

		[[nodiscard]] auto button() -> QWidget*
		{
			for (auto* const object : Window.github_refresh_action()->associatedObjects()) {
				if (auto* const tool = qobject_cast<QToolButton*>(object); tool != nullptr) {
					return tool;
				}
			}
			return nullptr;
		}
	};

	void test_the_reviewers_refresh_is_seen_from_its_first_turn()
	{
		auto subject = fixture{ };
		expect(subject.Window.pull_request_refresh().UpdatedAt == 10'000, "an opened pull request is up to date as it opens");
		expect(subject.Window.refresh_freshness_text() == QStringLiteral("Updated just now"), "the status bar says so");
		subject.Window.publish(read(core::pull_request_read_stage::pending, 2U));
		// No event loop turn: the read's own pending stage is what is shown.
		expect(subject.Window.pull_request_refresh().Phase == core::refresh_phase::refreshing
				   && subject.Window.pull_request_refresh().Trigger == core::refresh_trigger::reviewer,
			   "the reviewer's refresh is under way in the gesture's turn");
		auto* const button = subject.button();
		expect(button != nullptr && button->property(qt::strip_style::busy_property).toBool(), "the button reads as engaged");
		auto* const note = subject.Window.refresh_note_widget();
		expect(note->isVisible() && note->tone() == qt::refresh_note_tone::busy && note->text() == QStringLiteral("Refreshing the pull request…"),
			   "the note under the button says it in words");
		expect(subject.Window.refresh_freshness_text() == QStringLiteral("Refreshing…"), "and the status bar");
		expect(std::ranges::count(subject.Heard, QStringLiteral("Refreshing the pull request.")) == 1, "a screen reader hears it start");
		*subject.Now += 30;
		auto loaded        = read(core::pull_request_read_stage::loaded, 2U);
		loaded.PullRequest = pull_request(2U, { conversation("IC_1", application::github_conversation_kind::issue_comment), conversation("IC_2", application::github_conversation_kind::issue_comment), conversation("PRRC_1", application::github_conversation_kind::review_comment), conversation("PRR_1", application::github_conversation_kind::review) },
										  "bbbb");
		subject.Window.publish(loaded);
		const auto& state = subject.Window.pull_request_refresh();
		expect(state.Phase == core::refresh_phase::updated && state.Changes.NewComments == 2U && state.Changes.NewReviews == 1U && state.Changes.NewPush,
			   "the read's answer is compared with what was shown");
		expect(note->tone() == qt::refresh_note_tone::settled && note->text() == QStringLiteral("Updated: 2 new comments, 1 new review and 1 new push."),
			   "the note says what changed");
		expect(std::ranges::count(subject.Heard, note->text()) == 1, "and so does the announcement, once");
		expect(!button->property(qt::strip_style::busy_property).toBool(), "the button rests");
		*subject.Now += 150;
		expect(subject.Window.refresh_freshness_text() == QStringLiteral("Updated 2 min ago"), "the freshness ages on the window's clock");
	}

	void test_nothing_new_is_said_as_nothing_new()
	{
		auto subject = fixture{ };
		subject.Window.publish(read(core::pull_request_read_stage::pending, 2U));
		auto loaded        = read(core::pull_request_read_stage::loaded, 2U);
		loaded.PullRequest = pull_request(1U, { conversation("IC_1", application::github_conversation_kind::issue_comment) });
		subject.Window.publish(loaded);
		expect(subject.Window.refresh_note_widget()->text() == QStringLiteral("Up to date: nothing new."), "a refresh that found nothing says so");
		expect(std::ranges::count(subject.Heard, QStringLiteral("Up to date: nothing new.")) == 1, "the reviewer hears it");
	}

	void test_a_failure_keeps_the_review_and_offers_retry()
	{
		auto subject = fixture{ };
		subject.Window.publish(read(core::pull_request_read_stage::pending, 2U));
		subject.Window.publish(read(core::pull_request_read_stage::failed, 2U, true, core::github_reason_code::network_unavailable));
		const auto& state = subject.Window.pull_request_refresh();
		expect(state.Phase == core::refresh_phase::failed && state.Failure == core::refresh_failure::offline && state.UpdatedAt == 10'000, "an offline refresh keeps what was shown");
		auto* const note = subject.Window.refresh_note_widget();
		expect(note->tone() == qt::refresh_note_tone::failed && note->text().startsWith(QStringLiteral("Couldn't refresh: GitHub could not be reached.")), "the note says why");
		expect(note->retry_button()->isVisible(), "with Retry");
		expect(subject.button()->property(qt::strip_style::alert_property).value<QColor>().isValid(), "the button keeps a failure dot");
		expect(subject.Window.refresh_freshness_label()->text().contains(QStringLiteral("Retry")), "the status bar offers Retry too");
		expect(std::ranges::any_of(subject.Heard, [](const QString& aSaid) { return aSaid.startsWith(QStringLiteral("Couldn't refresh")) && aSaid.endsWith(QStringLiteral("Retry is available.")); }), "a screen reader hears the failure and the way on");
		auto refreshes = 0;
		QObject::connect(subject.Window.github_refresh_action(), &QAction::triggered, [&refreshes] { ++refreshes; });
		subject.Window.github_refresh_action()->setEnabled(true);
		note->retry_button()->click();
		expect(refreshes == 1, "Retry is the Refresh");
		subject.Window.publish(read(core::pull_request_read_stage::pending, 3U));
		auto limited = read(core::pull_request_read_stage::failed, 3U, true, core::github_reason_code::rate_limited);
		limited.HostDiagnostics.push_back({ .Reason = core::github_reason_code::rate_limited, .RetryAfterSeconds = 90U });
		subject.Window.publish(limited);
		expect(subject.Window.pull_request_refresh().Failure == core::refresh_failure::rate_limited && subject.Window.pull_request_refresh().RetryAfterSeconds == 90U,
			   "a rate limit is told apart, with GitHub's wait");
		expect(note->text().contains(QStringLiteral("rate limit")) && note->text().contains(QStringLiteral("Try again in 2 min.")), "and says how long to wait");
	}

	void test_an_automatic_update_is_quiet_unless_it_brings_news()
	{
		auto subject = fixture{ };
		subject.Heard.clear();
		subject.Window.run_automatic_refresh([&subject] { subject.Window.publish(read(core::pull_request_read_stage::pending, 2U)); });
		expect(subject.Window.pull_request_refresh().Trigger == core::refresh_trigger::automatic, "a read the application made is automatic");
		expect(!subject.Window.refresh_note_widget()->isVisible() && std::ranges::none_of(subject.Heard, [](const QString& aSaid) { return aSaid.contains(QStringLiteral("efresh")) || aSaid.contains(QStringLiteral("pdated")); }), "it starts without a note or an announcement");
		expect(subject.Window.refresh_freshness_text() == QStringLiteral("Updating…"), "the status bar says it is updating");
		auto loaded        = read(core::pull_request_read_stage::loaded, 2U);
		loaded.PullRequest = pull_request(1U, { conversation("IC_1", application::github_conversation_kind::issue_comment) });
		subject.Window.publish(loaded);
		expect(!subject.Window.refresh_note_widget()->isVisible() && std::ranges::none_of(subject.Heard, [](const QString& aSaid) { return aSaid.contains(QStringLiteral("pdated")) || aSaid.contains(QStringLiteral("Up to date")); }), "with nothing new it ends quietly");
		subject.Window.run_automatic_refresh([&subject] { subject.Window.publish(read(core::pull_request_read_stage::pending, 3U)); });
		auto news        = read(core::pull_request_read_stage::loaded, 3U);
		news.PullRequest = pull_request(2U, { conversation("IC_1", application::github_conversation_kind::issue_comment), conversation("IC_9", application::github_conversation_kind::issue_comment) });
		subject.Window.publish(news);
		expect(subject.Window.refresh_note_widget()->text() == QStringLiteral("Updated automatically: 1 new comment."), "with news it says so, and that it was automatic");
		expect(std::ranges::count(subject.Heard, QStringLiteral("Updated automatically: 1 new comment.")) == 1, "announced once");
	}

	void test_another_windows_answer_is_said_as_such()
	{
		auto subject = fixture{ };
		subject.Window.publish(application::github_presentation_update{
			.PullRequest              = pull_request(5U, { conversation("IC_1", application::github_conversation_kind::issue_comment), conversation("PRR_2", application::github_conversation_kind::review) }),
			.PreserveWindowNavigation = true });
		expect(subject.Window.pull_request_refresh().Trigger == core::refresh_trigger::elsewhere && subject.Window.pull_request_refresh().Changes.NewReviews == 1U,
			   "a sibling window's read handed over is a refresh made elsewhere");
		expect(subject.Window.refresh_note_widget()->text() == QStringLiteral("Updated from another window: 1 new review."), "and is said so");
	}

	// Negative control: a status change with no read stage named by the core
	// is not presented as a refresh; nothing here runs on a timer of its own.
	void test_without_the_read_stage_nothing_is_said()
	{
		auto subject = fixture{ };
		subject.Heard.clear();
		subject.Window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::loading_pull_request } } });
		settle(200);
		expect(subject.Window.pull_request_refresh().Phase == core::refresh_phase::never, "no read stage, no refresh");
		expect(!subject.Window.refresh_note_widget()->isVisible() && !subject.button()->property(qt::strip_style::busy_property).toBool(), "no note, no engaged button");
		subject.Window.publish(read(core::pull_request_read_stage::pending, 9U, false));
		expect(subject.Window.pull_request_refresh().Phase == core::refresh_phase::never, "a first read of another pull request is an opening, not a refresh");
	}

	void test_reduced_motion_holds_a_still_mark()
	{
		auto subject = fixture{ };
		subject.Window.adopt_dif(core::resolve_dif({ }, core::motion_observation::reduced));
		subject.Window.publish(read(core::pull_request_read_stage::pending, 2U));
		const auto icon = subject.Window.github_refresh_action()->icon().pixmap(24).toImage();
		settle(300);
		expect(subject.Window.github_refresh_action()->icon().pixmap(24).toImage() == icon, "under reduced motion the glyph does not turn");
		subject.Window.adopt_dif(core::resolve_dif({ }, core::motion_observation::full));
		subject.Window.publish(read(core::pull_request_read_stage::pending, 2U));
		const auto first = subject.Window.github_refresh_action()->icon().pixmap(24).toImage();
		settle(300);
		expect(subject.Window.github_refresh_action()->icon().pixmap(24).toImage() != first, "under full motion it turns");
	}

	void test_home_discovery_is_followed()
	{
		auto subject         = fixture{ };
		auto projection      = application::queue_landing_projection{ .Status = application::queue_landing_status::ready, .Session = { .Host = "github.com", .Account = "octo" } };
		projection.Discovery = { .Host = "github.com", .Account = "octo", .State = application::queue_discovery_state::current, .LastSuccess = 9'000, .Repositories = 1U };
		subject.Window.publish(projection);
		subject.Window.choose_home(true);
		settle();
		expect(subject.Window.home_refresh().UpdatedAt == 9'000, "Home's freshness is discovery's last success");
		subject.Window.queue_refresh_action()->setEnabled(true);
		projection.Discovery.State = application::queue_discovery_state::refreshing;
		subject.Window.queue_refresh_action()->trigger();
		subject.Window.publish(projection);
		expect(subject.Window.home_refresh().Phase == core::refresh_phase::refreshing && subject.Window.home_refresh().Trigger == core::refresh_trigger::reviewer,
			   "Home's Refresh is the reviewer's");
		expect(subject.Window.refresh_note_widget()->text() == QStringLiteral("Finding my pull requests…"), "Home's note says it");
		projection.Discovery.State             = application::queue_discovery_state::stale;
		projection.Discovery.Problem           = application::queue_discovery_problem::rate_limited;
		projection.Discovery.RetryAfterSeconds = 30U;
		subject.Window.publish(projection);
		expect(subject.Window.home_refresh().Failure == core::refresh_failure::rate_limited, "a rate-limited discovery fails as rate limited");
		expect(subject.Window.refresh_note_widget()->text().contains(QStringLiteral("Try again in 30 s.")), "with GitHub's wait");
	}

	void render(const QString& aDirectory)
	{
		if (aDirectory.isEmpty()) {
			return;
		}
		QDir{ }.mkpath(aDirectory);
		auto subject = fixture{ };
		subject.Window.publish(read(core::pull_request_read_stage::pending, 2U));
		subject.Window.grab().save(aDirectory + QStringLiteral("/refresh-started.png"));
		auto loaded        = read(core::pull_request_read_stage::loaded, 2U);
		loaded.PullRequest = pull_request(2U, { conversation("IC_1", application::github_conversation_kind::issue_comment), conversation("IC_2", application::github_conversation_kind::issue_comment) }, "bbbb");
		subject.Window.publish(loaded);
		subject.Window.grab().save(aDirectory + QStringLiteral("/refresh-updated.png"));
		subject.Window.publish(read(core::pull_request_read_stage::pending, 3U));
		subject.Window.publish(read(core::pull_request_read_stage::failed, 3U, true, core::github_reason_code::network_unavailable));
		subject.Window.grab().save(aDirectory + QStringLiteral("/refresh-failed.png"));
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	const auto   arguments = QApplication::arguments();
	const auto   at        = arguments.indexOf(QStringLiteral("--render-directory"));
	test_the_reviewers_refresh_is_seen_from_its_first_turn();
	test_nothing_new_is_said_as_nothing_new();
	test_a_failure_keeps_the_review_and_offers_retry();
	test_an_automatic_update_is_quiet_unless_it_brings_news();
	test_another_windows_answer_is_said_as_such();
	test_without_the_read_stage_nothing_is_said();
	test_reduced_motion_holds_a_still_mark();
	test_home_discovery_is_followed();
	render(at >= 0 ? arguments.value(at + 1) : QString{ });
	std::cout << "refresh-feedback-window-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
