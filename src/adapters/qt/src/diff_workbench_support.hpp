#ifndef BPR_APP_DIFF_WORKBENCH_SUPPORT_HPP
#define BPR_APP_DIFF_WORKBENCH_SUPPORT_HPP
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/suggestion_editor.hpp"

#include <QAbstractButton>
#include <QColor>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QFlags>
#include <QFocusEvent>
#include <QFont>
#include <QList>
#include <QMargins>
#include <QMetaObject>
#include <QMouseEvent>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QShowEvent>
#include <QSize>
#include <QString>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QWheelEvent>
#include <QWidget>

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

// The source pane, thread tick scroll bar and helpers the diff workbench's sources share.
namespace bpr::qt {
	/// Width of the change-group boundary rule, in device-independent pixels.
	constexpr auto BoundaryRuleWidth = 2.0;

	/// [[BPR-1335]]. Breathing room either side of the widest number the
	/// gutter shows. The owner found the gutter wider than its numbers.
	constexpr auto GutterPadding = 4;

	/// [[BPR-1335]]. The "+" button's side: GitHub's 20 px, shrunk to fit a
	/// short row but never below 12 px.
	constexpr auto AddButtonSide = 20;

	constexpr auto AddButtonMinSide = 12;

	/// \brief [[BPR-1290]]. How far past a tick's top or bottom edge the
	/// pointer still finds it, in pixels: close ticks are hard to hit.
	constexpr auto TickReach = 4;

	/// \brief [[BPR-1290]]. The code's scroll bar with a tick per thread:
	/// amber for the open one, blue for a new one, outlined for my draft.
	///
	/// \details A plain press is the scroll bar's own: it pages or drags.
	/// Shift and a press on a tick go to its thread (the owner, rejecting
	/// the built drawer v001: "I want to shift click on a comment marker on
	/// the scrollbar to scroll to it."). Where ticks crowd, the pointer
	/// finds the tick whose centre is nearest, within `TickReach` of its
	/// edge. A tick names where Shift-click goes in its tooltip, and while
	/// Shift is held over one the pointer is a hand and that tick is ringed,
	/// so the tick a press would choose is the one shown.
	class thread_tick_scroll_bar final : public QScrollBar
	{
	public:
		thread_tick_scroll_bar(const Qt::Orientation aOrientation, QWidget* const aParent);

		~thread_tick_scroll_bar() override;

		thread_tick_scroll_bar(const thread_tick_scroll_bar&)                    = delete;
		auto operator=(const thread_tick_scroll_bar&) -> thread_tick_scroll_bar& = delete;
		thread_tick_scroll_bar(thread_tick_scroll_bar&&)                         = delete;
		auto operator=(thread_tick_scroll_bar&&) -> thread_tick_scroll_bar&      = delete;

		std::function<void(core::diff_side, std::uint32_t)> GoTo;
		QColor                                              Thread{ 0xD2, 0x99, 0x22 };
		QColor                                              New{ 0x09, 0x69, 0xDA };

		void set_find_ticks(std::vector<qreal> aFractions);
		void set_ticks(std::vector<diff_thread_tick> aTicks);

		[[nodiscard]] auto ticks() const noexcept -> const std::vector<diff_thread_tick>&;

		/// \brief A tick's rectangle in the bar's coordinates.
		[[nodiscard]] auto tick_rect(const diff_thread_tick& aTick) const -> QRect;

		/// \brief What a tick's tooltip says, anchored to the band across
		/// the bar in which the pointer finds it, so the tooltip stays
		/// while the pointer is anywhere a press would choose that tick.
		[[nodiscard]] auto tooltip_of(const diff_thread_tick& aTick) const -> anchored_tooltip_target;

		/// \brief The tick a point finds: of those within `TickReach` of
		/// the point, the one whose centre is nearest; the first on a tie.
		[[nodiscard]] auto tick_at(const QPoint aPosition) const -> std::optional<std::size_t>;

		/// \brief The tick Shift and a press would go to now, if any.
		[[nodiscard]] auto targeted() const -> std::optional<diff_thread_tick>;

	protected:
		void paintEvent(QPaintEvent* const aEvent) override;

		void mousePressEvent(QMouseEvent* const aEvent) override;

		void mouseMoveEvent(QMouseEvent* const aEvent) override;

		void enterEvent(QEnterEvent* const aEvent) override;

		void leaveEvent(QEvent* const aEvent) override;

		/// Shift pressed or let go while the pointer rests on the bar.
		auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override;

	private:
		std::vector<qreal>                                       FindFractions;
		std::vector<diff_thread_tick>                            Ticks;
		std::optional<QPoint>                                    Pointer;
		std::optional<std::pair<core::diff_side, std::uint32_t>> Target;
		bool                                                     ShiftHeld = false;
		bool                                                     Watching  = false;

		/// The key events go to the focus, not to the bar: while the
		/// pointer is over it, the application's are watched for Shift.
		void watch_shift(const bool aWatch);

		/// Rings the tick Shift-click would choose, shows the hand and
		/// names where it goes at once: with Shift held the tooltip does
		/// not wait for the hover delay (the owner, rejecting the built
		/// drawer v002: "Tooltip can show up immediately with the shift
		/// mouseover."). Without Shift a tick's tooltip keeps its delay.
		void follow_pointer();
	};

	struct unified_source_row
	{
		std::optional<std::uint32_t> Base;
		std::optional<std::uint32_t> Head;
	};

	/// \brief [[BPR-1336]]. The zoom shown for a moment over the comparison.
	///
	/// \details A pill at the top centre of the panes: the percentage in the
	/// interface font's semibold weight with tabular figures, so stepping
	/// through the ladder does not make it twitch, then the size on screen
	/// in the regular weight. It takes the window's own surface
	/// and text roles, so every theme draws it in its own colours. It never
	/// takes the pointer or the keyboard, and it appears and leaves without
	/// animation: the code changing size is the motion.
	class zoom_pill final : public QWidget
	{
	public:
		explicit zoom_pill(QWidget* const aParent);

		void set_zoom(const QString& aPercent, const QString& aSize);

		[[nodiscard]] auto percent_text() const -> const QString&;
		[[nodiscard]] auto size_text() const -> const QString&;

	protected:
		void paintEvent(QPaintEvent*) override;

	private:
		static constexpr qreal inset_x = 14.0;
		static constexpr qreal inset_y = 6.0;
		static constexpr qreal gap     = 8.0;

		/// The interface font, not the comparison's code font this widget
		/// would otherwise inherit: the pill must not itself grow with the
		/// zoom it reports.
		[[nodiscard]] static auto pill_fonts() -> std::pair<QFont, QFont>;

		QString Percent;
		QString Size;
	};

	/// \brief Reserves the gutter each pane numbers its own lines in.
	///
	/// \details `code_gutter_background` and `code_line_number_text` have been
	/// declared roles in every built-in palette since RFC-BPR-0006, and
	/// `appearance_contrast` has been validating that pair's readability in
	/// every profile, against text nothing drew. BPR-0455 draws it, after the
	/// BPR-0414 owner review could not tell which pane row was line 42.
	class source_pane final : public QPlainTextEdit
	{
	public:
		explicit source_pane(QWidget* aParent = nullptr);
		/// [[BPR-1798]]. A ring around one Find match: every match has one,
		/// in a colour that reaches the indicator ratio on the ground the
		/// match lands on, and the current match's is heavier.
		struct find_ring
		{
			QTextCursor Cursor;
			QColor      Colour;
			bool        Current = false;
		};
		std::vector<find_ring> FindRings;
		void                   set_unified_rows(const std::vector<unified_source_row>* aRows);
		void                   set_row_colors(const QColor& aRemoved, const QColor& aAdded);
		void                   set_unified_selection(std::optional<core::line_span> aBase, std::optional<core::line_span> aHead);
		/// \brief [[BPR-1107]]. The lines this side's selection washes.
		void set_selected_lines(std::optional<core::line_span> aLines);
		/// \brief [[BPR-1107]]. When this pane draws white space.
		void set_white_space(const core::white_space_drawing aDrawing);

		/// \brief The gutter's width, as `refresh_gutter_width` last measured it.
		[[nodiscard]] auto gutter_width() const noexcept -> int;
		/// \brief [[BPR-1335]]. Sizes the gutter to the longest number it shows.
		///
		/// \details The longest number is the pane's last block, so the
		/// gutter is sized from the document and not from the visible rows,
		/// and scrolling never moves the code sideways. A file that ends in a
		/// newline has an empty last block, numbered as VS Code numbers it.
		/// The width is the digits times the font's widest digit, a pad
		/// either side, and the marker column when it is on. Runs when the
		/// document, the font, the scale, the unified rows, the marker
		/// column or the numbers' visibility change, and touches the layout
		/// only when the width or the heading row actually changed.
		void refresh_gutter_width();
		void set_line_numbers_visible(bool aVisible);
		void set_gutter_colors(const QColor& aBackground, const QColor& aText);
		void paint_line_numbers(QPaintEvent* aEvent);
		void paint_markers(QPainter& aPainter);
		/// \brief [[BPR-1335]]. Tints a thread's line-number cell, under its
		/// number: amber, blue when new, lighter when resolved; my draft is
		/// outlined, dashed.
		void               paint_thread_tint(QPainter& aPainter, std::uint32_t aLine, const QRectF& aCell);
		[[nodiscard]] auto painted_line_numbers() const noexcept -> std::uint64_t;

		/// \brief Adopts the lines each change group starts on.
		///
		/// \details Held sorted so a repaint can find the visible ones by
		/// bisection. Computed once when a result is published rather than
		/// per paint, because the group count follows the size of the diff
		/// and only the search may be allowed to follow the viewport.
		void set_group_boundaries(std::vector<std::uint32_t> aBoundaries);

		void set_boundary_rule_color(const QColor& aColor);

		void set_conversation_anchor(std::optional<std::uint32_t> aLine);

		/// \brief One line's row in viewport coordinates, or empty.
		[[nodiscard]] auto line_row_rect(const std::uint32_t aLine) const -> QRect;

		/// \brief [[BPR-1212]] slice 1. The rows a mark spans: its line, or
		/// all the lines of a draft or thread on several.
		[[nodiscard]] auto mark_span_rect(const diff_line_mark& aMark) const -> QRect;

		[[nodiscard]] auto conversation_anchor_rect() const -> QRect;

		/// \brief [[BPR-0648]]. The lines carrying a thread, painted at the edge.
		void set_line_marks(std::vector<diff_line_mark> aMarks);

		/// \brief [[BPR-1290]], [[BPR-1335]]. Whether the gutter shows the
		/// threads: a thread's number cell is tinted, and the head pane offers
		/// its "+" button. The gutter stays its numbers' width.
		void set_marker_column(const bool aEnabled);

		/// \brief Whether the gutter shows the threads at all.
		[[nodiscard]] auto markers_shown() const noexcept -> bool;

		/// \brief [[BPR-1335]]. The thread on a line, if any, by bisection.
		[[nodiscard]] auto mark_on(const std::uint32_t aLine) const -> const diff_line_mark*;

		/// \brief How far the viewport sits below the gutter's top: a
		/// unified pane's heading row, nothing in a split pane.
		[[nodiscard]] auto gutter_header() const -> int;

		/// \brief [[BPR-1335]]. A line's number cell, in the gutter's coordinates.
		[[nodiscard]] auto number_cell(const std::uint32_t aLine) const -> QRect;

		void set_mark_dispatch(std::function<void(const diff_line_mark&)> aDispatch);

		/// \brief [[BPR-1212]], [[BPR-1335]]. The "+" button on a head line
		/// without a thread, shown while the pointer is in its column; none
		/// without a dispatch. It is told the first and last lines.
		void set_add_dispatch(std::function<void(std::uint32_t, std::uint32_t)> aDispatch);

		/// \brief [[BPR-1724]]. Told when a line's number is clicked
		/// (`false`), or Shift-clicked or dragged onto (`true`), with its
		/// zero-based row: GitHub's way of selecting lines by their numbers.
		void set_number_dispatch(std::function<void(std::uint32_t, bool)> aDispatch);

		/// \brief [[BPR-1724]]. The row whose number a point of the
		/// gutter is on, or none: the heading row and below the last line
		/// are no row's.
		[[nodiscard]] auto number_line_at(const QPoint aGutterPoint) const -> std::optional<std::uint32_t>;

		/// \brief [[BPR-1724]]. A press on a line's number selects it,
		/// or with Shift extends the selection to it, and starts a drag
		/// over the numbers. The code takes the keyboard, so Return and
		/// Shift+arrows go on from there. Returns whether it was taken.
		auto press_number(const std::uint32_t aLine, const bool aExtend) -> bool;
		/// \brief [[BPR-1724]]. A drag over the numbers extends the
		/// selection to the line under the pointer.
		void               extend_number_drag(const int aGutterY);
		void               finish_number_drag();
		[[nodiscard]] auto add_dragging() const noexcept -> bool;

		/// \brief Shows the "+" button on a line, or hides it. A drag in
		/// progress keeps its button where it started.
		void set_add_hover(std::optional<std::uint32_t> aLine);

		/// \brief The line a viewport y offers the "+" on: a line of a split
		/// pane with no thread on it, while the pointer is on its row.
		[[nodiscard]] auto add_line_at(const int aViewportY) const -> std::optional<std::uint32_t>;

		/// \brief [[BPR-1335]] v002. Where a line's "+" is drawn, in the
		/// pane's coordinates: at the start of its code, across the
		/// boundary with its number. Empty while the line is not laid out.
		[[nodiscard]] auto add_button_rect(const std::uint32_t aLine) const -> QRect;

		/// \brief [[BPR-1335]] v002. The line whose "+" a point of the pane
		/// is over: the line's own "+" column, the strip across the number
		/// and code boundary that the button is drawn in. Elsewhere on the
		/// line, on its code or its digits, nothing is drawn. The owner, on
		/// v001: "i find it somewhat distracting always being drawn. Maybe we
		/// only draw it when the cursor is over the column where it would
		/// go?"
		[[nodiscard]] auto add_line_under(const QPoint aPanePoint) const -> std::optional<std::uint32_t>;

		/// \brief The line nearest a viewport y, clamped to the document.
		[[nodiscard]] auto line_at(const int aViewportY) const -> std::optional<std::uint32_t>;

		/// \brief The "+" button's place in the pane, or empty while hidden.
		[[nodiscard]] auto add_rect() const -> QRect;

		/// \brief [[BPR-1335]]. The pointer left the code, the gutter or the
		/// button. The button stays while the pointer is on it.
		void leave_add_hover();

		/// \brief [[BPR-1335]]. A press on the "+" starts a range on its line;
		/// moving extends it to the line under the pointer, washed as a
		/// draft being written is; release adds the comment on it.
		void               begin_add_drag(const std::uint32_t aLine);
		void               extend_add_drag(const QPoint aGlobal);
		void               finish_add_drag();
		[[nodiscard]] auto add_drag_rect() const -> QRect;

		void press_add(const std::uint32_t aFirst, const std::uint32_t aLast);

		/// \brief What a pointer presses to reach a mark's thread: its
		/// tinted number cell, in the gutter's coordinates.
		[[nodiscard]] auto marker_rect(const diff_line_mark& aMark) const -> QRect;

		/// \brief The mark whose tinted cell is under a point of the gutter.
		/// Cells are exact, so neighbouring lines' cells never overlap.
		[[nodiscard]] auto mark_at(const QPoint aGutterPoint) const -> const diff_line_mark*;

		void press_mark(const diff_line_mark& aMark);

		[[nodiscard]] auto thread_colour() const noexcept -> const QColor&;

		[[nodiscard]] auto new_colour() const noexcept -> const QColor&;

		[[nodiscard]] auto line_marks() const noexcept -> const std::vector<diff_line_mark>&;

		void set_mark_colors(const QColor& aThread, const QColor& aNewThread);

		[[nodiscard]] auto painted_rules() const noexcept -> std::uint64_t;

		[[nodiscard]] auto painted_white_space() const noexcept -> std::uint64_t;

		/// \brief The first block the viewport can show, and where it sits.
		///
		/// \details `firstVisibleBlock` and `contentOffset` are protected
		/// on `QPlainTextEdit`, so this is the only place they can be read.
		/// Together they say which line is at the top and how far above the
		/// viewport edge it starts, which is everything needed to turn a
		/// per-line document measurement into a scroll origin.
		/// \brief [[BPR-1734]]. Repaints the rows the last and the
		/// current text selection cover, across the whole viewport.
		/// \details With the style macOS uses, a selection that runs past a
		/// line's end is painted to the pane's edge (`paintEvent`), but Qt
		/// asks only for the document's width when the selection changes or
		/// turns the inactive colour, so the part past the longest line kept
		/// its old fill. Only the selected rows are repainted, so the work
		/// stays bounded by the viewport.
		void repaint_full_width_selection();

		/// \brief [[RFC-BPR-0090]]. The first line any of whose row shows.
		[[nodiscard]] auto top_visible_line() const -> std::optional<std::uint32_t>;

		/// \brief [[RFC-BPR-0090]]. A line's row top in viewport coordinates,
		/// unrounded, so text drawn over the pane meets its rows exactly.
		[[nodiscard]] auto exact_line_top(const std::uint32_t aLine) const -> std::optional<qreal>;

		[[nodiscard]] auto visible_origin() const
			-> std::optional<std::pair<std::size_t, qreal>>;

		/// \brief Scrolls so a line is at the top.
		///
		/// \details The pane does not wrap, so its native bar counts
		/// lines. [[BPR-1291]]: the pane scrolls on centre, which lets its
		/// bar reach the last line, as scroll-past-end needs.
		void set_top_line(const std::size_t aLine);

		/// [[BPR-1527]]. A shared frame supplies both parts of its origin.
		/// Keep the residual through the native top-line change instead of
		/// clearing it and resizing before its replacement arrives.
		void place_top_line_and_offset(const std::size_t aLine, const qreal aOffset);

		/// \brief Places the native viewport, including native selection and hits.
		/// \details The scrollbar still chooses the first block. The remaining
		/// pixels move the viewport widget itself, clipped to its original
		/// content area. No painter-only transform or second text layout is used.
		void               set_within_line_offset(const qreal aOffset);
		[[nodiscard]] auto within_line_offset() const noexcept -> int;
		[[nodiscard]] auto placing_origin() const noexcept -> bool;
		[[nodiscard]] auto changes_viewport_extent(const QSize aSize) const noexcept -> bool;
		[[nodiscard]] auto viewport_extent() const noexcept -> int;
		[[nodiscard]] auto within_line_position() const noexcept -> qreal;
		void               reset_pixel_gesture();
		void               set_scroll_observer(std::function<void()> aObserver);

	protected:
		void scrollContentsBy(const int aDx, const int aDy) override;

		// [[BPR-1734]]. Focus changes the selection's colour.
		void focusInEvent(QFocusEvent* const aEvent) override;
		void focusOutEvent(QFocusEvent* const aEvent) override;

		void wheelEvent(QWheelEvent* const aEvent) override;

	protected:
		/// \brief Draws a rule where each visible change group begins.
		///
		/// \details A boundary is a position between two lines, not a
		/// property of either one, so it is drawn on the edge between them.
		/// The previous implementation filled the unchanged line above the
		/// group instead. That said "this line is special" about a line
		/// that had not changed, in the same full-width tint the added and
		/// removed bands use, carrying the opposite meaning — and it was
		/// mistaken for the selected line at attended review.
		///
		/// Only blocks the viewport can show are walked, and the boundary
		/// list is entered by bisection rather than scanned, so the work
		/// here follows the visible line count and not the diff's size.
		void paintEvent(QPaintEvent* const aEvent) override;

		/// \brief [[BPR-1107]]. Draws each visible line's white space as the
		/// setting asks: a space as a dot, a tab as a line to its stop.
		///
		/// \details The Appearance preview draws through the same shared helper
		/// in the same `code_line_number_text` role, so the preview shows what
		/// the review window shows. A line is in the selection when the
		/// pane's washed lines hold it. Only the blocks the viewport can show
		/// are walked.
		void paint_white_space_of_visible_lines(QPainter& aPainter);

		/// \brief The visible rows whose selected state can affect white space.
		/// \details [[BPR-1525]]. A keyboard selection changes a few rows,
		/// so invalidate their old/new difference rather than force a full
		/// viewport paint. Qt invalidates the text bands independently.
		[[nodiscard]] auto selected_rows_region() const -> QRegion;

		/// \brief Whether one block lies under the pane's selection wash.
		[[nodiscard]] auto selected_block(const int aBlock) const -> bool;

		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		/// \brief [[BPR-1335]]. The pointer on a head line's code shows its
		/// "+"; a press or a drag in the code is the text's, and hides it.
		auto viewportEvent(QEvent* aEvent) -> bool override;

		/// \brief [[BPR-1212]] v002, [[BPR-1533]]. Paints the text as
		/// `QPlainTextEdit` does, except the text selection.
		///
		/// \details Qt draws a text selection in the palette's
		/// `HighlightedText`, which replaces every glyph's colour, so the
		/// selected code lost its syntax colours (the owner, v001: "are we
		/// meant to lose all syntax highlighting when we select
		/// something?"). v002 made it a translucent wash under the
		/// intraline emphasis, which hid it: on the selected line it was
		/// the line's own wash again, 1.19:1 in Dark, and on a changed run
		/// the emphasis covered it (the owner, 2026-10-15: "quite hard to
		/// see a selected block"). Now it is the top layer, an opaque
		/// fill of the pane's `Highlight` (`code_text_selection_fill`:
		/// full strength, mixed toward the canvas in Light, which the
		/// owner found "slightly overdone" at full strength), with each
		/// glyph in its own colour made readable on it
		/// (`append_text_selection_ranges`), as Line details draws its
		/// selection. Only public and
		/// protected `QPlainTextEdit` members are used; the panes are
		/// read-only and draw no caret, placeholder or preedit, which Qt's
		/// paint also skips for them.
		void paint_text(QPaintEvent* aEvent);
		/// \brief [[BPR-1798]]. Draws `FindRings` over the painted text:
		/// one pixel for a match, two for the current one. Each ring
		/// walks only its own blocks, so the work follows the visible
		/// matches rather than matches times visible lines.
		void paint_find_rings();

	public:
		/// \brief [[BPR-1533]]. The ink of unclassified text and the
		/// variant whose floor the selected glyphs are held to.
		void set_selection_ink(const QColor& aNormal, const core::appearance_variant aVariant);

	protected:
	public:
		/// \brief [[BPR-1336]]. Hears that the pane has relaid out in a new font.
		///
		/// \details The pane does not wrap, so every line is one block of the
		/// same height and its bar counts lines: a font change keeps the top
		/// line and moves every line below it. The observer is told after the
		/// relayout, while the pane still shows the old top line, which is
		/// what lets it keep a line where the reader was looking.
		void set_font_observer(std::function<void()> aObserver);
		/// \brief The height of one row, from the pane's own layout.
		[[nodiscard]] auto row_height() const -> qreal;
		/// \brief Where a line's row starts in the viewport, if it is laid out.
		[[nodiscard]] auto row_top(const std::uint32_t aLine) const -> std::optional<qreal>;

	private:
		void                                   update_gutter();
		void                                   update_gutter(const QRegion& aRegion);
		void                                   place_gutter();
		[[nodiscard]] auto                     measured_gutter_width() const -> int;
		void                                   restore_viewport_fill();
		QRect                                  ViewportArea;
		qreal                                  RetainedOffset = 0.0;
		pixel_scroll_gesture                   PixelGesture;
		std::function<void()>                  ScrollObserver;
		bool                                   ApplyingPixelWheel = false;
		int                                    AppliedOffset      = 0;
		bool                                   PlacingOrigin      = false;
		bool                                   LineNumbersVisible = true;
		int                                    GutterWidth        = 0;
		const std::vector<unified_source_row>* UnifiedRows        = nullptr;
		QColor                                 RemovedColor;
		QColor                                 AddedColor;
		std::optional<core::line_span>         SelectedBase;
		std::optional<core::line_span>         SelectedHead;

		class gutter_area;
		class add_button;

		gutter_area* Gutter    = nullptr;
		add_button*  AddButton = nullptr;
		QColor       GutterBackground;
		QColor       GutterText;
		/// [[BPR-1734]]. The text selection last painted, as positions.
		std::optional<std::pair<int, int>>                PaintedSelection;
		code_gutter_text_cache                            GutterLabels;
		std::vector<std::uint32_t>                        Boundaries;
		QColor                                            RuleColor;
		std::optional<std::uint32_t>                      ConversationAnchor;
		std::vector<diff_line_mark>                       Marks;
		bool                                              MarkerColumn = false;
		std::function<void(std::uint32_t, std::uint32_t)> AddDispatch;
		std::optional<std::uint32_t>                      AddHover;
		/// [[BPR-1335]]. A drag on the "+": the line it started on and the
		/// line under the pointer.
		std::optional<std::pair<std::uint32_t, std::uint32_t>> AddDrag;
		/// [[BPR-1724]]. A drag over the numbers: the line under the
		/// pointer.
		std::function<void(std::uint32_t, bool)>   NumberDispatch;
		std::optional<std::uint32_t>               NumberDrag;
		std::function<void(const diff_line_mark&)> MarkDispatch;
		QColor                                     ThreadMarkColor{ 0xD2, 0x99, 0x22 };
		QColor                                     NewThreadMarkColor{ 0x09, 0x69, 0xDA };
		std::uint64_t                              PaintedRules       = 0U;
		std::uint64_t                              PaintedLineNumbers = 0U;
		std::uint64_t                              PaintedWhiteSpace  = 0U;
		std::optional<core::line_span>             SelectedLines;
		core::white_space_drawing                  WhiteSpace = core::white_space_drawing::none;
		std::function<void()>                      FontObserver;
		QPointer<QTextDocument>                    CachedTextDocument;
		QList<QTextCursor>                         CachedTextCursors;
		QColor                                     SelectionNormal;
		core::appearance_variant                   SelectionVariant = core::appearance_variant::light;
	};

	/// \brief Paints one pane's own line numbers beside its own text.
	///
	/// \details A child of the pane rather than content inside its viewport,
	/// because `setViewportMargins` is what actually moves the text aside;
	/// painting numbers into the unshrunk viewport would put them on top of
	/// the code. It also keeps BPR-0414's four-pixel conversation marker
	/// correct for free: that rect is viewport-relative, so it follows the
	/// new left edge instead of ending up behind the numbers.
	class source_pane::gutter_area final : public QWidget
	{
	public:
		explicit gutter_area(source_pane* const aPane);

		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* const aEvent) override;
		/// [[BPR-1290]], [[BPR-1335]]. A thread's tinted number opens or
		/// closes it.
		/// [[BPR-1724]]. Anywhere else on a number, a click selects its
		/// line, a Shift-click extends the selection to it, and a drag
		/// selects the lines it crosses, as on GitHub. Shift on a tinted
		/// number extends too. The "+" column's part in the number's
		/// padding stays the "+"'s, shown or not, so a press there never
		/// selects instead of adding a comment.
		void mousePressEvent(QMouseEvent* const aEvent) override;
		/// [[BPR-1335]]. The pointer in the "+" column's part in the number's
		/// padding shows the line's "+", as its part over the code does.
		void mouseMoveEvent(QMouseEvent* const aEvent) override;
		void mouseReleaseEvent(QMouseEvent* const aEvent) override;
		void leaveEvent(QEvent* const aEvent) override;
		/// [[BPR-1721]]. A right-click on a line's number asks for
		/// the pane's menu at that line, as one on its code does. The
		/// scroll area drops a pointer's menu request that is not its
		/// viewport's, so the numbers opened no menu.
		void contextMenuEvent(QContextMenuEvent* const aEvent) override;
		auto event(QEvent* const aEvent) -> bool override;

	private:
		source_pane* Pane = nullptr;
	};

	/// \brief [[BPR-1335]]. The head pane's "+": GitHub's blue button at
	/// the start of the line's code.
	///
	/// \details The owner, rejecting a "+" that took the line number's
	/// place (session 3c9017b5): "I like option 1 but I don't like the +
	/// replacing the line number, i think github actual does it better".
	/// GitHub shows a small blue button on the hovered line, over the
	/// boundary between the number and the code, and the number stays.
	/// It is drawn only while the pointer is over that column (v002).
	/// Here its left edge sits in the gutter's right padding, so it never
	/// covers a digit, and the rest lies over the code's left margin and
	/// first characters, as GitHub's does.
	///
	/// A child of the pane, above the gutter and the viewport, so it is one
	/// widget across their boundary with its own tooltip, cursor and
	/// accessible button. Pressing it never reaches the code, so it cannot
	/// start or change a text selection; a press anywhere else in the code
	/// is the text's. A click comments on its line; a drag comments on the
	/// lines from where it started to the one under the pointer. It takes
	/// no keyboard focus: Return and the context menu are the keyboard's
	/// way to the same comment, and its tooltip says so.
	class source_pane::add_button final : public QAbstractButton
	{
	public:
		explicit add_button(source_pane* const aPane);

		void set_line(const std::uint32_t aLine);

		[[nodiscard]] auto line() const noexcept -> std::uint32_t;

		void set_colour(const QColor& aColour);

	protected:
		void               paintEvent(QPaintEvent* const /*aEvent*/) override;
		void               mousePressEvent(QMouseEvent* const aEvent) override;
		void               mouseMoveEvent(QMouseEvent* const aEvent) override;
		void               mouseReleaseEvent(QMouseEvent* const aEvent) override;
		void               enterEvent(QEnterEvent* const aEvent) override;
		void               leaveEvent(QEvent* const aEvent) override;
		[[nodiscard]] auto hitButton(const QPoint& aPoint) const -> bool override;

	private:
		source_pane*  Pane = nullptr;
		std::uint32_t Line = 0U;
		QColor        Colour{ 0x09, 0x69, 0xDA };
	};

	/// Disabling Qt's flag alone retains its allocated glyph data. Qt frees
	/// it on completion of a noncached draw; a clip above every layout line
	/// completes that operation without painting or changing line geometry.
	void release_text_cache(QTextLayout& aLayout, QPainter& aPainter);

	[[nodiscard]] auto pane(QPlainTextEdit* const aPane) noexcept
		-> source_pane*;

	[[nodiscard]] auto text_document(
		const std::optional<core::decoded_text_document>& aDocument) -> QString;

	/// \brief One side's detached document and how far it is filled.
	struct staged_document
	{
		QPointer<QTextDocument> Document;
		QTextCursor             End;
		QString                 Text;
		qsizetype               Position = 0;

		void               start(QPlainTextEdit& aPane, const std::optional<core::decoded_text_document>& aSource);
		[[nodiscard]] auto complete() const noexcept -> bool;
		/// Appends up to `aLines` lines. A batch ends on a line break, so the
		/// next one starts a fresh block, exactly as one replacement would.
		void append(const int aLines);
		void discard();
	};

	/// \brief The lines one pane could currently show, with a margin.
	///
	/// \details Painting is bounded to this. `QPlainTextEdit` walks its
	/// whole extra-selection list on every paint, so one selection per
	/// changed line over a sixty-thousand-line comparison would cost sixty
	/// thousand list entries per repaint while the widget count stayed at
	/// sixty-eight. RFC-BPR-0005's bounded-widget evidence would still hold
	/// and the surface would still be slow, which is precisely the failure
	/// that evidence cannot see.
	///
	/// The margin is a screenful either side so a scroll reveals painted
	/// lines rather than a band appearing late.
	[[nodiscard]] auto paintable_line_bounds(QPlainTextEdit& aPane)
		-> core::line_span;

	struct diff_workbench::unified_presentation
	{
		struct group_rows
		{
			diff_link_map_identity Identity;
			core::line_span        Removed;
			core::line_span        Added;
		};
		std::vector<unified_source_row> Rows;
		std::vector<std::uint32_t>      BaseRows;
		std::vector<std::uint32_t>      HeadRows;
		std::vector<group_rows>         Groups;
	};

	struct diff_workbench::find_presentation
	{
		struct decoration
		{
			int           Begin     = 0;
			int           End       = 0;
			int           PrefixEnd = 0;
			std::uint32_t Match     = 0U;
		};
		source_find_bar*                                                                        Bar = nullptr;
		QPointer<QPlainTextEdit>                                                                Recent;
		QPointer<QPlainTextEdit>                                                                Target;
		std::array<bool, 3>                                                                     RealCaret{ };
		std::optional<core::source_search_range>                                                Capture;
		std::optional<core::source_search_range>                                                Fixed;
		source_find_intent                                                                      Intent;
		std::function<void(const source_find_intent&)>                                          Dispatch;
		std::function<void()>                                                                   Changed;
		std::array<std::vector<decoration>, 3>                                                  Decorations;
		std::shared_ptr<const std::vector<application::source_search_match>>                    Records;
		std::weak_ptr<const std::vector<application::source_search_match>>                      MappedRecords;
		std::optional<std::uint32_t>                                                            Current;
		bool                                                                                    Rebind            = false;
		bool                                                                                    ClosedNavigable   = false;
		bool                                                                                    Recall            = false;
		bool                                                                                    Navigating        = false;
		bool                                                                                    ExternalSelection = false;
		std::optional<core::change_navigation_direction>                                        ReopenDirection;
		std::optional<core::source_match_location>                                              PendingReveal;
		std::optional<core::source_match_location>                                              CopyMatch;
		QTextCursor                                                                             CopyCursor;
		std::map<std::pair<core::diff_side, core::byte_span>, std::vector<std::pair<int, int>>> Fragments;
		std::vector<std::pair<int, int>>                                                        EmptyFragments;
		std::array<std::map<std::size_t, int>, 2>                                               Positions;
	};

	struct diff_workbench::head_edit
	{
		QPointer<suggestion_editor> Editor;
		QPointer<QPlainTextEdit>    Host;
		QPointer<QObject>           Follower;
		/// The selected head lines, and how many there are.
		std::uint32_t First         = 0U;
		std::uint32_t Last          = 0U;
		std::uint32_t OriginalCount = 1U;
		/// The fill of each head line, as the head pane paints it.
		std::vector<QColor>     Fills;
		bool                    OnePane = false;
		QMetaObject::Connection Scrolled;
		std::function<void()>   Ended;

		/// The edit's first and last editable lines now.
		[[nodiscard]] auto range() const -> std::pair<std::uint64_t, std::uint64_t>;
		[[nodiscard]] auto count() const -> std::uint64_t;
	};

	struct diff_workbench::staged_publication
	{
		application::diff_publication Publication;
		staged_document               Left;
		staged_document               Right;
	};

	struct diff_workbench::geometry_measurement
	{
		std::shared_ptr<const core::computed_file_diff> Result;
		plain_text_pane_measurement                     Left;
		plain_text_pane_measurement                     Right;
	};
}
#endif
