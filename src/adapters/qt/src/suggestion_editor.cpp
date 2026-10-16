#include "bpr/qt/suggestion_editor.hpp"

#include "bpr/core/appearance_contrast.hpp"

#include "source_display_text.hpp"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QGlyphRun>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleHints>
#include <QTextDocument>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The text document's own margin, which the review panes lay their text out from.
		auto document_margin() -> qreal
		{
			static const auto margin = QTextDocument{ }.documentMargin();
			return margin;
		}

		auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		auto text_of(const std::string_view aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		/// A standard key sequence and the motion it means.
		struct standard_motion
		{
			QKeySequence::StandardKey Key;
			core::text_motion         Motion;
			bool                      Extend;
		};
		constexpr auto standard_motions = std::array{
			standard_motion{ QKeySequence::MoveToNextChar, core::text_motion::right, false },
			standard_motion{ QKeySequence::MoveToPreviousChar, core::text_motion::left, false },
			standard_motion{ QKeySequence::MoveToNextLine, core::text_motion::down, false },
			standard_motion{ QKeySequence::MoveToPreviousLine, core::text_motion::up, false },
			standard_motion{ QKeySequence::MoveToNextWord, core::text_motion::word_right, false },
			standard_motion{ QKeySequence::MoveToPreviousWord, core::text_motion::word_left, false },
			standard_motion{ QKeySequence::MoveToStartOfLine, core::text_motion::line_start, false },
			standard_motion{ QKeySequence::MoveToEndOfLine, core::text_motion::line_end, false },
			standard_motion{ QKeySequence::MoveToStartOfDocument, core::text_motion::document_start, false },
			standard_motion{ QKeySequence::MoveToEndOfDocument, core::text_motion::document_end, false },
			standard_motion{ QKeySequence::SelectNextChar, core::text_motion::right, true },
			standard_motion{ QKeySequence::SelectPreviousChar, core::text_motion::left, true },
			standard_motion{ QKeySequence::SelectNextLine, core::text_motion::down, true },
			standard_motion{ QKeySequence::SelectPreviousLine, core::text_motion::up, true },
			standard_motion{ QKeySequence::SelectNextWord, core::text_motion::word_right, true },
			standard_motion{ QKeySequence::SelectPreviousWord, core::text_motion::word_left, true },
			standard_motion{ QKeySequence::SelectStartOfLine, core::text_motion::line_start, true },
			standard_motion{ QKeySequence::SelectEndOfLine, core::text_motion::line_end, true },
			standard_motion{ QKeySequence::SelectStartOfDocument, core::text_motion::document_start, true },
			standard_motion{ QKeySequence::SelectEndOfDocument, core::text_motion::document_end, true },
		};

		auto arrow_motion(const int aKey) -> std::optional<core::text_motion>
		{
			switch (aKey) {
			case Qt::Key_Left:  return core::text_motion::left;
			case Qt::Key_Right: return core::text_motion::right;
			case Qt::Key_Up:    return core::text_motion::up;
			case Qt::Key_Down:  return core::text_motion::down;
			default:            return std::nullopt;
			}
		}

		/// The text interface a screen reader reads the code view through,
		/// in UTF-16 offsets of the whole text, as `QPlainTextEdit` offers.
		class code_view_accessible final : public QAccessibleWidget, public QAccessibleTextInterface
		{
		public:
			explicit code_view_accessible(suggestion_editor* const aView)
				: QAccessibleWidget{ aView, QAccessible::EditableText }
			{
			}

			auto interface_cast(const QAccessible::InterfaceType aType) -> void* override
			{
				return aType == QAccessible::TextInterface ? static_cast<QAccessibleTextInterface*>(this) : QAccessibleWidget::interface_cast(aType);
			}

			[[nodiscard]] auto state() const -> QAccessible::State override
			{
				auto state           = QAccessibleWidget::state();
				state.multiLine      = true;
				state.selectableText = true;
				state.readOnly       = view().read_only();
				state.editable       = !view().read_only();
				return state;
			}

			[[nodiscard]] auto text(const QAccessible::Text aKind) const -> QString override
			{
				return aKind == QAccessible::Value ? whole() : QAccessibleWidget::text(aKind);
			}

			void selection(const int aIndex, int* const aStart, int* const aEnd) const override
			{
				*aStart = 0;
				*aEnd   = 0;
				if (aIndex == 0 && view().has_selection()) {
					const auto [start, end] = view().selected_bytes();
					*aStart                 = units_of(start);
					*aEnd                   = units_of(end);
				}
			}

			[[nodiscard]] auto selectionCount() const -> int override
			{
				return view().has_selection() ? 1 : 0;
			}

			void addSelection(const int aStart, const int aEnd) override
			{
				view().select_bytes(bytes_of(aStart), bytes_of(aEnd));
			}

			void removeSelection(const int aIndex) override
			{
				static_cast<void>(aIndex);
				const auto caret = view().selected_bytes().second;
				view().select_bytes(caret, caret);
			}

			void setSelection(const int aIndex, const int aStart, const int aEnd) override
			{
				static_cast<void>(aIndex);
				view().select_bytes(bytes_of(aStart), bytes_of(aEnd));
			}

			[[nodiscard]] auto cursorPosition() const -> int override
			{
				const auto& state = view().model().state();
				return units_of(view().model().byte_offset_of(state.Carets[state.Primary].Head));
			}

			void setCursorPosition(const int aPosition) override
			{
				const auto at = bytes_of(aPosition);
				view().select_bytes(at, at);
			}

			[[nodiscard]] auto text(const int aStart, const int aEnd) const -> QString override
			{
				return whole().mid(aStart, aEnd - aStart);
			}

			[[nodiscard]] auto characterCount() const -> int override
			{
				return static_cast<int>(whole().size());
			}

			[[nodiscard]] auto characterRect(const int aOffset) const -> QRect override
			{
				// [[BPR-1438]]. A wide cluster's rectangle covers both its cells.
				const auto at    = view().model().point_of(bytes_of(aOffset));
				const auto span  = view().model().cluster_span(at);
				auto       exact = view().cell_rectangle(at);
				if (span.second > span.first) {
					exact.setWidth(exact.width() * static_cast<qreal>(span.second - span.first));
				}
				const auto cell = exact.toAlignedRect();
				return QRect{ view().viewport()->mapToGlobal(cell.topLeft()), cell.size() };
			}

			[[nodiscard]] auto offsetAtPoint(const QPoint& aPoint) const -> int override
			{
				const auto at = view().point_at(view().viewport()->mapFromGlobal(aPoint));
				return units_of(view().model().byte_offset_of(at));
			}

			void scrollToSubstring(const int aStart, const int aEnd) override
			{
				static_cast<void>(aEnd);
				view().scroll_to_line(view().model().point_of(bytes_of(aStart)).Line);
			}

			[[nodiscard]] auto attributes(const int aOffset, int* const aStart, int* const aEnd) const -> QString override
			{
				*aStart = aOffset;
				*aEnd   = aOffset;
				return { };
			}

		private:
			[[nodiscard]] auto view() const -> suggestion_editor&
			{
				return *static_cast<suggestion_editor*>(widget());
			}

			[[nodiscard]] auto whole() const -> QString
			{
				return text_of(view().text());
			}

			[[nodiscard]] auto units_of(const std::uint64_t aBytes) const -> int
			{
				const auto bytes = view().text();
				return static_cast<int>(text_of(std::string_view{ bytes }.substr(0, std::min<std::size_t>(aBytes, bytes.size()))).size());
			}

			[[nodiscard]] auto bytes_of(const int aUnits) const -> std::uint64_t
			{
				return static_cast<std::uint64_t>(whole().left(std::max(aUnits, 0)).toUtf8().size());
			}
		};

		auto code_view_accessible_factory(const QString& aKey, QObject* const aObject) -> QAccessibleInterface*
		{
			static_cast<void>(aKey);
			if (auto* const view = dynamic_cast<suggestion_editor*>(aObject); view != nullptr) {
				return new code_view_accessible{ view };
			}
			return nullptr;
		}

		void install_code_view_accessibility()
		{
			static auto installed = false;
			if (!installed) {
				QAccessible::installFactory(&code_view_accessible_factory);
				installed = true;
			}
		}
	}

	auto suggestion_code_font() -> QFont
	{
		return fixed_pitch_code_font(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	}

	auto fixed_pitch_code_font(QFont aFont) -> QFont
	{
		if (QFontInfo{ aFont }.fixedPitch()) {
			return aFont;
		}
		aFont.setStyleHint(QFont::TypeWriter);
		aFont.setFixedPitch(true);
		aFont.setFamilies({ aFont.family(), QStringLiteral("Menlo"), QStringLiteral("SF Mono"), QStringLiteral("Consolas"), QStringLiteral("Cascadia Mono"), QStringLiteral("DejaVu Sans Mono"),
							QStringLiteral("Liberation Mono"), QStringLiteral("Noto Sans Mono"), QStringLiteral("Courier New") });
		return aFont;
	}

	auto suggestion_syntax_lines(const classified_text& aClassified, const std::string_view aText, const code_role_colors& aColors) -> std::vector<suggestion_line_syntax>
	{
		auto lines = std::vector<suggestion_line_syntax>{ };
		auto begin = std::uint64_t{ 0 };
		for (auto& line : core::split_text_lines(aText)) {
			const auto ending  = line.Ending == core::text_line_ending::crlf ? 2U : line.Ending == core::text_line_ending::none ? 0U
																																: 1U;
			auto       formats = code_syntax_format_ranges_at(aClassified, begin, std::span{ reinterpret_cast<const std::uint8_t*>(line.Content.data()), line.Content.size() }, aColors);
			begin += line.Content.size() + ending;
			lines.push_back({ .Content = std::move(line.Content), .Formats = std::move(formats) });
		}
		return lines;
	}

	suggestion_editor::suggestion_editor(QWidget* aParent)
		: QAbstractScrollArea{ aParent }
		, CodeFont{ suggestion_code_font() }
	{
		install_code_view_accessibility();
		setObjectName(QStringLiteral("suggestionEditor"));
		setAccessibleName(QStringLiteral("Suggested code"));
		setAccessibleDescription(QStringLiteral("Edit the proposed code. Alt+Shift+Arrow makes a box selection; Ctrl+Tab leaves the editor."));
		setAttribute(Qt::WA_InputMethodEnabled);
		setFocusPolicy(Qt::StrongFocus);
		viewport()->setCursor(Qt::IBeamCursor);
		setFont(CodeFont);
		measure();
		update_scrollbars();
	}

	void suggestion_editor::set_text(const std::string_view aText, const std::uint32_t aTabWidth)
	{
		const auto same = Editor.text() == aText && Editor.state().TabWidth == aTabWidth;
		auto       next = core::suggestion_text_editor{ aText, aTabWidth };
		next.set_indentation(Editor.indentation());
		Editor = std::move(next);
		Preedit.clear();
		measure_lines();
		if (!same) {
			Syntax.clear();
			SyntaxByContent.clear();
		}
		update_scrollbars();
		viewport()->update();
	}

	auto suggestion_editor::text() const -> std::string
	{
		return Editor.text();
	}

	auto suggestion_editor::model() const noexcept -> const core::suggestion_text_editor&
	{
		return Editor;
	}

	void suggestion_editor::set_read_only(const bool aReadOnly)
	{
		ReadOnly = aReadOnly;
		Preedit.clear();
		setAttribute(Qt::WA_InputMethodEnabled, !aReadOnly);
		viewport()->update();
	}

	auto suggestion_editor::read_only() const noexcept -> bool
	{
		return ReadOnly;
	}

	void suggestion_editor::set_indentation(core::text_indentation aIndentation)
	{
		Editor.set_indentation(std::move(aIndentation));
	}

	auto suggestion_editor::preedit() const noexcept -> const QString&
	{
		return Preedit;
	}

	void suggestion_editor::set_code_font(const QFont& aFont)
	{
		CodeFont = fixed_pitch_code_font(aFont);
		setFont(CodeFont);
		measure();
		update_scrollbars();
		viewport()->update();
		updateGeometry();
	}

	void suggestion_editor::set_contrast_variant(const core::appearance_variant aVariant)
	{
		if (Variant != aVariant) {
			Variant = aVariant;
			SelectionInk.clear();
			viewport()->update();
		}
	}

	auto suggestion_editor::selection_ink(const QColor& aColour, const QColor& aFill) const -> QColor
	{
		// [[BPR-1156]]. The core's rule, once per colour and fill: few
		// colours appear on a page, and a paint asks for each many times.
		if (InkFill != aFill.rgba()) {
			SelectionInk.clear();
			InkFill = aFill.rgba();
		}
		if (const auto found = SelectionInk.constFind(aColour.rgba()); found != SelectionInk.cend()) {
			return *found;
		}
		const auto channel = [](const QColor& aSource) {
			return core::appearance_color{ static_cast<std::uint8_t>(aSource.red()), static_cast<std::uint8_t>(aSource.green()), static_cast<std::uint8_t>(aSource.blue()), 255U };
		};
		const auto ink = core::readable_selection_ink(channel(aColour), channel(aFill), Variant);
		return *SelectionInk.insert(aColour.rgba(), QColor{ ink.Red, ink.Green, ink.Blue });
	}

	void suggestion_editor::measure()
	{
		// One line of the code font as the review panes' text layout sets it,
		// so a row here has the pitch and baseline of a row there.
		auto layout = QTextLayout{ QStringLiteral("M"), CodeFont, viewport() };
		layout.beginLayout();
		auto line = layout.createLine();
		line.setLeadingIncluded(true);
		layout.endLayout();
		RowHeight   = std::max<qreal>(1.0, line.height());
		Ascent      = line.ascent();
		ColumnWidth = std::max<qreal>(1.0, QFontMetricsF{ CodeFont, viewport() }.horizontalAdvance(QLatin1Char('M')));
		ShapedGlyphs.clear();
	}

	void suggestion_editor::set_syntax(std::vector<suggestion_line_syntax> aLines)
	{
		Syntax = std::move(aLines);
		SyntaxByContent.clear();
		for (std::size_t index = 0; index < Syntax.size(); ++index) {
			SyntaxByContent.try_emplace(Syntax[index].Content, index);
		}
		viewport()->update();
	}

	auto suggestion_editor::syntax_of(const std::uint64_t aLine) const -> const QList<QTextLayout::FormatRange>*
	{
		const auto& content = Editor.state().Lines[aLine].Content;
		if (aLine < Syntax.size() && Syntax[aLine].Content == content) {
			return &Syntax[aLine].Formats;
		}
		const auto moved = SyntaxByContent.find(content);
		return moved == SyntaxByContent.end() ? nullptr : &Syntax[moved->second].Formats;
	}

	void suggestion_editor::select_bytes(const std::uint64_t aStart, const std::uint64_t aEnd)
	{
		const auto start = Editor.point_of(static_cast<std::size_t>(aStart));
		const auto end   = Editor.point_of(static_cast<std::size_t>(aEnd));
		Editor.apply(core::place_caret{ .At = start });
		if (end != start) {
			Editor.apply(core::place_caret{ .At = end, .Extend = true });
		}
		update_scrollbars();
		ensure_visible();
		viewport()->update();
		announce_selection();
	}

	auto suggestion_editor::selected_bytes() const -> std::pair<std::uint64_t, std::uint64_t>
	{
		const auto& caret = Editor.state().Carets[Editor.state().Primary];
		return { Editor.byte_offset_of(std::min(caret.Anchor, caret.Head)), Editor.byte_offset_of(std::max(caret.Anchor, caret.Head)) };
	}

	auto suggestion_editor::has_selection() const -> bool
	{
		// A selection only in virtual space selects no byte.
		if (Editor.state().Carets[Editor.state().Primary].empty()) {
			return false;
		}
		const auto [start, end] = selected_bytes();
		return start != end;
	}

	void suggestion_editor::measure_lines()
	{
		WidestLine = 0U;
		for (std::uint64_t line = 0; line < Editor.state().Lines.size(); ++line) {
			WidestLine = std::max(WidestLine, Editor.line_width(line));
		}
	}

	void suggestion_editor::scroll_to_line(const std::uint64_t aLine)
	{
		update_scrollbars();
		verticalScrollBar()->setValue(static_cast<int>(std::min<std::uint64_t>(aLine, static_cast<std::uint64_t>(verticalScrollBar()->maximum()))));
		viewport()->update();
	}

	auto suggestion_editor::first_visible_line() const -> std::uint64_t
	{
		return static_cast<std::uint64_t>(std::max(verticalScrollBar()->value(), 0));
	}

	auto suggestion_editor::apply(const core::suggestion_edit& aEdit) -> bool
	{
		if (ReadOnly && core::changes_text(aEdit)) {
			return false;
		}
		// Only an edit can change the text, so only an edit pays for comparing it.
		const auto edit    = core::changes_text(aEdit);
		const auto before  = edit ? Editor.text() : std::string{ };
		const auto changed = Editor.apply(aEdit);
		const auto edited  = edit && changed && Editor.text() != before;
		if (edited) {
			measure_lines();
		}
		if (changed) {
			update_scrollbars();
			ensure_visible();
			viewport()->update();
			announce_selection();
			if (edited && Changed) {
				Changed();
			}
		}
		return changed;
	}

	void suggestion_editor::announce_selection()
	{
		if (SelectionChanged) {
			SelectionChanged();
		}
		if (auto* const accessible = QAccessible::isActive() ? QAccessible::queryAccessibleInterface(this) : nullptr; accessible != nullptr && accessible->textInterface() != nullptr) {
			auto moved = QAccessibleTextCursorEvent{ this, accessible->textInterface()->cursorPosition() };
			QAccessible::updateAccessibility(&moved);
		}
	}

	auto suggestion_editor::row_height() const -> qreal
	{
		return RowHeight;
	}

	auto suggestion_editor::visible_rows() const -> int
	{
		return std::max(1, static_cast<int>(std::floor(static_cast<qreal>(viewport()->height()) / RowHeight)));
	}

	auto suggestion_editor::column_x(const std::uint64_t aColumn) const -> qreal
	{
		return document_margin() + static_cast<qreal>(aColumn) * ColumnWidth - horizontalScrollBar()->value();
	}

	auto suggestion_editor::shaped(const QString& aGlyph) const -> const shaped_glyph&
	{
		// [[BPR-1156]]. Shaped alone, on this device, as a one-character
		// drawText shapes it, so the glyph and its offset are the ones that
		// call drew. A move to a screen of another ratio shapes again.
		if (const auto ratio = viewport()->devicePixelRatioF(); ratio != ShapedRatio) {
			ShapedGlyphs.clear();
			ShapedRatio = ratio;
		}
		if (const auto found = ShapedGlyphs.constFind(aGlyph); found != ShapedGlyphs.cend()) {
			return *found;
		}
		auto glyph  = shaped_glyph{ };
		auto layout = QTextLayout{ aGlyph, CodeFont, viewport() };
		layout.beginLayout();
		const auto line = layout.createLine();
		layout.endLayout();
		glyph.Advance = line.naturalTextWidth();
		if (const auto runs = layout.glyphRuns(); runs.size() == 1 && runs.front().glyphIndexes().size() == 1) {
			glyph.Font   = runs.front().rawFont();
			glyph.Index  = runs.front().glyphIndexes().front();
			glyph.Offset = runs.front().positions().front() - QPointF{ 0.0, line.ascent() };
			glyph.Single = true;
		}
		return *ShapedGlyphs.insert(aGlyph, glyph);
	}

	void suggestion_editor::update_scrollbars()
	{
		const auto& state   = Editor.state();
		const auto  visible = visible_rows();
		verticalScrollBar()->setRange(0, std::max(0, static_cast<int>(state.Lines.size()) - visible));
		verticalScrollBar()->setPageStep(visible);
		auto widest = WidestLine;
		for (const auto& caret : state.Carets) {
			widest = std::max({ widest, caret.Head.Column, caret.Anchor.Column });
		}
		const auto margin  = document_margin();
		const auto content = static_cast<int>(std::ceil(static_cast<qreal>(widest + 2U) * ColumnWidth + 2.0 * margin));
		horizontalScrollBar()->setRange(0, std::max(0, content - viewport()->width()));
		horizontalScrollBar()->setPageStep(viewport()->width());
	}

	void suggestion_editor::ensure_visible()
	{
		const auto& head    = Editor.state().Carets[Editor.state().Primary].Head;
		const auto  visible = visible_rows();
		const auto  line    = static_cast<int>(head.Line);
		if (line < verticalScrollBar()->value()) {
			verticalScrollBar()->setValue(line);
		} else if (line >= verticalScrollBar()->value() + visible) {
			verticalScrollBar()->setValue(line - visible + 1);
		}
		const auto margin = static_cast<int>(document_margin());
		const auto x      = static_cast<int>(static_cast<qreal>(head.Column) * ColumnWidth) + margin;
		if (x < horizontalScrollBar()->value()) {
			horizontalScrollBar()->setValue(std::max(0, x - margin));
		} else if (x > horizontalScrollBar()->value() + viewport()->width() - 2 * margin) {
			horizontalScrollBar()->setValue(x - viewport()->width() + 2 * margin);
		}
	}

	auto suggestion_editor::cell_rectangle(const core::text_point& aPoint) const -> QRectF
	{
		const auto top = (static_cast<qreal>(aPoint.Line) - static_cast<qreal>(verticalScrollBar()->value())) * RowHeight;
		return { column_x(aPoint.Column), top, ColumnWidth, RowHeight };
	}

	auto suggestion_editor::caret_rectangle() const -> QRect
	{
		const auto cell = cell_rectangle(Editor.state().Carets[Editor.state().Primary].Head);
		return QRect{ static_cast<int>(std::floor(cell.left())), static_cast<int>(std::floor(cell.top())), 2, static_cast<int>(std::ceil(RowHeight)) };
	}

	auto suggestion_editor::point_at(const QPoint& aPosition) const -> core::text_point
	{
		const auto lines  = static_cast<int>(Editor.state().Lines.size());
		const auto row    = static_cast<int>(std::floor(static_cast<qreal>(aPosition.y()) / RowHeight));
		const auto line   = std::clamp(verticalScrollBar()->value() + row, 0, lines - 1);
		const auto offset = std::max<qreal>(0.0, (static_cast<qreal>(aPosition.x()) - column_x(0U)) / ColumnWidth);
		const auto at     = core::text_point{ static_cast<std::uint64_t>(line), static_cast<std::uint64_t>(std::floor(offset)) };
		// [[BPR-1438]]. Inside a wide cluster, the nearer of its edges. A tab
		// keeps the nearer column, which the core takes to the tab's start.
		const auto& content = Editor.state().Lines[at.Line].Content;
		if (const auto [first, after] = Editor.cluster_span(at); after - first == 2U && content[Editor.locate(at).first] != '\t') {
			return { at.Line, offset - static_cast<qreal>(first) < 1.0 ? first : after };
		}
		return { at.Line, static_cast<std::uint64_t>(std::round(offset)) };
	}

	auto suggestion_editor::cell_at(const QPoint& aPosition) const -> core::text_point
	{
		const auto edge   = point_at(aPosition);
		const auto offset = (static_cast<qreal>(aPosition.x()) - column_x(0U)) / ColumnWidth;
		return { edge.Line, static_cast<std::uint64_t>(std::max<qreal>(0.0, std::floor(offset))) };
	}

	void suggestion_editor::paintEvent(QPaintEvent* aEvent)
	{
		static_cast<void>(aEvent);
		auto        painter = QPainter{ viewport() };
		const auto& state   = Editor.state();
		const auto  first   = first_visible_line();
		const auto  last    = std::min<std::uint64_t>(state.Lines.size(), first + static_cast<std::uint64_t>(visible_rows()) + 2U);
		const auto  top     = [&](const std::uint64_t aLine) { return static_cast<qreal>(aLine - first) * RowHeight; };
		painter.setFont(CodeFont);
		painter.fillRect(viewport()->rect(), palette().base());

		// [[BPR-1156]]. Selections: the solid `Highlight` every text box in
		// the window selects with, under the glyphs. A selection ends at the
		// line's text, plus one cell where it takes the line break; only a
		// box's carets stand in virtual space, and a box paints there.
		const auto selection = palette().color(QPalette::Highlight);
		// The selected columns of each visible line, whose glyphs take the selection's ink.
		auto selected = std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>>(static_cast<std::size_t>(last - std::min(first, last)));
		for (const auto& caret : state.Carets) {
			if (caret.empty()) {
				continue;
			}
			const auto start = std::min(caret.Anchor, caret.Head);
			const auto end   = std::max(caret.Anchor, caret.Head);
			for (auto line = std::max(start.Line, first); line <= end.Line && line < last; ++line) {
				const auto lineWidth = Editor.line_width(line);
				auto       from      = line == start.Line ? start.Column : 0U;
				auto       to        = end.Column;
				if (line != end.Line) {
					from = std::min(from, lineWidth);
					to   = lineWidth + 1U;
				}
				if (to > from) {
					painter.fillRect(QRectF{ column_x(from), top(line), column_x(to) - column_x(from), RowHeight }, selection);
					selected[static_cast<std::size_t>(line - first)].emplace_back(from, to);
				}
			}
		}
		// One cluster per column cell, so carets, selections and boxes line up
		// whatever the font's own advances are. A classified glyph takes its
		// role's colour; everything else is the native text colour.
		const auto text = palette().text().color();
		const auto tab  = state.TabWidth;
		// [[BPR-1156]]. Each glyph is still shaped alone and placed in its own
		// cell, but a run of glyphs in one colour and font is drawn in one
		// call, and the colours come from a per-line table rather than a
		// search of the ranges for every glyph.
		auto       unitColours = std::vector<QColor>{ };
		auto       runFont     = QRawFont{ };
		auto       runColour   = QColor{ };
		auto       runIndexes  = QList<quint32>{ };
		auto       runPoints   = QList<QPointF>{ };
		const auto flush       = [&] {
			if (runIndexes.isEmpty()) {
				return;
			}
			auto run = QGlyphRun{ };
			run.setRawFont(runFont);
			run.setGlyphIndexes(runIndexes);
			run.setPositions(runPoints);
			painter.setPen(runColour);
			painter.drawGlyphRun(QPointF{ }, run);
			runIndexes.clear();
			runPoints.clear();
		};
		for (auto line = first; line < last; ++line) {
			const auto  y       = top(line) + Ascent;
			const auto& content = state.Lines[line].Content;
			// The colour of each UTF-16 unit, the first range in the list that
			// covers a unit winning, as a search in list order would choose.
			unitColours.assign(content.size() + 1U, text);
			if (const auto* formats = syntax_of(line); formats != nullptr) {
				for (auto range = formats->crbegin(); range != formats->crend(); ++range) {
					if (!range->format.hasProperty(QTextFormat::ForegroundBrush)) {
						continue;
					}
					const auto colour = range->format.foreground().color();
					const auto from   = std::clamp<qsizetype>(range->start, 0, static_cast<qsizetype>(unitColours.size()));
					const auto to     = std::clamp<qsizetype>(static_cast<qsizetype>(range->start) + range->length, from, static_cast<qsizetype>(unitColours.size()));
					std::fill(unitColours.begin() + from, unitColours.begin() + to, colour);
				}
			}
			// The cluster's position in UTF-16 units, which is how the shared
			// painter's ranges count.
			auto        unit  = std::size_t{ 0 };
			const auto& spans = selected[static_cast<std::size_t>(line - first)];
			// [[BPR-1438]]. One extended grapheme cluster per cell, or per two
			// cells when the core measures it wide, its scalars shaped together
			// so a combining mark sits on its base.
			for (const auto& cluster : core::text_clusters(content, tab)) {
				const auto source = QString::fromUtf8(content.data() + cluster.Offset, static_cast<qsizetype>(cluster.Length));
				const auto at     = unit;
				unit += static_cast<std::size_t>(source.size());
				if (content[cluster.Offset] == '\t') {
					continue;
				}
				const auto  column = cluster.Column;
				const auto& role   = unitColours[std::min(at, unitColours.size() - 1U)];
				const auto  inside = std::ranges::any_of(spans, [column](const auto& aSpan) { return aSpan.first <= column && column < aSpan.second; });
				const auto  colour = inside ? selection_ink(role, selection) : role;
				// [[BPR-1432]]. A lone CR is content, shown as its symbol.
				const auto  glyph  = content[cluster.Offset] == '\r' ? QString{ QChar{ lone_carriage_return_symbol } } : source;
				auto        origin = QPointF{ column_x(column), y };
				const auto& found  = shaped(glyph);
				// A wide cluster is centred in its cells, and narrowed when a
				// fallback face draws it wider than both.
				const auto box  = static_cast<qreal>(cluster.Width) * ColumnWidth;
				const auto wide = cluster.Width > 1U;
				if (wide && found.Advance > box) {
					flush();
					painter.save();
					painter.setPen(colour);
					painter.translate(origin);
					painter.scale(box / found.Advance, 1.0);
					painter.drawText(QPointF{ }, glyph);
					painter.restore();
					continue;
				}
				if (wide) {
					origin.rx() += (box - found.Advance) / 2.0;
				}
				if (found.Single) {
					if (found.Font != runFont || colour != runColour) {
						flush();
						runFont   = found.Font;
						runColour = colour;
					}
					runIndexes.append(found.Index);
					runPoints.append(origin + found.Offset);
				} else {
					flush();
					painter.setPen(colour);
					painter.drawText(origin, glyph);
				}
			}
		}
		flush();
		painter.setPen(text);
		// Carets, and the IME composition at the primary one.
		const auto caretColour = hasFocus() ? palette().text().color() : palette().placeholderText().color();
		for (std::size_t index = 0; index < state.Carets.size(); ++index) {
			const auto& head = state.Carets[index].Head;
			if (head.Line < first || head.Line >= last) {
				continue;
			}
			const auto y = top(head.Line);
			if (index == state.Primary && !Preedit.isEmpty()) {
				const auto width = QFontMetricsF{ CodeFont, viewport() }.horizontalAdvance(Preedit);
				painter.fillRect(QRectF{ column_x(head.Column), y, width, RowHeight }, palette().alternateBase());
				painter.drawText(QPointF{ column_x(head.Column), y + Ascent }, Preedit);
				painter.drawLine(QPointF{ column_x(head.Column), y + RowHeight - 1.0 }, QPointF{ column_x(head.Column) + width, y + RowHeight - 1.0 });
			}
			painter.fillRect(QRectF{ column_x(head.Column) - 1.0, y, index == state.Primary ? 2.0 : 1.5, RowHeight }, caretColour);
		}
	}

	auto suggestion_editor::event(QEvent* aEvent) -> bool
	{
		// Escape collapses before it can cancel what the editor is part of.
		if (aEvent->type() == QEvent::ShortcutOverride) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Escape && collapsible()) {
				aEvent->accept();
				return true;
			}
			// [[BPR-1532]]. Alt+Shift+Arrow is the box gesture, read-only too,
			// so a window command on Alt+Shift+Up or Alt+Shift+Down (First and
			// Last change) never takes it from here.
			const auto modifiers = key->modifiers();
			if (arrow_motion(key->key()) && modifiers.testFlag(Qt::AltModifier) && modifiers.testFlag(Qt::ShiftModifier)) {
				aEvent->accept();
				return true;
			}
		}
		return QAbstractScrollArea::event(aEvent);
	}

	/// Whether Escape has something to collapse: a selection or extra carets.
	auto suggestion_editor::collapsible() const -> bool
	{
		return Editor.state().Carets.size() > 1U || !Editor.state().Carets.front().empty();
	}

	void suggestion_editor::keyPressEvent(QKeyEvent* aEvent)
	{
		const auto modifiers = aEvent->modifiers();
		const auto shift     = modifiers.testFlag(Qt::ShiftModifier);
		const auto alt       = modifiers.testFlag(Qt::AltModifier);
		// Alt+Shift+Arrow is the box gesture on every platform.
		if (const auto motion = arrow_motion(aEvent->key()); motion && alt && shift) {
			apply(core::extend_box{ *motion });
			return;
		}
		// Ctrl+Tab leaves; read-only, Tab does too, as it does a read-only text pane.
		if ((aEvent->key() == Qt::Key_Tab || aEvent->key() == Qt::Key_Backtab) && (modifiers.testFlag(Qt::ControlModifier) || ReadOnly)) {
			QAbstractScrollArea::focusNextPrevChild(aEvent->key() == Qt::Key_Tab && !shift);
			return;
		}
		if (aEvent->matches(QKeySequence::Undo)) {
			apply(core::undo_edit{ });
			return;
		}
		if (aEvent->matches(QKeySequence::Redo)) {
			apply(core::redo_edit{ });
			return;
		}
		if (aEvent->matches(QKeySequence::Copy) || aEvent->matches(QKeySequence::Cut)) {
			copy_selection(aEvent->matches(QKeySequence::Cut) && !ReadOnly);
			return;
		}
		if (aEvent->matches(QKeySequence::Paste)) {
			paste();
			return;
		}
		if (aEvent->matches(QKeySequence::SelectAll)) {
			apply(core::select_all{ });
			return;
		}
		// Home and End act on each caret's own line everywhere.
		if ((aEvent->key() == Qt::Key_Home || aEvent->key() == Qt::Key_End) && !alt && !modifiers.testFlag(Qt::ControlModifier)) {
			apply(core::move_carets{ .Motion = aEvent->key() == Qt::Key_Home ? core::text_motion::line_start : core::text_motion::line_end, .Extend = shift });
			return;
		}
		for (const auto& standard : standard_motions) {
			if (aEvent->matches(standard.Key)) {
				apply(core::move_carets{ .Motion = standard.Motion, .Extend = standard.Extend });
				return;
			}
		}
		if (aEvent->key() == Qt::Key_Escape) {
			if (collapsible()) {
				apply(core::collapse_carets{ });
				return;
			}
			aEvent->ignore();
			return;
		}
		if (ReadOnly) {
			// [[BPR-1157]]. Return and Shift+Return are the reader's: they make
			// a note or begin a suggestion where the caret is.
			const auto extra = modifiers & ~(Qt::ShiftModifier | Qt::KeypadModifier);
			if ((aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) && extra == Qt::NoModifier && Activated && Activated(shift)) {
				return;
			}
			QAbstractScrollArea::keyPressEvent(aEvent);
			return;
		}
		switch (aEvent->key()) {
		case Qt::Key_Return:
		case Qt::Key_Enter:     apply(core::insert_line_break{ }); return;
		case Qt::Key_Backspace: apply(core::delete_backward{ }); return;
		case Qt::Key_Delete:    apply(core::delete_forward{ }); return;
		case Qt::Key_Tab:       apply(core::insert_text{ "\t" }); return;
		default:                break;
		}
		const auto typed = aEvent->text();
		if (!typed.isEmpty() && std::ranges::all_of(typed, [](const QChar aCharacter) { return aCharacter.isPrint() || aCharacter.isSurrogate(); })) {
			apply(core::insert_text{ utf8(typed) });
			return;
		}
		QAbstractScrollArea::keyPressEvent(aEvent);
	}

	void suggestion_editor::inputMethodEvent(QInputMethodEvent* aEvent)
	{
		if (ReadOnly) {
			aEvent->ignore();
			return;
		}
		if (!aEvent->commitString().isEmpty()) {
			apply(core::insert_text{ utf8(aEvent->commitString()) });
		}
		if (Preedit != aEvent->preeditString()) {
			Preedit = aEvent->preeditString();
			viewport()->update();
		}
		aEvent->accept();
	}

	auto suggestion_editor::inputMethodQuery(const Qt::InputMethodQuery aQuery) const -> QVariant
	{
		const auto& state = Editor.state();
		const auto& head  = state.Carets[state.Primary].Head;
		switch (aQuery) {
		case Qt::ImEnabled:         return !ReadOnly;
		case Qt::ImCursorRectangle: return caret_rectangle();
		case Qt::ImFont:            return CodeFont;
		case Qt::ImHints:           return static_cast<int>(Qt::ImhMultiLine | Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
		case Qt::ImSurroundingText: return text_of(state.Lines[head.Line].Content);
		case Qt::ImCursorPosition:
		case Qt::ImAnchorPosition:  {
			const auto& content = state.Lines[head.Line].Content;
			const auto  offset  = Editor.locate(head).first;
			return static_cast<int>(text_of(std::string_view{ content }.substr(0, offset)).size());
		}
		default: return QAbstractScrollArea::inputMethodQuery(aQuery);
		}
	}

	void suggestion_editor::copy()
	{
		copy_selection(false);
	}

	void suggestion_editor::copy_selection(const bool aCut)
	{
		const auto copied = Editor.copy();
		if (copied.Text.empty()) {
			return;
		}
		auto* mime = new QMimeData{ };
		mime->setText(text_of(copied.Text));
		if (copied.Box) {
			mime->setData(QString::fromLatin1(box_mime_type), QByteArray{ "1" });
			// Visual Studio's marker, so a box copied here pastes as a box there.
			mime->setData(QStringLiteral("MSDEVColumnSelect"), QByteArray{ });
		}
		QApplication::clipboard()->setMimeData(mime);
		if (aCut) {
			// Replacing every selection with nothing removes exactly what was copied.
			apply(core::insert_text{ });
		}
	}

	void suggestion_editor::paste()
	{
		const auto* mime = QApplication::clipboard()->mimeData();
		if (ReadOnly || mime == nullptr || !mime->hasText()) {
			return;
		}
		const auto box = mime->hasFormat(QString::fromLatin1(box_mime_type)) || mime->hasFormat(QStringLiteral("MSDEVColumnSelect"));
		apply(core::paste_text{ .Text = utf8(mime->text()), .Box = box });
	}

	auto suggestion_editor::standard_menu(QWidget* aParent) -> QMenu*
	{
		auto* const menu      = new QMenu{ aParent };
		const auto  selected  = std::ranges::any_of(Editor.state().Carets, [](const core::text_caret& aCaret) { return !aCaret.empty(); });
		const auto  addAction = [&](const QString& aText, const QKeySequence::StandardKey aKey, const bool aEnabled, auto aDo) {
			auto* const action = menu->addAction(aText);
			action->setShortcut(QKeySequence{ aKey });
			action->setEnabled(aEnabled);
			connect(action, &QAction::triggered, this, aDo);
		};
		if (!ReadOnly) {
			addAction(QStringLiteral("&Undo"), QKeySequence::Undo, Editor.can_undo(), [this] { apply(core::undo_edit{ }); });
			addAction(QStringLiteral("&Redo"), QKeySequence::Redo, Editor.can_redo(), [this] { apply(core::redo_edit{ }); });
			menu->addSeparator();
			addAction(QStringLiteral("Cu&t"), QKeySequence::Cut, selected, [this] { copy_selection(true); });
		}
		addAction(QStringLiteral("&Copy"), QKeySequence::Copy, selected, [this] { copy_selection(false); });
		if (!ReadOnly) {
			const auto* mime = QApplication::clipboard()->mimeData();
			addAction(QStringLiteral("&Paste"), QKeySequence::Paste, mime != nullptr && mime->hasText(), [this] { paste(); });
		}
		menu->addSeparator();
		addAction(QStringLiteral("Select All"), QKeySequence::SelectAll, true, [this] { apply(core::select_all{ }); });
		return menu;
	}

	void suggestion_editor::mousePressEvent(QMouseEvent* aEvent)
	{
		if (aEvent->button() != Qt::LeftButton) {
			QAbstractScrollArea::mousePressEvent(aEvent);
			return;
		}
		setFocus(Qt::MouseFocusReason);
		const auto at = point_at(aEvent->position().toPoint());
		// A third click soon after a double click, where it was, selects the line.
		const auto distance = (aEvent->position() - DoubleClickedAt).manhattanLength();
		if (DoubleClicked.isValid() && DoubleClicked.elapsed() < QApplication::doubleClickInterval() && distance <= QApplication::styleHints()->mouseDoubleClickDistance()) {
			DoubleClicked.invalidate();
			apply(core::select_line_at{ at });
			hold(core::selection_unit::line);
			return;
		}
		if (aEvent->modifiers().testFlag(Qt::AltModifier)) {
			apply(core::add_caret{ at });
			Dragging = false;
		} else {
			apply(core::place_caret{ .At = at, .Extend = aEvent->modifiers().testFlag(Qt::ShiftModifier) });
			hold(core::selection_unit::character);
		}
	}

	void suggestion_editor::hold(const core::selection_unit aUnit)
	{
		DragOrigin = Editor.state().Carets[Editor.state().Primary];
		DragUnit   = aUnit;
		Dragging   = true;
	}

	void suggestion_editor::mouseDoubleClickEvent(QMouseEvent* aEvent)
	{
		if (aEvent->button() != Qt::LeftButton || aEvent->modifiers() != Qt::NoModifier) {
			mousePressEvent(aEvent);
			return;
		}
		DoubleClicked.start();
		DoubleClickedAt = aEvent->position();
		// The character under the pointer, not the nearer edge: a double
		// click on the right half of a word's last letter means that word.
		apply(core::select_word_at{ cell_at(aEvent->position().toPoint()) });
		hold(core::selection_unit::word);
	}

	void suggestion_editor::mouseMoveEvent(QMouseEvent* aEvent)
	{
		// [[BPR-1156]]. The pointer drifts while a hand holds the button: the
		// selection grows from what the press selected and never loses it, so a
		// double-clicked word stays whole (owner note on composer-layout v005).
		if (Dragging && aEvent->buttons().testFlag(Qt::LeftButton)) {
			const auto position = aEvent->position().toPoint();
			apply(core::drag_selection{ .Origin = DragOrigin, .To = DragUnit == core::selection_unit::character ? point_at(position) : cell_at(position), .Unit = DragUnit });
		}
	}

	void suggestion_editor::mouseReleaseEvent(QMouseEvent* aEvent)
	{
		if (aEvent->button() == Qt::LeftButton) {
			Dragging = false;
		}
		QAbstractScrollArea::mouseReleaseEvent(aEvent);
	}

	void suggestion_editor::resizeEvent(QResizeEvent* aEvent)
	{
		QAbstractScrollArea::resizeEvent(aEvent);
		update_scrollbars();
	}

	void suggestion_editor::focusInEvent(QFocusEvent* aEvent)
	{
		QAbstractScrollArea::focusInEvent(aEvent);
		viewport()->update();
	}

	void suggestion_editor::focusOutEvent(QFocusEvent* aEvent)
	{
		QAbstractScrollArea::focusOutEvent(aEvent);
		viewport()->update();
	}

	auto suggestion_editor::focusNextPrevChild(const bool aNext) -> bool
	{
		// Editable, Tab inserts a tab and Ctrl+Tab leaves (keyPressEvent).
		return ReadOnly ? QAbstractScrollArea::focusNextPrevChild(aNext) : false;
	}

	void suggestion_editor::changeEvent(QEvent* aEvent)
	{
		QAbstractScrollArea::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange) {
			measure();
			update_scrollbars();
		}
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::FontChange) {
			viewport()->update();
		}
	}

	auto suggestion_editor::sizeHint() const -> QSize
	{
		return { static_cast<int>(60.0 * ColumnWidth + 2.0 * document_margin()), static_cast<int>(std::ceil(RowHeight * 8.0)) + 2 * frameWidth() };
	}

	auto suggestion_editor::minimumSizeHint() const -> QSize
	{
		return { static_cast<int>(20.0 * ColumnWidth + 2.0 * document_margin()), static_cast<int>(std::ceil(RowHeight * 3.0)) + 2 * frameWidth() };
	}
}
