#include "bpr/qt/suggestion_editor.hpp"

#include <QApplication>
#include <QClipboard>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int margin = 6;

		auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
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
	}

	auto suggestion_code_font() -> QFont
	{
		auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		font.setStyleHint(QFont::TypeWriter);
		font.setFixedPitch(true);
		if (!QFontInfo{ font }.fixedPitch()) {
			font.setFamilies({ font.family(), QStringLiteral("Menlo"), QStringLiteral("SF Mono"), QStringLiteral("Consolas"), QStringLiteral("Cascadia Mono"),
							   QStringLiteral("DejaVu Sans Mono"), QStringLiteral("Liberation Mono"), QStringLiteral("Noto Sans Mono"), QStringLiteral("Courier New") });
		}
		return font;
	}

	suggestion_editor::suggestion_editor(QWidget* aParent)
		: QAbstractScrollArea{ aParent }
		, CodeFont{ suggestion_code_font() }
	{
		setObjectName(QStringLiteral("suggestionEditor"));
		setAccessibleName(QStringLiteral("Suggested code"));
		setAccessibleDescription(QStringLiteral("Edit the proposed code. Alt+Shift+Arrow makes a box selection; Ctrl+Tab leaves the editor."));
		setAttribute(Qt::WA_InputMethodEnabled);
		setFocusPolicy(Qt::StrongFocus);
		viewport()->setCursor(Qt::IBeamCursor);
		setFont(CodeFont);
		update_scrollbars();
	}

	void suggestion_editor::set_text(const std::string_view aText, const std::uint32_t aTabWidth)
	{
		Editor = core::suggestion_text_editor{ aText, aTabWidth };
		Preedit.clear();
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

	auto suggestion_editor::preedit() const noexcept -> const QString&
	{
		return Preedit;
	}

	void suggestion_editor::set_code_font(const QFont& aFont)
	{
		CodeFont = aFont;
		setFont(CodeFont);
		update_scrollbars();
		viewport()->update();
		updateGeometry();
	}

	auto suggestion_editor::apply(const core::suggestion_edit& aEdit) -> bool
	{
		const auto before  = Editor.text();
		const auto changed = Editor.apply(aEdit);
		if (changed) {
			update_scrollbars();
			ensure_visible();
			viewport()->update();
			if (Editor.text() != before && Changed) {
				Changed();
			}
		}
		return changed;
	}

	auto suggestion_editor::line_height() const -> int
	{
		return std::max(1, QFontMetrics{ CodeFont, viewport() }.lineSpacing());
	}

	auto suggestion_editor::column_width() const -> qreal
	{
		return std::max<qreal>(1.0, QFontMetricsF{ CodeFont, viewport() }.horizontalAdvance(QLatin1Char('M')));
	}

	auto suggestion_editor::column_x(const std::uint64_t aColumn) const -> qreal
	{
		return margin + static_cast<qreal>(aColumn) * column_width() - horizontalScrollBar()->value();
	}

	void suggestion_editor::update_scrollbars()
	{
		const auto& state   = Editor.state();
		const auto  visible = std::max(1, viewport()->height() / line_height());
		verticalScrollBar()->setRange(0, std::max(0, static_cast<int>(state.Lines.size()) - visible));
		verticalScrollBar()->setPageStep(visible);
		auto widest = std::uint64_t{ 0 };
		for (std::uint64_t line = 0; line < state.Lines.size(); ++line) {
			widest = std::max(widest, Editor.line_width(line));
		}
		for (const auto& caret : state.Carets) {
			widest = std::max({ widest, caret.Head.Column, caret.Anchor.Column });
		}
		const auto content = static_cast<int>(std::ceil(static_cast<qreal>(widest + 2U) * column_width())) + 2 * margin;
		horizontalScrollBar()->setRange(0, std::max(0, content - viewport()->width()));
		horizontalScrollBar()->setPageStep(viewport()->width());
	}

	void suggestion_editor::ensure_visible()
	{
		const auto& head    = Editor.state().Carets[Editor.state().Primary].Head;
		const auto  visible = std::max(1, viewport()->height() / line_height());
		const auto  line    = static_cast<int>(head.Line);
		if (line < verticalScrollBar()->value()) {
			verticalScrollBar()->setValue(line);
		} else if (line >= verticalScrollBar()->value() + visible) {
			verticalScrollBar()->setValue(line - visible + 1);
		}
		const auto x = static_cast<int>(static_cast<qreal>(head.Column) * column_width()) + margin;
		if (x < horizontalScrollBar()->value()) {
			horizontalScrollBar()->setValue(std::max(0, x - margin));
		} else if (x > horizontalScrollBar()->value() + viewport()->width() - 2 * margin) {
			horizontalScrollBar()->setValue(x - viewport()->width() + 2 * margin);
		}
	}

	auto suggestion_editor::caret_rectangle() const -> QRect
	{
		const auto& head = Editor.state().Carets[Editor.state().Primary].Head;
		const auto  y    = (static_cast<int>(head.Line) - verticalScrollBar()->value()) * line_height();
		return QRect{ static_cast<int>(std::floor(column_x(head.Column))), y, 2, line_height() };
	}

	auto suggestion_editor::point_at(const QPoint& aPosition) const -> core::text_point
	{
		const auto lines  = static_cast<int>(Editor.state().Lines.size());
		const auto line   = std::clamp(verticalScrollBar()->value() + aPosition.y() / line_height(), 0, lines - 1);
		const auto offset = (static_cast<qreal>(aPosition.x()) + horizontalScrollBar()->value() - margin) / column_width();
		return { static_cast<std::uint64_t>(line), static_cast<std::uint64_t>(std::max<qreal>(0.0, std::round(offset))) };
	}

	void suggestion_editor::paintEvent(QPaintEvent* aEvent)
	{
		static_cast<void>(aEvent);
		auto        painter = QPainter{ viewport() };
		const auto& state   = Editor.state();
		const auto  height  = line_height();
		const auto  first   = static_cast<std::uint64_t>(verticalScrollBar()->value());
		const auto  last    = std::min<std::uint64_t>(state.Lines.size(), first + static_cast<std::uint64_t>(viewport()->height() / height) + 2U);
		const auto  ascent  = QFontMetrics{ CodeFont, viewport() }.ascent();
		painter.setFont(CodeFont);
		painter.fillRect(viewport()->rect(), palette().base());

		// Selections: each caret's range, virtual space included, then the text over them.
		auto selection = palette().highlight().color();
		selection.setAlpha(hasFocus() ? 110 : 60);
		for (const auto& caret : state.Carets) {
			if (caret.empty()) {
				continue;
			}
			const auto start = std::min(caret.Anchor, caret.Head);
			const auto end   = std::max(caret.Anchor, caret.Head);
			for (auto line = std::max(start.Line, first); line <= end.Line && line < last; ++line) {
				const auto from = line == start.Line ? start.Column : 0U;
				// A selection that continues past a line includes its line break.
				const auto to = line == end.Line ? end.Column : Editor.line_width(line) + 1U;
				const auto y  = static_cast<int>(line - first) * height;
				painter.fillRect(QRectF{ column_x(from), static_cast<qreal>(y), column_x(to) - column_x(from), static_cast<qreal>(height) }, selection);
			}
		}
		// One glyph per column cell, so carets, selections and boxes line up
		// whatever the font's own advances are.
		painter.setPen(palette().text().color());
		const auto tab = state.TabWidth;
		for (auto line = first; line < last; ++line) {
			const auto  y       = static_cast<qreal>(static_cast<int>(line - first) * height + ascent);
			const auto& content = state.Lines[line].Content;
			auto        column  = std::uint64_t{ 0 };
			for (std::size_t index = 0; index < content.size();) {
				if (content[index] == '\t') {
					column += tab - (column % tab);
					++index;
					continue;
				}
				auto length = std::size_t{ 1 };
				while (index + length < content.size() && (static_cast<unsigned char>(content[index + length]) & 0xC0U) == 0x80U && length < 4U) {
					++length;
				}
				painter.drawText(QPointF{ column_x(column), y }, QString::fromUtf8(content.data() + index, static_cast<qsizetype>(length)));
				index += length;
				++column;
			}
		}
		// Carets, and the IME composition at the primary one.
		const auto caretColour = hasFocus() ? palette().text().color() : palette().placeholderText().color();
		for (std::size_t index = 0; index < state.Carets.size(); ++index) {
			const auto& head = state.Carets[index].Head;
			if (head.Line < first || head.Line >= last) {
				continue;
			}
			const auto y = static_cast<int>(head.Line - first) * height;
			if (index == state.Primary && !Preedit.isEmpty()) {
				const auto width = QFontMetricsF{ CodeFont, viewport() }.horizontalAdvance(Preedit);
				painter.fillRect(QRectF{ column_x(head.Column), static_cast<qreal>(y), width, static_cast<qreal>(height) }, palette().alternateBase());
				painter.drawText(QPointF{ column_x(head.Column), static_cast<qreal>(y + ascent) }, Preedit);
				painter.drawLine(QPointF{ column_x(head.Column), static_cast<qreal>(y + height - 1) }, QPointF{ column_x(head.Column) + width, static_cast<qreal>(y + height - 1) });
			}
			painter.fillRect(QRectF{ column_x(head.Column) - 1.0, static_cast<qreal>(y), index == state.Primary ? 2.0 : 1.5, static_cast<qreal>(height) }, caretColour);
		}
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
		if ((aEvent->key() == Qt::Key_Tab || aEvent->key() == Qt::Key_Backtab) && modifiers.testFlag(Qt::ControlModifier)) {
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
			copy_selection(aEvent->matches(QKeySequence::Cut));
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
		switch (aEvent->key()) {
		case Qt::Key_Return:
		case Qt::Key_Enter:     apply(core::insert_line_break{ }); return;
		case Qt::Key_Backspace: apply(core::delete_backward{ }); return;
		case Qt::Key_Delete:    apply(core::delete_forward{ }); return;
		case Qt::Key_Tab:       apply(core::insert_text{ "\t" }); return;
		case Qt::Key_Escape:
			if (Editor.state().Carets.size() > 1U || !Editor.state().Carets.front().empty()) {
				apply(core::collapse_carets{ });
				return;
			}
			aEvent->ignore();
			return;
		default: break;
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
		case Qt::ImEnabled:         return true;
		case Qt::ImCursorRectangle: return caret_rectangle();
		case Qt::ImFont:            return CodeFont;
		case Qt::ImHints:           return static_cast<int>(Qt::ImhMultiLine | Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
		case Qt::ImSurroundingText: {
			const auto& content = state.Lines[head.Line].Content;
			return QString::fromUtf8(content.data(), static_cast<qsizetype>(content.size()));
		}
		case Qt::ImCursorPosition:
		case Qt::ImAnchorPosition: {
			const auto& content = state.Lines[head.Line].Content;
			const auto  offset  = Editor.locate(head).first;
			return static_cast<int>(QString::fromUtf8(content.data(), static_cast<qsizetype>(offset)).size());
		}
		default: return QAbstractScrollArea::inputMethodQuery(aQuery);
		}
	}

	void suggestion_editor::copy_selection(const bool aCut)
	{
		const auto copied = Editor.copy();
		if (copied.Text.empty()) {
			return;
		}
		auto* mime = new QMimeData{ };
		mime->setText(QString::fromUtf8(copied.Text.data(), static_cast<qsizetype>(copied.Text.size())));
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
		if (mime == nullptr || !mime->hasText()) {
			return;
		}
		const auto box = mime->hasFormat(QString::fromLatin1(box_mime_type)) || mime->hasFormat(QStringLiteral("MSDEVColumnSelect"));
		apply(core::paste_text{ .Text = utf8(mime->text()), .Box = box });
	}

	void suggestion_editor::mousePressEvent(QMouseEvent* aEvent)
	{
		if (aEvent->button() != Qt::LeftButton) {
			QAbstractScrollArea::mousePressEvent(aEvent);
			return;
		}
		setFocus(Qt::MouseFocusReason);
		const auto at = point_at(aEvent->position().toPoint());
		if (aEvent->modifiers().testFlag(Qt::AltModifier)) {
			apply(core::add_caret{ at });
		} else {
			apply(core::place_caret{ .At = at, .Extend = aEvent->modifiers().testFlag(Qt::ShiftModifier) });
		}
	}

	void suggestion_editor::mouseMoveEvent(QMouseEvent* aEvent)
	{
		if (aEvent->buttons().testFlag(Qt::LeftButton) && !aEvent->modifiers().testFlag(Qt::AltModifier)) {
			apply(core::place_caret{ .At = point_at(aEvent->position().toPoint()), .Extend = true });
		}
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
		// Tab inserts a tab; Ctrl+Tab leaves (keyPressEvent).
		static_cast<void>(aNext);
		return false;
	}

	void suggestion_editor::changeEvent(QEvent* aEvent)
	{
		QAbstractScrollArea::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::FontChange) {
			viewport()->update();
		}
	}

	auto suggestion_editor::sizeHint() const -> QSize
	{
		return { static_cast<int>(60.0 * column_width()) + 2 * margin, line_height() * 8 + 2 * frameWidth() };
	}

	auto suggestion_editor::minimumSizeHint() const -> QSize
	{
		return { static_cast<int>(20.0 * column_width()) + 2 * margin, line_height() * 3 + 2 * frameWidth() };
	}
}
