#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_RUNTIME_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_RUNTIME_PORTS_HPP

#include "bpr/core/epoch_model.hpp"

#include <array>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace bpr::application {
	inline constexpr std::size_t maximum_epoch_outstanding_jobs = 1'024U;
	inline constexpr std::size_t maximum_epoch_deferred_ingress = 1'024U;
	inline constexpr auto        maximum_epoch_blob_wait        = std::chrono::minutes{ 10 };

	/// \brief Describes the epoch host residency limits.
	struct epoch_host_limits
	{
		std::size_t OutstandingJobs = 64U;
		std::size_t DeferredIngress = 64U;
		// How long a synchronous blob read may wait for the repository actor
		// before it gives up.
		//
		// The bound is a backstop, not a latency target. Work is enqueued behind
		// whatever the actor is already running and epoch maintenance can hold it
		// for most of a minute, so a tight bound would fail legitimate reads. What
		// makes an abandoned read *prompt* is the caller's stop token, which the
		// wait now honours; this is what releases a read whose actor never
		// finishes at all. Before it existed the wait had neither, and a starved
		// classification stayed outstanding until shutdown retired it — which is
		// how a pane rendered monochrome for a whole session. BPR-0390.
		std::chrono::milliseconds BlobWait = std::chrono::seconds{ 90 };

		auto operator<=>(const epoch_host_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const epoch_host_limits& aLimits) noexcept
		-> bool
	{
		return aLimits.OutstandingJobs > 0U
			&& aLimits.OutstandingJobs <= maximum_epoch_outstanding_jobs
			&& aLimits.DeferredIngress > 0U
			&& aLimits.DeferredIngress <= maximum_epoch_deferred_ingress
			// A zero bound would abandon every read before the actor could run,
			// which is worse than the unbounded wait it replaced.
			&& aLimits.BlobWait > std::chrono::milliseconds::zero()
			&& aLimits.BlobWait <= maximum_epoch_blob_wait;
	}

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
