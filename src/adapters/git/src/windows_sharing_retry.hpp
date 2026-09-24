#ifndef BPR_SRC_ADAPTERS_GIT_SRC_WINDOWS_SHARING_RETRY_HPP
#define BPR_SRC_ADAPTERS_GIT_SRC_WINDOWS_SHARING_RETRY_HPP

#include <QtGlobal>

#if defined(Q_OS_WIN)
	#include <qt_windows.h>

	#include <array>
	#include <cstddef>

namespace bpr::adapters::git::detail {
	/// \brief Repeats a file operation while another handle briefly holds the file.
	///
	/// \details [[BPR-1040]]. A file this adapter wrote a moment ago can still
	/// be open elsewhere. On the Windows station, three operations on a
	/// just-written pack spool failed that way: publishing the receiving file,
	/// opening the completed spool exclusively to feed Git, and renaming it away
	/// for removal. The rename failed with `ERROR_SHARING_VIOLATION`, no
	/// user-mode process held the file (the Restart Manager listed none), and
	/// the same rename succeeded 50 ms later. A real-time scanner that reads
	/// each new file is the usual holder. [[BPR-1104]] found a fourth: deleting
	/// the spool once it is renamed away, which failed and succeeded the same
	/// way. It also found a holder this retry cannot outlast: a Git child that
	/// inherited a stream on the spool, now prevented at the launch
	/// (`standard_handles_only` in installed_git_process.cpp).
	///
	/// `aAttempt` performs the operation once and returns `ERROR_SUCCESS` or the
	/// Windows error. Only the errors such a holder produces are repeated:
	/// sharing and lock violations, and access denied, which a file with a
	/// pending scan can also answer. The retry is bounded: eight waits from
	/// 5 ms, doubling, about 1.3 s in all. After that the last error is
	/// returned, so the caller fails exactly as it did before.
	template <typename Attempt>
	[[nodiscard]] auto retry_while_held(Attempt&& aAttempt) -> DWORD
	{
		static constexpr auto delays = std::array<DWORD, 8U>{ 5U, 10U, 20U, 40U, 80U, 160U, 320U, 640U };
		for (std::size_t attempt = 0U;; ++attempt) {
			const auto error = aAttempt();
			const auto held  = error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION
				|| error == ERROR_ACCESS_DENIED;
			if (error == ERROR_SUCCESS || !held || attempt == delays.size()) {
				return error;
			}
			::Sleep(delays[attempt]);
		}
	}
} // namespace bpr::adapters::git::detail
#endif

#endif
