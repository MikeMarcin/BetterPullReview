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
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
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

	void test_completion_waits_for_visibility_and_refuses_superseded_presentation()
	{
		auto subject       = pane{ };
		auto announcements = std::vector<QString>{ };
		Announcements      = &announcements;
		const auto before  = QAccessible::installUpdateHandler(capture);
		subject.Veil->present(core::pending_activity::loading, false, false, words());
		subject.advance(core::pending_reveal_milliseconds);
		announcements.clear();
		subject.Window.hide();
		subject.Veil->present_ready();
		expect(announcements.empty(), "completion waits for the presentation handoff");
		subject.Window.show();
		settle(20);
		expect(announcements.size() == 1U && announcements.front() == words().Done, "a pane shown in the same event turn announces completion once");

		subject.Veil->present(core::pending_activity::loading, false, false, words());
		subject.advance(2 * core::pending_reveal_milliseconds);
		announcements.clear();
		subject.Window.hide();
		subject.Veil->present_ready();
		subject.Veil->present(core::pending_activity::loading, false, false, words());
		subject.Window.show();
		settle(20);
		expect(announcements.empty(), "a new load suppresses the queued completion of the previous one");

		subject.advance(3 * core::pending_reveal_milliseconds);
		announcements.clear();
		subject.Window.hide();
		subject.Veil->present_ready();
		settle(20);
		expect(announcements.empty(), "completion of a pane that stays hidden is silent");
		subject.Window.show();
		settle(20);
		expect(announcements.empty(), "showing the pane later does not replay its completion");
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
		// [[BPR-2180]]. Conversations and Threads wait as thread cards, and the
		// status bar's line turns the mark while it says the read.
		expect(panes.Conversations->shape() == qt::pending_shape::cards && panes.Threads->shape() == qt::pending_shape::cards, "conversations and Threads wait as thread cards");
		expect(panes.Status != nullptr, "the status line has the pattern's mark");
		window.publish(application::github_presentation_update{ .PullRequest = application::github_pull_request_projection{ .Owner = "octo", .RepositoryName = "repo", .Number = core::pull_request_number{ 7U }, .Title = "Seven" } });
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::pull_request_ready } } });
		settle(20);
		expect(!panes.Files->busy() && !panes.Content->busy(), "the accepted review is ready");
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::loading_pull_request } } });
		settle(20);
		expect(panes.Content->busy() && panes.Content->look() == core::pending_look::quiet, "a refresh keeps the review as it is before the reveal");
		expect(!panes.Files->busy(), "the drawers keep their content during a refresh");
		auto* const status = window.findChild<QLabel*>(QStringLiteral("workbenchStatus"));
		expect(panes.Status->turning() && status->text().startsWith(QStringLiteral("Loading")), "the status line turns the mark while it says the read");
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::pull_request_ready } } });
		settle(20);
		expect(!panes.Content->busy(), "a settled refresh lifts the veil");
		expect(!panes.Status->turning() || status->text().contains(QStringLiteral("prepared")), "the mark stops with the read, unless the line says comparisons are being prepared");
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

	/// [[BPR-2180]]. Whether `aImage` holds anything but `aGround` along row
	/// `aY` between `aLeft` and `aRight`.
	[[nodiscard]] auto inked(const QImage& aImage, const int aY, const int aLeft, const int aRight, const QColor& aGround) -> bool
	{
		for (auto x = aLeft; x < aRight; ++x) {
			if (aImage.pixelColor(x, aY) != aGround) {
				return true;
			}
		}
		return false;
	}

	// [[BPR-2180]]. Thread cards and people: drawn in their own shape, counted,
	// and limited to the count; a pane whose heading says its wait draws its
	// placeholders in the words' place and says nothing over them.
	void test_cards_and_people_take_their_shape()
	{
		auto cards = pane{ qt::pending_shape::cards };
		cards.Veil->present(core::pending_activity::loading, false, false, words());
		settle(20);
		const auto all = cards.Veil->placeholders();
		expect(cards.Veil->shape() == qt::pending_shape::cards && all > 0 && all % 6 == 0, "cards draw a rail, face, name, age and two lines each");
		cards.Veil->set_placeholder_count(2);
		cards.Veil->repaint();
		expect(cards.Veil->placeholders() == 12 && all > 12, "a known count of cards is drawn, and no more");
		cards.advance(core::pending_reveal_milliseconds);
		expect(cards.Veil->shown_text() == words().Loading && cards.Veil->placeholders() == 12, "after the reveal the words stand above the cards, which stay");

		auto line = QWidget{ };
		line.resize(320, 26);
		auto people = qt::pending_veil{ line, qt::pending_shape::people, QStringLiteral("People") };
		line.show();
		people.present(core::pending_activity::loading, false, false, words());
		settle(20);
		expect(people.placeholders() == 4, "one line of people: three faces and the line beside them");
		auto roster = QWidget{ };
		roster.resize(320, 240);
		auto many = qt::pending_veil{ roster, qt::pending_shape::people, QStringLiteral("Participants") };
		roster.show();
		many.present(core::pending_activity::loading, false, false, words());
		settle(20);
		expect(many.placeholders() > 3 && many.placeholders() % 3 == 0, "a taller place is a roster: face, name and standing on each row");

		// A heading says the wait: the words' line holds placeholders instead.
		auto said   = pane{ };
		auto beside = pane{ };
		beside.Veil->set_words_beside(true);
		said.Veil->present(core::pending_activity::loading, false, false, words());
		beside.Veil->present(core::pending_activity::loading, false, false, words());
		said.advance(core::pending_reveal_milliseconds);
		beside.advance(core::pending_reveal_milliseconds);
		const auto text       = QFontMetrics{ beside.Veil->font() }.height();
		const auto top        = (qt::pending_veil::inset / 2) + ((((text * 3) / 2) + 4) / 2);
		const auto ground     = beside.Veil->grab().toImage().pixelColor(beside.Veil->width() - 2, beside.Veil->height() - 2);
		const auto wordsRight = qt::pending_veil::inset + QFontMetrics{ beside.Veil->font() }.height() + 12;
		expect(beside.Veil->shown_text() == words().Loading, "assistive technology still hears the wait");
		expect(beside.Veil->placeholders() > said.Veil->placeholders(), "the words' place holds rows instead");
		expect(inked(beside.Veil->grab().toImage(), top, qt::pending_veil::inset + 2, wordsRight + 40, ground), "a placeholder stands in the words' place");
		beside.Veil->set_retry([] { });
		beside.Veil->present(core::pending_activity::failed, false, false, words());
		settle(20);
		expect(beside.Veil->retry_button()->isVisible() && beside.Veil->shown_text() == words().Failure, "a failure is still said in place");
	}

	// [[BPR-2180]]. The Overview's side column waits section by section: each
	// section stands in its place under its heading, in its own shape, and
	// shows when its own part of the read arrives.
	void test_overview_side_sections_wait_on_their_own()
	{
		auto page = qt::pull_request_overview_page{ };
		page.resize(1'100, 720);
		page.show();
		settle();
		const auto veils = page.pending_veils();
		expect(veils.Merge != nullptr && veils.MyReview != nullptr && veils.Checks != nullptr && veils.People != nullptr && veils.Roster != nullptr, "every side section has its own veil");
		expect(veils.Merge->shape() == qt::pending_shape::pairs && veils.MyReview->shape() == qt::pending_shape::pairs && veils.Checks->shape() == qt::pending_shape::rows
				   && veils.People->shape() == qt::pending_shape::people,
			   "each in its own shape");
		page.present_loading();
		page.present_observation(application::github_loading_overview_projection{ .Title = "Seven", .AuthorLogin = "octo-author", .BaseRef = "main", .HeadRef = "topic", .ReportedFiles = 2U, .ReportedCommits = 1U });
		settle(20);
		for (auto* const veil : { veils.Merge, veils.MyReview, veils.Checks, veils.People }) {
			expect(veil->busy() && veil->isVisible() && veil->height() > 0, "every section waits in its own place");
		}
		expect(veils.Side->look() == core::pending_look::quiet || veils.Side->look() == core::pending_look::partial, "the column is never covered whole: its plate says what is coming");
		page.present_observed_checks(application::overview_checks_projection{ });
		settle(20);
		expect(!veils.Checks->busy() && veils.People->busy() && veils.Merge->busy() && veils.MyReview->busy(), "the checks show while the other sections still wait");
		page.present_observed_people(application::overview_people_projection{ });
		settle(20);
		expect(!veils.People->busy() && !veils.Roster->busy() && veils.Merge->busy(), "the people show on their own");
		page.present(application::pull_request_overview_projection{ .Owner = "octo", .RepositoryName = "repo", .Number = core::pull_request_number{ 7U }, .Title = "Ready" });
		settle(20);
		for (auto* const veil : { veils.Merge, veils.MyReview, veils.Checks, veils.People, veils.Side }) {
			expect(!veil->busy(), "the accepted review makes every section ready");
		}
	}

	// [[BPR-2180]]. The private tour waits as stop cards; Finish review holds
	// its pending comments' place.
	void test_tour_and_finish_review_wait_in_shape()
	{
		auto tour = qt::review_tour_editor{ };
		tour.resize(720, 560);
		tour.show();
		settle();
		auto* const tourVeil = qt::pending_veil::find(tour, QStringLiteral("tourPendingVeil"));
		expect(tourVeil != nullptr && tourVeil->shape() == qt::pending_shape::cards, "the tour has a veil of stop cards");
		tour.present(application::tour_presentation{ .Loading = true });
		settle(20);
		expect(tourVeil->busy() && tourVeil->words().Loading == QStringLiteral("Loading private tour…"), "a loading tour waits as cards");
		tour.present(application::tour_presentation{ });
		settle(20);
		expect(!tourVeil->busy(), "a loaded tour shows itself");

		auto       editor        = qt::review_submission_editor{ [](auto) { } };
		const auto head          = core::parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01");
		const auto draft         = core::review_submission_draft{ .Id            = core::review_submission_draft_id{ 1U },
																  .Target        = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = head.value() },
																  .ActingAccount = core::github_account_id{ 4U } };
		auto       controls      = core::controls(draft);
		controls.CommentsUnknown = true;
		editor.resize(900, 640);
		editor.show();
		editor.present(qt::review_submission_editor_projection{ .Draft = draft, .Controls = controls });
		settle(20);
		auto* const pending = editor.findChild<QWidget*>(QStringLiteral("wholeReviewPending"));
		auto* const veil    = pending != nullptr ? qt::pending_veil::find(*pending, QStringLiteral("wholeReviewPendingVeil")) : nullptr;
		expect(veil != nullptr && pending->isVisible() && pending->height() > QFontMetrics{ pending->font() }.height() * 4, "unread pending comments hold their place");
		expect(veil->busy() && veil->placeholders() == 3, "with three comment rows in it");
		editor.present(qt::review_submission_editor_projection{ .Draft = draft, .Controls = core::controls(draft) });
		settle(20);
		expect(!veil->busy() && !pending->isVisible(), "read, and none: the place is gone");
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
		// [[BPR-2180]]. Cards and people, in a light and a dark palette.
		for (const auto dark : { false, true }) {
			auto shades = QPalette{ };
			if (dark) {
				shades.setColor(QPalette::Window, QColor{ 0x1e, 0x1f, 0x22 });
				shades.setColor(QPalette::WindowText, QColor{ 0xe6, 0xe6, 0xe6 });
				shades.setColor(QPalette::Base, QColor{ 0x16, 0x17, 0x19 });
				shades.setColor(QPalette::Text, QColor{ 0xe6, 0xe6, 0xe6 });
				shades.setColor(QPalette::PlaceholderText, QColor{ 0x9a, 0x9a, 0x9a });
			}
			const auto theme = dark ? QStringLiteral("dark") : QStringLiteral("light");
			auto       cards = pane{ qt::pending_shape::cards };
			cards.Window.setPalette(shades);
			cards.Veil->present(core::pending_activity::loading, false, false, words());
			cards.advance(core::pending_reveal_milliseconds);
			cards.Window.grab().save(aDirectory + QStringLiteral("/veil-cards-%1.png").arg(theme));
			auto people = pane{ qt::pending_shape::people };
			people.Window.setPalette(shades);
			people.Veil->present(core::pending_activity::loading, false, false, words());
			people.advance(core::pending_reveal_milliseconds);
			people.Window.grab().save(aDirectory + QStringLiteral("/veil-people-%1.png").arg(theme));
		}
		// [[BPR-2180]]. The Overview's side column, each section waiting.
		{
			auto page = qt::pull_request_overview_page{ };
			page.resize(1'100, 720);
			page.show();
			settle();
			page.present_loading();
			page.present_observation(application::github_loading_overview_projection{ .Title = "Seven", .AuthorLogin = "octo-author", .BaseRef = "main", .HeadRef = "topic", .ReportedFiles = 2U, .ReportedCommits = 1U });
			settle(core::pending_reveal_milliseconds + 100);
			page.grab().save(aDirectory + QStringLiteral("/overview-side-sections.png"));
		}
		// [[BPR-2180]]. The private tour and Finish review as they wait: both
		// read local storage, which is usually done before the reveal.
		auto tour = qt::review_tour_editor{ };
		tour.resize(720, 560);
		tour.show();
		settle();
		tour.present(application::tour_presentation{ .Loading = true });
		settle(core::pending_reveal_milliseconds + 100);
		tour.grab().save(aDirectory + QStringLiteral("/tour-loading.png"));
		auto       editor        = qt::review_submission_editor{ [](auto) { } };
		const auto head          = core::parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01");
		const auto draft         = core::review_submission_draft{ .Id            = core::review_submission_draft_id{ 1U },
																  .Target        = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = head.value() },
																  .ActingAccount = core::github_account_id{ 4U } };
		auto       controls      = core::controls(draft);
		controls.CommentsUnknown = true;
		editor.resize(900, 640);
		editor.show();
		editor.present(qt::review_submission_editor_projection{ .Draft = draft, .Controls = controls });
		settle(core::pending_reveal_milliseconds + 100);
		editor.grab().save(aDirectory + QStringLiteral("/finish-review-loading.png"));
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	const auto   arguments = QApplication::arguments();
	const auto   at        = arguments.indexOf(QStringLiteral("--render-directory"));
	test_a_first_load_is_quiet_then_said_then_slow();
	test_a_fast_load_never_flashes();
	test_completion_waits_for_visibility_and_refuses_superseded_presentation();
	test_a_refresh_veils_retained_content_and_blocks_nothing();
	test_partial_content_is_never_dimmed();
	test_a_failure_offers_retry();
	test_reduced_motion_is_still();
	test_assistive_technology_sees_busy();
	test_overview_sections_arrive_in_turn();
	test_window_panes_follow_the_read();
	test_the_updates_line_turns_before_its_words();
	test_cards_and_people_take_their_shape();
	test_overview_side_sections_wait_on_their_own();
	test_tour_and_finish_review_wait_in_shape();
	render(at >= 0 ? arguments.value(at + 1) : QString{ });
	std::cout << "pending-veil-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
