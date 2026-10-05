#ifndef BPR_TESTS_WORKSPACE_CONTRACT_WINDOWS_JUNCTION_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_WINDOWS_JUNCTION_HPP

/// \file
/// \brief [[BPR-1284]]. Makes a directory junction for the workspace contract
/// rows. Windows only: a junction is an `IO_REPARSE_TAG_MOUNT_POINT` reparse
/// point, which an ordinary account may set on an empty directory it owns,
/// with no symbolic-link privilege.

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <winioctl.h>

	#include <cstddef>
	#include <cstdint>
	#include <cstring>
	#include <filesystem>
	#include <string>
	#include <system_error>
	#include <vector>

namespace bpr::tests::workspace_contract {
	/// \brief Makes `aLink` a new directory junction to the directory `aTarget`.
	/// \returns False, leaving nothing at `aLink`, when this account or volume
	/// cannot make one.
	[[nodiscard]] inline auto try_junction(const std::filesystem::path& aTarget, const std::filesystem::path& aLink) -> bool
	{
		auto error = std::error_code{ };
		if (!std::filesystem::create_directory(aLink, error) || error) {
			return false;
		}
		const auto handle = CreateFileW(aLink.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (handle == INVALID_HANDLE_VALUE) {
			std::filesystem::remove(aLink, error);
			return false;
		}
		// The mount-point arm of REPARSE_DATA_BUFFER, which user-mode headers
		// leave out: tag, data length, reserved, then four name offsets and
		// lengths in bytes, then the NT substitute name and the print name,
		// each terminated.
		const auto target     = std::filesystem::absolute(aTarget).wstring();
		const auto substitute = std::wstring{ L"\\??\\" } + target;
		const auto names      = (substitute.size() + 1U + target.size() + 1U) * sizeof(wchar_t);
		auto       buffer     = std::vector<std::uint8_t>(8U + 8U + names);
		const auto put16      = [&buffer](const std::size_t aAt, const std::size_t aValue) {
			const auto value = static_cast<std::uint16_t>(aValue);
			std::memcpy(buffer.data() + aAt, &value, sizeof(value));
		};
		const auto tag = static_cast<std::uint32_t>(IO_REPARSE_TAG_MOUNT_POINT);
		std::memcpy(buffer.data(), &tag, sizeof(tag));
		put16(4U, 8U + names);
		put16(8U, 0U);
		put16(10U, substitute.size() * sizeof(wchar_t));
		put16(12U, (substitute.size() + 1U) * sizeof(wchar_t));
		put16(14U, target.size() * sizeof(wchar_t));
		std::memcpy(buffer.data() + 16U, substitute.c_str(), (substitute.size() + 1U) * sizeof(wchar_t));
		std::memcpy(buffer.data() + 16U + ((substitute.size() + 1U) * sizeof(wchar_t)), target.c_str(), (target.size() + 1U) * sizeof(wchar_t));
		auto       returned = DWORD{ 0 };
		const auto made     = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr) != FALSE;
		CloseHandle(handle);
		if (!made) {
			std::filesystem::remove(aLink, error);
		}
		return made;
	}
}
#endif

#endif
