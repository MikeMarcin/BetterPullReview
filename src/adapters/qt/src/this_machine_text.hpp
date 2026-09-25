#ifndef BPR_SRC_ADAPTERS_QT_SRC_THIS_MACHINE_TEXT_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_THIS_MACHINE_TEXT_HPP

/// \file
/// \brief How a sentence names the machine the application runs on.
///
/// \details [[BPR-0773]] set the rule in the device sign-in row and the server
/// card: "this Mac" on macOS, "this computer" on Windows and Linux. Each file
/// kept a private copy, so Home's first-launch card spelled the noun itself and
/// said "this Mac" everywhere ([[BPR-1137]]). Every sentence that names the
/// machine takes the noun from here.
///
/// Internal to the Qt adapter, like the rest of its presentation wording.

#include <QString>

namespace bpr::qt::detail {
	[[nodiscard]] inline auto this_machine_text() -> QString
	{
#if defined(__APPLE__)
		return QStringLiteral("this Mac");
#else
		return QStringLiteral("this computer");
#endif
	}
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_THIS_MACHINE_TEXT_HPP
