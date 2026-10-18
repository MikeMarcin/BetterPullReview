// [[BPR-1576]]. The application's one loading-state pattern, presented: a
// pane's placeholders, its turning mark and words after the reveal, what a slow
// wait waits on, a light veil and a rule over content being replaced, partial
// content's plate, a failure with Retry, reduced motion, high contrast, busy
// state and announcements for assistive technology, and nothing under the veil
// moving. Then the panes that use it: the Overview's sections from opening to
// ready, the window's own panes while a first read runs and while a refresh
// replaces the review, and the Updates line's mark. Every look is driven by
// real facts and an injected clock; nothing is shown without them.
#include "../support/test_expectation.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QThread>
#include <QTreeView>
#include <QVBoxLayout>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	void settle(const int aMilliseconds = 60)
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

	/// A pane with rows the veil lies over, and a fake monotonic clock.
	struct pane
	{
		QWidget                           Window;
		QListWidget*                      List = nullptr;
		std::shared_ptr<std::int64_t>     Now  = std::make_shared<std::int64_t>(0);
		std::unique_ptr<qt::pending_veil> Veil;

		explicit pane(const qt::pending_shape aShape = qt::pending_shape::rows)
		{
			auto* const layout = new QVBoxLayout{ &Window };
			List               = new QListWidget{ &Window };
			for (auto row = 0; row < 12; ++row) {
				List->addItem(QStringLiteral("review/file-%1.cpp").arg(row));
			}
			layout->addWidget(List);
			Window.resize(480, 360);
			Veil = std::make_unique<qt::pending_veil>(*List, aShape, QStringLiteral("Files"));
			Veil->set_clock([now = Now] { return *now; });
			Window.show();
			settle();
		}

		void advance(const std::int64_t aMilliseconds)
		{
			*Now += aMilliseconds;
			Veil->advance();
			settle(20);
		}
	};

	[[nodiscard]] auto words() -> qt::pending_words
	{
		return { .Loading = QStringLiteral("Loading the file list…"),
				 .Waiting = QStringLiteral("Still waiting for GitHub: the changed files."),
				 .Coming  = QStringLiteral("Counting comments by file…"),
				 .Failure = QStringLiteral("Couldn't load the file list. GitHub could not be reached."),
				 .Done    = QStringLiteral("Files loaded.") };
	}

	void test_a_first_load_is_quiet_then_said_then_slow()
	{
		auto subject       = pane{ };
		auto announcements = std::vector<QString>{ };
		Announcements      = &announcements;
		const auto before  = QAccessible::installUpdateHandler(capture);
		subject.Veil->present(core::pending_activity::loading, false, false, words());
		settle(20);
		expect(subject.Veil->look() == core::pending_look::placeholder && subject.Veil->isVisible(), "a first load shows placeholders at once");
		expect(subject.Veil->shown_text().isEmpty() && subject.Veil->busy(), "before the reveal nothing is said, but the pane is busy");
		expect(subject.Veil->placeholders() > 0, "placeholders are drawn in the pane's shape");
		expect(announcements.empty(), "a wait that has not shown is not announced");
		subject.advance(core::pending_reveal_milliseconds);
		expect(subject.Veil->look() == core::pending_look::loading && subject.Veil->shown_text() == words().Loading, "after the reveal the words and the mark");
		expect(announcements.size() == 1U && announcements.back() == words().Loading, "the revealed wait is announced once");
		subject.advance(core::pending_slow_milliseconds);
		expect(subject.Veil->look() == core::pending_look::loading_slow && subject.Veil->shown_text() == words().Waiting, "a slow wait says what it waits on");
		expect(announcements.size() == 2U && announcements.back() == words().Waiting, "and that is announced");
		subject.Veil->present_ready();
		settle(20);
		expect(!subject.Veil->isVisible() && !subject.Veil->busy(), "ready shows the pane's own content");
		expect(announcements.size() == 3U && announcements.back() == words().Done, "a revealed wait announces its end");
		static_cast<void>(QAccessible::installUpdateHandler(before));
		Announcements = nullptr;
	}

	// Negative control: without time passing there is no spinner, no words and
	// no announcement however long the test waits in real time; the veil's
	// looks come from the facts and the presenter's clock, never a timer of its own.
	void test_a_fast_load_never_flashes()
	{
		auto subject       = pane{ };
		auto announcements = std::vector<QString>{ };
		Announcements      = &announcements;
		const auto before  = QAccessible::installUpdateHandler(capture);
		subject.Veil->present(core::pending_activity::loading, false, false, words());
		settle(core::pending_reveal_milliseconds + 200);
		expect(subject.Veil->look() == core::pending_look::placeholder, "an unadvanced clock never reveals the wait");
		subject.advance(core::pending_reveal_milliseconds - 1);
		expect(subject.Veil->look() == core::pending_look::placeholder, "one millisecond short of the reveal is still quiet");
		subject.Veil->present_ready();
		settle(20);
		expect(announcements.empty(), "a load faster than the reveal is never announced, start or end");
		static_cast<void>(QAccessible::installUpdateHandler(before));
		Announcements = nullptr;
	}

	[[nodiscard]] auto pixel(QWidget& aWindow, const QWidget& aWidget, const QPoint aAt) -> QColor
	{
		const auto image = aWindow.grab().toImage();
		const auto point = aWidget.mapTo(&aWindow, aAt) * image.devicePixelRatio();
		return image.pixelColor(point);
	}

	void test_a_refresh_veils_retained_content_and_blocks_nothing()
	{
		auto       subject  = pane{ };
		const auto row      = subject.List->visualItemRect(subject.List->item(3)).center();
		const auto plain    = pixel(subject.Window, *subject.List->viewport(), QPoint{ 4, row.y() });
		const auto geometry = subject.List->geometry();
		subject.Veil->present(core::pending_activity::loading, true, false, words());
		settle(20);
		expect(subject.Veil->look() == core::pending_look::quiet && subject.Veil->testAttribute(Qt::WA_TransparentForMouseEvents),
			   "a refresh before the reveal leaves the content as it is and lets the pointer through");
		expect(pixel(subject.Window, *subject.List->viewport(), QPoint{ 4, row.y() }) == plain, "nothing is drawn over a quiet refresh");
		subject.advance(core::pending_reveal_milliseconds);
		expect(subject.Veil->look() == core::pending_look::refreshing && subject.Veil->testAttribute(Qt::WA_TransparentForMouseEvents),
			   "a revealed refresh veils the content and blocks nothing");
		expect(subject.List->geometry() == geometry, "the veil moves nothing in the pane");
		subject.Veil->set_high_contrast(true);
		settle(20);
		const auto top = pixel(subject.Window, *subject.Veil, QPoint{ 40, 0 });
		expect(top != plain, "the rule runs along the pane's top edge");
		subject.Veil->set_high_contrast(false);
		subject.advance(core::pending_slow_milliseconds);
		expect(subject.Veil->look() == core::pending_look::refreshing_slow && !subject.Veil->plate_rect().isEmpty(), "a slow refresh says what it waits on, on a plate");
	}

	void test_partial_content_is_never_dimmed()
	{
		auto subject = pane{ };
		subject.Veil->present(core::pending_activity::loading, true, true, words());
		subject.advance(core::pending_reveal_milliseconds);
		expect(subject.Veil->look() == core::pending_look::partial && subject.Veil->shown_text() == words().Coming, "what is still coming is said");
		const auto plate = subject.Veil->plate_rect();
		expect(!plate.isEmpty() && plate.bottom() < subject.Veil->height() && plate.top() > subject.Veil->height() / 2, "on a plate at the pane's foot");
		expect(subject.Veil->testAttribute(Qt::WA_TransparentForMouseEvents), "partial content stays usable");
	}

	void test_a_failure_offers_retry()
	{
		auto subject = pane{ };
		auto retried = 0;
		subject.Veil->set_retry([&retried] { ++retried; });
		subject.Veil->present(core::pending_activity::failed, false, false, words());
		settle(20);
		expect(subject.Veil->look() == core::pending_look::failed && subject.Veil->retry_button()->isVisible(), "a failure in place of the content offers Retry");
		subject.Veil->retry_button()->click();
		expect(retried == 1, "Retry asks again");
		subject.Veil->present(core::pending_activity::failed, true, false, words());
		settle(20);
		expect(subject.Veil->look() == core::pending_look::failed_retained && subject.Veil->retry_button()->isVisible(), "a failed refresh keeps the content and offers Retry");
		expect(!subject.Veil->mask().isEmpty() && subject.Veil->mask().boundingRect() == subject.Veil->plate_rect(),
			   "only the failure strip takes the pointer");
	}

	void test_reduced_motion_is_still()
	{
		auto subject = pane{ };
		subject.Veil->set_full_motion(false);
		subject.Veil->present(core::pending_activity::loading, false, false, words());
		subject.advance(core::pending_reveal_milliseconds);
		const auto frame = subject.Veil->frame();
		settle(300);
		expect(subject.Veil->frame() == frame && subject.Veil->sweep() == 0.0, "under reduced motion nothing turns or sweeps");
		subject.Veil->set_full_motion(true);
		settle(300);
		expect(subject.Veil->frame() != frame || subject.Veil->sweep() != 0.0, "under full motion the mark turns");
	}

	void test_assistive_technology_sees_busy()
	{
		auto subject = pane{ };
		subject.Veil->present(core::pending_activity::loading, false, false, words());
		subject.advance(core::pending_reveal_milliseconds);
		auto* const accessible = QAccessible::queryAccessibleInterface(subject.Veil.get());
		expect(accessible != nullptr && accessible->state().busy, "the veil reports busy");
		expect(accessible->text(QAccessible::Name).contains(words().Loading), "and names what is loading");
		auto mark = qt::pending_mark{ };
		mark.set_turning(false);
		expect(mark.sizePolicy().retainSizeWhenHidden(), "a hidden mark keeps the words' place");
	}

	// The Overview's sections from opening to ready: what has arrived shows
	// while what has not says so.
	void test_overview_sections_arrive_in_turn()
	{
		auto page = qt::pull_request_overview_page{ };
		page.resize(1'100, 720);
		page.show();
		settle();
		const auto veils = page.pending_veils();
		page.present_loading();
		settle(20);
		expect(veils.Header->busy() && veils.Description->busy() && veils.Files->busy() && veils.Side->busy(), "opening, every section waits");
		page.present_observation(application::github_loading_overview_projection{
			.Title = "Keep review snapshots deterministic", .Body = "A description.", .AuthorLogin = "octo-author", .BaseRef = "main", .HeadRef = "topic", .ReportedFiles = 2U, .ReportedCommits = 1U });
		settle(20);
		expect(!veils.Header->busy() && !veils.Description->busy(), "the title and description show as soon as GitHub names them");
		expect(veils.Files->busy() && veils.Files->words().Loading.contains(QStringLiteral("2 files reported")), "the file list says how many files are coming");
		expect(veils.Side->busy(), "reviewers, checks and progress still wait");
		page.set_read_waiting(QStringLiteral("the changed files, 3 pages so far"));
		expect(veils.Files->words().Waiting == QStringLiteral("Still waiting for GitHub: the changed files, 3 pages so far."), "a slow wait names the read's step");
		page.present(application::pull_request_overview_projection{ .Owner = "octo", .RepositoryName = "repo", .Number = core::pull_request_number{ 7U }, .Title = "Ready" });
		settle(20);
		expect(!veils.Header->busy() && !veils.Files->busy() && !veils.Side->busy(), "the accepted review makes every section ready");
		page.present_loading();
		page.clear();
		settle(20);
		expect(!veils.Files->busy() && !veils.Side->busy(), "a cleared page waits on nothing");
	}

	// The window's own panes: a first read, then a refresh of the accepted review.
	void test_window_panes_follow_the_read()
	{
		auto window = qt::workbench_window{ };
		window.resize(1'280, 820);
		window.show();
		settle();
		const auto& panes = window.pending_veils();
		expect(panes.Content != nullptr && panes.Files != nullptr && panes.Conversations != nullptr && panes.HomeList != nullptr, "the window lays a veil over each waiting pane");
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::loading_pull_request } } });
		settle(20);
		expect(panes.Files->busy() && panes.Conversations->busy() && !panes.Content->busy(), "a first read: the drawers wait; there is no review to veil");
		window.publish(application::github_presentation_update{ .PullRequest = application::github_pull_request_projection{ .Owner = "octo", .RepositoryName = "repo", .Number = core::pull_request_number{ 7U }, .Title = "Seven" } });
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::pull_request_ready } } });
		settle(20);
		expect(!panes.Files->busy() && !panes.Content->busy(), "the accepted review is ready");
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::loading_pull_request } } });
		settle(20);
		expect(panes.Content->busy() && panes.Content->look() == core::pending_look::quiet, "a refresh keeps the review as it is before the reveal");
		expect(!panes.Files->busy(), "the drawers keep their content during a refresh");
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::pull_request_ready } } });
		settle(20);
		expect(!panes.Content->busy(), "a settled refresh lifts the veil");
		window.adopt_dif(core::resolve_dif({ }, core::motion_observation::reduced));
		expect(!window.pending_full_motion(), "the system's Reduce Motion stills every wait");
		window.adopt_dif(core::resolve_dif({ }, core::motion_observation::full));
		expect(window.pending_full_motion() && panes.Content->full_motion(), "full motion turns them");
	}

	void test_the_updates_line_turns_before_its_words()
	{
		auto control = qt::epoch_comparison_control{ { } };
		control.resize(900, 40);
		control.show();
		settle();
		control.set_readiness(QStringLiteral("Loading review history…"), true);
		control.set_readiness_busy(true, false);
		expect(control.readiness_mark()->turning(), "a wait on the Updates line shows the mark");
		control.set_readiness_busy(false, false);
		expect(!control.readiness_mark()->turning(), "and drops it when the wait ends");
	}

	void render(const QString& aDirectory)
	{
		if (aDirectory.isEmpty()) {
			return;
		}
		QDir{ }.mkpath(aDirectory);
		auto subject = pane{ };
		subject.Veil->present(core::pending_activity::loading, false, false, words());
		subject.Window.grab().save(aDirectory + QStringLiteral("/veil-placeholder.png"));
		subject.advance(core::pending_reveal_milliseconds);
		subject.Window.grab().save(aDirectory + QStringLiteral("/veil-loading.png"));
		subject.advance(core::pending_slow_milliseconds);
		subject.Window.grab().save(aDirectory + QStringLiteral("/veil-slow.png"));
		subject.Veil->present(core::pending_activity::loading, true, false, words());
		subject.advance(core::pending_reveal_milliseconds);
		subject.Window.grab().save(aDirectory + QStringLiteral("/veil-refreshing.png"));
		subject.Veil->present(core::pending_activity::loading, true, true, words());
		subject.advance(core::pending_reveal_milliseconds);
		subject.Window.grab().save(aDirectory + QStringLiteral("/veil-partial.png"));
		subject.Veil->set_retry([] { });
		subject.Veil->present(core::pending_activity::failed, false, false, words());
		settle(20);
		subject.Window.grab().save(aDirectory + QStringLiteral("/veil-failed.png"));
		subject.Veil->present(core::pending_activity::failed, true, false, words());
		settle(20);
		subject.Window.grab().save(aDirectory + QStringLiteral("/veil-failed-retained.png"));
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	const auto   arguments = QApplication::arguments();
	const auto   at        = arguments.indexOf(QStringLiteral("--render-directory"));
	test_a_first_load_is_quiet_then_said_then_slow();
	test_a_fast_load_never_flashes();
	test_a_refresh_veils_retained_content_and_blocks_nothing();
	test_partial_content_is_never_dimmed();
	test_a_failure_offers_retry();
	test_reduced_motion_is_still();
	test_assistive_technology_sees_busy();
	test_overview_sections_arrive_in_turn();
	test_window_panes_follow_the_read();
	test_the_updates_line_turns_before_its_words();
	render(at >= 0 ? arguments.value(at + 1) : QString{ });
	std::cout << "pending-veil-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
