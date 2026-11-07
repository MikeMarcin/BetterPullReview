#include "github_application/fixture_uiforetw_observation.hpp"

#include "review_submission_workflow.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/core/appearance_contrast.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "terminal_diagnostics.hpp"
	#include <QAbstractScrollArea>
	#include <QAbstractSlider>
	#include <QApplication>
	#include <QBrush>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QList>
	#include <QPalette>
	#include <QPoint>
	#include <QScrollBar>
	#include <QSize>
	#include <QString>
	#include <QTextBlock>
	#include <QTextCharFormat>
	#include <QTextCursor>
	#include <QTextDocument>
	#include <QTextEdit>
	#include <QTextFormat>
	#include <QTextStream>
	#include <QWidget>
	#include <algorithm>
	#include <iterator>
	#include <vector>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto decoded_line(
		const core::decoded_text_document& aDocument,
		const std::uint32_t                aLine) -> std::string_view
	{
		if (aLine >= aDocument.Lines.size()) {
			return { };
		}
		const auto& span = aDocument.Lines[aLine].Content;
		if (span.End < span.Begin || span.End > aDocument.Bytes.size()) {
			return { };
		}
		return {
			reinterpret_cast<const char*>(aDocument.Bytes.data() + span.Begin),
			static_cast<std::size_t>(span.End - span.Begin),
		};
	}

	auto find_intraline_pair(
		const core::computed_file_diff& aResult,
		const std::string_view          aLeftFragment,
		const std::string_view          aRightFragment)
		-> std::optional<intraline_pair_location>
	{
		if (!aResult.LeftDocument.has_value()
			|| !aResult.RightDocument.has_value()) {
			return std::nullopt;
		}
		for (const auto& edit : aResult.Intraline) {
			if (edit.Left.empty() || edit.Right.empty()) {
				continue;
			}
			const auto pair = std::ranges::find_if(
				aResult.Alignment.Pairs,
				[&edit](const core::line_pair& aPair) {
					return aPair.Id == edit.Pair;
				});
			if (pair == aResult.Alignment.Pairs.end()) {
				continue;
			}
			const auto left           = decoded_line(*aResult.LeftDocument, pair->LeftLine);
			const auto right          = decoded_line(*aResult.RightDocument, pair->RightLine);
			const auto leftAt         = left.find(aLeftFragment);
			const auto rightAt        = right.find(aRightFragment);
			const auto insideFragment = [](const core::byte_span aSpan,
										   const std::size_t     aBegin,
										   const std::size_t     aExtent) {
				return aBegin != std::string_view::npos
					&& aSpan.Begin >= aBegin
					&& aSpan.End <= aBegin + aExtent;
			};
			if (insideFragment(edit.Left, leftAt, aLeftFragment.size())
				&& insideFragment(
					edit.Right, rightAt, aRightFragment.size())) {
				const auto segment = std::ranges::find_if(
					aResult.Alignment.Segments,
					[&pair](const core::alignment_segment& aSegment) {
						return aSegment.Id == pair->Segment;
					});
				if (segment == aResult.Alignment.Segments.end()) {
					continue;
				}
				return intraline_pair_location{
					.Left      = pair->LeftLine,
					.Right     = pair->RightLine,
					.LeftSpan  = edit.Left,
					.RightSpan = edit.Right,
					.Segment   = pair->Segment,
					.Group     = segment->Group,
				};
			}
		}
		return std::nullopt;
	}

	auto character_offset(
		const std::span<const std::uint8_t> aBytes,
		const std::uint64_t                 aOffset) -> int
	{
		const auto bounded = std::min<std::size_t>(
			static_cast<std::size_t>(aOffset), aBytes.size());
		return static_cast<int>(
			QString::fromUtf8(
				reinterpret_cast<const char*>(aBytes.data()),
				static_cast<qsizetype>(bounded))
				.size());
	}

	auto intraline_selection_rect(
		QPlainTextEdit&                    aPane,
		const core::decoded_text_document& aDocument,
		const std::uint32_t                aLine,
		const core::byte_span              aSpan) -> std::optional<QRect>
	{
		if (aLine >= aDocument.Lines.size() || aSpan.empty()) {
			return std::nullopt;
		}
		const auto& decoded = aDocument.Lines[aLine];
		if (decoded.Content.End < decoded.Content.Begin
			|| decoded.Content.End > aDocument.Bytes.size()) {
			return std::nullopt;
		}
		const auto bytes = std::span<const std::uint8_t>{
			aDocument.Bytes.data() + decoded.Content.Begin,
			static_cast<std::size_t>(decoded.Content.End - decoded.Content.Begin)
		};
		const auto from  = character_offset(bytes, aSpan.Begin);
		const auto to    = character_offset(bytes, aSpan.End);
		const auto block = aPane.document()->findBlockByNumber(
			static_cast<int>(aLine));
		if (!block.isValid() || from >= to) {
			return std::nullopt;
		}
		auto start = QTextCursor{ block };
		start.setPosition(block.position() + from);
		auto finish = QTextCursor{ block };
		finish.setPosition(block.position() + to);
		return aPane.cursorRect(start).united(aPane.cursorRect(finish));
	}

	auto navigate_to_group(
		qt::diff_workbench&             aWorkbench,
		const core::computed_file_diff& aResult,
		const core::change_group_id&    aTarget) -> bool
	{
		const auto target = std::ranges::find(
			aResult.Alignment.Groups, aTarget, &core::change_group::Id);
		if (target == aResult.Alignment.Groups.end()) {
			return false;
		}
		const auto targetIndex = static_cast<std::size_t>(
			std::distance(aResult.Alignment.Groups.begin(), target));
		for (auto attempt = std::size_t{ 0U };
			 attempt <= aResult.Alignment.Groups.size(); ++attempt) {
			const auto selection = aWorkbench.selection();
			if (selection.has_value() && selection->Group == aTarget) {
				return true;
			}
			const auto current      = selection.has_value()
					&& selection->Group.has_value()
				? std::ranges::find(
					  aResult.Alignment.Groups,
					  *selection->Group,
					  &core::change_group::Id)
				: aResult.Alignment.Groups.end();
			const auto currentIndex = current != aResult.Alignment.Groups.end()
				? std::optional<std::size_t>{ static_cast<std::size_t>(
					  std::distance(aResult.Alignment.Groups.begin(), current)) }
				: std::nullopt;
			const auto direction    = !currentIndex.has_value()
					|| *currentIndex < targetIndex
				? core::change_navigation_direction::next
				: core::change_navigation_direction::previous;
			if (!aWorkbench.navigate_change(
					direction, core::change_navigation_policy::stop)) {
				return false;
			}
			QApplication::processEvents();
		}
		return false;
	}

	auto reveal_intraline_pairs(
		qt::diff_workbench&             aWorkbench,
		const core::computed_file_diff& aResult,
		const intraline_pair_location&  aFirst,
		const intraline_pair_location&  aSecond,
		const bool                      aReport) -> bool
	{
		const auto reveal = [](
								QPlainTextEdit&                    aPane,
								const core::decoded_text_document& aDocument,
								const std::uint32_t                aFirstLine,
								const core::byte_span              aFirstSpan,
								const std::uint32_t                aSecondLine,
								const core::byte_span              aSecondSpan) {
			const auto first = intraline_selection_rect(
				aPane, aDocument, aFirstLine, aFirstSpan);
			const auto second = intraline_selection_rect(
				aPane, aDocument, aSecondLine, aSecondSpan);
			if (!first.has_value() || !second.has_value()) {
				return false;
			}
			auto* const horizontal   = aPane.horizontalScrollBar();
			const auto  documentLeft = std::min(first->left(), second->left())
				+ horizontal->value();
			const auto documentRight = std::max(first->right(), second->right())
				+ horizontal->value();
			const auto target = std::clamp(
				((documentLeft + documentRight) / 2)
					- (aPane.viewport()->width() / 2),
				horizontal->minimum(),
				horizontal->maximum());
			horizontal->setValue(target);
			return true;
		};
		if (aSecond.Left < aFirst.Left || aSecond.Right < aFirst.Right
			|| !reveal(
				*aWorkbench.left_pane(),
				*aResult.LeftDocument,
				aFirst.Left,
				aFirst.LeftSpan,
				aSecond.Left,
				aSecond.LeftSpan)
			|| !reveal(
				*aWorkbench.right_pane(),
				*aResult.RightDocument,
				aFirst.Right,
				aFirst.RightSpan,
				aSecond.Right,
				aSecond.RightSpan)) {
			// [[BPR-0859]]. The horizontal scroll could not be placed, so
			// no later visibility report would be meaningful.
			if (aReport) {
				bpr::app::detail::terminal_text_stream{ }
					<< "UIFORETW_REVEAL_PAIR_UNPLACED:first_left="
					<< static_cast<qulonglong>(aFirst.Left)
					<< " second_left=" << static_cast<qulonglong>(aSecond.Left)
					<< " first_right=" << static_cast<qulonglong>(aFirst.Right)
					<< " second_right="
					<< static_cast<qulonglong>(aSecond.Right) << '\n';
			}
			return false;
		}
		QApplication::processEvents();
		// [[BPR-0859]]. Every mark is weighed, not only the first, and an
		// offscreen one prints the rectangle it computed beside the
		// viewport it had to fit inside.
		const auto visible = [aReport](
								 QPlainTextEdit&                    aPane,
								 const char* const                  aName,
								 const core::decoded_text_document& aDocument,
								 const std::uint32_t                aLine,
								 const core::byte_span              aSpan) {
			const auto rect = intraline_selection_rect(
				aPane, aDocument, aLine, aSpan);
			const auto viewport
				= QRect{ QPoint{ 0, 0 }, aPane.viewport()->size() };
			const auto shown = rect.has_value() && !rect->isEmpty()
				&& viewport.contains(*rect, true);
			if (!shown && aReport) {
				const auto* const vertical = aPane.verticalScrollBar();
				bpr::app::detail::terminal_text_stream{ }
					<< "UIFORETW_REVEAL_PAIR_OFFSCREEN:" << aName
					<< " line=" << static_cast<qulonglong>(aLine)
					<< " placed=" << (rect.has_value() ? 1 : 0) << " rect="
					<< (rect.has_value()
							? QStringLiteral("%1,%2 %3x%4")
								  .arg(rect->left())
								  .arg(rect->top())
								  .arg(rect->width())
								  .arg(rect->height())
							: QStringLiteral("none"))
					<< " viewport=" << viewport.width() << 'x'
					<< viewport.height() << " vscroll=" << vertical->value()
					<< '/' << vertical->maximum()
					<< " top_block="
					<< aPane.cursorForPosition(QPoint{ 0, 0 }).blockNumber()
					<< " shown_pane=" << (aPane.isVisible() ? 1 : 0) << '\n';
			}
			return shown;
		};
		const auto firstLeft = visible(
			*aWorkbench.left_pane(),
			"left-first",
			*aResult.LeftDocument,
			aFirst.Left,
			aFirst.LeftSpan);
		const auto secondLeft = visible(
			*aWorkbench.left_pane(),
			"left-second",
			*aResult.LeftDocument,
			aSecond.Left,
			aSecond.LeftSpan);
		const auto firstRight = visible(
			*aWorkbench.right_pane(),
			"right-first",
			*aResult.RightDocument,
			aFirst.Right,
			aFirst.RightSpan);
		const auto secondRight = visible(
			*aWorkbench.right_pane(),
			"right-second",
			*aResult.RightDocument,
			aSecond.Right,
			aSecond.RightSpan);
		return firstLeft && secondLeft && firstRight && secondRight;
	}

	auto role_renderings(
		const QPlainTextEdit&          aPane,
		const QColor&                  aRole,
		const QColor&                  aRowBand,
		const core::appearance_variant aVariant) -> std::array<QColor, 3U>
	{
		const auto highlight = aPane.palette().color(QPalette::Highlight);
		const auto blended   = core::composited(
			core::appearance_color{ static_cast<std::uint8_t>(highlight.red()),
									static_cast<std::uint8_t>(highlight.green()),
									static_cast<std::uint8_t>(highlight.blue()),
									255U },
			core::appearance_color{ static_cast<std::uint8_t>(aRole.red()),
									static_cast<std::uint8_t>(aRole.green()),
									static_cast<std::uint8_t>(aRole.blue()),
									255U },
			core::selection_wash_alpha);
		// The third rendering is what a selected row shows: the band that
		// row wears, with the wash over it, strengthened in its own hue.
		auto derived = QColor{ };
		if (aRowBand.isValid()) {
			const auto washedBand = core::composited(
				core::appearance_color{
					static_cast<std::uint8_t>(highlight.red()),
					static_cast<std::uint8_t>(highlight.green()),
					static_cast<std::uint8_t>(highlight.blue()),
					255U },
				core::appearance_color{
					static_cast<std::uint8_t>(aRowBand.red()),
					static_cast<std::uint8_t>(aRowBand.green()),
					static_cast<std::uint8_t>(aRowBand.blue()),
					255U },
				core::selection_wash_alpha);
			const auto mark
				= core::derived_emphasis_fill(washedBand, aVariant);
			derived = QColor{ mark.Red, mark.Green, mark.Blue };
		}
		return { aRole,
				 QColor{ blended.Red, blended.Green, blended.Blue },
				 derived };
	}

	auto role_paint_count(
		const QPlainTextEdit&          aPane,
		const QColor&                  aExpected,
		const QColor&                  aRowBand,
		const core::appearance_variant aVariant) -> std::size_t
	{
		// BPR-0323 marks the changed characters with a rule as well as a
		// fill, and drops the fill on a selected row because a tint there
		// read as a foreign colour. The pane already fixes which side a
		// span belongs to, so an underlined span is the role just as much
		// as a filled one; requiring the fill would read the new mark as
		// a missing one.
		const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
		return static_cast<std::size_t>(std::ranges::count_if(
			aPane.extraSelections(),
			[&accepted](const QTextEdit::ExtraSelection& aSelection) {
				return !aSelection.format.boolProperty(
						   QTextFormat::FullWidthSelection)
					&& (std::ranges::contains(
							accepted,
							aSelection.format.background().color())
						|| aSelection.format.fontUnderline())
					&& aSelection.cursor.hasSelection();
			}));
	}

	auto intraline_roles_visible(
		const qt::diff_workbench&     aWorkbench,
		const qt::appearance_profile& aProfile,
		const std::size_t             aMinimum) -> bool
	{
		return role_paint_count(
				   *aWorkbench.left_pane(),
				   aProfile.color(
					   core::appearance_role::diff_intraline_removed_background),
				   aProfile.color(core::appearance_role::diff_removed_background),
				   aProfile.variant())
			>= aMinimum
			&& role_paint_count(
				   *aWorkbench.right_pane(),
				   aProfile.color(
					   core::appearance_role::diff_intraline_added_background),
				   aProfile.color(core::appearance_role::diff_added_background),
				   aProfile.variant())
			>= aMinimum;
	}
#endif
}
