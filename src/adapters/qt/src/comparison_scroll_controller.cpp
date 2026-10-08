#include "bpr/qt/comparison_scroll_controller.hpp"

#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyleHints>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto valid_configuration(
			const comparison_scroll_configuration& aConfiguration) noexcept -> bool
		{
			return std::isfinite(aConfiguration.SynchronizationRailFraction)
				&& aConfiguration.SynchronizationRailFraction >= 0.0
				&& aConfiguration.SynchronizationRailFraction <= 1.0
				&& std::isfinite(aConfiguration.AngleStepExtent)
				&& aConfiguration.AngleStepExtent > 0.0
				&& std::isfinite(aConfiguration.PageFraction)
				&& aConfiguration.PageFraction > 0.0
				&& aConfiguration.PageFraction <= 1.0
				&& aConfiguration.ScrollbarPrecision > 0
				&& aConfiguration.ScrollbarPrecision <= 100'000;
		}

		[[nodiscard]] auto same_position(
			const qreal aLeft, const qreal aRight) noexcept -> bool
		{
			return std::abs(aLeft - aRight) <= 0.000'000'001;
		}
	} // namespace

	comparison_scroll_controller::comparison_scroll_controller(
		QScrollBar&                     aSharedScrollBar,
		comparison_scroll_configuration aConfiguration,
		QObject*                        aParent)
		: QObject{ aParent }
		, SharedScrollBar{ &aSharedScrollBar }
		, Configuration{ valid_configuration(aConfiguration)
							 ? aConfiguration
							 : comparison_scroll_configuration{ } }
	{
		SharedScrollBar->setOrientation(Qt::Vertical);
		SharedScrollBar->setFocusPolicy(Qt::StrongFocus);
		SharedScrollBar->setEnabled(false);
		SharedScrollBar->setRange(0, Configuration.ScrollbarPrecision);
		connect(
			SharedScrollBar,
			&QScrollBar::actionTriggered,
			this,
			[this](const int aAction) { handle_scrollbar_action(aAction); });
		connect(
			SharedScrollBar,
			&QScrollBar::valueChanged,
			this,
			[this](const int aValue) {
				if (AxisSuspended || ApplyingFrame || !Axis.has_value()) {
					return;
				}
				if (PendingScrollbarTarget.has_value()
					&& PendingScrollbarTarget->Value == aValue) {
					const auto target = PendingScrollbarTarget->Position;
					PendingScrollbarTarget.reset();
					apply_position(target, false);
					return;
				}
				PendingScrollbarTarget.reset();
				++Statistics.ScrollbarInputs;
				apply_position(position_for_scrollbar_value(aValue), false);
			});
	}

	comparison_scroll_controller::~comparison_scroll_controller()
	{
		for (auto& binding : InputBindings) {
			if (binding.Surface != nullptr) {
				binding.Surface->removeEventFilter(this);
			}
			if (binding.KeySurface != nullptr
				&& binding.KeySurface != binding.Surface) {
				binding.KeySurface->removeEventFilter(this);
			}
		}
	}

	void comparison_scroll_controller::bind_input_surface(
		const comparison_scroll_receiver aReceiver,
		QWidget&                         aSurface,
		QScrollBar*                      aHorizontalScrollBar)
	{
		auto& binding = InputBindings[receiver_index(aReceiver)];
		if (binding.Surface != nullptr
			&& binding.Surface != binding.KeySurface) {
			binding.Surface->removeEventFilter(this);
		}
		binding.Surface             = &aSurface;
		binding.HorizontalScrollBar = aHorizontalScrollBar;
		binding.HorizontalResidual  = 0.0;
		aSurface.installEventFilter(this);
	}

	void comparison_scroll_controller::bind_key_surface(
		const comparison_scroll_receiver aReceiver,
		QWidget&                         aSurface)
	{
		auto& binding = InputBindings[receiver_index(aReceiver)];
		if (binding.KeySurface != nullptr
			&& binding.KeySurface != binding.Surface) {
			binding.KeySurface->removeEventFilter(this);
		}
		binding.KeySurface = &aSurface;
		aSurface.installEventFilter(this);
	}

	void comparison_scroll_controller::set_frame_sink(
		comparison_scroll_frame_sink aSink)
	{
		FrameSink = std::move(aSink);
		if (CurrentFrame.has_value() && FrameSink) {
			apply_position(Q, true);
		}
	}

	void comparison_scroll_controller::set_repaint_sink(
		comparison_scroll_repaint_sink aSink)
	{
		RepaintSink = std::move(aSink);
	}

	void comparison_scroll_controller::set_axis(measured_alignment_axis aAxis)
	{
		if (ApplyingFrame) {
			PendingAxis      = std::move(aAxis);
			ClearAxisPending = false;
			return;
		}

		const auto sameComparison = Axis.has_value()
			&& Axis->comparison() == aAxis.comparison();
		const auto anchor           = sameComparison ? retained_anchor() : std::nullopt;
		const auto previousPosition = Q;
		// [[BPR-1291]]. A remeasurement keeps the line on the rail where it
		// is, except at an end: a comparison shown from its top, or with its
		// last lines at the top, stays so when the rail moves with the
		// viewport.
		const auto atStart = sameComparison && same_position(Q, MinimumQ);
		const auto atEnd   = sameComparison && !atStart && same_position(Q, MaximumQ);
		PendingAxis.reset();
		ClearAxisPending = false;
		AxisSuspended    = false;
		Axis             = std::move(aAxis);
		SegmentIndexes.clear();
		const auto segments = Axis->segments();
		for (std::size_t index = 0U; index < segments.size(); ++index) {
			SegmentIndexes.emplace(segments[index].Id, index);
		}
		update_live_range();
		if (SharedScrollBar != nullptr) {
			SharedScrollBar->setEnabled(MaximumQ > MinimumQ);
		}
		const auto anchoredPosition = anchor.has_value()
			? position_for_anchor(*anchor)
			: std::nullopt;
		const auto retained         = atEnd
			? MaximumQ
			: anchoredPosition.value_or(clamp_position(previousPosition));
		apply_position(sameComparison && !atStart ? retained : MinimumQ, true);
	}

	void comparison_scroll_controller::suspend_axis()
	{
		AxisSuspended = true;
		PendingAxis.reset();
		PendingScrollbarTarget.reset();
		if (SharedScrollBar != nullptr) {
			SharedScrollBar->setEnabled(false);
		}
	}

	void comparison_scroll_controller::clear_axis()
	{
		if (ApplyingFrame) {
			PendingAxis.reset();
			ClearAxisPending = true;
			return;
		}

		PendingAxis.reset();
		PendingScrollbarTarget.reset();
		ClearAxisPending = false;
		AxisSuspended    = false;
		Axis.reset();
		SegmentIndexes.clear();
		CurrentFrame.reset();
		Q        = 0.0;
		MinimumQ = 0.0;
		MaximumQ = 0.0;
		if (SharedScrollBar != nullptr) {
			const auto blocker = QSignalBlocker{ SharedScrollBar };
			SharedScrollBar->setValue(0);
			SharedScrollBar->setEnabled(false);
		}
	}

	void comparison_scroll_controller::set_position(const qreal aPosition)
	{
		apply_position(aPosition, false);
	}

	void comparison_scroll_controller::scroll_by(const qreal aDelta)
	{
		if (!std::isfinite(aDelta)) {
			return;
		}
		apply_position(Q + aDelta, false);
	}

	void comparison_scroll_controller::page_up()
	{
		scroll_by(-page_extent());
	}

	void comparison_scroll_controller::page_down()
	{
		scroll_by(page_extent());
	}

	void comparison_scroll_controller::home()
	{
		set_position(minimum_position());
	}

	void comparison_scroll_controller::end()
	{
		set_position(maximum_position());
	}

	auto comparison_scroll_controller::reveal_segment(
		const core::alignment_segment_id aSegment,
		const qreal                      aFraction) -> bool
	{
		if (AxisSuspended || !Axis.has_value() || !std::isfinite(aFraction)) {
			return false;
		}
		const auto found = SegmentIndexes.find(aSegment);
		if (found == SegmentIndexes.end()) {
			return false;
		}
		const auto& segment  = Axis->segments()[found->second];
		const auto  fraction = std::clamp(aFraction, 0.0, 1.0);
		set_position(std::lerp(
			segment.ComparisonBegin, segment.ComparisonEnd, fraction));
		return true;
	}

	auto comparison_scroll_controller::reveal_pane_origin(
		const core::diff_side aSide,
		const qreal           aOrigin) -> bool
	{
		if (AxisSuspended || !Axis.has_value() || !std::isfinite(aOrigin)) {
			return false;
		}
		set_position(position_reaching(aSide, aOrigin + rail_extent(aSide)));
		return true;
	}

	auto comparison_scroll_controller::scrollbar_fraction(
		const core::diff_side aSide,
		const std::uint32_t   aLine) const noexcept -> std::optional<qreal>
	{
		// [[BPR-1290]]. Where the bar stands when the line is at the rail, as
		// a reveal would put it.
		if (!Axis.has_value() || MaximumQ <= MinimumQ) {
			return std::nullopt;
		}
		const auto origin = aSide == core::diff_side::left ? Axis->left_line_origin(aLine) : Axis->right_line_origin(aLine);
		if (!origin.has_value()) {
			return std::nullopt;
		}
		const auto position = clamp_position(position_reaching(aSide, *origin + rail_extent(aSide)));
		return (position - MinimumQ) / (MaximumQ - MinimumQ);
	}

	auto comparison_scroll_controller::position() const noexcept -> qreal
	{
		return Q;
	}

	auto comparison_scroll_controller::minimum_position() const noexcept -> qreal
	{
		return MinimumQ;
	}

	auto comparison_scroll_controller::maximum_position() const noexcept -> qreal
	{
		return MaximumQ;
	}

	auto comparison_scroll_controller::current_frame() const noexcept
		-> const std::optional<comparison_scroll_frame>&
	{
		return CurrentFrame;
	}

	auto comparison_scroll_controller::statistics() const noexcept
		-> comparison_scroll_statistics
	{
		return Statistics;
	}

	auto comparison_scroll_controller::eventFilter(
		QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent == nullptr) {
			return QObject::eventFilter(aWatched, aEvent);
		}
		if (aEvent->type() == QEvent::Wheel) {
			const auto receiver = wheel_receiver_for(aWatched);
			if (!receiver.has_value()) {
				return QObject::eventFilter(aWatched, aEvent);
			}
			return handle_wheel_event(
				*receiver, *static_cast<QWheelEvent*>(aEvent));
		}
		if (aEvent->type() == QEvent::KeyPress) {
			const auto receiver = key_receiver_for(aWatched);
			if (!receiver.has_value()) {
				return QObject::eventFilter(aWatched, aEvent);
			}
			return handle_key_event(*static_cast<QKeyEvent*>(aEvent));
		}
		return QObject::eventFilter(aWatched, aEvent);
	}

	auto comparison_scroll_controller::receiver_index(
		const comparison_scroll_receiver aReceiver) noexcept -> std::size_t
	{
		return static_cast<std::size_t>(aReceiver);
	}

	auto comparison_scroll_controller::wheel_receiver_for(
		QObject* const aSurface) const noexcept
		-> std::optional<comparison_scroll_receiver>
	{
		for (std::size_t index = 0U; index < InputBindings.size(); ++index) {
			if (InputBindings[index].Surface == aSurface) {
				return static_cast<comparison_scroll_receiver>(index);
			}
		}
		return std::nullopt;
	}

	auto comparison_scroll_controller::key_receiver_for(
		QObject* const aSurface) const noexcept
		-> std::optional<comparison_scroll_receiver>
	{
		for (std::size_t index = 0U; index < InputBindings.size(); ++index) {
			const auto&       binding    = InputBindings[index];
			const auto* const keySurface = binding.KeySurface != nullptr
				? binding.KeySurface.data()
				: binding.Surface.data();
			if (keySurface == aSurface) {
				return static_cast<comparison_scroll_receiver>(index);
			}
		}
		return std::nullopt;
	}

	auto comparison_scroll_controller::retained_anchor() const noexcept
		-> std::optional<retained_axis_anchor>
	{
		if (!Axis.has_value() || !CurrentFrame.has_value()
			|| !CurrentFrame->Segment.has_value()) {
			return std::nullopt;
		}
		const auto found = SegmentIndexes.find(*CurrentFrame->Segment);
		if (found == SegmentIndexes.end()) {
			return std::nullopt;
		}
		const auto& segment = Axis->segments()[found->second];
		const auto  extent  = segment.ComparisonEnd - segment.ComparisonBegin;
		return retained_axis_anchor{
			.Segment  = segment.Id,
			.Fraction = extent > 0.0
				? std::clamp((Q - segment.ComparisonBegin) / extent, 0.0, 1.0)
				: 0.0,
			.PastEnd  = std::max(0.0, Q - Axis->comparison_extent()),
		};
	}

	auto comparison_scroll_controller::position_for_anchor(
		const retained_axis_anchor& aAnchor) const noexcept
		-> std::optional<qreal>
	{
		if (!Axis.has_value()) {
			return std::nullopt;
		}
		const auto found = SegmentIndexes.find(aAnchor.Segment);
		if (found == SegmentIndexes.end()) {
			return std::nullopt;
		}
		const auto& segment = Axis->segments()[found->second];
		return std::lerp(
				   segment.ComparisonBegin,
				   segment.ComparisonEnd,
				   std::clamp(aAnchor.Fraction, 0.0, 1.0))
			+ aAnchor.PastEnd;
	}

	auto comparison_scroll_controller::make_frame(
		const qreal aPosition) const noexcept
		-> std::optional<comparison_scroll_frame>
	{
		if (!Axis.has_value()) {
			return std::nullopt;
		}
		const auto projected = Axis->project(aPosition);
		if (!projected.has_value()) {
			return std::nullopt;
		}
		// Past the comparison's end there is nothing left to align, so both
		// sides advance together and their ends stay level.
		const auto pastEnd = std::max(0.0, aPosition - Axis->comparison_extent());
		return comparison_scroll_frame{
			.Comparison = Axis->comparison(),
			.Position   = projected->ComparisonPosition + pastEnd,
			.Segment    = projected->Segment,
			.Left       = make_pane_frame(
				projected->LeftDocumentY + pastEnd,
				Axis->last_line_origin(core::diff_side::left),
				Axis->left_viewport_extent()),
			.Right = make_pane_frame(
				projected->RightDocumentY + pastEnd,
				Axis->last_line_origin(core::diff_side::right),
				Axis->right_viewport_extent()),
		};
	}

	auto comparison_scroll_controller::make_pane_frame(
		const qreal aProjectedY,
		const qreal aMaximumOrigin,
		const qreal aViewportExtent) const noexcept
		-> comparison_pane_scroll_frame
	{
		const auto rail = Configuration.SynchronizationRailFraction
			* aViewportExtent;
		const auto desiredOrigin = aProjectedY - rail;
		const auto maximumOrigin = std::max(0.0, aMaximumOrigin);
		const auto origin        = std::clamp(
			desiredOrigin, 0.0, maximumOrigin);
		return {
			.ProjectedDocumentY = aProjectedY,
			.DocumentOriginY    = origin,
			.ViewportY          = aProjectedY - origin,
			.LeadingClamped     = (desiredOrigin < 0.0),
			.TrailingClamped    = (desiredOrigin > maximumOrigin),
		};
	}

	auto comparison_scroll_controller::rail_extent(
		const core::diff_side aSide) const noexcept -> qreal
	{
		if (!Axis.has_value()) {
			return 0.0;
		}
		return Configuration.SynchronizationRailFraction
			* (aSide == core::diff_side::left ? Axis->left_viewport_extent()
											  : Axis->right_viewport_extent());
	}

	/// The least position at which a side's projection reaches a document y.
	/// Past the comparison's end the projection carries on one to one, as
	/// `make_frame` extends it.
	auto comparison_scroll_controller::position_reaching(
		const core::diff_side aSide,
		const qreal           aDocumentY) const noexcept -> qreal
	{
		if (!Axis.has_value() || !std::isfinite(aDocumentY)) {
			return 0.0;
		}
		const auto left   = aSide == core::diff_side::left;
		const auto extent = Axis->comparison_extent();
		const auto ending = Axis->project(extent);
		const auto endY   = ending.has_value() ? (left ? ending->LeftDocumentY : ending->RightDocumentY) : 0.0;
		if (aDocumentY >= endY) {
			return extent + (aDocumentY - endY);
		}
		const auto segments = Axis->segments();
		const auto found    = std::ranges::lower_bound(
			segments,
			aDocumentY,
			std::ranges::less{ },
			[left](const measured_alignment_segment& aSegment) {
				return left ? aSegment.LeftEnd : aSegment.RightEnd;
			});
		if (found == segments.end()) {
			return extent;
		}
		const auto begin = left ? found->LeftBegin : found->RightBegin;
		const auto end   = left ? found->LeftEnd : found->RightEnd;
		if (aDocumentY <= begin || end <= begin) {
			return found->ComparisonBegin;
		}
		return std::lerp(
			found->ComparisonBegin,
			found->ComparisonEnd,
			(aDocumentY - begin) / (end - begin));
	}

	auto comparison_scroll_controller::clamp_position(
		const qreal aPosition) const noexcept -> qreal
	{
		return std::isfinite(aPosition)
			? std::clamp(aPosition, MinimumQ, MaximumQ)
			: MinimumQ;
	}

	/// The range runs from the first position at which a pane leaves its
	/// top to the last at which a pane is still short of its last line.
	/// Outside it the rail would move with every pane clamped, which is the
	/// bar moving while nothing scrolls. A pane with one line never moves, so
	/// it sets neither end; with no pane that moves the range is one point.
	void comparison_scroll_controller::update_live_range() noexcept
	{
		MinimumQ   = 0.0;
		MaximumQ   = 0.0;
		auto found = false;
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			const auto last = Axis.has_value() ? Axis->last_line_origin(side) : 0.0;
			if (last <= 0.0) {
				continue;
			}
			const auto rail    = rail_extent(side);
			const auto leaves  = position_reaching(side, rail);
			const auto arrives = position_reaching(side, last + rail);
			MinimumQ           = found ? std::min(MinimumQ, leaves) : leaves;
			MaximumQ           = found ? std::max(MaximumQ, arrives) : arrives;
			found              = true;
		}
	}

	void comparison_scroll_controller::apply_position(
		const qreal aPosition, const bool aForce)
	{
		if (AxisSuspended || !Axis.has_value() || !std::isfinite(aPosition)) {
			return;
		}
		if (ApplyingFrame) {
			++Statistics.ReentrantInputsSuppressed;
			return;
		}

		const auto next = clamp_position(aPosition);
		if (!aForce && same_position(next, Q)) {
			return;
		}
		const auto frame = make_frame(next);
		if (!frame.has_value()) {
			return;
		}

		Q             = next;
		CurrentFrame  = frame;
		ApplyingFrame = true;
		update_scrollbar_projection();
		const auto self = QPointer<comparison_scroll_controller>{ this };
		try {
			const auto sink = FrameSink;
			if (sink) {
				sink(*CurrentFrame);
			}
		} catch (...) {
			if (self == nullptr) {
				return;
			}
			++Statistics.FrameSinkFailures;
		}
		if (self == nullptr) {
			return;
		}
		ApplyingFrame = false;
		++Statistics.FramesApplied;
		schedule_repaint();
		drain_pending_axis_change();
	}

	void comparison_scroll_controller::update_scrollbar_projection()
	{
		if (SharedScrollBar == nullptr) {
			return;
		}
		const auto blocker = QSignalBlocker{ SharedScrollBar };
		SharedScrollBar->setRange(0, Configuration.ScrollbarPrecision);
		const auto total = MaximumQ - MinimumQ;
		const auto page  = total > 0.0
			? std::clamp(
				  static_cast<int>(std::llround(
					  page_extent() / total
					  * static_cast<qreal>(Configuration.ScrollbarPrecision))),
				  1,
				  Configuration.ScrollbarPrecision)
			: Configuration.ScrollbarPrecision;
		SharedScrollBar->setPageStep(page);
		SharedScrollBar->setSingleStep(std::max(1, page / 10));
		SharedScrollBar->setValue(scrollbar_value_for(Q));
	}

	void comparison_scroll_controller::schedule_repaint()
	{
		if (!RepaintSink) {
			return;
		}
		if (RepaintPending) {
			++Statistics.RepaintRequestsCoalesced;
			return;
		}
		RepaintPending = true;
		QTimer::singleShot(0, this, [this]() { deliver_repaint(); });
	}

	void comparison_scroll_controller::deliver_repaint()
	{
		if (!RepaintPending) {
			return;
		}
		RepaintPending  = false;
		const auto self = QPointer<comparison_scroll_controller>{ this };
		try {
			const auto sink = RepaintSink;
			if (sink) {
				sink();
				if (self != nullptr) {
					++Statistics.RepaintDeliveries;
				}
			}
		} catch (...) {
			if (self == nullptr) {
				return;
			}
			++Statistics.RepaintSinkFailures;
		}
	}

	void comparison_scroll_controller::drain_pending_axis_change()
	{
		if (ApplyingFrame || DrainingAxisChange
			|| (!ClearAxisPending && !PendingAxis.has_value())) {
			return;
		}
		DrainingAxisChange  = true;
		AxisChangeScheduled = false;
		const auto self     = QPointer<comparison_scroll_controller>{ this };
		if (ClearAxisPending) {
			ClearAxisPending = false;
			clear_axis();
		} else {
			auto axis = std::move(*PendingAxis);
			PendingAxis.reset();
			set_axis(std::move(axis));
		}
		if (self == nullptr) {
			return;
		}
		DrainingAxisChange = false;
		if (ClearAxisPending || PendingAxis.has_value()) {
			schedule_pending_axis_change();
		}
	}

	void comparison_scroll_controller::schedule_pending_axis_change()
	{
		if (AxisChangeScheduled) {
			return;
		}
		AxisChangeScheduled = true;
		QTimer::singleShot(0, this, [this]() {
			AxisChangeScheduled = false;
			drain_pending_axis_change();
		});
	}

	auto comparison_scroll_controller::handle_wheel_event(
		const comparison_scroll_receiver aReceiver,
		QWheelEvent&                     aEvent) -> bool
	{
		if (AxisSuspended) {
			// Do not let a native pane scroll independently using stale geometry.
			aEvent.accept();
			return true;
		}
		if (!Axis.has_value()) {
			return false;
		}
		++Statistics.WheelEvents;
		auto* const binding        = &InputBindings[receiver_index(aReceiver)];
		const auto  pixel          = aEvent.pixelDelta();
		const auto  angle          = aEvent.angleDelta();
		const auto  horizontalOnly = aEvent.modifiers().testFlag(Qt::ShiftModifier)
			&& pixel.x() == 0 && angle.x() == 0;
		auto handled = false;

		if (!pixel.isNull()) {
			++Statistics.PixelWheelEvents;
			const auto horizontal = horizontalOnly
				? static_cast<qreal>(pixel.y())
				: static_cast<qreal>(pixel.x());
			if (horizontal != 0.0) {
				handled = apply_horizontal_delta(*binding, -horizontal)
					|| handled;
			}
			if (!horizontalOnly && pixel.y() != 0) {
				scroll_by(-static_cast<qreal>(pixel.y()));
				handled = true;
			}
		} else if (!angle.isNull()) {
			++Statistics.AngleWheelEvents;
			const auto horizontal = horizontalOnly ? angle.y() : angle.x();
			if (horizontal != 0) {
				handled = apply_horizontal_delta(
							  *binding,
							  -static_cast<qreal>(horizontal) / 120.0
								  * angle_step_extent())
					|| handled;
			}
			if (!horizontalOnly && angle.y() != 0) {
				scroll_by(
					-static_cast<qreal>(angle.y()) / 120.0
					* angle_step_extent());
				handled = true;
			}
		}

		if (!handled && aEvent.phase() != Qt::NoScrollPhase) {
			handled = true;
		}
		if (handled) {
			aEvent.accept();
		}
		return handled;
	}

	auto comparison_scroll_edge_for(const QKeyEvent& aEvent) -> std::optional<comparison_scroll_edge>
	{
		if (aEvent.matches(QKeySequence::MoveToStartOfDocument)) {
			return comparison_scroll_edge::top;
		}
		if (aEvent.matches(QKeySequence::MoveToEndOfDocument)) {
			return comparison_scroll_edge::bottom;
		}
#if defined(Q_OS_MACOS)
		// macOS marks the arrow keys as keypad keys.
		if ((aEvent.modifiers() & ~Qt::KeypadModifier) == Qt::ControlModifier) {
			if (aEvent.key() == Qt::Key_Up) {
				return comparison_scroll_edge::top;
			}
			if (aEvent.key() == Qt::Key_Down) {
				return comparison_scroll_edge::bottom;
			}
		}
#endif
		return std::nullopt;
	}

	auto comparison_scroll_controller::handle_key_event(QKeyEvent& aEvent) -> bool
	{
		if (AxisSuspended) {
			switch (aEvent.key()) {
			case Qt::Key_Up:
			case Qt::Key_Down:
			case Qt::Key_PageUp:
			case Qt::Key_PageDown:
			case Qt::Key_Home:
			case Qt::Key_End:
				aEvent.accept();
				return true;
			default:
				break;
			}
		}
		// [[BPR-1328]]. The document start and end go to the top and bottom
		// of the comparison.
		if (const auto edge = comparison_scroll_edge_for(aEvent); edge.has_value() && Axis.has_value() && !AxisSuspended) {
			if (*edge == comparison_scroll_edge::top) {
				home();
			} else {
				end();
			}
			aEvent.accept();
			return true;
		}
		if ((!Axis.has_value() && !AxisSuspended) || aEvent.modifiers() != Qt::NoModifier) {
			return false;
		}
		switch (aEvent.key()) {
		case Qt::Key_PageUp:
			page_up();
			break;
		case Qt::Key_PageDown:
			page_down();
			break;
		case Qt::Key_Home:
			home();
			break;
		case Qt::Key_End:
			end();
			break;
		default:
			return false;
		}
		aEvent.accept();
		return true;
	}

	void comparison_scroll_controller::handle_scrollbar_action(const int aAction)
	{
		if (AxisSuspended || ApplyingFrame || !Axis.has_value() || SharedScrollBar == nullptr) {
			return;
		}
		auto target = std::optional<qreal>{ };
		switch (static_cast<QAbstractSlider::SliderAction>(aAction)) {
		case QAbstractSlider::SliderSingleStepAdd:
			target = clamp_position(Q + single_step_extent());
			break;
		case QAbstractSlider::SliderSingleStepSub:
			target = clamp_position(Q - single_step_extent());
			break;
		case QAbstractSlider::SliderPageStepAdd:
			target = clamp_position(Q + page_extent());
			break;
		case QAbstractSlider::SliderPageStepSub:
			target = clamp_position(Q - page_extent());
			break;
		case QAbstractSlider::SliderToMinimum:
			target = minimum_position();
			break;
		case QAbstractSlider::SliderToMaximum:
			target = maximum_position();
			break;
		case QAbstractSlider::SliderMove:
		case QAbstractSlider::SliderNoAction:
			return;
		}

		++Statistics.ScrollbarInputs;
		const auto value = scrollbar_value_for(*target);
		if (same_position(*target, Q)) {
			PendingScrollbarTarget.reset();
			update_scrollbar_projection();
			return;
		}
		if (value == SharedScrollBar->value()) {
			PendingScrollbarTarget.reset();
			apply_position(*target, false);
			return;
		}
		PendingScrollbarTarget = pending_scrollbar_target{
			.Value    = value,
			.Position = *target,
		};
		const auto self = QPointer<comparison_scroll_controller>{ this };
		SharedScrollBar->setSliderPosition(value);
		if (self == nullptr) {
			return;
		}
		if (PendingScrollbarTarget.has_value()
			&& SharedScrollBar->value() == value) {
			PendingScrollbarTarget.reset();
			apply_position(*target, false);
		}
	}

	auto comparison_scroll_controller::apply_horizontal_delta(
		input_binding& aBinding,
		const qreal    aDelta) -> bool
	{
		auto* const scrollBar = aBinding.HorizontalScrollBar.data();
		if (scrollBar == nullptr || !std::isfinite(aDelta)) {
			return false;
		}
		const auto accumulated      = aBinding.HorizontalResidual + aDelta;
		const auto whole            = static_cast<int>(std::trunc(accumulated));
		aBinding.HorizontalResidual = accumulated - static_cast<qreal>(whole);
		const auto previous         = scrollBar->value();
		scrollBar->setValue(previous + whole);
		if ((scrollBar->value() == scrollBar->minimum() && accumulated < 0.0)
			|| (scrollBar->value() == scrollBar->maximum() && accumulated > 0.0)) {
			aBinding.HorizontalResidual = 0.0;
		}
		++Statistics.HorizontalEvents;
		return true;
	}

	auto comparison_scroll_controller::angle_step_extent() const noexcept -> qreal
	{
		const auto* const hints = QGuiApplication::styleHints();
		const auto        lines = hints != nullptr
			? std::max(1, hints->wheelScrollLines())
			: 1;
		return static_cast<qreal>(lines) * Configuration.AngleStepExtent;
	}

	auto comparison_scroll_controller::page_extent() const noexcept -> qreal
	{
		if (!Axis.has_value()) {
			return 0.0;
		}
		return Configuration.PageFraction
			* std::min(
				   Axis->left_viewport_extent(),
				   Axis->right_viewport_extent());
	}

	auto comparison_scroll_controller::single_step_extent() const noexcept -> qreal
	{
		return page_extent() / 10.0;
	}

	auto comparison_scroll_controller::scrollbar_value_for(
		const qreal aPosition) const noexcept -> int
	{
		const auto total = MaximumQ - MinimumQ;
		return total > 0.0
			? static_cast<int>(std::llround(
				  (clamp_position(aPosition) - MinimumQ) / total
				  * static_cast<qreal>(Configuration.ScrollbarPrecision)))
			: 0;
	}

	auto comparison_scroll_controller::position_for_scrollbar_value(
		const int aValue) const noexcept -> qreal
	{
		return MinimumQ
			+ (MaximumQ - MinimumQ)
			* static_cast<qreal>(std::clamp(
				aValue, 0, Configuration.ScrollbarPrecision))
			/ static_cast<qreal>(Configuration.ScrollbarPrecision);
	}
} // namespace bpr::qt
