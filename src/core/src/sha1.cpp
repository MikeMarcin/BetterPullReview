#include "bpr/core/sha1.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace bpr::core {
	void sha1_stream::update(const std::span<const std::uint8_t> aBytes) noexcept
	{
		ByteCount += aBytes.size();
		for (const auto byte : aBytes) {
			push(byte);
		}
	}

	auto sha1_stream::finish() noexcept -> std::array<std::uint8_t, 20>
	{
		const auto bitCount = ByteCount * 8U;
		push(0x80U);
		while (BlockSize != 56U) {
			push(0U);
		}
		for (std::size_t shift = 8U; shift != 0U; --shift) {
			push(static_cast<std::uint8_t>(bitCount >> ((shift - 1U) * 8U)));
		}
		std::array<std::uint8_t, 20> result{ };
		for (std::size_t index = 0U; index < State.size(); ++index) {
			result[index * 4U]      = static_cast<std::uint8_t>(State[index] >> 24U);
			result[index * 4U + 1U] = static_cast<std::uint8_t>(State[index] >> 16U);
			result[index * 4U + 2U] = static_cast<std::uint8_t>(State[index] >> 8U);
			result[index * 4U + 3U] = static_cast<std::uint8_t>(State[index]);
		}
		return result;
	}

	void sha1_stream::push(const std::uint8_t aValue) noexcept
	{
		Block[BlockSize++] = aValue;
		if (BlockSize == Block.size()) {
			transform();
			BlockSize = 0U;
		}
	}

	void sha1_stream::transform() noexcept
	{
		std::array<std::uint32_t, 80> words{ };
		for (std::size_t index = 0U; index < 16U; ++index) {
			words[index] = (static_cast<std::uint32_t>(Block[index * 4U]) << 24U) | (static_cast<std::uint32_t>(Block[index * 4U + 1U]) << 16U)
				| (static_cast<std::uint32_t>(Block[index * 4U + 2U]) << 8U) | static_cast<std::uint32_t>(Block[index * 4U + 3U]);
		}
		for (std::size_t index = 16U; index < words.size(); ++index) {
			words[index] = std::rotl(words[index - 3U] ^ words[index - 8U] ^ words[index - 14U] ^ words[index - 16U], 1);
		}
		auto a = State[0];
		auto b = State[1];
		auto c = State[2];
		auto d = State[3];
		auto e = State[4];
		for (std::size_t index = 0U; index < words.size(); ++index) {
			std::uint32_t mix      = 0U;
			std::uint32_t constant = 0U;
			if (index < 20U) {
				mix      = (b & c) | (~b & d);
				constant = 0x5A827999U;
			} else if (index < 40U) {
				mix      = b ^ c ^ d;
				constant = 0x6ED9EBA1U;
			} else if (index < 60U) {
				mix      = (b & c) | (b & d) | (c & d);
				constant = 0x8F1BBCDCU;
			} else {
				mix      = b ^ c ^ d;
				constant = 0xCA62C1D6U;
			}
			const auto next = std::rotl(a, 5) + mix + e + constant + words[index];
			e               = d;
			d               = c;
			c               = std::rotl(b, 30);
			b               = a;
			a               = next;
		}
		State[0] += a;
		State[1] += b;
		State[2] += c;
		State[3] += d;
		State[4] += e;
	}

	auto sha1(const std::span<const std::uint8_t> aBytes) noexcept -> std::array<std::uint8_t, 20>
	{
		auto stream = sha1_stream{ };
		stream.update(aBytes);
		return stream.finish();
	}
}
