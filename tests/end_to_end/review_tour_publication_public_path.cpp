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
	const std::string mode = argc > 1 ? argv[1] : "native";
	QTemporaryDir     data;
	const auto        dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		return EXIT_FAILURE;
	}
	auto server             = std::make_shared<app::detail::review_submission_fixture_state>();
	auto make               = [&] {std::unique_ptr<app::github_application> started;bool terminal=false;auto startup=access::start(app::detail::github_public_path_fixture::profile_window_restoration_v1,app::detail::github_public_path_fixture_options{.ApplicationDataRoot=dataRoot,.ReviewSubmissionServer=server},[&](auto result){terminal=true;if(result){started=std::move(*result);
}});wait([&]{return terminal && startup && startup->is_quiescent();},"publication app starts");expect(started!=nullptr,"publication startup accepted");return started; };
	server->StagedQualified = mode != "unsupported";
	auto application        = make();
	try {
		open(*application);
		auto&      window = application->window();
		auto&      editor = *window.tour_editor();
		const auto button = [&](const char* name) {auto* value=editor.findChild<QPushButton*>(QString::fromUtf8(name));expect(value!=nullptr,"normal publication control exists");return value; };
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
		if (argc > 2) {
			expect(window.grab().save(QString::fromUtf8(argv[2])), "publication preview capture saved");
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
			open(*application);
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
		}
		if (mode == "native" || mode == "compat") {
			saved(*application->window().tour_editor());
			shutdown(*application);
			application.reset();
			application = make();
			open(*application);
			auto& restored = *application->window().tour_editor();
			wait([&] {const auto& draft=restored.shown().Draft;return draft && draft->Remote && draft->Stops.size()==2 && draft->Stops[0].Remote && draft->Stops[1].Remote; }, "completed publication attribution survives app restart");
			restored.findChild<QPushButton*>(QStringLiteral("tourPreparePublication"))->click();
			expect(!restored.findChild<QPushButton*>(QStringLiteral("tourPublish"))->isEnabled(), "published draft cannot offer a duplicate publication");
			std::scoped_lock lock{ server->Mutex };
			expect(server->StagedWrites == (mode == "compat" ? 6 : 5), "completed restart and preview perform zero writes");
		}
		shutdown(*application);
		std::cout << "review-tour-publication:" << mode << ":pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "review-tour-publication:fail " << error.what() << '\n';
		shutdown(*application);
		return EXIT_FAILURE;
	}
}
