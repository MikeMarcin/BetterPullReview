#include "composer_code_layout.hpp"

#include "fenced_code_highlighter.hpp"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>

namespace bpr::qt {
	namespace {
		using line_role = fenced_code_highlighter::line_role;

		/// A huge rectangle: the whole document needs drawing again.
		[[nodiscard]] auto everything() -> QRectF
		{
			return { 0.0, 0.0, 1000000000.0, 1000000000.0 };
		}

		/// \brief Whether `aBlock` holds this layout's own unwrapped lines,
		/// not the plain layout's wrapped ones.
		[[nodiscard]] auto laid_out_unwrapped(const QTextBlock& aBlock) -> bool
		{
			const auto* const layout = aBlock.layout();
			return layout->lineCount() > 0 && layout->textOption().wrapMode() == QTextOption::NoWrap;
		}
	} // namespace

	composer_code_layout::composer_code_layout(QTextDocument* aDocument)
		: QPlainTextDocumentLayout{ aDocument }
	{ }

	void composer_code_layout::set_box_width(const qreal aWidth)
	{
		if (qFuzzyCompare(BoxWidth + 1.0, aWidth + 1.0)) {
			return;
		}
		BoxWidth = aWidth;
		Stale    = true;
		rebuild();
		emit documentSizeChanged(documentSize());
		emit update(everything());
	}

	void composer_code_layout::set_bar_extent(const qreal aExtent)
	{
		if (qFuzzyCompare(BarExtent + 1.0, aExtent + 1.0)) {
			return;
		}
		BarExtent = aExtent;
		Stale     = true;
		rebuild();
		emit documentSizeChanged(documentSize());
		emit update(everything());
	}

	auto composer_code_layout::runs() const -> const std::vector<code_run>&
	{
		if (Stale || static_cast<int>(RunOf.size()) != document()->blockCount()) {
			rebuild();
		}
		return Runs;
	}

	auto composer_code_layout::run_of(const int aBlockNumber) const -> std::optional<std::size_t>
	{
		static_cast<void>(runs());
		if (aBlockNumber < 0 || aBlockNumber >= static_cast<int>(RunOf.size()) || RunOf[static_cast<std::size_t>(aBlockNumber)] < 0) {
			return std::nullopt;
		}
		return static_cast<std::size_t>(RunOf[static_cast<std::size_t>(aBlockNumber)]);
	}

	void composer_code_layout::set_scroll(const std::size_t aRun, const qreal aOffset)
	{
		static_cast<void>(runs());
		if (aRun >= Runs.size()) {
			return;
		}
		const auto offset = std::clamp(aOffset, 0.0, Runs[aRun].Room);
		if (qFuzzyCompare(Runs[aRun].Scroll + 1.0, offset + 1.0)) {
			return;
		}
		Scrolls[aRun]     = offset;
		Runs[aRun].Scroll = offset;
		place_lines(aRun);
		emit update(everything());
	}

	void composer_code_layout::reveal(const QTextBlock& aBlock, const qreal aX)
	{
		const auto run = run_of(aBlock.blockNumber());
		if (!run || fenced_code_highlighter::role_of(aBlock) != line_role::code) {
			return;
		}
		const auto margin = document()->documentMargin();
		const auto left   = margin + inset;
		const auto right  = std::max(left, BoxWidth - margin - inset);
		const auto scroll = Runs[*run].Scroll;
		if (aX < left) {
			set_scroll(*run, scroll - (left - aX));
		} else if (aX > right) {
			set_scroll(*run, scroll + (aX - right));
		}
	}

	void composer_code_layout::show_line_starts()
	{
		for (auto index = std::size_t{ 0U }; index < runs().size(); ++index) {
			set_scroll(index, 0.0);
		}
	}

	auto composer_code_layout::documentSize() const -> QSizeF
	{
		auto size = QPlainTextDocumentLayout::documentSize();
		if (BoxWidth > 0.0) {
			size.setWidth(std::min(size.width(), BoxWidth));
		}
		return size;
	}

	auto composer_code_layout::blockBoundingRect(const QTextBlock& aBlock) const -> QRectF
	{
		if (!aBlock.isValid()) {
			return { };
		}
		const auto run = run_of(aBlock.blockNumber());
		if (!run) {
			// Words: the plain layout's own, wrapped at the field's width. A
			// line that was code a moment ago is laid out again as words.
			if (laid_out_unwrapped(aBlock) && document()->defaultTextOption().wrapMode() != QTextOption::NoWrap) {
				aBlock.layout()->clearLayout();
			}
			return QPlainTextDocumentLayout::blockBoundingRect(aBlock);
		}
		if (!run_is_laid_out(Runs[*run])) {
			lay_out_run(*run);
		}
		if (!aBlock.isVisible()) {
			return { };
		}
		auto height = aBlock.layout()->boundingRect().bottom() + BottomPad[static_cast<std::size_t>(aBlock.blockNumber())];
		if (!aBlock.next().isValid()) {
			height += document()->documentMargin();
		}
		return { 0.0, 0.0, std::max(1.0, BoxWidth), height };
	}

	void composer_code_layout::documentChanged(const int aFrom, const int aCharsRemoved, const int aCharsAdded)
	{
		// Read the lines again first: the plain layout measures the changed
		// block through `blockBoundingRect`, which must know what is code.
		Stale = true;
		QPlainTextDocumentLayout::documentChanged(aFrom, aCharsRemoved, aCharsAdded);
		rebuild();
		emit documentSizeChanged(documentSize());
		emit update(everything());
	}

	void composer_code_layout::rebuild() const
	{
		const auto count = static_cast<std::size_t>(document()->blockCount());
		Runs.clear();
		RunOf.assign(count, -1);
		BottomPad.assign(count, 0.0);
		auto open = false;
		for (auto block = document()->firstBlock(); block.isValid(); block = block.next()) {
			const auto role   = fenced_code_highlighter::role_of(block);
			const auto number = block.blockNumber();
			if (role == line_role::opening_fence) {
				Runs.push_back({ .First = number, .Last = number });
				open = true;
			} else if (open && (role == line_role::code || role == line_role::closing_fence)) {
				Runs.back().Last   = number;
				Runs.back().Closed = role == line_role::closing_fence;
			} else {
				open = false;
				continue;
			}
			RunOf[static_cast<std::size_t>(number)] = static_cast<int>(Runs.size() - 1U);
			if (role == line_role::closing_fence) {
				open = false;
			}
		}
		// Each block keeps its sideways scroll by its place among the blocks.
		Scrolls.resize(Runs.size(), 0.0);
		Stale = false;
		for (auto index = std::size_t{ 0U }; index < Runs.size(); ++index) {
			lay_out_run(index);
		}
		// A line that was code and is words again is laid out as words.
		for (auto block = document()->firstBlock(); block.isValid(); block = block.next()) {
			if (RunOf[static_cast<std::size_t>(block.blockNumber())] < 0 && laid_out_unwrapped(block) && document()->defaultTextOption().wrapMode() != QTextOption::NoWrap) {
				block.layout()->clearLayout();
			}
		}
	}

	void composer_code_layout::lay_out_run(const std::size_t aRun) const
	{
		auto&      run     = Runs[aRun];
		auto       natural = 0.0;
		auto       last    = -1;
		const auto scroll  = Scrolls[aRun];
		for (auto block = document()->findBlockByNumber(run.First); block.isValid() && block.blockNumber() <= run.Last; block = block.next()) {
			const auto role = fenced_code_highlighter::role_of(block);
			const auto code = role == line_role::code;
			// The header row, its fence, keeps its place; the code moves.
			lay_out_code_block(block, code ? inset - scroll : inset, role == line_role::opening_fence || (code && block.blockNumber() == run.First + 1) ? pad : 0.0);
			if (code) {
				last = block.blockNumber();
				for (auto line = 0; line < block.layout()->lineCount(); ++line) {
					natural = std::max(natural, block.layout()->lineAt(line).naturalTextWidth());
				}
			}
		}
		run.Natural   = natural;
		run.Room      = std::max(0.0, natural - std::max(0.0, BoxWidth - 2.0 * (inset + document()->documentMargin())));
		Scrolls[aRun] = std::clamp(scroll, 0.0, run.Room);
		run.Scroll    = Scrolls[aRun];
		if (run.Scroll != scroll) {
			place_lines(aRun);
		}
		BottomPad[static_cast<std::size_t>(run.First)] = pad;
		if (run.Closed) {
			BottomPad[static_cast<std::size_t>(run.Last)] = pad;
		}
		if (last >= 0) {
			BottomPad[static_cast<std::size_t>(last)] = pad + (run.Room > 0.0 ? BarExtent : 0.0);
		}
	}

	void composer_code_layout::lay_out_code_block(const QTextBlock& aBlock, const qreal aX, const qreal aTop) const
	{
		auto* const layout = aBlock.layout();
		auto        option = document()->defaultTextOption();
		option.setWrapMode(QTextOption::NoWrap);
		layout->setTextOption(option);
		const auto margin = document()->documentMargin();
		auto       top    = aTop;
		layout->beginLayout();
		for (auto line = layout->createLine(); line.isValid(); line = layout->createLine()) {
			line.setLeadingIncluded(true);
			line.setLineWidth(std::max(1.0, BoxWidth));
			line.setPosition(QPointF{ margin + aX, top });
			top += line.height();
		}
		layout->endLayout();
		auto block = aBlock;
		block.setLineCount(aBlock.isVisible() ? layout->lineCount() : 0);
	}

	void composer_code_layout::place_lines(const std::size_t aRun) const
	{
		const auto& run = Runs[aRun];
		const auto  x   = document()->documentMargin() + inset - run.Scroll;
		for (auto block = document()->findBlockByNumber(run.First); block.isValid() && block.blockNumber() <= run.Last; block = block.next()) {
			if (fenced_code_highlighter::role_of(block) != line_role::code) {
				continue;
			}
			for (auto index = 0; index < block.layout()->lineCount(); ++index) {
				auto line = block.layout()->lineAt(index);
				line.setPosition(QPointF{ x, line.position().y() });
			}
		}
	}

	auto composer_code_layout::run_is_laid_out(const code_run& aRun) const -> bool
	{
		for (auto block = document()->findBlockByNumber(aRun.First); block.isValid() && block.blockNumber() <= aRun.Last; block = block.next()) {
			if (!laid_out_unwrapped(block)) {
				return false;
			}
		}
		return true;
	}
} // namespace bpr::qt
