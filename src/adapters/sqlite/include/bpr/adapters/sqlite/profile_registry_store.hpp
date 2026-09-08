#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PROFILE_REGISTRY_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PROFILE_REGISTRY_STORE_HPP

#include "bpr/application/profile_registry.hpp"

#include <filesystem>
#include <memory>

namespace bpr::adapters::sqlite {
	/// \brief Creates a lazy registry store at the owner-private application root.
	/// \details Owns only state/v1/profile-registry.sqlite3. First load atomically
	/// records Default adoption in place; it never opens domain databases or the vault.
	/// Calls must be serialized on an effect executor. New profile root tokens do not
	/// create directories or authorize legacy root access.
	[[nodiscard]] auto make_profile_registry_store(std::filesystem::path aStorageRoot)
		-> std::unique_ptr<application::profile_registry_port>;
}
#endif
