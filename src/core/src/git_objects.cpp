#include "bpr/core/git_objects.hpp"

#include "bpr/core/review_cycle_material.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto type_name(const git_object_type aType) noexcept -> std::string_view
		{
			switch (aType) {
			case git_object_type::blob:
				return "blob";
			case git_object_type::tree:
				return "tree";
			case git_object_type::commit:
				return "commit";
			}
			return "blob";
		}

		[[nodiscard]] auto as_bytes(const std::string_view aText) noexcept -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		void append(std::vector<std::uint8_t>& aOut, const std::string_view aText)
		{
			aOut.insert(aOut.end(), aText.begin(), aText.end());
		}

		void append_hex(std::vector<std::uint8_t>& aOut, const canonical_git_oid& aOid)
		{
			append(aOut, aOid.view());
		}

		[[nodiscard]] auto octal_mode(const std::uint32_t aMode) -> std::string
		{
			auto text  = std::string{ };
			auto value = aMode;
			do {
				text.insert(text.begin(), static_cast<char>('0' + (value & 7U)));
				value >>= 3U;
			} while (value != 0U);
			return text;
		}

		[[nodiscard]] auto valid_tree_path(const std::string_view aPath) noexcept -> bool
		{
			if (aPath.empty() || aPath.front() == '/' || aPath.back() == '/') {
				return false;
			}
			auto start = std::size_t{ 0 };
			while (start <= aPath.size()) {
				const auto end     = aPath.find('/', start);
				const auto segment = aPath.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
				if (segment.empty() || segment == "." || segment == ".." || segment.find('\0') != std::string_view::npos) {
					return false;
				}
				if (end == std::string_view::npos) {
					break;
				}
				start = end + 1U;
			}
			return true;
		}

		struct tree_builder
		{
			std::span<const git_tree_entry> Entries;
			git_hash_algorithm              Algorithm = git_hash_algorithm::invalid;
			std::vector<git_tree_object>    Trees;

			/// Builds the tree for `aPrefix` (empty or ending in `/`) from the
			/// entries in [aBegin, aEnd), all of which lie beneath it.
			auto build(const std::string& aPrefix, std::size_t aBegin, const std::size_t aEnd) -> std::expected<git_tree_oid, git_tree_error>
			{
				auto bytes = std::vector<std::uint8_t>{ };
				auto files = std::vector<std::string_view>{ };
				auto dirs  = std::vector<std::string_view>{ };
				while (aBegin < aEnd) {
					const auto& entry = Entries[aBegin];
					const auto  rest  = std::string_view{ entry.Path }.substr(aPrefix.size());
					const auto  slash = rest.find('/');
					if (slash == std::string_view::npos) {
						files.push_back(rest);
						append(bytes, octal_mode(entry.Mode));
						bytes.push_back(' ');
						append(bytes, rest);
						bytes.push_back(0U);
						bytes.insert(bytes.end(), entry.Object.bytes().begin(), entry.Object.bytes().begin() + static_cast<std::ptrdiff_t>(entry.Object.byte_count()));
						++aBegin;
						continue;
					}
					const auto name  = rest.substr(0U, slash);
					const auto child = aPrefix + std::string{ name } + "/";
					auto       end   = aBegin;
					while (end < aEnd && std::string_view{ Entries[end].Path }.starts_with(child)) {
						++end;
					}
					dirs.push_back(name);
					const auto built = build(child, aBegin, end);
					if (!built) {
						return std::unexpected{ built.error() };
					}
					append(bytes, "40000 ");
					append(bytes, name);
					bytes.push_back(0U);
					bytes.insert(bytes.end(), built->bytes().begin(), built->bytes().begin() + static_cast<std::ptrdiff_t>(built->byte_count()));
					aBegin = end;
				}
				std::ranges::sort(files);
				for (const auto directory : dirs) {
					if (std::ranges::binary_search(files, directory)) {
						return std::unexpected{ git_tree_error::file_directory_conflict };
					}
				}
				const auto digest = git_object_digest(Algorithm, git_object_type::tree, bytes);
				if (!digest) {
					return std::unexpected{ git_tree_error::algorithm };
				}
				const auto id = git_tree_oid{ Algorithm, *digest };
				Trees.push_back({ .Path = aPrefix.empty() ? std::string{ } : aPrefix.substr(0U, aPrefix.size() - 1U), .Id = id, .Bytes = std::move(bytes) });
				return id;
			}
		};

		[[nodiscard]] auto valid_signature_text(const std::string_view aText) noexcept -> bool
		{
			if (aText.empty() || aText.size() > maximum_git_signature_bytes || aText.front() == ' ' || aText.back() == ' ') {
				return false;
			}
			return aText.find_first_of(std::string_view{ "<>\n\r\0", 5 }) == std::string_view::npos;
		}

		void append_signature(std::vector<std::uint8_t>& aOut, const std::string_view aField, const git_signature& aSignature)
		{
			append(aOut, aField);
			aOut.push_back(' ');
			append(aOut, aSignature.Name);
			append(aOut, " <");
			append(aOut, aSignature.Email);
			append(aOut, "> ");
			append(aOut, std::to_string(aSignature.Seconds));
			aOut.push_back(' ');
			const auto minutes = aSignature.OffsetMinutes < 0 ? -aSignature.OffsetMinutes : aSignature.OffsetMinutes;
			aOut.push_back(aSignature.OffsetMinutes < 0 ? '-' : '+');
			const auto hours = minutes / 60;
			const auto rest  = minutes % 60;
			aOut.push_back(static_cast<std::uint8_t>('0' + hours / 10));
			aOut.push_back(static_cast<std::uint8_t>('0' + hours % 10));
			aOut.push_back(static_cast<std::uint8_t>('0' + rest / 10));
			aOut.push_back(static_cast<std::uint8_t>('0' + rest % 10));
			aOut.push_back('\n');
		}

		[[nodiscard]] auto valid_signature(const git_signature& aSignature) noexcept -> bool
		{
			return valid_signature_text(aSignature.Name) && valid_signature_text(aSignature.Email) && aSignature.OffsetMinutes > -24 * 60
				&& aSignature.OffsetMinutes < 24 * 60 && aSignature.Seconds >= 0;
		}
	}

	auto parse_git_entry_oid(const std::string_view aText) noexcept -> std::optional<git_entry_oid>
	{
		const auto blob = parse_git_blob_oid(aText);
		if (!blob) {
			return std::nullopt;
		}
		return retag<git_entry_oid_kind>(*blob);
	}

	auto canonical_hex(const git_entry_oid& aOid) noexcept -> canonical_git_oid
	{
		return canonical_hex(retag<git_blob_oid_kind>(aOid));
	}

	git_object_hasher::git_object_hasher(const git_hash_algorithm aAlgorithm, const git_object_type aType, const std::uint64_t aSize) noexcept
		: Algorithm{ aAlgorithm }
		, Declared{ aSize }
	{
		const auto size   = std::to_string(aSize);
		auto       header = std::string{ type_name(aType) };
		header.push_back(' ');
		header += size;
		header.push_back('\0');
		Sha1.update(as_bytes(header));
		Sha256.update(as_bytes(header));
	}

	void git_object_hasher::update(const std::span<const std::uint8_t> aBytes) noexcept
	{
		Delivered += aBytes.size();
		if (Algorithm == git_hash_algorithm::sha1) {
			Sha1.update(aBytes);
		} else if (Algorithm == git_hash_algorithm::sha256) {
			Sha256.update(aBytes);
		}
	}

	auto git_object_hasher::finish() noexcept -> std::optional<std::array<std::uint8_t, 32>>
	{
		if (Delivered != Declared) {
			return std::nullopt;
		}
		auto result = std::array<std::uint8_t, 32>{ };
		if (Algorithm == git_hash_algorithm::sha1) {
			const auto digest = Sha1.finish();
			std::ranges::copy(digest, result.begin());
			return result;
		}
		if (Algorithm == git_hash_algorithm::sha256) {
			return Sha256.finish();
		}
		return std::nullopt;
	}

	auto git_object_digest(const git_hash_algorithm aAlgorithm, const git_object_type aType, const std::span<const std::uint8_t> aContent)
		-> std::optional<std::array<std::uint8_t, 32>>
	{
		auto hasher = git_object_hasher{ aAlgorithm, aType, aContent.size() };
		hasher.update(aContent);
		return hasher.finish();
	}

	auto git_blob_identity(const std::span<const std::uint8_t> aBytes, const git_hash_algorithm aAlgorithm) -> git_blob_oid
	{
		const auto digest = git_object_digest(aAlgorithm, git_object_type::blob, aBytes);
		return digest ? git_blob_oid{ aAlgorithm, *digest } : git_blob_oid{ };
	}

	auto valid_git_entry_mode(const std::uint32_t aMode) noexcept -> bool
	{
		return aMode == git_mode_regular || aMode == git_mode_executable || aMode == git_mode_symlink || aMode == git_mode_gitlink;
	}

	auto build_git_trees(const std::span<const git_tree_entry> aEntries, const git_hash_algorithm aAlgorithm) -> std::expected<git_tree_build, git_tree_error>
	{
		if (aAlgorithm != git_hash_algorithm::sha1 && aAlgorithm != git_hash_algorithm::sha256) {
			return std::unexpected{ git_tree_error::algorithm };
		}
		if (aEntries.size() > maximum_git_tree_entries) {
			return std::unexpected{ git_tree_error::bounds };
		}
		for (std::size_t index = 0; index < aEntries.size(); ++index) {
			const auto& entry = aEntries[index];
			if (!valid_tree_path(entry.Path)) {
				return std::unexpected{ git_tree_error::path };
			}
			if (!valid_git_entry_mode(entry.Mode)) {
				return std::unexpected{ git_tree_error::mode };
			}
			if (!entry.Object.valid() || entry.Object.algorithm() != aAlgorithm) {
				return std::unexpected{ git_tree_error::algorithm };
			}
			if (index > 0U && !(aEntries[index - 1U].Path < entry.Path)) {
				return std::unexpected{ git_tree_error::order };
			}
		}
		auto builder = tree_builder{ .Entries = aEntries, .Algorithm = aAlgorithm, .Trees = { } };
		auto root    = builder.build(std::string{ }, 0U, aEntries.size());
		if (!root) {
			return std::unexpected{ root.error() };
		}
		return git_tree_build{ .Root = *root, .Trees = std::move(builder.Trees) };
	}

	auto encode_git_commit(const git_commit_content& aCommit) -> std::expected<std::vector<std::uint8_t>, git_commit_error>
	{
		const auto algorithm = aCommit.Tree.algorithm();
		if (!aCommit.Tree.valid()
			|| std::ranges::any_of(aCommit.Parents, [algorithm](const git_commit_oid& aParent) { return !aParent.valid() || aParent.algorithm() != algorithm; })) {
			return std::unexpected{ git_commit_error::algorithm };
		}
		if (!valid_signature(aCommit.Author) || !valid_signature(aCommit.Committer)) {
			return std::unexpected{ git_commit_error::signature };
		}
		const auto message = as_bytes(aCommit.Message);
		if (aCommit.Message.empty() || aCommit.Message.size() > maximum_git_message_bytes || aCommit.Message.back() != '\n'
			|| classify_material(message).Kind != material_text_kind::utf8) {
			return std::unexpected{ git_commit_error::message };
		}
		auto out = std::vector<std::uint8_t>{ };
		append(out, "tree ");
		append_hex(out, canonical_hex(aCommit.Tree));
		out.push_back('\n');
		for (const auto& parent : aCommit.Parents) {
			append(out, "parent ");
			append_hex(out, canonical_hex(parent));
			out.push_back('\n');
		}
		append_signature(out, "author", aCommit.Author);
		append_signature(out, "committer", aCommit.Committer);
		out.push_back('\n');
		out.insert(out.end(), message.begin(), message.end());
		return out;
	}

	auto git_commit_identity(const git_commit_content& aCommit) -> std::expected<git_commit_oid, git_commit_error>
	{
		const auto encoded = encode_git_commit(aCommit);
		if (!encoded) {
			return std::unexpected{ encoded.error() };
		}
		const auto digest = git_object_digest(aCommit.Tree.algorithm(), git_object_type::commit, *encoded);
		if (!digest) {
			return std::unexpected{ git_commit_error::algorithm };
		}
		return git_commit_oid{ aCommit.Tree.algorithm(), *digest };
	}
	auto verified_commit_tree(const std::span<const std::uint8_t> aCommitObject, const git_commit_oid& aCommit) -> std::optional<git_tree_oid>
	{
		const auto digest = git_object_digest(aCommit.algorithm(), git_object_type::commit, aCommitObject);
		if (!aCommit.valid() || !digest || git_commit_oid{ aCommit.algorithm(), *digest } != aCommit) {
			return std::nullopt;
		}
		const auto text   = std::string_view{ reinterpret_cast<const char*>(aCommitObject.data()), aCommitObject.size() };
		const auto length = aCommit.byte_count() * 2U;
		if (!text.starts_with("tree ") || text.size() < 5U + length + 1U || text[5U + length] != '\n') {
			return std::nullopt;
		}
		const auto tree = parse_git_tree_oid(text.substr(5U, length));
		if (!tree || tree->algorithm() != aCommit.algorithm()) {
			return std::nullopt;
		}
		return tree;
	}

	auto line_feed_form(const std::span<const std::uint8_t> aBytes) -> std::optional<std::vector<std::uint8_t>>
	{
		auto out   = std::vector<std::uint8_t>{ };
		auto found = false;
		out.reserve(aBytes.size());
		for (std::size_t index = 0; index < aBytes.size(); ++index) {
			if (aBytes[index] == '\r' && index + 1U < aBytes.size() && aBytes[index + 1U] == '\n') {
				found = true;
				continue;
			}
			out.push_back(aBytes[index]);
		}
		if (!found) {
			return std::nullopt;
		}
		return out;
	}
}
