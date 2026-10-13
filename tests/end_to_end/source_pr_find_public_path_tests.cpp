#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSysInfo>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QThread>
#include <QToolButton>
#include <QTreeView>
#include <iostream>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (!aCondition() && timer.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	void settle()
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < 180) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_FIND_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "owned render root");
		expect(aWindow.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")), "full shipped composition render saved");
	}
	void query(qt::source_find_bar& aFind, const QString& aQuery)
	{
		aFind.query_editor()->setPlainText(aQuery);
		aFind.flush();
		wait_for([&] { return aFind.projection().Status != core::source_search_status::searching; }, "pane query terminal");
		settle();
	}
	void open(app::github_application& aApplication, const std::string_view aPath)
	{
		auto&      window = aApplication.window();
		const auto index  = window.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath));
		expect(index.isValid(), "fixture path in accepted catalogue");
		window.files_drawer()->contents_view()->setCurrentIndex(index);
		wait_for([&] {auto* surface=window.comparison_surface();return surface->current_result() && surface->current_context()->Key.File==core::file_identity_of(aPath) && !surface->geometry_pending(); }, "actual installed-Git file comparison");
	}
}
int main(int aArgc, char** aArgv)
{
	QApplication  applicationQt{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-source-find-XXXXXX") };
	auto          options       = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = e2e::private_data_root(temporary);
	expect(options.ApplicationDataRoot.has_value(), "private data root");
	std::unique_ptr<app::github_application> application;
	bool                                     ready   = false;
	auto                                     startup = access::start(app::detail::github_public_path_fixture::source_find_v1, options, [&](auto aResult) {ready=true;if(aResult){application=std::move(*aResult);
} });
	wait_for([&] { return ready && startup->is_quiescent(); }, "shipped composition startup");
	expect(application != nullptr, "startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);

	auto* searchAction = window.findChild<QAction*>(QStringLiteral("sourceFilesAction"));
	auto* nextAction   = window.findChild<QAction*>(QStringLiteral("sourceFilesNextAction"));
	expect(searchAction && nextAction && !searchAction->isEnabled(), "Home cannot Search retained PR");
	for (const auto name : { "sourceFindAction", "sourceFindNextAction", "sourceFindPreviousAction", "sourceFilesAction", "sourceFilesNextAction", "sourceFilesPreviousAction" }) {
		auto* owner = window.findChild<QAction*>(QString::fromUtf8(name));
		expect(owner, "all6Find commands composed");
		for (const auto& shortcut : owner->shortcuts()) {
			auto count = 0;
			for (const auto* candidate : window.findChildren<QAction*>()) {
				count += candidate->shortcuts().contains(shortcut) ? 1 : 0;
			}
			expect(count == 1, "each pane and PR shortcut has exactly one window owner");
		}
	}

	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "connect fixture");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/source-find#77"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "PR overview");
	expect(searchAction->isEnabled(), "Search available from Overview");
	auto entry  = app::detail::course_modality_step{ };
	entry.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, entry) != app::detail::course_modality_result::pending; }, "Review entry");
	window.choose_modality(qt::review_modality::review);
	settle();
	wait_for([&] { return window.files_drawer()->contents()->rowCount() > 0; }, "accepted files");
	open(*application, "src/other.cpp");
	auto*      surface = window.comparison_surface();
	const auto first   = surface->current_context()->Key;
	auto*      filter  = window.findChild<QLineEdit*>(QStringLiteral("filesFilter"));
	filter->setText(QStringLiteral("other"));
	searchAction->trigger();
	auto* panel = window.pr_search();
	expect(panel && panel->isVisible(), "Search tab composed beside Files");
	query(*panel->bar(), QStringLiteral("offscreen needle"));
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete; }, "full corpus terminal");
	expect(panel->projection().Summary.Matches == 2U, "unopened filtered file searched on both distinct blobs");
	expect(surface->current_context()->Key == first, "query never autoopens result");
	expect(filter->text() == QStringLiteral("other"), "Files filter retained independently");
	expect(panel->tree()->model()->rowCount() == 1 && panel->tree()->model()->rowCount(panel->tree()->model()->index(0, 0)) == 2, "ordered file group and both side records");
	const auto hit = panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0));
	panel->tree()->setCurrentIndex(hit);
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(panel->tree(), &enter);
	wait_for([&] { return surface->current_result() && surface->selection() && surface->current_context()->Key.File == core::file_identity_of("src/find.cpp") && surface->left_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"); }, "exact result ordinary file load and left span reveal");
	expect(surface->selection()->PreferredLine.Side == core::diff_side::left && surface->left_pane()->hasFocus(), "semantic Comment side and keyboard focus follow left result");
	tests::window_system_key(window, Qt::Key_F4);
	wait_for([&] { return surface->selection() && surface->selection()->PreferredLine.Side == core::diff_side::right && surface->right_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"); }, "physical F4 exact right result");
	expect(surface->right_pane()->hasFocus(), "F4 focuses the exact right occurrence reader");

	const auto  beforeTraversal = panel->projection().Statistics;
	const auto* comparison      = surface->current_result().get();
	for (int step = 0; step < 1000; ++step) {
		nextAction->trigger();
	}
	settle();
	expect(panel->projection().Statistics.Compilations == beforeTraversal.Compilations && panel->projection().Statistics.ReadCalls == beforeTraversal.ReadCalls && panel->projection().Statistics.Scans == beforeTraversal.Scans, "1000 PR result traversals perform no compile scan or source read");
	expect(surface->current_result().get() == comparison, "same-file PR traversal reuses ordinary accepted diff");

	filter->clear();
	wait_for([&] { return window.exclude_action()->isEnabled(); }, "private receipt action enabled on exact result");
	window.exclude_action()->trigger();
	wait_for([&] { return !window.files_drawer()->contents()->index_for_file(core::file_identity_of("src/find.cpp")).isValid(); }, "private excluded file withheld from ordinary Files order");
	expect(panel->projection().Summary.Matches == 2U, "private receipt cannot shrink full PR search corpus");
	nextAction->trigger();
	settle();
	expect(surface->current_context()->Key.File == core::file_identity_of("src/find.cpp") && !window.files_drawer()->contents()->index_for_file(core::file_identity_of("src/find.cpp")).isValid(), "result reveal retains excluded receipt");
	window.exclude_action()->trigger();
	wait_for([&] { return window.files_drawer()->contents()->index_for_file(core::file_identity_of("src/find.cpp")).isValid(); }, "explicit receipt restoration");

	surface->right_pane()->setFocus();
	window.findChild<QAction*>(QStringLiteral("sourceFindAction"))->trigger();
	query(*surface->source_find(), QStringLiteral("needle"));
	expect(surface->source_find()->projection().Summary.Matches > 0, "pane query independently searches retained source");
	const auto paneQuery = surface->source_find()->query();
	panel->tree()->setCurrentIndex(panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0)));
	QApplication::sendEvent(panel->tree(), &enter);
	wait_for([&] { return surface->left_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"); }, "PR exact span selected while pane decorations own source");
	settle();
	expect(surface->source_find()->isVisible() && surface->source_find()->query() == paneQuery && panel->bar()->query() == "offscreen needle" && surface->left_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"), "pane publication never overwrites external occurrence or independent queries");
	surface->source_find()->findChild<QToolButton*>(QStringLiteral("sourceFindClose"))->click();

	surface->right_pane()->setFocus();
	tests::window_system_key(window, Qt::Key_Home, Qt::ControlModifier);
	tests::window_system_key(window, Qt::Key_Down);
	settle();
	const auto ordinaryReading = surface->selection();
	const auto pendingLocation = *panel->projection().Matches->front().Match.Locations[0];
	expect(ordinaryReading && ordinaryReading->PreferredLine.Side == core::diff_side::right && ordinaryReading->PreferredLine.Line != pendingLocation.Line, "race starts from a distinct ordinary reading position");
	filter->clear();
	open(*application, "src/other.cpp");
	panel->tree()->setCurrentIndex(panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0)));
	QApplication::sendEvent(panel->tree(), &enter);
	auto*      files = window.files_drawer()->contents_view();
	const auto other = window.files_drawer()->contents()->index_for_file(core::file_identity_of("src/other.cpp"));
	files->setCurrentIndex({ });
	files->setCurrentIndex(other);
	wait_for([&] { return surface->current_result() && surface->current_context()->Key.File == core::file_identity_of("src/other.cpp") && !surface->geometry_pending(); }, "later ordinary file navigation defeats queued reveal");
	open(*application, "src/find.cpp");
	settle();
	std::cout << "REVEAL_RACE before=" << (ordinaryReading ? static_cast<int>(ordinaryReading->PreferredLine.Side) : -1) << ":" << (ordinaryReading ? ordinaryReading->PreferredLine.Line : 0) << " after=" << (surface->selection() ? static_cast<int>(surface->selection()->PreferredLine.Side) : -1) << ":" << (surface->selection() ? surface->selection()->PreferredLine.Line : 0) << '\n'
			  << std::flush;
	expect(!surface->selection() || surface->selection()->PreferredLine.Line != pendingLocation.Line || surface->selection()->PreferredLine.Side != pendingLocation.Side, "cancelled reveal cannot revive after returning to its old file");

	window.compose_review_action()->trigger();
	auto* draft = window.review_submission_editor_view()->body_editor();
	wait_for([&] { return draft->isVisible() && !draft->isReadOnly(); }, "real PR composer");
	draft->setPlainText(QStringLiteral("unsent search course text"));
	draft->setFocus();
	settle();
	const auto beforeEditor = panel->projection().CurrentMatch;
	tests::window_system_key(window, Qt::Key_F4);
	expect(window.current_modality() == qt::review_modality::discuss && draft->toPlainText() == QStringLiteral("unsent search course text") && panel->projection().CurrentMatch == beforeEditor, "F4 cannot hijack unrelated composer");
	tests::window_system_key(window, Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier);
	wait_for([&] { return window.current_modality() == qt::review_modality::review && panel->bar()->query_editor()->hasFocus(); }, "physical Find Files enters Review from composer");
	expect(draft->toPlainText() == QStringLiteral("unsent search course text"), "Find Files retains unsent review body");
	QDialog modal{ &window };
	modal.setWindowModality(Qt::ApplicationModal);
	modal.show();
	settle();
	expect(!searchAction->isEnabled() && !nextAction->isEnabled(), "modal owns all PR search keys");
	modal.close();
	window.activateWindow();
	settle();
	panel->bar()->query_editor()->setFocus();
	settle();
	expect(panel->bar()->query_editor()->hasFocus(), "Search owns focus before Escape");
	const auto retainedCount = panel->projection().Summary.Matches;
	tests::window_system_key(window, Qt::Key_Escape);
	expect(surface->isAncestorOf(QApplication::focusWidget()) && qobject_cast<QPlainTextEdit*>(QApplication::focusWidget()), "Search Escape returns to source");
	expect(panel->projection().Summary.Matches == retainedCount, "Escape retains PR results");
	window.resize(860, 760);
	settle();
	render(window, QStringLiteral("pr-search-narrow"));
	window.toggle_drawers_action()->trigger();
	searchAction->trigger();
	settle();
	render(window, QStringLiteral("pr-search-narrow-focused-drawer"));
	expect(panel->bar()->query_editor()->width() > 80 && surface->width() > 160, "narrow native layout retains query and usable source");
	for (auto* toggle : panel->bar()->findChildren<QToolButton*>()) {
		if (toggle->isVisible()) {
			expect(!toggle->accessibleName().isEmpty(), "all visible native search buttons labelled");
			expect(panel->bar()->rect().contains(QRect{ toggle->mapTo(panel->bar(), QPoint{ }), toggle->size() }), "native second row fits narrow drawer");
		}
	}
	window.resize(1280, 860);
	settle();
	render(window, QStringLiteral("pr-search-first-public"));
	panel->bar()->query_editor()->setFocus();
	panel->bar()->query_editor()->setPlainText(QStringLiteral("needle"));
	tests::window_system_key(window, Qt::Key_Escape);
	wait_for([&] { return panel->projection().Query == "needle" && panel->projection().Status == core::source_search_status::complete; }, "Escape leaves pending typing search alive");
	panel->bar()->query_editor()->setPlainText(QStringLiteral("pending close query"));
	panel->bar()->findChild<QToolButton*>(QStringLiteral("sourceFindClose"))->click();
	wait_for([&] { return application->status().SourceSearchQuiescent; }, "explicit Close cancels pending debounce and work");
	settle();
	expect(!panel->projection().Open && !panel->isVisible() && filter->text().isEmpty(), "Close restores Files tab and retains its independent filters");

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "Search shutdown quiescent");
	std::cout << "SOURCE_PR_FIND_PUBLIC_PATH_OK\n";
}
