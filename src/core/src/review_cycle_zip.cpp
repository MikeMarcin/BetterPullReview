#include "bpr/core/review_cycle_zip.hpp"

#include "bpr/core/review_cycle_json.hpp"

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

		constexpr auto crc_table = [] {
			auto table = std::array<std::uint32_t, 256>{ };
			for (std::uint32_t index = 0; index < 256U; ++index) {
				auto value = index;
				for (int bit = 0; bit < 8; ++bit) {
					value = (value & 1U) != 0U ? 0xEDB88320U ^ (value >> 1U) : value >> 1U;
				}
				table[index] = value;
			}
			return table;
		}();

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

		// ---- inflate (RFC 1951), after Mark Adler's puff ----------------

		constexpr int maximum_code_bits = 15;

		struct huffman
		{
			std::array<std::uint16_t, maximum_code_bits + 1> Count{ };
			std::array<std::uint16_t, 288>                   Symbol{ };
		};

		/// Builds decoding tables from code lengths. Zero for a complete code,
		/// positive for an incomplete one, negative for an over-subscribed one.
		auto construct(huffman& aTable, const std::span<const std::uint16_t> aLengths) -> int
		{
			aTable.Count.fill(0U);
			for (const auto length : aLengths) {
				++aTable.Count[length];
			}
			if (aTable.Count[0] == aLengths.size()) {
				return 0;
			}
			auto left = 1;
			for (int length = 1; length <= maximum_code_bits; ++length) {
				left *= 2;
				left -= aTable.Count[static_cast<std::size_t>(length)];
				if (left < 0) {
					return left;
				}
			}
			auto offsets = std::array<std::uint16_t, maximum_code_bits + 1>{ };
			for (std::size_t length = 1; length < maximum_code_bits; ++length) {
				offsets[length + 1U] = static_cast<std::uint16_t>(offsets[length] + aTable.Count[length]);
			}
			for (std::size_t symbol = 0; symbol < aLengths.size(); ++symbol) {
				if (aLengths[symbol] != 0U) {
					aTable.Symbol[offsets[aLengths[symbol]]++] = static_cast<std::uint16_t>(symbol);
				}
			}
			return left;
		}

		class inflater
		{
		public:
			inflater(const std::span<const std::uint8_t> aInput, const std::uint64_t aLimit)
				: Input{ aInput }
				, Limit{ aLimit }
			{
			}

			auto run() -> std::optional<std::string>
			{
				auto last = 0U;
				do {
					last            = bits(1);
					const auto type = bits(2);
					if (Failed) {
						return std::nullopt;
					}
					switch (type) {
					case 0:  stored_block(); break;
					case 1:  fixed_block(); break;
					case 2:  dynamic_block(); break;
					default: Failed = true; break;
					}
				} while (!Failed && last == 0U);
				if (Failed) {
					return std::nullopt;
				}
				return std::move(Output);
			}

		private:
			auto bits(const int aNeed) -> std::uint32_t
			{
				auto value = Buffer;
				while (Count < aNeed) {
					if (Position == Input.size()) {
						Failed = true;
						return 0U;
					}
					value |= static_cast<std::uint32_t>(Input[Position++]) << static_cast<unsigned>(Count);
					Count += 8;
				}
				Buffer = value >> static_cast<unsigned>(aNeed);
				Count -= aNeed;
				return value & ((1U << static_cast<unsigned>(aNeed)) - 1U);
			}

			auto emit(const char aByte) -> bool
			{
				if (Output.size() >= Limit) {
					Failed = true;
					return false;
				}
				Output.push_back(aByte);
				return true;
			}

			void stored_block()
			{
				Buffer = 0U;
				Count  = 0;
				if (Input.size() - Position < 4U) {
					Failed = true;
					return;
				}
				const auto length  = get16(Input, Position);
				const auto inverse = get16(Input, Position + 2U);
				Position += 4U;
				if (length != (~inverse & 0xFFFFU) || Input.size() - Position < length) {
					Failed = true;
					return;
				}
				auto index = std::uint32_t{ 0 };
				while (index < length && emit(static_cast<char>(Input[Position + index]))) {
					++index;
				}
				Position += length;
			}

			auto decode(const huffman& aTable) -> int
			{
				auto code  = 0;
				auto first = 0;
				auto index = 0;
				for (int length = 1; length <= maximum_code_bits; ++length) {
					code += static_cast<int>(bits(1));
					if (Failed) {
						return -1;
					}
					const auto count = static_cast<int>(aTable.Count[static_cast<std::size_t>(length)]);
					if (code - count < first) {
						const auto at = index + (code - first);
						return aTable.Symbol[static_cast<std::size_t>(at)];
					}
					index += count;
					first += count;
					first *= 2;
					code *= 2;
				}
				Failed = true;
				return -1;
			}

			void codes(const huffman& aLengths, const huffman& aDistances)
			{
				static constexpr auto lengthBase    = std::array<std::uint16_t, 29>{ 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
				static constexpr auto lengthExtra   = std::array<std::uint8_t, 29>{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
				static constexpr auto distanceBase  = std::array<std::uint16_t, 30>{ 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
				static constexpr auto distanceExtra = std::array<std::uint8_t, 30>{ 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
				while (!Failed) {
					auto symbol = decode(aLengths);
					if (Failed || symbol == 256) {
						return;
					}
					if (symbol < 256) {
						emit(static_cast<char>(symbol));
						continue;
					}
					symbol -= 257;
					if (symbol >= 29) {
						Failed = true;
						return;
					}
					const auto length = lengthBase[static_cast<std::size_t>(symbol)] + bits(lengthExtra[static_cast<std::size_t>(symbol)]);
					const auto code   = decode(aDistances);
					if (Failed || code >= 30) {
						Failed = true;
						return;
					}
					const auto distance = distanceBase[static_cast<std::size_t>(code)] + bits(distanceExtra[static_cast<std::size_t>(code)]);
					if (Failed || distance > Output.size()) {
						Failed = true;
						return;
					}
					auto copied = std::uint32_t{ 0 };
					while (copied < length && emit(Output[Output.size() - distance])) {
						++copied;
					}
				}
			}

			void fixed_block()
			{
				static const auto tables = [] {
					auto lengths = std::array<std::uint16_t, 288>{ };
					std::fill(lengths.begin(), lengths.begin() + 144, std::uint16_t{ 8 });
					std::fill(lengths.begin() + 144, lengths.begin() + 256, std::uint16_t{ 9 });
					std::fill(lengths.begin() + 256, lengths.begin() + 280, std::uint16_t{ 7 });
					std::fill(lengths.begin() + 280, lengths.end(), std::uint16_t{ 8 });
					auto result = std::pair<huffman, huffman>{ };
					construct(result.first, lengths);
					auto distances = std::array<std::uint16_t, 30>{ };
					distances.fill(5U);
					construct(result.second, distances);
					return result;
				}();
				codes(tables.first, tables.second);
			}

			void dynamic_block()
			{
				static constexpr auto order         = std::array<std::uint8_t, 19>{ 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
				const auto            lengthCount   = bits(5) + 257U;
				const auto            distanceCount = bits(5) + 1U;
				const auto            codeCount     = bits(4) + 4U;
				if (Failed || lengthCount > 286U || distanceCount > 30U) {
					Failed = true;
					return;
				}
				auto lengths = std::array<std::uint16_t, 316>{ };
				for (std::uint32_t index = 0; index < codeCount; ++index) {
					lengths[order[index]] = static_cast<std::uint16_t>(bits(3));
				}
				auto lengthCode = huffman{ };
				if (Failed || construct(lengthCode, std::span{ lengths.data(), 19U }) != 0) {
					Failed = true;
					return;
				}
				lengths.fill(0U);
				for (std::uint32_t index = 0; index < lengthCount + distanceCount;) {
					const auto symbol = decode(lengthCode);
					if (Failed) {
						return;
					}
					if (symbol < 16) {
						lengths[index++] = static_cast<std::uint16_t>(symbol);
						continue;
					}
					auto value  = std::uint16_t{ 0 };
					auto repeat = 0U;
					if (symbol == 16) {
						if (index == 0U) {
							Failed = true;
							return;
						}
						value  = lengths[index - 1U];
						repeat = 3U + bits(2);
					} else if (symbol == 17) {
						repeat = 3U + bits(3);
					} else {
						repeat = 11U + bits(7);
					}
					if (Failed || index + repeat > lengthCount + distanceCount) {
						Failed = true;
						return;
					}
					while (repeat-- != 0U) {
						lengths[index++] = value;
					}
				}
				if (lengths[256] == 0U) {
					Failed = true;
					return;
				}
				auto       literals      = huffman{ };
				auto       distances     = huffman{ };
				const auto literalCheck  = construct(literals, std::span{ lengths.data(), lengthCount });
				const auto distanceCheck = construct(distances, std::span{ lengths.data() + lengthCount, distanceCount });
				// Only a code with a single one-bit length may be incomplete.
				if ((literalCheck != 0 && (literalCheck < 0 || lengthCount != static_cast<std::uint32_t>(literals.Count[0] + literals.Count[1])))
					|| (distanceCheck != 0 && (distanceCheck < 0 || distanceCount != static_cast<std::uint32_t>(distances.Count[0] + distances.Count[1])))) {
					Failed = true;
					return;
				}
				codes(literals, distances);
			}

			std::span<const std::uint8_t> Input;
			std::uint64_t                 Limit;
			std::size_t                   Position = 0;
			std::uint32_t                 Buffer   = 0;
			int                           Count    = 0;
			bool                          Failed   = false;
			std::string                   Output;
		};

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
		auto value = 0xFFFFFFFFU;
		for (const auto symbol : aBytes) {
			value = crc_table[(value ^ static_cast<unsigned char>(symbol)) & 0xFFU] ^ (value >> 8U);
		}
		return value ^ 0xFFFFFFFFU;
	}

	auto inflate_cycle_deflate(const std::span<const std::uint8_t> aStream, const std::uint64_t aExpectedBytes) -> std::expected<std::string, cycle_zip_problem>
	{
		auto output = inflater{ aStream, aExpectedBytes }.run();
		if (!output || output->size() != aExpectedBytes) {
			return std::unexpected(cycle_zip_problem::inflate);
		}
		return std::move(*output);
	}

	auto write_cycle_zip(const std::span<const cycle_zip_member> aMembers) -> std::string
	{
		auto archive = std::string{ };
		auto central = std::string{ };
		for (const auto& member : aMembers) {
			const auto crc  = cycle_crc32(member.Bytes);
			const auto at   = static_cast<std::uint32_t>(archive.size());
			const auto size = static_cast<std::uint32_t>(member.Bytes.size());
			const auto name = static_cast<std::uint32_t>(member.Name.size());
			// Local header: version 2.0, UTF-8 names, stored, 1980-01-01 00:00.
			put32(archive, local_signature);
			for (const auto field : { 20U, std::uint32_t{ utf8_names }, std::uint32_t{ stored }, 0U, 0x21U }) {
				put16(archive, field);
			}
			put32(archive, crc);
			put32(archive, size);
			put32(archive, size);
			put16(archive, name);
			put16(archive, 0U);
			archive += member.Name;
			archive += member.Bytes;
			// Central entry: made by Unix 2.0, a regular file with mode 0644.
			put32(central, central_signature);
			for (const auto field : { 0x0314U, 20U, std::uint32_t{ utf8_names }, std::uint32_t{ stored }, 0U, 0x21U }) {
				put16(central, field);
			}
			put32(central, crc);
			put32(central, size);
			put32(central, size);
			for (const auto field : { name, 0U, 0U, 0U, 0U }) {
				put16(central, field);
			}
			put32(central, 0x81A40000U);
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
