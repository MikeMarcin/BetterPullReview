#include "bpr/core/device_authorization_transition.hpp"

#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto next_request(device_authorization_state& aState)
			-> request_id
		{
			const auto request = aState.NextRequest;
			aState.NextRequest = request_id{ request.value() + 1U };
			return request;
		}

		[[nodiscard]] auto command(
			const device_authorization_command_id  aId,
			const bool                             aEnabled,
			const device_authorization_reason_code aReason
			= device_authorization_reason_code::none)
			-> device_authorization_command_projection
		{
			return device_authorization_command_projection{
				.Id             = aId,
				.Enabled        = aEnabled,
				.DisabledReason = aEnabled ? device_authorization_reason_code::none
										   : aReason,
			};
		}

		[[nodiscard]] auto reject(const device_authorization_reason_code aReason)
			-> device_authorization_transition_output
		{
			device_authorization_transition_output output;
			output.Changed = device_authorization_changed_domain::diagnostics;
			output.Diagnostics.push_back(
				device_authorization_diagnostic{ .Code = aReason });
			return output;
		}

		auto publish_commands(
			const device_authorization_state&       aState,
			device_authorization_transition_output& aOutput) -> void
		{
			aOutput.Commands = device_authorization_commands(aState);
			aOutput.Changed  = aOutput.Changed
				| device_authorization_changed_domain::commands;
		}

		auto publish_status(
			const device_authorization_state&       aState,
			device_authorization_transition_output& aOutput) -> void
		{
			aOutput.Status = device_authorization_status_delta{
				.Stage = aState.Stage,
			};
			aOutput.Changed = aOutput.Changed
				| device_authorization_changed_domain::status;
		}

		[[nodiscard]] auto current_flow(
			const device_authorization_state&           aState,
			const device_authorization_request_context& aContext) noexcept
			-> bool
		{
			return aState.Pending.has_value() && *aState.Pending == aContext
				&& aState.ApplicationSession == aContext.ApplicationSession
				&& aState.IdentityGeneration == aContext.IdentityGeneration
				&& aState.Generation == aContext.Generation
				&& aState.Profile == aContext.Profile;
		}

		[[nodiscard]] auto enter_terminal(
			device_authorization_state&            aState,
			const device_authorization_stage       aStage,
			const device_authorization_reason_code aReason)
			-> device_authorization_transition_output
		{
			aState.Stage          = aStage;
			aState.TerminalReason = aReason;
			aState.Pending.reset();

			device_authorization_transition_output output;
			if (aReason != device_authorization_reason_code::none) {
				output.Changed = device_authorization_changed_domain::diagnostics;
				output.Diagnostics.push_back(
					device_authorization_diagnostic{ .Code = aReason });
			}
			publish_status(aState, output);
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto apply(
			device_authorization_state&              aState,
			const begin_device_authorization_intent& aIntent)
			-> device_authorization_transition_output
		{
			if (
				aState.Stage == device_authorization_stage::requested
				|| aState.Stage
					== device_authorization_stage::awaiting_authorization) {
				return reject(
					device_authorization_reason_code::flow_in_progress);
			}
			if (aState.Stage == device_authorization_stage::cancelling) {
				return reject(
					device_authorization_reason_code::cancellation_in_progress);
			}

			aState.Generation = device_authorization_generation{
				aState.Generation.value() + 1U,
			};
			const auto context = device_authorization_request_context{
				.Request            = next_request(aState),
				.ApplicationSession = aState.ApplicationSession,
				.Profile            = aIntent.Profile,
				.IdentityGeneration = aState.IdentityGeneration,
				.Generation         = aState.Generation,
			};
			aState.Stage          = device_authorization_stage::requested;
			aState.Pending        = context;
			aState.Profile        = aIntent.Profile;
			aState.TerminalReason = device_authorization_reason_code::none;

			device_authorization_transition_output output;
			output.Requests.push_back(
				begin_device_authorization_request{ .Context = context });
			publish_status(aState, output);
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto apply(
			device_authorization_state& aState,
			const cancel_device_authorization_intent&)
			-> device_authorization_transition_output
		{
			if (aState.Stage == device_authorization_stage::cancelling) {
				return reject(
					device_authorization_reason_code::cancellation_in_progress);
			}
			if (
				aState.Stage != device_authorization_stage::requested
				&& aState.Stage
					!= device_authorization_stage::awaiting_authorization) {
				return reject(device_authorization_reason_code::no_active_flow);
			}

			aState.Stage = device_authorization_stage::cancelling;

			device_authorization_transition_output output;
			output.Requests.push_back(
				cancel_device_authorization_request{ .Context = *aState.Pending });
			publish_status(aState, output);
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto apply(
			device_authorization_state&                aState,
			const device_authorization_awaiting_event& aEvent)
			-> device_authorization_transition_output
		{
			if (!current_flow(aState, aEvent.Context)) {
				return reject(device_authorization_reason_code::stale_completion);
			}
			if (aState.Stage == device_authorization_stage::cancelling) {
				return reject(
					device_authorization_reason_code::cancellation_in_progress);
			}
			if (aState.Stage != device_authorization_stage::requested) {
				return reject(device_authorization_reason_code::stale_completion);
			}

			aState.Stage = device_authorization_stage::awaiting_authorization;

			device_authorization_transition_output output;
			publish_status(aState, output);
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto resolve(
			device_authorization_state&                 aState,
			const device_authorization_request_context& aContext,
			const device_authorization_stage            aStage,
			const device_authorization_reason_code      aReason)
			-> device_authorization_transition_output
		{
			if (!current_flow(aState, aContext)) {
				return reject(device_authorization_reason_code::stale_completion);
			}
			if (aState.Stage == device_authorization_stage::cancelling) {
				return reject(
					device_authorization_reason_code::cancellation_in_progress);
			}
			return enter_terminal(aState, aStage, aReason);
		}

		[[nodiscard]] auto apply(
			device_authorization_state&                  aState,
			const device_authorization_authorized_event& aEvent)
			-> device_authorization_transition_output
		{
			return resolve(
				aState,
				aEvent.Context,
				device_authorization_stage::authorized,
				device_authorization_reason_code::none);
		}

		[[nodiscard]] auto apply(
			device_authorization_state&              aState,
			const device_authorization_denied_event& aEvent)
			-> device_authorization_transition_output
		{
			return resolve(
				aState,
				aEvent.Context,
				device_authorization_stage::denied,
				device_authorization_reason_code::authorization_denied);
		}

		[[nodiscard]] auto apply(
			device_authorization_state&               aState,
			const device_authorization_expired_event& aEvent)
			-> device_authorization_transition_output
		{
			return resolve(
				aState,
				aEvent.Context,
				device_authorization_stage::expired,
				device_authorization_reason_code::authorization_expired);
		}

		[[nodiscard]] auto apply(
			device_authorization_state&                aState,
			const device_authorization_disabled_event& aEvent)
			-> device_authorization_transition_output
		{
			return resolve(
				aState,
				aEvent.Context,
				device_authorization_stage::disabled,
				device_authorization_reason_code::device_flow_disabled);
		}

		[[nodiscard]] auto apply(
			device_authorization_state&                   aState,
			const device_authorization_unavailable_event& aEvent)
			-> device_authorization_transition_output
		{
			return resolve(
				aState,
				aEvent.Context,
				device_authorization_stage::unavailable,
				device_authorization_reason_code::device_flow_unavailable);
		}

		[[nodiscard]] auto apply(
			device_authorization_state&              aState,
			const device_authorization_failed_event& aEvent)
			-> device_authorization_transition_output
		{
			return resolve(
				aState,
				aEvent.Context,
				device_authorization_stage::failed,
				reason_for(aEvent.Failure));
		}

		[[nodiscard]] auto apply(
			device_authorization_state&                 aState,
			const device_authorization_cancelled_event& aEvent)
			-> device_authorization_transition_output
		{
			if (!current_flow(aState, aEvent.Context)) {
				return reject(device_authorization_reason_code::stale_completion);
			}
			return enter_terminal(
				aState,
				device_authorization_stage::cancelled,
				device_authorization_reason_code::cancelled);
		}
	}

	auto device_authorization_commands(const device_authorization_state& aState)
		-> std::vector<device_authorization_command_projection>
	{
		const auto active = aState.Stage == device_authorization_stage::requested
			|| aState.Stage == device_authorization_stage::awaiting_authorization;
		const auto cancelling
			= aState.Stage == device_authorization_stage::cancelling;

		auto commands = std::vector<device_authorization_command_projection>{ };
		commands.push_back(command(
			device_authorization_command_id::begin_device_authorization,
			!active && !cancelling,
			cancelling
				? device_authorization_reason_code::cancellation_in_progress
				: device_authorization_reason_code::flow_in_progress));
		commands.push_back(command(
			device_authorization_command_id::cancel_device_authorization,
			active,
			cancelling
				? device_authorization_reason_code::cancellation_in_progress
				: device_authorization_reason_code::no_active_flow));
		return commands;
	}

	auto transition(
		device_authorization_state&       aState,
		const device_authorization_event& aEvent)
		-> device_authorization_transition_output
	{
		return std::visit(
			[&aState](const auto& aValue) { return apply(aState, aValue); },
			aEvent);
	}
}
