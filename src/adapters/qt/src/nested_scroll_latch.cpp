#include "bpr/qt/nested_scroll_latch.hpp"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QScopeGuard>
#include <QScrollBar>
#include <QWheelEvent>
#include <QWidget>

#include <cstdlib>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// The one latch, owned by the application.
		QPointer<nested_scroll_latch> Installed;
		bool                          Delivering = false;

		/// \brief The scroll areas whose viewport holds `aWidget`, innermost
		/// first, up to its window.
		[[nodiscard]] auto areas_of(QWidget& aWidget) -> std::vector<QAbstractScrollArea*>
		{
			auto areas = std::vector<QAbstractScrollArea*>{ };
			for (auto* widget = &aWidget; widget != nullptr; widget = widget->isWindow() ? nullptr : widget->parentWidget()) {
				auto* const area = qobject_cast<QAbstractScrollArea*>(widget);
				if (area != nullptr && area->viewport() != nullptr && (area->viewport() == &aWidget || area->viewport()->isAncestorOf(&aWidget))) {
					areas.push_back(area);
				}
			}
			return areas;
		}

		/// \brief The gesture's axis and its movement along it: a trackpad's
		/// pixels where it gives them, else the wheel's angle.
		[[nodiscard]] auto movement_of(const QWheelEvent& aEvent) -> std::pair<Qt::Orientation, int>
		{
			const auto delta = aEvent.pixelDelta().isNull() ? aEvent.angleDelta() : aEvent.pixelDelta();
			return std::abs(delta.x()) > std::abs(delta.y()) ? std::pair{ Qt::Horizontal, delta.x() } : std::pair{ Qt::Vertical, delta.y() };
		}

		/// \brief Whether to offer the first movement to this area. A negative
		/// delta moves a scrollbar towards its maximum.
		[[nodiscard]] auto can_start_gesture(const QAbstractScrollArea& aArea, const Qt::Orientation aAxis, const int aDelta) -> bool
		{
			if (aArea.property(nested_scroll_latch::wheel_axis_without_range_property).toInt() == static_cast<int>(aAxis)) {
				return true;
			}
			const auto* const bar = aAxis == Qt::Vertical ? aArea.verticalScrollBar() : aArea.horizontalScrollBar();
			if (bar == nullptr || bar->maximum() <= bar->minimum()) {
				return false;
			}
			return aDelta < 0 ? bar->value() < bar->maximum() : bar->value() > bar->minimum();
		}
	} // namespace

	void nested_scroll_latch::install()
	{
		auto* const application = QCoreApplication::instance();
		if (application == nullptr || Installed != nullptr) {
			return;
		}
		Installed = new nested_scroll_latch{ application };
		application->installEventFilter(Installed);
	}

	auto nested_scroll_latch::delivering() noexcept -> bool
	{
		return Delivering;
	}

	auto nested_scroll_latch::owner() -> QAbstractScrollArea*
	{
		return Installed != nullptr ? Installed->Owner.data() : nullptr;
	}

	void nested_scroll_latch::begin(QWidget& aOrigin)
	{
		Origin  = &aOrigin;
		Owner   = nullptr;
		Decided = false;
		Ended   = false;
	}

	auto nested_scroll_latch::deliver(QAbstractScrollArea& aOwner, const QWheelEvent& aEvent) -> bool
	{
		auto* const target = aOwner.viewport();
		auto        copy   = QWheelEvent{ target->mapFromGlobal(aEvent.globalPosition()),
										  aEvent.globalPosition(),
										  aEvent.pixelDelta(),
										  aEvent.angleDelta(),
										  aEvent.buttons(),
										  aEvent.modifiers(),
										  aEvent.phase(),
										  aEvent.inverted(),
										  Qt::MouseEventNotSynthesized,
										  aEvent.pointingDevice() };
		copy.setTimestamp(aEvent.timestamp());
		const auto restore = qScopeGuard([previous = Delivering] { Delivering = previous; });
		Delivering         = true;
		// Sent, not posted: Qt does not pass a sent wheel event to the
		// owner's parents, so what the owner leaves goes nowhere.
		QCoreApplication::sendEvent(target, &copy);
		return copy.isAccepted();
	}

	auto nested_scroll_latch::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent->type() != QEvent::Wheel || !aEvent->spontaneous()) {
			return false;
		}
		auto* const wheel    = static_cast<QWheelEvent*>(aEvent);
		auto* const receiver = qobject_cast<QWidget*>(aWatched);
		const auto  phase    = wheel->phase();
		if (receiver == nullptr || phase == Qt::NoScrollPhase) {
			return false;
		}
		// A gesture begins with ScrollBegin, or with a ScrollUpdate after an
		// end. Momentum after the end is the same gesture's.
		if (phase == Qt::ScrollBegin || (phase == Qt::ScrollUpdate && Ended) || Origin == nullptr) {
			begin(*receiver);
		}
		const auto areas = areas_of(*Origin);
		if (areas.size() < 2U) {
			// One scroller, or none: Qt's own latch is enough.
			if (phase == Qt::ScrollEnd) {
				Ended = true;
			}
			return false;
		}
		auto delivered = false;
		if (!Decided) {
			if (const auto [axis, delta] = movement_of(*wheel); delta != 0) {
				// [[BPR-2292]]. A scroller with room that way is offered the
				// first movement, and owns the gesture only if it takes it.
				// One that declines the axis (an in-place reader or code box
				// passes every vertical wheel to what holds it) is passed
				// over, whatever range its hidden bar reports: under the
				// macOS style an in-place reader's vertical bar keeps a range
				// its page does not have, and the gesture went nowhere.
				for (auto index = std::size_t{ 0U }; index + 1U < areas.size() && Owner == nullptr; ++index) {
					if (can_start_gesture(*areas[index], axis, delta) && deliver(*areas[index], *wheel)) {
						Owner = areas[index];
					}
				}
				delivered = Owner != nullptr;
				if (Owner == nullptr) {
					Owner = areas.back();
				}
				Decided = true;
			}
		}
		if (Owner != nullptr && !delivered) {
			static_cast<void>(deliver(*Owner, *wheel));
		}
		if (phase == Qt::ScrollEnd) {
			Ended = true;
		}
		// Taken here, where it began: Qt then sends the rest of the gesture
		// to this widget, and so to this filter, never to a parent.
		wheel->accept();
		return true;
	}
} // namespace bpr::qt
