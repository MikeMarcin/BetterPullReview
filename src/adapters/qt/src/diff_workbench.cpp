#include "bpr/qt/diff_workbench.hpp"
#include "bpr/core/appearance_contrast.hpp"

#include "bpr/application/workbench_runtime_ports.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/type_preview.hpp"

#include "line_details_splitter.hpp"
#include "source_display_text.hpp"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
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
#include <QPointer>
#include <QPushButton>
#include <QRegion>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSet>
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
#include <map>

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
		/// \brief [[BPR-1503]]. The current find match's outline colour.
		///
		/// \details The current-match wash is held to the text floor, which
		/// keeps it pale on a light canvas and dark on a dark one, too faint
		/// to draw a one-pixel rule with. The outline is the same hue moved
		/// toward the canvas's opposite until it reaches the indicator ratio
		/// the theme checks every other mark on the canvas against.
		[[nodiscard]] auto find_outline_color(const core::appearance_color aCurrent, const core::appearance_color aCanvas) -> QColor
		{
			const auto canvasLight = core::contrast_ratio_hundredths(aCanvas, core::appearance_color{ 0U, 0U, 0U, 255U })
				> core::contrast_ratio_hundredths(aCanvas, core::appearance_color{ 255U, 255U, 255U, 255U });
			const auto toward = canvasLight ? core::appearance_color{ 0U, 0U, 0U, 255U } : core::appearance_color{ 255U, 255U, 255U, 255U };
			auto       mark   = aCurrent;
			for (auto alpha = 0U; alpha <= 255U && core::contrast_ratio_hundredths(mark, aCanvas) < core::indicator_contrast_minimum_hundredths; alpha += 8U) {
				mark = core::composited(toward, aCurrent, static_cast<std::uint8_t>(std::min(alpha, 255U)));
			}
			return to_presentation_color(mark);
		}
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

			void set_find_ticks(std::vector<qreal> aFractions)
			{
				FindFractions = std::move(aFractions);
				std::ranges::sort(FindFractions);
				update();
			}
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
				if (Ticks.empty() && FindFractions.empty()) {
					return;
				}
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto faint  = palette().color(QPalette::Active, QPalette::PlaceholderText);
				auto       option = QStyleOptionSlider{ };
				initStyleOption(&option);
				const auto groove = style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarGroove, this);
				painter.setPen(palette().color(QPalette::Highlight));
				// One binary lookup per track pixel, irrespective of occurrence count.
				for (int y = 0; y < groove.height(); ++y) {
					const auto from = static_cast<qreal>(y) / std::max(1, groove.height());
					const auto at   = std::ranges::lower_bound(FindFractions, from);
					if (at != FindFractions.end() && *at < static_cast<qreal>(y + 1) / std::max(1, groove.height())) {
						painter.drawLine(groove.left(), groove.top() + y, groove.left() + 2, groove.top() + y);
					}
				}
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
			std::vector<qreal>                                       FindFractions;
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
			QList<QTextCursor> FindOutlines;
			/// [[BPR-1503]]. The current match's outline: the theme's current
			/// match colour, carried to the indicator ratio on the canvas.
			QColor FindOutlineColor;
			void   set_unified_rows(const std::vector<unified_source_row>* aRows)
			{
				UnifiedRows = aRows;
				refresh_gutter_width();
			}
			void set_row_colors(const QColor& aRemoved, const QColor& aAdded);
			void set_unified_selection(std::optional<core::line_span> aBase, std::optional<core::line_span> aHead)
			{
				if (SelectedBase == aBase && SelectedHead == aHead) {
					return;
				}
				const auto previous = selected_rows_region();
				SelectedBase        = aBase;
				SelectedHead        = aHead;
				const auto changed  = previous.xored(selected_rows_region());
				viewport()->update(changed);
				update_gutter(changed);
			}
			/// \brief [[BPR-1107]]. The lines this side's selection washes.
			void set_selected_lines(std::optional<core::line_span> aLines)
			{
				if (SelectedLines != aLines) {
					const auto previous = selected_rows_region();
					SelectedLines       = aLines;
					viewport()->update(previous.xored(selected_rows_region()));
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
				if (Boundaries != aBoundaries) {
					Boundaries = std::move(aBoundaries);
					viewport()->update();
				}
			}

			void set_boundary_rule_color(const QColor& aColor)
			{
				if (RuleColor != aColor) {
					RuleColor = aColor;
					viewport()->update();
				}
			}

			void set_conversation_anchor(std::optional<std::uint32_t> aLine)
			{
				if (ConversationAnchor != aLine) {
					ConversationAnchor = aLine;
					viewport()->update();
				}
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
			/// all the lines of a draft or thread on several.
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

			/// \brief [[BPR-1724]]. Told when a line's number is clicked
			/// (`false`), or Shift-clicked or dragged onto (`true`), with its
			/// zero-based row: GitHub's way of selecting lines by their numbers.
			void set_number_dispatch(std::function<void(std::uint32_t, bool)> aDispatch)
			{
				NumberDispatch = std::move(aDispatch);
				NumberDrag.reset();
			}

			/// \brief [[BPR-1724]]. The row whose number a point of the
			/// gutter is on, or none: the heading row and below the last line
			/// are no row's.
			[[nodiscard]] auto number_line_at(const QPoint aGutterPoint) const -> std::optional<std::uint32_t>
			{
				const auto y    = aGutterPoint.y() - gutter_header();
				const auto line = line_at(y);
				if (!line.has_value()) {
					return std::nullopt;
				}
				const auto row = line_row_rect(*line);
				if (row.isEmpty() || y < row.top() || y > row.bottom()) {
					return std::nullopt;
				}
				return line;
			}

			/// \brief [[BPR-1724]]. A press on a line's number selects it,
			/// or with Shift extends the selection to it, and starts a drag
			/// over the numbers. The code takes the keyboard, so Return and
			/// Shift+arrows go on from there. Returns whether it was taken.
			auto press_number(const std::uint32_t aLine, const bool aExtend) -> bool
			{
				if (!NumberDispatch) {
					return false;
				}
				NumberDrag = aLine;
				setFocus(Qt::MouseFocusReason);
				const auto dispatch = NumberDispatch;
				dispatch(aLine, aExtend);
				return true;
			}
			/// \brief [[BPR-1724]]. A drag over the numbers extends the
			/// selection to the line under the pointer.
			void extend_number_drag(const int aGutterY)
			{
				if (!NumberDrag.has_value() || !NumberDispatch) {
					return;
				}
				const auto line = line_at(aGutterY - gutter_header());
				if (line.has_value() && *line != *NumberDrag) {
					NumberDrag          = line;
					const auto dispatch = NumberDispatch;
					dispatch(*line, true);
				}
			}
			void finish_number_drag()
			{
				NumberDrag.reset();
			}
			[[nodiscard]] auto add_dragging() const noexcept -> bool
			{
				return AddDrag.has_value();
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
			/// \brief [[RFC-BPR-0090]]. The first line any of whose row shows.
			[[nodiscard]] auto top_visible_line() const -> std::optional<std::uint32_t>
			{
				const auto block = firstVisibleBlock();
				return block.isValid() && block.blockNumber() >= 0 ? std::optional{ static_cast<std::uint32_t>(block.blockNumber()) } : std::nullopt;
			}

			/// \brief [[RFC-BPR-0090]]. A line's row top in viewport coordinates,
			/// unrounded, so text drawn over the pane meets its rows exactly.
			[[nodiscard]] auto exact_line_top(const std::uint32_t aLine) const -> std::optional<qreal>
			{
				const auto block = document()->findBlockByNumber(static_cast<int>(aLine));
				if (!block.isValid() || !block.isVisible()) {
					return std::nullopt;
				}
				return blockBoundingGeometry(block).translated(contentOffset()).top();
			}

			[[nodiscard]] auto visible_origin() const
				-> std::optional<std::pair<std::size_t, qreal>>
			{
				const auto block = firstVisibleBlock();
				if (!block.isValid() || block.blockNumber() < 0) {
					return std::nullopt;
				}
				return std::pair{
					static_cast<std::size_t>(block.blockNumber()),
					contentOffset().y() - AppliedOffset,
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

			/// [[BPR-1527]]. A shared frame supplies both parts of its origin.
			/// Keep the residual through the native top-line change instead of
			/// clearing it and resizing before its replacement arrives.
			void place_top_line_and_offset(const std::size_t aLine, const qreal aOffset)
			{
				const auto holdOffset = QScopedValueRollback<bool>{ ApplyingPixelWheel, true };
				set_top_line(aLine);
				set_within_line_offset(aOffset);
			}

			/// \brief Places the native viewport, including native selection and hits.
			/// \details The scrollbar still chooses the first block. The remaining
			/// pixels move the viewport widget itself, clipped to its original
			/// content area. No painter-only transform or second text layout is used.
			void set_within_line_offset(const qreal aOffset)
			{
				RetainedOffset    = std::max(0.0, aOffset);
				const auto offset = std::max(0, qRound(RetainedOffset));
				auto       area   = ViewportArea.isValid() ? ViewportArea : viewport()->geometry();
				// Legacy bars have their own band, including native frame spacing.
				// [[BPR-1666]]. An overlay keeps Qt's full viewport: clipping it
				// above a transient bar changes the shared rail whenever it shows.
				auto* const horizontal = horizontalScrollBar();
				if (UnifiedRows == nullptr && horizontal->isVisible()
					&& horizontal->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, horizontal) == 0) {
					area.setBottom(horizontal->mapTo(this, QPoint{ }).y() - 1);
				}
				// The shared bar owns split-view extent. Only the displaced
				// pixels need overscan: an extra row would overlap the horizontal
				// bar and make Qt refuse backing-store scroll reuse. Unified
				// keeps its native vertical scrollbar's existing extent.
				const auto overscan  = UnifiedRows == nullptr ? offset : qCeil(QFontMetricsF{ font() }.lineSpacing() + 2.0 * document()->documentMargin());
				const auto placement = QRect{ area.x(), area.y() - offset, area.width(), area.height() + overscan };
				const auto clipping  = QRegion{ QRect{ 0, offset, area.width(), area.height() } };
				if (offset == AppliedOffset && viewport()->geometry() == placement && viewport()->mask() == clipping) {
					return;
				}
				PlacingOrigin = true;
				ViewportArea  = area;
				AppliedOffset = offset;
				viewport()->setGeometry(placement);
				viewport()->setMask(clipping);
				viewport()->update();
				update_gutter();
				PlacingOrigin = false;
			}
			[[nodiscard]] auto within_line_offset() const noexcept -> int { return AppliedOffset; }
			[[nodiscard]] auto placing_origin() const noexcept -> bool { return PlacingOrigin; }
			[[nodiscard]] auto changes_viewport_extent(const QSize aSize) const noexcept -> bool { return !ViewportArea.isValid() || aSize != ViewportArea.size(); }
			[[nodiscard]] auto viewport_extent() const noexcept -> int { return ViewportArea.isValid() ? ViewportArea.height() : viewport()->height(); }
			[[nodiscard]] auto within_line_position() const noexcept -> qreal { return RetainedOffset; }
			void               reset_pixel_gesture() { PixelGesture.reset(); }
			void               set_scroll_observer(std::function<void()> aObserver) { ScrollObserver = std::move(aObserver); }

		protected:
			void scrollContentsBy(const int aDx, const int aDy) override
			{
				// [[BPR-1492]]. The base records the new top line first. Clearing
				// the within-line offset can resize the viewport, and that resize
				// readjusts the bar to the top line the base last recorded, so
				// clearing it first sent a font change's hold back to the old line.
				QPlainTextEdit::scrollContentsBy(aDx, aDy);
				if (aDy != 0 && !ApplyingPixelWheel) {
					set_within_line_offset(0.0);
				}
				// Qt's rectangle scroll also copies into the masked prefix.
				// Restore the native parent frame that prefix can cover.
				if (UnifiedRows == nullptr && AppliedOffset > 0) {
					const auto covered = viewport()->geometry().intersected(QRect{ ViewportArea.x(), 0, ViewportArea.width(), ViewportArea.y() });
					if (!covered.isEmpty()) {
						update(covered);
					}
				}
			}

			void wheelEvent(QWheelEvent* const aEvent) override
			{
				if (UnifiedRows == nullptr || aEvent->modifiers().testFlag(Qt::ControlModifier)) {
					QPlainTextEdit::wheelEvent(aEvent);
					return;
				}
				const auto delta = PixelGesture.consume(*aEvent);
				if (aEvent->pixelDelta().isNull()) {
					if (aEvent->angleDelta().isNull() && aEvent->phase() != Qt::NoScrollPhase) {
						aEvent->accept();
						return;
					}
					set_within_line_offset(0.0);
					QPlainTextEdit::wheelEvent(aEvent);
					return;
				}
				const auto  shift      = aEvent->modifiers().testFlag(Qt::ShiftModifier) && aEvent->pixelDelta().x() == 0;
				auto* const horizontal = horizontalScrollBar();
				horizontal->setValue(horizontal->value() - (shift ? aEvent->pixelDelta().y() : delta.x()));
				if (!shift && delta.y() != 0) {
					auto* const vertical = verticalScrollBar();
					// The terminal native block includes the bottom document margin;
					// it must not change the row stride when reversing from End.
					const auto height = blockBoundingRect(document()->firstBlock()).height();
					if (height > 0.0) {
						const auto origin  = std::clamp(vertical->value() * height + RetainedOffset - delta.y(), 0.0, vertical->maximum() * height);
						const auto line    = static_cast<int>(std::floor(origin / height));
						ApplyingPixelWheel = true;
						vertical->setValue(line);
						set_within_line_offset(origin - line * height);
						ApplyingPixelWheel = false;
						if (ScrollObserver) {
							ScrollObserver();
						}
					}
				}
				aEvent->accept();
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
				paint_text(aEvent);
				for (const auto& outlineCursor : FindOutlines) {
					auto outlinePainter = QPainter{ viewport() };
					outlinePainter.setPen(QPen{ FindOutlineColor.isValid() ? FindOutlineColor : palette().color(QPalette::Highlight), 1.5 });
					const auto first = outlineCursor.selectionStart();
					const auto last  = outlineCursor.selectionEnd();
					for (auto visible = firstVisibleBlock(); visible.isValid(); visible = visible.next()) {
						const auto rowBox = blockBoundingGeometry(visible).translated(contentOffset());
						if (rowBox.top() > viewport()->height()) {
							break;
						}
						if (visible.position() + visible.length() <= first || visible.position() > last) {
							continue;
						}
						auto beginCursor = QTextCursor{ visible };
						beginCursor.setPosition(std::max(first, visible.position()));
						auto endCursor = QTextCursor{ visible };
						endCursor.setPosition(std::min(last, visible.position() + visible.length() - 1));
						const auto beginRect = cursorRect(beginCursor);
						const auto endRect   = cursorRect(endCursor);
						const auto spanBox   = QRect{ beginRect.left(), beginRect.top(), std::max(2, endRect.left() - beginRect.left()), beginRect.height() };
						outlinePainter.drawRect(spanBox.adjusted(0, 0, 0, -1));
					}
				}
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
				// [[BPR-1721]]. A thread on several lines washes them all
				// when open, as my draft on several does; its edge mark stays
				// on its last line, where GitHub anchors it.
				for (const auto& mark : Marks) {
					const auto row   = mark.Writing ? mark_span_rect(mark) : line_row_rect(mark.Line);
					const auto lines = mark.Open || mark.Hovered ? mark_span_rect(mark) : row;
					if (row.isEmpty() || lines.top() > viewport()->height() || lines.bottom() < 0) {
						continue;
					}
					painter.setPen(Qt::NoPen);
					if (mark.Open || mark.Hovered) {
						auto wash = mark.Writing ? palette().color(QPalette::Active, QPalette::Highlight) : ThreadMarkColor;
						wash.setAlphaF(mark.Open ? (mark.Writing ? 0.14F : 0.18F) : 0.08F);
						painter.fillRect(QRect{ 0, lines.top(), viewport()->width(), lines.height() }, wash);
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
				auto block = firstVisibleBlock();
				if (!block.isValid()) {
					return;
				}
				const auto limit = static_cast<qreal>(viewport()->height());
				auto       top   = blockBoundingGeometry(block).translated(contentOffset()).top();
				for (; block.isValid() && top <= limit; block = block.next()) {
					// Native block geometry walks from the first visible block.
					// Accumulate its heights once, as the text painter does.
					top += blockBoundingRect(block).height();
					if (block.isVisible()) {
						PaintedWhiteSpace += paint_white_space_of_block(aPainter, *this, block, WhiteSpace, selected_block(block.blockNumber()), GutterText);
					}
				}
			}

			/// \brief The visible rows whose selected state can affect white space.
			/// \details [[BPR-1525]]. A keyboard selection changes a few rows,
			/// so invalidate their old/new difference rather than force a full
			/// viewport paint. Qt invalidates the text bands independently.
			[[nodiscard]] auto selected_rows_region() const -> QRegion
			{
				auto region = QRegion{ };
				auto block  = firstVisibleBlock();
				if (!block.isValid()) {
					return region;
				}
				auto top = blockBoundingGeometry(block).translated(contentOffset()).top();
				for (; block.isValid() && top <= viewport()->height(); block = block.next()) {
					const auto bottom = top + blockBoundingRect(block).height();
					if (block.isVisible() && selected_block(block.blockNumber())) {
						region += QRect{ 0, qFloor(top), std::max(viewport()->width(), GutterWidth), qCeil(bottom) - qFloor(top) };
					}
					top = bottom;
				}
				return region;
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

		public:
			/// \brief [[BPR-1533]]. The ink of unclassified text and the
			/// variant whose floor the selected glyphs are held to.
			void set_selection_ink(const QColor& aNormal, const core::appearance_variant aVariant)
			{
				if (SelectionNormal != aNormal || SelectionVariant != aVariant) {
					SelectionNormal  = aNormal;
					SelectionVariant = aVariant;
					viewport()->update();
				}
			}

		protected:
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
				return blockBoundingGeometry(block).translated(contentOffset()).top() - AppliedOffset;
			}

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

			gutter_area*                                      Gutter    = nullptr;
			add_button*                                       AddButton = nullptr;
			QColor                                            GutterBackground;
			QColor                                            GutterText;
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
			/// [[BPR-1724]]. Anywhere else on a number, a click selects its
			/// line, a Shift-click extends the selection to it, and a drag
			/// selects the lines it crosses, as on GitHub. Shift on a tinted
			/// number extends too. The "+" column's part in the number's
			/// padding stays the "+"'s, shown or not, so a press there never
			/// selects instead of adding a comment.
			void mousePressEvent(QMouseEvent* const aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton) {
					const auto point = aEvent->position().toPoint();
					const auto shift = aEvent->modifiers().testFlag(Qt::ShiftModifier);
					if (const auto* const mark = Pane->mark_at(point); mark != nullptr && !shift) {
						const auto pressed = *mark;
						Pane->press_mark(pressed);
						aEvent->accept();
						return;
					}
					if (const auto add = Pane->add_line_under(mapToParent(point)); add.has_value()) {
						Pane->set_add_hover(add);
						Pane->begin_add_drag(*add);
						aEvent->accept();
						return;
					}
					if (const auto line = Pane->number_line_at(point); line.has_value() && Pane->press_number(*line, shift)) {
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
				if (aEvent->buttons().testFlag(Qt::LeftButton)) {
					if (Pane->add_dragging()) {
						Pane->extend_add_drag(aEvent->globalPosition().toPoint());
					} else {
						Pane->extend_number_drag(point.y());
					}
				}
				Pane->set_add_hover(aEvent->buttons() == Qt::NoButton ? Pane->add_line_under(mapToParent(point)) : std::nullopt);
				setCursor(Pane->mark_at(point) != nullptr ? Qt::PointingHandCursor : Qt::ArrowCursor);
				QWidget::mouseMoveEvent(aEvent);
			}
			void mouseReleaseEvent(QMouseEvent* const aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton) {
					Pane->finish_number_drag();
					if (Pane->add_dragging()) {
						Pane->finish_add_drag();
						aEvent->accept();
						return;
					}
				}
				QWidget::mouseReleaseEvent(aEvent);
			}
			void leaveEvent(QEvent* const aEvent) override
			{
				Pane->leave_add_hover();
				QWidget::leaveEvent(aEvent);
			}
			/// [[BPR-1721]]. A right-click on a line's number asks for
			/// the pane's menu at that line, as one on its code does. The
			/// scroll area drops a pointer's menu request that is not its
			/// viewport's, so the numbers opened no menu.
			void contextMenuEvent(QContextMenuEvent* const aEvent) override
			{
				if (Pane->contextMenuPolicy() != Qt::CustomContextMenu) {
					QWidget::contextMenuEvent(aEvent);
					return;
				}
				aEvent->accept();
				Q_EMIT Pane->customContextMenuRequested(QPoint{ 0, Pane->viewport()->mapFrom(Pane, mapToParent(aEvent->pos())).y() });
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

		void source_pane::update_gutter(const QRegion& aRegion)
		{
			auto column = QRegion{ };
			for (const auto& damaged : aRegion) {
				column += QRect{ 0, damaged.y() + gutter_header() - AppliedOffset, Gutter->width(), damaged.height() };
			}
			Gutter->update(column);
		}

		void source_pane::place_gutter()
		{
			const auto area = contentsRect();
			Gutter->setGeometry(QRect{ area.left(), area.top(), gutter_width(), area.height() });
		}

		void source_pane::set_row_colors(const QColor& aRemoved, const QColor& aAdded)
		{
			if (RemovedColor != aRemoved || AddedColor != aAdded) {
				RemovedColor = aRemoved;
				AddedColor   = aAdded;
				Gutter->update();
			}
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
							if (aDy != 0) {
								// Rows move under the fixed heading; repaint the column.
								Gutter->update();
							} else {
								// Text damage already names the changed rows. Translate
								// their native viewport coordinates under the heading.
								Gutter->update(0, aRect.y() + gutter_header() - AppliedOffset - 1, Gutter->width(), aRect.height() + 2);
							}
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
			if (GutterBackground != aBackground || GutterText != aText) {
				GutterBackground = aBackground;
				GutterText       = aText;
				Gutter->update();
			}
		}

		void source_pane::resizeEvent(QResizeEvent* const aEvent)
		{
			if (!PlacingOrigin) {
				ViewportArea = viewport()->geometry();
			}
			if (lineWrapMode() == QPlainTextEdit::NoWrap) {
				// [[BPR-1516]]. Native width reflow forgets the longest line until
				// it is laid out again, hiding and restoring the horizontal bar.
				// Unwrapped lines keep their geometry at every width. Deliver the
				// height change only; Qt still adjusts both bars from the actual
				// viewport, without discarding its measured natural line widths.
				auto heightChange = QResizeEvent{ aEvent->size(), QSize{ aEvent->size().width(), aEvent->oldSize().height() } };
				QPlainTextEdit::resizeEvent(&heightChange);
			} else {
				QPlainTextEdit::resizeEvent(aEvent);
			}
			if (!PlacingOrigin) {
				set_within_line_offset(RetainedOffset);
			}
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

		/// Disabling Qt's flag alone retains its allocated glyph data. Qt frees
		/// it on completion of a noncached draw; a clip above every layout line
		/// completes that operation without painting or changing line geometry.
		void release_text_cache(QTextLayout& aLayout, QPainter& aPainter)
		{
			aLayout.setCacheEnabled(false);
			if (aLayout.lineCount() == 0) {
				// A native relayout can clear only the line list, retaining glyph
				// data. Complete an empty layout to release it and keep zero lines.
				aLayout.beginLayout();
				aLayout.endLayout();
				return;
			}
			const auto above = aLayout.position().y() + aLayout.boundingRect().top() - 2.0;
			aLayout.draw(&aPainter, QPointF{ }, { }, QRectF{ 0.0, above, 1.0, 1.0 });
		}

		void source_pane::paint_text(QPaintEvent* const aEvent)
		{
			auto painter = QPainter{ viewport() };
			// [[BPR-1525]]. Qt otherwise discards glyph layout data after a
			// draw. Retain only this viewport's blocks, and let Qt invalidate
			// their caches when text, font or formats change. Persistent cursors
			// follow replacements within a document; bare QTextBlock handles
			// become invalid when a publication clears its previous blocks.
			if (CachedTextDocument != document()) {
				if (CachedTextDocument) {
					for (const auto& cachedCursor : CachedTextCursors) {
						const auto block = cachedCursor.block();
						if (block.isValid() && block.layout() != nullptr) {
							release_text_cache(*block.layout(), painter);
						}
					}
				}
				CachedTextCursors.clear();
				CachedTextDocument = document();
			}
			auto visibleCursors = QList<QTextCursor>{ };
			auto visibleNumbers = QSet<int>{ };
			visibleCursors.reserve(CachedTextCursors.size());
			visibleNumbers.reserve(CachedTextCursors.size());
			auto       retained     = CachedTextCursors.cbegin();
			const auto retainedEnd  = CachedTextCursors.cend();
			auto       offset       = contentOffset();
			auto       exposed      = aEvent->rect();
			const auto viewportRect = viewport()->rect();
			const auto maximumWidth = document()->documentLayout()->documentSize().width();
			// As Qt does: the brush origin for wave underlines, and the right
			// margin kept clear of a full-width selection.
			painter.setBrushOrigin(offset);
			const auto right = static_cast<int>(offset.x() + std::max(static_cast<qreal>(viewportRect.width()), maximumWidth)
												- document()->documentMargin() + cursorWidth());
			exposed.setRight(std::min(exposed.right(), right));
			painter.setClipRect(exposed);

			auto context = getPaintContext();
			// Qt puts the text selection last, in Highlight and HighlightedText.
			// [[BPR-1533]]. It stays last, the top layer, but is drawn per block
			// below in the selection's fill with each glyph's own readable ink.
			const auto cursor = textCursor();
			// [[BPR-1503]]. Find selects the current match; its own wash and
			// outline mark it, so the selection is not stacked on top.
			const bool findCurrent   = std::ranges::any_of(FindOutlines, [&cursor](const QTextCursor& aOutline) {
				return aOutline.selectionStart() == cursor.selectionStart() && aOutline.selectionEnd() == cursor.selectionEnd();
			});
			auto       textSelection = std::optional<std::pair<int, int>>{ };
			auto       fullWidth     = false;
			if (cursor.hasSelection() && !context.selections.isEmpty() && context.selections.back().cursor == cursor) {
				// The style's choice: on macOS a selection past a line's end
				// runs to the pane's edge.
				fullWidth = context.selections.back().format.boolProperty(QTextFormat::FullWidthSelection);
				context.selections.removeLast();
				if (!findCurrent) {
					textSelection = std::pair{ cursor.selectionStart(), cursor.selectionEnd() };
				}
			}
			const auto selectionInk = code_text_selection{
				.Fill      = code_text_selection_fill(palette().color(hasFocus() ? QPalette::Active : QPalette::Inactive, QPalette::Highlight), palette().color(QPalette::Base), SelectionVariant),
				.Normal    = SelectionNormal.isValid() ? SelectionNormal : context.palette.text().color(),
				.Variant   = SelectionVariant,
				.FullWidth = fullWidth,
			};
			painter.setPen(context.palette.text().color());

			const auto caret = textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard) || !isReadOnly();
			for (auto block = firstVisibleBlock(); block.isValid(); block = block.next()) {
				const auto rect = blockBoundingRect(block).translated(offset);
				if (!block.isVisible()) {
					offset.ry() += rect.height();
					continue;
				}
				auto* const layout = block.layout();
				if (layout != nullptr) {
					layout->setCacheEnabled(true);
					// Persistent cursors stay in document order through edits. Copy
					// their shared Qt state for retained rows instead of allocating
					// and registering a new private cursor for every row on every paint.
					while (retained != retainedEnd && retained->blockNumber() < block.blockNumber()) {
						++retained;
					}
					visibleCursors.push_back(retained != retainedEnd && retained->block() == block ? *retained : QTextCursor{ block });
					visibleNumbers.insert(block.blockNumber());
				}
				if (layout != nullptr && rect.bottom() >= exposed.top() && rect.top() <= exposed.bottom()) {
					if (const auto background = block.blockFormat().background(); background != Qt::NoBrush) {
						auto contents = rect;
						contents.setWidth(std::max(rect.width(), maximumWidth));
						painter.fillRect(contents, background);
					}
					auto       ranges   = QList<QTextLayout::FormatRange>{ };
					const auto position = block.position();
					const auto length   = block.length();
					for (const auto& selection : std::as_const(context.selections)) {
						const auto from = selection.cursor.selectionStart() - position;
						const auto to   = selection.cursor.selectionEnd() - position;
						if (from < length && to > 0 && to > from) {
							ranges.push_back(QTextLayout::FormatRange{ .start = from, .length = to - from, .format = selection.format });
						} else if (!selection.cursor.hasSelection() && selection.format.hasProperty(QTextFormat::FullWidthSelection)
								   && block.contains(selection.cursor.position())) {
							// A full-width selection needs only a position.
							const auto line  = layout->lineForTextPosition(selection.cursor.position() - position);
							auto       range = QTextLayout::FormatRange{ .start = line.textStart(), .length = line.textLength(), .format = selection.format };
							if (range.start + range.length == length - 1) {
								++range.length;
							}
							ranges.push_back(range);
						}
					}
					if (textSelection && textSelection->first - position < length && textSelection->second > position) {
						append_text_selection_ranges(ranges, block, std::max(0, textSelection->first - position), std::min(length, textSelection->second - position), selectionInk);
					}
					layout->draw(&painter, offset, ranges, exposed);
					if (caret && context.cursorPosition >= position && context.cursorPosition < position + length) {
						layout->drawCursor(&painter, offset, context.cursorPosition - position, cursorWidth());
					}
				}
				offset.ry() += rect.height();
				if (offset.y() > viewportRect.height()) {
					break;
				}
			}
			for (const auto& cachedCursor : CachedTextCursors) {
				const auto block = cachedCursor.block();
				if (block.isValid() && !visibleNumbers.contains(block.blockNumber()) && block.layout() != nullptr) {
					release_text_cache(*block.layout(), painter);
				}
			}
			CachedTextCursors = std::move(visibleCursors);
		}

		void source_pane::paint_line_numbers(QPaintEvent* const aEvent)
		{
			auto painter = QPainter{ Gutter };
			GutterLabels.retain_visible_rows(static_cast<int>(CachedTextCursors.size()));
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
				GutterLabels.draw(painter, QRect{ 0, 0, column - GutterPadding, headerHeight }, QStringLiteral("base"));
				GutterLabels.draw(painter, QRect{ column, 0, column - GutterPadding, headerHeight }, QStringLiteral("head"));
				painter.setClipRect(QRect{ 0, headerHeight, Gutter->width(), Gutter->height() - headerHeight });
			}
			auto top = blockBoundingGeometry(block)
						   .translated(contentOffset())
						   .top()
				+ headerHeight - AppliedOffset;
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
									GutterLabels.draw(painter, QRect{ side == core::diff_side::left ? 0 : columnWidth, qRound(top), columnWidth - GutterPadding, qRound(height) }, QString::number(*number + 1U));
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
						GutterLabels.draw(painter, QRect{ 0, qRound(top), right, qRound(height) }, QString::number(block.blockNumber() + 1));
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
				// [[BPR-1721]]. All the lines of a thread on several.
				const auto row = mark_span_rect(mark);
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
			return source_display_text(
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
				// [[BPR-1525]]. This detached document is not presented yet.
				// Defer layout while filling rather than shape a default rich
				// layout that adoption immediately replaces with the pane's.
				Document->setLayoutEnabled(false);
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
			aDocument.setLayoutEnabled(true);
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
			return source_display_text(
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

	void diff_workbench::initialize_source_find()
	{
		Find         = std::make_unique<find_presentation>();
		Find->Bar    = new source_find_bar{ this };
		Find->Recent = Right;
		// [[BPR-1503]]. The find widget floats over the focused pane and is in
		// no layout, so opening it never takes height from the code.
		Find->Bar->Dispatch = [this](const source_find_gesture aGesture) { dispatch_source_find(aGesture); };
		Find->Bar->installEventFilter(this);
	}
	void diff_workbench::place_source_find()
	{
		if (!Find || Find->Bar->isHidden()) {
			return;
		}
		const QPlainTextEdit* target = Find->Target ? Find->Target.data() : OnePaneEnabled ? Unified
																						   : Right;
		if (!target->isVisible()) {
			target = OnePaneEnabled ? Unified : Right->isVisible() ? Right
																   : Left;
		}
		const auto* viewport = target->viewport();
		const auto  origin   = viewport->mapTo(this, QPoint{ 0, 0 });
		const auto  area     = QRect{ origin, viewport->size() };
		// Inset from the pane's top-right corner as VS Code's widget is. A
		// narrow pane keeps every control: the panel then extends left over
		// the neighbouring pane rather than dropping any of them.
		constexpr auto preferredInset = 8;
		const auto     least          = Find->Bar->fit_to(this->width() - 2 * preferredInset);
		// [[BPR-1527]]. Keep every control at its minimum before reserving
		// outer breathing room; a 242px surface can hold the 230px panel.
		const auto inset       = std::clamp((this->width() - least) / 2, 0, preferredInset);
		const auto wanted      = std::min(Find->Bar->preferred_width(), area.width() - 2 * inset);
		const auto panelWidth  = std::min(std::max(least, wanted), std::max(least, this->width() - 2 * inset));
		const auto panelHeight = Find->Bar->sizeHint().height();
		// Where even that runs out of room on the left, the anchor slides right
		// toward the surface's edge before any control is squeezed.
		const auto left = std::max(0, std::min(area.right() - preferredInset - panelWidth + 1, this->width() - inset - panelWidth));
		Find->Bar->setGeometry(left, area.top() + preferredInset / 2, panelWidth, panelHeight);
		Find->Bar->raise();
	}
	void diff_workbench::set_source_find_dispatch(std::function<void(const source_find_intent&)> aDispatch) { Find->Dispatch = std::move(aDispatch); }
	void diff_workbench::set_source_find_changed(std::function<void()> aChanged) { Find->Changed = std::move(aChanged); }
	auto diff_workbench::source_find() const -> source_find_bar* { return Find->Bar; }
	auto diff_workbench::source_find_available() const -> bool
	{
		if (!CurrentResult || !SemanticIndex || !isVisible()) {
			return false;
		}
		if (!OnePaneEnabled) {
			const auto* focus = QApplication::focusWidget();
			if (focus == Left) {
				return CurrentResult->LeftDocument.has_value();
			}
			if (focus == Right) {
				return CurrentResult->RightDocument.has_value();
			}
		}
		return CurrentResult->LeftDocument || CurrentResult->RightDocument;
	}
	auto diff_workbench::source_find_navigable() const -> bool { return Find->Current.has_value() || Find->ClosedNavigable; }

	auto diff_workbench::source_byte_at(QPlainTextEdit& aPane, const int aPosition) const -> std::optional<std::pair<core::diff_side, std::size_t>>
	{
		if (!CurrentResult) {
			return std::nullopt;
		}
		const auto block = aPane.document()->findBlock(aPosition);
		if (!block.isValid()) {
			return std::nullopt;
		}
		auto side = &aPane == Left ? core::diff_side::left : core::diff_side::right;
		auto line = static_cast<std::uint32_t>(block.blockNumber());
		if (&aPane == Unified) {
			if (line >= UnifiedPresentation->Rows.size()) {
				return std::nullopt;
			}
			const auto& row = UnifiedPresentation->Rows[line];
			side            = row.Head ? core::diff_side::right : core::diff_side::left;
			line            = row.Head.value_or(row.Base.value_or(0U));
		}
		const auto& document = side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		if (!document) {
			return std::nullopt;
		}
		if (line == document->Lines.size() && aPosition == aPane.document()->characterCount() - 1) {
			return std::pair{ side, document->Bytes.size() };
		}
		if (line >= document->Lines.size()) {
			return std::nullopt;
		}
		auto      offset = static_cast<std::size_t>(document->Lines[line].Content.Begin);
		int       units  = 0;
		const int wanted = aPosition - block.position();
		while (units < wanted && offset < document->Lines[line].Content.End) {
			const auto lead  = document->Bytes[offset];
			const auto count = (lead & 0xF8U) == 0xF0U ? 4U : (lead & 0xF0U) == 0xE0U ? 3U
				: (lead & 0xE0U) == 0xC0U                                             ? 2U
																					  : 1U;
			offset += count;
			units += count == 4U ? 2 : 1;
		}
		return units == wanted ? std::optional{ std::pair{ side, offset } } : std::nullopt;
	}
	auto diff_workbench::source_text_position(const core::diff_side aSide, const std::size_t aByte) -> std::optional<int>
	{
		if (!CurrentResult) {
			return std::nullopt;
		}
		auto& positions = Find->Positions[aSide == core::diff_side::left ? 0U : 1U];
		if (const auto saved = positions.find(aByte); saved != positions.end()) {
			return saved->second;
		}
		const auto& document = aSide == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		if (!document || aByte > document->Bytes.size()) {
			return std::nullopt;
		}
		auto* editor = OnePaneEnabled ? Unified : aSide == core::diff_side::left ? Left
																				 : Right;
		if (aByte == document->Bytes.size() && (document->Bytes.empty() || document->HasFinalNewline)) {
			const auto end = editor->document()->characterCount() - 1;
			positions.emplace(aByte, end);
			return end;
		}

		if (document->Lines.empty()) {
			return aByte == 0U ? std::optional{ 0 } : std::nullopt;
		}
		const auto  after = std::ranges::upper_bound(document->Lines, aByte, { }, [](const core::decoded_text_line& aLine) { return aLine.Content.Begin; });
		const auto  index = after == document->Lines.begin() ? 0U : static_cast<std::size_t>(after - document->Lines.begin() - 1);
		const auto& line  = document->Lines[index];
		const auto  row   = OnePaneEnabled ? unified_row_for(aSide, static_cast<std::uint32_t>(index)) : std::optional{ static_cast<std::uint32_t>(index) };
		if (!row) {
			return std::nullopt;
		}
		const auto block = editor->document()->findBlockByNumber(static_cast<int>(*row));
		if (!block.isValid()) {
			return std::nullopt;
		}
		auto begin     = static_cast<std::size_t>(line.Content.Begin);
		auto native    = block.position();
		auto preceding = positions.upper_bound(aByte);
		if (preceding != positions.begin()) {
			--preceding;
			if (preceding->first >= line.Content.Begin && preceding->first <= line.Content.End) {
				begin  = preceding->first;
				native = preceding->second;
			}
		}
		const auto end = std::min<std::uint64_t>(aByte, line.Content.End);
		++Statistics.FindEndpointConversions;
		for (auto offset = begin; offset < end;) {
			const auto lead  = document->Bytes[offset];
			const auto count = (lead & 0xF8U) == 0xF0U ? 4U : (lead & 0xF0U) == 0xE0U ? 3U
				: (lead & 0xE0U) == 0xC0U                                             ? 2U
																					  : 1U;
			offset += count;
			if (offset > end) {
				return std::nullopt;
			}
			native += count == 4U ? 2 : 1;
		}
		positions.emplace(aByte, native);
		return native;
	}

	auto diff_workbench::source_match_fragments(const core::source_match_location& aLocation) -> const std::vector<std::pair<int, int>>&
	{
		const auto key = std::pair{ aLocation.Side, aLocation.Span };
		if (const auto saved = Find->Fragments.find(key); saved != Find->Fragments.end()) {
			return saved->second;
		}
		const auto first = source_text_position(aLocation.Side, aLocation.Span.Begin);
		const auto last  = source_text_position(aLocation.Side, aLocation.Span.End);
		if (!first || !last) {
			return Find->EmptyFragments;
		}
		if (!OnePaneEnabled || aLocation.Span.empty()) {
			return Find->Fragments.emplace(key, std::vector<std::pair<int, int>>{ { *first, *last } }).first->second;
		}
		const auto& document = aLocation.Side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		if (!document) {
			return Find->EmptyFragments;
		}
		auto pieces = std::vector<std::pair<int, int>>{ };
		auto line   = std::ranges::upper_bound(document->Lines, aLocation.Span.Begin, { }, [](const core::decoded_text_line& aLine) { return aLine.Content.Begin; });
		if (line != document->Lines.begin()) {
			--line;
		}
		for (; line != document->Lines.end() && line->Content.Begin < aLocation.Span.End; ++line) {
			const auto from = source_text_position(aLocation.Side, std::max(line->Content.Begin, aLocation.Span.Begin));
			const auto to   = source_text_position(aLocation.Side, std::min(line->Content.End, aLocation.Span.End));
			if (!from || !to) {
				return Find->EmptyFragments;
			}
			auto end = *to;
			if (aLocation.Span.End >= line->Terminator.End && line->Terminator.End > line->Content.End) {
				end = std::min(end + 1, Unified->document()->characterCount() - 1);
			}
			if (!pieces.empty() && pieces.back().second == *from) {
				pieces.back().second = end;
			} else {
				pieces.emplace_back(*from, end);
			}
		}
		return Find->Fragments.emplace(key, std::move(pieces)).first->second;
	}
	auto diff_workbench::source_search_seed() const -> QString
	{
		const auto* target  = OnePaneEnabled ? Unified : Find->Recent && Find->Recent->isVisible() ? Find->Recent.data()
																								   : Right;
		const auto* focused = QApplication::focusWidget();
		if (focused == Left || focused == Right || focused == Unified) {
			target = static_cast<const QPlainTextEdit*>(focused);
		}
		const auto cursor = target->textCursor();
		if (!cursor.hasSelection()) {
			return { };
		}
		if (cursor.selectionEnd() - cursor.selectionStart() > static_cast<int>(core::maximum_source_search_query_bytes)) {
			return QString{ static_cast<qsizetype>(core::maximum_source_search_query_bytes + 1U), QLatin1Char(' ') };
		}
		return cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
	}
	void diff_workbench::open_source_find()
	{
		if (!source_find_available()) {
			return;
		}
		auto* target = OnePaneEnabled ? Unified : Find->Recent && Find->Recent->isVisible() && ((Find->Recent == Left && CurrentResult->LeftDocument) || (Find->Recent == Right && CurrentResult->RightDocument)) ? Find->Recent.data()
			: CurrentResult->RightDocument                                                                                                                                                                        ? Right
																																																				  : Left;
		if (auto* focused = QApplication::focusWidget(); focused == Left || focused == Right || focused == Unified) {
			target = static_cast<QPlainTextEdit*>(focused);
		}
		const bool rebind   = Find->Rebind;
		const bool retarget = rebind || Find->Target != target || Find->Bar->isHidden();
		Find->Rebind        = false;
		if (!retarget) {
			Find->Bar->seed({ }, false);
			return;
		}
		Find->Target = target;
		Find->Fixed.reset();
		Find->Capture.reset();
		Find->Bar->clear_selection();
		auto& intent          = Find->Intent;
		intent.Target         = target == Unified ? core::source_search_target::unified : target == Left ? core::source_search_target::left
																										 : core::source_search_target::right;
		auto       cursor     = target->textCursor();
		const auto first      = source_byte_at(*target, cursor.selectionStart());
		const auto last       = source_byte_at(*target, cursor.selectionEnd());
		bool       contiguous = first && last && first->first == last->first;
		if (contiguous && target == Unified) {
			const auto beginRow = target->document()->findBlock(cursor.selectionStart()).blockNumber();
			const auto endRow   = target->document()->findBlock(std::max(cursor.selectionStart(), cursor.selectionEnd() - 1)).blockNumber();
			auto       previous = std::optional<std::uint32_t>{ };
			for (int row = beginRow; row <= endRow && contiguous; ++row) {
				const auto& identity = UnifiedPresentation->Rows[static_cast<std::size_t>(row)];
				const auto  actual   = first->first == core::diff_side::left ? identity.Base : identity.Head;
				contiguous           = actual && (!previous || *actual == *previous + 1U);
				previous             = actual;
			}
		}
		if (cursor.hasSelection() && contiguous && last->second > first->second) {
			Find->Capture = core::source_search_range{ first->first, { first->second, last->second } };
		}
		const auto caretIndex = target == Left ? 0U : target == Right ? 1U
																	  : 2U;
		const bool largeSeed  = cursor.hasSelection() && cursor.selectionEnd() - cursor.selectionStart() > static_cast<int>(core::maximum_source_search_query_bytes);
		auto       seed       = largeSeed ? QString{ static_cast<qsizetype>(core::maximum_source_search_query_bytes + 1U), QLatin1Char(' ') } : cursor.hasSelection() ? cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'))
																																									  : QString{ };
		// [[BPR-1503]]. As VS Code does, the current match is the occurrence
		// the seed came from: a selection or the word under the caret is
		// searched from its start, so it is "1 of N" there rather than the
		// next occurrence after it.
		auto originPosition = Find->Recall || target->textCursor().hasSelection() ? target->textCursor().selectionStart() : target->textCursor().position();
		if (seed.isEmpty() && Find->RealCaret[caretIndex]) {
			cursor.select(QTextCursor::WordUnderCursor);
			seed = cursor.selectedText();
			if (!seed.isEmpty()) {
				originPosition = cursor.selectionStart();
			}
		}
		auto origin = source_byte_at(*target, originPosition);
		if (!Find->RealCaret[caretIndex] && !target->textCursor().hasSelection() && Interaction.Selection) {
			const auto  side     = Interaction.Selection->PreferredLine.Side;
			const auto& document = side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
			if (document && Interaction.Selection->PreferredLine.Line < document->Lines.size()) {
				origin = std::pair{ side, document->Lines[Interaction.Selection->PreferredLine.Line].Content.Begin };
			}
		}
		intent.OriginSide = origin ? origin->first : target == Left ? core::diff_side::left
																	: core::diff_side::right;
		intent.OriginByte = origin ? origin->second : 0U;
		intent.OriginRow  = static_cast<std::uint32_t>(std::max(0, target->textCursor().blockNumber()));
		Find->Bar->set_scope(intent.Target, Find->Capture.has_value(), target == Unified ? QStringLiteral("Select a contiguous range from one actual source version") : QStringLiteral("Select source text first"));
		Find->Bar->show();
		place_source_find();
		Find->Bar->seed(Find->Recall || rebind ? QString{ } : seed, true);
		Find->Recall = false;
		dispatch_source_find(source_find_gesture::edit);
		if (Find->Changed) {
			Find->Changed();
		}
	}
	void diff_workbench::dispatch_source_find(const source_find_gesture aGesture)
	{
		if (aGesture == source_find_gesture::close && !Find->Bar->isHidden()) {
			close_source_find();
			return;
		}
		auto intent    = Find->Intent;
		intent.Gesture = aGesture;
		intent.Query   = Find->Bar->query();
		intent.Options = Find->Bar->options();
		if (aGesture == source_find_gesture::selection) {
			Find->Fixed = Find->Bar->in_selection() ? Find->Capture : std::nullopt;
		}
		intent.Selection = Find->Fixed;
		if (aGesture == source_find_gesture::next || aGesture == source_find_gesture::previous) {
			Find->ExternalSelection = false;
			Find->Navigating        = true;
		}
		if (aGesture == source_find_gesture::query_dirty) {
			Find->ExternalSelection = false;
			Find->ClosedNavigable   = false;
			Find->Records.reset();
			Find->PendingReveal.reset();
			Find->Current.reset();
			for (auto& list : Find->Decorations) {
				list.clear();
			}
			update_diff_paint();
			refresh_find_ticks();
		}
		if (Find->Dispatch) {
			Find->Dispatch(intent);
		}
	}
	void diff_workbench::navigate_source_find(const core::change_navigation_direction aDirection)
	{
		if (Find->Bar->isHidden()) {
			Find->ReopenDirection = aDirection;
			Find->Recall          = true;
			open_source_find();
			return;
		}
		dispatch_source_find(aDirection == core::change_navigation_direction::next ? source_find_gesture::next : source_find_gesture::previous);
	}
	void diff_workbench::close_source_find()
	{
		Find->ReopenDirection.reset();
		Find->ClosedNavigable = Find->Current.has_value();
		Find->PendingReveal.reset();
		Find->Bar->hide();
		dispatch_source_find(source_find_gesture::close);
		Find->Records.reset();
		Find->Current.reset();
		for (auto& list : Find->Decorations) {
			list.clear();
		}
		update_diff_paint();
		refresh_find_ticks();
		if (Find->Target && Find->Target->isVisible()) {
			Find->Target->setFocus(Qt::ShortcutFocusReason);
		}
		if (Find->Changed) {
			Find->Changed();
		}
	}
	void diff_workbench::retarget_source_find()
	{
		clear_source_find_document();
		if (Find->Rebind) {
			QTimer::singleShot(0, this, [this] {
				if (Find->Rebind && !Find->Bar->isHidden() && source_find_available()) {
					open_source_find();
				}
			});
		}
	}
	void diff_workbench::clear_source_find_document()
	{
		if (!Find) {
			return;
		}
		Find->CopyMatch.reset();
		Find->ReopenDirection.reset();
		Find->MappedRecords.reset();
		Find->ClosedNavigable = false;
		Find->Rebind          = !Find->Bar->isHidden();
		Find->PendingReveal.reset();
		Find->Fragments.clear();
		for (auto& positions : Find->Positions) {
			positions.clear();
		}
		Find->Capture.reset();
		Find->Fixed.reset();
		Find->Records.reset();
		Find->Current.reset();
		Find->RealCaret = { };
		Find->Bar->clear_selection();
		for (auto& list : Find->Decorations) {
			list.clear();
		}
		dispatch_source_find(source_find_gesture::document_changed);
		refresh_find_ticks();
		if (Find->Changed) {
			Find->Changed();
		}
	}
	auto diff_workbench::reveal_source_match(const core::source_match_location& aLocation, const bool aExternal) -> bool
	{
		if (!CurrentContext || !CurrentResult || !SemanticIndex || geometry_pending()) {
			return false;
		}
		const auto blob = aLocation.Side == core::diff_side::left ? CurrentContext->Key.LeftBlob : CurrentContext->Key.RightBlob;
		if (blob != std::optional{ aLocation.Blob }) {
			return false;
		}
		if (aExternal) {
			Find->ExternalSelection = true;
			Find->PendingReveal.reset();
		}
		const auto& pieces = source_match_fragments(aLocation);
		if (pieces.empty()) {
			return false;
		}
		const auto first = std::optional{ pieces.front().first };
		const auto last  = std::optional{ pieces.front().second };
		if (!first || !last || *last < *first) {
			return false;
		}
		const auto  line     = core::source_line_ref{ CurrentResult->Alignment.Digest, aLocation.Side, aLocation.Line };
		const auto& document = aLocation.Side == core::diff_side::left ? CurrentResult->LeftDocument : CurrentResult->RightDocument;
		const bool  terminal = document && aLocation.Span.empty() && aLocation.Span.Begin == document->Bytes.size() && aLocation.Line == document->Lines.size();
		const auto  selected = core::select_line(*SemanticIndex, line);
		if (!selected && !terminal) {
			return false;
		}
		// Qt's cursor may scroll its own pane: restore the comparison position,
		// then perform the only vertical reveal through the existing controller.
		const auto shared = Controller->position();
		auto*      editor = OnePaneEnabled ? Unified : aLocation.Side == core::diff_side::left ? Left
																							   : Right;
		auto       cursor = QTextCursor{ editor->document() };
		cursor.setPosition(*first);
		cursor.setPosition(*last, QTextCursor::KeepAnchor);
		editor->setTextCursor(cursor);
		Find->CopyMatch  = aLocation;
		Find->CopyCursor = cursor;
		if (!OnePaneEnabled) {
			static_cast<void>(Controller->set_position(shared));
		}
		Interaction.Selection = selected;
		if (selected) {
			static_cast<void>(reveal_exact_line(*selected, line));
		} else if (OnePaneEnabled) {
			reveal_unified_row(static_cast<std::uint32_t>(std::max(0, editor->document()->findBlock(*first).blockNumber())));
		} else {
			Controller->end();
		}
		auto beginCursor = cursor;
		beginCursor.setPosition(*first);
		const auto native = editor->cursorRect(beginCursor);
		if (native.left() < 0) {
			editor->horizontalScrollBar()->setValue(editor->horizontalScrollBar()->value() + native.left());
		} else if (native.right() > editor->viewport()->width()) {
			editor->horizontalScrollBar()->setValue(editor->horizontalScrollBar()->value() + native.right() - editor->viewport()->width());
		}
		update_detail();
		update_diff_paint();
		emit_selection();
		return true;
	}
	void diff_workbench::observe_source_find_execution(const std::uint64_t aMatching, const std::uint64_t aCached)
	{
		Statistics.FindMatchingRuns = aMatching;
		Statistics.FindCachedRuns   = aCached;
	}
	void diff_workbench::present_source_find(const application::source_search_projection& aProjection)
	{
		if (aProjection.Scope && (!CurrentContext || !CurrentResult || aProjection.Scope->Document != std::optional{ core::source_search_document{ CurrentContext->Key.File, CurrentResult->Alignment.Digest, CurrentContext->Key.LeftBlob, CurrentContext->Key.RightBlob } })) {
			return;
		}
		const bool recordsChanged = Find->Records != aProjection.Matches;
		if (recordsChanged) {
			if (aProjection.Matches && Find->MappedRecords.lock() != aProjection.Matches) {
				Find->Fragments.clear();
				for (auto& positions : Find->Positions) {
					positions.clear();
				}
				Find->MappedRecords = aProjection.Matches;
			}
			Find->Records = aProjection.Matches;
			for (auto& list : Find->Decorations) {
				list.clear();
			}
			if (aProjection.Matches && aProjection.Scope) {
				const auto target    = aProjection.Scope->Target;
				const auto paneIndex = target == core::source_search_target::left ? 0U : target == core::source_search_target::right ? 1U
																																	 : 2U;
				auto&      list      = Find->Decorations[paneIndex];
				for (std::size_t index = 0U; index < aProjection.Matches->size(); ++index) {
					const auto& record   = (*aProjection.Matches)[index].Match;
					const auto& location = target == core::source_search_target::left ? record.Locations[0] : record.Locations[1] ? record.Locations[1]
																																  : record.Locations[0];
					if (!location) {
						continue;
					}
					for (const auto& [begin, end] : source_match_fragments(*location)) {
						list.push_back({ begin, end, 0, static_cast<std::uint32_t>(index) });
					}
				}
				std::ranges::sort(list, { }, &find_presentation::decoration::Begin);
				int maximum = 0;
				for (auto& item : list) {
					maximum        = std::max(maximum, item.End);
					item.PrefixEnd = maximum;
				}
			}
		}
		if (aProjection.CurrentMatch && Find->ReopenDirection) {
			const auto direction = *Find->ReopenDirection;
			Find->ReopenDirection.reset();
			dispatch_source_find(direction == core::change_navigation_direction::next ? source_find_gesture::next : source_find_gesture::previous);
			return;
		}
		const bool reveal = aProjection.CurrentMatch && !Find->ExternalSelection && (Find->Navigating || Find->Current != aProjection.CurrentMatch || Find->Bar->projection().Matches != aProjection.Matches);
		Find->Current     = aProjection.CurrentMatch;
		Find->Bar->present(aProjection);
		if (reveal && aProjection.Matches && *aProjection.CurrentMatch < aProjection.Matches->size()) {
			const auto& record   = (*aProjection.Matches)[*aProjection.CurrentMatch].Match;
			const auto& location = aProjection.Scope && aProjection.Scope->Target == core::source_search_target::left ? record.Locations[0] : record.Locations[1] ? record.Locations[1]
																																								  : record.Locations[0];
			if (location) {
				if (Find->Navigating) {
					Find->Intent.OriginSide = location->Side;
					Find->Intent.OriginByte = location->Span.Begin;
					Find->Intent.OriginRow  = record.UnifiedRow;
				}
				Find->PendingReveal = *location;
				if (reveal_source_match(*location)) {
					Find->PendingReveal.reset();
				}
			}
		}
		Find->Navigating = false;
		update_diff_paint();
		if (recordsChanged) {
			refresh_find_ticks();
		}
		if (Find->Changed) {
			Find->Changed();
		}
	}
	void diff_workbench::append_find_selections(QPlainTextEdit& aPane, QList<QTextEdit::ExtraSelection>& aSelections)
	{
		if (!Find) {
			return;
		}
		pane(&aPane)->FindOutlines.clear();
		if (Find->Bar->isHidden()) {
			return;
		}
		const auto& records = Find->Decorations[&aPane == Left ? 0U : &aPane == Right ? 1U
																					  : 2U];
		const auto  visible = paintable_line_bounds(aPane);
		const auto  start   = aPane.document()->findBlockByNumber(static_cast<int>(visible.Begin));
		const auto  finish  = aPane.document()->findBlockByNumber(static_cast<int>(visible.End));
		const auto  begin   = start.isValid() ? start.position() : 0;
		const auto  end     = finish.isValid() ? finish.position() : aPane.document()->characterCount();
		auto        at      = std::ranges::lower_bound(records, begin, { }, &find_presentation::decoration::PrefixEnd);
		for (; at != records.end() && at->Begin < end; ++at) {
			if (at->End < begin) {
				continue;
			}
			++Statistics.FindHighlightsVisited;
			auto selection   = QTextEdit::ExtraSelection{ };
			selection.cursor = QTextCursor{ aPane.document() };
			selection.cursor.setPosition(at->Begin);
			selection.cursor.setPosition(at->End, QTextCursor::KeepAnchor);
			// [[BPR-1503]]. Every match carries the theme's find wash, and the
			// current one its stronger wash plus an outline, composited at the
			// selection's opacity so the contrast checks hold for both.
			const bool current = Find->Current == std::optional{ at->Match };
			auto       color   = find_wash(current);
			color.setAlpha(core::selection_wash_alpha);
			selection.format.setBackground(color);
			aSelections.push_back(selection);
			if (current) {
				pane(&aPane)->FindOutlines.push_back(selection.cursor);
			}
		}
	}
	void diff_workbench::refresh_find_ticks()
	{
		if (!Find) {
			return;
		}
		auto shared  = std::vector<qreal>{ };
		auto unified = std::vector<qreal>{ };
		if (Find->Records && !Find->Bar->isHidden()) {
			for (const auto& record : *Find->Records) {
				const auto& location = Find->Intent.Target == core::source_search_target::left ? record.Match.Locations[0] : record.Match.Locations[1] ? record.Match.Locations[1]
																																					   : record.Match.Locations[0];
				if (!location) {
					continue;
				}
				if (const auto fraction = Controller->scrollbar_fraction(location->Side, location->Line)) {
					shared.push_back(*fraction);
				} else if (location->Span.empty()) {
					shared.push_back(1.0);
				}
				if (const auto row = unified_row_for(location->Side, location->Line)) {
					unified.push_back(static_cast<qreal>(*row) / std::max(1, Unified->document()->blockCount() - 1));
				}
			}
		}
		static_cast<thread_tick_scroll_bar*>(SharedScroll)->set_find_ticks(std::move(shared));
		static_cast<thread_tick_scroll_bar*>(Unified->verticalScrollBar())->set_find_ticks(std::move(unified));
	}

	namespace {
		/// [[RFC-BPR-0090]]. Tells its owner when the watched widget resizes.
		class resize_follower final : public QObject
		{
		public:
			resize_follower(QWidget& aWatched, std::function<void()> aResized)
				: QObject{ &aWatched }
				, Resized{ std::move(aResized) }
			{
				aWatched.installEventFilter(this);
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::Resize && Resized) {
					Resized();
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			std::function<void()> Resized;
		};
	} // namespace

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
		[[nodiscard]] auto range() const -> std::pair<std::uint64_t, std::uint64_t>
		{
			const auto editable = Editor != nullptr ? Editor->model().editable_range() : std::nullopt;
			return editable.value_or(std::pair<std::uint64_t, std::uint64_t>{ First, Last });
		}
		[[nodiscard]] auto count() const -> std::uint64_t
		{
			const auto [first, last] = range();
			return last - first + 1U;
		}
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
		// [[BPR-1576]]. The comparison's wait is said over the panes in the
		// shape of code; the status line above keeps saying the outcome.
		Veil = new pending_veil{ *this, pending_shape::code, QStringLiteral("Comparison"), top };
		Veil->set_announces(false);
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
		initialize_source_find();
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
			// [[BPR-1724]]. A click, Shift-click or drag on the numbers.
			pane(source)->set_number_dispatch([this, source](const std::uint32_t aRow, const bool aExtend) { select_numbered_line(source, aRow, aExtend); });
		}
		pane(Unified)->set_scroll_observer([this] {
			update_unified_paint();
			if (FrameObserver) {
				FrameObserver();
			}
		});
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

	auto diff_workbench::head_edit_row(const std::uint32_t aHeadLine) const -> std::uint64_t
	{
		const auto& edit = *HeadEdit;
		if (aHeadLine < edit.First) {
			return aHeadLine;
		}
		if (aHeadLine <= edit.Last) {
			return edit.First + std::min<std::uint64_t>(aHeadLine - edit.First, edit.count() - 1U);
		}
		return std::uint64_t{ aHeadLine } + edit.count() - edit.OriginalCount;
	}

	auto diff_workbench::head_line_of_edit_row(const std::uint64_t aRow) const -> std::uint32_t
	{
		const auto& edit  = *HeadEdit;
		const auto  count = edit.count();
		if (aRow < edit.First) {
			return static_cast<std::uint32_t>(aRow);
		}
		if (aRow < edit.First + count) {
			return edit.First + static_cast<std::uint32_t>(std::min<std::uint64_t>(aRow - edit.First, edit.OriginalCount - 1U));
		}
		return static_cast<std::uint32_t>(aRow - count + edit.OriginalCount);
	}

	auto diff_workbench::begin_head_edit(const head_edit_request& aRequest) -> suggestion_editor*
	{
		end_head_edit();
		if (!CurrentResult || !CurrentResult->RightDocument.has_value() || aRequest.First > aRequest.Last) {
			return nullptr;
		}
		const auto& document = *CurrentResult->RightDocument;
		if (document.Bytes.size() > core::maximum_suggestion_editor_bytes || aRequest.Last >= document.Lines.size()) {
			return nullptr;
		}
		auto text  = std::string{ document.Bytes.begin(), document.Bytes.end() };
		auto lines = core::split_text_lines(text);
		if (aRequest.Last >= lines.size()) {
			return nullptr;
		}
		auto spanCount = static_cast<std::uint64_t>(aRequest.Last - aRequest.First) + 1U;
		if (aRequest.Span.has_value() && !aRequest.Span->empty()) {
			const auto from = lines.begin() + aRequest.First;
			lines.erase(from, from + static_cast<std::ptrdiff_t>(spanCount));
			lines.insert(lines.begin() + aRequest.First, aRequest.Span->begin(), aRequest.Span->end());
			spanCount = aRequest.Span->size();
			text      = core::join_text_lines(lines);
		}

		auto edit           = std::make_unique<head_edit>();
		edit->First         = aRequest.First;
		edit->Last          = aRequest.Last;
		edit->OriginalCount = aRequest.Last - aRequest.First + 1U;
		edit->OnePane       = OnePaneEnabled;
		auto* const host    = OnePaneEnabled ? Unified : Right;
		edit->Host          = host;
		// The head pane's own fills, per head line.
		edit->Fills.assign(document.Lines.size(), QColor{ });
		if (Colors.Supplied) {
			for (const auto& segment : CurrentResult->Alignment.Segments) {
				const auto colour = moved(segment.Id) ? Colors.Moved
					: segment.Kind == core::alignment_kind::right_only || segment.Kind == core::alignment_kind::replacement
					? Colors.Added
					: QColor{ };
				for (auto line = segment.Right.Begin; colour.isValid() && line < segment.Right.End && line < edit->Fills.size(); ++line) {
					edit->Fills[line] = colour;
				}
			}
		}

		auto* const editor = new suggestion_editor{ host };
		editor->setObjectName(QStringLiteral("diffHeadSuggestionEditor"));
		editor->setAccessibleName(QStringLiteral("Suggested change to lines %1 to %2").arg(aRequest.First + 1U).arg(aRequest.Last + 1U));
		editor->setFrameShape(QFrame::NoFrame);
		editor->set_code_font(host->font());
		editor->set_contrast_variant(Colors.Variant);
		const auto column = std::max(1.0, QFontMetricsF{ host->font() }.horizontalAdvance(QLatin1Char{ 'M' }));
		const auto tab    = static_cast<std::uint32_t>(std::max(1.0, std::round(host->tabStopDistance() / column)));
		editor->set_text(text, tab);
		editor->set_indentation(core::indentation_for({ }, text));
		editor->set_editable_lines(aRequest.First, aRequest.First + spanCount - 1U);
		if (RightSpans) {
			const auto classified = classify_text_lines(std::span{ document.Bytes }, RightSpans);
			editor->set_syntax(suggestion_syntax_lines(classified, std::string_view{ reinterpret_cast<const char*>(document.Bytes.data()), document.Bytes.size() }, Colors.Syntax));
		}
		HeadEdit                    = std::move(edit);
		const auto gutterBackground = Colors.Syntax[static_cast<std::size_t>(core::appearance_role::code_gutter_background)];
		const auto gutterText       = Colors.Syntax[static_cast<std::size_t>(core::appearance_role::code_line_number_text)];
		auto       gutterWidth      = pane(Right)->gutter_width();
		if (OnePaneEnabled || gutterWidth <= 0) {
			const auto digits = QString::number(document.Lines.size() + spanCount).size();
			gutterWidth       = qCeil(QFontMetricsF{ host->font() }.horizontalAdvance(QString(digits, QLatin1Char{ '9' }))) + 3 * GutterPadding;
		}
		editor->set_gutter(suggestion_editor::gutter_presentation{
			.Width      = gutterWidth,
			.Padding    = GutterPadding,
			.Font       = host->font(),
			.Background = gutterBackground,
			.Text       = gutterText,
			.Number     = [this](const std::uint64_t aRow) -> QString {
				if (!HeadEdit) {
					return { };
				}
				const auto& current = *HeadEdit;
				// A row the edit added has no number in the head file yet.
				if (aRow >= current.First && aRow < current.First + current.count() && aRow - current.First >= current.OriginalCount) {
					return { };
				}
				return QString::number(head_line_of_edit_row(aRow) + 1U);
			},
		});
		editor->set_row_fill([this](const std::uint64_t aRow) -> std::optional<QColor> {
			if (!HeadEdit) {
				return std::nullopt;
			}
			const auto& current = *HeadEdit;
			if (aRow >= current.First && aRow < current.First + current.count() && aRow - current.First >= current.OriginalCount) {
				return std::nullopt;
			}
			const auto line = head_line_of_edit_row(aRow);
			return line < current.Fills.size() && current.Fills[line].isValid() ? std::optional{ current.Fills[line] } : std::nullopt;
		});
		editor->set_range_colours(aRequest.Wash, aRequest.Edge, aRequest.Flash);
		editor->set_empty_range_note(aRequest.EmptyNote);
		editor->setGeometry(host->rect());
		editor->Changed = [this, changed = aRequest.Changed] {
			sync_head_edit();
			if (changed) {
				changed();
			}
		};
		HeadEdit->Ended    = aRequest.Ended;
		HeadEdit->Editor   = editor;
		HeadEdit->Follower = new resize_follower{ *host, [this] {
													 if (HeadEdit && HeadEdit->Editor && HeadEdit->Host) {
														 HeadEdit->Editor->setGeometry(HeadEdit->Host->rect());
														 sync_head_edit();
													 }
												 } };
		if (!OnePaneEnabled) {
			// The comparison stays the one thing that scrolls; the edit follows it.
			editor->set_external_scroll(true);
			editor->Wheeled    = [this](QWheelEvent* const aEvent) { static_cast<void>(scroll_by_wheel(*aEvent)); };
			editor->Reveal     = [this](const std::uint64_t aRow) { reveal_head_edit_row(aRow); };
			HeadEdit->Scrolled = connect(Right->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { sync_head_edit(); });
		}
		editor->horizontalScrollBar()->setValue(host->horizontalScrollBar()->value());
		editor->show();
		editor->raise();
		// One pane's map draws the interleaved rows, which the edit hides.
		if (OnePaneEnabled) {
			UnifiedMap->hide();
		}
		if (OnePaneEnabled) {
			// The range where its head rows were in the interleaved view.
			auto above = std::uint64_t{ 0U };
			if (const auto row = unified_row_for(core::diff_side::right, aRequest.First); row.has_value()) {
				const auto rect = pane(Unified)->line_row_rect(*row);
				if (!rect.isEmpty() && rect.top() > 0) {
					above = static_cast<std::uint64_t>(std::floor(static_cast<qreal>(rect.top()) / std::max<qreal>(1.0, editor->line_height())));
				}
			}
			editor->scroll_to_line(aRequest.First > above ? aRequest.First - above : 0U);
		}
		// The caret at the end of the last selected line.
		const auto last = aRequest.First + spanCount - 1U;
		static_cast<void>(editor->apply(core::place_caret{ .At = { last, editor->model().line_width(last) } }));
		sync_head_edit();
		editor->setFocus(Qt::OtherFocusReason);
		return editor;
	}

	void diff_workbench::end_head_edit()
	{
		if (!HeadEdit) {
			return;
		}
		auto edit = std::move(HeadEdit);
		QObject::disconnect(edit->Scrolled);
		if (edit->Follower) {
			delete edit->Follower.data();
		}
		const auto hadFocus = edit->Editor && edit->Editor->hasFocus();
		if (edit->Editor) {
			// Gestures may still be on the editor's stack: it goes once they return.
			edit->Editor->hide();
			edit->Editor->deleteLater();
		}
		LinkMap->set_dimmed_from(std::nullopt);
		if (edit->OnePane && OnePaneEnabled) {
			UnifiedMap->show();
		}
		if (hadFocus && edit->Host) {
			edit->Host->setFocus(Qt::OtherFocusReason);
		}
		if (edit->Ended) {
			edit->Ended();
		}
	}

	auto diff_workbench::head_editor() const noexcept -> suggestion_editor*
	{
		return HeadEdit ? HeadEdit->Editor.data() : nullptr;
	}

	auto diff_workbench::head_edit_lines() const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>
	{
		return HeadEdit ? std::optional{ std::pair{ HeadEdit->First, HeadEdit->Last } } : std::nullopt;
	}

	void diff_workbench::sync_head_edit()
	{
		if (!HeadEdit || !HeadEdit->Editor || HeadEdit->OnePane) {
			return;
		}
		auto* const editor = HeadEdit->Editor.data();
		auto* const source = pane(Right);
		const auto  top    = source->top_visible_line();
		if (!top) {
			return;
		}
		const auto row = source->exact_line_top(*top);
		if (!row) {
			return;
		}
		// Where the head pane's top row is, in the editor's viewport, and the
		// head's fractional line at the editor's top edge.
		const auto pitch  = std::max<qreal>(1.0, editor->line_height());
		const auto y      = *row + static_cast<qreal>(Right->viewport()->mapTo(Right, QPoint{ }).y() - editor->viewport()->mapTo(Right, QPoint{ }).y());
		const auto head   = static_cast<qreal>(*top) - y / pitch;
		const auto first  = static_cast<qreal>(HeadEdit->First);
		const auto before = static_cast<qreal>(HeadEdit->OriginalCount);
		const auto after  = static_cast<qreal>(HeadEdit->count());
		// Rows above the range meet their head rows exactly, the range's rows
		// pass at the rate its length differs, and rows below meet theirs once
		// the range has gone above the top: the edit scrolls without a jump.
		const auto row0 = head < first ? head : head < first + before ? first + (head - first) * after / before
																	  : head + after - before;
		editor->set_vertical_offset(row0 * pitch);
		// Ribbons that touch or follow the range no longer meet their rows.
		const auto rangeRow = source->line_row_rect(HeadEdit->First);
		auto       dimFrom  = std::optional<qreal>{ };
		if (!rangeRow.isEmpty()) {
			dimFrom = static_cast<qreal>(LinkMap->mapFrom(Comparison, Right->viewport()->mapTo(Comparison, QPoint{ 0, rangeRow.top() })).y());
		} else if (*top > HeadEdit->First) {
			dimFrom = -1.0e9;
		}
		LinkMap->set_dimmed_from(dimFrom);
	}

	void diff_workbench::reveal_head_edit_row(const std::uint64_t aRow)
	{
		if (!HeadEdit || !HeadEdit->Editor || HeadEdit->OnePane) {
			return;
		}
		auto* const editor  = HeadEdit->Editor.data();
		const auto  pitch   = std::max<qreal>(1.0, editor->line_height());
		const auto  overrun = [editor, pitch, aRow] {
			const auto top    = static_cast<qreal>(aRow) * pitch - editor->vertical_offset();
			const auto height = static_cast<qreal>(editor->viewport()->height());
			return top < 0.0 ? top : top + pitch > height ? top + pitch - height
														  : 0.0;
		};
		// The comparison moves by the pixels the row is out of view. The edit
		// covers the head pane, so its viewport's own area and clip say where
		// its rows show, not what is visible of it. The reveal measures the
		// pane's origin, which can land a few pixels short: it goes again while
		// that brings the row nearer.
		for (auto attempt = 0; attempt < 3; ++attempt) {
			const auto before = overrun();
			if (std::abs(before) < 0.5 || !HeadEdit || !HeadEdit->Editor) {
				return;
			}
			const auto clip    = Right->viewport()->mask();
			const auto visible = clip.isEmpty() ? Right->viewport()->rect() : clip.boundingRect().intersected(Right->viewport()->rect());
			if (visible.isEmpty()) {
				return;
			}
			const auto shift = static_cast<int>(before < 0.0 ? std::floor(before) : std::ceil(before));
			const auto rect  = shift < 0 ? QRect{ 0, visible.top() + shift, 1, 1 } : QRect{ 0, visible.bottom() + shift, 1, 1 };
			reveal_keyboard_row(Right, rect, visible);
			if (std::abs(overrun()) >= std::abs(before)) {
				return;
			}
		}
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
		// [[RFC-BPR-0090]]. An edit holds the text it was opened on.
		end_head_edit();
		const auto previousSelection = Interaction.Selection;
		HeldLine.reset();
		for (auto* source : { Left, Right, Unified }) {
			pane(source)->reset_pixel_gesture();
			pane(source)->set_within_line_offset(0.0);
		}
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
		ComparisonLoading = false;
		apply_pending();
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
		ComparisonLoading = false;
		apply_pending();
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
		ComparisonLoading = true;
		apply_pending();
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
		{
			const auto outline = find_outline_color(aProfile.palette()[core::index_of(role::find_current_match_background)], aProfile.palette()[core::index_of(role::code_background)]);
			for (auto* const source : { Left, Right, Unified }) {
				pane(source)->FindOutlineColor = outline;
			}
			if (Find) {
				Find->Bar->adopt_appearance(aProfile);
			}
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
		if (ComparisonLoading && CurrentContext.has_value()) {
			Veil->present(core::pending_activity::loading, false, false,
						  { .Loading = QStringLiteral("Loading the comparison…"),
							.Waiting = QStringLiteral("Still preparing the comparison: a large file takes longer.") });
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
} // namespace bpr::qt
