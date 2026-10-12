#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REMEMBERED_CONNECTION_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REMEMBERED_CONNECTION_STORE_HPP
#include "bpr/application/remembered_connection_store.hpp"
#include <filesystem>
#include <memory>
namespace bpr::adapters::sqlite {
	/// \brief Opens profile-private state/v1/remembered-connections.sqlite3 without repair.
	[[nodiscard]] auto make_remembered_connection_store(std::filesystem::path aRoot, core::bpr_profile_id aProfile) -> std::unique_ptr<application::remembered_connection_store_port>;
}
#endif
