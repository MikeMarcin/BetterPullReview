#include "bpr/application/epoch_runtime_ports.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
	#include <bcrypt.h>
#elif defined(__APPLE__)
	#include <cerrno>
	#include <sys/random.h>
#elif defined(__linux__)
	#include <cerrno>
	#include <sys/random.h>
#else
	#error "BetterPullReview system epoch ports require macOS, Linux, or Windows"
#endif

namespace bpr::application {
	namespace {
		[[nodiscard]] auto fill_entropy(epoch_entropy_bytes& aBytes) noexcept -> bool
		{
#if defined(_WIN32)
			static_assert(
				epoch_entropy_bytes{ }.size()
				<= static_cast<std::size_t>(std::numeric_limits<ULONG>::max()));
			return ::BCryptGenRandom(
					   nullptr,
					   aBytes.data(),
					   static_cast<ULONG>(aBytes.size()),
					   BCRYPT_USE_SYSTEM_PREFERRED_RNG)
				== 0;
#elif defined(__APPLE__)
			for (;;) {
				if (::getentropy(aBytes.data(), aBytes.size()) == 0) {
					return true;
				}
				if (errno != EINTR) {
					return false;
				}
			}
#elif defined(__linux__)
			std::size_t offset = 0U;
			while (offset < aBytes.size()) {
				const auto count = ::getrandom(
					aBytes.data() + offset, aBytes.size() - offset, 0U);
				if (count > 0) {
					offset += static_cast<std::size_t>(count);
					continue;
				}
				if (count < 0 && errno == EINTR) {
					continue;
				}
				return false;
			}
			return true;
#endif
		}

		class system_epoch_entropy final : public epoch_entropy_port
		{
		public:
			[[nodiscard]] auto generate() noexcept
				-> std::optional<epoch_entropy_bytes> override
			{
				auto bytes = epoch_entropy_bytes{ };
				if (!fill_entropy(bytes) || !valid(bytes)) {
					return std::nullopt;
				}
				return bytes;
			}
		};

		class system_epoch_clock final : public epoch_clock_port
		{
		public:
			[[nodiscard]] auto observe_utc() noexcept
				-> std::optional<core::observed_utc_milliseconds> override
			{
				const auto elapsed = std::chrono::duration_cast<
					std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch());
				const auto count = elapsed.count();
				if (count < 0
					|| count > core::maximum_observed_utc_milliseconds) {
					return std::nullopt;
				}
				return core::observed_utc_milliseconds{
					static_cast<std::int64_t>(count)
				};
			}
		};
	} // namespace

	auto make_system_epoch_entropy() -> std::unique_ptr<epoch_entropy_port>
	{
		return std::make_unique<system_epoch_entropy>();
	}

	auto make_system_epoch_clock() -> std::unique_ptr<epoch_clock_port>
	{
		return std::make_unique<system_epoch_clock>();
	}
} // namespace bpr::application
