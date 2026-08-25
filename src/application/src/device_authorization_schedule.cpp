#include "bpr/application/device_authorization_schedule.hpp"

#include <algorithm>
#include <limits>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto saturating_sum(
			const std::uint32_t aLeft,
			const std::uint32_t aRight) noexcept -> std::uint32_t
		{
			constexpr auto ceiling = std::numeric_limits<std::uint32_t>::max();
			if (aLeft > ceiling - aRight) {
				return ceiling;
			}
			return aLeft + aRight;
		}

		[[nodiscard]] auto clamped_bounds(
			device_authorization_schedule_bounds aBounds) noexcept
			-> device_authorization_schedule_bounds
		{
			aBounds.IntervalSeconds = std::clamp(
				aBounds.IntervalSeconds,
				minimum_device_authorization_interval_seconds,
				maximum_device_authorization_interval_seconds);
			aBounds.ExpiresInSeconds = std::clamp(
				aBounds.ExpiresInSeconds,
				1U,
				maximum_device_authorization_validity_seconds);
			aBounds.TotalBoundSeconds = std::clamp(
				aBounds.TotalBoundSeconds,
				1U,
				device_authorization_total_bound_seconds);
			aBounds.MaximumPolls = std::clamp(
				aBounds.MaximumPolls,
				1U,
				maximum_device_authorization_polls);
			return aBounds;
		}
	}

	device_authorization_schedule::device_authorization_schedule(
		device_authorization_schedule_bounds aBounds) noexcept
		: Bounds{ clamped_bounds(aBounds) }
	{
	}

	auto device_authorization_schedule::bounds() const noexcept
		-> const device_authorization_schedule_bounds&
	{
		return Bounds;
	}

	auto device_authorization_schedule::interval_seconds() const noexcept
		-> std::uint32_t
	{
		return Bounds.IntervalSeconds;
	}

	auto device_authorization_schedule::polls() const noexcept -> std::uint32_t
	{
		return Polls;
	}

	auto device_authorization_schedule::deadline_seconds() const noexcept
		-> std::uint32_t
	{
		return std::min(Bounds.ExpiresInSeconds, Bounds.TotalBoundSeconds);
	}

	void device_authorization_schedule::slow_down(
		const std::uint32_t aIntervalSeconds) noexcept
	{
		const auto increased = saturating_sum(
			Bounds.IntervalSeconds,
			device_authorization_slow_down_increment_seconds);
		Bounds.IntervalSeconds = std::min(
			std::max(increased, aIntervalSeconds),
			maximum_device_authorization_interval_seconds);
	}

	void device_authorization_schedule::record_poll(
		const std::uint32_t aElapsedSeconds) noexcept
	{
		LastPollSeconds = std::max(LastPollSeconds, aElapsedSeconds);
		Polls           = saturating_sum(Polls, 1U);
	}

	auto device_authorization_schedule::decide(
		const std::uint32_t aElapsedSeconds) const noexcept
		-> device_authorization_schedule_decision
	{
		// Time is answered before poll count so a flow that ran out of both
		// reports the reason the reviewer can see for themselves.
		const auto deadline = deadline_seconds();
		if (aElapsedSeconds >= deadline) {
			return device_authorization_schedule_decision{
				.Action      = Bounds.ExpiresInSeconds <= Bounds.TotalBoundSeconds
					? device_authorization_schedule_action::expired
					: device_authorization_schedule_action::bound_exceeded,
				.WaitSeconds = 0U,
			};
		}
		if (Polls >= Bounds.MaximumPolls) {
			return device_authorization_schedule_decision{
				.Action      = device_authorization_schedule_action::bound_exceeded,
				.WaitSeconds = 0U,
			};
		}

		// The grant instant counts as the last poll, so the first poll waits a
		// full interval like every later one.
		const auto due = saturating_sum(LastPollSeconds, Bounds.IntervalSeconds);
		if (aElapsedSeconds >= due) {
			return device_authorization_schedule_decision{
				.Action      = device_authorization_schedule_action::poll_now,
				.WaitSeconds = 0U,
			};
		}

		// Waking at the deadline rather than at a poll that would fall past it
		// is what makes the terminal exact instead of one interval late.
		const auto wake = std::min(due, deadline);
		return device_authorization_schedule_decision{
			.Action      = device_authorization_schedule_action::wait,
			.WaitSeconds = wake - aElapsedSeconds,
		};
	}

	auto device_authorization_schedule::remaining_seconds(
		const std::uint32_t aElapsedSeconds) const noexcept -> std::uint32_t
	{
		const auto deadline = deadline_seconds();
		if (aElapsedSeconds >= deadline) {
			return 0U;
		}
		return deadline - aElapsedSeconds;
	}
}
