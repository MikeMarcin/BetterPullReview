#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ZIP_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ZIP_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The ZIP container of a portable review bundle (RFC-BPR-0056,
/// BPR-1160), as pure byte functions: no file, directory or process is
/// touched, and nothing is extracted anywhere.
/// \details The writer emits stored (uncompressed) members with fixed
/// timestamps and UTF-8 names, so the same members always give the same
/// bytes. The reader accepts stored and deflated members from any producer,
/// and refuses before inflating anything: multi-disk and ZIP64 archives,
/// encryption, other methods, links, directories and other special entries,
/// names that are not UTF-8, overlapping or out-of-order member data, and any
/// member or total over its bound or its expansion ratio. Inflation stops at
/// the declared size and every member's CRC-32 must match.
namespace bpr::core {
	struct cycle_zip_member
	{
		std::string Name;
		std::string Bytes;
		auto        operator<=>(const cycle_zip_member&) const = default;
	};

	/// \brief Reader bounds; the defaults are the bundle budget.
	struct cycle_zip_limits
	{
		std::size_t   Entries      = 10'000U;
		std::uint64_t DecodedBytes = 512U * 1024U * 1024U;
		std::uint64_t Expansion    = 100U;
		std::size_t   NameBytes    = 4096U;
	};

	enum class cycle_zip_problem : std::uint8_t {
		/// Not a well-formed archive: signatures, lengths or offsets disagree.
		structure,
		/// Several disks, ZIP64 records or trailing bytes after the directory.
		unsupported_layout,
		encrypted,
		/// A compression method other than stored or deflate.
		unsupported_method,
		/// A symbolic link, directory or other non-regular entry.
		special_entry,
		/// An empty name, a name that is not UTF-8, or a name over its bound.
		name,
		/// More entries than admitted.
		count,
		/// A member or the total over its bound, or an expansion over the ratio.
		size,
		/// Member data overlapping another member or the directory.
		overlap,
		/// Corrupt deflate data, or output longer or shorter than declared.
		inflate,
		/// The CRC-32 of the data does not match the entry.
		checksum,
	};
	[[nodiscard]] auto stable_name(cycle_zip_problem aValue) noexcept -> std::string_view;

	/// \brief A deterministic archive of stored members, in the order given.
	[[nodiscard]] auto write_cycle_zip(std::span<const cycle_zip_member> aMembers) -> std::string;
	/// \brief Every member of an untrusted archive, in directory order, or
	/// the first problem found. Nothing is returned in part.
	[[nodiscard]] auto read_cycle_zip(std::span<const std::uint8_t> aArchive, const cycle_zip_limits& aLimits = { }) -> std::expected<std::vector<cycle_zip_member>, cycle_zip_problem>;

	/// \brief CRC-32 (ISO-HDLC), as ZIP records it.
	[[nodiscard]] auto cycle_crc32(std::string_view aBytes) noexcept -> std::uint32_t;
	/// \brief Inflates one raw deflate stream (RFC 1951) that must produce
	/// exactly `aExpectedBytes`; nothing when it is corrupt or its output
	/// differs in length.
	[[nodiscard]] auto inflate_cycle_deflate(std::span<const std::uint8_t> aStream, std::uint64_t aExpectedBytes) -> std::expected<std::string, cycle_zip_problem>;
}

#endif
