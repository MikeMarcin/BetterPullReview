#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_RELEASE_UPDATE_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_RELEASE_UPDATE_STORE_HPP
#include "bpr/application/release_update_port.hpp"
#include <filesystem>

namespace bpr::adapters::sqlite {
	/// \brief Lazy private updater store under the application-wide registry root.
	[[nodiscard]] auto make_release_update_store(std::filesystem::path aRoot)
		-> std::shared_ptr<application::release_update_settings_port>;
}
#endif
