#include "bpr/core/sha256.hpp"

#include <CommonCrypto/CommonDigest.h>

#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <type_traits>

namespace bpr::core {
	static_assert(std::is_trivially_copyable_v<CC_SHA256_CTX>);
	static_assert(CC_SHA256_DIGEST_LENGTH == 32);

	auto sha256_stream::start_platform() noexcept -> bool
	{
		static_assert(sizeof(CC_SHA256_CTX) <= sizeof(PlatformState));
		CC_SHA256_CTX context{ };
		if (CC_SHA256_Init(&context) != 1) {
			return false;
		}
		std::memcpy(PlatformState.data(), &context, sizeof(context));
		return true;
	}

	void sha256_stream::copy_platform(const sha256_stream&) noexcept
	{
		// The enclosing copy constructor already copied the context bytes.
	}

	void sha256_stream::destroy_platform() noexcept { }

	void sha256_stream::update_platform(std::span<const std::uint8_t> aBytes) noexcept
	{
		CC_SHA256_CTX context{ };
		std::memcpy(&context, PlatformState.data(), sizeof(context));
		while (!aBytes.empty()) {
			const auto take = std::min(aBytes.size(), static_cast<std::size_t>(std::numeric_limits<CC_LONG>::max()));
			if (CC_SHA256_Update(&context, aBytes.data(), static_cast<CC_LONG>(take)) != 1) {
				std::terminate();
			}
			aBytes = aBytes.subspan(take);
		}
		std::memcpy(PlatformState.data(), &context, sizeof(context));
	}

	auto sha256_stream::finish_platform() noexcept -> std::array<std::uint8_t, 32>
	{
		CC_SHA256_CTX context{ };
		std::memcpy(&context, PlatformState.data(), sizeof(context));
		std::array<std::uint8_t, 32> result{ };
		if (CC_SHA256_Final(result.data(), &context) != 1) {
			std::terminate();
		}
		return result;
	}
}
