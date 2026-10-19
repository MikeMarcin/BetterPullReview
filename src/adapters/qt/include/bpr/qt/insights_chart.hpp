#ifndef BPR_QT_INSIGHTS_CHART_HPP
#define BPR_QT_INSIGHTS_CHART_HPP
#include <QWidget>
#include <optional>
#include <vector>
class QPainter;
namespace bpr::qt {
	/// \brief One rendered interval; an absent quantity is a visible gap.
	struct insights_chart_point
	{
		std::int64_t          UnixSeconds = 0;
		std::optional<double> Background;
		double                Foreground = 0;
		std::optional<double> Events;
		bool                  Reset            = false;
		bool                  GapBefore        = false;
		std::optional<double> BackgroundBudget = std::nullopt;
	};
	/// \brief Shared lane chart projection. Both quantities share a unit; events have their own scale.
	struct insights_chart_data
	{
		QString                           Name;
		QString                           Description;
		QString                           Quantity;
		QString                           BackgroundName;
		QString                           ForegroundName;
		QString                           EventsName;
		QString                           Empty;
		std::int64_t                      StartUnixSeconds = 0;
		std::int64_t                      EndUnixSeconds   = 0;
		std::int64_t                      GapSeconds       = 90;
		std::optional<double>             Capacity;
		std::optional<double>             Reference;
		std::optional<double>             EventsReference;
		std::vector<insights_chart_point> Points;
	};
	/// \brief Theme-aware, gap-preserving stacked area chart with separate event lane and local hover card.
	class insights_chart final : public QWidget
	{
	public:
		explicit insights_chart(QWidget* aParent = nullptr);
		void               apply(insights_chart_data aData);
		void               set_colors(QColor aBackground, QColor aForeground);
		[[nodiscard]] auto hovered_index() const noexcept -> std::optional<std::size_t>;
		[[nodiscard]] auto hover_card_rect() const -> QRectF;
		[[nodiscard]] auto quantity_maximum() const -> double;

	protected:
		void paintEvent(QPaintEvent*) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void leaveEvent(QEvent*) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		[[nodiscard]] auto         x_for(std::int64_t aAt) const -> double;
		[[nodiscard]] auto         main_plot() const -> QRectF;
		[[nodiscard]] auto         event_plot() const -> QRectF;
		void                       paint_lanes(QPainter& aPainter);
		void                       paint_quantity(QPainter& aPainter, const QRectF& aPlot, double aMaximum);
		void                       paint_events(QPainter& aPainter, const QRectF& aPlot, double aMaximum);
		void                       paint_hover(QPainter& aPainter);
		insights_chart_data        Data;
		QColor                     BackgroundColor;
		QColor                     ForegroundColor;
		std::optional<std::size_t> Hovered;
		double                     HoverY = 0.0;
	};
}
#endif
