#include "bpr/qt/workbench_window.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QList>
#include <QListView>
#include <QMenu>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QSplitter>
#include <QToolBar>
#include <QToolButton>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

	/// \brief Records one reported host-owned drawer width.
	struct width_report
	{
		core::workbench_drawer Drawer = core::workbench_drawer::files;
		std::uint32_t          Width  = 0U;
	};

	/// \brief Records the semantic intents one window emitted.
	struct intent_log
	{
		std::vector<core::workbench_interaction_intent> Workbench;
		std::vector<core::navigate_diff_change_intent>  Change;
		std::vector<width_report>                       Width;
	};

	/// \brief Counts the model signals one drawer model actually emitted.
	struct model_signal_log
	{
		std::size_t Inserted = 0U;
		std::size_t Removed  = 0U;
		std::size_t Moved    = 0U;
		std::size_t Changed  = 0U;
		std::size_t Reset    = 0U;
	};

	void observe_model_signals(
		const QAbstractItemModel& aModel,
		model_signal_log&         aLog)
	{
		QObject::connect(&aModel,
						 &QAbstractItemModel::rowsInserted,
						 &aModel,
						 [&aLog](const QModelIndex&, int, int) { ++aLog.Inserted; });
		QObject::connect(&aModel,
						 &QAbstractItemModel::rowsRemoved,
						 &aModel,
						 [&aLog](const QModelIndex&, int, int) { ++aLog.Removed; });
		QObject::connect(
			&aModel,
			&QAbstractItemModel::rowsMoved,
			&aModel,
			[&aLog](const QModelIndex&, int, int, const QModelIndex&, int) {
				++aLog.Moved;
			});
		QObject::connect(
			&aModel,
			&QAbstractItemModel::dataChanged,
			&aModel,
			[&aLog](const QModelIndex&, const QModelIndex&, const QList<int>&) {
				++aLog.Changed;
			});
		QObject::connect(&aModel,
						 &QAbstractItemModel::modelAboutToBeReset,
						 &aModel,
						 [&aLog] { ++aLog.Reset; });
	}

	[[nodiscard]] auto drawer_row(
		const char* const aIdentity,
		const char* const aTitle,
		const char* const aDetail) -> qt::workbench_drawer_row
	{
		return {
			.Identity = QString::fromLatin1(aIdentity),
			.Title    = QString::fromLatin1(aTitle),
			.Detail   = QString::fromLatin1(aDetail),
		};
	}

	[[nodiscard]] auto drawer_states(
		const core::workbench_drawer_state aFiles,
		const core::workbench_drawer_state aConversations)
		-> core::workbench_drawer_delta
	{
		return { .Files = aFiles, .Conversations = aConversations };
	}

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

		expect(window.files_drawer() != nullptr
				   && window.conversations_drawer() != nullptr
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

	void test_drawer_toggles_follow_core_command_projections()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};

		auto* const files         = window.files_drawer_toggle_action();
		auto* const conversations = window.conversations_drawer_toggle_action();
		expect(files->isCheckable() && conversations->isCheckable()
				   && !files->shortcut().isEmpty()
				   && !conversations->shortcut().isEmpty()
				   && window.actions().contains(files)
				   && window.actions().contains(conversations),
			   "each drawer toggle is a checkable window action with a shortcut");
		expect(!window.primary_toolbar()->actions().contains(files)
				   && !window.primary_toolbar()->actions().contains(conversations)
				   && window.drawer_toolbar()->actions().contains(files)
				   && window.drawer_toolbar()->actions().contains(conversations),
			   "the drawer toggles never compete with primary navigation for "
			   "toolbar width");
		for (auto* const toggle : { files, conversations }) {
			auto* const button = qobject_cast<QToolButton*>(
				window.drawer_toolbar()->widgetForAction(toggle));
			expect(button != nullptr && !button->accessibleName().isEmpty()
					   && !button->accessibleDescription().isEmpty()
					   && button->focusPolicy() == Qt::StrongFocus,
				   "each drawer toggle is named, described, and focusable");
		}

		expect(!files->isEnabled() && !conversations->isEnabled(),
			   "no published projection offers no drawer gesture");
		expect(files->toolTip()
				   == QStringLiteral(
					   "Open a pull request to use its review drawers."),
			   "an unprojected drawer toggle carries the inactive-workbench "
			   "reason");

		auto  projection     = both_files_enabled();
		auto& commands       = *projection.Commands;
		commands.Commands[2] = file_command(
			core::workbench_command_id::toggle_files_drawer,
			false,
			core::workbench_reason_code::no_active_workbench);
		projection.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										   core::workbench_drawer_state::expanded);
		window.publish(projection);
		expect(!files->isEnabled() && conversations->isEnabled(),
			   "drawer QAction enabled state follows the core command projection");
		expect(files->toolTip()
					   == QStringLiteral(
						   "Open a pull request to use its review drawers.")
				   && files->statusTip() == files->toolTip(),
			   "a disabled drawer toggle carries its exact core reason");
		expect(conversations->isChecked()
				   && conversations->toolTip()
					   == QStringLiteral("Hide the conversation drawer and give "
										 "its width to the comparison."),
			   "an expanded drawer offers the gesture that collapses it");

		conversations->trigger();
		expect(log.Workbench.size() == 1U,
			   "a drawer toggle emits exactly one semantic intent");
		const auto* const toggle = std::get_if<core::toggle_workbench_drawer_intent>(
			&log.Workbench[0]);
		expect(toggle != nullptr
				   && toggle->Drawer == core::workbench_drawer::conversations,
			   "the emitted intent names the toggled drawer");
		expect(conversations->isChecked()
				   && window.conversations_drawer()->state()
					   == core::workbench_drawer_state::expanded,
			   "triggering a toggle changes no drawer state locally; the core "
			   "decides");
		expect(log.Width.empty(),
			   "a collapse gesture never travels the host width path");

		projection         = both_files_enabled();
		projection.Drawers = drawer_states(
			core::workbench_drawer_state::expanded,
			core::workbench_drawer_state::collapsed);
		window.publish(projection);
		expect(!conversations->isChecked()
				   && window.conversations_drawer()->state()
					   == core::workbench_drawer_state::collapsed,
			   "the decided drawer state reaches the toggle and the drawer");
		expect(conversations->toolTip()
				   == QStringLiteral(
					   "Show the conversation drawer at its saved width."),
			   "a collapsed drawer offers the gesture that reopens it");
		expect(window.statistics().DrawerUpdates == 2U,
			   "each published drawer delta is observed exactly once");
	}

	void test_collapsing_a_drawer_returns_space_to_the_centre()
	{
		intent_log           log;
		qt::workbench_window window{
			{ },
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window.resize(window.compact_width_threshold() + 320, 720);
		window.show();
		QApplication::processEvents();
		window.publish(opened);
		QApplication::processEvents();

		auto* const centre = window.comparison_surface_placeholder();
		expect(window.files_drawer()->width() == 320
				   && window.conversations_drawer()->width() == 260,
			   "each drawer takes exactly the width the host published");
		const auto openCentre = centre->width();
		expect(openCentre > 0, "the centre has a measurable width when open");

		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
										  core::workbench_drawer_state::expanded);
		window.publish(collapsed);
		QApplication::processEvents();
		expect(centre->width() >= openCentre + 320,
			   "collapsing a drawer returns its whole width to the centre");
		expect(!window.files_drawer()->isVisible(),
			   "a collapsed drawer leaves the central surface");
		expect(window.conversations_drawer()->width() == 260,
			   "collapsing one drawer leaves the other drawer's width alone");

		auto reopened    = application::workbench_projection{ };
		reopened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										 core::workbench_drawer_state::expanded);
		window.publish(reopened);
		QApplication::processEvents();
		expect(window.files_drawer()->isVisible()
				   && window.files_drawer()->width() == 320,
			   "reopening a drawer restores its persisted width");
		expect(centre->width() == openCentre,
			   "reopening a drawer returns the borrowed width to the drawer");

		// A width persisted while the drawer was closed is the width it reopens
		// at, which a cached widget geometry could not produce.
		window.publish(collapsed);
		QApplication::processEvents();
		auto widened   = application::workbench_projection{ };
		widened.Layout = application::workbench_layout_delta{
			.FilesWidth         = 420U,
			.ConversationsWidth = 260U,
		};
		window.publish(widened);
		window.publish(reopened);
		QApplication::processEvents();
		expect(window.files_drawer()->width() == 420,
			   "reopening restores the width persisted while it was collapsed");

		expect(log.Width.empty(),
			   "applying a published layout reports no width back to the host");
		window.hide();
	}

	void test_drawer_resize_emits_the_host_layout_path()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window.resize(window.compact_width_threshold() + 320, 720);
		window.show();
		QApplication::processEvents();
		window.publish(opened);
		QApplication::processEvents();

		auto* const splitter = window.findChild<QSplitter*>(
			QStringLiteral("workbenchCentralSplitter"));
		expect(splitter != nullptr && splitter->count() == 3,
			   "the centre and both drawers share one resizable splitter");
		expect(!splitter->childrenCollapsible(),
			   "a drag can never collapse a drawer behind the core's back");

		log.Width.clear();
		log.Workbench.clear();
		const auto sizes = splitter->sizes();
		splitter->setSizes({ 400, sizes[1] - 80, sizes[2] });
		QApplication::processEvents();
		expect(window.files_drawer()->width() == 400,
			   "a splitter drag resizes the drawer");
		expect(log.Width.size() == 1U
				   && log.Width[0].Drawer == core::workbench_drawer::files
				   && log.Width[0].Width == 400U,
			   "a width change leaves once through the host layout path");
		expect(log.Workbench.empty(),
			   "a width change never becomes a semantic drawer intent");

		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
										  core::workbench_drawer_state::expanded);
		auto reopened     = application::workbench_projection{ };
		reopened.Drawers  = drawer_states(core::workbench_drawer_state::expanded,
										  core::workbench_drawer_state::expanded);
		log.Width.clear();
		window.publish(collapsed);
		QApplication::processEvents();
		window.publish(reopened);
		QApplication::processEvents();
		expect(window.files_drawer()->width() == 400,
			   "the resized width is the width the drawer reopens at");
		expect(log.Width.empty(),
			   "collapsing and reopening reports no new width to the host");
		window.hide();
	}

	void test_drawer_contents_use_incremental_model_signals()
	{
		qt::workbench_window window;
		auto* const          contents = window.files_drawer()->contents();
		expect(contents != nullptr
				   && window.files_drawer()->contents_view()->model() == contents,
			   "the drawer view is bound to its stable-identity model");

		model_signal_log emitted;
		observe_model_signals(*contents, emitted);

		contents->apply({ drawer_row("a", "alpha.cpp", "Modified"),
						  drawer_row("b", "beta.cpp", "Added"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(contents->rowCount() == 3 && emitted.Inserted == 3U
				   && emitted.Reset == 0U,
			   "first contents arrive as row insertions rather than a reset");

		const QPersistentModelIndex retained{ contents->index_for_identity(
			QStringLiteral("b")) };
		expect(retained.isValid(), "a stable identity resolves to a live index");

		contents->apply({ drawer_row("a", "alpha.cpp", "Modified"),
						  drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(emitted.Changed == 1U && emitted.Inserted == 3U
				   && emitted.Removed == 0U && emitted.Moved == 0U
				   && emitted.Reset == 0U,
			   "a bounded state change is one data change, not a reset");
		expect(retained.isValid()
				   && contents->identity_at(retained) == QStringLiteral("b"),
			   "a persistent index survives a bounded state change");
		expect(contents->data(retained, Qt::AccessibleDescriptionRole).toString()
				   == QStringLiteral("Modified"),
			   "the changed row carries its new accessible description");

		contents->apply({ drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("a", "alpha.cpp", "Modified"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(emitted.Moved == 1U && emitted.Inserted == 3U
				   && emitted.Removed == 0U && emitted.Reset == 0U,
			   "a bounded reorder is one row move, not a reset");
		expect(retained.isValid() && retained.row() == 0
				   && contents->identity_at(retained) == QStringLiteral("b"),
			   "a moved row keeps its identity and its persistent index");

		contents->apply({ drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(contents->rowCount() == 2 && emitted.Removed == 1U
				   && emitted.Inserted == 3U && emitted.Reset == 0U,
			   "a bounded removal is one row removal, not a reset");
		expect(contents->index_for_identity(QStringLiteral("a")).isValid() == false,
			   "a retired identity no longer resolves");

		contents->apply({ drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("d", "delta.cpp", "Renamed"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(contents->rowCount() == 3 && emitted.Inserted == 4U
				   && emitted.Reset == 0U,
			   "a bounded insertion is one row insertion, not a reset");

		const auto statistics = contents->statistics();
		expect(statistics.ModelResets == 0U
				   && statistics.RowsInserted == emitted.Inserted
				   && statistics.RowsRemoved == emitted.Removed
				   && statistics.RowsMoved == emitted.Moved
				   && statistics.RowsChanged == emitted.Changed,
			   "the model's own record agrees with the signals it emitted");

		auto* const conversations = window.conversations_drawer()->contents();
		expect(conversations != nullptr && conversations != contents
				   && conversations->rowCount() == 0,
			   "each drawer owns its own independent contents");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_every_primary_action_translates_to_a_semantic_intent();
	test_action_state_follows_core_command_projections();
	test_compact_overflow_preserves_keyboard_access();
	test_shell_accessibility_status_and_scale();
	test_drawer_toggles_follow_core_command_projections();
	test_collapsing_a_drawer_returns_space_to_the_centre();
	test_drawer_resize_emits_the_host_layout_path();
	test_drawer_contents_use_incremental_model_signals();
	std::cout << "workbench-window-tests:pass\n";
	return EXIT_SUCCESS;
}
