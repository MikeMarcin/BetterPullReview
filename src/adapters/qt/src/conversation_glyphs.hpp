#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_GLYPHS_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_GLYPHS_HPP

#include <QColor>
#include <QIcon>
#include <QRectF>

#include <cstdint>

class QPainter;
class QPalette;

namespace bpr::qt {
	/// \brief [[BPR-1290]]. The inline drawer's icons, drawn on a 24-unit grid
	/// from the owner's accepted prototype (`docs/design/BPR-1290-inline-drawer-v1`).
	enum class conversation_glyph : std::uint8_t {
		/// Show line: a target.
		target,
		resolve,
		unresolve,
		/// Mark read: two ticks.
		read,
		more,
		/// Close an open thread: a chevron pointing up.
		collapse,
		reply,
		/// Publish: a paper plane.
		send,
		lock,
		edit,
		trash,
		/// Only new: a filled dot in a ring.
		only_new,
		/// Hide resolved: a struck ring.
		hide_resolved,
		/// Next new reply: a dot above a downward arrow.
		next_new,
		up,
		down,
		resolved,
	};

	/// \brief Strokes one glyph into \p aBox in \p aInk.
	void paint_conversation_glyph(QPainter& aPainter, const QRectF& aBox, conversation_glyph aGlyph, const QColor& aInk);

	/// \brief One glyph as an icon in the palette's button ink, with its
	/// disabled state, at 1x, 1.25x and 2x.
	[[nodiscard]] auto conversation_icon(conversation_glyph aGlyph, const QPalette& aPalette) -> QIcon;
	/// \brief One glyph as an icon in a fixed ink.
	[[nodiscard]] auto conversation_icon(conversation_glyph aGlyph, const QColor& aInk) -> QIcon;
} // namespace bpr::qt

#endif
