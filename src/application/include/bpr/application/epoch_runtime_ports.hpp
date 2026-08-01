#pragma once

#include "bpr/core/epoch_model.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace bpr::application
{
using epoch_entropy_bytes = std::array<std::uint8_t, 32U>;

[[nodiscard]] constexpr auto valid(
    const epoch_entropy_bytes& bytes) noexcept -> bool
{
    for(const auto byte : bytes)
    {
        if(byte != 0U)
        {
            return true;
        }
    }
    return false;
}

class epoch_entropy_port
{
public:
    virtual ~epoch_entropy_port() = default;

    // Each success is one fresh operating-system entropy observation. The
    // host rejects zero and retries catalog-reported identity collisions.
    [[nodiscard]] virtual auto generate() noexcept
        -> std::optional<epoch_entropy_bytes> = 0;
};

class epoch_clock_port
{
public:
    virtual ~epoch_clock_port() = default;

    [[nodiscard]] virtual auto observe_utc() noexcept
        -> std::optional<core::observed_utc_milliseconds> = 0;
};

// The returned ports have no ambient configuration. Entropy comes directly
// from the operating-system CSPRNG and time comes from the system UTC clock.
[[nodiscard]] auto make_system_epoch_entropy()
    -> std::unique_ptr<epoch_entropy_port>;
[[nodiscard]] auto make_system_epoch_clock()
    -> std::unique_ptr<epoch_clock_port>;
} // namespace bpr::application
