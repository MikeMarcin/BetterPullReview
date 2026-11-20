#include "bpr/qt/report_tally_view.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The row's own padding, left and right.
		constexpr int row_pad = 8;
		/// Between the words and the bars, and the bars and the counts.
		constexpr int word_gap  = 12;
		constexpr int count_gap = 10;
		/// The lane the rule hangs in: under the attachment's page, whose
		/// centre is 20 points in.
		constexpr int lane  = 34;
		constexpr int spine = 20;
		/// A bar's thickness.
		constexpr int bar_height = 4;

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
	} // namespace

	report_tally_row::report_tally_row(const core::dressed_count& aCount, report_tally_view& aView)
		: QPushButton{ &aView }
		, Count{ aCount }
		, View{ aView }
	{
		setObjectName(QStringLiteral("agentTurnReportTallyRow"));
		setCursor(Qt::PointingHandCursor);
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover, true);
		// It takes the summary's width, so its hover and press span it; what
		// it draws stays compact (`track_rect`).
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		const auto word = QString::fromStdString(aCount.Word);
		setAccessibleName(tr("%1: %2").arg(word, counted(aCount)));
		setToolTip(tr("Open the report at its first %1").arg(word));
		setAccessibleDescription(toolTip());
	}

	auto report_tally_row::sizeHint() const -> QSize { return minimumSizeHint(); }

	/// No narrower than its word, the shortest track and its count: the
	/// drawer never pans sideways.
	auto report_tally_row::minimumSizeHint() const -> QSize
	{
		const auto metrics = QFontMetrics{ font() };
		const auto width   = row_pad + View.word_column() + word_gap + report_tally_view::least_bar + count_gap + View.count_column() + row_pad;
		return { width, std::max(22, metrics.height() + 8) };
	}

	auto report_tally_row::word_rect() const -> QRect
	{
		const auto metrics = QFontMetrics{ font() };
		return { row_pad, (height() - metrics.height()) / 2, metrics.horizontalAdvance(QString::fromStdString(Count.Word)), metrics.height() };
	}

	/// Right-aligned in the counts' column, just after the track.
	auto report_tally_row::count_rect() const -> QRect
	{
		const auto metrics = QFontMetrics{ semibold(font()) };
		const auto advance = metrics.horizontalAdvance(counted(Count));
		const auto right   = track_rect().right() + 1 + count_gap + View.count_column();
		return { right - advance, (height() - metrics.height()) / 2, advance, metrics.height() };
	}

	/// The room between the words and the counts, at most
	/// `preferred_bar` long: a wide drawer keeps the summary compact.
	auto report_tally_row::track_rect() const -> QRect
	{
		const auto left  = row_pad + View.word_column() + word_gap;
		const auto right = width() - row_pad - View.count_column() - count_gap;
		return { left, (height() - bar_height) / 2, std::clamp(right - left, 0, report_tally_view::preferred_bar), bar_height };
	}

	auto report_tally_row::bar_rect() const -> QRect
	{
		auto       bar     = track_rect();
		const auto largest = std::max<std::uint32_t>(1U, View.largest());
		bar.setWidth(std::max(bar_height, static_cast<int>(static_cast<qint64>(bar.width()) * Count.Count / largest)));
		return bar;
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
		// The track the bar stands in, faint, so a short bar reads as a share.
		painter.setPen(Qt::NoPen);
		const auto track = QRectF{ track_rect() };
		painter.setBrush(with_alpha(text, 0.07F));
		painter.drawRoundedRect(track, bar_height / 2.0, bar_height / 2.0);
		painter.setBrush(Count.Tone == core::report_tone::neutral ? with_alpha(text, 0.45F) : ink);
		painter.drawRoundedRect(QRectF{ bar_rect() }, bar_height / 2.0, bar_height / 2.0);
		painter.setPen(ink);
		painter.setFont(font());
		painter.drawText(word_rect(), Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(Count.Word));
		painter.setPen(text);
		painter.setFont(semibold(font()));
		painter.drawText(count_rect(), Qt::AlignRight | Qt::AlignVCenter, counted(Count));
	}

	report_tally_view::report_tally_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentTurnReportTally"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(lane, 4, 0, 0);
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
		Largest = 0U;
		for (const auto& count : aCounts) {
			Largest = std::max(Largest, count.Count);
		}
		for (const auto& count : aCounts) {
			auto* const row = new report_tally_row{ count, *this };
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

	auto report_tally_view::word_column() const -> int
	{
		const auto metrics = QFontMetrics{ font() };
		auto       widest  = 0;
		for (const auto* const row : Rows) {
			widest = std::max(widest, metrics.horizontalAdvance(QString::fromStdString(row->count().Word)));
		}
		return widest;
	}

	auto report_tally_view::count_column() const -> int
	{
		const auto metrics = QFontMetrics{ semibold(font()) };
		auto       widest  = 0;
		for (const auto* const row : Rows) {
			widest = std::max(widest, metrics.horizontalAdvance(counted(row->count())));
		}
		return widest;
	}

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

	/// The rule the rows hang from: down from under the attachment's page,
	/// a short tick to each row, ending at the last.
	void report_tally_view::paintEvent(QPaintEvent* /*aEvent*/)
	{
		if (Rows.empty()) {
			return;
		}
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ with_alpha(palette().color(QPalette::Active, QPalette::WindowText), 0.18F), 1.0 });
		const auto last = Rows.back()->geometry().center().y();
		painter.drawLine(QPointF{ spine + 0.5, 0.0 }, QPointF{ spine + 0.5, last + 0.5 });
		for (const auto* const row : Rows) {
			const auto middle = row->geometry().center().y() + 0.5;
			painter.drawLine(QPointF{ spine + 0.5, middle }, QPointF{ lane - 4.0, middle });
		}
	}
} // namespace bpr::qt
