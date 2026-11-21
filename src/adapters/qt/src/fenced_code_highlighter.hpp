#ifndef BPR_SRC_ADAPTERS_QT_SRC_FENCED_CODE_HIGHLIGHTER_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_FENCED_CODE_HIGHLIGHTER_HPP

#include <QColor>
#include <QFont>
#include <QSyntaxHighlighter>

class QTextBlock;
class QTextDocument;

/// \file
/// \brief [[BPR-2353]] (owner, 2026-11-21: "as soon as you make a
/// ```suggestion block in a comment draft that whole comment gets rendered as
/// an un-word-wrapped block including things outside of the code block").
/// A comment field's fenced code blocks, of any language and `suggestion`
/// alike, styled on their own lines only: the fences quiet and the lines
/// between them in the code font. The words around a block keep the field's
/// font. The field keeps one wrap mode for all of it; the field paints the
/// block's ground from `role_of`.
namespace bpr::qt {
	/// \brief Styles each fenced code block in one comment field's document.
	///
	/// \details A fence is three or more backticks or tildes after at most
	/// three spaces, as GitHub reads one; it closes on a line of the same
	/// character at least as long and nothing else. A block left open runs to
	/// the end of the text, as GitHub renders it. The highlighter changes only
	/// how lines are laid out and drawn, never the document's text, so the
	/// field's `textChanged` sees nothing from it.
	class fenced_code_highlighter final : public QSyntaxHighlighter
	{
	public:
		/// \brief What one line of the field is.
		enum class line_role : unsigned char {
			prose,
			opening_fence,
			code,
			closing_fence,
		};

		/// \brief Styles `aDocument`'s fenced blocks in `aCodeFont`, their
		/// fences in `aQuiet`.
		fenced_code_highlighter(QTextDocument* aDocument, const QFont& aCodeFont, const QColor& aQuiet);

		/// \brief Uses `aFont` for code lines and `aQuiet` for fences from now
		/// on, restyling the text when either differs. The field calls it as
		/// its font or palette changes.
		void set_style(const QFont& aFont, const QColor& aQuiet);

		/// \brief What `aBlock` is, as this highlighter last styled it;
		/// `prose` for a block it has not seen.
		[[nodiscard]] static auto role_of(const QTextBlock& aBlock) -> line_role;

	protected:
		void highlightBlock(const QString& aText) override;

	private:
		QFont  CodeFont;
		QColor Quiet;
	};
} // namespace bpr::qt

#endif
