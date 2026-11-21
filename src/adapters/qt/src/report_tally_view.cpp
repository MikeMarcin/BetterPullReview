#include "bpr/qt/report_tally_view.hpp"

#include "bpr/qt/anchored_tooltip.hpp"

#include <QEnterEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTransform>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace bpr::qt {
	namespace {
		/// A legend row's padding on the left, before its swatch, and on
		/// the right, after its count.
		constexpr int row_inset = 4;
		constexpr int row_pad   = 8;
		/// Between the swatch and the word, and the least between the word
		/// and the count.
		constexpr int swatch_gap = 8;
		constexpr int count_gap  = 12;
		/// Between two slices, as the report page's meter has between its
		/// cells.
		constexpr double slice_gap = 2.0;

		[[nodiscard]] auto counted(const core::dressed_count& aCount) -> QString { return QString::number(aCount.Count); }

		[[nodiscard]] auto with_alpha(QColor aColour, const float aAlpha) -> QColor
		{
			aColour.setAlphaF(aAlpha);
			return aColour;
		}

		[[nodiscard]] auto semibold(QFont aFont) -> QFont
		{
			aFont.setWeight(QFont::DemiBold);
			return aFont;
		}

		/// The swatch's side: half a line of the font, never less than 8.
		[[nodiscard]] auto swatch_side(const QFontMetrics& aMetrics) -> int { return std::max(8, aMetrics.height() / 2); }

		/// Where an angle (degrees clockwise from twelve o'clock) points, at
		/// `aRadius` from `aCentre`.
		[[nodiscard]] auto on_ring(const QPointF aCentre, const double aRadius, const double aDegrees) -> QPointF
		{
			const auto radians = aDegrees * std::numbers::pi / 180.0;
			return { aCentre.x() + (aRadius * std::sin(radians)), aCentre.y() - (aRadius * std::cos(radians)) };
		}
	} // namespace

	report_tally_row::report_tally_row(const core::dressed_count& aCount, const std::size_t aIndex, report_tally_view& aView)
		: QPushButton{ &aView }
		, Count{ aCount }
		, Index{ aIndex }
		, View{ aView }
	{
		setObjectName(QStringLiteral("agentTurnReportTallyRow"));
		setCursor(Qt::PointingHandCursor);
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover, true);
		// Every row takes the legend's width, so the washes line up and the
		// counts make one column.
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		const auto word = QString::fromStdString(aCount.Word);
		setAccessibleName(tr("%1: %2").arg(word, counted(aCount)));
		setToolTip(tr("Open the report at its first %1").arg(word));
		setAccessibleDescription(toolTip());
	}

	auto report_tally_row::sizeHint() const -> QSize { return minimumSizeHint(); }

	/// No narrower than its swatch, word and count: the drawer never pans
	/// sideways. One line tall.
	auto report_tally_row::minimumSizeHint() const -> QSize
	{
		const auto metrics = QFontMetrics{ font() };
		const auto word    = metrics.horizontalAdvance(QString::fromStdString(Count.Word));
		const auto count   = QFontMetrics{ semibold(font()) }.horizontalAdvance(counted(Count));
		return { row_inset + swatch_side(metrics) + swatch_gap + word + count_gap + count + row_pad, View.line_height() };
	}

	auto report_tally_row::swatch_rect() const -> QRect
	{
		const auto side = swatch_side(QFontMetrics{ font() });
		return { row_inset, (height() - side) / 2, side, side };
	}

	auto report_tally_row::word_rect() const -> QRect
	{
		const auto metrics = QFontMetrics{ font() };
		return { swatch_rect().right() + 1 + swatch_gap, (height() - metrics.height()) / 2, metrics.horizontalAdvance(QString::fromStdString(Count.Word)), metrics.height() };
	}

	auto report_tally_row::count_rect() const -> QRect
	{
		const auto metrics = QFontMetrics{ semibold(font()) };
		const auto wide    = metrics.horizontalAdvance(counted(Count));
		return { width() - row_pad - wide, (height() - metrics.height()) / 2, wide, metrics.height() };
	}

	void report_tally_row::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto text = palette().color(QPalette::Active, QPalette::WindowText);
		const auto box  = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		if (View.highlighted() == Index || isDown()) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(with_alpha(text, isDown() ? 0.10F : 0.06F));
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
		if (hasFocus()) {
			painter.setBrush(Qt::NoBrush);
			painter.setPen(QPen{ palette().color(QPalette::Highlight), 1.5 });
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
		painter.setPen(Qt::NoPen);
		painter.setBrush(View.fill_of(Count));
		painter.drawRoundedRect(QRectF{ swatch_rect() }, 2.0, 2.0);
		painter.setPen(View.ink_of(Count));
		painter.setFont(font());
		painter.drawText(word_rect(), Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(Count.Word));
		painter.setPen(text);
		painter.setFont(semibold(font()));
		painter.drawText(count_rect(), Qt::AlignRight | Qt::AlignVCenter, counted(Count));
	}

	void report_tally_row::enterEvent(QEnterEvent* aEvent)
	{
		QPushButton::enterEvent(aEvent);
		View.set_hovered(Index);
	}

	void report_tally_row::leaveEvent(QEvent* aEvent)
	{
		QPushButton::leaveEvent(aEvent);
		if (View.highlighted() == Index) {
			View.set_hovered(std::nullopt);
		}
	}

	void report_tally_row::focusInEvent(QFocusEvent* aEvent)
	{
		QPushButton::focusInEvent(aEvent);
		View.focus_changed();
	}

	void report_tally_row::focusOutEvent(QFocusEvent* aEvent)
	{
		QPushButton::focusOutEvent(aEvent);
		View.focus_changed();
	}

	report_tally_pie::report_tally_pie(report_tally_view& aView)
		: QWidget{ &aView }
		, View{ aView }
	{
		setObjectName(QStringLiteral("agentTurnReportTallyPie"));
		setMouseTracking(true);
		setFocusPolicy(Qt::NoFocus);
		setFixedSize(sizeHint());
		// A slice's tooltip is its row's, anchored to the slice.
		anchor_tooltips(*this, [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
			const auto over = slice_at(QPointF{ aPosition });
			if (!over) {
				return std::nullopt;
			}
			return anchored_tooltip_target{ .Anchor = slice_path(*over, 0.0).boundingRect().toAlignedRect(), .Text = View.rows()[*over]->toolTip() };
		});
	}

	auto report_tally_pie::sizeHint() const -> QSize { return { Diameter + (2 * lift), Diameter + (2 * lift) }; }

	void report_tally_pie::set_ring_diameter(const int aDiameter)
	{
		if (aDiameter == Diameter) {
			return;
		}
		Diameter = aDiameter;
		setFixedSize(sizeHint());
		update();
	}

	auto report_tally_pie::minimumSizeHint() const -> QSize { return sizeHint(); }

	auto report_tally_pie::centre() const -> QPointF { return QRectF{ rect() }.center(); }

	auto report_tally_pie::outer_radius() const -> double { return Diameter / 2.0; }

	auto report_tally_pie::inner_radius() const -> double { return outer_radius() * hole; }

	auto report_tally_pie::slice_at(const QPointF aPoint) const -> std::optional<std::size_t>
	{
		const auto rows   = View.rows();
		const auto across = aPoint - centre();
		const auto reach  = std::hypot(across.x(), across.y());
		if (rows.empty() || reach < inner_radius() || reach > outer_radius() + lift) {
			return std::nullopt;
		}
		auto angle = std::atan2(across.x(), -across.y()) * 180.0 / std::numbers::pi;
		if (angle < 0.0) {
			angle += 360.0;
		}
		const auto found = std::ranges::find_if(rows, [&](const report_tally_row* const aRow) { return angle < View.slice_start(aRow->index()) + View.slice_span(aRow->index()); });
		return found == rows.end() ? rows.size() - 1U : (*found)->index();
	}

	auto report_tally_pie::slice_middle(const std::size_t aIndex) const -> QPointF
	{
		return on_ring(centre(), (outer_radius() + inner_radius()) / 2.0, View.slice_start(aIndex) + (View.slice_span(aIndex) / 2.0));
	}

	auto report_tally_pie::slice_path(const std::size_t aIndex, const double aLift) const -> QPainterPath
	{
		const auto middle = centre();
		const auto out    = outer_radius() + aLift;
		const auto in     = inner_radius();
		auto       path   = QPainterPath{ };
		if (View.rows().size() == 1U) {
			// One label is the whole ring.
			path.setFillRule(Qt::OddEvenFill);
			path.addEllipse(middle, out, out);
			path.addEllipse(middle, in, in);
			return path;
		}
		const auto start = View.slice_start(aIndex);
		const auto span  = View.slice_span(aIndex);
		const auto outer = QRectF{ middle.x() - out, middle.y() - out, 2.0 * out, 2.0 * out };
		const auto inner = QRectF{ middle.x() - in, middle.y() - in, 2.0 * in, 2.0 * in };
		// Qt's arcs run anticlockwise from three o'clock.
		path.arcMoveTo(outer, 90.0 - start);
		path.arcTo(outer, 90.0 - start, -span);
		path.arcTo(inner, 90.0 - start - span, span);
		path.closeSubpath();
		// The gaps: a bar of the gap's width along each edge.
		auto gaps = QPainterPath{ };
		for (const auto edge : { start, start + span }) {
			auto bar = QPainterPath{ };
			bar.addRect(QRectF{ 0.0, -slice_gap / 2.0, out + 2.0, slice_gap });
			gaps.addPath(QTransform{ }.translate(middle.x(), middle.y()).rotate(edge - 90.0).map(bar));
		}
		return path.subtracted(gaps);
	}

	void report_tally_pie::paintEvent(QPaintEvent* /*aEvent*/)
	{
		const auto rows = View.rows();
		if (rows.empty()) {
			return;
		}
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto lit = View.highlighted();
		for (const auto* const row : rows) {
			painter.fillPath(slice_path(row->index(), lit == row->index() ? lift : 0.0), View.fill_of(row->count()));
		}
		// The total in the hole, where it fits.
		const auto total  = QString::number(View.total());
		const auto font   = semibold(this->font());
		const auto inside = QRectF{ centre().x() - inner_radius(), centre().y() - inner_radius(), 2.0 * inner_radius(), 2.0 * inner_radius() };
		if (QFontMetrics{ font }.horizontalAdvance(total) <= inside.width() - 6.0) {
			painter.setFont(font);
			painter.setPen(palette().color(QPalette::Active, QPalette::WindowText));
			painter.drawText(inside, Qt::AlignCenter, total);
		}
	}

	void report_tally_pie::mouseMoveEvent(QMouseEvent* aEvent)
	{
		const auto over = slice_at(aEvent->position());
		if (over) {
			setCursor(Qt::PointingHandCursor);
		} else {
			unsetCursor();
		}
		View.set_hovered(over);
		QWidget::mouseMoveEvent(aEvent);
	}

	void report_tally_pie::mousePressEvent(QMouseEvent* aEvent)
	{
		Pressed = aEvent->button() == Qt::LeftButton ? slice_at(aEvent->position()) : std::nullopt;
		aEvent->setAccepted(Pressed.has_value());
	}

	void report_tally_pie::mouseReleaseEvent(QMouseEvent* aEvent)
	{
		const auto pressed = std::exchange(Pressed, std::nullopt);
		if (aEvent->button() == Qt::LeftButton && pressed && slice_at(aEvent->position()) == pressed) {
			View.open(*pressed);
			aEvent->accept();
			return;
		}
		aEvent->ignore();
	}

	void report_tally_pie::leaveEvent(QEvent* aEvent)
	{
		QWidget::leaveEvent(aEvent);
		unsetCursor();
		View.set_hovered(std::nullopt);
	}

	report_tally_view::report_tally_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentTurnReportTally"));
		auto* const row = new QHBoxLayout{ this };
		// The view's least width is its own (`minimumSizeHint`): the layout
		// would hold it at the full donut's.
		row->setSizeConstraint(QLayout::SetNoConstraint);
		row->setContentsMargins(0, 6, 0, 0);
		row->setSpacing(legend_gap);
		Pie = new report_tally_pie{ *this };
		row->addWidget(Pie, 0, Qt::AlignVCenter);
		Legend = new QWidget{ this };
		Legend->setObjectName(QStringLiteral("agentTurnReportTallyLegend"));
		Legend->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
		auto* const column = new QVBoxLayout{ Legend };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);
		row->addWidget(Legend, 0, Qt::AlignVCenter);
		row->addStretch(1);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		hide();
	}

	void report_tally_view::set_counts(const std::vector<core::dressed_count>& aCounts)
	{
		if (aCounts == counts()) {
			setVisible(!aCounts.empty());
			return;
		}
		for (auto* const row : Rows) {
			row->hide();
			row->deleteLater();
		}
		Rows.clear();
		Hovered.reset();
		Total = 0U;
		for (const auto& count : aCounts) {
			Total += count.Count;
		}
		for (const auto& count : aCounts) {
			auto* const row = new report_tally_row{ count, Rows.size(), *this };
			connect(row, &QPushButton::clicked, this, [this, index = Rows.size()] { open(index); });
			Legend->layout()->addWidget(row);
			Rows.push_back(row);
		}
		Pie->setAccessibleName(tr("Report summary: %1 in all").arg(Total));
		Pie->update();
		setVisible(!aCounts.empty());
		updateGeometry();
		fit_ring();
	}

	void report_tally_view::set_tones(const tone_colours& aTones)
	{
		if (ToneSet && Tones == aTones) {
			return;
		}
		Tones   = aTones;
		ToneSet = true;
		Pie->update();
		for (auto* const row : Rows) {
			row->update();
		}
	}

	auto report_tally_view::rows() const -> std::vector<report_tally_row*> { return Rows; }

	auto report_tally_view::counts() const -> std::vector<core::dressed_count>
	{
		auto result = std::vector<core::dressed_count>{ };
		result.reserve(Rows.size());
		for (const auto* const row : Rows) {
			result.push_back(row->count());
		}
		return result;
	}

	auto report_tally_view::line_height() const -> int { return std::max(20, QFontMetrics{ font() }.height() + 6); }

	/// The counts above it, as a share of the whole circle.
	auto report_tally_view::slice_start(const std::size_t aIndex) const -> double
	{
		if (Total == 0U) {
			return 0.0;
		}
		auto above = std::uint64_t{ 0U };
		for (std::size_t index = 0U; index < aIndex && index < Rows.size(); ++index) {
			above += Rows[index]->count().Count;
		}
		return 360.0 * static_cast<double>(above) / static_cast<double>(Total);
	}

	auto report_tally_view::slice_span(const std::size_t aIndex) const -> double
	{
		if (Total == 0U || aIndex >= Rows.size()) {
			return 0.0;
		}
		return 360.0 * static_cast<double>(Rows[aIndex]->count().Count) / static_cast<double>(Total);
	}

	auto report_tally_view::ink_of(const core::dressed_count& aCount) const -> QColor
	{
		const auto tones = ToneSet ? Tones : builtin_tone_colours(palette().color(QPalette::Window).lightness() < 128);
		return tone_colour(aCount.Tone, tones).value_or(palette().color(QPalette::Active, QPalette::WindowText));
	}

	auto report_tally_view::fill_of(const core::dressed_count& aCount) const -> QColor
	{
		return aCount.Tone == core::report_tone::neutral ? with_alpha(palette().color(QPalette::Active, QPalette::WindowText), 0.45F) : ink_of(aCount);
	}

	auto report_tally_view::highlighted() const -> std::optional<std::size_t>
	{
		if (Hovered) {
			return Hovered;
		}
		const auto focused = std::ranges::find_if(Rows, [](const report_tally_row* const aRow) { return aRow->hasFocus(); });
		return focused == Rows.end() ? std::nullopt : std::optional{ (*focused)->index() };
	}

	void report_tally_view::set_hovered(const std::optional<std::size_t> aIndex)
	{
		if (Hovered == aIndex) {
			return;
		}
		Hovered = aIndex;
		Pie->update();
		for (auto* const row : Rows) {
			row->update();
		}
	}

	void report_tally_view::open(const std::size_t aIndex) const
	{
		if (Open && aIndex < Rows.size()) {
			Open(Rows[aIndex]->count().First);
		}
	}

	void report_tally_view::focus_changed()
	{
		Pie->update();
		for (auto* const row : Rows) {
			row->update();
		}
	}

	auto report_tally_view::width_for(const int aDiameter) const -> int
	{
		const auto margins = layout()->contentsMargins();
		return margins.left() + aDiameter + (2 * report_tally_pie::lift) + legend_gap + Legend->minimumSizeHint().width() + margins.right();
	}

	auto report_tally_view::minimumSizeHint() const -> QSize
	{
		auto least = QWidget::minimumSizeHint();
		least.setWidth(width_for(report_tally_pie::compact_diameter));
		return least;
	}

	void report_tally_view::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		fit_ring();
	}

	void report_tally_view::fit_ring() { Pie->set_ring_diameter(width() >= width_for(report_tally_pie::diameter) ? report_tally_pie::diameter : report_tally_pie::compact_diameter); }

	void report_tally_view::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange) {
			for (auto* const row : Rows) {
				row->updateGeometry();
			}
			if (Legend->layout() != nullptr) {
				Legend->layout()->invalidate();
			}
			layout()->invalidate();
			updateGeometry();
			fit_ring();
		}
	}
} // namespace bpr::qt
