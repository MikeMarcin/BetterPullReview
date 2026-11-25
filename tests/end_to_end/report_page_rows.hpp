#ifndef BPR_TESTS_END_TO_END_REPORT_PAGE_ROWS_HPP
#define BPR_TESTS_END_TO_END_REPORT_PAGE_ROWS_HPP

#include "bpr/qt/agent_report_view.hpp"

#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QString>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>

#include <string>

/// \file
/// \brief A report page's keep rows, judged as the page laid them out in the
/// application's own window. The owner's report, narrowed by its side chat,
/// stood a row's sentence a letter to a line beside its chips ("at
/// renderer.cpp:11. Nothing is published").
namespace bpr::e2e {
	/// \brief What is wrong with the rows of the report shown, in words;
	/// empty when nothing is.
	/// \details A row is right when the page has no sideways range to speak of; when
	/// each chip keeps its words on one line, inside the page; and when the
	/// sentence that says what the row does (each ends "Nothing is published"
	/// or "Nothing was made") breaks only between words, inside the page.
	[[nodiscard]] inline auto report_rows_wrong(const qt::agent_report_view& aReport) -> std::string
	{
		auto* const page     = aReport.dressed_view();
		auto* const document = page->document();
		auto* const layout   = document->documentLayout();
		const auto  width    = static_cast<qreal>(aReport.column()->width());
		auto        wrong    = std::string{ };
		// A row wider than the page gives the page a sideways range of what
		// it lacks. A pixel or two is the page's own rounding of a bordered
		// box, at any width.
		if (page->horizontalScrollBar()->maximum() > 2) {
			wrong += "the page has a sideways range of " + std::to_string(page->horizontalScrollBar()->maximum()) + "; ";
		}
		auto sentences = 0;
		for (auto block = document->begin(); block.isValid(); block = block.next()) {
			const auto text = block.text();
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				// A chip is a link alone in its block, its words kept together
				// by no-break spaces or one word; its border and padding stand
				// 11 px outside its words.
				const auto part = fragment.fragment();
				if (part.charFormat().anchorHref().isEmpty() || part.text() != text || !part.charFormat().anchorHref().startsWith(QStringLiteral("report-action-"))) {
					continue;
				}
				if (block.layout()->lineCount() > 1) {
					wrong += "the chip \"" + text.toStdString() + "\" takes " + std::to_string(block.layout()->lineCount()) + " lines; ";
				}
				if (const auto right = layout->blockBoundingRect(block).right() + 11.0; right > width + 0.5) {
					wrong += "the chip \"" + text.toStdString() + "\" reaches " + std::to_string(static_cast<int>(right)) + " of a " + std::to_string(static_cast<int>(width)) + " px page; ";
				}
			}
			if (!text.endsWith(QStringLiteral("Nothing is published")) && !text.endsWith(QStringLiteral("Nothing was made"))) {
				continue;
			}
			++sentences;
			if (const auto right = layout->blockBoundingRect(block).right(); right > width + 0.5) {
				wrong += "\"" + text.toStdString() + "\" reaches " + std::to_string(static_cast<int>(right)) + " of a " + std::to_string(static_cast<int>(width)) + " px page; ";
			}
			const auto* const laid = block.layout();
			for (auto index = 0; index + 1 < laid->lineCount(); ++index) {
				const auto line = laid->lineAt(index);
				if (!text.at(line.textStart() + line.textLength() - 1).isSpace()) {
					wrong += "\"" + text.toStdString() + "\" breaks inside a word, its line " + std::to_string(index + 1) + " of " + std::to_string(laid->lineCount()) + " being \""
						+ text.mid(line.textStart(), line.textLength()).toStdString() + "\"; ";
					break;
				}
			}
		}
		if (sentences == 0) {
			wrong += "the page has no keep row; ";
		}
		return wrong;
	}
} // namespace bpr::e2e

#endif // BPR_TESTS_END_TO_END_REPORT_PAGE_ROWS_HPP
