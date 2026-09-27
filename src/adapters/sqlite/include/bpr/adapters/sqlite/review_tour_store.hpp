#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_TOUR_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_TOUR_STORE_HPP
#include "bpr/application/review_tour_store.hpp"
#include <filesystem>
#include <memory>
namespace bpr::adapters::sqlite {
	/// \brief Lazy tour store using the existing admitted private root and connection policy.
	[[nodiscard]] auto make_review_tour_store(std::filesystem::path aRoot, core::bpr_profile_id aProfile) -> std::unique_ptr<application::review_tour_store>;
}
#endif
