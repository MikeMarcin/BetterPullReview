#include "bpr/qt/line_details_pane.hpp"

#include "bpr/qt/code_sample_painter.hpp"

#include "source_display_text.hpp"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QLocale>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace bpr::qt {
	namespace {
		using core::line_details_row;
		using core::line_details_row_kind;

		/// The gutter's sign for a row: what the row is, in diff notation.
		[[nodiscard]] auto row_mark(const line_details_row& aRow) -> QString
		{
			switch (aRow.Kind) {
			case line_details_row_kind::changed:
				return aRow.Side == core::diff_side::left ? QStringLiteral("−") : QStringLiteral("+");
			case line_details_row_kind::removed:
				return QStringLiteral("−");
			case line_details_row_kind::added:
				return QStringLiteral("+");
			case line_details_row_kind::unchanged:
			case line_details_row_kind::absent:
				break;
			}
			return { };
		}

		[[nodiscard]] auto side_name(const core::diff_side aSide) -> QString
		{
			return aSide == core::diff_side::left ? QStringLiteral("base") : QStringLiteral("head");
		}

		[[nodiscard]] auto lines_text(const core::line_span aLines) -> QString
		{
			return aLines.End - aLines.Begin == 1U
				? QStringLiteral("line %1").arg(aLines.Begin + 1U)
				: QStringLiteral("lines %1–%2").arg(aLines.Begin + 1U).arg(aLines.End);
		}

		/// What an absent row says: that its side has no version of the line,
		/// and, for an unpaired line in a replacement, which lines it replaces.
		[[nodiscard]] auto absent_text(const line_details_row& aRow) -> QString
		{
			if (aRow.Replaces.has_value() && !aRow.Replaces->empty()) {
				return QStringLiteral("No matching line; replaces %1 %2")
					.arg(side_name(aRow.Side), lines_text(*aRow.Replaces));
			}
			return QStringLiteral("Not in %1").arg(side_name(aRow.Side));
		}

		[[nodiscard]] auto kind_text(const line_details_row_kind aKind) -> QString
		{
			switch (aKind) {
			case line_details_row_kind::unchanged:
				return QStringLiteral("unchanged");
			case line_details_row_kind::changed:
				return QStringLiteral("changed");
			case line_details_row_kind::removed:
				return QStringLiteral("removed");
			case line_details_row_kind::added:
				return QStringLiteral("added");
			case line_details_row_kind::absent:
				break;
			}
			return QStringLiteral("absent");
		}

		[[nodiscard]] auto row_bytes(const line_details_row& aRow, const core::computed_file_diff& aDiff)
			-> std::span<const std::uint8_t>
		{
			const auto& document = aRow.Side == core::diff_side::left ? aDiff.LeftDocument : aDiff.RightDocument;
			if (!document.has_value() || aRow.Content.End < aRow.Content.Begin || aRow.Content.End > document->Bytes.size()) {
				return { };
			}
			return { document->Bytes.data() + aRow.Content.Begin,
					 static_cast<std::size_t>(aRow.Content.End - aRow.Content.Begin) };
		}
	} // namespace

	class line_details_pane::gutter final : public QWidget
	{
	public:
		explicit gutter(line_details_pane& aPane)
			: QWidget{ &aPane }
			, Pane{ aPane }
		{
			setObjectName(QStringLiteral("lineDetailsGutter"));
		}

	protected:
		void paintEvent(QPaintEvent* const aEvent) override
		{
			Pane.paint_gutter(aEvent);
		}

	private:
		line_details_pane& Pane;
	};

	line_details_pane::line_details_pane(QWidget* const aParent)
		: QPlainTextEdit{ aParent }
		, Gutter{ new gutter{ *this } }
	{
		setObjectName(QStringLiteral("diffSelectedChangeDetail"));
		setAccessibleName(QStringLiteral("Line details"));
		setAccessibleDescription(QStringLiteral("The current line's base and head versions in full, base above head"));
		setReadOnly(true);
		setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		// The owner, on v001: "Turn off word wrap, let it have a horizontal
		// scroll if the line is too long." Every row is one line of one
		// document, so the rows share the pane's one horizontal bar and a
		// column stays a column; the gutter sits outside the viewport and
		// does not move with it.
		setLineWrapMode(QPlainTextEdit::NoWrap);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		setPlaceholderText(QStringLiteral("Select a line to see its base and head versions in full."));
		connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& aRect, const int aDy) {
			if (aDy != 0) {
				Gutter->scroll(0, aDy);
			} else {
				Gutter->update(0, aRect.y(), Gutter->width(), aRect.height());
			}
		});
		// A bar's own style can change under the pane: macOS switches
		// between overlay and legacy bars when the system setting changes.
		horizontalScrollBar()->installEventFilter(this);
		place_gutter();
	}

	auto line_details_pane::projection() const noexcept -> const core::line_details_projection&
	{
		return Shown;
	}

	auto line_details_pane::presentations() const noexcept -> std::uint64_t
	{
		return Presentations;
	}

	auto line_details_pane::gutter_width() const -> int
	{
		auto widest = std::uint32_t{ 99U };
		for (const auto& row : Shown.Rows) {
			if (row.Line.has_value()) {
				widest = std::max(widest, *row.Line + 1U);
			}
		}
		const auto digits = QString::number(widest).size();
		const auto glyph  = fontMetrics().horizontalAdvance(QLatin1Char('9'));
		// Number, a gap, the sign, and a margin before the code.
		return glyph * (static_cast<int>(digits) + 3) + 8;
	}

	auto line_details_pane::gutter_widget() const noexcept -> QWidget*
	{
		return Gutter;
	}

	auto line_details_pane::gutter_text(const int aBlock) const -> QString
	{
		if (Empty || aBlock < 0 || static_cast<std::size_t>(aBlock) >= RowBlocks) {
			return { };
		}
		const auto& row    = Shown.Rows[static_cast<std::size_t>(aBlock)];
		const auto  number = row.Line.has_value() ? QString::number(*row.Line + 1U) : QString{ };
		const auto  mark   = row_mark(row);
		return mark.isEmpty() ? number : number + QLatin1Char(' ') + mark;
	}

	void line_details_pane::present_empty()
	{
		const auto wasEmpty = Empty && document()->isEmpty();
		Shown               = { };
		ShownDiff           = nullptr;
		RowBlocks           = 0U;
		Emphasis.clear();
		RowBytes.clear();
		Empty = true;
		if (!wasEmpty) {
			++Presentations;
			clear();
			setExtraSelections({ });
		}
		setAccessibleDescription(QStringLiteral("No line selected"));
		place_gutter();
	}

	void line_details_pane::present(const core::line_details_projection& aProjection,
									const core::computed_file_diff&      aDiff)
	{
		if (aProjection.Rows.empty()) {
			present_empty();
			return;
		}
		if (!Empty && ShownDiff == &aDiff && Shown == aProjection) {
			return;
		}
		++Presentations;
		Shown     = aProjection;
		ShownDiff = &aDiff;
		Empty     = false;
		RowBlocks = Shown.Rows.size();
		Emphasis.clear();
		RowBytes.assign(Shown.Rows.size(), QByteArray{ });

		auto note = QTextCharFormat{ };
		note.setForeground(palette().color(QPalette::PlaceholderText));
		note.setFontItalic(true);
		const auto plain  = QTextCharFormat{ };
		const auto locale = QLocale{ };

		// One edit block: the document lays out once, not once per row.
		auto* const target = document();
		target->clear();
		auto cursor = QTextCursor{ target };
		cursor.beginEditBlock();
		auto description = QStringList{ };
		for (std::size_t index = 0U; index < Shown.Rows.size(); ++index) {
			const auto& row = Shown.Rows[index];
			if (index > 0U) {
				cursor.insertBlock();
			}
			const auto block = static_cast<int>(index);
			if (row.Kind == line_details_row_kind::absent) {
				cursor.insertText(absent_text(row), note);
				description.push_back(absent_text(row));
				continue;
			}
			const auto bytes = row_bytes(row, aDiff);
			RowBytes[index]  = QByteArray{ reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()) };
			cursor.insertText(
				source_display_text(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())), plain);
			for (const auto& span : row.Emphasis) {
				const auto from = character_offset(bytes, span.Begin);
				const auto to   = character_offset(bytes, span.End);
				if (from < to) {
					Emphasis.push_back({ .Block = block, .From = from, .To = to });
				}
			}
			if (row.OmittedBytes > 0U) {
				cursor.insertText(
					QStringLiteral(" … %1 more bytes").arg(locale.toString(static_cast<qulonglong>(row.OmittedBytes))), note);
			}
			description.push_back(QStringLiteral("%1 line %2, %3")
									  .arg(side_name(row.Side))
									  .arg(row.Line.value_or(0U) + 1U)
									  .arg(kind_text(row.Kind)));
		}
		if (Shown.OmittedLines > 0U) {
			cursor.insertBlock();
			cursor.insertText(Shown.OmittedLines == 1U
								  ? QStringLiteral("1 more selected line")
								  : QStringLiteral("%1 more selected lines").arg(Shown.OmittedLines),
							  note);
		}
		cursor.endEditBlock();
		setAccessibleDescription(description.join(QStringLiteral("; ")));
		moveCursor(QTextCursor::Start);
		verticalScrollBar()->setValue(0);
		place_gutter();
		apply_syntax();
		repaint_bands();
		reveal_first_change();
	}

	auto line_details_pane::first_change_x() const -> std::optional<int>
	{
		if (Empty) {
			return std::nullopt;
		}
		auto        first    = std::optional<qreal>{ };
		const auto  consider = [&first](const qreal aX) { first = first.has_value() ? std::min(*first, aX) : aX; };
		auto* const layout   = document()->documentLayout();
		// The x of one character of one row. Asking for the block's rectangle
		// lays the row out if it is not yet, which gives its characters a place.
		const auto characterX = [this, layout](const int aBlock, const int aCharacter) -> std::optional<qreal> {
			const auto block = document()->findBlockByNumber(aBlock);
			if (!block.isValid() || layout->blockBoundingRect(block).isNull() || block.layout()->lineCount() == 0) {
				return std::nullopt;
			}
			return block.layout()->lineAt(0).cursorToX(aCharacter);
		};
		for (std::size_t index = 0U; index < RowBlocks; ++index) {
			// A whole added or removed row is the change from its first column.
			const auto kind = Shown.Rows[index].Kind;
			if (kind == line_details_row_kind::added || kind == line_details_row_kind::removed) {
				if (const auto x = characterX(static_cast<int>(index), 0)) {
					consider(*x);
				}
			}
		}
		for (const auto& span : Emphasis) {
			if (const auto x = characterX(span.Block, span.From)) {
				consider(*x);
			}
		}
		if (!first.has_value()) {
			return std::nullopt;
		}
		return static_cast<int>(std::floor(*first));
	}

	void line_details_pane::reveal_first_change()
	{
		auto* const bar   = horizontalScrollBar();
		const auto  first = first_change_x();
		// A change that starts in the first screenful is read from the line's
		// start. One further right is brought in with a little of the text
		// before it, so it reads in context.
		const auto glyph   = fontMetrics().horizontalAdvance(QLatin1Char('9'));
		const auto context = std::min(viewport()->width() / 4, glyph * 12);
		if (!first.has_value() || *first + glyph * 12 <= viewport()->width()) {
			bar->setValue(0);
			return;
		}
		bar->setValue(std::max(0, *first - context));
	}

	void line_details_pane::set_colors(const line_details_colors& aColors)
	{
		if (Colors == aColors) {
			return;
		}
		Colors = aColors;
		apply_syntax();
		repaint_bands();
		Gutter->update();
	}

	void line_details_pane::set_token_spans(application::shared_token_spans aBase, application::shared_token_spans aHead)
	{
		if (BaseSpans == aBase && HeadSpans == aHead) {
			return;
		}
		BaseSpans = std::move(aBase);
		HeadSpans = std::move(aHead);
		apply_syntax();
		// The emphasis keeps each token's colour over its fill, so it is laid
		// again over the new colours.
		repaint_bands();
	}

	void line_details_pane::apply_syntax()
	{
		if (Empty) {
			return;
		}
		auto kept = std::vector<application::token_span>{ };
		for (std::size_t index = 0U; index < RowBlocks; ++index) {
			const auto block = document()->findBlockByNumber(static_cast<int>(index));
			if (!block.isValid()) {
				continue;
			}
			const auto& row     = Shown.Rows[index];
			const auto& spans   = row.Side == core::diff_side::left ? BaseSpans : HeadSpans;
			auto        formats = QList<QTextLayout::FormatRange>{ };
			if (spans && row.Line.has_value() && row.Kind != line_details_row_kind::absent) {
				const auto& bytes = RowBytes[index];
				const auto  shown = std::span<const std::uint8_t>{ reinterpret_cast<const std::uint8_t*>(bytes.constData()),
																   static_cast<std::size_t>(bytes.size()) };
				// Only what the row shows is coloured: a token past the core's
				// cut is left out, and one across it ends there, so a cut line
				// costs its shown part however long the line is.
				kept.clear();
				for (const auto& span : spans_of_line(*spans, *row.Line)) {
					if (span.Column >= shown.size()) {
						continue;
					}
					auto clipped   = span;
					clipped.Length = static_cast<std::uint32_t>(
						std::min<std::size_t>(span.Length, shown.size() - span.Column));
					kept.push_back(clipped);
				}
				formats = code_syntax_format_ranges({ .LineBytes = shown, .Spans = kept, .Colors = &Colors.Syntax });
			}
			static_cast<void>(apply_syntax_format_ranges(*document(), block, formats));
		}
	}

	void line_details_pane::repaint_bands()
	{
		auto selections = QList<QTextEdit::ExtraSelection>{ };
		if (Empty) {
			setExtraSelections(selections);
			return;
		}
		for (std::size_t index = 0U; index < RowBlocks; ++index) {
			const auto& row  = Shown.Rows[index];
			auto        band = QColor{ };
			if (row.Kind == line_details_row_kind::removed
				|| (row.Kind == line_details_row_kind::changed && row.Side == core::diff_side::left)) {
				band = Colors.Removed;
			} else if (row.Kind == line_details_row_kind::added
					   || (row.Kind == line_details_row_kind::changed && row.Side == core::diff_side::right)) {
				band = Colors.Added;
			}
			if (!band.isValid()) {
				continue;
			}
			// A full-width band, so a row shorter than its partner still wears
			// it all the way across when the strip is scrolled.
			const auto block = document()->findBlockByNumber(static_cast<int>(index));
			auto       wash  = QTextEdit::ExtraSelection{ };
			wash.cursor      = QTextCursor{ block };
			wash.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
			wash.format.setBackground(band);
			wash.format.setProperty(QTextFormat::FullWidthSelection, true);
			selections.push_back(wash);
		}
		const auto& normal = Colors.Syntax[core::index_of(core::appearance_role::syntax_normal)];
		for (const auto& span : Emphasis) {
			const auto  side = Shown.Rows[static_cast<std::size_t>(span.Block)].Side;
			const auto& fill = side == core::diff_side::left ? Colors.IntralineRemoved : Colors.IntralineAdded;
			if (!fill.isValid()) {
				continue;
			}
			// The panes' own mark: the fill as a background, each token's
			// colour kept over it.
			append_intraline_emphasis_span(selections, document()->findBlockByNumber(span.Block), span.From, span.To, fill,
										   Colors.Supplied && normal.isValid()
											   ? std::optional{ code_emphasis_ink{ .Normal = normal, .Variant = Colors.Variant } }
											   : std::nullopt);
		}
		setExtraSelections(selections);
	}

	auto line_details_pane::overlay_bar_reserve() const -> int
	{
		const auto* const bar = horizontalScrollBar();
		return bar->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, bar) > 0
			? bar->sizeHint().height()
			: 0;
	}

	void line_details_pane::place_gutter()
	{
		const auto width = gutter_width();
		// An overlay bar (macOS's default, Qt's transient style) takes no
		// height of its own: it floats over the bottom of the viewport while
		// the strip scrolls, which is when its rows are being read, and at the
		// strip's smallest height that is the head row. So the strip keeps the
		// bar's band free under the rows itself, as a legacy bar does.
		setViewportMargins(width, 0, 0, overlay_bar_reserve());
		// Beside the viewport only: the horizontal bar runs under the gutter
		// too, and a gutter as tall as the frame would cover its start.
		const auto view = viewport()->geometry();
		Gutter->setGeometry(QRect{ contentsRect().left(), view.top(), width, view.height() });
		Gutter->update();
	}

	auto line_details_pane::viewportEvent(QEvent* const aEvent) -> bool
	{
		const auto handled = QPlainTextEdit::viewportEvent(aEvent);
		// The bar appearing or going changes the viewport's height, not the pane's.
		if (aEvent->type() == QEvent::Resize) {
			place_gutter();
		}
		return handled;
	}

	auto line_details_pane::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == horizontalScrollBar() && aEvent->type() == QEvent::StyleChange) {
			place_gutter();
		}
		return QPlainTextEdit::eventFilter(aWatched, aEvent);
	}

	void line_details_pane::resizeEvent(QResizeEvent* const aEvent)
	{
		QPlainTextEdit::resizeEvent(aEvent);
		place_gutter();
	}

	void line_details_pane::changeEvent(QEvent* const aEvent)
	{
		QPlainTextEdit::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange
			|| aEvent->type() == QEvent::PaletteChange) {
			place_gutter();
		}
	}

	void line_details_pane::paint_gutter(QPaintEvent* const aEvent)
	{
		auto painter = QPainter{ Gutter };
		painter.fillRect(aEvent->rect(),
						 Colors.GutterBackground.isValid() ? Colors.GutterBackground : palette().color(QPalette::AlternateBase));
		if (Empty) {
			return;
		}
		painter.setPen(Colors.GutterText.isValid() ? Colors.GutterText : palette().color(QPalette::PlaceholderText));
		const auto glyph     = fontMetrics().horizontalAdvance(QLatin1Char('9'));
		const auto markWidth = glyph * 2;
		const auto right     = Gutter->width() - 6;
		for (auto block = firstVisibleBlock(); block.isValid(); block = block.next()) {
			const auto top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
			if (top > aEvent->rect().bottom()) {
				break;
			}
			const auto number = block.blockNumber();
			if (number < 0 || static_cast<std::size_t>(number) >= RowBlocks) {
				continue;
			}
			const auto& row    = Shown.Rows[static_cast<std::size_t>(number)];
			const auto  height = fontMetrics().height();
			if (row.Line.has_value()) {
				painter.drawText(QRect{ 0, top, right - markWidth, height }, Qt::AlignRight | Qt::AlignVCenter,
								 QString::number(*row.Line + 1U));
			}
			painter.drawText(QRect{ right - markWidth, top, markWidth, height }, Qt::AlignHCenter | Qt::AlignVCenter,
							 row_mark(row));
			// A pair after the first starts under a quiet rule, so a selection
			// of several lines reads as pairs rather than as one column.
			if (number > 0 && number % 2 == 0) {
				painter.save();
				painter.setPen(palette().color(QPalette::Mid));
				painter.drawLine(0, top, Gutter->width(), top);
				painter.restore();
			}
		}
	}
} // namespace bpr::qt
