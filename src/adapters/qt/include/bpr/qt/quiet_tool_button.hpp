#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_TOOL_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_TOOL_BUTTON_HPP

#include <QToolButton>

namespace bpr::qt {
	/// \brief Text choices and secondary actions using the Overview's quiet chip.
	/// \details A checkable choice is raised only when selected. An ordinary
	/// action is raised at rest unless autoRaise is set. Qt still owns signals,
	/// exclusivity, accessibility and input. Menus, arrows, icon-only and stacked
	/// icon/text tools retain native painting; they are specialized variants.
	class quiet_tool_button final : public QToolButton
	{
	public:
		explicit quiet_tool_button(QWidget* aParent = nullptr);
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		[[nodiscard]] auto uses_chip() const -> bool;
		bool               KeyboardFocus = false;
	};
} // namespace bpr::qt

#endif
