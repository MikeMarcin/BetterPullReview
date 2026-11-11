#include "diff_workbench_support.hpp"

#include "source_display_text.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QBrush>
#include <QChar>
#include <QCoreApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QInputEvent>
#include <QKeyEvent>
#include <QLatin1Char>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QSet>
#include <QSinglePointEvent>
#include <QSizeF>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextFormat>
#include <QTextLine>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <tuple>

namespace bpr::qt {
	namespace {
		/// \brief `aOver`, at its own opacity, laid over `aUnder`, as the
		/// gutter's painter lays a tint or a wash.
		[[nodiscard]] auto laid(const QColor& aOver, const core::appearance_color aUnder) noexcept -> core::appearance_color
		{
			auto opaque  = to_appearance_color(aOver);
			opaque.Alpha = 255U;
			return core::composited(opaque, aUnder, static_cast<std::uint8_t>(aOver.alpha()));
		}

		/// \brief The margin a marked number's ink keeps over the text floor.
		constexpr auto number_blend_margin_hundredths = 10U;

		/// \brief The opacity the gutter washes an open thread's lines with.
		constexpr auto open_wash_opacity = 0.18F;
		/// \brief The opacity the gutter washes my draft's lines with while it is written.
		constexpr auto writing_wash_opacity = 0.16F;

		[[nodiscard]] auto with_opacity(QColor aColour, const float aOpacity) -> QColor
		{
			aColour.setAlphaF(aOpacity);
			return aColour;
		}
	} // namespace

	thread_tick_scroll_bar::thread_tick_scroll_bar(const Qt::Orientation aOrientation, QWidget* const aParent)
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

	thread_tick_scroll_bar::~thread_tick_scroll_bar()
	{
		watch_shift(false);
	}

	void thread_tick_scroll_bar::set_find_ticks(std::vector<qreal> aFractions)
	{
		FindFractions = std::move(aFractions);
		std::ranges::sort(FindFractions);
		update();
	}

	void thread_tick_scroll_bar::set_ticks(std::vector<diff_thread_tick> aTicks)
	{
		if (Ticks == aTicks) {
			return;
		}
		Ticks = std::move(aTicks);
		follow_pointer();
		update();
	}

	auto thread_tick_scroll_bar::ticks() const noexcept -> const std::vector<diff_thread_tick>&
	{
		return Ticks;
	}

	auto thread_tick_scroll_bar::tick_rect(const diff_thread_tick& aTick) const -> QRect
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

	auto thread_tick_scroll_bar::tooltip_of(const diff_thread_tick& aTick) const -> anchored_tooltip_target
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

	auto thread_tick_scroll_bar::tick_at(const QPoint aPosition) const -> std::optional<std::size_t>
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

	auto thread_tick_scroll_bar::targeted() const -> std::optional<diff_thread_tick>
	{
		const auto at = std::ranges::find_if(Ticks, [this](const diff_thread_tick& aTick) {
			return Target.has_value() && aTick.Side == Target->first && aTick.Line == Target->second;
		});
		return at == Ticks.end() ? std::nullopt : std::optional{ *at };
	}

	void thread_tick_scroll_bar::paintEvent(QPaintEvent* const aEvent)
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

	void thread_tick_scroll_bar::mousePressEvent(QMouseEvent* const aEvent)
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

	void thread_tick_scroll_bar::mouseMoveEvent(QMouseEvent* const aEvent)
	{
		Pointer   = aEvent->position().toPoint();
		ShiftHeld = aEvent->modifiers().testFlag(Qt::ShiftModifier);
		watch_shift(true);
		follow_pointer();
		QScrollBar::mouseMoveEvent(aEvent);
	}

	void thread_tick_scroll_bar::enterEvent(QEnterEvent* const aEvent)
	{
		Pointer   = aEvent->position().toPoint();
		ShiftHeld = aEvent->modifiers().testFlag(Qt::ShiftModifier);
		watch_shift(true);
		follow_pointer();
		QScrollBar::enterEvent(aEvent);
	}

	void thread_tick_scroll_bar::leaveEvent(QEvent* const aEvent)
	{
		Pointer.reset();
		watch_shift(false);
		follow_pointer();
		QScrollBar::leaveEvent(aEvent);
	}

	auto thread_tick_scroll_bar::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::KeyRelease) {
			if (static_cast<QKeyEvent*>(aEvent)->key() == Qt::Key_Shift) {
				ShiftHeld = aEvent->type() == QEvent::KeyPress;
				follow_pointer();
			}
		}
		return QScrollBar::eventFilter(aWatched, aEvent);
	}

	void thread_tick_scroll_bar::watch_shift(const bool aWatch)
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

	void thread_tick_scroll_bar::follow_pointer()
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

	zoom_pill::zoom_pill(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("diffCodeZoomIndicator"));
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setFocusPolicy(Qt::NoFocus);
		hide();
	}

	void zoom_pill::set_zoom(const QString& aPercent, const QString& aSize)
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

	auto zoom_pill::percent_text() const -> const QString& { return Percent; }

	auto zoom_pill::size_text() const -> const QString& { return Size; }

	void zoom_pill::paintEvent(QPaintEvent*)
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

	auto zoom_pill::pill_fonts() -> std::pair<QFont, QFont>
	{
		auto size = QApplication::font();
		size.setFeature(QFont::Tag{ "tnum" }, 1U);
		auto percent = size;
		percent.setWeight(QFont::DemiBold);
		percent.setPointSizeF(size.pointSizeF() * 1.35);
		return { percent, size };
	}

	void source_pane::set_unified_rows(const std::vector<unified_source_row>* aRows)
	{
		UnifiedRows = aRows;
		refresh_gutter_width();
	}

	void source_pane::set_unified_selection(std::optional<core::line_span> aBase, std::optional<core::line_span> aHead)
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

	void source_pane::set_selected_lines(std::optional<core::line_span> aLines)
	{
		if (SelectedLines != aLines) {
			const auto previous = selected_rows_region();
			SelectedLines       = aLines;
			viewport()->update(previous.xored(selected_rows_region()));
		}
	}

	void source_pane::set_white_space(const core::white_space_drawing aDrawing)
	{
		if (WhiteSpace != aDrawing) {
			WhiteSpace = aDrawing;
			viewport()->update();
		}
	}

	auto source_pane::gutter_width() const noexcept -> int
	{
		return GutterWidth;
	}

	auto source_pane::painted_line_numbers() const noexcept -> std::uint64_t
	{
		return PaintedLineNumbers;
	}

	void source_pane::set_group_boundaries(std::vector<std::uint32_t> aBoundaries)
	{
		if (Boundaries != aBoundaries) {
			Boundaries = std::move(aBoundaries);
			viewport()->update();
		}
	}

	void source_pane::set_boundary_rule_color(const QColor& aColor)
	{
		if (RuleColor != aColor) {
			RuleColor = aColor;
			viewport()->update();
		}
	}

	void source_pane::set_conversation_anchor(std::optional<std::uint32_t> aLine)
	{
		if (ConversationAnchor != aLine) {
			ConversationAnchor = aLine;
			viewport()->update();
		}
	}

	auto source_pane::line_row_rect(const std::uint32_t aLine) const -> QRect
	{
		const auto block = document()->findBlockByNumber(static_cast<int>(aLine));
		if (!block.isValid() || !block.isVisible()) {
			return { };
		}
		const auto geometry = blockBoundingGeometry(block).translated(contentOffset());
		return QRect{ 0, qRound(geometry.top()), viewport()->width(),
					  std::max(1, qRound(geometry.height())) };
	}

	auto source_pane::mark_span_rect(const diff_line_mark& aMark) const -> QRect
	{
		const auto last = line_row_rect(aMark.Line);
		if (!aMark.FirstLine.has_value() || *aMark.FirstLine >= aMark.Line) {
			return last;
		}
		const auto first = line_row_rect(*aMark.FirstLine);
		return first.isEmpty() || last.isEmpty() ? last : first.united(last);
	}

	auto source_pane::conversation_anchor_rect() const -> QRect
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

	void source_pane::set_line_marks(std::vector<diff_line_mark> aMarks)
	{
		std::ranges::stable_sort(aMarks, { }, &diff_line_mark::Line);
		if (Marks == aMarks) {
			return;
		}
		Marks = std::move(aMarks);
		viewport()->update();
		update_gutter();
	}

	void source_pane::set_marker_column(const bool aEnabled)
	{
		if (MarkerColumn == aEnabled) {
			return;
		}
		MarkerColumn = aEnabled;
		set_add_hover(std::nullopt);
		viewport()->update();
		update_gutter();
	}

	auto source_pane::markers_shown() const noexcept -> bool
	{
		return MarkerColumn && LineNumbersVisible;
	}

	auto source_pane::mark_on(const std::uint32_t aLine) const -> const diff_line_mark*
	{
		const auto found = std::ranges::lower_bound(Marks, aLine, { }, &diff_line_mark::Line);
		return found != Marks.end() && found->Line == aLine ? &*found : nullptr;
	}

	auto source_pane::gutter_header() const -> int
	{
		return UnifiedRows != nullptr ? viewportMargins().top() : 0;
	}

	auto source_pane::number_cell(const std::uint32_t aLine) const -> QRect
	{
		const auto row = line_row_rect(aLine);
		if (row.isEmpty()) {
			return { };
		}
		return QRect{ 0, row.top() + gutter_header() - AppliedOffset, gutter_width(), row.height() };
	}

	void source_pane::set_mark_dispatch(std::function<void(const diff_line_mark&)> aDispatch)
	{
		MarkDispatch = std::move(aDispatch);
	}

	void source_pane::set_add_dispatch(std::function<void(std::uint32_t, std::uint32_t)> aDispatch)
	{
		AddDispatch = std::move(aDispatch);
		AddDrag.reset();
		set_add_hover(std::nullopt);
	}

	void source_pane::set_number_dispatch(std::function<void(std::uint32_t, bool)> aDispatch)
	{
		NumberDispatch = std::move(aDispatch);
		NumberDrag.reset();
	}

	auto source_pane::number_line_at(const QPoint aGutterPoint) const -> std::optional<std::uint32_t>
	{
		if (aGutterPoint.y() < gutter_header()) {
			return std::nullopt;
		}
		const auto y    = aGutterPoint.y() - gutter_header() + AppliedOffset;
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

	auto source_pane::press_number(const std::uint32_t aLine, const bool aExtend) -> bool
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

	void source_pane::extend_number_drag(const int aGutterY)
	{
		if (!NumberDrag.has_value() || !NumberDispatch) {
			return;
		}
		const auto line = line_at(aGutterY - gutter_header() + AppliedOffset);
		if (line.has_value() && *line != *NumberDrag) {
			NumberDrag          = line;
			const auto dispatch = NumberDispatch;
			dispatch(*line, true);
		}
	}

	void source_pane::finish_number_drag()
	{
		NumberDrag.reset();
	}

	auto source_pane::add_dragging() const noexcept -> bool
	{
		return AddDrag.has_value();
	}

	auto source_pane::add_line_at(const int aViewportY) const -> std::optional<std::uint32_t>
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

	auto source_pane::add_button_rect(const std::uint32_t aLine) const -> QRect
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

	auto source_pane::add_line_under(const QPoint aPanePoint) const -> std::optional<std::uint32_t>
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

	auto source_pane::line_at(const int aViewportY) const -> std::optional<std::uint32_t>
	{
		const auto block = cursorForPosition(QPoint{ 0, aViewportY }).block();
		if (!block.isValid() || block.blockNumber() < 0) {
			return std::nullopt;
		}
		return static_cast<std::uint32_t>(block.blockNumber());
	}

	void source_pane::begin_add_drag(const std::uint32_t aLine)
	{
		AddDrag = std::pair{ aLine, aLine };
		viewport()->update();
		update_gutter();
	}

	void source_pane::extend_add_drag(const QPoint aGlobal)
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

	void source_pane::finish_add_drag()
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

	auto source_pane::add_drag_rect() const -> QRect
	{
		if (!AddDrag.has_value()) {
			return { };
		}
		const auto [first, last] = std::minmax(AddDrag->first, AddDrag->second);
		const auto top           = line_row_rect(first);
		const auto bottom        = line_row_rect(last);
		return top.isEmpty() || bottom.isEmpty() ? QRect{ } : top.united(bottom);
	}

	void source_pane::press_add(const std::uint32_t aFirst, const std::uint32_t aLast)
	{
		if (AddDispatch) {
			AddDispatch(aFirst, aLast);
		}
	}

	auto source_pane::marker_rect(const diff_line_mark& aMark) const -> QRect
	{
		return markers_shown() ? number_cell(aMark.Line) : QRect{ };
	}

	auto source_pane::mark_at(const QPoint aGutterPoint) const -> const diff_line_mark*
	{
		for (const auto& mark : Marks) {
			if (marker_rect(mark).contains(aGutterPoint)) {
				return &mark;
			}
		}
		return nullptr;
	}

	void source_pane::press_mark(const diff_line_mark& aMark)
	{
		if (MarkDispatch) {
			MarkDispatch(aMark);
		}
	}

	auto source_pane::thread_colour() const noexcept -> const QColor&
	{
		return ThreadMarkColor;
	}

	auto source_pane::new_colour() const noexcept -> const QColor&
	{
		return NewThreadMarkColor;
	}

	auto source_pane::line_marks() const noexcept -> const std::vector<diff_line_mark>&
	{
		return Marks;
	}

	void source_pane::set_mark_colors(const QColor& aThread, const QColor& aNewThread)
	{
		if (ThreadMarkColor == aThread && NewThreadMarkColor == aNewThread) {
			return;
		}
		ThreadMarkColor    = aThread;
		NewThreadMarkColor = aNewThread;
		viewport()->update();
		update_gutter();
	}

	auto source_pane::painted_rules() const noexcept -> std::uint64_t
	{
		return PaintedRules;
	}

	auto source_pane::painted_white_space() const noexcept -> std::uint64_t
	{
		return PaintedWhiteSpace;
	}

	void source_pane::repaint_full_width_selection()
	{
		const auto cursor  = textCursor();
		const auto current = cursor.hasSelection() ? std::optional{ std::pair{ cursor.selectionStart(), cursor.selectionEnd() } } : std::nullopt;
		const auto last    = std::exchange(PaintedSelection, current);
		if (!style()->styleHint(QStyle::SH_RichText_FullWidthSelection, nullptr, this)) {
			return;
		}
		auto region = QRegion{ };
		for (const auto& span : { last, current }) {
			if (!span) {
				continue;
			}
			auto block = firstVisibleBlock();
			auto top   = block.isValid() ? blockBoundingGeometry(block).translated(contentOffset()).top() : 0.0;
			for (; block.isValid() && top <= viewport()->height(); block = block.next()) {
				const auto bottom = top + blockBoundingRect(block).height();
				if (block.isVisible() && block.position() + block.length() > span->first && block.position() <= span->second) {
					region += QRect{ 0, qFloor(top), viewport()->width(), qCeil(bottom) - qFloor(top) };
				}
				top = bottom;
			}
		}
		if (!region.isEmpty()) {
			viewport()->update(region);
		}
	}

	auto source_pane::top_visible_line() const -> std::optional<std::uint32_t>
	{
		const auto block = firstVisibleBlock();
		return block.isValid() && block.blockNumber() >= 0 ? std::optional{ static_cast<std::uint32_t>(block.blockNumber()) } : std::nullopt;
	}

	auto source_pane::exact_line_top(const std::uint32_t aLine) const -> std::optional<qreal>
	{
		const auto block = document()->findBlockByNumber(static_cast<int>(aLine));
		if (!block.isValid() || !block.isVisible()) {
			return std::nullopt;
		}
		return blockBoundingGeometry(block).translated(contentOffset()).top();
	}

	auto source_pane::visible_origin() const
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

	void source_pane::set_top_line(const std::size_t aLine)
	{
		auto* const bar = verticalScrollBar();
		if (bar != nullptr) {
			bar->setValue(static_cast<int>(std::min<std::size_t>(aLine, std::numeric_limits<int>::max())));
		}
	}

	void source_pane::place_top_line_and_offset(const std::size_t aLine, const qreal aOffset)
	{
		const auto holdOffset = QScopedValueRollback<bool>{ ApplyingPixelWheel, true };
		set_top_line(aLine);
		set_within_line_offset(aOffset);
	}

	void source_pane::set_within_line_offset(const qreal aOffset)
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

	auto source_pane::within_line_offset() const noexcept -> int { return AppliedOffset; }

	auto source_pane::placing_origin() const noexcept -> bool { return PlacingOrigin; }

	auto source_pane::changes_viewport_extent(const QSize aSize) const noexcept -> bool { return !ViewportArea.isValid() || aSize != ViewportArea.size(); }

	auto source_pane::viewport_extent() const noexcept -> int { return ViewportArea.isValid() ? ViewportArea.height() : viewport()->height(); }

	auto source_pane::displayed_source_area() const noexcept -> QRect { return ViewportArea.isValid() ? ViewportArea : viewport()->geometry(); }

	auto source_pane::within_line_position() const noexcept -> qreal { return RetainedOffset; }

	void source_pane::reset_pixel_gesture() { PixelGesture.reset(); }

	void source_pane::set_scroll_observer(std::function<void()> aObserver) { ScrollObserver = std::move(aObserver); }

	void source_pane::scrollContentsBy(const int aDx, const int aDy)
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

	void source_pane::focusInEvent(QFocusEvent* const aEvent)
	{
		QPlainTextEdit::focusInEvent(aEvent);
		repaint_full_width_selection();
	}

	void source_pane::focusOutEvent(QFocusEvent* const aEvent)
	{
		QPlainTextEdit::focusOutEvent(aEvent);
		repaint_full_width_selection();
	}

	void source_pane::wheelEvent(QWheelEvent* const aEvent)
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

	void source_pane::paintEvent(QPaintEvent* const aEvent)
	{
		paint_text(aEvent);
		paint_find_rings();
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

	void source_pane::paint_white_space_of_visible_lines(QPainter& aPainter)
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

	auto source_pane::selected_rows_region() const -> QRegion
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

	auto source_pane::selected_block(const int aBlock) const -> bool
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

	void source_pane::set_selection_ink(const QColor& aNormal, const core::appearance_variant aVariant)
	{
		if (SelectionNormal != aNormal || SelectionVariant != aVariant) {
			SelectionNormal  = aNormal;
			SelectionVariant = aVariant;
			viewport()->update();
		}
	}

	void source_pane::set_font_observer(std::function<void()> aObserver)
	{
		FontObserver = std::move(aObserver);
	}

	auto source_pane::row_height() const -> qreal
	{
		const auto block = firstVisibleBlock();
		return block.isValid() ? blockBoundingRect(block).height() : 0.0;
	}

	auto source_pane::row_top(const std::uint32_t aLine) const -> std::optional<qreal>
	{
		const auto block = document()->findBlockByNumber(static_cast<int>(aLine));
		if (!block.isValid() || !block.isVisible()) {
			return std::nullopt;
		}
		return blockBoundingGeometry(block).translated(contentOffset()).top() - AppliedOffset;
	}

	source_pane::gutter_area::gutter_area(source_pane* const aPane)
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

	auto source_pane::gutter_area::sizeHint() const -> QSize
	{
		return QSize{ Pane->gutter_width(), 0 };
	}

	void source_pane::gutter_area::paintEvent(QPaintEvent* const aEvent)
	{
		Pane->paint_line_numbers(aEvent);
	}

	void source_pane::gutter_area::mousePressEvent(QMouseEvent* const aEvent)
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

	void source_pane::gutter_area::mouseMoveEvent(QMouseEvent* const aEvent)
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

	void source_pane::gutter_area::mouseReleaseEvent(QMouseEvent* const aEvent)
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

	void source_pane::gutter_area::leaveEvent(QEvent* const aEvent)
	{
		Pane->leave_add_hover();
		QWidget::leaveEvent(aEvent);
	}

	void source_pane::gutter_area::contextMenuEvent(QContextMenuEvent* const aEvent)
	{
		if (Pane->contextMenuPolicy() != Qt::CustomContextMenu) {
			QWidget::contextMenuEvent(aEvent);
			return;
		}
		aEvent->accept();
		Q_EMIT Pane->customContextMenuRequested(QPoint{ 0, Pane->viewport()->mapFrom(Pane, mapToParent(aEvent->pos())).y() });
	}

	auto source_pane::gutter_area::event(QEvent* const aEvent) -> bool
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

	source_pane::add_button::add_button(source_pane* const aPane)
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

	void source_pane::add_button::set_line(const std::uint32_t aLine)
	{
		Line = aLine;
		setAccessibleDescription(QStringLiteral("Adds a comment on line %1. Drag across lines to comment on all of them; Return does the same for the selected lines").arg(aLine + 1U));
	}

	auto source_pane::add_button::line() const noexcept -> std::uint32_t
	{
		return Line;
	}

	void source_pane::add_button::set_colour(const QColor& aColour)
	{
		if (Colour != aColour) {
			Colour = aColour;
			update();
		}
	}

	void source_pane::add_button::paintEvent(QPaintEvent* const /*aEvent*/)
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

	void source_pane::add_button::mousePressEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() != Qt::LeftButton) {
			aEvent->ignore();
			return;
		}
		setDown(true);
		Pane->begin_add_drag(Line);
		aEvent->accept();
	}

	void source_pane::add_button::mouseMoveEvent(QMouseEvent* const aEvent)
	{
		Pane->extend_add_drag(aEvent->globalPosition().toPoint());
		aEvent->accept();
	}

	void source_pane::add_button::mouseReleaseEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() != Qt::LeftButton) {
			aEvent->ignore();
			return;
		}
		setDown(false);
		Pane->finish_add_drag();
		aEvent->accept();
	}

	void source_pane::add_button::enterEvent(QEnterEvent* const aEvent)
	{
		update();
		QAbstractButton::enterEvent(aEvent);
	}

	void source_pane::add_button::leaveEvent(QEvent* const aEvent)
	{
		update();
		if (!isDown()) {
			Pane->leave_add_hover();
		}
		QAbstractButton::leaveEvent(aEvent);
	}

	auto source_pane::add_button::hitButton(const QPoint& aPoint) const -> bool
	{
		return rect().contains(aPoint);
	}

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
		// [[BPR-1734]]. A full-width selection is repainted to the edge.
		connect(this, &QPlainTextEdit::selectionChanged, this, [this] { repaint_full_width_selection(); });
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

	void source_pane::paint_find_rings()
	{
		if (FindRings.empty()) {
			return;
		}
		auto       painter = QPainter{ viewport() };
		const auto offset  = contentOffset();
		const auto height  = viewport()->height();
		for (const auto& ring : FindRings) {
			// Inset by half the pen, so the ring lies on the match's own box
			// and does not spill onto the neighbouring characters.
			const auto width = ring.Current ? 2.0 : 1.0;
			painter.setPen(QPen{ ring.Colour.isValid() ? ring.Colour : palette().color(QPalette::Highlight), width });
			const auto first = ring.Cursor.selectionStart();
			const auto last  = ring.Cursor.selectionEnd();
			for (auto block = document()->findBlock(first); block.isValid() && block.position() <= last; block = block.next()) {
				const auto rowBox = blockBoundingGeometry(block).translated(offset);
				if (rowBox.top() > height) {
					break;
				}
				if (!block.isVisible() || rowBox.bottom() < 0.0 || block.position() + block.length() <= first) {
					continue;
				}
				auto beginCursor = QTextCursor{ block };
				beginCursor.setPosition(std::max(first, block.position()));
				auto endCursor = QTextCursor{ block };
				endCursor.setPosition(std::min(last, block.position() + block.length() - 1));
				const auto beginRect = cursorRect(beginCursor);
				const auto endRect   = cursorRect(endCursor);
				const auto spanBox   = QRectF{ QRect{ beginRect.left(), beginRect.top(), std::max(2, endRect.left() - beginRect.left()), beginRect.height() } };
				const auto inset     = width / 2.0;
				painter.drawRect(spanBox.adjusted(inset, inset, -inset, -inset));
			}
		}
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
		const bool findCurrent   = std::ranges::any_of(FindRings, [&cursor](const find_ring& aRing) {
			return aRing.Current && aRing.Cursor.selectionStart() == cursor.selectionStart() && aRing.Cursor.selectionEnd() == cursor.selectionEnd();
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
		const auto ink    = GutterText.isValid() ? GutterText : palette().color(QPalette::WindowText);
		const auto ground = to_appearance_color(GutterBackground.isValid() ? GutterBackground : palette().color(QPalette::Window));
		painter.setPen(ink);
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
						auto        cell = ground;
						if (fill.isValid()) {
							painter.fillRect(QRectF{ 0.0, top, static_cast<qreal>(Gutter->width()), height }, fill);
							cell = laid(fill, cell);
						}
						const auto contains = [](const std::optional<core::line_span> aSpan, const std::optional<std::uint32_t> aLine) {
							return aSpan && aLine && *aLine >= aSpan->Begin && *aLine < aSpan->End;
						};
						if (contains(SelectedBase, row.Base) || contains(SelectedHead, row.Head)) {
							auto wash = palette().color(QPalette::Highlight);
							wash.setAlpha(core::selection_wash_alpha);
							painter.fillRect(QRectF{ 0.0, top, static_cast<qreal>(Gutter->width()), height }, wash);
							cell = laid(wash, cell);
						}
						const auto columnWidth = numbers / 2;
						paint_thread_tint(painter, static_cast<std::uint32_t>(block.blockNumber()), QRectF{ 0.0, top, static_cast<qreal>(numbers), height });
						painter.setPen(number_ink(static_cast<std::uint32_t>(block.blockNumber()), ink, to_presentation_color(cell)));
						for (const auto side : { core::diff_side::left, core::diff_side::right }) {
							const auto number = side == core::diff_side::left ? row.Base : row.Head;
							if (number) {
								GutterLabels.draw(painter, QRect{ side == core::diff_side::left ? 0 : columnWidth, qRound(top), columnWidth - GutterPadding, qRound(height) }, QString::number(*number + 1U));
								++PaintedLineNumbers;
							}
						}
					}
				} else {
					auto cell = ground;
					if (const auto drag = add_drag_rect(); !drag.isEmpty() && top < drag.bottom() + 1 && top + height > drag.top()) {
						auto wash = palette().color(QPalette::Active, QPalette::Highlight);
						wash.setAlphaF(0.16F);
						painter.fillRect(QRectF{ 0.0, top, static_cast<qreal>(numbers), height }, wash);
						cell = laid(wash, cell);
					}
					paint_thread_tint(painter, static_cast<std::uint32_t>(block.blockNumber()), QRectF{ 0.0, top, static_cast<qreal>(numbers), height });
					painter.setPen(number_ink(static_cast<std::uint32_t>(block.blockNumber()), ink, to_presentation_color(cell)));
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
		if (const auto* const mark = mark_on(aLine); mark != nullptr) {
			if (mark->Draft) {
				aPainter.save();
				aPainter.setRenderHint(QPainter::Antialiasing, true);
				aPainter.setPen(QPen{ palette().color(QPalette::Active, QPalette::PlaceholderText), 1.2, Qt::DashLine });
				aPainter.setBrush(Qt::NoBrush);
				aPainter.drawRoundedRect(aCell.adjusted(1.5, 1.5, -1.5, -1.5), 3.0, 3.0);
				aPainter.restore();
			} else {
				aPainter.fillRect(aCell, thread_tint(*mark));
			}
		}
		// The open thread's and my draft's washes go under the number too:
		// laid over it, they dimmed its ink with its ground, and even white
		// missed High Contrast Dark's 7:1 on an open thread's amber.
		for (const auto& wash : number_washes(aLine)) {
			aPainter.fillRect(aCell, wash);
		}
	}

	auto source_pane::thread_tint(const diff_line_mark& aMark) const -> QColor
	{
		return with_opacity(aMark.Kind == diff_line_mark_kind::new_thread ? NewThreadMarkColor : ThreadMarkColor, aMark.Resolved ? 0.12F : 0.30F);
	}

	auto source_pane::number_washes(const std::uint32_t aLine) const -> std::vector<QColor>
	{
		auto washes = std::vector<QColor>{ };
		for (const auto& mark : Marks) {
			const auto first = mark.FirstLine.has_value() && *mark.FirstLine < mark.Line ? *mark.FirstLine : mark.Line;
			if (aLine < first || aLine > mark.Line) {
				continue;
			}
			// [[BPR-1290]]. The open thread's lines washed amber;
			// [[BPR-1212]], my draft's being written washed blue.
			if (mark.Writing) {
				washes.push_back(with_opacity(palette().color(QPalette::Active, QPalette::Highlight), writing_wash_opacity));
			} else if (mark.Open) {
				washes.push_back(with_opacity(ThreadMarkColor, open_wash_opacity));
			}
		}
		return washes;
	}

	auto source_pane::number_ink(const std::uint32_t aLine, const QColor& aInk, const QColor& aCell) const -> QColor
	{
		if (!markers_shown() || Marks.empty()) {
			return aInk;
		}
		// The cell as `paint_thread_tint` leaves it: the thread's tint, then
		// any open thread's or writing draft's wash.
		const auto* const mark   = mark_on(aLine);
		const auto        tinted = mark != nullptr && !mark->Draft;
		const auto        washes = number_washes(aLine);
		if (!tinted && washes.empty()) {
			return aInk;
		}
		auto ground = to_appearance_color(aCell);
		if (tinted) {
			ground = laid(thread_tint(*mark), ground);
		}
		for (const auto& wash : washes) {
			ground = laid(wash, ground);
		}
		// The painter's own blending rounds a tint a little differently from
		// `composited`, so the move aims a margin above the floor and the
		// painted number still reaches it.
		const auto floor = core::minimum_contrast_hundredths(core::contrast_kind::text, SelectionVariant) + number_blend_margin_hundredths;
		const auto reads = [&ground, floor](const core::appearance_color aCandidate) { return core::contrast_ratio_hundredths(aCandidate, ground) >= floor; };
		const auto from  = to_appearance_color(aInk);
		if (reads(from)) {
			return aInk;
		}
		// Toward the code's own ink first, so the number stays the theme's
		// grey; past it toward black or white only where that is not enough.
		const auto strong  = to_appearance_color(SelectionNormal.isValid() ? SelectionNormal : palette().color(QPalette::Active, QPalette::Text));
		const auto extreme = core::emphasis_foreground(ground);
		for (const auto& [start, target] : { std::pair{ from, strong }, std::pair{ strong, extreme } }) {
			for (auto step = 1U; step <= 255U; ++step) {
				if (const auto candidate = core::composited(target, start, static_cast<std::uint8_t>(step)); reads(candidate)) {
					return to_presentation_color(candidate);
				}
			}
		}
		return to_presentation_color(extreme);
	}

	void source_pane::paint_markers(QPainter& aPainter)
	{
		// [[BPR-1290]]. The open thread's number is barred at the gutter's
		// left edge. [[BPR-1212]]: my draft being written is bracketed at its
		// right edge, where its card joins. Their washes are laid under the
		// numbers, by `paint_thread_tint`.
		if (!markers_shown()) {
			return;
		}
		aPainter.save();
		aPainter.setClipRect(QRect{ 0, gutter_header(), Gutter->width(), Gutter->height() - gutter_header() });
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto header = gutter_header() - AppliedOffset;
		for (const auto& mark : Marks) {
			if (mark.Writing) {
				// A draft on several lines spans them all, drawn while any
				// of them shows.
				const auto row = mark_span_rect(mark);
				if (!row.isEmpty() && row.bottom() + header >= 0 && row.top() + header <= Gutter->height()) {
					auto bracket = QPainterPath{ };
					bracket.addRoundedRect(QRectF{ static_cast<qreal>(Gutter->width() - 3), static_cast<qreal>(row.top() + header + 2), 3.0, static_cast<qreal>(std::max(1, row.height() - 4)) }, 1.5, 1.5);
					aPainter.fillPath(bracket, palette().color(QPalette::Active, QPalette::Highlight));
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
			aPainter.fillRect(QRect{ 0, row.top() + header, 3, row.height() }, ThreadMarkColor);
		}
		aPainter.restore();
	}

	auto pane(QPlainTextEdit* const aPane) noexcept
		-> source_pane*
	{
		return static_cast<source_pane*>(aPane);
	}

	auto text_document(
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

	void staged_document::start(QPlainTextEdit& aPane, const std::optional<core::decoded_text_document>& aSource)
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

	auto staged_document::complete() const noexcept -> bool
	{
		return Position >= Text.size();
	}

	void staged_document::append(const int aLines)
	{
		auto end = Position;
		for (auto line = 0; line < aLines && end < Text.size(); ++line) {
			const auto next = Text.indexOf(QLatin1Char('\n'), end);
			end             = next < 0 ? Text.size() : next + 1;
		}
		End.insertText(Text.sliced(Position, end - Position));
		Position = end;
	}

	void staged_document::discard()
	{
		delete Document.data();
	}

	auto paintable_line_bounds(QPlainTextEdit& aPane)
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

	auto diff_workbench::head_edit::range() const -> std::pair<std::uint64_t, std::uint64_t>
	{
		const auto editable = Editor != nullptr ? Editor->model().editable_range() : std::nullopt;
		return editable.value_or(std::pair<std::uint64_t, std::uint64_t>{ First, Last });
	}

	auto diff_workbench::head_edit::count() const -> std::uint64_t
	{
		const auto [first, last] = range();
		return last - first + 1U;
	}
}
