#ifndef BPR_SRC_ADAPTERS_QT_SRC_COMPOSER_CODE_LAYOUT_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_COMPOSER_CODE_LAYOUT_HPP

#include <QPlainTextDocumentLayout>

#include <cstddef>
#include <optional>
#include <vector>

class QTextBlock;
class QTextDocument;

/// \file
/// \brief [[BPR-2354]] (owner, 2026-11-21: "Can we not do the nested code
/// block element like we do for agent replies inside the composer?"). The
/// comment field's layout: its words wrap at the field's width, as a plain
/// text field's do, and each fenced code block is laid out as the agent
/// reply's code box holds code, unwrapped and scrolling sideways on its own.
/// The field never scrolls sideways.
namespace bpr::qt {
	/// \brief A plain text layout whose fenced code blocks are unwrapped
	/// boxes, each with its own sideways scroll.
	///
	/// \details The document stays one plain text document: one undo stack,
	/// one selection, one caret, input methods and accessibility as any
	/// `QPlainTextEdit` has them, and its text exactly as written, fences
	/// included. Which lines are code is the `fenced_code_highlighter`'s
	/// reading of the same document. A code line is laid out on one line,
	/// and a block's sideways scroll moves its lines' x position, so drawing,
	/// hit testing, the caret and the selection all agree. The field paints
	/// the box behind the lines (`runs`) and hosts each block's scroll bar.
	class composer_code_layout final : public QPlainTextDocumentLayout
	{
	public:
		/// \brief The code's inset from the box's sides, as the agent
		/// reply's code box has it.
		static constexpr qreal inset = 10.0;
		/// \brief Room above the header row's text and under it, and under
		/// the last row.
		static constexpr qreal pad = 4.0;

		/// \brief One fenced block: its first and last block numbers (its
		/// fences when it has them), the widest line's natural width, and
		/// how far it scrolls.
		struct code_run
		{
			int   First   = 0;
			int   Last    = 0;
			bool  Closed  = false;
			qreal Natural = 0.0;
			qreal Scroll  = 0.0;
			qreal Room    = 0.0;
		};

		explicit composer_code_layout(QTextDocument* aDocument);

		/// \brief The field's viewport width: the box's width.
		void set_box_width(qreal aWidth);
		/// \brief The height a block's scroll bar takes under its last code
		/// line, when it has one.
		void set_bar_extent(qreal aExtent);

		/// \brief The document's fenced blocks, in order.
		[[nodiscard]] auto runs() const -> const std::vector<code_run>&;
		/// \brief The block `aBlockNumber` is in, if it is in one.
		[[nodiscard]] auto run_of(int aBlockNumber) const -> std::optional<std::size_t>;
		/// \brief Scrolls block `aRun` to `aOffset`, held within its room.
		void set_scroll(std::size_t aRun, qreal aOffset);
		/// \brief Scrolls the block holding `aBlock` so that `aX`, a position
		/// in its line as laid out now, shows inside the box.
		void reveal(const QTextBlock& aBlock, qreal aX);
		/// \brief Scrolls every block back to its lines' first column.
		void show_line_starts();

		[[nodiscard]] auto blockBoundingRect(const QTextBlock& aBlock) const -> QRectF override;
		/// \brief The document's size, never wider than the box.
		/// \details The field scrolls sideways by the width this reports
		/// beyond its viewport, bar or no bar. The plain layout reports its
		/// widest laid-out line and 8 px more once it lays that line's block
		/// out again, as after every change of width, so words that wrap
		/// within 8 px of the field's edge gave the field a few pixels of
		/// range. The caret at the end of a code line wider than the box,
		/// where a suggestion's caret is put, then panned the field by that
		/// range before the block scrolled to it, and the words lost their
		/// first letters. The words wrap at the box's width and a block's
		/// code scrolls in its own box, so nothing is wider than the box.
		[[nodiscard]] auto documentSize() const -> QSizeF override;

	protected:
		void documentChanged(int aFrom, int aCharsRemoved, int aCharsAdded) override;

	private:
		void               rebuild() const;
		void               lay_out_run(std::size_t aRun) const;
		void               lay_out_code_block(const QTextBlock& aBlock, qreal aX, qreal aTop) const;
		void               place_lines(std::size_t aRun) const;
		[[nodiscard]] auto run_is_laid_out(const code_run& aRun) const -> bool;

		qreal                         BoxWidth  = 0.0;
		qreal                         BarExtent = 0.0;
		mutable std::vector<code_run> Runs;
		mutable std::vector<int>      RunOf;
		mutable std::vector<qreal>    BottomPad;
		mutable std::vector<qreal>    Scrolls;
		mutable bool                  Stale = true;
	};
} // namespace bpr::qt

#endif
