#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_EDITOR_HPP

#include "bpr/core/suggestion_text_editing.hpp"

#include <QAbstractScrollArea>
#include <QFont>
#include <QList>
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
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace bpr::qt {
	/// \brief A fixed-pitch code font: the platform's fixed font, falling
	/// back through common monospace families when that one is proportional.
	[[nodiscard]] auto suggestion_code_font() -> QFont;

	/// \brief The syntax colours of one editor line and the exact content
	/// they were classified from.
	struct suggestion_line_syntax
	{
		std::string                     Content;
		QList<QTextLayout::FormatRange> Formats;
	};

	/// \brief The code editor of Suggest change (BPR-1156): the whole file,
	/// edited in place in the main pane.
	///
	/// \details A small native editor over `core::suggestion_text_editor`: it
	/// paints lines in the code font with tabs at their stops, draws every
	/// caret and selection, and translates keys, IME composition, mouse
	/// gestures and the clipboard into semantic edits. It is not an IDE: no
	/// completion, file access or process, and it runs no lexer; syntax
	/// colours arrive already classified (`set_syntax`). Alt+Shift+Arrow
	/// grows a box selection on every platform; Home and End (with Shift)
	/// act on each caret's own line on every platform, alongside the
	/// platform's own line-start and line-end sequences; Tab inserts a tab
	/// and Ctrl+Tab moves focus. A box copy carries a marker so pasting it on
	/// one caret makes a box again.
	class suggestion_editor final : public QAbstractScrollArea
	{
	public:
		/// \brief The clipboard format that marks a box copy.
		static constexpr const char* box_mime_type = "application/x-bpr-box-selection";

		explicit suggestion_editor(QWidget* aParent = nullptr);

		/// \brief Replaces the text; history starts again and one caret sits at the start.
		void set_text(std::string_view aText, std::uint32_t aTabWidth = core::default_suggestion_tab_width);
		/// \brief The exact bytes being proposed.
		[[nodiscard]] auto text() const -> std::string;
		[[nodiscard]] auto model() const noexcept -> const core::suggestion_text_editor&;
		/// \brief The IME composition shown at the primary caret, not yet text.
		[[nodiscard]] auto preedit() const noexcept -> const QString&;
		/// \brief Uses the composed code typography; the palette stays native.
		void set_code_font(const QFont& aFont);
		/// \brief Colours lines from a classification of the text (BPR-1156).
		/// \details Formats are the shared code painter's ranges, in UTF-16
		/// units of each line. A line is coloured only by an entry whose
		/// content it still equals, at its own index or elsewhere, so a line
		/// typed since the classification stays plain until the next one
		/// arrives and a line that merely moved keeps its colours. New text
		/// clears the colours.
		void set_syntax(std::vector<suggestion_line_syntax> aLines);
		/// \brief Places the only caret by byte offsets of the text: a
		/// selection of [`aStart`, `aEnd`), or a caret when they are equal.
		void select_bytes(std::uint64_t aStart, std::uint64_t aEnd);
		/// \brief Shows `aLine` as the first visible line, then keeps the
		/// primary caret in view.
		void scroll_to_line(std::uint64_t aLine);
		/// \brief Applies one semantic edit as a gesture would.
		auto apply(const core::suggestion_edit& aEdit) -> bool;
		/// \brief The primary caret's rectangle in viewport coordinates.
		[[nodiscard]] auto caret_rectangle() const -> QRect;
		/// \brief The text position under a viewport point, in virtual space
		/// beyond a line's end.
		[[nodiscard]] auto point_at(const QPoint& aPosition) const -> core::text_point;

		/// \brief Called after every change of the text.
		std::function<void()> Changed;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		[[nodiscard]] auto inputMethodQuery(Qt::InputMethodQuery aQuery) const -> QVariant override;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		void inputMethodEvent(QInputMethodEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
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
		[[nodiscard]] auto line_height() const -> int;
		[[nodiscard]] auto column_width() const -> qreal;
		[[nodiscard]] auto column_x(std::uint64_t aColumn) const -> qreal;
		[[nodiscard]] auto syntax_of(std::uint64_t aLine) const -> const QList<QTextLayout::FormatRange>*;

		core::suggestion_text_editor                 Editor;
		QString                                      Preedit;
		QFont                                        CodeFont;
		std::vector<suggestion_line_syntax>          Syntax;
		std::unordered_map<std::string, std::size_t> SyntaxByContent;
	};
}

#endif
