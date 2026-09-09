#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_STORAGE_INTERNAL_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_STORAGE_INTERNAL_HPP

#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "state_directory.hpp"

#include <string_view>

namespace bpr::adapters::sqlite::detail {
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
}
#endif
