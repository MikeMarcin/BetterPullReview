#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief Records the semantic intents one window emitted.
	struct intent_log
	{
		std::vector<core::workbench_interaction_intent> Workbench;
		std::vector<core::navigate_diff_change_intent>  Change;
	};

	[[nodiscard]] auto file_command(
		const core::workbench_command_id  aId,
		const bool                        aEnabled,
		const core::workbench_reason_code aReason)
		-> core::workbench_command_projection
	{
		return { .Id = aId, .Enabled = aEnabled, .DisabledReason = aReason };
	}

	[[nodiscard]] auto file_commands(
		const core::workbench_command_projection& aPrevious,
		const core::workbench_command_projection& aNext)
		-> application::workbench_projection
	{
		auto delta        = core::workbench_command_delta{ };
		delta.Commands[0] = aPrevious;
		delta.Commands[1] = aNext;
		delta.Commands[2] = file_command(
			core::workbench_command_id::toggle_files_drawer,
			true,
			core::workbench_reason_code::none);
		delta.Commands[3] = file_command(
			core::workbench_command_id::toggle_conversations_drawer,
			true,
			core::workbench_reason_code::none);
		return { .Commands = delta };
	}

	[[nodiscard]] auto both_files_enabled() -> application::workbench_projection
	{
		return file_commands(
			file_command(core::workbench_command_id::previous_file,
						 true,
						 core::workbench_reason_code::none),
			file_command(core::workbench_command_id::next_file,
						 true,
						 core::workbench_reason_code::none));
	}

	[[nodiscard]] auto both_changes_enabled()
		-> qt::workbench_change_navigation_delta
	{
		return {
			.Previous = { .Enabled = true,
						  .Outcome = core::change_navigation_outcome::selected },
			.Next     = { .Enabled = true,
						  .Outcome = core::change_navigation_outcome::selected },
			.Policy   = core::change_navigation_policy::stop,
		};
	}

	[[nodiscard]] auto navigation_actions(const qt::workbench_window& aWindow)
		-> std::array<QAction*, 4>
	{
		return {
			aWindow.previous_file_action(),
			aWindow.next_file_action(),
			aWindow.previous_change_action(),
			aWindow.next_change_action(),
		};
	}

	[[nodiscard]] auto file_direction(
		const core::workbench_interaction_intent& aIntent)
		-> std::optional<core::change_navigation_direction>
	{
		const auto* const navigate = std::get_if<
			core::navigate_workbench_file_intent>(&aIntent);
		return navigate == nullptr
			? std::nullopt
			: std::optional{ navigate->Direction };
	}

	void test_every_primary_action_translates_to_a_semantic_intent()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};

		window.publish(both_files_enabled());
		window.publish_change_navigation(both_changes_enabled());
		expect(window.previous_file_action()->isEnabled()
				   && window.next_file_action()->isEnabled()
				   && window.previous_change_action()->isEnabled()
				   && window.next_change_action()->isEnabled(),
			   "an enabling projection enables every primary action");

		window.previous_file_action()->trigger();
		window.next_file_action()->trigger();
		window.previous_change_action()->trigger();
		window.next_change_action()->trigger();

		expect(log.Workbench.size() == 2U && log.Change.size() == 2U,
			   "each primary action emits exactly one semantic intent");
		expect(file_direction(log.Workbench[0])
					   == core::change_navigation_direction::previous
				   && file_direction(log.Workbench[1])
					   == core::change_navigation_direction::next,
			   "file actions emit navigate-workbench-file intents by direction");
		expect(log.Change[0].Direction
					   == core::change_navigation_direction::previous
				   && log.Change[1].Direction
					   == core::change_navigation_direction::next,
			   "change actions emit navigate-diff-change intents by direction");
		expect(log.Change[0].Policy == core::change_navigation_policy::stop
				   && log.Change[1].Policy == core::change_navigation_policy::stop,
			   "the emitted change intent repeats the decided navigation policy");

		auto wrapping   = both_changes_enabled();
		wrapping.Policy = core::change_navigation_policy::wrap;
		window.publish_change_navigation(wrapping);
		window.next_change_action()->trigger();
		expect(log.Change.size() == 3U
				   && log.Change[2].Policy == core::change_navigation_policy::wrap,
			   "a changed decided policy travels with the next emitted gesture");
	}

	void test_action_state_follows_core_command_projections()
	{
		qt::workbench_window window;

		expect(!window.previous_file_action()->isEnabled()
				   && !window.next_file_action()->isEnabled()
				   && !window.previous_change_action()->isEnabled()
				   && !window.next_change_action()->isEnabled(),
			   "no published projection offers no navigation gesture");
		expect(window.previous_file_action()->toolTip()
				   == QStringLiteral("Open a pull request to review its files."),
			   "an unprojected file action carries the inactive-workbench reason");
		expect(window.previous_change_action()->toolTip()
				   == QStringLiteral("No file comparison is open."),
			   "an unprojected change action carries the absent-comparison reason");

		window.publish(file_commands(
			file_command(core::workbench_command_id::previous_file,
						 false,
						 core::workbench_reason_code::at_first_file),
			file_command(core::workbench_command_id::next_file,
						 true,
						 core::workbench_reason_code::none)));
		expect(!window.previous_file_action()->isEnabled()
				   && window.next_file_action()->isEnabled(),
			   "QAction enabled state follows the core command projection");
		expect(window.previous_file_action()->toolTip()
				   == QStringLiteral("This is the first actionable file."),
			   "a disabled file action's tooltip carries its exact core reason");
		expect(window.previous_file_action()->statusTip()
				   == window.previous_file_action()->toolTip(),
			   "the disabled file reason also reaches the status tip");
		expect(window.next_file_action()->toolTip()
				   == QStringLiteral("Select the next actionable file."),
			   "an enabled file action describes the gesture it offers");

		window.publish(file_commands(
			file_command(core::workbench_command_id::previous_file,
						 false,
						 core::workbench_reason_code::no_actionable_files),
			file_command(core::workbench_command_id::next_file,
						 false,
						 core::workbench_reason_code::at_last_file)));
		expect(window.previous_file_action()->toolTip()
				   == QStringLiteral("This pull request has no actionable files."),
			   "a different core reason produces a different exact file reason");
		expect(window.next_file_action()->toolTip()
					   == QStringLiteral("This is the last actionable file.")
				   && window.next_file_action()->statusTip()
					   == window.next_file_action()->toolTip(),
			   "each disabled file action carries its own exact core reason");

		window.publish_change_navigation({
			.Previous = { .Enabled = false,
						  .Outcome = core::change_navigation_outcome::at_start },
			.Next     = { .Enabled = true,
						  .Outcome = core::change_navigation_outcome::selected },
			.Policy   = core::change_navigation_policy::stop,
		});
		expect(!window.previous_change_action()->isEnabled()
				   && window.next_change_action()->isEnabled(),
			   "QAction enabled state follows the decided change projection");
		expect(window.previous_change_action()->toolTip()
					   == QStringLiteral("This is the first change in this comparison.")
				   && window.previous_change_action()->statusTip()
					   == window.previous_change_action()->toolTip(),
			   "a disabled change action's tooltip carries its exact core reason");

		window.publish_change_navigation({
			.Previous = { .Enabled = false,
						  .Outcome = core::change_navigation_outcome::no_changes },
			.Next     = { .Enabled = false,
						  .Outcome = core::change_navigation_outcome::no_changes },
			.Policy   = core::change_navigation_policy::stop,
		});
		expect(window.previous_change_action()->toolTip()
				   == QStringLiteral(
					   "This comparison has no changes to navigate."),
			   "a different core outcome produces a different exact change reason");

		const auto before = window.statistics();
		window.fail({ .Failure
					  = application::workbench_host_failure::observation_limit });
		expect(window.statistics().Failures == before.Failures + 1U,
			   "a host failure is observed by the window");
		expect(window.next_file_action()->toolTip()
					   == QStringLiteral("This is the last actionable file.")
				   && !window.next_file_action()->isEnabled(),
			   "a host failure does not let the window re-decide command legality");
		expect(window.status_text().contains(QStringLiteral(
				   "more actionable files than the workbench retains")),
			   "the host failure is explicit in status text");
	}

	void test_compact_overflow_preserves_keyboard_access()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};
		window.publish(both_files_enabled());
		window.publish_change_navigation(both_changes_enabled());

		const auto threshold = window.compact_width_threshold();
		expect(threshold > window.minimumWidth(),
			   "the compact overflow is reachable at a supported window width");

		window.resize(threshold + 320, 720);
		window.show();
		QApplication::processEvents();
		expect(window.toolbar_mode() == qt::workbench_toolbar_mode::expanded,
			   "a wide window keeps primary navigation top-level");
		for (auto* const action : navigation_actions(window)) {
			expect(window.primary_toolbar()->actions().contains(action),
				   "every primary action is top-level in the wide toolbar");
			expect(qobject_cast<QToolButton*>(
					   window.primary_toolbar()->widgetForAction(action))
					   != nullptr,
				   "every top-level action has a focusable toolbar button");
		}
		expect(!window.navigation_overflow_button()->isVisible(),
			   "the compact overflow stays out of the way at wide widths");

		window.resize(window.minimumWidth(), 720);
		QApplication::processEvents();
		expect(window.toolbar_mode() == qt::workbench_toolbar_mode::compact,
			   "a narrow window moves primary navigation into the overflow");
		expect(window.navigation_overflow_button()->isVisible(),
			   "the compact overflow is explicit rather than implicit");
		expect(window.navigation_overflow_button()->focusPolicy() != Qt::NoFocus,
			   "the compact overflow itself is keyboard focusable");
		expect(!window.navigation_overflow_button()->shortcut().isEmpty(),
			   "the compact overflow opens from a keyboard mnemonic");
		expect(window.navigation_overflow_button()->menu() != nullptr
				   && window.navigation_overflow_button()->menu()
					   == window.navigation_overflow_menu(),
			   "the compact overflow presents the navigation menu");

		for (auto* const action : navigation_actions(window)) {
			expect(!window.primary_toolbar()->actions().contains(action),
				   "a narrow toolbar no longer shows the action top-level");
			expect(window.navigation_overflow_menu()->actions().contains(action),
				   "every primary action stays reachable inside the overflow");
			expect(window.actions().contains(action),
				   "every primary action stays associated with the window so its "
				   "shortcut keeps working in the compact overflow");
			expect(!action->shortcut().isEmpty(),
				   "every primary action keeps a keyboard shortcut when compact");
			expect(action->shortcutContext() == Qt::WindowShortcut,
				   "the retained shortcut is addressable from the whole window");
			expect(action->isVisible(),
				   "no primary action silently disappears when compact");
			expect(action->isEnabled(),
				   "compact layout does not change command legality");
		}

		log.Workbench.clear();
		log.Change.clear();
		window.previous_file_action()->trigger();
		window.next_change_action()->trigger();
		expect(log.Workbench.size() == 1U && log.Change.size() == 1U,
			   "compact primary actions still translate to semantic intents");

		window.resize(threshold + 320, 720);
		QApplication::processEvents();
		expect(window.toolbar_mode() == qt::workbench_toolbar_mode::expanded,
			   "widening the window restores top-level primary navigation");
		for (auto* const action : navigation_actions(window)) {
			expect(window.primary_toolbar()->actions().contains(action)
					   && window.actions().contains(action)
					   && action->isEnabled(),
				   "restoring the wide layout preserves the action and its state");
		}
		window.hide();
	}

	void test_shell_accessibility_status_and_scale()
	{
		qt::workbench_window window;
		expect(!window.primary_toolbar()->accessibleName().isEmpty()
				   && !window.primary_toolbar()->accessibleDescription().isEmpty(),
			   "the primary toolbar carries an accessible name and description");
		for (auto* const action : navigation_actions(window)) {
			auto* const button = qobject_cast<QToolButton*>(
				window.primary_toolbar()->widgetForAction(action));
			expect(button != nullptr && !button->accessibleName().isEmpty()
					   && !button->accessibleDescription().isEmpty(),
				   "every primary action carries an accessible name and "
				   "description");
			expect(button->focusPolicy() == Qt::StrongFocus,
				   "every primary action is keyboard focusable");
		}
		expect(!window.navigation_overflow_button()->accessibleName().isEmpty()
				   && !window.navigation_overflow_menu()
						   ->accessibleName()
						   .isEmpty(),
			   "the compact overflow carries accessible names");

		expect(window.files_drawer_placeholder() != nullptr
				   && window.conversations_drawer_placeholder() != nullptr
				   && window.epoch_selector_placeholder() != nullptr
				   && window.comparison_surface_placeholder() != nullptr
				   && window.detail_pane_placeholder() != nullptr,
			   "the shell names every later-card surface");

		expect(window.status_text()
				   == QStringLiteral("Open a pull request to begin reviewing."),
			   "an inactive workbench states why navigation is unavailable");
		auto selection = both_files_enabled();
		selection.Selection
			= core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{
												   .Ordinal           = 3U,
												   .ActionableCount   = 12U,
												   .PreviousAvailable = true,
												   .NextAvailable     = true,
											   } };
		const auto before = window.statistics();
		window.publish(selection);
		expect(window.status_text()
				   == QStringLiteral("Actionable file 3 of 12."),
			   "the retained cursor is explicit in text as well as enabled state");
		expect(window.statistics().Announcements == before.Announcements + 1U,
			   "a published presentation change announces once");
		window.publish(selection);
		expect(window.statistics().Announcements == before.Announcements + 1U,
			   "an unchanged status is not announced again");

		window.resize(window.compact_width_threshold() + 320, 720);
		window.show();
		QApplication::processEvents();
		const auto expected = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expected.isEmpty()) {
			expect(std::abs(window.devicePixelRatioF() - expected.toDouble())
					   < 0.001,
				   "the window runs at the exact expected device scale");
		}
		expect(window.toolbar_mode() == qt::workbench_toolbar_mode::expanded
				   && window.compact_width_threshold() > window.minimumWidth(),
			   "the compact threshold stays style-derived and reachable at this "
			   "scale");
		window.hide();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_every_primary_action_translates_to_a_semantic_intent();
	test_action_state_follows_core_command_projections();
	test_compact_overflow_preserves_keyboard_access();
	test_shell_accessibility_status_and_scale();
	std::cout << "workbench-window-tests:pass\n";
	return EXIT_SUCCESS;
}
