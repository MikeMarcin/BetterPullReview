/// \file
/// \brief Proves the Settings window places panels and holds no state of its own.
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QVBoxLayout>
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

	void test_large_page_scrolls_without_growing_the_window()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		auto* const          panel    = new QWidget;
		auto* const          layout   = new QVBoxLayout{ panel };
		auto* const          first    = new QPushButton{ QStringLiteral("First setting"), panel };
		auto* const          last     = new QPushButton{ QStringLiteral("Last setting"), panel };
		layout->addWidget(first);
		layout->addSpacing(1200);
		layout->addWidget(last);
		settings->adopt_panel(qt::settings_page::general, panel);
		settings->resize(760, 480);
		settings->present(qt::settings_page::general);
		QApplication::processEvents();
		expect(settings->size() == QSize(760, 480), "a tall page does not force Settings beyond its requested size");
		auto* const scroll = qobject_cast<QScrollArea*>(settings->pages()->currentWidget());
		expect(scroll != nullptr && scroll->verticalScrollBar()->maximum() > 0,
			   "the page scrolls inside the bounded Settings window");
		first->setFocus();
		QKeyEvent tab{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
		QApplication::sendEvent(first, &tab);
		QApplication::processEvents();
		expect(last->hasFocus() && scroll->viewport()->rect().contains(last->mapTo(scroll->viewport(), last->rect().center())),
			   "keyboard navigation scrolls the last setting into view");
		const auto position = scroll->verticalScrollBar()->value();
		settings->sidebar()->setCurrentRow(static_cast<int>(qt::settings_page::appearance));
		settings->sidebar()->setCurrentRow(static_cast<int>(qt::settings_page::general));
		QApplication::processEvents();
		expect(scroll->verticalScrollBar()->value() == position && panel->parentWidget() == settings->page_widget(qt::settings_page::general),
			   "switching pages retains scroll and panel identity");
		const auto raster = settings->grab();
		expect(!raster.isNull(), "the composed Settings window renders at minimum size");
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(last, &escape);
		QApplication::processEvents();
		expect(!settings->isVisible() && panel->parentWidget() != nullptr,
			   "Escape hides Settings without deleting its panels");
	}

	void test_composed_pages_fit_and_render_at_minimum_size()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		settings->resize(760, 480);
		for (const auto page : { qt::settings_page::accounts_and_hosts, qt::settings_page::reviews_and_queue, qt::settings_page::appearance }) {
			settings->present(page);
			QApplication::processEvents();
			expect(settings->size() == QSize(760, 480), "every live composed page fits the minimum Settings window");
			auto* const scroll = qobject_cast<QScrollArea*>(settings->pages()->currentWidget());
			expect(scroll != nullptr && scroll->viewport()->height() <= 480,
				   "real account, review and appearance controls stay in the page viewport");
			const auto raster = settings->grab();
			expect(!raster.isNull(), "each live composed page renders");
			const auto root = qEnvironmentVariable("BPR_SETTINGS_RENDER_ROOT");
			if (!root.isEmpty()) {
				expect(raster.save(root + QStringLiteral("/settings-%1.png").arg(static_cast<int>(page))), "save composed Settings evidence");
			}
		}
		expect(settings->windowTitle().contains(QStringLiteral("Default profile")), "Settings identifies the profile being edited");
	}

	void test_close_cancels_preview_without_destroying_accepted_controls()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		auto* const          panel    = workbench.appearance_settings();
		auto                 cancels  = 0U;
		panel->set_dispatch([&](application::appearance_intent aIntent) {
			expect(std::holds_alternative<application::cancel_appearance_palette_preview_intent>(aIntent),
				   "closing emits only the preview cancellation intent");
			++cancels;
		});
		settings->present(qt::settings_page::appearance);
		QApplication::processEvents();
		panel->preview_import(qt::builtin_appearance_palette(core::appearance_variant::dark));
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(settings, &escape);
		expect(!settings->isVisible() && !panel->preview_palette().has_value() && cancels == 1U,
			   "Escape cancels the visible preview");
		settings->present(qt::settings_page::appearance);
		settings->close();
		expect(cancels == 2U && workbench.appearance_settings() == panel,
			   "native close also cancels pending imports before a preview arrives and keeps the same controls");
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
		expect(workbench.session_panel() != nullptr && workbench.device_authorization() != nullptr
				   && settings->page_widget(qt::settings_page::accounts_and_hosts)
						  ->isAncestorOf(workbench.session_panel())
				   && settings->page_widget(qt::settings_page::accounts_and_hosts)
						  ->isAncestorOf(workbench.device_authorization()),
			   "the session chrome and the device sign-in live on the Accounts and "
			   "hosts page, as the design places them");
		action->trigger();
		QApplication::processEvents();
		expect(settings->isVisible() && settings->current_page() == qt::settings_page::appearance,
			   "the action presents Settings on Appearance");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_composed_pages_fit_and_render_at_minimum_size();
	test_large_page_scrolls_without_growing_the_window();
	test_close_cancels_preview_without_destroying_accepted_controls();
	test_every_page_exists_with_a_notice();
	test_adopting_a_panel_replaces_the_notice();
	test_the_workbench_mounts_settings_with_the_preferences_shortcut();
	std::cout << "settings-window-tests:pass\n";
	return EXIT_SUCCESS;
}
