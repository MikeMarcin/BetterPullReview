// [[BPR-1348]]. Double-clicking a file's row on Overview opens that file in
// Review, through the real composition, on the nested-folders pull request
// with Conventional Comments threads served by the fake GitHub server.
//
// The owner, 2026-10-09, in BPR-1339's look: "bug: I double clicked main.cpp
// and it opened to renderer.cpp in the review." renderer.cpp was the file open
// in Review; the course opens it the way the attended launcher did, returns to
// Overview, and double-clicks other files' rows by pointer, in a nested folder
// and at the root, then presses Return on one.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeView>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_until(const std::function<bool()>& aCondition)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	void wait_for(const std::function<bool()>& aCondition, const std::string& aMessage)
	{
		wait_until(aCondition);
		expect(aCondition(), aMessage);
	}

	/// \brief The Overview row of a file, at its first column.
	[[nodiscard]] auto overview_row(const qt::overview_file_model& aModel, const QString& aPath, const QModelIndex& aParent = { })
		-> QModelIndex
	{
		for (auto row = 0; row < aModel.rowCount(aParent); ++row) {
			const auto index = aModel.index(row, 0, aParent);
			if (aModel.rowCount(index) > 0) {
				if (const auto found = overview_row(aModel, aPath, index); found.isValid()) {
					return found;
				}
			} else if (index.data(qt::overview_file_model::path_role).toString() == aPath) {
				return index;
			}
		}
		return { };
	}

	/// \brief The pull request path of the file the comparison shows.
	[[nodiscard]] auto current_path(const qt::workbench_window& aWindow) -> QString
	{
		const auto& context    = aWindow.comparison_surface()->current_context();
		const auto& projection = aWindow.overview_page()->projection();
		if (!context || !projection) {
			return { };
		}
		for (const auto& row : projection->Files) {
			if (core::file_identity_of(row.Path) == context->Key.File) {
				return QString::fromStdString(row.Path);
			}
		}
		return { };
	}

	/// \brief A reviewer's double-click on a row's file name: press, release,
	/// the second press Qt reports as a double-click, release.
	void double_click(QAbstractItemView& aView, const QModelIndex& aIndex)
	{
		aView.scrollTo(aIndex);
		QApplication::processEvents();
		auto* const viewport = aView.viewport();
		const auto  cell     = aView.visualRect(aIndex);
		const auto  local    = QPointF{ QPoint{ cell.left() + std::min(cell.width() / 2, 40), cell.center().y() } };
		const auto  global   = QPointF{ viewport->mapToGlobal(local.toPoint()) };
		for (const auto type : { QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::MouseButtonDblClick, QEvent::MouseButtonRelease }) {
			const auto buttons = type == QEvent::MouseButtonRelease ? Qt::MouseButtons{ Qt::NoButton } : Qt::MouseButtons{ Qt::LeftButton };
			auto       event   = QMouseEvent{ type, local, global, Qt::LeftButton, buttons, Qt::NoModifier };
			QApplication::sendEvent(viewport, &event);
		}
		QApplication::processEvents();
	}

	void open_from_overview(qt::workbench_window& aWindow, const QString& aPath, const QString& aWas)
	{
		aWindow.choose_modality(qt::review_modality::overview);
		wait_for([&] { return aWindow.showing_overview(); }, "Overview shows again");
		auto* const page = aWindow.overview_page();
		const auto  row  = overview_row(*page->file_model(), aPath);
		expect(row.isValid(), "Overview lists " + aPath.toStdString());
		double_click(*page->files_view(), row);
		const auto opened = [&] { return !aWindow.showing_overview() && current_path(aWindow) == aPath; };
		wait_until(opened);
		expect(opened(), "double-clicking " + aPath.toStdString() + " on Overview opens it in Review, not " + aWas.toStdString() + " (Review shows " + current_path(aWindow).toStdString() + ")");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication  qtApplication{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-overview-double-click-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::conventional_comments_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composed startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1400, 900);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "the pull request opens on Overview");

	// The attended launcher's way in: Review, then renderer.cpp's row.
	const auto renderer = QStringLiteral("src/engine/renderer.cpp");
	wait_for([&] { return access::reveal_fixture_file(*application, renderer.toStdString()) == app::detail::fixture_file_reveal_result::shown; },
			 "renderer.cpp opens in Review");
	expect(current_path(window) == renderer, "Review shows renderer.cpp");

	// The owner's gesture: main.cpp, in src/, below the engine folder.
	const auto mainFile = QStringLiteral("src/main.cpp");
	open_from_overview(window, mainFile, renderer);
	// A file at the root, and one two folders down.
	open_from_overview(window, QStringLiteral("README.md"), mainFile);
	open_from_overview(window, QStringLiteral("src/engine/shaders/post/bloom.frag"), QStringLiteral("README.md"));

	// Return on a row opens that row too.
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return window.showing_overview(); }, "Overview shows again for Return");
	auto* const page = window.overview_page();
	const auto  row  = overview_row(*page->file_model(), mainFile);
	page->files_view()->scrollTo(row);
	page->files_view()->setCurrentIndex(row);
	page->files_view()->setFocus(Qt::OtherFocusReason);
	tests::window_system_key(*page->files_view(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
	wait_for([&] { return !window.showing_overview() && current_path(window) == mainFile; }, "Return on main.cpp's row opens it in Review");

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "composed course shuts down and drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "composed private course root cleaned");
	std::cout << "OVERVIEW_DOUBLE_CLICK_PUBLIC_PATH_OK qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << '\n';
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
