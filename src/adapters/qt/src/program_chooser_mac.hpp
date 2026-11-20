#ifndef BPR_SRC_ADAPTERS_QT_SRC_PROGRAM_CHOOSER_MAC_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_PROGRAM_CHOOSER_MAC_HPP

#include <QString>

class QWidget;

namespace bpr::qt::detail {
	/// \brief [[BPR-2303]] (owner, 2026-11-19: "can we allow the custom agent
	/// executable browser to open .app bundles somehow?"). The native open
	/// panel for an agent's program, with app bundles shown as folders.
	///
	/// \details A file, or an app bundle as a whole, may be chosen, and a
	/// double-click opens any bundle like a folder so a helper inside it can
	/// be chosen. Any other folder is refused by the panel itself. Qt's own
	/// file dialog cannot browse into packages: it has no option for
	/// `treatsFilePackagesAsDirectories`. Returns the chosen path, or empty.
	[[nodiscard]] auto choose_program_mac(QWidget* aParent, const QString& aTitle, const QString& aStart) -> QString;
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_PROGRAM_CHOOSER_MAC_HPP
