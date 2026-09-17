#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_NATIVE_FILESYSTEM_PATH_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_NATIVE_FILESYSTEM_PATH_HPP

#include <QDir>
#include <QString>

#include <cstddef>
#include <filesystem>
#include <string>

namespace bpr::app {
	/// \brief Converts a Qt path into the host path the adapters admit.
	/// \details [[BPR-0806]]. Qt spells every Windows path with '/', and the
	/// storage root validator refuses '/' by design, so the separators become
	/// native here, where a path leaves Qt, rather than in the validator. Other
	/// hosts keep Qt's UTF-8 spelling unchanged.
	[[nodiscard]] inline auto native_filesystem_path(const QString& aPath) -> std::filesystem::path
	{
#if defined(_WIN32)
		return std::filesystem::path{ QDir::toNativeSeparators(aPath).toStdWString() };
#else
		const auto bytes = aPath.toUtf8();
		return std::filesystem::path{ std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) } };
#endif
	}
} // namespace bpr::app

#endif // BPR_SRC_APP_INCLUDE_BPR_APP_NATIVE_FILESYSTEM_PATH_HPP
