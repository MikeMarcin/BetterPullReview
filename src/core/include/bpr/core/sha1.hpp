#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA1_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA1_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// \file
/// \brief SHA-1, for Git object identities only (RFC-BPR-0059).
/// \details Git repositories in the SHA-1 object format name every object by
/// this digest, so a publication candidate's blob, tree and commit identities
/// can be computed and checked locally. It is never used as a security digest.
namespace bpr::core {
	/// \brief SHA-1 over bytes delivered in pieces.
	class sha1_stream
	{
	public:
		void update(std::span<const std::uint8_t> aBytes) noexcept;
		/// \brief The digest of everything delivered. Call once.
		[[nodiscard]] auto finish() noexcept -> std::array<std::uint8_t, 20>;

	private:
		void push(std::uint8_t aValue) noexcept;
		void transform() noexcept;

		std::array<std::uint32_t, 5> State{
			0x67452301U,
			0xEFCDAB89U,
			0x98BADCFEU,
			0x10325476U,
			0xC3D2E1F0U,
		};
		std::array<std::uint8_t, 64> Block{ };
		std::size_t                  BlockSize = 0U;
		std::uint64_t                ByteCount = 0U;
	};

	[[nodiscard]] auto sha1(std::span<const std::uint8_t> aBytes) noexcept -> std::array<std::uint8_t, 20>;
}

#endif
