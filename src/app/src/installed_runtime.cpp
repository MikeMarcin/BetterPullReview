#include "installed_runtime.hpp"

#if defined(BPR_DISTRIBUTION_BUILD)
	#include <QCoreApplication>
	#include <QDir>
	#include <QFileInfo>
	#include <QString>

	#include <array>
	#include <cstddef>
	#include <cstdint>
	#include <string>
	#include <string_view>
	#if defined(_WIN32)
		#include <windows.h>
	#elif defined(__APPLE__)
		#include <mach-o/dyld.h>
	#else
		#include <unistd.h>
	#endif
#endif

namespace bpr::app {
	auto restrict_installed_runtime(int aArgc, char** aArgv) -> bool
	{
#if defined(BPR_DISTRIBUTION_BUILD)
		// These Qt arguments load code independently of libraryPaths(). The
		// public application has no plugin-loading command-line capability.
		for (int index = 1; index < aArgc; ++index) {
			auto argument = std::string_view{ aArgv[index] };
			if (argument.starts_with("--")) {
				argument.remove_prefix(1U);
			}
			for (const auto option : { "-platformpluginpath", "-plugin", "-qmljsdebugger" }) {
				if (argument == option || argument.starts_with(std::string{ option } + "=")) {
					return false;
				}
			}
		}
		for (const auto name : { "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QT_QPA_GENERIC_PLUGINS" }) {
			qunsetenv(name);
		}
		QString executable;
	#if defined(_WIN32)
		std::array<wchar_t, 32768> buffer{ };
		const auto                 size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (size == 0U || size >= buffer.size() || !SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)) {
			return false;
		}
		executable = QString::fromWCharArray(buffer.data(), static_cast<int>(size));
	#elif defined(__APPLE__)
		std::array<char, 32768> buffer{ };
		auto                    size = static_cast<std::uint32_t>(buffer.size());
		if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
			return false;
		}
		executable = QString::fromUtf8(buffer.data());
	#else
		std::array<char, 32768> buffer{ };
		const auto              size = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
		if (size <= 0 || static_cast<std::size_t>(size) >= buffer.size()) {
			return false;
		}
		executable = QString::fromUtf8(buffer.data(), static_cast<qsizetype>(size));
	#endif
		const auto directory = QFileInfo{ executable }.canonicalPath();
	#if defined(BPR_INSTALLED_TOUR_CLI) && defined(__APPLE__)
		const auto plugins = QDir{ directory }.absoluteFilePath(QStringLiteral("../lib/bpr/plugins"));
	#elif defined(BPR_INSTALLED_TOUR_CLI) && defined(__linux__)
		const auto plugins = QDir{ directory }.absoluteFilePath(QStringLiteral("../lib/bpr"));
	#elif defined(__APPLE__)
		const auto plugins = QDir{ directory }.absoluteFilePath(QStringLiteral("../PlugIns"));
	#else
		const auto plugins = QDir{ directory }.absoluteFilePath(QStringLiteral("plugins"));
	#endif
		const auto canonical = QFileInfo{ plugins }.canonicalFilePath();
		if (directory.isEmpty() || canonical.isEmpty() || canonical != QDir::cleanPath(plugins) || !QFileInfo{ canonical }.isDir()) {
			return false;
		}
		QCoreApplication::setLibraryPaths({ canonical });
#else
		static_cast<void>(aArgc);
		static_cast<void>(aArgv);
#endif
		return true;
	}
}
