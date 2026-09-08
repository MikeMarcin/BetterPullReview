#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_DEVICE_BINDING_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_DEVICE_BINDING_STORE_HPP
#include "bpr/application/device_binding_store.hpp"
#include <filesystem>
#include <memory>
namespace bpr::adapters::sqlite {
	/// \brief Constructs a lazy metadata-only device binding store under the private state root.
	[[nodiscard]] auto make_device_binding_store(std::filesystem::path aStorageRoot, core::bpr_profile_id aBprProfile = core::default_bpr_profile_id)
		-> std::unique_ptr<application::device_binding_store_port>;
}
#endif
