#include "bpr/core/workbench_interaction.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace bpr::core {
	namespace {
		/// \brief Describes one planned workbench navigation outcome.
		struct navigation_plan
		{
			std::optional<workbench_navigation_decision> Decision;
			workbench_reason_code                        Rejection = workbench_reason_code::none;
		};

		[[nodiscard]] constexpr auto forward(
			const change_navigation_direction aDirection) noexcept -> bool
		{
			return aDirection == change_navigation_direction::next;
		}

		/// \brief Computes the decision that enters the filtered actionable order.
		///
		/// \details Entering from nothing and wrapping past an end are the same
		/// decision: the first entry going forward, the last going backward.
		[[nodiscard]] constexpr auto enter_order(
			const workbench_navigation_cursor& aCursor,
			const change_navigation_direction  aDirection) noexcept
			-> workbench_navigation_decision
		{
			return forward(aDirection)
				? workbench_navigation_decision{
					  .Target  = workbench_navigation_target::first_actionable,
					  .Ordinal = 1U,
				  }
				: workbench_navigation_decision{
					  .Target  = workbench_navigation_target::last_actionable,
					  .Ordinal = aCursor.ActionableCount,
				  };
		}

		/// \brief Computes the decision that steps to the adjacent entry.
		///
		/// \details A resident selection knows its own 1-based ordinal, and the
		/// validated neighbour facts keep the stepped ordinal inside the order. A
		/// selection that left the filtered actionable order has no ordinal to
		/// step from, so only the host can name its neighbour.
		[[nodiscard]] constexpr auto step_order(
			const workbench_navigation_cursor& aCursor,
			const change_navigation_direction  aDirection) noexcept
			-> workbench_navigation_decision
		{
			const auto resident = aCursor.Ordinal != 0U;
			if (forward(aDirection)) {
				return {
					.Target  = workbench_navigation_target::next_actionable,
					.Ordinal = resident
						? std::optional<std::uint32_t>{ aCursor.Ordinal + 1U }
						: std::optional<std::uint32_t>{ },
				};
			}
			return {
				.Target  = workbench_navigation_target::previous_actionable,
				.Ordinal = resident
					? std::optional<std::uint32_t>{ aCursor.Ordinal - 1U }
					: std::optional<std::uint32_t>{ },
			};
		}

		// This is the single definition of file-navigation legality. The command
		// projection and the navigate transition both use it so an enabled action
		// and the gesture it triggers can never disagree.
		[[nodiscard]] constexpr auto plan_navigation(
			const workbench_interaction_state& aState,
			const change_navigation_direction  aDirection) noexcept
			-> navigation_plan
		{
			if (!aState.Navigation.has_value()) {
				return { .Rejection = workbench_reason_code::no_active_workbench };
			}

			const auto& cursor = *aState.Navigation;
			if (cursor.ActionableCount == 0U) {
				return { .Rejection = workbench_reason_code::no_actionable_files };
			}
			if (!cursor.File.has_value()) {
				return { .Decision = enter_order(cursor, aDirection) };
			}

			const auto available = forward(aDirection) ? cursor.NextAvailable
													   : cursor.PreviousAvailable;
			if (available) {
				return { .Decision = step_order(cursor, aDirection) };
			}
			if (aState.NavigationPolicy == change_navigation_policy::wrap) {
				return { .Decision = enter_order(cursor, aDirection) };
			}
			return {
				.Rejection = forward(aDirection)
					? workbench_reason_code::at_last_file
					: workbench_reason_code::at_first_file,
			};
		}

		// This is the single definition of change-navigation legality. It reads
		// the same bounded facts RFC-BPR-0004's `navigate_change` derives from
		// the semantic index, and it names the two ends of the change order
		// explicitly. Enablement is deliberately not derived from whether a
		// navigation result carried a group: `at_start` and `at_end` both report
		// the current group, so group presence cannot tell a step from a refusal.
		[[nodiscard]] constexpr auto plan_change_navigation(
			const workbench_interaction_state& aState,
			const change_navigation_direction  aDirection) noexcept
			-> workbench_reason_code
		{
			if (!aState.Navigation.has_value()) {
				return workbench_reason_code::no_active_workbench;
			}
			if (!aState.Change.has_value()) {
				return workbench_reason_code::no_open_comparison;
			}

			const auto& cursor = *aState.Change;
			if (cursor.ChangeCount == 0U) {
				// A retained selection against a comparison with no change order
				// at all is the selection that order cannot resolve.
				return cursor.Group.has_value()
					? workbench_reason_code::invalid_change_selection
					: workbench_reason_code::no_changes;
			}
			if (!cursor.Group.has_value()) {
				// Change navigation enters a nonempty order from either end.
				return workbench_reason_code::none;
			}
			if (cursor.Ordinal == 0U) {
				return workbench_reason_code::invalid_change_selection;
			}

			const auto available = forward(aDirection) ? cursor.NextAvailable
													   : cursor.PreviousAvailable;
			if (available
				|| aState.NavigationPolicy == change_navigation_policy::wrap) {
				return workbench_reason_code::none;
			}
			return forward(aDirection)
				? workbench_reason_code::at_last_change
				: workbench_reason_code::at_first_change;
		}

		// A drawer and the source panes' numbering both belong to the workbench
		// surface, so neither can be changed unless the host is publishing one.
		// An inactive workbench disables every workbench command, and nothing
		// here is exempt from that.
		[[nodiscard]] constexpr auto surface_rejection(
			const workbench_interaction_state& aState) noexcept
			-> workbench_reason_code
		{
			return aState.Navigation.has_value()
				? workbench_reason_code::none
				: workbench_reason_code::no_active_workbench;
		}

		/// \brief [[BPR-1328]]. A navigation command the reviewer can always press.
		///
		/// \details The owner, rejecting arrows that crossed into the next
		/// file, 2026-10-08: "I want the arrow keys to be there and
		/// selectable, pressable so the ui doesnt change but I want them to
		/// just try to move to the next change and realize there are no more
		/// and effectively do nothing. Just let it be a bounded prev and next
		/// that you can always use." Only an inactive workbench, where there is
		/// nothing to navigate, disables the command; every other refusal is
		/// its bound, and a press there does nothing.
		[[nodiscard]] constexpr auto bounded_command(
			const workbench_command_id  aId,
			const workbench_reason_code aRejection) noexcept
			-> workbench_command_projection
		{
			if (aRejection == workbench_reason_code::no_active_workbench) {
				return { .Id = aId, .Enabled = false, .DisabledReason = aRejection };
			}
			return {
				.Id             = aId,
				.Enabled        = true,
				.DisabledReason = workbench_reason_code::none,
				.Bound          = aRejection,
			};
		}

		[[nodiscard]] constexpr auto navigation_command(
			const workbench_interaction_state& aState,
			const workbench_command_id         aId,
			const change_navigation_direction  aDirection) noexcept
			-> workbench_command_projection
		{
			const auto plan = plan_navigation(aState, aDirection);
			return bounded_command(aId, plan.Decision.has_value() ? workbench_reason_code::none : plan.Rejection);
		}

		[[nodiscard]] constexpr auto change_command(
			const workbench_interaction_state& aState,
			const workbench_command_id         aId,
			const change_navigation_direction  aDirection) noexcept
			-> workbench_command_projection
		{
			return bounded_command(aId, plan_change_navigation(aState, aDirection));
		}

		/// \brief [[BPR-0675]]. A drawer can always be collapsed.
		///
		/// \details A drawer is not the comparison. It holds the pull
		/// request's files, its threads and the reviewer's repositories,
		/// which exist whether or not a comparison could be prepared, and
		/// the room it takes is the reviewer's to give back at any time.
		/// This was gated on an active workbench, so a pull request whose
		/// comparison failed left three drawers full of content that could
		/// not be collapsed and two strip toggles that did nothing. Line
		/// numbers keep the gate: there are no source panes to number.
		[[nodiscard]] constexpr auto drawer_command(
			const workbench_interaction_state&,
			const workbench_command_id aId) noexcept
			-> workbench_command_projection
		{
			return {
				.Id             = aId,
				.Enabled        = true,
				.DisabledReason = workbench_reason_code::none,
			};
		}

		/// \brief The one drawer state a drawer names; a switch, so a drawer
		/// this function does not know is a compile error, never an alias.
		/// \brief [[BPR-1295]]. Hiding and showing the drawers is Review's.
		///
		/// \details Unlike a single drawer's toggle it is disabled where it
		/// cannot act: Overview keeps every drawer collapsed, in Discuss
		/// the conversation is the page, and in Threads ([[BPR-1293]])
		/// the finder is. Like a drawer's, it does not wait for
		/// a comparison: the room is the reviewer's either way.
		[[nodiscard]] constexpr auto drawers_rejection(
			const workbench_interaction_state& aState) noexcept
			-> workbench_reason_code
		{
			switch (aState.Modality) {
			case workbench_modality::overview:
				return workbench_reason_code::overview_keeps_drawers_collapsed;
			case workbench_modality::discuss:
				return workbench_reason_code::discuss_shows_the_conversation;
			case workbench_modality::threads:
				return workbench_reason_code::threads_finds_a_thread;
			case workbench_modality::review:
				break;
			}
			return workbench_reason_code::none;
		}

		[[nodiscard]] constexpr auto drawers_command(
			const workbench_interaction_state& aState) noexcept
			-> workbench_command_projection
		{
			const auto rejection = drawers_rejection(aState);
			return {
				.Id             = workbench_command_id::toggle_drawers,
				.Enabled        = rejection == workbench_reason_code::none,
				.DisabledReason = rejection,
			};
		}

		[[nodiscard]] constexpr auto drawer_state_of(
			workbench_interaction_state& aState,
			const workbench_drawer       aDrawer) noexcept -> workbench_drawer_state&
		{
			switch (aDrawer) {
			case workbench_drawer::files:
				return aState.FilesDrawer;
			case workbench_drawer::conversations:
				return aState.ConversationsDrawer;
			case workbench_drawer::repositories:
				return aState.RepositoriesDrawer;
			}
			return aState.FilesDrawer;
		}

		[[nodiscard]] constexpr auto published_drawers(
			const workbench_interaction_state& aState) noexcept
			-> workbench_drawer_delta
		{
			return {
				.Files         = aState.FilesDrawer,
				.Conversations = aState.ConversationsDrawer,
				.Repositories  = aState.RepositoriesDrawer,
			};
		}

		[[nodiscard]] constexpr auto expanded(
			const workbench_drawer_state aDrawerState) noexcept -> bool
		{
			return aDrawerState == workbench_drawer_state::expanded;
		}

		[[nodiscard]] constexpr auto any_expanded(
			const workbench_drawer_delta& aDrawers) noexcept -> bool
		{
			return expanded(aDrawers.Files) || expanded(aDrawers.Conversations)
				|| expanded(aDrawers.Repositories);
		}

		/// \brief [[BPR-1295]]. Review's preset from the drawers as they are:
		/// Files and Comments open, Repositories as they were.
		[[nodiscard]] constexpr auto review_preset(
			const workbench_drawer_delta& aDrawers) noexcept
			-> workbench_drawer_delta
		{
			return {
				.Files         = workbench_drawer_state::expanded,
				.Conversations = workbench_drawer_state::expanded,
				.Repositories  = aDrawers.Repositories,
			};
		}

		constexpr void adopt_drawers(
			workbench_interaction_state&  aState,
			const workbench_drawer_delta& aDrawers) noexcept
		{
			aState.FilesDrawer         = aDrawers.Files;
			aState.ConversationsDrawer = aDrawers.Conversations;
			aState.RepositoriesDrawer  = aDrawers.Repositories;
		}

		[[nodiscard]] constexpr auto toggled(
			const workbench_drawer_state aDrawerState) noexcept
			-> workbench_drawer_state
		{
			return aDrawerState == workbench_drawer_state::expanded
				? workbench_drawer_state::collapsed
				: workbench_drawer_state::expanded;
		}

		[[nodiscard]] constexpr auto toggled(
			const workbench_line_numbers aLineNumbers) noexcept
			-> workbench_line_numbers
		{
			return aLineNumbers == workbench_line_numbers::shown
				? workbench_line_numbers::hidden
				: workbench_line_numbers::shown;
		}

		[[nodiscard]] constexpr auto toggled(
			const workbench_one_pane aOnePane) noexcept
			-> workbench_one_pane
		{
			return aOnePane == workbench_one_pane::disabled
				? workbench_one_pane::enabled
				: workbench_one_pane::disabled;
		}

		[[nodiscard]] constexpr auto toggled(
			const workbench_updates_line aUpdatesLine) noexcept
			-> workbench_updates_line
		{
			return aUpdatesLine == workbench_updates_line::folded
				? workbench_updates_line::open
				: workbench_updates_line::folded;
		}

		[[nodiscard]] constexpr auto line_numbers_command(
			const workbench_interaction_state& aState,
			const workbench_command_id         aId) noexcept
			-> workbench_command_projection
		{
			const auto rejection = surface_rejection(aState);
			return {
				.Id             = aId,
				.Enabled        = rejection == workbench_reason_code::none,
				.DisabledReason = rejection,
			};
		}

		/// \brief Adds the command projection to an output when it changed.
		[[nodiscard]] auto with_changed_commands(
			const workbench_interaction_state& aState,
			const workbench_command_delta&     aBefore,
			workbench_interaction_output       aOutput)
			-> workbench_interaction_output
		{
			const auto commands = project_workbench_commands(aState);
			if (commands != aBefore) {
				aOutput.Commands = commands;
			}
			return aOutput;
		}

		/// \brief [[BPR-1295]]. The drawer and command facts that changed
		/// since `aBefore` and `aCommands` were read.
		[[nodiscard]] auto drawer_changes(
			const workbench_interaction_state& aState,
			const workbench_drawer_delta&      aBefore,
			const workbench_command_delta&     aCommands)
			-> workbench_interaction_output
		{
			auto output = workbench_interaction_output{ };
			if (const auto after = published_drawers(aState); after != aBefore) {
				output.Drawers = after;
			}
			return with_changed_commands(aState, aCommands, output);
		}

		[[nodiscard]] auto apply_drawer(
			workbench_interaction_state& aState,
			const workbench_drawer       aDrawer,
			const workbench_drawer_state aRequested)
			-> workbench_interaction_output
		{
			// [[BPR-0675]]. No surface rejection: see `drawer_command`. The
			// gesture is about the room a drawer takes, not about what is
			// being compared inside it.
			auto& retained = drawer_state_of(aState, aDrawer);
			if (retained == aRequested) {
				return { };
			}
			// [[BPR-0727]]. Overview keeps every drawer collapsed; the owner
			// chose that a drawer opened by hand is refused rather than
			// switching the modality. Collapsing is still the reviewer's.
			if (aState.Modality == workbench_modality::overview
				&& aRequested == workbench_drawer_state::expanded) {
				return {
					.Reason = workbench_reason_code::overview_keeps_drawers_collapsed,
				};
			}

			const auto before  = published_drawers(aState);
			const auto command = project_workbench_commands(aState);
			retained           = aRequested;
			// [[BPR-1295]]. A drawer opened by hand while the drawers are
			// hidden is the reviewer's new arrangement; showing the drawers
			// can no longer undo it, so the remembered set is dropped.
			aState.HiddenDrawers.reset();
			return drawer_changes(aState, before, command);
		}

		[[nodiscard]] auto apply_line_numbers(
			workbench_interaction_state& aState,
			const workbench_line_numbers aRequested)
			-> workbench_interaction_output
		{
			const auto rejection = surface_rejection(aState);
			if (rejection != workbench_reason_code::none) {
				return { .Reason = rejection };
			}
			if (aState.LineNumbers == aRequested) {
				return { };
			}

			const auto before  = project_workbench_commands(aState);
			aState.LineNumbers = aRequested;
			return with_changed_commands(
				aState,
				before,
				workbench_interaction_output{
					.LineNumbers = workbench_line_numbers_delta{
						.State = aState.LineNumbers,
					},
				});
		}

		[[nodiscard]] auto apply_one_pane(
			workbench_interaction_state& aState,
			const workbench_one_pane     aRequested)
			-> workbench_interaction_output
		{
			const auto rejection = surface_rejection(aState);
			if (rejection != workbench_reason_code::none) {
				return { .Reason = rejection };
			}
			if (aState.OnePane == aRequested) {
				return { };
			}

			const auto before = project_workbench_commands(aState);
			aState.OnePane    = aRequested;
			return with_changed_commands(
				aState,
				before,
				workbench_interaction_output{
					.OnePane = workbench_one_pane_delta{
						.State = aState.OnePane,
					},
				});
		}

		/// \brief [[BPR-1130]]. Folds or opens the Updates line.
		///
		/// \details No surface rejection and no modality rule: the line is the
		/// review's chrome in every modality, and the room it takes is the
		/// reviewer's to give back, as a drawer's is. Only this gesture changes
		/// it, so nothing folds or opens on its own.
		[[nodiscard]] auto apply_updates_line(
			workbench_interaction_state& aState,
			const workbench_updates_line aRequested)
			-> workbench_interaction_output
		{
			if (aState.UpdatesLine == aRequested) {
				return { };
			}
			aState.UpdatesLine = aRequested;
			return {
				.UpdatesLine = workbench_updates_line_delta{
					.State = aState.UpdatesLine,
				},
			};
		}

		[[nodiscard]] auto apply(
			const workbench_interaction_state&  aState,
			const select_workbench_file_intent& aIntent)
			-> workbench_interaction_output
		{
			if (!aIntent.File.valid()) {
				return { .Reason = workbench_reason_code::invalid_file_identity };
			}
			if (!aState.Navigation.has_value()) {
				return { .Reason = workbench_reason_code::no_active_workbench };
			}
			if (aState.Navigation->File == aIntent.File) {
				return { };
			}
			return {
				.Decision = workbench_navigation_decision{
					.Target = workbench_navigation_target::exact_file,
					.File   = aIntent.File,
				},
			};
		}

		[[nodiscard]] auto apply(
			const workbench_interaction_state&    aState,
			const navigate_workbench_file_intent& aIntent)
			-> workbench_interaction_output
		{
			const auto plan = plan_navigation(aState, aIntent.Direction);
			return {
				.Decision = plan.Decision,
				.Reason   = plan.Rejection,
			};
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state&       aState,
			const set_workbench_drawer_intent& aIntent)
			-> workbench_interaction_output
		{
			return apply_drawer(aState, aIntent.Drawer, aIntent.State);
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state&          aState,
			const toggle_workbench_drawer_intent& aIntent)
			-> workbench_interaction_output
		{
			return apply_drawer(
				aState, aIntent.Drawer, toggled(drawer_state_of(aState, aIntent.Drawer)));
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state&         aState,
			const set_workbench_modality_intent& aIntent)
			-> workbench_interaction_output
		{
			const auto before  = published_drawers(aState);
			const auto command = project_workbench_commands(aState);
			const auto leaving = aState.Modality;
			const auto target  = aIntent.Modality;
			if (target == workbench_modality::review) {
				if (leaving == workbench_modality::review && aState.ReviewEntered) {
					// [[BPR-1295]]. Review is already shown: entering it again
					// keeps what the reviewer arranged, and choosing it again is
					// the way back to its preset.
					if (aIntent.Entry == workbench_modality_entry::choose) {
						adopt_drawers(aState, review_preset(before));
						aState.HiddenDrawers.reset();
					}
				} else if (aState.ReviewArrangement.has_value()) {
					// Review keeps the arrangement it last had in this window,
					// hidden drawers included.
					const auto arrangement = *aState.ReviewArrangement;
					adopt_drawers(aState, arrangement.Drawers);
					aState.HiddenDrawers = arrangement.Hidden;
				} else {
					// The window's first entry to Review.
					adopt_drawers(aState, review_preset(before));
					aState.HiddenDrawers.reset();
				}
				aState.ReviewArrangement.reset();
				aState.ReviewEntered = true;
				aState.Modality      = target;
				return drawer_changes(aState, before, command);
			}
			if (leaving == target) {
				return { };
			}
			if (leaving == workbench_modality::review && aState.ReviewEntered) {
				aState.ReviewArrangement = workbench_review_arrangement{
					.Drawers = before,
					.Hidden  = aState.HiddenDrawers,
				};
			}
			aState.HiddenDrawers.reset();
			aState.Modality = target;
			// [[BPR-0727]]. The modality is retained for the drawer rule it
			// carries; Overview collapses the three drawers in the same
			// decision so the rule holds from the moment it is chosen.
			if (target == workbench_modality::overview) {
				aState.FilesDrawer         = workbench_drawer_state::collapsed;
				aState.ConversationsDrawer = workbench_drawer_state::collapsed;
				aState.RepositoriesDrawer  = workbench_drawer_state::collapsed;
			}
			return drawer_changes(aState, before, command);
		}

		/// \brief [[BPR-1295]]. Hides every open drawer, or shows them again.
		[[nodiscard]] auto apply(
			workbench_interaction_state& aState,
			const toggle_workbench_drawers_intent&)
			-> workbench_interaction_output
		{
			if (const auto rejection = drawers_rejection(aState);
				rejection != workbench_reason_code::none) {
				return { .Reason = rejection };
			}
			const auto before  = published_drawers(aState);
			const auto command = project_workbench_commands(aState);
			if (any_expanded(before)) {
				aState.HiddenDrawers = before;
				adopt_drawers(aState,
							  workbench_drawer_delta{
								  .Files         = workbench_drawer_state::collapsed,
								  .Conversations = workbench_drawer_state::collapsed,
								  .Repositories  = workbench_drawer_state::collapsed,
							  });
			} else {
				adopt_drawers(aState, project_workbench_drawer_restore(aState).Drawers);
				aState.HiddenDrawers.reset();
			}
			return drawer_changes(aState, before, command);
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state&             aState,
			const set_workbench_line_numbers_intent& aIntent)
			-> workbench_interaction_output
		{
			return apply_line_numbers(aState, aIntent.State);
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state& aState,
			const toggle_workbench_line_numbers_intent&)
			-> workbench_interaction_output
		{
			return apply_line_numbers(aState, toggled(aState.LineNumbers));
		}
		[[nodiscard]] auto apply(
			workbench_interaction_state&         aState,
			const set_workbench_one_pane_intent& aIntent)
			-> workbench_interaction_output
		{
			return apply_one_pane(aState, aIntent.State);
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state& aState,
			const toggle_workbench_one_pane_intent&)
			-> workbench_interaction_output
		{
			return apply_one_pane(aState, toggled(aState.OnePane));
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state&             aState,
			const set_workbench_updates_line_intent& aIntent)
			-> workbench_interaction_output
		{
			return apply_updates_line(aState, aIntent.State);
		}

		[[nodiscard]] auto apply(
			workbench_interaction_state& aState,
			const toggle_workbench_updates_line_intent&)
			-> workbench_interaction_output
		{
			return apply_updates_line(aState, toggled(aState.UpdatesLine));
		}
	} // namespace

	auto valid(const workbench_navigation_cursor& aCursor) noexcept -> bool
	{
		if (aCursor.Ordinal > aCursor.ActionableCount) {
			return false;
		}
		if (aCursor.File.has_value() && !aCursor.File->valid()) {
			return false;
		}
		if (!aCursor.File.has_value() && aCursor.Ordinal != 0U) {
			return false;
		}
		if (aCursor.ActionableCount == 0U) {
			// An empty filtered actionable order reaches nothing either way.
			return !aCursor.PreviousAvailable && !aCursor.NextAvailable;
		}
		if (!aCursor.File.has_value()) {
			// Navigation enters a nonempty order from either end.
			return aCursor.PreviousAvailable && aCursor.NextAvailable;
		}
		if (aCursor.Ordinal == 0U) {
			// The selection left the filtered actionable order, so only the host
			// knows whether that order still holds an entry on either side of it.
			return true;
		}
		return aCursor.PreviousAvailable == (aCursor.Ordinal > 1U)
			&& aCursor.NextAvailable == (aCursor.Ordinal < aCursor.ActionableCount);
	}

	auto valid(const workbench_change_cursor& aCursor) noexcept -> bool
	{
		if (aCursor.Ordinal > aCursor.ChangeCount) {
			return false;
		}
		if (aCursor.Group.has_value() && !aCursor.Group->valid()) {
			return false;
		}
		if (!aCursor.Group.has_value() && aCursor.Ordinal != 0U) {
			return false;
		}
		if (aCursor.ChangeCount == 0U) {
			// An empty change order reaches nothing either way.
			return !aCursor.PreviousAvailable && !aCursor.NextAvailable;
		}
		if (!aCursor.Group.has_value()) {
			// Change navigation enters a nonempty order from either end.
			return aCursor.PreviousAvailable && aCursor.NextAvailable;
		}
		if (aCursor.Ordinal == 0U) {
			// The semantic index cannot place the selected group, so the order
			// offers no neighbour on either side of it. This is where the change
			// cursor and the file cursor genuinely differ.
			return !aCursor.PreviousAvailable && !aCursor.NextAvailable;
		}
		return aCursor.PreviousAvailable == (aCursor.Ordinal > 1U)
			&& aCursor.NextAvailable == (aCursor.Ordinal < aCursor.ChangeCount);
	}

	auto project_workbench_commands(
		const workbench_interaction_state& aState) noexcept
		-> workbench_command_delta
	{
		return {
			.Commands = {
				navigation_command(
					aState,
					workbench_command_id::previous_file,
					change_navigation_direction::previous),
				navigation_command(
					aState,
					workbench_command_id::next_file,
					change_navigation_direction::next),
				change_command(
					aState,
					workbench_command_id::previous_change,
					change_navigation_direction::previous),
				change_command(
					aState,
					workbench_command_id::next_change,
					change_navigation_direction::next),
				drawer_command(aState, workbench_command_id::toggle_files_drawer),
				drawer_command(
					aState, workbench_command_id::toggle_conversations_drawer),
				line_numbers_command(
					aState, workbench_command_id::toggle_line_numbers),
				drawer_command(aState, workbench_command_id::toggle_repositories_drawer),
				line_numbers_command(aState, workbench_command_id::toggle_one_pane),
				drawer_command(aState, workbench_command_id::toggle_updates_line),
				drawers_command(aState),
			},
			.Policy = aState.NavigationPolicy,
		};
	}

	auto project_workbench_drawer_restore(
		const workbench_interaction_state& aState) noexcept
		-> workbench_drawer_restore_delta
	{
		return {
			.Drawers = aState.HiddenDrawers.value_or(review_preset(published_drawers(aState))),
		};
	}

	auto observe_workbench_navigation(
		workbench_interaction_state&                      aState,
		const std::optional<workbench_navigation_cursor>& aCursor)
		-> workbench_interaction_output
	{
		if (aCursor.has_value() && !valid(*aCursor)) {
			return { .Reason = workbench_reason_code::invalid_navigation_cursor };
		}
		if (aState.Navigation == aCursor) {
			return { };
		}

		const auto before = project_workbench_commands(aState);
		aState.Navigation = aCursor;
		return with_changed_commands(
			aState,
			before,
			workbench_interaction_output{
				.Selection = workbench_selection_delta{ .Cursor = aCursor },
			});
	}

	auto observe_workbench_change_navigation(
		workbench_interaction_state&                  aState,
		const std::optional<workbench_change_cursor>& aCursor)
		-> workbench_interaction_output
	{
		if (aCursor.has_value() && !valid(*aCursor)) {
			return { .Reason = workbench_reason_code::invalid_change_cursor };
		}
		if (aState.Change == aCursor) {
			return { };
		}

		const auto before = project_workbench_commands(aState);
		aState.Change     = aCursor;
		return with_changed_commands(aState, before, workbench_interaction_output{ });
	}

	auto transition(
		workbench_interaction_state&        aState,
		const workbench_interaction_intent& aIntent)
		-> workbench_interaction_output
	{
		return std::visit(
			[&aState](const auto& aValue) { return apply(aState, aValue); },
			aIntent);
	}

	auto route_workbench_thread(const open_workbench_thread_intent& aIntent) noexcept
		-> workbench_thread_route
	{
		if (aIntent.Destination == workbench_thread_destination::review || aIntent.Draft) {
			return workbench_thread_route{
				.Modality     = workbench_modality::review,
				.OpenComments = true,
				.Reason       = aIntent.Destination == workbench_thread_destination::discuss
					? workbench_thread_route_reason::draft_opens_in_review
					: workbench_thread_route_reason::none,
			};
		}
		return workbench_thread_route{
			.Modality = workbench_modality::discuss,
			.Clear    = aIntent.Hiding,
		};
	}
} // namespace bpr::core
