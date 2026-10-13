#include "bpr/core/review_cycle_git.hpp"

#include "bpr/core/git_config.hpp"
#include "bpr/core/review_cycle_outgoing.hpp"
#include "bpr/core/sha1.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		constexpr std::size_t sha1_bytes   = 20U;
		constexpr std::size_t sha256_bytes = 32U;
		/// ctime, mtime, dev, ino, mode, uid, gid and size: ten 32-bit words.
		constexpr std::size_t stat_bytes = 40U;

		auto hash_bytes(const git_hash_algorithm aAlgorithm) -> std::size_t
		{
			switch (aAlgorithm) {
			case git_hash_algorithm::sha1:
				return sha1_bytes;
			case git_hash_algorithm::sha256:
				return sha256_bytes;
			case git_hash_algorithm::invalid:
				return 0U;
			}
			return 0U;
		}

		auto big_endian_32(const std::span<const std::uint8_t> aBytes, const std::size_t aOffset) -> std::uint32_t
		{
			return (static_cast<std::uint32_t>(aBytes[aOffset]) << 24U) | (static_cast<std::uint32_t>(aBytes[aOffset + 1]) << 16U)
				| (static_cast<std::uint32_t>(aBytes[aOffset + 2]) << 8U) | static_cast<std::uint32_t>(aBytes[aOffset + 3]);
		}

		auto big_endian_16(const std::span<const std::uint8_t> aBytes, const std::size_t aOffset) -> std::uint16_t
		{
			return static_cast<std::uint16_t>((static_cast<unsigned>(aBytes[aOffset]) << 8U) | aBytes[aOffset + 1]);
		}

		/// Git's offset varint for index version 4 path compression.
		auto read_varint(const std::span<const std::uint8_t> aBytes, std::size_t& aOffset, const std::size_t aLimit) -> std::optional<std::uint64_t>
		{
			if (aOffset >= aLimit) {
				return std::nullopt;
			}
			auto byte  = aBytes[aOffset++];
			auto value = static_cast<std::uint64_t>(byte & 0x7FU);
			while ((byte & 0x80U) != 0) {
				if (aOffset >= aLimit || value > (UINT64_MAX >> 8U)) {
					return std::nullopt;
				}
				byte  = aBytes[aOffset++];
				value = ((value + 1U) << 7U) | (byte & 0x7FU);
			}
			return value;
		}

		auto trim(std::string_view aText) -> std::string_view
		{
			constexpr auto space = std::string_view{ " \t\r\n" };
			const auto     first = aText.find_first_not_of(space);
			if (first == std::string_view::npos) {
				return { };
			}
			aText = aText.substr(first);
			return aText.substr(0, aText.find_last_not_of(space) + 1);
		}

		/// Git's white space: space, tab, line feed and carriage return.
		auto is_git_space(const char aCharacter) -> bool
		{
			return aCharacter == ' ' || aCharacter == '\t' || aCharacter == '\n' || aCharacter == '\r';
		}

		/// The trailer is all zero (`index.skipHash`) or the repository hash
		/// of everything before it.
		auto trailer_matches(const std::span<const std::uint8_t> aBytes, const git_hash_algorithm aAlgorithm, const std::size_t aHashLength) -> bool
		{
			const auto body    = aBytes.first(aBytes.size() - aHashLength);
			const auto trailer = aBytes.last(aHashLength);
			if (std::ranges::all_of(trailer, [](const std::uint8_t aByte) { return aByte == 0; })) {
				return true;
			}
			if (aAlgorithm == git_hash_algorithm::sha1) {
				// A body collision detection refuses has no checksum to match.
				const auto digest = sha1(body);
				return digest && std::ranges::equal(*digest, trailer);
			}
			return std::ranges::equal(sha256(body), trailer);
		}

		auto commit_of(const std::string_view aHex, const git_hash_algorithm aAlgorithm) -> std::optional<git_commit_oid>
		{
			const auto parsed = parse_git_commit_oid(aHex);
			if (!parsed || parsed->algorithm() != aAlgorithm) {
				return std::nullopt;
			}
			return parsed;
		}

		/// Calls `aLine` for every line of `aText` until it returns true.
		template <typename TVisit>
		void each_line(std::string_view aText, TVisit aLine)
		{
			while (!aText.empty()) {
				const auto end = aText.find('\n');
				if (aLine(aText.substr(0, end))) {
					return;
				}
				if (end == std::string_view::npos) {
					return;
				}
				aText.remove_prefix(end + 1);
			}
		}
	}

	auto parse_git_index(const std::span<const std::uint8_t> aBytes, const git_hash_algorithm aAlgorithm, const std::uint64_t aMaximumEntries)
		-> std::expected<git_index, git_index_error>
	{
		const auto hashLength = hash_bytes(aAlgorithm);
		if (hashLength == 0 || aBytes.size() < 12U + hashLength) {
			return std::unexpected(git_index_error::truncated);
		}
		if (aBytes[0] != 'D' || aBytes[1] != 'I' || aBytes[2] != 'R' || aBytes[3] != 'C') {
			return std::unexpected(git_index_error::signature);
		}
		if (!trailer_matches(aBytes, aAlgorithm, hashLength)) {
			return std::unexpected(git_index_error::checksum);
		}
		auto       index = git_index{ .Version = big_endian_32(aBytes, 4) };
		const auto count = big_endian_32(aBytes, 8);
		if (index.Version < 2 || index.Version > 4) {
			return std::unexpected(git_index_error::unsupported_version);
		}
		if (count > aMaximumEntries) {
			return std::unexpected(git_index_error::bounds);
		}
		// Entries and extensions end where the trailing checksum starts.
		const auto limit    = aBytes.size() - hashLength;
		auto       offset   = std::size_t{ 12 };
		auto       previous = std::string{ };
		index.Entries.reserve(count);
		for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
			const auto start = offset;
			if (limit - offset < stat_bytes + hashLength + 2U) {
				return std::unexpected(git_index_error::truncated);
			}
			auto entry = git_index_entry{ .Mode = big_endian_32(aBytes, offset + 24U) };
			auto oid   = std::array<std::uint8_t, 32>{ };
			std::ranges::copy(aBytes.subspan(offset + stat_bytes, hashLength), oid.begin());
			entry.Object     = git_blob_oid{ aAlgorithm, oid };
			const auto flags = big_endian_16(aBytes, offset + stat_bytes + hashLength);
			offset += stat_bytes + hashLength + 2U;
			entry.Stage         = static_cast<std::uint8_t>((static_cast<unsigned>(flags) >> 12U) & 0x3U);
			const auto extended = (flags & 0x4000U) != 0;
			const auto length   = static_cast<std::size_t>(flags & 0x0FFFU);
			if (extended) {
				if (index.Version < 3 || limit - offset < 2U) {
					return std::unexpected(git_index_error::malformed);
				}
				const auto more    = big_endian_16(aBytes, offset);
				entry.SkipWorktree = (more & 0x4000U) != 0;
				entry.IntentToAdd  = (more & 0x2000U) != 0;
				offset += 2U;
			}
			if (index.Version == 4) {
				const auto strip = read_varint(aBytes, offset, limit);
				if (!strip || *strip > previous.size()) {
					return std::unexpected(git_index_error::malformed);
				}
				const auto end = std::find(aBytes.begin() + static_cast<std::ptrdiff_t>(offset), aBytes.begin() + static_cast<std::ptrdiff_t>(limit), std::uint8_t{ 0 });
				if (end == aBytes.begin() + static_cast<std::ptrdiff_t>(limit)) {
					return std::unexpected(git_index_error::truncated);
				}
				entry.Path = previous.substr(0, previous.size() - static_cast<std::size_t>(*strip));
				entry.Path.append(aBytes.begin() + static_cast<std::ptrdiff_t>(offset), end);
				offset = static_cast<std::size_t>(end - aBytes.begin()) + 1U;
			} else {
				const auto end = std::find(aBytes.begin() + static_cast<std::ptrdiff_t>(offset), aBytes.begin() + static_cast<std::ptrdiff_t>(limit), std::uint8_t{ 0 });
				if (end == aBytes.begin() + static_cast<std::ptrdiff_t>(limit)) {
					return std::unexpected(git_index_error::truncated);
				}
				entry.Path.assign(aBytes.begin() + static_cast<std::ptrdiff_t>(offset), end);
				if (length != 0x0FFFU && entry.Path.size() != length) {
					return std::unexpected(git_index_error::malformed);
				}
				// Versions 2 and 3 pad each entry with one to eight NULs to a
				// multiple of eight bytes.
				const auto fixed  = offset - start;
				const auto padded = (fixed + entry.Path.size() + 8U) & ~std::size_t{ 7U };
				if (padded > limit - start) {
					return std::unexpected(git_index_error::truncated);
				}
				offset = start + padded;
			}
			if (entry.Path.empty() || entry.Path < previous) {
				return std::unexpected(git_index_error::malformed);
			}
			index.Unmerged          = index.Unmerged || entry.Stage != 0;
			index.SparseDirectories = index.SparseDirectories || (entry.Mode == 0040000U && entry.Path.ends_with('/'));
			previous                = entry.Path;
			index.Entries.push_back(std::move(entry));
		}
		while (limit - offset >= 8U) {
			const auto signature = std::string_view{ reinterpret_cast<const char*>(aBytes.data() + offset), 4U };
			const auto size      = big_endian_32(aBytes, offset + 4U);
			if (limit - offset - 8U < size) {
				return std::unexpected(git_index_error::truncated);
			}
			// Git skips an extension whose signature starts with A-Z and
			// refuses any other it does not understand.
			const auto optional = signature.front() >= 'A' && signature.front() <= 'Z';
			if (!optional && signature != "link" && signature != "sdir") {
				return std::unexpected(git_index_error::required_extension);
			}
			index.SplitIndex        = index.SplitIndex || signature == "link";
			index.SparseDirectories = index.SparseDirectories || signature == "sdir";
			offset += 8U + size;
		}
		return index;
	}

	namespace {
		/// The extensions Git honours in a version 0 repository.
		auto version_zero_extension(const std::string_view aName) -> bool
		{
			return aName == "noop" || aName == "preciousobjects" || aName == "partialclone" || aName == "worktreeconfig";
		}

		/// The extensions Git honours only in a version 1 repository.
		auto version_one_extension(const std::string_view aName) -> bool
		{
			return aName == "noop-v1" || aName == "objectformat" || aName == "compatobjectformat" || aName == "refstorage" || aName == "relativeworktrees";
		}

		auto refusal(const git_repository_refusal aReason, std::string aKey = { }) -> std::unexpected<git_repository_problem>
		{
			return std::unexpected(git_repository_problem{ .Reason = aReason, .Key = std::move(aKey) });
		}

		auto extension_name(const git_config_entry& aEntry) -> std::string
		{
			return aEntry.Subsection ? *aEntry.Subsection + "." + aEntry.Key : aEntry.Key;
		}

		auto core_key(const git_config_entry& aEntry, const std::string_view aKey) -> bool
		{
			return aEntry.Section == "core" && !aEntry.Subsection && aEntry.Key == aKey;
		}

		/// `core.ignoreCase` from one config file, the last setting winning.
		auto ignore_case(const std::vector<git_config_entry>& aEntries, bool& aIgnoreCase) -> std::optional<git_repository_problem>
		{
			for (const auto& entry : aEntries) {
				if (core_key(entry, "ignorecase")) {
					const auto value = git_config_bool(entry.Value);
					if (!value) {
						return git_repository_problem{ .Reason = git_repository_refusal::invalid_value, .Key = "core.ignorecase" };
					}
					aIgnoreCase = *value;
				}
			}
			return std::nullopt;
		}
	}

	auto read_git_repository_format(const std::string_view aConfig) -> std::expected<git_repository_format, git_repository_problem>
	{
		const auto entries = parse_git_config(aConfig);
		if (!entries) {
			return refusal(git_repository_refusal::malformed_config);
		}
		auto format       = git_repository_format{ };
		auto version      = std::int32_t{ 0 };
		auto unknown      = std::optional<std::string>{ };
		auto versionOne   = std::optional<std::string>{ };
		auto objectFormat = git_hash_algorithm::sha1;
		auto reftable     = false;
		for (const auto& entry : *entries) {
			if (core_key(entry, "repositoryformatversion")) {
				const auto value = git_config_int(entry.Value);
				if (!value) {
					return refusal(git_repository_refusal::format_version, "core.repositoryformatversion");
				}
				version = *value;
				continue;
			}
			if (entry.Section != "extensions") {
				continue;
			}
			const auto name = extension_name(entry);
			const auto key  = "extensions." + name;
			// Git validates every value it interprets, whatever the version.
			if (name == "preciousobjects" || name == "worktreeconfig" || name == "relativeworktrees") {
				const auto value = git_config_bool(entry.Value);
				if (!value) {
					return refusal(git_repository_refusal::invalid_value, key);
				}
				if (name == "worktreeconfig") {
					format.WorktreeConfig = *value;
				}
			} else if (name == "partialclone" || name == "compatobjectformat") {
				if (!entry.Value) {
					return refusal(git_repository_refusal::invalid_value, key);
				}
			} else if (name == "objectformat") {
				if (entry.Value == "sha1") {
					objectFormat = git_hash_algorithm::sha1;
				} else if (entry.Value == "sha256") {
					objectFormat = git_hash_algorithm::sha256;
				} else {
					return refusal(git_repository_refusal::object_format, key);
				}
			} else if (name == "refstorage") {
				if (entry.Value == "files" || entry.Value == "reftable") {
					reftable = entry.Value == "reftable";
				} else {
					return refusal(git_repository_refusal::ref_storage, key);
				}
			}
			if (version_zero_extension(name)) {
				continue;
			}
			if (version_one_extension(name)) {
				// Git knows compatObjectFormat; these readers do not read the
				// object map it adds and decline it with the unknown ones.
				if (name == "compatobjectformat") {
					unknown = unknown.value_or(key);
				}
				versionOne = versionOne.value_or(key);
				continue;
			}
			unknown = unknown.value_or(key);
		}
		if (version < 0 || version > 1) {
			return refusal(git_repository_refusal::format_version, "core.repositoryformatversion");
		}
		if (version == 0 && versionOne) {
			return refusal(git_repository_refusal::version_zero_extension, *versionOne);
		}
		if (version == 1) {
			if (unknown) {
				return refusal(git_repository_refusal::unknown_extension, *unknown);
			}
			if (reftable) {
				return refusal(git_repository_refusal::reftable, "extensions.refstorage");
			}
			format.ObjectFormat = objectFormat;
		}
		// Version 0 keeps SHA-1 and files: Git ignores unknown extensions there
		// and honours only the version 0 ones.
		if (const auto problem = ignore_case(*entries, format.IgnoreCase)) {
			return std::unexpected(*problem);
		}
		return format;
	}

	auto apply_git_worktree_config(const std::string_view aConfig, git_repository_format& aFormat) -> std::optional<git_repository_problem>
	{
		const auto entries = parse_git_config(aConfig);
		if (!entries) {
			return git_repository_problem{ .Reason = git_repository_refusal::malformed_config, .Key = "config.worktree" };
		}
		return ignore_case(*entries, aFormat.IgnoreCase);
	}

	auto git_index_problem(const git_index_error aError) noexcept -> git_repository_problem
	{
		switch (aError) {
		case git_index_error::checksum:
			return { .Reason = git_repository_refusal::index_checksum };
		case git_index_error::required_extension:
			return { .Reason = git_repository_refusal::index_extension };
		default:
			return { .Reason = git_repository_refusal::index_format };
		}
	}

	auto admissible_git_reference(const std::string_view aName) noexcept -> bool
	{
		if (!aName.starts_with("refs/") || aName.size() > maximum_outgoing_reference_bytes || aName.ends_with('/') || aName.ends_with(".lock")
			|| aName.find("..") != std::string_view::npos || aName.find("@{") != std::string_view::npos) {
			return false;
		}
		for (const auto character : aName) {
			const auto value = static_cast<unsigned char>(character);
			if (value < 0x21U || value == 0x7FU || std::string_view{ "~^:?*[\\" }.find(character) != std::string_view::npos) {
				return false;
			}
		}
		auto start = std::size_t{ 0 };
		while (start < aName.size()) {
			const auto end     = aName.find('/', start);
			const auto segment = aName.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			if (segment.empty() || segment.front() == '.') {
				return false;
			}
			if (end == std::string_view::npos) {
				break;
			}
			start = end + 1U;
		}
		return true;
	}

	auto parse_git_head(const std::string_view aHead, const git_hash_algorithm aAlgorithm) -> git_head
	{
		auto contents = aHead;
		while (!contents.empty() && is_git_space(contents.back())) {
			contents.remove_suffix(1);
		}
		if (contents.starts_with("ref:")) {
			auto reference = contents.substr(4);
			while (!reference.empty() && is_git_space(reference.front())) {
				reference.remove_prefix(1);
			}
			if (admissible_git_reference(reference)) {
				return { .Reference = std::string{ reference } };
			}
			return { };
		}
		const auto length = hash_bytes(aAlgorithm) * 2U;
		if (length == 0 || contents.size() < length || (contents.size() > length && !is_git_space(contents[length]))) {
			return { };
		}
		return { .Commit = commit_of(contents.substr(0, length), aAlgorithm) };
	}

	auto parse_git_reference(const std::string_view aContents, const git_hash_algorithm aAlgorithm) -> std::optional<git_commit_oid>
	{
		return parse_git_head(aContents, aAlgorithm).Commit;
	}

	auto find_packed_reference(const std::string_view aPackedRefs, const std::string_view aReference, const git_hash_algorithm aAlgorithm)
		-> std::optional<git_commit_oid>
	{
		auto found = std::optional<git_commit_oid>{ };
		each_line(aPackedRefs, [&](const std::string_view aLine) {
			if (aLine.empty() || aLine.front() == '#' || aLine.front() == '^') {
				return false;
			}
			const auto space = aLine.find(' ');
			if (space == std::string_view::npos || trim(aLine.substr(space + 1)) != aReference) {
				return false;
			}
			found = commit_of(aLine.substr(0, space), aAlgorithm);
			return true;
		});
		return found;
	}
}
