#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace bpr::core {
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
			transform(Block);
			BlockSize = 0U;
		}
		while (rest.size() >= Block.size()) {
			transform(rest.first<64>());
			rest = rest.subspan(Block.size());
		}
		std::copy(rest.begin(), rest.end(), Block.begin());
		BlockSize = rest.size();
	}

	auto sha256_stream::finish() noexcept -> std::array<std::uint8_t, 32>
	{
		const auto bitCount = ByteCount * 8U;
		push(0x80U);
		while (BlockSize != 56U) {
			push(0U);
		}
		for (std::size_t shift = 8U; shift != 0U; --shift) {
			push(static_cast<std::uint8_t>(bitCount >> ((shift - 1U) * 8U)));
		}
		std::array<std::uint8_t, 32> result{ };
		for (std::size_t index = 0U; index < State.size(); ++index) {
			result[index * 4U]      = static_cast<std::uint8_t>(State[index] >> 24U);
			result[index * 4U + 1U] = static_cast<std::uint8_t>(State[index] >> 16U);
			result[index * 4U + 2U] = static_cast<std::uint8_t>(State[index] >> 8U);
			result[index * 4U + 3U] = static_cast<std::uint8_t>(State[index]);
		}
		return result;
	}

	void sha256_stream::push(const std::uint8_t aValue) noexcept
	{
		Block[BlockSize++] = aValue;
		if (BlockSize == Block.size()) {
			transform(Block);
			BlockSize = 0U;
		}
	}

	void sha256_stream::transform(const std::span<const std::uint8_t, 64> aBlock) noexcept
	{
		static constexpr std::array<std::uint32_t, 64> constants{
			0x428a2f98U,
			0x71374491U,
			0xb5c0fbcfU,
			0xe9b5dba5U,
			0x3956c25bU,
			0x59f111f1U,
			0x923f82a4U,
			0xab1c5ed5U,
			0xd807aa98U,
			0x12835b01U,
			0x243185beU,
			0x550c7dc3U,
			0x72be5d74U,
			0x80deb1feU,
			0x9bdc06a7U,
			0xc19bf174U,
			0xe49b69c1U,
			0xefbe4786U,
			0x0fc19dc6U,
			0x240ca1ccU,
			0x2de92c6fU,
			0x4a7484aaU,
			0x5cb0a9dcU,
			0x76f988daU,
			0x983e5152U,
			0xa831c66dU,
			0xb00327c8U,
			0xbf597fc7U,
			0xc6e00bf3U,
			0xd5a79147U,
			0x06ca6351U,
			0x14292967U,
			0x27b70a85U,
			0x2e1b2138U,
			0x4d2c6dfcU,
			0x53380d13U,
			0x650a7354U,
			0x766a0abbU,
			0x81c2c92eU,
			0x92722c85U,
			0xa2bfe8a1U,
			0xa81a664bU,
			0xc24b8b70U,
			0xc76c51a3U,
			0xd192e819U,
			0xd6990624U,
			0xf40e3585U,
			0x106aa070U,
			0x19a4c116U,
			0x1e376c08U,
			0x2748774cU,
			0x34b0bcb5U,
			0x391c0cb3U,
			0x4ed8aa4aU,
			0x5b9cca4fU,
			0x682e6ff3U,
			0x748f82eeU,
			0x78a5636fU,
			0x84c87814U,
			0x8cc70208U,
			0x90befffaU,
			0xa4506cebU,
			0xbef9a3f7U,
			0xc67178f2U,
		};
		std::array<std::uint32_t, 64> words{ };
		for (std::size_t index = 0U; index < 16U; ++index) {
			const auto offset = index * 4U;
			words[index]      = static_cast<std::uint32_t>(aBlock[offset]) << 24U
				| static_cast<std::uint32_t>(aBlock[offset + 1U]) << 16U
				| static_cast<std::uint32_t>(aBlock[offset + 2U]) << 8U
				| static_cast<std::uint32_t>(aBlock[offset + 3U]);
		}
		for (std::size_t index = 16U; index < words.size(); ++index) {
			const auto s0 = std::rotr(words[index - 15U], 7)
				^ std::rotr(words[index - 15U], 18)
				^ (words[index - 15U] >> 3U);
			const auto s1 = std::rotr(words[index - 2U], 17)
				^ std::rotr(words[index - 2U], 19)
				^ (words[index - 2U] >> 10U);
			words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
		}

		auto a = State[0];
		auto b = State[1];
		auto c = State[2];
		auto d = State[3];
		auto e = State[4];
		auto f = State[5];
		auto g = State[6];
		auto h = State[7];
		for (std::size_t index = 0U; index < words.size(); ++index) {
			const auto sum1     = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
			const auto choice   = (e & f) ^ (~e & g);
			const auto first    = h + sum1 + choice + constants[index] + words[index];
			const auto sum0     = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
			const auto majority = (a & b) ^ (a & c) ^ (b & c);
			const auto second   = sum0 + majority;
			h                   = g;
			g                   = f;
			f                   = e;
			e                   = d + first;
			d                   = c;
			c                   = b;
			b                   = a;
			a                   = first + second;
		}
		State[0] += a;
		State[1] += b;
		State[2] += c;
		State[3] += d;
		State[4] += e;
		State[5] += f;
		State[6] += g;
		State[7] += h;
	}

	auto sha256(const std::span<const std::uint8_t> aBytes) noexcept
		-> std::array<std::uint8_t, 32>
	{
		auto digest = sha256_stream{ };
		digest.update(aBytes);
		return digest.finish();
	}
}
