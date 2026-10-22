#ifndef BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_CLONE_FOLDER_SCAN_HPP
#define BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_CLONE_FOLDER_SCAN_HPP

#include "bpr/core/clone_discovery.hpp"

#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace bpr::adapters::workspace {
	/// \brief [[BPR-PENDING-AQ3]], RFC-BPR-0091 owner decision D2. Looks in
	/// each listed folder for clones, by metadata alone.
	///
	/// \details Each folder itself and its directories down to
	/// `core::clone_search_depth` levels below it are looked at, at most
	/// `core::maximum_clone_search_directories` per folder. Symbolic links,
	/// junctions and other reparse points are never followed, hidden
	/// directories are skipped, and a clone is not searched inside. A clone is
	/// a directory whose `.git` is a directory, not a link; a `.git` file (a
	/// linked worktree or a submodule) is not followed. Only `.git/config`
	/// (at most `core::maximum_clone_config_bytes`) and `.git/HEAD` are read,
	/// as regular files that are not links. Nothing is written, no Git or
	/// other program runs, and a folder that cannot be read is skipped.
	[[nodiscard]] auto scan_clone_folders(std::span<const std::string> aFolders, std::stop_token aStop = { }) -> std::vector<core::clone_candidate>;

	/// \brief The one folder a reviewer chose, read the same way, or nothing
	/// when it is not a clone.
	[[nodiscard]] auto read_clone(const std::string& aPath) -> std::optional<core::clone_candidate>;

	/// \brief Which of `aFolders` exist as directories, not links, in order.
	/// Reads no directory's contents.
	[[nodiscard]] auto existing_folders(std::span<const std::string> aFolders) -> std::vector<std::string>;
} // namespace bpr::adapters::workspace

#endif // BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_CLONE_FOLDER_SCAN_HPP
