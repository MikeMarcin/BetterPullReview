#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPLICATION_MENU_WORDING_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPLICATION_MENU_WORDING_HPP

#include <QTranslator>

class QCoreApplication;

/// \file
/// \brief The words Qt's macOS integration puts in the application menu.
namespace bpr::qt {
	/// \brief Names the application menu's Settings item "Settings…".
	///
	/// \details The Settings action keeps `QAction::PreferencesRole`, so on
	/// macOS it moves into the application menu with Command-comma. There Qt's
	/// Cocoa integration does not show the action's own text: it asks for
	/// `"Preferences..."` in the `MAC_APPLICATION_MENU` context. The owner
	/// (2026-11-15): "Also can we renamed Preferences to settings in the menu
	/// too. it's confusing to have it named differently". This translator
	/// answers that one request with "Settings…", the action's own text on
	/// Windows and Linux, and leaves every other text to Qt.
	class application_menu_wording final : public QTranslator
	{
	public:
		using QTranslator::QTranslator;

		/// \brief "Settings…" for the application menu's preferences item; a
		/// null string, which leaves the text as it was, for everything else.
		[[nodiscard]] auto translate(const char* aContext, const char* aSourceText, const char* aDisambiguation, int aN) const -> QString override;
		[[nodiscard]] auto isEmpty() const -> bool override;
	};

	/// \brief The text of the Settings action in every menu, the macOS
	/// application menu included.
	[[nodiscard]] auto settings_menu_text() -> QString;

	/// \brief Installs `application_menu_wording` on \p aApplication, owned by
	/// it. Call once, before the first menu bar is built.
	void install_application_menu_wording(QCoreApplication& aApplication);
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPLICATION_MENU_WORDING_HPP
