#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_STORAGE_INTERNAL_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_STORAGE_INTERNAL_HPP

#include "../../native/storage_failure.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "state_directory.hpp"

#include <expected>
#include <string_view>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	/// \brief Verifies genuine native lease authority for the exact current root and process.
	[[nodiscard]] auto validates_profile_runtime_lease(const std::filesystem::path& aRoot, const std::shared_ptr<application::profile_runtime_lease>& aLease) -> bool;
	[[nodiscard]] auto acquire_native_profile_runtime_lease(const std::filesystem::path& aRoot, std::stop_token aStop)
		-> std::expected<std::shared_ptr<application::profile_runtime_lease>, application::profile_runtime_lease_failure>;
	/// \brief Checks canonical path, ownership and private permissions without creating state directories.
	[[nodiscard]] auto validate_private_storage_root(const std::filesystem::path& aRoot)
		-> std::expected<void, state_directory_failure>;
	/// \brief Creates one private child or refuses an existing unsafe child without permission repair.
	/// \details Child names must be a single literal component. Uses the same held-parent
	/// platform primitives as state/v1 creation; the returned path is not a retained lease.
	[[nodiscard]] auto ensure_private_storage_child(const std::filesystem::path& aParent, std::string_view aName)
		-> std::expected<std::filesystem::path, state_directory_failure>;
	/// \brief Admits an existing store file under state/v1 only when it is a private regular file.
	/// \details An absent file is admitted, for sqlite to create. POSIX refuses group or other
	/// permission bits; Windows applies the private-object rules (owner, one grant, one link, no
	/// reparse point) and makes an inherited private grant explicit. Nothing wider is repaired.
	[[nodiscard]] auto admit_private_store_file(state_directory& aDirectory, std::string_view aName)
		-> std::expected<void, state_directory_failure>;
	/// \brief Restricts a store file this process has just opened under state/v1 to its owner.
	[[nodiscard]] auto restrict_private_store_file(state_directory& aDirectory, std::string_view aName)
		-> std::expected<void, state_directory_failure>;
	/// \brief Opens one private store file under state/v1 with SQLite.
	/// \details Admits the file, opens it by its UTF-8 path, and restricts it to its
	/// owner. On Windows the handle SQLite actually holds is then bound to the
	/// admitted name ([[BPR-0813]]): it must be a private regular file with one link
	/// and no reparse point, its final path must be that name, and its identity must
	/// be the identity that name has now. A reparse point or second link substituted
	/// between admission and open is refused. Returns an open connection the caller
	/// owns; on failure nothing is left open.
	[[nodiscard]] auto open_private_store_database(state_directory& aDirectory, std::string_view aName, int aFlags)
		-> std::expected<::sqlite3*, state_directory_failure>;
	/// \brief The database failure for a refused `open_private_store_database`.
	/// \details For the stores whose own vocabulary is the database's ([[BPR-0883]]):
	/// a refused file stays `invalid_permissions`, a name that cannot be spelled
	/// stays `invalid_path`, and anything else is `unavailable`.
	[[nodiscard]] auto private_store_database_failure(state_directory_failure aFailure) noexcept
		-> native_storage::failure;
}
#endif
