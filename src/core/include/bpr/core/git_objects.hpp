#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GIT_OBJECTS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GIT_OBJECTS_HPP

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/sha1.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Canonical Git blob, tree and commit encodings and their identities
/// (RFC-BPR-0059).
/// \details A publication candidate is checked against these encodings
/// before anything is sent: the identity an object store or a provider
/// returns must be the one computed here from the exact bytes, tree entries
/// and commit metadata, for the repository's own object format. Nothing here
/// converts an identity between formats.
namespace bpr::core {
	struct git_entry_oid_kind;
	/// \brief What a tree entry names: a blob, or for a gitlink a commit.
	using git_entry_oid = git_object_oid<git_entry_oid_kind>;

	/// \brief The same identity bytes under another object kind.
	template <typename TTo, typename TFrom>
	[[nodiscard]] constexpr auto retag(const git_object_oid<TFrom>& aOid) noexcept -> git_object_oid<TTo>
	{
		return git_object_oid<TTo>{ aOid.algorithm(), aOid.bytes() };
	}
	[[nodiscard]] auto parse_git_entry_oid(std::string_view aText) noexcept -> std::optional<git_entry_oid>;
	[[nodiscard]] auto canonical_hex(const git_entry_oid& aOid) noexcept -> canonical_git_oid;

	enum class git_object_type : std::uint8_t {
		blob,
		tree,
		commit,
	};

	/// \brief Hashes one object's header and content, delivered in pieces.
	/// \details The size is declared up front, as Git's header requires; the
	/// digest is refused when the delivered bytes do not add up to it.
	class git_object_hasher
	{
	public:
		git_object_hasher(git_hash_algorithm aAlgorithm, git_object_type aType, std::uint64_t aSize) noexcept;
		void update(std::span<const std::uint8_t> aBytes) noexcept;
		/// \brief The identity, or nothing for an invalid algorithm or a size
		/// that does not match what was delivered. Call once.
		[[nodiscard]] auto finish() noexcept -> std::optional<std::array<std::uint8_t, 32>>;

	private:
		git_hash_algorithm Algorithm = git_hash_algorithm::invalid;
		std::uint64_t      Declared  = 0U;
		std::uint64_t      Delivered = 0U;
		sha1_stream        Sha1;
		sha256_stream      Sha256;
	};

	/// \brief The identity of one whole object.
	[[nodiscard]] auto git_object_digest(git_hash_algorithm aAlgorithm, git_object_type aType, std::span<const std::uint8_t> aContent)
		-> std::optional<std::array<std::uint8_t, 32>>;
	/// \brief The blob identity of exactly these bytes; invalid for an invalid algorithm.
	[[nodiscard]] auto git_blob_identity(std::span<const std::uint8_t> aBytes, git_hash_algorithm aAlgorithm) -> git_blob_oid;

	/// \brief One non-tree entry of a complete tree listing, by its full path.
	struct git_tree_entry
	{
		/// `/`-separated from the tree's root; a valid cycle path.
		std::string   Path;
		std::uint32_t Mode = 0;
		git_entry_oid Object;
		auto          operator<=>(const git_tree_entry&) const = default;
	};

	/// \brief One encoded tree object.
	struct git_tree_object
	{
		/// The directory it describes; empty for the root.
		std::string               Path;
		git_tree_oid              Id;
		std::vector<std::uint8_t> Bytes;
		auto                      operator<=>(const git_tree_object&) const = default;
	};

	struct git_tree_build
	{
		git_tree_oid Root;
		/// Every tree, children before their parent, the root last.
		std::vector<git_tree_object> Trees;
	};

	enum class git_tree_error : std::uint8_t {
		/// Entries are not in strictly increasing path-byte order.
		order,
		/// A path is not a valid root-relative path.
		path,
		/// A mode other than regular, executable, symbolic link or gitlink.
		mode,
		/// An identity is invalid or of another object format.
		algorithm,
		/// A file and a directory share a path.
		file_directory_conflict,
		/// More entries than a candidate admits.
		bounds,
	};

	inline constexpr std::size_t maximum_git_tree_entries = 250'000U;

	/// \brief True for the four modes a tree entry other than a tree may carry.
	[[nodiscard]] auto valid_git_entry_mode(std::uint32_t aMode) noexcept -> bool;

	/// \brief Builds every tree object of a complete listing sorted by path
	/// bytes (which is Git's tree order). An empty listing is the empty tree.
	[[nodiscard]] auto build_git_trees(std::span<const git_tree_entry> aEntries, git_hash_algorithm aAlgorithm) -> std::expected<git_tree_build, git_tree_error>;

	/// \brief An author or committer line's fields.
	struct git_signature
	{
		std::string  Name;
		std::string  Email;
		std::int64_t Seconds = 0;
		/// Minutes east of UTC.
		std::int32_t OffsetMinutes                           = 0;
		auto         operator<=>(const git_signature&) const = default;
	};

	/// \brief One commit's canonical content. The message is kept exactly.
	struct git_commit_content
	{
		git_tree_oid                Tree;
		std::vector<git_commit_oid> Parents;
		git_signature               Author;
		git_signature               Committer;
		std::string                 Message;
		auto                        operator<=>(const git_commit_content&) const = default;
	};

	enum class git_commit_error : std::uint8_t {
		/// Tree and parents are not all valid identities of one format.
		algorithm,
		/// A name or email is empty, has surrounding spaces, or holds `<`,
		/// `>`, a line break or NUL; or the offset is not below a day.
		signature,
		/// The message is empty, holds NUL, is not UTF-8 or does not end with
		/// a line feed.
		message,
	};

	inline constexpr std::size_t maximum_git_signature_bytes = 256U;
	inline constexpr std::size_t maximum_git_message_bytes   = 64U * 1024U;

	[[nodiscard]] auto encode_git_commit(const git_commit_content& aCommit) -> std::expected<std::vector<std::uint8_t>, git_commit_error>;
	[[nodiscard]] auto git_commit_identity(const git_commit_content& aCommit) -> std::expected<git_commit_oid, git_commit_error>;

	/// \brief The tree a commit object names, when the object's bytes hash to
	/// `aCommit`; nothing otherwise.
	[[nodiscard]] auto verified_commit_tree(std::span<const std::uint8_t> aCommitObject, const git_commit_oid& aCommit) -> std::optional<git_tree_oid>;

	/// \brief The bytes with every CRLF pair made LF, or nothing when there
	/// is no CRLF pair. A lone CR is kept. This is the only line-ending
	/// transformation a candidate may name.
	[[nodiscard]] auto line_feed_form(std::span<const std::uint8_t> aBytes) -> std::optional<std::vector<std::uint8_t>>;
}

#endif
