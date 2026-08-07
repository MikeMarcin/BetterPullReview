#include "bpr/qt/diff_workbench.hpp"

#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QApplication>
#include <QEvent>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
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

namespace bpr::qt {
	namespace {
		class source_pane final : public QPlainTextEdit
		{
		public:
			using QPlainTextEdit::QPlainTextEdit;

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
				const auto appliedFraction = static_cast<qreal>(
												 bar->value() - bar->minimum())
					/ static_cast<qreal>(bar->maximum() - bar->minimum());
				return appliedFraction * aMaximumOrigin;
			}
		};

		[[nodiscard]] auto pane(QPlainTextEdit* const aPane) noexcept
			-> source_pane*
		{
			return static_cast<source_pane*>(aPane);
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
		void append_selection_band(
			QList<QTextEdit::ExtraSelection>& aSelections,
			QPlainTextEdit&                   aPane,
			const core::source_line_ref&      aLine)
		{
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(aLine.Line));
			if (!block.isValid()) {
				return;
			}
			auto band   = QTextEdit::ExtraSelection{ };
			band.cursor = QTextCursor{ block };
			band.format.setProperty(QTextFormat::FullWidthSelection, true);
			auto color = aPane.palette().color(QPalette::Highlight);
			color.setAlpha(72);
			band.format.setBackground(color);
			aSelections.push_back(band);
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
					color.setAlpha(72);
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
		auto* const toolbar = new QHBoxLayout;
		PreviousChange      = new QToolButton{ this };
		PreviousChange->setText(QStringLiteral("Previous change"));
		PreviousChange->setAccessibleName(QStringLiteral("Previous change"));
		PreviousChange->setToolTip(QStringLiteral("Select the previous semantic change"));
		PreviousChange->setFocusPolicy(Qt::StrongFocus);
		NextChange = new QToolButton{ this };
		NextChange->setText(QStringLiteral("Next change"));
		NextChange->setAccessibleName(QStringLiteral("Next change"));
		NextChange->setToolTip(QStringLiteral("Select the next semantic change"));
		NextChange->setFocusPolicy(Qt::StrongFocus);
		Status = new QLabel{ QStringLiteral("Select a file comparison"), this };
		Status->setObjectName(QStringLiteral("diffWorkbenchStatus"));
		Status->setAccessibleName(QStringLiteral("Comparison status"));
		Status->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
		toolbar->addWidget(PreviousChange);
		toolbar->addWidget(NextChange);
		toolbar->addWidget(Status, 1);
		root->addLayout(toolbar);

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

		connect(PreviousChange, &QToolButton::clicked, this, [this] {
			navigate(core::change_navigation_direction::previous);
		});
		connect(NextChange, &QToolButton::clicked, this, [this] {
			navigate(core::change_navigation_direction::next);
		});
		QWidget::setTabOrder(PreviousChange, NextChange);
		QWidget::setTabOrder(NextChange, Left);
		QWidget::setTabOrder(Left, LinkMap);
		QWidget::setTabOrder(LinkMap, Right);
		QWidget::setTabOrder(Right, SharedScroll);
		QWidget::setTabOrder(SharedScroll, Detail);
		update_action_state();
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
		CurrentResult = aPublication.Result;
		SemanticIndex = std::move(semanticIndex);
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
		update_action_state();
		schedule_geometry_rebuild(measured_geometry_change::accepted_diff);
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
		CurrentContext.reset();
		clear_projection(QStringLiteral("Comparison invalidated"));
	}

	void diff_workbench::set_loading(const core::diff_request_context& aContext)
	{
		if (!core::valid(aContext)) {
			return;
		}
		clear_projection(QStringLiteral("Loading selected file comparison…"));
		CurrentContext = aContext;
	}

	void diff_workbench::clear()
	{
		CurrentContext.reset();
		clear_projection(QStringLiteral("Select a file comparison"));
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

	auto diff_workbench::previous_change_button() const noexcept -> QToolButton*
	{
		return PreviousChange;
	}

	auto diff_workbench::next_change_button() const noexcept -> QToolButton*
	{
		return NextChange;
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
		return Statistics;
	}

	auto diff_workbench::event(QEvent* const aEvent) -> bool
	{
		const auto result = QWidget::event(aEvent);
		switch (aEvent->type()) {
		case QEvent::FontChange:
		case QEvent::ApplicationFontChange:
			schedule_geometry_rebuild(measured_geometry_change::font);
			break;
		case QEvent::PaletteChange:
		case QEvent::ApplicationPaletteChange:
		case QEvent::StyleChange:
			update_selection_paint();
			schedule_geometry_rebuild(measured_geometry_change::viewport_or_divider);
			break;
		case QEvent::Show:
		case QEvent::DevicePixelRatioChange:
		case QEvent::ScreenChangeInternal:
			schedule_geometry_rebuild(measured_geometry_change::screen);
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

		schedule_geometry_rebuild(measured_geometry_change::tab_width);
	}

	auto diff_workbench::eventFilter(QObject* const aWatched, QEvent* const aEvent)
		-> bool
	{
		const auto sourceViewport = aWatched == Left->viewport()
			|| aWatched == Right->viewport();
		if (sourceViewport && aEvent->type() == QEvent::Resize) {
			schedule_geometry_rebuild(
				measured_geometry_change::viewport_or_divider);
		}
		if (
			CurrentResult && aEvent->type() == QEvent::MouseButtonPress
			&& sourceViewport) {
			const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
			if (mouse->button() == Qt::LeftButton) {
				auto* const source = aWatched == Left->viewport() ? Left : Right;
				const auto  cursor = source->cursorForPosition(mouse->position().toPoint());
				if (cursor.blockNumber() >= 0) {
					select_line(
						aWatched == Left->viewport()
							? core::diff_side::left
							: core::diff_side::right,
						static_cast<std::uint32_t>(cursor.blockNumber()));
				}
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void diff_workbench::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		schedule_geometry_rebuild(
			measured_geometry_change::viewport_or_divider);
	}

	void diff_workbench::schedule_geometry_rebuild(
		const measured_geometry_change aChange)
	{
		if (aChange != measured_geometry_change::none) {
			AxisCache.invalidate(aChange);
		}
		if (GeometryScheduled || !CurrentResult) {
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
			return;
		}
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
		const auto revealSelection = !Controller->current_frame().has_value()
			|| Controller->current_frame()->Comparison != axis.comparison();
		LinkMap->set_geometry(axis, CurrentResult->Alignment.Moves);
		Controller->set_axis(axis);
		++Statistics.GeometryRebuilds;
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
			auto rendered                 = aFrame;
			rendered.Left.DocumentOriginY = pane(Left)->set_document_origin(
				aFrame.Left.DocumentOriginY,
				std::max(
					0.0,
					axis->left_document_extent()
						- axis->left_viewport_extent()));
			rendered.Right.DocumentOriginY = pane(Right)->set_document_origin(
				aFrame.Right.DocumentOriginY,
				std::max(
					0.0,
					axis->right_document_extent()
						- axis->right_viewport_extent()));
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

	void diff_workbench::select_line(
		const core::diff_side aSide,
		const std::uint32_t   aLine)
	{
		if (!CurrentResult || !SemanticIndex.has_value()) {
			return;
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::select_diff_line_intent{
				.Line = {
					.Comparison = CurrentResult->Alignment.Digest,
					.Side       = aSide,
					.Line       = aLine,
				},
			});
		if (changed) {
			++Statistics.SelectionChanges;
			apply_interaction(false, true);
		}
	}

	void diff_workbench::select_link(
		const diff_link_map_identity& aIdentity,
		const bool                    aReveal)
	{
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
	}

	void diff_workbench::navigate(
		const core::change_navigation_direction aDirection)
	{
		if (!CurrentResult || !SemanticIndex.has_value()) {
			return;
		}
		++Statistics.SemanticQueries;
		const auto changed = core::transition(
			Interaction,
			*SemanticIndex,
			core::navigate_diff_change_intent{
				.Direction = aDirection,
				.Policy    = core::change_navigation_policy::stop,
			});
		if (changed) {
			++Statistics.SelectionChanges;
			apply_interaction(true, changed);
		}
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
				static_cast<void>(Controller->reveal_segment(*selection.Segment));
			}
		} else {
			LinkMap->set_selected(std::nullopt, false);
		}
	}

	void diff_workbench::apply_interaction(
		const bool aReveal,
		const bool aAnnounceSelection)
	{
		synchronize_geometry_selection(aReveal);
		update_detail();
		update_selection_paint();
		update_action_state();
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
		using role = core::appearance_role;
		Colors     = diff_presentation_colors{
			.Added            = aProfile.color(role::diff_added_background),
			.Removed          = aProfile.color(role::diff_removed_background),
			.IntralineAdded   = aProfile.color(role::diff_intraline_added_background),
			.IntralineRemoved = aProfile.color(role::diff_intraline_removed_background),
			.Moved            = aProfile.color(role::diff_moved_background),
			.Supplied         = true,
		};
		update_diff_paint();
	}

	/// \brief Paints the diff the core decided, in one pass per pane.
	///
	/// \details Order is the whole design. Qt layers extra selections in the
	/// order they are given, so the line band goes down first, the intraline
	/// emphasis over it, and the selection last. A reviewer therefore sees
	/// "this line changed", "this is the part that changed", and "this is where
	/// I am" as three readable levels rather than one winning and two
	/// vanishing.
	///
	/// Nothing here decides what changed. Every span comes from
	/// `core::diff_semantics`, and a second opinion about which lines differ is
	/// the defect RFC-BPR-0005's reuse rules forbid.
	void diff_workbench::update_diff_paint()
	{
		const auto paint = [this](
							   QPlainTextEdit&       aPane,
							   const core::diff_side aSide) {
			auto       selections = QList<QTextEdit::ExtraSelection>{ };
			const auto paintable  = paintable_line_bounds(aPane);

			if (CurrentResult && Colors.Supplied) {
				const auto& alignment = CurrentResult->Alignment;
				for (const auto& segment : alignment.Segments) {
					const auto span  = aSide == core::diff_side::left
						? segment.Left
						: segment.Right;
					const auto color = [&]() -> QColor {
						switch (segment.Kind) {
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
					append_line_band(
						selections, aPane, intersection(span, paintable), color);
				}

				append_intraline_emphasis(selections, aPane, aSide, paintable);
			}

			if (CurrentResult && Interaction.Selection.has_value()
				&& CurrentDetail.has_value()) {
				const auto& line = aSide == core::diff_side::left
					? CurrentDetail->LeftLine
					: CurrentDetail->RightLine;
				if (line.has_value()) {
					append_selection_band(selections, aPane, *line);
				}
			}

			aPane.setExtraSelections(selections);
		};

		paint(*Left, core::diff_side::left);
		paint(*Right, core::diff_side::right);
	}

	/// \brief Marks the exact byte spans the core reported as changed.
	void diff_workbench::append_intraline_emphasis(
		QList<QTextEdit::ExtraSelection>& aSelections,
		QPlainTextEdit&                   aPane,
		const core::diff_side             aSide,
		const core::line_span             aPaintable)
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
			auto emphasis   = QTextEdit::ExtraSelection{ };
			emphasis.cursor = QTextCursor{ block };
			emphasis.cursor.setPosition(block.position() + from);
			emphasis.cursor.setPosition(
				block.position() + to, QTextCursor::KeepAnchor);
			emphasis.format.setBackground(color);
			aSelections.push_back(emphasis);
		}
	}

	void diff_workbench::update_action_state()
	{
		if (!CurrentResult || !SemanticIndex.has_value()
			|| CurrentResult->Alignment.Groups.empty()) {
			PreviousChange->setEnabled(false);
			NextChange->setEnabled(false);
			return;
		}
		auto index = std::optional<std::size_t>{ };
		if (
			Interaction.Selection.has_value()
			&& Interaction.Selection->Group.has_value()) {
			index = SemanticIndex->group_index(*Interaction.Selection->Group);
		}
		PreviousChange->setEnabled(index.has_value() && *index != 0U);
		NextChange->setEnabled(
			!index.has_value()
			|| *index + 1U < CurrentResult->Alignment.Groups.size());
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

	void diff_workbench::clear_projection(QString aStatus)
	{
		CurrentDetail.reset();
		SemanticIndex.reset();
		CurrentResult.reset();
		Interaction.Selection.reset();
		Left->clear();
		Right->clear();
		Detail->setPlainText(QStringLiteral("No change selected"));
		Controller->clear_axis();
		LinkMap->clear_geometry();
		set_line_selection(*Left, std::nullopt);
		set_line_selection(*Right, std::nullopt);
		AxisCache.invalidate(measured_geometry_change::file_or_epoch);
		update_status(std::move(aStatus));
		update_action_state();
	}
} // namespace bpr::qt
