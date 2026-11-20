#include "bpr/qt/report_tally_view.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The row's own padding on the right.
		constexpr int row_pad = 8;
		/// The bar's centre: under the attachment's page, whose centre is
		/// 20 points in.
		constexpr int spine = 20;
		/// Between a count's word and its number.
		constexpr int count_gap = 6;
		/// Between two segments, as the report page's meter has between its
		/// cells.
		constexpr int segment_gap = 2;

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

		/// Where a label starts: past the bar and its gap.
		constexpr int label_left = spine + (report_tally_view::bar_width / 2) + report_tally_view::label_gap;
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
		// It takes the summary's width, so its hover and press span its
		// segment and its label.
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		const auto word = QString::fromStdString(aCount.Word);
		setAccessibleName(tr("%1: %2").arg(word, counted(aCount)));
		setToolTip(tr("Open the report at its first %1").arg(word));
		setAccessibleDescription(toolTip());
	}

	auto report_tally_row::sizeHint() const -> QSize { return minimumSizeHint(); }

	/// No narrower than the bar, its word and its count: the drawer never
	/// pans sideways. As tall as its band.
	auto report_tally_row::minimumSizeHint() const -> QSize
	{
		const auto word  = QFontMetrics{ font() }.horizontalAdvance(QString::fromStdString(Count.Word));
		const auto count = QFontMetrics{ semibold(font()) }.horizontalAdvance(counted(Count));
		return { label_left + word + count_gap + count + row_pad, View.band_height(Index) };
	}

	/// The whole band, less half a gap at each end another segment meets.
	auto report_tally_row::segment_rect() const -> QRect
	{
		const auto first = Index == 0U;
		const auto last  = Index + 1U == View.rows().size();
		const auto top   = first ? 0 : segment_gap / 2;
		const auto below = last ? 0 : segment_gap - (segment_gap / 2);
		return { spine - (report_tally_view::bar_width / 2), top, report_tally_view::bar_width, height() - top - below };
	}

	auto report_tally_row::word_rect() const -> QRect
	{
		const auto metrics = QFontMetrics{ font() };
		return { label_left, (height() - metrics.height()) / 2, metrics.horizontalAdvance(QString::fromStdString(Count.Word)), metrics.height() };
	}

	auto report_tally_row::count_rect() const -> QRect
	{
		const auto metrics = QFontMetrics{ semibold(font()) };
		return { word_rect().right() + 1 + count_gap, (height() - metrics.height()) / 2, metrics.horizontalAdvance(counted(Count)), metrics.height() };
	}

	void report_tally_row::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto text = palette().color(QPalette::Active, QPalette::WindowText);
		const auto box  = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		if (underMouse() || isDown()) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(with_alpha(text, isDown() ? 0.10F : 0.06F));
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
		if (hasFocus()) {
			painter.setBrush(Qt::NoBrush);
			painter.setPen(QPen{ palette().color(QPalette::Highlight), 1.5 });
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
		const auto ink = View.ink_of(Count);
		// The segment: the bar's ends are round, where two segments meet
		// they are square.
		// One path, filled once, so a translucent tone is even throughout.
		const auto segment = QRectF{ segment_rect() };
		const auto radius  = report_tally_view::bar_width / 2.0;
		auto       shape   = QPainterPath{ };
		shape.addRoundedRect(segment, radius, radius);
		if (Index != 0U) {
			auto square = QPainterPath{ };
			square.addRect(QRectF{ segment.left(), segment.top(), segment.width(), radius });
			shape = shape.united(square);
		}
		if (Index + 1U != View.rows().size()) {
			auto square = QPainterPath{ };
			square.addRect(QRectF{ segment.left(), segment.bottom() - radius, segment.width(), radius });
			shape = shape.united(square);
		}
		painter.fillPath(shape, Count.Tone == core::report_tone::neutral ? with_alpha(text, 0.45F) : ink);
		painter.setPen(ink);
		painter.setFont(font());
		painter.drawText(word_rect(), Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(Count.Word));
		painter.setPen(text);
		painter.setFont(semibold(font()));
		painter.drawText(count_rect(), Qt::AlignLeft | Qt::AlignVCenter, counted(Count));
	}

	report_tally_view::report_tally_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentTurnReportTally"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 4, 0, 0);
		column->setSpacing(0);
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
		Total = 0U;
		for (const auto& count : aCounts) {
			Total += count.Count;
		}
		for (const auto& count : aCounts) {
			auto* const row = new report_tally_row{ count, Rows.size(), *this };
			connect(row, &QPushButton::clicked, this, [this, first = count.First] {
				if (Open) {
					Open(first);
				}
			});
			layout()->addWidget(row);
			Rows.push_back(row);
		}
		setVisible(!aCounts.empty());
		updateGeometry();
	}

	void report_tally_view::set_tones(const tone_colours& aTones)
	{
		if (ToneSet && Tones == aTones) {
			return;
		}
		Tones   = aTones;
		ToneSet = true;
		update();
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

	/// A band's top: a line for each band above it, and the share of the
	/// pool (a line a band) the counts above it take. Rounded from the
	/// running sum, so the bands always add up to the bar.
	auto report_tally_view::band_top(const std::size_t aIndex) const -> int
	{
		const auto line  = static_cast<qint64>(line_height());
		const auto bands = static_cast<qint64>(Rows.size());
		auto       above = qint64{ 0 };
		for (std::size_t index = 0U; index < aIndex && index < Rows.size(); ++index) {
			above += Rows[index]->count().Count;
		}
		const auto shared = Total == 0U ? 0 : ((line * bands * above) + (Total / 2U)) / Total;
		return static_cast<int>((line * static_cast<qint64>(std::min(aIndex, Rows.size()))) + shared);
	}

	auto report_tally_view::band_height(const std::size_t aIndex) const -> int { return band_top(aIndex + 1U) - band_top(aIndex); }

	auto report_tally_view::bar_height() const -> int { return band_top(Rows.size()); }

	auto report_tally_view::ink_of(const core::dressed_count& aCount) const -> QColor
	{
		const auto tones = ToneSet ? Tones : builtin_tone_colours(palette().color(QPalette::Window).lightness() < 128);
		return tone_colour(aCount.Tone, tones).value_or(palette().color(QPalette::Active, QPalette::WindowText));
	}

	void report_tally_view::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange) {
			for (auto* const row : Rows) {
				row->updateGeometry();
			}
			if (layout() != nullptr) {
				layout()->invalidate();
			}
			updateGeometry();
		}
	}
} // namespace bpr::qt
