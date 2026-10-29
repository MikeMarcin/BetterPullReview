#ifndef BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_CODE_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_CODE_HPP

#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/markdown_blob_snippet.hpp"
#include "bpr/core/markdown_code.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QString>
#include <QTextFormat>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class QPainter;
class QPalette;
class QTextBlock;
class QTextDocument;

/// \file
/// \brief RFC-BPR-0036 owner amendment 2 ([[BPR-1562]], [[BPR-1563]],
/// [[BPR-1564]]). Fenced code in the reader as GitHub draws it: language
/// colours from the review panes' classifier, `diff` blocks marked by line,
/// and suggestions as GitHub's Suggested change.
/// \details The core finds the blocks and decides their language, marks and
/// rows; this unit finds each block in the imported document by comparing its
/// lines, so a block it cannot place exactly stays plain, and draws them.
/// Colours are layout formats, never document text: the source, copy and
/// geometry of a block are what the import made, apart from a suggestion's
/// header and removed rows, which presentation adds.
namespace bpr::qt::detail {
	/// Block property: the fenced block, by ordinal, a block belongs to.
	inline constexpr auto markdown_code_ordinal_property = QTextFormat::UserProperty + 0x0B60;
	/// Block property: the block's line within its block's text.
	inline constexpr auto markdown_code_line_property = QTextFormat::UserProperty + 0x0B61;
	/// Block property: a suggestion row: `markdown_code_row`.
	inline constexpr auto markdown_code_row_property = QTextFormat::UserProperty + 0x0B62;
	/// Block property: a suggestion row's file line number.
	inline constexpr auto markdown_code_number_property = QTextFormat::UserProperty + 0x0B63;

	/// Block property: a suggestion row's gutter width, and of that its numbers'.
	inline constexpr auto markdown_code_gutter_property  = QTextFormat::UserProperty + 0x0B64;
	inline constexpr auto markdown_code_numbers_property = QTextFormat::UserProperty + 0x0B65;

	enum class markdown_code_row : int {
		none = 0,
		removed,
		added,
		header,
		/// [[BPR-1802]]. A line of a permalink's snippet box.
		snippet,
	};

	/// \brief [[BPR-1802]]. A permalink shown as GitHub's snippet box: its
	/// fence's info names it by its place in the reader's list.
	struct markdown_code_snippet
	{
		core::markdown_blob_permalink Permalink;
		/// The permalink as written, which the title links.
		QString Address;
		/// The commit's page, which the short commit links.
		QString CommitAddress;
	};

	/// \brief The colours fenced code is drawn in: the theme's roles, held
	/// readable on the reader's code background.
	struct markdown_code_colours
	{
		QColor Back;
		QColor Normal;
		QColor AddedFill;
		QColor AddedInk;
		QColor AddedGutter;
		QColor AddedWord;
		QColor RemovedFill;
		QColor RemovedInk;
		QColor RemovedGutter;
		QColor RemovedWord;
		QColor ChangedFill;
		QColor ChangedInk;
		QColor Range;
		QColor Header;
		QColor Rule;
		QColor Muted;
		/// [[BPR-1802]]. A snippet header's links, readable on the code background.
		QColor Link;
		/// 4.5, or 7 in a high-contrast theme.
		double Target = 4.5;
	};

	[[nodiscard]] auto markdown_code_colours_of(const markdown_code_theme& aTheme, const QPalette& aPalette) -> markdown_code_colours;

	/// \brief The fenced blocks of one rendered body and what was learned
	/// about them.
	class markdown_code_store final
	{
	public:
		struct block
		{
			core::markdown_fenced_code   Fence;
			core::markdown_code_language Language;
			/// Its lines were found in the document, in order.
			bool Placed = false;
			/// Shown as a Suggested change.
			bool Suggestion = false;
			/// The file name its text is classified by.
			std::string                        Selector;
			std::shared_ptr<const std::string> Text;
			application::shared_token_spans    Spans;
			/// A suggestion's commented lines, and their spans.
			std::shared_ptr<const std::string>         Removed;
			application::shared_token_spans            RemovedSpans;
			std::vector<core::markdown_suggestion_row> Rows;
			bool                                       Numbered = false;
			/// [[BPR-1802]]. A permalink's snippet box, by its place in the
			/// reader's list.
			std::optional<markdown_code_snippet> Snippet;
		};

		/// \brief Before the import: an empty suggestion gets one marked line,
		/// so the import keeps a block where its rows go. Nothing changes when
		/// suggestions are not shown or the body holds the mark.
		static void prepare(QString& aText, bool aSuggestions);

		/// \brief Owner look v004 ([[BPR-1753]]): the file name inline
		/// code is classified as, such as `code.cpp`; empty, the default,
		/// leaves inline code plain. Kept through `clear` and `place`.
		void               set_inline_selector(std::string aSelector);
		[[nodiscard]] auto inline_selector() const noexcept -> const std::string& { return InlineSelector; }
		/// \brief The inline code texts of the body, once each, and their spans.
		struct inline_code
		{
			std::shared_ptr<const std::string> Text;
			application::shared_token_spans    Spans;
		};
		[[nodiscard]] auto inline_code_texts() const noexcept -> const std::vector<inline_code>& { return Inline; }

		/// \brief After the import: finds every fenced block's lines, and
		/// shows each suggestion when `aSuggestions` says this body is a line
		/// comment.
		/// [[BPR-1802]]: and each fence `aSnippets` names as a snippet box.
		void place(QTextDocument& aDocument, const QString& aPrepared, const std::optional<markdown_suggestion_presentation>& aSuggestions,
				   std::span<const markdown_code_snippet> aSnippets = { });
		void clear();

		/// \brief Every text to classify, once each.
		[[nodiscard]] auto requests() const -> std::vector<application::markdown_code_syntax_request>;
		/// \brief Keeps the spans of every block showing that text.
		/// \returns Whether any block took them.
		auto apply(const application::markdown_code_syntax_publication& aSpans) -> bool;

		/// \brief Colours and lays out every placed block. Idempotent: run after
		/// every other format refresh.
		void style(QTextDocument& aDocument, const markdown_code_colours& aColours, const code_role_colors& aRoles, core::appearance_variant aVariant, const QFont& aFont, const QFont& aCode);

		/// \brief Paints the suggestion rows' fills, under the text.
		void paint_under(QPainter& aPainter, const QTextDocument& aDocument, QPointF aOffset) const;
		/// \brief Paints the suggestion boxes' numbers, signs and outline.
		void paint_over(QPainter& aPainter, const QTextDocument& aDocument, QPointF aOffset) const;

		[[nodiscard]] auto blocks() const noexcept -> const std::vector<block>& { return Blocks; }
		[[nodiscard]] auto suggestions() const noexcept -> bool;
		/// \brief [[BPR-1802]]. Whether any block is a snippet box.
		[[nodiscard]] auto snippets() const noexcept -> bool;

	private:
		struct box
		{
			int   Ordinal = 0;
			qreal Left    = 0.0;
			qreal Right   = 0.0;
			qreal Top     = 0.0;
			qreal Rule    = 0.0;
			qreal Bottom  = 0.0;
			qreal Gutter  = 0.0;
			qreal Numbers = 0.0;
			bool  Snippet = false;
		};
		[[nodiscard]] auto boxes(const QTextDocument& aDocument) const -> std::vector<box>;
		/// [[BPR-1802]]. One block of a snippet box: its header or a line.
		void style_snippet(QTextDocument& aDocument, const QTextBlock& aBlock, const block& aEntry, std::uint32_t aIndex, const code_role_colors& aRoles, const QFont& aFont, qreal aDigit) const;

		/// Each inline code run outside fenced code: the fragments' text.
		void                     place_inline(const QTextDocument& aDocument);
		void                     style_inline(QTextDocument& aDocument, const code_role_colors& aRoles) const;
		std::vector<block>       Blocks;
		std::string              InlineSelector;
		std::vector<inline_code> Inline;
		markdown_code_colours    Colours;
		QFont                    CodeFont;
	};
} // namespace bpr::qt::detail

#endif
