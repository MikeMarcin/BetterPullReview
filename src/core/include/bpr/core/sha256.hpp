#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// \file
/// \brief SHA-256, computed with HACL*'s generated, formally verified SHA-2
/// (RFC-BPR-0080, [[BPR-1451]]). Every SHA-256 the core computes, Git
/// identities in the SHA-256 object format included, goes through here.
namespace bpr::core {
	/// \brief Computes the sha256 result.
	[[nodiscard]] auto sha256(std::span<const std::uint8_t> aBytes) noexcept
		-> std::array<std::uint8_t, 32>;

	/// \brief SHA-256 over bytes delivered in pieces, so a large input is
	/// hashed chunk by chunk without being held whole.
	/// \details It holds the eight state words, one partial block and the
	/// byte count, allocates nothing, and may be copied.
	class sha256_stream
	{
	public:
		sha256_stream() noexcept;
		void update(std::span<const std::uint8_t> aBytes) noexcept;
		/// \brief The digest of everything delivered. Call once.
		[[nodiscard]] auto finish() noexcept -> std::array<std::uint8_t, 32>;

	private:
		std::array<std::uint32_t, 8> State{ };
		std::array<std::uint8_t, 64> Block{ };
		std::size_t                  BlockSize = 0U;
		std::uint64_t                ByteCount = 0U;
	};
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
