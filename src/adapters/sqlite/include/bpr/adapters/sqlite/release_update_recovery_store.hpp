#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_RELEASE_UPDATE_RECOVERY_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_RELEASE_UPDATE_RECOVERY_STORE_HPP
#include "bpr/application/release_update_recovery.hpp"
#include <filesystem>
#include <memory>
namespace bpr::adapters::sqlite {
	/// \brief Profile-private, bounded update recovery text; opens only on the effect lane.
	[[nodiscard]] auto make_release_update_recovery_store(std::filesystem::path aRoot)
		-> std::shared_ptr<application::release_update_recovery_port>;
}
#endif
