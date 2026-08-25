#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EVENT_LOOP_DEVICE_TIMER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EVENT_LOOP_DEVICE_TIMER_HPP

#include "bpr/application/device_authorization_application_host.hpp"

#include <QTimer>

#include <cstdint>
#include <functional>

namespace bpr::qt {
	/// \brief Arms one interruptible poll delay on the Qt event loop.
	///
	/// \details The production timer behind `device_authorization_timer_port`.
	/// The host owns the schedule and its bounds and decides every delay; this
	/// owns only the waiting, which is the part that needs a running event loop
	/// and therefore could not be verified until composition existed.
	///
	/// One timer, single-shot, restarted rather than accumulated: the port's
	/// contract is that at most one delay is armed at a time, so arming a
	/// second replaces the first instead of leaving two callbacks racing for
	/// the same poll. `cancel` stops it without firing, which is what makes a
	/// cancelled or shut-down flow stop polling immediately rather than at the
	/// next interval boundary.
	///
	/// Expiry runs on the thread that owns the timer, which composition makes
	/// the decision lane's thread, so the callback reaches the host already
	/// serialized and this adapter posts nothing itself.
	class event_loop_device_timer final
		: public application::device_authorization_timer_port
	{
	public:
		event_loop_device_timer();

		[[nodiscard]] auto schedule_after(
			std::uint32_t         aSeconds,
			std::function<void()> aExpiry) -> bool override;
		void cancel() noexcept override;

		[[nodiscard]] auto armed() const noexcept -> bool;

	private:
		QTimer                Timer;
		std::function<void()> Expiry;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_EVENT_LOOP_DEVICE_TIMER_HPP
