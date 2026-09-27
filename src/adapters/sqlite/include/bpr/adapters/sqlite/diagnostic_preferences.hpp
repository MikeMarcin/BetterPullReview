#ifndef BPR_SQLITE_DIAGNOSTIC_PREFERENCES_HPP
#define BPR_SQLITE_DIAGNOSTIC_PREFERENCES_HPP
#include "bpr/application/diagnostic_settings.hpp"
#include <filesystem>
namespace bpr::adapters::sqlite {
	/// \brief Reads bounded profile capture choices before any diagnostic record is admitted.
	[[nodiscard]] auto load_diagnostic_preferences(const std::filesystem::path& aRoot) noexcept -> application::diagnostic_preference_load;
	/// \brief Atomically saves the exact accepted schema; no provider or review data is stored.
	[[nodiscard]] auto store_diagnostic_preferences(const std::filesystem::path& aRoot, application::diagnostic_preferences aPreferences) noexcept -> bool;
}
#endif
