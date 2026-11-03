#ifndef BPR_SQLITE_VIEW_FILTER_MEMORY_STORE_HPP
#define BPR_SQLITE_VIEW_FILTER_MEMORY_STORE_HPP

#include "bpr/core/view_filter_memory.hpp"

#include <filesystem>

/// \file
/// \brief [[BPR-1844]]. `state/v1/view-filters.sqlite3`: the profile's
/// remembered Discuss, Threads and Review Comments filters, one row.
/// \details A file of its own, like the diagnostic preferences, so losing it
/// loses nothing else, and the workbench settings' schema is untouched. The
/// row carries its version; `core::read_view_filter_memory` decides what an
/// unknown value or version means. Holds no host, repository, pull request
/// or account.
namespace bpr::adapters::sqlite {
	/// \brief Reads the memory under the profile root. A store that cannot be
	/// opened or read reads as nothing remembered, reported recovered.
	[[nodiscard]] auto load_view_filter_memory(const std::filesystem::path& aRoot) noexcept -> core::view_filter_memory_read;

	/// \brief Writes the memory under the profile root, as this build's
	/// version. Refuses, and leaves the row, when a newer build wrote it.
	[[nodiscard]] auto store_view_filter_memory(const std::filesystem::path& aRoot, const core::view_filter_defaults& aDefaults) noexcept -> bool;
}

#endif
