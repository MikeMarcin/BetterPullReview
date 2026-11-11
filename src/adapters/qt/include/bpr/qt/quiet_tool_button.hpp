#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_TOOL_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_QUIET_TOOL_BUTTON_HPP

#include <QPalette>
#include <QToolButton>

namespace bpr::qt {
	/// \brief Text choices, secondary actions and icon tools using the Overview's quiet chip.
	/// \details A checkable choice is raised only when selected. An ordinary
	/// action is raised at rest unless autoRaise is set. Qt still owns signals,
	/// exclusivity, accessibility and input. An icon tool (an icon and no words)
	/// draws the same face around its icon and keeps the style's size. Menus,
	/// arrows and stacked icon/text tools retain native painting; they are
	/// specialized variants.
	///
	/// The macOS style gives a plain auto-raise `QToolButton` outside a toolbar
	/// a native bezel in the system appearance's grey, whatever the profile's
	/// palette, so a dark profile on a light system put light ink on a pale
	/// face. Every face here is mixed from the palette instead.
	class quiet_tool_button final : public QToolButton
	{
	public:
		explicit quiet_tool_button(QWidget* aParent = nullptr);
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

		/// \brief The palette role of the ground the button sits on: `Window`
		/// (the default) for a pane, `Base` for a card or field. Its faces, ink
		/// and focus ring are derived from that ground.
		void set_surface(QPalette::ColorRole aSurface);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		[[nodiscard]] auto  uses_chip() const -> bool;
		[[nodiscard]] auto  uses_glyph() const -> bool;
		[[nodiscard]] auto  shows_icon() const -> bool;
		QPalette::ColorRole Surface       = QPalette::Window;
		bool                KeyboardFocus = false;
	};
} // namespace bpr::qt

#endif
