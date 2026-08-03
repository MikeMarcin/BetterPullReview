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
#include <string>
#include <utility>

namespace bpr::qt {
	namespace {
		class source_pane final : public QPlainTextEdit
		{
		public:
			using QPlainTextEdit::QPlainTextEdit;

			void set_document_origin(const qreal aOrigin, const qreal aMaximumOrigin)
			{
				auto* const bar = verticalScrollBar();
				if (bar == nullptr || aMaximumOrigin <= 0.0 || bar->maximum() <= bar->minimum()) {
					if (bar != nullptr) {
						bar->setValue(bar->minimum());
					}
					return;
				}
				const auto fraction = std::clamp(aOrigin / aMaximumOrigin, 0.0, 1.0);
				const auto value    = std::lerp(
					static_cast<qreal>(bar->minimum()),
					static_cast<qreal>(bar->maximum()),
					fraction);
				bar->setValue(static_cast<int>(std::llround(value)));
			}

			[[nodiscard]] auto actual_document_origin() const noexcept -> qreal
			{
				const auto block = firstVisibleBlock();
				if (!block.isValid()) {
					return 0.0;
				}
				const auto documentTop = blockBoundingGeometry(block).top();
				const auto viewportTop = blockBoundingGeometry(block)
											 .translated(contentOffset())
											 .top();
				return std::max(0.0, documentTop - viewportTop);
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

		[[nodiscard]] auto group_index(
			const core::computed_file_diff& aDiff,
			const core::change_group_id     aGroup) -> std::optional<std::size_t>
		{
			const auto found = std::ranges::find(
				aDiff.Alignment.Groups, aGroup, &core::change_group::Id);
			return found == aDiff.Alignment.Groups.end()
				? std::nullopt
				: std::optional<std::size_t>{
					  static_cast<std::size_t>(
						  std::distance(aDiff.Alignment.Groups.begin(), found))
				  };
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
			|| !core::validate_computed_file_diff(*aPublication.Result).valid()
			|| aPublication.Result->Alignment.Key != aPublication.Context.Key
			|| !CurrentContext.has_value()
			|| *CurrentContext != aPublication.Context) {
			++Statistics.RejectedStaleInput;
			return;
		}
		CurrentContext = aPublication.Context;
		CurrentResult  = aPublication.Result;
		Left->setPlainText(text_document(CurrentResult->LeftDocument));
		Right->setPlainText(text_document(CurrentResult->RightDocument));
		static_cast<void>(core::reconcile_diff_interaction(Interaction, *CurrentResult));
		if (!Interaction.Selection.has_value()) {
			if (!CurrentResult->Alignment.Groups.empty()) {
				static_cast<void>(core::transition(
					Interaction,
					*CurrentResult,
					core::select_diff_group_intent{
						.Group = CurrentResult->Alignment.Groups.front().Id,
					}));
			} else if (CurrentResult->Alignment.LeftLineCount != 0U) {
				static_cast<void>(core::transition(
					Interaction,
					*CurrentResult,
					core::select_diff_line_intent{
						.Line = {
							.Comparison = CurrentResult->Alignment.Digest,
							.Side       = core::diff_side::left,
							.Line       = 0U,
						},
					}));
			} else if (CurrentResult->Alignment.RightLineCount != 0U) {
				static_cast<void>(core::transition(
					Interaction,
					*CurrentResult,
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
			break;
		}
		return result;
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
		const auto axis = *AxisCache.axis();
		LinkMap->set_geometry(axis, CurrentResult->Alignment.Moves);
		Controller->set_axis(axis);
		++Statistics.GeometryRebuilds;
		apply_interaction(true, false);
	}

	void diff_workbench::apply_frame(const comparison_scroll_frame& aFrame)
	{
		if (
			ApplyingFrame || !CurrentResult
			|| aFrame.Comparison != CurrentResult->Alignment.Digest) {
			return;
		}
		ApplyingFrame           = true;
		const auto leftGeometry = measure_plain_text_pane(
			*Left, CurrentResult->Alignment.LeftLineCount);
		const auto rightGeometry = measure_plain_text_pane(
			*Right, CurrentResult->Alignment.RightLineCount);
		if (leftGeometry.has_value() && rightGeometry.has_value()) {
			pane(Left)->set_document_origin(
				aFrame.Left.DocumentOriginY,
				std::max(0.0, leftGeometry->DocumentExtent - leftGeometry->ViewportExtent));
			pane(Right)->set_document_origin(
				aFrame.Right.DocumentOriginY,
				std::max(0.0, rightGeometry->DocumentExtent - rightGeometry->ViewportExtent));
			auto rendered                  = aFrame;
			rendered.Left.DocumentOriginY  = pane(Left)->actual_document_origin();
			rendered.Right.DocumentOriginY = pane(Right)->actual_document_origin();
			rendered.Left.ViewportY        = rendered.Left.ProjectedDocumentY
				- rendered.Left.DocumentOriginY;
			rendered.Right.ViewportY = rendered.Right.ProjectedDocumentY
				- rendered.Right.DocumentOriginY;
			static_cast<void>(LinkMap->set_frame(rendered));
		} else {
			static_cast<void>(LinkMap->set_frame(aFrame));
		}
		ApplyingFrame = false;
		++Statistics.FrameApplications;
	}

	void diff_workbench::select_line(
		const core::diff_side aSide,
		const std::uint32_t   aLine)
	{
		if (!CurrentResult) {
			return;
		}
		const auto changed = core::transition(
			Interaction,
			*CurrentResult,
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
		if (!CurrentResult || !aIdentity.Group.has_value()) {
			return;
		}
		const auto changed = core::transition(
			Interaction,
			*CurrentResult,
			core::select_diff_group_intent{ .Group = *aIdentity.Group });
		if (changed) {
			++Statistics.SelectionChanges;
		}
		apply_interaction(aReveal, changed);
	}

	void diff_workbench::navigate(
		const core::change_navigation_direction aDirection)
	{
		if (!CurrentResult) {
			return;
		}
		const auto changed = core::transition(
			Interaction,
			*CurrentResult,
			core::navigate_diff_change_intent{
				.Direction = aDirection,
				.Policy    = core::change_navigation_policy::stop,
			});
		if (changed) {
			++Statistics.SelectionChanges;
			apply_interaction(true, changed);
		}
	}

	void diff_workbench::apply_interaction(
		const bool aReveal,
		const bool aAnnounceSelection)
	{
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			LinkMap->set_selected(std::nullopt, false);
			update_detail();
			update_selection_paint();
			update_action_state();
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
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			Detail->setPlainText(QStringLiteral("No change selected"));
			return;
		}
		const auto detail = core::detail_for_selection(
			*CurrentResult, *Interaction.Selection);
		const auto leftText = detail_side_text(
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
			const auto index = group_index(
				*CurrentResult, *Interaction.Selection->Group);
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
		if (!CurrentResult || !Interaction.Selection.has_value()) {
			set_line_selection(*Left, std::nullopt);
			set_line_selection(*Right, std::nullopt);
			return;
		}
		const auto detail = core::detail_for_selection(
			*CurrentResult, *Interaction.Selection);
		set_line_selection(*Left, detail.LeftLine);
		set_line_selection(*Right, detail.RightLine);
	}

	void diff_workbench::update_action_state()
	{
		if (!CurrentResult || CurrentResult->Alignment.Groups.empty()) {
			PreviousChange->setEnabled(false);
			NextChange->setEnabled(false);
			return;
		}
		auto index = std::optional<std::size_t>{ };
		if (
			Interaction.Selection.has_value()
			&& Interaction.Selection->Group.has_value()) {
			index = group_index(*CurrentResult, *Interaction.Selection->Group);
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
		CurrentResult.reset();
		Interaction.Selection.reset();
		Left->clear();
		Right->clear();
		Detail->setPlainText(QStringLiteral("No change selected"));
		Controller->clear_axis();
		LinkMap->clear_geometry();
		LinkMap->set_selected(std::nullopt, false);
		set_line_selection(*Left, std::nullopt);
		set_line_selection(*Right, std::nullopt);
		AxisCache.invalidate(measured_geometry_change::file_or_epoch);
		update_status(std::move(aStatus));
		update_action_state();
	}
} // namespace bpr::qt
