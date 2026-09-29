#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_MANIFEST_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_MANIFEST_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "bpr/core/review_cycle_material.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief The immutable scoped manifest of one captured revision
/// (RFC-BPR-0054).
/// \details A manifest lists what one capture admitted, in path-byte order,
/// with each entry's kind, raw content hash, portable mode and byte profile;
/// what it left out and why; and which tracked paths the working tree no
/// longer holds. A path missing from a partial manifest is not deleted, and a
/// scope change is a new scope version, never a mass deletion. The encoding
/// is canonical: the same manifest always has the same bytes, whose SHA-256
/// is the revision's material digest.
namespace bpr::core {
	/// Instability retries after the first attempt, then `capture_unstable`.
	inline constexpr std::uint32_t maximum_capture_retries = 3U;
	/// The largest file a capture reads by default.
	inline constexpr std::uint64_t maximum_captured_file_bytes = 128U * 1024U * 1024U;

	enum class manifest_entry_kind : std::uint8_t {
		file,
		/// The link's target bytes are the content; never followed.
		symlink,
		/// A submodule or nested repository: a boundary, not recursed.
		gitlink,
		/// A directory standing for content beneath it that was not listed.
		directory,
	};

	enum class manifest_origin : std::uint8_t {
		/// Tracked by the checkout's index.
		tracked,
		/// Present in a Git working tree, not tracked and not ignored.
		untracked,
		/// In a folder or file root with no repository.
		unversioned,
	};

	enum class manifest_availability : std::uint8_t {
		captured,
		/// Larger than the capture's file budget; visible, not read.
		too_large,
		/// Sparse checkout left it out of the working tree; nothing is fetched.
		sparse,
		/// Permission or I/O refused the read.
		unreadable,
		/// A device, socket, pipe or non-link reparse point.
		unsupported_kind,
		/// On another volume or behind a link; nothing beneath it was read.
		outside_root,
		/// The platform cannot read this link's target.
		link_target_unsupported,
		/// A scope path that names nothing.
		missing_scope,
		/// A submodule or nested repository's content, not captured.
		not_materialized,
	};

	/// \brief One admitted path.
	struct manifest_entry
	{
		std::string           Path;
		manifest_entry_kind   Kind         = manifest_entry_kind::file;
		manifest_origin       Origin       = manifest_origin::unversioned;
		manifest_availability Availability = manifest_availability::captured;
		/// Portable Git mode; zero when unknown.
		std::uint32_t Mode = 0;
		/// The platform has no execute bit, so the staged mode was kept
		/// rather than inventing one.
		bool          ModeFromIndex = false;
		std::uint64_t Size          = 0;
		/// SHA-256 of the raw bytes read (a link's target bytes for a link).
		/// This is not a Git object identity.
		std::optional<cycle_material_digest> Content;
		/// Files only: encoding, line endings and LFS pointer shape.
		material_profile Profile;
		/// The staged Git object, tagged with the repository's algorithm.
		std::optional<git_blob_oid> Staged;
		bool                        Unmerged    = false;
		bool                        IntentToAdd = false;
		/// A `filter` attribute (such as LFS) applies: the Git object is a
		/// transformation of these bytes, which capture never performs.
		bool FilterManaged                            = false;
		auto operator<=>(const manifest_entry&) const = default;
	};

	/// \brief One path left out, and why. A directory stands for everything
	/// beneath it.
	struct manifest_exclusion
	{
		std::string                Path;
		bool                       Directory                                    = false;
		workspace_exclusion_reason Reason                                       = workspace_exclusion_reason::ignored;
		auto                       operator<=>(const manifest_exclusion&) const = default;
	};

	struct cycle_manifest
	{
		std::uint64_t            ScopeVersion = 0;
		std::vector<std::string> Scope;
		workspace_root_kind      RootKind = workspace_root_kind::folder;
		cycle_capture_coverage   Coverage = cycle_capture_coverage::partial;
		cycle_capture_quality    Quality  = cycle_capture_quality::observed_stable;
		/// Invalid for roots with no repository.
		git_hash_algorithm            ObjectFormat = git_hash_algorithm::invalid;
		std::optional<std::string>    HeadReference;
		std::optional<git_commit_oid> HeadCommit;
		bool                          IndexUnmerged = false;
		bool                          IndexSparse   = false;
		/// The index is split, so the tracked set could not be read whole.
		bool IndexSplit = false;
		/// Names seen but not admissible as paths (for example too long).
		std::uint64_t                   RefusedPaths = 0;
		std::vector<manifest_entry>     Entries;
		std::vector<std::string>        Deletions;
		std::vector<manifest_exclusion> Exclusions;
		auto                            operator<=>(const cycle_manifest&) const = default;
	};

	enum class manifest_error : std::uint8_t {
		truncated,
		format,
		bounds,
		/// Paths unsorted, duplicated, not admitted, or deleted and present.
		paths,
	};

	[[nodiscard]] auto encode_manifest(const cycle_manifest& aManifest) -> std::vector<std::uint8_t>;
	/// \brief Decodes and validates; any deviation from the canonical form is refused.
	[[nodiscard]] auto decode_manifest(std::span<const std::uint8_t> aBytes) -> std::expected<cycle_manifest, manifest_error>;
	/// \brief Structural checks shared by encode and decode.
	[[nodiscard]] auto validate_manifest(const cycle_manifest& aManifest) -> std::optional<manifest_error>;

	/// \brief The bounded summary the core records for a manifest.
	[[nodiscard]] auto observe_manifest(const cycle_manifest& aManifest, const cycle_material_digest& aMaterial, std::uint64_t aWorkspaceVersion)
		-> cycle_capture_observation;
	/// \brief Distinct content digests the manifest references, sorted.
	[[nodiscard]] auto manifest_content(const cycle_manifest& aManifest) -> std::vector<std::pair<cycle_material_digest, std::uint64_t>>;
	/// \brief The entry for a path, or null.
	[[nodiscard]] auto find_entry(const cycle_manifest& aManifest, std::string_view aPath) -> const manifest_entry*;

	/// \brief What one pass saw of one path, for comparing passes.
	struct capture_stamp
	{
		std::string               Path;
		manifest_entry_kind       Kind = manifest_entry_kind::file;
		workspace_object_identity Identity;
		std::uint64_t             Size                                    = 0;
		std::int64_t              Modified                                = 0;
		std::int64_t              Changed                                 = 0;
		auto                      operator<=>(const capture_stamp&) const = default;
	};
	struct capture_pass_comparison
	{
		/// Paths added, removed or changed between passes.
		std::uint64_t Changed = 0;
		/// The first changed paths, in order, for diagnostics.
		std::vector<std::string> Examples;
		[[nodiscard]] auto       stable() const noexcept -> bool
		{
			return Changed == 0;
		}
	};
	/// \brief Compares two passes sorted by path. Matching stamps are
	/// necessary for a quiescent observation, never proof of atomicity.
	[[nodiscard]] auto compare_capture_passes(std::span<const capture_stamp> aFirst, std::span<const capture_stamp> aSecond, std::size_t aExamples)
		-> capture_pass_comparison;
}

#endif
