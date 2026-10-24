#ifndef BPR_SRC_ADAPTERS_QT_SRC_BUTTON_SURFACE_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_BUTTON_SURFACE_HPP

class QAbstractButton;
class QFocusEvent;
class QPainter;

namespace bpr::qt {
	/// \brief Keeps keyboard focus visible without making a mouse click look selected.
	void update_button_keyboard_focus(bool& aKeyboardFocus, const QFocusEvent& aEvent);

	/// \brief Draws the accepted Overview chip, hover/press and keyboard focus surface.
	/// \details Content and interaction remain with the button. All colours are read
	/// from its current palette; no native bezel or cached theme colour is involved.
	void paint_button_surface(QPainter& aPainter, const QAbstractButton& aButton, bool aRaised, bool aKeyboardFocus);
} // namespace bpr::qt

#endif
