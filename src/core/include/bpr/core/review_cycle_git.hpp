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
/// \brief Git metadata read as data for workspace capture (RFC-BPR-0054,
/// RFC-BPR-0083).
/// \details A capture learns which paths a checkout tracks, their staged
/// modes and object identities, and which are sparse or unmerged, by parsing
/// the index bytes the host read. Git itself is never run, so no hook,
/// filter, `fsmonitor` or lazy fetch can execute. Where Git would refuse to
/// read a repository, or would read it in a way these readers do not, they
/// refuse with a named reason rather than guess.
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
		/// The trailing checksum is neither all zero nor the hash of the rest.
		checksum,
		/// An extension Git requires readers to understand (its signature does
		/// not start with `A`-`Z`) that is neither `link` nor `sdir`.
		required_extension,
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
	/// \details The trailing checksum is verified with the repository's hash
	/// algorithm, as Git verifies it; an all-zero trailer, which Git writes
	/// under `index.skipHash`, is accepted. Optional extensions are skipped.
	[[nodiscard]] auto parse_git_index(std::span<const std::uint8_t> aBytes, git_hash_algorithm aAlgorithm, std::uint64_t aMaximumEntries)
		-> std::expected<git_index, git_index_error>;

	/// \brief Why a repository's metadata is not read.
	enum class git_repository_refusal : std::uint8_t {
		/// The config is one Git itself would refuse to read.
		malformed_config,
		/// `core.repositoryformatversion` is above 1, below 0 or not a number.
		format_version,
		/// A version 1 repository names an extension these readers do not
		/// read (Git may know it; these readers decline to guess).
		unknown_extension,
		/// A version 0 repository names an extension Git honours only in
		/// version 1, which Git itself refuses.
		version_zero_extension,
		/// `extensions.objectFormat` names no hash Git knows.
		object_format,
		/// `extensions.refStorage` names no reference format Git knows.
		ref_storage,
		/// `extensions.refStorage = reftable`. Refused under the owner's
		/// RFC-BPR-0083 Q2 answer (a) of 2026-10-13; whether to read it is
		/// C3's evaluation.
		reftable,
		/// A key these readers use has a value Git refuses.
		invalid_value,
		/// The index is not one these readers can parse.
		index_format,
		/// The index fails its trailing checksum.
		index_checksum,
		/// The index carries a required extension these readers do not read.
		index_extension,
	};

	/// \brief A refusal and the config key or index part it concerns, for
	/// the words that name it.
	struct git_repository_problem
	{
		git_repository_refusal Reason = git_repository_refusal::malformed_config;
		/// The key as Git names it (`extensions.refstorage`), when one applies.
		std::string Key;
		auto        operator<=>(const git_repository_problem&) const = default;
	};

	/// \brief What a repository's config says about reading it.
	struct git_repository_format
	{
		git_hash_algorithm ObjectFormat = git_hash_algorithm::sha1;
		/// `extensions.worktreeConfig`: each worktree's `config.worktree` is
		/// read after the shared config.
		bool WorktreeConfig = false;
		/// `core.ignoreCase`: ignore and attribute patterns fold ASCII case.
		bool IgnoreCase                                      = false;
		auto operator<=>(const git_repository_format&) const = default;
	};

	/// \brief Reads the shared `config` as Git's setup code does:
	/// `core.repositoryformatversion`, every `extensions.*` key and
	/// `core.ignoreCase`.
	/// \details Includes are not followed, and the system and global files
	/// are never read. Git reads repository-format keys without includes too;
	/// an include that sets `core.ignoreCase` is a recorded divergence.
	/// The extensions read are `noop`, `noop-v1`, `objectFormat`,
	/// `partialClone`, `preciousObjects`, `worktreeConfig`,
	/// `relativeWorktrees` and `refStorage = files`. Each leaves the files
	/// these readers open unchanged, except that `worktreeConfig` adds
	/// `config.worktree` (see `apply_git_worktree_config`). A reftable
	/// repository is refused with its own reason; anything else in a
	/// version 1 repository, including `compatObjectFormat`, is refused as
	/// unknown.
	[[nodiscard]] auto read_git_repository_format(std::string_view aConfig) -> std::expected<git_repository_format, git_repository_problem>;

	/// \brief Applies a worktree's `config.worktree`, which Git reads after
	/// the shared config when `extensions.worktreeConfig` is set: its
	/// `core.ignoreCase` wins.
	[[nodiscard]] auto apply_git_worktree_config(std::string_view aConfig, git_repository_format& aFormat) -> std::optional<git_repository_problem>;

	/// \brief The repository refusal an index parse failure becomes.
	[[nodiscard]] auto git_index_problem(git_index_error aError) noexcept -> git_repository_problem;

	/// \brief A reference name these readers follow: under `refs/`, with no
	/// empty, `.`-led or `..` segment, no control character, space or
	/// `~^:?*[\`, no `@{` and no `.lock` ending.
	[[nodiscard]] auto admissible_git_reference(std::string_view aName) noexcept -> bool;

	/// \brief What `HEAD` or a loose reference file names: a symbolic
	/// reference, a commit, or neither when it is malformed.
	struct git_head
	{
		std::optional<std::string>    Reference;
		std::optional<git_commit_oid> Commit;
	};
	/// \brief Parses `HEAD` or a loose reference file as Git does: trailing
	/// white space is dropped; `ref:` and optional white space introduce a
	/// symbolic reference, which must be admissible; otherwise the contents
	/// start with an object name of the repository's length, followed by the
	/// end or white space.
	[[nodiscard]] auto parse_git_head(std::string_view aHead, git_hash_algorithm aAlgorithm) -> git_head;
	/// \brief A loose reference file's commit; nothing for a symbolic one.
	[[nodiscard]] auto parse_git_reference(std::string_view aContents, git_hash_algorithm aAlgorithm) -> std::optional<git_commit_oid>;
	/// \brief A reference's commit in `packed-refs`.
	[[nodiscard]] auto find_packed_reference(std::string_view aPackedRefs, std::string_view aReference, git_hash_algorithm aAlgorithm)
		-> std::optional<git_commit_oid>;
}

#endif
