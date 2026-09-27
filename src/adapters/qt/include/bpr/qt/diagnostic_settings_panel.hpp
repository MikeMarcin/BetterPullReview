#ifndef BPR_QT_DIAGNOSTIC_SETTINGS_PANEL_HPP
#define BPR_QT_DIAGNOSTIC_SETTINGS_PANEL_HPP
#include "bpr/application/diagnostic_settings.hpp"
#include <QWidget>
#include <functional>
class QCheckBox;
class QButtonGroup;
class QPushButton;
class QLabel;
namespace bpr::qt {
	struct diagnostic_settings_dispatch
	{
		std::function<void(application::diagnostic_preferences)> Apply;
		std::function<void()>                                    Clear;
		std::function<void()>                                    Open;
		std::function<void()>                                    Refresh;
	};
	/// \brief Displays accepted profile diagnostics state and confirmation gestures only.
	class diagnostic_settings_panel final : public QWidget
	{
	public:
		explicit diagnostic_settings_panel(QWidget* aParent = nullptr);
		void adopt(const application::diagnostic_settings_projection& aProjection);
		void set_dispatch(diagnostic_settings_dispatch aDispatch);

	private:
		void                                        apply();
		diagnostic_settings_dispatch                Dispatch;
		application::diagnostic_settings_projection Projection;
		QCheckBox*                                  Enabled    = nullptr;
		QButtonGroup*                               Detail     = nullptr;
		QButtonGroup*                               Privacy    = nullptr;
		QPushButton*                                Clear      = nullptr;
		QPushButton*                                Open       = nullptr;
		QLabel*                                     Status     = nullptr;
		bool                                        Applying   = false;
		bool                                        Confirming = false;
	};
}
#endif
