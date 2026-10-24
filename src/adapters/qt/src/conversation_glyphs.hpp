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
		/// View a post's source: angle brackets around a slash.
		source,
		/// Back to the rendered post: three lines of text, the first a heading.
		rendered,
		/// Copy a post's source: two sheets, one behind the other.
		copy,
		/// [[BPR-1293]]. Find a thread: a small thread of posts with a
		/// lens beside its last (prototype v2's finder button).
		finder,
		/// The finder's search field: a lens.
		search,
		/// The finder's label filter: a tag.
		tag,
		/// [[BPR-1212]]. Open a draft in Discuss: two arrows out to the corners.
		maximize,
		/// [[BPR-1212]]. Back to the code: two arrows in from the corners.
		minimize,
		/// [[BPR-1212]]. Suggest change: a pencil over a line.
		suggest,
		/// [[BPR-1212]]. Add comment: a speech balloon.
		comment,
		/// [[BPR-1337]]. Clear filters: a funnel with a cross beside its stem.
		clear_filters,
		/// [[BPR-1337]]. Dismiss what a pill names: a cross.
		dismiss,
		/// [[BPR-1741]]. Add reaction: a face with a plus.
		react,
	};

	/// \brief Strokes one glyph into \p aBox in \p aInk.
	void paint_conversation_glyph(QPainter& aPainter, const QRectF& aBox, conversation_glyph aGlyph, const QColor& aInk);

	/// \brief One glyph as an icon in the palette's button ink, with its
	/// disabled state, painted at the size and scale a button asks for.
	[[nodiscard]] auto conversation_icon(conversation_glyph aGlyph, const QPalette& aPalette) -> QIcon;
	/// \brief One glyph as an icon in a fixed ink.
	[[nodiscard]] auto conversation_icon(conversation_glyph aGlyph, const QColor& aInk) -> QIcon;
} // namespace bpr::qt

#endif
