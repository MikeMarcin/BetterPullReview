/// \file
/// \brief Proves the Settings window places panels and holds no state of its own.
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_every_page_exists_with_a_notice()
	{
		qt::settings_window window;
		expect(window.sidebar() != nullptr && window.pages() != nullptr
				   && window.sidebar()->count() == static_cast<int>(qt::settings_page_count)
				   && window.pages()->count() == static_cast<int>(qt::settings_page_count),
			   "the sidebar and the page stack carry one entry per page");
		for (auto index = 0; index < static_cast<int>(qt::settings_page_count); ++index) {
			const auto  page    = static_cast<qt::settings_page>(index);
			auto* const surface = window.page_widget(page);
			expect(surface != nullptr, "every page has a surface");
			auto* const notice = surface->findChild<QLabel*>(QStringLiteral("settingsPageNotice"));
			expect(notice != nullptr && !notice->text().isEmpty(),
				   "a page without a panel says where its settings live");
			expect(window.sidebar()->item(index)->text() == qt::settings_page_title(page),
				   "sidebar rows carry the page titles");
		}
		expect(window.current_page() == qt::settings_page::appearance,
			   "the window opens on Appearance");
	}

	void test_adopting_a_panel_replaces_the_notice()
	{
		qt::settings_window window;
		auto* const         panel = new QWidget;
		panel->setObjectName(QStringLiteral("adoptedPanel"));
		window.adopt_panel(qt::settings_page::appearance, panel);
		auto* const surface = window.page_widget(qt::settings_page::appearance);
		expect(panel->parentWidget() == surface, "the adopted panel becomes the page's child");
		auto* const notice = surface->findChild<QLabel*>(QStringLiteral("settingsPageNotice"));
		expect(notice != nullptr && notice->isHidden(), "the notice is hidden once a panel is placed");
		window.present(qt::settings_page::general);
		QApplication::processEvents();
		expect(window.isVisible() && window.current_page() == qt::settings_page::general,
			   "present shows the window on the requested page");
		window.close();
		QApplication::processEvents();
		expect(!window.isVisible() && surface->findChild<QWidget*>(QStringLiteral("adoptedPanel")) == panel,
			   "closing hides the window and keeps the adopted panel");
	}

	void test_the_workbench_mounts_settings_with_the_preferences_shortcut()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		auto* const          action   = workbench.settings_action();
		expect(settings != nullptr && settings->parentWidget() == &workbench,
			   "the workbench owns a Settings window");
		expect(action != nullptr && action->shortcut() == QKeySequence{ QKeySequence::Preferences }
				   && action->menuRole() == QAction::PreferencesRole,
			   "Settings is reached by the platform preferences shortcut");
		auto* const appearance = workbench.appearance_settings();
		expect(appearance != nullptr
				   && appearance->parentWidget() == settings->page_widget(qt::settings_page::appearance),
			   "the appearance panel lives on the Settings window's Appearance page");
		expect(workbench.findChild<QWidget*>(QStringLiteral("appearanceSettingsPanel")) == appearance
				   && workbench.findChild<QWidget*>(QStringLiteral("workbenchReviewHeader")) == nullptr
				   && !workbench.files_drawer()->isAncestorOf(appearance),
			   "there is no review header any more, and the appearance panel is not "
			   "in the files drawer either");
		action->trigger();
		QApplication::processEvents();
		expect(settings->isVisible() && settings->current_page() == qt::settings_page::appearance,
			   "the action presents Settings on Appearance");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_every_page_exists_with_a_notice();
	test_adopting_a_panel_replaces_the_notice();
	test_the_workbench_mounts_settings_with_the_preferences_shortcut();
	std::cout << "settings-window-tests:pass\n";
	return EXIT_SUCCESS;
}
