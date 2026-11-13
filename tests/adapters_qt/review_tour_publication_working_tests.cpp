// [[BPR-1782]]. Publish tour reads as working from the press until GitHub
// answers, as Compose review's Publish does. [[BPR-1991]], [[BPR-1992]]:
// RFC-BPR-0099. The Tour tab is three bands (head, route, agent bar) with only
// the route scrolling; the head says where the tour stands with its one action;
// the panel states the chosen form, advances a checklist as GitHub confirms
// each step, and says a refusal or failure in an error-role banner.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
namespace {
	using namespace bpr;
	using phase  = application::tour_publication_phase;
	using step   = application::tour_step_state;
	using state  = core::tour_head_state;
	using action = core::tour_head_action;
	void expect(bool aPass, const char* aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	template <typename Widget>
	auto child(QWidget& aParent, const char* aName) -> Widget*
	{
		auto* const value = aParent.findChild<Widget*>(QString::fromUtf8(aName));
		expect(value != nullptr, aName);
		return value;
	}
	void save(QWidget& aWidget, const std::string& aPrefix, const char* aSuffix)
	{
		if (aPrefix.empty()) {
			return;
		}
		QApplication::processEvents();
		expect(aWidget.grab().save(QString::fromStdString(aPrefix + aSuffix)), "render saved");
	}
	auto ready_stop(const std::uint64_t aId, std::string aTitle) -> core::tour_stop
	{
		auto value        = tests::review_tour_fixture().Stops.front();
		value.Id          = core::tour_stop_id{ aId };
		value.Title       = std::move(aTitle);
		value.Body        = "Keeps the snapshot. The rest follows.";
		value.Preparation = core::tour_preparation::ready;
		// [[BPR-2014]]. A stop ready to publish is accepted.
		value.Acceptance = core::accept_tour_stop(tests::review_tour_fixture(), value, { });
		return value;
	}
	auto steps(std::initializer_list<std::pair<const char*, step>> aRows) -> std::vector<application::tour_publication_step_view>
	{
		auto result = std::vector<application::tour_publication_step_view>{ };
		for (const auto& [label, at] : aRows) {
			result.push_back({ .Label = label, .State = at });
		}
		return result;
	}

	/// AC01. With a route long enough to scroll, the head and the agent bar
	/// stay at the top and bottom while only the route scrolls.
	void test_bands(const std::string& aPrefix)
	{
		auto draft = tests::review_tour_fixture();
		draft.Stops.clear();
		for (std::uint64_t id = 1U; id <= 24U; ++id) {
			draft.Stops.push_back(ready_stop(id, "Stop number " + std::to_string(id)));
		}
		draft.LastStopId = 24U;
		qt::review_tour_editor editor;
		editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
		editor.resize(380, 620);
		editor.show();
		editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
		editor.open_step(core::tour_step::stops);
		QApplication::processEvents();
		auto* const head   = editor.head_bar();
		auto* const scroll = editor.route_scroll();
		const auto  top    = [&editor](const QWidget* aWidget) { return aWidget->mapTo(&editor, QPoint{ 0, 0 }).y(); };
		expect(scroll->verticalScrollBar()->maximum() > 0, "AC01: the route is long enough to scroll");
		expect(head->isVisible() && top(head) == 0 && !scroll->isAncestorOf(head), "AC01: the head is the tab's top band, outside the scrolling route");
		// [[BPR-2051]]: no agent bar; the route runs to the tab's bottom.
		expect(editor.findChild<QWidget*>(QStringLiteral("tourAgentBar")) == nullptr, "the agent bar left for the window's footer");
		auto* const publish = head->button(action::publish);
		expect(publish != nullptr && publish->isVisible(), "AC01: Publish… is in the head");
		const auto headAt = top(head);
		scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
		QApplication::processEvents();
		expect(top(head) == headAt && head->isVisible() && publish->isVisible(), "AC01: scrolled to the end, the head stays where it was");
		expect(editor.findChild<QPushButton*>(QStringLiteral("tourPreviewAndPublish")) == nullptr, "the bottom Preview and publish is gone");
		save(editor, aPrefix, "-bands.png");
	}

	/// AC02. The head shows each state of the table, and its action.
	void test_head_states()
	{
		qt::review_tour_editor editor;
		auto                   triggered = std::vector<qt::tour_editor_action>{ };
		auto                   expanded  = 0;
		editor.set_dispatch([](core::tour_command) { }, [&](const qt::tour_editor_action aAction) { triggered.push_back(aAction); });
		editor.set_expand_dispatch([&](const bool aPublication) { expanded += aPublication ? 1 : 0; });
		editor.resize(420, 640);
		editor.show();
		auto draft = tests::review_tour_fixture();
		// A stop not accepted is still to accept.
		draft.Stops.front().Acceptance.reset();
		draft.Stops.push_back(ready_stop(2U, "Second"));
		draft.LastStopId = 2U;
		editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
		QApplication::processEvents();
		auto* const head = editor.head_bar();
		const auto  text = [head] { return head->state_label()->accessibleName(); };
		// The fixture's first stop is not ready to publish: one to accept.
		if (text() != QStringLiteral("Private draft, 2 stops, 1 to accept")) {
			std::cerr << text().toStdString() << '\n';
		}
		expect(editor.head().State == state::not_publishable && text() == QStringLiteral("Private draft, 2 stops, 1 to accept"), "not publishable: the stop count and how many to accept");
		expect(head->button(action::publish) != nullptr && !head->button(action::publish)->isEnabled() && head->button(action::publish)->toolTip().contains(QStringLiteral("Accept")), "not publishable: Publish… disabled, its tooltip naming what is missing");
		draft.Stops.front() = ready_stop(1U, "First");
		++draft.Revision;
		editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
		QApplication::processEvents();
		expect(editor.head().State == state::publishable && text() == QStringLiteral("Private draft, 2 stops, all accepted") && head->button(action::publish)->isEnabled(), "publishable: all accepted, Publish… enabled");
		head->button(action::publish)->click();
		expect(expanded == 1 && !triggered.empty() && triggered.back() == qt::tour_editor_action::preview_publication, "Publish… opens the tour page and computes its preview");
		editor.present_publication({ .Status = "Stop keeps what GitHub already has.", .Phase = phase::publishing, .Steps = steps({ { "Stops 1 and 2, in the pending review", step::sent }, { "Links for stop 1", step::sending }, { "Links for stop 2", step::waiting }, { "Submit the review as a comment", step::waiting } }), .Step = 2U, .Total = 4U, .Stop = true });
		QApplication::processEvents();
		expect(editor.head().State == state::publishing && text() == QStringLiteral("Publishing, step 2 of 4") && head->progress()->isVisible() && head->progress()->maximum() == 4 && head->progress()->value() == 1, "publishing: step N of M with a determinate bar");
		expect(head->shown_actions() == std::vector{ action::details }, "publishing: Details");
		editor.present_publication({ .Phase = phase::stopped, .Steps = steps({ { "Stops 1 and 2, in the pending review", step::sent }, { "Links for stop 1", step::failed } }), .Step = 2U, .Total = 4U, .Resume = true, .Reconcile = true });
		QApplication::processEvents();
		expect(editor.head().State == state::stopped && text() == QStringLiteral("Publishing stopped, at step 2 of 4") && head->shown_actions() == std::vector{ action::resume, action::details }, "stopped: step N of M, Resume and Details");
		expect(head->state_label()->palette().color(QPalette::WindowText) == qt::tour_error_colour(head->palette()), "stopped: in the error role");
		head->button(action::resume)->click();
		expect(triggered.back() == qt::tour_editor_action::resume_publication, "stopped: Resume resumes");
		editor.present_publication({ });
		draft.Remote = core::tour_remote_binding{ .Entity = "PRR_1", .WebUrl = "https://github.com/o/r/pull/5#pullrequestreview-1", .Own = true };
		for (auto& stop : draft.Stops) {
			stop.Remote   = core::tour_remote_binding{ .Kind = core::tour_remote_kind::line_comment, .Entity = "C" + std::to_string(stop.Id.value()), .WebUrl = "https://github.com/o/r/pull/5#discussion_r" + std::to_string(stop.Id.value()), .Own = true };
			stop.Baseline = core::tour_field_baseline{ .Raw = stop.Body, .DraftText = stop.Body };
		}
		++draft.Revision;
		editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
		QApplication::processEvents();
		expect(editor.head().State == state::published && text() == QStringLiteral("Published") && head->shown_actions() == std::vector{ action::on_github }, "published, unchanged: On GitHub");
		head->button(action::on_github)->click();
		expect(triggered.back() == qt::tour_editor_action::open_published, "On GitHub opens the published tour");
		draft.Stops.back().Body = "Keeps the snapshot. Edited since.";
		++draft.Revision;
		editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
		QApplication::processEvents();
		expect(editor.head().State == state::published_changed && text() == QStringLiteral("Published, 1 change not on GitHub") && head->shown_actions() == std::vector{ action::update, action::on_github }, "published, changed: the count, Update on GitHub… and On GitHub");
	}

	/// AC04, AC05 and AC06 as the panel draws them; the projection's own
	/// rules are the application's tests.
	void test_panel(const std::string& aPrefix)
	{
		qt::review_tour_editor editor;
		auto                   triggered = std::vector<qt::tour_editor_action>{ };
		editor.set_dispatch([](core::tour_command) { }, [&](const qt::tour_editor_action aAction) { triggered.push_back(aAction); });
		editor.resize(1180, 760);
		editor.set_mode(qt::tour_editor_mode::page);
		editor.present({ .Draft = std::make_shared<const core::review_tour>(tests::review_tour_fixture()) });
		editor.show();
		QApplication::processEvents();

		auto* const publish  = child<QPushButton>(editor, "tourPublish");
		auto* const preview  = child<QPushButton>(editor, "tourPreparePublication");
		auto* const stop     = child<QPushButton>(editor, "tourStop");
		auto* const resume   = child<QPushButton>(editor, "tourResume");
		auto* const check    = child<QPushButton>(editor, "tourReconcile");
		auto* const describe = child<QCheckBox>(editor, "tourDescriptionEdit");
		auto* const status   = child<QLabel>(editor, "tourPublicationStatus");
		auto* const panel    = child<QWidget>(editor, "tourPublicationWorking");
		auto* const title    = child<QLabel>(editor, "tourPublicationWorkingTitle");
		auto* const link     = child<QLabel>(editor, "tourPublicationLink");
		auto* const form     = child<QLabel>(editor, "tourPublicationForm");
		auto* const keep     = child<QPushButton>(editor, "tourKeepOneReview");
		auto* const keepList = child<QLabel>(editor, "tourKeepOneReviewList");
		auto* const count    = child<QLabel>(editor, "tourPublicationStepCount");
		auto* const banner   = editor.publication_banner();
		auto* const list     = editor.publication_steps();
		expect(editor.findChild<QComboBox*>(QStringLiteral("tourPublicationMode")) == nullptr, "AC03: no mode list is offered");

		// [[BPR-1787]]. The brief goes in the description unless the author says otherwise.
		expect(describe->isChecked() && describe->toolTip().contains(QStringLiteral("Problem and Solution go in the PR description")), "Update PR description starts on and says where the brief goes");

		// Admitted: the chosen form in one sentence, with the stops that forced it.
		editor.present_publication({ .Status        = "Preview ready.",
									 .Markdown      = "## Tour\n\nWhat GitHub will show.",
									 .Steps         = steps({ { "Stops 1 to 3, in the pending review", step::waiting }, { "Stop 4, “Dirty”, as a file comment", step::waiting }, { "Links for stop 1", step::waiting } }),
									 .Total         = 3U,
									 .Form          = "Publishes in 3 steps. Three stops go in one review. Stop 4 sits outside the lines GitHub shows in this diff, so it is sent as a file comment.",
									 .KeepOneReview = { { core::tour_stop_id{ 4U }, "Stop 4, “Dirty”" } },
									 .Publish       = true,
									 .Reconcile     = true });
		QApplication::processEvents();
		expect(publish->text() == QStringLiteral("Publish tour") && publish->isEnabled(), "admitted: Publish tour can be pressed");
		expect(form->isVisible() && form->text().contains(QStringLiteral("Stop 4 sits outside")), "AC03: the panel's sentence names the stop that forced the form");
		expect(keep->isVisible() && !keepList->isVisible(), "a mixed form offers Keep it to one review instead");
		keep->click();
		QApplication::processEvents();
		expect(keepList->isVisible() && keepList->text().contains(QStringLiteral("Stop 4")), "Keep it to one review lists the stops to move, changing nothing");
		expect(list->isVisible() && list->row_texts().size() == 3 && list->row_texts().front().startsWith(QStringLiteral("Waiting:")) && !count->isVisible(), "admitted: the steps wait");
		expect(panel->isHidden() && banner->isHidden() && status->text() == QStringLiteral("Preview ready."), "admitted: no banner; the status line says where it stands");
		save(editor, aPrefix, "-preview.png");

		// AC04. In flight: waiting, sending, sent, in order; the head's count is the panel's.
		for (std::size_t sent = 0U; sent <= 2U; ++sent) {
			auto rows = std::vector<application::tour_publication_step_view>{ };
			for (std::size_t i = 0U; i < 3U; ++i) {
				rows.push_back({ .Label = std::array{ "Stops 1 to 3, in the pending review", "Stop 4, \u201cDirty\u201d, as a file comment", "Links for stop 1" }[i], .State = i < sent ? step::sent : i == sent ? step::sending
																																																				 : step::waiting });
			}
			editor.present_publication({ .Status = "Stop keeps what GitHub already has. Nothing will be sent twice.", .Phase = phase::publishing, .Steps = rows, .Step = sent + 1U, .Total = 3U, .Stop = true });
			QApplication::processEvents();
			expect(list->row_texts().size() == 3 && list->row_texts()[static_cast<qsizetype>(sent)].startsWith(QStringLiteral("Sending:")), "AC04: the current step is sending");
			expect(sent == 0U || list->row_texts()[static_cast<qsizetype>(sent - 1U)].startsWith(QStringLiteral("Sent:")), "AC04: the steps before it are sent");
			expect(count->text() == QStringLiteral("Step %1 of 3").arg(sent + 1U) && editor.head().Step == sent + 1U && editor.head().Steps == 3U, "AC04: the head's count agrees with the panel's");
		}
		expect(publish->text() == QStringLiteral("Publishing…") && !publish->isEnabled() && !preview->isEnabled() && !describe->isEnabled(), "in flight: nothing that would change what is sent can be used");
		expect(stop->isEnabled() && !resume->isEnabled() && !check->isEnabled(), "in flight: Stop stays available");
		expect(!panel->isHidden() && status->isHidden() && title->text() == QStringLiteral("Publishing the tour to GitHub…") && !form->isVisible(), "in flight: the waiting panel says it; the status line steps aside");
		save(editor, aPrefix, "-in-flight.png");

		// AC05. A step fails after earlier steps were sent: the banner, Resume primary.
		editor.present_publication({ .Status    = "Partially published.",
									 .Phase     = phase::stopped,
									 .Steps     = { { .Label = "Stops 1 to 3, in the pending review", .State = step::sent }, { .Label = "Stop 4, “Dirty”, as a file comment", .State = step::failed, .Reason = "Refused by GitHub (422)" }, { .Label = "Links for stop 1", .State = step::not_sent } },
									 .Step      = 2U,
									 .Total     = 3U,
									 .Banner    = application::tour_publication_banner{ .Title = "Publishing stopped at step 2 of 3", .Why = "Step 2, Stop 4, “Dirty”, as a file comment, was refused by GitHub (HTTP 422).", .OnGitHub = "One step is on GitHub and stays there. 2 steps are not sent.", .Actions = { application::tour_publication_remedy::resume, application::tour_publication_remedy::check_status } },
									 .Resume    = true,
									 .Reconcile = true });
		QApplication::processEvents();
		expect(banner->isVisible() && banner->title()->text() == QStringLiteral("Publishing stopped at step 2 of 3") && banner->why()->text().contains(QStringLiteral("HTTP 422")) && banner->on_github()->text().contains(QStringLiteral("not sent")), "AC05: the banner names the step, the reason, and what is and is not on GitHub");
		const auto buttons = banner->buttons();
		expect(buttons.size() == 2U && buttons.front()->text() == QStringLiteral("Resume") && buttons.front()->metaObject()->className() == QStringLiteral("QPushButton"), "AC05: Resume is primary");
		expect(status->isHidden() && panel->isHidden(), "AC05: never a status line");
		expect(list->row_texts()[1] == QStringLiteral("Failed: Stop 4, “Dirty”, as a file comment. Refused by GitHub (422)"), "AC05: the failed step carries the provider's short reason");
		const auto placeholder = banner->palette().color(QPalette::PlaceholderText);
		for (auto* const label : { banner->title(), banner->why(), banner->on_github() }) {
			expect(label->foregroundRole() != QPalette::PlaceholderText && label->palette().color(label->foregroundRole()) != placeholder, "AC05: nothing in the failure is in the placeholder colour");
		}
		buttons.front()->click();
		expect(triggered.back() == qt::tour_editor_action::resume_publication, "AC05: the banner's Resume resumes");
		expect(editor.head().State == state::stopped, "AC05: the head says it stopped too");
		save(editor, aPrefix, "-failed.png");

		// AC06. Refused before any write: the stops, the operation, both ways out.
		editor.present_publication({ .Status = "Not published.", .Phase = phase::attention, .Banner = application::tour_publication_banner{ .Title = "This connection cannot publish this tour", .Why = "Stops 4 and 8 need file comments, and this connection cannot create them.", .OnGitHub = "Nothing was sent. Seven of nine stops can publish as they are.", .Actions = { application::tour_publication_remedy::move_stops, application::tour_publication_remedy::other_connection }, .MoveTo = core::tour_stop_id{ 1U } } });
		QApplication::processEvents();
		const auto refused = banner->buttons();
		expect(banner->isVisible() && banner->why()->text().contains(QStringLiteral("Stops 4 and 8")) && banner->why()->text().contains(QStringLiteral("file comments")) && banner->on_github()->text().startsWith(QStringLiteral("Nothing was sent")), "AC06: the refusal names the stops and the operation");
		expect(refused.size() == 2U && refused[0]->text() == QStringLiteral("Move those stops") && refused[1]->text() == QStringLiteral("Use another connection"), "AC06: both ways out");
		refused[1]->click();
		expect(triggered.back() == qt::tour_editor_action::choose_connection, "AC06: Use another connection opens the connection controls");
		save(editor, aPrefix, "-refused.png");

		// Success: published, with where GitHub shows it.
		editor.present_publication({ .Status = "Published: one COMMENT review with verified tour links.", .Markdown = "## Tour\n\nObserved.", .Phase = phase::published, .Link = "https://github.com/octo-org/review-repository/pull/17#pullrequestreview-501", .Reconcile = true });
		QApplication::processEvents();
		expect(publish->text() == QStringLiteral("Published") && !publish->isEnabled() && banner->isHidden(), "success: Publish says Published and cannot repost");
		expect(!link->isHidden() && link->text() == QStringLiteral("On GitHub: https://github.com/octo-org/review-repository/pull/17#pullrequestreview-501"), "success: the way to see it");
	}
}

namespace {
	/// The owner, accepting the local draft's publish steps on 2026-11-13: "I'm
	/// not sure if those circles are meant to be misaligned vertically with the
	/// text beside them." Each row's mark is centred on its label's first line,
	/// in a column as wide as the Status tab's and in one narrow enough to wrap,
	/// and when the page gives the list more height than its rows need (the
	/// Status tab's page did: the rows grew and their words sank).
	void test_step_marks_centred(const std::string& aPrefix)
	{
		for (const auto& [width, roomy] : { std::pair{ 420, false }, std::pair{ 150, false }, std::pair{ 420, true } }) {
			auto host   = QWidget{ };
			auto column = QVBoxLayout{ &host };
			auto list   = qt::tour_step_list{ &host };
			column.addWidget(&list);
			if (!roomy) {
				column.addStretch(1);
			}
			list.present(steps({ { "Push feature/search-results", step::waiting }, { "Create the draft pull request", step::sending }, { "Publish the tour (1 to accept)", step::failed }, { "Link it to this draft", step::sent } }));
			host.resize(width, 400);
			host.show();
			for (int round = 0; round < 4; ++round) {
				QApplication::processEvents();
			}
			const auto rows = list.findChildren<QWidget*>(QStringLiteral("tourPublicationStep"));
			expect(rows.size() == 4, "four step rows");
			for (auto* const row : rows) {
				auto* const mark  = row->findChild<QWidget*>(QStringLiteral("tourPublicationStepMark"));
				auto* const label = row->findChild<QLabel*>(QStringLiteral("tourPublicationStepLabel"));
				expect(mark != nullptr && label != nullptr, "each row has its mark and its label");
				const auto markCentre = mark->mapTo(row, QPointF{ 0.0, mark->height() / 2.0 }).y();
				// Where the label draws its words: a label taller than its
				// text places the text by its vertical alignment.
				const auto box        = label->contentsRect();
				const auto text       = std::min(box.height(), label->heightForWidth(label->width()));
				const auto vertical   = label->alignment() & Qt::AlignVertical_Mask;
				const auto top        = vertical == Qt::AlignTop ? 0.0 : vertical == Qt::AlignBottom ? double(box.height() - text) : (box.height() - text) / 2.0;
				const auto lineCentre = label->mapTo(row, QPointF{ 0.0, box.top() + top + (label->fontMetrics().height() / 2.0) }).y();
				if (std::abs(markCentre - lineCentre) > 1.0) {
					std::cerr << "row " << label->text().toStdString() << " at width " << width << ": mark centre " << markCentre << ", first line centre " << lineCentre << '\n';
				}
				expect(std::abs(markCentre - lineCentre) <= 1.0, "the step's mark is centred on its label's first line");
			}
			if (width == 420 && !roomy) {
				// Every label fits on one line here, so every row is one line tall.
				for (auto* const row : rows) {
					expect(row->height() == rows.back()->height(), "one-line steps are rows of one height");
				}
			}
			save(host, aPrefix, roomy ? "step-marks-roomy.png" : width == 420 ? "step-marks.png" : "step-marks-narrow.png");
		}
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };
	test_bands(prefix);
	test_head_states();
	test_panel(prefix);
	test_step_marks_centred(prefix);
	std::cout << "review-tour-publication-working:pass\n";
	return EXIT_SUCCESS;
}
