#include "bpr/qt/code_sample_painter.hpp"

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
#include <cstddef>

namespace bpr::qt {
	namespace {
		/// Breathing room either side of the widest number a sample gutter shows.
		constexpr auto SampleGutterPadding = 6.0;
		/// The gap between the gutter and a row's first glyph.
		constexpr auto SampleCodeInset = 8.0;
		/// A sample row is this multiple of the code font's own line spacing, which
		/// is the airier setting the accepted design draws a sample at.
		constexpr auto SampleRowSpacing = 1.45;
		/// How far a tab marker's arrow reaches back along the tab stop.
		constexpr auto TabMarkerArm = 3.0;
		/// A tab marker is a hint rather than a character, so it sits below the
		/// strength of the text it measures.
		constexpr auto TabMarkerAlpha = 140;

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

	auto paint_tab_markers(
		QPainter&           aPainter,
		const QRectF&       aRow,
		const std::uint32_t aIndent,
		const qreal         aTabStopDistance,
		const QColor&       aColor) -> std::uint32_t
	{
		if (aIndent == 0U || aTabStopDistance <= 0.0 || !aColor.isValid()) {
			return 0U;
		}
		auto faint = aColor;
		faint.setAlpha(TabMarkerAlpha);
		aPainter.save();
		aPainter.setPen(QPen{ faint, 1.0 });
		const auto middle  = aRow.center().y();
		const auto arm     = std::min(TabMarkerArm, aTabStopDistance / 4.0);
		auto       painted = std::uint32_t{ 0U };
		for (auto depth = std::uint32_t{ 0U }; depth < aIndent; ++depth) {
			const auto left = aRow.left()
				+ static_cast<qreal>(depth) * aTabStopDistance;
			const auto right = left + aTabStopDistance * 0.6;
			if (right > aRow.right()) {
				break;
			}
			aPainter.drawLine(QPointF{ left, middle }, QPointF{ right, middle });
			aPainter.drawLine(
				QPointF{ right - arm, middle - arm }, QPointF{ right, middle });
			aPainter.drawLine(
				QPointF{ right - arm, middle + arm }, QPointF{ right, middle });
			++painted;
		}
		aPainter.restore();
		return painted;
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
			if (row.Indent > 0U) {
				painted.TabMarkersPainted += paint_tab_markers(
					aPainter,
					QRectF{ textLeft, top, aRect.right() - textLeft, rowHeight },
					row.Indent,
					tabStop,
					role_colour(
						aColors, core::appearance_role::code_line_number_text));
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
