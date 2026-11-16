#include "bpr/qt/application_menu_wording.hpp"

#include <QCoreApplication>

#include <string_view>

namespace bpr::qt {
	auto application_menu_wording::translate(const char* const aContext, const char* const aSourceText, const char* const aDisambiguation, const int aN) const -> QString
	{
		static_cast<void>(aDisambiguation);
		static_cast<void>(aN);
		if (aContext != nullptr && aSourceText != nullptr && std::string_view{ aContext } == "MAC_APPLICATION_MENU" && std::string_view{ aSourceText } == "Preferences...") {
			return settings_menu_text();
		}
		return { };
	}

	auto application_menu_wording::isEmpty() const -> bool { return false; }

	auto settings_menu_text() -> QString { return QStringLiteral("Settings…"); }

	void install_application_menu_wording(QCoreApplication& aApplication)
	{
		QCoreApplication::installTranslator(new application_menu_wording{ &aApplication });
	}
} // namespace bpr::qt
