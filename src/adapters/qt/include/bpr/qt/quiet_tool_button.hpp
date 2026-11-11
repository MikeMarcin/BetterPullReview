#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_TOOL_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_TOOL_BUTTON_HPP

#include <QColor>
#include <QPalette>
#include <QToolButton>

namespace bpr::qt {
	/// \brief Text choices, secondary actions and icon tools using the Overview's quiet chip.
	/// \details A checkable choice is raised only when selected: its face is a
	/// shade toward the ink, with a border at 3:1 against the ground, so on
	/// and off differ by at least the indicator floor. An ordinary action is
	/// raised at rest unless autoRaise is set. Qt still owns signals,
	/// exclusivity, accessibility (a checkable button reports its state) and
	/// input. An icon tool (an icon or an arrow and no words shown) draws the
	/// same face around its mark and keeps the style's size. A menu the
	/// button opens is shown by a small chevron: at the trailing edge of a
	/// chip, at the lower right of an icon tool. A button with words beside
	/// an arrow draws the arrow as a chevron in its ink. Split menu buttons
	/// (`MenuButtonPopup`) and words under an icon retain native painting.
	///
	/// The macOS style gives a plain auto-raise `QToolButton` outside a toolbar
	/// a native bezel in the system appearance's grey, whatever the profile's
	/// palette, so a dark profile on a light system put light ink on a pale
	/// face. Every face here is mixed from the palette, or from the colours
	/// the surface gives, instead.
	class quiet_tool_button : public QToolButton
	{
	public:
		explicit quiet_tool_button(QWidget* aParent = nullptr);
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

		/// \brief The palette role of the ground the button sits on: `Window`
		/// (the default) for a pane, `Base` for a card or field. Its faces, ink
		/// and focus ring are derived from that ground.
		void set_surface(QPalette::ColorRole aSurface);

		/// \brief The ground the button sits on and the ink it is read in, for
		/// a surface painted in colours no palette role holds (a code block's
		/// header in the code theme). Invalid colours return to the role.
		void set_surface_colours(const QColor& aGround, const QColor& aInk);

		/// \brief Draws the icon as a mask in the surface's ink. For one-colour
		/// marks, so they follow the ground's ink in every theme and state
		/// whatever palette the icon was made from.
		void set_tinted(bool aTinted);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		[[nodiscard]] auto  uses_chip() const -> bool;
		[[nodiscard]] auto  uses_glyph() const -> bool;
		[[nodiscard]] auto  shows_icon() const -> bool;
		[[nodiscard]] auto  shows_arrow() const -> bool;
		[[nodiscard]] auto  shows_menu() const -> bool;
		QPalette::ColorRole Surface = QPalette::Window;
		QColor              Ground;
		QColor              Ink;
		bool                Tinted        = false;
		bool                KeyboardFocus = false;
	};
} // namespace bpr::qt

#endif
