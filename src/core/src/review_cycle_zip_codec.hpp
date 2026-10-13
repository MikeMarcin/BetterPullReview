#ifndef BPR_SRC_CORE_SRC_REVIEW_CYCLE_ZIP_CODEC_HPP
#define BPR_SRC_CORE_SRC_REVIEW_CYCLE_ZIP_CODEC_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/// \file
/// \brief The review bundle's zlib codec, behind a header without zlib.
/// \details [[BPR-1435]]. zlib is built with `Z_PREFIX`, whose macros rename
/// names such as `inflate`; only `review_cycle_zip_codec.cpp` includes it.
namespace bpr::core::zip_codec {
	/// \brief The ZIP checksum (CRC-32/ISO-HDLC) of `aBytes`.
	[[nodiscard]] auto checksum(std::string_view aBytes) noexcept -> std::uint32_t;
	/// \brief Inflates one raw deflate stream (RFC 1951) that must end after
	/// exactly `aExpectedBytes`; nothing when it is corrupt, truncated or
	/// would produce more. Bytes after the final block are ignored.
	[[nodiscard]] auto inflate_raw(std::span<const std::uint8_t> aStream, std::uint64_t aExpectedBytes) -> std::optional<std::string>;
}

#endif
