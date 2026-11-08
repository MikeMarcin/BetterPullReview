#ifndef BPR_APP_GITHUB_APPLICATION_DEFECT_IDENTITY_HPP
#define BPR_APP_GITHUB_APPLICATION_DEFECT_IDENTITY_HPP
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string_view>

// The durable identity of a local defect thread.
namespace bpr::app {
	struct local_defect_identity
	{
		std::uint64_t Conversation = 0U;
		std::uint64_t Defect       = 0U;

		auto operator<=>(const local_defect_identity&) const = default;
	};

	[[nodiscard]] auto durable_identity_half(
		const std::array<std::uint8_t, 32>& aDigest,
		const std::size_t                   aOffset) noexcept -> std::uint64_t;

	[[nodiscard]] auto defect_identity(
		const std::string_view aIdentity) noexcept -> local_defect_identity;
}
#endif
