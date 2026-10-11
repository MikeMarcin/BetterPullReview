// [[BPR-1336]]. The comparison's zoom through the real composition: the View
// menu's commands and ⌘-scroll, the profile's appearance host and store, both
// panes' font, the gutter and the reader's line, on the 1557-line file of the
// constructed inline-drawer pull request served by the fake GitHub server.
//
// The owner, 2026-10-08: "I think we should have a more accessible zoom/text
// size for the diff pane". `--attended` opens the same file and waits for the
// owner to try the zoom.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	[[nodiscard]] auto open_path(const qt::workbench_window& aWindow) -> QString
	{
		const auto& context = aWindow.comparison_surface()->current_context();
		if (!context) {
			return { };
		}
		const auto* files = aWindow.files_drawer()->contents();
		return files->data(files->index_for_file(context->Key.File), Qt::DisplayRole).toString();
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	const auto    attended = aArgc > 1 && std::string_view{ aArgv[1] } == "--attended";
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-diff-zoom-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::inline_drawer_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composed startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/inline-drawer#22"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "the pull request opens on Overview");
	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true, .ConversationsDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
	expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and the Files drawer");

	// The long file, on its first change.
	auto* const surface = window.comparison_surface();
	auto* const files   = window.files_drawer()->contents();
	wait_for([&] { return files->rowCount() > 2; }, "the pull request's files are listed");
	auto* const view = window.files_drawer()->contents_view();
	for (int row = 0; row < view->model()->rowCount(); ++row) {
		const auto index = view->model()->index(row, 0);
		if (index.data().toString().endsWith(QStringLiteral("conversation_cards_view.cpp"))) {
			view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			break;
		}
	}
	wait_for([&] { return open_path(window).endsWith(QStringLiteral("conversation_cards_view.cpp")); }, "choosing the long file in Files opens it");
	expect(open_path(window).endsWith(QStringLiteral("conversation_cards_view.cpp")), "the 1557-line file is open");
	wait_for([&] { return surface->current_result() && !surface->geometry_pending(); }, "its comparison is measured");

	if (attended) {
		std::cout << "DIFF_ZOOM_ATTENDED_READY source=synthetic fixture=inline-drawer-v1 file=" << open_path(window).toStdString() << '\n'
				  << std::flush;
		const auto result = QApplication::exec();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "attended shutdown drains");
		application.reset();
		expect(e2e::remove_course_root(temporary), "attended private root cleaned");
		return result;
	}

	// The reader's line: the change the file opened on, wherever the reveal put it.
	// [[BPR-1383]]. A Discuss comment opens inline in Review through the
	// selected Git/cache comparison, with Comments closed and Notes selected.
	window.conversations_drawer_toggle_action()->trigger();
	wait_for([&] { return window.conversations_drawer()->state() == core::workbench_drawer_state::collapsed; }, "Comments closes before the Discuss pick");
	window.tray_tabs()->setCurrentIndex(1);
	window.choose_modality(qt::review_modality::discuss);
	QApplication::processEvents();
	auto* const notes  = window.conversation_panel()->cards();
	const auto  target = std::ranges::find_if(notes->cards(), [](const qt::thread_card& aCard) {
		return aCard.Path.endsWith(QStringLiteral("conversation_cards_view.cpp")) && aCard.Line && aCard.Side && !aCard.Outdated && !aCard.AnchorUnavailable;
	});
	expect(target != notes->cards().end() && !target->Posts.empty(), "Discuss offers a current inline thread");
	const auto thread = *target;
	notes->open_thread(thread.Id);
	QApplication::processEvents();
	auto* const lineButton = notes->open_card()->findChild<QToolButton*>(QStringLiteral("threadShowLine"));
	expect(lineButton != nullptr && lineButton->isEnabled(), "the Discuss thread offers its line button");
	lineButton->click();
	QApplication::processEvents();
	expect(window.current_modality() == qt::review_modality::review && window.conversations_drawer()->isVisible() && window.tray_tabs()->currentIndex() == 0,
		   "Discuss Open in Review opens the Comments drawer and its Comments tab");
	wait_for([&] {
		return surface->current_context() && surface->current_context()->Key.File == core::file_identity_of(thread.Path.toStdString())
			&& surface->current_result() && !surface->geometry_pending() && notes->open_thread_id() == thread.Id
			&& notes->picked_post() == thread.Posts.front().Id && surface->conversation_anchor();
	},
			 "Discuss opens the exact inline thread after its comparison is ready");
	expect(surface->conversation_anchor()->Side == *thread.Side && surface->conversation_anchor()->Line + 1U == *thread.Line,
		   "the inline conversation is on the thread's exact source line");
	const auto artifactRoot = qEnvironmentVariable("BPR_SETUP_ARTIFACT_ROOT");
	if (!artifactRoot.isEmpty()) {
		expect(QDir{ }.mkpath(artifactRoot) && window.grab().save(artifactRoot + QStringLiteral("/discuss-open-in-review.png")), "the full composed inline Review render is saved");
	}

	auto* const right    = surface->right_pane();
	const auto  selected = surface->selection();
	expect(selected.has_value(), "the first change is selected");
	const auto line     = selected->PreferredLine;
	const auto lineTop  = [&] { return surface->line_rect(line.Side, line.Line).top(); };
	const auto before   = lineTop();
	const auto size     = right->font().pointSizeF();
	const auto codeSize = window.appearance_settings()->code_size_editor()->value() / 100.0;
	expect(std::abs(size - codeSize) < 0.01, "the comparison starts at Code size");
	const auto zoomedTo = [&](const double aFactor) {
		return std::abs(right->font().pointSizeF() - codeSize * aFactor) < 0.01
			&& std::abs(surface->left_pane()->font().pointSizeF() - codeSize * aFactor) < 0.01;
	};

	// View > Zoom In, twice.
	auto writes = application->status().AppearanceDurableWrites;
	expect(window.zoom_in_action()->isEnabled() && !window.actual_size_action()->isEnabled(), "Zoom In is offered and Actual Size has nothing to do");
	window.zoom_in_action()->trigger();
	window.zoom_in_action()->trigger();
	// [[BPR-1336]] follow-up. The indicator shows for 1.2 s after each step, and
	// persisting and remeasuring the 1557-line comparison can outlast that in a
	// Windows Debug build, so what it said is read while it shows.
	auto       indicated       = false;
	const auto watch_indicator = [&] {
		indicated = indicated
			|| (surface->code_zoom_indicator()->isVisible()
				&& surface->code_zoom_indicator()->accessibleName().startsWith(QStringLiteral("Code zoom 125%")));
	};
	wait_for([&] {
		watch_indicator();
		return zoomedTo(1.25) && application->status().AppearanceDurableWrites > writes && !surface->geometry_pending();
	},
			 "two Zoom Ins show both panes at 125% of Code size, persisted and remeasured");
	const auto height = surface->line_rect(line.Side, line.Line).height();
	expect(std::abs(lineTop() - before) <= height, "the reader's line stays where it was: " + std::to_string(before) + " to " + std::to_string(lineTop()));
	expect(indicated, "the indicator says 125%");
	expect(window.appearance_settings()->code_zoom_selector()->currentData().toUInt() == 125U
			   && std::abs(window.appearance_settings()->code_size_editor()->value() / 100.0 - codeSize) < 0.001,
		   "Settings shows the zoom, and Code size is unchanged");

	// ⌘-scroll back down one step.
	writes     = application->status().AppearanceDurableWrites;
	auto wheel = QWheelEvent{ QPointF{ 40.0, 40.0 }, right->viewport()->mapToGlobal(QPointF{ 40.0, 40.0 }), QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false };
	QCoreApplication::sendEvent(right->viewport(), &wheel);
	wait_for([&] { return zoomedTo(1.1) && application->status().AppearanceDurableWrites > writes && !surface->geometry_pending(); },
			 "⌘-scroll one notch toward the reader zooms out one step");
	expect(std::abs(lineTop() - before) <= surface->line_rect(line.Side, line.Line).height(), "and the reader's line still stays where it was");

	// Actual Size.
	writes = application->status().AppearanceDurableWrites;
	window.actual_size_action()->trigger();
	wait_for([&] { return zoomedTo(1.0) && application->status().AppearanceDurableWrites > writes && !surface->geometry_pending(); },
			 "Actual Size returns both panes to Code size");
	expect(std::abs(lineTop() - before) <= surface->line_rect(line.Side, line.Line).height() && !window.actual_size_action()->isEnabled(),
		   "the line is back where it started and Actual Size has nothing left to do");

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "shutdown drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "private root cleaned");
	std::cout << "DIFF_ZOOM_PUBLIC_PATH_OK\n";
	return EXIT_SUCCESS;
}
