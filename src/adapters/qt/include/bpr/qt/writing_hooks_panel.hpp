#ifndef BPR_QT_WRITING_HOOKS_PANEL_HPP
#define BPR_QT_WRITING_HOOKS_PANEL_HPP
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/application/writing_hooks.hpp"
#include <QWidget>
#include <functional>
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;
namespace bpr::qt {
	/// \brief Profile hook instruction controls and an explicit nonpublishing test.
	class writing_hooks_panel final : public QWidget
	{
	public:
		explicit writing_hooks_panel(QWidget* aParent = nullptr);
		void set_dispatch(std::function<bool(core::writing_hook, std::string)> aSave, std::function<void(core::writing_hook)> aTest);
		void present(const core::writing_hook_settings& aSettings);
		/// \brief [[BPR-1090]] The profile's label helper, which decides whether the feedback hook is sent.
		void               present_label_helper(application::review_label_helper aHelper);
		void               show_notice(std::string_view aText);
		[[nodiscard]] auto selected_hook() const -> core::writing_hook;

	private:
		void                                                 show_hook();
		core::writing_hook_settings                          Settings;
		application::review_label_helper                     LabelHelper = application::review_label_helper::off;
		std::function<bool(core::writing_hook, std::string)> Save;
		std::function<void(core::writing_hook)>              Test;
		QComboBox*                                           Hook;
		QPlainTextEdit*                                      Instructions;
		QLabel*                                              Source;
		QLabel*                                              Scope;
		QPushButton*                                         TestButton = nullptr;
		QLabel*                                              Status;
	};
}
#endif
