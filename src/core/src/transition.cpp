#include "bpr/core/transition.hpp"

#include <type_traits>

namespace bpr::core {
	namespace {
		auto refresh_command(const bool aEnabled, const reason_code aReason)
			-> command_projection
		{
			return command_projection{
				.Id             = command_id::refresh,
				.Enabled        = aEnabled,
				.Checked        = false,
				.DisabledReason = aReason,
			};
		}

		auto reject_stale_completion() -> transition_output
		{
			transition_output output;
			output.Diagnostics.push_back(diagnostic{
				.Code   = reason_code::stale_completion,
				.Detail = "Completion identity does not match the active refresh",
			});
			return output;
		}

		auto is_current(
			const application_state& aState,
			const request_context&   aContext) -> bool
		{
			return aContext.Session == aState.Session
				&& aContext.Generation == aState.RefreshGeneration
				&& aState.PendingRefresh.has_value()
				&& aContext.Request == *aState.PendingRefresh;
		}

		auto request_refresh(application_state& aState) -> transition_output
		{
			if (aState.PendingRefresh.has_value()) {
				transition_output output;
				output.Diagnostics.push_back(diagnostic{
					.Code   = reason_code::refresh_in_progress,
					.Detail = "A foundation refresh is already in progress",
				});
				return output;
			}

			const auto request       = aState.NextRequest;
			aState.NextRequest       = request_id{ request.value() + 1U };
			aState.RefreshGeneration = request_generation{ aState.RefreshGeneration.value() + 1U };
			aState.PendingRefresh    = request;

			const request_context context{
				.Request    = request,
				.Session    = aState.Session,
				.Generation = aState.RefreshGeneration,
			};

			transition_output output;
			output.Changed = changed_domain::commands | changed_domain::status;
			output.Requests.emplace_back(refresh_foundation_request{ .Context = context });
			output.Commands.push_back(
				refresh_command(false, reason_code::refresh_in_progress));
			output.Status = status_delta{ .Message = "Refreshing foundation state" };
			return output;
		}

		auto complete_refresh(
			application_state&                aState,
			const foundation_refreshed_event& aEvent) -> transition_output
		{
			if (!is_current(aState, aEvent.Context)) {
				return reject_stale_completion();
			}

			aState.GitHubRevision = aEvent.GitHubRevision;
			aState.PendingRefresh.reset();

			transition_output output;
			output.Changed = changed_domain::commands | changed_domain::status
				| changed_domain::authority;
			output.Commands.push_back(refresh_command(true, reason_code::none));
			output.Status = status_delta{ .Message = "Foundation state is current" };
			return output;
		}

		auto fail_refresh(
			application_state&                     aState,
			const foundation_refresh_failed_event& aEvent) -> transition_output
		{
			if (!is_current(aState, aEvent.Context)) {
				return reject_stale_completion();
			}

			aState.PendingRefresh.reset();

			transition_output output;
			output.Changed = changed_domain::commands | changed_domain::status;
			output.Commands.push_back(refresh_command(true, reason_code::none));
			output.Status = status_delta{ .Message = "Foundation refresh failed" };
			output.Diagnostics.push_back(diagnostic{
				.Code   = reason_code::foundation_unavailable,
				.Detail = "The foundation capability was unavailable",
			});
			return output;
		}
	}

	auto resolve_shared_revision(
		const foundation_observation& aObservation) noexcept -> std::uint64_t
	{
		return aObservation.GitHubRevision;
	}

	auto transition(
		application_state&            aState,
		const application_event&      aEvent,
		const foundation_observation& aObservation) -> transition_output
	{
		static_cast<void>(aObservation);

		return std::visit(
			[&aState](const auto& aTypedEvent) -> transition_output {
				using event_type = std::decay_t<decltype(aTypedEvent)>;
				if constexpr (std::is_same_v<event_type, refresh_requested_intent>) {
					return request_refresh(aState);
				} else if constexpr (
					std::is_same_v<event_type, foundation_refreshed_event>) {
					return complete_refresh(aState, aTypedEvent);
				} else {
					return fail_refresh(aState, aTypedEvent);
				}
			},
			aEvent);
	}
}
