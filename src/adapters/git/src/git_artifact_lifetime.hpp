#ifndef BPR_SRC_ADAPTERS_GIT_SRC_GIT_ARTIFACT_LIFETIME_HPP
#define BPR_SRC_ADAPTERS_GIT_SRC_GIT_ARTIFACT_LIFETIME_HPP

#include <cstdint>
#include <filesystem>
#include <optional>

#if defined(__linux__)
	#include <fcntl.h>
	#include <linux/fs.h>
	#include <sys/ioctl.h>
#endif

namespace bpr::adapters::git::detail {
#if defined(__linux__)
	// Type and owner checks follow on the retained descriptor. Nonblocking is
	// essential: a concurrently substituted FIFO must not wait for a writer.
	[[nodiscard]] inline auto open_observed_file(const std::filesystem::path& aPath) noexcept -> int
	{
		return ::open(aPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
	}
#endif

	// Native inode lifetime, not ctime or the content-change version. Query
	// failure is unavailable evidence; generation zero is a valid result.
	[[nodiscard]] inline auto file_generation(const int aDescriptor) noexcept -> std::optional<std::uint32_t>
	{
#if defined(__linux__)
		long generation = 0;
		if (::ioctl(aDescriptor, FS_IOC_GETVERSION, &generation) != 0) {
			return std::nullopt;
		}
		return static_cast<std::uint32_t>(generation);
#else
		static_cast<void>(aDescriptor);
		return std::nullopt;
#endif
	}
}

#endif // BPR_SRC_ADAPTERS_GIT_SRC_GIT_ARTIFACT_LIFETIME_HPP
