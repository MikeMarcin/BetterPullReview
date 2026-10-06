#ifndef BPR_SRC_PLATFORM_INCLUDE_BPR_PLATFORM_WINDOWS_SHARING_RETRY_HPP
#define BPR_SRC_PLATFORM_INCLUDE_BPR_PLATFORM_WINDOWS_SHARING_RETRY_HPP

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>

	#include <array>
	#include <cstddef>

namespace bpr::platform {
	/// \brief Which failures `retry_while_held` treats as a brief hold.
	enum class held_errors : unsigned char {
		/// Sharing and lock violations only.
		sharing_and_lock,
		/// Those and access denied, which a file with a pending scan can also
		/// answer. A caller whose access denied can be lasting (a read-only
		/// destination) uses `sharing_and_lock`.
		sharing_lock_and_access_denied,
	};

	/// \brief Repeats a file operation while another handle briefly holds the file.
	///
	/// \details [[BPR-1040]]. A file written a moment ago can still be open
	/// elsewhere. On the Windows station, three operations on a just-written
	/// Git pack spool failed that way: publishing the receiving file, opening
	/// the completed spool exclusively, and renaming it away for removal. The
	/// rename failed with `ERROR_SHARING_VIOLATION`, no user-mode process held
	/// the file (the Restart Manager listed none), and the same rename
	/// succeeded 50 ms later. A real-time scanner that reads each new file is
	/// the usual holder. [[BPR-1104]] found a fourth: deleting the spool once it
	/// is renamed away. [[BPR-1239]] moved this here from the Git adapter so
	/// Save to file shares it without an adapter-to-adapter edge.
	///
	/// `aAttempt` performs the operation once and returns `ERROR_SUCCESS` or the
	/// Windows error. Only the errors `aHeld` names are repeated. The retry is
	/// bounded: eight waits from 5 ms, doubling, about 1.3 s in all. After that
	/// the last error is returned, so the caller fails exactly as it would
	/// have without the retry.
	template <typename Attempt>
	[[nodiscard]] auto retry_while_held(const held_errors aHeld, Attempt&& aAttempt) -> DWORD
	{
		static constexpr auto delays = std::array<DWORD, 8U>{ 5U, 10U, 20U, 40U, 80U, 160U, 320U, 640U };
		for (std::size_t attempt = 0U;; ++attempt) {
			const auto error = aAttempt();
			const auto held  = error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION
				|| (aHeld == held_errors::sharing_lock_and_access_denied && error == ERROR_ACCESS_DENIED);
			if (error == ERROR_SUCCESS || !held || attempt == delays.size()) {
				return error;
			}
			::Sleep(delays[attempt]);
		}
	}
} // namespace bpr::platform
#endif

#endif
