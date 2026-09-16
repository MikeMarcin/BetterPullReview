#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/profile_management_window.hpp"
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

#include <cstdlib>
#include <iostream>
#include <string_view>
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
		qt::profile_management_window                       window;
		std::vector<application::profile_management_intent> intents;
		window.set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); });
		auto accepted = projection();
		window.set_window_counts({ { core::default_bpr_profile_id, 2 } });
		window.set_current_profile(core::default_bpr_profile_id);
		window.adopt(accepted);
		// Shown, because what the sheet presents is the claim: a child of a hidden
		// window is never visible, so an in-place editor could not be judged at all.
		window.present();
		QApplication::processEvents();

		// The retired controls are gone: no permanent name field, no permanent status
		// sentence, no Reload Profiles button.
		expect(!window.name_editor()->isVisible() && !window.editing(), "the name field does not exist outside an edit");
		expect(window.status_text().isEmpty() && !window.try_again_button()->isVisible(),
			   "no status sentence and no retry are shown before a failure");
		expect(window.findChild<QPushButton*>(QStringLiteral("reloadProfiles")) == nullptr
				   && window.findChild<QLabel*>(QStringLiteral("profileRecoveryNotice")) == nullptr,
			   "the Reload Profiles button and the recovery notice are absent");

		// One row per profile: the name, and the state at the right, which is also the
		// row's accessible description. The current profile is selected on open.
		expect(window.profiles()->count() == 2, "one row per profile");
		expect(window.profiles()->currentItem() == window.profiles()->item(0), "the current profile's row is selected on open");
		expect(window.profiles()->item(0)->data(Qt::AccessibleDescriptionRole).toString() == QStringLiteral("Open in 2 windows")
				   && window.profiles()->item(1)->data(Qt::AccessibleDescriptionRole).toString() == QStringLiteral("Closed"),
			   "each row states its profile's runtime, counted by the composition");
		expect(window.open_button()->text() == QStringLiteral("New window"), "the filled action says New window for an open profile");
		auto* work = window.profiles()->item(1);
		window.profiles()->setCurrentRow(1);
		expect(window.open_button()->text() == QStringLiteral("Open"), "and Open for a closed one");

		// Rename edits the selected row in place.
		window.rename_button()->click();
		expect(window.editing() && window.name_editor()->isVisible()
				   && window.name_editor()->text() == QStringLiteral("Work & Research"),
			   "Rename turns the selected row's name into an editor holding its literal label");
		expect(window.save_button()->isVisible() && window.cancel_button()->isVisible()
				   && !window.rename_button()->isVisible() && !window.open_button()->isVisible(),
			   "Save and Cancel replace the footer's buttons");
		window.name_editor()->setText(QStringLiteral("Renamed / Work"));
		window.save_button()->click();
		auto* rename = std::get_if<application::rename_bpr_profile_intent>(&intents.back());
		expect(rename != nullptr && rename->Profile == core::bpr_profile_id{ 2U } && rename->Label == "Renamed / Work",
			   "rename sends the selected ID and literal candidate");

		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::rename,
			.Profile   = core::bpr_profile_id{ 2U },
			.Reason    = application::profile_management_failure::registry_unavailable
		};
		window.adopt(accepted);
		expect(window.name_editor()->text() == QStringLiteral("Renamed / Work") && window.profiles()->currentItem() == work,
			   "refusal preserves both candidate and selected row identity");
		expect(window.status_text().contains(QStringLiteral("kept")) && window.try_again_button()->isVisible(),
			   "the failure is one notice with Try again");
		window.try_again_button()->click();
		expect(std::holds_alternative<application::reload_bpr_profiles_intent>(intents.back()), "Try again reloads");

		window.save_button()->click();
		accepted.Failure.reset();
		accepted.Profiles[1].Label = "Renamed / Work";
		accepted.AcceptedEdit      = application::profile_management_edit{ application::profile_management_operation::rename, core::bpr_profile_id{ 2U } };
		window.adopt(accepted);
		expect(window.profiles()->item(1) == work && work->text() == QStringLiteral("Renamed / Work"),
			   "the accepted rename updates the original row, which is the confirmation");
		expect(!window.editing() && !window.name_editor()->isVisible() && window.status_text().isEmpty(),
			   "the editor and the notice close on acceptance, and nothing is announced");

		// New profile appends a row already in its editor.
		window.create_button()->click();
		expect(window.editing() && window.profiles()->count() == 3
				   && window.profiles()->currentRow() == 2
				   && window.name_editor()->text().isEmpty()
				   && window.name_editor()->placeholderText() == QStringLiteral("Profile name"),
			   "New profile appends an editing row with its placeholder");
		window.name_editor()->setText(QStringLiteral("New Profile"));
		window.save_button()->click();
		expect(std::get<application::create_bpr_profile_intent>(intents.back()).Label == "New Profile", "create sends only its label");
		accepted.AcceptedEdit.reset();
		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::create,
			.Reason    = application::profile_management_failure::invalid_label
		};
		window.adopt(accepted);
		expect(window.name_editor()->text() == QStringLiteral("New Profile") && window.editing(),
			   "create refusal keeps its candidate and its editor");
		accepted.Failure.reset();
		accepted.Profiles.insert(accepted.Profiles.begin(), { core::bpr_profile_id{ 3U }, "Third" });
		window.adopt(accepted);
		expect(window.name_editor()->text() == QStringLiteral("New Profile"),
			   "later unrelated publications preserve a refused candidate");
		expect(window.profiles()->row(work) == 2, "insertion preserves the stable rows");

		// Escape cancels and keeps the name.
		const auto before = intents.size();
		auto       escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(window.name_editor(), &escape);
		expect(!window.editing() && !window.name_editor()->isVisible() && intents.size() == before,
			   "Escape cancels the edit, dispatches nothing and leaves the name as it was");
		expect(window.profiles()->count() == 3, "the pending row leaves with the cancelled edit");
	}

	void test_keyboard_accessibility_and_compact_layout()
	{
		qt::profile_management_window                       window;
		std::vector<application::profile_management_intent> intents;
		window.set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); });
		window.adopt(projection());
		window.present();
		QApplication::processEvents();
		expect(window.width() == qt::profile_management_window::sheet_width
				   && window.height() == qt::profile_management_window::sheet_height,
			   "the sheet opens at the accepted size");
		expect(window.profiles()->visualItemRect(window.profiles()->item(0)).height()
				   == qt::profile_management_window::row_height,
			   "rows are the accepted height");
		expect(window.profiles()->hasFocus(), "present starts keyboard focus in the rows");
		auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QApplication::sendEvent(window.profiles(), &down);
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(window.profiles(), &enter);
		expect(intents.size() == 1U && std::get<application::open_bpr_profile_intent>(intents.back()).Profile == core::bpr_profile_id{ 2U },
			   "keyboard navigation and Return open the selected profile ID");
		window.profiles()->itemDoubleClicked(window.profiles()->item(1));
		expect(intents.size() == 2U && std::get<application::open_bpr_profile_intent>(intents.back()).Profile == core::bpr_profile_id{ 2U },
			   "double-click opens the same profile through the same intent");
		expect(!window.create_button()->accessibleName().isEmpty()
				   && !window.rename_button()->accessibleName().isEmpty()
				   && !window.open_button()->accessibleName().isEmpty(),
			   "every footer action has an accessible name");
		window.rename_button()->click();
		QApplication::processEvents();
		expect(window.name_editor()->accessibleName() == QStringLiteral("Profile name")
				   && !window.save_button()->accessibleName().isEmpty()
				   && !window.cancel_button()->accessibleName().isEmpty(),
			   "the in-place editor and its two actions have accessible names");
		expect(window.rect().contains(window.save_button()->geometry())
				   && window.profiles()->height() > 60,
			   "the sheet keeps its rows and actions within layout bounds");
		window.cancel_button()->click();
		auto escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(&window, &escape);
		expect(!window.isVisible() && intents.size() == 2U, "Escape hides the sheet without opening a profile");
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
		auto* newWindow = window.findChild<QAction*>(QStringLiteral("newBprWindowAction"));
		expect(newWindow != nullptr && newWindow->isEnabled(), "New Window is reachable in the real profile menu");
		newWindow->trigger();
		expect(std::get<application::new_bpr_window_intent>(intents.back()).Profile == core::default_bpr_profile_id, "New Window dispatches this window's current profile identity");
		auto visible = 0;
		for (const auto* action : menu->actions()) {
			visible += action->isVisible() ? 1 : 0;
		}
		expect(visible == 4, "Profiles shows the existing profiles, New Window and management, and nothing else");

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
		expect(profiles->actions().size() == 4, "Profiles lists the profiles, New Window and management");
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
		// menu, which is what keeps Preferences… there with no window open.
		auto* const view = menu.view_menu();
		expect(view != nullptr && view->title() == QStringLiteral("&View"), "the desktop bar offers View");
		const auto viewActions = view->actions();
		expect(viewActions.size() == 4 && viewActions[0]->text() == QStringLiteral("&Line numbers")
				   && viewActions[1]->text() == QStringLiteral("&One pane") && viewActions[2]->isSeparator()
				   && viewActions[3]->text() == QStringLiteral("&Settings…"),
			   "with the window menu's own commands, in the window menu's own order");
		expect(!viewActions[0]->isEnabled() && !viewActions[1]->isEnabled() && !viewActions[3]->isEnabled(),
			   "none of them actionable, because no window's presentation is theirs to change");
		expect(viewActions[3]->menuRole() == QAction::PreferencesRole,
			   "and Settings keeps the preferences role, so the application menu keeps its merged item");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_sheet_is_rows_edited_in_place();
	test_keyboard_accessibility_and_compact_layout();
	test_native_profiles_menu_keeps_action_identity();
	test_the_desktop_menu_bar_reopens_the_most_recent_profile();
	test_profile_owned_settings_mount_and_session_lifetime();
	std::cout << "profile-management-window-tests:pass\n";
	return EXIT_SUCCESS;
}
