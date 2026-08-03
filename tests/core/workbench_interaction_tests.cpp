#include "bpr/core/workbench_interaction.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto expect(const bool             aCondition,
				const std::string_view aLabel,
				const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aLabel << ": " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] constexpr auto file(const std::uint8_t aSeed) -> workbench_file_id
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return workbench_file_id{ bytes };
	}

	/// \brief Builds a cursor whose selection sits inside the actionable order.
	[[nodiscard]] auto resident(const std::uint32_t aOrdinal,
								const std::uint32_t aCount,
								const std::uint8_t  aSeed = 7U)
		-> workbench_navigation_cursor
	{
		return {
			.File              = file(aSeed),
			.Ordinal           = aOrdinal,
			.ActionableCount   = aCount,
			.PreviousAvailable = aOrdinal > 1U,
			.NextAvailable     = aOrdinal < aCount,
		};
	}

	/// \brief Builds a cursor whose selection left the actionable order.
	[[nodiscard]] auto nonresident(const std::uint32_t aCount,
								   const bool          aPrevious,
								   const bool          aNext,
								   const std::uint8_t  aSeed = 9U)
		-> workbench_navigation_cursor
	{
		return {
			.File              = file(aSeed),
			.ActionableCount   = aCount,
			.PreviousAvailable = aPrevious,
			.NextAvailable     = aNext,
		};
	}

	/// \brief Builds a cursor with no selected file at all.
	[[nodiscard]] auto unselected(const std::uint32_t aCount)
		-> workbench_navigation_cursor
	{
		return {
			.ActionableCount   = aCount,
			.PreviousAvailable = aCount != 0U,
			.NextAvailable     = aCount != 0U,
		};
	}

	[[nodiscard]] auto active(
		const workbench_navigation_cursor& aCursor,
		const change_navigation_policy     aPolicy = change_navigation_policy::stop)
		-> workbench_interaction_state
	{
		return {
			.Navigation       = aCursor,
			.NavigationPolicy = aPolicy,
		};
	}

	[[nodiscard]] auto navigate(const workbench_interaction_state& aState,
								const change_navigation_direction  aDirection)
		-> workbench_interaction_output
	{
		auto probe = aState;
		auto output
			= transition(probe, navigate_workbench_file_intent{ .Direction = aDirection });
		expect(probe == aState,
			   "a navigation decision leaves the retained cursor to the host");
		return output;
	}

	[[nodiscard]] auto decided(const workbench_interaction_output& aOutput,
							   const workbench_navigation_target   aTarget,
							   const std::optional<std::uint32_t>  aOrdinal) -> bool
	{
		return aOutput.Reason == workbench_reason_code::none
			&& aOutput.Decision.has_value()
			&& aOutput.Decision->Target == aTarget
			&& aOutput.Decision->Ordinal == aOrdinal
			&& !aOutput.Decision->File.has_value();
	}

	[[nodiscard]] auto rejected(const workbench_interaction_output& aOutput,
								const workbench_reason_code         aReason) -> bool
	{
		return aOutput.Reason == aReason && !aOutput.Decision.has_value()
			&& !aOutput.Selection.has_value() && !aOutput.Drawers.has_value()
			&& !aOutput.Commands.has_value();
	}

	auto test_cursor_contract() -> void
	{
		expect(valid(resident(1U, 1U)) && valid(resident(1U, 3U))
				   && valid(resident(2U, 3U)) && valid(resident(3U, 3U)),
			   "a resident selection agreeing with its ordinal is well formed");
		expect(valid(unselected(0U)) && valid(unselected(4U)),
			   "an unselected cursor is well formed in an empty or filled order");
		expect(valid(nonresident(4U, false, false))
				   && valid(nonresident(4U, true, false))
				   && valid(nonresident(4U, false, true))
				   && valid(nonresident(4U, true, true)),
			   "a nonresident selection may report either neighbour fact");
		expect(valid(workbench_navigation_cursor{ .File = file(3U) }),
			   "a selected file with an empty actionable order is well formed");

		auto beyond            = resident(2U, 3U);
		beyond.ActionableCount = 1U;
		expect(!valid(beyond), "an ordinal past the actionable count is rejected");

		auto unnamed = resident(2U, 3U);
		unnamed.File.reset();
		expect(!valid(unnamed), "an ordinal without a selected file is rejected");

		auto zeroed = resident(2U, 3U);
		zeroed.File = workbench_file_id{ };
		expect(!valid(zeroed), "a zero file identity is rejected");

		auto wrongPrevious              = resident(1U, 3U);
		wrongPrevious.PreviousAvailable = true;
		expect(!valid(wrongPrevious),
			   "the first resident entry cannot claim a previous entry");

		auto wrongNext          = resident(3U, 3U);
		wrongNext.NextAvailable = true;
		expect(!valid(wrongNext),
			   "the last resident entry cannot claim a next entry");

		auto missingNext          = resident(1U, 3U);
		missingNext.NextAvailable = false;
		expect(!valid(missingNext),
			   "a resident entry cannot deny a neighbour its ordinal implies");

		auto strandedEntry              = unselected(4U);
		strandedEntry.PreviousAvailable = false;
		expect(!valid(strandedEntry),
			   "an unselected cursor cannot deny entry into a filled order");

		auto emptyWithNeighbour          = unselected(0U);
		emptyWithNeighbour.NextAvailable = true;
		expect(!valid(emptyWithNeighbour),
			   "an empty actionable order reaches nothing in either direction");
	}

	auto test_navigation_follows_actionable_order() -> void
	{
		const auto middle = active(resident(2U, 3U));
		expect(decided(navigate(middle, change_navigation_direction::next),
					   workbench_navigation_target::next_actionable,
					   3U),
			   "next steps to the following ordinal of the actionable order");
		expect(decided(navigate(middle, change_navigation_direction::previous),
					   workbench_navigation_target::previous_actionable,
					   1U),
			   "previous steps to the preceding ordinal of the actionable order");

		const auto last = active(resident(3U, 3U));
		expect(rejected(navigate(last, change_navigation_direction::next),
						workbench_reason_code::at_last_file),
			   "the stop policy refuses to step past the last actionable entry");
		expect(decided(navigate(last, change_navigation_direction::previous),
					   workbench_navigation_target::previous_actionable,
					   2U),
			   "the last actionable entry still steps backward");

		const auto first = active(resident(1U, 3U));
		expect(rejected(navigate(first, change_navigation_direction::previous),
						workbench_reason_code::at_first_file),
			   "the stop policy refuses to step before the first actionable entry");

		const auto wrappedLast
			= active(resident(3U, 3U), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedLast, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U),
			   "the wrap policy returns to the first actionable entry");
		const auto wrappedFirst
			= active(resident(1U, 3U), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedFirst, change_navigation_direction::previous),
					   workbench_navigation_target::last_actionable,
					   3U),
			   "the wrap policy returns to the last actionable entry");

		const auto only = active(resident(1U, 1U));
		expect(rejected(navigate(only, change_navigation_direction::next),
						workbench_reason_code::at_last_file)
				   && rejected(navigate(only, change_navigation_direction::previous),
							   workbench_reason_code::at_first_file),
			   "a single actionable entry stops in both directions");
		const auto wrappedOnly
			= active(resident(1U, 1U), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedOnly, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U)
				   && decided(
					   navigate(wrappedOnly, change_navigation_direction::previous),
					   workbench_navigation_target::last_actionable,
					   1U),
			   "wrapping a single actionable entry reselects it");

		const auto stranded = active(nonresident(4U, true, true));
		expect(decided(navigate(stranded, change_navigation_direction::next),
					   workbench_navigation_target::next_actionable,
					   std::nullopt)
				   && decided(navigate(stranded, change_navigation_direction::previous),
							  workbench_navigation_target::previous_actionable,
							  std::nullopt),
			   "only the host can name the neighbour of a nonresident selection");

		const auto strandedAtEnd = active(nonresident(4U, true, false));
		expect(rejected(navigate(strandedAtEnd, change_navigation_direction::next),
						workbench_reason_code::at_last_file),
			   "a nonresident selection with no following entry stops");
		const auto wrappedFromEnd
			= active(nonresident(4U, true, false), change_navigation_policy::wrap);
		expect(decided(navigate(wrappedFromEnd, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U),
			   "a nonresident selection with no following entry wraps");

		const auto empty = active(unselected(3U));
		expect(decided(navigate(empty, change_navigation_direction::next),
					   workbench_navigation_target::first_actionable,
					   1U)
				   && decided(navigate(empty, change_navigation_direction::previous),
							  workbench_navigation_target::last_actionable,
							  3U),
			   "navigation with no selection enters the actionable order at an end");

		const auto nothing = active(unselected(0U));
		expect(rejected(navigate(nothing, change_navigation_direction::next),
						workbench_reason_code::no_actionable_files)
				   && rejected(navigate(nothing, change_navigation_direction::previous),
							   workbench_reason_code::no_actionable_files),
			   "an empty actionable order refuses navigation");

		const auto inactive = workbench_interaction_state{ };
		expect(rejected(navigate(inactive, change_navigation_direction::next),
						workbench_reason_code::no_active_workbench)
				   && rejected(navigate(inactive, change_navigation_direction::previous),
							   workbench_reason_code::no_active_workbench),
			   "an inactive workbench refuses navigation");
	}

	[[nodiscard]] auto intent_for(const workbench_command_id aCommand)
		-> workbench_interaction_intent
	{
		if (aCommand == workbench_command_id::previous_file
			|| aCommand == workbench_command_id::next_file) {
			return navigate_workbench_file_intent{
				.Direction = aCommand == workbench_command_id::previous_file
					? change_navigation_direction::previous
					: change_navigation_direction::next,
			};
		}
		return toggle_workbench_drawer_intent{
			.Drawer = aCommand == workbench_command_id::toggle_files_drawer
				? workbench_drawer::files
				: workbench_drawer::conversations,
		};
	}

	/// \brief Describes one named workbench state used by the command table.
	struct labelled_state
	{
		std::string_view            Label;
		workbench_interaction_state State;
	};

	auto check_command_agreement(const labelled_state& aCase) -> void
	{
		const auto commands = project_workbench_commands(aCase.State);
		for (std::size_t index = 0U; index < workbench_command_count; ++index) {
			const auto command    = commands.Commands[index];
			const auto identifier = static_cast<workbench_command_id>(index);
			expect(command.Id == identifier,
				   aCase.Label,
				   "the command delta projects one entry per command in order");

			auto       probe  = aCase.State;
			const auto output = transition(probe, intent_for(identifier));
			expect(command.Enabled == (output.Reason == workbench_reason_code::none),
				   aCase.Label,
				   "an enabled command is exactly an accepted gesture");
			expect(command.DisabledReason == output.Reason,
				   aCase.Label,
				   "a disabled command explains itself with the rejection reason");
			expect(probe == aCase.State || command.Enabled,
				   aCase.Label,
				   "a rejected gesture changes no retained fact");
		}
	}

	auto test_command_projection() -> void
	{
		const auto cases = std::array<labelled_state, 8U>{ {
			{ "inactive", workbench_interaction_state{ } },
			{ "empty order", active(unselected(0U)) },
			{ "no selection", active(unselected(3U)) },
			{ "first of three", active(resident(1U, 3U)) },
			{ "middle of three", active(resident(2U, 3U)) },
			{ "last of three", active(resident(3U, 3U)) },
			{ "last of three wrapping",
			  active(resident(3U, 3U), change_navigation_policy::wrap) },
			{ "nonresident at end", active(nonresident(4U, true, false)) },
		} };
		for (const auto& state : cases) {
			check_command_agreement(state);
		}

		const auto inactive = project_workbench_commands(workbench_interaction_state{ });
		expect(std::ranges::none_of(inactive.Commands,
									[](const workbench_command_projection& aCommand) {
										return aCommand.Enabled;
									}),
			   "an inactive workbench disables every workbench command");
		expect(std::ranges::all_of(
				   inactive.Commands,
				   [](const workbench_command_projection& aCommand) {
					   return aCommand.DisabledReason
						   == workbench_reason_code::no_active_workbench;
				   }),
			   "an inactive workbench explains every disabled command the same way");

		const auto emptyOrder = project_workbench_commands(active(unselected(0U)));
		expect(!emptyOrder.Commands[0].Enabled && !emptyOrder.Commands[1].Enabled
				   && emptyOrder.Commands[0].DisabledReason
					   == workbench_reason_code::no_actionable_files
				   && emptyOrder.Commands[1].DisabledReason
					   == workbench_reason_code::no_actionable_files,
			   "an empty actionable order disables both navigation commands");
		expect(emptyOrder.Commands[2].Enabled && emptyOrder.Commands[3].Enabled
				   && emptyOrder.Commands[2].DisabledReason
					   == workbench_reason_code::none,
			   "an active workbench keeps both drawer commands available");

		const auto stopped = project_workbench_commands(active(resident(3U, 3U)));
		expect(stopped.Commands[0].Enabled && !stopped.Commands[1].Enabled
				   && stopped.Commands[1].DisabledReason
					   == workbench_reason_code::at_last_file,
			   "the last actionable entry disables next file under the stop policy");
		const auto wrapped = project_workbench_commands(
			active(resident(3U, 3U), change_navigation_policy::wrap));
		expect(wrapped.Commands[0].Enabled && wrapped.Commands[1].Enabled,
			   "the wrap policy keeps both navigation commands available");
		const auto atFirst = project_workbench_commands(active(resident(1U, 3U)));
		expect(!atFirst.Commands[0].Enabled
				   && atFirst.Commands[0].DisabledReason
					   == workbench_reason_code::at_first_file,
			   "the first actionable entry disables previous file under stop");
	}

	auto test_selection_gestures() -> void
	{
		auto       state   = active(resident(2U, 3U, 7U));
		const auto initial = state;

		const auto invalid = transition(
			state, select_workbench_file_intent{ .File = workbench_file_id{ } });
		expect(rejected(invalid, workbench_reason_code::invalid_file_identity)
				   && state == initial,
			   "an invalid file identity is refused");

		auto       inactive = workbench_interaction_state{ };
		const auto refused
			= transition(inactive, select_workbench_file_intent{ .File = file(7U) });
		expect(rejected(refused, workbench_reason_code::no_active_workbench)
				   && inactive == workbench_interaction_state{ },
			   "an inactive workbench refuses a file selection");

		const auto reselected
			= transition(state, select_workbench_file_intent{ .File = file(7U) });
		expect(reselected == workbench_interaction_output{ } && state == initial,
			   "reselecting the selected file decides and publishes nothing");

		const auto selected
			= transition(state, select_workbench_file_intent{ .File = file(8U) });
		expect(selected.Reason == workbench_reason_code::none
				   && selected.Decision.has_value()
				   && selected.Decision->Target
					   == workbench_navigation_target::exact_file
				   && selected.Decision->File == file(8U)
				   && !selected.Decision->Ordinal.has_value()
				   && !selected.Selection.has_value() && state == initial,
			   "selecting another file decides an exact target for the host");

		const auto resolved
			= observe_workbench_navigation(state, resident(3U, 3U, 8U));
		expect(resolved.Reason == workbench_reason_code::none
				   && resolved.Selection.has_value()
				   && resolved.Selection->Cursor == resident(3U, 3U, 8U)
				   && state.Navigation == resident(3U, 3U, 8U),
			   "the host resolution publishes the accepted selection");
		expect(resolved.Commands.has_value()
				   && !resolved.Commands->Commands[1].Enabled,
			   "an accepted selection republishes changed command state");
	}

	auto test_navigation_observations() -> void
	{
		auto       state = workbench_interaction_state{ };
		const auto activated
			= observe_workbench_navigation(state, resident(2U, 3U, 7U));
		expect(activated.Selection.has_value()
				   && activated.Selection->Cursor == resident(2U, 3U, 7U)
				   && activated.Commands.has_value()
				   && activated.Commands->Commands[0].Enabled
				   && activated.Commands->Commands[2].Enabled,
			   "the first observation activates the workbench and its commands");

		const auto repeated
			= observe_workbench_navigation(state, resident(2U, 3U, 7U));
		expect(repeated == workbench_interaction_output{ },
			   "an unchanged observation publishes nothing");

		const auto sibling
			= observe_workbench_navigation(state, resident(2U, 3U, 11U));
		expect(sibling.Selection.has_value() && !sibling.Commands.has_value(),
			   "a selection change that keeps command legality publishes no commands");

		auto malformed            = resident(2U, 3U, 11U);
		malformed.NextAvailable   = false;
		const auto before         = state;
		const auto rejectedCursor = observe_workbench_navigation(state, malformed);
		expect(rejected(rejectedCursor,
						workbench_reason_code::invalid_navigation_cursor)
				   && state == before,
			   "a malformed cursor is rejected without changing retained facts");

		const auto cleared = observe_workbench_navigation(state, std::nullopt);
		expect(cleared.Selection.has_value()
				   && !cleared.Selection->Cursor.has_value()
				   && cleared.Commands.has_value()
				   && !cleared.Commands->Commands[0].Enabled
				   && !cleared.Commands->Commands[2].Enabled
				   && !state.Navigation.has_value(),
			   "an absent observation deactivates the workbench");
	}

	auto test_drawer_state() -> void
	{
		auto inactive = workbench_interaction_state{ };
		expect(inactive.FilesDrawer == workbench_drawer_state::expanded
				   && inactive.ConversationsDrawer == workbench_drawer_state::expanded,
			   "both drawers start expanded");
		const auto refused = transition(
			inactive,
			toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files });
		expect(rejected(refused, workbench_reason_code::no_active_workbench)
				   && inactive == workbench_interaction_state{ },
			   "an inactive workbench refuses a drawer gesture");

		auto       state          = active(resident(2U, 3U));
		const auto collapsedFiles = transition(
			state,
			toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files });
		expect(collapsedFiles.Reason == workbench_reason_code::none
				   && collapsedFiles.Drawers.has_value()
				   && collapsedFiles.Drawers->Files
					   == workbench_drawer_state::collapsed
				   && collapsedFiles.Drawers->Conversations
					   == workbench_drawer_state::expanded
				   && state.FilesDrawer == workbench_drawer_state::collapsed
				   && state.ConversationsDrawer == workbench_drawer_state::expanded,
			   "toggling one drawer collapses it and leaves the other alone");
		expect(!collapsedFiles.Commands.has_value()
				   && !collapsedFiles.Decision.has_value()
				   && !collapsedFiles.Selection.has_value(),
			   "a drawer gesture publishes no command or selection change");

		const auto restored = transition(
			state,
			toggle_workbench_drawer_intent{ .Drawer = workbench_drawer::files });
		expect(restored.Drawers.has_value()
				   && restored.Drawers->Files == workbench_drawer_state::expanded
				   && state.FilesDrawer == workbench_drawer_state::expanded,
			   "toggling the same drawer again expands it");

		const auto unchanged = transition(
			state,
			set_workbench_drawer_intent{
				.Drawer = workbench_drawer::conversations,
				.State  = workbench_drawer_state::expanded,
			});
		expect(unchanged == workbench_interaction_output{ },
			   "setting the drawer state it already has publishes nothing");

		const auto set = transition(
			state,
			set_workbench_drawer_intent{
				.Drawer = workbench_drawer::conversations,
				.State  = workbench_drawer_state::collapsed,
			});
		expect(set.Drawers.has_value()
				   && set.Drawers->Conversations == workbench_drawer_state::collapsed
				   && set.Drawers->Files == workbench_drawer_state::expanded
				   && state.ConversationsDrawer == workbench_drawer_state::collapsed,
			   "setting a drawer state publishes both semantic drawer states");
	}
} // namespace

int main()
{
	test_cursor_contract();
	test_navigation_follows_actionable_order();
	test_command_projection();
	test_selection_gestures();
	test_navigation_observations();
	test_drawer_state();
	return EXIT_SUCCESS;
}
