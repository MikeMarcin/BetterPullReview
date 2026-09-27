#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include "bpr/core/review_tour_markdown.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QInputMethodEvent>
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
#include <QToolButton>
#include <iostream>
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
	void open(app::github_application& aApp)
	{
		aApp.show();
		auto& window = aApp.window();
		window.github_connect_action()->trigger();
		wait([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "connected controls");
		window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
		window.home_open_action()->trigger();
		wait([&] { return window.prepare_tour_action()->isEnabled() && !window.tour_editor()->shown().Loading; }, "ordinary PR open composes tour host");
		window.prepare_tour_action()->trigger();
		QApplication::processEvents();
		expect(window.tray_tabs()->tabText(window.tray_tabs()->currentIndex()) == QStringLiteral("Tour") && window.tour_editor()->isVisible(), "Prepare review tour opens the composed Tour tray");
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
	QTemporaryDir data;
	const auto    dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		return EXIT_FAILURE;
	}
	{
		auto settings = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = *dataRoot });
		if (!settings) {
			return EXIT_FAILURE;
		}
		auto preferences               = application::workbench_profile_preferences{ };
		preferences.Behaviour.MarkRead = application::review_mark_read_behaviour::when_fully_seen;
		if (!(*settings)->store_preferences(preferences, { })) {
			return EXIT_FAILURE;
		}
	}
	auto server = std::make_shared<app::detail::review_submission_fixture_state>();
	auto make   = [&] {
		std::unique_ptr<app::github_application> started;
		bool                                     terminal = false;
		auto                                     startup  = access::start(app::detail::github_public_path_fixture::profile_window_restoration_v1,
																		  app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot, .ReviewTourCourse = argc > 1 && std::string_view{ argv[1] } == "--cold", .ReviewSubmissionServer = server },
																		  [&](auto aResult) { terminal = true; if (aResult) { started = std::move(*aResult); } });
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(started != nullptr, "startup succeeds");
		return started;
	};
	auto application = make();
	try {
		expect(!application->window().prepare_tour_action()->isEnabled() && application->window().tour_editor()->problem_editor()->isReadOnly(), "no PR has no invented writable draft");
		open(*application);
		auto& window = application->window();
		auto& editor = *window.tour_editor();
		expect(editor.problem_editor()->toPlainText().isEmpty() && editor.solution_editor()->toPlainText().isEmpty(), "new draft is empty, coaching is not source");

		if (argc > 1 && std::string_view{ argv[1] } == "--cold") {
			auto* choices = editor.findChild<QComboBox*>(QStringLiteral("tourImportChoices"));
			wait([&] { return choices && choices->count() == 1; }, "published root discovered through real provider snapshot");
			editor.findChild<QPushButton*>(QStringLiteral("tourImport"))->click();
			expect(editor.shown().Draft->Stops.size() == 2 && !editor.shown().Draft->ReadOnly, "cold profile reconstructs editable own route");
			expect(editor.shown().Draft->Stops[0].Remote->DatabaseId == 71002 && editor.shown().Draft->Stops[1].Remote->DatabaseId == 71001, "website list order overrides returned collection order");
			expect(editor.shown().Draft->Problem == "\r\n**Keep raw** problem.\r\n\r\n" && editor.shown().Draft->Solution == "\r\nSmall exact solution.\r\n\r\n", "ordinary description sections preserve raw CRLF bytes");
			auto* description = window.overview_page()->body_view();
			description->show_source(true);
			auto selection = description->source_view()->textCursor();
			selection.setPosition(0);
			selection.setPosition(10, QTextCursor::KeepAnchor);
			description->source_view()->setTextCursor(selection);
			editor.findChild<QPushButton*>(QStringLiteral("tourSolutionMap"))->click();
			expect(editor.shown().Draft->Solution == "Preface α\r\n" && editor.shown().Draft->SolutionBaseline->SourceSpan == std::optional{ std::pair<std::size_t, std::size_t>{ 0, 12 } }, "manual native source selection maps Unicode and CRLF back to exact UTF-8 bytes");
			const auto baseline = *editor.shown().Draft->ProblemBaseline;
			editor.problem_editor()->selectAll();
			editor.problem_editor()->insertPlainText(QStringLiteral("Revised problem."));
			saved(editor);
			const auto       span = *baseline.SourceSpan;
			const std::array edits{ core::tour_text_edit{ { span.first, span.second }, baseline.Raw.substr(span.first, span.second - span.first), editor.shown().Draft->Problem } };
			const auto       edited = core::edit_tour_markdown(baseline.Raw, edits);
			expect(edited && *edited == baseline.Raw.substr(0, span.first) + "Revised problem." + baseline.Raw.substr(span.second), "only explicitly mapped problem bytes change; testing prose and backlink remain exact");
			const auto accepted = *editor.shown().Draft;
			expect(access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations == 0 && access::review_mutation_evidence(*application).Submissions == 0, "cold import and editing have no inference or remote writes");
			shutdown(*application);
			application.reset();
			application = make();
			open(*application);
			expect(*application->window().tour_editor()->shown().Draft == accepted, "real store reopen retains raw baselines and remote attribution");
			shutdown(*application);
			std::cout << "review-tour-cold-import:pass\n";
			return EXIT_SUCCESS;
		}
		const auto mutationBefore = access::review_mutation_evidence(*application);
		const auto agentBefore    = access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations;
		expect(agentBefore == 0U, "no agent configured or launched for authoring");
		const auto problem = QStringLiteral("## Before\n\nLiteral <details><summary>why</summary>\n\n```cpp\nreturn 1;\n```\n</details>\n\n| A | B |\n| - | - |\n| x | y |\n");
		editor.problem_editor()->insertPlainText(problem);
		saved(editor);
		editor.problem_editor()->undo();
		expect(editor.problem_editor()->toPlainText().isEmpty() && editor.shown().Draft->Problem.empty(), "native undo survives asynchronous save acknowledgement");
		editor.problem_editor()->redo();
		expect(editor.shown().Draft->Problem == problem.toStdString(), "native redo restores exact Markdown source");
		editor.roadmap()->setCurrentIndex(1);
		editor.solution_editor()->setFocus();
		QInputMethodEvent preedit{ QStringLiteral("に"), { } };
		QApplication::sendEvent(editor.solution_editor(), &preedit);
		expect(editor.shown().Draft->Solution.empty(), "IME preedit is not committed into the draft");
		QInputMethodEvent commit;
		commit.setCommitString(QStringLiteral("日本語 — "));
		QApplication::sendEvent(editor.solution_editor(), &commit);
		QApplication::clipboard()->setText(QStringLiteral("**central approach**\n\n[read more](https://example.invalid/)"));
		editor.solution_editor()->paste();
		const auto solution = editor.solution_editor()->toPlainText();
		expect(solution.startsWith(QStringLiteral("日本語 — **central")), "native IME commit and clipboard preserve Unicode Markdown");
		saved(editor);
		window.add_tour_stop_action()->trigger();
		expect(editor.shown().Draft->Stops.empty() && editor.findChild<QLabel*>(QStringLiteral("tourSaveStatus"))->text().contains(QStringLiteral("Select a line")), "unavailable source refuses capture while preserving manual draft");
		select_file(window, "review.cpp");
		expect(window.behaviour().MarkRead == application::review_mark_read_behaviour::when_fully_seen, "real application loaded opt-in automatic read preference from admitted store");
		select_range(window, core::diff_side::right, 2, 4);
		window.add_tour_stop_action()->trigger();
		expect(editor.shown().Draft->Stops.size() == 1U && editor.selected_stop() && editor.selected_stop()->Original->First.Line == 2U && editor.selected_stop()->Original->Last.Line == 4U && editor.selected_stop()->Original->Blob, "Add tour stop captures exact range and blob through normal source action");
		const auto firstId = editor.selected_stop()->Id;
		editor.stop_title()->insert(QStringLiteral("Why this boundary matters"));
		editor.stop_body()->insertPlainText(QStringLiteral("First thought.\n\nSecond thought."));
		select_range(window, core::diff_side::left, 1, 1);
		editor.findChild<QPushButton*>(QStringLiteral("tourAddRelated"))->click();
		expect(editor.selected_stop()->Related.size() == 1 && editor.selected_stop()->Related.front().First.Side == core::diff_side::left, "related base source is retained without new native write authority");
		select_range(window, core::diff_side::right, 0, 0);
		window.add_tour_stop_action()->trigger();
		editor.stop_title()->insert(QStringLiteral("Why this boundary matters"));
		editor.stop_body()->insertPlainText(QStringLiteral("A different thought in the same file."));
		const auto secondId = editor.selected_stop()->Id;
		select_file(window, "secondary.cpp");
		select_range(window, core::diff_side::right, 1, 2);
		window.add_tour_stop_action()->trigger();
		editor.stop_title()->insert(QStringLiteral("Cross-file invariant"));
		editor.stop_body()->insertPlainText(QStringLiteral("Keep this relationship intact."));
		const auto thirdId = editor.selected_stop()->Id;
		window.activateWindow();
		editor.outline()->setFocus();
		QApplication::processEvents();
		QKeyEvent moveUp{ QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier };
		QApplication::sendEvent(editor.outline(), &moveUp);
		expect(editor.shown().Draft->Stops[1].Id == thirdId && editor.selected_stop()->Id == thirdId && editor.outline()->currentRow() == 1, "keyboard reorder retains stable selected identity and independent cross-file route order");
		expect(editor.shown().Draft->Stops[0].Id == firstId && editor.shown().Draft->Stops[2].Id == secondId, "duplicate titles remain distinct stops");
		editor.outline()->setCurrentRow(0);
		wait([&] { return window.comparison_surface()->current_context() && window.comparison_surface()->current_context()->Key.File == core::file_identity_of("review.cpp") && window.comparison_surface()->selected_source_range() && window.comparison_surface()->selected_source_range()->first.Line == 2U; }, "outline returns to first stop source");
		expect(editor.stop_body()->toPlainText() == QStringLiteral("First thought.\n\nSecond thought."), "stop text and native document survive excursions");
		const auto beforeSplit = *editor.selected_stop();
		auto       splitCursor = editor.stop_body()->textCursor();
		splitCursor.setPosition(16);
		editor.stop_body()->setTextCursor(splitCursor);
		editor.findChild<QAction*>(QStringLiteral("tourSplit"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 4 && editor.selected_stop()->Id != firstId, "split creates a fresh stable stop");
		editor.outline()->setCurrentRow(0);
		editor.findChild<QAction*>(QStringLiteral("tourMerge"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 3 && editor.selected_stop()->Body == beforeSplit.Body && editor.selected_stop()->Related == beforeSplit.Related, "split then merge preserves every text byte and related source");
		editor.findChild<QAction*>(QStringLiteral("tourUndo"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 4, "structural undo restores merged stop");
		editor.findChild<QAction*>(QStringLiteral("tourUndo"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 3 && *editor.selected_stop() == beforeSplit, "structural undo restores original split text and identity");
		editor.findChild<QAction*>(QStringLiteral("tourDuplicate"))->trigger();
		const auto duplicateId = editor.selected_stop()->Id;
		expect(duplicateId != firstId && editor.selected_stop()->Title == beforeSplit.Title, "duplicate title has a new local ID");
		editor.findChild<QAction*>(QStringLiteral("tourRemove"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 3, "remove is a local structural command");
		editor.findChild<QAction*>(QStringLiteral("tourUndo"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 4, "remove is undoable");
		editor.findChild<QAction*>(QStringLiteral("tourRedo"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 3, "redo preserves accepted arrangement");
		editor.outline()->setCurrentRow(0);
		select_file(window, "secondary.cpp");
		expect(editor.selected_stop()->Id == firstId && editor.stop_body()->toPlainText().toStdString() == beforeSplit.Body, "normal file excursion retains the tour stop and editor");
		editor.findChild<QPushButton*>(QStringLiteral("tourReturn"))->click();
		wait([&] { return window.comparison_surface()->current_context() && window.comparison_surface()->current_context()->Key.File == core::file_identity_of("review.cpp") && window.comparison_surface()->selected_source_range() && window.comparison_surface()->selected_source_range()->second.Line == 4U; }, "Return to tour restores exact source range after normal file excursion");
		editor.findChild<QPushButton*>(QStringLiteral("tourComplete"))->click();
		expect(editor.shown().Draft->Stops.front().Preparation == core::tour_preparation::ready && editor.selected_stop()->Id == thirdId, "Save and next marks authored stop Ready and follows conceptual order");
		saved(editor);
		const auto beforeInsert = editor.shown().Draft->Stops;
		select_file(window, "secondary.cpp");
		editor.findChild<QAction*>(QStringLiteral("tourInsertBefore"))->trigger();
		expect(editor.shown().Draft->Stops.size() == 4 && editor.shown().Draft->Stops[2].Id == thirdId && editor.selected_stop()->Id == editor.shown().Draft->Stops[1].Id, "Insert before locates the selected stop by stable ID across copied projections");
		editor.findChild<QAction*>(QStringLiteral("tourUndo"))->trigger();
		expect(editor.shown().Draft->Stops == beforeInsert, "insert is one undoable structural edit");
		editor.outline()->setCurrentRow(1);
		saved(editor);
		const auto boundedBody = editor.stop_body()->toPlainText();
		editor.stop_body()->selectAll();
		editor.stop_body()->insertPlainText(QString(static_cast<qsizetype>(core::maximum_tour_field_bytes) + 1, QLatin1Char{ 'x' }));
		expect(editor.shown().Refusal == core::tour_refusal::bounds && editor.stop_body()->toPlainText().size() == static_cast<qsizetype>(core::maximum_tour_field_bytes) + 1, "oversized stop body remains visible and is refused without truncation");
		const auto boundedRevision = editor.shown().Draft->Revision;
		window.add_tour_stop_action()->trigger();
		editor.findChild<QAction*>(QStringLiteral("tourInsertBefore"))->trigger();
		editor.findChild<QPushButton*>(QStringLiteral("tourAddRelated"))->click();
		editor.outline()->setCurrentRow(0);
		expect(editor.shown().Draft->Revision == boundedRevision && editor.selected_stop()->Id == thirdId && editor.stop_body()->toPlainText().size() == static_cast<qsizetype>(core::maximum_tour_field_bytes) + 1 && editor.problem_editor()->isReadOnly() && editor.solution_editor()->isReadOnly(), "tour actions and other fields cannot discard a refused editor candidate");
		editor.stop_body()->undo();
		expect(editor.stop_body()->toPlainText() == boundedBody && editor.shown().Refusal == core::tour_refusal::none, "native undo recovers a refused oversized edit");
		saved(editor);
		if (argc > 1) {
			const auto render = [&](int aWidth, int aHeight, const QString& aSuffix) {
				window.resize(aWidth, aHeight);
				editor.show_stops();
				QApplication::processEvents();
				wait([&] {auto* surface=window.comparison_surface(); return surface->current_result() && surface->current_context() && surface->current_context()->Key.File==core::file_identity_of("secondary.cpp") && surface->current_semantic_index() && !surface->geometry_pending() && !surface->right_pane()->toPlainText().isEmpty(); }, "source and tour are both ready before rendering");
				auto* scroll = window.findChild<QScrollArea*>(QStringLiteral("workbenchTourScroll"));
				expect(scroll && scroll->horizontalScrollBar()->maximum() == 0 && editor.stop_body()->isVisible() && window.comparison_surface()->isVisible(), "code and explanation fit together without horizontal drawer clipping");
				expect(window.grab().save(QString::fromLocal8Bit(argv[1]) + aSuffix + QStringLiteral(".png")), "full-window source and tour render");
				std::cout << "tour-layout " << aSuffix.toStdString() << " actual=" << window.width() << "x" << window.height() << " dpr=" << window.devicePixelRatioF() << " Qt=" << QT_VERSION_STR << "\n";
			};
			render(1440, 1000, QStringLiteral("-wide"));
			render(960, 720, QStringLiteral("-narrow"));
			const auto original = editor.font();
			auto       large    = original;
			large.setPointSize(18);
			editor.setFont(large);
			render(1100, 900, QStringLiteral("-large-font"));
			editor.setFont(original);
		}
		const auto accepted = *editor.shown().Draft;
		editor.roadmap()->setCurrentIndex(editor.roadmap()->count() - 1);
		expect(editor.preview()->original_body().find(accepted.Problem) != std::string::npos && editor.preview()->original_body().find(accepted.Solution) != std::string::npos, "preview retains raw source without rendering feedback into draft");
		editor.preview()->show_source(true);
		expect(*editor.shown().Draft == accepted, "preview Source toggle is read-only");
		editor.roadmap()->setCurrentIndex(0);
		expect(editor.problem_editor()->toPlainText() == problem, "free section revisit preserves text");
		if (argc > 1) {
			window.resize(1280, 900);
			QApplication::processEvents();
			expect(window.grab().save(QString::fromLocal8Bit(argv[1])), "composed editor render");
		}
		const auto mutationAfter = access::review_mutation_evidence(*application);
		expect(mutationAfter.Submissions == mutationBefore.Submissions && mutationAfter.StoredComments == mutationBefore.StoredComments && mutationAfter.Replies == mutationBefore.Replies && mutationAfter.Resolutions == mutationBefore.Resolutions
				   && access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations == 0U,
			   "entry, editing, preview and section navigation perform zero remote mutations or agent launches");
		shutdown(*application);
		{
			expect(std::filesystem::exists(*dataRoot / "state" / "v1" / "review-ledger.sqlite3"), "real composed receipt store exists before inspection");
			auto ledger = adapters::sqlite::make_review_ledger_store({ .StorageRoot = *dataRoot, .Host = "github.com", .Account = "1001", .Repository = "octo-org/review-repository", .PullRequest = 17 });
			expect(ledger.has_value(), "read actual composed ledger after shutdown");
			for (const auto* path : { "review.cpp", "secondary.cpp" }) {
				const auto receipt = (*ledger)->load(core::file_identity_of(path));
				expect(receipt && !*receipt, "tour route minted zero durable viewed-file receipts even with automatic mark-read opted in");
			}
		}
		application.reset();
		application = make();
		open(*application);
		auto& restored = *application->window().tour_editor();
		expect(*restored.shown().Draft == accepted && restored.problem_editor()->toPlainText() == problem && restored.solution_editor()->toPlainText() == solution, "actual application reconstruction restores exact tour from admitted SQLite root");
		// Failures are already exercised at the real host/store seam. Verify the
		// native surface keeps the accepted editor and exposes explicit recovery.
		auto failure    = restored.shown();
		failure.Failure = application::tour_store_failure::unavailable;
		restored.present(failure);
		expect(restored.problem_editor()->toPlainText() == problem && restored.findChild<QLabel*>(QStringLiteral("tourSaveStatus"))->text().contains(QStringLiteral("not yet durable")), "local failure preserves source and explains durability");
		restored.present({ });
		expect(restored.problem_editor()->isReadOnly() && restored.problem_editor()->toPlainText().isEmpty(), "unavailable owner clears prior account text");
		restored.present(failure);
		// A missing product wire must be detectable: typing cannot masquerade as
		// durable success solely because a native editor displays the candidate.
		restored.set_dispatch({ }, { });
		restored.problem_editor()->insertPlainText(QStringLiteral("unsaved wire negative control"));
		expect(restored.problem_editor()->toPlainText().toStdString() != restored.shown().Draft->Problem, "removed dispatch wire is detected by accepted-state oracle");
		shutdown(*application);
		std::cout << "review-tour-composition:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "review-tour-composition:fail " << error.what() << '\n';
		shutdown(*application);
		return EXIT_FAILURE;
	}
}
