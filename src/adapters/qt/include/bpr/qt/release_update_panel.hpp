#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RELEASE_UPDATE_PANEL_HPP

#include "bpr/application/release_update_projection.hpp"

#include <QPointer>
#include <QString>
#include <QWidget>
#include <functional>
#include <map>
#include <memory>
#include <vector>

class QAction;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QMenuBar;
class QProgressBar;
class QPushButton;
class QTextEdit;

namespace bpr::qt {
	/// \brief Shared updater presentation for Settings and the standalone release offer.
	///
	/// One headline states where the update stands and one quiet line explains
	/// it. Only commands the core currently allows are shown; the most
	/// consequential of them is the default button ([[BPR-1671]]). Release
	/// notes read as formatted Markdown, without HTML, resources or links.
	class release_update_panel final : public QWidget
	{
	public:
		explicit release_update_panel(QWidget* aParent = nullptr);
		void               set_dispatch(application::release_update_dispatch aDispatch);
		void               adopt(const application::release_update_projection& aProjection, bool aDurable);
		[[nodiscard]] auto action(core::release_update_command aCommand) const -> QAction*;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void                                                 emphasize();
		void                                                 present_notes();
		application::release_update_dispatch                 Dispatch;
		application::release_update_projection               Projection;
		std::map<core::release_update_command, QAction*>     Actions;
		std::map<core::release_update_command, QPushButton*> Buttons;
		QCheckBox*                                           Automatic = nullptr;
		QLabel*                                              Status    = nullptr;
		QLabel*                                              Detail    = nullptr;
		QTextEdit*                                           Notes     = nullptr;
		QString                                              NotesSource;
		QLabel*                                              Notice   = nullptr;
		QProgressBar*                                        Progress = nullptr;
		QPushButton*                                         View     = nullptr;
		QHBoxLayout*                                         Row      = nullptr;
	};
	/// \brief Keeps one visible macOS application-menu gesture across window and desktop bars.
	class release_update_menu final : public QObject
	{
	public:
		explicit release_update_menu(std::function<void()> aCheck);
		~release_update_menu() override;
		void attach(QMenuBar& aMenu);

	private:
		std::function<void()>           Check;
		std::unique_ptr<QObject>        Native;
		std::vector<QPointer<QMenuBar>> Menus;
	};
}
#endif
