#include "bpr/core/home_open.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto in_flight(const std::optional<home_open_state>& aState) noexcept -> bool
		{
			return aState.has_value()
				&& (aState->Phase == home_open_phase::opening || aState->Phase == home_open_phase::waiting);
		}
	} // namespace

	auto apply_home_open(
		const std::optional<home_open_state>& aState,
		const github_transition_output&       aTransition) -> std::optional<home_open_state>
	{
		if (!aTransition.Read.has_value() || aTransition.Read->Refresh) {
			return aState;
		}
		const auto& read = *aTransition.Read;
		if (read.Stage == pull_request_read_stage::pending) {
			return home_open_state{
				.Request    = read.Request,
				.Repository = read.Repository,
				.Number     = read.Number,
				.Phase      = home_open_phase::opening,
			};
		}
		if (!in_flight(aState) || aState->Request != read.Request) {
			return aState;
		}
		switch (read.Stage) {
		case pull_request_read_stage::loaded:
		case pull_request_read_stage::cancelled:
			return std::nullopt;
		case pull_request_read_stage::failed: {
			auto failed   = *aState;
			failed.Phase  = home_open_phase::failed;
			failed.Reason = read.Reason;
			return failed;
		}
		case pull_request_read_stage::pending:
			break;
		}
		return aState;
	}

	auto apply_home_open(
		const std::optional<home_open_state>& aState,
		const home_open_waited&               aWaited) -> std::optional<home_open_state>
	{
		if (!aState.has_value() || aState->Phase != home_open_phase::opening || aState->Request != aWaited.Request) {
			return aState;
		}
		auto waiting  = *aState;
		waiting.Phase = home_open_phase::waiting;
		return waiting;
	}

	auto reconcile_home_open(
		const std::optional<home_open_state>& aState,
		const github_state&                   aLane) -> std::optional<home_open_state>
	{
		if (!in_flight(aState)) {
			return aState;
		}
		if (aLane.PendingPullRequest.has_value() && aLane.PendingPullRequest->Request == aState->Request) {
			return aState;
		}
		return std::nullopt;
	}

	auto home_open_cancellable(const std::optional<home_open_state>& aState) noexcept -> bool
	{
		return aState.has_value() && aState->Phase == home_open_phase::waiting;
	}

	auto admit_home_open_cancel(
		const std::optional<home_open_state>& aState,
		const github_state&                   aLane,
		const request_id                      aRequest) noexcept -> bool
	{
		return home_open_cancellable(aState) && aState->Request == aRequest
			&& aLane.PendingPullRequest.has_value() && aLane.PendingPullRequest->Request == aRequest;
	}

	auto home_open_retryable(const std::optional<home_open_state>& aState) noexcept -> bool
	{
		if (!aState.has_value() || aState->Phase != home_open_phase::failed) {
			return false;
		}
		return aState->Reason != github_reason_code::invalid_repository
			&& aState->Reason != github_reason_code::invalid_pull_request_number
			&& aState->Reason != github_reason_code::invalid_profile;
	}

	auto home_open_in_flight(
		const std::optional<home_open_state>& aState,
		const repository_locator&             aRepository,
		const pull_request_number             aNumber) noexcept -> bool
	{
		return in_flight(aState) && aState->Repository == aRepository && aState->Number == aNumber;
	}

	auto home_loading_yields(const std::span<const github_state> aLanes) noexcept -> bool
	{
		return std::ranges::any_of(aLanes, [](const github_state& aLane) { return aLane.PendingPullRequest.has_value(); });
	}
} // namespace bpr::core
