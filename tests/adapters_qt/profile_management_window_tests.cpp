#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
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
		expect(menu->actions().size() == 4, "Profiles includes existing profiles, New Window and management");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_management_keeps_identity_and_rejected_candidates();
	test_keyboard_accessibility_and_compact_layout();
	test_native_profiles_menu_keeps_action_identity();
	test_profile_owned_settings_mount_and_session_lifetime();
	std::cout << "profile-management-window-tests:pass\n";
	return EXIT_SUCCESS;
}
