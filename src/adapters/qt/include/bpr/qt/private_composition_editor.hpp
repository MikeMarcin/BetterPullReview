#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_COMPOSITION_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_COMPOSITION_EDITOR_HPP
#include "bpr/application/private_review_projection.hpp"
#include <QDialog>
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;
namespace bpr::qt {
	/// Local composition has no publish intent. Publication stays in the ordinary draft editor.
	class private_composition_editor final : public QDialog
	{
	public:
		explicit private_composition_editor(QWidget* = nullptr);
		void set_dispatch(application::private_composition_dispatch);
		void present(const application::private_composition_projection&);
		void show_status(std::string_view);

	private:
		void                                        dispatch(application::private_composition_action);
		application::private_composition_dispatch   Dispatch;
		application::private_composition_projection Shown;
		QComboBox*                                  Mode;
		QComboBox*                                  Target;
		QComboBox*                                  Workspace;
		QComboBox*                                  Profile;
		QLineEdit*                                  Model;
		QLineEdit*                                  Effort;
		QPlainTextEdit*                             Sources;
		QPlainTextEdit*                             Body;
		QLabel*                                     Status;
		QLabel*                                     Actor;
		QPushButton*                                Accept;
		bool                                        Applying = false;
	};
}
#endif
