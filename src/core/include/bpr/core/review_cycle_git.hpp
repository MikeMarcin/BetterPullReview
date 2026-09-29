#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_GIT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_GIT_HPP

#include "bpr/core/epoch_model.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Git metadata read as data for workspace capture (RFC-BPR-0054).
/// \details A capture learns which paths a checkout tracks, their staged
/// modes and object identities, and which are sparse or unmerged, by parsing
/// the index bytes the host read. Git itself is never run, so no hook,
/// filter, `fsmonitor` or lazy fetch can execute.
namespace bpr::core {
	enum class git_index_error : std::uint8_t {
		truncated,
		/// Not a `DIRC` index.
		signature,
		/// An index version other than 2, 3 or 4.
		unsupported_version,
		/// More entries than the capture admits.
		bounds,
		/// An entry path is empty, unsorted or longer than the data allows.
		malformed,
	};

	/// \brief One staged entry. The object identity is tagged with the
	/// repository's hash algorithm and is not a hash of working-tree bytes.
	struct git_index_entry
	{
		std::string   Path;
		std::uint32_t Mode = 0;
		git_blob_oid  Object;
		/// Zero for a merged entry; 1-3 for the sides of a conflict.
		std::uint8_t Stage = 0;
		/// Sparse checkout: the working tree is not expected to hold it.
		bool SkipWorktree = false;
		/// Recorded with `git add -N`: tracked, with no staged content.
		bool IntentToAdd                               = false;
		auto operator<=>(const git_index_entry&) const = default;
	};

	struct git_index
	{
		std::uint32_t                Version = 0;
		std::vector<git_index_entry> Entries;
		/// A sparse index: some entries are directories standing for
		/// everything beneath them.
		bool SparseDirectories = false;
		/// A split index: entries live partly in a shared file this parser
		/// does not read, so the tracked set is incomplete.
		bool SplitIndex = false;
		/// Some path has a nonzero stage.
		bool Unmerged = false;
	};

	/// \brief Parses index versions 2, 3 and 4 for `aAlgorithm`.
	/// \details The trailing checksum is not verified; the host rereads the
	/// index after capture and treats any change as instability.
	[[nodiscard]] auto parse_git_index(std::span<const std::uint8_t> aBytes, git_hash_algorithm aAlgorithm, std::uint64_t aMaximumEntries)
		-> std::expected<git_index, git_index_error>;

	/// \brief The repository hash algorithm named by `extensions.objectFormat`
	/// in a Git config file; SHA-1 when absent, invalid when unrecognized.
	[[nodiscard]] auto git_object_format(std::string_view aConfig) -> git_hash_algorithm;

	/// \brief What `HEAD` names: a symbolic reference, a detached commit, or
	/// neither when it is malformed.
	struct git_head
	{
		std::optional<std::string>    Reference;
		std::optional<git_commit_oid> Commit;
	};
	[[nodiscard]] auto parse_git_head(std::string_view aHead, git_hash_algorithm aAlgorithm) -> git_head;
	/// \brief A loose reference file's commit.
	[[nodiscard]] auto parse_git_reference(std::string_view aContents, git_hash_algorithm aAlgorithm) -> std::optional<git_commit_oid>;
	/// \brief A reference's commit in `packed-refs`.
	[[nodiscard]] auto find_packed_reference(std::string_view aPackedRefs, std::string_view aReference, git_hash_algorithm aAlgorithm)
		-> std::optional<git_commit_oid>;
}

#endif
