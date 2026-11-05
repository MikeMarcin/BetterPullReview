#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_COMPARISON_PREPARATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_COMPARISON_PREPARATION_HPP

#include "bpr/core/epoch_model.hpp"

#include <cstddef>
#include <cstdint>
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

	/// \brief [[BPR-1871]]. What the comparison area shows for a file the
	/// reviewer opened that is not on screen yet.
	enum class opened_file_wait : std::uint8_t {
		/// Nothing is waiting: no file is open, or the opened one is shown.
		none,
		/// The file is on its way: the pull request's history or the file's
		/// own comparison is still being read. It opens by itself.
		loading,
		/// The file cannot open: its read failed, or no comparison will come.
		failed,
	};

	/// \brief [[BPR-1871]]. The facts the opened file's wait is decided from.
	struct opened_file_wait_facts
	{
		/// The reviewer has a file open (from Files, a link, a search, a thread).
		bool Opened = false;
		/// Its comparison is on screen.
		bool Shown = false;
		/// Its own read ended in a failure other than a missing comparison.
		bool FileFailed = false;
		/// The host answered that no comparison is ready for it yet.
		bool ComparisonRefused = false;
		/// The review history has an accepted comparison a diff can be read at.
		bool ComparisonAvailable = false;
		/// The review history is resolving a newly chosen comparison.
		bool ComparisonPending = false;
		/// The review history's status.
		epoch_status Epoch = epoch_status::inactive;

		auto operator==(const opened_file_wait_facts&) const -> bool = default;
	};

	/// \brief [[BPR-1871]]. Decides the opened file's wait.
	///
	/// \details The owner, 2026-11-05: a file opened while its pull request
	/// still loaded showed "No file open". A file the reviewer opened is
	/// loading until it is shown or something says it never will be: its own
	/// read failed, or the host refused it for want of a comparison and the
	/// review history has stopped without one (ready with nothing pending,
	/// unavailable, failed or cancelled). While the history is still being
	/// read (inactive, loading its catalogue, preparing, committing,
	/// resolving or maintaining), a refusal only means "not yet".
	[[nodiscard]] auto decide_opened_file_wait(const opened_file_wait_facts& aFacts) noexcept -> opened_file_wait;

	/// \brief [[BPR-1871]]. Whether the reviewer's opening of a file stops a
	/// preparation round already running.
	///
	/// \details [[BPR-1344]] queues preparation behind the reviewer's own
	/// work, but a round already running holds the repository actor until its
	/// current job ends, and a job may be a remote blob batch. A round that
	/// prepares the opened file is kept, since its reads are the file's own; a
	/// round preparing only other files is stopped, so the opened file's reads
	/// come next, and preparation starts again once the reviewer's work is
	/// done.
	[[nodiscard]] auto opening_preempts_preparation(bool aRoundRunning, bool aRoundPreparesOpenedFile) noexcept -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_COMPARISON_PREPARATION_HPP
