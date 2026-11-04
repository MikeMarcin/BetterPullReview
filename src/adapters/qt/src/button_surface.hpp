#ifndef BPR_SRC_ADAPTERS_QT_SRC_BUTTON_SURFACE_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_BUTTON_SURFACE_HPP

#include <QSize>

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

	/// \brief The room the chip keeps between its edge and its content, each side.
	inline constexpr int chip_padding = 6;
	/// \brief The room between a chip's icon and its words.
	inline constexpr int chip_icon_gap = 4;

	/// \brief [[BPR-1855]]. The chip's content: its words, and its icon when shown.
	///
	/// \details The words are measured in the button's own font with fractional
	/// advances rounded up, so they fit at every display scale and with any
	/// typeface. Mnemonic markers take no room.
	[[nodiscard]] auto chip_content_size(const QAbstractButton& aButton, QSize aIconSize, bool aShowIcon) -> QSize;

	/// \brief [[BPR-1855]]. Draws the chip's icon and words centred inside its padding.
	///
	/// \details The chip owns its content geometry. The style's own label
	/// elements are not used: they may keep a further margin of their own
	/// (Windows 11 takes six pixels from each side of a push button's label),
	/// which the chip's measured size does not include, and the words would be
	/// clipped at both ends. The style still draws the words and mnemonic.
	void paint_button_label(QPainter& aPainter, const QAbstractButton& aButton, QSize aIconSize, bool aShowIcon);
} // namespace bpr::qt

#endif
