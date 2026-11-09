#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/type_preview.hpp"
#include "diff_workbench_support.hpp"
#include "line_details_splitter.hpp"
#include "source_display_text.hpp"

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QBrush>
#include <QChar>
#include <QColor>
#include <QEvent>
#include <QFlags>
#include <QInputEvent>
#include <QLabel>
#include <QLatin1Char>
#include <QList>
#include <QMargins>
#include <QNativeGestureEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedLayout>
#include <QString>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QTimer>
#include <QVariant>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto line_text_at(
			const std::optional<core::decoded_text_document>& aDocument,
			const std::uint32_t                               aLine) -> QString
		{
			if (
				!aDocument.has_value() || aLine >= aDocument->Lines.size()) {
				return QStringLiteral("(no source line)");
			}
			const auto& span = aDocument->Lines[aLine].Content;
			if (
				span.End < span.Begin || span.End > aDocument->Bytes.size()
				|| span.End - span.Begin
					> static_cast<std::uint64_t>(
						std::numeric_limits<qsizetype>::max())) {
				return QStringLiteral("(unavailable source line)");
			}
			const auto* bytes = aDocument->Bytes.data()
				+ static_cast<std::size_t>(span.Begin);
			return source_display_text(
				reinterpret_cast<const char*>(bytes),
				static_cast<qsizetype>(span.End - span.Begin));
		}

		/// \brief Returns the overlap of two line spans, empty when disjoint.
		[[nodiscard]] constexpr auto intersection(
			const core::line_span aLeft, const core::line_span aRight) noexcept
			-> core::line_span
		{
			const auto begin = std::max(aLeft.Begin, aRight.Begin);
			const auto end   = std::min(aLeft.End, aRight.End);
			return core::line_span{ .Begin = begin,
									.End   = begin < end ? end : begin };
		}

		void set_line_selection(
			QPlainTextEdit&                             aPane,
			const std::optional<core::source_line_ref>& aLine)
		{
			auto selections = QList<QTextEdit::ExtraSelection>{ };
			if (aLine.has_value()) {
				const auto block = aPane.document()->findBlockByNumber(
					static_cast<int>(aLine->Line));
				if (block.isValid()) {
					auto selection   = QTextEdit::ExtraSelection{ };
					selection.cursor = QTextCursor{ block };
					selection.format.setProperty(
						QTextFormat::FullWidthSelection, true);
					auto color = aPane.palette().color(QPalette::Highlight);
					color.setAlpha(core::selection_wash_alpha);
					selection.format.setBackground(color);
					selections.push_back(selection);
				}
			}
			aPane.setExtraSelections(selections);
		}
	}

	void diff_workbench::synchronize_geometry_selection(const bool aReveal)
	{
		auto readingPreserved = false;
		if (aReveal && PresentationReadingLine && SemanticIndex && (OnePaneEnabled || AxisCache.axis() != nullptr)) {
			const auto reading = core::select_line(*SemanticIndex, *PresentationReadingLine);
			if (reading) {
				readingPreserved = reveal_exact_line(*reading, *PresentationReadingLine, true);
			}
			PresentationReadingLine.reset();
		}
		if (OnePaneEnabled) {
			if (aReveal && !readingPreserved && Interaction.Selection) {
				const auto& selection = *Interaction.Selection;
				const auto  exact     = ConversationAnchor.value_or(RestoredReadingLine.value_or(selection.PreferredLine));
				auto        row       = unified_row_for(exact.Side, exact.Line);
				if (!ConversationAnchor && !RestoredReadingLine && selection.Kind == core::semantic_selection_kind::group && selection.Group && SemanticIndex) {
					const auto group = SemanticIndex->group_index(*selection.Group);
					if (group && *group < UnifiedPresentation->Groups.size()) {
						row = UnifiedPresentation->Groups[*group].Removed.Begin;
					}
				}
				if (row) {
					reveal_unified_row(*row);
				}
				RestoredReadingLine.reset();
			}
			update_unified_link_map();
			return;
		}
		GeometryRevealPending = GeometryRevealPending || (aReveal && AxisCache.axis() == nullptr);
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			LinkMap->set_selected(std::nullopt, false);
			return;
		}
		const auto& selection = *Interaction.Selection;
		if (selection.Segment.has_value()) {
			LinkMap->set_selected(
				diff_link_map_identity{
					.Segment = *selection.Segment,
					.Group   = selection.Group,
				},
				false);
			if (aReveal && !readingPreserved) {
				const auto reading = RestoredReadingLine && selection.PreferredLine == *RestoredReadingLine
					&& reveal_exact_line(selection, *RestoredReadingLine);
				if (reading) {
					RestoredReadingLine.reset();
				}
				const auto exact = reading || (ConversationAnchor.has_value() && reveal_exact_line(selection, *ConversationAnchor));
				if (!exact) {
					static_cast<void>(Controller->reveal_segment(*selection.Segment));
				}
			}
		} else {
			LinkMap->set_selected(std::nullopt, false);
		}
	}

	auto diff_workbench::reveal_exact_line(
		const core::semantic_selection& aSelection,
		const core::source_line_ref& aLine, const bool aAtTop) -> bool
	{
		if (OnePaneEnabled) {
			const auto row = unified_row_for(aLine.Side, aLine.Line);
			if (row) {
				reveal_unified_row(*row);
			}
			return row.has_value();
		}
		const auto* const axis = AxisCache.axis();
		if (axis == nullptr || !aSelection.Segment.has_value()) {
			return false;
		}
		const auto index = SemanticIndex ? SemanticIndex->segment_index(*aSelection.Segment) : std::nullopt;
		if (!index || *index >= axis->segments().size() || axis->segments()[*index].Id != *aSelection.Segment) {
			return false;
		}
		const auto* const measured = &axis->segments()[*index];
		const auto        origin   = aLine.Side == core::diff_side::left
			? axis->left_line_origin(aLine.Line)
			: axis->right_line_origin(aLine.Line);
		if (!origin.has_value()) {
			return false;
		}
		if (aAtTop) {
			// The linked controller positions its rail, not its viewport top,
			// so a presentation flip asks for the viewport top directly. It
			// would otherwise move the former top line down to the rail.
			return Controller->reveal_pane_origin(aLine.Side, aLine.Line == 0U ? 0.0 : *origin);
		}
		const auto begin    = aLine.Side == core::diff_side::left
			? measured->LeftBegin
			: measured->RightBegin;
		const auto end      = aLine.Side == core::diff_side::left
			? measured->LeftEnd
			: measured->RightEnd;
		const auto fraction = end > begin ? (*origin - begin) / (end - begin) : 0.0;
		return Controller->reveal_segment(*aSelection.Segment, fraction);
	}

	void diff_workbench::apply_interaction(
		const bool aReveal,
		const bool aAnnounceSelection)
	{
		if (Interaction.Selection) {
			PresentationReadingSide = Interaction.Selection->PreferredLine.Side;
		}
		synchronize_geometry_selection(aReveal);
		update_detail();
		update_selection_paint();
		if (aAnnounceSelection) {
			announce_accessible_status(
				Status->text() + QStringLiteral(". ") + SelectionHeading);
		}
	}

	void diff_workbench::update_detail()
	{
		CurrentDetail.reset();
		SelectionHeading = QStringLiteral("No line selected");
		if (CurrentResult && SemanticIndex && Interaction.Selection.has_value()) {
			++Statistics.SemanticQueries;
			CurrentDetail    = core::detail_for_selection(*SemanticIndex, *Interaction.Selection);
			SelectionHeading = QStringLiteral("Selected line %1").arg(Interaction.Selection->PreferredLine.Line + 1U);
			if (Interaction.Selection->Group.has_value()) {
				if (const auto index = SemanticIndex->group_index(*Interaction.Selection->Group); index.has_value()) {
					SelectionHeading = QStringLiteral("Change %1 of %2").arg(*index + 1U).arg(CurrentResult->Alignment.Groups.size());
				}
			}
			if (const auto note = thread_note(); !note.isEmpty()) {
				SelectionHeading += QStringLiteral(". ") + note;
			}
		}
		update_line_details();
	}

	auto diff_workbench::thread_note() const -> QString
	{
		if (!Interaction.Selection.has_value()) {
			return { };
		}
		const auto&       preferred = Interaction.Selection->PreferredLine;
		const auto* const mark      = pane(preferred.Side == core::diff_side::left ? Left : Right)->mark_on(preferred.Line);
		if (mark == nullptr || mark->Description.isEmpty()) {
			return { };
		}
		return QStringLiteral("Thread: %1. The context menu opens it").arg(mark->Description);
	}

	void diff_workbench::queue_line_details()
	{
		// A drag changes the panes' selection on every mouse move; one
		// projection per event-loop turn is enough to follow it.
		if (LineDetailsQueued) {
			return;
		}
		LineDetailsQueued = true;
		QTimer::singleShot(0, this, [this] {
			LineDetailsQueued = false;
			update_line_details();
		});
	}

	void diff_workbench::update_line_details()
	{
		if (!CurrentResult || !SemanticIndex || !Interaction.Selection.has_value()) {
			Detail->present_empty();
			return;
		}
		// Hidden, the strip asks the core nothing; showing it again projects.
		if (Detail->isHidden()) {
			return;
		}
		auto side  = Interaction.Selection->PreferredLine.Side;
		auto lines = core::line_span{ Interaction.Selection->PreferredLine.Line,
									  Interaction.Selection->PreferredLine.Line + 1U };
		// A text selection across lines in a pane shows each selected line,
		// while it holds the current line. A change step or a conversation
		// that moves the current line elsewhere leaves an old text selection
		// behind; the strip follows the current line then.
		const auto& current = Interaction.Selection->PreferredLine;
		if (const auto range = selected_source_range();
			range.has_value() && range->first.Side == range->second.Side && range->first.Side == current.Side
			&& range->first.Line <= current.Line && current.Line <= range->second.Line) {
			side  = range->first.Side;
			lines = core::line_span{ range->first.Line, range->second.Line + 1U };
		}
		++Statistics.LineDetailsProjections;
		Detail->present(core::line_details(*SemanticIndex, side, lines), *CurrentResult);
		// [[BPR-1335]]. A thread on the current line is said with it, so a
		// reader who never sees its tinted number still hears that it is there.
		if (const auto note = thread_note(); !note.isEmpty()) {
			Detail->setAccessibleDescription(Detail->accessibleDescription() + QStringLiteral("; ") + note);
		}
		Detail->setTabStopDistance(Left->tabStopDistance());
	}

	void diff_workbench::update_selection_paint()
	{
		update_diff_paint();
	}

	void diff_workbench::set_line_numbers_visible(const bool aVisible)
	{
		pane(Left)->set_line_numbers_visible(aVisible);
		pane(Right)->set_line_numbers_visible(aVisible);
		pane(Unified)->set_line_numbers_visible(aVisible);
		// The panes' viewports just moved, and the band is measured from a live
		// viewport rather than inferred, so it has to be re-declared for the
		// same reason a resize does it.
		declare_link_map_band();
	}

	void diff_workbench::set_one_pane_enabled(const bool aEnabled)
	{
		if (OnePaneEnabled == aEnabled) {
			return;
		}
		end_head_edit();
		if (CurrentResult && Interaction.Selection) {
			const auto side   = PresentationReadingSide.value_or(Interaction.Selection->PreferredLine.Side);
			auto*      source = pane(OnePaneEnabled ? Unified : side == core::diff_side::left ? Left
																							  : Right);
			if (const auto origin = source->visible_origin()) {
				auto line    = static_cast<std::uint32_t>(origin->first);
				auto ownSide = side;
				if (OnePaneEnabled && line < UnifiedPresentation->Rows.size()) {
					const auto& row = UnifiedPresentation->Rows[line];
					ownSide         = row.Head ? core::diff_side::right : core::diff_side::left;
					line            = row.Head.value_or(row.Base.value_or(0U));
				}
				PresentationReadingLine = core::source_line_ref{ CurrentResult->Alignment.Digest, ownSide, line };
				PresentationReadingSide = ownSide;
			}
		}
		Controller->reset_pixel_gesture();
		for (auto* source : { Left, Right, Unified }) {
			pane(source)->reset_pixel_gesture();
			pane(source)->set_within_line_offset(0.0);
		}
		clear_source_find_document();
		OnePaneEnabled = aEnabled;
		rebuild_unified_document();
		Comparison->setVisible(!aEnabled);
		SharedScroll->setVisible(!aEnabled);
		UnifiedMap->setVisible(aEnabled);
		Unified->setVisible(aEnabled);
		if (aEnabled) {
			synchronize_geometry_selection(true);
		} else {
			GeometryRevealPending = true;
			schedule_geometry_rebuild(measured_geometry_change::viewport_or_divider,
									  diff_geometry_rebuild_trigger::pane_viewport_resize);
		}
		update_diff_paint();
		if (FrameObserver) {
			FrameObserver();
		}
	}

	auto diff_workbench::one_pane_enabled() const noexcept -> bool
	{
		return OnePaneEnabled;
	}

	void diff_workbench::rebuild_unified_document()
	{
		const auto blocker    = QSignalBlocker{ Unified->verticalScrollBar() };
		auto&      projection = *UnifiedPresentation;
		projection.Rows.clear();
		projection.Groups.clear();
		projection.BaseRows.clear();
		projection.HeadRows.clear();
		// The inactive presentation owns no source document. Build its rows only
		// when chosen, after preserving the exact reading anchor above.
		if (!CurrentResult || !OnePaneEnabled) {
			Unified->clear();
			UnifiedMap->set_links({ }, std::nullopt, { });
			return;
		}
		const auto& alignment = CurrentResult->Alignment;
		projection.BaseRows.resize(alignment.LeftLineCount);
		projection.HeadRows.resize(alignment.RightLineCount);
		projection.Rows.reserve(static_cast<std::size_t>(alignment.LeftLineCount) + alignment.RightLineCount);
		auto       text   = QString{ };
		const auto append = [&](const std::optional<std::uint32_t> aBase, const std::optional<std::uint32_t> aHead) {
			const auto row = static_cast<std::uint32_t>(projection.Rows.size());
			if (row != 0U) {
				text += QLatin1Char('\n');
			}
			text += aHead ? line_text_at(CurrentResult->RightDocument, *aHead) : line_text_at(CurrentResult->LeftDocument, *aBase);
			projection.Rows.push_back({ aBase, aHead });
			if (aBase) {
				projection.BaseRows[*aBase] = row;
			}
			if (aHead) {
				projection.HeadRows[*aHead] = row;
			}
		};
		auto nextGroup = std::size_t{ 0U };
		for (auto index = std::size_t{ 0U }; index < alignment.Segments.size();) {
			const auto& segment = alignment.Segments[index];
			if (segment.Kind == core::alignment_kind::equal) {
				for (auto offset = 0U; offset < segment.Left.extent(); ++offset) {
					append(segment.Left.Begin + offset, segment.Right.Begin + offset);
				}
				++index;
				continue;
			}
			// The core already grouped adjacent changed segments. Preserve that
			// exact block and ordinal, projecting all base rows before all head rows.
			const auto& group = alignment.Groups[nextGroup++];
			const auto  begin = static_cast<std::uint32_t>(projection.Rows.size());
			for (auto line = group.Left.Begin; line < group.Left.End; ++line) {
				append(line, std::nullopt);
			}
			const auto middle = static_cast<std::uint32_t>(projection.Rows.size());
			for (auto line = group.Right.Begin; line < group.Right.End; ++line) {
				append(std::nullopt, line);
			}
			projection.Groups.push_back({
				.Identity = { segment.Id, group.Id },
				.Removed  = { begin, middle },
				.Added    = { middle, static_cast<std::uint32_t>(projection.Rows.size()) },
			});
			index = group.Members.End;
		}
		Unified->setPlainText(text);
	}

	auto diff_workbench::unified_row_for(const core::diff_side aSide, const std::uint32_t aLine) const -> std::optional<std::uint32_t>
	{
		const auto& rows = aSide == core::diff_side::left ? UnifiedPresentation->BaseRows : UnifiedPresentation->HeadRows;
		return aLine < rows.size() ? std::optional{ rows[aLine] } : std::nullopt;
	}

	void diff_workbench::reveal_unified_row(const std::uint32_t aRow)
	{
		pane(Unified)->set_within_line_offset(0.0);
		Unified->verticalScrollBar()->setValue(static_cast<int>(aRow));
		update_unified_paint();
		if (FrameObserver) {
			FrameObserver();
		}
	}

	void diff_workbench::update_unified_link_map()
	{
		if (!OnePaneEnabled || !CurrentResult) {
			return;
		}
		const auto top      = Unified->viewport()->mapTo(this, QPoint{ 0, pane(Unified)->within_line_offset() }).y() - UnifiedMap->mapTo(this, QPoint{ }).y();
		const auto viewport = QRectF{ 0.0, static_cast<qreal>(top), static_cast<qreal>(UnifiedMap->width()), static_cast<qreal>(pane(Unified)->viewport_extent()) };
		const auto visible  = core::line_span{
			static_cast<std::uint32_t>(std::max(0, Unified->cursorForPosition(QPoint{ 0, 0 }).blockNumber())),
			static_cast<std::uint32_t>(std::max(0, Unified->cursorForPosition(QPoint{ 0, Unified->viewport()->height() }).blockNumber()) + 1),
		};
		auto        links  = std::vector<visible_one_pane_diff_link>{ };
		const auto& groups = UnifiedPresentation->Groups;
		const auto  first  = std::ranges::lower_bound(groups, visible.Begin, { }, [](const unified_presentation::group_rows& aGroup) { return aGroup.Added.End; });
		const auto  band   = [&](const core::line_span aRows) -> std::optional<QRectF> {
			if (aRows.empty()) {
				return std::nullopt;
			}
			const auto start = pane(Unified)->line_row_rect(aRows.Begin);
			const auto end   = pane(Unified)->line_row_rect(aRows.End - 1U);
			return QRectF{ 0.0, static_cast<qreal>(start.top() + top), static_cast<qreal>(UnifiedMap->width()), static_cast<qreal>(end.bottom() + 1 - start.top()) };
		};
		for (auto group = first; group != groups.end() && group->Removed.Begin < visible.End; ++group) {
			links.push_back({ .Identity = group->Identity, .Removed = band(group->Removed), .Added = band(group->Added) });
		}
		UnifiedMap->set_links(std::move(links), Interaction.Selection ? Interaction.Selection->Group : std::nullopt, viewport);
	}

	void diff_workbench::update_unified_paint()
	{
		if (!Unified || !CurrentResult || !OnePaneEnabled) {
			return;
		}
		const auto  visible      = paintable_line_bounds(*Unified);
		const auto& rows         = UnifiedPresentation->Rows;
		auto        selections   = QList<QTextEdit::ExtraSelection>{ };
		auto        base         = core::line_span{ static_cast<std::uint32_t>(UnifiedPresentation->BaseRows.size()), 0U };
		auto        head         = core::line_span{ static_cast<std::uint32_t>(UnifiedPresentation->HeadRows.size()), 0U };
		const auto  selectedBase = selected_lines(core::diff_side::left);
		const auto  selectedHead = selected_lines(core::diff_side::right);
		const auto  contains     = [](const std::optional<core::line_span> aSpan, const std::optional<std::uint32_t> aLine) {
			return aSpan && aLine && *aLine >= aSpan->Begin && *aLine < aSpan->End;
		};
		const auto currentBase = current_line(core::diff_side::left);
		const auto currentHead = current_line(core::diff_side::right);
		for (auto index = visible.Begin; index < visible.End && index < rows.size(); ++index) {
			const auto& row = rows[index];
			if (row.Base) {
				base.Begin = std::min(base.Begin, *row.Base);
				base.End   = *row.Base + 1U;
			}
			if (row.Head) {
				head.Begin = std::min(head.Begin, *row.Head);
				head.End   = *row.Head + 1U;
			}
			const auto span = core::line_span{ index, index + 1U };
			// [[BPR-0986]]. As in the two panes: an unchanged reading row, one
			// with both a base and a head line, wears the current-line band.
			if (Colors.Supplied && row.Base && row.Head && ((currentBase && row.Base == currentBase) || (currentHead && row.Head == currentHead))) {
				append_line_band(selections, *Unified, span, code_sample_row_fill(code_sample_row_kind::current_line, Colors.Syntax));
				++Statistics.CurrentLineBandsPainted;
			}
			if (Colors.Supplied) {
				append_line_band(selections, *Unified, span, !row.Head ? Colors.Removed : !row.Base ? Colors.Added
																									: QColor{ });
			}
			if (contains(selectedBase, row.Base) || contains(selectedHead, row.Head)) {
				append_selection_band(selections, *Unified, span, span);
			}
		}
		apply_syntax_formats(*Unified, core::diff_side::left, base);
		apply_syntax_formats(*Unified, core::diff_side::right, head);
		if (Colors.Supplied) {
			append_intraline_emphasis(selections, *Unified, core::diff_side::left, base, selectedBase);
			append_intraline_emphasis(selections, *Unified, core::diff_side::right, head, selectedHead);
		}
		append_find_selections(*Unified, selections);
		Unified->setExtraSelections(selections);
		pane(Unified)->set_unified_selection(selectedBase, selectedHead);
		pane(Unified)->set_row_colors(Colors.Removed, Colors.Added);
		pane(Unified)->set_gutter_colors(Colors.Syntax[static_cast<std::size_t>(core::appearance_role::code_gutter_background)],
										 Colors.Syntax[static_cast<std::size_t>(core::appearance_role::code_line_number_text)]);
		auto marks = std::vector<diff_line_mark>{ };
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			for (const auto& mark : line_marks(side)) {
				if (const auto row = unified_row_for(side, mark.Line)) {
					auto unified = mark;
					unified.Line = *row;
					// [[BPR-1212]] slice 1. A range draft's first line, as its row.
					unified.FirstLine = mark.FirstLine.has_value() ? unified_row_for(side, *mark.FirstLine) : std::nullopt;
					marks.push_back(std::move(unified));
				}
			}
		}
		pane(Unified)->set_line_marks(std::move(marks));
		pane(Unified)->set_conversation_anchor(ConversationAnchor ? unified_row_for(ConversationAnchor->Side, ConversationAnchor->Line) : std::nullopt);
		update_unified_link_map();
	}

	void diff_workbench::set_code_zoom_dispatch(std::function<void(core::code_zoom_step)> aDispatch)
	{
		CodeZoomDispatch = std::move(aDispatch);
	}

	auto diff_workbench::code_zoom_indicator() const noexcept -> QWidget*
	{
		return ZoomIndicator;
	}

	void diff_workbench::show_code_zoom(const std::uint32_t aPercent, const std::uint32_t aCentipoints)
	{
		const auto  size    = point_size_text(aCentipoints);
		const auto  percent = QStringLiteral("%1%").arg(aPercent);
		auto* const pill    = static_cast<zoom_pill*>(ZoomIndicator);
		pill->set_zoom(percent, QStringLiteral("%1 pt").arg(size));
		// Over the top centre of the code, or of the idle page in its place.
		const auto area = Vertical->geometry();
		ZoomIndicator->move(area.left() + (area.width() - ZoomIndicator->width()) / 2, area.top() + 12);
		ZoomIndicator->raise();
		ZoomIndicator->show();
		ZoomIndicatorTimer->start();
		announce_accessible_status(QStringLiteral("Code zoom %1, %2 points").arg(percent, size));
	}

	/// \brief [[BPR-1336]]. Turns ⌘-scroll and pinch into zoom steps.
	///
	/// \details One step for each wheel notch (120 eighths of a degree, a
	/// trackpad's small deltas adding up to the same), and one for each 0.15 of
	/// pinch. A reversal starts the count again, so a hesitant gesture does not
	/// step against the reader's hand.
	auto diff_workbench::code_zoom_gesture(QEvent& aEvent) -> bool
	{
		const auto dispatch = [this](const core::code_zoom_step aStep) {
			++Statistics.CodeZoomGestures;
			if (CodeZoomDispatch) {
				CodeZoomDispatch(aStep);
			}
		};
		if (aEvent.type() == QEvent::Wheel) {
			auto& wheel = static_cast<QWheelEvent&>(aEvent);
			if (!wheel.modifiers().testFlag(Qt::ControlModifier)) {
				ZoomWheelAngle = 0;
				return false;
			}
			const auto delta = wheel.angleDelta().y() != 0 ? wheel.angleDelta().y() : wheel.angleDelta().x();
			if ((delta > 0 && ZoomWheelAngle < 0) || (delta < 0 && ZoomWheelAngle > 0)) {
				ZoomWheelAngle = 0;
			}
			ZoomWheelAngle += delta;
			constexpr auto notch = 120;
			while (std::abs(ZoomWheelAngle) >= notch) {
				const auto in = ZoomWheelAngle > 0;
				ZoomWheelAngle += in ? -notch : notch;
				dispatch(in ? core::code_zoom_step::in : core::code_zoom_step::out);
			}
			wheel.accept();
			return true;
		}
		auto& gesture = static_cast<QNativeGestureEvent&>(aEvent);
		switch (gesture.gestureType()) {
		case Qt::BeginNativeGesture:
		case Qt::EndNativeGesture:
			ZoomPinch = 0.0;
			return false;
		case Qt::ZoomNativeGesture: {
			if ((gesture.value() > 0.0 && ZoomPinch < 0.0) || (gesture.value() < 0.0 && ZoomPinch > 0.0)) {
				ZoomPinch = 0.0;
			}
			ZoomPinch += gesture.value();
			constexpr auto threshold = 0.15;
			while (std::abs(ZoomPinch) >= threshold) {
				const auto in = ZoomPinch > 0.0;
				ZoomPinch += in ? -threshold : threshold;
				dispatch(in ? core::code_zoom_step::in : core::code_zoom_step::out);
			}
			gesture.accept();
			return true;
		}
		default:
			return false;
		}
	}

	/// \brief [[BPR-1336]]. Notes where the reader's line is before a font change.
	///
	/// \details The comparison is given a new code font before its panes are,
	/// so here they still show the old one. The line noted is the reader's own
	/// when it is on screen, and otherwise the one at the synchronization rail,
	/// which is the line the controller keeps across any remeasurement. The
	/// note lasts until the end of the turn: the panes' own font changes follow
	/// it in the same turn or not at all.
	void diff_workbench::note_reading_position()
	{
		Hold.reset();
		if (!CurrentResult) {
			return;
		}
		auto* const reference = OnePaneEnabled ? Unified
			: Interaction.Selection && Interaction.Selection->PreferredLine.Side == core::diff_side::left
			? Left
			: Right;
		const auto  previous  = pane(reference)->row_height();
		const auto  height    = static_cast<qreal>(pane(reference)->viewport_extent());
		if (previous <= 0.0 || height <= 0.0) {
			return;
		}
		auto noted = reading_hold{ .Previous = previous };
		if (Interaction.Selection) {
			const auto& line = Interaction.Selection->PreferredLine;
			const auto  row  = OnePaneEnabled ? unified_row_for(line.Side, line.Line) : std::optional{ line.Line };
			const auto  top  = row ? pane(reference)->row_top(*row) : std::nullopt;
			if (top && *top >= 0.0 && *top + previous <= height) {
				noted.Offset = *top;
				noted.Line   = line;
			}
		}
		if (!noted.Line) {
			noted.Offset = height * comparison_scroll_configuration{ }.SynchronizationRailFraction;
		}
		Hold = noted;
		QTimer::singleShot(0, this, [this] { Hold.reset(); });
	}

	/// \brief [[BPR-1336]]. Keeps the noted line where it was in one pane.
	///
	/// \details Called by each pane after it relaid out in the new font and
	/// before anything scrolled it, so it still shows its old top line. Each
	/// pane is scrolled at once by whole lines of the new height, so the reader
	/// never sees the line leave; the exact position follows from the
	/// remeasured axis.
	void diff_workbench::hold_reading_position(QPlainTextEdit& aPane)
	{
		const auto unified = &aPane == Unified;
		if (!Hold || unified != OnePaneEnabled) {
			return;
		}
		auto* const source  = pane(&aPane);
		const auto  current = source->row_height();
		const auto  origin  = source->visible_origin();
		if (current <= 0.0 || !origin || qFuzzyCompare(current, Hold->Previous)) {
			return;
		}
		const auto top   = static_cast<qreal>(origin->first);
		const auto first = origin->second;
		const auto side  = &aPane == Left ? core::diff_side::left : core::diff_side::right;
		auto       row   = top + std::round((Hold->Offset - first) / Hold->Previous);
		if (Hold->Line) {
			if (unified) {
				row = static_cast<qreal>(unified_row_for(Hold->Line->Side, Hold->Line->Line).value_or(static_cast<std::uint32_t>(row)));
			} else if (Hold->Line->Side == side) {
				row = static_cast<qreal>(Hold->Line->Line);
			}
		}
		// [[BPR-1492]]. `first` includes the within-line offset the old origin
		// kept ([[BPR-1413]]). The new top line is placed flush, with no such
		// offset, so only the noted offset counts here; counting `first` again
		// put the reader's line most of a row lower than it was.
		const auto newTop = static_cast<std::uint32_t>(std::max(0.0, row - std::round(Hold->Offset / current)));
		source->set_top_line(newTop);
		source->set_within_line_offset(0.0);
		if (Hold->Line && !unified && Hold->Line->Side == side) {
			HeldLine = held_line{ .Side = side, .Top = newTop };
		}
	}

	auto diff_workbench::find_wash(const bool aCurrent) const -> QColor
	{
		if (!Colors.Supplied) {
			return palette().color(QPalette::Highlight);
		}
		return Colors.Syntax[core::index_of(aCurrent ? core::appearance_role::find_current_match_background : core::appearance_role::find_match_background)];
	}

	void diff_workbench::adopt_appearance(const appearance_profile& aProfile)
	{
		using role  = core::appearance_role;
		auto colors = diff_presentation_colors{
			.Added            = aProfile.color(role::diff_added_background),
			.Removed          = aProfile.color(role::diff_removed_background),
			.IntralineAdded   = aProfile.color(role::diff_intraline_added_background),
			.IntralineRemoved = aProfile.color(role::diff_intraline_removed_background),
			.Moved            = aProfile.color(role::diff_moved_background),
			.Syntax           = { },
			.Palette          = aProfile.palette(),
			.Variant          = aProfile.variant(),
			.Supplied         = true,
		};
		// The same table the theme tiles and the type preview read, so one change
		// to how a role becomes a colour cannot reach one surface and miss another.
		colors.Syntax = code_role_colors_of(aProfile.palette());
		Colors        = std::move(colors);
		if (Find) {
			Find->Bar->adopt_appearance(aProfile);
		}
		// [[BPR-1334]]. Line details repeats the panes' lines in their colours.
		Detail->set_colors(line_details_colors{
			.Added            = Colors.Added,
			.Removed          = Colors.Removed,
			.IntralineAdded   = Colors.IntralineAdded,
			.IntralineRemoved = Colors.IntralineRemoved,
			.GutterBackground = Colors.Syntax[core::index_of(role::code_gutter_background)],
			.GutterText       = Colors.Syntax[core::index_of(role::code_line_number_text)],
			.Syntax           = Colors.Syntax,
			.Variant          = Colors.Variant,
			.Supplied         = true,
		});
		// [[BPR-1107]]. When the panes draw white space, from the same
		// typography record the preview reads.
		for (auto* const target : { Left, Right, Unified }) {
			pane(target)->set_white_space(aProfile.typography().Code.WhiteSpace);
			// [[BPR-1533]]. The selection's glyphs keep their colours, held to
			// this variant's floor on the selection.
			pane(target)->set_selection_ink(Colors.Syntax[core::index_of(role::syntax_normal)], Colors.Variant);
		}
		update_diff_paint();
	}

	auto diff_workbench::adopt_token_spans(
		const core::diff_side           aSide,
		application::shared_token_spans aSpans) -> bool
	{
		if (aSpans && !std::ranges::is_sorted(*aSpans, { }, [](const application::token_span& aSpan) {
				return aSpan.Line;
			})) {
			// Refused whole rather than sorted here. A copy would double the
			// memory of the one thing this design deliberately shares, and a
			// linear scan per repaint is the unbounded cost BPR-0196 removed.
			// Refusing makes a classifier that broke the contract visible.
			++Statistics.RejectedTokenSpans;
			return false;
		}
		auto& retained = aSide == core::diff_side::left ? LeftSpans : RightSpans;
		retained       = std::move(aSpans);
		++Statistics.TokenSpanAdoptions;
		// [[BPR-1334]]. Line details colours its rows from the same spans.
		Detail->set_token_spans(LeftSpans, RightSpans);
		update_diff_paint();
		SyntaxAwaited[aSide == core::diff_side::left ? 0U : 1U] = false;
		apply_pending();
		return true;
	}

	void diff_workbench::await_syntax(const core::diff_side aSide)
	{
		SyntaxAwaited[aSide == core::diff_side::left ? 0U : 1U] = true;
		apply_pending();
	}

	void diff_workbench::settle_syntax()
	{
		SyntaxAwaited = { };
		apply_pending();
	}

	auto diff_workbench::comparison_veil() const noexcept -> pending_veil*
	{
		return Veil;
	}

	void diff_workbench::apply_pending()
	{
		if (Veil == nullptr) {
			return;
		}
		// [[BPR-1871]]. A file the reviewer opened is named by its last part,
		// as the Overview's plate names one; the status line above says its
		// whole path.
		const auto name = WaitingPath.section(QLatin1Char('/'), -1);
		if (!WaitingFailure.isEmpty() && !CurrentContext.has_value()) {
			Veil->present(core::pending_activity::failed, false, false, { .Failure = WaitingFailure });
			return;
		}
		if (ComparisonLoading && CurrentContext.has_value()) {
			Veil->present(core::pending_activity::loading, false, false,
						  { .Loading = name.isEmpty() ? QStringLiteral("Loading the comparison…") : QStringLiteral("Loading %1…").arg(name),
							.Waiting = QStringLiteral("Still preparing the comparison: a large file takes longer.") });
			return;
		}
		if (!WaitingPath.isEmpty() && !CurrentContext.has_value()) {
			Veil->present(core::pending_activity::loading, false, false,
						  { .Loading = QStringLiteral("Loading %1…").arg(name),
							// Short enough for a narrow pane: the line above names the file.
							.Waiting = WaitingHistory
								? QStringLiteral("Still fetching the pull request's history.")
								: QStringLiteral("Still reading the file from the repository.") });
			return;
		}
		// The text is there to read; only its colours are still coming.
		if (CurrentResult && (SyntaxAwaited[0U] || SyntaxAwaited[1U])) {
			Veil->present(core::pending_activity::loading, true, true, { .Coming = QStringLiteral("Colouring syntax…") });
			return;
		}
		Veil->present_ready();
	}

	auto diff_workbench::token_spans(const core::diff_side aSide) const noexcept
		-> const application::shared_token_spans&
	{
		return aSide == core::diff_side::left ? LeftSpans : RightSpans;
	}

	/// \brief Paints the diff the core decided, in one pass per pane.
	///
	/// \details Order is the whole design. Qt layers extra selections in the
	/// order they are given, so the line band goes down first, the translucent
	/// selection wash over it, and the opaque intraline emphasis last. A reviewer therefore sees
	/// "this line changed", "this is the part that changed", and "this is where
	/// I am" as three readable levels. Intraline must be last: otherwise the
	/// selected-row wash replaces the exact changed-character color in the
	/// raster even though the corresponding ExtraSelection still exists.
	///
	/// Nothing here decides what changed. Every span comes from
	/// `core::diff_semantics`, and a second opinion about which lines differ is
	/// the defect RFC-BPR-0005's reuse rules forbid.
	void diff_workbench::update_diff_paint()
	{
		if (OnePaneEnabled) {
			update_unified_paint();
			return;
		}
		pane(Left)->set_conversation_anchor(
			ConversationAnchor.has_value()
					&& ConversationAnchor->Side == core::diff_side::left
				? std::optional{ ConversationAnchor->Line }
				: std::nullopt);
		pane(Right)->set_conversation_anchor(
			ConversationAnchor.has_value()
					&& ConversationAnchor->Side == core::diff_side::right
				? std::optional{ ConversationAnchor->Line }
				: std::nullopt);
		apply_group_boundaries();

		const auto paint = [this](
							   QPlainTextEdit&       aPane,
							   const core::diff_side aSide) {
			auto       selections = QList<QTextEdit::ExtraSelection>{ };
			const auto paintable  = paintable_line_bounds(aPane);

			// Under the bands rather than over them, because it is a
			// foreground and they are backgrounds.
			apply_syntax_formats(aPane, aSide, paintable);

			if (CurrentResult && Colors.Supplied) {
				const auto  current   = current_line(aSide);
				const auto& alignment = CurrentResult->Alignment;
				// Segments are ordered by line, so the visible run is found by
				// search. Scanning all of them was the shape of cost BPR-0196's
				// selection count could not see: it emitted a bounded number of
				// bands and walked every segment in the file to decide which,
				// which on an all-changed comparison is one visit per line per
				// repaint.
				const auto first = std::ranges::lower_bound(
					alignment.Segments,
					paintable.Begin,
					{ },
					[aSide](const core::alignment_segment& aSegment) {
						return (aSide == core::diff_side::left ? aSegment.Left
															   : aSegment.Right)
							.End;
					});
				for (auto segment = first; segment != alignment.Segments.end();
					 ++segment) {
					const auto span = aSide == core::diff_side::left
						? segment->Left
						: segment->Right;
					if (span.Begin >= paintable.End) {
						break;
					}
					++Statistics.AlignmentSegmentsVisited;
					const auto isMoved = moved(segment->Id);
					const auto color   = [&]() -> QColor {
						if (isMoved) {
							// A move is a deletion here and an insertion there.
							// Painting it as an ordinary removal and addition
							// would discard the one thing move detection
							// computed: that these two blocks are the same
							// code.
							return Colors.Moved;
						}
						switch (segment->Kind) {
						case core::alignment_kind::equal:
							// Unchanged lines are left untinted on purpose. The
							// eye should go to what moved, and a background on
							// every line marks nothing.
							return { };
						case core::alignment_kind::left_only:
							return Colors.Removed;
						case core::alignment_kind::right_only:
							return Colors.Added;
						case core::alignment_kind::replacement:
							// A replacement is a removal on the left and an
							// addition on the right, which is what the reviewer
							// is actually looking at on each side.
							return aSide == core::diff_side::left
								? Colors.Removed
								: Colors.Added;
						}
						return { };
					}();
					const auto visible = intersection(span, paintable);
					// [[BPR-0986]]. An unchanged reading line wears the current-line
					// band, as the Appearance preview's current row does. A changed
					// one keeps the band that says it changed, so each line wears
					// one band, and the selection wash lies over either.
					if (!color.isValid() && current.has_value() && *current >= visible.Begin && *current < visible.End) {
						append_line_band(selections, aPane, core::line_span{ .Begin = *current, .End = *current + 1U }, code_sample_row_fill(code_sample_row_kind::current_line, Colors.Syntax));
						++Statistics.CurrentLineBandsPainted;
					}
					append_line_band(selections, aPane, visible, color);
					if (isMoved && !visible.empty()) {
						++Statistics.MovedBandsPainted;
					}
				}
			}

			auto washed = std::optional<core::line_span>{ };
			if (CurrentResult && Interaction.Selection.has_value()) {
				const auto lines = selected_lines(aSide);
				if (lines.has_value()) {
					append_selection_band(
						selections, aPane, *lines, paintable);
					washed = lines;
				}
			}
			pane(&aPane)->set_selected_lines(washed);

			if (CurrentResult && Colors.Supplied) {
				append_intraline_emphasis(
					selections, aPane, aSide, paintable, washed);
			}

			append_find_selections(aPane, selections);
			aPane.setExtraSelections(selections);
		};

		paint(*Left, core::diff_side::left);
		paint(*Right, core::diff_side::right);
	}

	/// \brief The lines one side's selection covers.
	///
	/// \details A group selection names a change group, and the group carries
	/// its own span per side, so the answer is exact rather than derived from
	/// the preferred line. A line selection is that one line. Anything else —
	/// a group the alignment no longer holds — yields nothing rather than
	/// falling back to the preferred line, because a selection drawn somewhere
	/// other than where the model says it is would be worse than none.
	auto diff_workbench::selected_lines(const core::diff_side aSide) const
		-> std::optional<core::line_span>
	{
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			return std::nullopt;
		}
		const auto& selection = *Interaction.Selection;
		if (OnePaneEnabled && selection.Kind == core::semantic_selection_kind::line) {
			return selection.PreferredLine.Side == aSide
				? std::optional{ core::line_span{ selection.PreferredLine.Line, selection.PreferredLine.Line + 1U } }
				: std::nullopt;
		}
		if (selection.Kind == core::semantic_selection_kind::group) {
			if (!selection.Group.has_value()) {
				return std::nullopt;
			}
			const auto group = std::ranges::find(
				CurrentResult->Alignment.Groups,
				*selection.Group,
				&core::change_group::Id);
			if (group == CurrentResult->Alignment.Groups.end()) {
				return std::nullopt;
			}
			return aSide == core::diff_side::left ? group->Left : group->Right;
		}
		if (!CurrentDetail.has_value()) {
			return std::nullopt;
		}
		const auto& line = aSide == core::diff_side::left
			? CurrentDetail->LeftLine
			: CurrentDetail->RightLine;
		return line.has_value()
			? std::optional<core::line_span>{ core::line_span{
				  .Begin = line->Line, .End = line->Line + 1U } }
			: std::nullopt;
	}

	/// \brief The line the reader is on, when it is on this side.
	///
	/// \details [[BPR-0986]]. The selection's preferred line: where a click, a
	/// change step or a restored reading position put the reader. It is one
	/// line on one side, which is what the Appearance preview's current row
	/// stands for; a line past the document's end is no line.
	auto diff_workbench::current_line(const core::diff_side aSide) const
		-> std::optional<std::uint32_t>
	{
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			return std::nullopt;
		}
		const auto& preferred = Interaction.Selection->PreferredLine;
		const auto  count     = aSide == core::diff_side::left
			? CurrentResult->Alignment.LeftLineCount
			: CurrentResult->Alignment.RightLineCount;
		return preferred.Side == aSide && preferred.Line < count
			? std::optional{ preferred.Line }
			: std::nullopt;
	}

	auto diff_workbench::moved(const core::alignment_segment_id& aSegment) const
		-> bool
	{
		return MovedSegments.contains(aSegment);
	}

	/// \brief Hands each pane the lines its change groups start on.
	///
	/// \details The panes show whole documents side by side, not a unified hunk
	/// stream, so a boundary cannot be a row of text: inserting one would make
	/// pane line N stop being document line N, and every mapping built on that
	/// equality - intraline emphasis, token spans, selection, the measured axis -
	/// would silently shift by the number of rows above it.
	///
	/// So it is drawn as a rule on the edge above the group's first line, which
	/// is where the boundary actually is. The earlier version tinted the
	/// unchanged line above the group instead, and BPR-0202 records why that was
	/// wrong: a full-width fill is how this surface says "this line changed", and
	/// spending it on a line that did not change inverts the vocabulary. At
	/// attended review the marking was read as the selected line.
	///
	/// A group starting at line zero is skipped. There is no edge above the first
	/// line of a file, and a rule drawn there would mark the document's start
	/// rather than a separation between two groups.
	void diff_workbench::apply_group_boundaries()
	{
		auto left  = std::vector<std::uint32_t>{ };
		auto right = std::vector<std::uint32_t>{ };
		if (CurrentResult) {
			for (const auto& group : CurrentResult->Alignment.Groups) {
				if (group.First.Left > 0U) {
					left.push_back(group.First.Left);
				}
				if (group.First.Right > 0U) {
					right.push_back(group.First.Right);
				}
			}
		}
		std::ranges::sort(left);
		std::ranges::sort(right);
		pane(Left)->set_group_boundaries(std::move(left));
		pane(Right)->set_group_boundaries(std::move(right));

		const auto rule = Colors.Syntax[static_cast<std::size_t>(
			core::appearance_role::diff_group_boundary_rule)];
		pane(Left)->set_boundary_rule_color(rule);
		pane(Right)->set_boundary_rule_color(rule);

		// Roles RFC-BPR-0006 declared and `appearance_contrast` has been
		// validating all along; BPR-0455 is the first thing to read them.
		const auto gutterBackground = Colors.Syntax[static_cast<std::size_t>(
			core::appearance_role::code_gutter_background)];
		const auto gutterText       = Colors.Syntax[static_cast<std::size_t>(
			core::appearance_role::code_line_number_text)];
		pane(Left)->set_gutter_colors(gutterBackground, gutterText);
		pane(Right)->set_gutter_colors(gutterBackground, gutterText);
	}

	/// \brief Colours the classified tokens of the lines a pane can show.
	///
	/// \details The formats go on each block's own layout rather than into the
	/// document. `QTextCursor::setCharFormat` would mutate the document once
	/// per span — an edit, an undo entry, and a relayout apiece — to express a
	/// change that alters no text. The block layout is what a highlighter
	/// writes to for the same reason, and it is destroyed with the block, so
	/// nothing written here can outlive the document it describes.
	///
	/// Syntax is a foreground and the diff bands are backgrounds, so the two
	/// compose rather than compete: an added line keeps its tint and its
	/// keywords keep their colour. That is why syntax is not an extra
	/// selection — the bands are, and a selection that had to carry both would
	/// make one of them win.
	///
	/// Bounded to the paintable window, and applied only where the formats
	/// actually differ. Re-setting a block's formats marks its contents dirty,
	/// so an unconditional write would relayout every visible block on every
	/// scroll to restate colours it already carried.
	void diff_workbench::apply_syntax_formats(
		QPlainTextEdit&       aPane,
		const core::diff_side aSide,
		const core::line_span aPaintable)
	{
		const auto& spans = aSide == core::diff_side::left ? LeftSpans
														   : RightSpans;
		if (!CurrentResult) {
			return;
		}
		const auto& document = aSide == core::diff_side::left
			? CurrentResult->LeftDocument
			: CurrentResult->RightDocument;
		if (!document) {
			return;
		}

		// The window is found by search because the spans are ordered, which
		// `adopt_token_spans` established. Scanning from the front would make
		// the cost of one screenful proportional to the whole file.
		const auto  emptySpans = std::vector<application::token_span>{ };
		const auto& tokens     = spans ? *spans : emptySpans;
		auto        span       = std::ranges::lower_bound(
			tokens,
			aPaintable.Begin,
			{ },
			[](const application::token_span& aSpan) { return aSpan.Line; });
		const auto last = tokens.end();
		for (auto line = aPaintable.Begin;
			 line < aPaintable.End && line < document->Lines.size();
			 ++line) {
			while (span != last && span->Line < line) {
				++span;
				++Statistics.SyntaxSpansVisited;
			}
			const auto row = &aPane == Unified ? unified_row_for(aSide, line) : std::optional{ line };
			if (!row || (&aPane == Unified && aSide == core::diff_side::left && UnifiedPresentation->Rows[*row].Head)) {
				continue;
			}
			const auto block = aPane.document()->findBlockByNumber(static_cast<int>(*row));
			if (!block.isValid() || block.layout() == nullptr) {
				continue;
			}
			const auto& decoded = document->Lines[line];
			const auto  bytes   = std::span<const std::uint8_t>{
				document->Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(
					decoded.Content.End - decoded.Content.Begin)
			};
			// The run of spans this line owns, counted as they are stepped over so
			// the cost of one screenful stays visible in the statistics, and handed
			// to the shared painter whole: the ranges, the byte-to-character
			// conversion and the write-only-where-different rule are now the same
			// ones the Appearance preview paints through.
			const auto first  = span;
			const auto struck = &aPane == Unified && aSide == core::diff_side::left;
			while (span != last && span->Line == line) {
				++span;
				++Statistics.SyntaxSpansVisited;
			}
			const auto formats = code_syntax_format_ranges(
				code_syntax_format_request{
					.LineBytes       = bytes,
					.Spans           = std::span<const application::token_span>{ first, span },
					.Colors          = &Colors.Syntax,
					.StrikeOut       = struck,
					.StrikeOutLength = static_cast<int>(block.text().size()),
				});
			if (!apply_syntax_format_ranges(*aPane.document(), block, formats)) {
				continue;
			}
			++Statistics.SyntaxBlocksFormatted;
		}
	}

	/// \brief Marks the exact byte spans the core reported as changed.
	void diff_workbench::append_intraline_emphasis(
		QList<QTextEdit::ExtraSelection>&    aSelections,
		QPlainTextEdit&                      aPane,
		const core::diff_side                aSide,
		const core::line_span                aPaintable,
		const std::optional<core::line_span> aWashed)
	{
		const auto& document = aSide == core::diff_side::left
			? CurrentResult->LeftDocument
			: CurrentResult->RightDocument;
		if (!document) {
			return;
		}
		const auto color = aSide == core::diff_side::left
			? Colors.IntralineRemoved
			: Colors.IntralineAdded;
		if (!color.isValid()) {
			return;
		}

		// The fill a selected row's mark uses. The requirement is exact: a blue
		// line gets a blue mark, just darker or lighter. The mark therefore
		// derives from the band that row is actually wearing -- its own colour
		// with the wash over it -- and the core keeps the hue while moving
		// saturation and lightness away from the ground.
		const auto derivedFill = [this, &aPane](const core::diff_side aRowSide) {
			const auto band = aRowSide == core::diff_side::left ? Colors.Removed
																: Colors.Added;
			if (!band.isValid()) {
				return QColor{ };
			}
			const auto highlight = aPane.palette().color(QPalette::Highlight);
			const auto washed    = core::composited(
				core::appearance_color{ static_cast<std::uint8_t>(highlight.red()),
										static_cast<std::uint8_t>(highlight.green()),
										static_cast<std::uint8_t>(highlight.blue()),
										255U },
				core::appearance_color{ static_cast<std::uint8_t>(band.red()),
										static_cast<std::uint8_t>(band.green()),
										static_cast<std::uint8_t>(band.blue()),
										255U },
				core::selection_wash_alpha);
			const auto mark = core::derived_emphasis_fill(washed, Colors.Variant);
			return QColor{ mark.Red, mark.Green, mark.Blue };
		};

		const auto& pairs = CurrentResult->Alignment.Pairs;
		for (const auto& edit : CurrentResult->Intraline) {
			const auto pair = std::ranges::find_if(
				pairs,
				[&edit](const core::line_pair& aPair) {
					return aPair.Id == edit.Pair;
				});
			if (pair == pairs.end()) {
				continue;
			}
			const auto line = aSide == core::diff_side::left ? pair->LeftLine
															 : pair->RightLine;
			const auto span = aSide == core::diff_side::left ? edit.Left
															 : edit.Right;
			if (span.Begin >= span.End || line >= document->Lines.size()
				|| line < aPaintable.Begin || line >= aPaintable.End) {
				continue;
			}
			const auto& decoded = document->Lines[line];
			const auto  bytes   = std::span<const std::uint8_t>{
				document->Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(
					decoded.Content.End - decoded.Content.Begin)
			};
			const auto row = &aPane == Unified ? unified_row_for(aSide, line) : std::optional{ line };
			if (!row) {
				continue;
			}
			const auto block = aPane.document()->findBlockByNumber(static_cast<int>(*row));
			if (!block.isValid()) {
				continue;
			}
			const auto from = character_offset(bytes, span.Begin);
			const auto to   = character_offset(bytes, span.End);
			if (from >= to) {
				continue;
			}
			const auto selected = aWashed.has_value()
				&& line >= aWashed->Begin && line < aWashed->End;

			// On a selected row the fill is derived from what that row is
			// actually wearing, so it is a deeper shade of the band rather
			// than a pink patch on a blue one. Everywhere else the accepted
			// fill stays exactly as accepted.
			const auto fill = selected ? derivedFill(aSide) : color;
			append_intraline_emphasis_span(
				aSelections, block, from, to, fill,
				Colors.Supplied ? std::optional{ code_emphasis_ink{ .Normal  = Colors.Syntax[core::index_of(core::appearance_role::syntax_normal)],
																	.Variant = Colors.Variant } }
								: std::nullopt);
		}
	}

	void diff_workbench::announce_accessible_status(QString aText)
	{
		if (aText.isEmpty() || aText == LastAnnouncement) {
			return;
		}
		LastAnnouncement = std::move(aText);
		const auto self  = QPointer<diff_workbench>{ this };
		auto       event = QAccessibleAnnouncementEvent{ this, LastAnnouncement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
		if (self == nullptr) {
			return;
		}
	}

	void diff_workbench::set_status_shown(const bool aShown)
	{
		StatusShown = aShown;
		Status->setVisible(StatusShown && !Idle);
	}

	void diff_workbench::set_idle_direction(idle_direction aDirection)
	{
		IdleDirection = std::move(aDirection);
		IdleTitle->setText(IdleDirection.Title);
		IdleDetail->setText(IdleDirection.Detail);
		IdlePage->setAccessibleName(IdleDirection.Title);
		IdlePage->setAccessibleDescription(IdleDirection.Detail);
		IdleOpenFirst->setVisible(static_cast<bool>(IdleDirection.OpenFirst));
		IdleShowFiles->setVisible(static_cast<bool>(IdleDirection.ShowFiles));
	}

	void diff_workbench::set_idle(const bool aIdle)
	{
		Idle = aIdle;
		PanesSlot->setCurrentWidget(Idle ? IdlePage : PanesTop);
		Status->setVisible(StatusShown && !Idle);
	}

	auto diff_workbench::idle() const noexcept -> bool
	{
		return Idle;
	}

	auto diff_workbench::idle_page() const noexcept -> QWidget*
	{
		return IdlePage;
	}

	auto diff_workbench::idle_open_first_button() const noexcept -> QPushButton*
	{
		return IdleOpenFirst;
	}

	auto diff_workbench::idle_show_files_button() const noexcept -> QPushButton*
	{
		return IdleShowFiles;
	}

	void diff_workbench::set_status_observer(std::function<void(const QString&, bool)> aObserver)
	{
		StatusObserver = std::move(aObserver);
		if (StatusObserver) {
			StatusObserver(Status->text(), !StatusRoutine);
		}
	}

	auto diff_workbench::status_notable() const noexcept -> bool
	{
		return !StatusRoutine;
	}

	void diff_workbench::update_status(QString aText, const bool aRoutine)
	{
		Status->setText(std::move(aText));
		Status->setAccessibleDescription(Status->text());
		StatusRoutine = aRoutine;
		if (StatusObserver) {
			StatusObserver(Status->text(), !StatusRoutine);
		}
		announce_accessible_status(Status->text());
	}

	void diff_workbench::emit_reading_gesture() noexcept
	{
		if (Find) {
			Find->CopyMatch.reset();
			Find->PendingReveal.reset();
		}
		RestoredReadingLine.reset();
		if (ReadingGestureSink) {
			try {
				ReadingGestureSink();
			} catch (...) { }
		}
	}

	void diff_workbench::emit_selection(const bool aInteractive) noexcept
	{
		if (aInteractive) {
			emit_reading_gesture();
		}
		if (SelectionSink) {
			try {
				SelectionSink(Interaction.Selection);
			} catch (...) {
			}
		}
		emit_selection_observer(aInteractive);
	}

	void diff_workbench::emit_selection_observer(
		const bool aInteractive) noexcept
	{
		if (!SelectionObserver) {
			return;
		}
		try {
			SelectionObserver(Interaction.Selection, aInteractive);
		} catch (...) {
		}
	}

	void diff_workbench::clear_projection(QString aStatus)
	{
		end_head_edit();
		clear_source_find_document();
		HeldLine.reset();
		RestoredReadingLine.reset();
		PresentationReadingLine.reset();
		PresentationReadingSide.reset();
		const auto hadSelection = Interaction.Selection.has_value();
		drop_staged_publication();
		PendingMeasurement.reset();
		GeometryRevealPending = false;
		CurrentDetail.reset();
		SemanticIndex.reset();
		CurrentResult.reset();
		rebuild_unified_document();
		pane(Unified)->set_line_marks({ });
		pane(Unified)->set_conversation_anchor(std::nullopt);
		// A classification belongs to one blob. Keeping it across a cleared
		// projection would colour the next comparison's text with the previous
		// file's tokens, which reads as a highlighter bug and is a stale-state
		// bug. The composition re-adopts once the new blobs are classified.
		LeftSpans.reset();
		RightSpans.reset();
		SyntaxAwaited     = { };
		ComparisonLoading = false;
		apply_pending();
		Detail->set_token_spans(nullptr, nullptr);
		MovedSegments.clear();
		Interaction.Selection.reset();
		ConversationAnchor.reset();
		LeftHeights.clear();
		RightHeights.clear();
		Left->clear();
		Right->clear();
		Detail->present_empty();
		Controller->clear_axis();
		LinkMap->clear_geometry();
		set_line_selection(*Left, std::nullopt);
		set_line_selection(*Right, std::nullopt);
		pane(Left)->set_conversation_anchor(std::nullopt);
		pane(Right)->set_conversation_anchor(std::nullopt);
		AxisCache.invalidate(measured_geometry_change::file_or_epoch);
		PendingGeometryTriggers = diff_geometry_rebuild_trigger::none;
		update_status(std::move(aStatus));
		if (hadSelection) {
			emit_selection();
		}
	}
}
