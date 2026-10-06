#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ANCHORED_TOOLTIP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ANCHORED_TOOLTIP_HPP

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

#include <functional>
#include <optional>

class QWidget;

/// \file
/// \brief [[BPR-1083]] v004. A tooltip that sits right next to what it names.
/// \details `QToolTip` places its window at the pointer plus the platform
/// cursor's height, which on macOS put a label mark's name well below and to
/// the right of a 16 px mark (owner, rejecting `preview-tooltips-v003`: "The
/// tooltip feels strangely far away."). These tooltips are anchored to a
/// rectangle instead: directly below it with their left edges aligned, or
/// directly above it when below would leave the screen. The window looks like
/// the style's own tooltip (its panel, palette and font) and goes away when
/// the pointer leaves the anchor, on a press, a key or the wheel, when the
/// owner hides, or after ten seconds. Presentation only.
namespace bpr::qt {
	/// \brief The gap between an anchor and its tooltip, in pixels.
	inline constexpr int anchored_tooltip_gap = 4;

	/// \brief Where a tooltip of `aTip`'s size goes for an anchor, both in
	/// global coordinates, on a screen's available area: below the anchor,
	/// left edges aligned and `anchored_tooltip_gap` apart, or above it when
	/// below would leave the screen; moved left only to stay on the screen.
	[[nodiscard]] auto anchored_tooltip_geometry(const QRect& aAnchor, const QSize& aTip, const QRect& aScreen) -> QRect;

	/// \brief Shows `aText` next to `aAnchor`, a rectangle in `aOwner`'s
	/// coordinates, replacing any anchored tooltip on screen. The same text
	/// for the same anchor leaves the tooltip as it is.
	void show_anchored_tooltip(QWidget& aOwner, const QRect& aAnchor, const QString& aText);
	/// \brief Hides the anchored tooltip, if one is on screen.
	void hide_anchored_tooltip();
	/// \brief The anchored tooltip's window while it is on screen, or null.
	[[nodiscard]] auto anchored_tooltip() -> QWidget*;

	/// \brief What one point of a widget names: the rectangle the tooltip is
	/// anchored to, in the widget's coordinates, and its text.
	struct anchored_tooltip_target
	{
		QRect   Anchor;
		QString Text;
	};
	/// \brief The target under a point of a widget, or none.
	using anchored_tooltip_source = std::function<std::optional<anchored_tooltip_target>(QPoint aPosition)>;
	/// \brief Answers `aWidget`'s tooltip events with an anchored tooltip:
	/// what `aSource` names at the point, or, with no source, the widget's
	/// own `toolTip()` anchored to the whole widget. A point that names
	/// nothing shows nothing.
	void anchor_tooltips(QWidget& aWidget, anchored_tooltip_source aSource = { });
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ANCHORED_TOOLTIP_HPP
