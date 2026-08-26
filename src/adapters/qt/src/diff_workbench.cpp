#include "bpr/qt/diff_workbench.hpp"

#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QApplication>
#include <QEvent>
#include <QFontMetricsF>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// Width of the change-group boundary rule, in device-independent pixels.
		constexpr auto BoundaryRuleWidth = 2.0;
		/// Breathing room either side of the widest number the gutter shows.
		constexpr auto GutterPadding = 6;
		/// A one-glyph column reads as a margin, not as a column of numbers.
		constexpr auto GutterMinimumDigits = 2;

		/// \brief Reserves the gutter each pane numbers its own lines in.
		///
		/// \details `code_gutter_background` and `code_line_number_text` have been
		/// declared roles in every built-in palette since RFC-BPR-0006, and
		/// `appearance_contrast` has been validating that pair's readability in
		/// every profile, against text nothing drew. BPR-0455 draws it, after the
		/// BPR-0414 owner review could not tell which pane row was line 42.
		class source_pane final : public QPlainTextEdit
		{
		public:
			explicit source_pane(QWidget* aParent = nullptr);

			/// \brief Widths the gutter to the widest number it will ever show.
			///
			/// \details Sized from the block count rather than from the visible
			/// rows, so scrolling never moves the code sideways. Two digits is the
			/// floor: a nine-line file whose gutter is one glyph wide reads as a
			/// margin rather than as a column.
			[[nodiscard]] auto gutter_width() const -> int;
			[[nodiscard]] auto gutter() const noexcept -> QWidget*;
			void               set_gutter_colors(const QColor& aBackground, const QColor& aText);
			void               paint_line_numbers(QPaintEvent* aEvent);
			[[nodiscard]] auto painted_line_numbers() const noexcept -> std::uint64_t
			{
				return PaintedLineNumbers;
			}

			/// \brief Adopts the lines each change group starts on.
			///
			/// \details Held sorted so a repaint can find the visible ones by
			/// bisection. Computed once when a result is published rather than
			/// per paint, because the group count follows the size of the diff
			/// and only the search may be allowed to follow the viewport.
			void set_group_boundaries(std::vector<std::uint32_t> aBoundaries)
			{
				Boundaries = std::move(aBoundaries);
				viewport()->update();
			}

			void set_boundary_rule_color(const QColor& aColor)
			{
				RuleColor = aColor;
				viewport()->update();
			}

			void set_conversation_anchor(std::optional<std::uint32_t> aLine)
			{
				ConversationAnchor = aLine;
				viewport()->update();
			}

			[[nodiscard]] auto conversation_anchor_rect() const -> QRect
			{
				if (!ConversationAnchor.has_value()) {
					return { };
				}
				const auto block = document()->findBlockByNumber(
					static_cast<int>(*ConversationAnchor));
				if (!block.isValid() || !block.isVisible()) {
					return { };
				}
				const auto geometry = blockBoundingGeometry(block).translated(contentOffset());
				return QRect{ 1, qRound(geometry.top()), 4,
							  std::max(1, qRound(geometry.height())) };
			}

			[[nodiscard]] auto painted_rules() const noexcept -> std::uint64_t
			{
				return PaintedRules;
			}

			/// \brief The first block the viewport can show, and where it sits.
			///
			/// \details `firstVisibleBlock` and `contentOffset` are protected
			/// on `QPlainTextEdit`, so this is the only place they can be read.
			/// Together they say which line is at the top and how far above the
			/// viewport edge it starts, which is everything needed to turn a
			/// per-line document measurement into a scroll origin.
			[[nodiscard]] auto visible_origin() const
				-> std::optional<std::pair<std::size_t, qreal>>
			{
				const auto block = firstVisibleBlock();
				if (!block.isValid() || block.blockNumber() < 0) {
					return std::nullopt;
				}
				return std::pair{
					static_cast<std::size_t>(block.blockNumber()),
					contentOffset().y(),
				};
			}

			[[nodiscard]] auto set_document_origin(
				const qreal aOrigin, const qreal aMaximumOrigin) -> qreal
			{
				auto* const bar = verticalScrollBar();
				if (bar == nullptr || aMaximumOrigin <= 0.0 || bar->maximum() <= bar->minimum()) {
					if (bar != nullptr) {
						bar->setValue(bar->minimum());
					}
					return 0.0;
				}
				const auto fraction = std::clamp(aOrigin / aMaximumOrigin, 0.0, 1.0);
				const auto value    = std::lerp(
					static_cast<qreal>(bar->minimum()),
					static_cast<qreal>(bar->maximum()),
					fraction);
				bar->setValue(static_cast<int>(std::llround(value)));
				// An estimate, and known to be one: the bar counts blocks, so
				// the fraction between its minimum and maximum is not the
				// fraction of the document above the viewport. BPR-0208 records
				// the drift this causes in the ribbons and why fixing it needs
				// the measured boundaries rather than a local change here.
				const auto appliedFraction = static_cast<qreal>(
												 bar->value() - bar->minimum())
					/ static_cast<qreal>(bar->maximum() - bar->minimum());
				return appliedFraction * aMaximumOrigin;
			}

		protected:
			/// \brief Draws a rule where each visible change group begins.
			///
			/// \details A boundary is a position between two lines, not a
			/// property of either one, so it is drawn on the edge between them.
			/// The previous implementation filled the unchanged line above the
			/// group instead. That said "this line is special" about a line
			/// that had not changed, in the same full-width tint the added and
			/// removed bands use, carrying the opposite meaning — and it was
			/// mistaken for the selected line at attended review.
			///
			/// Only blocks the viewport can show are walked, and the boundary
			/// list is entered by bisection rather than scanned, so the work
			/// here follows the visible line count and not the diff's size.
			void paintEvent(QPaintEvent* const aEvent) override
			{
				QPlainTextEdit::paintEvent(aEvent);
				auto block = firstVisibleBlock();
				if (!block.isValid()) {
					return;
				}
				auto       top   = blockBoundingGeometry(block)
									   .translated(contentOffset())
									   .top();
				const auto limit = static_cast<qreal>(viewport()->height());
				const auto width = viewport()->width();
				auto       next  = std::ranges::lower_bound(
					Boundaries,
					static_cast<std::uint32_t>(std::max(block.blockNumber(), 0)));
				auto painter = QPainter{ viewport() };
				if (!Boundaries.empty() && RuleColor.isValid()) {
					painter.setPen(QPen{ RuleColor, BoundaryRuleWidth });
					while (block.isValid() && top <= limit && next != Boundaries.end()) {
						if (block.isVisible()
							&& *next == static_cast<std::uint32_t>(block.blockNumber())) {
							const auto y = qRound(top);
							painter.drawLine(0, y, width, y);
							++PaintedRules;
							++next;
						}
						top += blockBoundingRect(block).height();
						block = block.next();
					}
				}
				if (const auto marker = conversation_anchor_rect(); !marker.isEmpty()) {
					auto color = palette().color(QPalette::Highlight);
					color.setAlpha(220);
					painter.setPen(Qt::NoPen);
					painter.setBrush(color);
					painter.drawRoundedRect(marker, 2.0, 2.0);
				}
			}

			void resizeEvent(QResizeEvent* aEvent) override;
			void changeEvent(QEvent* aEvent) override;

		private:
			void refresh_gutter_width();

			class gutter_area;

			gutter_area*                 Gutter = nullptr;
			QColor                       GutterBackground;
			QColor                       GutterText;
			std::vector<std::uint32_t>   Boundaries;
			QColor                       RuleColor;
			std::optional<std::uint32_t> ConversationAnchor;
			std::uint64_t                PaintedRules       = 0U;
			std::uint64_t                PaintedLineNumbers = 0U;
		};

		/// \brief Paints one pane's own line numbers beside its own text.
		///
		/// \details A child of the pane rather than content inside its viewport,
		/// because `setViewportMargins` is what actually moves the text aside;
		/// painting numbers into the unshrunk viewport would put them on top of
		/// the code. It also keeps BPR-0414's four-pixel conversation marker
		/// correct for free: that rect is viewport-relative, so it follows the
		/// new left edge instead of ending up behind the numbers.
		class source_pane::gutter_area final : public QWidget
		{
		public:
			explicit gutter_area(source_pane* const aPane)
				: QWidget{ aPane }
				, Pane{ aPane }
			{
				setObjectName(QStringLiteral("diffSourceLineNumbers"));
				// The numbers are the pane's own content read another way, and a
				// screen reader announcing every visible integer beside the code
				// would bury it. The pane keeps the accessible name.
				setAccessibleName(QStringLiteral("Line numbers"));
				setFocusPolicy(Qt::NoFocus);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				return QSize{ Pane->gutter_width(), 0 };
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				Pane->paint_line_numbers(aEvent);
			}

		private:
			source_pane* Pane = nullptr;
		};

		auto source_pane::gutter() const noexcept -> QWidget*
		{
			return Gutter;
		}

		source_pane::source_pane(QWidget* const aParent)
			: QPlainTextEdit{ aParent }
		{
			Gutter = new gutter_area{ this };
			// The width follows the digit count, so it changes when a document is
			// replaced and not while scrolling one.
			connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) {
				refresh_gutter_width();
			});
			connect(this,
					&QPlainTextEdit::updateRequest,
					this,
					[this](const QRect& aRect, const int aDy) {
						if (aDy != 0) {
							Gutter->scroll(0, aDy);
						} else {
							Gutter->update(
								0, aRect.y(), Gutter->width(), aRect.height());
						}
					});
			refresh_gutter_width();
		}

		auto source_pane::gutter_width() const -> int
		{
			auto digits = 1;
			for (auto count = std::max(1, blockCount()); count >= 10; count /= 10) {
				++digits;
			}
			return 2 * GutterPadding
				+ fontMetrics().horizontalAdvance(QLatin1Char('9'))
				* std::max(digits, GutterMinimumDigits);
		}

		void source_pane::refresh_gutter_width()
		{
			setViewportMargins(gutter_width(), 0, 0, 0);
		}

		void source_pane::set_gutter_colors(
			const QColor& aBackground,
			const QColor& aText)
		{
			GutterBackground = aBackground;
			GutterText       = aText;
			Gutter->update();
		}

		void source_pane::resizeEvent(QResizeEvent* const aEvent)
		{
			QPlainTextEdit::resizeEvent(aEvent);
			const auto area = contentsRect();
			Gutter->setGeometry(
				QRect{ area.left(), area.top(), gutter_width(), area.height() });
		}

		void source_pane::changeEvent(QEvent* const aEvent)
		{
			QPlainTextEdit::changeEvent(aEvent);
			// A different font is a different digit width, and the code beside it
			// has to move by exactly the same amount.
			if (aEvent != nullptr && aEvent->type() == QEvent::FontChange) {
				refresh_gutter_width();
			}
		}

		void source_pane::paint_line_numbers(QPaintEvent* const aEvent)
		{
			auto painter = QPainter{ Gutter };
			painter.fillRect(aEvent->rect(),
							 GutterBackground.isValid()
								 ? GutterBackground
								 : palette().color(QPalette::Window));
			auto block = firstVisibleBlock();
			if (!block.isValid()) {
				return;
			}
			painter.setPen(GutterText.isValid()
							   ? GutterText
							   : palette().color(QPalette::WindowText));
			auto       top   = blockBoundingGeometry(block)
								   .translated(contentOffset())
								   .top();
			const auto limit = static_cast<qreal>(aEvent->rect().bottom());
			const auto right = Gutter->width() - GutterPadding;
			// Only the blocks the exposed rect can show, so the work follows the
			// visible rows rather than the file's length.
			while (block.isValid() && top <= limit) {
				const auto height = blockBoundingRect(block).height();
				if (block.isVisible()
					&& top + height >= static_cast<qreal>(aEvent->rect().top())) {
					// One-based on screen, zero-based inside: RFC-BPR-0004 fixes
					// that split, and each pane numbers its own document rather
					// than being renumbered to match the other side.
					painter.drawText(0,
									 qRound(top),
									 right,
									 qRound(height),
									 Qt::AlignRight | Qt::AlignVCenter,
									 QString::number(block.blockNumber() + 1));
					++PaintedLineNumbers;
				}
				top += height;
				block = block.next();
			}
		}

		[[nodiscard]] auto pane(QPlainTextEdit* const aPane) noexcept
			-> source_pane*
		{
			return static_cast<source_pane*>(aPane);
		}

		/// \brief The document y the viewport's top edge sits at.
		///
		/// \details Taken from the axis's own per-line boundaries so the
		/// origin and the ribbon endpoints share one measurement basis and
		/// cannot disagree by construction. Falls back to the caller's estimate
		/// only when the pane cannot say which block is first or the axis has
		/// no boundary for it, which is a torn state rather than a normal one -
		/// returning something wrong quietly is what this replaces.
		[[nodiscard]] auto measured_document_origin(
			const source_pane&             aPane,
			const measured_alignment_axis& aAxis,
			const core::diff_side          aSide,
			const qreal                    aEstimate) -> qreal
		{
			const auto visible = aPane.visible_origin();
			if (!visible.has_value()) {
				return aEstimate;
			}
			const auto boundary = aSide == core::diff_side::left
				? aAxis.left_line_origin(visible->first)
				: aAxis.right_line_origin(visible->first);
			return boundary.has_value() ? *boundary - visible->second
										: aEstimate;
		}

		[[nodiscard]] auto text_document(
			const std::optional<core::decoded_text_document>& aDocument) -> QString
		{
			if (!aDocument.has_value()) {
				return { };
			}
			if (
				aDocument->Bytes.size()
				> static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
				return { };
			}
			return QString::fromUtf8(
				reinterpret_cast<const char*>(aDocument->Bytes.data()),
				static_cast<qsizetype>(aDocument->Bytes.size()));
		}

		[[nodiscard]] auto line_text_at(
			const std::optional<core::decoded_text_document>& aDocument,
			const std::uint32_t                               aLine) -> QString
		{
			if (
				!aDocument.has_value() || aLine >= aDocument->Lines.size()) {
				return QStringLiteral("(no source line)");
			}
			const auto& span = aDocument->Lines[aLine].Content;
			if (
				span.End < span.Begin || span.End > aDocument->Bytes.size()
				|| span.End - span.Begin
					> static_cast<std::uint64_t>(
						std::numeric_limits<qsizetype>::max())) {
				return QStringLiteral("(unavailable source line)");
			}
			const auto* bytes = aDocument->Bytes.data()
				+ static_cast<std::size_t>(span.Begin);
			return QString::fromUtf8(
				reinterpret_cast<const char*>(bytes),
				static_cast<qsizetype>(span.End - span.Begin));
		}

		[[nodiscard]] auto line_text(
			const std::optional<core::decoded_text_document>& aDocument,
			const std::optional<core::source_line_ref>&       aLine) -> QString
		{
			return aLine.has_value()
				? line_text_at(aDocument, aLine->Line)
				: QStringLiteral("(no source line)");
		}

		[[nodiscard]] auto span_text(
			const std::optional<core::decoded_text_document>& aDocument,
			const core::line_span                             aSpan) -> QString
		{
			if (
				!aDocument.has_value() || aSpan.Begin >= aSpan.End
				|| aSpan.End > aDocument->Lines.size()) {
				return QStringLiteral("(no source lines)");
			}
			auto result = QString{ };
			for (auto line = aSpan.Begin; line < aSpan.End; ++line) {
				if (!result.isEmpty()) {
					result += QLatin1Char('\n');
				}
				result += line_text_at(aDocument, line);
			}
			return result;
		}

		[[nodiscard]] auto detail_side_text(
			const std::optional<core::decoded_text_document>& aDocument,
			const std::optional<core::source_line_ref>&       aLine,
			const core::line_detail_input&                    aDetail,
			const core::diff_side                             aSide) -> QString
		{
			if (aLine.has_value()) {
				return line_text(aDocument, aLine);
			}
			if (
				aDetail.OpposingSpan.has_value()
				&& aDetail.OpposingSpan->Side == aSide) {
				return span_text(aDocument, aDetail.OpposingSpan->Lines);
			}
			if (
				aDetail.OpposingBoundary.has_value()
				&& aDetail.OpposingBoundary->Side == aSide) {
				return QStringLiteral("(insertion boundary before line %1)")
					.arg(aDetail.OpposingBoundary->Boundary + 1U);
			}
			return QStringLiteral("(no source line)");
		}

		[[nodiscard]] auto reason_text(const core::diff_reason_code aReason)
			-> QString
		{
			switch (aReason) {
			case core::diff_reason_code::none:
				return QStringLiteral("Diff ready");
			case core::diff_reason_code::binary_file:
				return QStringLiteral("Binary file comparison");
			case core::diff_reason_code::missing_left_blob:
			case core::diff_reason_code::missing_right_blob:
				return QStringLiteral("A selected file version is unavailable");
			case core::diff_reason_code::decode_failure:
				return QStringLiteral("File text could not be decoded");
			case core::diff_reason_code::materialization_limit:
				return QStringLiteral("File exceeds the comparison materialization limit");
			case core::diff_reason_code::compute_limit:
				return QStringLiteral("Comparison exceeded its bounded compute budget");
			case core::diff_reason_code::generated_file_unsupported:
				return QStringLiteral("Generated file comparison is unsupported");
			case core::diff_reason_code::cancelled:
				return QStringLiteral("Comparison cancelled");
			case core::diff_reason_code::stale:
				return QStringLiteral("Older comparison discarded");
			case core::diff_reason_code::invalid_alignment:
			case core::diff_reason_code::host_failure:
				return QStringLiteral("Comparison unavailable");
			}
			return QStringLiteral("Comparison unavailable");
		}

		/// \brief Converts a byte offset inside one line to a character index.
		///
		/// \details The core reports intraline edits as byte spans, because
		/// bytes are what it compared. Qt addresses text in UTF-16 code units,
		/// so the prefix is decoded and measured rather than assumed equal:
		/// every non-ASCII character ahead of the edit would otherwise shift
		/// the emphasis left by however many continuation bytes it carries.
		[[nodiscard]] auto character_offset(
			const std::span<const std::uint8_t> aLineBytes,
			const std::uint64_t                 aByteOffset) -> int
		{
			const auto bounded = std::min<std::size_t>(
				static_cast<std::size_t>(aByteOffset), aLineBytes.size());
			return static_cast<int>(
				QString::fromUtf8(
					reinterpret_cast<const char*>(aLineBytes.data()),
					static_cast<qsizetype>(bounded))
					.size());
		}

		/// \brief The lines one pane could currently show, with a margin.
		///
		/// \details Painting is bounded to this. `QPlainTextEdit` walks its
		/// whole extra-selection list on every paint, so one selection per
		/// changed line over a sixty-thousand-line comparison would cost sixty
		/// thousand list entries per repaint while the widget count stayed at
		/// sixty-eight. RFC-BPR-0005's bounded-widget evidence would still hold
		/// and the surface would still be slow, which is precisely the failure
		/// that evidence cannot see.
		///
		/// The margin is a screenful either side so a scroll reveals painted
		/// lines rather than a band appearing late.
		[[nodiscard]] auto paintable_line_bounds(QPlainTextEdit& aPane)
			-> core::line_span
		{
			const auto height = aPane.viewport()->height();
			const auto top    = aPane.cursorForPosition(QPoint{ 0, 0 })
									.blockNumber();
			const auto bottom = aPane.cursorForPosition(QPoint{ 0, height })
									.blockNumber();
			const auto span   = std::max(bottom - top, 0) + 1;
			const auto begin  = std::max(top - span, 0);
			const auto end    = bottom + span + 1;
			return core::line_span{
				.Begin = static_cast<std::uint32_t>(begin),
				.End   = static_cast<std::uint32_t>(std::max(end, 0)),
			};
		}

		/// \brief Returns the overlap of two line spans, empty when disjoint.
		[[nodiscard]] constexpr auto intersection(
			const core::line_span aLeft, const core::line_span aRight) noexcept
			-> core::line_span
		{
			const auto begin = std::max(aLeft.Begin, aRight.Begin);
			const auto end   = std::min(aLeft.End, aRight.End);
			return core::line_span{ .Begin = begin,
									.End   = begin < end ? end : begin };
		}

		/// \brief Appends one full-width background over a span of lines.
		void append_line_band(
			QList<QTextEdit::ExtraSelection>& aSelections,
			QPlainTextEdit&                   aPane,
			const core::line_span             aSpan,
			const QColor&                     aColor)
		{
			if (!aColor.isValid() || aSpan.empty()) {
				return;
			}
			for (auto line = aSpan.Begin; line < aSpan.End; ++line) {
				const auto block = aPane.document()->findBlockByNumber(
					static_cast<int>(line));
				if (!block.isValid()) {
					continue;
				}
				auto band   = QTextEdit::ExtraSelection{ };
				band.cursor = QTextCursor{ block };
				band.format.setProperty(QTextFormat::FullWidthSelection, true);
				band.format.setBackground(aColor);
				aSelections.push_back(band);
			}
		}

		/// \brief Appends the band marking where the reviewer is.
		/// \brief Washes the selected lines, bounded to what the pane shows.
		///
		/// \details A selection can be one line or a whole change group, and
		/// the core already says which: `semantic_selection_kind::group`
		/// carries the group's exact span per side. Painting only the
		/// preferred line whatever the kind made a group selection look like a
		/// line selection, and left the question of whether a change tint
		/// survives the wash unanswerable — a single washed row has nothing
		/// beside it to compare against (BPR-0205).
		///
		/// Clipped to the paintable window rather than the span, so selecting
		/// a group the size of the file costs the viewport and not the file.
		void append_selection_band(
			QList<QTextEdit::ExtraSelection>& aSelections,
			QPlainTextEdit&                   aPane,
			const core::line_span             aLines,
			const core::line_span             aPaintable)
		{
			const auto begin = std::max(aLines.Begin, aPaintable.Begin);
			const auto end   = std::min(aLines.End, aPaintable.End);
			auto       color = aPane.palette().color(QPalette::Highlight);
			color.setAlpha(core::selection_wash_alpha);
			for (auto line = begin; line < end; ++line) {
				const auto block = aPane.document()->findBlockByNumber(
					static_cast<int>(line));
				if (!block.isValid()) {
					continue;
				}
				auto band   = QTextEdit::ExtraSelection{ };
				band.cursor = QTextCursor{ block };
				band.format.setProperty(QTextFormat::FullWidthSelection, true);
				band.format.setBackground(color);
				aSelections.push_back(band);
			}
		}

		void set_line_selection(
			QPlainTextEdit&                             aPane,
			const std::optional<core::source_line_ref>& aLine)
		{
			auto selections = QList<QTextEdit::ExtraSelection>{ };
			if (aLine.has_value()) {
				const auto block = aPane.document()->findBlockByNumber(
					static_cast<int>(aLine->Line));
				if (block.isValid()) {
					auto selection   = QTextEdit::ExtraSelection{ };
					selection.cursor = QTextCursor{ block };
					selection.format.setProperty(
						QTextFormat::FullWidthSelection, true);
					auto color = aPane.palette().color(QPalette::Highlight);
					color.setAlpha(core::selection_wash_alpha);
					selection.format.setBackground(color);
					selections.push_back(selection);
				}
			}
			aPane.setExtraSelections(selections);
		}
	} // namespace

	diff_workbench::diff_workbench(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("diffWorkbench"));
		setAccessibleName(QStringLiteral("File comparison workbench"));
		setAccessibleDescription(QStringLiteral(
			"Linked natural-layout source panes, change correspondence map, and selected-change detail"));

		auto* const root = new QVBoxLayout{ this };
		root->setContentsMargins(0, 0, 0, 0);
		root->setSpacing(6);
		Status = new QLabel{ QStringLiteral("Select a file comparison"), this };
		Status->setObjectName(QStringLiteral("diffWorkbenchStatus"));
		Status->setAccessibleName(QStringLiteral("Comparison status"));
		Status->setAccessibleDescription(QStringLiteral(
			"Loading, failure, and semantic change count for the selected comparison"));
		Status->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
		root->addWidget(Status);

		Vertical = new QSplitter{ Qt::Vertical, this };
		Vertical->setObjectName(QStringLiteral("diffWorkbenchVerticalSplitter"));
		Vertical->setChildrenCollapsible(false);
		auto* const top       = new QWidget{ Vertical };
		auto* const topLayout = new QHBoxLayout{ top };
		topLayout->setContentsMargins(0, 0, 0, 0);
		topLayout->setSpacing(0);
		Comparison = new diff_comparison_splitter{ top };
		Comparison->setObjectName(QStringLiteral("diffComparisonSplitter"));
		Comparison->setAccessibleName(QStringLiteral("Linked source comparison"));
		Comparison->setChildrenCollapsible(false);
		Left = new source_pane{ Comparison };
		Left->setObjectName(QStringLiteral("diffLeftSource"));
		Left->setAccessibleName(QStringLiteral("Earlier source"));
		Left->setAccessibleDescription(QStringLiteral("Read-only earlier file version"));
		Right = new source_pane{ Comparison };
		Right->setObjectName(QStringLiteral("diffRightSource"));
		Right->setAccessibleName(QStringLiteral("Later source"));
		Right->setAccessibleDescription(QStringLiteral("Read-only later file version"));
		for (auto* const source : { Left, Right }) {
			source->setReadOnly(true);
			source->setLineWrapMode(QPlainTextEdit::NoWrap);
			source->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			source->setFocusPolicy(Qt::StrongFocus);
			source->viewport()->installEventFilter(this);
		}
		Comparison->addWidget(Left);
		Comparison->addWidget(Right);
		Comparison->setSizes({ 500, 500 });
		SharedScroll = new QScrollBar{ Qt::Vertical, top };
		SharedScroll->setObjectName(QStringLiteral("diffSharedScrollBar"));
		SharedScroll->setAccessibleName(QStringLiteral("Shared comparison position"));
		SharedScroll->setAccessibleDescription(QStringLiteral(
			"Moves both source panes through semantic comparison space"));
		SharedScroll->setFocusPolicy(Qt::StrongFocus);
		topLayout->addWidget(Comparison, 1);
		topLayout->addWidget(SharedScroll);

		Detail = new QPlainTextEdit{ Vertical };
		Detail->setObjectName(QStringLiteral("diffSelectedChangeDetail"));
		Detail->setAccessibleName(QStringLiteral("Selected change detail"));
		Detail->setAccessibleDescription(QStringLiteral(
			"Full-width line-over-line detail for the semantic selection"));
		Detail->setReadOnly(true);
		Detail->setLineWrapMode(QPlainTextEdit::NoWrap);
		Detail->setPlainText(QStringLiteral("No change selected"));
		Detail->setMaximumHeight(96);
		Vertical->addWidget(top);
		Vertical->addWidget(Detail);
		Vertical->setStretchFactor(0, 4);
		Vertical->setStretchFactor(1, 1);
		Vertical->setSizes({ 500, 150 });
		root->addWidget(Vertical, 1);

		LinkMap = Comparison->link_map();
		LinkMap->set_selection_announcements_enabled(false);
		Controller = new comparison_scroll_controller{ *SharedScroll, { }, this };
		Controller->bind_input_surface(
			comparison_scroll_receiver::left_pane,
			*Left->viewport(),
			Left->horizontalScrollBar());
		Controller->bind_input_surface(
			comparison_scroll_receiver::right_pane,
			*Right->viewport(),
			Right->horizontalScrollBar());
		Controller->bind_input_surface(
			comparison_scroll_receiver::link_map, *LinkMap);
		Controller->bind_key_surface(
			comparison_scroll_receiver::left_pane, *Left);
		Controller->bind_key_surface(
			comparison_scroll_receiver::right_pane, *Right);
		Controller->bind_key_surface(
			comparison_scroll_receiver::link_map, *LinkMap);
		Controller->set_frame_sink(
			[this](const comparison_scroll_frame& aFrame) { apply_frame(aFrame); });
		Controller->set_repaint_sink([this] { LinkMap->update(); });
		LinkMap->set_selection_sink(
			[this](const diff_link_map_identity& aIdentity) {
				select_link(aIdentity, false);
			});
		LinkMap->set_reveal_sink(
			[this](const diff_link_map_identity& aIdentity) {
				select_link(aIdentity, true);
			});

		QWidget::setTabOrder(Left, LinkMap);
		QWidget::setTabOrder(LinkMap, Right);
		QWidget::setTabOrder(Right, SharedScroll);
		QWidget::setTabOrder(SharedScroll, Detail);
	}

	diff_workbench::~diff_workbench() = default;

	void diff_workbench::publish(
		const application::diff_publication& aPublication)
	{
		if (
			!core::valid(aPublication.Context) || !aPublication.Result
			|| aPublication.Result->Alignment.Key != aPublication.Context.Key
			|| !CurrentContext.has_value()
			|| *CurrentContext != aPublication.Context) {
			++Statistics.RejectedStaleInput;
			return;
		}
		auto semanticIndex = core::build_diff_semantic_index(aPublication.Result);
		if (!semanticIndex.has_value()) {
			++Statistics.RejectedStaleInput;
			return;
		}
		const auto previousSelection = Interaction.Selection;
		CurrentResult                = aPublication.Result;
		SemanticIndex                = std::move(semanticIndex);
		// Built once here rather than searched per segment per repaint. The
		// move table is keyed by segment identity and is unordered with
		// respect to the segment list, so the alternative is a linear search
		// inside the paint loop.
		MovedSegments.clear();
		for (const auto& move : CurrentResult->Alignment.Moves) {
			MovedSegments.insert(move.SourceDeletion);
			MovedSegments.insert(move.DestinationInsertion);
		}
		++Statistics.SemanticIndexBuilds;
		Statistics.SemanticIndexEntries
			= SemanticIndex->statistics().IndexEntries;
		Left->setPlainText(text_document(CurrentResult->LeftDocument));
		Right->setPlainText(text_document(CurrentResult->RightDocument));
		++Statistics.SemanticQueries;
		static_cast<void>(core::reconcile_diff_interaction(Interaction, *SemanticIndex));
		if (!Interaction.Selection.has_value()) {
			if (!CurrentResult->Alignment.Groups.empty()) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_group_intent{
						.Group = CurrentResult->Alignment.Groups.front().Id,
					}));
			} else if (CurrentResult->Alignment.LeftLineCount != 0U) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_line_intent{
						.Line = {
							.Comparison = CurrentResult->Alignment.Digest,
							.Side       = core::diff_side::left,
							.Line       = 0U,
						},
					}));
			} else if (CurrentResult->Alignment.RightLineCount != 0U) {
				++Statistics.SemanticQueries;
				static_cast<void>(core::transition(
					Interaction,
					*SemanticIndex,
					core::select_diff_line_intent{
						.Line = {
							.Comparison = CurrentResult->Alignment.Digest,
							.Side       = core::diff_side::right,
							.Line       = 0U,
						},
					}));
			}
		}
		++Statistics.Publications;
		update_status(
			QStringLiteral("Diff ready — %1 semantic changes%2")
				.arg(CurrentResult->Alignment.Groups.size())
				.arg(aPublication.CacheHit ? QStringLiteral(" (cached)") : QString{ }));
		update_detail();
		update_selection_paint();
		schedule_geometry_rebuild(
			measured_geometry_change::accepted_diff,
			diff_geometry_rebuild_trigger::accepted_diff);
		if (Interaction.Selection != previousSelection) {
			emit_selection();
		}
	}

	void diff_workbench::fail(
		const application::diff_failure_publication& aFailure)
	{
		if (!CurrentContext.has_value() || *CurrentContext != aFailure.Context) {
			++Statistics.RejectedStaleInput;
			return;
		}
		++Statistics.Failures;
		clear_projection(reason_text(aFailure.Reason));
		CurrentContext.reset();
	}

	void diff_workbench::invalidate(
		const core::pull_request_authority& aAuthority)
	{
		if (
			!CurrentContext.has_value()
			|| CurrentContext->Key.Authority != aAuthority) {
			return;
		}
		++Statistics.Invalidations;
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		clear_projection(QStringLiteral("Comparison invalidated"));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_loading(const core::diff_request_context& aContext)
	{
		if (!core::valid(aContext)) {
			return;
		}
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext          = aContext;
		clear_projection(QStringLiteral("Loading selected file comparison…"));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::clear()
	{
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentContext.reset();
		clear_projection(QStringLiteral("Select a file comparison"));
		if (!hadSelection) {
			emit_selection_observer(false);
		}
	}

	void diff_workbench::set_selection_sink(selection_sink aSink)
	{
		SelectionSink = std::move(aSink);
		emit_selection();
	}

	void diff_workbench::set_selection_observer(selection_observer aObserver)
	{
		SelectionObserver = std::move(aObserver);
		emit_selection();
	}

	auto diff_workbench::reveal_conversation_line(
		const core::diff_side aSide,
		const std::uint32_t   aLine) -> bool
	{
		if (!CurrentResult) {
			return false;
		}
		const auto requested = core::source_line_ref{
			.Comparison = CurrentResult->Alignment.Digest,
			.Side       = aSide,
			.Line       = aLine,
		};
		const auto previous = Interaction.Selection;
		if (!select_line(aSide, aLine)) {
			return false;
		}
		ConversationAnchor = requested;
		static_cast<void>(reveal_exact_line(*Interaction.Selection, requested));
		update_diff_paint();
		if (Interaction.Selection == previous) {
			emit_selection_observer(true);
		}
		return true;
	}

	void diff_workbench::clear_conversation_anchor()
	{
		if (!ConversationAnchor.has_value()) {
			return;
		}
		ConversationAnchor.reset();
		update_diff_paint();
	}

	auto diff_workbench::conversation_anchor() const noexcept
		-> const std::optional<core::source_line_ref>&
	{
		return ConversationAnchor;
	}

	auto diff_workbench::conversation_anchor_marker_rect(
		const core::diff_side aSide) const -> QRect
	{
		return pane(aSide == core::diff_side::left ? Left : Right)
			->conversation_anchor_rect();
	}

	auto diff_workbench::left_pane() const noexcept -> QPlainTextEdit*
	{
		return Left;
	}

	auto diff_workbench::right_pane() const noexcept -> QPlainTextEdit*
	{
		return Right;
	}

	auto diff_workbench::detail_pane() const noexcept -> QPlainTextEdit*
	{
		return Detail;
	}

	auto diff_workbench::shared_scrollbar() const noexcept -> QScrollBar*
	{
		return SharedScroll;
	}

	auto diff_workbench::comparison_splitter() const noexcept
		-> diff_comparison_splitter*
	{
		return Comparison;
	}

	/// \brief Tells the link map where the pane viewports actually are.
	///
	/// \details The map is a sibling of the panes, so its own top and height
	/// are not theirs: a pane contributes a frame, and a horizontal scrollbar
	/// takes height from the viewport without taking any from the map. Ribbon
	/// endpoints are computed in viewport coordinates, so drawing them at
	/// their own y met the panes in the wrong place, which is BPR-0208. The
	/// band is declared rather than inferred because the difference depends on
	/// style and on whether a scrollbar is showing, and neither is knowable
	/// from the map.
	void diff_workbench::declare_link_map_band()
	{
		auto* const parent = LinkMap->parentWidget();
		if (parent == nullptr) {
			return;
		}
		const auto mapTop = LinkMap->mapTo(parent, QPoint{ 0, 0 }).y();
		const auto paneTop
			= Left->viewport()->mapTo(parent, QPoint{ 0, 0 }).y();
		LinkMap->set_viewport_band(
			static_cast<qreal>(paneTop - mapTop),
			static_cast<qreal>(Left->viewport()->height()));
	}

	auto diff_workbench::link_map() const noexcept -> diff_link_map*
	{
		return LinkMap;
	}

	auto diff_workbench::scroll_controller() const noexcept
		-> comparison_scroll_controller*
	{
		return Controller;
	}

	auto diff_workbench::selection() const noexcept
		-> const std::optional<core::semantic_selection>&
	{
		return Interaction.Selection;
	}

	auto diff_workbench::current_context() const noexcept
		-> const std::optional<core::diff_request_context>&
	{
		return CurrentContext;
	}

	auto diff_workbench::current_result() const noexcept
		-> const std::shared_ptr<const core::computed_file_diff>&
	{
		return CurrentResult;
	}

	auto diff_workbench::status_text() const -> QString
	{
		return Status->text();
	}

	auto diff_workbench::statistics() const noexcept
		-> diff_workbench_statistics
	{
		// Boundary rules are counted where they are drawn, not where they are
		// requested. The bands can be tallied at composition time because an
		// extra selection reaching the pane is the whole of their effect; a
		// rule is only real once the viewport has painted it, and counting the
		// intent would let a rule that never reaches the screen read as
		// painted.
		auto statistics                   = Statistics;
		statistics.GroupBoundariesPainted = pane(Left)->painted_rules()
			+ pane(Right)->painted_rules();
		return statistics;
	}

	auto diff_workbench::event(QEvent* const aEvent) -> bool
	{
		const auto result = QWidget::event(aEvent);
		switch (aEvent->type()) {
		case QEvent::FontChange:
		case QEvent::ApplicationFontChange:
			schedule_geometry_rebuild(
				measured_geometry_change::font,
				diff_geometry_rebuild_trigger::font_event);
			break;
		case QEvent::PaletteChange:
		case QEvent::ApplicationPaletteChange:
			update_selection_paint();
			break;
		case QEvent::StyleChange:
			update_selection_paint();
			schedule_geometry_rebuild(
				measured_geometry_change::viewport_or_divider,
				diff_geometry_rebuild_trigger::style_event);
			break;
		case QEvent::Show:
			schedule_geometry_rebuild(
				measured_geometry_change::screen,
				diff_geometry_rebuild_trigger::show_event);
			break;
		case QEvent::DevicePixelRatioChange:
			schedule_geometry_rebuild(
				measured_geometry_change::device_pixel_ratio,
				diff_geometry_rebuild_trigger::device_pixel_ratio_event);
			break;
		case QEvent::ScreenChangeInternal:
			schedule_geometry_rebuild(
				measured_geometry_change::screen,
				diff_geometry_rebuild_trigger::screen_change_event);
			break;
		default:
			if (aEvent->type() == code_typography_event::registered_type()) {
				adopt_code_typography(
					*static_cast<const code_typography_event*>(aEvent));
			}
			break;
		}
		return result;
	}

	void diff_workbench::adopt_code_typography(
		const code_typography_event& aTypography)
	{
		// A combined change already reached this surface as a font change, so
		// rebuilding again on every application would double the work and
		// attribute it to the wrong fact. Only a tab stop that actually moved
		// invalidates here, which is the case Qt reports no event for.
		const auto distance = aTypography.tab_stop_distance();
		if (AppliedTabStopDistance && qFuzzyCompare(*AppliedTabStopDistance, distance)) {
			return;
		}

		const auto known       = AppliedTabStopDistance.has_value();
		AppliedTabStopDistance = distance;
		if (!known) {
			return;
		}

		schedule_geometry_rebuild(
			measured_geometry_change::tab_width,
			diff_geometry_rebuild_trigger::tab_width);
	}

	auto diff_workbench::eventFilter(QObject* const aWatched, QEvent* const aEvent)
		-> bool
	{
		const auto sourceViewport = aWatched == Left->viewport()
			|| aWatched == Right->viewport();
		if (sourceViewport && aEvent->type() == QEvent::Resize) {
			schedule_geometry_rebuild(
				measured_geometry_change::viewport_or_divider,
				diff_geometry_rebuild_trigger::pane_viewport_resize);
		}
		if (
			CurrentResult && aEvent->type() == QEvent::MouseButtonPress
			&& sourceViewport) {
			const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
			if (mouse->button() == Qt::LeftButton) {
				auto* const source = aWatched == Left->viewport() ? Left : Right;
				const auto  cursor = source->cursorForPosition(mouse->position().toPoint());
				if (cursor.blockNumber() >= 0) {
					static_cast<void>(select_line(
						aWatched == Left->viewport()
							? core::diff_side::left
							: core::diff_side::right,
						static_cast<std::uint32_t>(cursor.blockNumber()),
						true));
				}
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void diff_workbench::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		// Before the rebuild, because a resize can add or remove a pane's
		// horizontal scrollbar and that changes the band without changing
		// anything the axis knows about.
		declare_link_map_band();
		schedule_geometry_rebuild(
			measured_geometry_change::viewport_or_divider,
			diff_geometry_rebuild_trigger::workbench_resize);
	}

	void diff_workbench::schedule_geometry_rebuild(
		const measured_geometry_change      aChange,
		const diff_geometry_rebuild_trigger aTrigger)
	{
		if (!CurrentResult) {
			return;
		}
		if (aChange != measured_geometry_change::none) {
			AxisCache.invalidate(aChange);
		}
		PendingGeometryTriggers = PendingGeometryTriggers | aTrigger;
		if (GeometryScheduled) {
			return;
		}
		GeometryScheduled = true;
		QPointer<diff_workbench> self{ this };
		QTimer::singleShot(0, this, [self] {
			if (self != nullptr) {
				self->GeometryScheduled = false;
				self->rebuild_geometry();
			}
		});
	}

	void diff_workbench::rebuild_geometry()
	{
		if (!CurrentResult) {
			PendingGeometryTriggers = diff_geometry_rebuild_trigger::none;
			return;
		}
		const auto changes      = AxisCache.pending_changes();
		const auto triggers     = PendingGeometryTriggers;
		PendingGeometryTriggers = diff_geometry_rebuild_trigger::none;
		Statistics.PaneMeasurements += 2U;
		const auto left = measure_plain_text_pane(
			*Left, CurrentResult->Alignment.LeftLineCount);
		const auto right = measure_plain_text_pane(
			*Right, CurrentResult->Alignment.RightLineCount);
		if (
			!left.has_value() || !right.has_value()
			|| AxisCache.rebuild(CurrentResult->Alignment, *left, *right)
				!= measured_alignment_error::none
			|| AxisCache.axis() == nullptr) {
			++Statistics.GeometryFailures;
			Controller->clear_axis();
			LinkMap->clear_geometry();
			return;
		}
		const auto axis            = *AxisCache.axis();
		const auto revealSelection = ConversationAnchor.has_value()
			|| !Controller->current_frame().has_value()
			|| Controller->current_frame()->Comparison != axis.comparison();
		declare_link_map_band();
		LinkMap->set_geometry(axis, CurrentResult->Alignment.Moves);
		Controller->set_axis(axis);
		++Statistics.GeometryRebuilds;
		if (Statistics.GeometryRebuildHistorySize
			< Statistics.GeometryRebuildHistory.size()) {
			Statistics.GeometryRebuildHistory[Statistics.GeometryRebuildHistorySize++] = {
				.Changes  = changes,
				.Triggers = triggers,
			};
		} else {
			Statistics.GeometryRebuildHistoryOverflow = true;
		}
		synchronize_geometry_selection(revealSelection);
	}

	void diff_workbench::apply_frame(const comparison_scroll_frame& aFrame)
	{
		if (
			ApplyingFrame || !CurrentResult
			|| aFrame.Comparison != CurrentResult->Alignment.Digest) {
			return;
		}
		ApplyingFrame          = true;
		const auto* const axis = AxisCache.axis();
		if (axis != nullptr && axis->comparison() == aFrame.Comparison) {
			auto rendered = aFrame;
			// Set the bar, then read the origin back off the same per-line
			// measurement the ribbon endpoints are built from. The value the
			// setter returns is a fraction of the scrollbar's travel, and
			// `QPlainTextEdit` scrolls in blocks with a last page it cannot
			// scroll past, so that fraction is not the fraction of the document
			// above the viewport. It agreed at the top of a file and drifted
			// down it, which put every ribbon out by the difference - BPR-0208.
			const auto leftEstimate = pane(Left)->set_document_origin(
				aFrame.Left.DocumentOriginY,
				std::max(
					0.0,
					axis->left_document_extent()
						- axis->left_viewport_extent()));
			const auto rightEstimate = pane(Right)->set_document_origin(
				aFrame.Right.DocumentOriginY,
				std::max(
					0.0,
					axis->right_document_extent()
						- axis->right_viewport_extent()));
			rendered.Left.DocumentOriginY = measured_document_origin(
				*pane(Left), *axis, core::diff_side::left, leftEstimate);
			rendered.Right.DocumentOriginY = measured_document_origin(
				*pane(Right), *axis, core::diff_side::right, rightEstimate);
			rendered.Left.ViewportY = rendered.Left.ProjectedDocumentY
				- rendered.Left.DocumentOriginY;
			rendered.Right.ViewportY = rendered.Right.ProjectedDocumentY
				- rendered.Right.DocumentOriginY;
			static_cast<void>(LinkMap->set_frame(rendered));
		} else {
			static_cast<void>(LinkMap->set_frame(aFrame));
		}
		ApplyingFrame = false;
		// Painting is bounded to what the panes can show, so a scroll that
		// reveals new lines must repaint them. Without this the bound would be
		// a rendering bug rather than an optimisation: the reviewer would
		// scroll into a changed region and find it untinted.
		update_diff_paint();
		++Statistics.FrameApplications;
	}

	auto diff_workbench::select_line(
		const core::diff_side aSide,
		const std::uint32_t   aLine,
		const bool            aReportUnchangedInteraction) -> bool
	{
		if (!CurrentResult || !SemanticIndex.has_value()) {
			clear_conversation_anchor();
			return false;
		}
		const auto requested = core::source_line_ref{
			.Comparison = CurrentResult->Alignment.Digest,
			.Side       = aSide,
			.Line       = aLine,
		};
		if (ConversationAnchor != requested) {
			clear_conversation_anchor();
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::select_diff_line_intent{
				.Line = requested,
			});
		if (changed) {
			++Statistics.SelectionChanges;
			apply_interaction(false, true);
			emit_selection(true);
		}
		const auto selected = Interaction.Selection.has_value()
			&& Interaction.Selection->PreferredLine == requested;
		if (!changed && selected && aReportUnchangedInteraction) {
			// A physical click remains an interaction even when the core selection
			// was already reconciled to this exact line.  The application sink has
			// no new state to persist, but the Qt-local observer still needs the
			// gesture origin to reverse-filter conversations from file scope.
			emit_selection_observer(true);
		}
		return selected;
	}

	void diff_workbench::select_link(
		const diff_link_map_identity& aIdentity,
		const bool                    aReveal)
	{
		clear_conversation_anchor();
		if (!CurrentResult || !SemanticIndex.has_value()
			|| !aIdentity.Group.has_value()) {
			return;
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::select_diff_group_intent{ .Group = *aIdentity.Group });
		if (changed) {
			++Statistics.SelectionChanges;
		}
		apply_interaction(aReveal, changed);
		if (changed) {
			emit_selection(true);
		}
	}

	auto diff_workbench::navigate_change(
		const core::change_navigation_direction aDirection,
		const core::change_navigation_policy    aPolicy)
		-> bool
	{
		clear_conversation_anchor();
		if (!CurrentResult || !SemanticIndex.has_value()) {
			return false;
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::navigate_diff_change_intent{
				.Direction = aDirection,
				.Policy    = aPolicy,
			});
		if (changed) {
			++Statistics.SelectionChanges;
			apply_interaction(true, changed);
			emit_selection(true);
		}
		return changed;
	}

	void diff_workbench::synchronize_geometry_selection(const bool aReveal)
	{
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			LinkMap->set_selected(std::nullopt, false);
			return;
		}
		const auto& selection = *Interaction.Selection;
		if (selection.Segment.has_value()) {
			LinkMap->set_selected(
				diff_link_map_identity{
					.Segment = *selection.Segment,
					.Group   = selection.Group,
				},
				false);
			if (aReveal) {
				const auto exact = ConversationAnchor.has_value()
					&& reveal_exact_line(selection, *ConversationAnchor);
				if (!exact) {
					static_cast<void>(Controller->reveal_segment(*selection.Segment));
				}
			}
		} else {
			LinkMap->set_selected(std::nullopt, false);
		}
	}

	auto diff_workbench::reveal_exact_line(
		const core::semantic_selection& aSelection,
		const core::source_line_ref&    aLine) -> bool
	{
		const auto* const axis = AxisCache.axis();
		if (axis == nullptr || !aSelection.Segment.has_value()) {
			return false;
		}
		const auto measured = std::ranges::find(
			axis->segments(), *aSelection.Segment, &measured_alignment_segment::Id);
		if (measured == axis->segments().end()) {
			return false;
		}
		const auto origin = aLine.Side == core::diff_side::left
			? axis->left_line_origin(aLine.Line)
			: axis->right_line_origin(aLine.Line);
		if (!origin.has_value()) {
			return false;
		}
		const auto begin    = aLine.Side == core::diff_side::left
			? measured->LeftBegin
			: measured->RightBegin;
		const auto end      = aLine.Side == core::diff_side::left
			? measured->LeftEnd
			: measured->RightEnd;
		const auto fraction = end > begin ? (*origin - begin) / (end - begin) : 0.0;
		return Controller->reveal_segment(*aSelection.Segment, fraction);
	}

	void diff_workbench::apply_interaction(
		const bool aReveal,
		const bool aAnnounceSelection)
	{
		synchronize_geometry_selection(aReveal);
		update_detail();
		update_selection_paint();
		if (aAnnounceSelection) {
			announce_accessible_status(
				Status->text() + QStringLiteral(". ")
				+ Detail->accessibleDescription());
		}
	}

	void diff_workbench::update_detail()
	{
		CurrentDetail.reset();
		if (!CurrentResult || !SemanticIndex.has_value()
			|| !Interaction.Selection.has_value()) {
			Detail->setPlainText(QStringLiteral("No change selected"));
			return;
		}
		++Statistics.SemanticQueries;
		CurrentDetail = core::detail_for_selection(
			*SemanticIndex, *Interaction.Selection);
		const auto& detail   = *CurrentDetail;
		const auto  leftText = detail_side_text(
			CurrentResult->LeftDocument,
			detail.LeftLine,
			detail,
			core::diff_side::left);
		const auto rightText = detail_side_text(
			CurrentResult->RightDocument,
			detail.RightLine,
			detail,
			core::diff_side::right);
		auto heading = QStringLiteral("Selected line %1")
						   .arg(Interaction.Selection->PreferredLine.Line + 1U);
		if (Interaction.Selection->Group.has_value()) {
			const auto index = SemanticIndex->group_index(
				*Interaction.Selection->Group);
			if (index.has_value()) {
				heading = QStringLiteral("Change %1 of %2")
							  .arg(*index + 1U)
							  .arg(CurrentResult->Alignment.Groups.size());
			}
		}
		Detail->setPlainText(
			heading + QStringLiteral("\n\nEarlier\n") + leftText
			+ QStringLiteral("\n\nLater\n") + rightText);
		Detail->setAccessibleDescription(heading);
	}

	void diff_workbench::update_selection_paint()
	{
		update_diff_paint();
	}

	void diff_workbench::adopt_appearance(const appearance_profile& aProfile)
	{
		using role  = core::appearance_role;
		auto colors = diff_presentation_colors{
			.Added            = aProfile.color(role::diff_added_background),
			.Removed          = aProfile.color(role::diff_removed_background),
			.IntralineAdded   = aProfile.color(role::diff_intraline_added_background),
			.IntralineRemoved = aProfile.color(role::diff_intraline_removed_background),
			.Moved            = aProfile.color(role::diff_moved_background),
			.Syntax           = { },
			.Palette          = aProfile.palette(),
			.Variant          = aProfile.variant(),
			.Supplied         = true,
		};
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			colors.Syntax[index] = aProfile.color(
				static_cast<core::appearance_role>(index));
		}
		Colors = std::move(colors);
		update_diff_paint();
	}

	auto diff_workbench::adopt_token_spans(
		const core::diff_side           aSide,
		application::shared_token_spans aSpans) -> bool
	{
		if (aSpans && !std::ranges::is_sorted(*aSpans, { }, [](const application::token_span& aSpan) {
				return aSpan.Line;
			})) {
			// Refused whole rather than sorted here. A copy would double the
			// memory of the one thing this design deliberately shares, and a
			// linear scan per repaint is the unbounded cost BPR-0196 removed.
			// Refusing makes a classifier that broke the contract visible.
			++Statistics.RejectedTokenSpans;
			return false;
		}
		auto& retained = aSide == core::diff_side::left ? LeftSpans : RightSpans;
		retained       = std::move(aSpans);
		++Statistics.TokenSpanAdoptions;
		update_diff_paint();
		return true;
	}

	auto diff_workbench::token_spans(const core::diff_side aSide) const noexcept
		-> const application::shared_token_spans&
	{
		return aSide == core::diff_side::left ? LeftSpans : RightSpans;
	}

	/// \brief Paints the diff the core decided, in one pass per pane.
	///
	/// \details Order is the whole design. Qt layers extra selections in the
	/// order they are given, so the line band goes down first, the translucent
	/// selection wash over it, and the opaque intraline emphasis last. A reviewer therefore sees
	/// "this line changed", "this is the part that changed", and "this is where
	/// I am" as three readable levels. Intraline must be last: otherwise the
	/// selected-row wash replaces the exact changed-character color in the
	/// raster even though the corresponding ExtraSelection still exists.
	///
	/// Nothing here decides what changed. Every span comes from
	/// `core::diff_semantics`, and a second opinion about which lines differ is
	/// the defect RFC-BPR-0005's reuse rules forbid.
	void diff_workbench::update_diff_paint()
	{
		pane(Left)->set_conversation_anchor(
			ConversationAnchor.has_value()
					&& ConversationAnchor->Side == core::diff_side::left
				? std::optional{ ConversationAnchor->Line }
				: std::nullopt);
		pane(Right)->set_conversation_anchor(
			ConversationAnchor.has_value()
					&& ConversationAnchor->Side == core::diff_side::right
				? std::optional{ ConversationAnchor->Line }
				: std::nullopt);
		apply_group_boundaries();

		const auto paint = [this](
							   QPlainTextEdit&       aPane,
							   const core::diff_side aSide) {
			auto       selections = QList<QTextEdit::ExtraSelection>{ };
			const auto paintable  = paintable_line_bounds(aPane);

			// Under the bands rather than over them, because it is a
			// foreground and they are backgrounds.
			apply_syntax_formats(aPane, aSide, paintable);

			if (CurrentResult && Colors.Supplied) {
				const auto& alignment = CurrentResult->Alignment;
				// Segments are ordered by line, so the visible run is found by
				// search. Scanning all of them was the shape of cost BPR-0196's
				// selection count could not see: it emitted a bounded number of
				// bands and walked every segment in the file to decide which,
				// which on an all-changed comparison is one visit per line per
				// repaint.
				const auto first = std::ranges::lower_bound(
					alignment.Segments,
					paintable.Begin,
					{ },
					[aSide](const core::alignment_segment& aSegment) {
						return (aSide == core::diff_side::left ? aSegment.Left
															   : aSegment.Right)
							.End;
					});
				for (auto segment = first; segment != alignment.Segments.end();
					 ++segment) {
					const auto span = aSide == core::diff_side::left
						? segment->Left
						: segment->Right;
					if (span.Begin >= paintable.End) {
						break;
					}
					++Statistics.AlignmentSegmentsVisited;
					const auto isMoved = moved(segment->Id);
					const auto color   = [&]() -> QColor {
						if (isMoved) {
							// A move is a deletion here and an insertion there.
							// Painting it as an ordinary removal and addition
							// would discard the one thing move detection
							// computed: that these two blocks are the same
							// code.
							return Colors.Moved;
						}
						switch (segment->Kind) {
						case core::alignment_kind::equal:
							// Unchanged lines are left untinted on purpose. The
							// eye should go to what moved, and a background on
							// every line marks nothing.
							return { };
						case core::alignment_kind::left_only:
							return Colors.Removed;
						case core::alignment_kind::right_only:
							return Colors.Added;
						case core::alignment_kind::replacement:
							// A replacement is a removal on the left and an
							// addition on the right, which is what the reviewer
							// is actually looking at on each side.
							return aSide == core::diff_side::left
								? Colors.Removed
								: Colors.Added;
						}
						return { };
					}();
					const auto visible = intersection(span, paintable);
					append_line_band(selections, aPane, visible, color);
					if (isMoved && !visible.empty()) {
						++Statistics.MovedBandsPainted;
					}
				}
			}

			auto washed = std::optional<core::line_span>{ };
			if (CurrentResult && Interaction.Selection.has_value()) {
				const auto lines = selected_lines(aSide);
				if (lines.has_value()) {
					append_selection_band(
						selections, aPane, *lines, paintable);
					washed = lines;
				}
			}

			if (CurrentResult && Colors.Supplied) {
				append_intraline_emphasis(
					selections, aPane, aSide, paintable, washed);
			}

			aPane.setExtraSelections(selections);
		};

		paint(*Left, core::diff_side::left);
		paint(*Right, core::diff_side::right);
	}

	/// \brief The lines one side's selection covers.
	///
	/// \details A group selection names a change group, and the group carries
	/// its own span per side, so the answer is exact rather than derived from
	/// the preferred line. A line selection is that one line. Anything else —
	/// a group the alignment no longer holds — yields nothing rather than
	/// falling back to the preferred line, because a selection drawn somewhere
	/// other than where the model says it is would be worse than none.
	auto diff_workbench::selected_lines(const core::diff_side aSide) const
		-> std::optional<core::line_span>
	{
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			return std::nullopt;
		}
		const auto& selection = *Interaction.Selection;
		if (selection.Kind == core::semantic_selection_kind::group) {
			if (!selection.Group.has_value()) {
				return std::nullopt;
			}
			const auto group = std::ranges::find(
				CurrentResult->Alignment.Groups,
				*selection.Group,
				&core::change_group::Id);
			if (group == CurrentResult->Alignment.Groups.end()) {
				return std::nullopt;
			}
			return aSide == core::diff_side::left ? group->Left : group->Right;
		}
		if (!CurrentDetail.has_value()) {
			return std::nullopt;
		}
		const auto& line = aSide == core::diff_side::left
			? CurrentDetail->LeftLine
			: CurrentDetail->RightLine;
		return line.has_value()
			? std::optional<core::line_span>{ core::line_span{
				  .Begin = line->Line, .End = line->Line + 1U } }
			: std::nullopt;
	}

	auto diff_workbench::moved(const core::alignment_segment_id& aSegment) const
		-> bool
	{
		return MovedSegments.contains(aSegment);
	}

	/// \brief Hands each pane the lines its change groups start on.
	///
	/// \details The panes show whole documents side by side, not a unified hunk
	/// stream, so a boundary cannot be a row of text: inserting one would make
	/// pane line N stop being document line N, and every mapping built on that
	/// equality - intraline emphasis, token spans, selection, the measured axis -
	/// would silently shift by the number of rows above it.
	///
	/// So it is drawn as a rule on the edge above the group's first line, which
	/// is where the boundary actually is. The earlier version tinted the
	/// unchanged line above the group instead, and BPR-0202 records why that was
	/// wrong: a full-width fill is how this surface says "this line changed", and
	/// spending it on a line that did not change inverts the vocabulary. At
	/// attended review the marking was read as the selected line.
	///
	/// A group starting at line zero is skipped. There is no edge above the first
	/// line of a file, and a rule drawn there would mark the document's start
	/// rather than a separation between two groups.
	void diff_workbench::apply_group_boundaries()
	{
		auto left  = std::vector<std::uint32_t>{ };
		auto right = std::vector<std::uint32_t>{ };
		if (CurrentResult) {
			for (const auto& group : CurrentResult->Alignment.Groups) {
				if (group.First.Left > 0U) {
					left.push_back(group.First.Left);
				}
				if (group.First.Right > 0U) {
					right.push_back(group.First.Right);
				}
			}
		}
		std::ranges::sort(left);
		std::ranges::sort(right);
		pane(Left)->set_group_boundaries(std::move(left));
		pane(Right)->set_group_boundaries(std::move(right));

		const auto rule = Colors.Syntax[static_cast<std::size_t>(
			core::appearance_role::diff_group_boundary_rule)];
		pane(Left)->set_boundary_rule_color(rule);
		pane(Right)->set_boundary_rule_color(rule);

		// Roles RFC-BPR-0006 declared and `appearance_contrast` has been
		// validating all along; BPR-0455 is the first thing to read them.
		const auto gutterBackground = Colors.Syntax[static_cast<std::size_t>(
			core::appearance_role::code_gutter_background)];
		const auto gutterText       = Colors.Syntax[static_cast<std::size_t>(
			core::appearance_role::code_line_number_text)];
		pane(Left)->set_gutter_colors(gutterBackground, gutterText);
		pane(Right)->set_gutter_colors(gutterBackground, gutterText);
	}

	/// \brief Colours the classified tokens of the lines a pane can show.
	///
	/// \details The formats go on each block's own layout rather than into the
	/// document. `QTextCursor::setCharFormat` would mutate the document once
	/// per span — an edit, an undo entry, and a relayout apiece — to express a
	/// change that alters no text. The block layout is what a highlighter
	/// writes to for the same reason, and it is destroyed with the block, so
	/// nothing written here can outlive the document it describes.
	///
	/// Syntax is a foreground and the diff bands are backgrounds, so the two
	/// compose rather than compete: an added line keeps its tint and its
	/// keywords keep their colour. That is why syntax is not an extra
	/// selection — the bands are, and a selection that had to carry both would
	/// make one of them win.
	///
	/// Bounded to the paintable window, and applied only where the formats
	/// actually differ. Re-setting a block's formats marks its contents dirty,
	/// so an unconditional write would relayout every visible block on every
	/// scroll to restate colours it already carried.
	void diff_workbench::apply_syntax_formats(
		QPlainTextEdit&       aPane,
		const core::diff_side aSide,
		const core::line_span aPaintable)
	{
		const auto& spans = aSide == core::diff_side::left ? LeftSpans
														   : RightSpans;
		if (!spans || !CurrentResult) {
			return;
		}
		const auto& document = aSide == core::diff_side::left
			? CurrentResult->LeftDocument
			: CurrentResult->RightDocument;
		if (!document) {
			return;
		}

		// The window is found by search because the spans are ordered, which
		// `adopt_token_spans` established. Scanning from the front would make
		// the cost of one screenful proportional to the whole file.
		auto span = std::ranges::lower_bound(
			*spans,
			aPaintable.Begin,
			{ },
			[](const application::token_span& aSpan) { return aSpan.Line; });
		const auto last = spans->end();
		for (auto line = aPaintable.Begin;
			 line < aPaintable.End && line < document->Lines.size();
			 ++line) {
			while (span != last && span->Line < line) {
				++span;
				++Statistics.SyntaxSpansVisited;
			}
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(line));
			if (!block.isValid() || block.layout() == nullptr) {
				continue;
			}
			const auto& decoded = document->Lines[line];
			const auto  bytes   = std::span<const std::uint8_t>{
				document->Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(
					decoded.Content.End - decoded.Content.Begin)
			};
			auto formats = QList<QTextLayout::FormatRange>{ };
			for (; span != last && span->Line == line; ++span) {
				++Statistics.SyntaxSpansVisited;
				const auto index = static_cast<std::size_t>(span->Role);
				if (
					span->Length == 0U || index >= Colors.Syntax.size()
					|| span->Role == core::appearance_role::syntax_normal) {
					// Normal text is what the pane already draws. Restating it
					// as a format would cost a range per unclassified word to
					// change nothing.
					continue;
				}
				const auto& color = Colors.Syntax[index];
				if (!color.isValid()) {
					continue;
				}
				const auto from = character_offset(bytes, span->Column);
				const auto to   = character_offset(
					bytes,
					static_cast<std::uint64_t>(span->Column) + span->Length);
				if (from >= to) {
					continue;
				}
				auto range   = QTextLayout::FormatRange{ };
				range.start  = from;
				range.length = to - from;
				range.format.setForeground(color);
				formats.push_back(range);
			}
			if (block.layout()->formats() == formats) {
				continue;
			}
			block.layout()->setFormats(formats);
			aPane.document()->markContentsDirty(
				block.position(), block.length());
			++Statistics.SyntaxBlocksFormatted;
		}
	}

	/// \brief Marks the exact byte spans the core reported as changed.
	void diff_workbench::append_intraline_emphasis(
		QList<QTextEdit::ExtraSelection>&    aSelections,
		QPlainTextEdit&                      aPane,
		const core::diff_side                aSide,
		const core::line_span                aPaintable,
		const std::optional<core::line_span> aWashed)
	{
		const auto& document = aSide == core::diff_side::left
			? CurrentResult->LeftDocument
			: CurrentResult->RightDocument;
		if (!document) {
			return;
		}
		const auto color = aSide == core::diff_side::left
			? Colors.IntralineRemoved
			: Colors.IntralineAdded;
		if (!color.isValid()) {
			return;
		}

		const auto ownForeground = Colors.Supplied
			&& core::emphasis_needs_own_foreground(
									   Colors.Palette,
									   aSide == core::diff_side::left
										   ? core::appearance_role::diff_intraline_removed_background
										   : core::appearance_role::diff_intraline_added_background,
									   Colors.Variant);

		// The fill a selected row's mark uses. The requirement is exact: a blue
		// line gets a blue mark, just darker or lighter. The mark therefore
		// derives from the band that row is actually wearing -- its own colour
		// with the wash over it -- and the core keeps the hue while moving
		// saturation and lightness away from the ground.
		const auto derivedFill = [this, &aPane](const core::diff_side aRowSide) {
			const auto band = aRowSide == core::diff_side::left ? Colors.Removed
																: Colors.Added;
			if (!band.isValid()) {
				return QColor{ };
			}
			const auto highlight = aPane.palette().color(QPalette::Highlight);
			const auto washed    = core::composited(
				core::appearance_color{ static_cast<std::uint8_t>(highlight.red()),
										static_cast<std::uint8_t>(highlight.green()),
										static_cast<std::uint8_t>(highlight.blue()),
										255U },
				core::appearance_color{ static_cast<std::uint8_t>(band.red()),
										static_cast<std::uint8_t>(band.green()),
										static_cast<std::uint8_t>(band.blue()),
										255U },
				core::selection_wash_alpha);
			const auto mark = core::derived_emphasis_fill(washed, Colors.Variant);
			return QColor{ mark.Red, mark.Green, mark.Blue };
		};

		const auto& pairs = CurrentResult->Alignment.Pairs;
		for (const auto& edit : CurrentResult->Intraline) {
			const auto pair = std::ranges::find_if(
				pairs,
				[&edit](const core::line_pair& aPair) {
					return aPair.Id == edit.Pair;
				});
			if (pair == pairs.end()) {
				continue;
			}
			const auto line = aSide == core::diff_side::left ? pair->LeftLine
															 : pair->RightLine;
			const auto span = aSide == core::diff_side::left ? edit.Left
															 : edit.Right;
			if (span.Begin >= span.End || line >= document->Lines.size()
				|| line < aPaintable.Begin || line >= aPaintable.End) {
				continue;
			}
			const auto& decoded = document->Lines[line];
			const auto  bytes   = std::span<const std::uint8_t>{
				document->Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(
					decoded.Content.End - decoded.Content.Begin)
			};
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(line));
			if (!block.isValid()) {
				continue;
			}
			const auto from = character_offset(bytes, span.Begin);
			const auto to   = character_offset(bytes, span.End);
			if (from >= to) {
				continue;
			}
			const auto selected = aWashed.has_value()
				&& line >= aWashed->Begin && line < aWashed->End;

			auto emphasis   = QTextEdit::ExtraSelection{ };
			emphasis.cursor = QTextCursor{ block };
			emphasis.cursor.setPosition(block.position() + from);
			emphasis.cursor.setPosition(
				block.position() + to, QTextCursor::KeepAnchor);
			// On a selected row the fill is derived from what that row is
			// actually wearing, so it is a deeper shade of the band rather
			// than a pink patch on a blue one. Everywhere else the accepted
			// fill stays exactly as accepted.
			const auto fill = selected ? derivedFill(aSide) : color;
			emphasis.format.setBackground(fill);
			// A fill bright enough to be unmistakable in a high-contrast
			// profile cannot also let that profile's own syntax colours read
			// on it, so the core says when the mark carries its own text
			// colour instead.
			if ((selected || ownForeground) && fill.isValid()) {
				const auto chosen = core::emphasis_foreground(
					core::appearance_color{ static_cast<std::uint8_t>(fill.red()),
											static_cast<std::uint8_t>(fill.green()),
											static_cast<std::uint8_t>(fill.blue()),
											255U });
				emphasis.format.setForeground(
					QColor{ chosen.Red, chosen.Green, chosen.Blue });
			}
			aSelections.push_back(emphasis);
		}
	}

	void diff_workbench::announce_accessible_status(QString aText)
	{
		if (aText.isEmpty() || aText == LastAnnouncement) {
			return;
		}
		LastAnnouncement = std::move(aText);
		const auto self  = QPointer<diff_workbench>{ this };
		auto       event = QAccessibleAnnouncementEvent{ this, LastAnnouncement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
		if (self == nullptr) {
			return;
		}
	}

	void diff_workbench::update_status(QString aText)
	{
		Status->setText(std::move(aText));
		Status->setAccessibleDescription(Status->text());
		announce_accessible_status(Status->text());
	}

	void diff_workbench::emit_selection(const bool aInteractive) noexcept
	{
		if (SelectionSink) {
			try {
				SelectionSink(Interaction.Selection);
			} catch (...) {
			}
		}
		emit_selection_observer(aInteractive);
	}

	void diff_workbench::emit_selection_observer(
		const bool aInteractive) noexcept
	{
		if (!SelectionObserver) {
			return;
		}
		try {
			SelectionObserver(Interaction.Selection, aInteractive);
		} catch (...) {
		}
	}

	void diff_workbench::clear_projection(QString aStatus)
	{
		const auto hadSelection = Interaction.Selection.has_value();
		CurrentDetail.reset();
		SemanticIndex.reset();
		CurrentResult.reset();
		// A classification belongs to one blob. Keeping it across a cleared
		// projection would colour the next comparison's text with the previous
		// file's tokens, which reads as a highlighter bug and is a stale-state
		// bug. The composition re-adopts once the new blobs are classified.
		LeftSpans.reset();
		RightSpans.reset();
		MovedSegments.clear();
		Interaction.Selection.reset();
		ConversationAnchor.reset();
		Left->clear();
		Right->clear();
		Detail->setPlainText(QStringLiteral("No change selected"));
		Controller->clear_axis();
		LinkMap->clear_geometry();
		set_line_selection(*Left, std::nullopt);
		set_line_selection(*Right, std::nullopt);
		pane(Left)->set_conversation_anchor(std::nullopt);
		pane(Right)->set_conversation_anchor(std::nullopt);
		AxisCache.invalidate(measured_geometry_change::file_or_epoch);
		PendingGeometryTriggers = diff_geometry_rebuild_trigger::none;
		update_status(std::move(aStatus));
		if (hadSelection) {
			emit_selection();
		}
	}
} // namespace bpr::qt
