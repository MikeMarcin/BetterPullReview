#ifndef BPR_SRC_ADAPTERS_QT_SRC_BUTTON_SURFACE_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_BUTTON_SURFACE_HPP

#include <QColor>
#include <QPalette>
#include <QSize>

#include <cstdint>

class QAbstractButton;
class QFocusEvent;
class QPainter;
class QPixmap;
class QRectF;

namespace bpr::qt {
	/// \brief Keeps keyboard focus visible without making a mouse click look selected.
	void update_button_keyboard_focus(bool& aKeyboardFocus, const QFocusEvent& aEvent);

	/// \brief The colours a quiet button is drawn in: the ground it sits on,
	/// the ink its words and marks take there (enabled and disabled), and its
	/// keyboard focus ring.
	struct button_colours
	{
		QColor Ground;
		QColor Ink;
		QColor DisabledInk;
		QColor Focus;
	};

	/// \brief The colours on the `aSurface` role of `aPalette`, with the ink
	/// `button_ink_role` reads there.
	[[nodiscard]] auto button_colours_on(const QPalette& aPalette, QPalette::ColorRole aSurface) -> button_colours;

	/// \brief The colours on a ground no palette role holds (a code block's
	/// header in the code theme's colours): `aInk` on `aGround`, the disabled
	/// ink halfway between, and the focus ring held to `aGround`.
	[[nodiscard]] auto button_colours_on(const QPalette& aPalette, const QColor& aGround, const QColor& aInk) -> button_colours;

	/// \brief Draws the accepted Overview chip, hover/press and keyboard focus
	/// surface in `aColours`. The faces are mixed from its ground toward its
	/// ink; a raised (selected) face has a border at 3:1 against the ground.
	void paint_button_surface(QPainter& aPainter, const QAbstractButton& aButton, const button_colours& aColours, bool aRaised, bool aKeyboardFocus);

	/// \brief Draws the accepted Overview chip, hover/press and keyboard focus surface.
	/// \details Content and interaction remain with the button. All colours are read
	/// from its current palette; no native bezel or cached theme colour is involved.
	/// `aSurface` is the palette role of the ground the button sits on (`Window`
	/// for a pane, `Base` for a card): the faces are mixed from it toward its
	/// ink, and the focus ring is held to it.
	void paint_button_surface(QPainter& aPainter, const QAbstractButton& aButton, bool aRaised, bool aKeyboardFocus, QPalette::ColorRole aSurface = QPalette::Window);

	/// \brief The ink role read on `aSurface`: `Text` on `Base`, `WindowText` elsewhere.
	[[nodiscard]] auto button_ink_role(QPalette::ColorRole aSurface) -> QPalette::ColorRole;

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
	/// clipped at both ends. The style still draws the words and mnemonic, in
	/// the ink of `aSurface`.
	void paint_button_label(QPainter& aPainter, const QAbstractButton& aButton, QSize aIconSize, bool aShowIcon, QPalette::ColorRole aSurface = QPalette::Window);

	/// \brief What a chip shows beside its words.
	enum class button_mark : std::uint8_t {
		none,
		/// The button's icon as drawn.
		icon,
		/// The button's icon as a mask in the ink.
		tinted_icon,
		/// A chevron in the ink pointing the button's arrow.
		arrow,
	};

	/// \brief As above, in `aColours`, with `aMark` beside the words, keeping
	/// `aTrailing` pixels free at the trailing edge (for a menu indicator).
	void paint_button_label(QPainter& aPainter, const QAbstractButton& aButton, const button_colours& aColours, QSize aIconSize, button_mark aMark,
							Qt::ArrowType aArrow = Qt::NoArrow, int aTrailing = 0);

	/// \brief The room a chip keeps for its menu indicator, gap included.
	inline constexpr int chip_menu_indicator = 12;

	/// \brief A small downward chevron in `aInk`: the menu a button opens.
	void paint_button_menu_indicator(QPainter& aPainter, const QRectF& aPlace, const QColor& aInk);

	/// \brief Draws an icon-only button's icon, centred, in its enabled or
	/// disabled mode and its checked state.
	void paint_button_glyph(QPainter& aPainter, const QAbstractButton& aButton, QSize aIconSize);

	/// \brief Draws an icon-only button's icon, centred, as a mask in `aTint`.
	void paint_button_glyph(QPainter& aPainter, const QAbstractButton& aButton, QSize aIconSize, const QColor& aTint);

	/// \brief `aImage`'s shape filled with `aInk`: a one-colour mark redrawn
	/// in the ink of the ground it is shown on.
	[[nodiscard]] auto tinted(const QPixmap& aImage, const QColor& aInk) -> QPixmap;

	/// \brief A chevron in `aInk` pointing `aArrow`, centred in `aPlace`.
	void paint_button_arrow(QPainter& aPainter, const QRectF& aPlace, Qt::ArrowType aArrow, const QColor& aInk);
} // namespace bpr::qt

#endif
