#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_MARKS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_MARKS_HPP

#include "bpr/core/appearance_model.hpp"
#include "bpr/core/feedback_convention.hpp"

#include <QColor>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class QPainter;
class QPalette;

/// \file
/// \brief [[BPR-1083]]. The one resource vocabulary of the twelve Conventional
/// Comments label marks (RFC-BPR-0050 R31; the delivery contract,
/// `docs/ux/conventional-comments-feedback.md`).
/// \details Presentation only. The glyphs are the owner-accepted path data of
/// `docs/design/BPR-1080-label-icons-selected.html` (`759ecc692`), compiled
/// here as `QPainterPath` construction on BPR-1080's 24-unit grid: no SVG
/// file, Qt SVG module, icon font, resource file or generated image. Every
/// surface draws a mark through this unit, takes its tint from the appearance
/// palette's `feedback_label_<id>` role, and names it with the words here, so
/// a label has one shape, one tint role and one name everywhere. Nothing here
/// reads or changes a body: marks follow the core's annotation of it.
namespace bpr::qt {
	/// \brief The three element kinds of a glyph (BPR-1080 drawing rules).
	enum class feedback_glyph_element_kind : std::uint8_t {
		/// The path is stroked with round caps and joins at `PenWidth`.
		stroke,
		/// The path is filled with no pen.
		fill,
		/// A filled circle at `Centre` with `Radius`.
		disc,
	};

	/// \brief One element of one glyph, in grid units.
	struct feedback_glyph_element
	{
		feedback_glyph_element_kind Kind = feedback_glyph_element_kind::stroke;
		/// Stroke width in grid units; zero for a fill or a disc.
		qreal        PenWidth = 0.0;
		QPainterPath Path;
		QPointF      Centre;
		qreal        Radius = 0.0;
	};

	/// \brief The grid the glyphs are drawn on.
	inline constexpr qreal feedback_glyph_grid = 24.0;
	/// \brief The default stroke width, in grid units.
	inline constexpr qreal feedback_glyph_pen = 2.6;
	/// \brief A mark's side, in device-independent pixels.
	inline constexpr qreal feedback_mark_size = 16.0;
	/// \brief The glyph's box, centred in the mark.
	inline constexpr qreal feedback_mark_glyph_size = 12.0;
	/// \brief The tinted border of a soft mark.
	inline constexpr qreal feedback_mark_border = 1.5;
	/// \brief The mark's corner radius. Labels are square; states are round.
	inline constexpr qreal feedback_mark_corner = 4.0;

	/// \brief The compiled glyph of one label, in its accepted element order.
	[[nodiscard]] auto feedback_label_glyph(core::feedback_label aLabel) -> std::span<const feedback_glyph_element>;

	/// \brief The label's two-letter shortcode, as in `qu` for `:qu:`
	/// (owner, BPR-1079 v2).
	[[nodiscard]] auto feedback_shortcode(core::feedback_label aLabel) noexcept -> std::string_view;
	/// \brief The label a two-letter code names, exactly and in lowercase.
	[[nodiscard]] auto find_feedback_shortcode(std::string_view aCode) noexcept -> std::optional<core::feedback_label>;
	/// \brief Completion for a typed code fragment: codes that start with it
	/// first, then labels whose name starts with it, each once, in catalog
	/// order. An empty fragment offers nothing.
	[[nodiscard]] auto feedback_shortcode_matches(std::string_view aFragment) -> std::vector<core::feedback_label>;

	/// \brief The appearance role that tints a label's mark.
	[[nodiscard]] auto feedback_label_role(core::feedback_label aLabel) noexcept -> core::appearance_role;
	/// \brief The twelve tints, in catalog order.
	using feedback_label_tints = std::array<QColor, core::feedback_label_count>;
	/// \brief The tints of one palette.
	[[nodiscard]] auto feedback_label_tints_of(const core::appearance_palette& aPalette) -> feedback_label_tints;

	/// \brief The colours one mark is painted with.
	struct feedback_mark_colours
	{
		QColor Tint;
		/// The surface under the mark, which the soft fill mixes with.
		QColor Panel;
		/// The glyph's ink on a soft mark.
		QColor Ink;
	};
	/// \brief The colours for one label on a widget's palette.
	[[nodiscard]] auto feedback_mark_colours_for(const QColor& aTint, const QPalette& aPalette) -> feedback_mark_colours;
	/// \brief The soft fill: the tint at 22% over a light panel, 30% over a dark one.
	[[nodiscard]] auto feedback_mark_soft_fill(const QColor& aTint, const QColor& aPanel) -> QColor;
	/// \brief The blocking glyph's ink: white or `#111418`, whichever contrasts
	/// more with the tint.
	[[nodiscard]] auto feedback_mark_blocking_ink(const QColor& aTint) -> QColor;

	/// \brief Draws one glyph into a box in the painter's current ink.
	void paint_feedback_glyph(QPainter& aPainter, const QRectF& aBox, core::feedback_label aLabel, const QColor& aInk);
	/// \brief Draws one 16 px mark: soft, or filled when blocking.
	void paint_feedback_mark(QPainter& aPainter, const QRectF& aMark, core::feedback_label aLabel, bool aBlocking, const feedback_mark_colours& aColours);
	/// \brief One mark as a pixmap for a control's icon, at a device pixel ratio.
	[[nodiscard]] auto feedback_mark_pixmap(core::feedback_label aLabel, bool aBlocking, const feedback_mark_colours& aColours, qreal aDevicePixelRatio) -> QPixmap;
	/// \brief The Plain mark: an empty square in the panel's mid tone.
	[[nodiscard]] auto feedback_plain_mark_pixmap(const QPalette& aPalette, qreal aDevicePixelRatio) -> QPixmap;
	/// \brief A thread rail: 2 px in the tint, or 4 px when blocking, drawn so
	/// that its right edge stays at `aRight` whatever its width.
	void paint_feedback_rail(QPainter& aPainter, qreal aRight, qreal aTop, qreal aBottom, const QColor& aColour, bool aBlocking);

	/// \brief What one body shows as a mark.
	/// \details Only a `recognized` prefix gets a mark (contract, derived):
	/// `plain`, `unknown_label`, `malformed` and `limited` bodies get none and
	/// read exactly as written. `Blocking` is the author's explicit
	/// `blocking` expectation and nothing else.
	struct feedback_body_mark
	{
		std::optional<core::feedback_label> Label;
		bool                                Blocking = false;
		/// "issue (blocking)": the label and its standard decorations in
		/// written order. Empty with no mark.
		QString Name;
		/// Where the subject begins, as a `QString` index into the body: the
		/// body a reader shows after the mark that stands in for the prefix.
		qsizetype SubjectOffset = 0;
	};
	/// \brief The mark an ordinary body carries.
	[[nodiscard]] auto feedback_body_mark_of(std::string_view aBody) -> feedback_body_mark;
	/// \brief The name of a recognized annotation's mark.
	[[nodiscard]] auto feedback_mark_name(const core::feedback_annotation& aAnnotation) -> QString;
	/// \brief A post's accessible name: its author first, as in
	/// "tbutler, issue (blocking)", or the author alone with no mark.
	[[nodiscard]] auto feedback_post_name(const QString& aAuthor, const feedback_body_mark& aMark) -> QString;
	/// \brief A label's catalog description, for a tooltip.
	[[nodiscard]] auto feedback_label_description(core::feedback_label aLabel) -> QString;
	/// \brief A label's catalog name.
	[[nodiscard]] auto feedback_label_text(core::feedback_label aLabel) -> QString;
	/// \brief A UTF-8 byte offset of `aBody` as a `QString` index. The offset
	/// must lie on a code point boundary.
	[[nodiscard]] auto feedback_text_index(std::string_view aBody, std::size_t aByteOffset) -> qsizetype;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_MARKS_HPP
