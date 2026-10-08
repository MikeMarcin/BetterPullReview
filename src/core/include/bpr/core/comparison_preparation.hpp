#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_COMPARISON_PREPARATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_COMPARISON_PREPARATION_HPP

#include <cstddef>
#include <optional>
#include <vector>

namespace bpr::core {
	/// \brief How many changed files are prepared ahead of the reviewer.
	///
	/// \details [[BPR-1327]]. A review with this many files or fewer is
	/// prepared whole. A larger one is prepared as a window of this many files
	/// around the cursor, recomputed whenever the cursor moves, so work stays
	/// proportional to what the reviewer is about to open rather than to the
	/// size of the pull request.
	inline constexpr std::size_t prepared_comparison_window = 16U;

	/// \brief The largest window a caller may ask for.
	inline constexpr std::size_t maximum_prepared_comparison_window = 64U;

	/// \brief The order in which changed files are prepared.
	///
	/// \details Positions index the reviewer's actionable file order. The file
	/// under the cursor comes first, then the files after it in order, then the
	/// files before it, nearest first. With more files than the window, at most
	/// a quarter of the window (rounded down, after the current file) goes to
	/// the files before the cursor; the rest goes forward, and either side
	/// takes up what the other cannot use near an end. With no cursor the
	/// order starts at the first file. A zero or oversized window, or a cursor
	/// outside the order, yields nothing.
	[[nodiscard]] auto prepared_comparison_order(
		std::size_t                aCount,
		std::optional<std::size_t> aCursor,
		std::size_t                aWindow = prepared_comparison_window)
		-> std::vector<std::size_t>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_COMPARISON_PREPARATION_HPP
