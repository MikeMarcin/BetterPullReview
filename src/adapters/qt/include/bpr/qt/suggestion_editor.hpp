#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_EDITOR_HPP

#include "bpr/core/appearance_model.hpp"
#include "bpr/core/suggestion_text_editing.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QAbstractScrollArea>
#include <QElapsedTimer>
#include <QFont>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QRawFont>
#include <QString>
#include <QTextLayout>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class QInputMethodEvent;
class QKeyEvent;
class QMenu;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace bpr::qt {
	/// \brief A fixed-pitch code font: the platform's fixed font, falling
	/// back through common monospace families when that one is proportional.
	[[nodiscard]] auto suggestion_code_font() -> QFont;

	/// \brief `aFont` when it is fixed pitch; otherwise the same size and
	/// weight falling back through common monospace families.
	/// \details The code view gives every column one cell, which only a
	/// fixed-pitch face fills evenly.
	[[nodiscard]] auto fixed_pitch_code_font(QFont aFont) -> QFont;

	/// \brief The syntax colours of one editor line and the exact content
	/// they were classified from.
	struct suggestion_line_syntax
	{
		std::string                     Content;
		QList<QTextLayout::FormatRange> Formats;
	};

	/// \brief Every line of `aText` with the format ranges of its classification.
	/// \details The one mapping from a whole-text classification to the code
	/// view's lines, for the cycle page's file and for the text Suggest
	/// change edits, so the two colour a line identically.
	[[nodiscard]] auto suggestion_syntax_lines(const classified_text& aClassified, std::string_view aText, const code_role_colors& aColors) -> std::vector<suggestion_line_syntax>;

	/// \brief The code view of the cycle page: the file read, and the same
	/// file edited in place by Suggest change (BPR-1156).
	///
	/// \details A small native editor over `core::suggestion_text_editor`: it
	/// paints lines in the code font with tabs at their stops, draws every
	/// caret and selection, and translates keys, IME composition, mouse
	/// gestures and the clipboard into semantic edits. Read-only, it is the
	/// file pane: every selection and movement gesture works and nothing
	/// changes the text. Editable, it is the same widget with the same
	/// rendering, so starting Suggest change changes nothing on screen but
	/// the caret (owner rejection of composer-layout v004, 2026-10-02).
	///
	/// Rows have the review panes' pitch and left margin (a laid-out line of
	/// the code font, leading included, and the text document's margin). A
	/// text selection is filled with `QPalette::Highlight`, solid, as every
	/// text box in the window fills one, and ends at the line's text plus one
	/// cell for a selected line break; the glyphs on it keep their syntax
	/// colours, moved only as far as `core::readable_selection_ink` needs for
	/// the variant's text floor (owner note on composer-layout v005). It is
	/// not an IDE: no completion, file access or process, and it runs no
	/// lexer; syntax colours arrive already classified (`set_syntax`).
	/// Alt+Shift+Arrow grows a box selection on every platform, and only a box
	/// reaches past a line's end; Home and End (with Shift) act on each
	/// caret's own line on every platform, alongside the platform's own
	/// line-start and line-end sequences; a double click selects a word and a
	/// triple click a line, and dragging on grows the selection by words or
	/// lines; Tab inserts a tab and Ctrl+Tab moves focus; Enter keeps the
	/// indentation (`set_indentation`). A box copy carries a marker so
	/// pasting it on one caret makes a box again.
	class suggestion_editor final : public QAbstractScrollArea
	{
	public:
		/// \brief The clipboard format that marks a box copy.
		static constexpr const char* box_mime_type = "application/x-bpr-box-selection";

		explicit suggestion_editor(QWidget* aParent = nullptr);

		/// \brief Replaces the text; history starts again and one caret sits at the start.
		/// \details The same bytes again keep their colours and the scroll position.
		void set_text(std::string_view aText, std::uint32_t aTabWidth = core::default_suggestion_tab_width);
		/// \brief The exact bytes being shown or proposed.
		[[nodiscard]] auto text() const -> std::string;
		[[nodiscard]] auto model() const noexcept -> const core::suggestion_text_editor&;
		/// \brief Read-only: only selection and movement gestures apply.
		void               set_read_only(bool aReadOnly);
		[[nodiscard]] auto read_only() const noexcept -> bool;
		/// \brief How Enter indents (`core::indentation_for`).
		void set_indentation(core::text_indentation aIndentation);
		/// \brief The IME composition shown at the primary caret, not yet text.
		[[nodiscard]] auto preedit() const noexcept -> const QString&;
		/// \brief Uses the composed code typography; the palette stays native.
		void set_code_font(const QFont& aFont);
		/// \brief The appearance variant whose text floor selected glyphs meet.
		void set_contrast_variant(core::appearance_variant aVariant);
		/// \brief Colours lines from a classification of the text (BPR-1156).
		/// \details Formats are the shared code painter's ranges, in UTF-16
		/// units of each line. A line is coloured only by an entry whose
		/// content it still equals, at its own index or elsewhere, so a line
		/// typed since the classification stays plain until the next one
		/// arrives and a line that merely moved keeps its colours. Other text
		/// clears the colours.
		void set_syntax(std::vector<suggestion_line_syntax> aLines);
		/// \brief Places the only caret by byte offsets of the text: a
		/// selection of [`aStart`, `aEnd`), or a caret when they are equal.
		void select_bytes(std::uint64_t aStart, std::uint64_t aEnd);
		/// \brief The primary caret's selection as byte offsets of the text,
		/// start first; equal when it is only a caret.
		[[nodiscard]] auto selected_bytes() const -> std::pair<std::uint64_t, std::uint64_t>;
		/// \brief Whether the primary caret selects anything.
		[[nodiscard]] auto has_selection() const -> bool;
		/// \brief Shows `aLine` as the first visible line; the carets stay where they are.
		void scroll_to_line(std::uint64_t aLine);
		/// \brief The first visible line.
		[[nodiscard]] auto first_visible_line() const -> std::uint64_t;
		/// \brief Applies one semantic edit as a gesture would; read-only, an
		/// edit that could change the text is refused.
		auto apply(const core::suggestion_edit& aEdit) -> bool;
		/// \brief The primary caret's rectangle in viewport coordinates.
		[[nodiscard]] auto caret_rectangle() const -> QRect;
		/// \brief The cell of one column of one line, in viewport coordinates.
		[[nodiscard]] auto cell_rectangle(const core::text_point& aPoint) const -> QRectF;
		/// \brief The text position under a viewport point, in virtual space
		/// beyond a line's end; a gesture other than the box places a caret
		/// at the line's end instead.
		[[nodiscard]] auto point_at(const QPoint& aPosition) const -> core::text_point;
		/// \brief The cell under a viewport point: the character a double
		/// click or a word drag means, where `point_at` is the nearer edge.
		[[nodiscard]] auto cell_at(const QPoint& aPosition) const -> core::text_point;
		/// \brief Undo, Redo, Cut, Copy, Paste and Select All when editable;
		/// Copy and Select All when read-only. The caller owns the menu.
		[[nodiscard]] auto standard_menu(QWidget* aParent) -> QMenu*;
		/// \brief Copies every selection to the clipboard.
		void copy();

		/// \brief Called after every change of the text.
		std::function<void()> Changed;
		/// \brief Called after any caret or selection moved.
		std::function<void()> SelectionChanged;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		[[nodiscard]] auto inputMethodQuery(Qt::InputMethodQuery aQuery) const -> QVariant override;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		void inputMethodEvent(QInputMethodEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseDoubleClickEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void focusOutEvent(QFocusEvent* aEvent) override;
		auto focusNextPrevChild(bool aNext) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void               copy_selection(bool aCut);
		void               paste();
		void               update_scrollbars();
		void               ensure_visible();
		void               measure();
		void               measure_lines();
		void               announce_selection();
		[[nodiscard]] auto row_height() const -> qreal;
		[[nodiscard]] auto visible_rows() const -> int;
		[[nodiscard]] auto column_x(std::uint64_t aColumn) const -> qreal;
		[[nodiscard]] auto syntax_of(std::uint64_t aLine) const -> const QList<QTextLayout::FormatRange>*;
		[[nodiscard]] auto selection_ink(const QColor& aColour, const QColor& aFill) const -> QColor;
		/// Remembers what a press selected, for a drag with the button held.
		void hold(core::selection_unit aUnit);

		core::suggestion_text_editor                 Editor;
		QString                                      Preedit;
		QFont                                        CodeFont;
		std::vector<suggestion_line_syntax>          Syntax;
		std::unordered_map<std::string, std::size_t> SyntaxByContent;
		bool                                         ReadOnly = false;
		/// The widest line in columns, measured when the text changes rather than per gesture.
		std::uint64_t WidestLine = 0U;
		/// The pitch, baseline and cell width of the code font, as the review panes lay it out.
		qreal RowHeight   = 1.0;
		qreal Ascent      = 0.0;
		qreal ColumnWidth = 1.0;
		/// \brief [[BPR-1156]]. One character shaped alone in the code font, as a
		/// one-character `drawText` shapes it, kept so a paint does not shape it again.
		struct shaped_glyph
		{
			QRawFont Font;
			quint32  Index = 0U;
			/// The glyph's offset from the cell's baseline origin.
			QPointF Offset;
			/// False when the character needs more than one glyph or a second font;
			/// such a character is drawn with `drawText` as before.
			bool Single = false;
		};
		mutable QHash<QString, shaped_glyph> ShapedGlyphs;
		mutable qreal                        ShapedRatio = 0.0;
		[[nodiscard]] auto                   shaped(const QString& aGlyph) const -> const shaped_glyph&;
		/// Where and when the last double click was, so a third click selects the line.
		QElapsedTimer DoubleClicked;
		QPointF       DoubleClickedAt;
		/// What the press under a held button selected, and the unit a drag grows it by.
		core::text_caret         DragOrigin;
		core::selection_unit     DragUnit = core::selection_unit::character;
		bool                     Dragging = false;
		core::appearance_variant Variant  = core::appearance_variant::light;
		/// The ink of each colour on the selection fill, for the fill in `InkFill`.
		mutable QHash<QRgb, QColor> SelectionInk;
		mutable QRgb                InkFill = 0U;
	};
}

#endif
