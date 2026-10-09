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
/// \brief [[BPR-1083]] v004, [[BPR-1354]]. Every tooltip sits right next to
/// what it names.
/// \details `QToolTip` places its window at the pointer plus the platform
/// cursor's height, which on macOS put a label mark's name well below and to
/// the right of a 16 px mark (owner, rejecting `preview-tooltips-v003`: "The
/// tooltip feels strangely far away."), a strip button's below the button's
/// neighbour ([[BPR-1328]]: "Whys this tooltip so far from the button?") and
/// the diff's "+" three lines down ([[BPR-1354]]: "bug: Why is this damned
/// tooltip far away again. This is a consistent problem."). These tooltips are
/// anchored to a rectangle instead: directly below it with their left edges
/// aligned, or directly above it when below would leave the screen. A line
/// longer than about eighty characters wraps. The window
/// looks like the style's own tooltip (its panel, palette and font) and goes
/// away when the pointer leaves the anchor, on a press, a key other than a
/// modifier (as `QToolTip`'s does) or the wheel, when the owner hides, or
/// after ten seconds.
///
/// Since [[BPR-1354]] this is the application's only tooltip placement. Once
/// the application object exists, every tooltip event of every widget is
/// answered here, whether or not the widget opted in: a widget's own
/// `toolTip()` anchored to the widget, an item view's item (`Qt::ToolTipRole`)
/// to the item, a tab bar's tab to the tab, a header's section to the section
/// and a menu's action (in a menu that shows them) to the action's row. A
/// widget that paints several controls names each with `anchor_tooltips`.
/// Presentation only.
namespace bpr::qt {
	/// \brief The gap between an anchor and its tooltip, in pixels.
	inline constexpr int anchored_tooltip_gap = 4;

	/// \brief Where a tooltip of `aTip`'s size goes for an anchor, both in
	/// global coordinates, on a screen's available area: below the anchor,
	/// left edges aligned and `anchored_tooltip_gap` apart, or above it when
	/// below would leave the screen; moved left only to stay on the screen.
	[[nodiscard]] auto anchored_tooltip_geometry(const QRect& aAnchor, const QSize& aTip, const QRect& aScreen) -> QRect;

	/// \brief What keeps an anchored tooltip on screen.
	enum class anchored_tooltip_hold {
		/// \brief The pointer over the anchor: a tooltip the pointer asked for.
		pointer,
		/// \brief The owner's keyboard focus, wherever the pointer is: a
		/// tooltip a keyboard reviewer asked for by focusing the control
		/// ([[BPR-0790]]). It goes when focus moves, on a press, a key or the
		/// wheel, when the owner hides, or after ten seconds.
		focus,
	};

	/// \brief Shows `aText` next to `aAnchor`, a rectangle in `aOwner`'s
	/// coordinates, replacing any anchored tooltip on screen. The same text
	/// for the same anchor leaves the tooltip as it is.
	void show_anchored_tooltip(QWidget& aOwner, const QRect& aAnchor, const QString& aText, anchored_tooltip_hold aHold = anchored_tooltip_hold::pointer);
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
	/// nothing shows nothing here and is handed to the parent, as Qt hands
	/// an unanswered tooltip event up.
	void anchor_tooltips(QWidget& aWidget, anchored_tooltip_source aSource = { });

	/// \brief [[BPR-1354]]. What the application-wide placement anchors at a
	/// point of `aWidget`, or none: a menu's action, a tab, a header section,
	/// an item of an item view (`aWidget` being its viewport), or else the
	/// widget's own `toolTip()` anchored to the whole widget.
	[[nodiscard]] auto standard_tooltip_target(const QWidget& aWidget, QPoint aPosition) -> std::optional<anchored_tooltip_target>;
	/// \brief [[BPR-1354]]. Answers every widget's tooltip events in the
	/// application with an anchored tooltip, through
	/// `standard_tooltip_target`, unless the widget names its own with
	/// `anchor_tooltips`. Runs by itself when the application object is
	/// made; calling it again does nothing.
	void install_anchored_tooltips();
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ANCHORED_TOOLTIP_HPP
