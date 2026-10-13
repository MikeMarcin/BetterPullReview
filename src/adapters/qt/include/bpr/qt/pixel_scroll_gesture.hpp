#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PIXEL_SCROLL_GESTURE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PIXEL_SCROLL_GESTURE_HPP

#include <QElapsedTimer>
#include <QPoint>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	/// \brief Discards incidental vertical motion without banking it for later.
	/// \details Qt-local input state shared by the split and unified readers.
	/// Dominance is assessed on delivered motion, before any scrollbar clamp.
	class pixel_scroll_gesture
	{
	public:
		void reset() noexcept
		{
			Horizontal    = false;
			ReleaseEvents = 0;
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
			auto       delta = aEvent.pixelDelta();
			const auto x     = std::abs(static_cast<qreal>(delta.x()));
			const auto y     = std::abs(static_cast<qreal>(delta.y()));
			if (x >= 4.0 * std::max(1.0, y)) {
				Horizontal    = true;
				ReleaseEvents = 0;
				delta.setY(0);
			} else if (Horizontal && y > 0) {
				if (y >= 8 || ++ReleaseEvents >= 2) {
					Horizontal    = false;
					ReleaseEvents = 0;
				} else {
					delta.setY(0);
				}
			}
			if (aEvent.phase() == Qt::ScrollEnd) {
				reset();
			}
			return delta;
		}

	private:
		const QPointingDevice* Device = nullptr;
		QElapsedTimer          LastInput;
		bool                   Horizontal    = false;
		int                    ReleaseEvents = 0;
	};
} // namespace bpr::qt

#endif
