#include "review_cycle_zip_codec.hpp"

#include <zlib.h>

#include <algorithm>
#include <limits>

namespace bpr::core::zip_codec {
	auto checksum(const std::string_view aBytes) noexcept -> std::uint32_t
	{
		// [[BPR-1435]]. zlib's CRC-32 (ISO-HDLC), the ZIP checksum.
		return static_cast<std::uint32_t>(::crc32_z(0UL, reinterpret_cast<const Bytef*>(aBytes.data()), aBytes.size()));
	}

	auto inflate_raw(const std::span<const std::uint8_t> aStream, const std::uint64_t aExpectedBytes) -> std::optional<std::string>
	{
		// [[BPR-1435]]. zlib raw inflate (RFC 1951) replaces a hand-written port
		// of puff. The stream must end, within the input, after exactly
		// `aExpectedBytes`; one sentinel byte of output space detects a stream
		// that would produce more. Bytes after the final block are ignored.
		if (aExpectedBytes >= std::numeric_limits<std::size_t>::max()) {
			return std::nullopt;
		}
		auto output = std::string(static_cast<std::size_t>(aExpectedBytes) + 1U, '\0');
		auto stream = z_stream{ };
		if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
			return std::nullopt;
		}
		const auto chunk  = static_cast<std::size_t>(std::numeric_limits<uInt>::max());
		auto       input  = std::size_t{ 0 };
		auto       status = Z_OK;
		while (status == Z_OK) {
			if (stream.avail_in == 0U && input < aStream.size()) {
				stream.next_in  = reinterpret_cast<const Bytef*>(aStream.data() + input);
				stream.avail_in = static_cast<uInt>(std::min(chunk, aStream.size() - input));
				input += stream.avail_in;
			}
			if (stream.avail_out == 0U) {
				const auto produced = static_cast<std::size_t>(stream.total_out);
				if (produced == output.size()) {
					break;
				}
				stream.next_out  = reinterpret_cast<Bytef*>(output.data() + produced);
				stream.avail_out = static_cast<uInt>(std::min(chunk, output.size() - produced));
			}
			status = inflate(&stream, Z_NO_FLUSH);
			if (status == Z_BUF_ERROR && stream.avail_in == 0U && input == aStream.size()) {
				break;
			}
			if (status == Z_BUF_ERROR) {
				status = Z_OK;
			}
		}
		const auto produced = static_cast<std::uint64_t>(stream.total_out);
		inflateEnd(&stream);
		if (status != Z_STREAM_END || produced != aExpectedBytes) {
			return std::nullopt;
		}
		output.resize(static_cast<std::size_t>(aExpectedBytes));
		return output;
	}

	auto deflate_raw(const std::string_view aBytes) -> std::optional<std::string>
	{
		// [[BPR-1725]]. zlib raw deflate at level 6 with its default
		// window and memory, so a pinned zlib always gives the same stream.
		auto stream = z_stream{ };
		if (deflateInit2(&stream, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
			return std::nullopt;
		}
		const auto chunk  = static_cast<std::size_t>(std::numeric_limits<uInt>::max());
		auto       output = std::string{ };
		auto       input  = std::size_t{ 0 };
		auto       status = Z_OK;
		output.resize(std::min<std::size_t>(chunk, deflateBound(&stream, static_cast<uLong>(std::min<std::size_t>(aBytes.size(), std::numeric_limits<uLong>::max())))));
		while (status == Z_OK) {
			if (stream.avail_in == 0U && input < aBytes.size()) {
				stream.next_in  = reinterpret_cast<const Bytef*>(aBytes.data() + input);
				stream.avail_in = static_cast<uInt>(std::min(chunk, aBytes.size() - input));
				input += stream.avail_in;
			}
			const auto produced = static_cast<std::size_t>(stream.total_out);
			if (produced == output.size()) {
				output.resize(output.size() + std::min<std::size_t>(chunk, output.size() / 2U + 64U));
			}
			stream.next_out  = reinterpret_cast<Bytef*>(output.data() + produced);
			stream.avail_out = static_cast<uInt>(std::min(chunk, output.size() - produced));
			status           = deflate(&stream, input == aBytes.size() ? Z_FINISH : Z_NO_FLUSH);
			if (status == Z_BUF_ERROR) {
				status = Z_OK;
			}
		}
		const auto produced = static_cast<std::size_t>(stream.total_out);
		deflateEnd(&stream);
		if (status != Z_STREAM_END) {
			return std::nullopt;
		}
		output.resize(produced);
		return output;
	}
}
