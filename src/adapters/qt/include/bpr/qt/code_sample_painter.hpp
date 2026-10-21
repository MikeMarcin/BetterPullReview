#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CODE_SAMPLE_PAINTER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CODE_SAMPLE_PAINTER_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/white_space.hpp"

#include <QByteArray>
#include <QCache>
#include <QColor>
#include <QFont>
#include <QList>
#include <QRectF>
#include <QStaticText>
#include <QString>
#include <QStringView>
#include <QTextEdit>
#include <QTextLayout>
#include <QTransform>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

class QPainter;
class QPaintDevice;
class QPlainTextEdit;
class QTextBlock;
class QTextDocument;

namespace bpr::qt {
	/// Keeps only a viewport's worth of number/header layouts. Qt supplies the
	/// exact right/vertical-center placement and all glyphs; colours stay live.
	class code_gutter_text_cache final
	{
	public:
		void retain_visible_rows(int aRows);
		void draw(QPainter& aPainter, const QRect& aRect, const QString& aText);

	private:
		struct label
		{
			QStaticText Text;
			QPointF     Offset;
			QSize       Cell;
		};
		QCache<QString, label> Labels;
		QFont                  Font;
		QTransform             Transform;
		const QPaintDevice*    Device           = nullptr;
		qreal                  DevicePixelRatio = 0.0;
		int                    LogicalDpiX      = 0;
		int                    LogicalDpiY      = 0;
	};

	/// \brief One colour per core role, indexed by the role's own value.
	///
	/// \details A token span names a role and never a colour, so this table is
	/// the only place a role becomes visible. It is the exact shape the review
	/// panes already painted from; naming it here is what lets a second surface
	/// consume the same selection rather than making a second one.
	using code_role_colors = std::array<QColor, core::appearance_role_count>;

	/// \brief Builds the role colour table of one composed semantic palette.
	///
	/// \details The whole palette rather than a chosen subset, because a painter
	/// that asked for the roles it expected would silently ignore a role the core
	/// later added. This is the loop `diff_workbench::adopt_appearance` used to
	/// carry inline, and it is equal to asking `appearance_profile::color` for
	/// every role of the palette that profile currently applies.
	[[nodiscard]] auto code_role_colors_of(const core::appearance_palette& aPalette)
		-> code_role_colors;

	/// \brief Converts a byte offset inside one line to a character index.
	///
	/// \details The core reports spans as byte offsets, because bytes are what it
	/// compared and what the classifier counted. Qt addresses text in UTF-16 code
	/// units, so the prefix is decoded and measured rather than assumed equal:
	/// every non-ASCII character ahead of a span would otherwise shift it left by
	/// however many continuation bytes it carries.
	[[nodiscard]] auto character_offset(
		std::span<const std::uint8_t> aLineBytes,
		std::uint64_t                 aByteOffset) -> int;

	/// \brief Appends one full-width background over a span of lines.
	///
	/// \details An invalid colour or an empty span appends nothing, which is how a
	/// surface says "this row carries no band" without the caller having to ask.
	void append_line_band(
		QList<QTextEdit::ExtraSelection>& aSelections,
		QPlainTextEdit&                   aPane,
		core::line_span                   aSpan,
		const QColor&                     aColor);

	/// \brief Washes the selected lines, bounded to what the pane shows.
	///
	/// \details The wash is the pane's own `QPalette::Highlight` at
	/// `core::selection_wash_alpha`, which is what makes it lie over whatever band
	/// the row already wears instead of replacing it. Reading the pane rather than
	/// the semantic palette is deliberate and is not a gap: `appearance_profile`
	/// derives `Highlight` from `code_selection_background`, so a pane that
	/// adopted the profile washes in exactly that role, and a pane that adopted
	/// nothing washes in the inherited native highlight rather than in a colour
	/// nobody chose.
	///
	/// Clipped to the paintable window rather than to the span, so selecting a
	/// group the size of the file costs the viewport and not the file.
	void append_selection_band(
		QList<QTextEdit::ExtraSelection>& aSelections,
		QPlainTextEdit&                   aPane,
		core::line_span                   aLines,
		core::line_span                   aPaintable);

	/// \brief The two syntax emphasis switches the typography record carries.
	///
	/// \details Both default to off, and off is exactly what the review panes
	/// pass: they colour a token and change no glyph weight. A surface that wants
	/// the reviewer's emphasis choices applied says so.
	struct code_syntax_emphasis
	{
		bool Bold   = false;
		bool Italic = false;

		auto operator<=>(const code_syntax_emphasis&) const = default;
	};

	/// \brief Returns the emphasis one syntax category asks for.
	///
	/// \details Bold and italic are user switches over the whole of syntax
	/// presentation, so a category that asks for either gets it only when the
	/// corresponding switch is on. Keeping the per-role rule here means a painter
	/// states what a token means and never has to know whether emphasis is
	/// wanted.
	[[nodiscard]] auto code_syntax_emphasis_of(
		core::appearance_role aRole,
		code_syntax_emphasis  aSwitches) noexcept -> code_syntax_emphasis;

	/// \brief Describes one line's classified spans and how to colour them.
	///
	/// \details `Spans` are the spans of that one line, in classifier order.
	/// `Colors` is borrowed and must outlive the call. `StrikeOut` marks the whole
	/// line struck, which the one-pane presentation uses for a removed row, and
	/// `StrikeOutLength` is that line's character count.
	struct code_syntax_format_request
	{
		std::span<const std::uint8_t>            LineBytes;
		std::span<const application::token_span> Spans;
		const code_role_colors*                  Colors          = nullptr;
		bool                                     StrikeOut       = false;
		int                                      StrikeOutLength = 0;
		code_syntax_emphasis                     Emphasis;
	};

	/// \brief Turns one line's classified spans into its layout format ranges.
	///
	/// \details A span of `syntax_normal` produces nothing: normal text is what a
	/// pane already draws, and restating it would cost a range per unclassified
	/// word to change nothing. A span whose role carries no valid colour, whose
	/// length is zero, or whose byte range collapses once decoded is skipped for
	/// the same reason.
	[[nodiscard]] auto code_syntax_format_ranges(
		const code_syntax_format_request& aRequest)
		-> QList<QTextLayout::FormatRange>;

	/// \brief Applies format ranges to one block, and only where they differ.
	///
	/// \details The formats go on the block's own layout rather than into the
	/// document: `QTextCursor::setCharFormat` would mutate the document once per
	/// span — an edit, an undo entry and a relayout apiece — to express a change
	/// that alters no text. Re-setting formats marks the block dirty, so an
	/// unconditional write would relayout every visible block on every scroll to
	/// restate colours it already carried.
	///
	/// \returns Whether the block's formats were replaced.
	auto apply_syntax_format_ranges(
		QTextDocument&                         aDocument,
		const QTextBlock&                      aBlock,
		const QList<QTextLayout::FormatRange>& aFormats) -> bool;

	/// \brief The ink an intraline emphasis keeps readable over its fill.
	///
	/// \details `Normal` is the code's own ink, `syntax_normal`; `Variant` is
	/// the palette's, which decides how far an unreadable ink moves.
	struct code_emphasis_ink
	{
		QColor                   Normal;
		core::appearance_variant Variant = core::appearance_variant::light;
	};

	/// \brief Marks characters `aFrom` to `aTo` of `aBlock` as changed.
	///
	/// \details The fill goes down as one background over the exact range. With
	/// an ink, its glyphs keep their colours over that fill: plain text in the
	/// normal ink, and each classified token, read from the block's layout
	/// formats (`apply_syntax_format_ranges`), in its own colour. Either moves
	/// only as far as `core::readable_emphasis_foreground` needs to stay
	/// readable on the fill. Each token selection repeats the fill under its
	/// ink, because Qt does not paint a foreground-only selection over text an
	/// earlier selection drew ([[BPR-1709]]); the fill is the one already
	/// there, so the source and the unchanged part of a token are untouched. Apply
	/// a block's syntax formats before calling this. The comparison panes and
	/// Line details ([[BPR-1334]]) both mark a change through here, so a token
	/// reads in one colour on both.
	void append_intraline_emphasis_span(
		QList<QTextEdit::ExtraSelection>&       aSelections,
		const QTextBlock&                       aBlock,
		int                                     aFrom,
		int                                     aTo,
		const QColor&                           aFill,
		const std::optional<code_emphasis_ink>& aInk);

	/// \brief How a code surface draws a text selection ([[BPR-1533]], [[BPR-1534]]).
	///
	/// \details `Fill` is opaque, from `code_text_selection_fill`; `Normal`
	/// is the ink of text no token colours (`syntax_normal`);
	/// `Variant` decides the text floor each ink is held to. `FullWidth` is
	/// the style's choice (macOS's) that a selection past a line's end runs
	/// to the pane's edge.
	struct code_text_selection
	{
		QColor                   Fill;
		QColor                   Normal;
		core::appearance_variant Variant   = core::appearance_variant::light;
		bool                     FullWidth = false;
	};

	/// \brief The opaque colour a code pane fills a text selection with.
	///
	/// \details `aSelection` is the pane's `QPalette::Highlight`, which
	/// `appearance_profile` derives from `code_selection_background`, and
	/// `aCanvas` its `QPalette::Base`, the code background. The selection is
	/// laid over the canvas at `core::text_selection_fill_alpha` of the
	/// variant: full strength except in light ([[BPR-1533]] v002).
	[[nodiscard]] auto code_text_selection_fill(const QColor& aSelection, const QColor& aCanvas, core::appearance_variant aVariant)
		-> QColor;

	/// \brief Appends the ranges that draw characters `aFrom` to `aTo` of one
	/// block as selected: the solid fill, each glyph in its own colour.
	///
	/// \details The selection is the top layer: it goes over the line's band,
	/// the selected line's wash and the intraline emphasis, as every text box
	/// in the window draws one. Unlike Qt's own selection, which repaints every
	/// glyph in `HighlightedText`, each run keeps its colour: a classified
	/// token's from the block's layout formats (`apply_syntax_format_ranges`),
	/// other text's from the document, else `Normal`. A colour moves only as
	/// far as `core::readable_selection_ink` needs to read on the fill, so it
	/// keeps its hue. The ranges are the block's, for `QTextLayout::draw`;
	/// `aTo` may include the block's line break, which the fill covers.
	void append_text_selection_ranges(
		QList<QTextLayout::FormatRange>& aRanges,
		const QTextBlock&                aBlock,
		int                              aFrom,
		int                              aTo,
		const code_text_selection&       aSelection);

	/// \brief Returns the spans of one line of a classification.
	///
	/// \details Found by search because the list is non-decreasing in `Line`,
	/// which is the contract `diff_workbench::adopt_token_spans` enforces and the
	/// reason painting one screenful does not cost the whole file.
	[[nodiscard]] auto spans_of_line(
		const application::token_span_list& aSpans,
		std::uint32_t                       aLine)
		-> std::span<const application::token_span>;

	/// \brief One whole text's classification and where its classifier lines
	/// begin, so a display line of that text can be coloured by its offset.
	///
	/// \details The classifier and every display surface split at LF only,
	/// as Git does ([[BPR-1432]]). [[BPR-1156]] for Suggest change,
	/// [[BPR-1157]] for the cycle page's source pane.
	struct classified_text
	{
		std::vector<std::uint64_t>      LineStarts;
		application::shared_token_spans Spans;
	};

	/// \brief Pairs the classification of `aText` with its LF line starts.
	[[nodiscard]] auto classify_text_lines(
		std::span<const std::uint8_t>   aText,
		application::shared_token_spans aSpans)
		-> classified_text;

	/// \brief The format ranges of one display line that begins at byte
	/// `aBegin` of a classified text, through `code_syntax_format_ranges`.
	///
	/// \details The spans of the classifier line holding the display line,
	/// cut to it and counted from its start; a display line never crosses an
	/// LF. An unclassified text gives no ranges.
	[[nodiscard]] auto code_syntax_format_ranges_at(
		const classified_text&        aText,
		std::uint64_t                 aBegin,
		std::span<const std::uint8_t> aContent,
		const code_role_colors&       aColors)
		-> QList<QTextLayout::FormatRange>;

	/// \brief What one sample row is: unchanged, removed, added, or where I am.
	enum class code_sample_row_kind : std::uint8_t {
		context,
		removed,
		added,
		current_line,
	};

	/// \brief The fill one sample row kind wears.
	///
	/// \details One rule, so the tile that paints directly and the code surface
	/// that emits bands cannot disagree about what "added" looks like. Context
	/// rows answer an invalid colour: unchanged lines are left untinted on
	/// purpose, because a background on every line marks nothing.
	[[nodiscard]] auto code_sample_row_fill(
		code_sample_row_kind    aKind,
		const code_role_colors& aColors) -> QColor;

	/// \brief One run of sample text and the category it belongs to.
	struct code_sample_token
	{
		QString               Text;
		core::appearance_role Role = core::appearance_role::syntax_normal;
	};

	/// \brief One line of a code sample: its number, its kind, its indent, its text.
	struct code_sample_row
	{
		std::uint32_t        Number = 0U;
		code_sample_row_kind Kind   = code_sample_row_kind::context;
		/// Leading tab stops, so the sample exercises the reviewer's tab width
		/// rather than a fixed number of spaces.
		std::uint32_t Indent = 0U;
		/// Whether the row carries the selection wash.
		bool                           Selected = false;
		std::vector<code_sample_token> Tokens;
	};

	using code_sample = std::vector<code_sample_row>;

	/// \brief RFC-BPR-0034's theme tile snippet: four lines of real code.
	///
	/// \details The accepted design's wider snippet, so a tile shows the diff and
	/// the syntax colours together: a keyword, a type, a function, a removed line,
	/// an added line and a comment, numbered in a gutter.
	[[nodiscard]] auto theme_tile_code_sample() -> code_sample;

	/// \brief RFC-BPR-0034's type preview snippet: six lines exercising every control.
	///
	/// \details An italic comment, bold keywords, a type, a function, the three
	/// pairs a ligature would join, two tab depths, a removed line, an added line
	/// carrying the selection and a macro, and the current line.
	[[nodiscard]] auto type_preview_code_sample() -> code_sample;

	/// \brief One sample expressed the way a code surface consumes a blob.
	///
	/// \details Text for the document, byte-offset spans for the painter, and each
	/// row's UTF-8 bytes so the byte-to-character conversion the review panes make
	/// is the conversion a preview makes too. A `syntax_normal` token produces no
	/// span, exactly as a classification of the real thing would not.
	struct code_sample_document
	{
		QString                      Text;
		application::token_span_list Spans;
		std::vector<QByteArray>      RowBytes;
	};

	/// \brief Expresses one sample as document text plus a classification.
	[[nodiscard]] auto build_code_sample_document(const code_sample& aSample)
		-> code_sample_document;

	/// \brief One row of a gutter column: where it is and what it says.
	struct code_gutter_row
	{
		qreal         Top    = 0.0;
		qreal         Height = 0.0;
		std::uint32_t Number = 0U;
	};

	/// \brief Paints a column of line numbers over its own background.
	///
	/// \returns How many numbers were painted.
	auto paint_code_gutter(
		QPainter&                        aPainter,
		const QRectF&                    aColumn,
		std::span<const code_gutter_row> aRows,
		const QColor&                    aBackground,
		const QColor&                    aText) -> std::uint32_t;

	/// \brief One white space character as laid out on a row.
	///
	/// \details Where the character starts and where the next one does, in the
	/// painter's coordinates, and whether it is a tab. A tab's right edge is its
	/// tab stop, so a tab that begins partway to a stop is shorter.
	struct white_space_mark
	{
		qreal Left  = 0.0;
		qreal Right = 0.0;
		bool  Tab   = false;

		auto operator<=>(const white_space_mark&) const = default;
	};

	/// \brief Classifies one line's characters for `core::white_space_drawn`.
	[[nodiscard]] auto white_space_characters(QStringView aLine)
		-> std::vector<core::white_space_character>;

	/// \brief Paints white space the Sublime Text way ([[BPR-1107]]).
	///
	/// \details A space is a small dot at the middle of its cell, and a tab a
	/// thin line from where it starts to its tab stop, both at the row's middle
	/// in `aColor` at a faint alpha. Drawn rather than written: a glyph would
	/// depend on the code font shipping one. This is the only white space
	/// renderer: the review panes, the type preview and the theme tiles all
	/// come here, so the preview shows what the review window shows.
	///
	/// \returns How many marks were painted.
	auto paint_white_space(
		QPainter&                         aPainter,
		const QRectF&                     aRow,
		std::span<const white_space_mark> aMarks,
		const QColor&                     aColor) -> std::uint32_t;

	/// \brief Paints the drawn white space of one laid-out editor line.
	///
	/// \details For a code surface that is a text editor: the characters are
	/// classified, the core decides which are drawn for `aDrawing` and whether
	/// the line is selected, and each drawn one is placed where the editor put
	/// it, so the tab stops are the editor's own.
	///
	/// \returns How many marks were painted.
	auto paint_white_space_of_block(
		QPainter&                 aPainter,
		const QPlainTextEdit&     aEditor,
		const QTextBlock&         aBlock,
		core::white_space_drawing aDrawing,
		bool                      aSelected,
		const QColor&             aColor) -> std::uint32_t;

	/// \brief How a directly painted sample is laid out.
	struct code_sample_paint_options
	{
		/// Row height; zero derives it from the font's own line spacing.
		qreal RowHeight = 0.0;
		/// Gutter width; zero derives it from the widest number shown.
		qreal GutterWidth = 0.0;
		bool  Gutter      = true;
		/// Tab stop distance; zero derives four space widths of the font.
		qreal                TabStopDistance = 0.0;
		code_syntax_emphasis Emphasis;
		/// [[BPR-1107]]. When white space is drawn.
		core::white_space_drawing WhiteSpace = core::white_space_drawing::none;
	};

	/// \brief What one directly painted sample actually drew.
	struct code_sample_paint_result
	{
		std::uint32_t RowsPainted       = 0U;
		std::uint32_t NumbersPainted    = 0U;
		std::uint32_t BandsPainted      = 0U;
		std::uint32_t WhiteSpacePainted = 0U;

		auto operator<=>(const code_sample_paint_result&) const = default;
	};

	/// \brief Paints one sample straight onto a painter, in one palette.
	///
	/// \details For surfaces too small to be a text editor. It decides no colour
	/// of its own: every fill and every glyph comes from the supplied role table,
	/// through the same `code_sample_row_fill` and `code_syntax_emphasis_of` rules
	/// a code surface uses, and the font is the one the caller was given. Rows
	/// that would fall outside the rect are not drawn, so the remainder of the
	/// rect keeps the sample's own code background.
	auto paint_code_sample(
		QPainter&                        aPainter,
		const QRectF&                    aRect,
		const code_sample&               aSample,
		const code_role_colors&          aColors,
		const QFont&                     aFont,
		const code_sample_paint_options& aOptions) -> code_sample_paint_result;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CODE_SAMPLE_PAINTER_HPP
