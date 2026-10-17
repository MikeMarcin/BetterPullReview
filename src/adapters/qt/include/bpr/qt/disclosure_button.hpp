#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DISCLOSURE_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DISCLOSURE_BUTTON_HPP

#include <QColor>
#include <QPalette>
#include <QSize>
#include <QToolButton>

class QPaintEvent;

namespace bpr::qt {
	/// \brief The fill a disclosure bar is painted on: the palette's raised
	/// surface (`Light`) when its text reads on it, otherwise the plain surface.
	///
	/// \details [[BPR-1579]]. `Light` is the raised surface that [[BPR-1513]]'s
	/// Appearance banners use: white on `#F6F7F9` in Light, `#2A3039` on
	/// `#1C2026` in Dark. A palette the profile did not derive, such as a
	/// top-level dialog's platform palette, need not pair `Light` with its
	/// `WindowText`; there the bar falls back to `Window`, which it does.
	[[nodiscard]] auto disclosure_fill(const QPalette& aPalette, QPalette::ColorGroup aGroup) -> QColor;

	/// \brief A checkable section header: a chevron and its title on a subtle
	/// raised fill, all three from the palette's surface and text roles.
	///
	/// \details [[BPR-1579]]. Every "details" disclosure was a plain
	/// `QToolButton` with an arrow, so the platform style drew it: on macOS a
	/// native bezel whose light grey follows the system appearance, not the
	/// profile, under the Dark profile's near-white text ("Check details: 3
	/// failed" on the Overview, the owner, 2026-10-17). This button paints
	/// itself instead, with no style sheet and no second palette: the fill is
	/// `disclosure_fill`, the title and chevron are `WindowText`, a hover or
	/// press tints the fill with `Mid`, and focus draws the window's focus
	/// ring. The chevron follows the checked state, and so does `arrowType()`:
	/// right when closed, down when open.
	class disclosure_button final : public QToolButton
	{
	public:
		explicit disclosure_button(QWidget* aParent = nullptr);

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};
} // namespace bpr::qt

#endif
