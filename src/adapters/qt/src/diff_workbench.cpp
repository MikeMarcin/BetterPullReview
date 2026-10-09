#include "bpr/qt/diff_workbench.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/type_preview.hpp"

#include "line_details_splitter.hpp"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QApplication>
#include <QCursor>
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
#include <QNativeGestureEvent>
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
#include <cstdlib>
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
		/// [[BPR-1335]]. The "+" button's side: GitHub's 20 px, shrunk to fit a
		/// short row but never below 12 px.
		constexpr auto AddButtonSide    = 20;
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
			explicit zoom_pill(QWidget* const aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("diffCodeZoomIndicator"));
				setAttribute(Qt::WA_TransparentForMouseEvents);
				setFocusPolicy(Qt::NoFocus);
				hide();
			}

			void set_zoom(const QString& aPercent, const QString& aSize)
			{
				Percent = aPercent;
				Size    = aSize;
				setAccessibleName(QStringLiteral("Code zoom %1, %2").arg(Percent, Size));
				const auto fonts = pill_fonts();
				const auto width = QFontMetricsF{ fonts.first }.horizontalAdvance(Percent)
					+ gap + QFontMetricsF{ fonts.second }.horizontalAdvance(Size);
				const auto height = std::max(QFontMetricsF{ fonts.first }.height(), QFontMetricsF{ fonts.second }.height());
				resize(static_cast<int>(std::ceil(width + 2.0 * inset_x)), static_cast<int>(std::ceil(height + 2.0 * inset_y)));
				update();
			}

			[[nodiscard]] auto percent_text() const -> const QString& { return Percent; }
			[[nodiscard]] auto size_text() const -> const QString& { return Size; }

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto box  = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				auto       fill = palette().color(QPalette::Window);
				fill.setAlpha(242);
				auto edge = palette().color(QPalette::WindowText);
				edge.setAlpha(48);
				painter.setPen(QPen{ edge, 1.0 });
				painter.setBrush(fill);
				painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
				const auto fonts = pill_fonts();
				const auto text  = box.adjusted(inset_x, 0.0, -inset_x, 0.0);
				painter.setFont(fonts.first);
				painter.setPen(palette().color(QPalette::WindowText));
				painter.drawText(text, Qt::AlignVCenter | Qt::AlignLeft, Percent);
				const auto after = QFontMetricsF{ fonts.first }.horizontalAdvance(Percent) + gap;
				// The window's own text colour for both, not the placeholder
				// grey: the reader zooming may be the reader who needs contrast.
				painter.setFont(fonts.second);
				painter.drawText(text.adjusted(after, 0.0, 0.0, 0.0), Qt::AlignVCenter | Qt::AlignLeft, Size);
			}

		private:
			static constexpr qreal inset_x = 14.0;
			static constexpr qreal inset_y = 6.0;
			static constexpr qreal gap     = 8.0;

			/// The interface font, not the comparison's code font this widget
			/// would otherwise inherit: the pill must not itself grow with the
			/// zoom it reports.
			[[nodiscard]] static auto pill_fonts() -> std::pair<QFont, QFont>
			{
				auto size = QApplication::font();
				size.setFeature(QFont::Tag{ "tnum" }, 1U);
				auto percent = size;
				percent.setWeight(QFont::DemiBold);
				percent.setPointSizeF(size.pointSizeF() * 1.35);
				return { percent, size };
			}

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
			void refresh_gutter_width();
			void set_line_numbers_visible(bool aVisible);
			void set_gutter_colors(const QColor& aBackground, const QColor& aText);
			void paint_line_numbers(QPaintEvent* aEvent);
			void paint_markers(QPainter& aPainter);
			/// \brief [[BPR-1335]]. Tints a thread's line-number cell, under its
			/// number: amber, blue when new, lighter when resolved; my draft is
			/// outlined, dashed.
			void               paint_thread_tint(QPainter& aPainter, std::uint32_t aLine, const QRectF& aCell);
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

			/// \brief [[BPR-1212]] slice 1. The rows a mark spans: its line, or
			/// all the lines of a draft written on several.
			[[nodiscard]] auto mark_span_rect(const diff_line_mark& aMark) const -> QRect
			{
				const auto last = line_row_rect(aMark.Line);
				if (!aMark.FirstLine.has_value() || *aMark.FirstLine >= aMark.Line) {
					return last;
				}
				const auto first = line_row_rect(*aMark.FirstLine);
				return first.isEmpty() || last.isEmpty() ? last : first.united(last);
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

			/// \brief [[BPR-1290]], [[BPR-1335]]. Whether the gutter shows the
			/// threads: a thread's number cell is tinted, and the head pane offers
			/// its "+" button. The gutter stays its numbers' width.
			void set_marker_column(const bool aEnabled)
			{
				if (MarkerColumn == aEnabled) {
					return;
				}
				MarkerColumn = aEnabled;
				set_add_hover(std::nullopt);
				viewport()->update();
				update_gutter();
			}

			/// \brief Whether the gutter shows the threads at all.
			[[nodiscard]] auto markers_shown() const noexcept -> bool
			{
				return MarkerColumn && LineNumbersVisible;
			}

			/// \brief [[BPR-1335]]. The thread on a line, if any, by bisection.
			[[nodiscard]] auto mark_on(const std::uint32_t aLine) const -> const diff_line_mark*
			{
				const auto found = std::ranges::lower_bound(Marks, aLine, { }, &diff_line_mark::Line);
				return found != Marks.end() && found->Line == aLine ? &*found : nullptr;
			}

			/// \brief How far the viewport sits below the gutter's top: a
			/// unified pane's heading row, nothing in a split pane.
			[[nodiscard]] auto gutter_header() const -> int
			{
				return UnifiedRows != nullptr ? viewport()->geometry().top() - contentsRect().top() : 0;
			}

			/// \brief [[BPR-1335]]. A line's number cell, in the gutter's coordinates.
			[[nodiscard]] auto number_cell(const std::uint32_t aLine) const -> QRect
			{
				const auto row = line_row_rect(aLine);
				if (row.isEmpty()) {
					return { };
				}
				return QRect{ 0, row.top() + gutter_header(), gutter_width(), row.height() };
			}

			void set_mark_dispatch(std::function<void(const diff_line_mark&)> aDispatch)
			{
				MarkDispatch = std::move(aDispatch);
			}

			/// \brief [[BPR-1212]], [[BPR-1335]]. The "+" button on a head line
			/// without a thread, shown while the pointer is in its column; none
			/// without a dispatch. It is told the first and last lines.
			void set_add_dispatch(std::function<void(std::uint32_t, std::uint32_t)> aDispatch)
			{
				AddDispatch = std::move(aDispatch);
				AddDrag.reset();
				set_add_hover(std::nullopt);
			}

			/// \brief Shows the "+" button on a line, or hides it. A drag in
			/// progress keeps its button where it started.
			void set_add_hover(std::optional<std::uint32_t> aLine);

			/// \brief The line a viewport y offers the "+" on: a line of a split
			/// pane with no thread on it, while the pointer is on its row.
			[[nodiscard]] auto add_line_at(const int aViewportY) const -> std::optional<std::uint32_t>
			{
				if (!AddDispatch || !markers_shown() || UnifiedRows != nullptr) {
					return std::nullopt;
				}
				const auto line = line_at(aViewportY);
				if (!line.has_value() || mark_on(*line) != nullptr) {
					return std::nullopt;
				}
				const auto row = line_row_rect(*line);
				if (aViewportY < row.top() || aViewportY > row.bottom()) {
					return std::nullopt;
				}
				return line;
			}

			/// \brief [[BPR-1335]] v002. Where a line's "+" is drawn, in the
			/// pane's coordinates: at the start of its code, across the
			/// boundary with its number. Empty while the line is not laid out.
			[[nodiscard]] auto add_button_rect(const std::uint32_t aLine) const -> QRect
			{
				const auto row = line_row_rect(aLine);
				if (row.isEmpty()) {
					return { };
				}
				// GitHub's place. The left edge stays in the gutter's padding,
				// two pixels into it, so a pixel of padding still parts it from
				// the digit.
				const auto side = std::clamp(row.height(), AddButtonMinSide, AddButtonSide);
				const auto code = viewport()->geometry().topLeft();
				return QRect{ code.x() - (GutterPadding - 2), code.y() + row.top() + (row.height() - side) / 2, side, side };
			}

			/// \brief [[BPR-1335]] v002. The line whose "+" a point of the pane
			/// is over: the line's own "+" column, the strip across the number
			/// and code boundary that the button is drawn in. Elsewhere on the
			/// line, on its code or its digits, nothing is drawn. The owner, on
			/// v001: "i find it somewhat distracting always being drawn. Maybe we
			/// only draw it when the cursor is over the column where it would
			/// go?"
			[[nodiscard]] auto add_line_under(const QPoint aPanePoint) const -> std::optional<std::uint32_t>
			{
				const auto line = add_line_at(aPanePoint.y() - viewport()->geometry().top());
				if (!line.has_value()) {
					return std::nullopt;
				}
				const auto place = add_button_rect(*line);
				if (aPanePoint.x() < place.left() || aPanePoint.x() > place.right()) {
					return std::nullopt;
				}
				return line;
			}

			/// \brief The line nearest a viewport y, clamped to the document.
			[[nodiscard]] auto line_at(const int aViewportY) const -> std::optional<std::uint32_t>
			{
				const auto block = cursorForPosition(QPoint{ 0, aViewportY }).block();
				if (!block.isValid() || block.blockNumber() < 0) {
					return std::nullopt;
				}
				return static_cast<std::uint32_t>(block.blockNumber());
			}

			/// \brief The "+" button's place in the pane, or empty while hidden.
			[[nodiscard]] auto add_rect() const -> QRect;

			/// \brief [[BPR-1335]]. The pointer left the code, the gutter or the
			/// button. The button stays while the pointer is on it.
			void leave_add_hover();

			/// \brief [[BPR-1335]]. A press on the "+" starts a range on its line;
			/// moving extends it to the line under the pointer, washed as a
			/// draft being written is; release adds the comment on it.
			void begin_add_drag(const std::uint32_t aLine)
			{
				AddDrag = std::pair{ aLine, aLine };
				viewport()->update();
				update_gutter();
			}
			void extend_add_drag(const QPoint aGlobal)
			{
				if (!AddDrag.has_value()) {
					return;
				}
				const auto line = line_at(viewport()->mapFromGlobal(aGlobal).y());
				if (line.has_value() && *line != AddDrag->second) {
					AddDrag->second = *line;
					viewport()->update();
					update_gutter();
				}
			}
			void finish_add_drag()
			{
				if (!AddDrag.has_value()) {
					return;
				}
				const auto [first, last] = std::minmax(AddDrag->first, AddDrag->second);
				AddDrag.reset();
				viewport()->update();
				update_gutter();
				press_add(first, last);
			}
			[[nodiscard]] auto add_drag_rect() const -> QRect
			{
				if (!AddDrag.has_value()) {
					return { };
				}
				const auto [first, last] = std::minmax(AddDrag->first, AddDrag->second);
				const auto top           = line_row_rect(first);
				const auto bottom        = line_row_rect(last);
				return top.isEmpty() || bottom.isEmpty() ? QRect{ } : top.united(bottom);
			}

			void press_add(const std::uint32_t aFirst, const std::uint32_t aLast)
			{
				if (AddDispatch) {
					AddDispatch(aFirst, aLast);
				}
			}

			/// \brief What a pointer presses to reach a mark's thread: its
			/// tinted number cell, in the gutter's coordinates.
			[[nodiscard]] auto marker_rect(const diff_line_mark& aMark) const -> QRect
			{
				return markers_shown() ? number_cell(aMark.Line) : QRect{ };
			}

			/// \brief The mark whose tinted cell is under a point of the gutter.
			/// Cells are exact, so neighbouring lines' cells never overlap.
			[[nodiscard]] auto mark_at(const QPoint aGutterPoint) const -> const diff_line_mark*
			{
				for (const auto& mark : Marks) {
					if (marker_rect(mark).contains(aGutterPoint)) {
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
				// with the gutter showing threads, its tinted number says so
				// instead, and the open thread's line is washed amber (the note
				// under the pointer, lightly).
				for (const auto& mark : Marks) {
					const auto row = mark.Writing ? mark_span_rect(mark) : line_row_rect(mark.Line);
					if (row.isEmpty() || row.top() > viewport()->height() || row.bottom() < 0) {
						continue;
					}
					painter.setPen(Qt::NoPen);
					if (mark.Open || mark.Hovered) {
						auto wash = mark.Writing ? palette().color(QPalette::Active, QPalette::Highlight) : ThreadMarkColor;
						wash.setAlphaF(mark.Open ? (mark.Writing ? 0.14F : 0.18F) : 0.08F);
						painter.fillRect(QRect{ 0, row.top(), viewport()->width(), row.height() }, wash);
					}
					if (markers_shown()) {
						continue;
					}
					painter.setBrush(mark.Kind == diff_line_mark_kind::new_thread
										 ? NewThreadMarkColor
										 : ThreadMarkColor);
					painter.drawRoundedRect(QRect{ 1, row.top() + 1, 4, std::max(1, row.height() - 2) }, 2.0, 2.0);
				}
				// [[BPR-1335]]. The lines a drag on the "+" covers, washed as the
				// draft it starts will be.
				if (const auto drag = add_drag_rect(); !drag.isEmpty()) {
					auto wash = palette().color(QPalette::Active, QPalette::Highlight);
					wash.setAlphaF(0.16F);
					painter.fillRect(QRect{ 0, drag.top(), viewport()->width(), drag.height() }, wash);
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
			/// \brief [[BPR-1335]]. The pointer on a head line's code shows its
			/// "+"; a press or a drag in the code is the text's, and hides it.
			auto viewportEvent(QEvent* aEvent) -> bool override;

		public:
			/// \brief [[BPR-1336]]. Hears that the pane has relaid out in a new font.
			///
			/// \details The pane does not wrap, so every line is one block of the
			/// same height and its bar counts lines: a font change keeps the top
			/// line and moves every line below it. The observer is told after the
			/// relayout, while the pane still shows the old top line, which is
			/// what lets it keep a line where the reader was looking.
			void set_font_observer(std::function<void()> aObserver)
			{
				FontObserver = std::move(aObserver);
			}
			/// \brief The height of one row, from the pane's own layout.
			[[nodiscard]] auto row_height() const -> qreal
			{
				const auto block = firstVisibleBlock();
				return block.isValid() ? blockBoundingRect(block).height() : 0.0;
			}
			/// \brief Where a line's row starts in the viewport, if it is laid out.
			[[nodiscard]] auto row_top(const std::uint32_t aLine) const -> std::optional<qreal>
			{
				const auto block = document()->findBlockByNumber(static_cast<int>(aLine));
				if (!block.isValid() || !block.isVisible()) {
					return std::nullopt;
				}
				return blockBoundingGeometry(block).translated(contentOffset()).top();
			}

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
			class add_button;

			gutter_area*                                      Gutter    = nullptr;
			add_button*                                       AddButton = nullptr;
			QColor                                            GutterBackground;
			QColor                                            GutterText;
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
			std::function<void(const diff_line_mark&)>             MarkDispatch;
			QColor                                                 ThreadMarkColor{ 0xD2, 0x99, 0x22 };
			QColor                                                 NewThreadMarkColor{ 0x09, 0x69, 0xDA };
			std::uint64_t                                          PaintedRules       = 0U;
			std::uint64_t                                          PaintedLineNumbers = 0U;
			std::uint64_t                                          PaintedWhiteSpace  = 0U;
			std::optional<core::line_span>                         SelectedLines;
			core::white_space_drawing                              WhiteSpace = core::white_space_drawing::none;
			std::function<void()>                                  FontObserver;
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
			/// [[BPR-1290]], [[BPR-1335]]. A thread's tinted number opens or
			/// closes it.
			void mousePressEvent(QMouseEvent* const aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton) {
					if (const auto* const mark = Pane->mark_at(aEvent->position().toPoint()); mark != nullptr) {
						const auto pressed = *mark;
						Pane->press_mark(pressed);
						aEvent->accept();
						return;
					}
				}
				QWidget::mousePressEvent(aEvent);
			}
			/// [[BPR-1335]]. The pointer in the "+" column's part in the number's
			/// padding shows the line's "+", as its part over the code does.
			void mouseMoveEvent(QMouseEvent* const aEvent) override
			{
				const auto point = aEvent->position().toPoint();
				Pane->set_add_hover(aEvent->buttons() == Qt::NoButton ? Pane->add_line_under(mapToParent(point)) : std::nullopt);
				setCursor(Pane->mark_at(point) != nullptr ? Qt::PointingHandCursor : Qt::ArrowCursor);
				QWidget::mouseMoveEvent(aEvent);
			}
			void leaveEvent(QEvent* const aEvent) override
			{
				Pane->leave_add_hover();
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
					hide_anchored_tooltip();
					aEvent->ignore();
					return true;
				}
				return QWidget::event(aEvent);
			}

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
			explicit add_button(source_pane* const aPane)
				: QAbstractButton{ aPane }
				, Pane{ aPane }
			{
				setObjectName(QStringLiteral("diffLineAddButton"));
				setFocusPolicy(Qt::NoFocus);
				setCursor(Qt::PointingHandCursor);
				setToolTip(QStringLiteral("Add comment (\u21A9)"));
				setAccessibleName(QStringLiteral("Add comment"));
				hide();
				// An accessibility client's press arrives as a click: its line.
				connect(this, &QAbstractButton::clicked, this, [this] { Pane->press_add(Line, Line); });
			}

			void set_line(const std::uint32_t aLine)
			{
				Line = aLine;
				setAccessibleDescription(QStringLiteral("Adds a comment on line %1. Drag across lines to comment on all of them; Return does the same for the selected lines").arg(aLine + 1U));
			}

			[[nodiscard]] auto line() const noexcept -> std::uint32_t
			{
				return Line;
			}

			void set_colour(const QColor& aColour)
			{
				if (Colour != aColour) {
					Colour = aColour;
					update();
				}
			}

		protected:
			void paintEvent(QPaintEvent* const /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				// Lighter under the pointer, darker while held, as GitHub's.
				const auto fill = isDown() ? Colour.darker(115) : underMouse() ? Colour.lighter(112)
																			   : Colour;
				const auto face = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				painter.setPen(Qt::NoPen);
				painter.setBrush(fill);
				painter.drawRoundedRect(face, face.width() * 0.3, face.height() * 0.3);
				painter.setPen(QPen{ Qt::white, std::max(1.5, face.width() / 11.0), Qt::SolidLine, Qt::RoundCap });
				const auto centre = face.center();
				const auto arm    = face.width() * 0.25;
				painter.drawLine(QPointF{ centre.x() - arm, centre.y() }, QPointF{ centre.x() + arm, centre.y() });
				painter.drawLine(QPointF{ centre.x(), centre.y() - arm }, QPointF{ centre.x(), centre.y() + arm });
			}
			void mousePressEvent(QMouseEvent* const aEvent) override
			{
				if (aEvent->button() != Qt::LeftButton) {
					aEvent->ignore();
					return;
				}
				setDown(true);
				Pane->begin_add_drag(Line);
				aEvent->accept();
			}
			void mouseMoveEvent(QMouseEvent* const aEvent) override
			{
				Pane->extend_add_drag(aEvent->globalPosition().toPoint());
				aEvent->accept();
			}
			void mouseReleaseEvent(QMouseEvent* const aEvent) override
			{
				if (aEvent->button() != Qt::LeftButton) {
					aEvent->ignore();
					return;
				}
				setDown(false);
				Pane->finish_add_drag();
				aEvent->accept();
			}
			void enterEvent(QEnterEvent* const aEvent) override
			{
				update();
				QAbstractButton::enterEvent(aEvent);
			}
			void leaveEvent(QEvent* const aEvent) override
			{
				update();
				if (!isDown()) {
					Pane->leave_add_hover();
				}
				QAbstractButton::leaveEvent(aEvent);
			}
			[[nodiscard]] auto hitButton(const QPoint& aPoint) const -> bool override
			{
				return rect().contains(aPoint);
			}

		private:
			source_pane*  Pane = nullptr;
			std::uint32_t Line = 0U;
			QColor        Colour{ 0x09, 0x69, 0xDA };
		};

		void source_pane::set_add_hover(const std::optional<std::uint32_t> aLine)
		{
			if (AddDrag.has_value()) {
				return;
			}
			const auto line = AddDispatch && markers_shown() ? aLine : std::nullopt;
			const auto row  = line.has_value() ? line_row_rect(*line) : QRect{ };
			if (!line.has_value() || row.isEmpty() || row.bottom() < 0 || row.top() > viewport()->height()) {
				AddHover.reset();
				AddButton->hide();
				return;
			}
			AddHover = line;
			AddButton->setGeometry(add_button_rect(*line));
			AddButton->set_line(*line);
			AddButton->set_colour(NewThreadMarkColor);
			AddButton->show();
			AddButton->raise();
		}

		auto source_pane::add_rect() const -> QRect
		{
			return AddButton->isHidden() ? QRect{ } : AddButton->geometry();
		}

		void source_pane::leave_add_hover()
		{
			if (AddDrag.has_value()) {
				return;
			}
			// Leaving the code or its number for the button keeps it; the next
			// move in the column, over the code or the gutter, places it again.
			if (!AddButton->isHidden() && AddButton->rect().contains(AddButton->mapFromGlobal(QCursor::pos()))) {
				return;
			}
			set_add_hover(std::nullopt);
		}

		auto source_pane::viewportEvent(QEvent* const aEvent) -> bool
		{
			switch (aEvent->type()) {
			case QEvent::MouseMove: {
				const auto* const move = static_cast<QMouseEvent*>(aEvent);
				set_add_hover(move->buttons() == Qt::NoButton ? add_line_under(viewport()->mapToParent(move->position().toPoint())) : std::nullopt);
				break;
			}
			case QEvent::MouseButtonPress:
			case QEvent::MouseButtonDblClick:
				set_add_hover(std::nullopt);
				break;
			case QEvent::Leave:
				leave_add_hover();
				break;
			default:
				break;
			}
			return QPlainTextEdit::viewportEvent(aEvent);
		}

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
			Gutter    = new gutter_area{ this };
			AddButton = new add_button{ this };
			// [[BPR-1335]]. The pointer in the "+" column over the code, not
			// only over the number's padding, shows the line's "+".
			viewport()->setMouseTracking(true);
			// The width follows the digit count, so it changes when a document is
			// replaced and not while scrolling one.
			connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) {
				refresh_gutter_width();
				set_add_hover(std::nullopt);
			});
			connect(this,
					&QPlainTextEdit::updateRequest,
					this,
					[this](const QRect& aRect, const int aDy) {
						if (UnifiedRows != nullptr) {
							Gutter->update();
						} else if (aDy != 0) {
							Gutter->scroll(0, aDy);
							// The line under a still pointer has changed; the
							// next move offers the "+" again.
							set_add_hover(std::nullopt);
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
				return digits(static_cast<std::uint32_t>(std::max(1, blockCount()))) * digit + 2 * GutterPadding;
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
			return 2 * column;
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
			set_add_hover(std::nullopt);
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
				if (FontObserver) {
					FontObserver();
				}
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
			const auto headerHeight = gutter_header();
			const auto numbers      = Gutter->width();
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
							paint_thread_tint(painter, static_cast<std::uint32_t>(block.blockNumber()), QRectF{ 0.0, top, static_cast<qreal>(numbers), height });
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
						if (const auto drag = add_drag_rect(); !drag.isEmpty() && top < drag.bottom() + 1 && top + height > drag.top()) {
							auto wash = palette().color(QPalette::Active, QPalette::Highlight);
							wash.setAlphaF(0.16F);
							painter.fillRect(QRectF{ 0.0, top, static_cast<qreal>(numbers), height }, wash);
						}
						paint_thread_tint(painter, static_cast<std::uint32_t>(block.blockNumber()), QRectF{ 0.0, top, static_cast<qreal>(numbers), height });
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

		void source_pane::paint_thread_tint(QPainter& aPainter, const std::uint32_t aLine, const QRectF& aCell)
		{
			// [[BPR-1335]]. The owner chose the tinted number (session
			// 3c9017b5): the number keeps its place and the gutter its width.
			if (!markers_shown()) {
				return;
			}
			const auto* const mark = mark_on(aLine);
			if (mark == nullptr) {
				return;
			}
			if (mark->Draft) {
				aPainter.save();
				aPainter.setRenderHint(QPainter::Antialiasing, true);
				aPainter.setPen(QPen{ palette().color(QPalette::Active, QPalette::PlaceholderText), 1.2, Qt::DashLine });
				aPainter.setBrush(Qt::NoBrush);
				aPainter.drawRoundedRect(aCell.adjusted(1.5, 1.5, -1.5, -1.5), 3.0, 3.0);
				aPainter.restore();
				return;
			}
			auto tint = mark->Kind == diff_line_mark_kind::new_thread ? NewThreadMarkColor : ThreadMarkColor;
			tint.setAlphaF(mark->Resolved ? 0.12F : 0.30F);
			aPainter.fillRect(aCell, tint);
		}

		void source_pane::paint_markers(QPainter& aPainter)
		{
			// [[BPR-1290]]. The open thread's number is washed amber and barred
			// at the gutter's left edge. [[BPR-1212]]: my draft being written is
			// washed blue and bracketed at its right edge, where its card joins.
			if (!markers_shown()) {
				return;
			}
			aPainter.save();
			aPainter.setClipping(false);
			aPainter.setRenderHint(QPainter::Antialiasing, true);
			const auto header = gutter_header();
			for (const auto& mark : Marks) {
				if (mark.Writing) {
					// A draft on several lines spans them all, drawn while any
					// of them shows.
					const auto row = mark_span_rect(mark);
					if (!row.isEmpty() && row.bottom() + header >= 0 && row.top() + header <= Gutter->height()) {
						const auto write = palette().color(QPalette::Active, QPalette::Highlight);
						auto       wash  = write;
						wash.setAlphaF(0.16F);
						aPainter.fillRect(QRect{ 0, row.top() + header, Gutter->width(), row.height() }, wash);
						auto bracket = QPainterPath{ };
						bracket.addRoundedRect(QRectF{ static_cast<qreal>(Gutter->width() - 3), static_cast<qreal>(row.top() + header + 2), 3.0, static_cast<qreal>(std::max(1, row.height() - 4)) }, 1.5, 1.5);
						aPainter.fillPath(bracket, write);
					}
					continue;
				}
				if (!mark.Open) {
					continue;
				}
				const auto row = line_row_rect(mark.Line);
				if (row.isEmpty() || row.bottom() + header < 0 || row.top() + header > Gutter->height()) {
					continue;
				}
				auto wash = ThreadMarkColor;
				wash.setAlphaF(0.18F);
				aPainter.fillRect(QRect{ 0, row.top() + header, Gutter->width(), row.height() }, wash);
				aPainter.fillRect(QRect{ 0, row.top() + header, 3, row.height() }, ThreadMarkColor);
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
		/// \brief [[BPR-1212]]. What a code pane's description adds: the keys
		/// that move and extend its selected line, and the two that start a
		/// comment or a suggestion on it, named as the platform names them
		/// (Return on macOS, Enter elsewhere), as Review Cycles does.
		[[nodiscard]] auto line_keys_description() -> QString
		{
#if defined(Q_OS_MACOS)
			const auto enter = QStringLiteral("Return");
#else
			const auto enter = QStringLiteral("Enter");
#endif
			return QStringLiteral("Up and Down move the selected line; with Shift they extend the selection. %1 comments on the selected lines; Shift+%1 suggests a change to them.").arg(enter);
		}

		/// \brief [[BPR-1212]]. A key the line cursor takes: no modifier but
		/// Shift, and the keypad's, which Enter on the keypad carries.
		[[nodiscard]] auto line_key_modifiers(const QKeyEvent& aEvent) noexcept -> bool
		{
			return (aEvent.modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) == Qt::NoModifier;
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
			"Linked natural-layout source panes, change correspondence map, and line details"));

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

		Vertical = new line_details_splitter{ this };
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
		Left->setAccessibleDescription(QStringLiteral("Read-only earlier file version. ") + line_keys_description());
		Right = new source_pane{ Comparison };
		Right->setObjectName(QStringLiteral("diffRightSource"));
		Right->setAccessibleName(QStringLiteral("Later source"));
		Right->setAccessibleDescription(QStringLiteral("Read-only later file version. ") + line_keys_description());
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
		Unified->setAccessibleDescription(QStringLiteral("Base and head line numbers; removed lines followed by added lines in each change. ") + line_keys_description());
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

		// [[BPR-1334]]. Line details: the current line, base over head, full
		// width and wrapped, under the comparison. It replaces the compact
		// detail pane in the same place rather than adding a second one.
		Detail = new line_details_pane{ Vertical };
		Detail->present_empty();
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
		// The comparison takes every change in the window's height; the strip
		// keeps the height the reader chose.
		Vertical->setStretchFactor(0, 1);
		Vertical->setStretchFactor(1, 0);
		Vertical->LineHeight = [this] { return Detail->fontMetrics().lineSpacing(); };
		Vertical->setSizes({ 500, Vertical->default_height() });
		// QSplitter names its handles after the widget they follow, so the
		// seam takes its own name once the strip is in place.
		if (auto* const seam = Vertical->handle(1); seam != nullptr) {
			seam->setObjectName(QStringLiteral("lineDetailsResize"));
		}
		root->addWidget(Vertical, 1);
		for (auto* const source : { Left, Right, Unified }) {
			connect(source, &QPlainTextEdit::selectionChanged, this, [this] { queue_line_details(); });
		}

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

		// [[BPR-1212]]. After the scroll controller's filters, so the line
		// cursor sees Up, Down and Return on a pane first.
		for (auto* const source : { Left, Right, Unified }) {
			source->installEventFilter(this);
		}
		// [[BPR-1336]]. ⌘-scroll and pinch over any code surface are zoom
		// gestures. The controller lets a Control-modified wheel through, so
		// this filter sees it whichever was installed first.
		LinkMap->installEventFilter(this);
		for (auto* const source : { Left, Right, Unified }) {
			pane(source)->set_font_observer([this, source] { hold_reading_position(*source); });
		}
		ZoomIndicator      = new zoom_pill{ this };
		ZoomIndicatorTimer = new QTimer{ this };
		ZoomIndicatorTimer->setSingleShot(true);
		ZoomIndicatorTimer->setInterval(1200);
		connect(ZoomIndicatorTimer, &QTimer::timeout, ZoomIndicator, &QWidget::hide);

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
		HeldLine.reset();
		CurrentResult = aPublication.Result;
		SemanticIndex = aPublication.Index;
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
		// New comments go on the head side only (core::supported_comment_target).
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

	auto diff_workbench::line_details() const noexcept -> line_details_pane*
	{
		return Detail;
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
				const auto  end   = Unified->document()->findBlock(cursor.selectionEnd() - 1).blockNumber();
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
			const auto end   = block(cursor.selectionEnd() - 1);
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

	auto diff_workbench::move_pane_line_cursor(QPlainTextEdit* const aPane, const int aStep, const bool aExtend) -> bool
	{
		if (!CurrentResult || !SemanticIndex) {
			return false;
		}
		if (aPane == Unified) {
			return move_unified_line_cursor(aStep, aExtend);
		}
		const auto side  = aPane == Left ? core::diff_side::left : core::diff_side::right;
		const auto count = side == core::diff_side::left ? CurrentResult->Alignment.LeftLineCount : CurrentResult->Alignment.RightLineCount;
		if (count == 0U) {
			return false;
		}
		auto* const document = aPane->document();
		const auto  block    = [document](const int aPosition) {
			return static_cast<std::uint32_t>(std::max(0, document->findBlock(aPosition).blockNumber()));
		};
		// Where the cursor is: the selected line, or the moving end of the
		// pane's selection when that holds it. With no line on this pane, the
		// first line in view is selected and nothing moves yet.
		auto moving = std::optional<std::uint32_t>{ };
		auto anchor = std::optional<std::uint32_t>{ };
		if (Interaction.Selection && Interaction.Selection->PreferredLine.Side == side && Interaction.Selection->PreferredLine.Line < count) {
			const auto preferred = Interaction.Selection->PreferredLine.Line;
			moving               = preferred;
			anchor               = preferred;
			const auto cursor    = aPane->textCursor();
			if (cursor.hasSelection()) {
				const auto from  = block(cursor.anchor());
				const auto ended = block(cursor.position());
				auto       to    = ended;
				// A selection dragged to the start of the next line ends above
				// it, unless that line is the selected one (a Shift-click).
				if (to > from && to != preferred && cursor.position() == document->findBlock(cursor.position()).position()) {
					--to;
				}
				if (preferred >= std::min(from, ended) && preferred <= std::max(from, ended)) {
					anchor = from;
					moving = to;
				}
			}
		}
		auto next = std::uint32_t{ 0U };
		if (!moving) {
			next   = std::min(block(aPane->cursorForPosition(QPoint{ 0, 0 }).position()), count - 1U);
			anchor = next;
		} else {
			const auto target = static_cast<std::int64_t>(*moving) + aStep;
			next              = static_cast<std::uint32_t>(std::clamp<std::int64_t>(target, 0, static_cast<std::int64_t>(count) - 1));
		}
		if (!aExtend) {
			anchor = next;
		}
		if (!select_line(side, next, true)) {
			return false;
		}
		// The cursor keeps itself in view through the linked scroll, never by
		// scrolling one pane on its own.
		if (!source_line_visible(side, next) && Interaction.Selection) {
			static_cast<void>(reveal_exact_line(*Interaction.Selection, Interaction.Selection->PreferredLine));
		}
		const auto first = document->findBlockByNumber(static_cast<int>(*anchor));
		const auto last  = document->findBlockByNumber(static_cast<int>(next));
		if (first.isValid() && last.isValid()) {
			auto cursor = QTextCursor{ document };
			if (*anchor == next) {
				cursor.setPosition(last.position());
			} else if (*anchor < next) {
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
			aPane->setTextCursor(cursor);
			vertical->setValue(top);
			horizontal->setValue(left);
		}
		announce_line_selection();
		return true;
	}

	auto diff_workbench::move_unified_line_cursor(const int aStep, const bool aExtend) -> bool
	{
		const auto& rows = UnifiedPresentation->Rows;
		if (rows.empty()) {
			return false;
		}
		auto* const document = Unified->document();
		const auto  rowOf    = [document](const int aPosition) {
			return static_cast<std::uint32_t>(std::max(0, document->findBlock(aPosition).blockNumber()));
		};
		const auto count  = static_cast<std::uint32_t>(rows.size());
		auto       moving = std::optional<std::uint32_t>{ };
		auto       anchor = std::optional<std::uint32_t>{ };
		if (Interaction.Selection) {
			if (const auto row = unified_row_for(Interaction.Selection->PreferredLine.Side, Interaction.Selection->PreferredLine.Line)) {
				moving            = *row;
				anchor            = *row;
				const auto cursor = Unified->textCursor();
				if (cursor.hasSelection()) {
					const auto from = rowOf(cursor.anchor());
					auto       to   = rowOf(cursor.position());
					if (to > from && cursor.position() == document->findBlock(cursor.position()).position()) {
						--to;
					}
					if (*row >= std::min(from, to) && *row <= std::max(from, to)) {
						anchor = from;
						moving = to;
					}
				}
			}
		}
		auto next = std::uint32_t{ 0U };
		if (!moving) {
			next   = std::min(rowOf(Unified->cursorForPosition(QPoint{ 0, 0 }).position()), count - 1U);
			anchor = next;
		} else {
			next = static_cast<std::uint32_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(*moving) + aStep, 0, static_cast<std::int64_t>(count) - 1));
		}
		if (!aExtend) {
			anchor = next;
		}
		const auto& identity = rows[next];
		if (!select_line(identity.Head ? core::diff_side::right : core::diff_side::left, identity.Head.value_or(identity.Base.value_or(0U)), true)) {
			return false;
		}
		const auto first = document->findBlockByNumber(static_cast<int>(*anchor));
		const auto last  = document->findBlockByNumber(static_cast<int>(next));
		if (first.isValid() && last.isValid()) {
			auto cursor = QTextCursor{ document };
			if (*anchor == next) {
				cursor.setPosition(last.position());
			} else if (*anchor < next) {
				cursor.setPosition(first.position());
				cursor.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
			} else {
				cursor.setPosition(first.position() + first.length() - 1);
				cursor.setPosition(last.position(), QTextCursor::KeepAnchor);
			}
			const auto left = Unified->horizontalScrollBar()->value();
			const auto top  = Unified->verticalScrollBar()->value();
			Unified->setTextCursor(cursor);
			Unified->horizontalScrollBar()->setValue(left);
			Unified->verticalScrollBar()->setValue(top);
		}
		if (!source_line_visible(identity.Head ? core::diff_side::right : core::diff_side::left, identity.Head.value_or(identity.Base.value_or(0U)))) {
			reveal_unified_row(next);
		}
		announce_line_selection();
		return true;
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
		const auto  height    = static_cast<qreal>(reference->viewport()->height());
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
		const auto newTop = static_cast<std::uint32_t>(std::max(0.0, row - std::round((Hold->Offset - first) / current)));
		source->set_top_line(newTop);
		if (Hold->Line && !unified && Hold->Line->Side == side) {
			HeldLine = held_line{ .Side = side, .Top = newTop };
		}
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
		// [[BPR-1334]]. Line details repeats the panes' lines in their colours.
		Detail->set_colors(line_details_colors{
			.Added            = Colors.Added,
			.Removed          = Colors.Removed,
			.IntralineAdded   = Colors.IntralineAdded,
			.IntralineRemoved = Colors.IntralineRemoved,
			.GutterBackground = Colors.Syntax[core::index_of(role::code_gutter_background)],
			.GutterText       = Colors.Syntax[core::index_of(role::code_line_number_text)],
			.Ink              = Colors.Syntax[core::index_of(role::syntax_normal)],
			.Variant          = Colors.Variant,
			.Supplied         = true,
		});
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
} // namespace bpr::qt
