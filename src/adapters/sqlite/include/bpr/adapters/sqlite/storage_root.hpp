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
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_ROOT_HPP
