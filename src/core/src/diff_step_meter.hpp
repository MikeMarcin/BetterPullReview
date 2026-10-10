#ifndef BPR_SRC_CORE_SRC_DIFF_STEP_METER_HPP
#define BPR_SRC_CORE_SRC_DIFF_STEP_METER_HPP

#include "bpr/core/diff_compute.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

// [[BPR-1366]] Private to the core's diff sources: the step accounting and
// stop checks shared by the engine, alignment validation and the semantic
// index build. Nothing here reads a clock or a thread primitive; the caller's
// stop predicate is the only way out of a computation before it ends.
namespace bpr::core::detail {
	/// \brief Thrown by a meter whose caller asked it to stop. Only the
	/// engine's driver and the stoppable index build catch it; a stop can never
	/// be mistaken for a work limit, which degrades the alignment instead.
	struct diff_compute_paused
	{
	};

	/// \brief Counts a computation's steps and polls the caller's stop
	/// predicate every `CheckInterval` steps.
	///
	/// \details `tick` counts work a pause would discard (inside a restart
	/// unit); `tick_kept` counts work the resumable state already records and
	/// marks a resume point. `commit` marks a resume point after a restart
	/// unit's result has been stored.
	class diff_step_meter final
	{
	public:
		diff_step_meter(diff_compute_stop* const aStop, const std::uint64_t aCheckInterval,
						const std::uint64_t aSteps) noexcept
			: Stop{ aStop }
			, Interval{ std::max<std::uint64_t>(aCheckInterval, 1U) }
			, Steps{ aSteps }
			, Committed{ aSteps }
			, SliceStart{ aSteps }
			, NextCheck{ aStop == nullptr ? std::numeric_limits<std::uint64_t>::max() : advance(aSteps) }
		{
		}

		void tick(const std::uint64_t aAmount)
		{
			Steps += aAmount;
			if (Steps >= NextCheck) [[unlikely]] {
				check();
			}
		}

		void tick_kept(const std::uint64_t aAmount)
		{
			Steps += aAmount;
			commit();
			if (Steps >= NextCheck) [[unlikely]] {
				check();
			}
		}

		void commit() noexcept
		{
			LargestUnit = std::max(LargestUnit, Steps - Committed);
			Committed   = Steps;
		}

		[[nodiscard]] auto steps() const noexcept -> std::uint64_t
		{
			return Steps;
		}

		[[nodiscard]] auto committed() const noexcept -> std::uint64_t
		{
			return Committed;
		}

		[[nodiscard]] auto slice_steps() const noexcept -> std::uint64_t
		{
			return Steps - SliceStart;
		}

		[[nodiscard]] auto largest_unit() const noexcept -> std::uint64_t
		{
			return LargestUnit;
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
		std::uint64_t      Committed        = 0U;
		std::uint64_t      SliceStart       = 0U;
		std::uint64_t      NextCheck        = 0U;
		std::uint64_t      LargestUnit      = 0U;
		std::uint64_t      LargestPollDelay = 0U;
	};

	/// \brief The public validators' meter: it counts nothing and never stops.
	struct unmetered
	{
		static void tick(std::uint64_t /*aAmount*/) noexcept
		{
		}

		static void tick_kept(std::uint64_t /*aAmount*/) noexcept
		{
		}

		static void commit() noexcept
		{
		}
	};

	/// \brief Step weights of validation records, so a step stays roughly the
	/// same amount of work in every phase. A record's identity is a digest.
	inline constexpr std::uint64_t validation_record_steps = 256U;

	/// \brief Where a staged validation of a computed diff resumes.
	struct validation_cursor
	{
		std::uint8_t  Stage   = 0U;
		std::size_t   Segment = 0U;
		std::uint64_t Offset  = 0U;
	};

	/// \brief `validate_alignment` with stop checks; a stop restarts it.
	[[nodiscard]] auto validate_alignment(const file_alignment& aAlignment, diff_step_meter& aMeter)
		-> alignment_validation_result;
	[[nodiscard]] auto validate_alignment(const material_alignment& aAlignment, diff_step_meter& aMeter)
		-> alignment_validation_result;

	/// \brief `validate_computed_file_diff` with stop checks, resuming from
	/// `aCursor`: a completed stage is not repeated, and the equal-content
	/// stage resumes at its line.
	[[nodiscard]] auto validate_computed_file_diff(const computed_file_diff& aDiff, validation_cursor& aCursor,
												   diff_step_meter& aMeter) -> diff_compute_validation_result;
} // namespace bpr::core::detail

#endif // BPR_SRC_CORE_SRC_DIFF_STEP_METER_HPP
