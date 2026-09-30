#ifndef BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_LOCAL_EXPORT_DESTINATION_HPP
#define BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_LOCAL_EXPORT_DESTINATION_HPP

#include "bpr/application/review_cycle_export.hpp"

#include <filesystem>
#include <memory>

/// \file
/// \brief A file the reviewer chose for a saved round (RFC-BPR-0056, BPR-1159).
namespace bpr::adapters::workspace {
	/// \brief The destination file at `aFile`, which must be absolute.
	/// \details Bytes are written to a new temporary file in the same folder,
	/// flushed to disk, then moved into place: without replacing anything for
	/// `create` (`link`, `RENAME_EXCL`/`RENAME_NOREPLACE` or `MoveFileExW`
	/// without replacement) and over the file for `replace` after its digest is
	/// checked again. The destination itself is never followed as a link and a
	/// failed save removes its temporary file. Nothing else in the folder is
	/// read, listed or changed.
	[[nodiscard]] auto make_local_export_destination(std::filesystem::path aFile) -> std::unique_ptr<application::cycle_export_destination>;
}

#endif
