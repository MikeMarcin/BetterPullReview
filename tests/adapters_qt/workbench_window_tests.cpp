#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractItemModel>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QList>
#include <QListView>
#include <QMenu>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
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
				   && window.comparison_surface() != nullptr
				   && window.detail_pane_placeholder() != nullptr,
			   "the shell names its composed comparison surface and every "
			   "later-card placeholder");

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

		auto* const centre = window.comparison_surface();
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

	[[nodiscard]] auto close(
		const qreal aLeft,
		const qreal aRight,
		const qreal aTolerance = 0.001) noexcept -> bool
	{
		return std::abs(aLeft - aRight) <= aTolerance;
	}

	void process_events()
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto message_for(
		const std::string_view aMessage,
		const std::string_view aStage) -> std::string
	{
		return std::string{ aMessage } + " (" + std::string{ aStage } + ')';
	}

	template <typename TIdentity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return TIdentity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const core::diff_options& aOptions)
		-> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "the comparison fixture options normalize");
		const auto scope     = opaque<core::pull_request_authority_scope_id>(11U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ 1 },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<core::file_identity>(31U),
			.Options   = *digest,
			.Algorithm = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto source_bytes(const std::string_view aText)
		-> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	/// \brief Computes one immutable RFC-BPR-0004 comparison fixture.
	[[nodiscard]] auto computed_comparison(
		const std::string_view aLeft,
		const std::string_view aRight) -> core::computed_file_diff
	{
		const auto options = core::diff_options{ };
		auto       left    = source_bytes(aLeft);
		auto       right   = source_bytes(aRight);
		const auto outcome = core::compute_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(outcome.Reason == core::diff_reason_code::none
				   && outcome.Result.has_value(),
			   "the comparison fixture computes and publishes");
		return *outcome.Result;
	}

	[[nodiscard]] auto request_context(const core::computed_file_diff& aDiff)
		-> core::diff_request_context
	{
		return {
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = aDiff.Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
	}

	/// \brief Builds two sources whose changes are spread through the file.
	///
	/// \details The two sides deliberately end up different lengths and carry
	/// empty-span insertions and deletions, so a surface that invented alignment
	/// rows to make both documents the same height could not pass.
	[[nodiscard]] auto comparison_fixture_text()
		-> std::pair<std::string, std::string>
	{
		auto       left   = std::string{ };
		auto       right  = std::string{ };
		const auto common = [&left, &right](
								const std::string_view aPrefix,
								const std::size_t      aCount) {
			for (std::size_t line = 0U; line < aCount; ++line) {
				const auto text = std::string{ aPrefix } + '-'
					+ std::to_string(line) + '\n';
				left += text;
				right += text;
			}
		};

		common("leading", 12U);
		left += "replaced-old-a\nreplaced-old-b\nreplaced-old-c\n";
		right += "replaced-new-a\nreplaced-new-b\n";
		common("alpha", 18U);
		right += "inserted-a\ninserted-b\ninserted-c\ninserted-d\n";
		common("beta", 18U);
		left += "deleted-a\ndeleted-b\n";
		common("gamma", 18U);
		left += "second-old-a\nsecond-old-b\n";
		right += "second-new-a\nsecond-new-b\nsecond-new-c\n";
		common("delta", 18U);
		right += "appended-a\nappended-b\nappended-c\n";
		common("tail", 16U);
		return { std::move(left), std::move(right) };
	}

	/// \brief Counts the source rows one composed pane document actually holds.
	///
	/// \details Qt leaves one trailing empty block for a document whose last
	/// source line ends in a newline. That block is a final-newline artifact,
	/// never a source row and never an alignment row.
	[[nodiscard]] auto document_rows(const QPlainTextEdit& aPane) -> std::uint32_t
	{
		const auto* const document = aPane.document();
		const auto        blocks   = static_cast<std::uint32_t>(document->blockCount());
		return blocks != 0U && document->lastBlock().text().isEmpty()
			? blocks - 1U
			: blocks;
	}

	/// \brief Measures live line boundaries from one composed natural pane.
	[[nodiscard]] auto measured_line_boundaries(
		const QPlainTextEdit& aPane,
		const std::uint32_t   aLines) -> std::vector<qreal>
	{
		const auto* const document   = aPane.document();
		const auto* const layout     = document->documentLayout();
		auto              extent     = document->documentMargin();
		auto              boundaries = std::vector<qreal>{ };
		boundaries.reserve(static_cast<std::size_t>(aLines) + 1U);
		boundaries.push_back(extent);
		auto block = document->begin();
		for (auto line = 0U; line < aLines && block.isValid(); ++line) {
			extent += layout->blockBoundingRect(block).height();
			boundaries.push_back(extent);
			block = block.next();
		}
		return boundaries;
	}

	[[nodiscard]] auto segment_index(
		const core::computed_file_diff&  aDiff,
		const core::alignment_segment_id aSegment) -> std::optional<std::size_t>
	{
		const auto found = std::ranges::find(
			aDiff.Alignment.Segments, aSegment, &core::alignment_segment::Id);
		return found == aDiff.Alignment.Segments.end()
			? std::nullopt
			: std::optional{ static_cast<std::size_t>(
				  std::distance(aDiff.Alignment.Segments.begin(), found)) };
	}

	[[nodiscard]] auto canonical_group(
		const core::computed_file_diff& aDiff,
		const core::change_group_id     aGroup) -> bool
	{
		return std::ranges::any_of(
			aDiff.Alignment.Groups,
			[aGroup](const core::change_group& aCandidate) {
				return aCandidate.Id == aGroup;
			});
	}

	/// \brief Records what one composed-surface observation actually painted.
	struct composed_observation
	{
		std::size_t Ribbons = 0U;
		std::size_t Wedges  = 0U;
		std::size_t Grouped = 0U;
	};

	/// \brief Re-checks every composed comparison rule at one interaction stage.
	///
	/// \details The stage name travels into each message so a failure names the
	/// exact scroll or resize state that produced it.
	[[nodiscard]] auto observe_composed_comparison(
		const qt::workbench_window&     aWindow,
		const core::computed_file_diff& aDiff,
		const QString&                  aLeftText,
		const QString&                  aRightText,
		const std::string_view          aStage) -> composed_observation
	{
		auto* const surface = aWindow.comparison_surface();
		auto* const left    = surface->left_pane();
		auto* const right   = surface->right_pane();

		const auto bars  = surface->comparison_splitter()
							   ->parentWidget()
							   ->findChildren<QScrollBar*>();
		const auto shown = std::ranges::count_if(
			bars, [](const QScrollBar* const aBar) {
				return aBar->orientation() == Qt::Vertical && aBar->isVisible();
			});
		expect(shown == 1,
			   message_for("the comparison region shows exactly one vertical "
						   "scrollbar",
						   aStage));
		expect(surface->shared_scrollbar()->isVisible()
				   && surface->shared_scrollbar()->isEnabled(),
			   message_for("the one visible vertical scrollbar is the shared "
						   "comparison scrollbar",
						   aStage));
		expect(left->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff
				   && right->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
			   message_for("neither top pane owns vertical scroll authority",
						   aStage));

		expect(left->toPlainText() == aLeftText
				   && right->toPlainText() == aRightText,
			   message_for("each top document is exactly its own source text",
						   aStage));
		expect(document_rows(*left) == aDiff.Alignment.LeftLineCount
				   && document_rows(*right) == aDiff.Alignment.RightLineCount,
			   message_for("neither top document contains a synthetic alignment "
						   "row",
						   aStage));

		const auto leftBoundaries = measured_line_boundaries(
			*left, aDiff.Alignment.LeftLineCount);
		const auto rightBoundaries = measured_line_boundaries(
			*right, aDiff.Alignment.RightLineCount);
		expect(leftBoundaries.size()
					   == static_cast<std::size_t>(aDiff.Alignment.LeftLineCount)
						   + 1U
				   && rightBoundaries.size()
					   == static_cast<std::size_t>(
							  aDiff.Alignment.RightLineCount)
						   + 1U,
			   message_for("each composed pane measures one boundary per source "
						   "line",
						   aStage));

		auto observation       = composed_observation{ };
		auto previous          = std::optional<std::size_t>{ };
		auto previousLeft      = qreal{ 0.0 };
		auto previousRight     = qreal{ 0.0 };
		auto previousLeftLine  = std::uint32_t{ 0U };
		auto previousRightLine = std::uint32_t{ 0U };
		expect(!surface->link_map()->visible_links().empty(),
			   message_for("the composed link map paints the visible "
						   "correspondences",
						   aStage));
		for (const auto& link : surface->link_map()->visible_links()) {
			const auto index = segment_index(aDiff, link.Identity.Segment);
			expect(index.has_value(),
				   message_for("every painted ribbon names a canonical "
							   "RFC-BPR-0004 segment",
							   aStage));
			const auto& segment = aDiff.Alignment.Segments[*index];
			expect(link.Kind == segment.Kind
					   && link.Identity.Group == segment.Group,
				   message_for("every ribbon carries its canonical segment kind "
							   "and group rather than a presentation-side "
							   "regrouping",
							   aStage));
			expect(segment.Group.has_value()
					   && canonical_group(aDiff, *segment.Group),
				   message_for("every painted correspondence belongs to a "
							   "canonical change group",
							   aStage));
			expect(close(link.LeftBottom - link.LeftTop,
						 leftBoundaries[segment.Left.End]
							 - leftBoundaries[segment.Left.Begin])
					   && close(link.RightBottom - link.RightTop,
								rightBoundaries[segment.Right.End]
									- rightBoundaries[segment.Right.Begin]),
				   message_for("each ribbon spans the measured pane interval of "
							   "its segment",
							   aStage));
			if (previous.has_value()) {
				expect(*index > *previous,
					   message_for("visible ribbons keep canonical segment "
								   "order",
								   aStage));
				expect(link.LeftTop >= previousLeft
						   && link.RightTop >= previousRight,
					   message_for("ordinary correspondence stays monotonic and "
								   "noncrossing",
								   aStage));
				expect(close(link.LeftTop - previousLeft,
							 leftBoundaries[segment.Left.Begin]
								 - leftBoundaries[previousLeftLine])
						   && close(link.RightTop - previousRight,
									rightBoundaries[segment.Right.Begin]
										- rightBoundaries[previousRightLine]),
					   message_for("ribbon spacing follows the measured pane "
								   "geometry between segments",
								   aStage));
			}
			previous          = index;
			previousLeft      = link.LeftTop;
			previousRight     = link.RightTop;
			previousLeftLine  = segment.Left.Begin;
			previousRightLine = segment.Right.Begin;
			++observation.Ribbons;
			observation.Grouped += link.Identity.Group.has_value() ? 1U : 0U;
			observation.Wedges += segment.Left.Begin == segment.Left.End
					|| segment.Right.Begin == segment.Right.End
				? 1U
				: 0U;
		}
		return observation;
	}

	/// \brief Publishes the fixture comparison into the composed workbench centre.
	[[nodiscard]] auto publish_fixture(
		qt::workbench_window&           aWindow,
		const core::computed_file_diff& aDiff) -> qt::diff_workbench*
	{
		auto* const surface = aWindow.comparison_surface();
		surface->set_loading(request_context(aDiff));
		surface->publish({
			.Context = request_context(aDiff),
			.Result  = std::make_shared<const core::computed_file_diff>(aDiff),
		});
		process_events();
		expect(surface->current_result() != nullptr,
			   "the composed surface accepts its exact publication");
		return surface;
	}

	void test_the_centre_is_the_composed_rfc_0004_comparison_surface()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);
		expect(diff.Alignment.Groups.size() >= 3U,
			   "the fixture carries several canonical change groups");
		expect(diff.Alignment.LeftLineCount != diff.Alignment.RightLineCount,
			   "the fixture would need invented rows to make both documents the "
			   "same height");

		qt::workbench_window window;
		window.resize(window.compact_width_threshold() + 320, 820);
		window.show();
		process_events();

		auto* const surface = window.comparison_surface();
		expect(surface != nullptr
				   && surface->parentWidget()
					   == window.findChild<QWidget*>(
						   QStringLiteral("workbenchCentralSurface")),
			   "the composed comparison surface is the workbench centre");
		expect(static_cast<application::diff_projection_port*>(surface)
				   != nullptr,
			   "the composed centre is the workbench's diff projection port");
		expect(surface->comparison_splitter() != nullptr
				   && surface->comparison_splitter()->count() == 2
				   && surface->link_map()
					   == surface->comparison_splitter()->link_map(),
			   "the centre holds exactly the two natural panes divided by the "
			   "single link map");
		expect(surface->scroll_controller() != nullptr
				   && surface->shared_scrollbar() != nullptr,
			   "the centre reuses one retained comparison position and its "
			   "shared scrollbar");

		static_cast<void>(publish_fixture(window, diff));
		const auto published = observe_composed_comparison(
			window,
			diff,
			QString::fromStdString(leftText),
			QString::fromStdString(rightText),
			"published");
		expect(published.Ribbons > 0U
				   && published.Grouped == published.Ribbons,
			   "every painted correspondence carries a canonical group identity");

		// The shell publishes command, drawer, and layout meaning only. A
		// workbench projection that also moved comparison state would be the
		// second authority RFC-BPR-0004 forbids.
		process_events();
		const auto before = surface->statistics();
		window.publish(both_files_enabled());
		window.publish_change_navigation(both_changes_enabled());
		expect(surface->statistics() == before
				   && surface->current_result() != nullptr,
			   "a workbench-shell projection never re-decides composed "
			   "comparison state");
		window.hide();
	}

	void test_composed_ribbons_follow_the_group_model_through_scroll_and_resize()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);
		const auto left                  = QString::fromStdString(leftText);
		const auto right                 = QString::fromStdString(rightText);

		qt::workbench_window window;
		window.resize(window.compact_width_threshold() + 320, 820);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		expect(surface->scroll_controller()->maximum_position() > 0.0,
			   "the composed comparison is taller than its viewport, so the "
			   "shared scrollbar has travel");

		auto totals = observe_composed_comparison(
			window, diff, left, right, "published");

		auto* const shared     = surface->shared_scrollbar();
		const auto  leftBefore = surface->left_pane()->verticalScrollBar()->value();
		const auto  rightBefore
			= surface->right_pane()->verticalScrollBar()->value();
		shared->setValue(shared->maximum());
		process_events();
		expect(surface->left_pane()->verticalScrollBar()->value() != leftBefore
				   && surface->right_pane()->verticalScrollBar()->value()
					   != rightBefore,
			   "the one shared vertical scrollbar moves both natural panes");
		const auto scrolled = observe_composed_comparison(
			window, diff, left, right, "scrolled");

		window.resize(window.width() - 200, 700);
		process_events();
		const auto resized = observe_composed_comparison(
			window, diff, left, right, "resized");

		totals.Ribbons += scrolled.Ribbons + resized.Ribbons;
		totals.Wedges += scrolled.Wedges + resized.Wedges;
		totals.Grouped += scrolled.Grouped + resized.Grouped;
		expect(totals.Grouped == totals.Ribbons,
			   "every ribbon agrees with the canonical group model through "
			   "scroll and resize");
		expect(totals.Wedges > 0U,
			   "an empty-span correspondence renders as a wedge in the composed "
			   "surface");
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
	test_drawer_toggles_follow_core_command_projections();
	test_collapsing_a_drawer_returns_space_to_the_centre();
	test_drawer_resize_emits_the_host_layout_path();
	test_drawer_contents_use_incremental_model_signals();
	test_the_centre_is_the_composed_rfc_0004_comparison_surface();
	test_composed_ribbons_follow_the_group_model_through_scroll_and_resize();
	std::cout << "workbench-window-tests:pass\n";
	return EXIT_SUCCESS;
}
