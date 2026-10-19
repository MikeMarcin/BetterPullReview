#include "bpr/core/sha256.hpp"

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// CNG declarations require the Windows types above.
#include <bcrypt.h>

#include <algorithm>
#include <exception>
#include <limits>

namespace bpr::core {
	auto sha256_stream::start_platform() noexcept -> bool
	{
		// Windows 10+ pseudo-handle: no mutable provider cache or provider
		// discovery. CNG owns the per-stream object's allocation and release.
		BCRYPT_HASH_HANDLE handle = nullptr;
		if (BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &handle, nullptr, 0, nullptr, 0, 0) < 0) {
			return false;
		}
		PlatformHandle = handle;
		return true;
	}

	void sha256_stream::copy_platform(const sha256_stream& aOther) noexcept
	{
		BCRYPT_HASH_HANDLE handle = nullptr;
		if (BCryptDuplicateHash(aOther.PlatformHandle, &handle, nullptr, 0, 0) < 0) {
			std::terminate();
		}
		PlatformHandle = handle;
	}

	void sha256_stream::destroy_platform() noexcept
	{
		(void)BCryptDestroyHash(PlatformHandle);
	}

	void sha256_stream::update_platform(std::span<const std::uint8_t> aBytes) noexcept
	{
		while (!aBytes.empty()) {
			const auto take = std::min(aBytes.size(), static_cast<std::size_t>((std::numeric_limits<ULONG>::max)()));
			// CNG's historical mutable pointer is input-only for HashData.
			if (BCryptHashData(PlatformHandle, const_cast<PUCHAR>(aBytes.data()), static_cast<ULONG>(take), 0) < 0) {
				std::terminate();
			}
			aBytes = aBytes.subspan(take);
		}
	}

	auto sha256_stream::finish_platform() noexcept -> std::array<std::uint8_t, 32>
	{
		std::array<std::uint8_t, 32> result{ };
		if (BCryptFinishHash(PlatformHandle, result.data(), static_cast<ULONG>(result.size()), 0) < 0) {
			std::terminate();
		}
		return result;
	}
}
