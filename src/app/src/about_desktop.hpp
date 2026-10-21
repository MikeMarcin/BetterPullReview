#ifndef BPR_SRC_APP_SRC_ABOUT_DESKTOP_HPP
#define BPR_SRC_APP_SRC_ABOUT_DESKTOP_HPP

#include "bpr/qt/about_window.hpp"

#include <filesystem>
#include <memory>

namespace bpr::app {
	/// \brief [[BPR-PENDING-AA]]. Where a release installs its legal documents
	/// for the running executable: `Contents/Resources/ThirdParty` in a macOS
	/// bundle, `ThirdParty` beside the executable elsewhere
	/// (`packaging/package_runtime.py`'s `legal_root`).
	[[nodiscard]] auto installed_legal_folder() -> std::filesystem::path;

	/// \brief [[BPR-PENDING-AA]]. What Help › About says about this build.
	///
	/// \details The version and configuration compiled in, the commit
	/// `cmake/BprBuildIdentity.cmake` recorded, the Qt and system this process
	/// runs on, and which of `aLegalFolder`'s documents are regular files.
	/// Read once, when the menu is composed.
	[[nodiscard]] auto about_facts_of_this_build(const std::filesystem::path& aLegalFolder) -> qt::about_facts;

	/// \brief [[BPR-PENDING-AA]]. Help › About for every window and the empty desktop.
	///
	/// \details The page's two web links open through the core's named
	/// reference pages and the system browser opener, as every reference page
	/// does; its two documents open from `installed_legal_folder()` with the
	/// system's viewer, and only when each is still a regular file there.
	[[nodiscard]] auto make_about_menu() -> std::unique_ptr<qt::about_menu>;
} // namespace bpr::app

#endif // BPR_SRC_APP_SRC_ABOUT_DESKTOP_HPP
