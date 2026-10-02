#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "review_cycle_desktop.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QToolButton>

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

/// \file
/// \brief The real workbench window with the real local-cycle composition
/// (BPR-1157): Open Project with no account to a first note and a suggestion
/// in the RFC-BPR-0052 scaffold, Home's Review Cycles section, resume from
/// recents, and a switch to a pull request and back. The source pane is in
/// the review panes' syntax colours, and a profile whose cycles another
/// version saved leaves Home in place and says so when a project is opened.

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

	/// Freezes F1 over the cycle's first note through the production runtime,
	/// beside the desktop, and returns the round.
	auto freeze_first_note(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject) -> core::cycle_decoded_round
	{
		auto lanes   = application::manual_executor{ };
		auto runtime = application::review_cycle_runtime{ adapters::sqlite::make_review_cycle_store(aProfileRoot, core::bpr_profile_id{ 1 }), lanes, lanes };
		runtime.select(aSubject);
		auto operation = [](const std::uint8_t aIndex) {
			auto bytes = std::array<std::uint8_t, 32>{ };
			bytes[0]   = 0xD5;
			bytes[1]   = aIndex;
			return application::cycle_operation_id{ bytes };
		};
		auto state = std::optional<application::cycle_store_result<std::optional<core::review_cycle_state>>>{ };
		runtime.load_selected([&](auto aValue) { state = std::move(aValue); });
		lanes.drain();
		expect(state && *state && **state, "cycle state");
		auto begun = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
		runtime.begin_round_selected(operation(1), (**state)->Revision, [&](auto aValue) { begun = std::move(aValue); });
		lanes.drain();
		expect(begun && *begun, "draft");
		const auto draft = core::cycle_round_draft_ref{ .Cycle = aSubject.Cycle, .Id = core::cycle_round_draft_id{ (*begun)->Allocated } };
		auto       saved = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
		runtime.save_round_selected(operation(2), draft, 1, { .Source = core::cycle_source_revision_id{ 1 }, .Goal = "Rename check.", .Items = { { .Item = core::cycle_feedback_item_id{ 1 }, .Wording = core::private_review_turn_revision_id{ 1 } } } },
									[&](auto aValue) { saved = std::move(aValue); });
		lanes.drain();
		expect(saved && *saved, "draft saved");
		auto frozen = std::optional<application::cycle_store_result<application::cycle_round_freeze_result>>{ };
		runtime.freeze_round_selected({ .Operation = operation(3), .Draft = draft, .ExpectedDraftRevision = (*saved)->Revision, .Editor = { .ShownRevision = (*saved)->Revision } },
									  [&](auto aValue) { frozen = std::move(aValue); });
		lanes.drain();
		expect(frozen && *frozen && (*frozen)->Frozen, "F1 frozen");
		auto content = (*frozen)->Frozen->Content;
		runtime.shutdown();
		lanes.drain();
		return content;
	}

	// Not `near`: the Windows SDK defines `near` as an empty macro.
	auto colour_near(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 24 && std::abs(aPixel.green() - aColour.green()) <= 24 && std::abs(aPixel.blue() - aColour.blue()) <= 24;
	}

	/// \brief Whether the first `aWord` in `aEdit` is painted in `aColour`:
	/// inside the word's own rectangle at least one pixel is near `aColour`
	/// and none is near the plain text colour. Relative to the word's own
	/// ink, so it holds at any scale and face.
	auto word_painted(QPlainTextEdit& aEdit, const QString& aWord, const QColor& aColour) -> bool
	{
		const auto found = aEdit.document()->find(aWord);
		if (found.isNull()) {
			return false;
		}
		auto start = found;
		start.setPosition(found.selectionStart());
		auto end = found;
		end.setPosition(found.selectionEnd());
		const auto left   = aEdit.cursorRect(start);
		const auto right  = aEdit.cursorRect(end);
		const auto pixmap = aEdit.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aEdit.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(left.top() * ratio); y < static_cast<int>((left.bottom() + 1) * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(left.left() * ratio); x < static_cast<int>(right.left() * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	/// \brief The same rule in the code view, over the first `aWord`'s cells.
	auto word_painted(qt::suggestion_editor& aView, const QString& aWord, const QColor& aColour) -> bool
	{
		const auto word  = aWord.toUtf8();
		const auto found = aView.text().find(std::string_view{ word.constData(), static_cast<std::size_t>(word.size()) });
		if (found == std::string::npos) {
			return false;
		}
		const auto left   = aView.cell_rectangle(aView.model().point_of(found));
		const auto right  = aView.cell_rectangle(aView.model().point_of(found + static_cast<std::size_t>(word.size())));
		const auto pixmap = aView.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aView.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(std::ceil(left.top() * ratio)); y < static_cast<int>(left.bottom() * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(std::ceil(left.left() * ratio)); x < static_cast<int>(right.left() * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	/// \brief The same rule in the suggestion editor, between the middles of
	/// columns `aFirst` and `aLast` of the caret's line.
	auto span_painted(qt::suggestion_editor& aEditor, const std::uint64_t aFirst, const std::uint64_t aLast, const QColor& aColour) -> bool
	{
		const auto line  = aEditor.caret_rectangle();
		auto       begin = -1;
		auto       end   = -1;
		for (auto x = 0; x < aEditor.viewport()->width() && end < 0; ++x) {
			const auto column = aEditor.point_at(QPoint{ x, line.center().y() }).Column;
			if (begin < 0 && column > aFirst) {
				begin = x;
			}
			if (column > aLast) {
				end = x;
			}
		}
		if (begin < 0 || end <= begin) {
			return false;
		}
		const auto pixmap = aEditor.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aEditor.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(line.top() * ratio); y < static_cast<int>((line.bottom() + 1) * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(begin * ratio); x < static_cast<int>(end * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	/// `aTop` laid over `aBase`, as a band with alpha is painted.
	auto over(const QColor& aTop, const QColor& aBase) -> QColor
	{
		const auto alpha = aTop.alphaF();
		return QColor::fromRgbF(static_cast<float>(aTop.redF() * alpha + aBase.redF() * (1.0 - alpha)), static_cast<float>(aTop.greenF() * alpha + aBase.greenF() * (1.0 - alpha)),
								static_cast<float>(aTop.blueF() * alpha + aBase.blueF() * (1.0 - alpha)));
	}

	/// Whether the preview row `aRow` is filled with `aColour` past the end
	/// of its text, where no glyph is. A flat fill rounds by a few units at most.
	auto row_filled(QPlainTextEdit& aPreview, const QString& aRow, const QColor& aColour) -> bool
	{
		for (auto block = aPreview.document()->firstBlock(); block.isValid(); block = block.next()) {
			if (block.text() != aRow) {
				continue;
			}
			const auto row    = aPreview.cursorRect(QTextCursor{ block });
			const auto pixmap = aPreview.viewport()->grab();
			const auto ratio  = pixmap.devicePixelRatio();
			const auto pixel  = pixmap.toImage().pixelColor(static_cast<int>((aPreview.viewport()->width() - 4) * ratio), static_cast<int>(row.center().y() * ratio));
			return std::abs(pixel.red() - aColour.red()) <= 3 && std::abs(pixel.green() - aColour.green()) <= 3 && std::abs(pixel.blue() - aColour.blue()) <= 3;
		}
		return false;
	}

	/// Marks a profile's cycle store as saved by another schema version.
	void mark_store_newer(const std::filesystem::path& aProfileRoot)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open((aProfileRoot / "state" / "v1" / "review-cycles.sqlite3").string().c_str(), &database) == SQLITE_OK, "open the owned store");
		expect(sqlite3_exec(database, "PRAGMA user_version=99", nullptr, nullptr, nullptr) == SQLITE_OK, "store marked newer");
		sqlite3_close(database);
	}

	/// Whether a preview row carries `aColour` on the characters [aStart, aStart + aLength).
	auto row_coloured(const QPlainTextEdit& aPreview, const QString& aRow, const int aStart, const int aLength, const QColor& aColour) -> bool
	{
		for (auto block = aPreview.document()->firstBlock(); block.isValid(); block = block.next()) {
			if (block.text() == aRow && block.layout() != nullptr) {
				return std::ranges::any_of(block.layout()->formats(), [&](const QTextLayout::FormatRange& aRange) {
					return aRange.start == aStart && aRange.length == aLength && aRange.format.foreground().color() == aColour;
				});
			}
		}
		return false;
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
	// BPR-1156: the packaged definitions the review panes classify with.
	const auto definitions = adapters::syntax::packaged_definition_catalog{ };
	auto       desktop     = app::review_cycle_desktop{ window, profile.path(), core::bpr_profile_id{ 1 }, lanes, lanes, { .Definitions = &definitions, .Effects = &lanes } };
	// The platform's own scheme, as the application observes it, so a native
	// render paints dark-scheme role colours on a dark window.
	const auto scheme     = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? core::os_color_scheme::dark : core::os_color_scheme::light;
	auto       appearance = qt::appearance_profile{ qt::default_appearance_settings(), { .Scheme = scheme } };
	// The window's palette as the application applies it, so the selection is
	// the profile's own code_selection_background wash (BPR-1156, v005).
	appearance.apply_to(window);
	desktop.adopt_appearance(appearance);
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
	expect(page->projection().Revisions.size() == 1U && page->source()->text().starts_with("# Parser") && page->source()->read_only(), "R0 full source in one pane");
	auto* const modality = window.findChild<QWidget*>(QStringLiteral("workbenchModalityAction"));
	static_cast<void>(modality);
	capture(window, QStringLiteral("desktop-cycle-r0"));
	auto* const status = window.findChild<QLabel*>(QStringLiteral("workbenchStatus"));
	expect(status != nullptr && !status->text().contains(QStringLiteral("pull request")), "the status line does not ask for a pull request");

	// The first note: open the parser, select a range, comment.
	emit page->files()->itemActivated(page->files()->item(1));
	settle(lanes);
	expect(page->projection().Source && page->projection().Source->Path == "src/parser.cpp", "file opened from the drawer");
	// BPR-1157: the owner rejected a plain source pane. The real classifier
	// colours the file the pane shows, as the diff panes would.
	const auto function = appearance.color(core::appearance_role::syntax_function);
	const auto flow     = appearance.color(core::appearance_role::syntax_control_flow);
	expect(word_painted(*page->source(), QStringLiteral("return"), flow) && word_painted(*page->source(), QStringLiteral("check"), function), "the source pane in the panes' syntax colours");
	capture(window, QStringLiteral("desktop-cycle-source-syntax"));
	page->source()->select_bytes(41U, 53U);
	QApplication::processEvents();
	const auto anchor = page->selection_anchor();
	expect(anchor && anchor->Original == "check(aText)" && anchor->Start == 41U, "exact CRLF byte anchor from the native selection");
	page->comment_action()->trigger();
	page->comment_editor()->setPlainText(QStringLiteral("Why check here?"));
	page->findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
	settle(lanes);
	expect(page->notes()->count() == 1 && page->notes()->item(0)->text().contains(QStringLiteral("Why check here?")), "first note saved and listed");

	// Suggest change from the source pane's context menu, on the same
	// selection: the whole file is edited in the main pane and each separate
	// change is one exact suggestion (owner amendment to BPR-1156,
	// 2026-10-01), coloured by the panes' classifier.
	page->source()->select_bytes(41U, 53U);
	QApplication::processEvents();
	capture(window, QStringLiteral("desktop-cycle-source-selection"));
	auto* const sourceMenu = page->source_menu(page->source()->cell_rectangle(page->source()->model().point_of(45U)).center().toPoint());
	expect(sourceMenu != nullptr && sourceMenu->actions().contains(page->suggest_action()) && page->suggest_action()->isEnabled() && page->source()->selected_bytes() == std::pair<std::uint64_t, std::uint64_t>{ 41U, 53U },
		   "Suggest change in the source pane's context menu; a click inside the selection keeps it");
	page->suggest_action()->trigger();
	delete sourceMenu;
	QApplication::processEvents();
	capture(window, QStringLiteral("desktop-cycle-suggest-begun"));
	auto* const suggestion = page->composer()->editor();
	auto* const preview    = page->composer()->preview();
	const auto  original   = read_file(project / "src" / "parser.cpp");
	expect(page->suggesting() && suggestion == page->source() && suggestion->isVisible() && !suggestion->read_only() && suggestion->text() == original, "the whole file in the main pane, in the same code view");
	// The source pane already classified these bytes, so the retained
	// classification colours the editor without waiting.
	expect(span_painted(*suggestion, 4U, 9U, flow), "the editor paints return in the control-flow colour the panes use");
	// Replace the selection, then add a line at the top: two separate changes.
	suggestion->apply(core::insert_text{ "validate(aText)" });
	suggestion->apply(core::move_carets{ .Motion = core::text_motion::document_start });
	suggestion->apply(core::insert_text{ "// Parses one value.\r\n" });
	settle(lanes);
	expect(suggestion->text() == "// Parses one value.\r\nint parse(const char* aText)\r\n{\r\n\treturn validate(aText);\r\n}\r\n", "edited in place, CRLF kept");
	expect(page->composer()->edits().size() == 2U && page->composer()->summary_text() == QStringLiteral("2 changes · lines 1, 3"), "two separate changes");
	expect(row_coloured(*preview, QStringLiteral("- \treturn check(aText);"), 3, 6, flow) && word_painted(*preview, QStringLiteral("return"), flow), "the preview's removed row colours return as control flow");
	expect(row_coloured(*preview, QStringLiteral("+ \treturn validate(aText);"), 3, 6, flow), "the added row too, once the edited file is classified");
	const auto base = preview->palette().color(QPalette::Base);
	expect(row_filled(*preview, QStringLiteral("- \treturn check(aText);"), over(appearance.color(core::appearance_role::diff_removed_background), base))
			   && row_filled(*preview, QStringLiteral("+ \treturn validate(aText);"), over(appearance.color(core::appearance_role::diff_added_background), base))
			   && row_filled(*preview, QStringLiteral("+ // Parses one value."), over(appearance.color(core::appearance_role::diff_added_background), base)),
		   "removed and added rows in the diff panes' fills");
	suggestion->apply(core::place_caret{ .At = { 3U, 0U } });
	QApplication::processEvents();
	expect(span_painted(*suggestion, 4U, 9U, flow), "the edited file is classified again and coloured");
	capture(window, QStringLiteral("desktop-cycle-suggest"));
	capture(*page->composer(), QStringLiteral("desktop-cycle-suggest-composer"));
	page->composer()->save_button()->click();
	settle(lanes);
	expect(page->notes()->count() == 2 && page->notes()->item(1)->text().contains(QStringLiteral("Suggested change · 2")) && page->projection().Notes[1].Suggestions == 2U, "one note with both suggestions saved");
	expect(!page->suggesting() && page->source()->isVisible() && page->source()->read_only() && page->source()->text() == original && read_file(project / "src" / "parser.cpp") == original,
		   "the source pane back on the captured bytes; the file on disk untouched");
	// Enter indents as a code editor does: after the opening brace, one tab
	// more, chosen for this C++ file (owner note of 2026-10-02).
	page->suggest_action()->trigger();
	page->source()->apply(core::place_caret{ .At = { 1U, 1U } });
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r") };
	QApplication::sendEvent(page->source(), &enter);
	expect(page->source()->text() == "int parse(const char* aText)\r\n{\r\n\t\r\n\treturn check(aText);\r\n}\r\n", "Enter after a brace indents one tab, CRLF kept");
	page->findChild<QPushButton*>(QStringLiteral("suggestionCancel"))->click();
	settle(lanes);
	expect(!page->suggesting() && page->source()->text() == original, "cancelled back to the captured bytes");
	capture(window, QStringLiteral("desktop-cycle-notes"));

	// One discussion across two files: link a README range to the first note.
	emit page->files()->itemActivated(page->files()->item(0));
	settle(lanes);
	page->source()->select_bytes(2U, 8U);
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

	// BPR-1163: someone edits the folder with ordinary tools and sends nothing
	// back; Capture revision is the reviewer's way back, with no callback.
	emit page->files()->itemActivated(page->files()->item(1));
	settle(lanes);
	write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\r\n{\r\n\treturn aText ? check(aText) : 0;\r\n}\r\n");
	expect(page->capture_action()->isEnabled() && page->projection().Revisions.size() == 1U, "Capture revision offered; nothing captured by itself");
	page->capture_action()->trigger();
	settle(lanes);
	expect(page->projection().Revisions.size() == 2U && page->projection().Source && page->projection().Source->Revision == core::cycle_source_revision_id{ 1 }, "R1 captured; the open tab keeps R0");
	expect(page->refresh_action()->isVisible(), "newer material is offered, not shown by itself");
	auto* const menu = page->view_button()->menu();
	emit        menu->aboutToShow();
	const auto  actions = menu->actions();
	const auto  all     = std::ranges::find_if(actions, [](const QAction* aAction) { return aAction->text() == QStringLiteral("All cycle changes"); });
	expect(all != actions.end() && (*all)->isEnabled(), "All cycle changes in the view chooser");
	(*all)->trigger();
	settle(lanes);
	expect(page->projection().Comparison && page->comparison()->isVisible() && page->comparison()->toPlainText().contains(QStringLiteral("+ \treturn aText ? check(aText) : 0;")),
		   "the comparison pane shows the shared engine's rows");
	expect(page->findChild<QLabel*>(QStringLiteral("cycleRevisionLabel"))->text() == QStringLiteral("All cycle changes · R1 → R2"), "the review bar names the view and its pair");
	page->mark_reviewed_action()->trigger();
	settle(lanes);
	expect(page->projection().Facts.LastReview && page->projection().Facts.LastReview->Revision == core::cycle_source_revision_id{ 2 }, "reviewed through R1");
	page->capture_action()->trigger();
	settle(lanes);
	expect(page->projection().Revisions.size() == 2U && page->updates_line()->text().contains(QStringLiteral("No change since R2")), "an unchanged capture says so");
	capture(window, QStringLiteral("desktop-cycle-compare"));

	// BPR-1164: a round frozen in this profile; what comes back is recorded
	// as claims, by hand or from a response file, and changes nothing else.
	const auto round   = freeze_first_note(profile.path(), page->projection().Cycle->Subject);
	const auto written = snapshot(project);
	desktop.open_project(project.string());
	settle(lanes);
	expect(page->projection().Facts.Rounds.size() == 1U && page->projection().Revisions.size() == 2U, "the frozen round is known; nothing captured on reopening");
	page->notes()->setCurrentRow(0);
	auto* const replies = page->reply_button()->menu();
	emit        replies->aboutToShow();
	replies->actions()[0]->trigger();
	page->reply_from()->setText(QStringLiteral("Dana (chat)"));
	page->reply_editor()->setPlainText(QStringLiteral("Kept check(); it validates too."));
	page->findChild<QPushButton*>(QStringLiteral("cycleReplySave"))->click();
	settle(lanes);
	expect(page->notes()->item(0)->text().contains(QStringLiteral("F1 reply · Dana (chat), entered by hand replied: Kept check(); it validates too.")), "a pasted reply beside its note");
	const auto responseFile = scratch.path() / "response.json";
	write_file(responseFile, *core::encode_response_document({
								 .ResponseId    = "RESP-1",
								 .CycleId       = round.Payload.CycleId,
								 .RoundId       = round.Payload.RoundId,
								 .RoundDigest   = round.Digest,
								 .ClaimedAuthor = { .DisplayName = "Harness", .Kind = core::cycle_claimed_author_kind::agent, .Model = "Claude Opus" },
								 .Entries       = { { .Item = "I1", .ItemRevision = 1, .Disposition = core::cycle_response_disposition::addressed, .Body = "Renamed to validate.", .Evidence = { { .Kind = core::cycle_evidence_kind::test_report, .Value = "tests passed" } } } },
							 }));
	desktop.host().import_response(core::cycle_round_id{ 1 }, responseFile.string());
	settle(lanes);
	expect(page->projection().Responses.size() == 2U && page->notes()->item(0)->text().contains(QStringLiteral("Harness (claims agent Claude Opus), response file says addressed: Renamed to validate.")),
		   "a response file's statement beside its note, as a claim");
	desktop.host().import_response(core::cycle_round_id{ 1 }, responseFile.string());
	settle(lanes);
	expect(page->projection().Responses.size() == 2U && page->updates_line()->text().contains(QStringLiteral("already added")), "the same file again adds nothing");
	expect(snapshot(project) == written && page->projection().Revisions.size() == 2U && page->projection().Facts.LastReview && page->projection().Facts.LastReview->Revision == core::cycle_source_revision_id{ 2 },
		   "replies wrote no source, captured nothing and moved no review mark");
	capture(window, QStringLiteral("desktop-cycle-replies"));

	// BPR-1165: the claim asks for verification; only the reviewer's decision gives it.
	expect(page->notes()->item(0)->text().contains(QStringLiteral("Recipient says addressed; needs your verification")), "addressed asks for verification");
	auto* const decisions = page->decision_menu(0);
	expect(decisions != nullptr && decisions->actions()[0]->text() == QStringLiteral("Verified in R2"), "verify against the revision shown");
	decisions->actions()[0]->trigger();
	settle(lanes);
	expect(page->notes()->item(0)->text().contains(QStringLiteral("Verified by you in R2")), "verified by the reviewer");
	page->next_round_button()->click();
	settle(lanes);
	expect(page->projection().NextRound && page->projection().NextRound->Resolved == 1U && page->projection().NextRound->Added == 1U && page->projection().NextRound->Carried == 0U,
		   "the next round's draft: the verified note left out, the other note added");
	expect(snapshot(project) == written && page->projection().Revisions.size() == 2U, "deciding and preparing wrote no source");
	capture(window, QStringLiteral("desktop-cycle-verified"));
	desktop.shutdown();
	settle(lanes);
	expect(desktop.is_quiescent(), "quiescent after shutdown");

	// BPR-1157 v001 was rejected on a window that showed an empty
	// "Open Project · Local cycle" page at start: an older build met a store
	// a newer one had saved. Home stays; the refusal is said on the page.
	{
		tests::sqlite_contract::scratch_root newer{ "cycle-desktop-newer" };
		{
			auto store = adapters::sqlite::make_review_cycle_store(newer.path(), core::bpr_profile_id{ 1 });
			expect(store->catalog({ }).has_value(), "a store to mark");
		}
		mark_store_newer(newer.path());
		auto other = qt::workbench_window{ };
		other.resize(1280, 800);
		other.show();
		auto refused = app::review_cycle_desktop{ other, newer.path(), core::bpr_profile_id{ 1 }, lanes, lanes };
		settle(lanes);
		expect(other.showing_home() && !other.showing_local_cycle(), "Home stays in place at start");
		refused.open_project(project.string());
		settle(lanes);
		auto* const notice = other.findChild<QLabel*>(QStringLiteral("cycleSourceNotice"));
		expect(other.showing_local_cycle() && notice != nullptr && notice->isVisible() && notice->text().contains(QStringLiteral("saved by a different version")), "the refusal is said where the source would be");
		capture(other, QStringLiteral("desktop-cycle-newer-store"));
		refused.shutdown();
		settle(lanes);
		expect(refused.is_quiescent(), "the refused desktop is quiescent");
	}
	std::cout << "review-cycle-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
