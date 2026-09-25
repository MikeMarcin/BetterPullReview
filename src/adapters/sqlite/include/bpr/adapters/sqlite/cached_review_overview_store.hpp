#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_CACHED_REVIEW_OVERVIEW_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_CACHED_REVIEW_OVERVIEW_STORE_HPP
#include "bpr/application/cached_review_overview_port.hpp"
#include <filesystem>
#include <memory>
namespace bpr::adapters::sqlite {
	/// \brief Creates a lazy store in an admitted profile-private root; construction performs no I/O.
	[[nodiscard]] auto make_cached_review_overview_store(std::filesystem::path aRoot)
		-> std::unique_ptr<application::cached_review_overview_port>;
}
#endif
