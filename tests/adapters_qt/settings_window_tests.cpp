/// \file
/// \brief Proves the Settings window places panels and holds no state of its own.
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/resident_filter.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QSysInfo>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

	/// \brief [[BPR-1301]]. Agents & context's Claude rows: the wrapper path,
	/// saved only through the host, and the one line about sign-in. Withdrawing
	/// the agent surfaces, as an agent-free build does, takes both away.
	void test_agents_page_claude_wrapper_rows()
	{
		qt::workbench_window workbench;
		auto* const          page    = workbench.settings()->page_widget(qt::settings_page::agents_and_context);
		auto* const          panel   = workbench.claude_wrapper();
		auto* const          path    = page->findChild<QLineEdit*>(QStringLiteral("claudeWrapperPath"));
		auto* const          save    = page->findChild<QPushButton*>(QStringLiteral("claudeWrapperSave"));
		auto* const          choose  = page->findChild<QPushButton*>(QStringLiteral("claudeWrapperChoose"));
		auto* const          status  = page->findChild<QLabel*>(QStringLiteral("claudeWrapperStatus"));
		auto* const          signIn  = page->findChild<QLabel*>(QStringLiteral("claudeSignIn"));
		auto* const          heading = page->findChild<QLabel*>(QStringLiteral("claudeWrapperHeading"));
		expect(panel != nullptr && path != nullptr && save != nullptr && choose != nullptr && status != nullptr && signIn != nullptr && heading != nullptr,
			   "Agents & context mounts the Claude rows");
		expect(heading->text() == QStringLiteral("Claude") && path->text().isEmpty() && status->text().isEmpty(), "nothing is claimed before the profile publishes");
		expect(signIn->text() == QStringLiteral("Sign-in is left to Claude and the wrapper; BetterPullReview never asks you to sign in to Claude."),
			   "one line says sign-in is Claude's and the wrapper's (owner, 2026-10-06: no switch)");
		expect(page->findChildren<QCheckBox*>().size() == 1, "no sign-in switch is offered; the only check box is the private draft");

		auto sent = std::vector<std::string>{ };
		panel->set_dispatch([&sent](std::string aPath) { sent.push_back(std::move(aPath)); });
		path->setText(QStringLiteral("/Users/me/bin/claude gateway ✓"));
		save->click();
		expect(sent == std::vector<std::string>{ "/Users/me/bin/claude gateway ✓" }, "Save hands the exact typed path to the host, spaces and all");
		expect(status->text() == QStringLiteral("Checking…"), "the row says the path is being checked");
		panel->show_answer({ .Path = sent.back(), .Refusal = core::agent_wrapper_refusal::not_executable });
		expect(status->text() == QStringLiteral("Not saved. You do not have permission to run this file."), "a refusal says why in one line");
		expect(path->text() == QStringLiteral("/Users/me/bin/claude gateway ✓"), "the refused text stays to be corrected");
		QMetaObject::invokeMethod(path, "returnPressed");
		expect(sent.size() == 2U, "Return saves too");
		panel->show_answer({ .Path = sent.back(), .Submitted = true });
		expect(status->text() == QStringLiteral("Saving…"), "an accepted path is being written");
		panel->present("/Users/me/bin/claude gateway ✓");
		expect(status->text() == QStringLiteral("Saved for this profile.") && path->text() == QStringLiteral("/Users/me/bin/claude gateway ✓"), "the published wrapper is shown as saved");
		// For a look at the rows as drawn, when BPR_SETTINGS_RENDER_DIR is set.
		if (const auto directory = qEnvironmentVariable("BPR_SETTINGS_RENDER_DIR"); !directory.isEmpty()) {
			workbench.settings()->present(qt::settings_page::agents_and_context);
			workbench.settings()->resize(900, 820);
			QApplication::processEvents();
			expect(workbench.settings()->grab().save(directory + QStringLiteral("/agents-and-context-claude.png")), "the Agents & context render is saved");
		}
		path->clear();
		save->click();
		expect(sent.back().empty() && status->text() == QStringLiteral("Clearing…"), "an empty field clears the wrapper");
		panel->present({ });
		expect(status->text() == QStringLiteral("No wrapper is saved for this profile.") && path->text().isEmpty(), "a cleared profile says so");
		panel->show_answer({ .Path = "/x" });
		expect(status->text() == QStringLiteral("Not saved. This profile's settings are not available yet."), "an unavailable profile is said plainly");

		// [[BPR-1301]], owner 2026-10-07. With no wrapper the row names the
		// claude a launch would run, looked up whenever the row is shown.
		auto* const found   = page->findChild<QLabel*>(QStringLiteral("claudeWrapperFound"));
		auto        lookups = 0;
		workbench.settings()->hide();
		panel->set_dispatch([&sent](std::string aPath) { sent.push_back(std::move(aPath)); }, [&lookups] { ++lookups; });
		expect(found != nullptr && found->text().isEmpty() && !found->isVisibleTo(page), "nothing is claimed about claude before a lookup answers");
		expect(lookups == 0, "a hidden row looks nothing up");
		workbench.settings()->present(qt::settings_page::agents_and_context);
		QApplication::processEvents();
		expect(lookups == 1, "showing the row with no wrapper looks claude up once");
		panel->show_found(core::claude_search_candidate{ .Path = "/opt/homebrew/bin/claude", .Origin = core::claude_found_in::path });
		expect(found->text() == QStringLiteral("Using /opt/homebrew/bin/claude (found on PATH).") && found->isVisibleTo(page), "the found path is shown, and where it was found");
		panel->show_found(core::claude_search_candidate{ .Path = "/Users/me/.local/bin/claude", .Origin = core::claude_found_in::usual_folder });
		expect(found->text() == QStringLiteral("Using /Users/me/.local/bin/claude (found in a usual install folder, not on PATH)."), "a usual folder is named as such");
		panel->show_found(std::nullopt);
		expect(found->text() == QStringLiteral("claude not found on PATH or in the usual install folders; set a wrapper."), "none found says what to do");
		expect((found->textInteractionFlags() & Qt::TextEditable) == 0, "the found line is read-only");
		panel->present("/opt/tools/claude-gateway");
		expect(!found->isVisibleTo(page) && lookups == 1, "a saved wrapper wins: the found line goes and nothing is looked up");
		panel->show_found(core::claude_search_candidate{ .Path = "/opt/homebrew/bin/claude" });
		expect(!found->isVisibleTo(page), "a late answer does not show over a saved wrapper");
		panel->present({ });
		expect(found->isVisibleTo(page) && lookups == 2, "clearing the wrapper looks claude up again");
		workbench.settings()->hide();
		workbench.settings()->present(qt::settings_page::agents_and_context);
		QApplication::processEvents();
		expect(lookups == 3, "opening Settings again looks again");
		workbench.settings()->hide();

		auto* const settings = workbench.settings();
		settings->search_field()->setText(QStringLiteral("process wrapper"));
		expect(result_names(*settings).contains(QStringLiteral("Process wrapper")), "the wrapper row is found by search");
		workbench.withdraw_agent_surfaces();
		settings->search_field()->setText(QStringLiteral("process wrapper"));
		expect(panel->isHidden() && !result_names(*settings).contains(QStringLiteral("Process wrapper")), "withdrawn agent surfaces take the Claude rows with them");
	}

	/// \brief [[BPR-1289]]. The index is the pages as shown: a withdrawn page is
	/// never searched, and a newly placed setting is found without a second list.
	void test_settings_search_reads_the_pages_as_shown()
	{
		qt::workbench_window workbench;
		auto* const          settings = workbench.settings();
		settings->search_field()->setText(QStringLiteral("private draft"));
		expect(result_names(*settings).contains(QStringLiteral("Private draft")), "a setting on a page is found");
		settings->withdraw_page(qt::settings_page::agents_and_context);
		expect(!result_names(*settings).contains(QStringLiteral("Private draft"))
				   && !result_names(*settings).contains(QStringLiteral("Agents & context")),
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

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_composed_pages_fit_and_render_at_minimum_size();
	test_large_page_scrolls_without_growing_the_window();
	test_close_cancels_preview_without_destroying_accepted_controls();
	test_every_page_exists_with_a_notice();
	{
		qt::workbench_window        window;
		auto*                       panel = window.writing_hooks();
		auto*                       hook  = panel->findChild<QComboBox*>("writingHookChoice");
		auto*                       text  = panel->findChild<QPlainTextEdit*>("writingHookInstructions");
		unsigned                    tests = 0, saves = 0;
		core::writing_hook_settings settings;
		panel->set_dispatch([&](core::writing_hook selected, std::string instructions) { ++saves;settings.Overrides[static_cast<std::size_t>(selected)]=std::move(instructions);++settings.Revision;panel->present(settings);return true; }, [&](core::writing_hook) { ++tests; });
		expect(hook->count() == 4 && !text->toPlainText().isEmpty(), "four shipped hooks preview without inference");
		for (int i = 0; i < 4; ++i) {
			hook->setCurrentIndex(i);
			text->setPlainText("precise");
			panel->findChild<QPushButton*>("writingHookSave")->click();
		}
		expect(tests == 0 && saves == 4, "loading and changing hooks never launches test");
		panel->findChild<QPushButton*>("writingHookReset")->click();
		expect(settings.Overrides[3].empty() && settings.Overrides[0] == "precise", "restore only current hook");
		panel->findChild<QPushButton*>("writingHookTest")->click();
		expect(tests == 1, "explicit test produces exactly one intent");
	}

	test_adopting_a_panel_replaces_the_notice();
	test_the_workbench_mounts_settings_with_the_preferences_shortcut();
	test_settings_shortcut_selects_the_active_workbench();
	test_settings_search_finds_and_goes_to_a_setting();
	test_settings_search_reads_the_pages_as_shown();
	test_agents_page_claude_wrapper_rows();
	std::cout << "settings-window-tests:pass\n";
	return EXIT_SUCCESS;
}
