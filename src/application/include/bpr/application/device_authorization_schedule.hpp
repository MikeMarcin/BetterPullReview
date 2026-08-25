#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_SCHEDULE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_SCHEDULE_HPP

#include <compare>
#include <cstdint>

namespace bpr::application {
	// The floor exists because the interval is a server-supplied number and the
	// host must not poll faster than it under any answer, including zero.
	inline constexpr std::uint32_t minimum_device_authorization_interval_seconds = 5U;
	// The ceiling exists for the opposite reason: a server that answers with an
	// enormous interval must not be able to park the flow past its own deadline.
	inline constexpr std::uint32_t maximum_device_authorization_interval_seconds = 300U;
	inline constexpr std::uint32_t maximum_device_authorization_validity_seconds = 3'600U;
	// RFC 8628 raises the interval by five seconds when the server says
	// `slow_down` without naming a larger one. The recorded GitHub contract does
	// name one, so this is the floor of the increase rather than the whole of it.
	inline constexpr std::uint32_t device_authorization_slow_down_increment_seconds = 5U;
	// The total bound is the host's own patience, not the server's. It is fixed
	// rather than derived so a server that reports an implausible validity cannot
	// hold the flow open for longer than the reviewer would ever wait.
	inline constexpr std::uint32_t device_authorization_total_bound_seconds = 900U;
	inline constexpr std::uint32_t maximum_device_authorization_polls       = 256U;

	/// \brief Describes the device authorization schedule bounds record.
	///
	/// \details Every number is seconds measured from the moment the device code
	/// was granted. `IntervalSeconds` and `ExpiresInSeconds` come from the server;
	/// the other two are the host's own limits and are never negotiated.
	struct device_authorization_schedule_bounds
	{
		std::uint32_t IntervalSeconds   = minimum_device_authorization_interval_seconds;
		std::uint32_t ExpiresInSeconds  = device_authorization_total_bound_seconds;
		std::uint32_t TotalBoundSeconds = device_authorization_total_bound_seconds;
		std::uint32_t MaximumPolls      = maximum_device_authorization_polls;

		auto operator<=>(const device_authorization_schedule_bounds&) const
			= default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const device_authorization_schedule_bounds& aBounds) noexcept -> bool
	{
		return aBounds.IntervalSeconds
			>= minimum_device_authorization_interval_seconds
			&& aBounds.IntervalSeconds
			<= maximum_device_authorization_interval_seconds
			&& aBounds.ExpiresInSeconds > 0U
			&& aBounds.ExpiresInSeconds
			<= maximum_device_authorization_validity_seconds
			&& aBounds.TotalBoundSeconds > 0U
			&& aBounds.TotalBoundSeconds <= device_authorization_total_bound_seconds
			&& aBounds.MaximumPolls > 0U
			&& aBounds.MaximumPolls <= maximum_device_authorization_polls;
	}

	enum class device_authorization_schedule_action : std::uint8_t {
		// Issue a poll now.
		poll_now,
		// Do nothing for the reported number of seconds and ask again.
		wait,
		// The device code the server granted is no longer usable.
		expired,
		// The host's own bound ran out first: too much time, or too many polls.
		bound_exceeded,
	};

	/// \brief Describes the device authorization schedule decision record.
	struct device_authorization_schedule_decision
	{
		device_authorization_schedule_action Action
			= device_authorization_schedule_action::wait;
		// Meaningful only for `wait`, and never longer than the time left before
		// a terminal, so a host that sleeps for exactly this long always wakes to
		// either a poll or the terminal itself.
		std::uint32_t WaitSeconds = 0U;

		auto operator<=>(const device_authorization_schedule_decision&) const
			= default;
	};

	/// \brief Decides when the next device-authorization poll may be issued.
	///
	/// \details Holds no clock and no transport. The host says how many seconds
	/// have passed since the grant and is told what to do, which is what keeps
	/// the whole poll course replayable from a list of numbers. Elapsed time is
	/// an argument rather than a member for the same reason the queue refresh
	/// scheduler expresses backoff as a duration: a moment to wait until would
	/// need a clock to compare against.
	///
	/// The interval only ever grows. A server answer can slow the host down and
	/// nothing can speed it up, so no sequence of answers — including a hostile
	/// one — makes the host poll faster than the interval it was given.
	class device_authorization_schedule final
	{
	public:
		/// \brief Constructs the schedule with the supplied bounds.
		///
		/// \details Out-of-range bounds are clamped rather than refused. The
		/// server chooses two of these four numbers and the host has to keep
		/// polling on any answer that parsed, so the useful behaviour is a
		/// bounded schedule rather than an unusable one.
		explicit device_authorization_schedule(
			device_authorization_schedule_bounds aBounds) noexcept;

		/// \brief Returns the bounds in force after clamping.
		[[nodiscard]] auto bounds() const noexcept
			-> const device_authorization_schedule_bounds&;
		/// \brief Returns the interval currently separating two polls.
		[[nodiscard]] auto interval_seconds() const noexcept -> std::uint32_t;
		/// \brief Returns how many polls have been issued.
		[[nodiscard]] auto polls() const noexcept -> std::uint32_t;
		/// \brief Returns the elapsed second at which the flow ends regardless.
		[[nodiscard]] auto deadline_seconds() const noexcept -> std::uint32_t;

		/// \brief Applies a server instruction to poll less often.
		///
		/// \details The new interval is the larger of the server's number and one
		/// increment above the current one, so `slow_down` with a smaller,
		/// equal, or absent interval still slows the host down.
		void slow_down(std::uint32_t aIntervalSeconds) noexcept;

		/// \brief Records that a poll was issued at the supplied elapsed second.
		void record_poll(std::uint32_t aElapsedSeconds) noexcept;

		/// \brief Decides what to do at the supplied elapsed second.
		[[nodiscard]] auto decide(std::uint32_t aElapsedSeconds) const noexcept
			-> device_authorization_schedule_decision;

		/// \brief Returns the seconds of validity left at the supplied second.
		///
		/// \details Counted to whichever terminal comes first, because that is
		/// the window in which the reviewer can still finish authorizing, and a
		/// larger number displayed against a shorter window would be a lie.
		[[nodiscard]] auto remaining_seconds(
			std::uint32_t aElapsedSeconds) const noexcept -> std::uint32_t;

	private:
		device_authorization_schedule_bounds Bounds;
		std::uint32_t                        LastPollSeconds = 0U;
		std::uint32_t                        Polls           = 0U;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_SCHEDULE_HPP
