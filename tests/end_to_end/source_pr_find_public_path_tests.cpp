#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
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
	expect(surface->selection()->PreferredLine.Side == core::diff_side::left, "semantic Comment side follows result");
	tests::window_system_key(window, Qt::Key_F4);
	wait_for([&] { return surface->selection() && surface->selection()->PreferredLine.Side == core::diff_side::right && surface->right_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"); }, "physical F4 exact right result");
	render(window, QStringLiteral("pr-search-first-public"));
	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "Search shutdown quiescent");
	std::cout << "SOURCE_PR_FIND_PUBLIC_PATH_OK\n";
}
