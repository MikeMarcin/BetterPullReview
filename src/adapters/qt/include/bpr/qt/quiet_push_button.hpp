#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_PUSH_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_PUSH_BUTTON_HPP

#include <QPushButton>

namespace bpr::qt {
	/// \brief Secondary push actions and choices using the Overview chip.
	/// \details Retains QPushButton's dialog, keyboard and accessibility behavior.
	/// Default, menu and icon-only buttons retain native presentation. A flat
	/// secondary stays bare at rest; checkable choices are raised when selected.
	class quiet_push_button final : public QPushButton
	{
	public:
		explicit quiet_push_button(QWidget* aParent = nullptr);
		quiet_push_button(const QString& aText, QWidget* aParent = nullptr);
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
