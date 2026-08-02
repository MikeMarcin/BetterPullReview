#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP

#include <array>
#include <cstdint>
#include <span>

namespace bpr::core {
	/// \brief Computes the sha256 result.
	[[nodiscard]] auto sha256(std::span<const std::uint8_t> aBytes) noexcept
		-> std::array<std::uint8_t, 32>;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
