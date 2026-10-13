#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_WORKSPACE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_WORKSPACE_HPP

#include "bpr/core/epoch_model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Local workspace binding and path admission for Review Cycles
/// (RFC-BPR-0054).
/// \details The core decides whether a relative path may be captured, how it
/// is labelled, which paths would collide on another platform and whether a
/// root the host opened is still the root a cycle was bound to. Opening
/// files, reading them and resolving native identities belong to the host.
namespace bpr::core {
	/// \brief Logical identity of a bound project, independent of where the
	/// checkout lives. It survives an explicit rebind and names the project in
	/// an export; it never names a filesystem location.
	using workspace_project_id = fixed_identity_256<struct workspace_project_id_tag>;

	inline constexpr std::size_t maximum_workspace_location_bytes = 4096U;
	inline constexpr std::size_t maximum_workspace_path_bytes     = 4096U;

	/// \brief What a deliberately chosen root is.
	enum class workspace_root_kind : std::uint8_t {
		/// A plain directory with no repository.
		folder,
		/// One regular file; the scope is that file.
		file,
		/// A directory whose `.git` is the repository directory.
		git_checkout,
		/// A directory whose `.git` file names a linked worktree's directory.
		git_linked_worktree,
	};

	/// \brief Host-native identity of one filesystem object: device and inode
	/// on POSIX, volume serial and 128-bit file ID on Windows.
	struct workspace_object_identity
	{
		std::uint64_t Volume                                              = 0;
		std::uint64_t ObjectHigh                                          = 0;
		std::uint64_t ObjectLow                                           = 0;
		auto          operator<=>(const workspace_object_identity&) const = default;
	};

	/// \brief What the host saw when it opened a root.
	/// \details `Location` is the host path exactly as the user chose it (UTF-8
	/// on Windows, raw bytes elsewhere). It is where to look again, not who
	/// the project is. `Root` is the chosen directory, or for a single file the
	/// directory holding it, so an editor that saves by replacing the file does
	/// not replace the root. `Repository` is the shared Git directory and
	/// `Worktree` the per-worktree Git directory; both are absent for plain roots.
	struct workspace_root_observation
	{
		workspace_root_kind                      Kind = workspace_root_kind::folder;
		std::string                              Location;
		workspace_object_identity                Root;
		std::optional<workspace_object_identity> Repository;
		std::optional<workspace_object_identity> Worktree;
		auto                                     operator<=>(const workspace_root_observation&) const = default;
	};

	/// \brief Result of comparing a candidate root against the cycle's latest
	/// captured material before an explicit rebind.
	/// \details The counts describe the candidate relative to the latest
	/// revision. They are shown to the reviewer and recorded; a rebind to a
	/// checkout with different bytes is allowed when confirmed, never silent.
	struct workspace_material_check
	{
		std::uint64_t Compared                                           = 0;
		std::uint64_t Matching                                           = 0;
		std::uint64_t Differing                                          = 0;
		std::uint64_t Missing                                            = 0;
		std::uint64_t Added                                              = 0;
		auto          operator<=>(const workspace_material_check&) const = default;
	};

	/// \brief A cycle's binding to one local root.
	/// \details `Version` advances on every explicit rebind; a capture names
	/// the version it was taken under, so bytes from a replaced or remapped
	/// root never land in the cycle unannounced. `Admission` is the material
	/// check the reviewer confirmed when this version replaced an earlier
	/// binding or bound over captured material.
	struct cycle_workspace_binding
	{
		workspace_project_id                    Project;
		std::uint64_t                           Version = 0;
		workspace_root_observation              Root;
		std::optional<workspace_material_check> Admission;
		auto                                    operator<=>(const cycle_workspace_binding&) const = default;
	};

	[[nodiscard]] auto valid(const workspace_root_observation& aRoot) noexcept -> bool;
	[[nodiscard]] auto valid(const cycle_workspace_binding& aBinding) noexcept -> bool;

	/// \brief How a freshly opened root relates to a binding.
	enum class workspace_verification : std::uint8_t {
		/// Same kind, same root object and, for Git, the same repository and worktree.
		matches,
		/// Something else now answers at the location: a replaced, recreated
		/// or same-named root. Capture refuses it until an explicit rebind.
		replaced,
		/// The root is the same object but its Git repository or worktree changed.
		repository_changed,
		/// A file became a folder or the reverse, or a Git root lost its repository.
		kind_changed,
	};
	/// \brief Compares an observation with the binding it is meant to serve.
	/// The location is not compared: identity decides, not spelling.
	[[nodiscard]] auto verify_workspace(const cycle_workspace_binding& aBinding, const workspace_root_observation& aObserved) noexcept
		-> workspace_verification;

	/// \brief True when a root kind names a directory rather than one file.
	[[nodiscard]] constexpr auto directory_root(const workspace_root_kind aKind) noexcept -> bool
	{
		return aKind != workspace_root_kind::file;
	}

	/// \brief An explicit mapping from a project named by an imported record
	/// to a project bound on this machine.
	struct workspace_import_mapping
	{
		workspace_project_id Imported;
		workspace_project_id Local;
		auto                 operator<=>(const workspace_import_mapping&) const = default;
	};
	/// \brief Resolves an imported project to a local one. Only an explicit
	/// mapping resolves; a matching path, folder name or cycle alias does not.
	[[nodiscard]] auto resolve_imported_project(std::span<const workspace_import_mapping> aMappings, const workspace_project_id& aImported)
		-> std::optional<workspace_project_id>;

	/// \brief Why a root-relative path is not admitted for capture.
	enum class workspace_path_refusal : std::uint8_t {
		none,
		empty,
		too_long,
		/// Leading `/`, or a Windows drive or UNC prefix.
		absolute,
		/// An empty, `.` or `..` segment.
		invalid_segment,
		/// A NUL byte.
		nul,
		/// A `.git` segment in any letter case: repository internals are
		/// never review material.
		repository_internal,
	};
	/// \brief Checks a `/`-separated root-relative path's raw bytes. Nothing is
	/// normalized: an admitted path is captured under exactly these bytes.
	[[nodiscard]] auto admit_workspace_path(std::string_view aPath) noexcept -> workspace_path_refusal;

	/// \brief Properties of an admitted path that limit where it can be
	/// materialized or how it can be shown. None of them is a refusal.
	struct workspace_path_portability
	{
		/// Not valid UTF-8; shown through an escaped label.
		bool NonUtf8 = false;
		/// A Windows device name such as `CON` or `com1.txt` in some segment.
		bool WindowsReservedName = false;
		/// A byte Windows refuses in names: `<>:"|?*`, `\` or a control character.
		bool WindowsForbiddenCharacter = false;
		/// A segment ending in `.` or space, which Windows strips.
		bool TrailingDotOrSpace                                   = false;
		auto operator<=>(const workspace_path_portability&) const = default;

		[[nodiscard]] constexpr auto portable() const noexcept -> bool
		{
			return !NonUtf8 && !WindowsReservedName && !WindowsForbiddenCharacter && !TrailingDotOrSpace;
		}
	};
	[[nodiscard]] auto path_portability(std::string_view aPath) noexcept -> workspace_path_portability;

	/// \brief A printable label for raw path bytes. Valid UTF-8 without
	/// control characters is returned unchanged; anything else is escaped as
	/// `\xNN`, so the label never claims to be the bytes it stands for.
	[[nodiscard]] auto workspace_path_label(std::string_view aPath) -> std::string;

	enum class workspace_collision_kind : std::uint8_t {
		/// Equal after case folding: one file on a case-insensitive volume.
		case_fold,
		/// Equal after canonical decomposition (and possibly case folding):
		/// one file on a normalization-insensitive volume.
		unicode_normalization,
	};
	/// \brief Two distinct admitted paths another platform would merge.
	struct workspace_path_collision
	{
		std::size_t              First                                              = 0;
		std::size_t              Second                                             = 0;
		workspace_collision_kind Kind                                               = workspace_collision_kind::case_fold;
		auto                     operator<=>(const workspace_path_collision&) const = default;
	};
	/// \brief The key two paths share when some platform would merge them.
	/// \details Applies canonical decomposition (NFD, combining marks in
	/// canonical order) and one-to-one case folding for every script, through
	/// utf8proc ([[BPR-1434]]). Full foldings such as ß to ss are not applied,
	/// because no case-insensitive file system makes them. Invalid UTF-8 bytes
	/// stay as themselves; two distinct paths are never merged by this key alone.
	[[nodiscard]] auto workspace_collision_key(std::string_view aPath) -> std::string;
	/// \brief Finds every pair of distinct paths that share a collision key.
	/// Indexes refer to `aPaths`; nothing is combined or dropped.
	[[nodiscard]] auto find_path_collisions(std::span<const std::string> aPaths) -> std::vector<workspace_path_collision>;
}

#endif
