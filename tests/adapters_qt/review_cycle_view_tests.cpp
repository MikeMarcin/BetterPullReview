#include "../support/test_expectation.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/qt/review_cycle_view.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTextCursor>
#include <QToolButton>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

/// \file
/// \brief The local cycle page (BPR-1157) presented from a projection:
/// review bar, Updates line, tabs, one full-source pane, notes, and exact byte
/// anchors from native selections, through real Qt events. BPR-1163: Capture
/// revision, the view chooser, Mark reviewed, the newer-material refresh and
/// the comparison pane, in the same review bar and pane.

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	constexpr std::string_view parser = "int f()\r\n{\r\n\treturn caf\xC3\xA9(1);\r\n}\r\n";

	auto bytes(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	auto subject() -> core::local_cycle_subject
	{
		auto incarnation = std::array<std::uint8_t, 32>{ };
		incarnation[0]   = 1;
		return { .Authority = { .Profile = core::bpr_profile_id{ 1 }, .Incarnation = core::cycle_catalog_incarnation{ incarnation } }, .Cycle = core::review_cycle_id{ 1 }, .Generation = 1 };
	}

	auto projection(const std::uint64_t aLatest) -> application::review_cycle_workbench_projection
	{
		auto view  = application::review_cycle_workbench_projection{ };
		view.Cycle = core::review_cycle_projection{ .Subject = subject(), .Revision = 3, .Title = "parser-project", .Captured = true };
		for (std::uint64_t revision = 1; revision <= aLatest; ++revision) {
			view.Revisions.emplace_back(revision);
		}
		view.Files = {
			{ .Path = "README.md", .Text = core::material_text_kind::utf8, .Size = 9 },
			{ .Path = "src/parser.cpp", .Text = core::material_text_kind::utf8, .Size = parser.size() },
		};
		view.Tabs      = { { core::document_tab_id{ 1 }, { .Revision = core::cycle_source_revision_id{ 1 }, .Path = "src/parser.cpp" } } };
		view.ActiveTab = core::document_tab_id{ 1 };
		view.Source    = application::review_cycle_source_view{
			.Tab      = core::document_tab_id{ 1 },
			.Revision = core::cycle_source_revision_id{ 1 },
			.Path     = "src/parser.cpp",
			.Status   = application::review_material_status::available,
			.Bytes    = bytes(parser),
			.Text     = core::material_text_kind::utf8,
		};
		const auto anchor = *core::make_anchor(core::cycle_source_revision_id{ 1 }, "src/parser.cpp", bytes(parser), 20, 28);
		view.Notes        = {
			{ .Item = { .Cycle = core::review_cycle_id{ 1 }, .Id = core::cycle_feedback_item_id{ 1 } }, .Primary = anchor, .Body = "Rename this.", .Suggestions = 1, .Correspondence = aLatest > 1 ? std::optional{ core::cycle_target_correspondence{ .Kind = core::cycle_correspondence::ambiguous, .Against = core::cycle_source_revision_id{ 2 }, .Candidates = 2 } } : std::nullopt },
			{ .Item = { .Cycle = core::review_cycle_id{ 1 }, .Id = core::cycle_feedback_item_id{ 2 } }, .Primary = core::cycle_whole_target{ }, .Body = "Pasted answer.", .Provenance = core::authored_provenance::self_reported },
		};
		return view;
	}

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_CYCLE_RENDER_DIR");
		const auto image     = aWidget.grab().toImage();
		expect(!image.isNull(), "rendered state");
		if (!directory.isEmpty()) {
			QDir{ }.mkpath(directory);
			expect(image.save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "render saved");
		}
	}

	/// Selects UTF-16 positions [aFrom, aTo) in the source pane, as a drag would.
	void select(qt::review_cycle_view& aView, const int aFrom, const int aTo)
	{
		auto cursor = aView.source()->textCursor();
		cursor.setPosition(aFrom);
		cursor.setPosition(aTo, QTextCursor::KeepAnchor);
		aView.source()->setTextCursor(cursor);
		settle();
	}

	void presents_the_scaffold_and_notes(qt::review_cycle_view& aView)
	{
		aView.present(projection(1));
		settle();
		expect(aView.review_bar() != nullptr && aView.review_bar()->role() == qt::strip_role::review_bar, "the scaffold's review bar");
		expect(aView.source()->toPlainText() == QStringLiteral("int f()\n{\n\treturn café(1);\n}\n"), "one block per line, whatever the ending");
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleRevisionLabel"))->text() == QStringLiteral("Full source · R1"), "full source names its revision");
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleFilePosition"))->text() == QStringLiteral("File 2 of 2"), "file position");
		expect(aView.tabs()->tabs().size() == 1U && aView.tabs()->tabs()[0].Active && aView.tabs()->tabs()[0].OwnPair.isEmpty(), "one tab on the latest revision");
		expect(aView.notes()->count() == 2 && aView.notes()->item(0)->text().contains(QStringLiteral("line 3")) && aView.notes()->item(0)->text().contains(QStringLiteral("Suggested change")), "anchored note with its line");
		expect(aView.notes()->item(1)->text().startsWith(QStringLiteral("Whole cycle")) && aView.notes()->item(1)->text().contains(QStringLiteral("self-reported")), "cycle note without a line; provenance shown");
		expect(!aView.compose_action()->isEnabled() && aView.comment_action()->isEnabled() && !aView.suggest_action()->isEnabled(), "Compose is not offered yet; Suggest needs a selection");
		expect(!aView.updates_line()->isVisible(), "Updates line folded");
		aView.updates_toggle()->click();
		expect(aView.updates_line()->isVisible() && aView.updates_line()->text().contains(QStringLiteral("Captured: R1")), "Updates line unfolds");
		capture(aView, QStringLiteral("cycle-full-source"));
		aView.updates_toggle()->click();
	}

	void anchors_exact_bytes_from_a_native_selection(qt::review_cycle_view& aView, application::review_cycle_suggestion_request& aSuggested, core::begin_cycle_feedback& aNoted)
	{
		// "café(1)" is UTF-16 positions 18..25 of the display text; bytes 20..28 of the CRLF file.
		select(aView, 18, 25);
		const auto anchor = aView.selection_anchor();
		expect(anchor && anchor->Start == 20U && anchor->End == 28U && anchor->Original == "caf\xC3\xA9(1)", "exact byte anchor across CRLF and UTF-8");
		expect(anchor->Blob == core::cycle_material_digest{ core::sha256(bytes(parser)) } && anchor->Before == "int f()\r\n{\r\n\treturn " && anchor->After == ";\r\n}\r\n", "digest and context");
		expect(aView.suggest_action()->isEnabled(), "Suggest change enabled by a selection");
		aView.comment_action()->trigger();
		aView.comment_editor()->setPlainText(QStringLiteral("Rename to coffee."));
		aView.findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
		expect(std::holds_alternative<core::cycle_source_anchor>(aNoted.Primary) && std::get<core::cycle_source_anchor>(aNoted.Primary) == *anchor && aNoted.Body == "Rename to coffee.", "comment on the range");

		// No selection: a whole-file note, no invented line.
		select(aView, 3, 3);
		aView.comment_action()->trigger();
		aView.comment_editor()->setPlainText(QStringLiteral("Split this file."));
		aView.findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
		expect(std::holds_alternative<core::cycle_file_target>(aNoted.Primary) && std::get<core::cycle_file_target>(aNoted.Primary).Path == "src/parser.cpp", "whole-file note");

		select(aView, 18, 25);
		aView.suggest_action()->trigger();
		settle();
		expect(aView.composer()->isVisible() && aView.composer()->replacement() == "caf\xC3\xA9(1)", "composer prefilled with the selected bytes");
		aView.composer()->editor()->apply(core::select_all{ });
		aView.composer()->editor()->apply(core::insert_text{ "coffee(1)" });
		capture(aView, QStringLiteral("cycle-suggest-change"));
		aView.composer()->save_button()->click();
		expect(aSuggested.Selection == *anchor && aSuggested.Replacement == "coffee(1)" && aSuggested.Intent == core::cycle_suggestion_intent::exact_replacement, "suggestion handed over exactly");
	}

	void states_newer_material_without_moving(qt::review_cycle_view& aView)
	{
		aView.present(projection(2));
		settle();
		expect(aView.tabs()->tabs()[0].OwnPair == QStringLiteral("R1"), "a tab on an older revision says so");
		expect(aView.notes()->item(0)->text().contains(QStringLiteral("Appears 2 times in R2; none chosen")), "ambiguous correspondence stated");
		select(aView, 18, 25);
		aView.suggest_action()->trigger();
		expect(aView.composer()->source_status() == qt::suggestion_source_status::newer_available && aView.selection_anchor()->Revision == core::cycle_source_revision_id{ 1 }, "newer material stated; anchor stays on R1");
		capture(aView, QStringLiteral("cycle-newer-material"));
	}

	struct view_gestures
	{
		std::vector<bool>                     Captures;
		std::vector<core::cycle_view_request> Compared;
		int                                   FullSource   = 0;
		int                                   Refreshed    = 0;
		int                                   MarkedReview = 0;
	};

	auto menu_action(QMenu* aMenu, const QString& aText) -> QAction*
	{
		for (auto* const action : aMenu->actions()) {
			if (action->text() == aText) {
				return action;
			}
		}
		return nullptr;
	}

	void captures_and_compares(qt::review_cycle_view& aView, const view_gestures& aGestures)
	{
		auto view           = projection(3);
		view.Facts.Rounds   = { { .Round = core::cycle_round_id{ 1 }, .Start = core::cycle_source_revision_id{ 1 } } };
		view.History        = core::cycle_history{ .Entries = {
													   { .Revision = core::cycle_source_revision_id{ 1 }, .Step = core::cycle_revision_step::initial, .Coverage = core::cycle_capture_coverage::complete },
													   { .Revision = core::cycle_source_revision_id{ 2 }, .Parent = core::cycle_source_revision_id{ 1 }, .Step = core::cycle_revision_step::head_moved, .Coverage = core::cycle_capture_coverage::complete },
													   { .Revision = core::cycle_source_revision_id{ 3 }, .Parent = core::cycle_source_revision_id{ 2 }, .Step = core::cycle_revision_step::branch_changed, .HeadReference = "refs/heads/feature", .SameMaterial = true, .Coverage = core::cycle_capture_coverage::complete },
												   } };
		view.NewerAvailable = true;
		aView.present(view);
		settle();
		expect(aView.capture_action()->isEnabled() && aView.mark_reviewed_action()->isEnabled() && aView.view_button()->isEnabled(), "capture, mark reviewed and the view chooser offered");
		expect(aView.refresh_action()->isVisible(), "newer material offered on an older full-source tab");
		aView.capture_action()->trigger();
		aView.capture_again_action()->trigger();
		expect(aGestures.Captures == std::vector<bool>{ false, true }, "capture, and a deliberate observation of the same bytes");
		aView.updates_toggle()->click();
		expect(aView.updates_line()->text().contains(QStringLiteral("R2 (head moved; commits between not captured)")) && aView.updates_line()->text().contains(QStringLiteral("R3 (other branch feature; same bytes)")),
			   "the Updates line keeps the chronologies explicit");
		aView.updates_toggle()->click();

		// The chooser: since my last review needs a checkpoint first.
		auto* const menu = aView.view_button()->menu();
		emit        menu->aboutToShow();
		expect(!menu_action(menu, QStringLiteral("Since my last review"))->isEnabled(), "no checkpoint, no since-my-last-review");
		expect(!menu_action(menu, QStringLiteral("Full source"))->isEnabled(), "already full source");
		menu_action(menu, QStringLiteral("All cycle changes"))->trigger();
		auto* const round = menu_action(menu, QStringLiteral("This round"))->menu();
		round->actions().front()->trigger();
		auto* const chosen = menu_action(menu, QStringLiteral("Choose revisions"))->menu();
		chosen->actions().front()->menu()->actions().back()->trigger();
		expect(aGestures.Compared.size() == 3U && std::holds_alternative<core::all_cycle_request>(aGestures.Compared[0]) && std::get<core::this_round_request>(aGestures.Compared[1]).Round == core::cycle_round_id{ 1 }
				   && std::get<core::chosen_pair_request>(aGestures.Compared[2]) == core::chosen_pair_request{ .Left = core::cycle_source_revision_id{ 1 }, .Right = core::cycle_source_revision_id{ 3 } },
			   "the chooser dispatches each named view");
		aView.mark_reviewed_action()->trigger();
		expect(aGestures.MarkedReview == 1, "mark reviewed");

		// A pinned comparison: rows in the same pane, no refresh offered.
		view.Facts.LastReview = core::cycle_review_checkpoint{ .Ordinal = 1, .Revision = core::cycle_source_revision_id{ 2 } };
		view.Tabs[0].View     = { .Revision = core::cycle_source_revision_id{ 2 }, .Path = "src/parser.cpp", .Comparison = core::cycle_tab_comparison{ .Request = core::chosen_pair_request{ .Left = core::cycle_source_revision_id{ 1 }, .Right = core::cycle_source_revision_id{ 2 } }, .Left = core::cycle_source_revision_id{ 1 } } };
		view.Source.reset();
		view.Comparison = application::review_cycle_comparison_view{
			.Tab     = core::document_tab_id{ 1 },
			.Request = core::chosen_pair_request{ .Left = core::cycle_source_revision_id{ 1 }, .Right = core::cycle_source_revision_id{ 2 } },
			.Pair    = { .Left = core::cycle_source_revision_ref{ .Cycle = core::review_cycle_id{ 1 }, .Id = core::cycle_source_revision_id{ 1 } }, .Right = { .Cycle = core::review_cycle_id{ 1 }, .Id = core::cycle_source_revision_id{ 2 } } },
			.Path    = "src/parser.cpp",
			.Pinned  = true,
			.Status  = application::review_material_status::available,
			.File    = core::cycle_path_comparison{ .Path = "src/parser.cpp", .Change = core::cycle_path_change::modified },
			.Rows    = { { .Kind = core::cycle_diff_row_kind::context, .LeftLine = 1U, .RightLine = 1U, .Text = "int f()" },
						 { .Kind = core::cycle_diff_row_kind::removed, .LeftLine = 3U, .Text = "\treturn caf\xC3\xA9(1);" },
						 { .Kind = core::cycle_diff_row_kind::added, .RightLine = 3U, .Text = "\treturn coffee(1);" } },
		};
		aView.present(view);
		settle();
		expect(aView.comparison()->isVisible() && aView.comparison()->toPlainText() == QStringLiteral("    1     1   int f()\n    3       - \treturn café(1);\n          3 + \treturn coffee(1);"), "rows with both line numbers and signs");
		expect(aView.comparison()->extraSelections().size() == 2, "added and removed rows marked");
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleRevisionLabel"))->text() == QStringLiteral("Chosen revisions · R1 → R2 (pinned)"), "the pinned pair named");
		expect(!aView.refresh_action()->isVisible(), "a pinned pair offers no refresh");
		expect(aView.tabs()->tabs()[0].OwnPair == QStringLiteral("R1→R2"), "the tab names its pair");
		expect(!aView.comment_action()->isEnabled(), "notes are made in full source");
		emit menu->aboutToShow();
		expect(menu_action(menu, QStringLiteral("Since my last review (R2)"))->isEnabled(), "since my last review once marked");
		menu_action(menu, QStringLiteral("Full source"))->trigger();
		expect(aGestures.FullSource == 1, "back to full source");
		capture(aView, QStringLiteral("cycle-comparison"));

		// A semantic view with newer material offers the refresh.
		view.Comparison->Request = core::all_cycle_request{ };
		view.Comparison->Pinned  = false;
		aView.present(view);
		settle();
		expect(aView.refresh_action()->isVisible(), "a semantic view with newer material offers the refresh");
		aView.refresh_action()->trigger();
		expect(aGestures.Refreshed == 1, "refresh dispatched");
		view.Comparison->File = core::cycle_path_comparison{ .Path = "src/parser.cpp", .Change = core::cycle_path_change::unknown };
		view.Comparison->Rows.clear();
		aView.present(view);
		settle();
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleSourceNotice"))->text().contains(QStringLiteral("no change is claimed")), "an unknown side claims no change");
	}

	void opens_files_and_steps(qt::review_cycle_view& aView, std::string& aOpened)
	{
		aView.findChild<QAction*>(QStringLiteral("cyclePreviousFile"))->trigger();
		expect(aOpened == "README.md", "previous file");
		auto* const item = aView.files()->item(1);
		emit        aView.files()->itemActivated(item);
		expect(aOpened == "src/parser.cpp", "files drawer opens a file");
		expect(!aView.source()->accessibleName().isEmpty() && !aView.files()->accessibleName().isEmpty() && !aView.notes()->accessibleName().isEmpty(), "named regions");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	auto         view      = qt::review_cycle_view{ };
	auto         noted     = core::begin_cycle_feedback{ };
	auto         suggested = application::review_cycle_suggestion_request{ };
	auto         opened    = std::string{ };
	auto         gestures  = view_gestures{ };
	view.set_dispatch({
		.OpenFile     = [&](const std::string& aPath, bool) { opened = aPath; },
		.Tab          = [](const core::document_tab_intent&) { },
		.ObserveLine  = [](core::document_tab_id, std::uint64_t) { },
		.Comment      = [&](core::begin_cycle_feedback aNote) { noted = std::move(aNote); },
		.Suggest      = [&](application::review_cycle_suggestion_request aRequest) { suggested = std::move(aRequest); },
		.Relate       = { },
		.Capture      = [&](const bool aDeliberate) { gestures.Captures.push_back(aDeliberate); },
		.Compare      = [&](const core::cycle_view_request& aRequest) { gestures.Compared.push_back(aRequest); },
		.FullSource   = [&] { ++gestures.FullSource; },
		.Refresh      = [&] { ++gestures.Refreshed; },
		.MarkReviewed = [&] { ++gestures.MarkedReview; },
	});
	view.resize(1280, 760);
	view.show();
	settle();
	presents_the_scaffold_and_notes(view);
	anchors_exact_bytes_from_a_native_selection(view, suggested, noted);
	states_newer_material_without_moving(view);
	opens_files_and_steps(view, opened);
	captures_and_compares(view, gestures);
	// The narrowest window the workbench supports still shows the three regions.
	view.resize(900, 600);
	settle();
	expect(view.minimumSizeHint().width() <= 900, "fits the compact window");
	capture(view, QStringLiteral("cycle-compact-900"));
	std::cout << "review-cycle-view-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
