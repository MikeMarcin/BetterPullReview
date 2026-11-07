#include "github_application/defect_identity.hpp"

#include "bpr/core/sha256.hpp"

#include <span>

namespace bpr::app {
	auto durable_identity_half(
		const std::array<std::uint8_t, 32>& aDigest,
		const std::size_t                   aOffset) noexcept -> std::uint64_t
	{
		auto result = std::uint64_t{ 0U };
		for (auto index = std::size_t{ 0U }; index < 8U; ++index) {
			result = (result << 8U) | aDigest[aOffset + index];
		}
		result &= std::uint64_t{ 0x7fff'ffff'ffff'ffffULL };
		return result == 0U ? 1U : result;
	}

	auto defect_identity(
		const std::string_view aIdentity) noexcept -> local_defect_identity
	{
		const auto bytes = std::span{
			reinterpret_cast<const std::uint8_t*>(aIdentity.data()),
			aIdentity.size(),
		};
		const auto digest = core::sha256(bytes);
		return {
			.Conversation = durable_identity_half(digest, 0U),
			.Defect       = durable_identity_half(digest, 8U),
		};
	}
}
