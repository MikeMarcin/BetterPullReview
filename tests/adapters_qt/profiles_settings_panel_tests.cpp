#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/profiles_settings_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <memory>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>
#include <variant>
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

	auto projection() -> application::profile_management_projection
	{
		return { .Profiles = {
					 { core::default_bpr_profile_id, "Default", application::profile_management_runtime::open },
					 { core::bpr_profile_id{ 2U }, "Work & Research", application::profile_management_runtime::closed } },
				 .Ready = true };
	}

	// [[BPR-0778]] RFC-BPR-0033 B4: the sheet is rows, edited in place, and it keeps a
	// refused candidate exactly as the retired form did.
	void test_the_sheet_is_rows_edited_in_place()
	{
		qt::settings_window shell;
		auto* const         panel = new qt::profiles_settings_panel;
		shell.adopt_panel(qt::settings_page::profiles, panel);
		std::vector<application::profile_management_intent> intents;
		panel->set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); });
		auto accepted = projection();
		panel->set_window_counts({ { core::default_bpr_profile_id, 2 } });
		panel->set_current_profile(core::default_bpr_profile_id);
		panel->adopt(accepted);
		// Shown, because what the sheet presents is the claim: a child of a hidden
		// window is never visible, so an in-place editor could not be judged at all.
		shell.present(qt::settings_page::profiles);
		panel->focus_profiles();
		QApplication::processEvents();

		// The retired controls are gone: no permanent name field, no permanent status
		// sentence, no Reload Profiles button.
		expect(!panel->name_editor()->isVisible() && !panel->editing(), "the name field does not exist outside an edit");
		expect(panel->status_text().isEmpty() && !panel->try_again_button()->isVisible(),
			   "no status sentence and no retry are shown before a failure");
		expect(panel->findChild<QPushButton*>(QStringLiteral("reloadProfiles")) == nullptr
				   && panel->findChild<QLabel*>(QStringLiteral("profileRecoveryNotice")) == nullptr,
			   "the Reload Profiles button and the recovery notice are absent");

		// One row per profile: the name, and the state at the right, which is also the
		// row's accessible description. The current profile is selected on open.
		expect(panel->profiles()->count() == 2, "one row per profile");
		expect(panel->profiles()->currentItem() == panel->profiles()->item(0), "the current profile's row is selected on open");
		expect(panel->profiles()->item(0)->data(Qt::AccessibleDescriptionRole).toString() == QStringLiteral("Open in 2 windows")
				   && panel->profiles()->item(1)->data(Qt::AccessibleDescriptionRole).toString() == QStringLiteral("Closed"),
			   "each row states its profile's runtime, counted by the composition");
		expect(panel->open_button()->text() == QStringLiteral("New window"), "the filled action says New window for an open profile");
		auto* work = panel->profiles()->item(1);
		panel->profiles()->setCurrentRow(1);
		expect(panel->open_button()->text() == QStringLiteral("Open"), "and Open for a closed one");

		// Rename edits the selected row in place.
		panel->rename_button()->click();
		expect(panel->editing() && panel->name_editor()->isVisible()
				   && panel->name_editor()->text() == QStringLiteral("Work & Research"),
			   "Rename turns the selected row's name into an editor holding its literal label");
		expect(panel->save_button()->isVisible() && panel->cancel_button()->isVisible()
				   && !panel->rename_button()->isVisible() && !panel->open_button()->isVisible(),
			   "Save and Cancel replace the footer's buttons");
		panel->name_editor()->setText(QStringLiteral("Renamed / Work"));
		panel->save_button()->click();
		auto* rename = std::get_if<application::rename_bpr_profile_intent>(&intents.back());
		expect(rename != nullptr && rename->Profile == core::bpr_profile_id{ 2U } && rename->Label == "Renamed / Work",
			   "rename sends the selected ID and literal candidate");

		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::rename,
			.Profile   = core::bpr_profile_id{ 2U },
			.Reason    = application::profile_management_failure::registry_unavailable
		};
		panel->adopt(accepted);
		expect(panel->name_editor()->text() == QStringLiteral("Renamed / Work") && panel->profiles()->currentItem() == work,
			   "refusal preserves both candidate and selected row identity");
		expect(panel->status_text().contains(QStringLiteral("kept")) && panel->try_again_button()->isVisible(),
			   "the failure is one notice with Try again");
		panel->try_again_button()->click();
		expect(std::holds_alternative<application::reload_bpr_profiles_intent>(intents.back()), "Try again reloads");

		panel->save_button()->click();
		accepted.Failure.reset();
		accepted.Profiles[1].Label = "Renamed / Work";
		accepted.AcceptedEdit      = application::profile_management_edit{ application::profile_management_operation::rename, core::bpr_profile_id{ 2U } };
		panel->adopt(accepted);
		expect(panel->profiles()->item(1) == work && work->text() == QStringLiteral("Renamed / Work"),
			   "the accepted rename updates the original row, which is the confirmation");
		expect(!panel->editing() && !panel->name_editor()->isVisible() && panel->status_text().isEmpty(),
			   "the editor and the notice close on acceptance, and nothing is announced");

		// New profile appends a row already in its editor.
		panel->create_button()->click();
		expect(panel->editing() && panel->profiles()->count() == 3
				   && panel->profiles()->currentRow() == 2
				   && panel->name_editor()->text().isEmpty()
				   && panel->name_editor()->placeholderText() == QStringLiteral("Profile name"),
			   "New profile appends an editing row with its placeholder");
		panel->name_editor()->setText(QStringLiteral("New Profile"));
		panel->save_button()->click();
		expect(std::get<application::create_bpr_profile_intent>(intents.back()).Label == "New Profile", "create sends only its label");
		accepted.AcceptedEdit.reset();
		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::create,
			.Reason    = application::profile_management_failure::invalid_label
		};
		panel->adopt(accepted);
		expect(panel->name_editor()->text() == QStringLiteral("New Profile") && panel->editing(),
			   "create refusal keeps its candidate and its editor");
		accepted.Failure.reset();
		accepted.Profiles.insert(accepted.Profiles.begin(), { core::bpr_profile_id{ 3U }, "Third" });
		panel->adopt(accepted);
		expect(panel->name_editor()->text() == QStringLiteral("New Profile"),
			   "later unrelated publications preserve a refused candidate");
		expect(panel->profiles()->row(work) == 2, "insertion preserves the stable rows");

		// Escape cancels and keeps the name.
		const auto before = intents.size();
		auto       escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(panel->name_editor(), &escape);
		expect(!panel->editing() && !panel->name_editor()->isVisible() && intents.size() == before,
			   "Escape cancels the edit, dispatches nothing and leaves the name as it was");
		expect(panel->profiles()->count() == 3, "the pending row leaves with the cancelled edit");
	}

	void test_keyboard_accessibility_and_compact_layout()
	{
		qt::settings_window shell;
		auto* const         panel = new qt::profiles_settings_panel;
		shell.adopt_panel(qt::settings_page::profiles, panel);
		std::vector<application::profile_management_intent> intents;
		panel->set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); });
		panel->adopt(projection());
		shell.present(qt::settings_page::profiles);
		panel->focus_profiles();
		QApplication::processEvents();
		// [[BPR-2198]]. Profiles is a Settings page, right after General.
		expect(shell.current_page() == qt::settings_page::profiles && shell.sidebar()->item(1)->text() == QStringLiteral("Profiles")
				   && shell.page_widget(qt::settings_page::profiles)->isAncestorOf(panel),
			   "Profiles is the Settings page right after General, and holds the list");
		expect(panel->import_button()->isHidden() || !panel->import_button()->isVisible(), "with no share handlers there is no Share profile settings");
		expect(panel->profiles()->visualItemRect(panel->profiles()->item(0)).height()
				   == qt::profiles_settings_panel::row_height,
			   "rows are the accepted height");
		expect(panel->profiles()->hasFocus(), "present starts keyboard focus in the rows");
		auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QApplication::sendEvent(panel->profiles(), &down);
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(panel->profiles(), &enter);
		expect(intents.size() == 1U && std::get<application::open_bpr_profile_intent>(intents.back()).Profile == core::bpr_profile_id{ 2U },
			   "keyboard navigation and Return open the selected profile ID");
		panel->profiles()->itemDoubleClicked(panel->profiles()->item(1));
		expect(intents.size() == 2U && std::get<application::open_bpr_profile_intent>(intents.back()).Profile == core::bpr_profile_id{ 2U },
			   "double-click opens the same profile through the same intent");
		expect(!panel->create_button()->accessibleName().isEmpty()
				   && !panel->rename_button()->accessibleName().isEmpty()
				   && !panel->open_button()->accessibleName().isEmpty(),
			   "every footer action has an accessible name");
		panel->rename_button()->click();
		QApplication::processEvents();
		expect(panel->name_editor()->accessibleName() == QStringLiteral("Profile name")
				   && !panel->save_button()->accessibleName().isEmpty()
				   && !panel->cancel_button()->accessibleName().isEmpty(),
			   "the in-place editor and its two actions have accessible names");
		expect(panel->rect().contains(panel->save_button()->geometry())
				   && panel->profiles()->height() > 60,
			   "the sheet keeps its rows and actions within layout bounds");
		panel->cancel_button()->click();
		auto escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(&shell, &escape);
		expect(!shell.isVisible() && intents.size() == 2U, "Escape closes Settings without opening a profile");
	}

	void test_profile_owned_settings_mount_and_session_lifetime()
	{
		qt::settings_window shell;
		auto                first       = std::make_unique<qt::workbench_window>();
		auto                second      = std::make_unique<qt::workbench_window>();
		auto*               firstLocal  = first->settings();
		auto                firstPanel  = QPointer<qt::appearance_settings_panel>{ first->appearance_settings() };
		auto                secondPanel = QPointer<qt::appearance_settings_panel>{ second->appearance_settings() };
		first->set_settings_presenter([&first, &shell](const qt::settings_page aPage) {
			first->mount_settings(shell);
			shell.present(aPage);
		});
		first->open_settings(qt::settings_page::accounts_and_hosts);
		expect(first->settings() == &shell && shell.current_page() == qt::settings_page::accounts_and_hosts
				   && shell.isAncestorOf(firstPanel) && !firstLocal->isVisible(),
			   "all Settings gestures mount the invoking live session in one profile shell");
		auto firstCancels  = 0U;
		auto firstPreviews = 0U;
		first->appearance_settings()->set_dispatch([&firstCancels, &firstPreviews](const auto& aIntent) {
			if (std::holds_alternative<application::cancel_appearance_palette_preview_intent>(aIntent)) {
				++firstCancels;
			}
			if (std::holds_alternative<application::preview_appearance_palette_intent>(aIntent)) {
				++firstPreviews;
			}
		});
		// [[BPR-0790]]. A colour is edited through a swatch and the picker seam now,
		// which is what queues the preview candidate this assertion is about.
		first->appearance_settings()->set_colour_picker(
			[](const auto&) { return std::optional{ QColor{ 0x11, 0x22, 0x33 } }; });
		first->appearance_settings()
			->colours()
			->swatch_of(core::appearance_role::diff_added_background)
			->click();
		first->unmount_settings();
		QApplication::processEvents();
		expect(firstCancels == 1U && firstPreviews == 0U && !first->appearance_settings()->preview_palette(),
			   "switching the mounted session cancels even a preview queued before delivery");
		expect(first->settings() == firstLocal && firstLocal->isAncestorOf(firstPanel) && !shell.isAncestorOf(firstPanel), "unmount restores every live panel to its callback owner");
		auto cancels = 0U;
		second->appearance_settings()->set_dispatch([&cancels](const auto& aIntent) {
			if (std::holds_alternative<application::cancel_appearance_palette_preview_intent>(aIntent)) {
				++cancels;
			}
		});
		second->mount_settings(shell);
		first.reset();
		expect(!firstPanel && secondPanel && shell.isAncestorOf(secondPanel), "closing the first session leaves the second session's mounted Settings alive");
		shell.close();
		expect(cancels == 1U, "the shared shell closes through only its currently mounted live panel");
		second.reset();
		expect(!secondPanel && shell.page_widget(qt::settings_page::appearance) != nullptr,
			   "session destruction unmounts before destroying callbacks and leaves the independent Settings shell intact");
		shell.close();
	}

	void test_native_profiles_menu_keeps_action_identity()
	{
		qt::workbench_window                                window;
		std::vector<application::profile_management_intent> intents;
		auto                                                managed = 0U;
		auto                                                quit    = 0U;
		window.set_profile_management_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); }, [&managed] { ++managed; }, [&quit] { ++quit; });
		auto accepted = projection();
		window.adopt_profile_management(accepted, core::default_bpr_profile_id);
		auto* menu = window.profiles_menu();
		expect(window.menu_bar()->actions().contains(menu->menuAction()), "Profiles is reachable from the native menu bar");
		auto* work = menu->actions()[1];
		expect(menu->actions()[0]->isChecked() && !work->isChecked() && work->text() == QStringLiteral("Work && Research"), "menu escapes literal ampersands and checks only the current profile");
		work->trigger();
		expect(std::get<application::open_bpr_profile_intent>(intents.back()).Profile == core::bpr_profile_id{ 2U }
				   && !work->isChecked(),
			   "profile activation preserves current-window check authority");
		accepted.Profiles[1].Label = "Renamed";
		window.adopt_profile_management(accepted, core::default_bpr_profile_id);
		expect(menu->actions()[1] == work && work->text() == QStringLiteral("Renamed"), "menu rename preserves the QAction identity");
		window.findChild<QAction*>(QStringLiteral("manageProfilesAction"))->trigger();
		window.findChild<QAction*>(QStringLiteral("quitProfilesAction"))->trigger();
		expect(managed == 1U && quit == 1U, "management and Quit invoke their desktop owner callbacks");
		// [[BPR-1863]]. New Window is File's first item, on the catalogue's key,
		// and no longer in Profiles.
		auto* newWindow = window.findChild<QAction*>(QStringLiteral("newBprWindowAction"));
		auto* fileMenu  = newWindow == nullptr ? nullptr : qobject_cast<QMenu*>(newWindow->parent());
		expect(newWindow != nullptr && newWindow->isEnabled() && fileMenu != nullptr
				   && window.menu_bar()->actions().front() == fileMenu->menuAction() && fileMenu->actions().front() == newWindow
				   && !menu->actions().contains(newWindow),
			   "New Window is the first item of the window's File menu and not in Profiles");
		expect(newWindow->shortcuts() == qt::shortcut_keys(qt::shortcut_command::new_window)
				   && newWindow->shortcut() == QKeySequence{ QKeySequence::New },
			   "and carries the platform's New key, as the windowless bar's does");
		newWindow->trigger();
		expect(std::get<application::new_bpr_window_intent>(intents.back()).Profile == core::default_bpr_profile_id, "New Window dispatches this window's current profile identity");
		auto visible = 0;
		for (const auto* action : menu->actions()) {
			visible += action->isVisible() ? 1 : 0;
		}
		expect(visible == 3 && menu->actions()[2] == window.findChild<QAction*>(QStringLiteral("manageProfilesAction")),
			   "Profiles shows the existing profiles and management, and nothing else");

		// [[BPR-0777]] RFC-BPR-0033 B3: a refusal while this window is live is said
		// here and in its status line, which is what lets the desktop keep the
		// chooser away.
		auto* const status = window.findChild<QAction*>(QStringLiteral("profileManagementStatusAction"));
		expect(status != nullptr && !status->isVisible(), "no refusal is said while none has happened");
		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::rename,
			.Profile   = core::bpr_profile_id{ 2U },
			.Reason    = application::profile_management_failure::registry_unavailable
		};
		window.adopt_profile_management(accepted, core::default_bpr_profile_id);
		expect(status->isVisible() && !status->isEnabled()
				   && status->text() == qt::profile_management_failure_text(application::profile_management_failure::registry_unavailable),
			   "the refusal is one non-actionable item in the window's own Profiles menu");
		expect(window.status_text().contains(QStringLiteral("Profiles could not be loaded or saved")),
			   "and the window's status line carries the same sentence");
		accepted.Failure.reset();
		window.adopt_profile_management(accepted, core::default_bpr_profile_id);
		expect(!status->isVisible() && !window.status_text().contains(QStringLiteral("could not be loaded or saved")),
			   "a later accepted projection clears it from both");
	}

	// [[BPR-0777]] RFC-BPR-0033 B1 and B2: with no window the application keeps its
	// own menu bar, and its gestures are the same typed intents a window's menu sends.
	void test_the_desktop_menu_bar_reopens_the_most_recent_profile()
	{
		qt::desktop_menu_bar                                menu;
		std::vector<application::profile_management_intent> intents;
		auto                                                managed = 0U;
		auto                                                quit    = 0U;
		menu.set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); }, [&managed] { ++managed; }, [&quit] { ++quit; });
		auto accepted                = projection();
		accepted.Profiles[0].Runtime = application::profile_management_runtime::closed;
		menu.adopt(accepted, core::bpr_profile_id{ 2U });
		expect(menu.parentWidget() == nullptr, "the desktop's menu bar belongs to the application, not to a window");
		auto* const profiles = menu.profiles_menu();
		expect(profiles->actions().size() == 3 && profiles->actions()[2] == menu.manage_profiles_action(),
			   "Profiles lists the profiles and management");
		auto* const fileMenu = qobject_cast<QMenu*>(menu.new_window_action()->parent());
		expect(fileMenu != nullptr && menu.actions().front() == fileMenu->menuAction() && fileMenu->actions().front() == menu.new_window_action()
				   && !profiles->actions().contains(menu.new_window_action()),
			   "[[BPR-1863]] New Window is the first item of File, as in a window's bar");
		expect(profiles->actions()[1]->isChecked() && !profiles->actions()[0]->isChecked(),
			   "the most recently used profile is the checked one");
		expect(menu.new_window_action()->isEnabled()
				   && menu.new_window_action()->shortcut() == QKeySequence{ QKeySequence::New },
			   "New Window is offered with no runtime open at all, on the platform's own shortcut");
		menu.new_window_action()->trigger();
		expect(std::get<application::new_bpr_window_intent>(intents.back()).Profile == core::bpr_profile_id{ 2U },
			   "it reopens the most recently used profile through the existing intent");
		profiles->actions()[0]->trigger();
		expect(std::get<application::open_bpr_profile_intent>(intents.back()).Profile == core::default_bpr_profile_id,
			   "choosing a profile opens exactly that profile");
		menu.manage_profiles_action()->trigger();
		menu.quit_action()->trigger();
		expect(managed == 1U && quit == 1U && intents.size() == 2U,
			   "management and Quit reach their desktop owner without inventing a profile intent");
		auto* const renamed        = profiles->actions()[1];
		accepted.Profiles[1].Label = "Renamed & Co";
		menu.adopt(accepted, core::bpr_profile_id{ 2U });
		expect(profiles->actions()[1] == renamed && renamed->text() == QStringLiteral("Renamed && Co"),
			   "a rename keeps the action identity and escapes a literal ampersand");
		auto closing                = accepted;
		closing.Profiles[1].Runtime = application::profile_management_runtime::closing;
		menu.adopt(closing, core::bpr_profile_id{ 2U });
		expect(!profiles->actions()[1]->isEnabled(), "a closing profile is not offered");
		// [[BPR-0796]] RFC-BPR-0033 B1. The desktop's bar carries the same menus a
		// window's bar carries, so closing the last window takes no menu off the
		// macOS menu bar. The window's own presentation commands are offered and
		// disabled, and Settings keeps the role macOS merges into the application
		// menu, which is what keeps Settings… there with no window open.
		auto* const view = menu.view_menu();
		expect(view != nullptr && view->title() == QStringLiteral("&View"), "the desktop bar offers View");
		const auto viewActions = view->actions();
		// [[BPR-1304]]. Read from a window's own View menu rather than restated:
		// [[BPR-1295]] added Hide Drawers and [[BPR-1293]] Threads to the
		// window's menu, and a list kept here could not see the second missing
		// from the desktop's bar.
		const auto described = [](const QList<QAction*>& aActions) {
			auto lines = QStringList{ };
			for (const auto* const action : aActions) {
				lines.push_back(action->isSeparator()
									? QStringLiteral("-")
									: action->text() + (action->isCheckable() ? QStringLiteral(" [checkable]") : QString{ }));
			}
			return lines;
		};
		{
			qt::workbench_window window;
			expect(window.view_menu() != nullptr && described(viewActions) == described(window.view_menu()->actions()),
				   "with the window menu's own commands, in the window menu's own order");
		}
		// [[BPR-1493]]. RFC-BPR-0077's Find submenu, RFC-BPR-0076's file and
		// change navigation and RFC-BPR-0074's Go to header/source lead the
		// window's View, so they lead this one, each offered and disabled.
		// [[BPR-1581]] [[BPR-1586]]: Show Thread in Discuss, in Review and in
		// Threads follow Threads, so twenty-eight; [[BPR-2041]] adds Go to line.
		expect(viewActions.size() == 29 && viewActions[0]->text() == QStringLiteral("&Find") && viewActions[0]->menu() != nullptr
				   && viewActions[0]->menu()->actions().size() == 6
				   && viewActions[1]->text() == QStringLiteral("&Previous file")
				   && viewActions[1]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Alt+Up") }
				   && viewActions[2]->text() == QStringLiteral("&Next file")
				   && viewActions[2]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Alt+Down") }
				   && viewActions[3]->text() == QStringLiteral("Pre&vious change")
				   && viewActions[3]->shortcut() == QKeySequence{ QStringLiteral("Alt+Up") }
				   && viewActions[4]->text() == QStringLiteral("Ne&xt change")
				   && viewActions[4]->shortcut() == QKeySequence{ QStringLiteral("Alt+Down") }
				   // [[BPR-1532]]. First and Last change, after Next change.
				   && viewActions[5]->text() == QStringLiteral("Fi&rst change")
				   && viewActions[5]->shortcut() == QKeySequence{ QStringLiteral("Alt+Shift+Up") }
				   && viewActions[6]->text() == QStringLiteral("Last c&hange")
				   && viewActions[6]->shortcut() == QKeySequence{ QStringLiteral("Alt+Shift+Down") } && viewActions[7]->isSeparator()
				   && viewActions[8]->text() == QStringLiteral("Go to header/source")
				   && viewActions[8]->shortcut() == QKeySequence{ QStringLiteral("Alt+O") }
				   && viewActions[9]->text() == QStringLiteral("Go to line…") && viewActions[10]->isSeparator(),
			   "Find, file and change navigation, First and Last change and Go to header/source lead it, on the window's keys");
		{
			qt::workbench_window window;
			const auto           titles = [](const QMenu* const aMenu) {
				auto names = QStringList{ };
				for (const auto* const action : aMenu->actions()) {
					names.push_back(action->text());
				}
				return names;
			};
			auto* const goToLine = window.findChild<QAction*>(QStringLiteral("goToLineAction"));
			expect(goToLine && !viewActions[9]->shortcuts().empty()
					   && viewActions[9]->shortcuts() == goToLine->shortcuts() && !viewActions[9]->isEnabled(),
				   "Go to line keeps the real window's keys and stays unavailable without its source");
			expect(titles(viewActions[0]->menu()) == titles(window.view_menu()->actions()[0]->menu())
					   && std::ranges::none_of(viewActions[0]->menu()->actions(), [](const QAction* const aAction) { return aAction->isEnabled(); }),
				   "and Find holds the window's Find commands in the window's order, each disabled");
		}
		expect(viewActions[11]->text() == QStringLiteral("&Line numbers")
				   && viewActions[12]->text() == QStringLiteral("&One pane")
				   && viewActions[13]->text() == QStringLiteral("Line &Details") && viewActions[13]->isCheckable()
				   && viewActions[14]->text() == QStringLiteral("Hide &Drawers")
				   && viewActions[14]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+B") }
				   && viewActions[15]->text() == QStringLiteral("&Threads")
				   && viewActions[15]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+J") }
				   // [[BPR-1581]] [[BPR-1586]]. The three thread moves follow Threads.
				   && viewActions[16]->text() == QStringLiteral("Show Thread in Discuss")
				   && viewActions[16]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Alt+3") }
				   && viewActions[17]->text() == QStringLiteral("Show Thread in Review")
				   && viewActions[17]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Alt+2") }
				   && viewActions[18]->text() == QStringLiteral("Show Thread in Threads")
				   && viewActions[18]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Alt+4") }
				   && viewActions[19]->text() == QStringLiteral("&Updates Line") && viewActions[19]->isCheckable()
				   && viewActions[20]->isSeparator() && viewActions[24]->isSeparator()
				   && viewActions[25]->text() == QStringLiteral("Next Tab")
				   && viewActions[26]->text() == QStringLiteral("Previous Tab") && viewActions[27]->isSeparator()
				   && viewActions[28]->text() == QStringLiteral("&Settings…"),
			   "Line Details, Hide Drawers on its key and Threads on its key among them");
		// [[BPR-1336]]. The zoom commands, on the window's keys, offered and off.
		expect(viewActions[21]->text() == QStringLiteral("Zoom In")
				   && viewActions[21]->shortcuts() == QList{ QKeySequence{ QStringLiteral("Ctrl+=") }, QKeySequence{ QStringLiteral("Ctrl++") } }
				   && viewActions[22]->text() == QStringLiteral("Zoom Out") && viewActions[22]->shortcuts() == QList{ QKeySequence{ QStringLiteral("Ctrl+-") } }
				   && viewActions[23]->text() == QStringLiteral("Actual Size") && viewActions[23]->shortcuts() == QList{ QKeySequence{ QStringLiteral("Ctrl+0") } }
				   && !viewActions[21]->isEnabled() && !viewActions[22]->isEnabled() && !viewActions[23]->isEnabled(),
			   "Zoom In, Zoom Out and Actual Size are offered with no window, on the window's keys, and disabled");
		{
			auto keys = QList<QKeySequence>{ };
			for (const auto* const action : menu.findChildren<QAction*>()) {
				for (const auto& key : action->shortcuts()) {
					expect(!keys.contains(key), "no key is bound twice on the desktop bar: " + key.toString().toStdString());
					keys.push_back(key);
				}
			}
		}
		expect(std::ranges::none_of(viewActions, [](const QAction* const aAction) { return !aAction->isSeparator() && aAction->isEnabled(); }),
			   "none of them actionable, because no window's presentation is theirs to change");
		expect(viewActions[28]->menuRole() == QAction::PreferencesRole,
			   "and Settings keeps the preferences role, so the application menu keeps its merged item");

		// [[BPR-1035]]. And the window's Pull Request menu, between Profiles and
		// View as the window has it, with its gestures offered and disabled.
		auto* const pullRequest = menu.pull_request_menu();
		expect(pullRequest != nullptr && pullRequest->title() == QStringLiteral("P&ull Request"), "the desktop bar offers Pull Request");
		const auto titles = [&menu] {
			auto names = QStringList{ };
			for (auto* const action : menu.actions()) {
				names.push_back(action->text());
			}
			return names;
		}();
		expect(titles == QStringList{ QStringLiteral("&File"), QStringLiteral("&Profiles"), QStringLiteral("P&ull Request"), QStringLiteral("&View"), QStringLiteral("&Help") },
			   "in the window bar's order: File, Profiles, Pull Request, View, Help");
		// [[BPR-1250]]. And the window's Help menu, its diagnostics offered and
		// disabled, under the name the update menu attaches to.
		auto* const help = menu.help_menu();
		expect(help != nullptr && help->title() == QStringLiteral("&Help") && help->objectName() == QStringLiteral("releaseUpdateMenu")
				   && help->accessibleName() == QStringLiteral("Help"),
			   "the desktop bar offers Help where Check for Updates… attaches");
		const auto helpActions = help->actions();
		// Keyboard Shortcuts first, followed by the approved Insights command
		// and diagnostics, as in the window's Help.
		expect(helpActions.size() == 7 && helpActions[0]->text() == QStringLiteral("&Keyboard Shortcuts") && helpActions[1]->isSeparator()
				   && helpActions[2]->text() == QStringLiteral("Insights…")
				   && helpActions[3]->text() == QStringLiteral("Diagnostics…") && helpActions[4]->isSeparator()
				   && helpActions[5]->text() == QStringLiteral("Export anonymized diagnostics…")
				   && helpActions[6]->text() == QStringLiteral("Export with identifying details…"),
			   "with the window's Keyboard Shortcuts, Insights and diagnostic commands in exact order");
		expect(std::ranges::none_of(helpActions, [](const QAction* const aAction) { return !aAction->isSeparator() && aAction->isEnabled(); }),
			   "none of Help's commands is actionable with no window open");
		const auto pullRequestActions = pullRequest->actions();
		// [[BPR-1338]]. Read from a window's own Pull Request menu, with each
		// command's key, rather than restated: BPR-1338 put Open on GitHub on
		// the window's menu and not on this bar, and the list kept here could
		// not see it missing. A window's Comment names why it is unavailable in
		// its own title ([[BPR-1250]]); that reason is the window's state, so the
		// command is compared without it.
		const auto withKeys = [&described](const QList<QAction*>& aActions) {
			auto lines = described(aActions);
			for (auto index = qsizetype{ 0 }; index < aActions.size(); ++index) {
				if (const auto reason = lines[index].indexOf(QStringLiteral(" unavailable — ")); reason >= 0) {
					lines[index].truncate(reason);
				}
				lines[index] += QLatin1Char{ ' ' } + aActions[index]->shortcut().toString(QKeySequence::PortableText);
			}
			return lines;
		};
		{
			qt::workbench_window window;
			const auto           desktopLines = withKeys(pullRequestActions);
			const auto           windowLines  = window.pull_request_menu() != nullptr ? withKeys(window.pull_request_menu()->actions()) : QStringList{ };
			if (desktopLines != windowLines) {
				std::cerr << "desktop Pull Request: " << desktopLines.join(QStringLiteral(" | ")).toStdString() << '\n'
						  << "window Pull Request:  " << windowLines.join(QStringLiteral(" | ")).toStdString() << '\n';
			}
			expect(!windowLines.isEmpty() && desktopLines == windowLines,
				   "Pull Request carries the window menu's own commands, on their keys, in the window menu's own order");
			auto* const windowHelp = window.findChild<QMenu*>(QStringLiteral("releaseUpdateMenu"));
			expect(windowHelp != nullptr && withKeys(helpActions) == withKeys(windowHelp->actions()),
				   "Help carries the window menu's own commands, on their keys, in the window menu's own order");
		}
		expect(pullRequest->accessibleDescription().endsWith(QStringLiteral("unavailable while no window is open")),
			   "and the menu says why none of them acts");
		// [[BPR-1852]]. The draft-state command follows Open on GitHub, hidden
		// while nothing is offered.
		expect(pullRequestActions.size() == 14 && pullRequestActions[0]->text() == QStringLiteral("Open on &GitHub")
				   && pullRequestActions[0]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Shift+O") }
				   && pullRequestActions[1]->text() == QStringLiteral("Mark &Ready for Review") && !pullRequestActions[1]->isVisible()
				   && pullRequestActions[2]->isSeparator()
				   && pullRequestActions[3]->text() == QStringLiteral("&Follow")
				   && pullRequestActions[4]->text() == QStringLiteral("Mark New Commits &Seen")
				   && pullRequestActions[5]->text() == QStringLiteral("What &Changed Since My Review")
				   && pullRequestActions[6]->text() == QStringLiteral("&Save Repository")
				   && pullRequestActions[7]->isSeparator()
				   && pullRequestActions[8]->text() == QStringLiteral("&Mark All Comments as Read")
				   && pullRequestActions[9]->isSeparator()
				   && pullRequestActions[10]->text() == QStringLiteral("Compose &review") && pullRequestActions[10]->isCheckable()
				   && pullRequestActions[11]->text() == QStringLiteral("Co&mment")
				   && pullRequestActions[12]->text() == QStringLiteral("Prepare review &tour")
				   && pullRequestActions[13]->text() == QStringLiteral("Add tour stop"),
			   "with Open on GitHub, the draft-state command, Follow, Seen, What changed, Save Repository, Mark All Comments as Read, composition and the review tour");
		expect(std::ranges::none_of(pullRequestActions, [](const QAction* const aAction) { return !aAction->isSeparator() && aAction->isEnabled(); }),
			   "none actionable with no pull request open");

		// [[BPR-1195]]. File survives Quit's merge into the macOS application
		// menu, and the scaffolding's commands keep their discoverable shortcuts.
		auto* const file = menu.actions()[0]->menu();
		expect(file != nullptr && file->accessibleName() == QStringLiteral("File")
				   && view->accessibleName() == QStringLiteral("View")
				   && pullRequest->accessibleName() == QStringLiteral("Pull Request"),
			   "the resident menus retain their accessible names");
		const auto fileActions = file->actions();
		// [[BPR-1863]]. New Window leads, before a separator.
		// [[BPR-2308]] retired Open Project… with Review Cycles.
		expect(fileActions.size() == 4 && fileActions[0] == menu.new_window_action() && fileActions[1]->isSeparator()
				   && fileActions[2] == menu.quit_action()
				   && fileActions[3]->text() == QStringLiteral("Close Tab") && !fileActions[3]->isEnabled(),
			   "File opens with New Window and retains Close Tab after Quit, disabled with no window");
		expect(fileActions[3]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+W") }
				   && pullRequestActions[10]->shortcut() == QKeySequence{ QStringLiteral("Alt+R") }
				   && pullRequestActions[11]->shortcut() == QKeySequence{ QStringLiteral("Alt+C") }
				   && pullRequestActions[8]->shortcut().isEmpty() && viewActions[19]->shortcut().isEmpty(),
			   "Close Tab and composition retain their keys, and Updates Line claims none");
#if defined(Q_OS_MACOS)
		expect(viewActions[25]->shortcut() == QKeySequence{ QStringLiteral("Meta+Tab") }
				   && viewActions[26]->shortcut() == QKeySequence{ QStringLiteral("Meta+Shift+Tab") },
			   "tab navigation keeps the physical Control key on macOS");
#else
		expect(viewActions[25]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Tab") }
				   && viewActions[26]->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Shift+Tab") },
			   "tab navigation keeps the Control key");
#endif
		for (auto* const action : { fileActions[3], viewActions[1], viewActions[2], viewActions[3], viewActions[4], viewActions[5], viewActions[6], viewActions[8], viewActions[9], viewActions[13], viewActions[14], viewActions[15], viewActions[19], viewActions[21], viewActions[22], viewActions[23], viewActions[25], viewActions[26], pullRequestActions[0], pullRequestActions[1], pullRequestActions[4], pullRequestActions[5], pullRequestActions[8], pullRequestActions[10], pullRequestActions[11], pullRequestActions[12], pullRequestActions[13], helpActions[0], helpActions[2], helpActions[3], helpActions[5], helpActions[6] }) {
			action->trigger();
		}
		expect(managed == 1U && quit == 1U && intents.size() == 2U,
			   "the disabled scaffolding commands cause no desktop effect");
	}
	/// [[BPR-2318]] (owner, 2026-11-20: "Ps profiles needs the same drag
	/// handle"). A grip on every row; a drag or Alt+Up and Alt+Down ask the
	/// host to move the profile, and the list shows what the host saves.
	void test_profiles_are_reordered_by_grip_and_keyboard()
	{
		qt::settings_window shell;
		auto* const         panel = new qt::profiles_settings_panel;
		shell.adopt_panel(qt::settings_page::profiles, panel);
		std::vector<application::profile_management_intent> intents;
		panel->set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); });
		auto accepted = projection();
		accepted.Profiles.push_back({ core::bpr_profile_id{ 3U }, "Home", application::profile_management_runtime::closed });
		panel->set_current_profile(core::default_bpr_profile_id);
		panel->adopt(accepted);
		shell.present(qt::settings_page::profiles);
		QApplication::processEvents();
		auto* const list = panel->profiles();
		for (auto row = 0; row < 3; ++row) {
			auto* const grip = panel->profile_grip(row);
			const auto  rect = list->visualItemRect(list->item(row));
			expect(grip != nullptr && grip->isVisible() && grip->geometry().left() == rect.left() && rect.contains(grip->geometry()), "every profile row has a visible grip at its left");
		}
		expect(panel->profile_grip(1)->accessibleName() == QStringLiteral("Reorder Work & Research") && panel->profile_grip(1)->testAttribute(Qt::WA_TransparentForMouseEvents)
				   && list->dragDropMode() == QAbstractItemView::InternalMove && list->dragEnabled(),
			   "the grip is named for its profile and a press on it starts the list's drag");
		const auto moved = [&intents]() -> std::pair<std::uint64_t, int> {
			const auto* const move = intents.empty() ? nullptr : std::get_if<application::move_bpr_profile_intent>(&intents.back());
			return move == nullptr ? std::pair{ std::uint64_t{ 0U }, 0 } : std::pair{ move->Profile.value(), move->Delta };
		};
		list->setCurrentRow(2);
		auto up = QKeyEvent{ QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier };
		QApplication::sendEvent(list, &up);
		expect(moved() == std::pair{ std::uint64_t{ 3U }, -1 } && list->item(2)->text() == QStringLiteral("Home"), "Alt+Up asks to move the selected profile; the row waits for the host");
		list->setCurrentRow(0);
		const auto before = intents.size();
		QApplication::sendEvent(list, &up);
		expect(intents.size() == before, "the first profile goes no higher");
		static_cast<void>(list->model()->moveRow(QModelIndex{ }, 0, QModelIndex{ }, 3));
		expect(moved() == std::pair{ std::uint64_t{ 1U }, 2 }, "a profile dropped two places down asks to move it there");
		accepted.Profiles = { accepted.Profiles[1], accepted.Profiles[2], accepted.Profiles[0] };
		panel->adopt(accepted);
		expect(list->item(0)->text() == QStringLiteral("Work & Research") && list->item(2)->text() == QStringLiteral("Default") && panel->profile_grip(2)->accessibleName() == QStringLiteral("Reorder Default"),
			   "the list shows the host's order, and the grips follow");
		panel->create_button()->click();
		expect(!list->dragEnabled(), "no profile is dragged while one is being named");
		list->setCurrentRow(1);
		const auto naming = intents.size();
		auto       down   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::AltModifier };
		QApplication::sendEvent(list, &down);
		expect(intents.size() == naming, "nor moved from the keyboard");
		panel->cancel_button()->click();
		expect(list->dragEnabled() && list->count() == 3, "Cancel gives the grips back");
	}

	// [[BPR-2198]] (owner, 2026-11-15). Import Profile Settings… and Export
	// Profile Settings… are on Profiles, with the one line under them.
	void test_share_profile_settings()
	{
		qt::settings_window shell;
		auto* const         panel = new qt::profiles_settings_panel;
		shell.adopt_panel(qt::settings_page::profiles, panel);
		auto imports = 0;
		auto exports = 0;
		panel->set_share_handlers([&imports] { ++imports; }, [&exports] { ++exports; });
		panel->adopt(projection());
		shell.present(qt::settings_page::profiles);
		QApplication::processEvents();
		expect(panel->import_button()->isVisible() && panel->export_button()->isVisible(), "the share buttons are shown once the profile can share");
		expect(panel->import_button()->text() == QStringLiteral("Import Profile Settings…") && panel->export_button()->text() == QStringLiteral("Export Profile Settings…"),
			   "the buttons are named for Profile Settings");
		const auto* const line = panel->findChild<QLabel*>(QStringLiteral("profileShareNote"));
		expect(line != nullptr && line->text() == QStringLiteral("A file with this profile's agents, prompts and workflows, to move to another computer or share."),
			   "one line says what the file is");
		expect(panel->import_button()->mapTo(panel, QPoint{ }).y() > panel->profiles()->mapTo(panel, QPoint{ }).y(), "sharing comes after the profiles");
		// [[BPR-2198]]. The coverage of the Agents share row it replaces.
		expect((panel->import_button()->focusPolicy() & Qt::TabFocus) != 0 && (panel->export_button()->focusPolicy() & Qt::TabFocus) != 0, "both are reached by Tab");
		expect(shell.findChild<QPushButton*>(QStringLiteral("settingsImportConfiguration")) == nullptr, "the Agents page has no share row any more");
		panel->import_button()->click();
		panel->export_button()->click();
		expect(imports == 1 && exports == 1, "each button asks its handler once");
		panel->present_exchange(QStringLiteral("Exported 2 agents."));
		expect(panel->exchange_note()->isVisible() && panel->exchange_note()->text() == QStringLiteral("Exported 2 agents."), "what the last exchange did is said under the buttons");
		shell.search_field()->setText(QStringLiteral("Export Profile"));
		QApplication::processEvents();
		expect(shell.search_results()->count() >= 1 && shell.search_results()->item(0)->text() == QStringLiteral("Export Profile Settings…"), "Settings search finds Export Profile Settings…");
		shell.close();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_sheet_is_rows_edited_in_place();
	test_keyboard_accessibility_and_compact_layout();
	test_profiles_are_reordered_by_grip_and_keyboard();
	test_share_profile_settings();
	test_profile_owned_settings_mount_and_session_lifetime();
	test_native_profiles_menu_keeps_action_identity();
	test_the_desktop_menu_bar_reopens_the_most_recent_profile();
	std::cout << "profiles-settings-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
