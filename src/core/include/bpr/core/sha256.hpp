#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bpr::core {
	/// \brief Computes the sha256 result.
	[[nodiscard]] auto sha256(std::span<const std::uint8_t> aBytes) noexcept
		-> std::array<std::uint8_t, 32>;

	/// \brief SHA-256 over bytes delivered in pieces, so a large input is
	/// hashed chunk by chunk without being held whole.
	class sha256_stream
	{
	public:
		void update(std::span<const std::uint8_t> aBytes) noexcept;
		/// \brief The digest of everything delivered. Call once.
		[[nodiscard]] auto finish() noexcept -> std::array<std::uint8_t, 32>;

	private:
		void push(std::uint8_t aValue) noexcept;
		void transform() noexcept;

		std::array<std::uint32_t, 8> State{
			0x6a09e667U,
			0xbb67ae85U,
			0x3c6ef372U,
			0xa54ff53aU,
			0x510e527fU,
			0x9b05688cU,
			0x1f83d9abU,
			0x5be0cd19U,
		};
		std::array<std::uint8_t, 64> Block{ };
		std::size_t                  BlockSize = 0U;
		std::uint64_t                ByteCount = 0U;
	};
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
