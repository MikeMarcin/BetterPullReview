#include "markdown_code.hpp"

#include "markdown_github.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::qt::detail {
	namespace {
		/// The line an empty suggestion is given so the import keeps a block
		/// for its rows. A private-use character: the noncharacters are the
		/// other reader marks' ([[BPR-1543]] sweeps U+FDE0 to U+FDEF), and
		/// md4c splits a line holding U+FDD0 in two. A body that holds it is
		/// read without the anchor.
		constexpr auto anchor_mark = QChar{ 0xE0E0 };

		constexpr auto header_text = QLatin1StringView{ "Suggested change" };
		/// GitHub's box: 8 px above and below, 6 px rounding, a 12 px header
		/// with 8 px padding, rows with 4 px of padding.
		constexpr auto box_gap       = 8.0;
		constexpr auto box_radius    = 6.0;
		constexpr auto header_pad    = 6.0;
		constexpr auto header_indent = 8.0;
		constexpr auto row_pad       = 2.0;
		/// Between two fenced blocks that would otherwise touch.
		constexpr auto block_gap = 8.0;

		[[nodiscard]] auto contrast(const QColor& aFirst, const QColor& aSecond) -> double
		{
			return contrast_ratio(to_appearance_color(aFirst), to_appearance_color(aSecond));
		}

		[[nodiscard]] auto toward(const QColor& aFrom, const QColor& aTo, const double aAmount) -> QColor
		{
			const auto mix = [aAmount](const float aA, const float aB) { return static_cast<float>(aA + (aB - aA) * aAmount); };
			return QColor::fromRgbF(mix(aFrom.redF(), aTo.redF()), mix(aFrom.greenF(), aTo.greenF()), mix(aFrom.blueF(), aTo.blueF()));
		}

		/// \brief A role's hue moved toward the text until it reads on `aBack`.
		[[nodiscard]] auto readable(const QColor& aHue, const QColor& aInk, const QColor& aBack, const double aTarget) -> QColor
		{
			auto colour = aHue;
			for (auto step = 1; step <= 20 && contrast(colour, aBack) < aTarget; ++step) {
				colour = toward(aHue, aInk, step / 20.0);
			}
			return colour;
		}

		[[nodiscard]] auto role(const code_role_colors& aRoles, const core::appearance_role aRole) -> QColor
		{
			return aRoles[static_cast<std::size_t>(aRole)];
		}

		[[nodiscard]] auto bytes_of(const std::string& aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		[[nodiscard]] auto lines_of(const std::vector<std::string>& aLines) -> std::string
		{
			auto text = std::string{ };
			for (std::size_t index = 0U; index < aLines.size(); ++index) {
				text += index == 0U ? "" : "\n";
				text += aLines[index];
			}
			return text;
		}

		/// \brief The text of one line of a block's classified text, and its spans.
		[[nodiscard]] auto line_formats(const std::string& aLine, const application::shared_token_spans& aSpans, const std::uint32_t aIndex, const code_role_colors& aRoles)
			-> QList<QTextLayout::FormatRange>
		{
			if (!aSpans) {
				return { };
			}
			return code_syntax_format_ranges({ .LineBytes = bytes_of(aLine), .Spans = spans_of_line(*aSpans, aIndex), .Colors = &aRoles });
		}

		[[nodiscard]] auto range_of(const std::string& aLine, const std::size_t aBegin, const std::size_t aEnd) -> std::pair<int, int>
		{
			const auto bytes = bytes_of(aLine);
			return { character_offset(bytes, aBegin), character_offset(bytes, aEnd) };
		}

		/// \brief Where a block's text lines are, in document coordinates.
		[[nodiscard]] auto text_rect(const QTextDocument& aDocument, const QTextBlock& aBlock) -> QRectF
		{
			const auto* layout = aBlock.layout();
			if (layout == nullptr || layout->lineCount() == 0 || !aBlock.isVisible()) {
				return { };
			}
			const auto origin = aDocument.documentLayout()->blockBoundingRect(aBlock).topLeft();
			const auto first  = layout->lineAt(0).rect();
			const auto last   = layout->lineAt(layout->lineCount() - 1).rect();
			return QRectF{ QPointF{ first.left(), first.top() } + origin, QPointF{ std::max(first.right(), last.right()), last.bottom() } + origin };
		}

		[[nodiscard]] auto row_of(const QTextBlock& aBlock) -> markdown_code_row
		{
			return static_cast<markdown_code_row>(aBlock.blockFormat().intProperty(markdown_code_row_property));
		}

		void set_block_format(QTextDocument& aDocument, const QTextBlock& aBlock, const QTextBlockFormat& aFormat)
		{
			if (aBlock.blockFormat() != aFormat) {
				auto cursor = QTextCursor{ &aDocument };
				cursor.setPosition(aBlock.position());
				cursor.setBlockFormat(aFormat);
			}
		}
	} // namespace

	auto markdown_code_colours_of(const markdown_code_theme& aTheme, const QPalette& aPalette) -> markdown_code_colours
	{
		const auto& roles   = aTheme.Colors;
		auto        colours = markdown_code_colours{ };
		colours.Back        = aPalette.color(QPalette::Active, QPalette::Window);
		colours.Normal      = role(roles, core::appearance_role::syntax_normal);
		if (!colours.Normal.isValid() || contrast(colours.Normal, colours.Back) < 4.5) {
			colours.Normal = aPalette.color(QPalette::Active, QPalette::Text);
		}
		const auto high = aTheme.Variant == core::appearance_variant::high_contrast_light || aTheme.Variant == core::appearance_variant::high_contrast_dark;
		colours.Target  = high ? 7.0 : 4.5;
		const auto kind = [&](const core::appearance_role aFill, const core::appearance_role aWord, QColor& aFillOut, QColor& aInk, QColor& aGutter, QColor& aWordOut) {
			aFillOut = role(roles, aFill);
			aWordOut = role(roles, aWord);
			// The word fill's hue, held readable on the line's fill.
			aInk    = readable(aWordOut, colours.Normal, aFillOut, colours.Target);
			aGutter = toward(aFillOut, aWordOut, 0.35);
		};
		kind(core::appearance_role::diff_added_background, core::appearance_role::diff_intraline_added_background, colours.AddedFill, colours.AddedInk, colours.AddedGutter, colours.AddedWord);
		kind(core::appearance_role::diff_removed_background, core::appearance_role::diff_intraline_removed_background, colours.RemovedFill, colours.RemovedInk, colours.RemovedGutter, colours.RemovedWord);
		colours.ChangedFill = role(roles, core::appearance_role::diff_moved_background);
		colours.ChangedInk  = readable(role(roles, core::appearance_role::state_attention), colours.Normal, colours.ChangedFill, colours.Target);
		// The range in the hunk header's ink, or its fill where the ink is
		// meant for that fill and not for code (the high-contrast themes).
		auto range = role(roles, core::appearance_role::diff_hunk_header_text);
		if (contrast(range, colours.Back) < colours.Target) {
			range = role(roles, core::appearance_role::diff_hunk_header_background);
		}
		colours.Range   = readable(range, colours.Normal, colours.Back, colours.Target);
		colours.Header  = readable(role(roles, core::appearance_role::syntax_constant), colours.Normal, colours.Back, colours.Target);
		const auto ink  = aPalette.color(QPalette::Active, QPalette::Text);
		const auto base = aPalette.color(QPalette::Active, QPalette::Base);
		colours.Rule    = toward(ink, base, high ? 0.0 : 0.74);
		colours.Muted   = toward(ink, base, high ? 0.0 : 0.32);
		return colours;
	}

	void markdown_code_store::prepare(QString& aText, const bool aSuggestions)
	{
		if (!aSuggestions || aText.contains(anchor_mark)) {
			return;
		}
		const auto utf8   = aText.toUtf8();
		const auto fences = core::scan_markdown_fenced_code(std::string_view{ utf8.constData(), static_cast<std::size_t>(utf8.size()) });
		const auto empty  = std::ranges::any_of(fences, [](const core::markdown_fenced_code& aFence) {
			return aFence.Lines.empty() && core::markdown_code_language_of(aFence.Info).Kind == core::markdown_code_kind::suggestion;
		});
		if (!empty) {
			return;
		}
		// A top-level empty suggestion is its opening fence followed by its
		// closing one; a mark goes between them.
		const auto leading = [](const QStringView aLine) {
			auto count = qsizetype{ 0 };
			while (count < aLine.size() && aLine[count] == QLatin1Char{ ' ' }) {
				++count;
			}
			return count;
		};
		auto lines = aText.split(QLatin1Char{ '\n' });
		for (qsizetype index = 0; index + 1 < lines.size(); ++index) {
			const auto opening = QStringView{ lines[index] }.trimmed();
			if (leading(lines[index]) > 3 || opening.size() < 3 || (opening[0] != QLatin1Char{ '`' } && opening[0] != QLatin1Char{ '~' })) {
				continue;
			}
			auto count = qsizetype{ 0 };
			while (count < opening.size() && opening[count] == opening[0]) {
				++count;
			}
			const auto info = opening.mid(count).trimmed();
			if (count < 3 || core::markdown_code_language_of(info.toUtf8().toStdString()).Kind != core::markdown_code_kind::suggestion) {
				continue;
			}
			const auto closing = QStringView{ lines[index + 1] }.trimmed();
			auto       run     = qsizetype{ 0 };
			while (run < closing.size() && closing[run] == opening[0]) {
				++run;
			}
			if (leading(lines[index + 1]) <= 3 && run >= count && closing.mid(run).trimmed().isEmpty()) {
				lines.insert(index + 1, QString{ anchor_mark });
				++index;
			}
		}
		aText = lines.join(QLatin1Char{ '\n' });
	}

	void markdown_code_store::clear()
	{
		Blocks.clear();
		Inline.clear();
	}

	void markdown_code_store::set_inline_selector(std::string aSelector)
	{
		InlineSelector = std::move(aSelector);
		if (InlineSelector.empty()) {
			Inline.clear();
		}
	}

	namespace {
		/// \brief Inline code as the import marks it: fixed pitch, not a
		/// keyboard key, outside fenced code and a suggestion's rows.
		[[nodiscard]] auto inline_code_block(const QTextBlock& aBlock) -> bool
		{
			const auto format = aBlock.blockFormat();
			return !format.hasProperty(QTextFormat::BlockCodeFence) && !format.hasProperty(QTextFormat::BlockCodeLanguage) && !format.hasProperty(markdown_code_ordinal_property)
				&& !format.hasProperty(markdown_code_row_property);
		}

		[[nodiscard]] auto inline_code_chars(const QTextCharFormat& aFormat) -> bool
		{
			return aFormat.fontFixedPitch() && !aFormat.boolProperty(markdown_keyboard_property);
		}

		/// \brief Each run of inline code in a block: where it starts in the
		/// block, and its text.
		template <typename Visit>
		void for_each_inline_run(const QTextBlock& aBlock, Visit&& aVisit)
		{
			auto start = -1;
			auto text  = QString{ };
			for (auto it = aBlock.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				if (inline_code_chars(fragment.charFormat())) {
					if (start < 0) {
						start = fragment.position() - aBlock.position();
					}
					text += fragment.text();
					continue;
				}
				if (start >= 0) {
					aVisit(start, text);
					start = -1;
					text.clear();
				}
			}
			if (start >= 0) {
				aVisit(start, text);
			}
		}
	} // namespace

	void markdown_code_store::place_inline(const QTextDocument& aDocument)
	{
		Inline.clear();
		if (InlineSelector.empty()) {
			return;
		}
		for (auto current = aDocument.begin(); current.isValid(); current = current.next()) {
			if (!inline_code_block(current)) {
				continue;
			}
			for_each_inline_run(current, [this](int /*aStart*/, const QString& aText) {
				auto utf8 = aText.toUtf8().toStdString();
				if (utf8.empty() || utf8.find('\n') != std::string::npos || std::ranges::any_of(Inline, [&utf8](const inline_code& aCode) { return *aCode.Text == utf8; })) {
					return;
				}
				Inline.push_back({ .Text = std::make_shared<const std::string>(std::move(utf8)), .Spans = { } });
			});
		}
	}

	void markdown_code_store::style_inline(QTextDocument& aDocument, const code_role_colors& aRoles) const
	{
		if (Inline.empty()) {
			return;
		}
		for (auto current = aDocument.begin(); current.isValid(); current = current.next()) {
			if (!inline_code_block(current)) {
				continue;
			}
			auto formats = QList<QTextLayout::FormatRange>{ };
			for_each_inline_run(current, [&](const int aStart, const QString& aText) {
				const auto utf8  = aText.toUtf8().toStdString();
				const auto found = std::ranges::find_if(Inline, [&utf8](const inline_code& aCode) { return *aCode.Text == utf8; });
				if (found == Inline.end() || !found->Spans) {
					return;
				}
				for (auto range : line_formats(utf8, found->Spans, 0U, aRoles)) {
					range.start += aStart;
					formats.push_back(range);
				}
			});
			if (!formats.isEmpty() || (current.layout() != nullptr && !current.layout()->formats().isEmpty())) {
				apply_syntax_format_ranges(aDocument, current, formats);
			}
		}
	}

	auto markdown_code_store::suggestions() const noexcept -> bool
	{
		return std::ranges::any_of(Blocks, &block::Suggestion);
	}

	void markdown_code_store::place(QTextDocument& aDocument, const QString& aPrepared, const std::optional<markdown_suggestion_presentation>& aSuggestions)
	{
		Blocks.clear();
		const auto utf8   = aPrepared.toUtf8();
		auto       fences = core::scan_markdown_fenced_code(std::string_view{ utf8.constData(), static_cast<std::size_t>(utf8.size()) });
		auto       code   = std::vector<QTextBlock>{ };
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) {
				code.push_back(block);
			}
		}
		const auto anchor  = QString{ anchor_mark }.toStdString();
		auto       at      = std::size_t{ 0U };
		auto       aligned = true;
		for (auto& fence : fences) {
			auto entry     = block{ };
			entry.Language = core::markdown_code_language_of(fence.Info);
			const auto n   = fence.Lines.size();
			// The import keeps one block per line; a block placed anywhere but
			// exactly here would colour another block's lines, so the first
			// difference leaves this block and every later one plain.
			if (aligned && at + n <= code.size()
				&& std::ranges::equal(fence.Lines, std::span{ code }.subspan(at, n), [](const std::string& aLine, const QTextBlock& aBlock) {
					   return aBlock.text() == QString::fromStdString(aLine);
				   })) {
				for (std::size_t index = 0U; index < n; ++index) {
					auto format = code[at + index].blockFormat();
					format.setProperty(markdown_code_ordinal_property, static_cast<int>(fence.Ordinal));
					format.setProperty(markdown_code_line_property, static_cast<int>(index));
					set_block_format(aDocument, code[at + index], format);
				}
				entry.Placed = n > 0U;
				at += n;
			} else {
				aligned = false;
			}
			if (fence.Lines.size() == 1U && fence.Lines[0] == anchor) {
				fence.Lines.clear();
			}
			entry.Fence = std::move(fence);
			if (entry.Placed && entry.Language.Kind == core::markdown_code_kind::language) {
				entry.Selector = std::string{ entry.Language.Selector };
				entry.Text     = std::make_shared<const std::string>(entry.Fence.text());
			}
			if (entry.Placed && entry.Language.Kind == core::markdown_code_kind::suggestion && aSuggestions) {
				entry.Suggestion = true;
				entry.Selector   = aSuggestions->Path;
				entry.Numbered   = aSuggestions->Lines.has_value();
				entry.Rows       = core::markdown_suggestion_rows(entry.Fence.Lines, aSuggestions->Lines);
				if (!entry.Fence.Lines.empty()) {
					entry.Text = std::make_shared<const std::string>(entry.Fence.text());
				}
				if (aSuggestions->Lines && !aSuggestions->Lines->Lines.empty()) {
					entry.Removed = std::make_shared<const std::string>(lines_of(aSuggestions->Lines->Lines));
				}
			}
			Blocks.push_back(std::move(entry));
		}
		// An anchor whose block was not placed is an empty line, never a mark.
		if (aPrepared.contains(anchor_mark)) {
			for (auto current = aDocument.begin(); current.isValid(); current = current.next()) {
				if (current.text().contains(anchor_mark) && !(current.blockFormat().hasProperty(markdown_code_ordinal_property) && Blocks[static_cast<std::size_t>(current.blockFormat().intProperty(markdown_code_ordinal_property))].Suggestion)) {
					auto cursor = QTextCursor{ current };
					cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
					cursor.insertText(cursor.selectedText().remove(anchor_mark));
				}
			}
		}
		// Each suggestion's header and removed rows, from the last so the
		// earlier ones are found where they were.
		for (auto it = Blocks.rbegin(); it != Blocks.rend(); ++it) {
			if (!it->Suggestion) {
				continue;
			}
			const auto ordinal = static_cast<int>(it->Fence.Ordinal);
			auto       first   = aDocument.begin();
			while (first.isValid() && !(first.blockFormat().hasProperty(markdown_code_ordinal_property) && first.blockFormat().intProperty(markdown_code_ordinal_property) == ordinal)) {
				first = first.next();
			}
			if (!first.isValid()) {
				it->Suggestion = false;
				continue;
			}
			const auto rowFormat = first.blockFormat();
			const auto rowChars  = first.begin().atEnd() ? first.charFormat() : first.begin().fragment().charFormat();
			auto       cursor    = QTextCursor{ &aDocument };
			cursor.setPosition(first.position());
			if (it->Fence.Lines.empty()) {
				// The anchor's line becomes the header.
				cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
				cursor.insertText(header_text, QTextCharFormat{ });
			} else {
				cursor.insertText(header_text, QTextCharFormat{ });
				cursor.insertBlock(rowFormat, rowChars);
			}
			auto header = aDocument.findBlock(first.position());
			auto format = header.blockFormat();
			format.clearProperty(QTextFormat::BlockCodeFence);
			format.clearProperty(QTextFormat::BlockCodeLanguage);
			format.clearProperty(markdown_code_line_property);
			format.setNonBreakableLines(false);
			format.setProperty(markdown_code_row_property, static_cast<int>(markdown_code_row::header));
			set_block_format(aDocument, header, format);
			// The added rows are the block's own lines.
			for (auto added = header.next(); added.isValid() && added.blockFormat().intProperty(markdown_code_ordinal_property) == ordinal
				 && added.blockFormat().hasProperty(markdown_code_ordinal_property);
				 added = added.next()) {
				auto rowAdded = added.blockFormat();
				rowAdded.setProperty(markdown_code_row_property, static_cast<int>(markdown_code_row::added));
				const auto  line = static_cast<std::size_t>(rowAdded.intProperty(markdown_code_line_property));
				const auto  rows = std::ranges::count_if(it->Rows, [](const core::markdown_suggestion_row& aRow) { return !aRow.Added; });
				const auto& row  = it->Rows[static_cast<std::size_t>(rows) + line];
				if (row.Number) {
					rowAdded.setProperty(markdown_code_number_property, static_cast<int>(*row.Number));
				}
				set_block_format(aDocument, added, rowAdded);
			}
			// The commented lines, after the header.
			cursor.setPosition(header.position());
			cursor.movePosition(QTextCursor::EndOfBlock);
			auto line = 0;
			for (const auto& row : it->Rows) {
				if (row.Added) {
					break;
				}
				auto removed = rowFormat;
				removed.setProperty(markdown_code_row_property, static_cast<int>(markdown_code_row::removed));
				removed.setProperty(markdown_code_line_property, line++);
				if (row.Number) {
					removed.setProperty(markdown_code_number_property, static_cast<int>(*row.Number));
				}
				cursor.insertBlock(removed, rowChars);
				cursor.insertText(QString::fromStdString(row.Text), rowChars);
			}
		}
		place_inline(aDocument);
	}

	auto markdown_code_store::requests() const -> std::vector<application::markdown_code_syntax_request>
	{
		auto       requests = std::vector<application::markdown_code_syntax_request>{ };
		const auto add      = [&requests](const std::string& aSelector, const std::shared_ptr<const std::string>& aText) {
			if (aSelector.empty() || !aText) {
				return;
			}
			if (std::ranges::none_of(requests, [&](const application::markdown_code_syntax_request& aRequest) { return aRequest.Selector == aSelector && *aRequest.Text == *aText; })) {
				requests.push_back({ .Selector = aSelector, .Text = aText });
			}
		};
		for (const auto& entry : Blocks) {
			if (entry.Placed && (entry.Language.Kind == core::markdown_code_kind::language || entry.Suggestion)) {
				add(entry.Selector, entry.Text);
				add(entry.Selector, entry.Removed);
			}
		}
		for (const auto& code : Inline) {
			add(InlineSelector, code.Text);
		}
		return requests;
	}

	auto markdown_code_store::apply(const application::markdown_code_syntax_publication& aSpans) -> bool
	{
		if (!aSpans.Text || !aSpans.Spans) {
			return false;
		}
		auto taken = false;
		for (auto& entry : Blocks) {
			if (entry.Selector != aSpans.Selector) {
				continue;
			}
			if (entry.Text && *entry.Text == *aSpans.Text && entry.Spans != aSpans.Spans) {
				entry.Spans = aSpans.Spans;
				taken       = true;
			}
			if (entry.Removed && *entry.Removed == *aSpans.Text && entry.RemovedSpans != aSpans.Spans) {
				entry.RemovedSpans = aSpans.Spans;
				taken              = true;
			}
		}
		if (aSpans.Selector == InlineSelector) {
			for (auto& code : Inline) {
				if (*code.Text == *aSpans.Text && code.Spans != aSpans.Spans) {
					code.Spans = aSpans.Spans;
					taken      = true;
				}
			}
		}
		return taken;
	}

	namespace {
		/// \brief GitHub's diff grammar marks of one line, in the theme's colours.
		[[nodiscard]] auto diff_formats(const std::string& aLine, const markdown_code_colours& aColours) -> QList<QTextLayout::FormatRange>
		{
			auto formats = QList<QTextLayout::FormatRange>{ };
			for (const auto& mark : core::markdown_diff_marks(aLine)) {
				const auto [from, to] = range_of(aLine, mark.Begin, mark.Begin + mark.Length);
				auto range            = QTextLayout::FormatRange{ };
				range.start           = from;
				range.length          = to - from;
				switch (mark.Kind) {
				case core::markdown_diff_mark_kind::inserted:
					range.format.setBackground(aColours.AddedFill);
					range.format.setForeground(aColours.AddedInk);
					break;
				case core::markdown_diff_mark_kind::deleted:
					range.format.setBackground(aColours.RemovedFill);
					range.format.setForeground(aColours.RemovedInk);
					break;
				case core::markdown_diff_mark_kind::changed:
					range.format.setBackground(aColours.ChangedFill);
					range.format.setForeground(aColours.ChangedInk);
					break;
				case core::markdown_diff_mark_kind::range:
					range.format.setForeground(aColours.Range);
					range.format.setFontWeight(QFont::Bold);
					break;
				case core::markdown_diff_mark_kind::header:
					range.format.setForeground(aColours.Header);
					break;
				case core::markdown_diff_mark_kind::separator:
					range.format.setForeground(aColours.Header);
					range.format.setFontWeight(QFont::Bold);
					break;
				}
				if (range.length > 0) {
					formats.push_back(range);
				}
			}
			return formats;
		}

		/// \brief A suggestion row's syntax colours, and its changed words on
		/// the intraline fill with every glyph's ink held readable on it.
		[[nodiscard]] auto row_formats(const core::markdown_suggestion_row& aRow, const application::shared_token_spans& aSpans, const std::uint32_t aLine,
									   const code_role_colors& aRoles, const markdown_code_colours& aColours, const core::appearance_variant aVariant)
			-> QList<QTextLayout::FormatRange>
		{
			auto formats = line_formats(aRow.Text, aSpans, aLine, aRoles);
			if (aRow.ChangedEnd <= aRow.ChangedBegin) {
				return formats;
			}
			const auto fill     = aRow.Added ? aColours.AddedWord : aColours.RemovedWord;
			const auto readable = [&](const QColor& aGlyph) {
				return to_presentation_color(core::readable_emphasis_foreground(to_appearance_color(aGlyph), to_appearance_color(fill), aVariant));
			};
			const auto [from, to] = range_of(aRow.Text, aRow.ChangedBegin, aRow.ChangedEnd);
			const auto syntax     = formats;
			auto       word       = QTextLayout::FormatRange{ };
			word.start            = from;
			word.length           = to - from;
			word.format.setBackground(fill);
			word.format.setForeground(readable(aColours.Normal));
			formats.push_back(word);
			// A classified token keeps its hue, held readable on the mark.
			for (const auto& token : syntax) {
				const auto begin = std::max(token.start, from);
				const auto end   = std::min(token.start + token.length, to);
				if (begin < end) {
					auto inked   = QTextLayout::FormatRange{ };
					inked.start  = begin;
					inked.length = end - begin;
					inked.format.setForeground(readable(token.format.foreground().color()));
					formats.push_back(inked);
				}
			}
			return formats;
		}
	} // namespace

	void markdown_code_store::style(QTextDocument& aDocument, const markdown_code_colours& aColours, const code_role_colors& aRoles, const core::appearance_variant aVariant, const QFont& aFont, const QFont& aCode)
	{
		Colours  = aColours;
		CodeFont = aCode;
		// Qt lays the first block of a document at its top whatever its top
		// margin, so a body that opens with a suggestion keeps the box's gap
		// on its root frame instead.
		{
			auto       frame = aDocument.rootFrame()->frameFormat();
			const auto opens = row_of(aDocument.begin()) == markdown_code_row::header;
			const auto top   = opens ? aDocument.documentMargin() + box_gap + header_pad : aDocument.documentMargin();
			if (frame.topMargin() != top) {
				frame.setTopMargin(top);
				aDocument.rootFrame()->setFrameFormat(frame);
			}
		}
		style_inline(aDocument, aRoles);
		if (Blocks.empty()) {
			return;
		}
		const auto digit  = QFontMetricsF{ aCode }.horizontalAdvance(QLatin1Char{ '0' });
		auto       header = QTextCharFormat{ };
		header.setForeground(aColours.Muted);
		header.setFontFamilies(aFont.families());
		header.setFontFixedPitch(false);
		if (aFont.pointSizeF() > 0.0) {
			header.setFontPointSize(aFont.pointSizeF() * 0.86);
		}
		for (auto current = aDocument.begin(); current.isValid(); current = current.next()) {
			auto format = current.blockFormat();
			if (!format.hasProperty(markdown_code_ordinal_property)) {
				continue;
			}
			const auto ordinal = static_cast<std::size_t>(format.intProperty(markdown_code_ordinal_property));
			if (ordinal >= Blocks.size()) {
				continue;
			}
			const auto& entry = Blocks[ordinal];
			const auto  index = static_cast<std::uint32_t>(format.intProperty(markdown_code_line_property));
			if (!entry.Suggestion) {
				if (index == 0U && current.previous().isValid() && current.previous().blockFormat().hasProperty(QTextFormat::BlockCodeFence) && format.topMargin() != block_gap) {
					// Two fenced blocks in a row read as two, as GitHub draws them.
					format.setTopMargin(block_gap);
					set_block_format(aDocument, current, format);
				}
				const auto shown = index < entry.Fence.Lines.size();
				apply_syntax_format_ranges(aDocument, current,
										   !shown                                                          ? QList<QTextLayout::FormatRange>{ }
											   : entry.Language.Kind == core::markdown_code_kind::language ? line_formats(entry.Fence.Lines[index], entry.Spans, index, aRoles)
											   : entry.Language.Kind == core::markdown_code_kind::diff     ? diff_formats(entry.Fence.Lines[index], aColours)
																										   : QList<QTextLayout::FormatRange>{ });
				continue;
			}
			const auto row = row_of(current);
			if (row == markdown_code_row::header) {
				format.setLeftMargin(header_indent);
				format.setTopMargin(box_gap + header_pad);
				format.setBottomMargin(header_pad);
				format.setLineHeight(100.0, QTextBlockFormat::ProportionalHeight);
				format.clearBackground();
				set_block_format(aDocument, current, format);
				if (current.begin().atEnd() || current.begin().fragment().charFormat() != header) {
					auto cursor = QTextCursor{ current };
					cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
					cursor.setCharFormat(header);
				}
				continue;
			}
			// The gutter: the numbers' column, wide enough for the last one,
			// then the sign's.
			const auto last        = entry.Rows.empty() || !entry.Rows.back().Number ? 0U : *entry.Rows.back().Number;
			const auto numberWidth = entry.Numbered ? std::ceil(digit * static_cast<qreal>(std::max<std::size_t>(2U, std::to_string(last).size())) + 16.0) : 0.0;
			const auto gutter      = numberWidth + std::ceil(digit + 12.0);
			const auto next        = current.next();
			const auto final       = !next.isValid() || !next.blockFormat().hasProperty(markdown_code_ordinal_property)
				|| next.blockFormat().intProperty(markdown_code_ordinal_property) != static_cast<int>(ordinal);
			format.setLeftMargin(gutter);
			format.setTopMargin(row_pad);
			format.setBottomMargin(final ? row_pad + box_gap : row_pad);
			format.setLineHeight(100.0, QTextBlockFormat::ProportionalHeight);
			format.clearBackground();
			format.setProperty(markdown_code_gutter_property, gutter);
			format.setProperty(markdown_code_numbers_property, numberWidth);
			set_block_format(aDocument, current, format);
			// The reader gives code a background of its own; a row's fill is
			// painted under the whole row instead.
			if (!current.begin().atEnd() && current.begin().fragment().charFormat().background().style() != Qt::NoBrush) {
				auto cursor = QTextCursor{ current };
				cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
				auto clear = QTextCharFormat{ };
				clear.setBackground(Qt::NoBrush);
				cursor.mergeCharFormat(clear);
			}
			const auto removed = row == markdown_code_row::removed;
			const auto before  = static_cast<std::size_t>(std::ranges::count_if(entry.Rows, [](const core::markdown_suggestion_row& aRow) { return !aRow.Added; }));
			const auto at      = removed ? static_cast<std::size_t>(index) : before + index;
			apply_syntax_format_ranges(aDocument, current,
									   at < entry.Rows.size() ? row_formats(entry.Rows[at], removed ? entry.RemovedSpans : entry.Spans, index, aRoles, aColours, aVariant)
															  : QList<QTextLayout::FormatRange>{ });
		}
	}

	auto markdown_code_store::boxes(const QTextDocument& aDocument) const -> std::vector<box>
	{
		auto result = std::vector<box>{ };
		if (!suggestions()) {
			return result;
		}
		const auto right = aDocument.size().width() - aDocument.documentMargin();
		for (auto current = aDocument.begin(); current.isValid(); current = current.next()) {
			const auto format = current.blockFormat();
			const auto row    = row_of(current);
			if (row == markdown_code_row::none || !current.isVisible()) {
				continue;
			}
			const auto ordinal = format.intProperty(markdown_code_ordinal_property);
			const auto rect    = text_rect(aDocument, current);
			if (rect.isNull()) {
				continue;
			}
			if (row == markdown_code_row::header) {
				auto opened    = box{ };
				opened.Ordinal = ordinal;
				opened.Left    = rect.left() - header_indent;
				opened.Right   = std::max(right, opened.Left + 1.0);
				opened.Top     = rect.top() - header_pad;
				opened.Rule    = rect.bottom() + header_pad;
				opened.Bottom  = opened.Rule;
				result.push_back(opened);
				continue;
			}
			if (result.empty() || result.back().Ordinal != ordinal) {
				continue;
			}
			auto& open   = result.back();
			open.Gutter  = format.doubleProperty(markdown_code_gutter_property);
			open.Numbers = format.doubleProperty(markdown_code_numbers_property);
			open.Bottom  = std::max(open.Bottom, rect.bottom() + row_pad);
		}
		return result;
	}

	void markdown_code_store::paint_under(QPainter& aPainter, const QTextDocument& aDocument, const QPointF aOffset) const
	{
		const auto all = boxes(aDocument);
		if (all.empty()) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		for (const auto& open : all) {
			auto clip = QPainterPath{ };
			clip.addRoundedRect(QRectF{ open.Left, open.Top, open.Right - open.Left, open.Bottom - open.Top }.translated(aOffset), box_radius, box_radius);
			aPainter.setClipPath(clip);
			for (auto current = aDocument.begin(); current.isValid(); current = current.next()) {
				const auto row = row_of(current);
				if ((row != markdown_code_row::removed && row != markdown_code_row::added) || current.blockFormat().intProperty(markdown_code_ordinal_property) != open.Ordinal) {
					continue;
				}
				const auto rect = text_rect(aDocument, current);
				if (rect.isNull()) {
					continue;
				}
				const auto added = row == markdown_code_row::added;
				const auto band  = QRectF{ open.Left, rect.top() - row_pad, open.Right - open.Left, rect.height() + 2.0 * row_pad }.translated(aOffset);
				aPainter.fillRect(band, added ? Colours.AddedFill : Colours.RemovedFill);
				if (open.Numbers > 0.0) {
					aPainter.fillRect(QRectF{ band.left(), band.top(), open.Numbers, band.height() }, added ? Colours.AddedGutter : Colours.RemovedGutter);
				}
			}
			aPainter.setClipping(false);
		}
		aPainter.restore();
	}

	void markdown_code_store::paint_over(QPainter& aPainter, const QTextDocument& aDocument, const QPointF aOffset) const
	{
		const auto all = boxes(aDocument);
		if (all.empty()) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		auto numberFont = CodeFont;
		if (numberFont.pointSizeF() > 0.0) {
			numberFont.setPointSizeF(numberFont.pointSizeF() * 0.92);
		}
		for (const auto& open : all) {
			for (auto current = aDocument.begin(); current.isValid(); current = current.next()) {
				const auto row = row_of(current);
				if ((row != markdown_code_row::removed && row != markdown_code_row::added) || current.blockFormat().intProperty(markdown_code_ordinal_property) != open.Ordinal) {
					continue;
				}
				const auto rect = text_rect(aDocument, current).translated(aOffset);
				if (rect.isNull()) {
					continue;
				}
				const auto added = row == markdown_code_row::added;
				if (open.Numbers > 0.0 && current.blockFormat().hasProperty(markdown_code_number_property)) {
					aPainter.setFont(numberFont);
					aPainter.setPen(Colours.Normal);
					aPainter.drawText(QRectF{ open.Left + aOffset.x(), rect.top(), open.Numbers - 8.0, rect.height() }, Qt::AlignRight | Qt::AlignVCenter,
									  QString::number(current.blockFormat().intProperty(markdown_code_number_property)));
				}
				aPainter.setFont(CodeFont);
				aPainter.setPen(Colours.Normal);
				aPainter.drawText(QRectF{ open.Left + aOffset.x() + open.Numbers + 6.0, rect.top(), open.Gutter - open.Numbers - 6.0, rect.height() }, Qt::AlignLeft | Qt::AlignVCenter,
								  added ? QStringLiteral("+") : QStringLiteral("-"));
			}
			const auto outline = QRectF{ open.Left + 0.5, open.Top + 0.5, open.Right - open.Left - 1.0, open.Bottom - open.Top - 1.0 }.translated(aOffset);
			aPainter.setPen(QPen{ Colours.Rule, 1.0 });
			aPainter.drawLine(QPointF{ outline.left(), open.Rule + aOffset.y() + 0.5 }, QPointF{ outline.right(), open.Rule + aOffset.y() + 0.5 });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawRoundedRect(outline, box_radius, box_radius);
		}
		aPainter.restore();
	}
} // namespace bpr::qt::detail
