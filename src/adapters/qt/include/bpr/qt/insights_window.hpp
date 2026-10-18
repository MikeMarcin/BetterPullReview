#ifndef BPR_QT_INSIGHTS_WINDOW_HPP
#define BPR_QT_INSIGHTS_WINDOW_HPP

#include "bpr/application/insights_projection.hpp"

#include <QWidget>

class QLabel;
class QTabWidget;
class QTableWidget;
class QTimer;
class QShowEvent;
class QHideEvent;
class QKeyEvent;

namespace bpr::qt {
	/// \brief Per-profile fixed-size Insights shell. All values arrive from the local snapshot host.
	class insights_window final : public QWidget
	{
	public:
		explicit insights_window(QWidget* aParent = nullptr);
		void set_dispatch(application::insights_dispatch aDispatch);
		void set_profile_label(const QString& aLabel);
		void present(QWidget* aOpener);
		void apply(const application::insights_snapshot& aSnapshot);
		/// \brief Requests local data only while this window is visible.
		void refresh();
		/// \brief The one content size, clamped to a screen's available geometry with an 18 px margin.
		[[nodiscard]] static auto content_size(const QSize& aAvailable) -> QSize;

	protected:
		void showEvent(QShowEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		void                           set_refresh_interval();
		application::insights_dispatch Dispatch;
		QTabWidget*                    Tabs             = nullptr;
		QLabel*                        ProfileLabel     = nullptr;
		QLabel*                        Status           = nullptr;
		QLabel*                        LocalData        = nullptr;
		QLabel*                        BackgroundWork   = nullptr;
		QLabel*                        StorageStatus    = nullptr;
		QTableWidget*                  Stores           = nullptr;
		QTimer*                        RefreshTimer     = nullptr;
		QTimer*                        AgeTimer         = nullptr;
		std::int64_t                   BuiltUnixSeconds = 0;
	};
}

#endif
