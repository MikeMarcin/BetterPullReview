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

		[[nodiscard]] constexpr auto navigation_command(
			const workbench_interaction_state& aState,
			const workbench_command_id         aId,
			const change_navigation_direction  aDirection) noexcept
			-> workbench_command_projection
		{
			const auto plan = plan_navigation(aState, aDirection);
			return {
				.Id             = aId,
				.Enabled        = plan.Decision.has_value(),
				.DisabledReason = plan.Rejection,
			};
		}

		[[nodiscard]] constexpr auto change_command(
			const workbench_interaction_state& aState,
			const workbench_command_id         aId,
			const change_navigation_direction  aDirection) noexcept
			-> workbench_command_projection
		{
			const auto rejection = plan_change_navigation(aState, aDirection);
			return {
				.Id             = aId,
				.Enabled        = rejection == workbench_reason_code::none,
				.DisabledReason = rejection,
			};
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

			const auto before = project_workbench_commands(aState);
			retained          = aRequested;
			return with_changed_commands(
				aState,
				before,
				workbench_interaction_output{
					.Drawers = published_drawers(aState),
				});
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
			// [[BPR-0727]]. The modality is retained for the drawer rule it
			// carries; Overview collapses the three drawers in the same
			// decision so the rule holds from the moment it is chosen.
			if (aState.Modality == aIntent.Modality) {
				return { };
			}
			aState.Modality = aIntent.Modality;
			if (aIntent.Modality != workbench_modality::overview) {
				return { };
			}
			const auto before          = published_drawers(aState);
			aState.FilesDrawer         = workbench_drawer_state::collapsed;
			aState.ConversationsDrawer = workbench_drawer_state::collapsed;
			aState.RepositoriesDrawer  = workbench_drawer_state::collapsed;
			const auto after           = published_drawers(aState);
			if (before == after) {
				return { };
			}
			return { .Drawers = after };
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
			},
			.Policy = aState.NavigationPolicy,
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
} // namespace bpr::core
