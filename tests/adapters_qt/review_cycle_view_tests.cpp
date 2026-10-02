#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/review_cycle_view.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTabBar>
#include <QToolButton>
#include <QWindow>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/// \file
/// \brief The local cycle page (BPR-1157) presented from a projection:
/// review bar, Updates line, tabs, one full-source pane, notes, and exact byte
/// anchors from native selections, through real Qt events. BPR-1163: Capture
/// revision, the view chooser, Mark reviewed, the newer-material refresh and
/// the comparison pane, in the same review bar and pane. The source pane
/// is coloured only from a classification of the exact bytes it shows.

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

	/// Selects bytes [aFrom, aTo) of the shown file in the source pane, as a drag would.
	void select(qt::review_cycle_view& aView, const std::uint64_t aFrom, const std::uint64_t aTo)
	{
		aView.source()->select_bytes(aFrom, aTo);
		settle();
	}

	void presents_the_scaffold_and_notes(qt::review_cycle_view& aView)
	{
		aView.present(projection(1));
		settle();
		expect(aView.review_bar() != nullptr && aView.review_bar()->role() == qt::strip_role::review_bar, "the scaffold's review bar");
		expect(aView.source()->text() == parser && aView.source()->model().state().Lines.size() == 5U && aView.source()->read_only(), "the exact bytes, one line per line whatever the ending, read-only");
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleRevisionLabel"))->text() == QStringLiteral("Full source · R1"), "full source names its revision");
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleFilePosition"))->text() == QStringLiteral("File 2 of 2"), "file position");
		expect(aView.tabs()->tabs().size() == 1U && aView.tabs()->tabs()[0].Active && aView.tabs()->tabs()[0].OwnPair.isEmpty(), "one tab on the latest revision");
		expect(aView.notes()->count() == 2 && aView.notes()->item(0)->text().contains(QStringLiteral("line 3")) && aView.notes()->item(0)->text().contains(QStringLiteral("Suggested change")), "anchored note with its line");
		expect(aView.notes()->item(1)->text().startsWith(QStringLiteral("Whole cycle")) && aView.notes()->item(1)->text().contains(QStringLiteral("self-reported")), "cycle note without a line; provenance shown");
		expect(!aView.compose_action()->isEnabled() && aView.comment_action()->isEnabled() && aView.suggest_action()->isEnabled(), "Compose is not offered yet; Suggest change needs only the caret");
		expect(!aView.updates_line()->isVisible(), "Updates line folded");
		aView.updates_toggle()->click();
		expect(aView.updates_line()->isVisible() && aView.updates_line()->text().contains(QStringLiteral("Captured: R1")), "Updates line unfolds");
		capture(aView, QStringLiteral("cycle-full-source"));
		aView.updates_toggle()->click();
	}

	/// Within a small distance of `aColour`. Not `near`: the Windows SDK
	/// defines `near` as an empty macro.
	auto colour_near(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 24 && std::abs(aPixel.green() - aColour.green()) <= 24 && std::abs(aPixel.blue() - aColour.blue()) <= 24;
	}

	/// \brief Whether the first `aWord` in the code view is painted in
	/// `aColour`: inside the word's own cells at least one pixel is near
	/// `aColour` and none is near the plain text colour.
	/// \details Relative to the word's own ink rather than a pixel count, so it
	/// holds at 1x and 2x and in any face: anti-aliasing blends a role colour
	/// toward the background, never toward the text colour. The same rule
	/// holds under a selection, which is a wash under the glyphs.
	auto bytes_painted(qt::suggestion_editor& aView, const std::size_t aOffset, const std::size_t aLength, const QColor& aColour) -> bool
	{
		const auto left   = aView.cell_rectangle(aView.model().point_of(aOffset));
		const auto right  = aView.cell_rectangle(aView.model().point_of(aOffset + aLength));
		const auto pixmap = aView.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto plain  = aView.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       other  = 0;
		for (auto y = static_cast<int>(std::ceil(left.top() * ratio)); y < static_cast<int>(left.bottom() * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(std::ceil(left.left() * ratio)); x < static_cast<int>(right.left() * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				other += colour_near(pixel, plain) ? 1 : 0;
			}
		}
		return role > 0 && other == 0;
	}

	auto word_painted(qt::suggestion_editor& aView, const QString& aWord, const QColor& aColour) -> bool
	{
		const auto word  = aWord.toUtf8();
		const auto found = aView.text().find(std::string_view{ word.constData(), static_cast<std::size_t>(word.size()) });
		return found != std::string::npos && bytes_painted(aView, found, static_cast<std::size_t>(word.size()), aColour);
	}

	/// `aTop` laid over `aBase`, as a wash with alpha is painted.
	auto over(const QColor& aTop, const QColor& aBase) -> QColor
	{
		const auto alpha = aTop.alphaF();
		return QColor::fromRgbF(static_cast<float>(aTop.redF() * alpha + aBase.redF() * (1.0 - alpha)), static_cast<float>(aTop.greenF() * alpha + aBase.greenF() * (1.0 - alpha)),
								static_cast<float>(aTop.blueF() * alpha + aBase.blueF() * (1.0 - alpha)));
	}

	/// A flat fill has no anti-aliasing: a few units of rounding at most.
	auto fill_is(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 3 && std::abs(aPixel.green() - aColour.green()) <= 3 && std::abs(aPixel.blue() - aColour.blue()) <= 3;
	}

	/// The pixel at the middle of one cell of the code view.
	auto cell_pixel(qt::suggestion_editor& aView, const QImage& aImage, const core::text_point& aPoint) -> QColor
	{
		const auto middle = aView.cell_rectangle(aPoint).center() * aImage.devicePixelRatio();
		return aImage.pixelColor(static_cast<int>(middle.x()), static_cast<int>(middle.y()));
	}

	/// Pixels that differ between two renders of the code view, outside the
	/// columns around the caret, whose colour follows focus.
	auto differing_pixels(const QImage& aBefore, const QImage& aAfter, const QRect& aCaret) -> int
	{
		if (aBefore.size() != aAfter.size()) {
			return -1;
		}
		const auto ratio = aBefore.devicePixelRatio();
		const auto caret = QRectF{ QPointF{ aCaret.left() - 2.0, static_cast<qreal>(aCaret.top()) }, QPointF{ aCaret.right() + 3.0, aCaret.bottom() + 1.0 } };
		auto       count = 0;
		for (auto y = 0; y < aBefore.height(); ++y) {
			for (auto x = 0; x < aBefore.width(); ++x) {
				if (!caret.contains(QPointF{ x / ratio, y / ratio }) && aBefore.pixel(x, y) != aAfter.pixel(x, y)) {
					++count;
				}
			}
		}
		return count;
	}

	/// \brief The owner rejected composer-layout v004 (2026-10-02): the text
	/// changed when Suggest change began, selected lines lost their syntax
	/// colours, and selection behaved differently in the two modes. The file
	/// is now read and edited in one code view.
	/// \details Rendered before and after Suggest change at the same scroll
	/// position, the pane is the same pixel for pixel but for the caret; a
	/// selected keyword keeps its role colour by the local role/text pixel
	/// rule, in the selection's ink; and the selection is the solid
	/// `Highlight` every text box selects with (owner note on v005).
	void suggesting_changes_nothing_on_screen()
	{
		auto       view = qt::review_cycle_view{ };
		const auto look = qt::appearance_profile{ };
		view.adopt_appearance(look);
		view.resize(1280, 760);
		view.show();
		auto file  = std::string{ };
		auto spans = application::token_span_list{ };
		for (auto block = 0U; block < 80U; ++block) {
			const auto number = std::to_string(block);
			file.append("int f").append(number).append("()\r\n{\r\n\treturn caf\xC3\xA9(").append(number).append(");\r\n}\r\n");
			spans.push_back({ .Line = 4U * block + 2U, .Column = 1U, .Length = 6U, .Role = core::appearance_role::syntax_control_flow });
			spans.push_back({ .Line = 4U * block + 2U, .Column = 8U, .Length = 5U, .Role = core::appearance_role::syntax_function });
			spans.push_back({ .Line = 4U * block + 2U, .Column = 14U, .Length = static_cast<std::uint32_t>(number.size()), .Role = core::appearance_role::syntax_number });
		}
		auto shown          = projection(1);
		shown.Source->Bytes = bytes(file);
		view.present(shown);
		settle();
		expect(view.adopt_source_syntax(file, std::make_shared<const application::token_span_list>(std::move(spans))), "classified");
		auto* const code = view.source();
		// Line 42 is "\treturn café(10);": select "return café(10)", a third of the way down.
		const auto line   = file.find("\treturn caf\xC3\xA9(10);") + 1U;
		const auto length = std::string_view{ "return caf\xC3\xA9(10)" }.size();
		const auto flow   = look.color(core::appearance_role::syntax_control_flow);
		const auto base   = code->palette().color(QPalette::Base);
		const auto fill   = code->palette().color(QPalette::Highlight);
		// The control-flow colour as it reads on the selection, moved only as
		// far as the variant's text floor needs.
		const auto ink = [&] {
			const auto channel = [](const QColor& aSource) {
				return core::appearance_color{ static_cast<std::uint8_t>(aSource.red()), static_cast<std::uint8_t>(aSource.green()), static_cast<std::uint8_t>(aSource.blue()), 255U };
			};
			const auto readable = core::readable_selection_ink(channel(flow), channel(fill), look.variant());
			return QColor{ readable.Red, readable.Green, readable.Blue };
		}();
		code->scroll_to_line(30U);
		code->select_bytes(line, line + length);
		code->setFocus(Qt::MouseFocusReason);
		settle();
		auto wash = fill;
		wash.setAlpha(core::selection_wash_alpha);
		expect(code->first_visible_line() == 30U && code->has_selection(), "a selection in view, scrolled down");
		expect(!fill_is(fill, base) && !fill_is(over(wash, base), fill), "the selection differs from the background and from the panes' line wash");
		expect(ink != code->palette().color(QPalette::Text), "the selection's ink for control flow is not plain text");
		const auto before = code->viewport()->grab().toImage();
		capture(*code, QStringLiteral("cycle-code-view-reading"));
		expect(bytes_painted(*code, line, 6U, ink), "reading: the selected keyword keeps its role, in the selection's ink");
		expect(fill_is(cell_pixel(*code, before, { 42U, 10U }), fill), "reading: the selection is the solid Highlight a text box selects with");
		expect(fill_is(cell_pixel(*code, before, { 41U, 30U }), base), "and outside it the code background");

		view.suggest_action()->trigger();
		settle();
		expect(view.suggesting() && code == view.composer()->editor() && !code->read_only() && view.window()->focusWidget() == code, "the same view, now editable and focused");
		expect(code->first_visible_line() == 30U && code->selected_bytes() == std::pair<std::uint64_t, std::uint64_t>{ line, line + length }, "the same scroll position and selection");
		const auto after = code->viewport()->grab().toImage();
		capture(*code, QStringLiteral("cycle-code-view-suggesting"));
		expect(bytes_painted(*code, line, 6U, ink), "editing: the selected keyword keeps its role, in the selection's ink");
		expect(cell_pixel(*code, after, { 42U, 10U }) == cell_pixel(*code, before, { 42U, 10U }), "the selection fill is identical in both modes");
		const auto changed = differing_pixels(before, after, code->caret_rectangle());
		if (changed != 0) {
			std::cerr << "differing pixels outside the caret: " << changed << '\n';
		}
		expect(changed == 0, "the pane is the same pixel for pixel but for the caret");
		view.findChild<QPushButton*>(QStringLiteral("suggestionCancel"))->click();
		settle();
		expect(!view.suggesting() && code->read_only() && code->first_visible_line() == 30U && differing_pixels(before, code->viewport()->grab().toImage(), code->caret_rectangle()) == 0, "and back to reading without a change");
	}

	/// \brief [[BPR-1157]]: the owner rejected a plain source pane. The page
	/// asks for its file's classification and paints only spans of the
	/// exact bytes it shows, through the review panes' role colours.
	void colours_the_source_from_its_classification()
	{
		auto       view         = qt::review_cycle_view{ };
		auto       requests     = std::vector<qt::source_syntax_request>{ };
		const auto look         = qt::appearance_profile{ };
		view.SourceSyntaxWanted = [&requests](const qt::source_syntax_request& aRequest) { requests.push_back(aRequest); };
		view.adopt_appearance(look);
		view.resize(1280, 760);
		view.show();
		view.present(projection(1));
		settle();
		expect(requests.size() == 1U && requests[0].Path == "src/parser.cpp" && requests[0].Document == parser, "the shown file is asked for, by path and exact bytes");
		const auto flow     = look.color(core::appearance_role::syntax_control_flow);
		const auto function = look.color(core::appearance_role::syntax_function);
		const auto number   = look.color(core::appearance_role::syntax_number);
		expect(flow != view.source()->palette().color(QPalette::Text) && function != view.source()->palette().color(QPalette::Text), "the roles differ from plain text");
		expect(!word_painted(*view.source(), QStringLiteral("return"), flow), "plain until its classification arrives");
		// Classifier lines split at LF: line 2 is "\treturn caf\xC3\xA9(1);\r", so
		// the number sits after a two-byte character.
		const auto spans = std::make_shared<const application::token_span_list>(application::token_span_list{
			{ .Line = 2U, .Column = 1U, .Length = 6U, .Role = core::appearance_role::syntax_control_flow },
			{ .Line = 2U, .Column = 8U, .Length = 5U, .Role = core::appearance_role::syntax_function },
			{ .Line = 2U, .Column = 14U, .Length = 1U, .Role = core::appearance_role::syntax_number },
		});
		expect(!view.adopt_source_syntax("int g()\n", spans), "spans of other bytes are refused");
		settle();
		expect(!word_painted(*view.source(), QStringLiteral("return"), flow), "and paint nothing");
		expect(view.adopt_source_syntax(parser, spans), "spans of the shown bytes are adopted");
		settle();
		expect(word_painted(*view.source(), QStringLiteral("return"), flow), "return in the control-flow colour");
		expect(word_painted(*view.source(), QStringLiteral("caf\u00E9"), function), "the call in the function colour");
		expect(word_painted(*view.source(), QStringLiteral("1"), number), "the number after a two-byte character in its own colour");
		capture(view, QStringLiteral("cycle-source-syntax"));
		view.present(projection(1));
		settle();
		expect(requests.size() == 1U && word_painted(*view.source(), QStringLiteral("return"), flow), "the same bytes again keep their colours and ask nothing");
		auto other          = projection(1);
		other.Source->Bytes = bytes("int g()\r\n{\r\n\treturn caf\xC3\xA9(2);\r\n}\r\n");
		view.present(other);
		settle();
		expect(requests.size() == 2U && !word_painted(*view.source(), QStringLiteral("return"), flow), "other bytes go plain and are asked for");
		expect(!view.adopt_source_syntax(parser, spans), "a late classification of the earlier bytes is refused");
	}

	/// A file too large to edit is still read whole in the same view, and
	/// Suggest change is not offered for it.
	void reads_a_file_too_large_to_edit()
	{
		auto view = qt::review_cycle_view{ };
		view.resize(1280, 760);
		view.show();
		auto large          = projection(1);
		auto file           = std::string(core::maximum_suggestion_editor_bytes, 'x') + "\nlast line\n";
		large.Source->Bytes = bytes(file);
		view.present(large);
		settle();
		expect(view.source()->text() == file && view.source()->read_only(), "held whole, never cut");
		view.source()->setFocus(Qt::OtherFocusReason);
		view.source()->apply(core::move_carets{ .Motion = core::text_motion::document_end, .Extend = true });
		expect(view.source()->selected_bytes() == std::pair<std::uint64_t, std::uint64_t>{ 0U, file.size() } && !view.suggest_action()->isEnabled() && view.comment_action()->isEnabled(), "selected and commented on, not suggested on");
	}

	void anchors_exact_bytes_from_a_native_selection(qt::review_cycle_view& aView, application::review_cycle_suggestion_request& aSuggested, core::begin_cycle_feedback& aNoted)
	{
		// "café(1)" is bytes 20..28 of the CRLF file.
		select(aView, 20, 28);
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

		// Suggest change from the selection: the whole file is edited in the
		// main pane, with the selection selected again (owner amendment to
		// BPR-1156, 2026-10-01).
		select(aView, 20, 28);
		aView.suggest_action()->trigger();
		settle();
		auto* const editor = aView.composer()->editor();
		expect(aView.suggesting() && aView.composer()->isVisible() && editor == aView.source() && editor->isVisible() && !editor->read_only(), "the source pane itself becomes the editor");
		expect(editor->text() == parser && aView.window()->focusWidget() == editor, "the whole file, focused");
		expect(editor->model().state().Carets.size() == 1U && editor->model().state().Carets[0].Anchor == core::text_point{ 2U, 11U } && editor->model().state().Carets[0].Head == core::text_point{ 2U, 18U },
			   "the selection selected again");
		expect(!aView.suggest_action()->isEnabled() && !aView.comment_action()->isEnabled(), "one suggestion at a time");
		editor->apply(core::insert_text{ "coffee(1)" });
		capture(aView, QStringLiteral("cycle-suggest-change"));
		aView.composer()->save_button()->click();
		settle();
		const auto line = *core::make_anchor(core::cycle_source_revision_id{ 1 }, "src/parser.cpp", bytes(parser), 12, 31);
		expect(aSuggested.Edits.size() == 1U && aSuggested.Edits[0].Selection == line && aSuggested.Edits[0].Replacement == "\treturn coffee(1);\r\n"
				   && aSuggested.Intent == core::cycle_suggestion_intent::exact_replacement,
			   "the changed line handed over exactly, whole");
		expect(!aView.suggesting() && aView.source()->isVisible() && aView.source()->read_only() && aView.source()->text() == parser, "the source pane back after Save, read-only, on the captured bytes");
	}

	/// The point of byte `aOffset` of the shown file in the source pane, in its viewport.
	auto point_of(qt::review_cycle_view& aView, const std::uint64_t aOffset) -> QPoint
	{
		const auto cell = aView.source()->cell_rectangle(aView.source()->model().point_of(aOffset));
		return QPointF{ cell.left() + 1.0, cell.center().y() }.toPoint();
	}

	/// Owner amendment to BPR-1156 (2026-10-01): Suggest change from a caret
	/// and from the source pane's context menu, and several separate changes
	/// in one file become one note's exact suggestions.
	void suggests_from_the_caret_and_the_context_menu(qt::review_cycle_view& aView, application::review_cycle_suggestion_request& aSuggested)
	{
		aView.present(projection(1));
		select(aView, 20, 28);
		// A right click outside the selection moves the caret there, as a click would.
		auto* menu = aView.source_menu(point_of(aView, 3));
		expect(menu != nullptr && !aView.source()->has_selection() && aView.source()->selected_bytes().first == 3U, "the caret moved to the click");
		const auto actions = menu->actions();
		expect(actions.size() > 3 && actions[0]->objectName() == QStringLiteral("cycleSourceComment") && actions[0]->text() == QStringLiteral("Comment") && actions[1]->objectName() == QStringLiteral("cycleSourceSuggest")
				   && actions[1]->text() == aView.suggest_action()->text() && actions[2]->isSeparator(),
			   "Comment and Suggest change first in the menu");
		// [[BPR-1157]]. Each shows the key that does the same in the code.
		expect(actions[0]->shortcut().count() == 1 && (actions[0]->shortcut()[0].key() == Qt::Key_Return || actions[0]->shortcut()[0].key() == Qt::Key_Enter)
				   && actions[0]->shortcut()[0].keyboardModifiers() == Qt::NoModifier && actions[1]->shortcut()[0].key() == actions[0]->shortcut()[0].key()
				   && actions[1]->shortcut()[0].keyboardModifiers() == Qt::ShiftModifier && actions[0]->isShortcutVisibleInContextMenu() && actions[1]->isShortcutVisibleInContextMenu(),
			   "Return and Shift+Return shown beside them");
		expect(aView.comment_action()->toolTip().contains(QStringLiteral("caret's line")) && aView.suggest_action()->toolTip().contains(QStringLiteral("Shift+")), "and named in the review bar's tooltips");
		expect(std::ranges::any_of(actions, [](const QAction* aAction) { return aAction->text().contains(QStringLiteral("Copy")); }), "then the pane's own Copy");
		expect(aView.suggest_action()->isEnabled(), "offered with only a caret");
		capture(aView, QStringLiteral("cycle-source-menu"));
		actions[1]->trigger();
		delete menu;
		settle();
		auto* const editor = aView.composer()->editor();
		expect(aView.suggesting() && editor->isVisible() && editor->text() == parser, "the whole file from the menu");
		expect(editor->model().state().Carets.size() == 1U && editor->model().state().Carets[0].empty() && editor->model().state().Carets[0].Head == core::text_point{ 0U, 3U }, "a caret where the click was");
		// Two separate changes: the first line and the third.
		editor->apply(core::insert_text{ "_impl" });
		editor->apply(core::place_caret{ .At = { 2U, 11U } });
		editor->apply(core::place_caret{ .At = { 2U, 18U }, .Extend = true });
		editor->apply(core::insert_text{ "coffee(2)" });
		settle();
		expect(aView.composer()->summary_text() == QStringLiteral("2 changes · lines 1, 3"), "two changes found");
		aView.findChild<QPlainTextEdit*>(QStringLiteral("suggestionRationale"))->setPlainText(QStringLiteral("Name it for what it does."));
		capture(aView, QStringLiteral("cycle-suggest-two-changes"));
		aView.composer()->save_button()->click();
		settle();
		const auto first = *core::make_anchor(core::cycle_source_revision_id{ 1 }, "src/parser.cpp", bytes(parser), 0, 9);
		const auto third = *core::make_anchor(core::cycle_source_revision_id{ 1 }, "src/parser.cpp", bytes(parser), 12, 31);
		expect(aSuggested.Edits.size() == 2U && aSuggested.Edits[0].Selection == first && aSuggested.Edits[0].Replacement == "int_impl f()\r\n" && aSuggested.Edits[1].Selection == third
				   && aSuggested.Edits[1].Replacement == "\treturn coffee(2);\r\n" && aSuggested.Rationale == "Name it for what it does.",
			   "one note, two exact suggestions in the original's coordinates");

		// Cancel gives the pane back with nothing handed over.
		aSuggested = { };
		aView.suggest_action()->trigger();
		editor->apply(core::insert_text{ "x" });
		aView.findChild<QPushButton*>(QStringLiteral("suggestionCancel"))->click();
		settle();
		expect(aSuggested.Edits.empty() && !aView.suggesting() && aView.source()->read_only() && aView.source()->text() == parser, "cancelled; the edit is gone from the pane");
	}

	void states_newer_material_without_moving(qt::review_cycle_view& aView)
	{
		aView.present(projection(2));
		settle();
		expect(aView.tabs()->tabs()[0].OwnPair == QStringLiteral("R1"), "a tab on an older revision says so");
		expect(aView.notes()->item(0)->text().contains(QStringLiteral("Appears 2 times in R2; none chosen")), "ambiguous correspondence stated");
		select(aView, 20, 28);
		aView.suggest_action()->trigger();
		expect(aView.composer()->source_status() == qt::suggestion_source_status::newer_available && aView.composer()->editor()->text() == parser, "newer material stated; the edit stays on R1's bytes");
		capture(aView, QStringLiteral("cycle-newer-material"));
		// A capture arriving while editing moves nothing.
		aView.present(projection(3));
		settle();
		expect(aView.suggesting() && aView.composer()->editor()->isVisible() && aView.composer()->editor()->text() == parser, "still editing R1");
		aView.findChild<QPushButton*>(QStringLiteral("suggestionCancel"))->click();
		settle();
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

	struct reply_gestures
	{
		std::vector<core::cycle_round_id>                                      Chosen;
		std::vector<std::pair<core::cycle_round_id, core::cycle_manual_reply>> Replies;
	};

	/// BPR-1164: replies entered by hand, response files chosen, and what was
	/// said shown beside each note as claims.
	void records_replies(qt::review_cycle_view& aView, const reply_gestures& aGestures)
	{
		auto view                = projection(1);
		view.Facts.Rounds        = { { .Round = core::cycle_round_id{ 1 }, .Start = core::cycle_source_revision_id{ 1 } } };
		view.Notes[0].Statements = {
			{ .Response = core::cycle_response_id{ 1 }, .Round = core::cycle_round_id{ 1 }, .Channel = core::cycle_response_channel::response_file, .From = "Harness", .ClaimedAuthor = core::cycle_claimed_author{ .DisplayName = "Harness", .Kind = core::cycle_claimed_author_kind::agent, .Model = "Claude Opus (verified)" }, .Disposition = core::cycle_response_disposition::addressed, .Body = "Renamed.\nDetails follow.", .Evidence = { { .Kind = core::cycle_evidence_kind::test_report, .Value = "all passed" } } },
			{ .Response = core::cycle_response_id{ 2 }, .Round = core::cycle_round_id{ 1 }, .Channel = core::cycle_response_channel::manual_entry, .From = "Dana (chat)", .Body = "I would keep the name." },
		};
		view.Responses = {
			{ .Id = { .Cycle = core::review_cycle_id{ 1 }, .Id = core::cycle_response_id{ 1 } }, .Round = core::cycle_round_id{ 1 }, .Channel = core::cycle_response_channel::response_file, .ForeignId = "RESP-A", .From = "Harness", .ClaimedAuthor = core::cycle_claimed_author{ .DisplayName = "Harness", .Kind = core::cycle_claimed_author_kind::agent, .Model = "Claude Opus (verified)" }, .Items = 1, .ClaimedResult = "R2" },
			{ .Id = { .Cycle = core::review_cycle_id{ 1 }, .Id = core::cycle_response_id{ 3 } }, .Round = core::cycle_round_id{ 1 }, .Channel = core::cycle_response_channel::manual_entry, .From = "Lee (email)", .Body = "Looks fine overall." },
		};
		aView.present(view);
		settle();
		const auto note = aView.notes()->item(0)->text();
		expect(note.contains(QStringLiteral("F1 reply · Harness (claims agent Claude Opus (verified)), response file says addressed: Renamed.")), "a structured statement shown as a claim");
		expect(note.contains(QStringLiteral("Evidence reported, not run by BPR · 1")) && note.contains(QStringLiteral("F1 reply · Dana (chat), entered by hand replied: I would keep the name.")), "evidence reported; prose beside it");
		expect(aView.notes()->count() == 4 && aView.notes()->item(2)->text().contains(QStringLiteral("Says the result is R2 (not captured here)"))
				   && aView.notes()->item(2)->text().contains(QStringLiteral("items it does not name have no response")),
			   "the response's coverage and claimed result");
		expect(aView.notes()->item(3)->text().contains(QStringLiteral("Lee (email), entered by hand")), "a reply about the round as a whole");

		// Paste a reply about the selected note.
		aView.notes()->setCurrentRow(0);
		auto* const menu = aView.reply_button()->menu();
		emit        menu->aboutToShow();
		expect(menu->actions().size() == 2 && menu->actions()[0]->text() == QStringLiteral("Paste a reply to F1…"), "a reply per frozen round");
		menu->actions()[0]->trigger();
		settle();
		expect(aView.reply_editor()->isVisible() && aView.reply_about_note()->isChecked() && aView.reply_about_note()->text().contains(QStringLiteral("I1")), "the reply editor, about the selected note");
		aView.reply_from()->setText(QStringLiteral("Sam"));
		aView.reply_editor()->setPlainText(QStringLiteral("Done, but not tested."));
		capture(aView, QStringLiteral("cycle-reply"));
		aView.findChild<QPushButton*>(QStringLiteral("cycleReplySave"))->click();
		expect(aGestures.Replies.size() == 1U && aGestures.Replies[0].first == core::cycle_round_id{ 1 } && aGestures.Replies[0].second.From == "Sam"
				   && aGestures.Replies[0].second.Items == std::vector<std::string>{ "I1" } && aGestures.Replies[0].second.Body == "Done, but not tested.",
			   "the reply handed over as entered");
		emit menu->aboutToShow();
		menu->actions()[1]->trigger();
		expect(aGestures.Chosen == std::vector<core::cycle_round_id>{ core::cycle_round_id{ 1 } }, "a response file chosen for F1");
	}

	struct decision_gestures
	{
		std::vector<std::pair<core::cycle_feedback_item_ref, std::optional<core::cycle_reviewer_decision>>> Decisions;
		int                                                                                                 Prepared = 0;
	};

	/// BPR-1165: each fact about a note in its own words; decisions only by the reviewer.
	void decides_explicitly(qt::review_cycle_view& aView, const decision_gestures& aGestures)
	{
		auto view         = projection(3);
		view.Facts.Rounds = { { .Round = core::cycle_round_id{ 1 }, .Start = core::cycle_source_revision_id{ 1 } } };
		const auto item   = view.Notes[0].Item;
		auto       state  = core::cycle_item_reconciliation{
			.Item        = item.Id,
			.Against     = core::cycle_source_revision_id{ 3 },
			.Primary     = core::cycle_target_correspondence{ .Kind = core::cycle_correspondence::unmapped, .Against = core::cycle_source_revision_id{ 3 } },
			.Suggestions = { { .Suggestion = core::cycle_suggestion_id{ 1 }, .Observation = core::cycle_application_observation::already_applied } },
			.Claim       = core::cycle_response_disposition::addressed,
			.Status      = core::cycle_item_status::needs_verification,
		};
		view.Notes[0].Correspondence.reset();
		view.Notes[0].Reconciliation = state;
		aView.present(view);
		settle();
		auto text = aView.notes()->item(0)->text();
		expect(text.contains(QStringLiteral("Original code: Changed in R3; original kept")) && text.contains(QStringLiteral("Suggested result present in R3 (not a verification)"))
				   && text.contains(QStringLiteral("Recipient says addressed; needs your verification")),
			   "code gone, suggestion present and a claim: still only needs verification");
		auto* const menu = aView.decision_menu(0);
		expect(menu != nullptr && menu->actions().size() == 5 && menu->actions()[0]->text() == QStringLiteral("Verified in R1"), "decisions offered against the revision shown");
		menu->actions()[0]->trigger();
		menu->actions()[4]->trigger();
		expect(aGestures.Decisions.size() == 2U && aGestures.Decisions[0].first == item && aGestures.Decisions[0].second == core::cycle_reviewer_decision::verified && !aGestures.Decisions[1].second,
			   "verify and clear dispatched");
		state.Status                 = core::cycle_item_status::recheck_needed;
		state.Currency               = core::cycle_verification_currency::material_changed;
		state.Decision               = core::cycle_verification_record{ .Item = item.Id, .Wording = core::private_review_turn_revision_id{ 1 }, .Checked = core::cycle_source_revision_id{ 2 }, .Decision = core::cycle_reviewer_decision::verified };
		view.Notes[0].Reconciliation = state;
		view.NextRound               = application::review_cycle_next_round{ .From = core::cycle_round_id{ 1 }, .Carried = 3, .Added = 1, .Resolved = 1 };
		aView.present(view);
		settle();
		expect(aView.notes()->item(0)->text().contains(QStringLiteral("Verified by you in R2; its code changed since, check again")), "a verification that became history");
		expect(aView.next_round_button()->isEnabled(), "next round offered once a round was frozen");
		aView.next_round_button()->click();
		expect(aGestures.Prepared == 1, "next round prepared on the reviewer's gesture");
		aView.updates_toggle()->click();
		expect(aView.updates_line()->text().contains(QStringLiteral("next round draft saved from F1: 3 carried, 1 new, 1 resolved left out")), "the draft summarized");
		aView.updates_toggle()->click();
		capture(aView, QStringLiteral("cycle-verification"));
	}

	/// \brief [[BPR-1157]]: ring pixels inside the left and right edges of a
	/// control's render, over the middle third of its height.
	/// \details The band is one ring wide. A pixel counts when it is within 24
	/// of the ring's colour on every channel, the local-colour rule the
	/// syntax checks use, so anti-aliasing and scale do not matter.
	auto ring_pixels(const QImage& aImage, const QColor& aRing) -> int
	{
		const auto band  = static_cast<int>(std::ceil(qt::focus_ring_width * aImage.devicePixelRatio()));
		auto       count = 0;
		for (auto y = aImage.height() / 3; y < (2 * aImage.height()) / 3; ++y) {
			for (auto x = 0; x < band; ++x) {
				count += colour_near(aImage.pixelColor(x, y), aRing) ? 1 : 0;
				count += colour_near(aImage.pixelColor(aImage.width() - 1 - x, y), aRing) ? 1 : 0;
			}
		}
		return count;
	}

	/// \brief Whether `aOn` shows a ring that `aOff`, the same control
	/// unfocused, does not: the band gains ring pixels on most of its rows.
	auto ring_appears(const QImage& aOff, const QImage& aOn, const QColor& aRing) -> bool
	{
		const auto rows = (2 * aOn.height()) / 3 - aOn.height() / 3;
		return aOff.size() == aOn.size() && ring_pixels(aOn, aRing) - ring_pixels(aOff, aRing) >= rows;
	}

	/// The focus events accessibility was told of, in order.
	auto focus_events() -> std::vector<QObject*>&
	{
		static auto events = std::vector<QObject*>{ };
		return events;
	}

	void record_focus(QAccessibleEvent* const aEvent)
	{
		if (aEvent->type() == QAccessible::Focus) {
			focus_events().push_back(aEvent->object());
		}
	}

	/// \brief [[BPR-1157]]. The owner rejected native-keyboard v001 (event
	/// 794): "Tab does cycle between things but it's not obvious at all what
	/// has focus." Tab through the page with window-system keys, on the dark
	/// profile the owner reviewed in: every stop shows the window's focus
	/// ring when focus arrives and loses it when focus leaves, and
	/// accessibility is told of each by its own name.
	void shows_where_focus_is()
	{
		auto view = qt::review_cycle_view{ };
		auto look = qt::appearance_profile{ qt::default_appearance_settings(), { .Scheme = core::os_color_scheme::dark } };
		look.apply_to(view);
		view.adopt_appearance(look);
		view.resize(1280, 760);
		view.show();
		view.activateWindow();
		view.present(projection(1));
		settle();
		auto* const code = view.source();
		const auto  ring = qt::focus_ring_colour(code->palette(), code->backgroundRole());
		const auto  fill = code->palette().color(QPalette::Highlight);
		expect(!colour_near(ring, fill), "the ring is lifted off the dark selection colour, which is too faint to find");
		expect(code->accessibleName() == QStringLiteral("Source") && code->accessibleDescription().contains(QStringLiteral("caret's line")), "the reading pane is named Source and says what Return does");

		QAccessible::installUpdateHandler(&record_focus);
		code->setFocus(Qt::TabFocusReason);
		settle();
		auto stops = std::vector<QWidget*>{ code };
		for (auto step = 0; step < 60; ++step) {
			auto* const from = QApplication::focusWidget();
			// The next stop is not known before Tab moves, so every control is
			// rendered unfocused first and compared once focus arrives.
			auto before = std::vector<std::pair<QWidget*, QImage>>{ };
			for (auto* const control : view.findChildren<QWidget*>()) {
				if ((control->focusPolicy() & Qt::TabFocus) != 0 && control->isVisible() && control != from) {
					before.emplace_back(control, control->grab().toImage());
				}
			}
			const auto held = from->grab().toImage();
			focus_events().clear();
			tests::window_system_key(*from, Qt::Key_Tab);
			auto* const to = QApplication::focusWidget();
			if (to == code) {
				break;
			}
			const auto unfocused = std::ranges::find(before, to, &std::pair<QWidget*, QImage>::first);
			const auto name      = (to->objectName() + QStringLiteral(" ") + to->accessibleName()).toStdString();
			expect(unfocused != before.end() && view.isAncestorOf(to), "Tab stays on the page");
			// The review bar's controls draw the ring over the bar; the others over their own background.
			const auto surface = [&view](const QWidget* aControl) { return view.review_bar()->isAncestorOf(aControl) ? QPalette::Window : aControl->backgroundRole(); };
			const auto colour  = qt::focus_ring_colour(to->palette(), surface(to));
			if (!ring_appears(unfocused->second, to->grab().toImage(), colour)) {
				std::cerr << "no ring on " << name << '\n';
			}
			expect(ring_appears(unfocused->second, to->grab().toImage(), colour), "the focus ring appears on the stop Tab reaches");
			const auto fromColour = qt::focus_ring_colour(from->palette(), surface(from));
			expect(ring_appears(from->grab().toImage(), held, fromColour), "and leaves the stop it left");
			expect(!focus_events().empty() && focus_events().back() == to, "accessibility is told focus moved there");
			auto* const accessible = QAccessible::queryAccessibleInterface(to);
			expect(accessible != nullptr && (!accessible->text(QAccessible::Name).isEmpty() || !accessible->text(QAccessible::Description).isEmpty()), "and the stop has a name");
			stops.push_back(to);
		}
		QAccessible::installUpdateHandler(nullptr);
		std::cout << "focus stops ringed:";
		for (const auto* const stop : stops) {
			std::cout << ' ' << (stop->objectName().isEmpty() ? stop->accessibleName() : stop->objectName()).toStdString();
		}
		std::cout << '\n';
		expect(QApplication::focusWidget() == code, "Tab comes round to the code again");
		const auto reached = [&stops](QWidget* aWidget) { return std::ranges::find(stops, aWidget) != stops.end(); };
		expect(reached(view.files()) && reached(view.notes()) && reached(view.updates_toggle()) && reached(view.tabs()->findChild<QWidget*>(QStringLiteral("documentTabBar")))
				   && reached(view.review_bar()->widgetForAction(view.comment_action())) && reached(view.view_button()),
			   "the stops include the review bar, the tabs, the files, the code and the notes");

		// The rule refuses a stop whose ring is missing: the code view with its
		// ring hidden shows no more ring than it does unfocused.
		view.files()->setFocus(Qt::TabFocusReason);
		settle();
		const auto unfocusedCode = code->grab().toImage();
		code->setFocus(Qt::TabFocusReason);
		settle();
		auto* const codeRing = code->findChild<QWidget*>(QStringLiteral("cycleKeyboardFocus"), Qt::FindDirectChildrenOnly);
		expect(codeRing != nullptr && codeRing->isVisible() && ring_appears(unfocusedCode, code->grab().toImage(), ring), "the code view's ring");
		auto* const accessibleRing = QAccessible::queryAccessibleInterface(codeRing);
		expect(accessibleRing != nullptr && accessibleRing->state().invisible && !accessibleRing->state().focusable, "the ring is invisible decoration to accessibility");
		codeRing->hide();
		expect(!ring_appears(unfocusedCode, code->grab().toImage(), ring), "negative control: without its ring the code view is found unringed");
		codeRing->show();
		// A focused code view's caret is drawn in the text colour, an unfocused one's dimmed.
		const auto caret = [&] {
			const auto image = code->viewport()->grab().toImage();
			const auto at    = QPointF{ code->caret_rectangle().center() } * image.devicePixelRatio();
			return image.pixelColor(static_cast<int>(at.x()), static_cast<int>(at.y()));
		};
		expect(colour_near(caret(), code->palette().color(QPalette::Text)), "the reading caret is drawn in the text colour while focused");
		view.files()->setFocus(Qt::TabFocusReason);
		settle();
		expect(!colour_near(caret(), code->palette().color(QPalette::Text)) && !codeRing->isVisible(), "and dimmed, with no ring, once focus leaves");
		capture(view, QStringLiteral("cycle-focus-files"));
		code->setFocus(Qt::TabFocusReason);
		settle();
		capture(view, QStringLiteral("cycle-focus-code"));
	}

	/// \brief [[BPR-1157]]. The owner's suggestion, recorded as the decision
	/// (native-keyboard v001, event 794): reading the code, Return makes a
	/// note and Shift+Return suggests a change, where the caret is. Editing,
	/// Return still breaks the line and indents. Window-system keys.
	void return_notes_and_suggests_where_the_caret_is(qt::review_cycle_view& aView, core::begin_cycle_feedback& aNoted)
	{
		aView.present(projection(1));
		aView.activateWindow();
		settle();
		auto* const code = aView.source();
		code->setFocus(Qt::TabFocusReason);
		// The caret alone, on line 3: a note on that line, not on the file.
		code->select_bytes(24U, 24U);
		tests::window_system_key(*code, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
		expect(aView.comment_editor()->isVisible() && QApplication::focusWidget() == aView.comment_editor(), "Return opens the note composer, focused");
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleCommentTarget"))->text() == QStringLiteral("src/parser.cpp · line 3"), "on the caret's line");
		aView.comment_editor()->setPlainText(QStringLiteral("Explain this line."));
		aView.findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
		const auto* line = std::get_if<core::cycle_source_anchor>(&aNoted.Primary);
		expect(line != nullptr && line->Start == 12U && line->End == 29U && line->Original == "\treturn caf\xC3\xA9(1);" && aNoted.Body == "Explain this line.", "the line's exact bytes, without its CRLF");
		expect(code->read_only() && code->text() == parser, "the file unchanged");

		// A selection: a note on exactly it, with the keypad's Enter as well.
		code->setFocus(Qt::TabFocusReason);
		code->select_bytes(20U, 28U);
		tests::window_system_key(*code, Qt::Key_Enter, Qt::KeypadModifier, QStringLiteral("\r"));
		expect(aView.comment_editor()->isVisible() && aView.findChild<QLabel*>(QStringLiteral("cycleCommentTarget"))->text() == QStringLiteral("src/parser.cpp · line 3"), "Enter notes the selection");
		aView.comment_editor()->setPlainText(QStringLiteral("Rename."));
		aView.findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
		const auto* range = std::get_if<core::cycle_source_anchor>(&aNoted.Primary);
		expect(range != nullptr && range->Start == 20U && range->End == 28U, "the selection's exact bytes");

		// Negative controls: another modifier is not the gesture.
		aNoted = { };
		code->setFocus(Qt::TabFocusReason);
		code->select_bytes(24U, 24U);
		tests::window_system_key(*code, Qt::Key_Return, Qt::ControlModifier, QStringLiteral("\r"));
		tests::window_system_key(*code, Qt::Key_Return, Qt::AltModifier, QStringLiteral("\r"));
		expect(!aView.comment_editor()->isVisible() && !aView.suggesting() && QApplication::focusWidget() == code, "Ctrl+Return and Alt+Return open nothing");

		// Shift+Return: Suggest change from the caret, in the same view.
		code->select_bytes(13U, 13U);
		tests::window_system_key(*code, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
		expect(aView.suggesting() && !code->read_only() && QApplication::focusWidget() == code && code->model().state().Carets.size() == 1U && code->model().state().Carets[0].Head == core::text_point{ 2U, 4U },
			   "Shift+Return begins Suggest change at the caret");
		// Editing, Return is the editor's: a line break, indented after the brace.
		code->apply(core::place_caret{ .At = { 1U, 1U } });
		tests::window_system_key(*code, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
		expect(code->text() == "int f()\r\n{\r\n\t\r\n\treturn caf\xC3\xA9(1);\r\n}\r\n" && aView.suggesting() && !aView.comment_editor()->isVisible(), "editing, Return breaks the line and indents, and opens no note");
		tests::window_system_key(*code, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
		expect(aView.suggesting() && std::ranges::count(code->text(), '\n') == 6, "and Shift+Return is a line break too");
		aView.findChild<QPushButton*>(QStringLiteral("suggestionCancel"))->click();
		settle();
		expect(!aView.suggesting() && code->read_only() && code->text() == parser, "cancelled back to reading");

		// The context menu's Comment acts where the caret is, as Return does.
		auto* const menu = aView.source_menu(point_of(aView, 3));
		menu->actions()[0]->trigger();
		delete menu;
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleCommentTarget"))->text() == QStringLiteral("src/parser.cpp · line 1"), "the menu's Comment notes the clicked line");
		aView.findChild<QPushButton*>(QStringLiteral("cycleCommentCancel"))->click();
		// The review bar's Comment still notes the file when nothing is selected.
		aView.comment_action()->trigger();
		expect(aView.findChild<QLabel*>(QStringLiteral("cycleCommentTarget"))->text() == QStringLiteral("src/parser.cpp (whole file)"), "the review bar's Comment notes the file");
		aView.findChild<QPushButton*>(QStringLiteral("cycleCommentCancel"))->click();
		settle();
	}

	/// \brief [[BPR-1157]]. The owner's request with native-keyboard v002
	/// (event 806): Escape cancels the open note or suggestion, and on macOS
	/// Control-C does too. Qt names the Mac's Control key Meta; Qt's Control
	/// is Command-C there and Ctrl+C elsewhere, and stays Copy. Window-system
	/// keys, with the negative controls beside each gesture.
	void cancels_the_open_composer_from_the_keyboard(qt::review_cycle_view& aView, core::begin_cycle_feedback& aNoted, application::review_cycle_suggestion_request& aSuggested)
	{
		aView.present(projection(1));
		aView.activateWindow();
		settle();
		auto* const code     = aView.source();
		auto* const editor   = aView.comment_editor();
		auto* const keys     = aView.findChild<QShortcut*>(QStringLiteral("cycleComposerCancelKeys"));
		const auto  noteLine = [&] {
			code->setFocus(Qt::TabFocusReason);
			code->select_bytes(24U, 24U);
			tests::window_system_key(*code, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
		};
		expect(keys != nullptr && !keys->isEnabled(), "the cancel keys are claimed only while something is composed");
		expect(aView.findChild<QPushButton*>(QStringLiteral("cycleCommentCancel"))->toolTip().contains(QStringLiteral("Esc")) && aView.composer()->cancel_button()->toolTip().contains(QStringLiteral("Esc")),
			   "each Cancel names its key");

		// Escape in the note: nothing saved, and the code has focus again.
		aNoted = { };
		noteLine();
		expect(editor->isVisible() && QApplication::focusWidget() == editor && keys->isEnabled(), "Return opens the note, and claims the cancel keys");
		editor->setPlainText(QStringLiteral("A draft to drop."));
		tests::window_system_key(*editor, Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
		expect(!editor->isVisible() && editor->toPlainText().isEmpty() && aNoted.Body.empty() && QApplication::focusWidget() == code && !keys->isEnabled(),
			   "Escape in the note cancels it: nothing saved, the code focused again");

		// Negative controls. Copy is not cancel; and Escape with focus outside
		// the open note leaves it alone.
		noteLine();
		editor->setPlainText(QStringLiteral("Keep me."));
		editor->selectAll();
		QApplication::clipboard()->clear();
		tests::window_system_key(*editor, Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"));
		expect(editor->isVisible() && editor->toPlainText() == QStringLiteral("Keep me.") && QApplication::clipboard()->text() == QStringLiteral("Keep me."),
			   "Qt's Control+C (Command-C on macOS, Ctrl+C elsewhere) copies the note's text and leaves it open");
		aView.files()->setFocus(Qt::TabFocusReason);
		tests::window_system_key(*aView.files(), Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
		expect(editor->isVisible() && editor->toPlainText() == QStringLiteral("Keep me."), "Escape outside the open note leaves it open");
		editor->setFocus(Qt::TabFocusReason);
		tests::window_system_key(*editor, Qt::Key_C, Qt::MetaModifier);
#if defined(Q_OS_MACOS)
		expect(!editor->isVisible() && aNoted.Body.empty() && QApplication::focusWidget() == code, "on macOS Control-C cancels the note");
#else
		expect(editor->isVisible() && editor->toPlainText() == QStringLiteral("Keep me."), "off macOS Meta+C is not a cancel key");
		aView.findChild<QPushButton*>(QStringLiteral("cycleCommentCancel"))->click();
		settle();
#endif

		// Escape in Suggest change: a selection collapses first, as in any
		// editor, so a box of carets is never lost to one key; then it cancels.
		aSuggested = { };
		code->setFocus(Qt::TabFocusReason);
		code->select_bytes(13U, 13U);
		tests::window_system_key(*code, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
		expect(aView.suggesting() && keys->isEnabled(), "Shift+Return begins Suggest change and claims the cancel keys");
		code->apply(core::insert_text{ "x" });
		code->select_bytes(13U, 20U);
		QApplication::clipboard()->clear();
		tests::window_system_key(*code, Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"));
		expect(aView.suggesting() && QApplication::clipboard()->text() == QStringLiteral("xreturn"), "Qt's Control+C copies the edited selection and keeps the suggestion");
		tests::window_system_key(*code, Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
		expect(aView.suggesting() && !code->has_selection() && code->text() != parser, "the first Escape collapses the selection and keeps the edit");
		aView.composer()->findChild<QPlainTextEdit*>(QStringLiteral("suggestionRationale"))->setFocus(Qt::TabFocusReason);
		tests::window_system_key(*QApplication::focusWidget(), Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
		expect(!aView.suggesting() && code->read_only() && code->text() == parser && aSuggested.Edits.empty() && !keys->isEnabled() && QApplication::focusWidget() == code,
			   "Escape in the tray's rationale cancels the suggestion: the captured bytes back, nothing saved");
		code->setFocus(Qt::TabFocusReason);
		code->select_bytes(13U, 13U);
		tests::window_system_key(*code, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
		code->apply(core::insert_text{ "y" });
		tests::window_system_key(*code, Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
		expect(!aView.suggesting() && code->text() == parser && aSuggested.Edits.empty(), "Escape in the code with only a caret cancels the suggestion");
		code->select_bytes(13U, 13U);
		tests::window_system_key(*code, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
		code->apply(core::insert_text{ "z" });
		code->select_bytes(13U, 20U);
		tests::window_system_key(*code, Qt::Key_C, Qt::MetaModifier);
#if defined(Q_OS_MACOS)
		expect(!aView.suggesting() && code->text() == parser && aSuggested.Edits.empty(), "on macOS Control-C cancels the suggestion at once");
#else
		expect(aView.suggesting() && code->text() != parser, "off macOS Meta+C is not a cancel key");
		aView.composer()->cancel_button()->click();
		settle();
#endif
		expect(!aView.suggesting() && code->read_only(), "back to reading");
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
	auto         replies   = reply_gestures{ };
	auto         decisions = decision_gestures{ };
	view.set_dispatch({
		.OpenFile         = [&](const std::string& aPath, bool) { opened = aPath; },
		.Tab              = [](const core::document_tab_intent&) { },
		.ObserveLine      = [](core::document_tab_id, std::uint64_t) { },
		.Comment          = [&](core::begin_cycle_feedback aNote) { noted = std::move(aNote); },
		.Suggest          = [&](application::review_cycle_suggestion_request aRequest) { suggested = std::move(aRequest); },
		.Relate           = { },
		.Capture          = [&](const bool aDeliberate) { gestures.Captures.push_back(aDeliberate); },
		.Compare          = [&](const core::cycle_view_request& aRequest) { gestures.Compared.push_back(aRequest); },
		.FullSource       = [&] { ++gestures.FullSource; },
		.Refresh          = [&] { ++gestures.Refreshed; },
		.MarkReviewed     = [&] { ++gestures.MarkedReview; },
		.ChooseResponse   = [&](const core::cycle_round_id aRound) { replies.Chosen.push_back(aRound); },
		.Reply            = [&](const core::cycle_round_id aRound, core::cycle_manual_reply aReply) { replies.Replies.emplace_back(aRound, std::move(aReply)); },
		.Decide           = [&](const core::cycle_feedback_item_ref& aItem, const std::optional<core::cycle_reviewer_decision> aDecision) { decisions.Decisions.emplace_back(aItem, aDecision); },
		.PrepareNextRound = [&] { ++decisions.Prepared; },
	});
	view.resize(1280, 760);
	view.show();
	settle();
	presents_the_scaffold_and_notes(view);
	colours_the_source_from_its_classification();
	suggesting_changes_nothing_on_screen();
	reads_a_file_too_large_to_edit();
	anchors_exact_bytes_from_a_native_selection(view, suggested, noted);
	suggests_from_the_caret_and_the_context_menu(view, suggested);
	return_notes_and_suggests_where_the_caret_is(view, noted);
	cancels_the_open_composer_from_the_keyboard(view, noted, suggested);
	shows_where_focus_is();
	states_newer_material_without_moving(view);
	opens_files_and_steps(view, opened);
	captures_and_compares(view, gestures);
	records_replies(view, replies);
	decides_explicitly(view, decisions);
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
