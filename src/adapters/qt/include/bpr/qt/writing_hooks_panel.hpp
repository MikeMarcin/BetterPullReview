#ifndef BPR_QT_WRITING_HOOKS_PANEL_HPP
#define BPR_QT_WRITING_HOOKS_PANEL_HPP
#include "bpr/application/writing_hooks.hpp"
#include <QWidget>
#include <functional>
class QComboBox;
class QPlainTextEdit;
class QLabel;
namespace bpr::qt {
	/// \brief Profile hook instruction controls and an explicit nonpublishing test.
	class writing_hooks_panel final : public QWidget
	{
	public:
		explicit writing_hooks_panel(QWidget* aParent = nullptr);
		void set_dispatch(std::function<bool(core::writing_hook, std::string)> aSave, std::function<void(core::writing_hook)> aTest);
		void present(const core::writing_hook_settings& aSettings);
		void show_notice(std::string_view aText);

	private:
		void                                                 show_hook();
		core::writing_hook_settings                          Settings;
		std::function<bool(core::writing_hook, std::string)> Save;
		std::function<void(core::writing_hook)>              Test;
		QComboBox*                                           Hook;
		QPlainTextEdit*                                      Instructions;
		QLabel*                                              Source;
		QLabel*                                              Status;
	};
}
#endif
