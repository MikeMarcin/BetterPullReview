#ifndef BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_LOCAL_WORKSPACE_SOURCE_HPP
#define BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_LOCAL_WORKSPACE_SOURCE_HPP

#include "bpr/application/workspace_source_port.hpp"

#include <memory>

/// \file
/// \brief The native read-only local workspace source (RFC-BPR-0054).
namespace bpr::adapters::workspace {
	/// \brief Opens roots on the local filesystem.
	/// \details POSIX resolves every descendant component with `openat` and
	/// `O_NOFOLLOW` beneath the held root descriptor and refuses a component
	/// on another device. Windows opens each component with
	/// `FILE_FLAG_OPEN_REPARSE_POINT`, holds every ancestor directory without
	/// delete sharing while a descendant is used, and refuses reparse points
	/// and other volumes; paths are WTF-8, so names that are not valid UTF-16
	/// survive. Windows reports symbolic-link targets as unsupported. No
	/// implementation writes, executes or contacts the network.
	[[nodiscard]] auto make_local_workspace_source() -> std::unique_ptr<application::workspace_source_port>;
}

#endif
