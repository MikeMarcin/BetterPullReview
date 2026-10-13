#include "bpr/core/sha256.hpp"

extern "C" {
#include <internal/Hacl_Hash_SHA2.h>
}

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// \file
/// \brief [[BPR-1451]]. The core SHA-256 over HACL*'s generated SHA-2
/// (cmake/BprHaclSha2Provider.cmake). Only the block-level calls are used, so
/// a stream allocates nothing; HACL*'s streaming API, which allocates, is
/// never called. The first-party SHA-256 it replaces is gone.
namespace bpr::core {
	namespace {
		constexpr std::size_t block_bytes = 64U;
		// Hacl_Hash_SHA2_sha256_update_nblocks takes a 32-bit length, so whole
		// blocks are handed over at most a gibibyte at a time.
		constexpr std::size_t most_per_call = std::size_t{ 1 } << 30U;
		static_assert(most_per_call % block_bytes == 0U);
	}

	auto sha256(const std::span<const std::uint8_t> aBytes) noexcept -> std::array<std::uint8_t, 32>
	{
		auto stream = sha256_stream{ };
		stream.update(aBytes);
		return stream.finish();
	}

	sha256_stream::sha256_stream() noexcept
	{
		Hacl_Hash_SHA2_sha256_init(State.data());
	}

	void sha256_stream::update(const std::span<const std::uint8_t> aBytes) noexcept
	{
		ByteCount += aBytes.size();
		auto rest = aBytes;
		// Top up a partly filled block first, then hash whole blocks straight
		// from the input, and keep only the tail ([[BPR-1358]]).
		if (BlockSize != 0U) {
			const auto take = std::min(Block.size() - BlockSize, rest.size());
			std::copy_n(rest.begin(), take, Block.begin() + static_cast<std::ptrdiff_t>(BlockSize));
			BlockSize += take;
			rest = rest.subspan(take);
			if (BlockSize != Block.size()) {
				return;
			}
			Hacl_Hash_SHA2_sha256_update_nblocks(static_cast<std::uint32_t>(block_bytes), Block.data(), State.data());
			BlockSize = 0U;
		}
		while (rest.size() >= block_bytes) {
			const auto whole = std::min(rest.size() - (rest.size() % block_bytes), most_per_call);
			// HACL* takes the input through a non-const pointer but only reads
			// it, so the caller's const bytes are never written.
			Hacl_Hash_SHA2_sha256_update_nblocks(static_cast<std::uint32_t>(whole), const_cast<std::uint8_t*>(rest.data()), State.data());
			rest = rest.subspan(whole);
		}
		std::ranges::copy(rest, Block.begin());
		BlockSize = rest.size();
	}

	auto sha256_stream::finish() noexcept -> std::array<std::uint8_t, 32>
	{
		// The padding and the bit length come from the total byte count and
		// the fewer-than-64 bytes still held.
		Hacl_Hash_SHA2_sha256_update_last(ByteCount, static_cast<std::uint32_t>(BlockSize), Block.data(), State.data());
		std::array<std::uint8_t, 32> result{ };
		Hacl_Hash_SHA2_sha256_finish(State.data(), result.data());
		return result;
	}
}
