#ifndef BPR_QT_INSIGHTS_CALENDAR_HPP
#define BPR_QT_INSIGHTS_CALENDAR_HPP
#include "bpr/core/insights.hpp"
#include <QWidget>
namespace bpr::qt {
	/// \brief Monday-first local-day activity calendar. Core supplies the range and dot sizes.
	class insights_calendar final : public QWidget
	{
	public:
		explicit insights_calendar(QWidget* aParent = nullptr);
		void apply(const core::insights_reviews_snapshot& aSnapshot);
		void set_color(QColor aColor);

	protected:
		void paintEvent(QPaintEvent*) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void leaveEvent(QEvent*) override;

	private:
		[[nodiscard]] auto              day_box(std::size_t aIndex) const -> QRectF;
		core::insights_reviews_snapshot Snapshot;
		QColor                          Ink;
		std::optional<std::size_t>      Hovered;
	};
}
#endif
