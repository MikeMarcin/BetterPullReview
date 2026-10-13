#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PIXEL_SCROLL_GESTURE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PIXEL_SCROLL_GESTURE_HPP

#include <QElapsedTimer>
#include <QPoint>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	/// \brief Discards motion on the unintended axis without banking it for later.
	/// \details Qt-local input state shared by the split and unified readers.
	/// Dominance is assessed on delivered motion, before any scrollbar clamp.
	class pixel_scroll_gesture
	{
	public:
		void reset() noexcept
		{
			Axis          = direction::undecided;
			ReleaseTravel = 0.0;
			Device        = nullptr;
			LastInput.invalidate();
		}

		[[nodiscard]] auto consume(const QWheelEvent& aEvent) -> QPoint
		{
			if (aEvent.phase() == Qt::ScrollBegin || Device != aEvent.pointingDevice()
				|| (aEvent.phase() == Qt::NoScrollPhase && LastInput.isValid() && LastInput.elapsed() > 250)) {
				reset();
			}
			Device = aEvent.pointingDevice();
			LastInput.start();
			auto       delta       = aEvent.pixelDelta();
			const auto x           = std::abs(static_cast<qreal>(delta.x()));
			const auto y           = std::abs(static_cast<qreal>(delta.y()));
			const auto predominant = x > 0.0 && x >= 2.0 * y ? direction::horizontal
				: y > 0.0 && y >= 2.0 * x                    ? direction::vertical
															 : direction::undecided;
			if (Axis == direction::undecided) {
				Axis = predominant;
			} else if (predominant == Axis) {
				ReleaseTravel = 0.0;
			} else if (!delta.isNull() && aEvent.phase() != Qt::ScrollMomentum) {
				// Evidence changes intent, never the eventual scroll distance.
				// Opposing wobble cancels; two tiny events cannot unlock an axis.
				ReleaseTravel += Axis == direction::horizontal ? delta.y() : delta.x();
				if (std::abs(ReleaseTravel) >= 12.0) {
					Axis          = predominant;
					ReleaseTravel = 0.0;
				}
			}
			if (Axis == direction::horizontal) {
				delta.setY(0);
			} else if (Axis == direction::vertical) {
				delta.setX(0);
			}
			if (aEvent.phase() == Qt::ScrollEnd) {
				reset();
			}
			return delta;
		}

	private:
		enum class direction { undecided,
							   horizontal,
							   vertical };
		const QPointingDevice* Device = nullptr;
		QElapsedTimer          LastInput;
		direction              Axis          = direction::undecided;
		qreal                  ReleaseTravel = 0.0;
	};
} // namespace bpr::qt

#endif
