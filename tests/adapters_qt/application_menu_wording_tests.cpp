/// \file
/// \brief The Settings item is "Settings…" in every menu (owner, 2026-11-15:
/// "Also can we renamed Preferences to settings in the menu too. it's
/// confusing to have it named differently").
///
/// \details On macOS the action's preferences role moves it into the
/// application menu, where Qt's Cocoa integration shows the
/// `MAC_APPLICATION_MENU` translation of "Preferences..." instead of the
/// action's text. The check reads that translation as the integration asks
/// for it, before and after the wording is installed, and the window's own
/// action, which Windows and Linux show as it is.

#include "bpr/qt/application_menu_wording.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto application_menu_preferences() -> QString { return QCoreApplication::translate("MAC_APPLICATION_MENU", "Preferences..."); }
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	expect(application_menu_preferences() == QStringLiteral("Preferences..."), "negative control: without the wording Qt's application menu says Preferences...");
	bpr::qt::install_application_menu_wording(application);
	expect(application_menu_preferences() == QStringLiteral("Settings…"), "the macOS application menu's item reads Settings…");
	expect(QCoreApplication::translate("MAC_APPLICATION_MENU", "Quit %1") == QStringLiteral("Quit %1") && QCoreApplication::translate("QDialogButtonBox", "Preferences...") == QStringLiteral("Preferences..."),
		   "every other text is left as Qt has it");

	bpr::qt::workbench_window window;
	auto* const               settings = window.settings_action();
	expect(settings != nullptr && settings->menuRole() == QAction::PreferencesRole, "Settings keeps the preferences role, so macOS keeps it in the application menu with Command-comma");
	expect(settings->text().remove(QLatin1Char{ '&' }) == bpr::qt::settings_menu_text(), "Windows and Linux show the action itself: Settings…");
	std::cout << "application-menu-wording:pass\n";
	return EXIT_SUCCESS;
}
