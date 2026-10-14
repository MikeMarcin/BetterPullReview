#ifndef BPR_SRC_CORE_SRC_DIFF_INTRALINE_HPP
#define BPR_SRC_CORE_SRC_DIFF_INTRALINE_HPP

#include "diff_step_meter.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// [[BPR-1481]] Private to the core's diff sources: the common scalars of one
// similar line pair, from dtl's O(NP) comparison (RFC-BPR-0082). dtl is used
// for this per-line comparison only, never as a whole-file line matcher.
namespace bpr::core::detail {
	/// \brief The largest (left + 1) x (right + 1) scalar grid dtl compares.
	/// dtl records one path point per snake, at most (left + 1) x (right + 1)
	/// for a whole comparison, and past 2,000,000 points it restarts on a path
	/// that can join unequal scalars. A grid below that bound never restarts.
	inline constexpr std::uint64_t intraline_dtl_cells_limit = 1'999'999U;

	/// \brief The strictly increasing (left, right) indices of the scalars dtl
	/// finds common to `aLeft` and `aRight`. Nothing when the pair's grid is
	/// larger than `intraline_dtl_cells_limit`, or when dtl's script breaks the
	/// engine's match rules; the caller then compares the pair as before.
	///
	/// Each scalar comparison is a step of `aMeter`, and the scalars recorded
	/// after it are steps too. A stop is honoured within 4,096 comparisons:
	/// dtl is finished quickly, its result dropped, and the stop thrown again,
	/// so no stop unwinds through dtl's own allocations.
	[[nodiscard]] auto intraline_common_scalars(std::span<const std::uint32_t> aLeft,
												std::span<const std::uint32_t> aRight,
												diff_step_meter&               aMeter)
		-> std::optional<std::vector<std::pair<std::size_t, std::size_t>>>;
} // namespace bpr::core::detail

#endif // BPR_SRC_CORE_SRC_DIFF_INTRALINE_HPP
