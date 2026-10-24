#include "bpr/core/review_cycle_zip.hpp"

#include "bpr/core/review_cycle_json.hpp"

#include "review_cycle_zip_codec.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr std::uint32_t local_signature     = 0x04034B50U;
		constexpr std::uint32_t central_signature   = 0x02014B50U;
		constexpr std::uint32_t end_signature       = 0x06054B50U;
		constexpr std::uint32_t zip64_end_locator   = 0x07064B50U;
		constexpr std::size_t   local_header_bytes  = 30U;
		constexpr std::size_t   central_entry_bytes = 46U;
		constexpr std::size_t   end_record_bytes    = 22U;
		constexpr std::uint16_t utf8_names          = 0x0800U;
		constexpr std::uint16_t stored              = 0U;
		constexpr std::uint16_t deflated            = 8U;

		void put16(std::string& aOut, const std::uint32_t aValue)
		{
			aOut.push_back(static_cast<char>(aValue & 0xFFU));
			aOut.push_back(static_cast<char>((aValue >> 8U) & 0xFFU));
		}

		void put32(std::string& aOut, const std::uint32_t aValue)
		{
			put16(aOut, aValue & 0xFFFFU);
			put16(aOut, aValue >> 16U);
		}

		auto get16(const std::span<const std::uint8_t> aBytes, const std::size_t aOffset) noexcept -> std::uint32_t
		{
			return static_cast<std::uint32_t>(aBytes[aOffset]) | (static_cast<std::uint32_t>(aBytes[aOffset + 1U]) << 8U);
		}

		auto get32(const std::span<const std::uint8_t> aBytes, const std::size_t aOffset) noexcept -> std::uint32_t
		{
			return get16(aBytes, aOffset) | (get16(aBytes, aOffset + 2U) << 16U);
		}

		struct entry
		{
			std::string   Name;
			std::uint32_t Method     = 0;
			std::uint32_t Crc        = 0;
			std::uint64_t Compressed = 0;
			std::uint64_t Size       = 0;
			std::size_t   Header     = 0;
			std::size_t   Data       = 0;
		};

		auto special(const std::uint32_t aMadeBy, const std::uint32_t aExternal, const std::string_view aName) noexcept -> bool
		{
			if (!aName.empty() && (aName.back() == '/' || aName.back() == '\\')) {
				return true;
			}
			if ((aMadeBy >> 8U) == 3U) {
				const auto type = (aExternal >> 16U) & 0xF000U;
				return type != 0U && type != 0x8000U;
			}
			return (aExternal & 0x10U) != 0U;
		}

		auto name_admitted(const std::string_view aName, const std::uint32_t aFlags, const std::size_t aLimit) -> bool
		{
			const auto ascii = std::ranges::all_of(aName, [](const char aSymbol) { return static_cast<unsigned char>(aSymbol) < 0x80U; });
			return !aName.empty() && aName.size() <= aLimit && aName.find('\0') == std::string_view::npos && ((aFlags & utf8_names) != 0U || ascii) && valid_cycle_utf8(aName);
		}

		/// Finds the end record, which must end the archive exactly.
		auto end_record(const std::span<const std::uint8_t> aArchive) -> std::optional<std::size_t>
		{
			if (aArchive.size() < end_record_bytes) {
				return std::nullopt;
			}
			const auto lowest = aArchive.size() > end_record_bytes + 0xFFFFU ? aArchive.size() - end_record_bytes - 0xFFFFU : 0U;
			for (auto position = aArchive.size() - end_record_bytes + 1U; position-- > lowest;) {
				if (get32(aArchive, position) == end_signature && position + end_record_bytes + get16(aArchive, position + 20U) == aArchive.size()) {
					return position;
				}
			}
			return std::nullopt;
		}

		/// Reads the central directory, checking each entry against its local header.
		auto directory(const std::span<const std::uint8_t> aArchive, const cycle_zip_limits& aLimits) -> std::expected<std::vector<entry>, cycle_zip_problem>
		{
			const auto end = end_record(aArchive);
			if (!end) {
				return std::unexpected(cycle_zip_problem::structure);
			}
			const auto disk = get16(aArchive, *end + 4U), directoryDisk = get16(aArchive, *end + 6U);
			const auto onDisk = get16(aArchive, *end + 8U), total = get16(aArchive, *end + 10U);
			const auto size = get32(aArchive, *end + 12U), offset = get32(aArchive, *end + 16U);
			const auto zip64 = *end >= 20U && get32(aArchive, *end - 20U) == zip64_end_locator;
			if (disk != 0U || directoryDisk != 0U || onDisk != total || total == 0xFFFFU || size == 0xFFFFFFFFU || offset == 0xFFFFFFFFU || zip64) {
				return std::unexpected(cycle_zip_problem::unsupported_layout);
			}
			if (total > aLimits.Entries) {
				return std::unexpected(cycle_zip_problem::count);
			}
			if (static_cast<std::uint64_t>(offset) + size != *end) {
				return std::unexpected(cycle_zip_problem::unsupported_layout);
			}
			auto entries = std::vector<entry>{ };
			auto cursor  = static_cast<std::size_t>(offset);
			auto decoded = std::uint64_t{ 0 };
			for (std::uint32_t index = 0; index < total; ++index) {
				if (*end - cursor < central_entry_bytes || get32(aArchive, cursor) != central_signature) {
					return std::unexpected(cycle_zip_problem::structure);
				}
				const auto madeBy = get16(aArchive, cursor + 4U), flags = get16(aArchive, cursor + 8U), method = get16(aArchive, cursor + 10U);
				const auto nameBytes = get16(aArchive, cursor + 28U), extraBytes = get16(aArchive, cursor + 30U), commentBytes = get16(aArchive, cursor + 32U);
				const auto next = cursor + central_entry_bytes + nameBytes + extraBytes + commentBytes;
				if (next > *end) {
					return std::unexpected(cycle_zip_problem::structure);
				}
				auto item = entry{
					.Name       = std::string{ reinterpret_cast<const char*>(aArchive.data() + cursor + central_entry_bytes), nameBytes },
					.Method     = method,
					.Crc        = get32(aArchive, cursor + 16U),
					.Compressed = get32(aArchive, cursor + 20U),
					.Size       = get32(aArchive, cursor + 24U),
					.Header     = get32(aArchive, cursor + 42U),
				};
				if ((flags & 0x41U) != 0U) {
					return std::unexpected(cycle_zip_problem::encrypted);
				}
				if (item.Compressed == 0xFFFFFFFFU || item.Size == 0xFFFFFFFFU || item.Header == 0xFFFFFFFFU) {
					return std::unexpected(cycle_zip_problem::unsupported_layout);
				}
				if (method != stored && method != deflated) {
					return std::unexpected(cycle_zip_problem::unsupported_method);
				}
				if (!name_admitted(item.Name, flags, aLimits.NameBytes)) {
					return std::unexpected(cycle_zip_problem::name);
				}
				if (special(madeBy, get32(aArchive, cursor + 38U), item.Name)) {
					return std::unexpected(cycle_zip_problem::special_entry);
				}
				decoded += item.Size;
				if ((method == stored && item.Size != item.Compressed) || item.Size > std::max<std::uint64_t>(1U, item.Compressed) * aLimits.Expansion || decoded > aLimits.DecodedBytes) {
					return std::unexpected(method == stored && item.Size != item.Compressed ? cycle_zip_problem::structure : cycle_zip_problem::size);
				}
				// The local header must agree with the directory.
				if (item.Header > offset || offset - item.Header < local_header_bytes || get32(aArchive, item.Header) != local_signature) {
					return std::unexpected(cycle_zip_problem::structure);
				}
				const auto localFlags = get16(aArchive, item.Header + 6U), localMethod = get16(aArchive, item.Header + 8U);
				const auto localName = get16(aArchive, item.Header + 26U), localExtra = get16(aArchive, item.Header + 28U);
				item.Data        = item.Header + local_header_bytes + localName + localExtra;
				const auto sized = (localFlags & 0x08U) != 0U || (get32(aArchive, item.Header + 18U) == item.Compressed && get32(aArchive, item.Header + 22U) == item.Size);
				if ((localFlags & 0x41U) != 0U || localMethod != method || !sized || localName != nameBytes || item.Data > offset || offset - item.Data < item.Compressed
					|| std::string_view{ reinterpret_cast<const char*>(aArchive.data() + item.Header + local_header_bytes), localName } != item.Name) {
					return std::unexpected(cycle_zip_problem::structure);
				}
				entries.push_back(std::move(item));
				cursor = next;
			}
			if (cursor != *end) {
				return std::unexpected(cycle_zip_problem::structure);
			}
			return entries;
		}

		/// Member data may not overlap another member.
		auto disjoint(std::vector<entry> aEntries) -> bool
		{
			std::ranges::sort(aEntries, { }, &entry::Header);
			for (std::size_t index = 1; index < aEntries.size(); ++index) {
				if (aEntries[index].Header < aEntries[index - 1U].Data + aEntries[index - 1U].Compressed) {
					return false;
				}
			}
			return true;
		}
	}

	auto stable_name(const cycle_zip_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_zip_problem::structure:          return "structure";
		case cycle_zip_problem::unsupported_layout: return "unsupported_layout";
		case cycle_zip_problem::encrypted:          return "encrypted";
		case cycle_zip_problem::unsupported_method: return "unsupported_method";
		case cycle_zip_problem::special_entry:      return "special_entry";
		case cycle_zip_problem::name:               return "name";
		case cycle_zip_problem::count:              return "count";
		case cycle_zip_problem::size:               return "size";
		case cycle_zip_problem::overlap:            return "overlap";
		case cycle_zip_problem::inflate:            return "inflate";
		case cycle_zip_problem::checksum:           return "checksum";
		}
		return "structure";
	}

	auto cycle_crc32(const std::string_view aBytes) noexcept -> std::uint32_t
	{
		return zip_codec::checksum(aBytes);
	}

	auto inflate_cycle_deflate(const std::span<const std::uint8_t> aStream, const std::uint64_t aExpectedBytes) -> std::expected<std::string, cycle_zip_problem>
	{
		auto output = zip_codec::inflate_raw(aStream, aExpectedBytes);
		if (!output) {
			return std::unexpected(cycle_zip_problem::inflate);
		}
		return std::move(*output);
	}

	auto write_cycle_zip(const std::span<const cycle_zip_member> aMembers, const cycle_zip_write_options& aOptions) -> std::string
	{
		auto archive = std::string{ };
		auto central = std::string{ };
		for (const auto& member : aMembers) {
			const auto crc  = cycle_crc32(member.Bytes);
			const auto at   = static_cast<std::uint32_t>(archive.size());
			const auto size = static_cast<std::uint32_t>(member.Bytes.size());
			const auto name = static_cast<std::uint32_t>(member.Name.size());
			// [[BPR-1725]]. A member deflate does not shrink stays stored.
			auto packed  = std::string{ };
			auto payload = member.Bytes;
			auto method  = std::uint32_t{ stored };
			if (aOptions.Deflate) {
				if (auto compressed = zip_codec::deflate_raw(member.Bytes); compressed && compressed->size() < member.Bytes.size()) {
					packed  = std::move(*compressed);
					payload = packed;
					method  = deflated;
				}
			}
			const auto packedSize = static_cast<std::uint32_t>(payload.size());
			// Local header: version 2.0, UTF-8 names, the method and the time.
			put32(archive, local_signature);
			for (const auto field : { 20U, std::uint32_t{ utf8_names }, method, std::uint32_t{ aOptions.DosTime }, std::uint32_t{ aOptions.DosDate } }) {
				put16(archive, field);
			}
			put32(archive, crc);
			put32(archive, packedSize);
			put32(archive, size);
			put16(archive, name);
			put16(archive, 0U);
			archive += member.Name;
			archive += payload;
			// Central entry: made by Unix 2.0, a regular file with its mode.
			put32(central, central_signature);
			for (const auto field : { 0x0314U, 20U, std::uint32_t{ utf8_names }, method, std::uint32_t{ aOptions.DosTime }, std::uint32_t{ aOptions.DosDate } }) {
				put16(central, field);
			}
			put32(central, crc);
			put32(central, packedSize);
			put32(central, size);
			for (const auto field : { name, 0U, 0U, 0U, 0U }) {
				put16(central, field);
			}
			put32(central, (0100000U | (std::uint32_t{ aOptions.Mode } & 0777U)) << 16U);
			put32(central, at);
			central += member.Name;
		}
		const auto offset = static_cast<std::uint32_t>(archive.size());
		archive += central;
		put32(archive, end_signature);
		for (const auto field : { 0U, 0U, static_cast<std::uint32_t>(aMembers.size()), static_cast<std::uint32_t>(aMembers.size()) }) {
			put16(archive, field);
		}
		put32(archive, static_cast<std::uint32_t>(central.size()));
		put32(archive, offset);
		put16(archive, 0U);
		return archive;
	}

	auto read_cycle_zip(const std::span<const std::uint8_t> aArchive, const cycle_zip_limits& aLimits) -> std::expected<std::vector<cycle_zip_member>, cycle_zip_problem>
	{
		auto entries = directory(aArchive, aLimits);
		if (!entries) {
			return std::unexpected(entries.error());
		}
		if (!disjoint(*entries)) {
			return std::unexpected(cycle_zip_problem::overlap);
		}
		auto members = std::vector<cycle_zip_member>{ };
		members.reserve(entries->size());
		for (auto& item : *entries) {
			const auto data  = aArchive.subspan(item.Data, static_cast<std::size_t>(item.Compressed));
			auto       bytes = std::string{ };
			if (item.Method == stored) {
				bytes.assign(reinterpret_cast<const char*>(data.data()), data.size());
			} else {
				auto inflated = inflate_cycle_deflate(data, item.Size);
				if (!inflated) {
					return std::unexpected(inflated.error());
				}
				bytes = std::move(*inflated);
			}
			if (cycle_crc32(bytes) != item.Crc) {
				return std::unexpected(cycle_zip_problem::checksum);
			}
			members.push_back({ .Name = std::move(item.Name), .Bytes = std::move(bytes) });
		}
		return members;
	}
}
