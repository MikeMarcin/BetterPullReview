#ifndef BPR_SRC_CORE_SRC_DIFF_STEP_METER_HPP
#define BPR_SRC_CORE_SRC_DIFF_STEP_METER_HPP

#include "bpr/core/diff_compute.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

// [[BPR-1366]] Private to the core's diff sources: the step accounting and
// stop checks shared by the engine, alignment validation and the semantic
// index build. Nothing here reads a clock or a thread primitive; the caller's
// stop predicate is the only way out of a computation before it ends.
// [[BPR-1485]] A stop abandons the computation: nothing is kept to resume it.
namespace bpr::core::detail {
	/// \brief Thrown by a meter whose caller asked it to stop. Only the
	/// engine's driver and the stoppable index build catch it; a stop can never
	/// be mistaken for a work limit, which degrades the alignment instead.
	struct diff_compute_stopped
	{
	};

	/// \brief Counts a computation's steps and polls the caller's stop
	/// predicate every `CheckInterval` steps.
	class diff_step_meter final
	{
	public:
		diff_step_meter(diff_compute_stop* const aStop, const std::uint64_t aCheckInterval) noexcept
			: Stop{ aStop }
			, Interval{ std::max<std::uint64_t>(aCheckInterval, 1U) }
			, NextCheck{ aStop == nullptr ? std::numeric_limits<std::uint64_t>::max() : advance(0U) }
		{
		}

		void tick(const std::uint64_t aAmount)
		{
			Steps += aAmount;
			if (Steps >= NextCheck) [[unlikely]] {
				check();
			}
		}

		/// \brief Polls the stop predicate now, whatever the count: for a wait
		/// that does no counted work.
		void poll();

		[[nodiscard]] auto steps() const noexcept -> std::uint64_t
		{
			return Steps;
		}

		/// The most steps a poll has come after it was due: a stop is
		/// honoured within the check interval plus this many steps.
		[[nodiscard]] auto largest_poll_delay() const noexcept -> std::uint64_t
		{
			return LargestPollDelay;
		}

	private:
		[[nodiscard]] auto advance(const std::uint64_t aFrom) const noexcept -> std::uint64_t
		{
			return aFrom > std::numeric_limits<std::uint64_t>::max() - Interval
				? std::numeric_limits<std::uint64_t>::max()
				: aFrom + Interval;
		}

		/// Polls the stop predicate; out of line, so the counting stays small
		/// in the engine's loops.
		void check();

		diff_compute_stop* Stop             = nullptr;
		std::uint64_t      Interval         = 1U;
		std::uint64_t      Steps            = 0U;
		std::uint64_t      NextCheck        = 0U;
		std::uint64_t      LargestPollDelay = 0U;
	};

	/// \brief The public validators' meter: it counts nothing and never stops.
	struct unmetered
	{
		static void tick(std::uint64_t /*aAmount*/) noexcept
		{
		}
	};

	/// \brief Step weights of validation records, so a step stays roughly the
	/// same amount of work in every phase. A record's identity is a digest.
	inline constexpr std::uint64_t validation_record_steps = 256U;

	/// \brief `validate_alignment` with stop checks.
	[[nodiscard]] auto validate_alignment(const file_alignment& aAlignment, diff_step_meter& aMeter)
		-> alignment_validation_result;
	[[nodiscard]] auto validate_alignment(const material_alignment& aAlignment, diff_step_meter& aMeter)
		-> alignment_validation_result;

	/// \brief `validate_computed_file_diff` with stop checks; a stop abandons it.
	[[nodiscard]] auto validate_computed_file_diff(const computed_file_diff& aDiff, diff_step_meter& aMeter)
		-> diff_compute_validation_result;
} // namespace bpr::core::detail

#endif // BPR_SRC_CORE_SRC_DIFF_STEP_METER_HPP
