#include "bpr/qt/insights_chart.hpp"
#include <QDateTime>
#include <QEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
namespace bpr::qt {
	namespace {
		auto number(const double aValue) -> QString { return QLocale{ }.toString(aValue, 'f', aValue < 10.0 && aValue != std::floor(aValue) ? 1 : 0); }
		auto tint(QColor aColor, const double aAlpha) -> QColor
		{
			aColor.setAlphaF(static_cast<float>(aAlpha));
			return aColor;
		}
		auto nonnegative(const double aValue) -> double { return std::isfinite(aValue) ? std::max(0.0, aValue) : 0.0; }
	}
	insights_chart::insights_chart(QWidget* aParent)
		: QWidget{ aParent }
	{
		setMouseTracking(true);
		setMinimumHeight(110);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	}
	void insights_chart::set_colors(QColor aBackground, QColor aForeground)
	{
		BackgroundColor = std::move(aBackground);
		ForegroundColor = std::move(aForeground);
		update();
	}
	void insights_chart::apply(insights_chart_data aData)
	{
		const auto rangeChanged = Data.StartUnixSeconds != aData.StartUnixSeconds || Data.EndUnixSeconds - Data.StartUnixSeconds != aData.EndUnixSeconds - aData.StartUnixSeconds;
		Data                    = std::move(aData);
		std::ranges::sort(Data.Points, { }, &insights_chart_point::UnixSeconds);
		if (rangeChanged || (Hovered && *Hovered >= Data.Points.size())) {
			Hovered.reset();
		}
		setAccessibleName(Data.Name);
		setAccessibleDescription(Data.Description);
		update();
	}
	auto insights_chart::hovered_index() const noexcept -> std::optional<std::size_t> { return Hovered; }
	auto insights_chart::main_plot() const -> QRectF
	{
		const auto top = event_plot().bottom() + 26.0;
		return { 10.0, top, std::max(1.0, static_cast<double>(width()) - 20.0), std::max(1.0, static_cast<double>(height()) - top - 32.0) };
	}
	auto insights_chart::event_plot() const -> QRectF { return { 10.0, 26.0, std::max(1.0, static_cast<double>(width()) - 20.0), std::max(18.0, std::floor((static_cast<double>(height()) - 84.0) * 0.24)) }; }
	auto insights_chart::x_for(const std::int64_t aAt) const -> double
	{
		const auto plot     = main_plot();
		const auto duration = static_cast<double>(std::max<std::int64_t>(1, Data.EndUnixSeconds - Data.StartUnixSeconds));
		return plot.left() + std::clamp(static_cast<double>(aAt - Data.StartUnixSeconds) / duration, 0.0, 1.0) * plot.width();
	}
	auto insights_chart::quantity_maximum() const -> double
	{
		double maximum = Data.Capacity ? nonnegative(*Data.Capacity) : 0.0;
		for (const auto& point : Data.Points) {
			if (point.Background) {
				maximum = std::max(maximum, nonnegative(*point.Background) + nonnegative(point.Foreground));
			}
		}
		return std::max(1.0, maximum);
	}
	void insights_chart::paint_events(QPainter& aPainter, const QRectF& aPlot, const double aMaximum)
	{
		const auto ink = ForegroundColor.isValid() ? ForegroundColor : palette().color(QPalette::PlaceholderText);
		const auto y   = [&](const double aValue) { return aPlot.bottom() - nonnegative(aValue) / aMaximum * aPlot.height(); };
		// ATT's event lane uses interval bars. Bound every slot by its
		// neighbours and the gap threshold, so missing time stays empty.
		for (std::size_t index = 0; index < Data.Points.size(); ++index) {
			const auto& point = Data.Points[index];
			if (!point.Events || *point.Events <= 0.0) {
				continue;
			}
			auto interval = Data.GapSeconds;
			if (index > 0) {
				interval = std::min(interval, point.UnixSeconds - Data.Points[index - 1].UnixSeconds);
			}
			if (index + 1 < Data.Points.size()) {
				interval = std::min(interval, Data.Points[index + 1].UnixSeconds - point.UnixSeconds);
			}
			const auto duration = static_cast<double>(std::max<std::int64_t>(1, Data.EndUnixSeconds - Data.StartUnixSeconds));
			const auto slot     = std::max(1.0, aPlot.width() * static_cast<double>(std::max<std::int64_t>(1, interval)) / duration * 0.8);
			const auto bar      = QRectF{ x_for(point.UnixSeconds) - slot / 2.0, y(*point.Events), slot, aPlot.bottom() - y(*point.Events) }.intersected(aPlot);
			aPainter.setPen(QPen{ ink, 1.0 });
			aPainter.setBrush(tint(ink, 0.65));
			aPainter.drawRect(bar);
		}
	}
	void insights_chart::paint_quantity(QPainter& aPainter, const QRectF& aPlot, const double aMaximum)
	{
		const auto  ink           = BackgroundColor.isValid() ? BackgroundColor : palette().color(QPalette::Highlight);
		const auto  foregroundInk = ForegroundColor.isValid() ? ForegroundColor : palette().color(QPalette::PlaceholderText);
		const auto  y             = [&](const double aValue) { return aPlot.bottom() - nonnegative(aValue) / aMaximum * aPlot.height(); };
		std::size_t first         = 0;
		while (first < Data.Points.size()) {
			if (!Data.Points[first].Background) {
				++first;
				continue;
			}
			std::size_t end = first + 1;
			while (end < Data.Points.size() && Data.Points[end].Background && Data.Points[end].UnixSeconds - Data.Points[end - 1].UnixSeconds <= Data.GapSeconds && !Data.Points[end].Reset) {
				++end;
			}
			QPainterPath lower, upper, lowerFill, upperFill;
			const auto   firstX = x_for(Data.Points[first].UnixSeconds);
			lower.moveTo(firstX, y(*Data.Points[first].Background));
			lowerFill.moveTo(firstX, aPlot.bottom());
			lowerFill.lineTo(lower.currentPosition());
			upper.moveTo(firstX, y(*Data.Points[first].Background + Data.Points[first].Foreground));
			upperFill.moveTo(lower.currentPosition());
			upperFill.lineTo(upper.currentPosition());
			for (std::size_t index = first + 1; index < end; ++index) {
				const auto& point = Data.Points[index];
				lower.lineTo(x_for(point.UnixSeconds), y(*point.Background));
				lowerFill.lineTo(lower.currentPosition());
				upper.lineTo(x_for(point.UnixSeconds), y(*point.Background + point.Foreground));
				upperFill.lineTo(upper.currentPosition());
			}
			lowerFill.lineTo(x_for(Data.Points[end - 1].UnixSeconds), aPlot.bottom());
			lowerFill.closeSubpath();
			for (std::size_t index = end; index > first; --index) {
				const auto& point = Data.Points[index - 1];
				upperFill.lineTo(x_for(point.UnixSeconds), y(*point.Background));
			}
			upperFill.closeSubpath();
			aPainter.fillPath(lowerFill, tint(ink, 0.26));
			aPainter.setPen(QPen{ ink, 1.8 });
			aPainter.drawPath(lower);
			if (!Data.ForegroundName.isEmpty()) {
				aPainter.fillPath(upperFill, tint(foregroundInk, 0.26));
				aPainter.setPen(QPen{ foregroundInk, 1.8 });
				aPainter.drawPath(upper);
			}
			aPainter.setPen(QPen{ ink, 1.8 });
			aPainter.drawPath(lower);
			if (end == first + 1) {
				aPainter.setBrush(ink);
				aPainter.setPen(Qt::NoPen);
				aPainter.drawEllipse(lower.currentPosition(), 2.0, 2.0);
				if (!Data.ForegroundName.isEmpty()) {
					aPainter.setBrush(foregroundInk);
					aPainter.drawEllipse(upper.currentPosition(), 2.0, 2.0);
				}
			}
			first = end;
		}
	}
	void insights_chart::paint_lanes(QPainter& aPainter)
	{
		double eventsMaximum = 1.0;
		for (const auto& point : Data.Points) {
			if (point.Events) {
				eventsMaximum = std::max(eventsMaximum, nonnegative(*point.Events));
			}
		}
		const auto mainPlot   = main_plot();
		const auto eventsPlot = event_plot();
		const auto maximum    = quantity_maximum();
		for (const auto& plot : { eventsPlot, mainPlot }) {
			aPainter.fillRect(plot, palette().color(QPalette::Base));
			aPainter.setPen(tint(palette().color(QPalette::WindowText), 0.24));
			aPainter.drawRect(plot);
			aPainter.setPen(tint(palette().color(QPalette::WindowText), 0.12));
			const auto divisions = plot == eventsPlot ? 2 : 4;
			for (int line = 1; line < divisions; ++line) {
				const auto y = plot.top() + plot.height() * static_cast<double>(line) / static_cast<double>(divisions);
				aPainter.drawLine(QPointF{ plot.left(), y }, QPointF{ plot.right(), y });
			}
		}
		paint_quantity(aPainter, mainPlot, maximum);
		paint_events(aPainter, eventsPlot, eventsMaximum);
		if (Data.Reference) {
			const auto y = mainPlot.bottom() - *Data.Reference / maximum * mainPlot.height();
			aPainter.setPen(QPen{ palette().color(QPalette::WindowText), 1.0, Qt::DashLine });
			aPainter.drawLine(QPointF{ mainPlot.left(), y }, QPointF{ mainPlot.right(), y });
		}
		aPainter.setPen(tint(palette().color(QPalette::WindowText), 0.35));
		for (const auto& point : Data.Points) {
			if (point.Reset) {
				aPainter.drawLine(QPointF{ x_for(point.UnixSeconds), eventsPlot.top() }, QPointF{ x_for(point.UnixSeconds), mainPlot.bottom() });
			}
		}
		aPainter.setPen(palette().color(QPalette::PlaceholderText));
		aPainter.drawText(QRectF{ 10.0, 5.0, mainPlot.width(), 17.0 }, Qt::AlignLeft | Qt::AlignVCenter, Data.EventsName);
		aPainter.drawText(QRectF{ 10.0, 5.0, mainPlot.width(), 17.0 }, Qt::AlignRight | Qt::AlignVCenter, tr("max %1").arg(number(eventsMaximum)));
		aPainter.drawText(QRectF{ 10.0, mainPlot.top() - 22.0, mainPlot.width(), 17.0 }, Qt::AlignLeft | Qt::AlignVCenter, Data.Quantity);
		aPainter.drawText(QRectF{ 10.0, mainPlot.top() - 22.0, mainPlot.width(), 17.0 }, Qt::AlignRight | Qt::AlignVCenter, tr("max %1").arg(number(maximum)));
		double     legendX = 10.0;
		const auto legend  = [&](const QString& aName, const QColor& aColor) {
			aPainter.setPen(QPen{ aColor, 2.0, Qt::SolidLine, Qt::RoundCap });
			aPainter.drawLine(QPointF{ legendX, mainPlot.bottom() + 17.0 }, QPointF{ legendX + 10.0, mainPlot.bottom() + 17.0 });
			aPainter.setPen(palette().color(QPalette::PlaceholderText));
			aPainter.drawText(QRectF{ legendX + 15.0, mainPlot.bottom() + 7.0, mainPlot.width(), 20.0 }, Qt::AlignLeft | Qt::AlignVCenter, aName);
			legendX += 30.0 + aPainter.fontMetrics().horizontalAdvance(aName);
		};
		legend(Data.EventsName, ForegroundColor.isValid() ? ForegroundColor : palette().color(QPalette::PlaceholderText));
		legend(Data.BackgroundName, BackgroundColor.isValid() ? BackgroundColor : palette().color(QPalette::Highlight));
		if (!Data.ForegroundName.isEmpty()) {
			legend(Data.ForegroundName, ForegroundColor.isValid() ? ForegroundColor : palette().color(QPalette::PlaceholderText));
		}
	}
	auto insights_chart::hover_card_rect() const -> QRectF
	{
		if (!Hovered || *Hovered >= Data.Points.size()) {
			return { };
		}
		const auto cardWidth  = std::min(280.0, static_cast<double>(width()) - 16.0);
		const auto cardHeight = std::min(Data.ForegroundName.isEmpty() ? 90.0 : 110.0, static_cast<double>(height()) - 16.0);
		const auto x          = x_for(Data.Points[*Hovered].UnixSeconds);
		const auto left       = x + 12.0 + cardWidth <= static_cast<double>(width()) - 8.0 ? x + 12.0 : x - 12.0 - cardWidth;
		const auto top        = HoverY + 12.0 + cardHeight <= static_cast<double>(height()) - 8.0 ? HoverY + 12.0 : HoverY - 12.0 - cardHeight;
		return { std::clamp(left, 8.0, std::max(8.0, static_cast<double>(width()) - 8.0 - cardWidth)), std::clamp(top, 8.0, std::max(8.0, static_cast<double>(height()) - 8.0 - cardHeight)), cardWidth, cardHeight };
	}
	void insights_chart::paint_hover(QPainter& aPainter)
	{
		if (!Hovered || *Hovered >= Data.Points.size()) {
			return;
		}
		const auto& point = Data.Points[*Hovered];
		const auto  x     = x_for(point.UnixSeconds);
		const auto  plot  = main_plot();
		aPainter.setPen(QPen{ tint(palette().color(QPalette::WindowText), 0.20), 1.0 });
		aPainter.drawLine(QPointF{ x, event_plot().top() }, QPointF{ x, plot.bottom() });
		if (point.Background) {
			aPainter.setBrush(BackgroundColor.isValid() ? BackgroundColor : palette().color(QPalette::Highlight));
			aPainter.setPen(QPen{ palette().color(QPalette::WindowText), 1.5 });
			aPainter.drawEllipse(QPointF{ x, plot.bottom() - (*point.Background + point.Foreground) / quantity_maximum() * plot.height() }, 3.5, 3.5);
		}
		const auto box = hover_card_rect();
		aPainter.setBrush(palette().color(QPalette::Base));
		aPainter.setPen(QPen{ tint(palette().color(QPalette::WindowText), 0.55), 1.0 });
		aPainter.drawRoundedRect(box, 7.0, 7.0);
		auto textBox = box.adjusted(10.0, 8.0, -10.0, -8.0);
		aPainter.setPen(palette().color(QPalette::WindowText));
		const auto time      = QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(point.UnixSeconds), QLocale::ShortFormat);
		const auto bodyFont  = aPainter.font();
		auto       titleFont = bodyFont;
		titleFont.setWeight(QFont::DemiBold);
		aPainter.setFont(titleFont);
		aPainter.drawText(QRectF{ textBox.left(), textBox.top(), textBox.width(), 20.0 }, Qt::AlignLeft | Qt::AlignVCenter, time);
		textBox.setTop(textBox.top() + 25.0);
		aPainter.setFont(bodyFont);
		const auto row = [&](const QString& aLabel, const QString& aValue) {
			aPainter.setPen(palette().color(QPalette::PlaceholderText));
			aPainter.drawText(QRectF{ textBox.left(), textBox.top(), textBox.width(), 20.0 }, Qt::AlignLeft | Qt::AlignVCenter, aLabel);
			aPainter.setPen(palette().color(QPalette::WindowText));
			aPainter.drawText(QRectF{ textBox.left(), textBox.top(), textBox.width(), 20.0 }, Qt::AlignRight | Qt::AlignVCenter, aValue);
			textBox.setTop(textBox.top() + 20.0);
		};
		row(Data.BackgroundName, point.Background ? number(*point.Background) : tr("No reading"));
		if (!Data.ForegroundName.isEmpty()) {
			row(Data.ForegroundName, number(point.Foreground));
		}
		row(Data.EventsName, point.Events ? number(*point.Events) : tr("No sample"));
	}
	void insights_chart::paintEvent(QPaintEvent*)
	{
		QPainter painter{ this };
		painter.setRenderHint(QPainter::Antialiasing);
		painter.fillRect(rect(), palette().color(QPalette::Window));
		auto textFont = font();
		textFont.setPointSizeF(textFont.pointSizeF() * 11.0 / 13.0);
		painter.setFont(textFont);
		if (Data.Points.empty()) {
			painter.setPen(palette().color(QPalette::PlaceholderText));
			painter.drawText(rect().adjusted(10, 10, -10, -10), Qt::AlignCenter, Data.Empty);
			return;
		}
		paint_lanes(painter);
		paint_hover(painter);
	}
	void insights_chart::mouseMoveEvent(QMouseEvent* aEvent)
	{
		if (Data.Points.empty()) {
			return;
		}
		std::size_t nearest  = 0;
		auto        distance = std::numeric_limits<double>::max();
		for (std::size_t index = 0; index < Data.Points.size(); ++index) {
			const auto candidate = std::abs(x_for(Data.Points[index].UnixSeconds) - aEvent->position().x());
			if (candidate < distance) {
				nearest  = index;
				distance = candidate;
			}
		}
		Hovered = nearest;
		HoverY  = aEvent->position().y();
		update();
	}
	void insights_chart::leaveEvent(QEvent*)
	{
		Hovered.reset();
		update();
	}
	void insights_chart::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::FontChange) {
			update();
		}
	}
}
