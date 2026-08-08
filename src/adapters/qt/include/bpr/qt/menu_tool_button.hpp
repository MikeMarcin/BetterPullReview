#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MENU_TOOL_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MENU_TOOL_BUTTON_HPP

#include <QToolButton>

namespace bpr::qt {
	/// \brief A tool button that leaves room for the menu indicator it draws.
	///
	/// \details `QToolButton` reserves indicator width in its size hint for
	/// `MenuButtonPopup`, where the indicator is a separate clickable zone, but
	/// not for `InstantPopup`, where the whole button opens the menu. The
	/// indicator is still painted at the trailing edge, so a text-only button
	/// asks for room for its label and then draws a chevron past the end of it.
	///
	/// The first attended review of BPR-0194 reported exactly that on a real
	/// display: the indicator drawn outside its own widget rectangle, with the
	/// label running edge to edge and no elision. Measured offscreen the button
	/// asked for 168 device-independent pixels to hold 153 of text and a 16
	/// pixel indicator — one short, which is enough to put the chevron over the
	/// frame.
	///
	/// The correction is asked of the style rather than assumed: the indicator's
	/// width is `PM_MenuButtonIndicator`, which varies by style and by device
	/// pixel ratio, and a constant added here would be right on the platform it
	/// was measured on and wrong everywhere else.
	class menu_tool_button final : public QToolButton
	{
	public:
		using QToolButton::QToolButton;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	private:
		/// \brief Adds the indicator width the base class leaves out.
		[[nodiscard]] auto with_indicator(QSize aHint) const -> QSize;
	};
} // namespace bpr::qt

#endif
