/// \file
/// \brief Proves the Settings window places panels and holds no state of its own.
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/prompt_features.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/resident_filter.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontInfo>
#include <QFormLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QProxyStyle>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QSysInfo>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

		workbench.set_storage_location(QStringLiteral("/fixture/") + QString(500, QLatin1Char('x')));
		workbench.publish(application::queue_landing_projection{
			.Status            = application::queue_landing_status::ready,
			.Session           = application::queue_session_id{ .Host = "github.com", .Account = "reviewer", .Generation = 1U },
			.SavedRepositories = { application::saved_repository{ .Host = "github.com", .Account = "reviewer", .Repository = "long-fixture-owner/" + std::string(100, 'r') } },
			.SavedServers      = { application::saved_server{ .Hostname = std::string(60, 'h') + ".example.com", .Account = std::string(39, 'a') } } });

		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark,
								 core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto appearance = qt::appearance_profile{ };
			appearance.select_mode(mode);
			appearance.apply_to(workbench);
			appearance.apply_to(*settings);
			for (auto index = 0; index < static_cast<int>(qt::settings_page_count); ++index) {
				const auto page = static_cast<qt::settings_page>(index);
				settings->present(page);
				QApplication::processEvents();
				expect(settings->size() == QSize(760, 480), "every live composed page fits the minimum Settings window");
				auto* const scroll = qobject_cast<QScrollArea*>(settings->pages()->currentWidget());
				expect(scroll != nullptr && scroll->viewport()->height() <= 480,
					   "composed controls stay in a bounded page viewport");
				std::cout << "settings-layout: page=" << index << " mode=" << static_cast<int>(mode)
						  << " horizontal=" << scroll->horizontalScrollBar()->maximum() << '\n';
				expect(scroll->horizontalScrollBar()->maximum() == 0, "minimum-size Settings does not require sideways scrolling, including long metadata");
				const auto raster = settings->grab();
				expect(!raster.isNull(), "each live composed page renders");
				const auto root = qEnvironmentVariable("BPR_SETTINGS_RENDER_ROOT");
				if (!root.isEmpty()) {
					expect(raster.save(root + QStringLiteral("/settings-%1-%2.png").arg(index).arg(static_cast<int>(mode))), "save composed Settings evidence");
				}
			}
		}
		std::cout << "settings-platform: " << QSysInfo::productType().toStdString() << ' '
				  << QSysInfo::currentCpuArchitecture().toStdString() << " Qt=" << qVersion()
				  << " scale=" << settings->devicePixelRatioF() << " pages=" << settings->pages()->count() << '\n';

		expect(settings->windowTitle().contains(QStringLiteral("Default profile")), "Settings identifies the profile being edited");
	}

	void test_close_cancels_preview_without_destroying_accepted_controls()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		auto* const          panel    = workbench.appearance_settings();
		auto                 cancels  = 0U;
		panel->set_dispatch([&](application::appearance_intent aIntent) {
			if (std::holds_alternative<application::preview_appearance_palette_intent>(aIntent)) {
				return;
			}
			expect(std::holds_alternative<application::cancel_appearance_palette_preview_intent>(aIntent),
				   "closing emits only the preview cancellation intent");
			++cancels;
		});
		settings->present(qt::settings_page::appearance);
		QApplication::processEvents();
		// A palette previewed by a picked colour; the picker is a seam.
		panel->set_colour_picker([](const qt::colour_pick_request&) -> std::optional<QColor> { return QColor{ 0x1a, 0x4a, 0x2e }; });
		panel->colours()->swatch_of(core::appearance_role::diff_added_background)->click();
		QApplication::processEvents();
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(settings, &escape);
		expect(!settings->isVisible() && !panel->preview_palette().has_value() && cancels == 1U,
			   "Escape cancels the visible preview");
		settings->present(qt::settings_page::appearance);
		settings->close();
		expect(cancels == 2U && workbench.appearance_settings() == panel,
			   "native close also cancels the preview and keeps the same controls");
	}

	void test_the_workbench_mounts_settings_with_the_preferences_shortcut()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		auto* const          action   = workbench.settings_action();
		expect(settings != nullptr && settings->parentWidget() == &workbench,
			   "the workbench owns a Settings window");
		expect(action != nullptr && action->shortcut() == QKeySequence{ Qt::CTRL | Qt::Key_Comma }
				   && action->menuRole() == QAction::PreferencesRole
				   && action->shortcutContext() == Qt::WindowShortcut,
			   "Settings has explicit Command/Ctrl-comma scoped to the invoking workbench");
		auto* const appearance = workbench.appearance_settings();
		expect(appearance != nullptr
				   && appearance->parentWidget() == settings->page_widget(qt::settings_page::appearance),
			   "the appearance panel lives on the Settings window's Appearance page");
		expect(workbench.findChild<QWidget*>(QStringLiteral("appearanceSettingsPanel")) == appearance
				   && workbench.findChild<QWidget*>(QStringLiteral("workbenchReviewHeader")) == nullptr
				   && !workbench.files_drawer()->isAncestorOf(appearance),
			   "there is no review header any more, and the appearance panel is not "
			   "in the files drawer either");
		// [[BPR-0771]]. The session is a presentation model now rather than a
		// widget, so what the page holds is its sentence and the device sign-in.
		auto* const accounts = settings->page_widget(qt::settings_page::accounts_and_hosts);
		expect(workbench.session_panel() != nullptr && workbench.device_authorization() != nullptr
				   && accounts != nullptr
				   && accounts->findChild<QLabel*>(QStringLiteral("accountsLede")) != nullptr
				   && accounts->isAncestorOf(workbench.device_authorization()),
			   "the accounts sentence and the device sign-in live on the Accounts and "
			   "hosts page, as the design places them");
		action->trigger();
		QApplication::processEvents();
		expect(settings->isVisible() && settings->current_page() == qt::settings_page::appearance,
			   "the action presents Settings on Appearance");
	}

	/// \brief [[BPR-1289]]. The results' first lines, the settings they name.
	[[nodiscard]] auto result_names(const qt::settings_window& aSettings) -> QStringList
	{
		auto names = QStringList{ };
		for (auto row = 0; row < aSettings.search_results()->count(); ++row) {
			names.push_back(aSettings.search_results()->item(row)->text().section(QLatin1Char('\n'), 0, 0));
		}
		return names;
	}

	void send_key(QWidget* const aTarget, const int aKey)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QApplication::sendEvent(aTarget, &press);
	}

	/// \brief [[BPR-1289]]. Saves the Settings window with a search showing,
	/// when `BPR_SETTINGS_RENDER_ROOT` names a directory for evidence.
	void save_search_render(qt::settings_window& aSettings, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_SETTINGS_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QApplication::processEvents();
		expect(aSettings.grab().save(root + QStringLiteral("/search-%1.png").arg(aName)), "save Settings search evidence");
	}

	/// \brief [[BPR-1597]]. Settings by task (owner's pick A, 2026-10-17,
	/// the middle page named Review): the sidebar's pages and their order, and
	/// search finding every row that left Reviews & queue on its new page and
	/// section. Negative control: nothing is found under Reviews & queue.
	void test_settings_are_organised_by_task()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		settings->resize(1'100, 900);
		settings->present(qt::settings_page::general);
		QApplication::processEvents();
		auto pages = QStringList{ };
		for (auto row = 0; row < settings->sidebar()->count(); ++row) {
			pages.push_back(settings->sidebar()->item(row)->text());
		}
		expect(pages == QStringList{ QStringLiteral("General"), QStringLiteral("Profiles"), QStringLiteral("Accounts & hosts"), QStringLiteral("Repositories"), QStringLiteral("Home & queue"), QStringLiteral("Review"), QStringLiteral("Comments"), QStringLiteral("Appearance"), QStringLiteral("Notifications"), QStringLiteral("Agents"), QStringLiteral("Prompts"), QStringLiteral("Storage & privacy") },
			   "the sidebar is General, Profiles, Accounts & hosts, Repositories, Home & queue, Review, Comments, then the rest as before");
		for (auto index = std::size_t{ 0U }; index < qt::settings_page_count; ++index) {
			const auto page = static_cast<qt::settings_page>(index);
			expect(qt::settings_page_title(page) == pages.at(static_cast<int>(index)), "each page's row is its own value");
		}
		auto* const search = settings->search_field();
		struct expected_row
		{
			QString Query;
			QString Name;
			QString Location;
		};
		const auto rows = std::array{
			expected_row{ QStringLiteral("A pull request opens in"), QStringLiteral("A pull request opens in"), QStringLiteral("Review · Opening") },
			expected_row{ QStringLiteral("On return, compare"), QStringLiteral("On return, compare"), QStringLiteral("Review · Opening") },
			expected_row{ QStringLiteral("Mark a file read"), QStringLiteral("Mark a file read"), QStringLiteral("Review · Reading") },
			expected_row{ QStringLiteral("Comments drawer opens on"), QStringLiteral("Comments drawer opens on"), QStringLiteral("Review · Reading") },
			expected_row{ QStringLiteral("Reading width"), QStringLiteral("Reading width"), QStringLiteral("Review · Reading") },
			expected_row{ QStringLiteral("Drawer controls"), QStringLiteral("Drawer controls"), QStringLiteral("Review · Layout") },
			expected_row{ QStringLiteral("Line details"), QStringLiteral("Line details"), QStringLiteral("Review · Layout") },
			expected_row{ QStringLiteral("Center ribbon width"), QStringLiteral("Center ribbon width"), QStringLiteral("Review · Layout") },
			expected_row{ QStringLiteral("Label marks"), QStringLiteral("Label marks"), QStringLiteral("Comments · Labels") },
			expected_row{ QStringLiteral("Feedback guidance"), QStringLiteral("Feedback guidance"), QStringLiteral("Comments · Comment workshop") },
			expected_row{ QStringLiteral("Images"), QStringLiteral("Images"), QStringLiteral("Comments · Descriptions and comments") },
			expected_row{ QStringLiteral("Observer reason"), QStringLiteral("Observer reason"), QStringLiteral("Home & queue · Queue defaults") },
		};
		for (const auto& each : rows) {
			search->setText(each.Query);
			QApplication::processEvents();
			auto found = false;
			for (auto row = 0; row < settings->search_results()->count(); ++row) {
				auto* const item = settings->search_results()->item(row);
				found            = found || (item->text().section(QLatin1Char('\n'), 0, 0) == each.Name && item->data(qt::settings_search_location_role).toString() == each.Location);
			}
			if (!found) {
				std::cerr << "results for " << each.Query.toStdString() << ':';
				for (auto row = 0; row < settings->search_results()->count(); ++row) {
					std::cerr << " [" << settings->search_results()->item(row)->text().toStdString() << " @ " << settings->search_results()->item(row)->data(qt::settings_search_location_role).toString().toStdString() << ']';
				}
				std::cerr << '\n';
			}
			expect(found, "search finds " + each.Name.toStdString() + " on " + each.Location.toStdString());
		}
		for (const auto* const query : { "Reviews & queue", "Label", "Comment", "Queue" }) {
			search->setText(QString::fromUtf8(query));
			QApplication::processEvents();
			for (auto row = 0; row < settings->search_results()->count(); ++row) {
				expect(!settings->search_results()->item(row)->data(qt::settings_search_location_role).toString().startsWith(QStringLiteral("Reviews & queue")),
					   "negative control: no result is under Reviews & queue");
			}
		}
		// Return goes to the row on its new page, changing nothing.
		search->setText(QStringLiteral("Feedback guidance"));
		QApplication::processEvents();
		auto* const helper = workbench.label_helper_switch();
		const auto  was    = helper->isChecked();
		expect(result_names(*settings).value(0) == QStringLiteral("Feedback guidance"), "Feedback guidance is the first result");
		send_key(search, Qt::Key_Return);
		QApplication::processEvents();
		expect(settings->current_page() == qt::settings_page::comments && settings->page_widget(qt::settings_page::comments)->isAncestorOf(helper) && helper->isChecked() == was,
			   "choosing Feedback guidance opens Comments and changes nothing");
		search->clear();
		QApplication::processEvents();
		// Each page hosts its own rows.
		expect(settings->page_widget(qt::settings_page::review)->isAncestorOf(workbench.behaviour_editor())
				   && settings->page_widget(qt::settings_page::home_and_queue)->isAncestorOf(workbench.queue_role_filter())
				   && settings->page_widget(qt::settings_page::comments)->isAncestorOf(workbench.label_columns_editor()),
			   "Review hosts the review rows, Home & queue the queue defaults, Comments the label rows");
	}

	/// \brief [[BPR-1289]]. The search field filters the settings the pages
	/// show, lists them by page and section, and goes to one without changing it.
	void test_settings_search_finds_and_goes_to_a_setting()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		settings->present(qt::settings_page::general);
		QApplication::processEvents();
		auto* const search = settings->search_field();
		expect(search != nullptr && search->accessibleName() == QStringLiteral("Search settings") && search->isClearButtonEnabled()
				   && search->focusPolicy() != Qt::NoFocus,
			   "the search field has an accessible name and a clear button and takes the keyboard");
		expect(settings->sidebar()->isVisibleTo(settings) && !settings->search_results()->isVisibleTo(settings)
				   && !settings->search_notice()->isVisibleTo(settings),
			   "an empty field shows the ordinary page list");
		auto findBound = false;
		for (const auto* const shortcut : settings->findChildren<QShortcut*>()) {
			findBound = findBound || shortcut->key() == QKeySequence{ QKeySequence::Find };
		}
		expect(findBound, "the platform's find key reaches the search field");

		search->setText(QStringLiteral("code font"));
		save_search_render(*settings, QStringLiteral("code-font"));
		expect(!settings->sidebar()->isVisibleTo(settings) && settings->search_results()->isVisibleTo(settings),
			   "results replace the page list while the field holds text");
		auto* const first = settings->search_results()->item(0);
		expect(!result_names(*settings).isEmpty() && result_names(*settings).front() == QStringLiteral("Code font")
				   && first->data(qt::settings_search_location_role).toString() == QStringLiteral("Appearance · Type")
				   && first->data(Qt::AccessibleTextRole).toString() == QStringLiteral("Code font, Appearance · Type"),
			   "a result names the setting, then its page and section");
		expect(first->data(qt::settings_search_name_spans_role).toList() == QVariantList{ QPoint{ 0, 9 } },
			   "the name's matched letters are marked for highlighting");
		auto* const appearance = settings->page_widget(qt::settings_page::appearance);
		auto* const codeFont   = appearance->findChild<QComboBox*>(QStringLiteral("appearanceCodeFamily"));
		expect(codeFont != nullptr, "the Code font row's chooser exists");
		const auto before = codeFont->currentIndex();
		send_key(search, Qt::Key_Return);
		QApplication::processEvents();
		expect(settings->current_page() == qt::settings_page::appearance, "Return opens the first result's page");
		auto* const focused = settings->focusWidget();
		expect(focused == codeFont, "the chosen setting has the focus");
		expect(codeFont->currentIndex() == before, "choosing a result changes no setting");

		search->setText(QStringLiteral("appear"));
		expect(result_names(*settings) == QStringList{ QStringLiteral("Appearance") }
				   && settings->search_results()->item(0)->data(qt::settings_search_location_role).toString().isEmpty()
				   && settings->search_results()->item(0)->data(Qt::AccessibleTextRole).toString() == QStringLiteral("Appearance, settings page"),
			   "a matching page is a result of its own, on one line");
		expect(settings->search_results()->item(0)->data(qt::settings_search_name_spans_role).toList() == QVariantList{ QPoint{ 0, 6 } },
			   "a page's matched letters are marked too");

		search->setText(QStringLiteral("type"));
		auto sectionMarked = false;
		for (auto row = 0; row < settings->search_results()->count(); ++row) {
			const auto* const item = settings->search_results()->item(row);
			if (item->text() == QStringLiteral("Code font")) {
				sectionMarked = item->data(qt::settings_search_location_spans_role).toList() == QVariantList{ QPoint{ 13, 4 } }
					&& item->data(qt::settings_search_name_spans_role).toList().isEmpty();
			}
		}
		expect(sectionMarked, "a setting found by its section marks the section in its location");
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark,
								 core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(mode);
			profile.apply_to(*settings);
			save_search_render(*settings, QStringLiteral("type-%1").arg(static_cast<int>(mode)));
		}

		search->setText(QStringLiteral("dif"));
		expect(!result_names(*settings).contains(QStringLiteral("Code font")),
			   "a setting whose description alone holds the query waits while names hold it");
		expect(settings->search_results()->currentItem() != nullptr && settings->search_results()->currentItem()->text() == QStringLiteral("Dif")
				   && settings->search_results()->currentRow() > 0,
			   "the best match is chosen, though the list keeps the pages' order");
		save_search_render(*settings, QStringLiteral("dif"));
		search->setText(QStringLiteral("notif"));
		save_search_render(*settings, QStringLiteral("notif"));
		const auto notifications = result_names(*settings);
		expect(notifications.size() >= 2 && notifications.front() == QStringLiteral("Notifications"),
			   "a page leads its group, followed by its matching settings");
		expect(notifications == result_names(*settings), "the order is stable");
		search->setFocus(Qt::OtherFocusReason);
		const auto chosen = settings->search_results()->currentRow();
		send_key(search, Qt::Key_Down);
		expect(settings->focusWidget() == search && settings->search_results()->currentRow() == chosen + 1,
			   "Down chooses the next result while the caret stays in the field");
		send_key(search, Qt::Key_Up);
		send_key(search, Qt::Key_Up);
		expect(settings->search_results()->currentRow() == 0, "Up chooses the previous result and stops at the first");
		send_key(search, Qt::Key_Down);
		send_key(search, Qt::Key_Return);
		QApplication::processEvents();
		expect(settings->current_page() == qt::settings_page::notifications && settings->focusWidget() != search,
			   "Return goes to the chosen result's page and setting");
		settings->search_results()->setFocus(Qt::OtherFocusReason);
		send_key(settings->search_results(), Qt::Key_Escape);
		expect(settings->isVisible() && settings->focusWidget() == search && search->text() == QStringLiteral("notif"),
			   "Escape in the results returns to the query and keeps it");

		search->setText(QStringLiteral("zzzq"));
		save_search_render(*settings, QStringLiteral("no-match"));
		expect(settings->search_notice()->isVisibleTo(settings) && !settings->search_results()->isVisibleTo(settings)
				   && static_cast<qt::filter_empty_notice*>(settings->search_notice())->message().contains(QStringLiteral("zzzq")),
			   "no match says so");
		static_cast<qt::filter_empty_notice*>(settings->search_notice())->clear_button()->click();
		expect(search->text().isEmpty() && settings->sidebar()->isVisibleTo(settings), "Clear restores the page list");

		search->setText(QStringLiteral("theme"));
		send_key(search, Qt::Key_Escape);
		expect(search->text().isEmpty() && settings->isVisible() && settings->sidebar()->isVisibleTo(settings),
			   "Escape clears a search and leaves Settings open");
		send_key(search, Qt::Key_Escape);
		expect(!settings->isVisible(), "Escape in an empty field still closes Settings");
	}

	/// \brief [[BPR-1753]], owner decision 2026-10-23. The Claude
	/// process wrapper and model rows are gone from Agents; the
	/// agent list holds each agent's program, arguments and model, and
	/// withdrawing the agent surfaces takes the list with them.
	void test_agents_page_is_the_agent_list()
	{
		qt::workbench_window workbench;
		auto* const          page = workbench.settings()->page_widget(qt::settings_page::agents_and_context);
		expect(page->findChild<QLineEdit*>(QStringLiteral("claudeWrapperPath")) == nullptr && page->findChild<QLineEdit*>(QStringLiteral("claudeModel")) == nullptr,
			   "the old Claude wrapper and model rows are not on the page");
		expect(page->findChild<QListWidget*>(QStringLiteral("settingsAgentList")) != nullptr && page->findChild<QPushButton*>(QStringLiteral("settingsAddAgent")) != nullptr,
			   "the page is the agent list with Add agent…");
		// [[BPR-1753]]. The review skill's Restore default has its own
		// "Save my copy to Downloads first", the run limit its toggle, and
		// (RFC-BPR-0104 as amended 2026-11-11) the scratch folders how long
		// an unused one is kept, and ([[BPR-2293]]) the retained agent
		// workspaces theirs; none is a sign-in switch.
		const auto boxes = page->findChildren<QCheckBox*>();
		expect(std::ranges::count_if(boxes,
									 [](const QCheckBox* aBox) {
										 return aBox->objectName() != QStringLiteral("reviewSkillKeepCopy") && aBox->objectName() != QStringLiteral("settingsRunLimitOn")
											 && aBox->objectName() != QStringLiteral("settingsScratchKeepOn") && aBox->objectName() != QStringLiteral("settingsWorkspaceKeepOn");
									 })
				   == 0,
			   "no sign-in switch is offered, and [[BPR-2051]] the next request's choices left the page (the footer picks the agent; the Agent tab what a question includes)");
		expect(page->findChild<QWidget*>(QStringLiteral("settingsNextRequest")) == nullptr && page->findChild<QComboBox*>(QStringLiteral("settingsAgentProfile")) == nullptr,
			   "[[BPR-2051]] the page opens on the agents roster");
		auto* const settings = workbench.settings();
		settings->search_field()->setText(QStringLiteral("process wrapper"));
		expect(!result_names(*settings).contains(QStringLiteral("Process wrapper")), "search finds no wrapper row");
	}

	/// \brief [[BPR-1289]]. The index is the pages as shown: a withdrawn page is
	/// never searched, and a newly placed setting is found without a second list.
	void test_settings_search_reads_the_pages_as_shown()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		settings->search_field()->setText(QStringLiteral("stop a run"));
		expect(result_names(*settings).contains(QStringLiteral("Stop a run after")), "a setting on a page is found");
		settings->withdraw_page(qt::settings_page::agents_and_context);
		expect(!result_names(*settings).contains(QStringLiteral("Stop a run after"))
				   && !result_names(*settings).contains(QStringLiteral("Agents")),
			   "a withdrawn page never appears");
		auto* const panel  = new QWidget;
		auto* const column = new QVBoxLayout{ panel };
		auto* const toggle = new QCheckBox{ QStringLiteral("Brand new toggle"), panel };
		toggle->setAccessibleDescription(QStringLiteral("Draws zebra stripes behind long lists"));
		column->addWidget(toggle);
		settings->adopt_panel(qt::settings_page::general, panel);
		settings->search_field()->setText(QStringLiteral("brand toggle"));
		expect(result_names(*settings) == QStringList{ QStringLiteral("Brand new toggle") }, "a newly placed setting is searchable");
		settings->search_field()->setText(QStringLiteral("zebra"));
		expect(result_names(*settings) == QStringList{ QStringLiteral("Brand new toggle") }
				   && settings->search_results()->item(0)->toolTip().contains(QStringLiteral("zebra stripes")),
			   "a word only its description holds finds the setting, and the tip shows the description");
	}

	void test_settings_shortcut_selects_the_active_workbench()
	{
		qt::workbench_window first;
		qt::workbench_window second;
		first.show();
		second.show();
		QApplication::processEvents();
		for (auto* const window : { &first, &second }) {
			window->activateWindow();
			window->setFocus();
			QApplication::processEvents();
			QKeyEvent press{ QEvent::KeyPress, Qt::Key_Comma, Qt::ControlModifier };
			QKeyEvent release{ QEvent::KeyRelease, Qt::Key_Comma, Qt::ControlModifier };
			QApplication::sendEvent(window, &press);
			QApplication::sendEvent(window, &release);
			QApplication::processEvents();
			expect(window->settings()->isVisible(), "the active workbench's comma shortcut is not ambiguous with its sibling");
			auto* const other = window == &first ? &second : &first;
			expect(!other->settings()->isVisible(), "the inactive workbench's Settings action does not receive the shortcut");
			window->settings()->close();
		}
	}
} // namespace

/// \brief [[BPR-2051]], owner decision 2026-11-11 (mockup "A · Roster").
/// `--agents-roster FOLDER` saves Settings › Agents with three
/// agents: one passed, one not signed in, one never checked. Run it under
/// `BPR_NATIVE_LOOK` with each `BPR_NATIVE_LOOK_SCHEME` for the owner's look.
void render_agents_roster(const QString& aFolder, const bool aLook = false)
{
	qt::workbench_window window;
	auto                 preferences = application::agent_setup_preferences{ };
	auto                 claude      = core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, "/opt/homebrew/bin/claude");
	claude.Model                     = "claude-opus-5-5";
	claude.Effort                    = "high";
	preferences.Agents               = *core::add_agent_entry({ }, claude);
	preferences.Agents               = *core::add_agent_entry(preferences.Agents, core::preset_agent_entry(core::setup_agent::codex, core::codex_protocol_choice::app_server, "/opt/homebrew/bin/codex"));
	preferences.Agents               = *core::add_agent_entry(preferences.Agents, { .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/claude-wrapper", .Model = "claude-sonnet-5-5", .Effort = "medium" });
	// [[BPR-2007]], [[BPR-2008]]. Codex may build and test; Work Claude's
	// scratch folder starts off; Claude Code keeps the defaults.
	preferences.Agents[1].BuildsAndTests = true;
	preferences.Agents[2].ScratchStarts  = false;
	preferences.EntryChecks              = { { .Id = preferences.Agents[0].Id, .Check = { .State = core::setup_check_state::passed, .Version = "2.1.321" } },
											 { .Id = preferences.Agents[1].Id, .Check = core::fail_check(core::agent_setup_failure::not_signed_in) } };
	window.agent_programs()->present(preferences, { { .Agent = core::setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude" } });
	// RFC-BPR-0104 as amended 2026-11-11: what the scratch folders hold.
	window.agent_programs()->present_scratch_usage(std::pair<std::uint64_t, std::size_t>{ 41U * 1024U * 1024U + 300U * 1024U, 3U });
	if (aLook) {
		// A row's sheet opens and its OK answers; nothing is kept. Clear
		// scratch folders answers as the application does.
		window.agent_programs()->set_dispatch({ .CheckAll = [] { }, .CheckEntry = [](std::uint32_t) { }, .Update = [](core::agent_entry, qt::agent_programs_panel::answer aAnswer) { aAnswer(std::nullopt); }, .SetScratchPolicy = [](std::uint32_t, std::uint32_t) { return true; }, .ClearScratch = [&window] { window.agent_programs()->present_scratch_usage(std::pair<std::uint64_t, std::size_t>{ 0U, 0U }, QStringLiteral("Cleared 41.3 MB from 3 folders.")); } });
	}
	auto* const settings = window.settings();
	settings->resize(980, 760);
	settings->present(qt::settings_page::agents_and_context);
	settings->show();
	for (int pass = 0; pass < 4; ++pass) {
		QApplication::processEvents();
	}
	if (aLook) {
		// The owner's look: the page live. Rows open their sheet, their menu
		// checks, moves or removes (nothing is saved: no host behind it).
		settings->setWindowTitle(QStringLiteral("Settings › Agents: the scratch folder limit, how long unused folders are kept, what they hold, and Clear scratch folders"));
		QApplication::exec();
		return;
	}
	const auto scheme = qEnvironmentVariable("BPR_NATIVE_LOOK_SCHEME", QStringLiteral("light"));
	expect(settings->grab().save(QStringLiteral("%1/agents-roster-%2.png").arg(aFolder, scheme)), "the roster render saves");
	auto* const programs = window.agent_programs();
	expect(programs->roster_row_text(0).contains(QStringLiteral("Signed in, version 2.1.321")) && programs->roster_row_text(1).contains(QStringLiteral("Not signed in"))
			   && programs->roster_row_text(1).contains(QStringLiteral("codex login")) && programs->roster_row_text(2).contains(QStringLiteral("Not checked")),
		   "each row says its kept check");
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	if (argc > 1 && std::string_view{ argv[1] } == "--agents-roster-look") {
		render_agents_roster({ }, true);
		return EXIT_SUCCESS;
	}
	if (argc > 2 && std::string_view{ argv[1] } == "--agents-roster") {
		render_agents_roster(QString::fromLocal8Bit(argv[2]));
		std::cout << "settings-window:rendered\n";
		return EXIT_SUCCESS;
	}
	test_composed_pages_fit_and_render_at_minimum_size();
	test_large_page_scrolls_without_growing_the_window();
	test_close_cancels_preview_without_destroying_accepted_controls();
	test_every_page_exists_with_a_notice();
	{
		qt::workbench_window        window;
		auto*                       panel = window.writing_hooks();
		auto*                       hook  = panel->findChild<QListWidget*>("writingHookChoice");
		auto*                       text  = panel->findChild<QPlainTextEdit*>("writingHookInstructions");
		unsigned                    tests = 0, saves = 0;
		core::writing_hook_settings settings;
		panel->set_dispatch([&](core::writing_hook aSelected, std::string aInstructions) { ++saves;settings.Overrides[static_cast<std::size_t>(aSelected)]=std::move(aInstructions);++settings.Revision;panel->present(settings);return true; }, [&](core::writing_hook) { ++tests; });
		expect(hook->count() == 24 && hook->item(0)->text() == "Tour tab" && hook->item(3)->text() == "Enhance a stop" && hook->item(4)->text() == "Comments and notes" && hook->item(5)->text() == "Review feedback"
				   && hook->item(6)->text() == "Enhance writing" && hook->item(7)->text() == "Local drafts" && hook->item(8)->text() == "Description writing"
				   && hook->item(9)->text() == "Change discussion" && hook->item(10)->text() == "Commit message" && hook->item(11)->text() == "Plan and run a check" && hook->item(12)->text() == "Agent tab" && hook->item(13)->text() == "Suggested comments"
				   && hook->item(14)->text() == "What changed" && hook->item(15)->text() == "Was this addressed" && hook->item(16)->text() == "Explain this file" && hook->item(17)->text() == "Suggest the change" && hook->item(18)->text() == "Update from target" && hook->item(19)->text() == "Reconcile after an update" && hook->item(20)->text() == "Review skill" && hook->item(21)->text() == "Features" && hook->item(22)->text() == "Writing style" && hook->item(23)->text() == "Comment labels" && !text->toPlainText().isEmpty(),
			   "[[BPR-1917]], [[BPR-2051]], [[BPR-2059]], RFC-BPR-0110, RFC-BPR-0114, RFC-BPR-0116 every built-in prompt is listed under where it is used: [[BPR-2274]] two tour prompts and a stop's Enhance, review feedback and the one Enhance writing prompt (owner look 2026-11-10), the three local-authoring prompts, the suggested-comment format, the returning questions, the update prompt, RFC-BPR-0096's reconcile prompt and the review skill, then [[BPR-2264]] the Writing style Feature under Features");
		{
			// RFC-BPR-0096, continue-at-latest amendment: Ask <agent> to
			// reconcile's prompt is edited, saved and reset like the others,
			// and is not tried on a sample passage.
			panel->select_hook(core::writing_hook::reconcile_move);
			const auto index = static_cast<std::size_t>(core::writing_hook::reconcile_move);
			expect(text->toPlainText().toUtf8().toStdString() == application::default_reconcile_prompt() && !panel->findChild<QPushButton*>("writingHookTest")->isEnabled(),
				   "the reconcile prompt shows its built-in text and has no sample test");
			text->setPlainText(QStringLiteral("Merge the saved branch and run the tests."));
			panel->findChild<QPushButton*>("writingHookSave")->click();
			expect(settings.Overrides[index] == "Merge the saved branch and run the tests." && application::effective_reconcile_prompt(settings) == "Merge the saved branch and run the tests.",
				   "a saved reconcile prompt is the one sent");
			panel->findChild<QPushButton*>("writingHookReset")->click();
			expect(text->toPlainText().toUtf8().toStdString() == application::default_reconcile_prompt(), "Reset brings the built-in reconcile prompt back");
		}
		{
			// [[BPR-2270]]. Comment labels shows the shipped convention;
			// cleared and saved it is kept blank (no labels), not reset, and
			// Reset brings the convention back.
			panel->select_hook(core::writing_hook::comment_labels);
			const auto index = static_cast<std::size_t>(core::writing_hook::comment_labels);
			expect(text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(core::writing_hook::comment_labels) && text->toPlainText().contains(QStringLiteral("`suggestion`"))
					   && !panel->findChild<QPushButton*>("writingHookTest")->isEnabled(),
				   "Comment labels shows the Conventional Comments convention and is tried by asking the agent");
			text->setPlainText(QString{ });
			panel->findChild<QPushButton*>("writingHookSave")->click();
			expect(settings.Overrides[index] == core::blank_feature_override && text->toPlainText().isEmpty()
					   && panel->findChild<QLabel*>("writingHookSaveState")->text().startsWith(QStringLiteral("Blank for this profile")),
				   "cleared and saved, Comment labels is kept blank and says nothing is sent");
			expect(application::comment_labels_instructions(settings).empty(), "and a blank Comment labels sends nothing");
			panel->findChild<QPushButton*>("writingHookReset")->click();
			expect(settings.Overrides[index].empty() && text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(core::writing_hook::comment_labels),
				   "Reset to built-in brings the convention back");
		}
		const std::array<core::writing_hook, 2> tourHooks = { core::writing_hook::outline, core::writing_hook::comment };
		for (const auto tourHook : tourHooks) {
			panel->select_hook(tourHook);
			text->setPlainText("precise");
			panel->findChild<QPushButton*>("writingHookSave")->click();
		}
		expect(tests == 0 && saves == 6, "loading and changing hooks never launches test: the reconcile prompt saved and reset, two tour prompts, and Comment labels blanked and reset");
		panel->findChild<QPushButton*>("writingHookReset")->click();
		expect(settings.Overrides[1].empty() && settings.Overrides[0] == "precise", "restore only current hook");
		panel->findChild<QPushButton*>("writingHookTest")->click();
		expect(tests == 1, "explicit test produces exactly one intent");
		// [[BPR-1090]] The fifth hook: its own version, its own override, its reset, and a line that
		// says it is sent only while the label helper is on. Test is for tour fields only.
		auto* scope  = panel->findChild<QLabel*>("writingHookScope");
		auto* source = panel->findChild<QLabel*>("writingHookSource");
		auto* test   = panel->findChild<QPushButton*>("writingHookTest");
		panel->select_hook(core::writing_hook::comment);
		expect(scope->isHidden() && test->isEnabled() && source->text().contains("tour-stop-comment-v2"), "a tour hook shows no feedback line and can be tested");
		panel->select_hook(core::writing_hook::feedback);
		expect(panel->selected_hook() == core::writing_hook::feedback && text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(core::writing_hook::feedback), "the feedback row previews the shipped guidance");
		expect(!scope->isHidden() && scope->text().startsWith("Off.") && source->text().contains("Built-in") && source->text().contains("review-feedback-v4"), "with the label helper off the row says the guidance is off");
		expect(!test->isEnabled() && !test->toolTip().isEmpty(), "the tour-field test is unavailable for the feedback hook, with the reason");
		test->click();
		expect(tests == 1, "and clicking it asks for nothing");
		panel->present_label_helper(application::review_label_helper::on);
		expect(scope->text().startsWith("On."), "turning the label helper on turns the guidance on");
		text->setPlainText("Team rule");
		panel->findChild<QPushButton*>("writingHookSave")->click();
		expect(settings.Overrides[2] == "Team rule" && settings.Overrides[0] == "precise" && source->text().contains("Changed from built-in"), "the feedback override is its own");
		panel->findChild<QPushButton*>("writingHookReset")->click();
		expect(settings.Overrides[2].empty() && text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(core::writing_hook::feedback), "Restore default brings back the shipped guidance");
		panel->present_label_helper(application::review_label_helper::off);
		expect(scope->text().startsWith("Off."), "and turning the helper off again says so");
		// [[BPR-2051]] Each Enhance prompt: its own default and version, its
		// own override, and its restore; the others are untouched.
		for (const auto [enhance, version] : std::array{ std::pair{ core::writing_hook::enhance_stop, "enhance-tour-stop-v1" }, std::pair{ core::writing_hook::enhance_writing, "enhance-writing-v1" } }) {
			const auto index = static_cast<std::size_t>(enhance);
			panel->select_hook(enhance);
			expect(text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(enhance) && source->text().contains(QString::fromLatin1(version)) && scope->isHidden() && test->isEnabled(),
				   "an Enhance prompt shows its shipped text and version, and can be tested");
			text->setPlainText("Our voice");
			panel->findChild<QPushButton*>("writingHookSave")->click();
			expect(settings.Overrides[index] == "Our voice" && settings.Overrides[0] == "precise" && source->text().contains("Changed from built-in"), "its override is its own");
			panel->findChild<QPushButton*>("writingHookReset")->click();
			expect(settings.Overrides[index].empty() && text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(enhance), "Reset to built-in restores it in one click");
		}
		// [[BPR-2059]] The suggested-comment format: its shipped text and
		// version, its own override and Reset; Test does not apply to it.
		{
			const auto index = static_cast<std::size_t>(core::writing_hook::suggested_comment);
			panel->select_hook(core::writing_hook::suggested_comment);
			expect(text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(core::writing_hook::suggested_comment) && source->text().contains(QStringLiteral("suggested-comment-v4"))
					   && !test->isEnabled(),
				   "the suggested-comment format shows its shipped text and version");
			text->setPlainText("Short comments");
			panel->findChild<QPushButton*>("writingHookSave")->click();
			expect(settings.Overrides[index] == "Short comments" && settings.Overrides[0] == "precise", "its override is its own");
			panel->findChild<QPushButton*>("writingHookReset")->click();
			expect(settings.Overrides[index].empty() && text->toPlainText().toUtf8().toStdString() == application::default_writing_hook(core::writing_hook::suggested_comment), "Reset to built-in restores it");
		}
		// For a look at the row as drawn, when BPR_SETTINGS_RENDER_DIR is set.
		if (const auto directory = qEnvironmentVariable("BPR_SETTINGS_RENDER_DIR"); !directory.isEmpty()) {
			window.settings()->present(qt::settings_page::agents_and_context);
			window.settings()->resize(900, 820);
			panel->ensurePolished();
			QApplication::processEvents();
			expect(panel->grab().save(directory + QStringLiteral("/writing-hooks-feedback-off.png")), "the feedback row render is saved");
			panel->present_label_helper(application::review_label_helper::on);
			QApplication::processEvents();
			expect(panel->grab().save(directory + QStringLiteral("/writing-hooks-feedback-on.png")), "the feedback row render is saved");
			panel->select_hook(core::writing_hook::outline);
			QApplication::processEvents();
			expect(panel->grab().save(directory + QStringLiteral("/writing-hooks-outline.png")), "the outline row render is saved");
			// [[BPR-2059]] The suggested-comment format, as shipped.
			panel->select_hook(core::writing_hook::suggested_comment);
			QApplication::processEvents();
			expect(panel->grab().save(directory + QStringLiteral("/writing-hooks-suggested-comment.png")), "the suggested-comment row render is saved");
		}
	}
	{
		// [[BPR-1917]] AC1, AC2. A changed prompt is marked in the list and
		// beside its title; unsaved edits survive moving to another prompt;
		// Reset to built-in restores the default and Undo reset brings the
		// text back unsaved; the preview shows what the run receives, with a
		// skill it names brought in ([[BPR-1916]] AC3: completion lists each
		// skill with its source).
		qt::workbench_window        window;
		auto*                       panel = window.writing_hooks();
		auto*                       list  = panel->findChild<QListWidget*>("writingHookChoice");
		auto*                       text  = panel->findChild<QPlainTextEdit*>("writingHookInstructions");
		auto*                       badge = panel->findChild<QLabel*>("writingHookBadge");
		auto*                       state = panel->findChild<QLabel*>("writingHookSaveState");
		auto*                       save  = panel->findChild<QPushButton*>("writingHookSave");
		auto*                       reset = panel->findChild<QPushButton*>("writingHookReset");
		auto*                       undo  = panel->findChild<QPushButton*>("writingHookUndo");
		core::writing_hook_settings settings;
		panel->set_dispatch([&](core::writing_hook aSelected, std::string aInstructions) { settings.Overrides[static_cast<std::size_t>(aSelected)] = std::move(aInstructions); ++settings.Revision; panel->present(settings); return true; }, [](core::writing_hook) { });
		const auto row_marker = [list](const int aRow) { return list->item(aRow)->data(Qt::UserRole + 3).toString(); };
		expect(badge->text() == "Built-in" && !save->isEnabled() && !reset->isEnabled() && row_marker(1).isEmpty(), "a built-in prompt says so and has nothing to save or reset");
		const auto builtIn = text->toPlainText();
		text->setPlainText(builtIn + "\nUse /fixture-technical-writer.");
		// [[BPR-2179]]: in the Prompts library a system prompt saves as it is
		// typed; until it does, it says so.
		expect(save->isEnabled() && row_marker(1) == "Unsaved" && (state->text().startsWith("Unsaved") || state->text() == QStringLiteral("Saving\u2026")), "an edit is marked unsaved, in the list too");
		panel->select_hook(core::writing_hook::comment);
		panel->select_hook(core::writing_hook::outline);
		expect(text->toPlainText().endsWith("Use /fixture-technical-writer.") && panel->unsaved_count() == 1U, "moving to another prompt keeps the unsaved edit");
		save->click();
		expect(badge->text() == "Changed from built-in" && row_marker(1) == "Changed" && !save->isEnabled() && reset->isEnabled() && panel->unsaved_count() == 0U, "a saved change is marked changed");
		auto library = application::make_skill_library(
			{ *application::skill_document_of("---\nname: fixture-technical-writer\ndescription: Short paragraphs.\n---\nFIXTURE_WRITER_GUIDANCE\n", "fixture-technical-writer", core::skill_source::personal, "~/.claude/skills", { }) },
			{ *application::skill_document_of("---\nname: fixture-technical-writer\ndescription: The team's.\n---\nTEAM\n", "fixture-technical-writer", core::skill_source::repository, ".claude/skills at base 1a2b3c4", { }) });
		panel->present_skills(library);
		panel->show_preview(true);
		expect(panel->preview_text().contains("Use /fixture-technical-writer.") && panel->preview_text().contains("--- Skill /fixture-technical-writer (personal, ~/.claude/skills")
				   && panel->preview_text().contains("FIXTURE_WRITER_GUIDANCE"),
			   "the preview shows the prompt and the skill it names, brought in");
		expect(panel->findChild<QLabel*>("writingHookSkills")->text().startsWith("Brings in /fixture-technical-writer (personal, ~/.claude/skills)"), "and says which skill, from where");
		const auto directory = qEnvironmentVariable("BPR_SETTINGS_RENDER_DIR");
		const auto render    = [&](const QString& aName) {
			if (!directory.isEmpty()) {
				// [[BPR-2179]]: the system prompts are in Prompts' library.
				window.settings()->present(qt::settings_page::prompts);
				window.prompts()->select_system(static_cast<int>(core::writing_hook::outline));
				window.settings()->resize(1100, 900);
				panel->resize(900, 640);
				QApplication::processEvents();
				expect(panel->grab().save(directory + QStringLiteral("/") + aName), "the prompt editor render is saved");
			}
		};
		render(QStringLiteral("prompt-editor-preview.png"));
		panel->show_preview(false);
		render(QStringLiteral("prompt-editor-changed.png"));
		// Completion while typing, with each skill's source.
		window.settings()->present(qt::settings_page::prompts);
		window.prompts()->select_system(static_cast<int>(core::writing_hook::outline));
		window.settings()->show();
		text->setFocus();
		QApplication::processEvents();
		auto cursor = text->textCursor();
		cursor.movePosition(QTextCursor::End);
		text->setTextCursor(cursor);
		text->insertPlainText(" Also /fix");
		QApplication::processEvents();
		auto* const completion = panel->skill_completion();
		expect(completion != nullptr, "the prompt field completes skills");
		{
			const auto rows = completion->rows();
			expect(rows.size() == 2 && rows[0] == "/fixture-technical-writer (Personal, ~/.claude/skills)" && rows[1] == "/repo:fixture-technical-writer (Repository, .claude/skills at base 1a2b3c4)",
				   "completion lists the personal skill first and the repository's, with their sources");
			if (!directory.isEmpty()) {
				expect(completion->popup()->grab().save(directory + QStringLiteral("/skill-completion.png")), "the completion render is saved");
			}
			completion->accept_current();
			expect(text->toPlainText().endsWith("Also /fixture-technical-writer "), "choosing a row inserts the name");
		}
		window.settings()->hide();
		reset->click();
		expect(settings.Overrides[0].empty() && badge->text() == "Built-in" && text->toPlainText() == builtIn && !undo->isHidden(), "Reset to built-in restores the default");
		undo->click();
		expect(text->toPlainText().contains("Use /fixture-technical-writer.") && panel->unsaved_count() == 1U && settings.Overrides[0].empty(), "Undo reset brings the text back, unsaved");
		// Owner look v001: the review skill reads like every other prompt. Its
		// title, purpose, use and state come from the prompts pane, its own
		// heading goes, and its editor and Reset are the others'.
		panel->select_review_skill();
		auto* const skillTitle = panel->findChild<QLabel*>("reviewSkillTitle");
		auto* const skillBadge = panel->findChild<QLabel*>("reviewSkillBadge");
		auto* const skill      = window.review_skill();
		expect(skillTitle != nullptr && skillTitle->isVisibleTo(panel) && skillTitle->font() == panel->findChild<QLabel*>("writingHookTitle")->font(), "the review skill's title is the prompts' title");
		expect(panel->findChild<QLabel*>("reviewSkillUsedBy")->isVisibleTo(panel) && skillBadge->text() == "Built-in", "it says where it is used and that it is built in");
		expect(skill->editor()->font() == text->font() && skill->findChild<QPushButton*>("reviewSkillRestore")->text() == "Reset to built-in", "its editor and Reset are the others'");
		const auto headings = skill->findChildren<QLabel*>();
		expect(std::ranges::none_of(headings, [skill](const QLabel* aLabel) { return aLabel->text() == "Review skill" && aLabel->isVisibleTo(skill); }), "its own heading is not shown twice");
		render(QStringLiteral("prompt-editor-review-skill.png"));
		panel->present_review_skill_changed(true);
		// [[BPR-2264]] The review skill's row ends the Agent tab's group, above Features.
		const auto skillRows = list->findItems(QStringLiteral("Review skill"), Qt::MatchExactly);
		expect(skillBadge->text() == "Changed from built-in" && skillRows.size() == 1 && skillRows.front()->data(Qt::UserRole + 3).toString() == "Changed"
				   && list->item(list->row(skillRows.front()) + 1)->text() == "Features",
			   "a changed review skill is marked as the others are");
	}

	test_adopting_a_panel_replaces_the_notice();
	test_the_workbench_mounts_settings_with_the_preferences_shortcut();
	test_settings_shortcut_selects_the_active_workbench();
	test_settings_search_finds_and_goes_to_a_setting();
	test_settings_search_reads_the_pages_as_shown();
	test_settings_are_organised_by_task();
	test_agents_page_is_the_agent_list();
	std::cout << "settings-window-tests:pass\n";
	return EXIT_SUCCESS;
}
