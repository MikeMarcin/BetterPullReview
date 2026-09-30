#include "../support/test_expectation.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/qt/review_cycle_view.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTextCursor>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

/// \file
/// \brief The local cycle page (BPR-1157) presented from a projection:
/// review bar, Updates line, tabs, one full-source pane, notes, and exact byte
/// anchors from native selections, through real Qt events.

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
	view.set_dispatch({
		.OpenFile    = [&](const std::string& aPath, bool) { opened = aPath; },
		.Tab         = [](const core::document_tab_intent&) { },
		.ObserveLine = [](core::document_tab_id, std::uint64_t) { },
		.Comment     = [&](core::begin_cycle_feedback aNote) { noted = std::move(aNote); },
		.Suggest     = [&](application::review_cycle_suggestion_request aRequest) { suggested = std::move(aRequest); },
	});
	view.resize(1280, 760);
	view.show();
	settle();
	presents_the_scaffold_and_notes(view);
	anchors_exact_bytes_from_a_native_selection(view, suggested, noted);
	states_newer_material_without_moving(view);
	opens_files_and_steps(view, opened);
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
