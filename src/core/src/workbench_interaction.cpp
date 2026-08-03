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

		// A drawer belongs to the workbench surface, so it can only be collapsed
		// or expanded while the host is publishing one.
		[[nodiscard]] constexpr auto drawer_rejection(
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

		[[nodiscard]] constexpr auto drawer_command(
			const workbench_interaction_state& aState,
			const workbench_command_id         aId) noexcept
			-> workbench_command_projection
		{
			const auto rejection = drawer_rejection(aState);
			return {
				.Id             = aId,
				.Enabled        = rejection == workbench_reason_code::none,
				.DisabledReason = rejection,
			};
		}

		[[nodiscard]] constexpr auto published_drawers(
			const workbench_interaction_state& aState) noexcept
			-> workbench_drawer_delta
		{
			return {
				.Files         = aState.FilesDrawer,
				.Conversations = aState.ConversationsDrawer,
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
			const auto rejection = drawer_rejection(aState);
			if (rejection != workbench_reason_code::none) {
				return { .Reason = rejection };
			}

			auto& retained = aDrawer == workbench_drawer::files
				? aState.FilesDrawer
				: aState.ConversationsDrawer;
			if (retained == aRequested) {
				return { };
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
			const auto retained = aIntent.Drawer == workbench_drawer::files
				? aState.FilesDrawer
				: aState.ConversationsDrawer;
			return apply_drawer(aState, aIntent.Drawer, toggled(retained));
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
				drawer_command(aState, workbench_command_id::toggle_files_drawer),
				drawer_command(
					aState, workbench_command_id::toggle_conversations_drawer),
			},
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
