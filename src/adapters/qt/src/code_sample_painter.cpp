#include "bpr/qt/code_sample_painter.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QLatin1Char>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointF>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace bpr::qt {
	void code_gutter_text_cache::retain_visible_rows(const int aRows)
	{
		Labels.setMaxCost(qsizetype{ 2 } * std::max(1, aRows) + 2);
	}

	void code_gutter_text_cache::draw(QPainter& aPainter, const QRect& aRect, const QString& aText)
	{
		constexpr auto alignment = Qt::AlignRight | Qt::AlignVCenter;
		// The gutter's labels are plain ASCII. Preserve the complete native
		// formatting path for any future caller supplying other text.
		if (aText.isEmpty() || !std::ranges::all_of(aText, [](const QChar aCharacter) {
				return (aCharacter >= QLatin1Char{ '0' } && aCharacter <= QLatin1Char{ '9' })
					|| (aCharacter >= QLatin1Char{ 'a' } && aCharacter <= QLatin1Char{ 'z' });
			})) {
			aPainter.drawText(aRect, alignment, aText);
			return;
		}
		const auto* const device = aPainter.device();
		if (Font != aPainter.font() || Transform != aPainter.transform() || Device != device
			|| DevicePixelRatio != device->devicePixelRatioF() || LogicalDpiX != device->logicalDpiX() || LogicalDpiY != device->logicalDpiY()) {
			Labels.clear();
			Font             = aPainter.font();
			Transform        = aPainter.transform();
			Device           = device;
			DevicePixelRatio = device->devicePixelRatioF();
			LogicalDpiX      = device->logicalDpiX();
			LogicalDpiY      = device->logicalDpiY();
		}
		auto* cached = Labels.object(aText);
		if (cached == nullptr || cached->Cell != aRect.size()) {
			auto made = std::make_unique<label>();
			made->Text.setText(aText);
			made->Text.setTextFormat(Qt::PlainText);
			made->Cell = aRect.size();
			// The floating overload retains Qt's subpixel alignment. The integer
			// bounding rectangle rounds that origin and shifts native glyphs.
			made->Offset = aPainter.boundingRect(QRectF{ QPointF{ }, QSizeF{ aRect.size() } }, alignment, aText).topLeft();
			Labels.insert(aText, made.release());
			cached = Labels.object(aText);
		}
		if (cached == nullptr) {
			aPainter.drawText(aRect, alignment, aText);
			return;
		}
		aPainter.save();
		aPainter.setClipRect(aRect, Qt::IntersectClip);
		aPainter.drawStaticText(QPointF{ aRect.topLeft() } + cached->Offset, cached->Text);
		aPainter.restore();
	}

	namespace {
		/// Breathing room either side of the widest number a sample gutter shows.
		constexpr auto SampleGutterPadding = 6.0;
		/// The gap between the gutter and a row's first glyph.
		constexpr auto SampleCodeInset = 8.0;
		/// A sample row is this multiple of the code font's own line spacing, which
		/// is the airier setting the accepted design draws a sample at.
		constexpr auto SampleRowSpacing = 1.45;
		/// [[BPR-1107]]. A tab's line stops this far short of each end.
		constexpr auto WhiteSpaceTabInset = 1.5;
		/// A space's dot: its radius as a share of the space's advance, bounded.
		constexpr auto WhiteSpaceDotShare   = 0.12;
		constexpr auto WhiteSpaceDotMinimum = 0.9;
		constexpr auto WhiteSpaceDotMaximum = 1.6;
		/// White space is a hint rather than a character, so it sits below the
		/// strength of the text around it.
		constexpr auto WhiteSpaceAlpha = 140;

		[[nodiscard]] auto role_colour(
			const code_role_colors&     aColors,
			const core::appearance_role aRole) -> QColor
		{
			return aColors[core::index_of(aRole)];
		}
	} // namespace

	auto code_role_colors_of(const core::appearance_palette& aPalette)
		-> code_role_colors
	{
		auto colors = code_role_colors{ };
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			colors[index] = to_presentation_color(aPalette[index]);
		}
		return colors;
	}

	auto character_offset(
		const std::span<const std::uint8_t> aLineBytes,
		const std::uint64_t                 aByteOffset) -> int
	{
		const auto bounded = std::min<std::size_t>(
			static_cast<std::size_t>(aByteOffset), aLineBytes.size());
		return static_cast<int>(
			QString::fromUtf8(
				reinterpret_cast<const char*>(aLineBytes.data()),
				static_cast<qsizetype>(bounded))
				.size());
	}

	void append_line_band(
		QList<QTextEdit::ExtraSelection>& aSelections,
		QPlainTextEdit&                   aPane,
		const core::line_span             aSpan,
		const QColor&                     aColor)
	{
		if (!aColor.isValid() || aSpan.empty()) {
			return;
		}
		for (auto line = aSpan.Begin; line < aSpan.End; ++line) {
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(line));
			if (!block.isValid()) {
				continue;
			}
			auto band   = QTextEdit::ExtraSelection{ };
			band.cursor = QTextCursor{ block };
			band.format.setProperty(QTextFormat::FullWidthSelection, true);
			band.format.setBackground(aColor);
			aSelections.push_back(band);
		}
	}

	void append_selection_band(
		QList<QTextEdit::ExtraSelection>& aSelections,
		QPlainTextEdit&                   aPane,
		const core::line_span             aLines,
		const core::line_span             aPaintable)
	{
		const auto begin = std::max(aLines.Begin, aPaintable.Begin);
		const auto end   = std::min(aLines.End, aPaintable.End);
		auto       color = aPane.palette().color(QPalette::Highlight);
		color.setAlpha(core::selection_wash_alpha);
		for (auto line = begin; line < end; ++line) {
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(line));
			if (!block.isValid()) {
				continue;
			}
			auto band   = QTextEdit::ExtraSelection{ };
			band.cursor = QTextCursor{ block };
			band.format.setProperty(QTextFormat::FullWidthSelection, true);
			band.format.setBackground(color);
			aSelections.push_back(band);
		}
	}

	auto code_syntax_emphasis_of(
		const core::appearance_role aRole,
		const code_syntax_emphasis  aSwitches) noexcept -> code_syntax_emphasis
	{
		const auto bold = aRole == core::appearance_role::syntax_keyword
			|| aRole == core::appearance_role::syntax_control_flow
			|| aRole == core::appearance_role::syntax_constant;
		const auto italic = aRole == core::appearance_role::syntax_comment
			|| aRole == core::appearance_role::syntax_documentation;
		return code_syntax_emphasis{
			.Bold   = bold && aSwitches.Bold,
			.Italic = italic && aSwitches.Italic,
		};
	}

	auto code_syntax_format_ranges(const code_syntax_format_request& aRequest)
		-> QList<QTextLayout::FormatRange>
	{
		auto formats = QList<QTextLayout::FormatRange>{ };
		if (aRequest.StrikeOut) {
			auto strike   = QTextLayout::FormatRange{ };
			strike.length = aRequest.StrikeOutLength;
			strike.format.setFontStrikeOut(true);
			formats.push_back(strike);
		}
		if (aRequest.Colors == nullptr) {
			return formats;
		}
		for (const auto& span : aRequest.Spans) {
			const auto index = static_cast<std::size_t>(span.Role);
			if (
				span.Length == 0U || index >= aRequest.Colors->size()
				|| span.Role == core::appearance_role::syntax_normal) {
				// Normal text is what the pane already draws. Restating it as a
				// format would cost a range per unclassified word to change
				// nothing.
				continue;
			}
			const auto& color = (*aRequest.Colors)[index];
			if (!color.isValid()) {
				continue;
			}
			const auto from = character_offset(aRequest.LineBytes, span.Column);
			const auto to   = character_offset(
				aRequest.LineBytes,
				static_cast<std::uint64_t>(span.Column) + span.Length);
			if (from >= to) {
				continue;
			}
			auto range   = QTextLayout::FormatRange{ };
			range.start  = from;
			range.length = to - from;
			range.format.setForeground(color);
			if (aRequest.StrikeOut) {
				range.format.setFontStrikeOut(true);
			}
			// Set only when asked for. A format that stated "not bold" is a
			// different format from one that says nothing about weight, and the
			// difference is what decides whether a repaint relayouts the block.
			const auto emphasis = code_syntax_emphasis_of(
				span.Role, aRequest.Emphasis);
			if (emphasis.Bold) {
				range.format.setFontWeight(QFont::Bold);
			}
			if (emphasis.Italic) {
				range.format.setFontItalic(true);
			}
			formats.push_back(range);
		}
		return formats;
	}

	auto apply_syntax_format_ranges(
		QTextDocument&                         aDocument,
		const QTextBlock&                      aBlock,
		const QList<QTextLayout::FormatRange>& aFormats) -> bool
	{
		if (aBlock.layout() == nullptr || aBlock.layout()->formats() == aFormats) {
			return false;
		}
		aBlock.layout()->setFormats(aFormats);
		aDocument.markContentsDirty(aBlock.position(), aBlock.length());
		return true;
	}

	void append_intraline_emphasis_span(
		QList<QTextEdit::ExtraSelection>&       aSelections,
		const QTextBlock&                       aBlock,
		const int                               aFrom,
		const int                               aTo,
		const QColor&                           aFill,
		const std::optional<code_emphasis_ink>& aInk)
	{
		if (!aBlock.isValid() || aFrom >= aTo) {
			return;
		}
		const auto readable = [&aFill, &aInk](const QColor& aGlyph) {
			return to_presentation_color(core::readable_emphasis_foreground(
				to_appearance_color(aGlyph), to_appearance_color(aFill), aInk->Variant));
		};
		auto emphasis   = QTextEdit::ExtraSelection{ };
		emphasis.cursor = QTextCursor{ aBlock };
		emphasis.cursor.setPosition(aBlock.position() + aFrom);
		emphasis.cursor.setPosition(aBlock.position() + aTo, QTextCursor::KeepAnchor);
		emphasis.format.setBackground(aFill);
		if (aInk.has_value()) {
			emphasis.format.setForeground(readable(aInk->Normal));
		}
		aSelections.push_back(emphasis);
		// Keep the exact edit range as one background layer, then lay each
		// classified token's colour over its part of it. A token layer carries
		// the same fill: Qt's layout skips a selection that sets a foreground
		// and no background wherever an earlier selection already drew the
		// text, so a foreground-only layer was never painted and every changed
		// glyph read in the normal ink, a green comment as white
		// ([[BPR-PENDING-R]]). The fill is the one already under it, so only
		// the ink changes; the source, layout and the unedited part of the
		// token stay untouched.
		if (!aInk.has_value() || aBlock.layout() == nullptr) {
			return;
		}
		for (const auto& token : aBlock.layout()->formats()) {
			const auto begin = std::max(aFrom, token.start);
			const auto end   = std::min(aTo, token.start + token.length);
			if (begin >= end || token.format.foreground().style() == Qt::NoBrush) {
				continue;
			}
			auto syntax   = QTextEdit::ExtraSelection{ };
			syntax.cursor = QTextCursor{ aBlock };
			syntax.cursor.setPosition(aBlock.position() + begin);
			syntax.cursor.setPosition(aBlock.position() + end, QTextCursor::KeepAnchor);
			syntax.format.setBackground(aFill);
			syntax.format.setForeground(readable(token.format.foreground().color()));
			aSelections.push_back(syntax);
		}
	}

	auto code_text_selection_fill(const QColor& aSelection, const QColor& aCanvas, const core::appearance_variant aVariant) -> QColor
	{
		if (!aSelection.isValid() || !aCanvas.isValid()) {
			return aSelection;
		}
		return to_presentation_color(core::composited(to_appearance_color(aSelection), to_appearance_color(aCanvas),
													  core::text_selection_fill_alpha(aVariant)));
	}

	void append_text_selection_ranges(
		QList<QTextLayout::FormatRange>& aRanges,
		const QTextBlock&                aBlock,
		const int                        aFrom,
		const int                        aTo,
		const code_text_selection&       aSelection)
	{
		if (!aBlock.isValid() || aFrom >= aTo || !aSelection.Fill.isValid()) {
			return;
		}
		struct run
		{
			int    Begin = 0;
			int    End   = 0;
			QColor Ink;
		};
		// The colour each character is drawn in without the selection: the
		// document's own for a fragment that sets one, else the normal ink,
		// then each token's over that.
		auto runs = std::vector<run>{ };
		for (auto fragment = aBlock.begin(); !fragment.atEnd(); ++fragment) {
			const auto part  = fragment.fragment();
			const auto begin = std::max(aFrom, part.position() - aBlock.position());
			const auto end   = std::min(aTo, part.position() - aBlock.position() + part.length());
			if (begin < end) {
				const auto foreground = part.charFormat().foreground();
				runs.push_back({ .Begin = begin, .End = end, .Ink = foreground.style() == Qt::NoBrush ? aSelection.Normal : foreground.color() });
			}
		}
		// The line break, past the text, is covered by the fill.
		const auto textEnd = std::max(aFrom, aBlock.length() - 1);
		if (textEnd < aTo) {
			runs.push_back({ .Begin = textEnd, .End = aTo, .Ink = aSelection.Normal });
		}
		if (aBlock.layout() != nullptr) {
			for (const auto& token : aBlock.layout()->formats()) {
				if (token.format.foreground().style() == Qt::NoBrush) {
					continue;
				}
				const auto tokenEnd = token.start + token.length;
				auto       split    = std::vector<run>{ };
				split.reserve(runs.size() + 2U);
				for (const auto& each : runs) {
					const auto begin = std::max(each.Begin, token.start);
					const auto end   = std::min(each.End, tokenEnd);
					if (begin >= end) {
						split.push_back(each);
						continue;
					}
					if (each.Begin < begin) {
						split.push_back({ .Begin = each.Begin, .End = begin, .Ink = each.Ink });
					}
					split.push_back({ .Begin = begin, .End = end, .Ink = token.format.foreground().color() });
					if (end < each.End) {
						split.push_back({ .Begin = end, .End = each.End, .Ink = each.Ink });
					}
				}
				runs = std::move(split);
			}
		}
		const auto fill = to_appearance_color(aSelection.Fill);
		for (const auto& each : runs) {
			auto range   = QTextLayout::FormatRange{ };
			range.start  = each.Begin;
			range.length = each.End - each.Begin;
			range.format.setBackground(aSelection.Fill);
			if (aSelection.FullWidth) {
				range.format.setProperty(QTextFormat::FullWidthSelection, true);
			}
			range.format.setForeground(to_presentation_color(
				core::readable_selection_ink(to_appearance_color(each.Ink.isValid() ? each.Ink : aSelection.Normal), fill, aSelection.Variant)));
			// Adjacent runs in one ink draw as one range.
			if (!aRanges.isEmpty() && aRanges.back().start + aRanges.back().length == range.start
				&& aRanges.back().format == range.format) {
				aRanges.back().length += range.length;
			} else {
				aRanges.push_back(range);
			}
		}
	}

	auto spans_of_line(
		const application::token_span_list& aSpans,
		const std::uint32_t                 aLine)
		-> std::span<const application::token_span>
	{
		const auto first = std::ranges::lower_bound(
			aSpans,
			aLine,
			{ },
			[](const application::token_span& aSpan) { return aSpan.Line; });
		const auto last = std::find_if(
			first,
			aSpans.end(),
			[aLine](const application::token_span& aSpan) {
				return aSpan.Line != aLine;
			});
		return std::span<const application::token_span>{ first, last };
	}

	auto classify_text_lines(
		const std::span<const std::uint8_t> aText,
		application::shared_token_spans     aSpans)
		-> classified_text
	{
		auto starts = std::vector<std::uint64_t>{ 0U };
		for (std::size_t index = 0; index < aText.size(); ++index) {
			if (aText[index] == static_cast<std::uint8_t>('\n')) {
				starts.push_back(index + 1U);
			}
		}
		return { .LineStarts = std::move(starts), .Spans = std::move(aSpans) };
	}

	auto code_syntax_format_ranges_at(
		const classified_text&              aText,
		const std::uint64_t                 aBegin,
		const std::span<const std::uint8_t> aContent,
		const code_role_colors&             aColors)
		-> QList<QTextLayout::FormatRange>
	{
		if (!aText.Spans || aText.LineStarts.empty()) {
			return { };
		}
		const auto line  = static_cast<std::size_t>(std::ranges::upper_bound(aText.LineStarts, aBegin) - aText.LineStarts.begin()) - 1U;
		const auto start = aText.LineStarts[line];
		const auto end   = aBegin + aContent.size();
		auto       spans = std::vector<application::token_span>{ };
		for (const auto& span : spans_of_line(*aText.Spans, static_cast<std::uint32_t>(line))) {
			const auto from = std::max<std::uint64_t>(start + span.Column, aBegin);
			const auto to   = std::min<std::uint64_t>(start + span.Column + span.Length, end);
			if (from < to) {
				spans.push_back({ .Line = 0U, .Column = static_cast<std::uint32_t>(from - aBegin), .Length = static_cast<std::uint32_t>(to - from), .Role = span.Role });
			}
		}
		return code_syntax_format_ranges({ .LineBytes = aContent, .Spans = spans, .Colors = &aColors });
	}

	auto code_sample_row_fill(
		const code_sample_row_kind aKind,
		const code_role_colors&    aColors) -> QColor
	{
		switch (aKind) {
		case code_sample_row_kind::context:
			// Unchanged lines are left untinted on purpose, exactly as the review
			// panes leave them: the eye should go to what moved.
			return { };
		case code_sample_row_kind::removed:
			return role_colour(
				aColors, core::appearance_role::diff_removed_background);
		case code_sample_row_kind::added:
			return role_colour(
				aColors, core::appearance_role::diff_added_background);
		case code_sample_row_kind::current_line:
			return role_colour(
				aColors, core::appearance_role::code_current_line_background);
		}
		return { };
	}

	auto theme_tile_code_sample() -> code_sample
	{
		using role = core::appearance_role;
		return code_sample{
			code_sample_row{
				.Number = 41U,
				.Kind   = code_sample_row_kind::context,
				.Tokens = {
					{ QStringLiteral("bool"), role::syntax_keyword },
					{ QStringLiteral(" "), role::syntax_normal },
					{ QStringLiteral("ShouldUseLSS"), role::syntax_function },
					{ QStringLiteral("("), role::syntax_operator },
					{ QStringLiteral("const"), role::syntax_keyword },
					{ QStringLiteral(" "), role::syntax_normal },
					{ QStringLiteral("FViewInfo"), role::syntax_type },
					{ QStringLiteral("& "), role::syntax_operator },
					{ QStringLiteral("View"), role::syntax_variable },
					{ QStringLiteral(")"), role::syntax_operator },
				},
			},
			code_sample_row{
				.Number = 42U,
				.Kind   = code_sample_row_kind::removed,
				.Tokens = {
					{ QStringLiteral("if"), role::syntax_control_flow },
					{ QStringLiteral(" (!"), role::syntax_operator },
					{ QStringLiteral("View"), role::syntax_variable },
					{ QStringLiteral("."), role::syntax_operator },
					{ QStringLiteral("bUseHairLSS"), role::syntax_variable },
					{ QStringLiteral(") "), role::syntax_operator },
					{ QStringLiteral("return"), role::syntax_control_flow },
					{ QStringLiteral(" "), role::syntax_normal },
					{ QStringLiteral("false"), role::syntax_constant },
					{ QStringLiteral(";"), role::syntax_operator },
				},
			},
			code_sample_row{
				.Number = 43U,
				.Kind   = code_sample_row_kind::added,
				.Tokens = {
					{ QStringLiteral("if"), role::syntax_control_flow },
					{ QStringLiteral(" (!"), role::syntax_operator },
					{ QStringLiteral("View"), role::syntax_variable },
					{ QStringLiteral("."), role::syntax_operator },
					{ QStringLiteral("bUseHairLSS"), role::syntax_variable },
					{ QStringLiteral(" || "), role::syntax_operator },
					{ QStringLiteral("Samples"), role::syntax_variable },
					{ QStringLiteral(" < "), role::syntax_operator },
					{ QStringLiteral("2"), role::syntax_number },
					{ QStringLiteral(") "), role::syntax_operator },
					{ QStringLiteral("return"), role::syntax_control_flow },
					{ QStringLiteral(" "), role::syntax_normal },
					{ QStringLiteral("false"), role::syntax_constant },
					{ QStringLiteral(";"), role::syntax_operator },
				},
			},
			code_sample_row{
				.Number = 44U,
				.Kind   = code_sample_row_kind::context,
				.Tokens = {
					{ QStringLiteral("// strand lighting needs two samples"), role::syntax_comment },
				},
			},
		};
	}

	auto type_preview_code_sample() -> code_sample
	{
		using role = core::appearance_role;
		return code_sample{
			code_sample_row{
				.Number = 40U,
				.Kind   = code_sample_row_kind::context,
				.Tokens = {
					{ QStringLiteral("// strands need two samples"),
					  role::syntax_comment },
				},
			},
			code_sample_row{
				.Number = 41U,
				.Kind   = code_sample_row_kind::context,
				.Tokens = {
					{ QStringLiteral("bool"), role::syntax_keyword },
					{ QStringLiteral(" "), role::syntax_normal },
					{ QStringLiteral("FHairLSS"), role::syntax_type },
					{ QStringLiteral("::"), role::syntax_operator },
					{ QStringLiteral("ShouldUse"), role::syntax_function },
					{ QStringLiteral("("), role::syntax_operator },
					{ QStringLiteral("FView"), role::syntax_type },
					{ QStringLiteral("& "), role::syntax_operator },
					{ QStringLiteral("View"), role::syntax_variable },
					{ QStringLiteral(") {"), role::syntax_operator },
				},
			},
			code_sample_row{
				.Number = 42U,
				.Kind   = code_sample_row_kind::context,
				.Indent = 1U,
				.Tokens = {
					{ QStringLiteral("if"), role::syntax_control_flow },
					{ QStringLiteral(" ("), role::syntax_operator },
					{ QStringLiteral("Samples"), role::syntax_variable },
					{ QStringLiteral(" != "), role::syntax_operator },
					{ QStringLiteral("2"), role::syntax_number },
					{ QStringLiteral(" && !"), role::syntax_operator },
					{ QStringLiteral("bUse"), role::syntax_variable },
					{ QStringLiteral(")"), role::syntax_operator },
				},
			},
			code_sample_row{
				.Number = 43U,
				.Kind   = code_sample_row_kind::removed,
				.Indent = 2U,
				.Tokens = {
					{ QStringLiteral("return"), role::syntax_control_flow },
					{ QStringLiteral(" "), role::syntax_normal },
					{ QStringLiteral("false"), role::syntax_constant },
					{ QStringLiteral(";"), role::syntax_operator },
				},
			},
			code_sample_row{
				.Number   = 44U,
				.Kind     = code_sample_row_kind::added,
				.Indent   = 2U,
				.Selected = true,
				.Tokens   = {
					{ QStringLiteral("return"), role::syntax_control_flow },
					{ QStringLiteral(" "), role::syntax_normal },
					{ QStringLiteral("Count"), role::syntax_variable },
					{ QStringLiteral(" >= "), role::syntax_operator },
					{ QStringLiteral("MAX_SAMPLES"), role::syntax_macro },
					{ QStringLiteral(";"), role::syntax_operator },
				},
			},
			code_sample_row{
				.Number = 45U,
				.Kind   = code_sample_row_kind::current_line,
				.Tokens = {
					{ QStringLiteral("}"), role::syntax_operator },
				},
			},
		};
	}

	auto build_code_sample_document(const code_sample& aSample)
		-> code_sample_document
	{
		auto built = code_sample_document{ };
		built.RowBytes.reserve(aSample.size());
		for (auto row = std::size_t{ 0U }; row < aSample.size(); ++row) {
			const auto& source = aSample[row];
			auto        text   = QString{
				static_cast<qsizetype>(source.Indent), QLatin1Char{ '\t' }
			};
			auto column = static_cast<std::uint32_t>(text.toUtf8().size());
			for (const auto& token : source.Tokens) {
				const auto bytes = static_cast<std::uint32_t>(
					token.Text.toUtf8().size());
				if (
					bytes > 0U
					&& token.Role != core::appearance_role::syntax_normal) {
					built.Spans.push_back(application::token_span{
						.Line   = static_cast<std::uint32_t>(row),
						.Column = column,
						.Length = bytes,
						.Role   = token.Role,
					});
				}
				text += token.Text;
				column += bytes;
			}
			built.RowBytes.push_back(text.toUtf8());
			if (row > 0U) {
				built.Text += QLatin1Char{ '\n' };
			}
			built.Text += text;
		}
		return built;
	}

	auto paint_code_gutter(
		QPainter&                              aPainter,
		const QRectF&                          aColumn,
		const std::span<const code_gutter_row> aRows,
		const QColor&                          aBackground,
		const QColor&                          aText) -> std::uint32_t
	{
		if (aBackground.isValid()) {
			aPainter.fillRect(aColumn, aBackground);
		}
		if (!aText.isValid()) {
			return 0U;
		}
		aPainter.setPen(aText);
		auto painted = std::uint32_t{ 0U };
		for (const auto& row : aRows) {
			aPainter.drawText(
				QRectF{ aColumn.left(),
						row.Top,
						aColumn.width() - SampleGutterPadding,
						row.Height },
				Qt::AlignRight | Qt::AlignVCenter,
				QString::number(row.Number));
			++painted;
		}
		return painted;
	}

	auto white_space_characters(const QStringView aLine)
		-> std::vector<core::white_space_character>
	{
		auto characters = std::vector<core::white_space_character>{ };
		characters.reserve(static_cast<std::size_t>(aLine.size()));
		for (const auto character : aLine) {
			characters.push_back(character == QLatin1Char{ ' ' }        ? core::white_space_character::space
									 : character == QLatin1Char{ '\t' } ? core::white_space_character::tab
																		: core::white_space_character::other);
		}
		return characters;
	}

	auto paint_white_space(
		QPainter&                               aPainter,
		const QRectF&                           aRow,
		const std::span<const white_space_mark> aMarks,
		const QColor&                           aColor) -> std::uint32_t
	{
		if (aMarks.empty() || !aColor.isValid()) {
			return 0U;
		}
		auto faint = aColor;
		faint.setAlpha(WhiteSpaceAlpha);
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto middle  = aRow.center().y();
		auto       painted = std::uint32_t{ 0U };
		for (const auto& mark : aMarks) {
			const auto width = mark.Right - mark.Left;
			if (width <= 0.0) {
				continue;
			}
			if (mark.Tab) {
				// A line from where the tab starts to its stop, a hair inside
				// each end so two tabs in a row read as two.
				const auto inset = std::min(WhiteSpaceTabInset, width / 4.0);
				aPainter.setPen(QPen{ faint, 1.0 });
				aPainter.setBrush(Qt::NoBrush);
				aPainter.drawLine(QPointF{ mark.Left + inset, middle }, QPointF{ mark.Right - inset, middle });
			} else {
				const auto radius = std::clamp(width * WhiteSpaceDotShare, WhiteSpaceDotMinimum, WhiteSpaceDotMaximum);
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(faint);
				aPainter.drawEllipse(QPointF{ (mark.Left + mark.Right) / 2.0, middle }, radius, radius);
			}
			++painted;
		}
		aPainter.restore();
		return painted;
	}

	auto paint_white_space_of_block(
		QPainter&                       aPainter,
		const QPlainTextEdit&           aEditor,
		const QTextBlock&               aBlock,
		const core::white_space_drawing aDrawing,
		const bool                      aSelected,
		const QColor&                   aColor) -> std::uint32_t
	{
		if (!aBlock.isValid() || !core::white_space_may_draw(aDrawing, aSelected) || !aColor.isValid()) {
			return 0U;
		}
		const auto text = aBlock.text();
		// [[BPR-1525]]. Most source rows contain no drawable white space.
		// Avoid constructing and scanning two per-character vectors on every
		// paint for those rows; the core still decides every row with marks.
		if (!text.contains(QLatin1Char{ ' ' }) && !text.contains(QLatin1Char{ '\t' })) {
			return 0U;
		}
		const auto drawn = core::white_space_drawn(white_space_characters(text), aDrawing, aSelected);
		auto       marks = std::vector<white_space_mark>{ };
		auto       row   = QRectF{ };
		auto       at    = QTextCursor{ aBlock };
		auto       first = std::size_t{ 0U };
		auto       last  = drawn.size();
		// [[BPR-1525]]. Project the visible part of an unwrapped LTR ASCII
		// line rather than query every offscreen mark in a long source row.
		// Qt owns both edge hit tests and every retained endpoint. Include an
		// adjacent character at each edge for a partially visible tab/space.
		const auto simple = aEditor.lineWrapMode() == QPlainTextEdit::NoWrap
			&& aEditor.layoutDirection() == Qt::LeftToRight
			&& aBlock.blockFormat().layoutDirection() != Qt::RightToLeft
			&& aEditor.document()->defaultTextOption().textDirection() != Qt::RightToLeft
			&& std::ranges::all_of(text, [](const QChar aCharacter) { return aCharacter == QLatin1Char{ '\t' } || (aCharacter.unicode() >= 0x20U && aCharacter.unicode() <= 0x7EU); });
		if (simple && !aEditor.viewport()->rect().isEmpty()) {
			const auto visible = aEditor.viewport()->rect();
			const auto middle  = std::clamp(aEditor.cursorRect(at).center().y(), visible.top(), visible.bottom());
			const auto begin   = aEditor.cursorForPosition(QPoint{ visible.left(), middle });
			const auto end     = aEditor.cursorForPosition(QPoint{ visible.right(), middle });
			if (begin.block() == aBlock && end.block() == aBlock && begin.position() <= end.position()) {
				first = static_cast<std::size_t>(std::max(0, begin.position() - aBlock.position() - 1));
				last  = std::min(last, static_cast<std::size_t>(end.position() - aBlock.position() + 1));
			}
		}
		for (auto index = first; index < last; ++index) {
			if (!drawn[index]) {
				continue;
			}
			// Where the editor put the character and the one after it, so the
			// tab stops, the font and the scroll offset are the editor's own.
			at.setPosition(aBlock.position() + static_cast<int>(index));
			const auto start = aEditor.cursorRect(at);
			at.setPosition(aBlock.position() + static_cast<int>(index) + 1);
			const auto end = aEditor.cursorRect(at);
			if (row.isNull()) {
				row = QRectF{ start };
			}
			marks.push_back(white_space_mark{
				.Left  = static_cast<qreal>(start.left()),
				.Right = static_cast<qreal>(end.left()),
				.Tab   = text.at(static_cast<qsizetype>(index)) == QLatin1Char{ '\t' },
			});
		}
		return paint_white_space(aPainter, row, marks, aColor);
	}

	auto paint_code_sample(
		QPainter&                        aPainter,
		const QRectF&                    aRect,
		const code_sample&               aSample,
		const code_role_colors&          aColors,
		const QFont&                     aFont,
		const code_sample_paint_options& aOptions) -> code_sample_paint_result
	{
		auto painted = code_sample_paint_result{ };
		aPainter.fillRect(
			aRect, role_colour(aColors, core::appearance_role::code_background));
		const auto metrics     = QFontMetricsF{ aFont };
		const auto rowHeight   = aOptions.RowHeight > 0.0
			? aOptions.RowHeight
			: metrics.lineSpacing() * SampleRowSpacing;
		const auto gutterWidth = !aOptions.Gutter ? 0.0
			: aOptions.GutterWidth > 0.0
			? aOptions.GutterWidth
			: metrics.horizontalAdvance(QStringLiteral("99"))
				+ 2.0 * SampleGutterPadding;
		const auto tabStop     = aOptions.TabStopDistance > 0.0
			? aOptions.TabStopDistance
			: metrics.horizontalAdvance(QLatin1Char{ ' ' }) * 4.0;
		const auto textLeft    = aRect.left() + gutterWidth + SampleCodeInset;
		auto       numbers     = std::vector<code_gutter_row>{ };
		auto       top         = aRect.top();
		for (const auto& row : aSample) {
			if (top + rowHeight > aRect.bottom()) {
				break;
			}
			const auto rowRect = QRectF{
				aRect.left(), top, aRect.width(), rowHeight
			};
			if (const auto fill = code_sample_row_fill(row.Kind, aColors);
				fill.isValid()) {
				aPainter.fillRect(rowRect, fill);
				++painted.BandsPainted;
			}
			if (row.Selected) {
				// The same wash a code surface lays over a selected row: the
				// selection colour at the core's own opacity, so the band the row
				// already wears still reads through it.
				auto wash = role_colour(
					aColors, core::appearance_role::code_selection_background);
				if (wash.isValid()) {
					wash.setAlpha(core::selection_wash_alpha);
					aPainter.fillRect(rowRect, wash);
					++painted.BandsPainted;
				}
			}
			// [[BPR-1107]]. The row's white space, laid out the way the text below
			// is drawn: the leading tabs to their stops, then every character of
			// every token at its own advance.
			if (aOptions.WhiteSpace != core::white_space_drawing::none) {
				auto line = QString{ static_cast<qsizetype>(row.Indent), QLatin1Char{ '\t' } };
				for (const auto& token : row.Tokens) {
					line += token.Text;
				}
				const auto drawn = core::white_space_drawn(white_space_characters(line), aOptions.WhiteSpace, row.Selected);
				auto       marks = std::vector<white_space_mark>{ };
				auto       index = std::size_t{ 0U };
				const auto mark  = [&](const qreal aLeft, const qreal aRight, const bool aTab) {
					if (index < drawn.size() && drawn[index]) {
						marks.push_back(white_space_mark{ .Left = aLeft, .Right = aRight, .Tab = aTab });
					}
					++index;
				};
				for (auto depth = std::uint32_t{ 0U }; depth < row.Indent; ++depth) {
					const auto left = textLeft + static_cast<qreal>(depth) * tabStop;
					mark(left, left + tabStop, true);
				}
				// Each character at the offset the token's own text reaches, in
				// the token's own emphasis, which is how the text is drawn below.
				auto tokenLeft = textLeft + static_cast<qreal>(row.Indent) * tabStop;
				for (const auto& token : row.Tokens) {
					auto       font     = aFont;
					const auto emphasis = code_syntax_emphasis_of(token.Role, aOptions.Emphasis);
					if (emphasis.Bold) {
						font.setBold(true);
					}
					if (emphasis.Italic) {
						font.setItalic(true);
					}
					const auto tokenMetrics = QFontMetricsF{ font };
					for (auto offset = qsizetype{ 0 }; offset < token.Text.size(); ++offset) {
						const auto character = token.Text.at(offset);
						const auto left      = tokenLeft + tokenMetrics.horizontalAdvance(token.Text.left(offset));
						const auto right     = character == QLatin1Char{ '\t' }
							? textLeft + (std::floor((left - textLeft) / tabStop) + 1.0) * tabStop
							: tokenLeft + tokenMetrics.horizontalAdvance(token.Text.left(offset + 1));
						mark(left, right, character == QLatin1Char{ '\t' });
					}
					tokenLeft += tokenMetrics.horizontalAdvance(token.Text);
				}
				painted.WhiteSpacePainted += paint_white_space(
					aPainter,
					QRectF{ textLeft, top, aRect.right() - textLeft, rowHeight },
					marks,
					role_colour(aColors, core::appearance_role::code_line_number_text));
			}
			auto x = textLeft + static_cast<qreal>(row.Indent) * tabStop;
			for (const auto& token : row.Tokens) {
				auto       font     = aFont;
				const auto emphasis = code_syntax_emphasis_of(
					token.Role, aOptions.Emphasis);
				if (emphasis.Bold) {
					font.setBold(true);
				}
				if (emphasis.Italic) {
					font.setItalic(true);
				}
				aPainter.setFont(font);
				aPainter.setPen(role_colour(aColors, token.Role));
				const auto advance = QFontMetricsF{ font }.horizontalAdvance(
					token.Text);
				aPainter.drawText(
					QRectF{ x, top, advance + 1.0, rowHeight },
					Qt::AlignLeft | Qt::AlignVCenter,
					token.Text);
				x += advance;
			}
			numbers.push_back(code_gutter_row{
				.Top = top, .Height = rowHeight, .Number = row.Number });
			++painted.RowsPainted;
			top += rowHeight;
		}
		if (aOptions.Gutter && !numbers.empty()) {
			// Over the bands and beside the code: the numbers column carries its
			// own background, and the rows it spans are the rows that were drawn.
			aPainter.setFont(aFont);
			painted.NumbersPainted = paint_code_gutter(
				aPainter,
				QRectF{ aRect.left(), aRect.top(), gutterWidth, top - aRect.top() },
				numbers,
				role_colour(
					aColors, core::appearance_role::code_gutter_background),
				role_colour(
					aColors, core::appearance_role::code_line_number_text));
		}
		return painted;
	}
} // namespace bpr::qt
