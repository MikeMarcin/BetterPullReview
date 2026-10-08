#include "bpr/app/diagnostic_export_directory.hpp"

#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/app/native_filesystem_path.hpp"

#include <QStandardPaths>

namespace bpr::app {
	auto diagnostic_export_locations() -> std::array<std::filesystem::path, 3>
	{
		return { native_filesystem_path(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)),
				 native_filesystem_path(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)),
				 native_filesystem_path(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)) };
	}
	auto resolve_diagnostic_export_directory(const std::array<std::filesystem::path, 3>& aLocations) -> application::diagnostic_export_directory
	{
		for (const auto& location : aLocations) {
			if (!location.empty() && location != location.root_path() && adapters::sqlite::admit_release_diagnostic_directory(location)) {
#if defined(_WIN32)
				return QDir::fromNativeSeparators(QString::fromStdWString(location.native())).toStdString();
#else
				return location.string();
#endif
			}
		}
		return std::unexpected(core::release_maintenance_reason::unavailable);
	}
}
