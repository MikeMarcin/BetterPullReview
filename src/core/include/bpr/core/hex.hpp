#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_HEX_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_HEX_HPP

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

/// \file
/// \brief Plain lowercase hexadecimal of bytes: two digits per byte, high
/// nibble first, nothing else.
/// \details [[BPR-1439]]. The one rendering of digests, identities and random
/// names for core and for the Qt-free targets that link it (application, the
/// SQLite and MCP adapters). Qt targets use Qt's own byte-array hex
/// conversion. Integer, pkt-line length, percent and JSON `\u` escapes are
/// different renderings and do not use this.
namespace bpr::core {
	/// \brief Appends two lowercase digits per byte of `aBytes` to `aOut`.
	inline void append_lower_hex(std::string& aOut, const std::span<const std::uint8_t> aBytes)
	{
		constexpr auto digits = std::string_view{ "0123456789abcdef" };
		aOut.reserve(aOut.size() + aBytes.size() * 2U);
		for (const auto byte : aBytes) {
			aOut.push_back(digits[byte >> 4U]);
			aOut.push_back(digits[byte & 0x0FU]);
		}
	}

	/// \brief Two lowercase digits per byte of `aBytes`.
	[[nodiscard]] inline auto lower_hex(const std::span<const std::uint8_t> aBytes) -> std::string
	{
		auto text = std::string{ };
		append_lower_hex(text, aBytes);
		return text;
	}

	/// \brief Two lowercase digits per byte of `aBytes`, read as raw octets.
	[[nodiscard]] inline auto lower_hex(const std::string_view aBytes) -> std::string
	{
		// A `char` and an octet share size and alignment; this only reads.
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		return lower_hex(std::span{ reinterpret_cast<const std::uint8_t*>(aBytes.data()), aBytes.size() });
	}
}

#endif
