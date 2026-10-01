#ifndef BPR_PLATFORM_UNIX_DESCRIPTOR_SWEEP_HPP
#define BPR_PLATFORM_UNIX_DESCRIPTOR_SWEEP_HPP

// [[BPR-1252]]. Bounds the descriptor sweep a frequent Unix launch makes in
// its child, so the launch costs what the open descriptors cost rather than
// what the descriptor limit allows.
//
// Every Unix launch sets Qt's `CloseFileDescriptors`. Without `close_range`,
// which macOS lacks, Qt's child closes each number from 3 to its soft
// `RLIMIT_NOFILE`, one `close` call each, between fork and exec. A soft limit
// of 1,048,576, as a shell with a raised limit hands down, made every launch
// about a million calls: 77.5 ms against 1.4 ms at 256, measured with Qt
// 6.11.2 on macOS arm64, and more under a full parallel test run. Git and the
// gh CLI launch once per command or request, so a review paid it many times
// over (the setup destination row took 48 s at the raised limit and 14 s at
// 256).
//
// The child calls `bound` from its child-process modifier, which Qt runs
// before it applies the process parameters. It lists the child's own open
// descriptors and lowers the child's soft limit to one past the highest of
// them, but never below `OPEN_MAX`. Qt's sweep then still reaches every open
// descriptor, and stops there. The program the child becomes inherits that
// soft limit: `OPEN_MAX` (10,240), the value `setrlimit(2)` itself
// recommends for a descriptor limit, unless a higher descriptor is open. A
// limit at or below that bound is left as it is.
//
// When the list cannot be read whole, nothing changes and Qt sweeps to the
// limit as before. Elsewhere `bound` does nothing.
//
// Header-only and free of Qt. It decides nothing about what runs.

#if defined(__APPLE__)
	#include <libproc.h>
	#include <sys/proc_info.h>
	#include <sys/resource.h>
	#include <sys/syslimits.h>
	#include <unistd.h>

	#include <algorithm>
	#include <array>
	#include <cstddef>
#endif

namespace bpr::platform::unix_descriptor_sweep {
#if defined(__APPLE__)
	/// \brief How many open descriptors the child can list on its own stack.
	///
	/// \details 16 KiB of stack. A child holding more falls back to the full
	/// sweep rather than to a bound it could not prove.
	inline constexpr std::size_t listed_descriptor_capacity = 2048U;
#endif

	/// \brief Lowers this process's soft descriptor limit to the bound that
	/// still covers every open descriptor.
	///
	/// \details Call only in a child between fork and exec, before its
	/// descriptors are closed: from the child-process modifier of Qt's process
	/// class. Makes four system calls and allocates nothing, so it is safe
	/// after a fork of a threaded process.
	inline void bound() noexcept
	{
#if defined(__APPLE__)
		std::array<proc_fdinfo, listed_descriptor_capacity> listed{ };

		constexpr auto capacity = static_cast<int>(sizeof(listed));
		const auto     bytes    = ::proc_pidinfo(::getpid(), PROC_PIDLISTFDS, 0, listed.data(), capacity);
		// A full buffer may have been cut short, and an unseen descriptor
		// above the bound would survive the sweep.
		if (bytes <= 0 || bytes >= capacity) {
			return;
		}
		const auto count   = static_cast<std::size_t>(bytes) / sizeof(proc_fdinfo);
		auto       highest = STDERR_FILENO;
		for (std::size_t index = 0U; index < count; ++index) {
			highest = std::max(highest, listed[index].proc_fd);
		}
		rlimit limit{ };
		if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) {
			return;
		}
		const auto ceiling = std::max<rlim_t>(static_cast<rlim_t>(highest) + 1U, OPEN_MAX);
		if (ceiling >= limit.rlim_cur) {
			return;
		}
		limit.rlim_cur = ceiling;
		// A refusal leaves the limit, and so the full sweep, as it was.
		static_cast<void>(::setrlimit(RLIMIT_NOFILE, &limit));
#endif
	}
}

#endif
