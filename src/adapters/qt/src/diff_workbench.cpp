#include "bpr/qt/diff_workbench.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QApplication>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMargins>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedLayout>
#include <QStyleOptionSlider>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// Width of the change-group boundary rule, in device-independent pixels.
		constexpr auto BoundaryRuleWidth = 2.0;
		/// [[BPR-1335]]. Breathing room either side of the widest number the
		/// gutter shows. The owner found the gutter wider than its numbers.
		constexpr auto GutterPadding = 4;
		/// [[BPR-1290]]. The gutter's column for thread markers, right of the
		/// numbers, and the marker's box in it.
		constexpr auto MarkerColumnWidth = 22;
		constexpr auto MarkerBox         = 18;

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
			thread_tick_scroll_bar(const Qt::Orientation aOrientation, QWidget* const aParent)
				: QScrollBar{ aOrientation, aParent }
			{
				setMouseTracking(true);
				anchor_tooltips(*this, [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
					const auto index = tick_at(aPosition);
					if (!index.has_value()) {
						return std::nullopt;
					}
					return tooltip_of(Ticks[*index]);
				});
			}

			~thread_tick_scroll_bar() override
			{
				watch_shift(false);
			}

			thread_tick_scroll_bar(const thread_tick_scroll_bar&)                    = delete;
			auto operator=(const thread_tick_scroll_bar&) -> thread_tick_scroll_bar& = delete;
			thread_tick_scroll_bar(thread_tick_scroll_bar&&)                         = delete;
			auto operator=(thread_tick_scroll_bar&&) -> thread_tick_scroll_bar&      = delete;

			std::function<void(core::diff_side, std::uint32_t)> GoTo;
			QColor                                              Thread{ 0xD2, 0x99, 0x22 };
			QColor                                              New{ 0x09, 0x69, 0xDA };

			void set_ticks(std::vector<diff_thread_tick> aTicks)
			{
				if (Ticks == aTicks) {
					return;
				}
				Ticks = std::move(aTicks);
				follow_pointer();
				update();
			}

			[[nodiscard]] auto ticks() const noexcept -> const std::vector<diff_thread_tick>&
			{
				return Ticks;
			}

			/// \brief A tick's rectangle in the bar's coordinates.
			[[nodiscard]] auto tick_rect(const diff_thread_tick& aTick) const -> QRect
			{
				auto option = QStyleOptionSlider{ };
				initStyleOption(&option);
				const auto groove = style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarGroove, this);
				const auto slider = style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, this);
				const auto travel = std::max(0, groove.height() - slider.height());
				const auto centre = groove.top() + static_cast<int>(std::lround(std::clamp(aTick.Fraction, 0.0, 1.0) * travel)) + slider.height() / 2;
				const auto height = aTick.Open ? 6 : 4;
				return QRect{ groove.left() + 2, centre - height / 2, std::max(2, groove.width() - 4), height };
			}

			/// \brief What a tick's tooltip says, anchored to the band across
			/// the bar in which the pointer finds it, so the tooltip stays
			/// while the pointer is anywhere a press would choose that tick.
			[[nodiscard]] auto tooltip_of(const diff_thread_tick& aTick) const -> anchored_tooltip_target
			{
				auto text = aTick.Description;
				if (!text.isEmpty()) {
					text[0] = text[0].toLower();
				}
				return anchored_tooltip_target{
					.Anchor = QRect{ 0, tick_rect(aTick).top() - TickReach, width(), tick_rect(aTick).height() + 2 * TickReach },
					.Text   = text.isEmpty() ? QStringLiteral("Shift-click to go to this thread") : QStringLiteral("Shift-click to go to %1").arg(text),
				};
			}

			/// \brief The tick a point finds: of those within `TickReach` of
			/// the point, the one whose centre is nearest; the first on a tie.
			[[nodiscard]] auto tick_at(const QPoint aPosition) const -> std::optional<std::size_t>
			{
				if (aPosition.x() < 0 || aPosition.x() >= width()) {
					return std::nullopt;
				}
				auto found   = std::optional<std::size_t>{ };
				auto nearest = std::numeric_limits<int>::max();
				for (std::size_t index = 0U; index < Ticks.size(); ++index) {
					const auto rect = tick_rect(Ticks[index]);
					if (aPosition.y() < rect.top() - TickReach || aPosition.y() > rect.bottom() + TickReach) {
						continue;
					}
					if (const auto distance = std::abs(aPosition.y() - rect.center().y()); distance < nearest) {
						nearest = distance;
						found   = index;
					}
				}
				return found;
			}

			/// \brief The tick Shift and a press would go to now, if any.
			[[nodiscard]] auto targeted() const -> std::optional<diff_thread_tick>
			{
				const auto at = std::ranges::find_if(Ticks, [this](const diff_thread_tick& aTick) {
					return Target.has_value() && aTick.Side == Target->first && aTick.Line == Target->second;
				});
				return at == Ticks.end() ? std::nullopt : std::optional{ *at };
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				QScrollBar::paintEvent(aEvent);
				if (Ticks.empty()) {
					return;
				}
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto faint = palette().color(QPalette::Active, QPalette::PlaceholderText);
				for (const auto& tick : Ticks) {
					const auto rect = QRectF{ tick_rect(tick) };
					painter.setOpacity(tick.Resolved && !tick.Open ? 0.45 : 1.0);
					if (tick.Draft && !tick.Open) {
						painter.setPen(QPen{ faint, 1.0 });
						painter.setBrush(Qt::NoBrush);
						painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);
					} else {
						painter.setPen(Qt::NoPen);
						painter.setBrush(tick.Open ? Thread : tick.Unread ? New
																		  : faint);
						painter.drawRoundedRect(rect, 2.0, 2.0);
					}
				}
				// The tick Shift-click would go to, ringed in the window's ink.
				if (const auto target = targeted(); target.has_value()) {
					painter.setOpacity(1.0);
					painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::WindowText), 1.5 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(QRectF{ tick_rect(*target) }.adjusted(-1.5, -1.5, 1.5, 1.5), 3.0, 3.0);
				}
			}

			void mousePressEvent(QMouseEvent* const aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton && aEvent->modifiers().testFlag(Qt::ShiftModifier) && GoTo) {
					if (const auto index = tick_at(aEvent->position().toPoint()); index.has_value()) {
						// The dispatch places the ticks again; it is handed a copy.
						const auto side = Ticks[*index].Side;
						const auto line = Ticks[*index].Line;
						aEvent->accept();
						GoTo(side, line);
						return;
					}
				}
				QScrollBar::mousePressEvent(aEvent);
			}

			void mouseMoveEvent(QMouseEvent* const aEvent) override
			{
				Pointer   = aEvent->position().toPoint();
				ShiftHeld = aEvent->modifiers().testFlag(Qt::ShiftModifier);
				watch_shift(true);
				follow_pointer();
				QScrollBar::mouseMoveEvent(aEvent);
			}

			void enterEvent(QEnterEvent* const aEvent) override
			{
				Pointer   = aEvent->position().toPoint();
				ShiftHeld = aEvent->modifiers().testFlag(Qt::ShiftModifier);
				watch_shift(true);
				follow_pointer();
				QScrollBar::enterEvent(aEvent);
			}

			void leaveEvent(QEvent* const aEvent) override
			{
				Pointer.reset();
				watch_shift(false);
				follow_pointer();
				QScrollBar::leaveEvent(aEvent);
			}

			/// Shift pressed or let go while the pointer rests on the bar.
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::KeyRelease) {
					if (static_cast<QKeyEvent*>(aEvent)->key() == Qt::Key_Shift) {
						ShiftHeld = aEvent->type() == QEvent::KeyPress;
						follow_pointer();
					}
				}
				return QScrollBar::eventFilter(aWatched, aEvent);
			}

		private:
			std::vector<diff_thread_tick>                            Ticks;
			std::optional<QPoint>                                    Pointer;
			std::optional<std::pair<core::diff_side, std::uint32_t>> Target;
			bool                                                     ShiftHeld = false;
			bool                                                     Watching  = false;

			/// The key events go to the focus, not to the bar: while the
			/// pointer is over it, the application's are watched for Shift.
			void watch_shift(const bool aWatch)
			{
				auto* const application = QCoreApplication::instance();
				if (aWatch == Watching || application == nullptr) {
					return;
				}
				Watching = aWatch;
				if (aWatch) {
					application->installEventFilter(this);
				} else {
					application->removeEventFilter(this);
				}
			}

			/// Rings the tick Shift-click would choose, shows the hand and
			/// names where it goes at once: with Shift held the tooltip does
			/// not wait for the hover delay (the owner, rejecting the built
			/// drawer v002: "Tooltip can show up immediately with the shift
			/// mouseover."). Without Shift a tick's tooltip keeps its delay.
			void follow_pointer()
			{
				auto target = std::optional<std::pair<core::diff_side, std::uint32_t>>{ };
				auto index  = std::optional<std::size_t>{ };
				if (ShiftHeld && Pointer.has_value()) {
					index = tick_at(*Pointer);
					if (index.has_value()) {
						target = std::pair{ Ticks[*index].Side, Ticks[*index].Line };
					}
				}
				if (target == Target) {
					return;
				}
				Target = std::move(target);
				if (Target.has_value()) {
					setCursor(Qt::PointingHandCursor);
					const auto tip = tooltip_of(Ticks[*index]);
					show_anchored_tooltip(*this, tip.Anchor, tip.Text);
				} else {
					unsetCursor();
				}
				update();
			}
		};

		struct unified_source_row
		{
			std::optional<std::uint32_t> Base;
			std::optional<std::uint32_t> Head;
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
			void set_unified_rows(const std::vector<unified_source_row>* aRows)
			{
				UnifiedRows = aRows;
				refresh_gutter_width();
			}
			void set_row_colors(const QColor& aRemoved, const QColor& aAdded);
			void set_unified_selection(std::optional<core::line_span> aBase, std::optional<core::line_span> aHead)
			{
				SelectedBase = aBase;
				SelectedHead = aHead;
			}
			/// \brief [[BPR-1107]]. The lines this side's selection washes.
			void set_selected_lines(std::optional<core::line_span> aLines)
			{
				if (SelectedLines != aLines) {
					SelectedLines = aLines;
					viewport()->update();
				}
			}
			/// \brief [[BPR-1107]]. When this pane draws white space.
			void set_white_space(const core::white_space_drawing aDrawing)
			{
				if (WhiteSpace != aDrawing) {
					WhiteSpace = aDrawing;
					viewport()->update();
				}
			}

			/// \brief The gutter's width, as `refresh_gutter_width` last measured it.
			[[nodiscard]] auto gutter_width() const noexcept -> int
			{
				return GutterWidth;
			}
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
			void               refresh_gutter_width();
			void               set_line_numbers_visible(bool aVisible);
			void               set_gutter_colors(const QColor& aBackground, const QColor& aText);
			void               paint_line_numbers(QPaintEvent* aEvent);
			void               paint_markers(QPainter& aPainter);
			[[nodiscard]] auto painted_line_numbers() const noexcept -> std::uint64_t
			{
				return PaintedLineNumbers;
			}

			/// \brief Adopts the lines each change group starts on.
			///
			/// \details Held sorted so a repaint can find the visible ones by
			/// bisection. Computed once when a result is published rather than
			/// per paint, because the group count follows the size of the diff
			/// and only the search may be allowed to follow the viewport.
			void set_group_boundaries(std::vector<std::uint32_t> aBoundaries)
			{
				Boundaries = std::move(aBoundaries);
				viewport()->update();
			}

			void set_boundary_rule_color(const QColor& aColor)
			{
				RuleColor = aColor;
				viewport()->update();
			}

			void set_conversation_anchor(std::optional<std::uint32_t> aLine)
			{
				ConversationAnchor = aLine;
				viewport()->update();
			}

			/// \brief One line's row in viewport coordinates, or empty.
			[[nodiscard]] auto line_row_rect(const std::uint32_t aLine) const -> QRect
			{
				const auto block = document()->findBlockByNumber(static_cast<int>(aLine));
				if (!block.isValid() || !block.isVisible()) {
					return { };
				}
				const auto geometry = blockBoundingGeometry(block).translated(contentOffset());
				return QRect{ 0, qRound(geometry.top()), viewport()->width(),
							  std::max(1, qRound(geometry.height())) };
			}

			[[nodiscard]] auto conversation_anchor_rect() const -> QRect
			{
				if (!ConversationAnchor.has_value()) {
					return { };
				}
				const auto row = line_row_rect(*ConversationAnchor);
				if (row.isEmpty()) {
					return { };
				}
				return QRect{ 1, row.top(), 4, row.height() };
			}

			/// \brief [[BPR-0648]]. The lines carrying a thread, painted at the edge.
			void set_line_marks(std::vector<diff_line_mark> aMarks)
			{
				std::ranges::stable_sort(aMarks, { }, &diff_line_mark::Line);
				if (Marks == aMarks) {
					return;
				}
				Marks = std::move(aMarks);
				viewport()->update();
				update_gutter();
			}

			/// \brief [[BPR-1290]]. A column in the gutter for thread markers.
			void set_marker_column(const bool aEnabled)
			{
				if (MarkerColumn == aEnabled) {
					return;
				}
				MarkerColumn = aEnabled;
				refresh_gutter_width();
				place_gutter();
				viewport()->update();
				update_gutter();
			}

			[[nodiscard]] auto marker_column_width() const -> int
			{
				return MarkerColumn && LineNumbersVisible ? MarkerColumnWidth : 0;
			}

			void set_mark_theme(const feedback_mark_theme& aTheme)
			{
				MarkTheme = aTheme;
				update_gutter();
			}

			void set_mark_dispatch(std::function<void(const diff_line_mark&)> aDispatch)
			{
				MarkDispatch = std::move(aDispatch);
			}

			/// \brief [[BPR-1212]]. The gutter's "+" on a line without a
			/// thread, shown under the pointer; none without a dispatch.
			void set_add_dispatch(std::function<void(std::uint32_t)> aDispatch)
			{
				AddDispatch = std::move(aDispatch);
				set_add_hover(std::nullopt);
			}

			void set_add_hover(const std::optional<std::uint32_t> aLine)
			{
				const auto line = AddDispatch && marker_column_width() > 0 ? aLine : std::nullopt;
				if (AddHover != line) {
					AddHover = line;
					update_gutter();
				}
			}

			/// \brief The line whose "+" a gutter point is over, if any.
			[[nodiscard]] auto add_line_at(const QPoint aGutterPoint) const -> std::optional<std::uint32_t>
			{
				if (!AddDispatch || marker_column_width() == 0 || UnifiedRows != nullptr || aGutterPoint.x() < gutter_width() - MarkerColumnWidth
					|| mark_at(aGutterPoint) != nullptr) {
					return std::nullopt;
				}
				const auto block = cursorForPosition(QPoint{ 0, aGutterPoint.y() }).block();
				if (!block.isValid() || block.blockNumber() < 0) {
					return std::nullopt;
				}
				const auto line = static_cast<std::uint32_t>(block.blockNumber());
				if (std::ranges::any_of(Marks, [line](const diff_line_mark& aMark) { return aMark.Line == line; })) {
					return std::nullopt;
				}
				return line;
			}

			[[nodiscard]] auto add_rect() const -> QRect
			{
				if (!AddHover.has_value()) {
					return { };
				}
				return marker_rect(diff_line_mark{ .Line = *AddHover });
			}

			void press_add(const std::uint32_t aLine)
			{
				if (AddDispatch) {
					AddDispatch(aLine);
				}
			}

			/// \brief Where a mark's marker sits, in the gutter's coordinates.
			[[nodiscard]] auto marker_rect(const diff_line_mark& aMark) const -> QRect
			{
				if (marker_column_width() == 0) {
					return { };
				}
				const auto row = line_row_rect(aMark.Line);
				if (row.isEmpty()) {
					return { };
				}
				const auto header = UnifiedRows != nullptr ? viewport()->geometry().top() - contentsRect().top() : 0;
				const auto left   = gutter_width() - MarkerColumnWidth + (MarkerColumnWidth - MarkerBox) / 2;
				return QRect{ left, row.top() + header + (row.height() - MarkerBox) / 2, MarkerBox, MarkerBox };
			}

			/// \brief The mark whose marker is under a point of the gutter.
			[[nodiscard]] auto mark_at(const QPoint aGutterPoint) const -> const diff_line_mark*
			{
				for (const auto& mark : Marks) {
					if (marker_rect(mark).adjusted(-2, -2, 2, 2).contains(aGutterPoint)) {
						return &mark;
					}
				}
				return nullptr;
			}

			void press_mark(const diff_line_mark& aMark)
			{
				if (MarkDispatch) {
					MarkDispatch(aMark);
				}
			}

			[[nodiscard]] auto thread_colour() const noexcept -> const QColor&
			{
				return ThreadMarkColor;
			}

			[[nodiscard]] auto new_colour() const noexcept -> const QColor&
			{
				return NewThreadMarkColor;
			}

			[[nodiscard]] auto line_marks() const noexcept -> const std::vector<diff_line_mark>&
			{
				return Marks;
			}

			void set_mark_colors(const QColor& aThread, const QColor& aNewThread)
			{
				if (ThreadMarkColor == aThread && NewThreadMarkColor == aNewThread) {
					return;
				}
				ThreadMarkColor    = aThread;
				NewThreadMarkColor = aNewThread;
				viewport()->update();
				update_gutter();
			}

			[[nodiscard]] auto painted_rules() const noexcept -> std::uint64_t
			{
				return PaintedRules;
			}

			[[nodiscard]] auto painted_white_space() const noexcept -> std::uint64_t
			{
				return PaintedWhiteSpace;
			}

			/// \brief The first block the viewport can show, and where it sits.
			///
			/// \details `firstVisibleBlock` and `contentOffset` are protected
			/// on `QPlainTextEdit`, so this is the only place they can be read.
			/// Together they say which line is at the top and how far above the
			/// viewport edge it starts, which is everything needed to turn a
			/// per-line document measurement into a scroll origin.
			[[nodiscard]] auto visible_origin() const
				-> std::optional<std::pair<std::size_t, qreal>>
			{
				const auto block = firstVisibleBlock();
				if (!block.isValid() || block.blockNumber() < 0) {
					return std::nullopt;
				}
				return std::pair{
					static_cast<std::size_t>(block.blockNumber()),
					contentOffset().y(),
				};
			}

			/// \brief Scrolls so a line is at the top.
			///
			/// \details The pane does not wrap, so its native bar counts
			/// lines. [[BPR-1291]]: the pane scrolls on centre, which lets its
			/// bar reach the last line, as scroll-past-end needs.
			void set_top_line(const std::size_t aLine)
			{
				auto* const bar = verticalScrollBar();
				if (bar != nullptr) {
					bar->setValue(static_cast<int>(std::min<std::size_t>(aLine, std::numeric_limits<int>::max())));
				}
			}

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
			void paintEvent(QPaintEvent* const aEvent) override
			{
				QPlainTextEdit::paintEvent(aEvent);
				auto block = firstVisibleBlock();
				if (!block.isValid()) {
					return;
				}
				auto       top   = blockBoundingGeometry(block)
									   .translated(contentOffset())
									   .top();
				const auto limit = static_cast<qreal>(viewport()->height());
				const auto width = viewport()->width();
				auto       next  = std::ranges::lower_bound(
					Boundaries,
					static_cast<std::uint32_t>(std::max(block.blockNumber(), 0)));
				auto painter = QPainter{ viewport() };
				if (!Boundaries.empty() && RuleColor.isValid()) {
					painter.setPen(QPen{ RuleColor, BoundaryRuleWidth });
					while (block.isValid() && top <= limit && next != Boundaries.end()) {
						if (block.isVisible()
							&& *next == static_cast<std::uint32_t>(block.blockNumber())) {
							const auto y = qRound(top);
							painter.drawLine(0, y, width, y);
							++PaintedRules;
							++next;
						}
						top += blockBoundingRect(block).height();
						block = block.next();
					}
				}
				// [[BPR-0648]]. A thread's line carries a mark at the edge:
				// amber for a thread, blue for a new one, beneath the anchor
				// marker so the current line's marker still reads. [[BPR-1290]]:
				// with a marker column the marker is in the gutter instead, and
				// the open thread's line is washed amber (the note under the
				// pointer, lightly).
				for (const auto& mark : Marks) {
					const auto row = line_row_rect(mark.Line);
					if (row.isEmpty() || row.top() > viewport()->height() || row.bottom() < 0) {
						continue;
					}
					painter.setPen(Qt::NoPen);
					if (mark.Open || mark.Hovered) {
						auto wash = mark.Writing ? palette().color(QPalette::Active, QPalette::Highlight) : ThreadMarkColor;
						wash.setAlphaF(mark.Open ? (mark.Writing ? 0.14F : 0.18F) : 0.08F);
						painter.fillRect(QRect{ 0, row.top(), viewport()->width(), row.height() }, wash);
					}
					if (marker_column_width() > 0) {
						continue;
					}
					painter.setBrush(mark.Kind == diff_line_mark_kind::new_thread
										 ? NewThreadMarkColor
										 : ThreadMarkColor);
					painter.drawRoundedRect(QRect{ 1, row.top() + 1, 4, std::max(1, row.height() - 2) }, 2.0, 2.0);
				}
				if (const auto marker = conversation_anchor_rect(); !marker.isEmpty()) {
					auto color = palette().color(QPalette::Highlight);
					color.setAlpha(220);
					painter.setPen(Qt::NoPen);
					painter.setBrush(color);
					painter.drawRoundedRect(marker, 2.0, 2.0);
				}
				paint_white_space_of_visible_lines(painter);
			}

			/// \brief [[BPR-1107]]. Draws each visible line's white space as the
			/// setting asks: a space as a dot, a tab as a line to its stop.
			///
			/// \details The Appearance preview draws through the same shared helper
			/// in the same `code_line_number_text` role, so the preview shows what
			/// the review window shows. A line is in the selection when the
			/// pane's washed lines hold it. Only the blocks the viewport can show
			/// are walked.
			void paint_white_space_of_visible_lines(QPainter& aPainter)
			{
				if (!GutterText.isValid() || WhiteSpace == core::white_space_drawing::none) {
					return;
				}
				const auto limit = static_cast<qreal>(viewport()->height());
				for (auto block = firstVisibleBlock(); block.isValid(); block = block.next()) {
					const auto geometry = blockBoundingGeometry(block).translated(contentOffset());
					if (geometry.top() > limit) {
						break;
					}
					if (!block.isVisible()) {
						continue;
					}
					PaintedWhiteSpace += paint_white_space_of_block(aPainter, *this, block, WhiteSpace, selected_block(block.blockNumber()), GutterText);
				}
			}

			/// \brief Whether one block lies under the pane's selection wash.
			[[nodiscard]] auto selected_block(const int aBlock) const -> bool
			{
				const auto contains = [](const std::optional<core::line_span> aSpan, const std::optional<std::uint32_t> aLine) {
					return aSpan && aLine && *aLine >= aSpan->Begin && *aLine < aSpan->End;
				};
				const auto block = static_cast<std::uint32_t>(std::max(aBlock, 0));
				if (UnifiedRows != nullptr) {
					return block < UnifiedRows->size()
						&& (contains(SelectedBase, (*UnifiedRows)[block].Base) || contains(SelectedHead, (*UnifiedRows)[block].Head));
				}
				return contains(SelectedLines, block);
			}

			void resizeEvent(QResizeEvent* aEvent) override;
			void changeEvent(QEvent* aEvent) override;
			void showEvent(QShowEvent* aEvent) override;

		private:
			void                                   update_gutter();
			void                                   place_gutter();
			[[nodiscard]] auto                     measured_gutter_width() const -> int;
			void                                   restore_viewport_fill();
			bool                                   LineNumbersVisible = true;
			int                                    GutterWidth        = 0;
			const std::vector<unified_source_row>* UnifiedRows        = nullptr;
			QColor                                 RemovedColor;
			QColor                                 AddedColor;
			std::optional<core::line_span>         SelectedBase;
			std::optional<core::line_span>         SelectedHead;

			class gutter_area;

			gutter_area*                               Gutter = nullptr;
			QColor                                     GutterBackground;
			QColor                                     GutterText;
			std::vector<std::uint32_t>                 Boundaries;
			QColor                                     RuleColor;
			std::optional<std::uint32_t>               ConversationAnchor;
			std::vector<diff_line_mark>                Marks;
			bool                                       MarkerColumn = false;
			std::function<void(std::uint32_t)>         AddDispatch;
			std::optional<std::uint32_t>               AddHover;
			feedback_mark_theme                        MarkTheme;
			std::function<void(const diff_line_mark&)> MarkDispatch;
			QColor                                     ThreadMarkColor{ 0xD2, 0x99, 0x22 };
			QColor                                     NewThreadMarkColor{ 0x09, 0x69, 0xDA };
			std::uint64_t                              PaintedRules       = 0U;
			std::uint64_t                              PaintedLineNumbers = 0U;
			std::uint64_t                              PaintedWhiteSpace  = 0U;
			std::optional<core::line_span>             SelectedLines;
			core::white_space_drawing                  WhiteSpace = core::white_space_drawing::none;
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
			explicit gutter_area(source_pane* const aPane)
				: QWidget{ aPane }
				, Pane{ aPane }
			{
				setObjectName(QStringLiteral("diffSourceLineNumbers"));
				// The numbers are the pane's own content read another way, and a
				// screen reader announcing every visible integer beside the code
				// would bury it. The pane keeps the accessible name.
				setAccessibleName(QStringLiteral("Line numbers"));
				setFocusPolicy(Qt::NoFocus);
				setMouseTracking(true);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				return QSize{ Pane->gutter_width(), 0 };
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				Pane->paint_line_numbers(aEvent);
			}
			/// [[BPR-1290]]. A thread's marker opens or closes it.
			/// [[BPR-1212]]. A "+" on a head line without one adds a comment.
			void mousePressEvent(QMouseEvent* const aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton) {
					if (const auto* const mark = Pane->mark_at(aEvent->position().toPoint()); mark != nullptr) {
						const auto pressed = *mark;
						Pane->press_mark(pressed);
						aEvent->accept();
						return;
					}
					if (const auto line = Pane->add_line_at(aEvent->position().toPoint()); line.has_value()) {
						Pane->press_add(*line);
						aEvent->accept();
						return;
					}
				}
				QWidget::mousePressEvent(aEvent);
			}
			void mouseMoveEvent(QMouseEvent* const aEvent) override
			{
				const auto add = Pane->add_line_at(aEvent->position().toPoint());
				Pane->set_add_hover(add);
				setCursor(Pane->mark_at(aEvent->position().toPoint()) != nullptr || add.has_value() ? Qt::PointingHandCursor : Qt::ArrowCursor);
				QWidget::mouseMoveEvent(aEvent);
			}
			void leaveEvent(QEvent* const aEvent) override
			{
				Pane->set_add_hover(std::nullopt);
				QWidget::leaveEvent(aEvent);
			}
			auto event(QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::ToolTip) {
					const auto* const help = static_cast<QHelpEvent*>(aEvent);
					if (const auto* const mark = Pane->mark_at(help->pos()); mark != nullptr && !mark->Description.isEmpty()) {
						show_anchored_tooltip(*this, Pane->marker_rect(*mark), mark->Description);
						return true;
					}
					if (const auto line = Pane->add_line_at(help->pos()); line.has_value()) {
						show_anchored_tooltip(*this, Pane->add_rect(), QStringLiteral("Add a comment on line %1  Return").arg(*line + 1U));
						return true;
					}
					hide_anchored_tooltip();
					aEvent->ignore();
					return true;
				}
				return QWidget::event(aEvent);
			}

		private:
			source_pane* Pane = nullptr;
		};

		void source_pane::update_gutter()
		{
			Gutter->update();
		}

		void source_pane::place_gutter()
		{
			const auto area = contentsRect();
			Gutter->setGeometry(QRect{ area.left(), area.top(), gutter_width(), area.height() });
		}

		void source_pane::set_row_colors(const QColor& aRemoved, const QColor& aAdded)
		{
			RemovedColor = aRemoved;
			AddedColor   = aAdded;
			Gutter->update();
		}

		source_pane::source_pane(QWidget* const aParent)
			: QPlainTextEdit{ aParent }
		{
			Gutter = new gutter_area{ this };
			// The width follows the digit count, so it changes when a document is
			// replaced and not while scrolling one.
			connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) {
				refresh_gutter_width();
			});
			connect(this,
					&QPlainTextEdit::updateRequest,
					this,
					[this](const QRect& aRect, const int aDy) {
						if (UnifiedRows != nullptr) {
							Gutter->update();
						} else if (aDy != 0) {
							Gutter->scroll(0, aDy);
						} else {
							Gutter->update(
								0, aRect.y(), Gutter->width(), aRect.height());
						}
					});
			refresh_gutter_width();
			restore_viewport_fill();
		}

		auto source_pane::measured_gutter_width() const -> int
		{
			// Hidden is zero width rather than a hidden widget of the old
			// width. Everything that positions the gutter or reserves room for
			// it reads this width, so zero here is what gives the space back
			// to the code instead of leaving an empty margin.
			if (!LineNumbersVisible) {
				return 0;
			}
			const auto metrics = fontMetrics();
			auto       digit   = 0;
			for (const auto glyph : QStringLiteral("0123456789")) {
				digit = std::max(digit, metrics.horizontalAdvance(glyph));
			}
			const auto digits = [](std::uint32_t aNumber) {
				auto count = 1;
				for (; aNumber >= 10U; aNumber /= 10U) {
					++count;
				}
				return count;
			};
			if (UnifiedRows == nullptr) {
				return digits(static_cast<std::uint32_t>(std::max(1, blockCount()))) * digit + 2 * GutterPadding + marker_column_width();
			}
			// Each side's lines rise down the rows, so its largest number is
			// on the last row that has one.
			auto longest = std::uint32_t{ 1U };
			auto base    = false;
			auto head    = false;
			for (auto row = UnifiedRows->rbegin(); row != UnifiedRows->rend() && !(base && head); ++row) {
				if (!base && row->Base) {
					longest = std::max(longest, *row->Base + 1U);
					base    = true;
				}
				if (!head && row->Head) {
					longest = std::max(longest, *row->Head + 1U);
					head    = true;
				}
			}
			const auto column = std::max({ digits(longest) * digit,
										   metrics.horizontalAdvance(QStringLiteral("base")),
										   metrics.horizontalAdvance(QStringLiteral("head")) })
				+ 2 * GutterPadding;
			return 2 * column + marker_column_width();
		}

		void source_pane::refresh_gutter_width()
		{
			const auto width   = measured_gutter_width();
			const auto margins = QMargins{ width, UnifiedRows != nullptr && LineNumbersVisible ? fontMetrics().height() + 4 : 0, 0, 0 };
			if (viewportMargins() != margins) {
				setViewportMargins(margins);
			}
			if (width != GutterWidth) {
				GutterWidth = width;
				place_gutter();
			}
		}

		void source_pane::set_line_numbers_visible(const bool aVisible)
		{
			if (LineNumbersVisible == aVisible) {
				return;
			}
			LineNumbersVisible = aVisible;
			Gutter->setVisible(aVisible);
			// The margin change resizes the viewport, which is what the
			// workbench's event filter watches to re-measure its geometry, so
			// the ribbons and the link-map band follow without being told here.
			refresh_gutter_width();
			const auto area = contentsRect();
			Gutter->setGeometry(
				QRect{ area.left(), area.top(), gutter_width(), area.height() });
			viewport()->update();
		}

		void source_pane::set_gutter_colors(
			const QColor& aBackground,
			const QColor& aText)
		{
			GutterBackground = aBackground;
			GutterText       = aText;
			Gutter->update();
		}

		void source_pane::resizeEvent(QResizeEvent* const aEvent)
		{
			QPlainTextEdit::resizeEvent(aEvent);
			const auto area = contentsRect();
			Gutter->setGeometry(
				QRect{ area.left(), area.top(), gutter_width(), area.height() });
		}

		void source_pane::changeEvent(QEvent* const aEvent)
		{
			QPlainTextEdit::changeEvent(aEvent);
			// A different font is a different digit width, and the code beside it
			// has to move by exactly the same amount.
			if (aEvent != nullptr && (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::DevicePixelRatioChange)) {
				refresh_gutter_width();
			}
			// A style change re-polishes the widget, which is the one path
			// that can clear the viewport fill again (BPR-0475).
			if (aEvent != nullptr && aEvent->type() == QEvent::StyleChange) {
				restore_viewport_fill();
			}
		}

		void source_pane::showEvent(QShowEvent* const aEvent)
		{
			// Runs after the style has polished this widget, so a polish that
			// cleared the viewport fill has already happened by now.
			restore_viewport_fill();
			QPlainTextEdit::showEvent(aEvent);
		}

		void source_pane::restore_viewport_fill()
		{
			// QWindows11Style clears the viewport's autoFillBackground during
			// polish so the style can paint its own panel fill, and
			// QPalette::Base is then never painted at all — measured as Base
			// at 0.0% of the viewport grab while every palette, pane and
			// viewport alike, still reported Base correctly (BPR-0475). The
			// applied appearance profile owns the code background, so the Qt
			// default is restored whenever a polish removed it; the guard
			// keeps this a no-op under styles that leave the default alone.
			if (!viewport()->autoFillBackground()) {
				viewport()->setAutoFillBackground(true);
			}
		}

		void source_pane::paint_line_numbers(QPaintEvent* const aEvent)
		{
			auto painter = QPainter{ Gutter };
			painter.fillRect(aEvent->rect(),
							 GutterBackground.isValid()
								 ? GutterBackground
								 : palette().color(QPalette::Window));
			auto block = firstVisibleBlock();
			if (!block.isValid()) {
				return;
			}
			painter.setPen(GutterText.isValid()
							   ? GutterText
							   : palette().color(QPalette::WindowText));
			const auto headerHeight = UnifiedRows != nullptr ? viewport()->geometry().top() - contentsRect().top() : 0;
			const auto numbers      = Gutter->width() - marker_column_width();
			if (UnifiedRows != nullptr) {
				const auto column = numbers / 2;
				painter.drawText(0, 0, column - GutterPadding, headerHeight, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("base"));
				painter.drawText(column, 0, column - GutterPadding, headerHeight, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("head"));
				painter.setClipRect(QRect{ 0, headerHeight, Gutter->width(), Gutter->height() - headerHeight });
			}
			auto top = blockBoundingGeometry(block)
						   .translated(contentOffset())
						   .top()
				+ headerHeight;
			const auto limit = static_cast<qreal>(aEvent->rect().bottom());
			const auto right = numbers - GutterPadding;
			// Only the blocks the exposed rect can show, so the work follows the
			// visible rows rather than the file's length.
			while (block.isValid() && top <= limit) {
				const auto height = blockBoundingRect(block).height();
				if (block.isVisible()
					&& top + height >= static_cast<qreal>(aEvent->rect().top())) {
					// One-based on screen, zero-based inside: RFC-BPR-0004 fixes
					// that split, and each pane numbers its own document rather
					// than being renumbered to match the other side.
					if (UnifiedRows != nullptr) {
						const auto index = static_cast<std::size_t>(block.blockNumber());
						if (index < UnifiedRows->size()) {
							const auto& row  = (*UnifiedRows)[index];
							const auto  fill = !row.Head ? RemovedColor : !row.Base ? AddedColor
																					: QColor{ };
							if (fill.isValid()) {
								painter.fillRect(QRectF{ 0.0, top, static_cast<qreal>(Gutter->width()), height }, fill);
							}
							const auto contains = [](const std::optional<core::line_span> aSpan, const std::optional<std::uint32_t> aLine) {
								return aSpan && aLine && *aLine >= aSpan->Begin && *aLine < aSpan->End;
							};
							if (contains(SelectedBase, row.Base) || contains(SelectedHead, row.Head)) {
								auto wash = palette().color(QPalette::Highlight);
								wash.setAlpha(core::selection_wash_alpha);
								painter.fillRect(QRectF{ 0.0, top, static_cast<qreal>(Gutter->width()), height }, wash);
							}
							const auto columnWidth = numbers / 2;
							for (const auto side : { core::diff_side::left, core::diff_side::right }) {
								const auto number = side == core::diff_side::left ? row.Base : row.Head;
								if (number) {
									painter.drawText(side == core::diff_side::left ? 0 : columnWidth, qRound(top), columnWidth - GutterPadding,
													 qRound(height), Qt::AlignRight | Qt::AlignVCenter, QString::number(*number + 1U));
									++PaintedLineNumbers;
								}
							}
						}
					} else {
						painter.drawText(0,
										 qRound(top),
										 right,
										 qRound(height),
										 Qt::AlignRight | Qt::AlignVCenter,
										 QString::number(block.blockNumber() + 1));
						++PaintedLineNumbers;
					}
				}
				top += height;
				block = block.next();
			}
			paint_markers(painter);
		}

		void source_pane::paint_markers(QPainter& aPainter)
		{
			// [[BPR-1290]]. A thread's marker: its first post's label mark, a
			// plain square, or a dashed one for my draft; a blue dot when it is
			// new, faded when resolved, ringed amber while it is open.
			if (marker_column_width() == 0) {
				return;
			}
			aPainter.save();
			aPainter.setClipping(false);
			aPainter.setRenderHint(QPainter::Antialiasing, true);
			const auto panel  = palette().color(QPalette::Active, QPalette::Base);
			const auto ink    = palette().color(QPalette::Active, QPalette::Text);
			const auto faint  = palette().color(QPalette::Active, QPalette::PlaceholderText);
			const auto gutter = GutterBackground.isValid() ? GutterBackground : palette().color(QPalette::Window);
			for (const auto& mark : Marks) {
				const auto box = marker_rect(mark);
				if (box.isEmpty() || box.bottom() < 0 || box.top() > Gutter->height()) {
					continue;
				}
				if (mark.Writing) {
					// [[BPR-1212]]. The bracket: the draft's lines, joined to
					// its card at the gutter's edge.
					const auto row    = line_row_rect(mark.Line);
					const auto header = UnifiedRows != nullptr ? viewport()->geometry().top() - contentsRect().top() : 0;
					const auto write  = palette().color(QPalette::Active, QPalette::Highlight);
					auto       wash   = write;
					wash.setAlphaF(0.16F);
					aPainter.fillRect(QRect{ 0, row.top() + header, Gutter->width(), row.height() }, wash);
					auto bracket = QPainterPath{ };
					bracket.addRoundedRect(QRectF{ static_cast<qreal>(Gutter->width() - 3), static_cast<qreal>(row.top() + header + 2), 3.0, static_cast<qreal>(std::max(1, row.height() - 4)) }, 1.5, 1.5);
					aPainter.fillPath(bracket, write);
				} else if (mark.Open) {
					const auto row    = line_row_rect(mark.Line);
					const auto header = UnifiedRows != nullptr ? viewport()->geometry().top() - contentsRect().top() : 0;
					auto       wash   = ThreadMarkColor;
					wash.setAlphaF(0.18F);
					aPainter.fillRect(QRect{ 0, row.top() + header, Gutter->width(), row.height() }, wash);
					aPainter.fillRect(QRect{ 0, row.top() + header, 3, row.height() }, ThreadMarkColor);
				}
				aPainter.setOpacity(mark.Resolved && !mark.Open ? 0.5 : 1.0);
				if (mark.Open && !mark.Writing) {
					aPainter.setPen(QPen{ ThreadMarkColor, 2.0 });
					aPainter.setBrush(panel);
					aPainter.drawRoundedRect(QRectF{ box }.adjusted(0.0, 0.0, 0.0, 0.0), 5.0, 5.0);
				}
				const auto face = QRectF{ box }.adjusted(2.0, 2.0, -2.0, -2.0);
				if (mark.Label.has_value()) {
					paint_feedback_mark(aPainter, face, *mark.Label, mark.Emphasis,
										feedback_mark_colours{ .Tint = MarkTheme.Tints[static_cast<std::size_t>(*mark.Label)], .Panel = panel, .Ink = ink, .Style = MarkTheme.Style });
				} else if (mark.Draft) {
					aPainter.setPen(QPen{ mark.Writing ? palette().color(QPalette::Active, QPalette::Highlight) : faint, 1.5, Qt::DashLine });
					aPainter.setBrush(Qt::NoBrush);
					aPainter.drawRoundedRect(face.adjusted(1.0, 1.0, -1.0, -1.0), 3.0, 3.0);
				} else {
					auto fill = faint;
					fill.setAlphaF(0.18F);
					aPainter.setPen(QPen{ faint, 1.5 });
					aPainter.setBrush(fill);
					aPainter.drawRoundedRect(face.adjusted(1.5, 1.5, -1.5, -1.5), 3.0, 3.0);
				}
				if (mark.Kind == diff_line_mark_kind::new_thread) {
					aPainter.setOpacity(1.0);
					aPainter.setPen(QPen{ gutter, 1.5 });
					aPainter.setBrush(NewThreadMarkColor);
					aPainter.drawEllipse(QPointF{ box.right() - 1.0, box.top() + 2.0 }, 3.0, 3.0);
				}
			}
			// [[BPR-1212]]. The "+" under the pointer: a blue square, a white cross.
			if (const auto add = add_rect(); !add.isEmpty()) {
				aPainter.setOpacity(1.0);
				const auto write = palette().color(QPalette::Active, QPalette::Highlight);
				const auto face  = QRectF{ add }.adjusted(1.0, 1.0, -1.0, -1.0);
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(write);
				aPainter.drawRoundedRect(face, 3.0, 3.0);
				aPainter.setPen(QPen{ palette().color(QPalette::Active, QPalette::HighlightedText), 1.6, Qt::SolidLine, Qt::RoundCap });
				const auto centre = face.center();
				const auto arm    = face.width() * 0.28;
				aPainter.drawLine(QPointF{ centre.x() - arm, centre.y() }, QPointF{ centre.x() + arm, centre.y() });
				aPainter.drawLine(QPointF{ centre.x(), centre.y() - arm }, QPointF{ centre.x(), centre.y() + arm });
			}
			aPainter.restore();
		}

		[[nodiscard]] auto pane(QPlainTextEdit* const aPane) noexcept
			-> source_pane*
		{
			return static_cast<source_pane*>(aPane);
		}

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

		[[nodiscard]] auto text_document(
			const std::optional<core::decoded_text_document>& aDocument) -> QString
		{
			if (!aDocument.has_value()) {
				return { };
			}
			if (
				aDocument->Bytes.size()
				> static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
				return { };
			}
			return QString::fromUtf8(
				reinterpret_cast<const char*>(aDocument->Bytes.data()),
				static_cast<qsizetype>(aDocument->Bytes.size()));
		}

		/// [[BPR-1264]]. Above this many lines on the two sides together a
		/// publication fills its documents over several GUI turns. Below it the
		/// single replacement is a few milliseconds on the slowest station.
		inline constexpr std::size_t staged_publication_lines = 4096U;
		/// Lines appended per batch, and the time after which a turn yields.
		inline constexpr int          staged_publication_batch_lines = 256;
		inline constexpr std::int64_t staged_publication_turn_ns     = 4'000'000;

		/// \brief One side's detached document and how far it is filled.
		struct staged_document
		{
			QPointer<QTextDocument> Document;
			QTextCursor             End;
			QString                 Text;
			qsizetype               Position = 0;

			void start(QPlainTextEdit& aPane, const std::optional<core::decoded_text_document>& aSource)
			{
				Text     = text_document(aSource);
				Document = new QTextDocument{ &aPane };
				// A read-only pane keeps no history, and recording one per batch
				// would cost time and memory for nothing.
				Document->setUndoRedoEnabled(false);
				End = QTextCursor{ Document.data() };
			}
			[[nodiscard]] auto complete() const noexcept -> bool
			{
				return Position >= Text.size();
			}
			/// Appends up to `aLines` lines. A batch ends on a line break, so the
			/// next one starts a fresh block, exactly as one replacement would.
			void append(const int aLines)
			{
				auto end = Position;
				for (auto line = 0; line < aLines && end < Text.size(); ++line) {
					const auto next = Text.indexOf(QLatin1Char('\n'), end);
					end             = next < 0 ? Text.size() : next + 1;
				}
				End.insertText(Text.sliced(Position, end - Position));
				Position = end;
			}
			void discard()
			{
				delete Document.data();
			}
		};

		/// \brief Gives a pane a filled document, keeping its typography.
		///
		/// \details QPlainTextEdit keeps its font, tab stop and margin on the
		/// document, so a new one is given the replaced one's, and the plain
		/// text layout the pane requires.
		void adopt_document(source_pane& aPane, QTextDocument& aDocument)
		{
			auto* const previous = aPane.document();
			aDocument.setDefaultFont(previous->defaultFont());
			aDocument.setDefaultTextOption(previous->defaultTextOption());
			aDocument.setDocumentMargin(previous->documentMargin());
			aDocument.setUndoRedoEnabled(previous->isUndoRedoEnabled());
			aDocument.setDocumentLayout(new QPlainTextDocumentLayout{ &aDocument });
			// The pane's first document belongs to its text control, which
			// deletes it on replacement; one adopted earlier is the pane's child.
			const auto adopted = previous->parent() == &aPane;
			aPane.setDocument(&aDocument);
			if (adopted) {
				delete previous;
			}
			// Replacing the document signals no block count change, and a
			// staged file would otherwise keep the empty pane's gutter
			// ([[BPR-1335]]).
			aPane.refresh_gutter_width();
		}

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
			return QString::fromUtf8(
				reinterpret_cast<const char*>(bytes),
				static_cast<qsizetype>(span.End - span.Begin));
		}

		[[nodiscard]] auto line_text(
			const std::optional<core::decoded_text_document>& aDocument,
			const std::optional<core::source_line_ref>&       aLine) -> QString
		{
			return aLine.has_value()
				? line_text_at(aDocument, aLine->Line)
				: QStringLiteral("(no source line)");
		}

		/// Lines of one span the detail pane shows before saying how many remain.
		inline constexpr std::uint32_t detail_span_lines = 64U;

		[[nodiscard]] auto span_text(
			const std::optional<core::decoded_text_document>& aDocument,
			const core::line_span                             aSpan) -> QString
		{
			if (
				!aDocument.has_value() || aSpan.Begin >= aSpan.End
				|| aSpan.End > aDocument->Lines.size()) {
				return QStringLiteral("(no source lines)");
			}
			// [[BPR-1264]]. The detail pane is a compact summary. An approximate
			// alignment's span can be most of the file, and copying all of it
			// here cost as much as filling a source pane.
			const auto shown  = std::min(aSpan.End, aSpan.Begin + detail_span_lines);
			auto       result = QString{ };
			for (auto line = aSpan.Begin; line < shown; ++line) {
				if (!result.isEmpty()) {
					result += QLatin1Char('\n');
				}
				result += line_text_at(aDocument, line);
			}
			if (shown < aSpan.End) {
				result += QStringLiteral("\n(%1 more lines)").arg(aSpan.End - shown);
			}
			return result;
		}

		[[nodiscard]] auto detail_side_text(
			const std::optional<core::decoded_text_document>& aDocument,
			const std::optional<core::source_line_ref>&       aLine,
			const core::line_detail_input&                    aDetail,
			const core::diff_side                             aSide) -> QString
		{
			if (aLine.has_value()) {
				return line_text(aDocument, aLine);
			}
			if (
				aDetail.OpposingSpan.has_value()
				&& aDetail.OpposingSpan->Side == aSide) {
				return span_text(aDocument, aDetail.OpposingSpan->Lines);
			}
			if (
				aDetail.OpposingBoundary.has_value()
				&& aDetail.OpposingBoundary->Side == aSide) {
				return QStringLiteral("(insertion boundary before line %1)")
					.arg(aDetail.OpposingBoundary->Boundary + 1U);
			}
			return QStringLiteral("(no source line)");
		}

		[[nodiscard]] auto reason_text(const core::diff_reason_code aReason)
			-> QString
		{
			switch (aReason) {
			case core::diff_reason_code::none:
				return QStringLiteral("Diff ready");
			case core::diff_reason_code::binary_file:
				return QStringLiteral("Binary file comparison");
			case core::diff_reason_code::missing_left_blob:
			case core::diff_reason_code::missing_right_blob:
				return QStringLiteral("A selected file version is unavailable");
			case core::diff_reason_code::decode_failure:
				return QStringLiteral("File text could not be decoded");
			case core::diff_reason_code::materialization_limit:
				return QStringLiteral("File exceeds the comparison materialization limit");
			case core::diff_reason_code::compute_limit:
				return QStringLiteral("Comparison unavailable — file exceeds the comparison work limit");
			case core::diff_reason_code::generated_file_unsupported:
				return QStringLiteral("Generated file comparison is unsupported");
			case core::diff_reason_code::cancelled:
				return QStringLiteral("Comparison cancelled");
			case core::diff_reason_code::stale:
				return QStringLiteral("Older comparison discarded");
			case core::diff_reason_code::invalid_alignment:
			case core::diff_reason_code::host_failure:
				return QStringLiteral("Comparison unavailable");
			}
			return QStringLiteral("Comparison unavailable");
		}

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
			-> core::line_span
		{
			const auto height = aPane.viewport()->height();
			const auto top    = aPane.cursorForPosition(QPoint{ 0, 0 })
									.blockNumber();
			const auto bottom = aPane.cursorForPosition(QPoint{ 0, height })
									.blockNumber();
			const auto span   = std::max(bottom - top, 0) + 1;
			const auto begin  = std::max(top - span, 0);
			const auto end    = bottom + span + 1;
			return core::line_span{
				.Begin = static_cast<std::uint32_t>(begin),
				.End   = static_cast<std::uint32_t>(std::max(end, 0)),
			};
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
	} // namespace

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

	diff_workbench::diff_workbench(QWidget* const aParent)
		: QWidget{ aParent }
		, UnifiedPresentation{ std::make_unique<unified_presentation>() }
	{
		setObjectName(QStringLiteral("diffWorkbench"));
		setAccessibleName(QStringLiteral("File comparison workbench"));
		setAccessibleDescription(QStringLiteral(
			"Linked natural-layout source panes, change correspondence map, and selected-change detail"));

		auto* const root = new QVBoxLayout{ this };
		root->setContentsMargins(0, 0, 0, 0);
		root->setSpacing(6);
		Status = new QLabel{ QStringLiteral("Select a file comparison"), this };
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setObjectName(QStringLiteral("diffWorkbenchStatus"));
		Status->setAccessibleName(QStringLiteral("Comparison status"));
		Status->setAccessibleDescription(QStringLiteral(
			"Loading, failure, and semantic change count for the selected comparison"));
		Status->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
		root->addWidget(Status);

		Vertical = new QSplitter{ Qt::Vertical, this };
		Vertical->setObjectName(QStringLiteral("diffWorkbenchVerticalSplitter"));
		Vertical->setChildrenCollapsible(false);
		auto* const top       = new QWidget{ Vertical };
		auto* const topLayout = new QHBoxLayout{ top };
		topLayout->setContentsMargins(0, 0, 0, 0);
		topLayout->setSpacing(0);
		Comparison = new diff_comparison_splitter{ top };
		Comparison->setObjectName(QStringLiteral("diffComparisonSplitter"));
		Comparison->setAccessibleName(QStringLiteral("Linked source comparison"));
		Comparison->setChildrenCollapsible(false);
		Left = new source_pane{ Comparison };
		Left->setObjectName(QStringLiteral("diffLeftSource"));
		Left->setAccessibleName(QStringLiteral("Earlier source"));
		Left->setAccessibleDescription(QStringLiteral("Read-only earlier file version"));
		Right = new source_pane{ Comparison };
		Right->setObjectName(QStringLiteral("diffRightSource"));
		Right->setAccessibleName(QStringLiteral("Later source"));
		Right->setAccessibleDescription(QStringLiteral("Read-only later file version"));
		for (auto* const source : { Left, Right }) {
			source->setReadOnly(true);
			source->setLineWrapMode(QPlainTextEdit::NoWrap);
			// [[BPR-1291]]. Lets the pane scroll until its last line is at the
			// top. Without it Qt stops a pane with its last line at the bottom.
			source->setCenterOnScroll(true);
			source->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			source->setFocusPolicy(Qt::StrongFocus);
			source->viewport()->installEventFilter(this);
		}
		Comparison->addWidget(Left);
		Comparison->addWidget(Right);
		Comparison->setSizes({ 500, 500 });
		SharedScroll = new thread_tick_scroll_bar{ Qt::Vertical, top };
		SharedScroll->setObjectName(QStringLiteral("diffSharedScrollBar"));
		SharedScroll->setAccessibleName(QStringLiteral("Shared comparison position"));
		SharedScroll->setAccessibleDescription(QStringLiteral(
			"Moves both source panes through semantic comparison space; Shift-click a thread's tick to go to that thread"));
		SharedScroll->setFocusPolicy(Qt::StrongFocus);
		topLayout->addWidget(Comparison, 1);
		topLayout->addWidget(SharedScroll);
		UnifiedMap = new one_pane_diff_link_map{ top };
		Unified    = new source_pane{ top };
		Unified->setVerticalScrollBar(new thread_tick_scroll_bar{ Qt::Vertical, Unified });
		Unified->setObjectName(QStringLiteral("diffUnifiedSource"));
		Unified->setAccessibleName(QStringLiteral("Unified source comparison"));
		Unified->setAccessibleDescription(QStringLiteral("Base and head line numbers; removed lines followed by added lines in each change"));
		Unified->setReadOnly(true);
		Unified->setLineWrapMode(QPlainTextEdit::NoWrap);
		Unified->setCenterOnScroll(true);
		Unified->setFocusPolicy(Qt::StrongFocus);
		pane(Unified)->set_unified_rows(&UnifiedPresentation->Rows);
		Unified->viewport()->installEventFilter(this);
		Unified->installEventFilter(this);
		UnifiedMap->installEventFilter(this);
		UnifiedMap->setFocusProxy(Unified);
		topLayout->addWidget(UnifiedMap);
		topLayout->addWidget(Unified, 1);
		UnifiedMap->hide();
		Unified->hide();
		UnifiedMap->set_selection_sink([this](const diff_link_map_identity& aIdentity) { select_link(aIdentity, true); });
		connect(Unified->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
			if (OnePaneEnabled) {
				update_unified_paint();
				++Statistics.FrameApplications;
				if (FrameObserver) {
					FrameObserver();
				}
			}
		});

		Detail = new QPlainTextEdit{ Vertical };
		Detail->setObjectName(QStringLiteral("diffSelectedChangeDetail"));
		Detail->setAccessibleName(QStringLiteral("Selected change detail"));
		Detail->setAccessibleDescription(QStringLiteral(
			"Full-width line-over-line detail for the semantic selection"));
		Detail->setReadOnly(true);
		Detail->setLineWrapMode(QPlainTextEdit::NoWrap);
		Detail->setPlainText(QStringLiteral("No change selected"));
		Detail->setMaximumHeight(96);
		// [[BPR-1287]]. The panes share their splitter slot with the idle
		// direction, one shown at a time, so the splitter keeps its two
		// children and the review details still open below either.
		auto* const slot = new QWidget{ Vertical };
		PanesSlot        = new QStackedLayout{ slot };
		PanesSlot->setContentsMargins(0, 0, 0, 0);
		PanesSlot->addWidget(top);
		PanesTop = top;
		Vertical->addWidget(slot);
		Vertical->addWidget(Detail);
		Vertical->setStretchFactor(0, 4);
		Vertical->setStretchFactor(1, 1);
		Vertical->setSizes({ 500, 150 });
		root->addWidget(Vertical, 1);

		// [[BPR-1287]]. What the area says while no file comparison is chosen,
		// in place of two empty panes and a bare status.
		IdlePage = new QWidget{ slot };
		IdlePage->setObjectName(QStringLiteral("diffWorkbenchIdle"));
		IdlePage->setAccessibleName(QStringLiteral("No file open"));
		auto* const idleColumn = new QVBoxLayout{ IdlePage };
		idleColumn->setContentsMargins(24, 24, 24, 24);
		idleColumn->setSpacing(8);
		idleColumn->addStretch(2);
		// Interface text, not code: the workbench's own font is the code
		// typography, which the panes inherit.
		auto interfaceFont = QApplication::font();
		interfaceFont.setFamilies(interfaceFont.families());
		interfaceFont.setPointSizeF(interfaceFont.pointSizeF());
		interfaceFont.setStyleHint(interfaceFont.styleHint());
		interfaceFont.setWeight(interfaceFont.weight());
		IdlePage->setFont(interfaceFont);
		IdleTitle = new QLabel{ IdlePage };
		IdleTitle->setObjectName(QStringLiteral("diffWorkbenchIdleTitle"));
		IdleTitle->setTextFormat(Qt::PlainText);
		IdleTitle->setAlignment(Qt::AlignHCenter);
		auto titleFont = interfaceFont;
		titleFont.setPointSizeF(interfaceFont.pointSizeF() * 1.35);
		titleFont.setWeight(QFont::DemiBold);
		IdleTitle->setFont(titleFont);
		IdleDetail = new QLabel{ IdlePage };
		IdleDetail->setObjectName(QStringLiteral("diffWorkbenchIdleDetail"));
		IdleDetail->setTextFormat(Qt::PlainText);
		IdleDetail->setAlignment(Qt::AlignHCenter);
		IdleDetail->setWordWrap(true);
		IdleDetail->setForegroundRole(QPalette::PlaceholderText);
		idleColumn->addWidget(IdleTitle);
		idleColumn->addWidget(IdleDetail);
		auto* const idleActions = new QHBoxLayout{ };
		idleActions->setSpacing(8);
		idleActions->addStretch(1);
		IdleOpenFirst = new QPushButton{ QStringLiteral("Open first file"), IdlePage };
		IdleOpenFirst->setObjectName(QStringLiteral("diffWorkbenchIdleOpenFirst"));
		IdleOpenFirst->setDefault(true);
		IdleShowFiles = new QPushButton{ QStringLiteral("Show files"), IdlePage };
		IdleShowFiles->setObjectName(QStringLiteral("diffWorkbenchIdleShowFiles"));
		idleActions->addWidget(IdleOpenFirst);
		idleActions->addWidget(IdleShowFiles);
		idleActions->addStretch(1);
		idleColumn->addLayout(idleActions);
		idleColumn->addStretch(3);
		PanesSlot->addWidget(IdlePage);
		connect(IdleOpenFirst, &QPushButton::clicked, this, [this] {
			if (IdleDirection.OpenFirst) {
				IdleDirection.OpenFirst();
			}
		});
		connect(IdleShowFiles, &QPushButton::clicked, this, [this] {
			if (IdleDirection.ShowFiles) {
				IdleDirection.ShowFiles();
			}
		});
		set_idle_direction({ .Title = QStringLiteral("No file open"), .Detail = QStringLiteral("Choose a file to see its changes.") });
		set_idle(true);

		LinkMap = Comparison->link_map();
		LinkMap->set_selection_announcements_enabled(false);
		Controller = new comparison_scroll_controller{ *SharedScroll, { }, this };
		Controller->bind_input_surface(
			comparison_scroll_receiver::left_pane,
			*Left->viewport(),
			Left->horizontalScrollBar());
		Controller->bind_input_surface(
			comparison_scroll_receiver::right_pane,
			*Right->viewport(),
			Right->horizontalScrollBar());
		Controller->bind_input_surface(
			comparison_scroll_receiver::link_map, *LinkMap);
		Controller->bind_key_surface(
			comparison_scroll_receiver::left_pane, *Left);
		Controller->bind_key_surface(
			comparison_scroll_receiver::right_pane, *Right);
		Controller->bind_key_surface(
			comparison_scroll_receiver::link_map, *LinkMap);
		Controller->set_frame_sink(
			[this](const comparison_scroll_frame& aFrame) { apply_frame(aFrame); });
		Controller->set_repaint_sink([this] { LinkMap->update(); });
		LinkMap->set_selection_sink(
			[this](const diff_link_map_identity& aIdentity) {
				select_link(aIdentity, false);
			});
		LinkMap->set_reveal_sink(
			[this](const diff_link_map_identity& aIdentity) {
				select_link(aIdentity, true);
			});

		QWidget::setTabOrder(Left, LinkMap);
		QWidget::setTabOrder(LinkMap, Right);
		QWidget::setTabOrder(Right, SharedScroll);
		QWidget::setTabOrder(SharedScroll, Detail);
	}

	diff_workbench::~diff_workbench() = default;

	struct diff_workbench::staged_publication
	{
		application::diff_publication Publication;
		staged_document               Left;
		staged_document               Right;
	};

	void diff_workbench::publish(
		const application::diff_publication& aPublication)
	{
		if (
			!core::valid(aPublication.Context) || !aPublication.Result
			|| aPublication.Result->Alignment.Key != aPublication.Context.Key
			|| !CurrentContext.has_value()
			|| *CurrentContext != aPublication.Context) {
			++Statistics.RejectedStaleInput;
			return;
		}
		if (!aPublication.Index || !aPublication.Index->matches_result(*aPublication.Result)) {
			++Statistics.RejectedStaleInput;
			return;
		}
		drop_staged_publication();
		const auto lines = [](const std::optional<core::decoded_text_document>& aDocument) {
			return aDocument.has_value() ? aDocument->Lines.size() : std::size_t{ 0U };
		};
		if (lines(aPublication.Result->LeftDocument) + lines(aPublication.Result->RightDocument) <= staged_publication_lines) {
			adopt_publication(aPublication, nullptr, nullptr);
			return;
		}
		// [[BPR-1264]]. Replacing both panes' text in one turn held the GUI
		// thread for the whole file twice. The documents are filled detached,
		// a bounded batch per turn, and adopted together once complete.
		auto staged         = std::make_shared<staged_publication>();
		staged->Publication = aPublication;
		staged->Left.start(*Left, aPublication.Result->LeftDocument);
		staged->Right.start(*Right, aPublication.Result->RightDocument);
		PendingPublication = std::move(staged);
		++Statistics.StagedPublications;
		queue_publication_continuation();
	}

	void diff_workbench::adopt_publication(const application::diff_publication& aPublication,
										   QTextDocument* const aLeft, QTextDocument* const aRight)
	{
		const auto previousSelection = Interaction.Selection;
		CurrentResult                = aPublication.Result;
		SemanticIndex                = aPublication.Index;
		// Built once here rather than searched per segment per repaint. The
		// move table is keyed by segment identity and is unordered with
		// respect to the segment list, so the alternative is a linear search
		// inside the paint loop.
		MovedSegments.clear();
		for (const auto& move : CurrentResult->Alignment.Moves) {
			MovedSegments.insert(move.SourceDeletion);
			MovedSegments.insert(move.DestinationInsertion);
		}
		++Statistics.SemanticIndexAdoptions;
		Statistics.SemanticIndexEntries
			= SemanticIndex->statistics().IndexEntries;
		// A document's revision does not identify its content ([[BPR-1123]]).
		LeftHeights.clear();
		RightHeights.clear();
		if (aLeft != nullptr && aRight != nullptr) {
			adopt_document(*pane(Left), *aLeft);
			adopt_document(*pane(Right), *aRight);
		} else {
			Left->setPlainText(text_document(CurrentResult->LeftDocument));
			Right->setPlainText(text_document(CurrentResult->RightDocument));
		}
		rebuild_unified_document();
		++Statistics.SemanticQueries;
		static_cast<void>(core::reconcile_diff_interaction(Interaction, *SemanticIndex));
		// [[BPR-1328]]. A change command that crossed into this file enters
		// its change order from the end the core decided.
		if (const auto entry = std::exchange(PendingChangeEntry, std::nullopt);
			entry.has_value() && entry->File == CurrentResult->Alignment.Key.File
			&& !CurrentResult->Alignment.Groups.empty()) {
			Interaction.Selection.reset();
			++Statistics.SemanticQueries;
			static_cast<void>(core::transition(
				Interaction,
				*SemanticIndex,
				core::navigate_diff_change_intent{
					.Direction = entry->Direction,
					.Policy    = core::change_navigation_policy::stop,
				}));
		}
		if (!Interaction.Selection.has_value()) {
			if (!CurrentResult->Alignment.Groups.empty()) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_group_intent{
						.Group = CurrentResult->Alignment.Groups.front().Id,
					}));
			} else if (CurrentResult->Alignment.LeftLineCount != 0U) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_line_intent{
						.Line = {
							.Comparison = CurrentResult->Alignment.Digest,
							.Side       = core::diff_side::left,
							.Line       = 0U,
						},
					}));
			} else if (CurrentResult->Alignment.RightLineCount != 0U) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_line_intent{
						.Line = {
							.Comparison = CurrentResult->Alignment.Digest,
							.Side       = core::diff_side::right,
							.Line       = 0U,
						},
					}));
			}
		}
		++Statistics.Publications;
		// [[BPR-0719]]: a work limit degrades the alignment; the status says so.
		const auto status = CurrentResult->Alignment.Content == core::diff_content_state::text
			? (CurrentResult->AlignmentApproximate
				   ? QStringLiteral("Diff ready — %1 semantic changes (approximate alignment)")
				   : QStringLiteral("Diff ready — %1 semantic changes"))
				  .arg(CurrentResult->Alignment.Groups.size())
			: reason_text(CurrentResult->Alignment.Diagnostic.Code);
		// [[BPR-1132]]. A ready, exact alignment is routine; a header that
		// shows the status only when it says more leaves it unsaid.
		update_status(status + (aPublication.CacheHit ? QStringLiteral(" (cached)") : QString{ }),
					  CurrentResult->Alignment.Content == core::diff_content_state::text && !CurrentResult->AlignmentApproximate);
		update_detail();
		update_selection_paint();
		schedule_geometry_rebuild(
			measured_geometry_change::accepted_diff,
			diff_geometry_rebuild_trigger::accepted_diff);
		if (Interaction.Selection != previousSelection) {
			emit_selection();
		}
	}

	void diff_workbench::fail(
		const application::diff_failure_publication& aFailure)
	{
		if (!CurrentContext.has_value() || *CurrentContext != aFailure.Context) {
			++Statistics.RejectedStaleInput;
			return;
		}
		++Statistics.Failures;
		set_idle(false);
		clear_projection(reason_text(aFailure.Reason));
		CurrentContext.reset();
	}

	void diff_workbench::invalidate(
		const core::pull_request_authority& aAuthority)
	{
		if (
			!CurrentContext.has_value()
			|| CurrentContext->Key.Authority != aAuthority) {
			return;
		}
		++Statistics.Invalidations;
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		set_idle(false);
		clear_projection(QStringLiteral("Comparison invalidated"));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_loading(const core::diff_request_context& aContext)
	{
		if (!core::valid(aContext)) {
			return;
		}
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext          = aContext;
		set_idle(false);
		clear_projection(QStringLiteral("Loading selected file comparison…"));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::clear(QString aStatus)
	{
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		// Only the plain "nothing chosen" is idle; a reason the host gives is
		// shown as before.
		const auto idle = aStatus.isEmpty();
		set_idle(idle);
		clear_projection(idle ? QStringLiteral("Select a file comparison") : std::move(aStatus));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_selection_sink(selection_sink aSink)
	{
		SelectionSink = std::move(aSink);
		emit_selection();
	}
	void diff_workbench::set_reading_gesture_sink(std::function<void()> aSink)
	{
		ReadingGestureSink = std::move(aSink);
	}

	void diff_workbench::set_selection_observer(selection_observer aObserver)
	{
		SelectionObserver = std::move(aObserver);
		emit_selection();
	}

	auto diff_workbench::reveal_conversation_line(
		const core::diff_side aSide,
		const std::uint32_t   aLine) -> bool
	{
		if (!CurrentResult) {
			return false;
		}
		const auto requested = core::source_line_ref{
			.Comparison = CurrentResult->Alignment.Digest,
			.Side       = aSide,
			.Line       = aLine,
		};
		const auto previous = Interaction.Selection;
		if (!select_line(aSide, aLine)) {
			return false;
		}
		ConversationAnchor = requested;
		static_cast<void>(reveal_exact_line(*Interaction.Selection, requested));
		update_diff_paint();
		if (Interaction.Selection == previous) {
			emit_selection_observer(true);
		}
		return true;
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
		if (OnePaneEnabled) {
			const auto index = unified_row_for(aSide, aLine);
			if (!CurrentResult || !index) {
				return { };
			}
			const auto row = pane(Unified)->line_row_rect(*index);
			return QRect{ Unified->viewport()->mapTo(this, row.topLeft()), row.size() };
		}
		auto* const source = pane(aSide == core::diff_side::left ? Left : Right);
		const auto  row    = source->line_row_rect(aLine);
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
	}

	auto diff_workbench::thread_markers() const noexcept -> bool
	{
		return ThreadMarkers;
	}

	void diff_workbench::set_mark_theme(const feedback_mark_theme& aTheme)
	{
		for (auto* const source : { Left, Right, Unified }) {
			pane(source)->set_mark_theme(aTheme);
		}
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

	void diff_workbench::set_line_add_dispatch(std::function<void(std::uint32_t)> aDispatch)
	{
		// New comments go on the head side only (core::supported_comment_target).
		pane(Right)->set_add_dispatch(std::move(aDispatch));
	}

	auto diff_workbench::line_add_rect() const -> QRect
	{
		const auto rect = pane(Right)->add_rect();
		return rect.isEmpty() ? rect : rect.translated(pane(Right)->contentsRect().topLeft());
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
										  aEvent.buttons(), aEvent.modifiers(), aEvent.phase(), aEvent.inverted(), aEvent.source() };
		if (OnePaneEnabled) {
			const auto before = Unified->verticalScrollBar()->value();
			QCoreApplication::sendEvent(target, &wheel);
			return Unified->verticalScrollBar()->value() != before;
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

	auto diff_workbench::detail_pane() const noexcept -> QPlainTextEdit*
	{
		return Detail;
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
			= Left->viewport()->mapTo(parent, QPoint{ 0, 0 }).y();
		LinkMap->set_viewport_band(
			static_cast<qreal>(paneTop - mapTop),
			static_cast<qreal>(Left->viewport()->height()));
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
				const auto end   = Unified->document()->findBlock(cursor.selectionEnd() - 1).blockNumber();
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
			last.Line  = static_cast<std::uint32_t>(editor->document()->findBlock(cursor.selectionEnd() - 1).blockNumber());
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
		if (OnePaneEnabled && aWatched == UnifiedMap && aEvent->type() == QEvent::Wheel) {
			QApplication::sendEvent(Unified->viewport(), aEvent);
			return true;
		}
		// [[BPR-1328]]. In the one-pane comparison, as in the two panes, the
		// platform's document start and end go to the top and bottom.
		if (Unified != nullptr && aWatched == Unified && aEvent->type() == QEvent::KeyPress) {
			if (const auto edge = comparison_scroll_edge_for(*static_cast<QKeyEvent*>(aEvent)); edge.has_value()) {
				Unified->verticalScrollBar()->triggerAction(*edge == comparison_scroll_edge::top ? QAbstractSlider::SliderToMinimum : QAbstractSlider::SliderToMaximum);
				aEvent->accept();
				return true;
			}
		}
		if (Unified != nullptr && aWatched == Unified->viewport()) {
			if (aEvent->type() == QEvent::Resize && OnePaneEnabled) {
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
		if (sourceViewport && aEvent->type() == QEvent::Resize) {
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
		// Before the rebuild, because a resize can add or remove a pane's
		// horizontal scrollbar and that changes the band without changing
		// anything the axis knows about.
		declare_link_map_band();
		schedule_geometry_rebuild(
			measured_geometry_change::viewport_or_divider,
			diff_geometry_rebuild_trigger::workbench_resize);
	}

	struct diff_workbench::geometry_measurement
	{
		std::shared_ptr<const core::computed_file_diff> Result;
		plain_text_pane_measurement                     Left;
		plain_text_pane_measurement                     Right;
	};

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
			PendingMeasurement = std::make_shared<geometry_measurement>(geometry_measurement{
				.Result = CurrentResult,
				.Left   = plain_text_pane_measurement{ *Left, CurrentResult->Alignment.LeftLineCount, LeftHeights },
				.Right  = plain_text_pane_measurement{ *Right, CurrentResult->Alignment.RightLineCount, RightHeights },
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
		const auto changes  = AxisCache.pending_changes();
		const auto triggers = PendingGeometryTriggers;
		if (AxisCache.rebuild(CurrentResult->Alignment, *leftResult, *rightResult)
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
			// Put the line nearest the frame's origin at the top, then read the
			// origin back off the same per-line measurement the ribbon
			// endpoints are built from, so the two cannot drift apart
			// (BPR-0208). The pane scrolls by whole lines.
			const auto place = [axis](source_pane& aPane, const core::diff_side aSide, const qreal aOrigin) {
				const auto line = axis->top_line_for_origin(aSide, aOrigin);
				aPane.set_top_line(line);
				const auto top = aSide == core::diff_side::left ? axis->left_line_origin(line) : axis->right_line_origin(line);
				return line == 0U ? 0.0 : top.value_or(aOrigin);
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
			emit_reading_gesture();
			emit_selection_observer(true);
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

	void diff_workbench::enter_next_comparison(
		const core::file_identity               aFile,
		const core::change_navigation_direction aDirection)
	{
		PendingChangeEntry = pending_change_entry{ .File = aFile, .Direction = aDirection };
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
		const auto measured = std::ranges::find(
			axis->segments(), *aSelection.Segment, &measured_alignment_segment::Id);
		if (measured == axis->segments().end()) {
			return false;
		}
		const auto origin = aLine.Side == core::diff_side::left
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
				Status->text() + QStringLiteral(". ")
				+ Detail->accessibleDescription());
		}
	}

	void diff_workbench::update_detail()
	{
		CurrentDetail.reset();
		if (!CurrentResult || !SemanticIndex
			|| !Interaction.Selection.has_value()) {
			Detail->setPlainText(QStringLiteral("No change selected"));
			return;
		}
		++Statistics.SemanticQueries;
		CurrentDetail = core::detail_for_selection(
			*SemanticIndex, *Interaction.Selection);
		const auto& detail   = *CurrentDetail;
		const auto  leftText = detail_side_text(
			CurrentResult->LeftDocument,
			detail.LeftLine,
			detail,
			core::diff_side::left);
		const auto rightText = detail_side_text(
			CurrentResult->RightDocument,
			detail.RightLine,
			detail,
			core::diff_side::right);
		auto heading = QStringLiteral("Selected line %1")
						   .arg(Interaction.Selection->PreferredLine.Line + 1U);
		if (Interaction.Selection->Group.has_value()) {
			const auto index = SemanticIndex->group_index(
				*Interaction.Selection->Group);
			if (index.has_value()) {
				heading = QStringLiteral("Change %1 of %2")
							  .arg(*index + 1U)
							  .arg(CurrentResult->Alignment.Groups.size());
			}
		}
		Detail->setPlainText(
			heading + QStringLiteral("\n\nEarlier\n") + leftText
			+ QStringLiteral("\n\nLater\n") + rightText);
		Detail->setAccessibleDescription(heading);
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
		const auto top      = Unified->viewport()->mapTo(this, QPoint{ }).y() - UnifiedMap->mapTo(this, QPoint{ }).y();
		const auto viewport = QRectF{ 0.0, static_cast<qreal>(top), static_cast<qreal>(UnifiedMap->width()), static_cast<qreal>(Unified->viewport()->height()) };
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
					marks.push_back(std::move(unified));
				}
			}
		}
		pane(Unified)->set_line_marks(std::move(marks));
		pane(Unified)->set_conversation_anchor(ConversationAnchor ? unified_row_for(ConversationAnchor->Side, ConversationAnchor->Line) : std::nullopt);
		update_unified_link_map();
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
		// [[BPR-1107]]. When the panes draw white space, from the same
		// typography record the preview reads.
		for (auto* const target : { Left, Right, Unified }) {
			pane(target)->set_white_space(aProfile.typography().Code.WhiteSpace);
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
		update_diff_paint();
		return true;
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

			auto emphasis   = QTextEdit::ExtraSelection{ };
			emphasis.cursor = QTextCursor{ block };
			emphasis.cursor.setPosition(block.position() + from);
			emphasis.cursor.setPosition(
				block.position() + to, QTextCursor::KeepAnchor);
			// On a selected row the fill is derived from what that row is
			// actually wearing, so it is a deeper shade of the band rather
			// than a pink patch on a blue one. Everywhere else the accepted
			// fill stays exactly as accepted.
			const auto fill = selected ? derivedFill(aSide) : color;
			emphasis.format.setBackground(fill);
			const auto readable = [this, fill](const QColor& aInk) {
				return to_presentation_color(core::readable_emphasis_foreground(
					to_appearance_color(aInk), to_appearance_color(fill), Colors.Variant));
			};
			if (Colors.Supplied) {
				emphasis.format.setForeground(readable(Colors.Syntax[core::index_of(core::appearance_role::syntax_normal)]));
			}
			aSelections.push_back(emphasis);
			// Keep the exact edit range as one background layer. Foreground-only
			// intersections preserve each classified token's hue and leave source,
			// layout and the unedited part of the token untouched.
			if (Colors.Supplied && block.layout() != nullptr) {
				for (const auto& token : block.layout()->formats()) {
					const auto begin = std::max(from, token.start);
					const auto end   = std::min(to, token.start + token.length);
					if (begin >= end || token.format.foreground().style() == Qt::NoBrush) {
						continue;
					}
					auto syntax   = QTextEdit::ExtraSelection{ };
					syntax.cursor = QTextCursor{ block };
					syntax.cursor.setPosition(block.position() + begin);
					syntax.cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
					syntax.format.setForeground(readable(token.format.foreground().color()));
					aSelections.push_back(syntax);
				}
			}
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
		MovedSegments.clear();
		Interaction.Selection.reset();
		ConversationAnchor.reset();
		LeftHeights.clear();
		RightHeights.clear();
		Left->clear();
		Right->clear();
		Detail->setPlainText(QStringLiteral("No change selected"));
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
} // namespace bpr::qt
