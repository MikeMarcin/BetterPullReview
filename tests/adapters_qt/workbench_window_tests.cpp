#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractItemModel>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QMenu>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPoint>
#include <QScrollBar>
#include <QSplitter>
#include <QStringList>
#include <QSysInfo>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include <QTreeView>

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
		std::vector<core::file_receipt_command>         Receipt;
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

	[[nodiscard]] constexpr auto workbench_file(const std::uint8_t aSeed)
		-> core::workbench_file_id
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return core::workbench_file_id{ bytes };
	}

	[[nodiscard]] constexpr auto file_material(const std::uint8_t aSeed)
		-> core::file_material_id
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return core::file_material_id{ bytes };
	}

	[[nodiscard]] constexpr auto comparison_digest(const std::uint8_t aSeed)
		-> core::file_comparison_digest
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0x3CU);
		return core::file_comparison_digest{ bytes };
	}

	[[nodiscard]] constexpr auto defect_history(
		const std::uint64_t                    aId,
		const core::defect_anchor_availability aAvailability) -> core::defect_history
	{
		const auto anchor = core::source_line_ref{
			.Comparison = comparison_digest(static_cast<std::uint8_t>(aId)),
			.Side       = core::diff_side::right,
			.Line       = static_cast<std::uint32_t>(aId + 10U),
		};
		return {
			.Defect = core::defect_record{
				.Id     = core::defect_id{ aId },
				.Anchor = anchor,
				.State  = core::defect_state::needs_verification,
			},
			.OriginalAnchor = anchor,
			.Availability   = aAvailability,
		};
	}

	[[nodiscard]] auto drawer_states(
		const core::workbench_drawer_state aFiles,
		const core::workbench_drawer_state aConversations)
		-> core::workbench_drawer_delta
	{
		return { .Files = aFiles, .Conversations = aConversations };
	}

	[[nodiscard]] auto command_state(
		const core::workbench_command_id  aId,
		const bool                        aEnabled,
		const core::workbench_reason_code aReason)
		-> core::workbench_command_projection
	{
		return { .Id = aId, .Enabled = aEnabled, .DisabledReason = aReason };
	}

	[[nodiscard]] auto offered(const core::workbench_command_id aId)
		-> core::workbench_command_projection
	{
		return command_state(aId, true, core::workbench_reason_code::none);
	}

	[[nodiscard]] auto refused(const core::workbench_command_id  aId,
							   const core::workbench_reason_code aReason)
		-> core::workbench_command_projection
	{
		return command_state(aId, false, aReason);
	}

	/// \brief Builds one core command delta with every entry in projection order.
	///
	/// \details Entry `i` projects `core::workbench_command_id{ i }`, exactly as
	/// the core publishes it, so a window that read the wrong entry would answer
	/// with the wrong command's identity rather than silently agreeing.
	[[nodiscard]] auto command_delta(
		const core::workbench_command_projection& aPreviousFile,
		const core::workbench_command_projection& aNextFile,
		const core::workbench_command_projection& aPreviousChange,
		const core::workbench_command_projection& aNextChange,
		const core::change_navigation_policy      aPolicy
		= core::change_navigation_policy::stop) -> core::workbench_command_delta
	{
		return {
			.Commands = {
				aPreviousFile,
				aNextFile,
				aPreviousChange,
				aNextChange,
				offered(core::workbench_command_id::toggle_files_drawer),
				offered(core::workbench_command_id::toggle_conversations_drawer),
			},
			.Policy = aPolicy,
		};
	}

	/// \brief Projects file legality while no comparison is open.
	[[nodiscard]] auto file_commands(
		const core::workbench_command_projection& aPrevious,
		const core::workbench_command_projection& aNext)
		-> application::workbench_projection
	{
		return {
			.Commands = command_delta(
				aPrevious,
				aNext,
				refused(core::workbench_command_id::previous_change,
						core::workbench_reason_code::no_open_comparison),
				refused(core::workbench_command_id::next_change,
						core::workbench_reason_code::no_open_comparison)),
		};
	}

	[[nodiscard]] auto both_files_enabled() -> application::workbench_projection
	{
		return file_commands(offered(core::workbench_command_id::previous_file),
							 offered(core::workbench_command_id::next_file));
	}

	/// \brief Projects every primary command as offered under one policy.
	[[nodiscard]] auto every_primary_command_offered(
		const core::change_navigation_policy aPolicy
		= core::change_navigation_policy::stop)
		-> application::workbench_projection
	{
		return {
			.Commands = command_delta(
				offered(core::workbench_command_id::previous_file),
				offered(core::workbench_command_id::next_file),
				offered(core::workbench_command_id::previous_change),
				offered(core::workbench_command_id::next_change),
				aPolicy),
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

	void test_all_four_primary_actions_follow_one_core_projection()
	{
		qt::workbench_window window;

		// Four different decisions in one delta. An action that read any entry
		// but its own, or that kept a second carrier for change legality, cannot
		// reproduce this pattern.
		const auto mixed = command_delta(
			refused(core::workbench_command_id::previous_file,
					core::workbench_reason_code::at_first_file),
			offered(core::workbench_command_id::next_file),
			offered(core::workbench_command_id::previous_change),
			refused(core::workbench_command_id::next_change,
					core::workbench_reason_code::at_last_change));
		window.publish({ .Commands = mixed });

		const auto actions = navigation_actions(window);
		for (std::size_t index = 0U; index < actions.size(); ++index) {
			expect(mixed.Commands[index].Id
					   == static_cast<core::workbench_command_id>(index),
				   "the published delta projects one entry per command in order");
			expect(actions[index]->isEnabled() == mixed.Commands[index].Enabled,
				   "every primary action follows its own entry of the one core "
				   "command projection");
		}
		expect(window.previous_file_action()->toolTip()
					   == QStringLiteral("This is the first actionable file.")
				   && window.next_change_action()->toolTip()
					   == QStringLiteral(
						   "This is the last change in this comparison."),
			   "each disabled primary action carries its own exact core reason");

		// The same delta shape decides the reverse pair, so no action can be
		// following a constant that merely agreed with the first publication.
		const auto reversed = command_delta(
			offered(core::workbench_command_id::previous_file),
			refused(core::workbench_command_id::next_file,
					core::workbench_reason_code::at_last_file),
			refused(core::workbench_command_id::previous_change,
					core::workbench_reason_code::at_first_change),
			offered(core::workbench_command_id::next_change));
		window.publish({ .Commands = reversed });
		for (std::size_t index = 0U; index < actions.size(); ++index) {
			expect(actions[index]->isEnabled() == reversed.Commands[index].Enabled,
				   "a replacement projection moves every primary action with it");
		}
		expect(window.previous_change_action()->toolTip()
					   == QStringLiteral(
						   "This is the first change in this comparison.")
				   && window.previous_change_action()->statusTip()
					   == window.previous_change_action()->toolTip(),
			   "the change reason reaches the tooltip and the status tip");
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

		window.publish(every_primary_command_offered());
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

		window.publish(
			every_primary_command_offered(core::change_navigation_policy::wrap));
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
				   == QStringLiteral("Open a pull request to review its changes."),
			   "an unprojected change action carries the inactive-workbench reason "
			   "in its own words");

		window.publish(file_commands(
			refused(core::workbench_command_id::previous_file,
					core::workbench_reason_code::at_first_file),
			offered(core::workbench_command_id::next_file)));
		expect(window.previous_change_action()->toolTip()
				   == QStringLiteral("No file comparison is open."),
			   "an active workbench with no open comparison says so exactly");
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
			refused(core::workbench_command_id::previous_file,
					core::workbench_reason_code::no_actionable_files),
			refused(core::workbench_command_id::next_file,
					core::workbench_reason_code::at_last_file)));
		expect(window.previous_file_action()->toolTip()
				   == QStringLiteral("This pull request has no actionable files."),
			   "a different core reason produces a different exact file reason");
		expect(window.next_file_action()->toolTip()
					   == QStringLiteral("This is the last actionable file.")
				   && window.next_file_action()->statusTip()
					   == window.next_file_action()->toolTip(),
			   "each disabled file action carries its own exact core reason");

		window.publish({
			.Commands = command_delta(
				offered(core::workbench_command_id::previous_file),
				offered(core::workbench_command_id::next_file),
				refused(core::workbench_command_id::previous_change,
						core::workbench_reason_code::at_first_change),
				offered(core::workbench_command_id::next_change)),
		});
		expect(!window.previous_change_action()->isEnabled()
				   && window.next_change_action()->isEnabled(),
			   "QAction enabled state follows the same core command projection "
			   "for change navigation");
		expect(window.previous_change_action()->toolTip()
					   == QStringLiteral("This is the first change in this comparison.")
				   && window.previous_change_action()->statusTip()
					   == window.previous_change_action()->toolTip(),
			   "a disabled change action's tooltip carries its exact core reason");

		window.publish({
			.Commands = command_delta(
				offered(core::workbench_command_id::previous_file),
				refused(core::workbench_command_id::next_file,
						core::workbench_reason_code::at_last_file),
				refused(core::workbench_command_id::previous_change,
						core::workbench_reason_code::no_changes),
				refused(core::workbench_command_id::next_change,
						core::workbench_reason_code::no_changes)),
		});
		expect(window.previous_change_action()->toolTip()
				   == QStringLiteral(
					   "This comparison has no changes to navigate."),
			   "a different core reason produces a different exact change reason");

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
		window.publish(every_primary_command_offered());

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
				   && window.epoch_selectors() != nullptr
				   && window.comparison_surface() != nullptr
				   && window.detail_pane() != nullptr,
			   "the shell names its composed epoch selectors, its composed "
			   "comparison surface, and its detail pane");

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

		auto       projection = both_files_enabled();
		auto&      commands   = *projection.Commands;
		const auto filesEntry = static_cast<std::size_t>(
			core::workbench_command_id::toggle_files_drawer);
		commands.Commands[filesEntry]
			= refused(core::workbench_command_id::toggle_files_drawer,
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

		// The conversations drawer no longer has a list model of its own: it
		// hosts RFC-BPR-0002's threaded conversation, which is a forest and
		// cannot be a QAbstractListModel. So the old "each drawer owns its own
		// independent contents" reading is restated rather than kept -- the two
		// drawers stopped being the same kind of thing, which is exactly what
		// BPR-0191's route 2 decided.
		expect(window.conversations_drawer()->contents() == nullptr
				   && window.conversations_drawer()->contents_view() == nullptr,
			   "a drawer that hosts a tree reports no list, rather than a hidden "
			   "one a caller could mistake for its content");
		expect(window.files_drawer()->contents() == contents,
			   "the files drawer is untouched by that: a file list is a list");
	}

	void test_file_rows_emit_only_semantic_user_selection()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(std::move(aIntent));
			},
			{ },
			{ }
		};
		const auto first  = workbench_file(1U);
		const auto second = workbench_file(2U);
		auto       rows   = application::workbench_projection{ };
		rows.Files        = std::vector<application::workbench_file_projection>{
			{ .File = first, .Path = "first.cpp" },
			{ .File = second, .Path = "second.cpp" },
		};
		window.publish(rows);
		expect(
			log.Workbench.empty()
				&& window.files_drawer()->contents()->file_at(
					   window.files_drawer()->contents()->index(1, 0))
					== std::optional{ second },
			"projection inserts retain opaque file identity without emitting intent");

		auto* const view = window.files_drawer()->contents_view();
		view->setCurrentIndex(view->model()->index(1, 0));
		expect(
			log.Workbench.size() == 1U
				&& std::get<core::select_workbench_file_intent>(log.Workbench.back()).File
					== second,
			"list selection emits the retained semantic identity without parsing text");

		auto reordered  = application::workbench_projection{ };
		reordered.Files = std::vector<application::workbench_file_projection>{
			{ .File = second, .Path = "renamed-second.cpp" },
			{ .File = first, .Path = "first.cpp" },
		};
		reordered.Selection = core::workbench_selection_delta{
			.Cursor = core::workbench_navigation_cursor{
				.File              = first,
				.Ordinal           = 2U,
				.ActionableCount   = 2U,
				.PreviousAvailable = true,
				.NextAvailable     = false,
			},
		};
		window.publish(reordered);
		expect(
			log.Workbench.size() == 1U
				&& view->currentIndex()
					== window.files_drawer()->contents()->index_for_file(first),
			"incremental reorder and host-selected row emit no false user selection");
	}

	void test_the_conversations_drawer_hosts_the_threaded_conversation()
	{
		qt::workbench_window window;

		auto* const panel = window.conversation_panel();
		expect(panel != nullptr, "the shell composes the conversation");
		expect(panel != nullptr && window.review_editor() != nullptr
				   && window.conversations_drawer()->isAncestorOf(panel)
				   && window.conversations_drawer()->isAncestorOf(
					   window.review_editor()),
			   "and it lives inside the conversations drawer rather than beside "
			   "it, together with the draft editor");

		// The drawer keeps every behaviour that makes it a drawer. Swapping what
		// it shows must not have cost it collapse, which is core-owned state.
		window.conversations_drawer()->adopt(
			core::workbench_drawer_state::collapsed);
		expect(window.conversations_drawer()->state()
				   == core::workbench_drawer_state::collapsed,
			   "a hosted drawer still adopts the collapse the core decided");
		window.conversations_drawer()->adopt(
			core::workbench_drawer_state::expanded);

		// The declared focus order names one stop per drawer. For the files
		// drawer that is still its list; for this one it is the tree, and the
		// window must not be reaching past the drawer to find it.
		expect(window.conversations_drawer()->focus_stop() == panel->view(),
			   "the drawer's one interactive stop is the conversation tree");
		expect(window.files_drawer()->focus_stop()
				   == window.files_drawer()->contents_view(),
			   "the files drawer's stop is still its list view");

		const auto order = window.focus_order();
		expect(std::find(order.begin(), order.end(),
						 static_cast<QWidget*>(panel->view()))
				   != order.end(),
			   "and the declared focus order names it");
		expect(std::find(order.begin(), order.end(), nullptr) == order.end(),
			   "no drawer contributes a null stop");
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

	/// \brief Builds one comparison key, optionally for a distinct file.
	///
	/// \details The seed only moves the file identity and its two blobs, so a
	/// harness that publishes several different files into one window keeps a
	/// distinct immutable comparison for each. Seed zero is the original key.
	[[nodiscard]] auto comparison_key(
		const core::diff_options& aOptions,
		const std::uint8_t        aSeed = 0U) -> core::file_comparison_key
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
			.LeftBlob  = blob(static_cast<std::uint8_t>(21U + aSeed)),
			.RightBlob = blob(static_cast<std::uint8_t>(51U + aSeed)),
			.File      = opaque<core::file_identity>(static_cast<std::uint8_t>(31U + aSeed)),
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
		const std::string_view aRight,
		const std::uint8_t     aSeed = 0U) -> core::computed_file_diff
	{
		const auto options = core::diff_options{ };
		auto       left    = source_bytes(aLeft);
		auto       right   = source_bytes(aRight);
		const auto outcome = core::compute_file_diff({
			.Key        = comparison_key(options, aSeed),
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
		window.publish(every_primary_command_offered());
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
		// Re-measured for the composed window rather than kept. 820 was measured
		// on a shell with no review header; the header takes up to 220, and at
		// the bottom of the fixture a shorter viewport shows a trailing page with
		// no correspondence in it at all. The assertion is unchanged -- the
		// window it is asserted on is the composed one now.
		window.resize(window.compact_width_threshold() + 320, 820 + 220);
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

		window.resize(window.width() - 200, 700 + 220);
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

	/// \brief Reads one canonical source line's exact text out of the result.
	///
	/// \details The line number always arrives from the canonical RFC-BPR-0004
	/// mapping. Nothing here decides which line a selection maps to; this only
	/// resolves the text the immutable decoded document already holds at it.
	[[nodiscard]] auto canonical_line_text(
		const std::optional<core::decoded_text_document>& aDocument,
		const std::uint32_t                               aLine) -> QString
	{
		expect(aDocument.has_value() && aLine < aDocument->Lines.size(),
			   "the canonical mapping names a line the immutable document holds");
		const auto& span = aDocument->Lines[aLine].Content;
		return QString::fromStdString(std::string{
			aDocument->Bytes.begin() + static_cast<std::ptrdiff_t>(span.Begin),
			aDocument->Bytes.begin() + static_cast<std::ptrdiff_t>(span.End) });
	}

	/// \brief Names the rows one side owes for the canonical selected detail.
	///
	/// \details Empty means the canonical mapping gave this side an insertion or
	/// deletion boundary rather than source lines, which is a real shape rather
	/// than a missing answer.
	[[nodiscard]] auto canonical_side_rows(
		const core::computed_file_diff& aDiff,
		const core::line_detail_input&  aDetail,
		const core::diff_side           aSide) -> QStringList
	{
		const auto& document = aSide == core::diff_side::left
			? aDiff.LeftDocument
			: aDiff.RightDocument;
		const auto& mapped   = aSide == core::diff_side::left
			? aDetail.LeftLine
			: aDetail.RightLine;
		if (mapped.has_value()) {
			return { canonical_line_text(document, mapped->Line) };
		}
		if (
			aDetail.OpposingSpan.has_value()
			&& aDetail.OpposingSpan->Side == aSide) {
			auto rows = QStringList{ };
			for (auto line = aDetail.OpposingSpan->Lines.Begin;
				 line < aDetail.OpposingSpan->Lines.End;
				 ++line) {
				rows.push_back(canonical_line_text(document, line));
			}
			return rows;
		}
		return { };
	}

	/// \brief Finds one contiguous run of rows at or below a starting row.
	[[nodiscard]] auto run_index(
		const QStringList& aRows,
		const QStringList& aRun,
		const qsizetype    aFrom) -> std::optional<qsizetype>
	{
		if (aRun.isEmpty()) {
			return std::nullopt;
		}
		for (auto start = std::max<qsizetype>(aFrom, 0);
			 start + aRun.size() <= aRows.size();
			 ++start) {
			auto matched = true;
			for (auto offset = qsizetype{ 0 }; offset < aRun.size(); ++offset) {
				if (aRows[start + offset] != aRun[offset]) {
					matched = false;
					break;
				}
			}
			if (matched) {
				return start;
			}
		}
		return std::nullopt;
	}

	/// \brief Re-checks that the lower detail pane owns the whole central width.
	///
	/// \details "Central" is the region between the two review drawers, which is
	/// the workbench centre itself. Agreeing with the comparison sub-region would
	/// not satisfy the claim, so the sub-region is measured too and must be
	/// strictly narrower.
	[[nodiscard]] auto observe_full_central_width(
		qt::workbench_window&  aWindow,
		const std::string_view aStage) -> int
	{
		auto* const surface = aWindow.comparison_surface();
		auto* const detail  = surface->detail_pane();
		auto* const centre  = aWindow.findChild<QWidget*>(
			QStringLiteral("workbenchCentralSurface"));
		expect(centre != nullptr && detail != nullptr,
			   message_for("the shell names its centre and its lower detail pane",
						   aStage));
		expect(detail->isVisible(),
			   message_for("the lower detail pane is part of the shown workbench",
						   aStage));
		const auto detailLeft = detail->mapTo(&aWindow, QPoint{ 0, 0 }).x();
		const auto centreLeft = centre->mapTo(&aWindow, QPoint{ 0, 0 }).x();
		expect(detailLeft == centreLeft && detail->width() == centre->width(),
			   message_for("the lower detail pane spans the whole central width",
						   aStage));
		expect(detail->width() > surface->comparison_splitter()->width(),
			   message_for("the lower detail pane is wider than the comparison "
						   "sub-region it sits under",
						   aStage));
		expect(detail->width() > surface->left_pane()->width()
				   && detail->width() > surface->right_pane()->width(),
			   message_for("the lower detail pane is wider than either source "
						   "pane",
						   aStage));
		for (auto* const drawer : { static_cast<QWidget*>(aWindow.files_drawer()),
									static_cast<QWidget*>(
										aWindow.conversations_drawer()) }) {
			if (!drawer->isVisible()) {
				continue;
			}
			const auto drawerLeft = drawer->mapTo(&aWindow, QPoint{ 0, 0 }).x();
			expect(drawerLeft >= detailLeft + detail->width()
					   || drawerLeft + drawer->width() <= detailLeft,
				   message_for("the lower detail pane never overlaps an open "
							   "review drawer",
							   aStage));
		}
		return detail->width();
	}

	void test_the_lower_detail_pane_is_full_width_line_over_line()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);
		expect(diff.Alignment.Groups.size() >= 3U,
			   "the fixture carries several canonical change groups");

		qt::workbench_window window;
		window.resize(window.compact_width_threshold() + 320, 820);
		window.show();
		process_events();

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window.publish(opened);
		process_events();

		auto* const surface = publish_fixture(window, diff);
		auto* const detail  = surface->detail_pane();
		expect(detail != nullptr && surface->isAncestorOf(detail),
			   "the lower detail pane is RFC-BPR-0004's composed detail surface "
			   "rather than a second pane the shell invented");

		const auto openWidth = observe_full_central_width(window, "drawers open");

		auto withoutFiles    = application::workbench_projection{ };
		withoutFiles.Drawers = drawer_states(
			core::workbench_drawer_state::collapsed,
			core::workbench_drawer_state::expanded);
		window.publish(withoutFiles);
		process_events();
		expect(observe_full_central_width(window, "file drawer collapsed")
				   >= openWidth + 320,
			   "collapsing the file drawer gives its whole width to the lower "
			   "detail pane");
		window.publish(opened);
		process_events();
		expect(observe_full_central_width(window, "file drawer reopened")
				   == openWidth,
			   "reopening the file drawer returns its width and leaves the lower "
			   "detail pane spanning the centre");

		auto withoutConversations    = application::workbench_projection{ };
		withoutConversations.Drawers = drawer_states(
			core::workbench_drawer_state::expanded,
			core::workbench_drawer_state::collapsed);
		window.publish(withoutConversations);
		process_events();
		expect(observe_full_central_width(window, "conversation drawer collapsed")
				   >= openWidth + 260,
			   "collapsing the conversation drawer gives its whole width to the "
			   "lower detail pane");
		window.publish(opened);
		process_events();
		expect(observe_full_central_width(window, "conversation drawer reopened")
				   == openWidth,
			   "reopening the conversation drawer returns its width and leaves "
			   "the lower detail pane spanning the centre");

		// Every canonical group is walked through RFC-BPR-0004's own change
		// navigation, and every row the pane shows is checked against the mapping
		// the core resolved for that exact selection.
		auto paired      = std::size_t{ 0U };
		auto bounded     = std::size_t{ 0U };
		auto lastEarlier = QStringList{ };
		auto lastLater   = QStringList{ };
		auto previous    = detail->toPlainText();
		for (std::size_t index = 0U; index < diff.Alignment.Groups.size();
			 ++index) {
			if (index != 0U) {
				surface->next_change_button()->click();
				process_events();
			}
			const auto& selection = surface->selection();
			expect(selection.has_value()
					   && selection->Group == diff.Alignment.Groups[index].Id,
				   "change navigation selects each canonical group in canonical "
				   "order");
			const auto mapping = core::detail_for_selection(diff, *selection);
			expect(mapping.Reason == core::semantic_lookup_reason::none
					   && mapping.Group == selection->Group,
				   "the canonical mapping resolves the selected group");
			const auto rows        = detail->toPlainText().split(QLatin1Char('\n'));
			const auto earlierRows = canonical_side_rows(
				diff, mapping, core::diff_side::left);
			const auto laterRows = canonical_side_rows(
				diff, mapping, core::diff_side::right);
			if (index == 1U) {
				expect(detail->toPlainText() != previous,
					   "selecting another change updates the lower detail pane");
			}
			if (earlierRows.isEmpty() || laterRows.isEmpty()) {
				const auto& present = earlierRows.isEmpty() ? laterRows
															: earlierRows;
				expect(run_index(rows, present, 0).has_value(),
					   "a boundary-shaped group still shows its canonically "
					   "mapped source rows");
				expect(mapping.OpposingBoundary.has_value(),
					   "the side without source rows is a canonical insertion or "
					   "deletion boundary");
				++bounded;
				continue;
			}
			const auto earlierAt = run_index(rows, earlierRows, 0);
			expect(earlierAt.has_value(),
				   "the lower pane shows the canonically mapped earlier rows");
			const auto laterAt = run_index(
				rows, laterRows, *earlierAt + earlierRows.size());
			expect(laterAt.has_value(),
				   "the lower pane shows the canonically mapped later rows below "
				   "the earlier rows, line over line");
			lastEarlier = earlierRows;
			lastLater   = laterRows;
			++paired;
		}
		expect(paired >= 2U && bounded >= 1U,
			   "the walk covered paired-line groups and boundary-shaped groups");
		expect(!lastEarlier.isEmpty() && !lastLater.isEmpty(),
			   "the walk ended with retained canonical rows to clear");

		surface->clear();
		process_events();
		const auto cleared = detail->toPlainText().split(QLatin1Char('\n'));
		expect(!surface->selection().has_value(),
			   "clearing the comparison clears the semantic selection");
		expect(!run_index(cleared, lastEarlier, 0).has_value()
				   && !run_index(cleared, lastLater, 0).has_value(),
			   "a cleared selection leaves no selected rows in the lower detail "
			   "pane");
		static_cast<void>(observe_full_central_width(window, "selection cleared"));
		window.hide();
	}

	/// \brief Builds one comparison whose later side has a chosen line count.
	///
	/// \details The earlier side keeps six retained anchor lines and the later
	/// side repeats a unique body between them, so every non-equal shape is a
	/// pure insertion and the later document reaches `6 * (aBodyLines + 1)`
	/// lines. Growing both sides together is not available here:
	/// `core::diff_compute_limits` caps the primary alignment matrix at sixteen
	/// million cells, so a comparison whose two sides are both enormous is
	/// refused by the bounded compute budget rather than materialized. A very
	/// large added file is the largest source the workbench can ever be asked
	/// to present through its real public path.
	[[nodiscard]] auto scaled_source_pair(const std::uint32_t aBodyLines)
		-> std::pair<std::string, std::string>
	{
		constexpr auto anchors = std::uint32_t{ 6U };
		auto           left    = std::string{ };
		auto           right   = std::string{ };
		right.reserve(static_cast<std::size_t>(anchors)
					  * (static_cast<std::size_t>(aBodyLines) + 1U) * 24U);
		for (std::uint32_t anchor = 0U; anchor < anchors; ++anchor) {
			for (std::uint32_t body = 0U; body < aBodyLines; ++body) {
				right += "inserted-";
				right += std::to_string(anchor);
				right += '-';
				right += std::to_string(body);
				right += '\n';
			}
			const auto retained = "retained-" + std::to_string(anchor) + '\n';
			left += retained;
			right += retained;
		}
		return { std::move(left), std::move(right) };
	}

	/// \brief Counts every live widget in one subtree, at any depth.
	///
	/// \details `findChildren` is recursive and untyped by role, so this is the
	/// complete retained widget tree beneath the subject: toolbars, tool
	/// buttons, the overflow menu, drawer panels, item views, splitter handles,
	/// the source panes, their viewports, and their scrollbars. A per-source-row
	/// widget layer anywhere beneath the subject would be counted here.
	[[nodiscard]] auto widget_count(const QWidget& aSubject) -> std::size_t
	{
		return static_cast<std::size_t>(aSubject.findChildren<QWidget*>().size());
	}

	/// \brief Records what one sized publication actually retained and cost.
	struct virtualized_observation
	{
		std::uint32_t LeftLines           = 0U;
		std::uint32_t RightLines          = 0U;
		std::size_t   WindowWidgets       = 0U;
		std::size_t   SurfaceWidgets      = 0U;
		std::size_t   PaneWidgets         = 0U;
		std::size_t   VisibleLinks        = 0U;
		std::uint64_t ScrollRebuilds      = 0U;
		int           ScrollTravel        = 0;
		qint64        PublishMilliseconds = 0;
		qint64        ScrollMilliseconds  = 0;
	};

	/// \brief Publishes one sized comparison and measures the composed surface.
	///
	/// \details Only the presentation cost is timed. The immutable comparison is
	/// computed first, so the recorded interval is the accepted publication and
	/// the geometry work the composed workbench then does, not the core diff.
	[[nodiscard]] auto observe_sized_publication(
		qt::workbench_window& aWindow,
		const std::uint32_t   aBodyLines,
		const std::uint8_t    aSeed) -> virtualized_observation
	{
		const auto [leftText, rightText] = scaled_source_pair(aBodyLines);
		const auto diff                  = computed_comparison(leftText, rightText, aSeed);
		expect(diff.Alignment.Content == core::diff_content_state::text,
			   "the sized fixture stays inside the bounded compute budget");

		auto* const   surface = aWindow.comparison_surface();
		QElapsedTimer publishTimer;
		publishTimer.start();
		static_cast<void>(publish_fixture(aWindow, diff));
		const auto published = publishTimer.elapsed();

		auto* const left  = surface->left_pane();
		auto* const right = surface->right_pane();
		expect(left->toPlainText() == QString::fromStdString(leftText)
				   && right->toPlainText() == QString::fromStdString(rightText),
			   "each composed pane holds exactly its own fixture source");
		expect(document_rows(*left) == diff.Alignment.LeftLineCount
				   && document_rows(*right) == diff.Alignment.RightLineCount,
			   "each composed pane really holds every source line of this size");

		auto* const   shared   = surface->shared_scrollbar();
		const auto    rebuilds = surface->statistics().GeometryRebuilds;
		QElapsedTimer scrollTimer;
		scrollTimer.start();
		shared->setValue(shared->maximum());
		process_events();
		const auto scrolled = scrollTimer.elapsed();

		// Counted after the scroll reaches the end of the document, so a row
		// widget created lazily for newly revealed source would be counted.
		return {
			.LeftLines      = diff.Alignment.LeftLineCount,
			.RightLines     = diff.Alignment.RightLineCount,
			.WindowWidgets  = widget_count(aWindow),
			.SurfaceWidgets = widget_count(*surface),
			.PaneWidgets    = widget_count(*right),
			.VisibleLinks   = surface->link_map()->visible_links().size(),
			.ScrollRebuilds = surface->statistics().GeometryRebuilds
				- rebuilds,
			.ScrollTravel        = shared->maximum() - shared->minimum(),
			.PublishMilliseconds = published,
			.ScrollMilliseconds  = scrolled,
		};
	}

	void test_a_large_file_leaves_the_widget_count_bounded()
	{
		qt::workbench_window window;
		window.resize(window.compact_width_threshold() + 320, 820);
		window.show();
		process_events();

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window.publish(opened);
		process_events();

		// The whole shell is already assembled and shown, so nothing counted
		// below is a widget the window merely had not built yet.
		const auto assembled = widget_count(window);
		expect(assembled > 0U,
			   "the assembled workbench retains a live widget tree to count");

		const auto small  = observe_sized_publication(window, 9U, 1U);
		const auto medium = observe_sized_publication(window, 999U, 2U);
		const auto large  = observe_sized_publication(window, 9'999U, 3U);

		expect(small.RightLines == 60U && medium.RightLines == 6'000U
				   && large.RightLines == 60'000U,
			   "the harness presents sixty, six thousand, and sixty thousand "
			   "source lines through the same composed workbench");
		expect(medium.RightLines >= 100U * small.RightLines
				   && large.RightLines >= 1'000U * small.RightLines,
			   "the reviewed source grows by orders of magnitude, not by a "
			   "constant");
		expect(large.ScrollTravel > 0,
			   "the sixty-thousand-line comparison really is longer than its "
			   "viewport");

		// The ratio is the claim. A harness that merely finished rendering a
		// large file would prove nothing about how the surface is built.
		expect(small.WindowWidgets == assembled
				   && medium.WindowWidgets == assembled
				   && large.WindowWidgets == assembled,
			   "a file a thousand times longer creates no additional widget "
			   "anywhere in the workbench window");
		expect(small.SurfaceWidgets == medium.SurfaceWidgets
				   && medium.SurfaceWidgets == large.SurfaceWidgets,
			   "the composed comparison surface retains one widget tree at every "
			   "source length");
		expect(small.PaneWidgets == medium.PaneWidgets
				   && medium.PaneWidgets == large.PaneWidgets,
			   "the source pane presenting sixty thousand lines has exactly the "
			   "child widgets it has presenting sixty");
		expect(100U * large.WindowWidgets < large.RightLines,
			   "the whole retained widget tree stays orders of magnitude smaller "
			   "than the source it presents");
		expect(small.VisibleLinks == medium.VisibleLinks
				   && medium.VisibleLinks == large.VisibleLinks,
			   "the painted correspondences follow the canonical change count "
			   "rather than the source length");
		expect(large.ScrollRebuilds == 0U,
			   "scrolling a sixty-thousand-line comparison rebuilds no measured "
			   "geometry");

		// Factual scale context for this machine and run. Elapsed time is
		// recorded, never asserted: one machine's timing is not a frame-rate
		// guarantee.
		std::cout << "workbench-window-virtualization:"
				  << " platform="
				  << QSysInfo::prettyProductName().toStdString()
				  << " architecture="
				  << QSysInfo::currentCpuArchitecture().toStdString()
				  << " qt=" << qVersion()
				  << " scale=" << window.devicePixelRatioF()
				  << " counted=window/surface/right-pane"
				  << " lines=" << small.RightLines << '/' << medium.RightLines
				  << '/' << large.RightLines
				  << " window-widgets=" << small.WindowWidgets << '/'
				  << medium.WindowWidgets << '/' << large.WindowWidgets
				  << " surface-widgets=" << small.SurfaceWidgets << '/'
				  << medium.SurfaceWidgets << '/' << large.SurfaceWidgets
				  << " pane-widgets=" << small.PaneWidgets << '/'
				  << medium.PaneWidgets << '/' << large.PaneWidgets
				  << " visible-links=" << small.VisibleLinks << '/'
				  << medium.VisibleLinks << '/' << large.VisibleLinks
				  << " publish-ms=" << small.PublishMilliseconds << '/'
				  << medium.PublishMilliseconds << '/'
				  << large.PublishMilliseconds
				  << " scroll-ms=" << small.ScrollMilliseconds << '/'
				  << medium.ScrollMilliseconds << '/'
				  << large.ScrollMilliseconds << '\n';
		window.hide();
	}

	// --- Composed review-history fixtures -----------------------------------
	//
	// RFC-BPR-0003 decides review-history identity, legality, and wording, and
	// its own suite proves those rules. These fixtures build the same
	// projections so this suite can prove the composition instead: that the
	// shell exposes the control, drives it through the window's projection
	// port, states its decided text, and places it in one declared focus order.

	/// \brief Builds one complete forty-character fixture commit OID text.
	///
	/// \details The characters vary along the whole string rather than
	/// repeating, so an abbreviation, a bare prefix, or an elided middle is a
	/// different value instead of a coincidental match.
	[[nodiscard]] auto epoch_commit_text(const std::uint8_t aSeed) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       text   = std::string{ };
		text.reserve(40U);
		for (std::size_t index = 0U; index < 40U; ++index) {
			text.push_back(digits[((index * 7U) + aSeed) % digits.size()]);
		}
		return text;
	}

	[[nodiscard]] auto epoch_commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(epoch_commit_text(aSeed));
		expect(parsed.has_value() && parsed->valid(),
			   "the fixture commit OID parses into a live identity");
		return *parsed;
	}

	[[nodiscard]] constexpr auto epoch_base_seed(const std::int64_t aOrdinal)
		-> std::uint8_t
	{
		return static_cast<std::uint8_t>(2 * aOrdinal);
	}

	[[nodiscard]] constexpr auto epoch_head_seed(const std::int64_t aOrdinal)
		-> std::uint8_t
	{
		return static_cast<std::uint8_t>((2 * aOrdinal) + 1);
	}

	[[nodiscard]] auto epoch_scope() -> core::pull_request_authority_scope_id
	{
		return opaque<core::pull_request_authority_scope_id>(61U);
	}

	[[nodiscard]] auto epoch_catalog() -> core::epoch_catalog_identity
	{
		return {
			.Incarnation = opaque<core::epoch_catalog_incarnation_id>(62U),
			.Revision    = core::epoch_catalog_revision{ 7 },
		};
	}

	[[nodiscard]] auto epoch_authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 12U },
			.ProfileRevision = core::github_profile_revision{ 22U },
			.Host            = core::git_host_id{ 32U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(63U),
			.Account         = core::github_account_id{ 42U },
			.Viewer          = core::github_viewer_actor_id{ 52U },
			.Repository      = core::repository_id{ 62U },
			.Cache           = opaque<core::repository_cache_scope_id>(64U),
			.PullRequest     = core::pull_request_id{ 72U },
			.Number          = core::pull_request_number{ 82U },
			.Scope           = epoch_scope(),
		};
	}

	[[nodiscard]] auto epoch_row(const std::int64_t aOrdinal)
		-> application::epoch_row_projection
	{
		return {
			.Id = {
				.Authority   = epoch_scope(),
				.Incarnation = epoch_catalog().Incarnation,
				.Ordinal     = core::epoch_ordinal{ aOrdinal },
			},
			.Snapshot              = core::provider_snapshot_id{ static_cast<std::uint64_t>(aOrdinal) },
			.IdentityGeneration    = core::github_identity_generation{ 3U },
			.PullRequestGeneration = core::request_generation{ 4U },
			.BaseBranch            = opaque<core::base_branch_id>(65U),
			.Base                  = epoch_commit(epoch_base_seed(aOrdinal)),
			.Head                  = epoch_commit(epoch_head_seed(aOrdinal)),
			.Cause                 = {
				.Origin   = core::epoch_origin::live_observation,
				.Base     = core::epoch_base_transition::unchanged,
				.Head     = core::epoch_head_transition::descendant,
				.Material = core::epoch_material_relation::different,
				.Primary  = core::epoch_primary_cause::author_upload,
			},
			.ObservedAt       = core::observed_utc_milliseconds{ 1'700'000'000'000LL + aOrdinal },
			.BaseAvailability = core::anchor_availability::available,
			.HeadAvailability = core::anchor_availability::available,
		};
	}

	[[nodiscard]] auto epoch_anchor(
		const application::epoch_row_projection& aRow,
		const core::comparison_anchor_kind       aKind) -> core::comparison_anchor
	{
		return {
			.Identity = {
				.Authority   = aRow.Id.Authority,
				.Incarnation = aRow.Id.Incarnation,
				.Kind        = aKind,
				.Epoch       = aRow.Id.Ordinal,
			},
			.Commit       = aKind == core::comparison_anchor_kind::epoch_base ? aRow.Base : aRow.Head,
			.Availability = core::anchor_availability::available,
		};
	}

	/// \brief Builds one bounded review-history page publication.
	[[nodiscard]] auto history_page_update(
		const core::epoch_history_page_phase           aPhase,
		std::vector<application::epoch_row_projection> aRows)
		-> application::epoch_presentation_update
	{
		const auto identity = epoch_catalog();
		return {
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = identity,
				.HistoryPage = application::epoch_history_page_projection{
					.Phase      = aPhase,
					.Catalog    = identity,
					.Navigation = core::epoch_history_navigation::newest,
					.Limit      = core::maximum_epoch_history_page_size,
					.Rows       = std::move(aRows),
				},
			},
		};
	}

	[[nodiscard]] auto retained_epochs(
		std::vector<application::epoch_row_projection> aRows)
		-> application::epoch_presentation_update
	{
		return history_page_update(core::epoch_history_page_phase::loaded,
								   std::move(aRows));
	}

	[[nodiscard]] auto enabled_epoch_commands() -> core::epoch_command_delta
	{
		const auto enabled = [](const core::epoch_command_id aId) {
			return core::epoch_command_projection{
				.Id             = aId,
				.Enabled        = true,
				.DisabledReason = core::epoch_reason_code::none,
			};
		};
		return {
			.Commands = {
				enabled(core::epoch_command_id::refresh_epochs),
				enabled(core::epoch_command_id::select_comparison),
				enabled(core::epoch_command_id::prune_selected_epochs),
				enabled(core::epoch_command_id::delete_repository_cache),
				enabled(core::epoch_command_id::cancel_epoch_refresh),
				enabled(core::epoch_command_id::cancel_epoch_maintenance),
			},
		};
	}

	/// \brief Publishes one exactly resolved retained comparison pair.
	[[nodiscard]] auto resolved_pair_update(const core::comparison_pair& aPair)
		-> application::epoch_presentation_update
	{
		const auto selection = core::explicit_selection(aPair.Left.Identity,
														aPair.Right.Identity);
		return {
			.Transition = application::epoch_transition_projection{
				.Changed = core::epoch_changed_domain::selection
					| core::epoch_changed_domain::comparison
					| core::epoch_changed_domain::authority
					| core::epoch_changed_domain::status
					| core::epoch_changed_domain::commands,
				.Authority = application::epoch_authority_projection{
					.Authority  = epoch_authority(),
					.Generation = core::epoch_generation{ 8U },
				},
				.Selection = core::comparison_selection_delta{
					.Selection = selection,
				},
				.Comparison = application::epoch_comparison_projection{
					.Selection        = selection,
					.Pair             = aPair,
					.Generation       = core::comparison_generation{ 9U },
					.Revision         = core::comparison_revision{ 3 },
					.AvailableForDiff = true,
				},
				.Status = core::epoch_status_delta{
					.Status = core::epoch_status::ready,
				},
				.Commands = enabled_epoch_commands(),
			},
		};
	}

	[[nodiscard]] auto anchor_intent(const application::epoch_intent& aIntent)
		-> const core::select_comparison_anchor_intent*
	{
		return std::get_if<core::select_comparison_anchor_intent>(&aIntent);
	}

	void test_composed_epoch_selectors_show_exact_retained_epochs()
	{
		std::vector<application::epoch_intent> epochs;
		qt::workbench_window                   window;

		auto* const selectors = window.epoch_selectors();
		expect(selectors != nullptr
				   && selectors->parentWidget()
					   == window.findChild<QWidget*>(
						   QStringLiteral("workbenchCentralSurface")),
			   "the composed epoch selectors sit in the workbench centre above "
			   "the comparison panes");
		expect(window.findChild<QWidget*>(
				   QStringLiteral("workbenchEpochSelectorPlaceholder"))
				   == nullptr,
			   "no empty selector placeholder survives beside the real control");
		selectors->set_dispatch(
			[&epochs](application::epoch_intent aIntent) {
				epochs.push_back(std::move(aIntent));
			});

		const auto first  = epoch_row(1);
		const auto second = epoch_row(2);
		const auto third  = epoch_row(3);
		window.publish(retained_epochs({ first, second, third }));
		expect(window.statistics().EpochUpdates == 1U,
			   "a review-history update reaches the shell through its own "
			   "projection port");

		auto* const left  = selectors->left_selector();
		auto* const right = selectors->right_selector();
		expect(left != nullptr && right != nullptr && left != right
				   && left->model() == selectors->history_model()
				   && right->model() == selectors->history_model(),
			   "the shell offers two independent selectors over one retained "
			   "review history");
		expect(left->count() == 6 && right->count() == 6,
			   "every retained epoch offers its base and its head anchor to both "
			   "selectors");
		expect(left->itemText(2)
					   == QStringLiteral(
						   "Epoch 2 · Base — Author upload — Available")
				   && left->itemText(3)
					   == QStringLiteral(
						   "Epoch 2 · Head — Author upload — "
						   "Available"),
			   "each selector entry labels the exact retained epoch and anchor");

		// An arbitrary retained pair: the oldest base against the newest head.
		const auto pair = core::comparison_pair{
			.Left  = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
			.Right = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
		};
		window.publish(resolved_pair_update(pair));

		auto* const leftOid  = selectors->left_oid_editor();
		auto* const rightOid = selectors->right_oid_editor();
		const auto  expectedLeft
			= QString::fromStdString(epoch_commit_text(epoch_base_seed(1)));
		const auto expectedRight
			= QString::fromStdString(epoch_commit_text(epoch_head_seed(3)));
		expect(expectedLeft.size() == 40 && expectedRight.size() == 40
				   && expectedLeft != expectedRight,
			   "the fixture carries two different complete forty-character OIDs");
		expect(leftOid->text() == expectedLeft
				   && rightOid->text() == expectedRight,
			   "each selector states the exact complete commit OID of its "
			   "selected anchor");
		expect(leftOid->text() != expectedLeft.left(7)
				   && leftOid->text() != expectedLeft.left(8)
				   && leftOid->text() != expectedLeft.left(12)
				   && !leftOid->text().contains(QStringLiteral("…"))
				   && !leftOid->text().contains(QStringLiteral("...")),
			   "the exact OID is the whole value rather than an abbreviation or "
			   "an elision");

		leftOid->selectAll();
		rightOid->selectAll();
		expect(leftOid->isReadOnly() && rightOid->isReadOnly()
				   && leftOid->selectedText() == expectedLeft
				   && rightOid->selectedText() == expectedRight,
			   "the whole exact OID is selectable and copyable out of the shell "
			   "without being editable");

		// Independence: moving one selector asks for one side and leaves the
		// other side's decided anchor exactly where the core put it.
		epochs.clear();
		const auto rightBefore = right->currentIndex();
		left->setCurrentIndex(3);
		expect(epochs.size() == 1U,
			   "one selector gesture emits exactly one semantic intent");
		const auto* const leftIntent = anchor_intent(epochs.front());
		expect(leftIntent != nullptr
				   && leftIntent->Side == core::comparison_side::left
				   && leftIntent->Anchor.Kind
					   == core::comparison_anchor_kind::epoch_head
				   && leftIntent->Anchor.Epoch == core::epoch_ordinal{ 2 },
			   "the left selector asks for the exact arbitrary retained epoch it "
			   "shows");
		expect(right->currentIndex() == rightBefore
				   && rightOid->text() == expectedRight,
			   "moving the left selector moves neither the right selector nor its "
			   "exact OID");
		expect(left->currentIndex() == 0,
			   "the selector re-asserts the decided anchor rather than keeping a "
			   "selection presentation chose for itself");

		epochs.clear();
		right->setCurrentIndex(0);
		expect(epochs.size() == 1U,
			   "the second selector gesture also emits exactly one intent");
		const auto* const rightIntent = anchor_intent(epochs.front());
		expect(rightIntent != nullptr
				   && rightIntent->Side == core::comparison_side::right
				   && rightIntent->Anchor.Kind
					   == core::comparison_anchor_kind::epoch_base
				   && rightIntent->Anchor.Epoch == core::epoch_ordinal{ 1 },
			   "the right selector independently asks for any retained epoch, "
			   "including one earlier than the left side shows");

		// The reverse pair proves the two sides really are independent rather
		// than one ordered range presented twice.
		const auto reversed = core::comparison_pair{
			.Left  = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
			.Right = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
		};
		window.publish(resolved_pair_update(reversed));
		expect(leftOid->text() == expectedRight
				   && rightOid->text() == expectedLeft,
			   "an arbitrary reversed retained pair reaches both selectors "
			   "exactly");
	}

	void test_review_history_state_is_explicit_in_workbench_text()
	{
		qt::workbench_window window;
		const auto           inactive = window.status_text();
		expect(inactive
				   == QStringLiteral("Open a pull request to begin reviewing."),
			   "before any review-history update the shell states only its own "
			   "state");

		window.publish(retained_epochs({ }));
		const auto empty = window.status_text();
		window.publish(retained_epochs({ epoch_row(1), epoch_row(2) }));
		const auto retained = window.status_text();
		window.publish(
			history_page_update(core::epoch_history_page_phase::loading, { }));
		const auto loading = window.status_text();
		window.publish(
			history_page_update(core::epoch_history_page_phase::failed, { }));
		const auto failed = window.status_text();
		window.publish(
			history_page_update(core::epoch_history_page_phase::cancelled, { }));
		const auto cancelled = window.status_text();

		const std::array<QString, 5> states{
			empty, retained, loading, failed, cancelled
		};
		for (std::size_t outer = 0U; outer < states.size(); ++outer) {
			expect(!states[outer].isEmpty(),
				   "every review-history state reaches the shell as text");
			for (std::size_t inner = outer + 1U; inner < states.size(); ++inner) {
				expect(states[outer] != states[inner],
					   "loading, failure, cancellation, empty, and retained "
					   "review history each read differently in text rather "
					   "than differing only in colour or enabled state");
			}
		}
		expect(empty.contains(
				   QStringLiteral("No review epochs have been recorded.")),
			   "an empty review history says so in words");
		expect(loading.contains(QStringLiteral("Loading")),
			   "a loading review history says so in words");
		expect(failed.contains(QStringLiteral("could not be loaded")),
			   "a failed review-history load says so in words");
		expect(cancelled.contains(QStringLiteral("canceled")),
			   "a canceled review-history load says so in words");
		for (const auto& state : states) {
			expect(state.startsWith(inactive),
				   "the shell states one combined line: its own state first, "
				   "then the review-history state the composed control decided");
		}

		// One published change, one announcement, made by whichever control
		// decided it.
		const auto before        = window.statistics();
		const auto controlBefore = window.epoch_selectors()->statistics();
		window.publish(retained_epochs({ epoch_row(1) }));
		expect(window.statistics().EpochUpdates == before.EpochUpdates + 1U,
			   "the window observes each review-history update exactly once");
		expect(window.statistics().Announcements == before.Announcements
				   && window.epoch_selectors()->statistics().Announcements
					   > controlBefore.Announcements,
			   "a review-history change is announced once, by the control that "
			   "decided it, rather than twice");

		// The shell's own host failure stays distinguishable inside the same
		// combined line.
		window.fail({ .Failure
					  = application::workbench_host_failure::settings_unavailable });
		expect(window.status_text().contains(
				   QStringLiteral("The saved workbench layout is unavailable."))
				   && window.status_text() != cancelled,
			   "a workbench host failure is explicit in the same combined line "
			   "and does not read like a review-history state");
	}

	[[nodiscard]] auto accessible_names(const QList<QWidget*>& aStops)
		-> QStringList
	{
		auto names = QStringList{ };
		for (auto* const stop : aStops) {
			names.push_back(stop->accessibleName());
		}
		return names;
	}

	/// \brief Walks the live focus chain and reports the declared stops it met.
	///
	/// \details The chain is circular, so the walk starts at the first declared
	/// stop and stops when it returns there. Widgets outside the declared order
	/// are skipped, which is what makes this a relative-order check rather than
	/// a claim about every widget Qt happens to keep in the chain.
	[[nodiscard]] auto walked_focus_order(const QList<QWidget*>& aStops)
		-> QList<QWidget*>
	{
		auto walked = QList<QWidget*>{ };
		if (aStops.isEmpty()) {
			return walked;
		}
		auto* const start  = aStops.front();
		auto*       cursor = start;
		for (auto step = 0; step < 8'192; ++step) {
			cursor = cursor->nextInFocusChain();
			if (cursor == nullptr || cursor == start) {
				break;
			}
			if (aStops.contains(cursor)) {
				walked.push_back(cursor);
			}
		}
		return walked;
	}

	void test_focus_order_is_declared_named_and_walkable()
	{
		qt::workbench_window window;
		window.resize(window.compact_width_threshold() + 320, 820);
		window.show();
		process_events();

		const auto expanded = window.focus_order();
		expect(expanded.size() >= 12,
			   "the shell declares every interactive stop it owns");
		auto declared = QStringList{ };
		for (auto* const stop : expanded) {
			expect(stop != nullptr && stop->focusPolicy() != Qt::NoFocus,
				   "every declared focus stop can actually take keyboard focus");
			expect(!stop->accessibleName().isEmpty()
					   && !stop->accessibleDescription().isEmpty(),
				   "every interactive element carries an accessible name and an "
				   "accessible description");
			expect(!declared.contains(stop->accessibleName()),
				   "no two interactive elements answer to one accessible name");
			declared.push_back(stop->accessibleName());
		}
		expect(declared.contains(QStringLiteral("Left review epoch"))
				   && declared.contains(QStringLiteral("Right review epoch"))
				   && declared.contains(QStringLiteral("Exact left commit OID"))
				   && declared.contains(QStringLiteral("Exact right commit OID")),
			   "both epoch selectors and both exact OIDs are reachable stops in "
			   "the shell's declared focus order");
		expect(walked_focus_order(expanded) == expanded.mid(1),
			   "the live focus chain walks the declared order exactly");

		// No reachable focusable widget anywhere in the assembled window may
		// answer to a name another reachable focusable widget already uses.
		// Reachable is part of the rule rather than a convenience: Qt keeps a
		// hidden zero-size first splitter handle, which no keyboard or assistive
		// user can ever land on.
		auto seen = QStringList{ };
		for (auto* const widget : window.findChildren<QWidget*>()) {
			if (widget->focusPolicy() == Qt::NoFocus || !widget->isVisible()
				|| widget->accessibleName().isEmpty()) {
				continue;
			}
			expect(!seen.contains(widget->accessibleName()),
				   message_for("no accessible name is shared by two focusable "
							   "widgets in the assembled workbench",
							   (widget->accessibleName() + QStringLiteral(" on ")
								+ widget->metaObject()->className()
								+ QStringLiteral(" ") + widget->objectName())
								   .toStdString()));
			seen.push_back(widget->accessibleName());
		}

		// An ambiguous shortcut overload triggers neither action, so one key
		// sequence may never be offered twice in one window.
		auto sequences = QList<QKeySequence>{ };
		for (auto* const action : window.findChildren<QAction*>()) {
			if (action->shortcut().isEmpty()) {
				continue;
			}
			expect(!sequences.contains(action->shortcut()),
				   "no keyboard command is claimed twice in the assembled "
				   "workbench");
			sequences.push_back(action->shortcut());
		}
		expect(!sequences.contains(
				   window.navigation_overflow_button()->shortcut()),
			   "the compact overflow's own shortcut is not claimed by an action "
			   "as well");

		window.resize(window.minimumWidth(), 720);
		process_events();
		expect(window.toolbar_mode() == qt::workbench_toolbar_mode::compact,
			   "the narrow window really entered the compact overflow");
		const auto compact = window.focus_order();
		expect(compact.front() == window.navigation_overflow_button(),
			   "the compact overflow is the first interactive stop once primary "
			   "navigation moves into it");
		for (auto* const stop : compact) {
			expect(!stop->accessibleName().isEmpty()
					   && !stop->accessibleDescription().isEmpty(),
				   "the compact layout keeps every stop named and described");
		}
		expect(walked_focus_order(compact) == compact.mid(1),
			   "the compact focus chain walks its declared order exactly");

		window.resize(window.compact_width_threshold() + 320, 820);
		process_events();
		expect(window.toolbar_mode() == qt::workbench_toolbar_mode::expanded,
			   "widening the window restores top-level primary navigation");
		const auto restored = window.focus_order();
		expect(accessible_names(restored) == declared,
			   "returning to the wide layout restores the same declared focus "
			   "order, not the order the rebuilt toolbar buttons landed in");
		expect(walked_focus_order(restored) == restored.mid(1),
			   "the restored focus chain walks the declared order exactly");
		window.hide();
	}

	void test_receipt_actions_follow_decided_projection_and_clear_while_loading()
	{
		intent_log           log;
		qt::workbench_window window;
		window.set_receipt_dispatch(
			[&log](core::file_receipt_command aCommand) {
				log.Receipt.push_back(std::move(aCommand));
			});
		window.show();
		process_events();

		const auto receipt = core::file_receipt{
			.File     = core::file_identity_of("review.cpp"),
			.Material = file_material(73U),
		};
		window.publish(application::project_file_receipt_actions(receipt));
		process_events();
		expect(window.seen_action()->isEnabled()
				   && window.read_action()->isEnabled()
				   && window.accept_action()->isEnabled()
				   && window.exclude_action()->isEnabled(),
			   "the four compact receipt gestures use the application projection's legality");

		window.seen_action()->trigger();
		expect(log.Receipt == std::vector{ core::file_receipt_command{
				   .File     = receipt.File,
				   .Material = receipt.Material,
				   .Kind     = core::file_receipt_command_kind::mark_seen,
			   } },
			   "a receipt action emits the exact selected file, material, and semantic command");

		auto advanced     = receipt;
		advanced.Read     = core::file_read_state::seen;
		advanced.Accepted = true;
		window.publish(application::project_file_receipt_actions(advanced));
		process_events();
		expect(window.seen_action()->isChecked()
				   && !window.seen_action()->isEnabled()
				   && window.accept_action()->text().contains(
					   QStringLiteral("Revoke"))
				   && !window.exclude_action()->isEnabled()
				   && window.status_text().contains(
					   QStringLiteral("Private progress: seen, accepted")),
			   "accepted receipt projection decides checked state, inverse gesture, and status");

		window.clear_review_ledger();
		process_events();
		expect(!window.seen_action()->isEnabled()
				   && !window.read_action()->isEnabled()
				   && !window.accept_action()->isEnabled()
				   && !window.exclude_action()->isEnabled(),
			   "starting another selected diff clears the prior file's gesture surface");

		window.clear_review_ledger(true);
		process_events();
		expect(window.status_text().contains(
				   QStringLiteral("Private review progress is unavailable.")),
			   "a store-open obstruction is visible in text while the workbench remains live");
		window.hide();
	}

	void test_selection_synchronisation_and_restart_persistence()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);

		intent_log log;
		auto       window = std::make_unique<qt::workbench_window>(
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{
				[&log](const core::workbench_drawer aDrawer,
					   const std::uint32_t          aWidth) {
					log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
				} });
		window->resize(window->compact_width_threshold() + 320, 820);
		window->show();
		process_events();

		auto persisted    = both_files_enabled();
		persisted.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										  core::workbench_drawer_state::expanded);
		persisted.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window->publish(persisted);
		process_events();
		auto* const surface = publish_fixture(*window, diff);

		// Selection synchronisation: one gesture, one semantic selection, and
		// every composed region answers with that same selection.
		const auto beforeSelection = surface->selection();
		const auto beforeDetail    = window->detail_pane()->toPlainText();
		surface->next_change_button()->click();
		process_events();
		const auto& selection = surface->selection();
		expect(selection.has_value() && selection != beforeSelection,
			   "selecting the next change moves the one semantic selection");
		expect(window->detail_pane() == surface->detail_pane(),
			   "the shell's detail pane is the composed surface's own pane, so "
			   "one selection can never produce two answers");
		expect(window->detail_pane()->toPlainText() != beforeDetail,
			   "the lower detail pane follows the selection the composed surface "
			   "made");
		const auto links = surface->link_map()->visible_links();
		expect(std::ranges::any_of(
				   links,
				   [&selection](const qt::visible_diff_link& aLink) {
					   return aLink.Identity.Group == selection->Group;
				   }),
			   "the link map paints the same selected group the lower detail "
			   "pane shows");

		// Restart persistence: a drag reports the host-owned width, and a fresh
		// window built from the settings the host persisted restores it.
		auto* const splitter = window->findChild<QSplitter*>(
			QStringLiteral("workbenchCentralSplitter"));
		expect(splitter != nullptr, "the shell owns one central splitter");
		const auto sizes = splitter->sizes();
		log.Width.clear();
		splitter->setSizes({ 380, sizes[1] - 60, sizes[2] });
		process_events();
		expect(!log.Width.empty()
				   && log.Width.back().Drawer == core::workbench_drawer::files
				   && log.Width.back().Width == 380U,
			   "a resized drawer reports exactly the width the host must "
			   "persist");
		const auto persistedWidth = log.Width.back().Width;

		auto closing    = application::workbench_projection{ };
		closing.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										core::workbench_drawer_state::collapsed);
		window->publish(closing);
		process_events();
		window->hide();
		window.reset();

		qt::workbench_window restarted;
		restarted.resize(restarted.compact_width_threshold() + 320, 820);
		restarted.show();
		process_events();
		expect(restarted.files_drawer()->width() != static_cast<int>(persistedWidth)
				   || restarted.conversations_drawer()->isVisible(),
			   "a fresh shell starts without the persisted layout, so restoring "
			   "it is a real transition");

		auto reloaded    = both_files_enabled();
		reloaded.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										 core::workbench_drawer_state::collapsed);
		reloaded.Layout  = application::workbench_layout_delta{
			.FilesWidth         = persistedWidth,
			.ConversationsWidth = 260U,
		};
		restarted.publish(reloaded);
		process_events();
		expect(restarted.files_drawer()->width()
				   == static_cast<int>(persistedWidth),
			   "a restarted shell restores the drawer width the host persisted");
		expect(!restarted.conversations_drawer()->isVisible()
				   && !restarted.conversations_drawer_toggle_action()->isChecked(),
			   "a restarted shell restores the persisted drawer state as well as "
			   "its width");
		restarted.hide();
	}

	void test_defect_queue_navigation_and_semantic_dispatch()
	{
		qt::workbench_window window;
		auto                 commands        = std::vector<core::defect_command>{ };
		auto                 classifications = std::vector<std::string>{ };
		window.set_defect_dispatch(
			[&commands](core::defect_command aCommand) {
				commands.push_back(std::move(aCommand));
			});
		window.set_defect_classification_dispatch(
			[&classifications](std::string aIdentity) {
				classifications.push_back(std::move(aIdentity));
			});
		window.conversation_model()->apply({
			application::github_conversation_projection{
				.Id   = "review-thread:thread-2",
				.Kind = application::github_conversation_kind::review_thread,
				.Path = "review.cpp",
				.Line = 22U,
			},
			application::github_conversation_projection{
				.Id   = "review-thread:file-thread",
				.Kind = application::github_conversation_kind::review_thread,
				.Path = "review.cpp",
			},
			application::github_conversation_projection{
				.Id   = "issue-comment:comment-1",
				.Kind = application::github_conversation_kind::issue_comment,
			},
		});
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id(
				"review-thread:thread-2"));
		window.classify_defect_action()->trigger();
		expect(
			classifications
				== std::vector<std::string>{ "review-thread:thread-2" },
			"classification emits the exact selected conversation identity");
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id(
				"review-thread:file-thread"));
		expect(!window.classify_defect_action()->isEnabled(),
			   "a file-level thread cannot fabricate a source-line defect anchor");
		window.classify_defect_action()->trigger();
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id(
				"issue-comment:comment-1"));
		expect(!window.classify_defect_action()->isEnabled(),
			   "a non-thread conversation cannot enter local defect classification");
		window.classify_defect_action()->trigger();
		expect(classifications.size() == 1U,
			   "unsupported conversation kinds emit no classification intent");

		window.present_defects({
			defect_history(2U, core::defect_anchor_availability::outdated),
			defect_history(1U, core::defect_anchor_availability::available),
		});
		expect(
			window.defect_count() == 2U
				&& window.defect_count_label()->text().contains(QStringLiteral("1/2"))
				&& window.defect_card()->shown_defect()->Id == core::defect_id{ 1U },
			"the queue is stable-ID ordered and exposes an exact count");
		window.next_defect_action()->trigger();
		expect(
			window.defect_card()->shown_defect()->Id == core::defect_id{ 2U }
				&& window.defect_card()->context_label()->text().contains(
					QStringLiteral("unavailable")),
			"navigation reaches the next retained history without losing context status");
		window.defect_card()->verify_action()->trigger();
		expect(
			commands.size() == 1U
				&& commands.front().Id == core::defect_id{ 2U }
				&& commands.front().Kind == core::defect_command_kind::verify,
			"the visible card forwards one explicit lifecycle intent");
		expect(
			window.defect_card()->shown_defect()->State
				== core::defect_state::needs_verification,
			"the window does not verify before a new accepted projection");
	}

	void test_queue_rule_gestures_wait_for_the_accepted_projection()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_rule_intent>{ };
		window.set_queue_rule_dispatch(
			[&intents](application::queue_rule_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		window.publish({ .Status = application::queue_landing_status::ready });
		window.queue_rule_kind_selector()->setCurrentIndex(2);
		window.queue_rule_pattern_editor()->setText(QStringLiteral("src/parser"));
		window.queue_rule_exclusion()->setChecked(true);
		window.add_queue_rule_action()->trigger();
		const auto rule = core::observer_rule{
			.Kind      = core::observer_rule_kind::path,
			.Pattern   = "src/parser",
			.Excluding = true,
		};
		expect(
			intents == std::vector{ application::queue_rule_intent{ .Action = application::queue_rule_action::add, .Rule = rule } },
			"the real add action emits one exact semantic local-rule intent");
		expect(window.queue_rule_kind_selector()->count() == 3
				   && window.queue_landing()->rowCount() == 0,
			   "Qt performs no store or enrollment effect before projection");

		const auto membership = core::queue_membership{
			.MatchedRule = true,
			.Rule        = core::observer_rule{
				.Kind = core::observer_rule_kind::path, .Pattern = "src/parser" }
		};
		window.publish({
			.Status = application::queue_landing_status::ready,
			.Rules  = { rule },
			.Items  = { core::queue_item{
				.Ref = core::pull_request_ref{
					.Host = "github.com", .Account = "reviewer", .Repository = "octo/app", .Number = 7 },
				.Membership = membership,
				.Bucket     = core::attention_bucket::quiet,
			} },
		});
		expect(window.queue_landing()->rowCount() == 1,
			   "an accepted projection inserts the matched review");
		window.remove_queue_rule_action()->trigger();
		expect(intents.size() == 2U
				   && intents.back() == application::queue_rule_intent{ .Action = application::queue_rule_action::remove, .Rule = rule },
			   "the real remove action emits the exact selected persisted rule");
		expect(window.queue_landing()->rowCount() == 1,
			   "the row remains until the host accepts removal");
		window.publish({ .Status = application::queue_landing_status::ready });
		expect(window.queue_landing()->rowCount() == 0,
			   "the accepted cut-match projection removes the row");
	}

	void test_queue_rule_selection_survives_unrelated_projection()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_rule_intent>{ };
		window.set_queue_rule_dispatch(
			[&intents](application::queue_rule_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		const auto first = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		const auto second = core::observer_rule{
			.Kind = core::observer_rule_kind::author, .Pattern = "octocat"
		};
		const auto projection = application::queue_landing_projection{
			.Status = application::queue_landing_status::ready,
			.Rules  = { first, second },
		};
		window.publish(projection);
		auto* const selector = window.findChild<QComboBox*>(
			QStringLiteral("observerRuleSelector"));
		expect(selector != nullptr && selector->count() == 2,
			   "two exact saved rules are visible");
		selector->setCurrentIndex(1);
		window.publish(projection);
		expect(selector->currentIndex() == 1,
			   "an unrelated projection preserves the exact selected rule");
		window.remove_queue_rule_action()->trigger();
		expect(
			intents == std::vector{ application::queue_rule_intent{
				.Action = application::queue_rule_action::remove,
				.Rule   = second,
			} },
			"Remove still targets the rule the reviewer selected");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_all_four_primary_actions_follow_one_core_projection();
	test_every_primary_action_translates_to_a_semantic_intent();
	test_action_state_follows_core_command_projections();
	test_compact_overflow_preserves_keyboard_access();
	test_shell_accessibility_status_and_scale();
	test_drawer_toggles_follow_core_command_projections();
	test_collapsing_a_drawer_returns_space_to_the_centre();
	test_drawer_resize_emits_the_host_layout_path();
	test_drawer_contents_use_incremental_model_signals();
	test_file_rows_emit_only_semantic_user_selection();
	test_the_conversations_drawer_hosts_the_threaded_conversation();
	test_the_centre_is_the_composed_rfc_0004_comparison_surface();
	test_composed_ribbons_follow_the_group_model_through_scroll_and_resize();
	test_the_lower_detail_pane_is_full_width_line_over_line();
	test_composed_epoch_selectors_show_exact_retained_epochs();
	test_review_history_state_is_explicit_in_workbench_text();
	test_focus_order_is_declared_named_and_walkable();
	test_receipt_actions_follow_decided_projection_and_clear_while_loading();
	test_selection_synchronisation_and_restart_persistence();
	test_defect_queue_navigation_and_semantic_dispatch();
	test_queue_rule_gestures_wait_for_the_accepted_projection();
	test_queue_rule_selection_survives_unrelated_projection();
	test_a_large_file_leaves_the_widget_count_bounded();
	std::cout << "workbench-window-tests:pass\n";
	return EXIT_SUCCESS;
}
