#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_WINDOW_REVIEW_RESTORATION_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_WINDOW_REVIEW_RESTORATION_STORE_HPP
#include "bpr/application/window_review_restoration_ports.hpp"
#include <filesystem>
#include <memory>
namespace bpr::adapters::sqlite {
	/// Creates a lazy store bound to one admitted profile-private root.
	/// Construction performs no I/O. Serialize load/save and destruction on an effect lane.
	[[nodiscard]] auto make_window_review_restoration_store(std::filesystem::path aStorageRoot)
		-> std::unique_ptr<application::window_review_restoration_port>;
}
#endif
