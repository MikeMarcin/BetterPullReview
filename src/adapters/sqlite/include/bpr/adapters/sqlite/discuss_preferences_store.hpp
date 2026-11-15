#ifndef BPR_SQLITE_DISCUSS_PREFERENCES_STORE_HPP
#define BPR_SQLITE_DISCUSS_PREFERENCES_STORE_HPP

#include "bpr/core/discuss_preferences.hpp"

#include <filesystem>

/// \file
/// \brief [[BPR-2171]]. `state/v1/discuss.sqlite3`: the profile's Discuss
/// preferences, each repository's bot marks and the column's width.
/// \details A file of its own, like the remembered filters, so losing it
/// loses nothing else and the workbench settings' schema is untouched. It
/// holds repository names and logins, nothing more; no account, credential
/// or pull request. `core::read_discuss_preferences` decides what an unknown
/// value or version means.
namespace bpr::adapters::sqlite {
	/// \brief Reads the preferences under the profile root. A store that
	/// cannot be opened or read reads as nothing chosen, reported recovered.
	[[nodiscard]] auto load_discuss_preferences(const std::filesystem::path& aRoot) noexcept -> core::discuss_preferences_read;

	/// \brief Writes the preferences under the profile root, as this build's
	/// version, in one transaction. Refuses, and leaves the store, when a
	/// newer build wrote it.
	[[nodiscard]] auto store_discuss_preferences(const std::filesystem::path& aRoot, const core::discuss_preferences& aPreferences) noexcept -> bool;
}

#endif
