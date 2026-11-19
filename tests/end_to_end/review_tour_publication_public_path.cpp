#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/diagnostic_files.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "cli_publication_course.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include "bpr/core/operation_diagnosis.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "bpr/core/review_tour_update.hpp"
#include "bpr/core/tour_publication_head.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QInputMethodEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;
	template <typename Predicate>
	void wait(Predicate aPredicate, const char* aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::default_queue_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}
	void open(app::github_application& aApp, e2e::cli_publication_course& aCli)
	{
		aApp.show();
		auto& window = aApp.window();
		aCli.connect(window);
		wait([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "connected controls");
		window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
		window.home_open_action()->trigger();
		wait([&] { return window.prepare_tour_action()->isEnabled() && !window.tour_editor()->shown().Loading; }, "ordinary PR open composes tour host");
		window.prepare_tour_action()->trigger();
		QApplication::processEvents();
		expect(window.tray_tabs()->accessibleTabName(window.tray_tabs()->currentIndex()) == QStringLiteral("Tour") && window.tour_editor()->isVisible(), "Prepare review tour opens the composed Tour tray");
	}
	void shutdown(app::github_application& aApp)
	{
		aApp.begin_shutdown();
		wait([&] { return aApp.status().ShutdownComplete; }, "tour saves drain during application shutdown");
	}
	void saved(qt::review_tour_editor& aEditor)
	{
		wait([&] { const auto& p = aEditor.shown(); return p.Draft && !p.Saving && p.SavedRevision == p.Draft->Revision; }, "tour saved through admitted SQLite store");
	}
	/// [[BPR-2014]], RFC-BPR-0106. Publication requires every stop Accepted:
	/// before each preview the author accepts what the course wrote, as the
	/// stop's Accept does (without moving the code: the courses' windows stay
	/// where they are).
	void accept_every_stop(qt::review_tour_editor& aEditor)
	{
		const auto draft = aEditor.shown().Draft;
		if (!draft || draft->ReadOnly) {
			return;
		}
		for (const auto& stop : draft->Stops) {
			const auto now   = aEditor.shown().Draft;
			const auto found = std::ranges::find(now->Stops, stop.Id, &core::tour_stop::Id);
			if (found == now->Stops.end() || core::tour_stop_accepted(*now, *found) || core::tour_stop_accept_need(*now, *found) != core::tour_accept_need::nothing) {
				continue;
			}
			aEditor.accept_stop(stop.Id);
			saved(aEditor);
		}
	}
	void select_file(qt::workbench_window& aWindow, const std::string& aPath)
	{
		auto*       files = aWindow.files_drawer()->contents_view();
		QModelIndex selected;
		wait([&] {for(int row=0;row<files->model()->rowCount();++row){const auto index=files->model()->index(row,0);if(index.data(Qt::DisplayRole).toString().toStdString()==aPath){selected=index;return true;}}return false; }, "exact fixture file is available");
		files->selectionModel()->setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait([&] {auto* surface=aWindow.comparison_surface();return surface->current_result() && surface->current_context() && surface->current_context()->Key.File==core::file_identity_of(aPath) && surface->current_semantic_index() && !surface->geometry_pending(); }, "installed Git/cache produces exact selected file");
	}
	void select_range(qt::workbench_window& aWindow, core::diff_side aSide, int aFirst, int aLast)
	{
		auto* surface = aWindow.comparison_surface();
		if (!surface->select_conversation_line(aSide, static_cast<std::uint32_t>(aFirst))) {
			std::cerr << "source-range first=" << aFirst << " last=" << aLast << " left-blocks=" << surface->left_pane()->blockCount() << " right-blocks=" << surface->right_pane()->blockCount() << "\n";
			throw std::runtime_error{ "source line exists" };
		}
		auto* pane = aSide == core::diff_side::left ? surface->left_pane() : surface->right_pane();
		pane->setFocus();
		auto cursor = pane->textCursor();
		cursor.setPosition(pane->document()->findBlockByNumber(aFirst).position());
		cursor.setPosition(pane->document()->findBlockByNumber(aLast).position() + 1, QTextCursor::KeepAnchor);
		pane->setTextCursor(cursor);
		expect(surface->selected_source_range() && surface->selected_source_range()->first.Line == static_cast<std::uint32_t>(aFirst) && surface->selected_source_range()->second.Line == static_cast<std::uint32_t>(aLast), "native text selection is an actual inclusive source range");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto                  cliMode = argc > 1 && std::string_view{ argv[1] } == "--cli";
	e2e::cli_publication_course cli{ cliMode };
	const std::string           mode = cliMode ? (argc > 2 ? argv[2] : "native") : (argc > 1 ? argv[1] : "native");
	QTemporaryDir               data;
	const auto                  dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		return EXIT_FAILURE;
	}
	QTemporaryDir coldData;
	auto          currentDataRoot      = dataRoot;
	auto          server               = std::make_shared<app::detail::review_submission_fixture_state>();
	server->CliStrictCourse            = cliMode;
	auto              make             = [&] {std::unique_ptr<app::github_application> started;bool terminal=false;auto options=app::detail::github_public_path_fixture_options{.ApplicationDataRoot=currentDataRoot,.ReviewSubmissionServer=server,.RepeatReadSessions=true};options.ViewerAuthors=mode=="compat";options.TourDestinationRepairsCourse=mode=="repairs" || mode=="mixed" || mode=="file" || mode=="compat";options.TourPushCourse=mode=="push";if(mode=="push"){options.EpochPhase=app::detail::github_epoch_fixture_phase::seed;options.RepeatReadSessions=false;}cli.configure(options);auto startup=access::start(app::detail::github_public_path_fixture::profile_window_restoration_v1,options,[&](auto aResult){terminal=true;if(aResult){started=std::move(*aResult);
}});wait([&]{return terminal && startup && startup->is_quiescent();},"publication app starts");expect(started!=nullptr,"publication startup accepted");return started; };
	const std::string originalTemplate = "# Existing template\r\n\r\nKeep [the ticket](https://example.invalid/123) and this checklist.\r\n- [ ] Original check\r\n";
	if (mode != "description") {
		server->StagedDescription = originalTemplate;
	}
	server->DraftPullRequest    = mode == "manual";
	server->StagedQualified     = mode != "unsupported";
	server->StagedFileQualified = mode == "file";
	auto application            = make();
	try {
		if (mode == "compat") {
			// [[BPR-1992]]. The plan chooses compatibility comments only for a
			// tour no review can carry: both stops point at an earlier head, so
			// one review cannot hold them and a mixed tour has no review stop.
			open(*application, cli);
			{
				auto& seedWindow = application->window();
				auto& seedEditor = *seedWindow.tour_editor();
				wait([&] { return seedEditor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().contains(QStringLiteral("Saved locally")) && seedEditor.shown().Draft != nullptr; }, "compatibility seed draft opens");
				select_file(seedWindow, "review.cpp");
				select_range(seedWindow, core::diff_side::right, 2, 4);
				seedWindow.add_tour_stop_action()->trigger();
				seedEditor.stop_title()->setPlainText(QStringLiteral("Capture the original"));
				seedEditor.stop_body()->setPlainText(QStringLiteral("Retain this exact source interval."));
				select_file(seedWindow, "secondary.cpp");
				select_range(seedWindow, core::diff_side::right, 1, 3);
				seedWindow.add_tour_stop_action()->trigger();
				seedEditor.stop_title()->setPlainText(QStringLiteral("Use the retained source"));
				seedEditor.stop_body()->setPlainText(QStringLiteral("Connect the explanation to the original decision."));
				saved(seedEditor);
				auto       fixtureDraft = *seedEditor.shown().Draft;
				const auto expected     = fixtureDraft.Revision;
				for (std::size_t index = 0U; index < fixtureDraft.Stops.size(); ++index) {
					auto& source      = *fixtureDraft.Stops[index].Original;
					source            = *fixtureDraft.Stops.front().Original;
					source.Commit     = *core::parse_git_commit_oid("d7d2f5c3e8762bbddc4e9e3444aa21a2fa74c3b7");
					source.First.Line = index == 0U ? 4U : 5U;
					source.Last.Line  = 5U;
					// Its explanation stands at the head the pull request is read at.
					fixtureDraft.Stops[index].ReaffirmedHead = fixtureDraft.ObservedHead.value_or(fixtureDraft.Head);
					// [[BPR-2014]]. And the author accepted it there.
					fixtureDraft.Stops[index].Acceptance = core::accept_tour_stop(fixtureDraft, fixtureDraft.Stops[index], { });
				}
				++fixtureDraft.Revision;
				shutdown(*application);
				application.reset();
				auto store = adapters::sqlite::make_review_tour_store(*dataRoot, fixtureDraft.Owner.Profile);
				expect(store->save(fixtureDraft, expected, { }).has_value(), "compatibility fixture saves through ordinary store");
			}
			application = make();
		}
		open(*application, cli);
		auto&      window = application->window();
		auto&      editor = *window.tour_editor();
		const auto button = [&](const char* aName) {auto* value=editor.findChild<QPushButton*>(QString::fromUtf8(aName));expect(value!=nullptr,"normal publication control exists");return value; };
		auto*      status = editor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"));
		wait([&] { return status->text().contains(QStringLiteral("Saved locally")); }, "read-only recovery discovery completes");
		// [[BPR-2117]], RFC-BPR-0106 AC04. Three stops accepted at the first
		// head; a push changes one stop's own lines and the lines a second
		// stop links to. Through the window, exactly those two turn Changed
		// since accepted, each with its reason, the third stays Accepted, and
		// Accept at the new head turns the first green.
		if (mode == "push") {
			const auto add = [&](const char* aFile, const int aFirst, const int aLast, const char* aTitle, const char* aBody) {
				select_file(window, aFile);
				select_range(window, core::diff_side::right, aFirst, aLast);
				window.add_tour_stop_action()->trigger();
				editor.stop_title()->setPlainText(QString::fromUtf8(aTitle));
				editor.stop_body()->setPlainText(QString::fromUtf8(aBody));
				saved(editor);
			};
			add("review.cpp", 1, 2, "Its own lines change", "The second line is pushed again.");
			add("review.cpp", 3, 4, "It links to lines that change", "These lines read `secondary.cpp:5-6`.");
			add("secondary.cpp", 0, 1, "Nothing of it changes", "Neither these lines nor `review.cpp:6` change.");
			const auto first = *editor.shown().Draft;
			expect(first.Stops.size() == 3U, "three stops");
			for (const auto& stop : first.Stops) {
				const auto& range = stop.Original;
				std::cout << "tour-push-stop: " << stop.Title << " " << (range ? range->Path + ":" + std::to_string(range->First.Line + 1U) + "-" + std::to_string(range->Last.Line + 1U) : std::string{ "none" }) << '\n';
			}
			expect(first.Stops[0].Original && first.Stops[0].Original->Path == "review.cpp" && first.Stops[0].Original->First.Line == 1U && first.Stops[0].Original->Last.Line == 2U, "the first stop is review.cpp lines 2 to 3");
			expect(first.Stops[1].Original && first.Stops[1].Original->First.Line == 3U && first.Stops[2].Original && first.Stops[2].Original->Path == "secondary.cpp" && first.Stops[2].Original->Last.Line == 1U, "the other stops are review.cpp 4 to 5 and secondary.cpp 1 to 2");
			accept_every_stop(editor);
			const auto decided = [&](const std::size_t aIndex) {
				const auto& draft = *editor.shown().Draft;
				return core::decide_tour_stop_acceptance(draft, draft.Stops[aIndex]);
			};
			for (std::size_t i = 0U; i < 3U; ++i) {
				expect(decided(i).State == core::tour_acceptance_state::accepted, "each stop is accepted at the first head");
			}
			const auto spoken = [&](const int aRow) { return editor.outline()->item(aRow)->data(Qt::AccessibleTextRole).toString(); };
			// The push is GitHub's: a refresh reads the pull request at its new head.
			window.github_refresh_action()->trigger();
			const auto observed = [&] { const auto& d = editor.shown().Draft; return d && d->ObservedHead ? std::string{ core::canonical_hex(*d->ObservedHead).view() } : std::string{ "none" }; };
			try {
				wait([&] { return observed() == "c8a86ccf72f2c7f7f325d053ad9a2a3241c0c933"; }, "the tour observes the pushed head");
			} catch (...) {
				std::cerr << "tour-push-observed: " << observed() << " head=" << core::canonical_hex(editor.shown().Draft->Head).view() << " status=" << application->window().github_status_text() << '\n';
				throw;
			}
			// The comparison of the new head on screen is what the window
			// checks the accepted code against.
			const auto show = [&](const std::size_t aIndex, const char* aPath) {
				editor.go_to_stop(editor.shown().Draft->Stops[aIndex].Id);
				wait([&] {auto* surface=window.comparison_surface();return surface->current_result() && surface->current_context() && surface->current_context()->Key.File==core::file_identity_of(aPath) && !surface->geometry_pending(); }, "the stop opens its file at the pushed head");
			};
			show(2U, "secondary.cpp");
			show(0U, "review.cpp");
			wait([&] { return decided(0).Reason != core::tour_acceptance_reason::unchecked && decided(1).Reason != core::tour_acceptance_reason::unchecked && decided(2).Reason != core::tour_acceptance_reason::unchecked; }, "every stop is compared with the pushed head");
			for (int row = 0; row < 3; ++row) {
				std::cout << "tour-push-after: " << spoken(row).toStdString() << " reason=" << static_cast<int>(decided(static_cast<std::size_t>(row)).Reason) << " reference=" << decided(static_cast<std::size_t>(row)).Reference << '\n';
			}
			expect(decided(0).State == core::tour_acceptance_state::changed && decided(0).Reason == core::tour_acceptance_reason::lines_changed && spoken(0).contains(QStringLiteral("Changed since accepted")), "the stop whose lines changed is Changed since accepted: its lines");
			expect(decided(1).State == core::tour_acceptance_state::changed && decided(1).Reason == core::tour_acceptance_reason::reference_changed && decided(1).Reference == "secondary.cpp:5-6" && spoken(1).contains(QStringLiteral("Changed since accepted")),
				   "the stop that links to changed lines is Changed since accepted: the link");
			expect(decided(2).State == core::tour_acceptance_state::accepted && spoken(2).contains(QStringLiteral("Accepted")) && !spoken(2).contains(QStringLiteral("Changed")), "the stop nothing of which changed stays Accepted");
			// Accept at the new head: its lines chosen there, then Accept.
			show(0U, "review.cpp");
			select_range(window, core::diff_side::right, 1, 2);
			editor.findChild<QAction*>(QStringLiteral("tourRetarget"))->trigger();
			saved(editor);
			editor.accept_stop(editor.shown().Draft->Stops[0].Id);
			saved(editor);
			expect(decided(0).State == core::tour_acceptance_state::accepted && spoken(0).contains(QStringLiteral("Accepted")) && !spoken(0).contains(QStringLiteral("Changed")), "Accept at the new head turns the first stop green");
			expect(decided(2).State == core::tour_acceptance_state::accepted, "the unchanged stop is still Accepted");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 0U, "the push and accepting write nothing to GitHub");
			}
			cli.check();
			shutdown(*application);
			std::cout << "review-tour-publication:push:pass\n";
			return EXIT_SUCCESS;
		}
		accept_every_stop(editor);
		button("tourPreparePublication")->click();
		expect(!button("tourPublish")->isEnabled(), "unfinished draft cannot publish");
		editor.problem_editor()->setPlainText(QStringLiteral("Avoid losing the original explanation."));
		editor.solution_editor()->setPlainText(QStringLiteral("Keep exact sources and a readable route."));
		if (mode == "compat") {
			wait([&] { return editor.shown().Draft && editor.shown().Draft->Stops.size() == 2U; }, "compatibility fixture opens with its two stops");
			select_file(window, "review.cpp");
		} else {
			select_file(window, "review.cpp");
			select_range(window, core::diff_side::right, 2, 4);
			window.add_tour_stop_action()->trigger();
			editor.stop_title()->setPlainText(QStringLiteral("Capture the original"));
			editor.stop_body()->setPlainText(QStringLiteral("Retain this exact source interval."));
			select_file(window, "secondary.cpp");
			select_range(window, core::diff_side::right, mode == "mixed" ? 2U : 1U, mode == "mixed" ? 4U : 3U);
			window.add_tour_stop_action()->trigger();
			editor.stop_title()->setPlainText(QStringLiteral("Use the retained source"));
			editor.stop_body()->setPlainText(mode == "native" ? QStringLiteral("Connect the explanation to the original decision (`review.cpp:2-4`).\n\nsecondary.cpp:1-3\n\nNot in this change: elsewhere.cpp:9.")
															  : QStringLiteral("Connect the explanation to the original decision."));
		}
		saved(editor);

		if (mode == "file") {
			auto       fixtureDraft                    = *editor.shown().Draft;
			const auto expected                        = fixtureDraft.Revision;
			fixtureDraft.Stops[1].Original->First.Line = 1;
			fixtureDraft.Stops[1].Original->Last.Line  = 1;
			++fixtureDraft.Revision;
			shutdown(*application);
			application.reset();
			{
				auto store = adapters::sqlite::make_review_tour_store(*dataRoot, fixtureDraft.Owner.Profile);
				expect(store->save(fixtureDraft, expected, { }).has_value(), "file choice fixture saves");
			}
			application = make();
			open(*application, cli);
			auto& fileWindow = application->window();
			auto& fileEditor = *fileWindow.tour_editor();
			wait([&] { return fileEditor.shown().Draft && fileEditor.shown().Draft->Stops.size() == 2; }, "file draft opens");
			select_file(fileWindow, "review.cpp");
			fileWindow.open_tour_page(true);
			fileWindow.resize(1200, 760);
			fileEditor.roadmap()->setCurrentIndex(fileEditor.roadmap()->count() - 1);
			auto find = [&](const char* aName) { return fileEditor.findChild<QPushButton*>(QString::fromUtf8(aName)); };
			// Unknown exact operation facts do not expose a file choice, even though ordinary tour publishing is qualified.
			server->StagedFileQualified = false;
			accept_every_stop(fileEditor);
			find("tourPreparePublication")->click();
			wait([&] { return fileEditor.findChild<QLabel*>(QStringLiteral("tourRepairReason2")); }, "unknown operations project reason");
			expect(!fileEditor.findChild<QPushButton*>(QStringLiteral("tourFileComment2")) && server->StagedWrites == 0, "general qualification grants no file operation");
			// [[BPR-1992]]. With no file comment on offer, the off-hunk stop goes as a
			// conversation comment, and no review can carry the tour: the plan chooses
			// compatibility comments and says which stop forced it.
			wait([&] { return find("tourPublish")->isEnabled(); }, "the plan chooses the form that carries every stop");
			expect(fileEditor.findChild<QLabel*>(QStringLiteral("tourPublicationForm"))->text().contains(QStringLiteral("Stop 2 sits outside the lines GitHub shows in this diff")) && fileEditor.findChild<QLabel*>(QStringLiteral("tourPublicationForm"))->text().contains(QStringLiteral("own conversation comment")), "the form sentence names the stop that forced it");
			// A fresh revision asks the provider again. Only this exact commit/path/range has recorded support.
			server->StagedFileQualified = true;
			fileEditor.outline()->setCurrentRow(0);
			fileEditor.findChild<QToolButton*>(QStringLiteral("tourStopEdit"))->click();
			fileEditor.stop_title()->setPlainText("Exact file capability course");
			saved(fileEditor);
			accept_every_stop(fileEditor);
			find("tourPreparePublication")->click();
			wait([&] { return find("tourFileComment2"); }, "exact standalone support exposes explicit file choice");
			find("tourFileComment2")->click();
			expect(fileEditor.shown().Draft->Stops[1].FileCommentRequested, "file choice is an ordinary local edit");
			fileEditor.findChild<QAction*>(QStringLiteral("tourUndo"))->trigger();
			expect(!fileEditor.shown().Draft->Stops[1].FileCommentRequested, "file choice can be undone");
			fileEditor.findChild<QAction*>(QStringLiteral("tourRedo"))->trigger();
			expect(fileEditor.shown().Draft->Stops[1].FileCommentRequested, "file choice can be redone");
			saved(fileEditor);
			fileEditor.findChild<QCheckBox*>(QStringLiteral("tourDescriptionEdit"))->setChecked(false);
			accept_every_stop(fileEditor);
			find("tourPreparePublication")->click();
			wait([&] { return find("tourPublish")->isEnabled() && fileEditor.findChild<QLabel*>(QStringLiteral("tourPublicationForm"))->text().contains(QStringLiteral("file comment")); }, "with a file comment on offer, the plan chooses the mixed tour");
			expect(fileEditor.findChild<QLabel*>(QStringLiteral("tourPublicationForm"))->text().contains(QStringLiteral("One stop goes in one review. Stop 2 sits outside the lines GitHub shows in this diff, so it is sent as a file comment.")), "the mixed form's sentence names the file-comment stop");
			expect(fileEditor.publication_preview()->original_body().contains("standalone file comment, a separate visible write outside the pending review"), "file preview states true membership and visible sequence");
			fileEditor.outline()->setCurrentRow(1);
			QApplication::processEvents();
			expect(fileEditor.publication_preview()->height() >= 160, "file plan retains a readable viewport");
			if (argc > 2) {
				expect(fileWindow.grab().save(QString::fromUtf8(argv[2]) + "-file.png"), "file full-window render");
			}
			// Interrupt the separate file write after its durable receipt, reopen, and resume only by command.
			{
				const auto lock  = std::scoped_lock{ server->Mutex };
				server->Boundary = [state = server.get()](std::size_t aIndex, std::string_view aBoundary) {if(aIndex==1 && aBoundary=="applied"){state->StagedBlockNext=true;} };
			}
			find("tourPublish")->click();
			wait([&] {const auto lock=std::scoped_lock{server->Mutex};return server->StagedBlocked; }, "file write is interrupted after transmission");
			find("tourStop")->click();
			wait([&] { return !find("tourStop")->isEnabled(); }, "file stop drains");
			shutdown(*application);
			application.reset();
			{
				const auto lock         = std::scoped_lock{ server->Mutex };
				server->Boundary        = { };
				server->StagedBlockNext = false;
			}
			application = make();
			open(*application, cli);
			auto& resumed = *application->window().tour_editor();
			auto  rb      = [&](const char* aName) { return resumed.findChild<QPushButton*>(QString::fromUtf8(aName)); };
			wait([&] { return resumed.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().contains("Partially published") || resumed.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().contains("Staged"); }, "file restart reconciles read-only");
			{
				const auto lock = std::scoped_lock{ server->Mutex };
				expect(server->StagedWrites == 2 && server->FileComments.size() == 1 && server->StagedComments.size() == 1, "standalone file is retained separately from pending membership");
			}
			accept_every_stop(resumed);
			rb("tourPreparePublication")->click();
			expect(rb("tourResume")->isEnabled(), "file recovery needs explicit Resume");
			rb("tourResume")->click();
			wait([&] { return resumed.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().startsWith("Published:"); }, "file mixed Resume completes");
			saved(resumed);
			expect(resumed.shown().Draft->Stops[1].Remote->Kind == core::tour_remote_kind::file_comment, "file binding keeps its remote kind");
			resumed.outline()->setCurrentRow(1);
			resumed.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(resumed);
			accept_every_stop(resumed);
			rb("tourPreparePublication")->click();
			wait([&] { return rb("tourPublish")->isEnabled(); }, "file maintenance preview");
			rb("tourPublish")->click();
			wait([&] { return resumed.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().startsWith("Published:"); }, "file maintenance completes");
			{
				const auto lock = std::scoped_lock{ server->Mutex };
				expect(server->StagedCreates == 1 && server->StagedSubmits == 1 && server->FileComments.size() == 1 && server->CompatibilityComments.empty(), "file recovery and maintenance never repost or pretend it joined the pending batch");
			}
			shutdown(*application);
			std::cout << "review-tour-publication:file:pass\n";
			return EXIT_SUCCESS;
		}
		if (mode == "mixed") {
			auto       fixtureDraft         = *editor.shown().Draft;
			const auto expected             = fixtureDraft.Revision;
			auto       historical           = fixtureDraft.Stops.front();
			historical.Id                   = core::tour_stop_id{ 3 };
			historical.Title                = "Keep the historical discussion";
			historical.Original->Commit     = *core::parse_git_commit_oid("d7d2f5c3e8762bbddc4e9e3444aa21a2fa74c3b7");
			historical.Original->First.Line = 4U;
			historical.Original->Last.Line  = 5U;
			fixtureDraft.Stops.insert(fixtureDraft.Stops.begin() + 1, historical);
			fixtureDraft.LastStopId = 3;
			++fixtureDraft.Revision;
			shutdown(*application);
			application.reset();
			{
				auto store = adapters::sqlite::make_review_tour_store(*dataRoot, fixtureDraft.Owner.Profile);
				expect(store->save(fixtureDraft, expected, { }).has_value(), "mixed fixture saves through ordinary store");
			}
			application = make();
			open(*application, cli);
			auto& mixedWindow = application->window();
			auto& mixedEditor = *mixedWindow.tour_editor();
			wait([&] { return mixedEditor.shown().Draft && mixedEditor.shown().Draft->Stops.size() == 3; }, "mixed fixture opens");
			select_file(mixedWindow, "review.cpp");
			mixedWindow.open_tour_page(true);
			mixedWindow.resize(1200, 760);
			mixedEditor.roadmap()->setCurrentIndex(mixedEditor.roadmap()->count() - 1);
			auto find = [&](const char* aName) { return mixedEditor.findChild<QPushButton*>(QString::fromUtf8(aName)); };
			accept_every_stop(mixedEditor);
			find("tourPreparePublication")->click();
			wait([&] { return mixedEditor.findChild<QPushButton*>(QStringLiteral("tourRepin3")); }, "repair remains the first offered option");
			// [[BPR-1992]]. No mode list: the plan chooses the mixed tour, says so with
			// the stop that forced it, and offers the repairs under Keep it to one review.
			expect(mixedEditor.findChild<QComboBox*>(QStringLiteral("tourPublicationMode")) == nullptr, "no mode list is offered");
			mixedEditor.findChild<QCheckBox*>(QStringLiteral("tourDescriptionEdit"))->setChecked(false);
			accept_every_stop(mixedEditor);
			find("tourPreparePublication")->click();
			wait([&] { return find("tourPublish")->isEnabled(); }, "the chosen mixed preview is publishable");
			auto* const form = mixedEditor.findChild<QLabel*>(QStringLiteral("tourPublicationForm"));
			expect(form->text().contains(QStringLiteral("Two stops go in one review. Stop 2 points at an earlier version of the code, so it is sent as a conversation comment.")), "the form sentence names the stop that forced the mixed tour");
			auto* const keep = find("tourKeepOneReview");
			expect(keep->isVisible(), "a mixed tour offers Keep it to one review instead");
			keep->click();
			QApplication::processEvents();
			expect(mixedEditor.findChild<QLabel*>(QStringLiteral("tourKeepOneReviewList"))->text().contains(QStringLiteral("Keep the historical discussion")) && find("tourPublish")->isEnabled(), "Keep it to one review lists the stop to move and changes nothing");
			keep->click();
			const auto preview = mixedEditor.publication_preview()->original_body();
			expect(preview.contains("non-atomic") && preview.contains("Stops outside Files changed:") && preview.contains("Stop 2: Keep the historical discussion") && preview.contains("one COMMENT submission"), "preview identifies exact outside-diff stops and visible sequence");
			expect(server->StagedWrites == 0, "mixed preview sends nothing");
			mixedEditor.outline()->setCurrentRow(1);
			QApplication::processEvents();
			expect(mixedEditor.publication_preview()->height() >= 160, "mixed plan retains a readable viewport");
			if (argc > 2) {
				expect(mixedWindow.grab().save(QString::fromUtf8(argv[2]) + "-mixed.png"), "mixed full-window render");
			}
			find("tourPublish")->click();
			wait([&] { return mixedEditor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().contains("Published:"); }, "mixed tour publishes");
			saved(mixedEditor);
			{
				const auto lock = std::scoped_lock{ server->Mutex };
				expect(server->StagedCreates == 1 && server->StagedSubmits == 1 && server->StagedComments.size() == 2 && server->CompatibilityComments.size() == 1 && server->StagedWrites == 6, "mixed route creates exactly one review and only its one conversation remainder");
				expect(server->StagedReview->Body.find("/files#r601") < server->StagedReview->Body.find("#issuecomment-701") && server->StagedReview->Body.find("#issuecomment-701") < server->StagedReview->Body.find("/files#r602"), "mixed overview follows original interleaved order");
				expect(server->CompatibilityComments.front().Body.contains("/files#r601") && server->CompatibilityComments.front().Body.contains("/files#r602"), "cross-kind previous and next use returned identities");
			}
			const auto original = *mixedEditor.shown().Draft;
			expect(original.Remote && original.Stops[1].Remote->Kind == core::tour_remote_kind::conversation_comment && original.Stops[2].Remote->DatabaseId == 602, "composed receipt binds one tour in route order");
			mixedEditor.outline()->setCurrentRow(1);
			mixedEditor.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(mixedEditor);
			accept_every_stop(mixedEditor);
			find("tourPreparePublication")->click();
			expect(find("tourPublish")->isEnabled(), "mixed tour can later preview ordinary maintenance");
			find("tourPublish")->click();
			wait([&] { return mixedEditor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().contains("Published:"); }, "mixed maintenance completes");
			saved(mixedEditor);
			{
				const auto lock = std::scoped_lock{ server->Mutex };
				expect(server->StagedCreates == 1 && server->StagedSubmits == 1 && server->CompatibilityComments.size() == 1 && server->StagedWritePaths.back().ends_with("/reviews/501"), "maintenance retains all cross-kind identities and writes original overview last");
			}
			shutdown(*application);
			std::cout << "review-tour-publication:mixed:pass\n";
			return EXIT_SUCCESS;
		}
		if (mode == "repairs") {
			// Fixture preparation uses the same admitted store. The running
			// course below obtains all mapping/material itself through Git.
			auto       fixtureDraft                         = *editor.shown().Draft;
			const auto expected                             = fixtureDraft.Revision;
			fixtureDraft.Stops.front().Original->Commit     = *core::parse_git_commit_oid("d7d2f5c3e8762bbddc4e9e3444aa21a2fa74c3b7");
			fixtureDraft.Stops.front().Original->First.Line = 4U;
			fixtureDraft.Stops.front().Original->Last.Line  = 5U;
			fixtureDraft.Stops.back().Original->First.Line  = 1U;
			fixtureDraft.Stops.back().Original->Last.Line   = 1U;
			fixtureDraft.Stops.back().Title                 = "Inspect the off-hunk source";
			const auto ordinary                             = fixtureDraft.Stops.front();
			for (std::uint64_t number = 3U; number <= 10U; ++number) {
				auto stop             = ordinary;
				stop.Id               = core::tour_stop_id{ number };
				stop.Title            = "Explain valid destination " + std::to_string(number);
				stop.Original->Commit = fixtureDraft.Head;
				fixtureDraft.Stops.push_back(std::move(stop));
			}
			fixtureDraft.LastStopId = 10U;
			++fixtureDraft.Revision;
			shutdown(*application);
			application.reset();
			{
				auto store = adapters::sqlite::make_review_tour_store(*dataRoot, fixtureDraft.Owner.Profile);
				expect(store->save(fixtureDraft, expected, { }).has_value(), "repair fixture persists without a UI backdoor");
			}
			application = make();
			open(*application, cli);
			auto& repairedWindow = application->window();
			auto& repairedEditor = *repairedWindow.tour_editor();
			auto  find           = [&](const char* aName) -> QPushButton* {
				for (auto* control : repairedEditor.findChildren<QPushButton*>(QString::fromUtf8(aName))) {
					if (control->isVisible()) {
						return control;
					}
				}
				return nullptr;
			};
			wait([&] { return repairedEditor.shown().Draft && repairedEditor.shown().Draft->Stops.size() == 10U; }, "ten-stop saved fixture loads");
			select_file(repairedWindow, "review.cpp");
			repairedWindow.open_tour_page(true);
			repairedWindow.resize(1200, 760);
			accept_every_stop(repairedEditor);
			find("tourPreparePublication")->click();
			// [[BPR-1992]]. The plan carries the tour as it can; the moves that keep it
			// to one review are under Keep it to one review instead.
			wait([&] { return find("tourKeepOneReview") != nullptr; }, "the plan offers Keep it to one review instead");
			find("tourKeepOneReview")->click();
			wait([&] { return find("tourRepin1") && find("tourRelocate2"); }, "public preview reads exact historical mapping and off-hunk range");
			expect(repairedEditor.findChild<QLabel*>(QStringLiteral("tourRepairReason1"))->text().contains(QStringLiteral("byte-identical")), "historical exact reason is visible");
			expect(repairedEditor.findChild<QLabel*>(QStringLiteral("tourRepairReason2"))->text().contains(QStringLiteral("Outside")), "off-hunk reason is visible");
			auto* reason = repairedEditor.findChild<QLabel*>(QStringLiteral("tourRepairReason2"));
			expect(QMetaObject::invokeMethod(reason, "linkActivated", Q_ARG(QString, QStringLiteral("stop"))), "repair reason's public link activates");
			expect(repairedEditor.selected_stop() && repairedEditor.selected_stop()->Id == core::tour_stop_id{ 2U }, "reason selects the affected stop");
			repairedWindow.open_tour_page(true);
			QApplication::processEvents();
			if (argc > 2) {
				expect(repairedWindow.grab().save(QString::fromUtf8(argv[2]) + QStringLiteral("-repairs.png")), "per-stop full-window capture");
			}
			find("tourRepin1")->click();
			expect(repairedEditor.shown().Draft->Stops.front().Mapping == core::tour_mapping::author_selected && repairedEditor.shown().Draft->Stops.front().Body == ordinary.Body, "one re-pin preserves the explanation");
			QApplication::processEvents();
			find("tourRelocate2")->click();
			auto* sourceText = repairedEditor.findChild<QPlainTextEdit*>(QStringLiteral("tourRepairSourceText"));
			expect(sourceText->isVisible() && sourceText->toPlainText().contains(QStringLiteral("secondary_line_3 = 0")), "nearest accepted context is previewed from exact Git material");
			expect(!repairedEditor.shown().Draft->Stops[1].Mapped, "preview changes no source");
			QApplication::processEvents();
			if (argc > 2) {
				expect(repairedWindow.grab().save(QString::fromUtf8(argv[2]) + QStringLiteral("-relocation.png")), "relocation source capture");
			}
			// Editing after preview invalidates its captured ordinary command.
			repairedEditor.findChild<QToolButton*>(QStringLiteral("tourStopEdit"))->click();
			repairedEditor.stop_title()->setPlainText(QStringLiteral("Inspect the off-hunk source carefully"));
			expect(!sourceText->isVisible(), "an intervening edit expires the source preview");
			repairedEditor.findChild<QPushButton*>(QStringLiteral("tourApplyRepair"))->click();
			expect(!repairedEditor.shown().Draft->Stops[1].Mapped, "expired preview cannot apply its captured source");
			QApplication::processEvents();
			find("tourRelocate2")->click();
			find("tourApplyRepair")->click();
			expect(repairedEditor.selected_stop() && repairedEditor.selected_stop()->Id == core::tour_stop_id{ 2U } && repairedEditor.shown().Draft->Stops[1].Mapped->First.Line == 2U, "apply keeps selection and clamps into three lines of context");
			repairedEditor.findChild<QAction*>(QStringLiteral("tourUndo"))->trigger();
			QApplication::processEvents();
			expect(!repairedEditor.shown().Draft->Stops[1].Mapped && find("tourRelocate2"), "ordinary Undo restores the blocker and re-plans");
			repairedEditor.findChild<QAction*>(QStringLiteral("tourRedo"))->trigger();
			expect(repairedEditor.shown().Draft->Stops[1].Mapped.has_value(), "ordinary Redo restores the repair");
			accept_every_stop(repairedEditor);
			find("tourPreparePublication")->click();
			wait([&] { return find("tourPublish")->isEnabled(); }, "both repairs re-plan as one review");
			const auto beforePublicationSnapshot = access::window_read_progress(*application).Snapshot;
			find("tourPublish")->click();
			wait([&] { return repairedEditor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().contains(QStringLiteral("Published:")); }, "repaired tour publishes");
			{
				const auto lock = std::scoped_lock{ server->Mutex };
				expect(server->StagedCreates == 1U && server->StagedSubmits == 1U && server->Submissions == 0U && server->StagedComments.size() == 10U && server->CompatibilityComments.empty(), "ten repaired stops publish in one COMMENT review with zero compatibility writes");
			}
			saved(repairedEditor);
			const auto published = *repairedEditor.shown().Draft;
			for (std::size_t i = 0; i < 2; ++i) {
				expect(!core::needs_tour_destination(published.Stops[i]), "published repairs are no longer pending retargets");
				expect(published.Stops[i].Original == fixtureDraft.Stops[i].Original && published.Stops[i].Mapped && published.Stops[i].Mapped->Blob, "publication preserves original and repaired byte identities");
				expect(published.Stops[i].Destination == core::publication_source(published.Stops[i]), "receipt retains the actual published source separately");
			}
			accept_every_stop(repairedEditor);
			wait([&] {
				const auto progress = access::window_read_progress(*application);
				return progress.Snapshot.has_value() && progress.Snapshot != beforePublicationSnapshot && !progress.Reading;
			},
				 "published pull request snapshot settles before maintenance preview");
			find("tourPreparePublication")->click();
			// The window may still be checking the receipt it just took; Preview is
			// pressed again until the publication accepts it.
			wait([&] {
				const auto done = repairedEditor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().contains("No published changes");
				if (!done && repairedEditor.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->isEnabled()) {
					accept_every_stop(repairedEditor);
					repairedEditor.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
				}
				return done;
			},
				 "second preview of repaired tour completes");
			expect(!repairedEditor.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "second preview of repaired tour offers no duplicate destinations or writes");
			repairedEditor.outline()->setCurrentRow(0);
			repairedEditor.findChild<QToolButton*>(QStringLiteral("tourStopEdit"))->click();
			repairedEditor.stop_body()->setPlainText(QStringLiteral("A maintained explanation at the repaired source."));
			saved(repairedEditor);
			accept_every_stop(repairedEditor);
			find("tourPreparePublication")->click();
			expect(find("tourPublish")->isEnabled(), "repaired discussion supports an explicit text maintenance preview");
			find("tourPublish")->click();
			wait([&] { return repairedEditor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().startsWith("Published:"); }, "repaired source maintenance completes");
			saved(repairedEditor);
			{
				const auto lock = std::scoped_lock{ server->Mutex };
				expect(server->StagedWrites == 14U && server->StagedCreates == 1U && server->StagedSubmits == 1U && server->StagedComments.size() == 10U && server->CompatibilityComments.empty(), "text maintenance adds only one edit after thirteen initial writes");
				expect(server->StagedComments.front().Body.contains("A maintained explanation at the repaired source.") && server->StagedWritePaths.back().ends_with("/pulls/comments/601"), "maintenance edits the repaired discussion rather than replacing it");
			}
			for (std::size_t i = 0; i < published.Stops.size(); ++i) {
				expect(repairedEditor.shown().Draft->Stops[i].Remote == published.Stops[i].Remote && repairedEditor.shown().Draft->Stops[i].Original == published.Stops[i].Original && repairedEditor.shown().Draft->Stops[i].Mapped == published.Stops[i].Mapped, "maintenance preserves every original, repaired range and remote identity");
			}
			shutdown(*application);
			std::cout << "review-tour-publication:repairs:pass\n";
			return EXIT_SUCCESS;
		}
		if (mode == "manual") {
			editor.outline()->setCurrentRow(1);
			editor.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(editor);
			expect(editor.shown().Draft->Stops.front().Title == "Use the retained source", "manual route reorder precedes publication");
			// BPR-1852 moved the draft state from summary text to the title chip.
			wait([&] {auto* chip=window.overview_page()->draft_chip();return chip && !chip->isHidden() && chip->accessibleName() == QStringLiteral("Draft pull request"); }, "actual provider snapshot renders draft PR status");
		}
		// [[BPR-2014]], RFC-BPR-0106. The author goes to each stop, whose code
		// shows, and accepts it there: publication requires every stop Accepted.
		{
			const auto ids = [&] {
				auto value = std::vector<core::tour_stop_id>{ };
				for (const auto& stop : editor.shown().Draft->Stops) {
					value.push_back(stop.Id);
				}
				return value;
			}();
			for (const auto id : ids) {
				editor.go_to_stop(id);
				QApplication::processEvents();
				if (editor.stop_view().Mode == core::tour_stop_view_mode::edit) {
					editor.save_stop_edit();
				} else if (editor.accept_button()->isVisible()) {
					editor.accept_button()->click();
				}
				saved(editor);
				const auto found = std::ranges::find(editor.shown().Draft->Stops, id, &core::tour_stop::Id);
				expect(found != editor.shown().Draft->Stops.end() && core::tour_stop_accepted(*editor.shown().Draft, *found), "the author accepts each stop where its code shows");
			}
		}
		editor.roadmap()->setCurrentIndex(editor.roadmap()->count() - 1);
		// [[BPR-1787]]. The brief goes in the description by default; the stop
		// course keeps it in the tour comment instead.
		auto* const describe = editor.findChild<QCheckBox*>(QStringLiteral("tourDescriptionEdit"));
		expect(describe->isChecked() && describe->toolTip().contains(QStringLiteral("Problem and Solution go in the PR description")), "Update PR description starts on and says where the brief goes");
		const bool describing = mode != "stop";
		describe->setChecked(describing);
		accept_every_stop(editor);
		button("tourPreparePublication")->click();
		if (mode == "unsupported") {
			// [[BPR-1992]], AC06. No form carries the tour on this connection: refused
			// before any write, naming the stops, the operation and both ways out.
			auto* const banner = editor.findChild<QWidget*>(QStringLiteral("tourPublicationBanner"));
			expect(!button("tourPublish")->isEnabled() && banner != nullptr && !banner->isHidden(), "unqualified batching cannot authorize publication; the refusal is a banner");
			const auto why = editor.findChild<QLabel*>(QStringLiteral("tourPublicationBannerWhy"))->text();
			expect(editor.findChild<QLabel*>(QStringLiteral("tourPublicationBannerTitle"))->text() == QStringLiteral("This connection cannot publish this tour") && why.contains(QStringLiteral("Stops 1 and 2 need review comments on their lines")) && why.contains(QStringLiteral("this connection cannot create them")), "the refusal names the stops and the operation");
			expect(editor.findChild<QLabel*>(QStringLiteral("tourPublicationBannerOnGitHub"))->text().startsWith(QStringLiteral("Nothing was sent.")) && button("tourBannerMoveStops") && button("tourBannerOtherConnection") && status->isHidden(), "both ways out, and never a status line");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 0, "unsupported preview performs zero writes");
			}
			shutdown(*application);
			std::cout << "review-tour-publication:unsupported:pass\n";
			return EXIT_SUCCESS;
		}
		// A historical stop's source is read before its destination is planned.
		wait([&] { return button("tourPublish")->isEnabled(); }, "complete qualified route exposes Publish only after preview");
		if (mode == "compat") {
			expect(editor.findChild<QLabel*>(QStringLiteral("tourPublicationForm"))->text().contains(QStringLiteral("Every stop goes as its own conversation comment. Stops 1 and 2 point at an earlier version of the code, so no single review can carry the tour.")), "[[BPR-1992]]: the plan chose compatibility comments and says why");
		}
		if (argc > (cliMode ? 3 : 2)) {
			expect(window.grab().save(QString::fromUtf8(argv[cliMode ? 3 : 2])), "publication preview capture saved");
		}
		auto* preview = editor.publication_preview();
		expect(preview->original_body().contains("Actor:") && preview->original_body().contains("Head:"), "preview freezes identity");
		if (describing) {
			expect(preview->original_body().contains("Separate optional description edit") && preview->original_body().contains("Problem and Solution go in the PR description, not the tour comment.") && preview->original_body().contains(core::tour_brief_in_description_note), "preview says the brief goes in the description");
		} else {
			expect(preview->original_body().contains("Problem and Solution go in the tour comment.") && preview->original_body().contains("## Problem") && preview->original_body().contains("The PR description will not be edited."), "preview says the brief stays in the tour comment");
		}
		if (mode == "compat") {
			expect(preview->original_body().contains("non-atomic"), "explicit compatibility preview describes visible partial sequence");
		}
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedWrites == 0 && server->Submissions == 0, "draft editing autosave navigation and preview perform zero remote writes");
			server->StagedConflict          = mode == "conflict";
			server->StagedRejectDescription = mode == "description";
			server->StagedBlockNext         = mode == "stop";
		}
		if (mode == "compat" && !cliMode) {
			expect(window.review_submission_editor_view()->shown_projection().Controls.Authored, "compatibility course publishes as the pull request author");
			{
				const auto lock            = std::scoped_lock{ server->Mutex };
				server->TourFactoryFailure = application::review_mutation_failure::unavailable;
			}
			button("tourPublish")->click();
			wait([&] { return !button("tourStop")->isEnabled(); }, "factory refusal terminates");
			expect(status->text().contains("Publication could not start") && server->StagedWrites == 0U, "pre-provider refusal is actionable and sends nothing");
			{
				const auto lock = std::scoped_lock{ server->Mutex };
				server->TourFactoryFailure.reset();
			}
			for (const auto http : { 403, 429, 500, 200 }) {
				accept_every_stop(editor);
				button("tourPreparePublication")->click();
				wait([&] { return button("tourPublish")->isEnabled() || button("tourResume")->isEnabled(); }, "the compatibility preview is ready");
				{
					const auto lock                 = std::scoped_lock{ server->Mutex };
					server->StagedBaselineStatus    = http == 200 ? 0 : http;
					server->StagedMalformedBaseline = http == 200;
				}
				button("tourPublish")->click();
				wait([&] { return !button("tourStop")->isEnabled(); }, "baseline refusal terminates");
				expect(status->text().contains("Could not read existing pull request comments") && status->text().contains(QStringLiteral("HTTP %1").arg(http)) && status->text().contains("This attempt sent nothing") && server->StagedWrites == 0U, "actual baseline failure keeps its bounded status and zero-write truth");
				if (http == 403 && argc > 2) {
					window.open_tour_page(true);
					QApplication::processEvents();
					expect(window.grab().save(QString::fromUtf8(argv[2]) + "-refused.png"), "baseline failure render saved");
				}
			}
			{
				const auto lock                 = std::scoped_lock{ server->Mutex };
				server->StagedBaselineStatus    = 0;
				server->StagedMalformedBaseline = false;
				server->StagedLargeBaseline     = true;
			}
			accept_every_stop(editor);
			button("tourPreparePublication")->click();
			wait([&] { return button("tourPublish")->isEnabled() || button("tourResume")->isEnabled(); }, "the compatibility preview is ready");
			for (const auto http : { 200, 403 }) {
				{
					const auto lock                 = std::scoped_lock{ server->Mutex };
					server->StagedBaselineStatus    = http == 200 ? 0 : http;
					server->StagedMalformedBaseline = http == 200;
					server->StagedFaultAtRead       = server->StagedBaselineReads + (http == 200 ? 2U : 1U);
				}
				button(http == 200 ? "tourPublish" : "tourResume")->click();
				wait([&] { return !button("tourStop")->isEnabled(); }, "failed verification read terminates");
				expect(status->text().contains("Could not check pull request comments") && status->text().contains(QStringLiteral("HTTP %1").arg(http)) && !status->text().contains("Publication conflict") && server->StagedWrites == 0U, "a malformed or denied second read is explained without inventing a remote change");
				if (http == 403 && argc > 2) {
					expect(window.grab().save(QString::fromUtf8(argv[2]) + "-verification-refused.png"), "verification failure render saved");
				}
				{
					const auto lock                 = std::scoped_lock{ server->Mutex };
					server->StagedBaselineStatus    = 0;
					server->StagedMalformedBaseline = false;
					server->StagedFaultAtRead       = 0U;
				}
				button("tourReconcile")->click();
				wait([&] { return !button("tourStop")->isEnabled(); }, "repaired verification is checked read-only");
				expect(server->StagedWrites == 0U, "Check status cannot publish the ready journal");
				accept_every_stop(editor);
				button("tourPreparePublication")->click();
				wait([&] { return button("tourPublish")->isEnabled() || button("tourResume")->isEnabled(); }, "the compatibility preview is ready");
				expect(button("tourResume")->isEnabled() && !button("tourPublish")->isEnabled(), "verification failure retains its exact journal for explicit Resume");
			}
		}
		if (cliMode) {
			cli.Strict->store(false);
			button("tourPublish")->click();
			expect(server->StagedWrites == 0U && status->text().contains(QStringLiteral("draft is kept")), "missing strict capability refuses tour without a popup or a write");
			cli.Strict->store(true);
		}
		if (cliMode) {
			for (const auto failure : { application::review_mutation_failure::credential_unavailable, application::review_mutation_failure::credential_tool_failed, application::review_mutation_failure::cancelled }) {
				{
					std::scoped_lock lock{ server->Mutex };
					server->CliAcquisitionFailure = failure;
				}
				button("tourPublish")->click();
				wait([&] { return !button("tourStop")->isEnabled() && status->text().contains(QStringLiteral("draft is kept")); }, "typed tour acquisition refusal retains private draft");
				expect(server->StagedWrites == 0U && editor.problem_editor()->toPlainText() == QStringLiteral("Avoid losing the original explanation."), "tour acquisition failure performs zero writes and keeps prose");
				if (failure != application::review_mutation_failure::cancelled) {
					expect(status->text().contains(QStringLiteral("octo-reviewer")) && status->text().contains(QStringLiteral("github.com")), "tour failure names its captured account and host");
				}
				accept_every_stop(editor);
				button("tourPreparePublication")->click();
			}
			{
				std::scoped_lock lock{ server->Mutex };
				server->CliAcquisitionFailure.reset();
			}
		}
		if (mode == "compat" && !cliMode) {
			const auto lock                 = std::scoped_lock{ server->Mutex };
			server->StagedMalformedBaseline = true;
			server->StagedFaultAtRead       = server->StagedBaselineReads + 2U;
		}
		button(mode == "compat" && !cliMode ? "tourResume" : "tourPublish")->click();
		// [[BPR-1782]]. From the press until GitHub answers, Publish reads as
		// working, names the captured plan's stages, and Stop stays.
		auto* working = editor.findChild<QWidget*>(QStringLiteral("tourPublicationWorking"));
		expect(working && button("tourPublish")->text() == QStringLiteral("Publishing\u2026") && !button("tourPublish")->isEnabled() && button("tourStop")->isEnabled() && !button("tourPreparePublication")->isEnabled(), "pressed Publish reads as working with Stop available");
		// [[BPR-1992]]. The plan's own steps, the first sending and the rest waiting
		// until GitHub confirms each; the head says the same count.
		const auto rows = editor.publication_steps()->row_texts();
		expect(!working->isHidden() && status->isHidden() && rows.size() >= 4 && rows.front().startsWith(QStringLiteral("Sending: ")) && rows.front().contains(mode == "compat" ? QStringLiteral("Stop 1, \u201cCapture the original\u201d, as a conversation comment") : QStringLiteral("Stops 1 and 2, in the pending review")) && rows.contains(QStringLiteral("Waiting: Links for stop 2")) && (!describing || rows.back() == QStringLiteral("Waiting: The description link")), "the checklist names the plan's own steps in order");
		expect(editor.head().State == core::tour_head_state::publishing && editor.head().Step == editor.publication_shown().Step && editor.head().Steps == editor.publication_shown().Total && editor.head().Steps == static_cast<std::size_t>(mode == "compat" ? (describing ? 6 : 5) : (describing ? 5 : 4)), "the head's count is the panel's");
		if (mode == "stop") {
			wait([&] {std::scoped_lock lock{server->Mutex};return server->StagedBlocked; }, "production session reaches first durable staged effect");
			expect(button("tourPublish")->text() == QStringLiteral("Publishing\u2026") && button("tourStop")->isEnabled() && !working->isHidden(), "working state holds while GitHub has not answered");
			// [[BPR-1992]]. The first write was authorized and is on its way: it is
			// the step sending, and nothing is marked sent before GitHub confirms it.
			wait([&] { return editor.publication_steps()->row_texts().size() >= 4 && editor.publication_steps()->row_texts().front().startsWith(QStringLiteral("Sending: ")); }, "the first step reads as sending");
			expect(std::ranges::none_of(editor.publication_steps()->row_texts(), [](const QString& aRow) { return aRow.startsWith(QStringLiteral("Sent: ")); }) && editor.head().State == core::tour_head_state::publishing && editor.head().Step == 1U, "nothing is sent until GitHub confirms it; the head is at step 1");
			if (argc > (cliMode ? 3 : 2)) {
				// The tour page, where the head's Details takes the author.
				button("tourHeadDetails")->click();
				QApplication::processEvents();
				expect(window.tour_page_open() && !working->isHidden(), "the tour page shows the working panel");
				expect(window.grab().save(QString::fromUtf8(argv[cliMode ? 3 : 2]) + "-working.png"), "in-flight publication capture saved");
			}
			button("tourStop")->click();
			wait([&] { return !button("tourStop")->isEnabled(); }, "Stop drains the active grouped grant");
			std::size_t before;
			{
				std::scoped_lock lock{ server->Mutex };
				before = server->StagedWrites;
				expect(before == 1, "Stop preserves one exact pending creation");
			}
			shutdown(*application);
			application.reset();
			application = make();
			open(*application, cli);
			auto& restored       = *application->window().tour_editor();
			auto* restoredStatus = restored.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"));
			wait([&] { return restoredStatus->text().contains(QStringLiteral("Staged on GitHub")); }, "restarted real app reconciles pending review before Resume");
			// [[BPR-1991]]. The head says the publication stopped part-way, with Resume.
			expect(restored.head().State == core::tour_head_state::stopped && restored.head().Actions.front() == core::tour_head_action::resume && restored.head().Step == 2U && !restored.publication_banner()->isHidden() && restored.publication_banner()->title()->text() == QStringLiteral("Publishing stopped at step 2 of 4") && restored.publication_banner()->buttons().front()->text() == QStringLiteral("Resume"), "the restarted head says where publishing stopped, with Resume");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == before, "reopening and reconciliation emit zero writes");
			}
			auto* prepare = restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"));
			prepare->click();
			auto* resume = restored.findChild<QPushButton*>(QStringLiteral("tourResume"));
			expect(resume->isEnabled(), "captured-plan preview authorizes explicit Resume control");
			if (!cliMode) {
				{
					const auto lock            = std::scoped_lock{ server->Mutex };
					server->TourFactoryFailure = application::review_mutation_failure::unavailable;
				}
				resume->click();
				wait([&] { return !restored.findChild<QPushButton*>(QStringLiteral("tourStop"))->isEnabled(); }, "resume factory refusal terminates");
				expect(restoredStatus->text().contains("Publication could not start") && server->StagedWrites == before, "resume failure sends nothing and retains staged prefix");
				{
					const auto lock = std::scoped_lock{ server->Mutex };
					server->TourFactoryFailure.reset();
				}
				prepare->click();
				expect(resume->isEnabled(), "pre-provider failure retains the original resumable journal state");
			}

			if (cliMode) {
				{
					std::scoped_lock lock{ server->Mutex };
					server->CliAcquisitionFailure = application::review_mutation_failure::credential_unavailable;
				}
				resume->click();
				wait([&] { return restoredStatus->text().startsWith(QStringLiteral("No further publication.")); }, "partial tour retains prior staged work when credential becomes unavailable");
				expect(server->StagedWrites == before && static_cast<bool>(restored.shown().Draft), "partial resume refusal performs zero new writes and retains draft");
				{
					std::scoped_lock lock{ server->Mutex };
					server->CliAcquisitionFailure.reset();
				}
				prepare->click();
				expect(resume->isEnabled(), "the retained original partial plan still permits explicit Resume after repreview");
			}
			resume->click();
			expect(resume->isEnabled() == false && restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->text() == QStringLiteral("Publishing\u2026") && restored.findChild<QPushButton*>(QStringLiteral("tourStop"))->isEnabled(), "Resume reads as working too");
			wait([&] { return restoredStatus->text().startsWith(QStringLiteral("Published:")); }, "Resume completes the original pending review");
			expect(restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->text() == QStringLiteral("Published") && !restoredStatus->isHidden() && restored.findChild<QLabel*>(QStringLiteral("tourPublicationLink"))->text().contains(QStringLiteral("#pullrequestreview-")), "resumed success says Published with where GitHub shows it");
			if (argc > (cliMode ? 3 : 2)) {
				application->window().open_tour_page(true);
				QApplication::processEvents();
				expect(application->window().tour_page_open(), "the tour page shows the published state");
				expect(application->window().grab().save(QString::fromUtf8(argv[cliMode ? 3 : 2]) + "-done.png"), "published capture saved");
			}
		} else if (mode == "conflict") {
			wait([&] { return status->text().contains(QStringLiteral("Publication conflict")); }, "existing pending conflict is visible");
			auto* const banner = editor.publication_banner();
			expect(working->isHidden() && !banner->isHidden() && (banner->title()->text() == QStringLiteral("Not published") || banner->title()->text().startsWith(QStringLiteral("Publishing stopped at step"))) && banner->why()->text().contains(QStringLiteral("Publication conflict")) && banner->buttons().front()->text() == QStringLiteral("Check status") && button("tourPublish")->text() == QStringLiteral("Publish tour") && !button("tourStop")->isEnabled(), "a refused publication is a banner with Check status, and stops working");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedWrites == 0, "existing website pending review is neither adopted nor modified");
		} else if (mode == "description") {
			wait([&] { return status->text().contains(QStringLiteral("Tour published; description update needs attention")); }, "description failure is separate from confirmed tour success");
			expect(!editor.publication_banner()->isHidden() && editor.publication_banner()->title()->text().startsWith(QStringLiteral("Publishing stopped at step")) && button("tourReconcile")->isEnabled() && button("tourPublish")->text() == QStringLiteral("Publish tour"), "description attention is said in the banner with Check status");
			button("tourReconcile")->click();
			wait([&] { return !button("tourStop")->isEnabled(); }, "description reconciliation completes read-only");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedCreates == 1 && server->StagedSubmits == 1 && server->StagedWrites == 4 && !server->StagedDescription, "description failure retains submitted review without reposting");
		} else {
			if (mode == "compat" && !cliMode) {
				wait([&] { return !button("tourStop")->isEnabled(); }, "post-write read failure terminates with its witness");
				expect(status->text().contains("result is still unknown") && status->text().contains("Could not check pull request comments") && !status->text().contains("This attempt sent nothing") && !status->text().contains("Publication conflict") && !button("tourResume")->isEnabled() && server->StagedWrites == 1U, "unknown UI keeps its failed read reason without authorizing a blind retry");
				{
					const auto lock                 = std::scoped_lock{ server->Mutex };
					server->StagedMalformedBaseline = false;
					server->StagedFaultAtRead       = 0U;
				}
				button("tourReconcile")->click();
				wait([&] { return !button("tourStop")->isEnabled(); }, "unknown compatibility receipt reconciles read-only");
				expect(server->StagedWrites == 1U, "unknown recovery never reposts the first comment");
				accept_every_stop(editor);
				button("tourPreparePublication")->click();
				wait([&] { return button("tourResume")->isEnabled(); }, "reconciled exact prefix permits explicit Resume");
				button("tourResume")->click();
			}
			wait([&] { return status->text().startsWith(mode == "compat" ? QStringLiteral("Published as compatibility") : QStringLiteral("Published:")); }, "normal Publish reaches verified success");
			expect(working->isHidden() && !status->isHidden() && button("tourPublish")->text() == QStringLiteral("Published") && !button("tourStop")->isEnabled() && editor.findChild<QLabel*>(QStringLiteral("tourPublicationLink"))->text().contains(mode == "compat" ? QStringLiteral("#issuecomment-") : QStringLiteral("#pullrequestreview-")), "success says Published with where GitHub shows it");
		}
		if (mode != "conflict" && mode != "description") {
			std::scoped_lock lock{ server->Mutex };
			if (mode == "compat") {
				expect(server->StagedCreates == 0 && server->StagedSubmits == 0 && server->CompatibilityComments.size() == 3 && server->StagedWrites == 6, "explicit compatibility emits three visible comments and two link edits, then description");
			} else {
				expect(server->StagedCreates == 1 && server->StagedSubmits == 1 && server->StagedComments.size() == 2 && server->StagedWrites == (describing ? 5U : 4U), "native app submits exactly one review after two navigation patches");
				// [[BPR-1785]]. Each published stop says what to do when Next does not jump.
				for (const auto& comment : server->StagedComments) {
					const auto stop = core::map_tour_stop(comment.Body);
					expect(stop.Heading && comment.Body.substr(stop.Heading->Begin, stop.Heading->End - stop.Heading->Begin).starts_with("### PR Tour · "), "the comment published to GitHub is headed PR Tour");
					expect(stop.Footer && comment.Body.ends_with(")\n" + std::string{ core::tour_navigation_note } + "\n"), "each published stop carries the navigation note directly under its links");
				}
				// [[BPR-1996]], RFC-BPR-0100 AC06. A stop's references are
				// published as commit-pinned permalinks: inline a Markdown
				// link, alone on its line the bare permalink GitHub draws as a
				// snippet; one naming no file of the pull request as written.
				if (mode == "native") {
					const auto cited = std::ranges::find_if(server->StagedComments, [](const auto& aComment) { return aComment.Body.find("original decision") != std::string::npos; });
					expect(cited != server->StagedComments.end(), "the citing stop was published");
					const auto& body = cited->Body;
					std::cout << "tour-references-published: " << QString::fromStdString(body).replace(QLatin1Char('\n'), QStringLiteral(" | ")).toStdString() << '\n';
					expect(body.find("([`review.cpp:2-4`](https://github.com/") != std::string::npos && body.find("#L2-L4)") != std::string::npos, "the inline reference is a Markdown link to the commit-pinned permalink");
					expect(body.find("\n\nhttps://github.com/") != std::string::npos && body.find("secondary.cpp#L1-L3\n") != std::string::npos, "the reference alone on its line is the bare permalink");
					expect(body.find("Not in this change: elsewhere.cpp:9.") != std::string::npos, "a reference naming no file of the pull request is published as written");
				}
			}
			// [[BPR-1787]]. The brief is published in exactly one place.
			const auto& tourBody = mode == "compat" ? server->CompatibilityComments.back().Body : server->StagedReview->Body;
			if (describing) {
				expect(server->StagedDescription && server->StagedDescription->contains(mode == "compat" ? "#issuecomment-703" : "#pullrequestreview-501"), "description uses actual returned index link only after publication");
				expect(server->StagedDescription->contains(originalTemplate), "first publication preserves original template bytes including CRLF");
				// [[BPR-1786]]. The tour link leads the description.
				expect(server->StagedDescription->starts_with("[Review tour](https://"), "the tour link is at the top of the description");
				expect(server->StagedDescription->contains("## Problem\r\nAvoid losing the original explanation.") && server->StagedDescription->contains("## Solution\r\nKeep exact sources and a readable route."), "the brief is in the description");
				expect(tourBody.starts_with(std::string{ core::tour_brief_in_description_note } + "\n\n## Review tour") && !tourBody.contains("## Problem") && !tourBody.contains("## Solution") && !tourBody.contains("Avoid losing the original explanation."), "the tour comment lists only the route");
			} else {
				expect(server->StagedDescription == originalTemplate, "the description is untouched when the brief stays in the tour comment");
				expect(tourBody.starts_with("## Problem\n\nAvoid losing the original explanation.\n\n## Solution\n\nKeep exact sources and a readable route.\n\n## Review tour"), "the brief is in the tour comment");
			}
		}
		if (mode == "reconciliation") {
			saved(editor);
			const auto rootEntity = editor.shown().Draft->Remote->Entity;
			editor.problem_editor()->setPlainText("Local overlapping problem.");
			saved(editor);
			{
				std::scoped_lock lock{ server->Mutex };
				auto&            description = *server->StagedDescription;
				description.replace(description.find("Avoid losing the original explanation."), std::string{ "Avoid losing the original explanation." }.size(), "Website overlapping problem.");
				description.replace(description.find("Keep exact sources and a readable route."), std::string{ "Keep exact sources and a readable route." }.size(), "Website revised solution.");
				description = "Website template preface.\n\n" + description + "\n\n## Testing\n\nKeep website template tail.\n";
				server->StagedComments.front().Body.insert(0, "Website raw prefix.\n\n");
				auto route = core::map_tour_markdown(server->StagedReview->Body).Lists.front();
				std::reverse(route.Entries.begin(), route.Entries.end());
				server->StagedReview->Body.replace(route.Source.Begin, route.Source.End - route.Source.Begin, *core::render_tour_index(route.Entries));
			}
			window.github_refresh_action()->trigger();
			wait([&] { return editor.shown().Draft && editor.shown().Draft->Conflicts.size() == 1; }, "website/local overlap arrives through real snapshot parser");
			expect(editor.problem_editor()->toPlainText() == "Local overlapping problem." && editor.solution_editor()->toPlainText().contains("Website revised solution."), "conflict preserves local problem and adopts unrelated website solution");
			expect(editor.shown().Draft->Stops.front().Remote->DatabaseId == 602 && editor.shown().Draft->Stops.back().Baseline->Raw.starts_with("Website raw prefix."), "website index reorders stable comments and raw prefix survives");
			expect(editor.shown().Draft->HasPriorObservation && !editor.shown().Draft->ObservedChanges.empty() && editor.findChild<QLabel*>(QStringLiteral("tourRevisionChanges"))->text().contains("Reordered"), "actual website observation exposes retained revision differences");
			const auto beforeNavigation = access::review_mutation_evidence(*application);
			editor.findChild<QPushButton*>(QStringLiteral("tourNextChanged"))->click();
			expect(editor.selected_stop() != nullptr, "changed-stop action navigates actual outline");
			const auto afterNavigation = access::review_mutation_evidence(*application);
			expect(beforeNavigation.Submissions == afterNavigation.Submissions && beforeNavigation.Resolutions == afterNavigation.Resolutions && !access::review_ledger_receipt_present(*application), "changed-stop navigation has no review-status effect");
			expect(editor.findChild<QPlainTextEdit*>(QStringLiteral("tourConflict"))->toPlainText().contains("Website overlapping problem."), "editor exposes the exact remote conflict version");
			saved(editor);
			editor.show_problem();
			window.resize(1440, 1000);
			QApplication::processEvents();
			if (argc > 2) {
				expect(window.grab().save(QString::fromUtf8(argv[2]) + "-conflict.png"), "actual B/L/R conflict capture saved");
			}
			if (argc > 3 && std::string_view{ argv[3] } == "--attended") {
				editor.show_problem();
				window.resize(1440, 1000);
				window.setWindowTitle(QStringLiteral("Review tour conflict — synthetic website — close to finish"));
				QApplication::setQuitOnLastWindowClosed(true);
				QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
				std::cout << "review-tour-conflict-attended:ready\n"
						  << std::flush;
				const auto result = QApplication::exec();
				shutdown(*application);
				return result;
			}
			shutdown(*application);
			application.reset();
			application = make();
			open(*application, cli);
			auto& restored = *application->window().tour_editor();
			wait([&] { return restored.shown().Draft && restored.shown().Draft->Conflicts.size() == 1; }, "unresolved B/L/R conflict survives real application restart");
			restored.findChild<QPushButton*>(QStringLiteral("tourKeepLocalConflict"))->click();
			saved(restored);
			expect(restored.shown().Draft->Conflicts.empty() && restored.shown().Draft->Problem == "Local overlapping problem.", "explicit local choice retains desired text");
			application->window().github_refresh_action()->trigger();
			wait([&] { return restored.shown().Draft && !restored.shown().Draft->RemoteStale; }, "resolved versions require fresh website observation");
			saved(restored);
			expect(access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations == 0, "refresh and conflict choice invoke no AI");
			shutdown(*application);
			application.reset();
			currentDataRoot = e2e::private_data_root(coldData);
			expect(currentDataRoot.has_value(), "clean private profile admitted");
			application = make();
			open(*application, cli);
			auto& cold    = *application->window().tour_editor();
			auto* choices = cold.findChild<QComboBox*>(QStringLiteral("tourImportChoices"));
			wait([&] { return choices->count() > 0; }, "clean profile discovers the actual shared published index");
			int selected = -1;
			for (int i = 0; i < choices->count(); ++i) {
				if (choices->itemText(i).contains(QString::fromStdString(rootEntity))) {
					selected = i;
				}
			}
			expect(selected >= 0, "cold profile selects exact published root");
			choices->setCurrentIndex(selected);
			cold.findChild<QPushButton*>(QStringLiteral("tourImport"))->click();
			wait([&] { return cold.shown().Draft && cold.shown().Draft->Remote; }, "explicit cold import fills editors");
			expect(cold.shown().Draft->Problem.contains("Website overlapping problem.") && !cold.shown().Draft->Problem.contains("Local overlapping problem.") && cold.shown().Draft->Stops.front().Remote->DatabaseId == 602, "cold import uses website truth and authoritative route");
			expect(!cold.shown().Draft->HasPriorObservation && cold.shown().Draft->ObservedChanges.empty() && cold.findChild<QLabel*>(QStringLiteral("tourRevisionChanges"))->text().contains("No prior local"), "clean profile explicitly has unknown private tour history");
			expect(!cold.shown().Draft->Stops.front().Original && cold.shown().Draft->Stops.front().Destination && cold.shown().Draft->Stops.front().Preparation == core::tour_preparation::editing, "cold profile fabricates no private preparation provenance");
			saved(cold);
			expect(access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations == 0, "cold reconstruction uses no inference");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 5 && server->StagedCreates == 1 && server->StagedSubmits == 1, "refresh conflict resolution restart and cold import produce zero server mutations");
			}
			accept_every_stop(cold);
			cold.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(!cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "unchanged cold reimport cannot publish normalization edits");
			cold.problem_editor()->setPlainText("Cold-profile deliberate revision.");
			saved(cold);
			// [[BPR-1787]]. The cold-imported brief lives in the description,
			// so its revision goes there whatever the checkbox says.
			cold.findChild<QCheckBox*>(QStringLiteral("tourDescriptionEdit"))->setChecked(false);
			std::string coldReview;
			{
				std::scoped_lock lock{ server->Mutex };
				coldReview = server->StagedReview->Body;
			}
			accept_every_stop(cold);
			cold.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(cold.publication_preview()->original_body().contains("Problem and Solution stay in the PR description, where they were published."), "the update preview says the brief stays in the description");
			expect(cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "cold-profile author can explicitly edit the existing tour");
			cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->click();
			wait([&] { return cold.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().startsWith("Published:"); }, "cold-profile body update preserves imported actor and source binding");
			saved(cold);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 6 && server->StagedCreates == 1 && server->StagedSubmits == 1 && server->StagedDescription->starts_with("Website template preface.") && server->StagedDescription->ends_with("Keep website template tail.\n"), "one narrow cold-profile description edit preserves template and every comment identity");
				expect(server->StagedDescription->contains("Cold-profile deliberate revision.") && server->StagedReview->Body == coldReview, "the brief revision goes to the description and the tour comment is unchanged");
			}
			const auto originalStops = cold.shown().Draft->Stops;
			expect(access::reaccept_fixture_revision(*application, true), "changed-head fixture enters actual accepted-revision path");
			wait([&] { return cold.shown().Draft && cold.shown().Draft->ObservedHead && cold.shown().Draft->Stops.front().Preparation == core::tour_preparation::needs_attention; }, "changed head exposes explanation attention");
			cold.outline()->setCurrentRow(0);
			// [[BPR-2014]], RFC-BPR-0106. The stop was accepted before the push.
			// This fixture's second head is a new handoff with no readable
			// content, so the window cannot compare the accepted code with it:
			// the stop stays yellow, "not compared yet", and is never left
			// green on evidence nobody read.
			const auto decided = [&] { return core::decide_tour_stop_acceptance(*cold.shown().Draft, cold.shown().Draft->Stops.front()); };
			expect(originalStops.front().Acceptance.has_value(), "the stop was accepted before the push");
			cold.go_to_stop(cold.shown().Draft->Stops.front().Id);
			saved(cold);
			expect(decided().State == core::tour_acceptance_state::changed && decided().Reason == core::tour_acceptance_reason::unchecked, "an uncompared push leaves the stop Changed since accepted, not compared yet");
			// The stop's Accept, which replaces the Head changed chip. Critical
			// review finding 3: pressed while its lines are not compared yet,
			// it keeps them to compare and is never green on unread evidence.
			cold.accept_stop(cold.shown().Draft->Stops.front().Id);
			saved(cold);
			const auto& kept = cold.shown().Draft->Stops.front();
			expect(decided().Reason == core::tour_acceptance_reason::unchecked && kept.Acceptance && std::ranges::any_of(kept.Acceptance->Material, [](const auto& aMaterial) { return !aMaterial.Reference; }),
				   "Accept while the stop's lines are not compared yet keeps them to compare");
			expect(kept.Remote == originalStops.front().Remote && kept.Destination == originalStops.front().Destination, "accepting preserves the original imported subject and comment");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 6, "a new head and accepting a stop issue zero remote writes");
			}
		}

		if (mode == "maintenance" || mode == "manual") {
			saved(editor);
			{
				std::scoped_lock lock{ server->Mutex };
				auto             reply    = server->StagedComments.front();
				server->ReplyParents[699] = reply.Id;
				reply.Id                  = 699;
				reply.NodeId              = "reply699";
				reply.WebUrl              = reply.WebUrl.substr(0, reply.WebUrl.find('#')) + "#discussion_r699";
				reply.Body                = "Website reviewer discussion must survive.";
				reply.Reply               = true;
				server->StagedComments.push_back(reply);
				if (mode == "manual") {
					server->StagedComments.front().Body.insert(0, "Website explanation prefix.\n\n");
					auto& description = *server->StagedDescription;
					description.replace(description.find("Keep exact sources and a readable route."), std::string{ "Keep exact sources and a readable route." }.size(), "Website-maintained solution.");
					description += "\n\n## Testing\n\nPreserve website template.\n";
				}
			}
			if (mode == "manual") {
				shutdown(*application);
				application.reset();
				application = make();
				open(*application, cli);
			}
			auto&      activeWindow  = application->window();
			auto&      activeEditor  = *activeWindow.tour_editor();
			const auto activeButton  = [&](const char* aName) { return activeEditor.findChild<QPushButton*>(QString::fromUtf8(aName)); };
			auto*      activeStatus  = activeEditor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"));
			auto*      activePreview = activeEditor.publication_preview();
			activeEditor.findChild<QCheckBox*>(QStringLiteral("tourDescriptionEdit"))->setChecked(false);
			if (mode == "manual") {
				wait([&] { return activeEditor.shown().Draft && activeEditor.shown().Draft->Solution.contains("Website-maintained solution.") && activeEditor.shown().Draft->Stops.front().Baseline->Raw.starts_with("Website explanation prefix."); }, "reopened application adopts website text and raw stop bytes");
				saved(activeEditor);
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 5, "website reopen remains read-only");
			}

			// [[BPR-2265]]. Two rereads that keep the pull request each renew
			// the window's publishing lease; the tour's Publish below still
			// reaches the server.
			for (auto round = 0; round < 2; ++round) {
				const auto snapshot = access::window_read_progress(*application).Snapshot;
				expect(access::reread_pull_request(*application), "the pull request is read again");
				wait([&] { const auto progress = access::window_read_progress(*application); return progress.Snapshot != snapshot && !progress.Reading; }, "the reread installs the pull request again, renewing the publishing lease");
			}
			activeEditor.outline()->setCurrentRow(1);
			activeEditor.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(activeEditor);
			accept_every_stop(activeEditor);
			activeButton("tourPreparePublication")->click();
			expect(activeButton("tourPublish")->isEnabled(), "published reorder offers explicit update preview");
			expect(activePreview->original_body().contains("non-atomic"), "maintenance preview is honest about separate writes");
			activeButton("tourPublish")->click();
			wait([&] { return activeStatus->text().startsWith(QStringLiteral("Published:")); }, "published reorder completes");
			saved(activeEditor);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 8 && server->StagedCreates == 1 && server->StagedSubmits == 1, "pure reorder only edits two bodies and original root");
				expect(server->StagedComments.size() == 3 && server->StagedComments.back().Body == "Website reviewer discussion must survive.", "replies retain identity and bytes");
				expect(server->StagedWritePaths.back().ends_with("/reviews/501"), "root update occurs last");
			}
			accept_every_stop(activeEditor);
			activeButton("tourPreparePublication")->click();
			expect(!activeButton("tourPublish")->isEnabled() && activeStatus->text().contains("No published changes"), "equal-body update offers no writes");
			select_file(activeWindow, "review.cpp");
			select_range(activeWindow, core::diff_side::right, 2, 4);
			activeWindow.add_tour_stop_action()->trigger();
			activeEditor.stop_title()->setPlainText("Later batch insertion");
			activeEditor.stop_body()->setPlainText("A later idea preserves the original discussion.");
			saved(activeEditor);
			activeEditor.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(activeEditor);
			accept_every_stop(activeEditor);
			activeButton("tourPreparePublication")->click();
			expect(activeButton("tourPublish")->isEnabled(), "later insertion has an explicit publication preview");
			{
				std::scoped_lock lock{ server->Mutex };
				server->StagedBlockNext = true;
			}
			activeButton("tourPublish")->click();
			wait([&] {std::scoped_lock lock{server->Mutex};return server->StagedBlocked; }, "new batch creation reaches durable witness");
			activeButton("tourStop")->click();
			wait([&] { return !activeButton("tourStop")->isEnabled(); }, "maintenance stop drains effects");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 9 && server->StagedCreates == 2, "new destination created once before interruption");
			}
			shutdown(*application);
			application.reset();
			application = make();
			open(*application, cli);
			auto& restored       = *application->window().tour_editor();
			auto* restoredStatus = restored.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"));
			wait([&] { return restoredStatus->text().contains("Staged on GitHub"); }, "restart reconciles insertion read-only");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 9, "restart does not resume writes");
			}
			accept_every_stop(restored);
			restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			auto* resume = restored.findChild<QPushButton*>(QStringLiteral("tourResume"));
			expect(resume->isEnabled(), "captured maintenance can explicitly resume");
			resume->click();
			wait([&] { return restoredStatus->text().startsWith("Published:"); }, "maintenance resume finishes original index");
			saved(restored);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedCreates == 2 && server->StagedSubmits == 2 && server->StagedWrites == 14, "insertion creates one later batch then minimum navigation and original root edits");
				expect(server->PreviousReviews.size() == 1 && server->PreviousReviews.front().Body.contains("Later batch insertion") && server->StagedWritePaths.back().ends_with("/reviews/501"), "route spans reviews and original index remains authoritative");
				expect(std::ranges::any_of(server->PreviousComments, [](const auto& aC) { return aC.Id == 699 && aC.Body == "Website reviewer discussion must survive."; }), "interruption preserves existing reply");
			}
			auto& restoredWindow = application->window();
			restored.outline()->setCurrentRow(0);
			const auto replacedId = restored.shown().Draft->Stops.front().Remote->DatabaseId;
			select_file(restoredWindow, "secondary.cpp");
			select_range(restoredWindow, core::diff_side::right, 1, 3);
			restored.findChild<QAction*>(QStringLiteral("tourRetarget"))->trigger();
			saved(restored);
			accept_every_stop(restored);
			restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "explicit source replacement can publish a new native subject");
			restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->click();
			wait([&] { return restoredStatus->text().startsWith("Published:"); }, "source replacement completes");
			saved(restored);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedCreates == 3 && server->StagedSubmits == 3 && server->StagedWrites == 20, "retarget creates only one replacement and minimal old-route edits");
				expect(std::ranges::any_of(server->PreviousComments, [replacedId](const auto& aC) { return aC.Id == replacedId && aC.Body.contains("replacement stop"); }) && server->StagedComments.front().Body.contains("original discussion"), "old and new immutable subjects visibly link without deleting discussion");
			}
			restored.outline()->setCurrentRow(2);
			restored.findChild<QAction*>(QStringLiteral("tourRemove"))->trigger();
			saved(restored);
			accept_every_stop(restored);
			restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->click();
			wait([&] { return restoredStatus->text().startsWith("Published:"); }, "replied-to retirement completes");
			saved(restored);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 23 && server->StagedCreates == 3 && server->StagedSubmits == 3, "retirement edits membership/navigation without remote deletion");
				expect(std::ranges::any_of(server->PreviousComments, [](const auto& aC) { return aC.Id == 699 && aC.Body == "Website reviewer discussion must survive."; }), "retiring root comment preserves its website reply");
			}
			if (mode == "manual") {
				const auto completed = *restored.shown().Draft;
				expect(access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations == 0 && !access::review_ledger_receipt_present(*application), "complete manual update has no AI or review receipt dependency");
				shutdown(*application);
				application.reset();
				currentDataRoot = e2e::private_data_root(coldData);
				expect(currentDataRoot.has_value(), "new clean profile admitted");
				application = make();
				open(*application, cli);
				auto& cold    = *application->window().tour_editor();
				auto* choices = cold.findChild<QComboBox*>(QStringLiteral("tourImportChoices"));
				wait([&] { return choices->count() > 0; }, "clean profile discovers shared website roots");
				int selected = -1;
				for (int i = 0; i < choices->count(); ++i) {
					if (choices->itemText(i).contains(QString::fromStdString(completed.Remote->Entity))) {
						selected = i;
					}
				}
				expect(selected >= 0, "complete manual root survives across all batches");
				choices->setCurrentIndex(selected);
				cold.findChild<QPushButton*>(QStringLiteral("tourImport"))->click();
				wait([&] { return cold.shown().Draft && cold.shown().Draft->Remote && cold.shown().Draft->Stops.size() == completed.Stops.size(); }, "cold profile reconstructs complete maintained route");
				const auto imported = *cold.shown().Draft;
				for (std::size_t i = 0; i < completed.Stops.size(); ++i) {
					expect(imported.Stops[i].Remote->Entity == completed.Stops[i].Remote->Entity && imported.Stops[i].Baseline->Raw == completed.Stops[i].Baseline->Raw, "cold route preserves exact remote ids/order/bodies across batches");
				}
				expect(!imported.HasPriorObservation && imported.ObservedChanges.empty() && imported.Solution.contains("Website-maintained solution."), "cold profile uses shared brief and declares unknown private history");
				accept_every_stop(cold);
				cold.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
				expect(!cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "cold maintained route offers no normalization writes");
				cold.outline()->setCurrentRow(0);
				cold.trigger(qt::tour_editor_action::navigate_stop); // [[BPR-2013]] the file-and-lines link
				expect(access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations == 0 && !access::review_ledger_receipt_present(*application), "cold import/navigation starts no AI or review receipt");
				if (argc > 2) {
					expect(application->window().grab().save(QString::fromUtf8(argv[2]) + "-cold.png"), "complete maintained cold route render");
				}
				saved(cold);
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 23 && server->StagedCreates == 3 && server->StagedSubmits == 3 && server->DraftPullRequest && server->StagedDescription->ends_with("Preserve website template.\n"), "independent server counts preserve draft status and website template through complete manual course");
			}
		}
		if ((mode == "native" || mode == "compat") && !cliMode) {
			saved(*application->window().tour_editor());
			shutdown(*application);
			application.reset();
			application = make();
			open(*application, cli);
			auto& restored = *application->window().tour_editor();
			wait([&] {const auto& draft=restored.shown().Draft;return draft && draft->Remote && draft->Stops.size()==2 && draft->Stops[0].Remote && draft->Stops[1].Remote; }, "completed publication attribution survives app restart");
			accept_every_stop(restored);
			restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(!restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "published draft cannot offer a duplicate publication");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedWrites == (mode == "compat" ? 6 : 5), "completed restart and preview perform zero writes");
		}
		// [[BPR-2116]], RFC-BPR-0100 AC06. A tour update that edits a published
		// stop writes its references as a first publication does, and the
		// published baseline keeps the author's text, so a restart sees no
		// change.
		if (mode == "native" && !cliMode) {
			auto&      edited      = *application->window().tour_editor();
			const auto press       = [&](const char* aName) { return edited.findChild<QPushButton*>(QString::fromUtf8(aName)); };
			auto*      shownStatus = edited.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"));
			edited.outline()->setCurrentRow(1);
			wait([&] { return edited.stop_title()->toPlainText() == QStringLiteral("Use the retained source"); }, "the citing published stop is open");
			edited.stop_body()->setPlainText(QStringLiteral("Edited after publishing (`review.cpp:3-4`).\n\nsecondary.cpp:2-3"));
			saved(edited);
			accept_every_stop(edited);
			press("tourPreparePublication")->click();
			expect(press("tourPublish")->isEnabled(), "an edited published stop offers an update");
			std::size_t before = 0U;
			{
				std::scoped_lock lock{ server->Mutex };
				before = server->StagedWrites;
			}
			press("tourPublish")->click();
			wait([&] { return shownStatus->text().startsWith(QStringLiteral("Published:")); }, "the update of an edited published stop completes");
			saved(edited);
			{
				std::scoped_lock lock{ server->Mutex };
				const auto       cited = std::ranges::find_if(server->StagedComments, [](const auto& aComment) { return aComment.Body.find("Edited after publishing") != std::string::npos; });
				expect(cited != server->StagedComments.end(), "the edited stop's comment was updated on GitHub");
				const auto& body = cited->Body;
				std::cout << "tour-references-updated: " << QString::fromStdString(body).replace(QLatin1Char('\n'), QStringLiteral(" | ")).toStdString() << '\n';
				expect(body.find("(`review.cpp:3-4`).") == std::string::npos, "the updated comment does not keep the reference as written");
				expect(body.find("([`review.cpp:3-4`](https://github.com/") != std::string::npos && body.find("#L3-L4)") != std::string::npos, "the updated inline reference is a Markdown link to the commit-pinned permalink");
				expect(body.find("\n\nhttps://github.com/") != std::string::npos && body.find("secondary.cpp#L2-L3\n") != std::string::npos, "the updated reference alone on its line is the bare permalink");
				expect(server->StagedWrites > before, "the update wrote to GitHub");
				before = server->StagedWrites;
			}
			expect(edited.shown().Draft && edited.shown().Draft->Stops.size() == 2U && edited.shown().Draft->Stops[1].Body == "Edited after publishing (`review.cpp:3-4`).\n\nsecondary.cpp:2-3", "the author's text stays as written");
			shutdown(*application);
			application.reset();
			application = make();
			open(*application, cli);
			auto& again = *application->window().tour_editor();
			wait([&] {const auto& draft=again.shown().Draft;return draft && draft->Remote && draft->Stops.size()==2 && draft->Stops[1].Remote && draft->Stops[1].Body.starts_with("Edited after publishing"); }, "the updated tour survives a restart");
			expect(core::tour_unpublished_changes(*again.shown().Draft) == 0U, "a restarted app sees the updated tour as unchanged");
			accept_every_stop(again);
			again.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(!again.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "a restarted app offers no update for the published edit");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedWrites == before, "the restart and its preview write nothing");
		}
		if (mode == "compat" && !cliMode) {
			wait([&] { return tests::read_diagnostic_logs(*dataRoot).contains("tour_state=published"); }, "normal app diagnostic log contains publication terminal");
			const auto            logs = tests::read_diagnostic_logs(*dataRoot);
			std::set<std::string> attempts;
			bool                  verificationMalformed = false, verificationDenied = false;
			for (const auto& line : QString::fromStdString(logs).split('\n')) {
				const auto detail = QJsonDocument::fromJson(line.toUtf8()).object()["detail"].toString().toStdString();
				if (const auto diagnosis = core::parse_operation_diagnosis(detail); diagnosis && diagnosis->Tour && diagnosis->Tour->Attempt != "none") {
					attempts.insert(diagnosis->Tour->Attempt);
					if (diagnosis->Tour->Stage == core::tour_publication_stage::verification) {
						verificationMalformed = verificationMalformed || diagnosis->Failure == core::operation_failure::malformed_response;
						verificationDenied    = verificationDenied || (diagnosis->Failure == core::operation_failure::refused_by_server && diagnosis->Tour->HttpStatus == 403U);
					}
				}
			}
			expect(verificationMalformed && verificationDenied, "normal app logs preserve classified verification errors after baseline admission");
			expect(logs.contains("tour_state=outcome_unknown") && logs.contains("tour_stage=recovery"), "post-write failure and read-only recovery are retained in the same correlated trace");
			expect(attempts.size() == 1U && attempts.begin()->size() == 64U, "preview admission refusals wire operations and restart correlate to this exact authority/attempt");
			for (const auto stage : { "preview", "admission", "publish", "factory", "baseline_read", "list_issue_comments", "create_issue_comment", "update_issue_comment", "update_description", "terminal" }) {
				expect(logs.contains(std::string{ "tour_stage=" } + stage), "normal logs retain every relevant lifecycle stage");
			}
			expect(logs.contains("failure=refused_by_server") && logs.contains("tour_http=403") && logs.contains("failure=malformed_response") && !logs.contains("Avoid losing the original explanation.") && !logs.contains(originalTemplate), "logs keep classified refusal but omit private prose");
			const auto exported = application::prepare_diagnostic_export(logs, core::release_maintenance_revision{ 1 }, { core::diagnostic_export_policy::anonymized }, { }, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
			expect(exported && exported->Bytes.contains("tour_stage=baseline_read") && exported->Bytes.contains("tour_state=published") && exported->Bytes.contains("tour_http=403") && !exported->Bytes.contains("Avoid losing the original explanation."), "product diagnostic export retains sanitized failure and success lifecycle");
		}
		if (mode == "stop" && !cliMode) {
			wait([&] { return tests::read_diagnostic_logs(*dataRoot).contains("tour_state=published"); }, "recovery terminal reaches normal app diagnostics");
			const auto logs     = tests::read_diagnostic_logs(*dataRoot);
			const auto exported = application::prepare_diagnostic_export(logs, core::release_maintenance_revision{ 1 }, { core::diagnostic_export_policy::anonymized }, { }, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
			expect(exported && exported->Bytes.contains("tour_stage=recovery") && exported->Bytes.contains("tour_stage=stop") && exported->Bytes.contains("tour_state=staged") && exported->Bytes.contains("tour_state=published"), "real diagnostic export retains Stop restart reconciliation explicit Resume and success");
		}
		cli.check();
		shutdown(*application);
		std::cout << "review-tour-publication:" << mode << ":pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		const auto evidence = access::evidence(*application);
		std::cerr << "read operation=" << evidence.LastReadOperation << " count=" << evidence.AccountReadCount << " recording-failure=" << evidence.RecordingFailure << "\n";
		std::cerr << "publication-status: " << application->window().tour_editor()->findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().toStdString() << "\n";
		std::cerr << "read-status: " << application->window().github_status_text() << " writes=" << server->StagedWrites << "\n";
		std::cerr << "review-tour-publication:fail " << error.what() << '\n';
		shutdown(*application);
		return EXIT_FAILURE;
	}
}
