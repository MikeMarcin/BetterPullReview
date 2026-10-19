#ifndef BPR_QT_INSIGHTS_WINDOW_HPP
#define BPR_QT_INSIGHTS_WINDOW_HPP

#include "bpr/application/insights_projection.hpp"

#include <QWidget>

class QLabel;
class QTabBar;
class QStackedWidget;
class QVBoxLayout;
class QEvent;
class QTableWidget;
class QTimer;
class QShowEvent;
class QHideEvent;
class QKeyEvent;

namespace bpr::qt {
	class insights_chart;
	class insights_calendar;
	/// \brief Per-profile fixed-size Insights shell. All values arrive from the local snapshot host.
	class insights_window final : public QWidget
	{
	public:
		explicit insights_window(QWidget* aParent = nullptr);
		void set_dispatch(application::insights_dispatch aDispatch);
		void set_profile_label(const QString& aLabel);
		void set_chart_colors(QColor aPrimary, QColor aSecondary);
		void present(QWidget* aOpener);
		void apply(const application::insights_snapshot& aSnapshot);
		/// \brief Requests local data only while this window is visible.
		void refresh();
		/// \brief The one content size, clamped to a screen's available geometry with an 18 px margin.
		[[nodiscard]] static auto content_size(const QSize& aAvailable) -> QSize;

	protected:
		void showEvent(QShowEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		void                            set_refresh_interval();
		void                            build_pages();
		void                            build_work_page(QVBoxLayout& aColumn);
		void                            update_work();
		void                            build_energy_page(QVBoxLayout& aColumn);
		void                            update_energy();
		void                            update_energy_chart();
		void                            update_energy_facts(std::size_t aRange);
		void                            build_reviews_page(QVBoxLayout& aColumn);
		void                            update_reviews();
		void                            update_review_records();
		void                            update_summary_reviews();
		void                            build_github_page(QVBoxLayout& aColumn);
		void                            update_github();
		void                            update_github_budgets();
		void                            update_github_requests(bool aDay);
		void                            update_github_processes(bool aDay);
		void                            update_github_chart(bool aDay);
		void                            update_problems();
		void                            build_empty_page(int aIndex, QVBoxLayout& aColumn);
		void                            refresh_typography();
		application::insights_dispatch  Dispatch;
		QTabBar*                        Tabs                 = nullptr;
		QStackedWidget*                 Pages                = nullptr;
		QLabel*                         LocalDataDetail      = nullptr;
		QLabel*                         BackgroundWorkDetail = nullptr;
		QLabel*                         ProfileLabel         = nullptr;
		QLabel*                         Status               = nullptr;
		QLabel*                         LocalData            = nullptr;
		QLabel*                         BackgroundWork       = nullptr;
		QLabel*                         StorageStatus        = nullptr;
		QTableWidget*                   Stores               = nullptr;
		QTimer*                         RefreshTimer         = nullptr;
		QTimer*                         AgeTimer             = nullptr;
		QWidget*                        BudgetBar            = nullptr;
		QLabel*                         Problems             = nullptr;
		QLabel*                         ProblemsDetail       = nullptr;
		std::array<QLabel*, 3>          GithubValues{ };
		std::array<QLabel*, 3>          GithubDetails{ };
		QTabBar*                        GithubRange       = nullptr;
		insights_chart*                 GithubChart       = nullptr;
		QTableWidget*                   RequestCategories = nullptr;
		QTableWidget*                   ProcessFacts      = nullptr;
		core::insights_github_snapshot  GithubSnapshot;
		core::insights_reviews_snapshot ReviewsSnapshot;
		QTabBar*                        ReviewsRange = nullptr;
		QLabel*                         CountedSince = nullptr;
		std::array<QLabel*, 4>          ReviewValues{ }, ReviewDetails{ };
		std::array<QLabel*, 2>          SummaryReviewValues{ }, SummaryReviewDetails{ };
		insights_calendar*              ReviewCalendar = nullptr;
		QTableWidget*                   ReviewEndings  = nullptr;
		QTableWidget*                   ReviewRecords  = nullptr;
		core::insights_energy_snapshot  EnergySnapshot;
		std::array<QLabel*, 3>          EnergyValues{ }, EnergyDetails{ };
		QLabel*                         SummaryIdle       = nullptr;
		QLabel*                         SummaryIdleDetail = nullptr;
		QTabBar*                        EnergyRange       = nullptr;
		QStackedWidget*                 EnergyPlots       = nullptr;
		insights_chart*                 EnergyChart       = nullptr;
		insights_calendar*              EnergyCalendar    = nullptr;
		QTableWidget*                   EnergyFacts       = nullptr;

		core::insights_work_snapshot WorkSnapshot;
		QTabBar*                     WorkRange = nullptr;
		std::array<QLabel*, 3>       WorkValues{ }, WorkDetails{ };
		QTableWidget*                WorkFacts = nullptr;
		QTableWidget*                SlowFiles = nullptr;
		QColor                       PrimaryChartColor;
		QColor                       SecondaryChartColor;
		std::int64_t                 BuiltUnixSeconds = 0;
	};
}

#endif
