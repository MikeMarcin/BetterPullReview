#include "bpr/core/sha256.hpp"

#include <exception>

namespace bpr::core {
	auto sha256_stream::start_platform() noexcept -> bool { return false; }
	void sha256_stream::copy_platform(const sha256_stream&) noexcept { }
	void sha256_stream::destroy_platform() noexcept { }
	void sha256_stream::update_platform(std::span<const std::uint8_t>) noexcept { std::terminate(); }
	auto sha256_stream::finish_platform() noexcept -> std::array<std::uint8_t, 32> { std::terminate(); }
}
