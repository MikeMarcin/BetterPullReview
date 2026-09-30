#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "review_cycle_desktop.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextCursor>
#include <QToolButton>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>

/// \file
/// \brief The real workbench window with the real local-cycle composition
/// (BPR-1157): Open Project with no account to a first note and a suggestion
/// in the RFC-BPR-0052 scaffold, Home's Review Cycles section, resume from
/// recents, and a switch to a pull request and back.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 4; ++round) {
			aLanes.drain();
			QApplication::processEvents();
		}
	}

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_CYCLE_DESKTOP_RENDER_DIR");
		const auto image     = aWidget.grab().toImage();
		expect(!image.isNull(), "rendered window");
		if (!directory.isEmpty()) {
			QDir{ }.mkpath(directory);
			expect(image.save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "render saved");
		}
		std::cout << "RENDER " << aName.toStdString() << ' ' << image.width() << 'x' << image.height() << '\n';
	}

	auto snapshot(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto files = std::map<std::string, std::string>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			if (entry.is_regular_file()) {
				files.emplace(entry.path().lexically_relative(aRoot).generic_string(), read_file(entry.path()));
			}
		}
		return files;
	}

	auto identity_text(qt::workbench_window& aWindow) -> QString
	{
		auto* const label = aWindow.findChild<QLabel*>(QStringLiteral("workbenchStripIdentity"));
		return label != nullptr && label->isVisible() ? label->text() : QString{ };
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication                         app{ aArgc, aArgv };
	tests::sqlite_contract::scratch_root profile{ "cycle-desktop-profile" };
	tests::sqlite_contract::scratch_root scratch{ "cycle-desktop-project" };
	const auto                           project = scratch.path() / "parser-project";
	write_file(project / "README.md", "# Parser\n");
	write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\r\n{\r\n\treturn check(aText);\r\n}\r\n");
	const auto before = snapshot(project);

	auto lanes  = application::manual_executor{ };
	auto window = qt::workbench_window{ };
	window.resize(1280, 800);
	window.show();
	auto desktop = app::review_cycle_desktop{ window, profile.path(), core::bpr_profile_id{ 1 }, lanes, lanes };
	settle(lanes);

	// Home offers the local way in with no account: File > Open Project… and the section.
	expect(window.showing_home() && window.open_project_action() != nullptr && window.open_project_action()->isEnabled(), "Open Project offered signed out");
	expect(window.findChild<QToolButton*>(QStringLiteral("homeOpenProjectButton")) != nullptr && !window.home_cycles_list()->isVisible(), "Home's Review Cycles section, no recents yet");
	capture(window, QStringLiteral("desktop-home-signed-out"));

	desktop.open_project(project.string());
	settle(lanes);
	auto* const page = window.local_cycle();
	expect(window.showing_local_cycle() && page != nullptr && page->isVisible(), "the cycle takes the review surface's place");
	expect(identity_text(window).contains(QStringLiteral("parser-project")) && identity_text(window).contains(QStringLiteral("Local cycle")), "title row names the cycle and badges it local");
	expect(page->projection().Revisions.size() == 1U && page->source()->toPlainText().startsWith(QStringLiteral("# Parser")), "R0 full source in one pane");
	auto* const modality = window.findChild<QWidget*>(QStringLiteral("workbenchModalityAction"));
	static_cast<void>(modality);
	capture(window, QStringLiteral("desktop-cycle-r0"));
	auto* const status = window.findChild<QLabel*>(QStringLiteral("workbenchStatus"));
	expect(status != nullptr && !status->text().contains(QStringLiteral("pull request")), "the status line does not ask for a pull request");

	// The first note: open the parser, select a range, comment.
	emit page->files()->itemActivated(page->files()->item(1));
	settle(lanes);
	expect(page->projection().Source && page->projection().Source->Path == "src/parser.cpp", "file opened from the drawer");
	auto cursor = page->source()->textCursor();
	cursor.setPosition(39);
	cursor.setPosition(51, QTextCursor::KeepAnchor);
	page->source()->setTextCursor(cursor);
	QApplication::processEvents();
	const auto anchor = page->selection_anchor();
	expect(anchor && anchor->Original == "check(aText)" && anchor->Start == 41U, "exact CRLF byte anchor from the native selection");
	page->comment_action()->trigger();
	page->comment_editor()->setPlainText(QStringLiteral("Why check here?"));
	page->findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
	settle(lanes);
	expect(page->notes()->count() == 1 && page->notes()->item(0)->text().contains(QStringLiteral("Why check here?")), "first note saved and listed");

	// A suggestion from the same selection.
	page->source()->setTextCursor(cursor);
	page->suggest_action()->trigger();
	page->composer()->editor()->apply(core::select_all{ });
	page->composer()->editor()->apply(core::insert_text{ "validate(aText)" });
	capture(window, QStringLiteral("desktop-cycle-suggest"));
	page->composer()->save_button()->click();
	settle(lanes);
	expect(page->notes()->count() == 2 && page->notes()->item(1)->text().contains(QStringLiteral("Suggested change")), "suggestion saved");
	capture(window, QStringLiteral("desktop-cycle-notes"));

	// One discussion across two files: link a README range to the first note.
	emit page->files()->itemActivated(page->files()->item(0));
	settle(lanes);
	auto readme = page->source()->textCursor();
	readme.setPosition(2);
	readme.setPosition(8, QTextCursor::KeepAnchor);
	page->source()->setTextCursor(readme);
	page->notes()->setCurrentRow(0);
	QApplication::processEvents();
	expect(page->relate_button()->isEnabled(), "a selection can join a note");
	page->relate_button()->click();
	settle(lanes);
	expect(page->projection().Notes[0].Related.size() == 1U && std::get<core::cycle_source_anchor>(page->projection().Notes[0].Related[0].Target).Path == "README.md", "related location in another file");
	expect(std::holds_alternative<core::cycle_source_anchor>(page->projection().Notes[0].Primary) && std::get<core::cycle_source_anchor>(page->projection().Notes[0].Primary).Path == "src/parser.cpp", "primary kept");
	expect(page->notes()->item(0)->text().contains(QStringLiteral("Related locations · 1")), "shown on the note");

	// No extra permanent row: the review bar and the tabs, the Updates line folded.
	const auto top = page->findChild<QWidget*>(QStringLiteral("cycleBody"))->mapTo(page, QPoint{ 0, 0 }).y();
	expect(top <= qt::workbench_strip::review_bar_height_pixels + qt::document_tab_strip::height_pixels + 4, "two rows above the panes");

	// Home, recents and back.
	window.choose_home(true);
	settle(lanes);
	expect(window.showing_home() && window.home_cycles_list()->isVisible() && window.home_cycles_list()->count() == 1, "recent cycle on Home");
	capture(window, QStringLiteral("desktop-home-recents"));
	emit window.home_cycles_list()->itemActivated(window.home_cycles_list()->item(0));
	settle(lanes);
	expect(window.showing_local_cycle() && page->notes()->count() == 2, "resumed from Home");

	// A pull request arriving takes the review surface back; the cycle is not lost.
	window.publish(application::github_presentation_update{
		.PullRequest = application::github_pull_request_projection{ .Owner = "octo", .RepositoryName = "parser", .Number = core::pull_request_number{ 7U }, .Title = "Tighten parsing" },
	});
	settle(lanes);
	expect(!window.showing_local_cycle() && !window.showing_home() && identity_text(window).contains(QStringLiteral("#7")) && !identity_text(window).contains(QStringLiteral("Local cycle")), "switched to the pull request");
	capture(window, QStringLiteral("desktop-switch-to-pull-request"));
	emit window.home_cycles_list()->itemActivated(window.home_cycles_list()->item(0));
	settle(lanes);
	expect(window.showing_local_cycle() && identity_text(window).contains(QStringLiteral("Local cycle")) && page->notes()->count() == 2, "and back to the cycle");

	expect(snapshot(project) == before, "the chosen folder was only read");
	desktop.shutdown();
	settle(lanes);
	expect(desktop.is_quiescent(), "quiescent after shutdown");
	std::cout << "review-cycle-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
