#ifndef BPR_SQLITE_PULL_REQUEST_ACTIVITY_STORE_HPP
#define BPR_SQLITE_PULL_REQUEST_ACTIVITY_STORE_HPP

#include "bpr/core/pull_request_activity.hpp"

#include <filesystem>

/// \file
/// \brief [[BPR-1850]]. `state/v1/pull-request-activity.sqlite3`: the
/// profile's activity notification setting, its last `Last-Modified` and the
/// notification threads it has seen.
/// \details A file of its own, so losing it costs at most a new baseline and
/// the setting returning to on. One versioned row and one row per seen
/// thread (identity and `updated_at`); `core::read_activity_memory` decides
/// what a damaged row or an unknown version means. Holds the canonical host
/// and numeric account the memory belongs to, and no repository, pull
/// request, title or comment.
namespace bpr::adapters::sqlite {
	/// \brief Reads the memory under the profile root. A store that cannot be
	/// opened or read reads as a fresh memory, reported recovered.
	[[nodiscard]] auto load_pull_request_activity(const std::filesystem::path& aRoot) noexcept -> core::activity_memory_read;

	/// \brief Writes the memory under the profile root, as this build's version,
	/// in one transaction. Refuses, and leaves the rows, when a newer build wrote them.
	[[nodiscard]] auto store_pull_request_activity(const std::filesystem::path& aRoot, const core::activity_memory& aMemory) noexcept -> bool;
}

#endif
