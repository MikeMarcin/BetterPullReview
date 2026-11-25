#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_PUSH_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_PUSH_BUTTON_HPP

#include <QPushButton>

namespace bpr::qt {
	/// \brief Secondary push actions and choices using the Overview chip.
	/// \details Retains QPushButton's dialog, keyboard and accessibility behavior.
	/// Default, menu and icon-only buttons retain native presentation. A flat
	/// secondary stays bare at rest; checkable choices are raised when selected.
	/// A chip whose words are a sentence may wrap them (`set_word_wrap`): in a
	/// row that gives it less than their width it takes further lines instead
	/// of being cut, as a drawer's labels do.
	class quiet_push_button final : public QPushButton
	{
	public:
		explicit quiet_push_button(QWidget* aParent = nullptr);
		quiet_push_button(const QString& aText, QWidget* aParent = nullptr);
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		/// \brief Lets the words of a chip without an icon take further lines
		/// where it is given less than their width. Its size hint stays the
		/// one line; its minimum becomes its longest word.
		void               set_word_wrap(bool aWrap);
		[[nodiscard]] auto word_wrap() const noexcept -> bool { return WordWrap; }
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		[[nodiscard]] auto uses_chip() const -> bool;
		[[nodiscard]] auto wraps() const -> bool;
		bool               KeyboardFocus = false;
		bool               WordWrap      = false;
	};
} // namespace bpr::qt

#endif
