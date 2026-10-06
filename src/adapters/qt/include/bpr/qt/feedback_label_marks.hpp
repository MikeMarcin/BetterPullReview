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
#include <cstddef>
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
	/// order. An empty fragment, a colon alone, offers all twelve in catalog
	/// order (owner, `shortcodes-v002`: "just typing : should be enough to
	/// open suggestions").
	[[nodiscard]] auto feedback_shortcode_matches(std::string_view aFragment) -> std::vector<core::feedback_label>;

	/// \brief The appearance role that tints a label's mark.
	[[nodiscard]] auto feedback_label_role(core::feedback_label aLabel) noexcept -> core::appearance_role;
	/// \brief The twelve tints, in catalog order.
	using feedback_label_tints = std::array<QColor, core::feedback_label_count>;
	/// \brief The tints of one palette.
	[[nodiscard]] auto feedback_label_tints_of(const core::appearance_palette& aPalette) -> feedback_label_tints;

	/// \brief What a mark carries in its middle (owner, 2026-10-05: "i think
	/// settings should also have a toggle to switch between the icons and the
	/// shortcode icons, because i liked both of them.").
	enum class feedback_mark_style : std::uint8_t {
		/// BPR-1080's twelve accepted glyphs. The default.
		icons,
		/// The label's two-letter shortcode (`pr`, `ni`, ... `qb`), in the same
		/// mark: the same tints, blocking fill, size and names.
		letters,
	};

	/// \brief Everything a surface needs to draw marks for the theme on
	/// screen: the twelve tints and the mark style.
	struct feedback_mark_theme
	{
		feedback_label_tints Tints;
		feedback_mark_style  Style = feedback_mark_style::icons;
	};
	/// \brief The built-in tints of the variant a palette's base colour implies,
	/// in icons: what a surface draws before the composition supplies a theme.
	[[nodiscard]] auto default_feedback_mark_theme(const QPalette& aPalette) -> feedback_mark_theme;

	/// \brief The standard decoration a mark and its rail show.
	/// \details Blocking is the owner's (BPR-1079): a filled mark and a 4 px
	/// rail. Non-blocking and if-minor are BPR-1083 v003's, after the owner's
	/// "I notice we have style for blocking but noting for non-blocking or
	/// if-minor": quieter than blocking and distinct from each other and from
	/// an undecorated label, by shape as well as colour.
	enum class feedback_mark_emphasis : std::uint8_t {
		/// No standard decoration, a conflicting set, or only custom ones: the
		/// soft mark and a 2 px solid rail.
		undecorated,
		/// `(blocking)`: the mark filled with the full tint and a 4 px rail.
		blocking,
		/// `(non-blocking)`: an open mark (no fill) with a dashed border, and a
		/// 2 px dashed rail.
		non_blocking,
		/// `(if-minor)`: the soft mark with its top-left corner squared off and
		/// filled in the tint, and a 2 px dotted rail.
		if_minor,
	};
	/// \brief The emphasis an annotation's expectation shows. Only an explicit
	/// standard decoration counts; `conflicting` and `unspecified` are
	/// undecorated.
	[[nodiscard]] auto feedback_mark_emphasis_of(core::feedback_expectation aExpectation) noexcept -> feedback_mark_emphasis;
	/// \brief A rail's width: 4 px when blocking, 2 px otherwise.
	[[nodiscard]] auto feedback_rail_width(feedback_mark_emphasis aEmphasis) noexcept -> qreal;
	/// \brief The side of the if-minor corner, along each edge from the mark's
	/// top-left corner. It clears every glyph and both letters.
	inline constexpr qreal feedback_mark_corner_tab = 6.0;
	/// \brief The dashes of a non-blocking mark's border, in pixels.
	inline constexpr qreal feedback_mark_dash     = 2.5;
	inline constexpr qreal feedback_mark_dash_gap = 1.5;
	/// \brief The dashes of a non-blocking rail and the dots of an if-minor
	/// rail, in pixels: a run and the gap after it.
	inline constexpr qreal feedback_rail_dash     = 6.0;
	inline constexpr qreal feedback_rail_dash_gap = 3.0;
	inline constexpr qreal feedback_rail_dot      = 2.0;
	inline constexpr qreal feedback_rail_dot_gap  = 2.0;

	/// \brief The colours one mark is painted with.
	struct feedback_mark_colours
	{
		QColor Tint;
		/// The surface under the mark, which the soft fill mixes with.
		QColor Panel;
		/// The glyph's ink on a soft mark.
		QColor Ink;
		/// Icons or letters.
		feedback_mark_style Style = feedback_mark_style::icons;
	};
	/// \brief The colours for one label on a widget's palette.
	[[nodiscard]] auto feedback_mark_colours_for(const QColor& aTint, const QPalette& aPalette, feedback_mark_style aStyle = feedback_mark_style::icons) -> feedback_mark_colours;
	/// \brief The colours for one label of a theme on a widget's palette.
	[[nodiscard]] auto feedback_mark_colours_for(const feedback_mark_theme& aTheme, core::feedback_label aLabel, const QPalette& aPalette) -> feedback_mark_colours;
	/// \brief The soft fill: the tint at 22% over a light panel, 30% over a dark one.
	[[nodiscard]] auto feedback_mark_soft_fill(const QColor& aTint, const QColor& aPanel) -> QColor;
	/// \brief The blocking glyph's ink: white or `#111418`, whichever contrasts
	/// more with the tint.
	[[nodiscard]] auto feedback_mark_blocking_ink(const QColor& aTint) -> QColor;

	/// \brief Draws one glyph into a box in the painter's current ink.
	void paint_feedback_glyph(QPainter& aPainter, const QRectF& aBox, core::feedback_label aLabel, const QColor& aInk);
	/// \brief Draws a label's two-letter code into a box in one ink.
	void paint_feedback_letters(QPainter& aPainter, const QRectF& aBox, core::feedback_label aLabel, const QColor& aInk);
	/// \brief Draws one 16 px mark in its emphasis (soft, filled, open and
	/// dashed, or soft with a corner), with the glyph or the letters
	/// `aColours.Style` names.
	void paint_feedback_mark(QPainter& aPainter, const QRectF& aMark, core::feedback_label aLabel, feedback_mark_emphasis aEmphasis, const feedback_mark_colours& aColours);
	/// \brief One mark as a pixmap for a control's icon, at a device pixel ratio.
	[[nodiscard]] auto feedback_mark_pixmap(core::feedback_label aLabel, feedback_mark_emphasis aEmphasis, const feedback_mark_colours& aColours, qreal aDevicePixelRatio) -> QPixmap;
	/// \brief The Plain mark: an empty square in the panel's mid tone.
	[[nodiscard]] auto feedback_plain_mark_pixmap(const QPalette& aPalette, qreal aDevicePixelRatio) -> QPixmap;
	/// \brief A thread rail in the tint: 2 px solid, 4 px when blocking, 2 px
	/// dashed when non-blocking or dotted when if-minor, drawn so that its
	/// right edge stays at `aRight` whatever its width.
	void paint_feedback_rail(QPainter& aPainter, qreal aRight, qreal aTop, qreal aBottom, const QColor& aColour, feedback_mark_emphasis aEmphasis);

	/// \brief What one body shows as a mark.
	/// \details Only a `recognized` prefix gets a mark (contract, derived):
	/// `plain`, `unknown_label`, `malformed` and `limited` bodies get none and
	/// read exactly as written. `Emphasis` is the author's explicit standard
	/// expectation and nothing else: built-in guidance never decorates a mark.
	struct feedback_body_mark
	{
		std::optional<core::feedback_label> Label;
		/// The author's explicit standard decoration, as the mark shows it.
		feedback_mark_emphasis Emphasis = feedback_mark_emphasis::undecorated;
		/// "issue (blocking)": the label and its standard decorations in
		/// written order. Empty with no mark.
		QString Name;
		/// Where the subject begins, as a `QString` index into the body: the
		/// body a reader shows after the mark that stands in for the prefix.
		qsizetype SubjectOffset = 0;
		/// The same place as a UTF-8 byte offset into the body.
		std::size_t SubjectByte = 0U;
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
