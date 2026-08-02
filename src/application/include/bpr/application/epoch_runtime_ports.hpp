#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_RUNTIME_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_RUNTIME_PORTS_HPP

#include "bpr/core/epoch_model.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace bpr::application {
	using epoch_entropy_bytes = std::array<std::uint8_t, 32U>;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const epoch_entropy_bytes& aBytes) noexcept -> bool
	{
		for (const auto byte : aBytes) {
			if (byte != 0U) {
				return true;
			}
		}
		return false;
	}

	/// \brief Provides the epoch entropy port contract.
	class epoch_entropy_port
	{
	public:
		virtual ~epoch_entropy_port() = default;

		// Each success is one fresh operating-system entropy observation. The
		// host rejects zero and retries catalog-reported identity collisions.
		[[nodiscard]] virtual auto generate() noexcept
			-> std::optional<epoch_entropy_bytes> = 0;
	};

	/// \brief Provides the epoch clock port contract.
	class epoch_clock_port
	{
	public:
		virtual ~epoch_clock_port() = default;

		[[nodiscard]] virtual auto observe_utc() noexcept
			-> std::optional<core::observed_utc_milliseconds> = 0;
	};

	// The returned ports have no ambient configuration. Entropy comes directly
	// from the operating-system CSPRNG and time comes from the system UTC clock.
	/// \brief Creates the system epoch entropy.
	[[nodiscard]] auto make_system_epoch_entropy()
		-> std::unique_ptr<epoch_entropy_port>;
	/// \brief Creates the system epoch clock.
	[[nodiscard]] auto make_system_epoch_clock()
		-> std::unique_ptr<epoch_clock_port>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_RUNTIME_PORTS_HPP
