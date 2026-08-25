#include "bpr/qt/event_loop_device_timer.hpp"

#include <chrono>
#include <limits>
#include <utility>

namespace bpr::qt {
	namespace {
		// Qt counts milliseconds in an int. The host's bounds are already far
		// below this, so the clamp is a guard against a future bound rather
		// than a behaviour the schedule relies on.
		constexpr std::uint32_t maximum_delay_seconds
			= static_cast<std::uint32_t>(
				  std::numeric_limits<int>::max())
			/ 1000U;
	} // namespace

	event_loop_device_timer::event_loop_device_timer()
	{
		Timer.setSingleShot(true);
		// Coarse timers may fire early; the poll interval is a floor the
		// service imposes, so an early wake would send a request the schedule
		// says is not due yet.
		Timer.setTimerType(Qt::PreciseTimer);
		QObject::connect(&Timer, &QTimer::timeout, &Timer, [this] {
			// Released before it runs, so an expiry that arms the next delay
			// cannot be overwritten by its own completion.
			auto expiry = std::exchange(Expiry, std::function<void()>{ });
			if (expiry) {
				expiry();
			}
		});
	}

	auto event_loop_device_timer::schedule_after(
		const std::uint32_t   aSeconds,
		std::function<void()> aExpiry) -> bool
	{
		if (!aExpiry || aSeconds > maximum_delay_seconds) {
			return false;
		}
		// Arming replaces rather than accumulates: the port promises at most
		// one live delay, and two would race for the same poll.
		Timer.stop();
		Expiry = std::move(aExpiry);
		Timer.start(std::chrono::seconds{ aSeconds });
		return Timer.isActive();
	}

	void event_loop_device_timer::cancel() noexcept
	{
		Timer.stop();
		Expiry = { };
	}

	auto event_loop_device_timer::armed() const noexcept -> bool
	{
		return Timer.isActive();
	}
} // namespace bpr::qt
