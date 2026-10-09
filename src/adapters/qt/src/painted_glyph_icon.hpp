#ifndef BPR_SRC_ADAPTERS_QT_SRC_PAINTED_GLYPH_ICON_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_PAINTED_GLYPH_ICON_HPP

#include <QColor>
#include <QIcon>

#include <functional>

class QPainter;

/// \file
/// \brief [[BPR-1296]]. A line-glyph icon painted when a view asks for it.
/// \details The strip, conversation and Share notes glyphs were icons holding
/// every mode, size and scale rendered in advance: 36 pixmaps for a strip or
/// share glyph, built again on every palette change and almost all never
/// drawn. They held 13.3 MiB of live heap once the window was constructed
/// (58.9 against 45.6 MiB, macOS Release), which kept the large-review byte
/// course at its memory limit. This icon paints only the size, scale and
/// mode a view requests.
namespace bpr::qt {
	/// \brief Strokes a glyph `aSide` logical pixels square at the painter's
	/// origin, in `aInk`. The painter is antialiased.
	using glyph_painter = std::function<void(QPainter& aPainter, qreal aSide, const QColor& aInk)>;

	/// \brief An icon that paints `aPaint` on request: in `aDisabled` for the
	/// disabled mode and in `aInk` for every other mode and state.
	/// \details A request for a size that is not square gets the largest
	/// square inside it, as a pixmap icon of square pixmaps did.
	[[nodiscard]] auto painted_glyph_icon(glyph_painter aPaint, const QColor& aInk, const QColor& aDisabled) -> QIcon;

	/// \brief Paints a glyph `aSide` logical pixels square at the painter's
	/// origin for one icon mode. The painter is antialiased.
	using mode_glyph_painter = std::function<void(QPainter& aPainter, qreal aSide, QIcon::Mode aMode)>;

	/// \brief [[BPR-1331]] v003. An icon that paints `aPaint` on request with
	/// the mode the view asks for, for a glyph of several colours: the private
	/// progress marks, whose menu rows ring the mark when highlighted.
	[[nodiscard]] auto painted_mode_icon(mode_glyph_painter aPaint) -> QIcon;
} // namespace bpr::qt

#endif
