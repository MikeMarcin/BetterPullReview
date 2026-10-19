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
	auto insights_chart::main_plot() const -> QRectF { return QRectF{ 10.0, 48.0, std::max(1.0, static_cast<double>(width()) - 20.0), std::max(1.0, static_cast<double>(height()) - 74.0) }; }
	auto insights_chart::event_plot() const -> QRectF { return { 10.0, 16.0, std::max(1.0, static_cast<double>(width()) - 20.0), 14.0 }; }
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
	void insights_chart::paint_series(QPainter& aPainter, const QRectF& aPlot, const double aMaximum, const bool aEvents)
	{
		const auto  ink           = BackgroundColor.isValid() ? BackgroundColor : palette().color(QPalette::Highlight);
		const auto  foregroundInk = ForegroundColor.isValid() ? ForegroundColor : palette().color(QPalette::PlaceholderText);
		const auto  value         = [aEvents](const insights_chart_point& aPoint) { return aEvents ? aPoint.Events : aPoint.Background; };
		const auto  y             = [&](const double aValue) { return aPlot.bottom() - nonnegative(aValue) / aMaximum * aPlot.height(); };
		std::size_t first         = 0;
		while (first < Data.Points.size()) {
			if (!value(Data.Points[first])) {
				++first;
				continue;
			}
			std::size_t end = first + 1;
			while (end < Data.Points.size() && value(Data.Points[end]) && Data.Points[end].UnixSeconds - Data.Points[end - 1].UnixSeconds <= Data.GapSeconds && !Data.Points[end].Reset) {
				++end;
			}
			QPainterPath lower, upper, lowerFill, upperFill;
			const auto   firstX = x_for(Data.Points[first].UnixSeconds);
			lower.moveTo(firstX, y(*value(Data.Points[first])));
			lowerFill.moveTo(firstX, aPlot.bottom());
			lowerFill.lineTo(lower.currentPosition());
			upper.moveTo(firstX, y(*value(Data.Points[first]) + (aEvents ? 0.0 : Data.Points[first].Foreground)));
			upperFill.moveTo(lower.currentPosition());
			upperFill.lineTo(upper.currentPosition());
			for (std::size_t index = first + 1; index < end; ++index) {
				const auto& point = Data.Points[index];
				lower.lineTo(x_for(point.UnixSeconds), y(*value(point)));
				lowerFill.lineTo(lower.currentPosition());
				upper.lineTo(x_for(point.UnixSeconds), y(*value(point) + (aEvents ? 0.0 : point.Foreground)));
				upperFill.lineTo(upper.currentPosition());
			}
			lowerFill.lineTo(x_for(Data.Points[end - 1].UnixSeconds), aPlot.bottom());
			lowerFill.closeSubpath();
			for (std::size_t index = end; index > first; --index) {
				const auto& point = Data.Points[index - 1];
				upperFill.lineTo(x_for(point.UnixSeconds), y(*value(point)));
			}
			upperFill.closeSubpath();
			aPainter.fillPath(lowerFill, tint(ink, 0.20));
			aPainter.setPen(QPen{ ink, 1.5 });
			aPainter.drawPath(lower);
			if (!aEvents && !Data.ForegroundName.isEmpty()) {
				aPainter.fillPath(upperFill, tint(foregroundInk, 0.20));
				aPainter.setPen(QPen{ foregroundInk, 1.5 });
				aPainter.drawPath(upper);
			}
			aPainter.setPen(QPen{ ink, 1.5 });
			aPainter.drawPath(lower);
			if (end == first + 1) {
				aPainter.setBrush(ink);
				aPainter.setPen(Qt::NoPen);
				aPainter.drawEllipse(lower.currentPosition(), 2.0, 2.0);
				if (!aEvents && !Data.ForegroundName.isEmpty()) {
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
		aPainter.setPen(tint(palette().color(QPalette::WindowText), 0.12));
		for (int line = 0; line <= 3; ++line) {
			const auto y = mainPlot.top() + mainPlot.height() * static_cast<double>(line) / 3.0;
			aPainter.drawLine(QPointF{ mainPlot.left(), y }, QPointF{ mainPlot.right(), y });
		}
		aPainter.drawLine(eventsPlot.bottomLeft(), eventsPlot.bottomRight());
		paint_series(aPainter, mainPlot, maximum, false);
		paint_series(aPainter, eventsPlot, eventsMaximum, true);
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
		aPainter.drawText(QRectF{ 10.0, 0.0, mainPlot.width(), 15.0 }, Qt::AlignLeft | Qt::AlignVCenter, Data.EventsName);
		aPainter.drawText(QRectF{ 10.0, 0.0, mainPlot.width(), 15.0 }, Qt::AlignRight | Qt::AlignVCenter, tr("max %1").arg(number(eventsMaximum)));
		aPainter.drawText(QRectF{ 10.0, 32.0, mainPlot.width(), 15.0 }, Qt::AlignLeft | Qt::AlignVCenter, Data.Quantity);
		aPainter.drawText(QRectF{ 10.0, 32.0, mainPlot.width(), 15.0 }, Qt::AlignRight | Qt::AlignVCenter, tr("max %1").arg(number(maximum)));
		QString legend = Data.BackgroundName;
		if (!Data.ForegroundName.isEmpty()) {
			legend += QStringLiteral("  +  ") + Data.ForegroundName;
		}
		legend += QStringLiteral("    ·    ") + Data.EventsName;
		aPainter.drawText(QRectF{ 10.0, mainPlot.bottom() + 5.0, mainPlot.width(), 18.0 }, Qt::AlignLeft | Qt::AlignVCenter, legend);
	}
	auto insights_chart::hover_card_rect() const -> QRectF
	{
		if (!Hovered || *Hovered >= Data.Points.size()) {
			return { };
		}
		const auto cardWidth  = std::min(250.0, static_cast<double>(width()) - 16.0);
		const auto cardHeight = std::min(Data.ForegroundName.isEmpty() ? 70.0 : 88.0, static_cast<double>(height()) - 16.0);
		const auto x          = x_for(Data.Points[*Hovered].UnixSeconds);
		const auto left       = x + 12.0 + cardWidth <= static_cast<double>(width()) - 8.0 ? x + 12.0 : x - 12.0 - cardWidth;
		return { std::clamp(left, 8.0, std::max(8.0, static_cast<double>(width()) - 8.0 - cardWidth)), 8.0, cardWidth, cardHeight };
	}
	void insights_chart::paint_hover(QPainter& aPainter)
	{
		if (!Hovered || *Hovered >= Data.Points.size()) {
			return;
		}
		const auto& point = Data.Points[*Hovered];
		const auto  x     = x_for(point.UnixSeconds);
		const auto  plot  = main_plot();
		aPainter.setPen(QPen{ palette().color(QPalette::WindowText), 1.0, Qt::DotLine });
		aPainter.drawLine(QPointF{ x, event_plot().top() }, QPointF{ x, plot.bottom() });
		if (point.Background) {
			aPainter.setBrush(BackgroundColor.isValid() ? BackgroundColor : palette().color(QPalette::Highlight));
			aPainter.setPen(QPen{ palette().color(QPalette::Base), 1.5 });
			aPainter.drawEllipse(QPointF{ x, plot.bottom() - (*point.Background + point.Foreground) / quantity_maximum() * plot.height() }, 3.5, 3.5);
		}
		const auto box = hover_card_rect();
		aPainter.setBrush(palette().color(QPalette::Base));
		aPainter.setPen(QPen{ tint(palette().color(QPalette::WindowText), 0.25), 1.0 });
		aPainter.drawRoundedRect(box, 5.0, 5.0);
		auto textBox = box.adjusted(9.0, 5.0, -9.0, -5.0);
		aPainter.setPen(palette().color(QPalette::WindowText));
		const auto  time = QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(point.UnixSeconds).time(), QLocale::ShortFormat);
		QStringList lines{ time, Data.BackgroundName + QStringLiteral(": ") + (point.Background ? number(*point.Background) : tr("No reading")) };
		if (!Data.ForegroundName.isEmpty()) {
			lines.push_back(Data.ForegroundName + QStringLiteral(": ") + number(point.Foreground));
		}
		lines.push_back(Data.EventsName + QStringLiteral(": ") + (point.Events ? number(*point.Events) : tr("No sample")));
		bool       title    = true;
		const auto bodyFont = aPainter.font();
		for (const auto& line : lines) {
			auto rowFont = bodyFont;
			if (title) {
				rowFont.setWeight(QFont::DemiBold);
			}
			aPainter.setFont(rowFont);
			title = false;
			aPainter.drawText(QRectF{ textBox.left(), textBox.top(), textBox.width(), 17.0 }, Qt::AlignLeft | Qt::AlignVCenter, line);
			textBox.setTop(textBox.top() + 17.0);
		}
	}
	void insights_chart::paintEvent(QPaintEvent*)
	{
		QPainter painter{ this };
		painter.setRenderHint(QPainter::Antialiasing);
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
