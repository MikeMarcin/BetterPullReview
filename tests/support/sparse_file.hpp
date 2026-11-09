#ifndef BPR_TESTS_SUPPORT_SPARSE_FILE_HPP
#define BPR_TESTS_SUPPORT_SPARSE_FILE_HPP

#include <cstdint>
#include <filesystem>
#include <limits>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <winioctl.h>
#else
	#include <fcntl.h>
	#include <unistd.h>
#endif

namespace bpr::tests {
	/// Creates only a new test-owned sparse file, never allocating its logical
	/// length on disk. The caller owns its private parent and exact cleanup.
	[[nodiscard]] inline auto create_sparse_file(const std::filesystem::path& aPath, const std::uint64_t aBytes) -> bool
	{
#if defined(_WIN32)
		if (aBytes > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max())) {
			return false;
		}
		const auto file = ::CreateFileW(aPath.c_str(), GENERIC_WRITE, 0U, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) {
			return false;
		}
		auto returned   = DWORD{ 0U };
		auto length     = LARGE_INTEGER{ };
		length.QuadPart = static_cast<LONGLONG>(aBytes);
		const auto made = ::DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0U, nullptr, 0U, &returned, nullptr) != FALSE
			&& ::SetFilePointerEx(file, length, nullptr, FILE_BEGIN) != FALSE && ::SetEndOfFile(file) != FALSE;
		static_cast<void>(::CloseHandle(file));
		return made;
#else
		if (aBytes > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
			return false;
		}
		const auto file = ::open(aPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
		if (file < 0) {
			return false;
		}
		const auto made = ::ftruncate(file, static_cast<off_t>(aBytes)) == 0;
		static_cast<void>(::close(file));
		return made;
#endif
	}
}

#endif
