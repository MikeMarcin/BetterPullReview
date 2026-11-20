#ifndef BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_APP_BUNDLE_READ_HPP
#define BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_APP_BUNDLE_READ_HPP

#include "bpr/core/agent_program_bundle.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace bpr::adapters::workspace {
	/// \brief The most bytes of an app's `Contents/Info.plist` that are read.
	inline constexpr std::size_t maximum_app_bundle_info_bytes = 1024U * 1024U;

	/// \brief [[BPR-2303]]. Reads a macOS app bundle a reviewer chose or typed
	/// as an agent's program, for `core::resolve_app_bundle_program`.
	///
	/// \details None when `aBundle` is not a folder; a link is not followed.
	/// `Contents/Info.plist`, a regular file that is not a link and at most
	/// `maximum_app_bundle_info_bytes`, gives `CFBundleExecutable` through
	/// CoreFoundation, which reads both XML and binary property lists; away
	/// from macOS it is never read. `Contents/MacOS` is listed by metadata,
	/// at most `core::maximum_app_bundle_programs` entries, following no link.
	/// Nothing is written and nothing runs.
	[[nodiscard]] auto read_app_bundle(const std::string& aBundle) -> std::optional<core::app_bundle_listing>;
} // namespace bpr::adapters::workspace

#endif // BPR_SRC_ADAPTERS_WORKSPACE_INCLUDE_BPR_ADAPTERS_WORKSPACE_APP_BUNDLE_READ_HPP
