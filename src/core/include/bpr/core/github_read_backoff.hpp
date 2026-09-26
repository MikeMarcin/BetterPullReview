#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_READ_BACKOFF_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_READ_BACKOFF_HPP

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace bpr::core {
	/// \brief Explicit host clock observation; the policy never samples time.
	struct github_read_time
	{
		std::uint64_t MonotonicMilliseconds = 0;
		std::uint64_t UnixSeconds           = 0;
	};

	/// \brief One resource's explicit-retry deadline. No automatic work is scheduled.
	/// \details Server minimums are never capped or shortened by an older response.
	/// Missing/past minimums use 60-second exponential backoff, capped at 900 seconds.
	class github_read_backoff
	{
	public:
		void observe(github_read_time aNow, std::optional<std::uint32_t> aRetryAfter,
					 std::optional<std::uint64_t> aPrimaryReset) noexcept
		{
			const auto reset   = aPrimaryReset && *aPrimaryReset > aNow.UnixSeconds
				? *aPrimaryReset - aNow.UnixSeconds
				: 0U;
			auto       seconds = std::max<std::uint64_t>(aRetryAfter.value_or(0U), reset);
			if (seconds == 0U) {
				seconds = std::min<std::uint64_t>(60U << Failures, 900U);
			}
			Failures            = std::min<std::uint8_t>(Failures + 1U, 4U);
			const auto maximum  = std::numeric_limits<std::uint64_t>::max();
			const auto capacity = (maximum - aNow.MonotonicMilliseconds) / 1000U;
			const auto deadline = seconds > capacity ? maximum : aNow.MonotonicMilliseconds + seconds * 1000U;
			Deadline            = std::max(Deadline, deadline);
		}

		[[nodiscard]] auto remaining_seconds(github_read_time aNow) const noexcept -> std::uint64_t
		{
			const auto remaining = Deadline > aNow.MonotonicMilliseconds ? Deadline - aNow.MonotonicMilliseconds : 0U;
			return remaining / 1000U + (remaining % 1000U != 0U ? 1U : 0U);
		}

		/// \brief Success only resets the fallback after the existing minimum has passed.
		void succeeded(github_read_time aNow) noexcept
		{
			if (remaining_seconds(aNow) == 0U) {
				Failures = 0;
			}
		}

	private:
		std::uint64_t Deadline = 0;
		std::uint8_t  Failures = 0;
	};
}
#endif
