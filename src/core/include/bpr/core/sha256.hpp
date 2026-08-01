#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace bpr::core
{
[[nodiscard]] auto sha256(std::span<const std::uint8_t> bytes) noexcept
    -> std::array<std::uint8_t, 32>;
}
