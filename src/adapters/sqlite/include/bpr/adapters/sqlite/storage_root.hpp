#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_ROOT_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_ROOT_HPP

#include <filesystem>

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
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_ROOT_HPP
