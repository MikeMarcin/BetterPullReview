// [[BPR-1334]]. Line details through the real composition, the workbench host
// and the Git/cache actor, on the constructed line-details pull request served
// by the fake GitHub server.
//
// The owner, 2026-10-08: "Can we add something akin to the beyond compare text
// details view as an optional on by default view page below the side by side
// which shows just the current cursor's line as a full width single pane
// diff". The course clicks a long changed line in the head pane and reads the
// strip under the comparison: the base version above the head version, whole,
// wrapped, with the changed characters emphasised. A change step moves it, a
// twelve-thousand-character line is cut with its marker, and View > Line
// Details hides and shows it. `--attended` stops on the long SQL line for the
// owner's look.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextLayout>
#include <QThread>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
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

	/// \brief One side's line text, as the comparison decoded it.
	[[nodiscard]] auto source_line(const qt::diff_workbench& aSurface, const core::diff_side aSide, const std::uint32_t aLine) -> QString
	{
		const auto& document = aSide == core::diff_side::left ? aSurface.current_result()->LeftDocument : aSurface.current_result()->RightDocument;
		expect(document.has_value() && aLine < document->Lines.size(), "the comparison holds the line");
		const auto span = document->Lines[aLine].Content;
		return QString::fromUtf8(reinterpret_cast<const char*>(document->Bytes.data() + span.Begin), static_cast<qsizetype>(span.End - span.Begin));
	}

	/// \brief Opens one file of the pull request and waits for its comparison.
	void open_file(app::github_application& aApplication, const std::string_view aPath)
	{
		auto result = app::detail::fixture_file_reveal_result::pending;
		wait_for([&] {
			result = access::reveal_fixture_file(aApplication, aPath);
			return result != app::detail::fixture_file_reveal_result::pending;
		},
				 "the file's comparison is revealed");
		expect(result == app::detail::fixture_file_reveal_result::shown, "the file is shown");
		auto* const surface = aApplication.window().comparison_surface();
		wait_for([&] { return surface->current_result() != nullptr && !surface->geometry_pending(); }, "the comparison is measured");
	}

	/// \brief Clicks one head line in its pane, as a hand does.
	void click_head_line(qt::diff_workbench& aSurface, const std::uint32_t aLine)
	{
		auto* const pane = aSurface.right_pane();
		const auto  row  = aSurface.line_rect(core::diff_side::right, aLine);
		expect(!row.isEmpty(), "the head line is on screen");
		const auto at = pane->viewport()->mapFrom(&aSurface, row.center());
		tests::window_system_clicks(*pane->viewport(), QPointF{ at }, 1);
	}

	/// \brief Whether the strip shows one line's base row over its head row.
	[[nodiscard]] auto shows_pair(const qt::diff_workbench& aSurface, const std::uint32_t aBase, const std::uint32_t aHead) -> bool
	{
		const auto* strip = aSurface.line_details();
		const auto& rows  = strip->projection().Rows;
		return rows.size() == 2U && rows[0].Line == std::optional{ aBase } && rows[1].Line == std::optional{ aHead }
		&& strip->document()->findBlockByNumber(0).text() == source_line(aSurface, core::diff_side::left, aBase)
			&& strip->document()->findBlockByNumber(1).text() == source_line(aSurface, core::diff_side::right, aHead);
	}

	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_LINE_DETAILS_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "render directory created");
		expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "composed window render saved");
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	const auto    attended = aArgc > 1 && std::string_view{ aArgv[1] } == "--attended";
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-line-details-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::line_details_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composed startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(attended ? 1440 : 1280, attended ? 900 : 860);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/line-details#34"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "the pull request opens on Overview");

	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
	expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and the Files drawer");

	auto* const surface = window.comparison_surface();
	auto* const strip   = surface->line_details();
	auto* const toggle  = window.line_details_action();
	expect(strip != nullptr && toggle != nullptr && toggle->isChecked() && window.view_menu()->actions().contains(toggle),
		   "View carries Line Details, checked: the strip is on by default");

	// The long SQL literal on line 6 of the query: a click on its head line.
	open_file(*application, "src/report/query.cpp");
	click_head_line(*surface, 5U);
	wait_for([&] { return strip->isVisible() && shows_pair(*surface, 5U, 5U); },
			 "a click on a long changed line shows its base version above its head version, whole");
	const auto& rows = strip->projection().Rows;
	// The head line only gains text here (a column, a condition and an
	// ORDER BY key), so the emphasis is all on the head row.
	expect(rows[0].Kind == core::line_details_row_kind::changed && rows[1].Kind == core::line_details_row_kind::changed
			   && rows[0].Emphasis.empty() && rows[1].Emphasis.size() >= 3U,
		   "the core marks the inserted characters on the head row");
	expect(strip->gutter_text(0) == QStringLiteral("6 −") && strip->gutter_text(1) == QStringLiteral("6 +"),
		   "each row carries its line number and its side's mark");
	expect(strip->document()->findBlockByNumber(1).layout()->lineCount() > 1 && strip->horizontalScrollBar()->maximum() == 0,
		   "the long head line wraps in the strip rather than scrolling sideways");
	expect(strip->width() > surface->right_pane()->width() && strip->width() > surface->left_pane()->width(),
		   "the strip is wider than either pane: it spans the comparison");
	render(window, QStringLiteral("01-sql-line"));

	if (attended) {
		std::cout << "LINE_DETAILS_ATTENDED_READY source=synthetic fixture=line-details-v1 file=src/report/query.cpp line=6\n"
				  << std::flush;
		const auto result = QApplication::exec();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "attended shutdown drains");
		application.reset();
		expect(e2e::remove_course_root(temporary), "attended private root cleaned");
		return result;
	}

	// A change step moves the current line, and the strip follows it.
	const auto before = strip->projection();
	window.next_change_action()->trigger();
	wait_for([&] { return strip->projection() != before && !strip->projection().Rows.empty(); }, "Next change moves Line details");
	const auto& preferred = surface->selection()->PreferredLine;
	expect(std::ranges::any_of(strip->projection().Rows, [&](const core::line_details_row& aRow) {
			   return aRow.Side == preferred.Side && aRow.Line == std::optional{ preferred.Line };
		   }),
		   "Line details shows the line the change step selected");

	// A twelve-thousand-character line is cut at 4,096 bytes with its marker.
	open_file(*application, "web/static/app.min.js");
	click_head_line(*surface, 0U);
	wait_for([&] { return strip->projection().Rows.size() == 2U && strip->projection().Rows[1].OmittedBytes > 0U; },
			 "the minified line is cut");
	const auto  omitted = strip->projection().Rows[1].OmittedBytes;
	const auto& head    = strip->document()->findBlockByNumber(1).text();
	expect(head.startsWith(source_line(*surface, core::diff_side::right, 0U).left(4'096))
			   && head.endsWith(QStringLiteral(" … %1 more bytes").arg(QLocale{ }.toString(static_cast<qulonglong>(omitted))))
			   && strip->projection().Rows[1].Content.End - strip->projection().Rows[1].Content.Begin == 4'096U,
		   "a line past the bound shows its first 4,096 bytes and says how many more there are");
	render(window, QStringLiteral("02-minified-line"));

	// View > Line Details hides and shows it, through the profile's preference.
	toggle->trigger();
	wait_for([&] { return !toggle->isChecked() && !strip->isVisible(); }, "View > Line Details hides the strip");
	toggle->trigger();
	wait_for([&] { return toggle->isChecked() && strip->isVisible() && !strip->projection().Rows.empty(); }, "and shows it again on the current line");

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "shutdown drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "private root cleaned");
	std::cout << "LINE_DETAILS_PUBLIC_PATH_OK\n";
	return 0;
}
