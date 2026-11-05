#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_ROOT_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_ROOT_HPP

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace bpr::adapters::sqlite {
	/// \brief Narrows a directory the caller owns to the owner-private shape
	/// every store in this adapter requires of its root.
	///
	/// \details Every durable store opens `<root>/state/v1` and refuses a root
	/// that another local principal can reach. The root is the one directory
	/// the adapter does not create, so the caller has to make it private, and
	/// what "private" means is this adapter's definition, not the caller's.
	/// On Windows that is the token user as owner, a protected DACL with one
	/// full-access entry for that user carrying the directory inheritance
	/// flags, and a second entry for the process's own AppContainer when it
	/// runs in one; `std::filesystem::permissions` cannot express any of it,
	/// which is why a root narrowed with mode `0700` alone was refused by the
	/// product on Windows (BPR-0436). On POSIX it is mode `0700`.
	///
	/// The call is idempotent and applies only to a directory the caller
	/// already owns; it returns `false` when the directory is missing, is a
	/// reparse point, cannot be written, or does not read back exactly as the
	/// adapter will later demand.
	[[nodiscard]] auto make_storage_root_owner_private(
		const std::filesystem::path& aRoot) -> bool;

	/// \brief Creates a storage root that does not exist yet, and each missing
	/// folder above it, in the owner-private shape, whatever the process umask.
	///
	/// \details `std::filesystem::create_directories` takes its modes from the
	/// umask, so under the user-private-group umask `0002` a Linux desktop login
	/// commonly has, every folder it made above the root was `0775`. The root
	/// itself was narrowed afterwards, but its parent stayed group-writable, and
	/// the agent workspace neighbourhood that lives beside the root refuses such
	/// a parent (BPR-0817).
	///
	/// `aRoot` must be absolute and lexically normal. The deepest existing
	/// ancestor must be a real directory, not a symbolic link or a file, whose
	/// canonical spelling is its own. On POSIX each missing component is created
	/// relative to its already opened parent without following links, opened,
	/// checked to be a directory owned by the effective user, and set to exactly
	/// `0700` through that descriptor. A component that a concurrent creator made
	/// first is accepted when it is then a real directory; its owner and mode
	/// are neither checked nor changed here. The root, however it came to exist,
	/// is then accepted only when the stores would admit it. On Windows, which
	/// has no umask, the missing chain is created with
	/// `std::filesystem::create_directories` and the root narrowed by
	/// `make_storage_root_owner_private`, exactly as before.
	///
	/// The call never changes the mode, owner or access-control list of a
	/// directory that already existed: a pre-existing root is accepted only when
	/// the stores would already admit it, and a pre-existing or concurrently
	/// created shared ancestor is left for the checks that refuse it, such as
	/// the agent workspace neighbourhood's refusal of a parent another user can
	/// write. It returns `false` on any failure and throws nothing.
	[[nodiscard]] auto create_storage_root_owner_private(
		const std::filesystem::path& aRoot) -> bool;

	/// \brief The spelling the file system itself gives a storage root's
	/// existing folders, when it names the same folders through no link.
	///
	/// \details [[BPR-1868]]. Every store admits a root only when its canonical
	/// spelling is its own, which is how a link anywhere in the path is
	/// refused. On a case-insensitive volume (macOS APFS by default) a path
	/// typed as `/Users/me/BPR-attended/data` opens the folder that is on disk
	/// as `/Users/me/bpr-attended`, and its canonical spelling then differs in
	/// letter case alone, so a root nothing was wrong with was refused and the
	/// application exited at startup.
	///
	/// This returns the canonical spelling of the deepest existing ancestor,
	/// with the missing components appended as given, only when every existing
	/// component is a real directory and not a link, the canonical spelling has
	/// as many components, and it names the same device and inode. Otherwise
	/// it returns `aRoot` unchanged, so a root spelled through a link, or one
	/// that is not absolute and lexically normal, still reaches the checks that
	/// refuse it. On Windows it returns `aRoot` unchanged. It never creates,
	/// changes or opens anything for writing, and throws nothing.
	[[nodiscard]] auto storage_root_on_disk_spelling(
		const std::filesystem::path& aRoot) -> std::filesystem::path;

	/// \brief The folder, inside a named data folder that is not the
	/// application's own, that the stores keep their data in instead.
	inline constexpr auto nested_storage_folder = std::string_view{ "BetterPullReview" };
	/// \brief The data root inside `nested_storage_folder`.
	inline constexpr auto nested_storage_leaf = std::string_view{ "data" };

	/// \brief Where the stores keep their data for a data folder the user
	/// named.
	///
	/// \details [[BPR-1870]]. The stores need a root only this user can
	/// reach. A folder the application made, or one that is empty or holds
	/// the catalog's `state/v1` (the application's own data, copied or made
	/// beforehand with `mkdir -p`), is used as it is and narrowed by
	/// `tighten_existing_storage_root`. A folder that holds anything else may
	/// be used by other programs (a sync client, a backup tool, a project), so
	/// its mode is never changed: the stores keep their data in
	/// `<root>/BetterPullReview/data` instead, which the application creates
	/// owner-private, and whose own private parent then holds the agent
	/// workspace neighbourhood. The same happens for an existing folder owned
	/// by another account. Once that nested root exists it is always chosen,
	/// so a later change to the outer folder never strands the data.
	///
	/// A root that is missing, not a real folder, or not absolute and
	/// lexically normal is returned unchanged for the checks that create or
	/// refuse it. Read-only; never throws.
	[[nodiscard]] auto storage_root_placement(
		const std::filesystem::path& aRoot) -> std::filesystem::path;

	/// \brief What `tighten_existing_storage_root` found and did.
	enum class storage_root_tightening : std::uint8_t {
		/// The stores already admit the root; nothing was changed.
		already_private,
		/// The root was the application's own and was narrowed in place.
		tightened,
		/// The root belongs to another account; nothing was changed.
		not_owned,
		/// The root holds something that is not the application's; nothing
		/// was changed.
		foreign_content,
		/// The root is reached through a link or another spelling, is not a
		/// folder, or could not be narrowed.
		refused,
	};

	/// \brief Narrows an existing data root that is the application's own to
	/// the owner-private shape, instead of refusing it.
	///
	/// \details [[BPR-1870]]. Owner, 2026-11-04: a folder made beforehand
	/// (`mkdir -p` gives `0755`) was refused at startup. The root is narrowed
	/// only when it is this user's real folder, reached through no link with
	/// its own canonical spelling, and is empty or holds the catalog's
	/// `state/v1`. On POSIX the folder is opened without following a link,
	/// shown to be the named one by device and inode, inspected through that
	/// descriptor and set to `0700` with `fchmod`; on Windows the exact
	/// owner-private descriptor is written through
	/// `make_storage_root_owner_private`. A folder with other content, another
	/// account's, or a link is never changed. Never throws.
	[[nodiscard]] auto tighten_existing_storage_root(
		const std::filesystem::path& aRoot) -> storage_root_tightening;
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_ROOT_HPP
