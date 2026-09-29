#include "bpr/core/review_cycle_manifest.hpp"

#include <algorithm>
#include <array>
#include <functional>

namespace bpr::core {
	namespace {
		constexpr auto magic = std::string_view{ "BPRCYCM1" };

		struct writer
		{
			std::vector<std::uint8_t> Bytes;

			void u8(const std::uint8_t aValue)
			{
				Bytes.push_back(aValue);
			}
			void flag(const bool aValue)
			{
				u8(aValue ? 1U : 0U);
			}
			void u32(const std::uint32_t aValue)
			{
				for (auto shift = 24; shift >= 0; shift -= 8) {
					Bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			}
			void u64(const std::uint64_t aValue)
			{
				for (auto shift = 56; shift >= 0; shift -= 8) {
					Bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			}
			void text(const std::string_view aValue)
			{
				u32(static_cast<std::uint32_t>(aValue.size()));
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
			void raw(const std::array<std::uint8_t, 32>& aValue)
			{
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
		};

		struct reader
		{
			std::span<const std::uint8_t> Bytes;
			std::size_t                   Offset = 0;
			bool                          Good   = true;

			auto take(const std::size_t aCount) -> std::span<const std::uint8_t>
			{
				if (!Good || Bytes.size() - Offset < aCount) {
					Good = false;
					return { };
				}
				const auto result = Bytes.subspan(Offset, aCount);
				Offset += aCount;
				return result;
			}
			auto u8() -> std::uint8_t
			{
				const auto value = take(1);
				return value.empty() ? 0U : value[0];
			}
			auto flag() -> bool
			{
				const auto value = u8();
				Good             = Good && value <= 1U;
				return value == 1U;
			}
			auto u32() -> std::uint32_t
			{
				auto value = std::uint32_t{ 0 };
				for (const auto byte : take(4)) {
					value = (value << 8U) | byte;
				}
				return value;
			}
			auto u64() -> std::uint64_t
			{
				auto value = std::uint64_t{ 0 };
				for (const auto byte : take(8)) {
					value = (value << 8U) | byte;
				}
				return value;
			}
			auto text(const std::size_t aMaximum) -> std::string
			{
				const auto size = u32();
				if (size > aMaximum) {
					Good = false;
					return { };
				}
				const auto bytes = take(size);
				return { bytes.begin(), bytes.end() };
			}
			auto raw() -> std::array<std::uint8_t, 32>
			{
				auto       value = std::array<std::uint8_t, 32>{ };
				const auto bytes = take(32);
				if (!bytes.empty()) {
					std::ranges::copy(bytes, value.begin());
				}
				return value;
			}
			/// An enumeration value no greater than `aLast`.
			template <typename TEnum>
			auto enumeration(const TEnum aLast) -> TEnum
			{
				const auto value = u8();
				Good             = Good && value <= static_cast<std::uint8_t>(aLast);
				return static_cast<TEnum>(value);
			}
		};

		void write_blob(writer& aOut, const std::optional<git_blob_oid>& aObject)
		{
			aOut.u8(aObject ? static_cast<std::uint8_t>(aObject->algorithm()) : 0U);
			if (aObject) {
				aOut.raw(aObject->bytes());
			}
		}

		auto read_algorithm(reader& aIn) -> git_hash_algorithm
		{
			return aIn.enumeration(git_hash_algorithm::sha256);
		}

		void write_entry(writer& aOut, const manifest_entry& aEntry)
		{
			aOut.text(aEntry.Path);
			aOut.u8(static_cast<std::uint8_t>(aEntry.Kind));
			aOut.u8(static_cast<std::uint8_t>(aEntry.Origin));
			aOut.u8(static_cast<std::uint8_t>(aEntry.Availability));
			aOut.u32(aEntry.Mode);
			aOut.flag(aEntry.ModeFromIndex);
			aOut.u64(aEntry.Size);
			aOut.flag(aEntry.Content.has_value());
			if (aEntry.Content) {
				aOut.raw(aEntry.Content->bytes());
			}
			aOut.u8(static_cast<std::uint8_t>(aEntry.Profile.Kind));
			aOut.u8(static_cast<std::uint8_t>(aEntry.Profile.LineEndings));
			aOut.flag(aEntry.Profile.FinalNewline);
			aOut.flag(aEntry.Profile.LfsPointer);
			write_blob(aOut, aEntry.Staged);
			aOut.flag(aEntry.Unmerged);
			aOut.flag(aEntry.IntentToAdd);
			aOut.flag(aEntry.FilterManaged);
		}

		auto read_entry(reader& aIn) -> manifest_entry
		{
			auto entry          = manifest_entry{ };
			entry.Path          = aIn.text(maximum_workspace_path_bytes);
			entry.Kind          = aIn.enumeration(manifest_entry_kind::directory);
			entry.Origin        = aIn.enumeration(manifest_origin::unversioned);
			entry.Availability  = aIn.enumeration(manifest_availability::not_materialized);
			entry.Mode          = aIn.u32();
			entry.ModeFromIndex = aIn.flag();
			entry.Size          = aIn.u64();
			if (aIn.flag()) {
				entry.Content = cycle_material_digest{ aIn.raw() };
			}
			entry.Profile.Kind         = aIn.enumeration(material_text_kind::binary);
			entry.Profile.LineEndings  = aIn.enumeration(material_line_endings::mixed);
			entry.Profile.FinalNewline = aIn.flag();
			entry.Profile.LfsPointer   = aIn.flag();
			entry.Profile.Size         = entry.Kind == manifest_entry_kind::file && entry.Content ? entry.Size : 0U;
			if (const auto algorithm = read_algorithm(aIn); algorithm != git_hash_algorithm::invalid) {
				entry.Staged = git_blob_oid{ algorithm, aIn.raw() };
			}
			entry.Unmerged      = aIn.flag();
			entry.IntentToAdd   = aIn.flag();
			entry.FilterManaged = aIn.flag();
			return entry;
		}

		auto admitted(const std::string& aPath) -> bool
		{
			return admit_workspace_path(aPath) == workspace_path_refusal::none;
		}

		template <typename TRange, typename TProjection>
		auto sorted_unique(const TRange& aRange, TProjection aProjection) -> bool
		{
			return std::ranges::adjacent_find(aRange, [&](const auto& aLeft, const auto& aRight) { return !(aProjection(aLeft) < aProjection(aRight)); })
				== std::ranges::end(aRange);
		}

		auto valid_entry(const manifest_entry& aEntry) -> bool
		{
			const auto captured = aEntry.Availability == manifest_availability::captured;
			const auto readable = aEntry.Kind == manifest_entry_kind::file || aEntry.Kind == manifest_entry_kind::symlink;
			return admitted(aEntry.Path) && (captured == (readable && aEntry.Content.has_value()))
				&& ((aEntry.Kind == manifest_entry_kind::file && aEntry.Content) ? aEntry.Profile.Size == aEntry.Size : aEntry.Profile == material_profile{ })
				&& (!aEntry.Staged || aEntry.Staged->valid()) && (!aEntry.Content || aEntry.Content->valid());
		}
	}

	auto validate_manifest(const cycle_manifest& aManifest) -> std::optional<manifest_error>
	{
		if (aManifest.Entries.size() > maximum_cycle_capture_entries || aManifest.Deletions.size() > maximum_cycle_capture_entries
			|| aManifest.Exclusions.size() > maximum_cycle_capture_entries || aManifest.Scope.size() > maximum_cycle_scope_paths) {
			return manifest_error::bounds;
		}
		if (validate_scope_paths(aManifest.Scope) != cycle_refusal::none || aManifest.ScopeVersion == 0) {
			return manifest_error::format;
		}
		if (aManifest.HeadCommit && (!aManifest.HeadCommit->valid() || aManifest.HeadCommit->algorithm() != aManifest.ObjectFormat)) {
			return manifest_error::format;
		}
		const auto path     = [](const manifest_entry& aEntry) -> const std::string& { return aEntry.Path; };
		const auto excluded = [](const manifest_exclusion& aExclusion) -> const std::string& { return aExclusion.Path; };
		if (!sorted_unique(aManifest.Entries, path) || !sorted_unique(aManifest.Deletions, std::identity{ }) || !sorted_unique(aManifest.Exclusions, excluded)) {
			return manifest_error::paths;
		}
		if (!std::ranges::all_of(aManifest.Entries, valid_entry) || !std::ranges::all_of(aManifest.Deletions, admitted)
			|| !std::ranges::all_of(aManifest.Exclusions, [](const manifest_exclusion& aExclusion) { return admitted(aExclusion.Path); })) {
			return manifest_error::paths;
		}
		const auto present = [&aManifest](const std::string& aPath) { return find_entry(aManifest, aPath) != nullptr; };
		if (std::ranges::any_of(aManifest.Deletions, present)) {
			return manifest_error::paths;
		}
		return std::nullopt;
	}

	auto encode_manifest(const cycle_manifest& aManifest) -> std::vector<std::uint8_t>
	{
		auto out = writer{ };
		out.Bytes.insert(out.Bytes.end(), magic.begin(), magic.end());
		out.u64(aManifest.ScopeVersion);
		out.u32(static_cast<std::uint32_t>(aManifest.Scope.size()));
		for (const auto& path : aManifest.Scope) {
			out.text(path);
		}
		out.u8(static_cast<std::uint8_t>(aManifest.RootKind));
		out.u8(static_cast<std::uint8_t>(aManifest.Coverage));
		out.u8(static_cast<std::uint8_t>(aManifest.Quality));
		out.u8(static_cast<std::uint8_t>(aManifest.ObjectFormat));
		out.flag(aManifest.HeadReference.has_value());
		if (aManifest.HeadReference) {
			out.text(*aManifest.HeadReference);
		}
		out.flag(aManifest.HeadCommit.has_value());
		if (aManifest.HeadCommit) {
			out.raw(aManifest.HeadCommit->bytes());
		}
		out.flag(aManifest.IndexUnmerged);
		out.flag(aManifest.IndexSparse);
		out.flag(aManifest.IndexSplit);
		out.u64(aManifest.RefusedPaths);
		out.u32(static_cast<std::uint32_t>(aManifest.Entries.size()));
		for (const auto& entry : aManifest.Entries) {
			write_entry(out, entry);
		}
		out.u32(static_cast<std::uint32_t>(aManifest.Deletions.size()));
		for (const auto& path : aManifest.Deletions) {
			out.text(path);
		}
		out.u32(static_cast<std::uint32_t>(aManifest.Exclusions.size()));
		for (const auto& exclusion : aManifest.Exclusions) {
			out.text(exclusion.Path);
			out.flag(exclusion.Directory);
			out.u8(static_cast<std::uint8_t>(exclusion.Reason));
		}
		return std::move(out.Bytes);
	}

	auto decode_manifest(const std::span<const std::uint8_t> aBytes) -> std::expected<cycle_manifest, manifest_error>
	{
		auto       in   = reader{ .Bytes = aBytes };
		const auto head = in.take(magic.size());
		if (!in.Good || !std::ranges::equal(head, magic, [](const std::uint8_t aByte, const char aCharacter) { return aByte == static_cast<std::uint8_t>(aCharacter); })) {
			return std::unexpected(in.Good ? manifest_error::format : manifest_error::truncated);
		}
		auto manifest         = cycle_manifest{ };
		manifest.ScopeVersion = in.u64();
		const auto scopeCount = in.u32();
		if (scopeCount > maximum_cycle_scope_paths) {
			return std::unexpected(manifest_error::bounds);
		}
		for (std::uint32_t index = 0; index < scopeCount && in.Good; ++index) {
			manifest.Scope.push_back(in.text(maximum_cycle_scope_path_bytes));
		}
		manifest.RootKind     = in.enumeration(workspace_root_kind::git_linked_worktree);
		manifest.Coverage     = in.enumeration(cycle_capture_coverage::partial);
		manifest.Quality      = in.enumeration(cycle_capture_quality::immutable_source);
		manifest.ObjectFormat = in.enumeration(git_hash_algorithm::sha256);
		if (in.flag()) {
			manifest.HeadReference = in.text(maximum_workspace_path_bytes);
		}
		if (in.flag()) {
			manifest.HeadCommit = git_commit_oid{ manifest.ObjectFormat, in.raw() };
		}
		manifest.IndexUnmerged = in.flag();
		manifest.IndexSparse   = in.flag();
		manifest.IndexSplit    = in.flag();
		manifest.RefusedPaths  = in.u64();
		const auto entryCount  = in.u32();
		if (entryCount > maximum_cycle_capture_entries) {
			return std::unexpected(manifest_error::bounds);
		}
		manifest.Entries.reserve(in.Good ? entryCount : 0U);
		for (std::uint32_t index = 0; index < entryCount && in.Good; ++index) {
			manifest.Entries.push_back(read_entry(in));
		}
		const auto deletionCount = in.u32();
		if (deletionCount > maximum_cycle_capture_entries) {
			return std::unexpected(manifest_error::bounds);
		}
		for (std::uint32_t index = 0; index < deletionCount && in.Good; ++index) {
			manifest.Deletions.push_back(in.text(maximum_workspace_path_bytes));
		}
		const auto exclusionCount = in.u32();
		if (exclusionCount > maximum_cycle_capture_entries) {
			return std::unexpected(manifest_error::bounds);
		}
		for (std::uint32_t index = 0; index < exclusionCount && in.Good; ++index) {
			auto exclusion      = manifest_exclusion{ .Path = in.text(maximum_workspace_path_bytes) };
			exclusion.Directory = in.flag();
			exclusion.Reason    = in.enumeration(workspace_exclusion_reason::requested);
			manifest.Exclusions.push_back(std::move(exclusion));
		}
		if (!in.Good) {
			return std::unexpected(manifest_error::truncated);
		}
		if (in.Offset != aBytes.size()) {
			return std::unexpected(manifest_error::format);
		}
		if (const auto invalid = validate_manifest(manifest)) {
			return std::unexpected(*invalid);
		}
		// Canonical: re-encoding must give the same bytes.
		if (!std::ranges::equal(encode_manifest(manifest), aBytes)) {
			return std::unexpected(manifest_error::format);
		}
		return manifest;
	}

	auto observe_manifest(const cycle_manifest& aManifest, const cycle_material_digest& aMaterial, const std::uint64_t aWorkspaceVersion)
		-> cycle_capture_observation
	{
		const auto unavailable = std::ranges::count_if(aManifest.Entries, [](const manifest_entry& aEntry) { return aEntry.Availability != manifest_availability::captured; });
		return {
			.ScopeVersion      = aManifest.ScopeVersion,
			.Material          = aMaterial,
			.Entries           = aManifest.Entries.size(),
			.ExplicitDeletions = aManifest.Deletions.size(),
			.Unavailable       = static_cast<std::uint64_t>(unavailable),
			.Coverage          = aManifest.Coverage,
			.Quality           = aManifest.Quality,
			.WorkspaceVersion  = aWorkspaceVersion,
		};
	}

	auto manifest_content(const cycle_manifest& aManifest) -> std::vector<std::pair<cycle_material_digest, std::uint64_t>>
	{
		auto content = std::vector<std::pair<cycle_material_digest, std::uint64_t>>{ };
		for (const auto& entry : aManifest.Entries) {
			if (entry.Content) {
				content.emplace_back(*entry.Content, entry.Size);
			}
		}
		std::ranges::sort(content);
		const auto [first, last] = std::ranges::unique(content);
		content.erase(first, last);
		return content;
	}

	auto find_entry(const cycle_manifest& aManifest, const std::string_view aPath) -> const manifest_entry*
	{
		const auto found = std::ranges::lower_bound(aManifest.Entries, aPath, { }, [](const manifest_entry& aEntry) { return std::string_view{ aEntry.Path }; });
		return found != aManifest.Entries.end() && found->Path == aPath ? &*found : nullptr;
	}

	auto compare_capture_passes(const std::span<const capture_stamp> aFirst, const std::span<const capture_stamp> aSecond, const std::size_t aExamples)
		-> capture_pass_comparison
	{
		auto       result = capture_pass_comparison{ };
		const auto note   = [&result, aExamples](const std::string& aPath) {
			++result.Changed;
			if (result.Examples.size() < aExamples) {
				result.Examples.push_back(aPath);
			}
		};
		auto left  = aFirst.begin();
		auto right = aSecond.begin();
		while (left != aFirst.end() || right != aSecond.end()) {
			if (right == aSecond.end() || (left != aFirst.end() && left->Path < right->Path)) {
				note(left->Path);
				++left;
			} else if (left == aFirst.end() || right->Path < left->Path) {
				note(right->Path);
				++right;
			} else {
				if (*left != *right) {
					note(left->Path);
				}
				++left;
				++right;
			}
		}
		return result;
	}
}
