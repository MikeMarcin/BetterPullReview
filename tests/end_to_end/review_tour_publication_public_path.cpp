#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "cli_publication_course.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include "bpr/core/review_tour_markdown.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
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
#include <QTimer>
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
	auto              make             = [&] {std::unique_ptr<app::github_application> started;bool terminal=false;auto options=app::detail::github_public_path_fixture_options{.ApplicationDataRoot=currentDataRoot,.ReviewSubmissionServer=server,.RepeatReadSessions=true};cli.configure(options);auto startup=access::start(app::detail::github_public_path_fixture::profile_window_restoration_v1,options,[&](auto aResult){terminal=true;if(aResult){started=std::move(*aResult);
}});wait([&]{return terminal && startup && startup->is_quiescent();},"publication app starts");expect(started!=nullptr,"publication startup accepted");return started; };
	const std::string originalTemplate = "# Existing template\r\n\r\nKeep [the ticket](https://example.invalid/123) and this checklist.\r\n- [ ] Original check\r\n";
	if (mode != "description") {
		server->StagedDescription = originalTemplate;
	}
	server->DraftPullRequest = mode == "manual";
	server->StagedQualified  = mode != "unsupported";
	auto application         = make();
	try {
		open(*application, cli);
		auto&      window = application->window();
		auto&      editor = *window.tour_editor();
		const auto button = [&](const char* aName) {auto* value=editor.findChild<QPushButton*>(QString::fromUtf8(aName));expect(value!=nullptr,"normal publication control exists");return value; };
		auto*      status = editor.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"));
		wait([&] { return status->text().contains(QStringLiteral("Saved locally")); }, "read-only recovery discovery completes");
		button("tourPreparePublication")->click();
		expect(!button("tourPublish")->isEnabled(), "unfinished draft cannot publish");
		editor.problem_editor()->setPlainText(QStringLiteral("Avoid losing the original explanation."));
		editor.solution_editor()->setPlainText(QStringLiteral("Keep exact sources and a readable route."));
		select_file(window, "review.cpp");
		select_range(window, core::diff_side::right, 2, 4);
		window.add_tour_stop_action()->trigger();
		editor.stop_title()->setText(QStringLiteral("Capture the original"));
		editor.stop_body()->setPlainText(QStringLiteral("Retain this exact source interval."));
		select_file(window, "secondary.cpp");
		select_range(window, core::diff_side::right, 1, 3);
		window.add_tour_stop_action()->trigger();
		editor.stop_title()->setText(QStringLiteral("Use the retained source"));
		editor.stop_body()->setPlainText(QStringLiteral("Connect the explanation to the original decision."));
		saved(editor);
		if (mode == "manual") {
			editor.outline()->setCurrentRow(1);
			editor.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(editor);
			expect(editor.shown().Draft->Stops.front().Title == "Use the retained source", "manual route reorder precedes publication");
			wait([&] {auto* summary=window.findChild<QLabel*>(QStringLiteral("overviewSummary"));return summary && summary->text().contains("Draft."); }, "actual provider snapshot renders draft PR status");
		}
		editor.roadmap()->setCurrentIndex(editor.roadmap()->count() - 1);
		if (mode == "compat") {
			editor.findChild<QComboBox*>(QStringLiteral("tourPublicationMode"))->setCurrentIndex(1);
		}
		editor.findChild<QCheckBox*>(QStringLiteral("tourDescriptionEdit"))->setChecked(true);
		button("tourPreparePublication")->click();
		if (mode == "unsupported") {
			expect(!button("tourPublish")->isEnabled() && status->text().contains(QStringLiteral("unavailable")), "unqualified batching cannot authorize publication");
			editor.findChild<QComboBox*>(QStringLiteral("tourPublicationMode"))->setCurrentIndex(1);
			button("tourPreparePublication")->click();
			expect(!button("tourPublish")->isEnabled(), "explicit fallback choice cannot manufacture operation support");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 0, "unsupported preview performs zero writes");
			}
			shutdown(*application);
			std::cout << "review-tour-publication:unsupported:pass\n";
			return EXIT_SUCCESS;
		}
		expect(button("tourPublish")->isEnabled(), "complete qualified route exposes Publish only after preview");
		if (argc > (cliMode ? 3 : 2)) {
			expect(window.grab().save(QString::fromUtf8(argv[cliMode ? 3 : 2])), "publication preview capture saved");
		}
		auto* preview = editor.publication_preview();
		expect(preview->original_body().contains("Separate optional description edit") && preview->original_body().contains("Actor:") && preview->original_body().contains("Head:"), "preview freezes identity and separate description scope");
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
				button("tourPreparePublication")->click();
			}
			{
				std::scoped_lock lock{ server->Mutex };
				server->CliAcquisitionFailure.reset();
			}
		}
		button("tourPublish")->click();
		if (mode == "stop") {
			wait([&] {std::scoped_lock lock{server->Mutex};return server->StagedBlocked; }, "production session reaches first durable staged effect");
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
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == before, "reopening and reconciliation emit zero writes");
			}
			auto* prepare = restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"));
			prepare->click();
			auto* resume = restored.findChild<QPushButton*>(QStringLiteral("tourResume"));
			expect(resume->isEnabled(), "captured-plan preview authorizes explicit Resume control");
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
			wait([&] { return restoredStatus->text().startsWith(QStringLiteral("Published:")); }, "Resume completes the original pending review");
		} else if (mode == "conflict") {
			wait([&] { return status->text().contains(QStringLiteral("Publication conflict")); }, "existing pending conflict is visible");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedWrites == 0, "existing website pending review is neither adopted nor modified");
		} else if (mode == "description") {
			wait([&] { return status->text().contains(QStringLiteral("Tour published; description update needs attention")); }, "description failure is separate from confirmed tour success");
			button("tourReconcile")->click();
			wait([&] { return !button("tourStop")->isEnabled(); }, "description reconciliation completes read-only");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedCreates == 1 && server->StagedSubmits == 1 && server->StagedWrites == 4 && !server->StagedDescription, "description failure retains submitted review without reposting");
		} else {
			wait([&] { return status->text().startsWith(mode == "compat" ? QStringLiteral("Published as compatibility") : QStringLiteral("Published:")); }, "normal Publish reaches verified success");
		}
		if (mode != "conflict" && mode != "description") {
			std::scoped_lock lock{ server->Mutex };
			if (mode == "compat") {
				expect(server->StagedCreates == 0 && server->StagedSubmits == 0 && server->CompatibilityComments.size() == 3 && server->StagedWrites == 6, "explicit compatibility emits three visible comments and two link edits, then description");
			} else {
				expect(server->StagedCreates == 1 && server->StagedSubmits == 1 && server->StagedComments.size() == 2 && server->StagedWrites == 5, "native app submits exactly one review after two navigation patches");
			}
			expect(server->StagedDescription && server->StagedDescription->contains(mode == "compat" ? "#issuecomment-703" : "#pullrequestreview-501"), "description uses actual returned index link only after publication");
			expect(server->StagedDescription->contains(originalTemplate), "first publication preserves original template bytes including CRLF");
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
			cold.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(!cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "unchanged cold reimport cannot publish normalization edits");
			cold.problem_editor()->setPlainText("Cold-profile deliberate revision.");
			saved(cold);
			cold.findChild<QCheckBox*>(QStringLiteral("tourDescriptionEdit"))->setChecked(true);
			cold.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "cold-profile author can explicitly edit the existing tour");
			cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->click();
			wait([&] { return cold.findChild<QLabel*>(QStringLiteral("tourPublicationStatus"))->text().startsWith("Published:"); }, "cold-profile body update preserves imported actor and source binding");
			saved(cold);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 7 && server->StagedCreates == 1 && server->StagedSubmits == 1 && server->StagedDescription->starts_with("Website template preface.") && server->StagedDescription->ends_with("Keep website template tail.\n"), "two narrow cold-profile edits preserve template and every comment identity");
			}
			const auto originalStops = cold.shown().Draft->Stops;
			expect(access::reaccept_fixture_revision(*application, true), "changed-head fixture enters actual accepted-revision path");
			wait([&] { return cold.shown().Draft && cold.shown().Draft->ObservedHead && cold.shown().Draft->Stops.front().Preparation == core::tour_preparation::needs_attention; }, "changed head exposes explanation attention");
			cold.outline()->setCurrentRow(0);
			cold.findChild<QPushButton*>(QStringLiteral("tourReaffirm"))->click();
			saved(cold);
			expect(cold.shown().Draft->Stops.front().ReaffirmedHead == cold.shown().Draft->ObservedHead && cold.shown().Draft->Stops.front().Remote == originalStops.front().Remote && cold.shown().Draft->Stops.front().Destination == originalStops.front().Destination, "explicit reaffirmation preserves original imported subject and comment");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->StagedWrites == 7, "head attention and reaffirmation issue zero remote writes");
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

			activeEditor.outline()->setCurrentRow(1);
			activeEditor.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(activeEditor);
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
			activeButton("tourPreparePublication")->click();
			expect(!activeButton("tourPublish")->isEnabled() && activeStatus->text().contains("No published changes"), "equal-body update offers no writes");
			select_file(activeWindow, "review.cpp");
			select_range(activeWindow, core::diff_side::right, 2, 4);
			activeWindow.add_tour_stop_action()->trigger();
			activeEditor.stop_title()->setText("Later batch insertion");
			activeEditor.stop_body()->setPlainText("A later idea preserves the original discussion.");
			saved(activeEditor);
			activeEditor.findChild<QAction*>(QStringLiteral("tourMoveUp"))->trigger();
			saved(activeEditor);
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
				cold.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
				expect(!cold.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "cold maintained route offers no normalization writes");
				cold.outline()->setCurrentRow(0);
				cold.findChild<QPushButton*>(QStringLiteral("tourReturn"))->click();
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
			restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(!restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "published draft cannot offer a duplicate publication");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedWrites == (mode == "compat" ? 6 : 5), "completed restart and preview perform zero writes");
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
