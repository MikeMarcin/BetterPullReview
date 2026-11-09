#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/type_preview.hpp"
#include "diff_workbench_support.hpp"
#include "line_details_splitter.hpp"
#include "source_column.hpp"

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractTextDocumentLayout>
#include <QAnyStringView>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QFlags>
#include <QGuiApplication>
#include <QInputEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMargins>
#include <QMouseEvent>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QPointerEvent>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSinglePointEvent>
#include <QSize>
#include <QSplitter>
#include <QSplitterHandle>
#include <QString>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// \brief The document y the viewport's top edge sits at.
		///
		/// \details Taken from the axis's own per-line boundaries so the
		/// origin and the ribbon endpoints share one measurement basis and
		/// cannot disagree by construction. Falls back to the caller's estimate
		/// only when the pane cannot say which block is first or the axis has
		/// no boundary for it, which is a torn state rather than a normal one -
		/// returning something wrong quietly is what this replaces.
		[[nodiscard]] auto measured_document_origin(
			const source_pane&             aPane,
			const measured_alignment_axis& aAxis,
			const core::diff_side          aSide,
			const qreal                    aEstimate) -> qreal
		{
			const auto visible = aPane.visible_origin();
			if (!visible.has_value()) {
				return aEstimate;
			}
			const auto boundary = aSide == core::diff_side::left
				? aAxis.left_line_origin(visible->first)
				: aAxis.right_line_origin(visible->first);
			return boundary.has_value() ? *boundary - visible->second
										: aEstimate;
		}

		/// Lines appended per batch, and the time after which a turn yields.
		inline constexpr int staged_publication_batch_lines = 256;

		inline constexpr std::int64_t staged_publication_turn_ns = 4'000'000;

		/// \brief [[BPR-1212]]. A key the line cursor takes: no modifier but
		/// Shift, and the keypad's, which Enter on the keypad carries.
		[[nodiscard]] auto line_key_modifiers(const QKeyEvent& aEvent) noexcept -> bool
		{
			return (aEvent.modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) == Qt::NoModifier;
		}

		/// \brief [[BPR-1724]]. The row a selection ending at `aEnd`
		/// covers last, for a selection that starts at `aStart`.
		/// \details A selection that reaches the start of a line with text has
		/// not entered it, so it ends on the line above. An empty line's start
		/// is the whole of it, so a drag whose pointer ends on an empty line
		/// covers that line; it stopped at the line above. `aCount` is the
		/// pane's rows: a file's final newline leaves an empty block after its
		/// last line, which is no line.
		[[nodiscard]] auto covered_row(const QTextDocument& aDocument, const int aStart, const int aEnd, const std::uint32_t aCount) -> std::uint32_t
		{
			const auto block = aDocument.findBlock(aEnd);
			auto       row   = std::max(0, block.blockNumber());
			if (aEnd > aStart && block.isValid() && aEnd == block.position() && block.length() > 1 && row > 0) {
				--row;
			}
			return std::min(static_cast<std::uint32_t>(row), aCount > 0U ? aCount - 1U : 0U);
		}
	}

	void diff_workbench::clear_conversation_anchor()
	{
		if (!ConversationAnchor.has_value()) {
			return;
		}
		ConversationAnchor.reset();
		update_diff_paint();
	}

	auto diff_workbench::conversation_anchor() const noexcept
		-> const std::optional<core::source_line_ref>&
	{
		return ConversationAnchor;
	}

	auto diff_workbench::conversation_anchor_marker_rect(
		const core::diff_side aSide) const -> QRect
	{
		if (OnePaneEnabled) {
			return ConversationAnchor && ConversationAnchor->Side == aSide
				? pane(Unified)->conversation_anchor_rect()
				: QRect{ };
		}
		return pane(aSide == core::diff_side::left ? Left : Right)
			->conversation_anchor_rect();
	}

	auto diff_workbench::line_rect(const core::diff_side aSide, const std::uint32_t aLine) const -> QRect
	{
		// [[BPR-1542]]. The open thread hangs from this rect while the code
		// scrolls under it, so it must be exact however far the line has
		// gone: Qt's own block geometry stops about a viewport above the top
		// row, and the thread stopped with it.
		if (OnePaneEnabled) {
			const auto index = unified_row_for(aSide, aLine);
			if (!CurrentResult || !index) {
				return { };
			}
			const auto row = exact_row_rect(Unified, *index);
			return QRect{ Unified->viewport()->mapTo(this, row.topLeft()), row.size() };
		}
		auto* const source = aSide == core::diff_side::left ? Left : Right;
		const auto  row    = exact_row_rect(source, aLine);
		if (row.isEmpty()) {
			return { };
		}
		return QRect{ source->viewport()->mapTo(this, row.topLeft()), row.size() };
	}

	auto diff_workbench::source_line_visible(core::diff_side aSide, std::uint32_t aLine) const -> bool
	{
		const auto* editor = OnePaneEnabled ? Unified : aSide == core::diff_side::left ? Left
																					   : Right;
		const auto  row    = line_rect(aSide, aLine);
		return editor->isVisible() && !row.isEmpty() && editor->viewport()->visibleRegion().intersects(QRect{ editor->viewport()->mapFrom(this, row.topLeft()), row.size() });
	}

	void diff_workbench::set_line_marks(const core::diff_side aSide, std::vector<diff_line_mark> aMarks)
	{
		for (auto& mark : aMarks) {
			mark.Side       = aSide;
			mark.SourceLine = mark.Line;
		}
		pane(aSide == core::diff_side::left ? Left : Right)->set_line_marks(std::move(aMarks));
		if (OnePaneEnabled) {
			update_unified_paint();
		}
		refresh_thread_ticks();
		refresh_find_ticks();
	}

	void diff_workbench::set_thread_markers(const bool aEnabled)
	{
		if (ThreadMarkers == aEnabled) {
			return;
		}
		ThreadMarkers = aEnabled;
		for (auto* const source : { Left, Right, Unified }) {
			pane(source)->set_marker_column(aEnabled);
		}
		refresh_thread_ticks();
		refresh_find_ticks();
	}

	auto diff_workbench::thread_markers() const noexcept -> bool
	{
		return ThreadMarkers;
	}

	void diff_workbench::set_mark_colours(const QColor& aThread, const QColor& aNew)
	{
		for (auto* const source : { Left, Right, Unified }) {
			pane(source)->set_mark_colors(aThread, aNew);
		}
		for (auto* const bar : { static_cast<QScrollBar*>(SharedScroll), Unified->verticalScrollBar() }) {
			if (auto* const ticked = dynamic_cast<thread_tick_scroll_bar*>(bar); ticked != nullptr && (ticked->Thread != aThread || ticked->New != aNew)) {
				ticked->Thread = aThread;
				ticked->New    = aNew;
				ticked->update();
			}
		}
	}

	void diff_workbench::set_line_mark_dispatch(std::function<void(core::diff_side, std::uint32_t)> aDispatch)
	{
		LineMarkDispatch   = std::move(aDispatch);
		const auto forward = [this](const diff_line_mark& aMark) {
			if (LineMarkDispatch) {
				LineMarkDispatch(aMark.Side, aMark.SourceLine);
			}
		};
		for (auto* const source : { Left, Right, Unified }) {
			pane(source)->set_mark_dispatch(forward);
		}
	}

	void diff_workbench::set_line_add_dispatch(std::function<void(std::uint32_t, std::uint32_t)> aDispatch)
	{
		// The gutter's "+" is on the head side. A base line takes a new comment
		// from the right-click menu or Return once GitHub numbers it ([[BPR-1212]]).
		pane(Right)->set_add_dispatch(std::move(aDispatch));
	}

	auto diff_workbench::line_add_rect() const -> QRect
	{
		return pane(Right)->add_rect();
	}

	void diff_workbench::set_thread_tick_dispatch(std::function<void(core::diff_side, std::uint32_t)> aDispatch)
	{
		ThreadTickDispatch = std::move(aDispatch);
		for (auto* const bar : { static_cast<QScrollBar*>(SharedScroll), Unified->verticalScrollBar() }) {
			if (auto* const ticked = dynamic_cast<thread_tick_scroll_bar*>(bar); ticked != nullptr) {
				ticked->GoTo = [this](const core::diff_side aSide, const std::uint32_t aLine) {
					if (ThreadTickDispatch) {
						ThreadTickDispatch(aSide, aLine);
					}
				};
			}
		}
	}

	void diff_workbench::refresh_thread_ticks()
	{
		auto shared  = std::vector<diff_thread_tick>{ };
		auto unified = std::vector<diff_thread_tick>{ };
		if (ThreadMarkers) {
			const auto rows = static_cast<qreal>(std::max(1, Unified->document()->blockCount() - 1));
			for (const auto side : { core::diff_side::left, core::diff_side::right }) {
				for (const auto& mark : line_marks(side)) {
					auto tick = diff_thread_tick{ .Side = side, .Line = mark.Line, .Unread = mark.Kind == diff_line_mark_kind::new_thread, .Resolved = mark.Resolved, .Draft = mark.Draft, .Open = mark.Open, .Description = mark.Description };
					if (const auto fraction = Controller->scrollbar_fraction(side, mark.Line); fraction.has_value()) {
						tick.Fraction = *fraction;
						shared.push_back(tick);
					}
					if (const auto row = unified_row_for(side, mark.Line); row.has_value()) {
						tick.Fraction = static_cast<qreal>(*row) / rows;
						unified.push_back(tick);
					}
				}
			}
		}
		if (auto* const bar = dynamic_cast<thread_tick_scroll_bar*>(SharedScroll); bar != nullptr) {
			bar->set_ticks(std::move(shared));
		}
		if (auto* const bar = dynamic_cast<thread_tick_scroll_bar*>(Unified->verticalScrollBar()); bar != nullptr) {
			bar->set_ticks(std::move(unified));
		}
	}

	auto diff_workbench::scroll_by_wheel(const QWheelEvent& aEvent) -> bool
	{
		if (!CurrentResult) {
			return false;
		}
		auto* const target = OnePaneEnabled ? Unified->viewport() : Right->viewport();
		const auto  centre = QPointF{ target->rect().center() };
		auto        wheel  = QWheelEvent{ centre, target->mapToGlobal(centre), aEvent.pixelDelta(), aEvent.angleDelta(),
										  aEvent.buttons(), aEvent.modifiers(), aEvent.phase(), aEvent.inverted(), aEvent.source(), aEvent.pointingDevice() };
		wheel.setTimestamp(aEvent.timestamp());
		if (OnePaneEnabled) {
			const auto before = std::pair{ Unified->verticalScrollBar()->value(), pane(Unified)->within_line_position() };
			QCoreApplication::sendEvent(target, &wheel);
			return std::pair{ Unified->verticalScrollBar()->value(), pane(Unified)->within_line_position() } != before;
		}
		const auto before = Controller->position();
		QCoreApplication::sendEvent(target, &wheel);
		return std::abs(Controller->position() - before) > 0.01;
	}

	auto diff_workbench::thread_ticks() const -> std::vector<diff_thread_tick>
	{
		auto* const bar = dynamic_cast<thread_tick_scroll_bar*>(OnePaneEnabled ? Unified->verticalScrollBar() : SharedScroll);
		return bar != nullptr ? bar->ticks() : std::vector<diff_thread_tick>{ };
	}

	auto diff_workbench::thread_tick_rect(const core::diff_side aSide, const std::uint32_t aLine) const -> QRect
	{
		auto* const bar = dynamic_cast<thread_tick_scroll_bar*>(OnePaneEnabled ? Unified->verticalScrollBar() : SharedScroll);
		if (bar == nullptr) {
			return { };
		}
		const auto& ticks = bar->ticks();
		const auto  at    = std::ranges::find_if(ticks, [aSide, aLine](const diff_thread_tick& aTick) { return aTick.Side == aSide && aTick.Line == aLine; });
		return at == ticks.end() ? QRect{ } : bar->tick_rect(*at);
	}

	auto diff_workbench::targeted_thread_tick() const -> std::optional<diff_thread_tick>
	{
		auto* const bar = dynamic_cast<thread_tick_scroll_bar*>(OnePaneEnabled ? Unified->verticalScrollBar() : SharedScroll);
		return bar != nullptr ? bar->targeted() : std::nullopt;
	}

	auto diff_workbench::line_mark_rect(const core::diff_side aSide, const std::uint32_t aLine) const -> QRect
	{
		auto* const source = pane(OnePaneEnabled ? Unified : aSide == core::diff_side::left ? Left
																							: Right);
		for (const auto& mark : source->line_marks()) {
			if (mark.Side == aSide && mark.SourceLine == aLine) {
				const auto box = source->marker_rect(mark);
				if (box.isEmpty()) {
					return { };
				}
				auto* const gutter = source->findChild<QWidget*>(QStringLiteral("diffSourceLineNumbers"));
				return gutter == nullptr ? QRect{ } : QRect{ gutter->mapTo(this, box.topLeft()), box.size() };
			}
		}
		return { };
	}

	auto diff_workbench::line_marks(const core::diff_side aSide) const -> const std::vector<diff_line_mark>&
	{
		return pane(aSide == core::diff_side::left ? Left : Right)->line_marks();
	}

	void diff_workbench::set_frame_observer(std::function<void()> aObserver)
	{
		FrameObserver = std::move(aObserver);
	}

	auto diff_workbench::left_pane() const noexcept -> QPlainTextEdit*
	{
		return Left;
	}

	auto diff_workbench::right_pane() const noexcept -> QPlainTextEdit*
	{
		return Right;
	}

	auto diff_workbench::unified_pane() const noexcept -> QPlainTextEdit*
	{
		return Unified;
	}

	auto diff_workbench::source_line_destination() const -> std::optional<core::source_line_ref>
	{
		if (!CurrentResult || !SemanticIndex || !isVisible()) {
			return std::nullopt;
		}
		const auto count = [this](const core::diff_side aSide) {
			const auto& document = aSide == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
			return document ? document->Lines.size() : 0U;
		};
		auto side = core::diff_side::right;
		if (OnePaneEnabled && Interaction.Selection) {
			side = Interaction.Selection->PreferredLine.Side;
		} else if (!OnePaneEnabled && Left->hasFocus()) {
			side = core::diff_side::left;
		} else if (!OnePaneEnabled && Right->hasFocus()) {
			side = core::diff_side::right;
		} else if (!OnePaneEnabled && RecentSourceContext == CurrentContext) {
			side = RecentSourceSide;
		}
		if (count(side) == 0U) {
			side = side == core::diff_side::left ? core::diff_side::right : core::diff_side::left;
		}
		if (count(side) == 0U) {
			return std::nullopt;
		}
		const auto line = Interaction.Selection && Interaction.Selection->PreferredLine.Side == side
			? Interaction.Selection->PreferredLine.Line
			: 0U;
		return core::source_line_ref{ CurrentResult->Alignment.Digest, side, line };
	}

	auto diff_workbench::go_to_source_line(const core::source_line_ref aLine, const std::uint32_t aColumn) -> bool
	{
		if (!CurrentResult || !SemanticIndex || aLine.Comparison != CurrentResult->Alignment.Digest
			|| !core::select_line(*SemanticIndex, aLine)) {
			return false;
		}
		const auto row = OnePaneEnabled ? unified_row_for(aLine.Side, aLine.Line) : std::optional{ aLine.Line };
		if (!row) {
			return false;
		}
		auto* const editor = OnePaneEnabled ? Unified : aLine.Side == core::diff_side::left ? Left
																							: Right;
		const auto  block  = editor->document()->findBlockByNumber(static_cast<int>(*row));
		if (!block.isValid() || !valid_source_column(block.text(), aColumn)) {
			return false;
		}
		const auto material = CurrentResult;
		const auto context  = CurrentContext;
		const auto shared   = Controller->position();
		for (auto* const source : { Left, Right, Unified }) {
			const auto current = static_cast<std::uint32_t>(source->textCursor().blockNumber());
			place_line_cursor(source, current, current);
		}
		auto cursor = QTextCursor{ block };
		cursor.setPosition(block.position() + static_cast<int>(aColumn));
		editor->setTextCursor(cursor);
		if (!OnePaneEnabled) {
			static_cast<void>(Controller->set_position(shared));
		}
		if (!select_line(aLine.Side, aLine.Line, true) || CurrentResult != material || CurrentContext != context || !Interaction.Selection) {
			return false;
		}
		PresentationReadingLine.reset();
		RestoredReadingLine.reset();
		HeldLine.reset();
		ConversationAnchor = aLine;
		// A resize or gutter change may still be remeasuring. The geometry
		// owner consumes this exact anchor when its shared axis is ready.
		GeometryRevealPending  = !reveal_exact_line(*Interaction.Selection, aLine);
		const auto  caret      = editor->cursorRect(cursor);
		auto* const horizontal = editor->horizontalScrollBar();
		if (caret.left() < 0) {
			horizontal->setValue(horizontal->value() + caret.left());
		} else if (caret.right() > editor->viewport()->width()) {
			horizontal->setValue(horizontal->value() + caret.right() - editor->viewport()->width());
		}
		update_selection_paint();
		focus_source(aLine.Side);
		announce_line_selection();
		return true;
	}

	void diff_workbench::focus_source()
	{
		(OnePaneEnabled ? Unified : Right)->setFocus(Qt::ShortcutFocusReason);
	}

	void diff_workbench::focus_source(core::diff_side aSide)
	{
		(OnePaneEnabled ? Unified : aSide == core::diff_side::left ? Left
																   : Right)
			->setFocus(Qt::ShortcutFocusReason);
	}

	auto diff_workbench::detail_pane() const noexcept -> QPlainTextEdit*
	{
		return Detail;
	}

	auto diff_workbench::line_details() const noexcept -> line_details_pane*
	{
		return Detail;
	}

	void diff_workbench::set_center_ribbon_width(const std::uint32_t aWidth)
	{
		if (aWidth >= application::minimum_center_ribbon_width && aWidth <= application::maximum_center_ribbon_width) {
			LinkMap->set_logical_width(static_cast<int>(aWidth));
		}
	}

	void diff_workbench::set_line_details_visible(const bool aVisible)
	{
		if (!Detail->isHidden() == aVisible) {
			return;
		}
		Detail->setVisible(aVisible);
		if (aVisible) {
			Vertical->apply_layout();
			update_line_details();
		}
	}

	auto diff_workbench::line_details_visible() const noexcept -> bool
	{
		return !Detail->isHidden();
	}

	void diff_workbench::set_line_details_height(const std::optional<std::uint32_t> aHeight)
	{
		Vertical->set_preference(aHeight);
	}

	auto diff_workbench::line_details_height() const noexcept -> std::optional<std::uint32_t>
	{
		return Vertical->preference();
	}

	void diff_workbench::set_line_details_resize_sink(std::function<void(std::optional<std::uint32_t>)> aSink)
	{
		Vertical->Changed = std::move(aSink);
	}

	auto diff_workbench::line_details_handle() const -> QSplitterHandle*
	{
		return Vertical->count() == 2 ? Vertical->handle(1) : nullptr;
	}

	auto diff_workbench::shared_scrollbar() const noexcept -> QScrollBar*
	{
		return OnePaneEnabled ? Unified->verticalScrollBar() : SharedScroll;
	}

	auto diff_workbench::comparison_splitter() const noexcept
		-> diff_comparison_splitter*
	{
		return Comparison;
	}

	/// \brief Tells the link map where the pane viewports actually are.
	///
	/// \details The map is a sibling of the panes, so its own top and height
	/// are not theirs: a pane contributes a frame, and a horizontal scrollbar
	/// takes height from the viewport without taking any from the map. Ribbon
	/// endpoints are computed in viewport coordinates, so drawing them at
	/// their own y met the panes in the wrong place, which is BPR-0208. The
	/// band is declared rather than inferred because the difference depends on
	/// style and on whether a scrollbar is showing, and neither is knowable
	/// from the map.
	void diff_workbench::declare_link_map_band()
	{
		auto* const parent = LinkMap->parentWidget();
		if (parent == nullptr) {
			return;
		}
		const auto mapTop = LinkMap->mapTo(parent, QPoint{ 0, 0 }).y();
		const auto paneTop
			= Left->viewport()->mapTo(parent, QPoint{ 0, pane(Left)->within_line_offset() }).y();
		LinkMap->set_viewport_band(
			static_cast<qreal>(paneTop - mapTop),
			static_cast<qreal>(pane(Left)->viewport_extent()));
	}

	auto diff_workbench::link_map() const noexcept -> diff_link_map*
	{
		return LinkMap;
	}

	auto diff_workbench::scroll_controller() const noexcept
		-> comparison_scroll_controller*
	{
		return Controller;
	}

	auto diff_workbench::selected_source_range() const -> std::optional<std::pair<core::source_line_ref, core::source_line_ref>>
	{
		if (!SemanticIndex || !Interaction.Selection) {
			return std::nullopt;
		}
		auto first = Interaction.Selection->PreferredLine;
		auto last  = first;
		if (OnePaneEnabled) {
			const auto cursor = Unified->textCursor();
			if (cursor.hasSelection()) {
				const auto begin = Unified->document()->findBlock(cursor.selectionStart()).blockNumber();
				const auto end   = static_cast<int>(covered_row(*Unified->document(), cursor.selectionStart(), cursor.selectionEnd(), static_cast<std::uint32_t>(UnifiedPresentation->Rows.size())));
				if (begin < 0 || end < begin || static_cast<std::size_t>(end) >= UnifiedPresentation->Rows.size()) {
					return std::nullopt;
				}
				std::optional<std::uint32_t> previous;
				for (int row = begin; row <= end; ++row) {
					const auto& source = UnifiedPresentation->Rows[static_cast<std::size_t>(row)];
					const auto  line   = first.Side == core::diff_side::left ? source.Base : source.Head;
					if (!line || (previous && *line != *previous + 1U)) {
						return std::nullopt;
					}
					if (!previous) {
						first.Line = *line;
					}
					last.Line = *line;
					previous  = line;
				}
			}
			return core::select_line(*SemanticIndex, first) && core::select_line(*SemanticIndex, last) ? std::optional{ std::pair{ first, last } } : std::nullopt;
		}
		const auto* editor = first.Side == core::diff_side::left ? Left : Right;
		const auto  cursor = editor->textCursor();
		if (cursor.hasSelection()) {
			first.Line = static_cast<std::uint32_t>(editor->document()->findBlock(cursor.selectionStart()).blockNumber());
			last.Line  = covered_row(*editor->document(), cursor.selectionStart(), cursor.selectionEnd(), pane_row_count(editor));
		}
		if (!core::select_line(*SemanticIndex, first) || !core::select_line(*SemanticIndex, last)) {
			return std::nullopt;
		}
		return std::pair{ first, last };
	}

	auto diff_workbench::restore_source_range(core::source_line_ref aFirst, core::source_line_ref aLast) -> bool
	{
		if (!SemanticIndex || aFirst.Comparison != aLast.Comparison || aFirst.Side != aLast.Side || aFirst.Line > aLast.Line || !core::select_line(*SemanticIndex, aLast)) {
			return false;
		}
		const auto selection = core::select_line(*SemanticIndex, aFirst);
		if (!selection || !restore_selection(*selection)) {
			return false;
		}
		auto*      editor = aFirst.Side == core::diff_side::left ? Left : Right;
		const auto first  = editor->document()->findBlockByNumber(static_cast<int>(aFirst.Line));
		const auto last   = editor->document()->findBlockByNumber(static_cast<int>(aLast.Line));
		if (!first.isValid() || !last.isValid()) {
			return false;
		}
		auto cursor = QTextCursor{ editor->document() };
		cursor.setPosition(first.position());
		cursor.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
		editor->setTextCursor(cursor);
		return true;
	}

	void diff_workbench::set_line_draft_dispatch(std::function<void(bool)> aDispatch)
	{
		LineDraftDispatch = std::move(aDispatch);
	}

	auto diff_workbench::last_announcement() const noexcept -> const QString&
	{
		return LastAnnouncement;
	}

	auto diff_workbench::line_draft_range() const -> std::optional<std::pair<core::source_line_ref, core::source_line_ref>>
	{
		if (!CurrentResult || !SemanticIndex || !Interaction.Selection) {
			return std::nullopt;
		}
		const auto preferred = Interaction.Selection->PreferredLine;
		auto       first     = preferred;
		auto       last      = preferred;
		if (OnePaneEnabled) {
			// The rows the pane's selection covers, when it holds the selected
			// line's row; each end keeps the side its row shows.
			const auto cursor = Unified->textCursor();
			const auto row    = unified_row_for(preferred.Side, preferred.Line);
			if (cursor.hasSelection() && row) {
				const auto  begin = Unified->document()->findBlock(cursor.selectionStart()).blockNumber();
				const auto  end   = static_cast<int>(covered_row(*Unified->document(), cursor.selectionStart(), cursor.selectionEnd(), static_cast<std::uint32_t>(UnifiedPresentation->Rows.size())));
				const auto& rows  = UnifiedPresentation->Rows;
				if (begin >= 0 && end >= begin && static_cast<std::size_t>(end) < rows.size() && *row >= static_cast<std::uint32_t>(begin) && *row <= static_cast<std::uint32_t>(end)) {
					const auto at = [&rows, &preferred](const int aRow) {
						const auto& source = rows[static_cast<std::size_t>(aRow)];
						return core::source_line_ref{ .Comparison = preferred.Comparison, .Side = source.Head ? core::diff_side::right : core::diff_side::left, .Line = source.Head.value_or(source.Base.value_or(0U)) };
					};
					first = at(begin);
					last  = at(end);
				}
			}
			return std::pair{ first, last };
		}
		const auto* const editor = preferred.Side == core::diff_side::left ? Left : Right;
		const auto        cursor = editor->textCursor();
		if (cursor.hasSelection()) {
			const auto block = [editor](const int aPosition) {
				return static_cast<std::uint32_t>(std::max(0, editor->document()->findBlock(aPosition).blockNumber()));
			};
			const auto begin = block(cursor.selectionStart());
			const auto end   = covered_row(*editor->document(), cursor.selectionStart(), cursor.selectionEnd(), pane_row_count(editor));
			// A selection ending at the start of a line covers that line only
			// when it is the selected one, as a Shift-click on it makes it. A
			// selection left elsewhere on the pane is not this line's.
			if (preferred.Line >= begin && preferred.Line <= std::max(end, block(cursor.selectionEnd()))) {
				first.Line = std::min(begin, preferred.Line);
				last.Line  = std::max(end, preferred.Line);
			}
		}
		return std::pair{ first, last };
	}

	auto diff_workbench::handle_line_key(QPlainTextEdit* const aPane, QEvent* const aEvent) -> bool
	{
		auto* const key = static_cast<QKeyEvent*>(aEvent);
		if (!line_key_modifiers(*key) || (aPane == Unified) != OnePaneEnabled) {
			return false;
		}
		const auto enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
		const auto arrow = key->key() == Qt::Key_Up || key->key() == Qt::Key_Down;
		if ((enter && !LineDraftDispatch) || (arrow && !CurrentResult) || (!enter && !arrow)) {
			return false;
		}
		// The pane claims the key before any window shortcut can take it.
		key->accept();
		if (aEvent->type() == QEvent::ShortcutOverride) {
			return true;
		}
		const auto shift = key->modifiers().testFlag(Qt::ShiftModifier);
		if (enter) {
			const auto dispatch = LineDraftDispatch;
			dispatch(shift);
			return true;
		}
		static_cast<void>(move_pane_line_cursor(aPane, key->key() == Qt::Key_Up ? -1 : 1, shift));
		return true;
	}

	auto diff_workbench::move_line_cursor(const int aStep, const bool aExtend) -> bool
	{
		auto* pane = OnePaneEnabled ? Unified : Right;
		if (!OnePaneEnabled && Left->hasFocus()) {
			pane = Left;
		}
		return move_pane_line_cursor(pane, aStep, aExtend);
	}

	auto diff_workbench::pane_row_count(const QPlainTextEdit* const aPane) const -> std::uint32_t
	{
		if (!CurrentResult) {
			return 0U;
		}
		if (aPane == Unified) {
			return static_cast<std::uint32_t>(UnifiedPresentation->Rows.size());
		}
		return aPane == Left ? CurrentResult->Alignment.LeftLineCount : CurrentResult->Alignment.RightLineCount;
	}

	auto diff_workbench::line_cursor_ends(QPlainTextEdit* const aPane) const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>
	{
		// Where the cursor is: the selected line's row, or the two ends of the
		// pane's selection when that holds it.
		if (!CurrentResult || !Interaction.Selection) {
			return std::nullopt;
		}
		const auto& preferred = Interaction.Selection->PreferredLine;
		const auto  count     = pane_row_count(aPane);
		auto        row       = std::optional<std::uint32_t>{ };
		if (aPane == Unified) {
			row = unified_row_for(preferred.Side, preferred.Line);
		} else if (preferred.Side == (aPane == Left ? core::diff_side::left : core::diff_side::right) && preferred.Line < count) {
			row = preferred.Line;
		}
		if (!row) {
			return std::nullopt;
		}
		auto       ends   = std::pair{ *row, *row };
		const auto cursor = aPane->textCursor();
		if (cursor.hasSelection()) {
			auto* const document = aPane->document();
			const auto  from     = static_cast<std::uint32_t>(std::max(0, document->findBlock(cursor.anchor()).blockNumber()));
			const auto  ended    = static_cast<std::uint32_t>(std::max(0, document->findBlock(cursor.position()).blockNumber()));
			// A selection dragged to the start of the next line ends above it,
			// unless that line is the selected one (a Shift-click) or is empty
			// ([[BPR-1724]]).
			const auto to = ended == *row ? ended : covered_row(*document, cursor.anchor(), cursor.position(), count);
			if (*row >= std::min(from, ended) && *row <= std::max(from, ended)) {
				ends = std::pair{ from, to };
			}
		}
		return ends;
	}

	auto diff_workbench::select_pane_row(QPlainTextEdit* const aPane, const std::uint32_t aRow) -> bool
	{
		if (aPane != Unified) {
			return select_line(aPane == Left ? core::diff_side::left : core::diff_side::right, aRow, true);
		}
		if (aRow >= UnifiedPresentation->Rows.size()) {
			return false;
		}
		// A row shows its head line when it has one, as a click on it selects.
		const auto& identity = UnifiedPresentation->Rows[aRow];
		return select_line(identity.Head ? core::diff_side::right : core::diff_side::left, identity.Head.value_or(identity.Base.value_or(0U)), true);
	}

	void diff_workbench::place_line_cursor(QPlainTextEdit* const aPane, const std::uint32_t aAnchor, const std::uint32_t aNext)
	{
		// Whole lines from the anchor to the moving end, which the caret is
		// on; the pane keeps its scroll, which the caller reveals.
		auto* const document = aPane->document();
		const auto  first    = document->findBlockByNumber(static_cast<int>(aAnchor));
		const auto  last     = document->findBlockByNumber(static_cast<int>(aNext));
		if (!first.isValid() || !last.isValid()) {
			return;
		}
		auto cursor = QTextCursor{ document };
		if (aAnchor == aNext) {
			cursor.setPosition(last.position());
		} else if (aAnchor < aNext) {
			cursor.setPosition(first.position());
			cursor.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
		} else {
			cursor.setPosition(first.position() + first.length() - 1);
			cursor.setPosition(last.position(), QTextCursor::KeepAnchor);
		}
		auto* const vertical   = aPane->verticalScrollBar();
		auto* const horizontal = aPane->horizontalScrollBar();
		const auto  top        = vertical->value();
		const auto  left       = horizontal->value();
		const auto  offset     = pane(aPane)->within_line_position();
		aPane->setTextCursor(cursor);
		vertical->setValue(top);
		horizontal->setValue(left);
		pane(aPane)->set_within_line_offset(offset);
	}

	auto diff_workbench::move_pane_line_cursor(QPlainTextEdit* const aPane, const int aStep, const bool aExtend) -> bool
	{
		if (!CurrentResult || !SemanticIndex) {
			return false;
		}
		const auto count = pane_row_count(aPane);
		if (count == 0U) {
			return false;
		}
		// With no line on this pane, the first line in view is selected and
		// nothing moves yet.
		const auto ends   = line_cursor_ends(aPane);
		auto       next   = std::uint32_t{ 0U };
		auto       anchor = std::uint32_t{ 0U };
		if (!ends) {
			next   = std::min(static_cast<std::uint32_t>(std::max(0, aPane->cursorForPosition(QPoint{ 0, 0 }).blockNumber())), count - 1U);
			anchor = next;
		} else {
			next   = static_cast<std::uint32_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(ends->second) + aStep, 0, static_cast<std::int64_t>(count) - 1));
			anchor = ends->first;
		}
		if (!aExtend) {
			anchor = next;
		}
		const auto row     = exact_row_rect(aPane, next);
		const auto visible = aPane->viewport()->visibleRegion().intersected(aPane->viewport()->mask()).boundingRect();
		if (!select_pane_row(aPane, next)) {
			return false;
		}
		if (aPane == Unified) {
			place_line_cursor(aPane, anchor, next);
			reveal_keyboard_row(aPane, row, visible);
		} else {
			// The cursor keeps itself in view through the linked scroll, never
			// by scrolling one pane on its own.
			reveal_keyboard_row(aPane, row, visible);
			place_line_cursor(aPane, anchor, next);
		}
		announce_line_selection();
		return true;
	}

	void diff_workbench::select_numbered_line(QPlainTextEdit* const aPane, const std::uint32_t aRow, const bool aExtend)
	{
		// [[BPR-1724]]. As on GitHub: a click on a number selects its
		// line, and a Shift-click or a drag over the numbers selects from the
		// selection's anchor to it. The pane holds the lines whole, as Shift
		// and the arrows leave them, so Return, the menu and Comment take them.
		if (!CurrentResult || !SemanticIndex || (aPane == Unified) != OnePaneEnabled || aRow >= pane_row_count(aPane)) {
			return;
		}
		const auto ends   = aExtend ? line_cursor_ends(aPane) : std::nullopt;
		const auto anchor = ends ? ends->first : aRow;
		if (!select_pane_row(aPane, aRow)) {
			return;
		}
		place_line_cursor(aPane, anchor, aRow);
		announce_line_selection();
	}

	auto diff_workbench::select_line_for_menu(QPlainTextEdit* const aPane, const QPoint aPosition) -> bool
	{
		// [[BPR-1212]]. A right-click outside the selection selects its line
		// first, as a click would; inside the selected lines it keeps them all.
		// [[BPR-1724]]. In one pane a row is the line it shows: its head
		// line, or the base line a removed row shows.
		if (!CurrentResult || !SemanticIndex || aPane == nullptr || (aPane == Unified) != OnePaneEnabled) {
			return false;
		}
		const auto block = aPane->cursorForPosition(aPosition).blockNumber();
		if (block < 0 || static_cast<std::uint32_t>(block) >= pane_row_count(aPane)) {
			return false;
		}
		const auto row     = static_cast<std::uint32_t>(block);
		auto       clicked = core::source_line_ref{ .Comparison = CurrentResult->Alignment.Digest, .Side = aPane == Left ? core::diff_side::left : core::diff_side::right, .Line = row };
		if (aPane == Unified) {
			const auto& identity = UnifiedPresentation->Rows[row];
			clicked.Side         = identity.Head ? core::diff_side::right : core::diff_side::left;
			clicked.Line         = identity.Head.value_or(identity.Base.value_or(0U));
		}
		const auto inside = [&] {
			if (!Interaction.Selection) {
				return false;
			}
			if (Interaction.Selection->PreferredLine == clicked) {
				return true;
			}
			const auto range = line_draft_range();
			if (!range) {
				return false;
			}
			if (aPane == Unified) {
				const auto first = unified_row_for(range->first.Side, range->first.Line);
				const auto last  = unified_row_for(range->second.Side, range->second.Line);
				return first && last && row >= std::min(*first, *last) && row <= std::max(*first, *last);
			}
			return range->first.Side == clicked.Side && range->second.Side == clicked.Side
				&& row >= std::min(range->first.Line, range->second.Line) && row <= std::max(range->first.Line, range->second.Line);
		}();
		if (!inside) {
			static_cast<void>(select_line(clicked.Side, clicked.Line, true));
		}
		return true;
	}

	void diff_workbench::reveal_keyboard_row(QPlainTextEdit* const aPane, const QRect& aRow, const QRect& aVisible)
	{
		// The caller captures the displayed content before selection changes it.
		// visibleRegion alone includes overscan behind the native scrollbar;
		// intersect the viewport mask so reveal uses the actual source edge.
		const auto& row     = aRow;
		const auto& visible = aVisible;
		if (row.isEmpty() || visible.isEmpty()) {
			return;
		}
		const auto delta = row.top() < visible.top() ? row.top() - visible.top()
													 : std::max(0, row.bottom() - visible.bottom());
		if (delta == 0) {
			return;
		}
		if (aPane == Unified) {
			// [[BPR-1476]]. The document margin shows above row 0 only while
			// row 0 is the top row, so a top row past it starts one margin
			// further down the document than its rows alone.
			auto* const vertical = aPane->verticalScrollBar();
			const auto  height   = pane(aPane)->line_row_rect(0U).height();
			const auto  margin   = aPane->document()->documentMargin();
			const auto  top      = [height, margin](const int aLine) { return aLine == 0 ? 0.0 : margin + aLine * height; };
			const auto  wanted   = std::clamp(top(vertical->value()) + pane(aPane)->within_line_position() + delta, 0.0, top(vertical->maximum()));
			const auto  line     = wanted < top(1) ? 0 : static_cast<int>(std::floor((wanted - margin) / height));
			vertical->setValue(line);
			pane(aPane)->set_within_line_offset(wanted - top(line));
			update_unified_paint();
			if (FrameObserver) {
				FrameObserver();
			}
			return;
		}
		const auto* const axis  = AxisCache.axis();
		const auto        frame = Controller->current_frame();
		if (axis == nullptr || !frame) {
			return;
		}
		const auto side    = aPane == Left ? core::diff_side::left : core::diff_side::right;
		const auto current = measured_document_origin(*pane(aPane), *axis, side, side == core::diff_side::left ? frame->Left.DocumentOriginY : frame->Right.DocumentOriginY);
		// Reveal exactly to the displayed edge. Rounding to a top-line
		// boundary leaves slack below the selected row at most view heights.
		static_cast<void>(Controller->reveal_pane_origin(side, current + delta));
	}

	auto diff_workbench::exact_row_rect(QPlainTextEdit* const aPane, const std::uint32_t aRow) const -> QRect
	{
		// Qt places a block by walking from the first visible one and stops
		// about two viewport heights below it, or one above, answering the
		// block it stopped at. A row selected further away would then be
		// revealed short of itself, so its top is taken from the first visible
		// row's: in split view by the axis's line tops, the basis the reveal's
		// origin is measured in, and in unified view by its one row stride.
		// Its height is the row's own, as Qt reports it.
		auto* const source = pane(aPane);
		const auto  origin = source->visible_origin();
		const auto  block  = aPane->document()->findBlockByNumber(static_cast<int>(aRow));
		if (!origin || !block.isValid() || !block.isVisible()) {
			return source->line_row_rect(aRow);
		}
		const auto* const layout   = aPane->document()->documentLayout();
		auto              distance = std::optional<qreal>{ };
		if (aPane == Unified) {
			const auto stride = layout->blockBoundingRect(aPane->document()->firstBlock()).height();
			distance          = (static_cast<qreal>(aRow) - static_cast<qreal>(origin->first)) * stride;
		} else if (const auto* const axis = AxisCache.axis(); axis != nullptr) {
			const auto top  = [axis, left = aPane == Left](const std::size_t aLine) { return left ? axis->left_line_origin(aLine) : axis->right_line_origin(aLine); };
			const auto from = top(origin->first);
			const auto to   = top(aRow);
			if (from && to) {
				distance = *to - *from;
			}
		}
		if (!distance) {
			return source->line_row_rect(aRow);
		}
		// The first visible row's top: its content offset, which the pane
		// reports less the within-line offset it has applied.
		const auto first = origin->second + source->within_line_offset();
		return QRect{ 0, qRound(first + *distance), aPane->viewport()->width(),
					  std::max(1, qRound(layout->blockBoundingRect(block).height())) };
	}

	/// \brief [[BPR-1212]]. Says the selected line or lines, once a move.
	void diff_workbench::announce_line_selection()
	{
		const auto range = line_draft_range();
		if (!range) {
			return;
		}
		const auto side  = range->second.Side == core::diff_side::left ? QStringLiteral("earlier") : QStringLiteral("later");
		const auto first = static_cast<qulonglong>(std::min(range->first.Line, range->second.Line)) + 1U;
		const auto last  = static_cast<qulonglong>(std::max(range->first.Line, range->second.Line)) + 1U;
		announce_accessible_status(first == last && range->first.Side == range->second.Side
									   ? QStringLiteral("Line %1 of the %2 version selected").arg(first).arg(side)
									   : QStringLiteral("Lines %1 to %2 selected").arg(first).arg(last));
	}

	auto diff_workbench::selection() const noexcept
		-> const std::optional<core::semantic_selection>&
	{
		return Interaction.Selection;
	}

	auto diff_workbench::current_context() const noexcept
		-> const std::optional<core::diff_request_context>&
	{
		return CurrentContext;
	}

	auto diff_workbench::current_semantic_index() const noexcept
		-> const std::shared_ptr<const core::diff_semantic_index>&
	{
		return SemanticIndex;
	}

	auto diff_workbench::current_result() const noexcept
		-> const std::shared_ptr<const core::computed_file_diff>&
	{
		return CurrentResult;
	}

	auto diff_workbench::status_text() const -> QString
	{
		return Status->text();
	}

	auto diff_workbench::statistics() const noexcept
		-> diff_workbench_statistics
	{
		// Boundary rules are counted where they are drawn, not where they are
		// requested. The bands can be tallied at composition time because an
		// extra selection reaching the pane is the whole of their effect; a
		// rule is only real once the viewport has painted it, and counting the
		// intent would let a rule that never reaches the screen read as
		// painted.
		auto statistics                   = Statistics;
		statistics.GroupBoundariesPainted = pane(Left)->painted_rules()
			+ pane(Right)->painted_rules();
		statistics.WhiteSpacePainted = pane(Left)->painted_white_space()
			+ pane(Right)->painted_white_space() + pane(Unified)->painted_white_space();
		return statistics;
	}

	auto diff_workbench::event(QEvent* const aEvent) -> bool
	{
		const auto result = QWidget::event(aEvent);
		switch (aEvent->type()) {
		case QEvent::FontChange:
			// [[BPR-1336]]. Heard before the panes are given the same font, so
			// they still show the line the reader was looking at.
			note_reading_position();
			schedule_geometry_rebuild(
				measured_geometry_change::font,
				diff_geometry_rebuild_trigger::font_event);
			break;
		case QEvent::ApplicationFontChange:
			schedule_geometry_rebuild(
				measured_geometry_change::font,
				diff_geometry_rebuild_trigger::font_event);
			break;
		case QEvent::PaletteChange:
		case QEvent::ApplicationPaletteChange:
			update_selection_paint();
			break;
		case QEvent::StyleChange:
			update_selection_paint();
			schedule_geometry_rebuild(
				measured_geometry_change::viewport_or_divider,
				diff_geometry_rebuild_trigger::style_event);
			break;
		case QEvent::Show:
			schedule_geometry_rebuild(
				measured_geometry_change::screen,
				diff_geometry_rebuild_trigger::show_event);
			break;
		case QEvent::DevicePixelRatioChange:
			schedule_geometry_rebuild(
				measured_geometry_change::device_pixel_ratio,
				diff_geometry_rebuild_trigger::device_pixel_ratio_event);
			break;
		case QEvent::ScreenChangeInternal:
			schedule_geometry_rebuild(
				measured_geometry_change::screen,
				diff_geometry_rebuild_trigger::screen_change_event);
			break;
		default:
			if (aEvent->type() == code_typography_event::registered_type()) {
				adopt_code_typography(
					*static_cast<const code_typography_event*>(aEvent));
			}
			break;
		}
		return result;
	}

	void diff_workbench::adopt_code_typography(
		const code_typography_event& aTypography)
	{
		// A combined change already reached this surface as a font change, so
		// rebuilding again on every application would double the work and
		// attribute it to the wrong fact. Only a tab stop that actually moved
		// invalidates here, which is the case Qt reports no event for.
		const auto distance = aTypography.tab_stop_distance();
		if (AppliedTabStopDistance && qFuzzyCompare(*AppliedTabStopDistance, distance)) {
			return;
		}

		const auto known       = AppliedTabStopDistance.has_value();
		AppliedTabStopDistance = distance;
		if (!known) {
			return;
		}

		schedule_geometry_rebuild(
			measured_geometry_change::tab_width,
			diff_geometry_rebuild_trigger::tab_width);
	}

	auto diff_workbench::eventFilter(QObject* const aWatched, QEvent* const aEvent)
		-> bool
	{
		if (Find && aWatched == Find->Bar && (aEvent->type() == QEvent::LayoutRequest || aEvent->type() == QEvent::Show)) {
			// A taller multi-line query, or a font change, asks for a new size;
			// the owner keeps it anchored to its pane.
			QTimer::singleShot(0, this, [this] { place_source_find(); });
		}
		if (Find && !Find->Bar->isHidden() && (aWatched == Left || aWatched == Right || aWatched == Unified)) {
			const auto type = aEvent->type();
			if (type == QEvent::Resize || type == QEvent::Move || type == QEvent::Show || type == QEvent::Hide) {
				place_source_find();
			}
			// Option+C and Option+R stay Comment and Compose review in the pane;
			// only the find query claims them for its options.
		}
		if (CurrentResult && (aWatched == Left || aWatched == Right)
			&& (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut)) {
			RecentSourceContext = CurrentContext;
			RecentSourceSide    = aWatched == Left ? core::diff_side::left : core::diff_side::right;
		}
		if (Find && (aWatched == Left || aWatched == Right || aWatched == Unified) && aEvent->type() == QEvent::FocusIn) {
			Find->Recent = static_cast<QPlainTextEdit*>(aWatched);
			if (Find->Changed) {
				Find->Changed();
			}
		}
		if (Find && aEvent->type() == QEvent::MouseButtonPress) {
			if (aWatched == Left->viewport()) {
				Find->RealCaret[0] = true;
			}
			if (aWatched == Right->viewport()) {
				Find->RealCaret[1] = true;
			}
			if (aWatched == Unified->viewport()) {
				Find->RealCaret[2] = true;
			}
		}

		if (Find && Find->CopyMatch && (aWatched == Left || aWatched == Right || aWatched == Unified)
			&& (aEvent->type() == QEvent::ShortcutOverride || aEvent->type() == QEvent::KeyPress)) {
			auto* editor = static_cast<QPlainTextEdit*>(aWatched);
			auto* key    = static_cast<QKeyEvent*>(aEvent);
			if (key->matches(QKeySequence::Copy) && editor->textCursor() == Find->CopyCursor && CurrentResult) {
				aEvent->accept();
				if (aEvent->type() == QEvent::KeyPress) {
					const auto& match    = *Find->CopyMatch;
					const auto& document = match.Side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
					if (document && match.Span.End <= document->Bytes.size()) {
						QApplication::clipboard()->setText(QString::fromUtf8(reinterpret_cast<const char*>(document->Bytes.data() + match.Span.Begin), static_cast<qsizetype>(match.Span.End - match.Span.Begin)));
					}
				}
				return true;
			}
		}
		if ((aEvent->type() == QEvent::Wheel || aEvent->type() == QEvent::NativeGesture)
			&& (aWatched == Left->viewport() || aWatched == Right->viewport() || aWatched == Unified->viewport()
				|| aWatched == LinkMap || aWatched == UnifiedMap)
			&& code_zoom_gesture(*aEvent)) {
			return true;
		}
		if ((aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::ShortcutOverride)
			&& (aWatched == Left || aWatched == Right || aWatched == Unified)
			&& handle_line_key(static_cast<QPlainTextEdit*>(aWatched), aEvent)) {
			return true;
		}
		if (OnePaneEnabled && aWatched == UnifiedMap && aEvent->type() == QEvent::Wheel) {
			QApplication::sendEvent(Unified->viewport(), aEvent);
			return true;
		}
		// [[BPR-1328]]. In the one-pane comparison, as in the two panes, the
		// platform's document start and end go to the top and bottom.
		if (Unified != nullptr && aWatched == Unified && aEvent->type() == QEvent::KeyPress) {
			if (const auto edge = comparison_scroll_edge_for(*static_cast<QKeyEvent*>(aEvent)); edge.has_value()) {
				pane(Unified)->set_within_line_offset(0.0);
				Unified->verticalScrollBar()->triggerAction(*edge == comparison_scroll_edge::top ? QAbstractSlider::SliderToMinimum : QAbstractSlider::SliderToMaximum);
				aEvent->accept();
				return true;
			}
		}
		if (OnePaneEnabled && aWatched == Unified && aEvent->type() == QEvent::KeyPress) {
			const auto key = static_cast<QKeyEvent*>(aEvent)->key();
			if (key == Qt::Key_PageUp || key == Qt::Key_PageDown || key == Qt::Key_Home || key == Qt::Key_End) {
				pane(Unified)->set_within_line_offset(0.0);
			}
		}
		if (Unified != nullptr && aWatched == Unified->viewport()) {
			if (aEvent->type() == QEvent::Resize && OnePaneEnabled && !pane(Unified)->placing_origin()) {
				QTimer::singleShot(0, this, [this] {
					update_unified_paint();
					if (FrameObserver) {
						FrameObserver();
					}
				});
			}
			if (CurrentResult && aEvent->type() == QEvent::MouseButtonPress) {
				const auto* mouse = static_cast<QMouseEvent*>(aEvent);
				const auto  row   = Unified->cursorForPosition(mouse->position().toPoint()).blockNumber();
				if (mouse->button() == Qt::LeftButton && row >= 0 && static_cast<std::size_t>(row) < UnifiedPresentation->Rows.size()) {
					const auto& identity = UnifiedPresentation->Rows[static_cast<std::size_t>(row)];
					static_cast<void>(select_line(identity.Head ? core::diff_side::right : core::diff_side::left,
												  identity.Head.value_or(identity.Base.value_or(0U)), true));
				}
			}
		}
		const auto sourceViewport = aWatched == Left->viewport()
			|| aWatched == Right->viewport();
		if (sourceViewport && aEvent->type() == QEvent::Resize
			&& !pane(aWatched == Left->viewport() ? Left : Right)->placing_origin()
			&& pane(aWatched == Left->viewport() ? Left : Right)->changes_viewport_extent(static_cast<QResizeEvent*>(aEvent)->size())) {
			schedule_geometry_rebuild(
				measured_geometry_change::viewport_or_divider,
				diff_geometry_rebuild_trigger::pane_viewport_resize);
		}
		if (
			CurrentResult && aEvent->type() == QEvent::MouseButtonPress
			&& sourceViewport) {
			const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
			if (mouse->button() == Qt::LeftButton) {
				auto* const source = aWatched == Left->viewport() ? Left : Right;
				const auto  cursor = source->cursorForPosition(mouse->position().toPoint());
				if (cursor.blockNumber() >= 0) {
					static_cast<void>(select_line(
						aWatched == Left->viewport()
							? core::diff_side::left
							: core::diff_side::right,
						static_cast<std::uint32_t>(cursor.blockNumber()),
						true));
				}
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void diff_workbench::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		place_source_find();
		// Before the rebuild, because a resize can add or remove a pane's
		// horizontal scrollbar and that changes the band without changing
		// anything the axis knows about.
		declare_link_map_band();
		schedule_geometry_rebuild(
			measured_geometry_change::viewport_or_divider,
			diff_geometry_rebuild_trigger::workbench_resize);
	}

	void diff_workbench::queue_publication_continuation()
	{
		if (PublicationScheduled) {
			return;
		}
		PublicationScheduled = true;
		QPointer<diff_workbench> self{ this };
		QTimer::singleShot(0, this, [self] {
			if (self != nullptr) {
				self->PublicationScheduled = false;
				self->continue_publication();
			}
		});
	}

	void diff_workbench::continue_publication()
	{
		const auto staged = PendingPublication;
		if (!staged) {
			return;
		}
		if (!CurrentContext.has_value() || *CurrentContext != staged->Publication.Context
			|| staged->Left.Document == nullptr || staged->Right.Document == nullptr) {
			drop_staged_publication();
			return;
		}
		auto timer = QElapsedTimer{ };
		timer.start();
		while (!staged->Left.complete() || !staged->Right.complete()) {
			(staged->Left.complete() ? staged->Right : staged->Left).append(staged_publication_batch_lines);
			// One batch is indivisible; check the time between batches.
			if (timer.nsecsElapsed() >= staged_publication_turn_ns) {
				break;
			}
		}
		++Statistics.PublicationFillBatches;
		if (!staged->Left.complete() || !staged->Right.complete()) {
			queue_publication_continuation();
			return;
		}
		PendingPublication.reset();
		adopt_publication(staged->Publication, staged->Left.Document.data(), staged->Right.Document.data());
	}

	void diff_workbench::drop_staged_publication()
	{
		if (!PendingPublication) {
			return;
		}
		++Statistics.SupersededStagedPublications;
		PendingPublication->Left.discard();
		PendingPublication->Right.discard();
		PendingPublication.reset();
	}

	void diff_workbench::schedule_geometry_rebuild(
		const measured_geometry_change      aChange,
		const diff_geometry_rebuild_trigger aTrigger)
	{
		if (!CurrentResult) {
			return;
		}
		if (aChange != measured_geometry_change::none) {
			AxisCache.invalidate(aChange);
		}
		PendingGeometryTriggers = PendingGeometryTriggers | aTrigger;
		if (PendingMeasurement) {
			++Statistics.MeasurementRestarts;
			PendingMeasurement.reset();
		}
		Controller->suspend_axis();
		LinkMap->clear_geometry();
		queue_geometry_continuation();
	}

	auto diff_workbench::geometry_pending() const noexcept -> bool
	{
		return PendingPublication || (CurrentResult && (GeometryScheduled || PendingMeasurement));
	}

	void diff_workbench::queue_geometry_continuation()
	{
		if (GeometryScheduled) {
			return;
		}
		GeometryScheduled = true;
		QPointer<diff_workbench> self{ this };
		QTimer::singleShot(0, this, [self] {
			if (self != nullptr) {
				self->GeometryScheduled = false;
				self->rebuild_geometry();
			}
		});
	}

	void diff_workbench::rebuild_geometry()
	{
		if (!CurrentResult) {
			PendingGeometryTriggers = diff_geometry_rebuild_trigger::none;
			return;
		}
		if (!PendingMeasurement) {
			// [[BPR-1516]]. Document boundaries do not depend on the viewport
			// origin. Keep its fractional placement through every measurement
			// turn; clearing it here shows a whole-row frame on each divider move
			// before the shared controller can apply the rebuilt axis.
			PendingMeasurement = std::make_shared<geometry_measurement>(geometry_measurement{
				.Result = CurrentResult,
				.Left   = plain_text_pane_measurement{ *Left, CurrentResult->Alignment.LeftLineCount, LeftHeights },
				.Right  = plain_text_pane_measurement{ *Right, CurrentResult->Alignment.RightLineCount, RightHeights, *Left, LeftHeights },
			});
			Statistics.PaneMeasurements += 2U;
		}
		// A shared local keeps state alive if native layout synchronously sends
		// a resize event which cancels the member. Identity is the generation.
		const auto measurement = PendingMeasurement;
		const auto current     = [&] {
			return PendingMeasurement == measurement && CurrentResult == measurement->Result;
		};
		auto timer = QElapsedTimer{ };
		timer.start();
		auto           lines        = std::uint32_t{ 0U };
		auto           left         = measurement->Left.advance(0U);
		auto           right        = measurement->Right.advance(0U);
		constexpr auto groupLines   = 16U;
		constexpr auto maximumLines = 512U;
		while (current() && lines < maximumLines
			   && left != pane_measurement_progress::invalidated
			   && right != pane_measurement_progress::invalidated
			   && (left == pane_measurement_progress::pending || right == pane_measurement_progress::pending)) {
			auto&      paneMeasurement = left == pane_measurement_progress::pending
				? measurement->Left
				: measurement->Right;
			const auto before          = paneMeasurement.measured_lines();
			const auto progress        = paneMeasurement.advance(std::min(groupLines, maximumLines - lines));
			lines += paneMeasurement.measured_lines() - before;
			if (left == pane_measurement_progress::pending) {
				left = progress;
			} else {
				right = progress;
			}
			// An individual native block is indivisible; check time between groups.
			if (timer.nsecsElapsed() >= 4'000'000) {
				break;
			}
		}
		++Statistics.MeasurementBatches;
		Statistics.MaximumMeasurementBatchLines = std::max(Statistics.MaximumMeasurementBatchLines, lines);
		if (!current()) {
			return;
		}
		if (left == pane_measurement_progress::invalidated || right == pane_measurement_progress::invalidated) {
			// Observable layout changes can occur without a workbench event.
			// A state that cannot even start is an actual admission failure.
			if (measurement->Left.measured_lines() + measurement->Right.measured_lines() > 0U) {
				schedule_geometry_rebuild(measured_geometry_change::viewport_or_divider,
										  diff_geometry_rebuild_trigger::pane_viewport_resize);
			} else {
				++Statistics.GeometryFailures;
				PendingMeasurement.reset();
				Controller->clear_axis();
			}
			return;
		}
		if (left == pane_measurement_progress::pending || right == pane_measurement_progress::pending) {
			queue_geometry_continuation();
			return;
		}
		const auto* leftResult  = measurement->Left.result();
		const auto* rightResult = measurement->Right.result();
		if (leftResult == nullptr || rightResult == nullptr) {
			schedule_geometry_rebuild(measured_geometry_change::viewport_or_divider,
									  diff_geometry_rebuild_trigger::pane_viewport_resize);
			return;
		}
		Statistics.MeasurementLinesReused += measurement->Left.reused_lines() + measurement->Right.reused_lines();
		auto leftMeasurements            = *leftResult;
		auto rightMeasurements           = *rightResult;
		leftMeasurements.ViewportExtent  = pane(Left)->viewport_extent();
		rightMeasurements.ViewportExtent = pane(Right)->viewport_extent();
		const auto changes               = AxisCache.pending_changes();
		const auto triggers              = PendingGeometryTriggers;
		if (AxisCache.rebuild(CurrentResult->Alignment, leftMeasurements, rightMeasurements)
				!= measured_alignment_error::none
			|| AxisCache.axis() == nullptr) {
			++Statistics.GeometryFailures;
			PendingMeasurement.reset();
			Controller->clear_axis();
			return;
		}
		const auto axis            = *AxisCache.axis();
		const auto revealSelection = GeometryRevealPending || ConversationAnchor.has_value()
			|| !Controller->current_frame().has_value()
			|| Controller->current_frame()->Comparison != axis.comparison();
		declare_link_map_band();
		if (!current()) {
			return;
		}
		PendingMeasurement.reset();
		PendingGeometryTriggers = diff_geometry_rebuild_trigger::none;
		GeometryRevealPending   = false;
		LinkMap->set_geometry(axis, CurrentResult->Alignment.Moves);
		Controller->set_axis(axis);
		refresh_thread_ticks();
		refresh_find_ticks();
		if (Find && Find->Changed) {
			Find->Changed();
		}
		if (Find && Find->Rebind && !Find->Bar->isHidden()) {
			QTimer::singleShot(0, this, [this] {if(Find->Rebind && !Find->Bar->isHidden() && source_find_available()){open_source_find();
} });
		}
		if (Find && Find->PendingReveal) {
			const auto target = *Find->PendingReveal;
			QTimer::singleShot(0, this, [this, target] {if(Find->PendingReveal==std::optional{target} && reveal_source_match(target)){Find->PendingReveal.reset();
} });
		}
		++Statistics.GeometryRebuilds;
		if (Statistics.GeometryRebuildHistorySize
			< Statistics.GeometryRebuildHistory.size()) {
			Statistics.GeometryRebuildHistory[Statistics.GeometryRebuildHistorySize++] = {
				.Changes  = changes,
				.Triggers = triggers,
			};
		} else {
			Statistics.GeometryRebuildHistoryOverflow = true;
		}
		synchronize_geometry_selection(revealSelection);
		// [[BPR-1336]]. The font change already put the reader's line back
		// where it was, by whole lines of the new height; the remeasured axis
		// now lets the controller hold it there and bring the other pane along.
		if (HeldLine && !OnePaneEnabled) {
			const auto origin = HeldLine->Side == core::diff_side::left ? axis.left_line_origin(HeldLine->Top) : axis.right_line_origin(HeldLine->Top);
			if (origin && Controller->reveal_pane_origin(HeldLine->Side, HeldLine->Top == 0U ? 0.0 : *origin)) {
				++Statistics.ReadingPositionsHeld;
			}
		}
		HeldLine.reset();
	}

	void diff_workbench::apply_frame(const comparison_scroll_frame& aFrame)
	{
		if (
			ApplyingFrame || !CurrentResult
			|| aFrame.Comparison != CurrentResult->Alignment.Digest) {
			return;
		}
		ApplyingFrame          = true;
		const auto* const axis = AxisCache.axis();
		if (axis != nullptr && axis->comparison() == aFrame.Comparison) {
			auto rendered = aFrame;
			// The containing row and its remaining pixels share one measured
			// origin with the ribbons; the actual native viewport moves with it.
			const auto place = [axis](source_pane& aPane, const core::diff_side aSide, const qreal aOrigin) {
				auto       line   = axis->top_line_for_origin(aSide, aOrigin);
				const auto origin = [axis, aSide](const std::size_t aLine) {
					return aLine == 0U ? 0.0 : (aSide == core::diff_side::left ? axis->left_line_origin(aLine) : axis->right_line_origin(aLine)).value_or(0.0);
				};
				// Projection can land a floating-point epsilon below an exact
				// boundary, particularly at End. Keep that boundary exact.
				if (line > 0U && origin(line) - aOrigin > 0.000'001) {
					--line;
				}
				aPane.place_top_line_and_offset(line, aOrigin - origin(line));
				return origin(line) + aPane.within_line_offset();
			};
			const auto leftEstimate  = place(*pane(Left), core::diff_side::left, aFrame.Left.DocumentOriginY);
			const auto rightEstimate = place(*pane(Right), core::diff_side::right, aFrame.Right.DocumentOriginY);
			if (!OnePaneEnabled && PresentationReadingLine) {
				// A presentation flip carries an exact top row. These panes do
				// not wrap, so their native bar counts those rows directly; using
				// the proportional document estimate can round to the next one.
				// The readback below keeps the link map on the resulting origin.
				auto* const source = PresentationReadingLine->Side == core::diff_side::left ? Left : Right;
				source->verticalScrollBar()->setValue(static_cast<int>(PresentationReadingLine->Line));
			}
			rendered.Left.DocumentOriginY = measured_document_origin(
				*pane(Left), *axis, core::diff_side::left, leftEstimate);
			rendered.Right.DocumentOriginY = measured_document_origin(
				*pane(Right), *axis, core::diff_side::right, rightEstimate);
			rendered.Left.ViewportY = rendered.Left.ProjectedDocumentY
				- rendered.Left.DocumentOriginY;
			rendered.Right.ViewportY = rendered.Right.ProjectedDocumentY
				- rendered.Right.DocumentOriginY;
			static_cast<void>(LinkMap->set_frame(rendered));
		} else {
			static_cast<void>(LinkMap->set_frame(aFrame));
		}
		ApplyingFrame = false;
		// Painting is bounded to what the panes can show, so a scroll that
		// reveals new lines must repaint them. Without this the bound would be
		// a rendering bug rather than an optimisation: the reviewer would
		// scroll into a changed region and find it untinted.
		update_diff_paint();
		sync_head_edit();
		++Statistics.FrameApplications;
		if (FrameObserver) {
			FrameObserver();
		}
	}

	auto diff_workbench::select_conversation_line(const core::diff_side aSide, const std::uint32_t aLine) -> bool
	{
		return select_line(aSide, aLine, true);
	}

	auto diff_workbench::restore_selection(const core::semantic_selection& aSelection) -> bool
	{
		if (!CurrentResult || !SemanticIndex) {
			return false;
		}
		const auto resolved = aSelection.Kind == core::semantic_selection_kind::group && aSelection.Group
			? core::select_group(*SemanticIndex, *aSelection.Group)
			: core::select_line(*SemanticIndex, aSelection.PreferredLine);
		if (resolved != aSelection) {
			return false;
		}
		clear_conversation_anchor();
		const auto changed  = aSelection.Kind == core::semantic_selection_kind::group
			? core::transition(Interaction, *SemanticIndex, core::select_diff_group_intent{ *aSelection.Group })
			: core::transition(Interaction, *SemanticIndex, core::select_diff_line_intent{ aSelection.PreferredLine });
		RestoredReadingLine = aSelection.PreferredLine;
		apply_interaction(true, changed);
		if (changed) {
			emit_selection();
		}
		return Interaction.Selection == aSelection;
	}

	auto diff_workbench::select_line(
		const core::diff_side aSide,
		const std::uint32_t   aLine,
		const bool            aReportUnchangedInteraction) -> bool
	{
		if (!CurrentResult || !SemanticIndex) {
			clear_conversation_anchor();
			return false;
		}
		const auto requested = core::source_line_ref{
			.Comparison = CurrentResult->Alignment.Digest,
			.Side       = aSide,
			.Line       = aLine,
		};
		if (ConversationAnchor != requested) {
			clear_conversation_anchor();
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::select_diff_line_intent{
				.Line = requested,
			});
		if (changed) {
			++Statistics.SelectionChanges;
			apply_interaction(false, true);
			emit_selection(true);
		}
		const auto selected = Interaction.Selection.has_value()
			&& Interaction.Selection->PreferredLine == requested;
		if (selected) {
			PresentationReadingSide = aSide;
		}
		if (!changed && selected && aReportUnchangedInteraction) {
			// Choosing the displayed line also supersedes deferred restoration.
			const auto repeated = Interaction.Selection;
			emit_reading_gesture();
			emit_selection_observer(true);
			if (repeated && repeated == Interaction.Selection && LineReselectionSink) {
				try {
					LineReselectionSink(*repeated);
				} catch (...) { }
			}
		}
		return selected;
	}

	void diff_workbench::select_link(
		const diff_link_map_identity& aIdentity,
		const bool                    aReveal)
	{
		clear_conversation_anchor();
		if (!CurrentResult || !SemanticIndex
			|| !aIdentity.Group.has_value()) {
			return;
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::select_diff_group_intent{ .Group = *aIdentity.Group });
		if (changed) {
			++Statistics.SelectionChanges;
		}
		apply_interaction(aReveal, changed);
		if (changed) {
			emit_selection(true);
		} else if (Interaction.Selection && Interaction.Selection->Group == aIdentity.Group) {
			emit_reading_gesture();
		}
	}

	auto diff_workbench::navigate_change(
		const core::change_navigation_direction aDirection,
		const core::change_navigation_policy    aPolicy)
		-> bool
	{
		clear_conversation_anchor();
		if (!CurrentResult || !SemanticIndex) {
			return false;
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::navigate_diff_change_intent{
				.Direction = aDirection,
				.Policy    = aPolicy,
			});
		if (changed) {
			++Statistics.SelectionChanges;
			apply_interaction(true, changed);
			emit_selection(true);
		}
		return changed;
	}

	auto diff_workbench::navigate_outermost_change(const core::change_navigation_direction aDirection) -> bool
	{
		clear_conversation_anchor();
		if (!CurrentResult || !SemanticIndex) {
			return false;
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::navigate_diff_change_intent{
				.Direction = aDirection,
				.Reach     = core::change_navigation_reach::outermost,
			});
		// Unchanged with a selected group is the end itself: the transition
		// selects it from anywhere else, and a file without changes leaves no
		// group selected.
		const auto reached = Interaction.Selection.has_value() && Interaction.Selection->Group.has_value();
		if (changed) {
			++Statistics.SelectionChanges;
		}
		if (changed || reached) {
			apply_interaction(true, changed);
		}
		if (changed) {
			emit_selection(true);
		} else if (reached) {
			emit_reading_gesture();
		}
		return changed;
	}
}
