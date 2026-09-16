#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
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

	void test_management_keeps_identity_and_rejected_candidates()
	{
		qt::profile_management_window                       window;
		std::vector<application::profile_management_intent> intents;
		window.set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); });
		auto accepted = projection();
		window.adopt(accepted);
		auto* work = window.profiles()->item(1);
		window.profiles()->setCurrentRow(1);
		expect(window.name_editor()->text() == QStringLiteral("Work & Research"), "selecting a stable profile shows its literal label");
		window.name_editor()->setText(QStringLiteral("Renamed / Work"));
		window.rename_button()->click();
		auto* rename = std::get_if<application::rename_bpr_profile_intent>(&intents.back());
		expect(rename != nullptr && rename->Profile == core::bpr_profile_id{ 2U } && rename->Label == "Renamed / Work", "rename sends the selected ID and literal candidate");
		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::rename,
			.Profile   = core::bpr_profile_id{ 2U },
			.Reason    = application::profile_management_failure::registry_unavailable
		};
		window.adopt(accepted);
		expect(window.name_editor()->text() == QStringLiteral("Renamed / Work") && window.profiles()->currentItem() == work,
			   "refusal preserves both candidate and selected row identity");
		expect(window.status_text().contains(QStringLiteral("kept")), "refusal is visible and explains retained text");
		window.rename_button()->click();
		accepted.Failure.reset();
		accepted.Profiles[1].Label = "Renamed / Work";
		accepted.AcceptedEdit      = application::profile_management_edit{ application::profile_management_operation::rename, core::bpr_profile_id{ 2U } };
		window.adopt(accepted);
		expect(window.profiles()->item(1) == work && work->text() == QStringLiteral("Renamed / Work"), "accepted rename updates the original row");
		window.name_editor()->setText(QStringLiteral("New Profile"));
		window.create_button()->click();
		expect(std::get<application::create_bpr_profile_intent>(intents.back()).Label == "New Profile", "create sends only its label");
		accepted.AcceptedEdit.reset();
		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::create,
			.Reason    = application::profile_management_failure::invalid_label
		};
		window.adopt(accepted);
		expect(window.name_editor()->text() == QStringLiteral("New Profile"), "create refusal keeps its candidate");
		accepted.Failure.reset();
		accepted.Profiles.insert(accepted.Profiles.begin(), { core::bpr_profile_id{ 3U }, "Third" });
		window.adopt(accepted);
		expect(window.name_editor()->text() == QStringLiteral("New Profile"), "later unrelated publications preserve a refused candidate");
		expect(window.profiles()->currentItem() == work && window.profiles()->row(work) == 2, "insertion preserves the selected stable row");
		expect(intents.size() == 3U, "projection updates emit no edit intents");
	}

	void test_keyboard_accessibility_and_compact_layout()
	{
		qt::profile_management_window                       window;
		std::vector<application::profile_management_intent> intents;
		window.set_dispatch([&intents](auto aIntent) { intents.push_back(std::move(aIntent)); });
		window.adopt(projection());
		window.present();
		window.resize(400, 340);
		QApplication::processEvents();
		expect(window.profiles()->hasFocus(), "present starts keyboard focus in the profile list");
		auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QApplication::sendEvent(window.profiles(), &down);
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(window.profiles(), &enter);
		expect(intents.size() == 1U && std::get<application::open_bpr_profile_intent>(intents.back()).Profile == core::bpr_profile_id{ 2U }, "keyboard navigation and Return open the selected profile ID");
		expect(window.name_editor()->accessibleName() == QStringLiteral("Profile name")
				   && !window.create_button()->accessibleName().isEmpty()
				   && !window.rename_button()->accessibleName().isEmpty()
				   && !window.open_button()->accessibleName().isEmpty(),
			   "every editing control has an accessible name");
		expect(window.rect().contains(window.name_editor()->geometry())
				   && window.rect().contains(window.open_button()->geometry())
				   && window.profiles()->height() > 60,
			   "the compact window keeps its list and actions within layout bounds");
		auto escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(&window, &escape);
		expect(!window.isVisible() && intents.size() == 1U, "Escape hides management without closing a profile");
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
		first->appearance_settings()->color_editor()->setText(QStringLiteral("#112233"));
		first->appearance_settings()->preview_button()->click();
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
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_management_keeps_identity_and_rejected_candidates();
	test_keyboard_accessibility_and_compact_layout();
	test_native_profiles_menu_keeps_action_identity();
	test_the_desktop_menu_bar_reopens_the_most_recent_profile();
	test_profile_owned_settings_mount_and_session_lifetime();
	std::cout << "profile-management-window-tests:pass\n";
	return EXIT_SUCCESS;
}
