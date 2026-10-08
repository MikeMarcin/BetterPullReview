#ifndef BPR_APP_DIAGNOSTIC_EXPORT_DIRECTORY_HPP
#define BPR_APP_DIAGNOSTIC_EXPORT_DIRECTORY_HPP

#include "bpr/application/release_maintenance_projection.hpp"

#include <array>
#include <filesystem>

namespace bpr::app {
	/// \brief Observes the three OS standard user folders on an effect lane.
	[[nodiscard]] auto diagnostic_export_locations() -> std::array<std::filesystem::path, 3>;
	/// \brief Selects the first output-admitted Downloads, Documents or home observation.
	/// \details No creation, enumeration, permission change, history or write grant.
	[[nodiscard]] auto resolve_diagnostic_export_directory(const std::array<std::filesystem::path, 3>& aLocations)
		-> application::diagnostic_export_directory;
}
#endif
