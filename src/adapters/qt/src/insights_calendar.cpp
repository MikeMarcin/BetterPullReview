#include "bpr/qt/insights_calendar.hpp"
#include <QDate>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <numeric>
#include <utility>
namespace bpr::qt {
	namespace {
		auto day_date(std::int64_t aDay) -> QDate { return QDate{ 1970, 1, 1 }.addDays(aDay); }
	}
	insights_calendar::insights_calendar(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("insightsReviewCalendar"));
		setAccessibleName(tr("Reviews submitted by local day"));
		setFixedHeight(126);
		setMinimumWidth(0);
		setMouseTracking(true);
	}
	void insights_calendar::apply(const core::insights_reviews_snapshot& aSnapshot)
	{
		Snapshot = aSnapshot;
		Hovered.reset();
		const auto total = std::accumulate(Snapshot.Calendar.begin(), Snapshot.Calendar.end(), std::uint64_t{ 0 }, [](auto aTotal, const auto& aDay) { return aTotal + aDay.Reviews; });
		setAccessibleDescription(total ? tr("%1 reviews submitted over the last 26 weeks, Monday first; dot size and activity colour show daily reviews, and days before counting started are empty.").arg(QLocale{ }.toString(total)) : tr("No submitted reviews yet. Reviews submitted on this computer will fill the 26-week calendar."));
		update();
	}
	void insights_calendar::set_color(QColor aColor)
	{
		Ink = std::move(aColor);
		update();
	}
	auto insights_calendar::day_box(const std::size_t aIndex) const -> QRectF
	{
		const auto stepX = static_cast<double>(width() - 44) / 26.0;
		const auto stepY = static_cast<double>(height() - 44) / 7.0;
		return { 34.0 + static_cast<double>(aIndex / 7) * stepX, 24.0 + static_cast<double>(aIndex % 7) * stepY, stepX, stepY };
	}
	void insights_calendar::paintEvent(QPaintEvent*)
	{
		QPainter painter{ this };
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(palette().color(QPalette::PlaceholderText));
		if (!Snapshot.Available || std::ranges::none_of(Snapshot.Calendar, [](const auto& aDay) { return aDay.Reviews != 0; })) {
			painter.drawText(rect().adjusted(12, 0, -12, 0), Qt::AlignCenter, tr("Reviews submitted on this computer will fill the calendar."));
			return;
		}
		auto smallFont = font();
		smallFont.setPointSizeF(smallFont.pointSizeF() * 11.0 / 13.0);
		painter.setFont(smallFont);
		for (const auto row : { 0, 2, 4 }) {
			painter.drawText(QRectF{ 0, day_box(static_cast<std::size_t>(row)).top(), 30, day_box(0).height() }, Qt::AlignLeft | Qt::AlignVCenter, QLocale{ }.standaloneDayName(row + 1, QLocale::NarrowFormat));
		}
		for (std::size_t index = 0; index < Snapshot.Calendar.size(); ++index) {
			const auto& point = Snapshot.Calendar[index];
			const auto  box   = day_box(index);
			const auto  date  = day_date(point.Day);
			if (index == 0 || date.day() == 1) {
				painter.setPen(palette().color(QPalette::PlaceholderText));
				painter.drawText(QRectF{ box.left(), 0, 80, 20 }, Qt::AlignLeft | Qt::AlignVCenter, QLocale{ }.standaloneMonthName(date.month(), QLocale::ShortFormat));
			}
			if (point.Day < Snapshot.StartedDay) {
				continue;
			}
			auto colour = Ink.isValid() ? Ink : palette().color(QPalette::Highlight);
			colour.setAlphaF(static_cast<float>(point.Reviews ? 0.25 + 0.75 * point.Size : 0.12));
			painter.setPen(Qt::NoPen);
			painter.setBrush(colour);
			const auto diameter = std::min(box.width(), box.height()) * 0.82 * point.Size;
			painter.drawEllipse(box.center(), diameter / 2, diameter / 2);
			if (Hovered == index) {
				painter.setPen(QPen{ palette().color(QPalette::WindowText), 1.0 });
				painter.setBrush(Qt::NoBrush);
				painter.drawEllipse(box.adjusted(1, 1, -1, -1));
			}
		}
		painter.setPen(palette().color(QPalette::PlaceholderText));
		painter.drawText(QRect{ 34, height() - 18, width() - 44, 18 }, Qt::AlignLeft | Qt::AlignVCenter, tr("26 weeks · Monday first"));
		painter.drawText(QRect{ 34, height() - 18, width() - 44, 18 }, Qt::AlignRight | Qt::AlignVCenter, tr("Less activity     More activity"));
		if (Hovered && *Hovered < Snapshot.Calendar.size()) {
			const auto&  point  = Snapshot.Calendar[*Hovered];
			const auto   anchor = day_box(*Hovered).center();
			const auto   left   = std::clamp(anchor.x() + 12.0, 8.0, std::max(8.0, static_cast<double>(width()) - 228.0));
			const auto   top    = std::clamp(anchor.y() - 68.0, 8.0, std::max(8.0, static_cast<double>(height()) - 68.0));
			const QRectF card{ left, top, 220.0, 60.0 };
			painter.setBrush(palette().color(QPalette::ToolTipBase));
			painter.setPen(palette().color(QPalette::Mid));
			painter.drawRoundedRect(card, 5, 5);
			painter.setPen(palette().color(QPalette::ToolTipText));
			auto titleFont = smallFont;
			titleFont.setWeight(QFont::DemiBold);
			painter.setFont(titleFont);
			painter.drawText(card.adjusted(10, 6, -10, -30), Qt::AlignLeft | Qt::AlignVCenter, QLocale{ }.toString(day_date(point.Day), QLocale::LongFormat));
			painter.setFont(smallFont);
			painter.drawText(card.adjusted(10, 28, -10, -6), Qt::AlignLeft | Qt::AlignVCenter, tr("Reviews submitted"));
			painter.drawText(card.adjusted(10, 28, -10, -6), Qt::AlignRight | Qt::AlignVCenter, QLocale{ }.toString(point.Reviews));
		}
	}
	void insights_calendar::mouseMoveEvent(QMouseEvent* aEvent)
	{
		Hovered.reset();
		for (std::size_t index = 0; index < Snapshot.Calendar.size(); ++index) {
			if (Snapshot.Calendar[index].Day >= Snapshot.StartedDay && day_box(index).contains(aEvent->position())) {
				Hovered = index;
				break;
			}
		}
		update();
	}
	void insights_calendar::leaveEvent(QEvent*)
	{
		Hovered.reset();
		update();
	}
}
