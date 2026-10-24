#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QHoverEvent>
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
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QThread>
#include <QToolButton>
#include <QTreeView>
#include <cmath>
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
	void click_tab(QTabBar& aTabs, const int aIndex)
	{
		const auto  centre = aTabs.tabRect(aIndex).center();
		QMouseEvent press{ QEvent::MouseButtonPress, centre, aTabs.mapToGlobal(centre), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QMouseEvent release{ QEvent::MouseButtonRelease, centre, aTabs.mapToGlobal(centre), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aTabs, &press);
		QApplication::sendEvent(&aTabs, &release);
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
	const auto hit      = panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0));
	const auto rightHit = panel->tree()->model()->index(1, 0, panel->tree()->model()->index(0, 0));
	expect(panel->match_columns(hit).Tags.left() == 4 && panel->match_columns(rightHit).Tags.left() == 4, "both result marker columns begin at the drawer viewport edge");
	render(window, QStringLiteral("pr-search-left-aligned-markers"));
	const auto  marker       = QPointF{ panel->match_columns(hit).Tags.left() + 2.0, static_cast<double>(panel->tree()->visualRect(hit).center().y()) };
	const auto  globalMarker = QPointF{ panel->tree()->viewport()->mapToGlobal(marker.toPoint()) };
	QMouseEvent markerPress{ QEvent::MouseButtonPress, marker, globalMarker, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
	QMouseEvent markerRelease{ QEvent::MouseButtonRelease, marker, globalMarker, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
	QApplication::sendEvent(panel->tree()->viewport(), &markerPress);
	QApplication::sendEvent(panel->tree()->viewport(), &markerRelease);
	wait_for([&] { return surface->current_result() && surface->selection() && surface->current_context()->Key.File == core::file_identity_of("src/find.cpp") && surface->left_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"); }, "left marker click opens the exact file and left occurrence");
	panel->tree()->setCurrentIndex(panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0)));
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

	// [[BPR-1509]]. Both drawer headers use compact native tabs; switching
	// pages retains the file/filter and the independent Search session.
	auto* const fileSearch = window.findChild<QTabWidget*>(QStringLiteral("fileSearchTabs"));
	expect(fileSearch && fileSearch->count() == 2, "Files and Search share the existing drawer");
	auto* const tabs            = fileSearch->tabBar();
	const auto  retainedFile    = surface->current_context()->Key;
	const auto  retainedSearch  = panel->projection().Matches;
	const auto  retainedMatch   = panel->projection().CurrentMatch;
	const auto  retainedSummary = panel->summary_text();
	const auto  retainedWidth   = window.files_drawer()->width();
	expect(tabs->tabRect(1).right() < fileSearch->width() - 20, "native tab labels leave room across the header");
	expect(std::abs(tabs->mapTo(&window, QPoint{ }).y() - window.tray_tabs()->mapTo(&window, QPoint{ }).y()) <= 2, "Files/Search and Comments/Notes start at the same drawer header edge");
	click_tab(*tabs, 0);
	expect(!panel->isVisible() && filter->isVisible() && filter->text() == QStringLiteral("other"), "mouse Files switch restores its retained filter");
	render(window, QStringLiteral("drawer-tabs-files"));
	auto* const accessibleTabs = QAccessible::queryAccessibleInterface(tabs);
	expect(accessibleTabs && accessibleTabs->role() == QAccessible::PageTabList && accessibleTabs->child(0)->text(QAccessible::Name) == QStringLiteral("Files") && accessibleTabs->child(1)->text(QAccessible::Name) == QStringLiteral("Search"), "native tab accessibility retains the page names");
	tabs->setFocus(Qt::TabFocusReason);
	expect(tabs->hasFocus(), "keyboard focus reaches the Files/Search header");
	render(window, QStringLiteral("drawer-tabs-files-focus"));
	tests::window_system_key(window, Qt::Key_Right);
	settle();
	render(window, QStringLiteral("drawer-tabs-search-focus"));
	expect(panel->bar()->query_editor()->hasFocus() && panel->isVisible() && accessibleTabs->child(1)->state().selected, "Right switches to Search with its existing query focus and selected accessibility");
	const auto  hoverPoint = tabs->tabRect(0).center();
	QHoverEvent hover{ QEvent::HoverMove, hoverPoint, tabs->mapToGlobal(hoverPoint), QPointF{ -1, -1 } };
	QApplication::sendEvent(tabs, &hover);
	render(window, QStringLiteral("drawer-tabs-files-hover"));
	QHoverEvent leave{ QEvent::HoverLeave, QPointF{ -1, -1 }, tabs->mapToGlobal(hoverPoint), hoverPoint };
	QApplication::sendEvent(tabs, &leave);
	tabs->setFocus(Qt::TabFocusReason);
	tests::window_system_key(window, Qt::Key_Left);
	settle();
	expect(fileSearch->currentIndex() == 0 && !panel->isVisible(), "Left switches back to Files");
	fileSearch->setTabEnabled(1, false);
	expect(!tabs->isTabEnabled(1), "Search tab disabled for native navigation");
	tests::window_system_key(window, Qt::Key_Right);
	expect(fileSearch->currentIndex() == 0 && !panel->isVisible(), "keyboard skips disabled tabs");
	render(window, QStringLiteral("drawer-tabs-search-disabled"));
	fileSearch->setTabEnabled(1, true);
	click_tab(*tabs, 1);
	expect(panel->isVisible() && surface->current_context()->Key == retainedFile && filter->text() == QStringLiteral("other") && window.files_drawer()->width() == retainedWidth, "tab switching retains file selection, Files filter and drawer width");
	expect(panel->projection().Query == "offscreen needle" && panel->projection().Matches == retainedSearch && panel->projection().CurrentMatch == retainedMatch && panel->summary_text() == retainedSummary, "tab switching retains Search query, exact results, selection and counts");
	render(window, QStringLiteral("drawer-tabs-search"));

	// [[BPR-1504]]. The side chips filter the rows, the counts and F4.
	auto* leftChip  = panel->findChild<QAbstractButton*>(QStringLiteral("sourceSearchSideLeft"));
	auto* rightChip = panel->findChild<QAbstractButton*>(QStringLiteral("sourceSearchSideRight"));
	expect(leftChip && rightChip && leftChip->isChecked() && rightChip->isChecked(), "both side chips start on");
	leftChip->click();
	settle();
	expect(panel->tree()->model()->rowCount(panel->tree()->model()->index(0, 0)) == 1 && panel->summary_text() == QStringLiteral("1 match in 1 file"), "Right only lists and counts the right occurrence");
	for (int step = 0; step < 3; ++step) {
		tests::window_system_key(window, Qt::Key_F4);
		wait_for([&] { return surface->selection() && surface->selection()->PreferredLine.Side == core::diff_side::right && surface->right_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"); }, "F4 under Right only stays on the right occurrence");
		expect(panel->projection().CurrentMatch && panel->projection().Matches->at(*panel->projection().CurrentMatch).Match.Locations[1], "current match is a shown right record");
	}
	window.resize(900, 760);
	settle();
	render(window, QStringLiteral("pr-search-900-right-only"));
	rightChip->click();
	expect(rightChip->isChecked(), "the last side on stays on");
	leftChip->click();
	rightChip->click();
	settle();
	expect(panel->summary_text() == QStringLiteral("1 match in 1 file") && panel->projection().CurrentMatch && panel->projection().Matches->at(*panel->projection().CurrentMatch).Match.Locations[0], "Left only moves the hidden current match to a shown left one");
	rightChip->click();
	settle();
	expect(panel->summary_text() == QStringLiteral("2 matches in 1 file"), "both sides restore every result");
	render(window, QStringLiteral("pr-search-900"));
	window.resize(1280, 860);
	settle();

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

	// [[BPR-1712]]. A result's menu opens it in a new tab: a tab of its
	// own after the active one, made active and shown at the result's line,
	// with the results kept and nothing searched again.
	{
		open(*application, "src/other.cpp");
		auto* const strip  = window.document_tabs();
		const auto  active = [&]() -> std::optional<qt::document_tab_presentation> {
            const auto found=std::ranges::find_if(strip->tabs(),&qt::document_tab_presentation::Active);
            return found==strip->tabs().end()?std::nullopt:std::optional{*found}; };
		wait_for([&] { return active() && active()->Path == QStringLiteral("src/other.cpp"); }, "the active tab shows src/other.cpp");
		const auto before  = strip->tabs().size();
		const auto origin  = active()->Id;
		const auto results = panel->projection().Matches;
		const auto scans   = panel->projection().Statistics.Scans;
		auto*      tree    = panel->tree();
		const auto deepHit = tree->model()->index(1, 0, tree->model()->index(0, 0));
		expect(deepHit.data(qt::source_search_sides_role).toInt() == 2 && deepHit.data(qt::source_search_line_role).toString() == QStringLiteral("1103"), "the right result is on line 1,103 of 1,107");
		tree->scrollTo(deepHit);
		const auto        point = tree->visualRect(deepHit).center();
		QContextMenuEvent context{ QContextMenuEvent::Mouse, point, tree->viewport()->mapToGlobal(point) };
		QApplication::sendEvent(tree->viewport(), &context);
		auto* const menu = tree->findChild<QMenu*>(QStringLiteral("sourceSearchResultMenu"));
		expect(menu != nullptr && menu->isVisible(), "right-click on a result opens its menu");
		QAction* newTab = nullptr;
		for (auto* action : menu->actions()) {
			newTab = action->objectName() == QStringLiteral("sourceSearchOpenInNewTab") ? action : newTab;
		}
		expect(newTab != nullptr && newTab->isEnabled() && newTab->text() == QStringLiteral("Open in New Tab"), "the composed menu offers Open in New Tab");
		// Chosen with a click, as a hand does: the menu closes, then the item acts.
		tests::window_system_clicks(*menu, QPointF{ menu->actionGeometry(newTab).center() }, 1);
		wait_for([&] { return strip->tabs().size() == before + 1U && active() && active()->Id != origin && surface->current_result() && surface->current_context()->Key.File == core::file_identity_of("src/find.cpp") && !surface->geometry_pending() && surface->right_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"); }, "Open in New Tab opens the result's file in a new active tab with the match selected");
		settle();
		const auto originIndex = strip->index_of(origin);
		const auto addedIndex  = strip->index_of(active()->Id);
		expect(originIndex && addedIndex && *addedIndex == *originIndex + 1 && strip->tabs()[static_cast<std::size_t>(*originIndex)].Path == QStringLiteral("src/other.cpp") && active()->Path == QStringLiteral("src/find.cpp"), "the new tab follows the one it came from, which keeps its file");
		auto* const pane   = surface->right_pane();
		const auto  cursor = pane->cursorRect();
		std::cout << "NEW_TAB_REVEAL block=" << pane->textCursor().blockNumber() << " cursor=" << cursor.top() << " x=" << cursor.left() << " cursor_width=" << cursor.width() << " viewport_width=" << pane->viewport()->width()
				  << " selected=" << pane->textCursor().selectedText().toStdString() << " horizontal=" << pane->horizontalScrollBar()->value()
				  << " viewport=" << pane->viewport()->height() << '\n'
				  << std::flush;
		expect(pane->textCursor().selectedText() == QStringLiteral("offscreen needle") && pane->viewport()->rect().contains(cursor.center()), "the result's line is in view in the new tab");
		expect(surface->selection() && surface->selection()->PreferredLine.Side == core::diff_side::right && surface->selection()->PreferredLine.Line == 1102U && window.focusWidget() == pane, "the new tab reads the right side at the result's line, with the window's keyboard there");
		expect(panel->projection().Matches == results && panel->projection().Statistics.Scans == scans, "the results stay and nothing is searched again");
		expect(panel->projection().CurrentMatch == std::optional<std::uint32_t>{ 1U }, "the opened result is the current one");
		render(window, QStringLiteral("pr-search-open-in-new-tab"));
		// Offscreen, no window is active once the popup has gone.
		window.activateWindow();
		settle();
	}

	const auto composedFrom = window.current_modality();
	window.compose_review_action()->trigger();
	auto* draft = window.review_submission_editor_view()->body_editor();
	wait_for([&] { return draft->isVisible() && !draft->isReadOnly(); }, "real PR composer");
	draft->setPlainText(QStringLiteral("unsent search course text"));
	draft->setFocus();
	settle();
	const auto beforeEditor = panel->projection().CurrentMatch;
	tests::window_system_key(window, Qt::Key_F4);
	expect(window.current_modality() == composedFrom && draft->toPlainText() == QStringLiteral("unsent search course text") && panel->projection().CurrentMatch == beforeEditor, "F4 cannot hijack unrelated composer");
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
