#include "bpr/core/github_window_read.hpp"

#include <utility>

namespace bpr::core {
	github_window_read::github_window_read(const bpr_window_context     aWindow,
										   const application_session_id aSession)
		: Window{ aWindow }
		, State{ .ApplicationSession = aSession }
	{
	}

	auto github_window_read::adopt(const github_window_session& aSession) -> bool
	{
		const auto& identity = aSession.Identity;
		if (Window.Profile.value() == 0U || Window.Window.value() == 0U
			|| Window.Generation.value() == 0U || State.ApplicationSession.value() == 0U
			|| aSession.Profile != Window.Profile
			|| identity.Context.ApplicationSession.value() == 0U
			|| identity.Context.Profile.value() == 0U
			|| identity.Context.IdentityGeneration.value() <= RetiredGeneration.value()) {
			return false;
		}
		if (Session) {
			return *Session == aSession;
		}
		auto candidate       = github_state{ .ApplicationSession = State.ApplicationSession,
											 .NextRequest        = State.NextRequest,
											 .ActiveProfile      = identity.Context.Profile,
											 .IdentityGeneration = identity.Context.IdentityGeneration,
											 .PendingSession     = identity.Context };
		candidate.AccessMode = aSession.Mode;
		// Establishment already occurred in the profile owner. Reuse its
		// identity validation without exposing any establishment intent.
		candidate.PendingSession->ApplicationSession = State.ApplicationSession;
		auto ready                                   = identity;
		ready.Context                                = *candidate.PendingSession;
		auto output                                  = transition(candidate, ready);
		if (!output.AcceptedSession) {
			return false;
		}
		State   = std::move(candidate);
		Session = aSession;
		output.Requests.clear();
		Adoption = std::move(output);
		return true;
	}

	auto github_window_read::retire(const github_window_session& aSession) -> bool
	{
		if (!Session || *Session != aSession) {
			return false;
		}
		RetiredGeneration = Session->Identity.Context.IdentityGeneration;
		Session.reset();
		Adoption.reset();
		State            = github_state{ .ApplicationSession = State.ApplicationSession,
										 .NextRequest        = State.NextRequest };
		OverviewObserved = false;
		return true;
	}

	auto github_window_read::apply(const bpr_window_context        aWindow,
								   const github_window_read_event& aEvent) -> github_window_read_output
	{
		if (aWindow != Window) {
			github_window_read_output result;
			result.Projection.Diagnostics.push_back({ .Code = github_reason_code::stale_completion });
			return result;
		}
		const auto before   = State;
		const auto previous = State.PendingPullRequest;
		auto       output   = std::visit([this](const auto& aTyped) {
			return transition(State, github_event{ aTyped });
		},
										 aEvent);
		if (State.PendingPullRequest != previous) {
			OverviewObserved = false;
		}
		if (State != before) {
			Adoption.reset();
		}
		github_window_read_output result;
		for (const auto& request : output.Requests) {
			if (const auto* fetch = std::get_if<fetch_pull_request_request>(&request)) {
				result.Fetch = *fetch;
			}
		}
		output.Requests.clear();
		result.Projection = std::move(output);
		return result;
	}

	void github_window_read::advance_request_floor(const request_id aNext) noexcept
	{
		if (aNext.value() > State.NextRequest.value()) {
			State.NextRequest = aNext;
		}
	}
	auto github_window_read::share_snapshot(const github_window_read& aSource, const std::uint64_t aSequence) noexcept -> bool
	{
		const auto& source = aSource.State;
		if (!Session || !aSource.Session || *Session != *aSource.Session
			|| Window.Profile != aSource.Window.Profile
			|| aSequence == 0U || aSequence <= SharedSnapshotSequence
			|| !State.AcceptedSnapshot || !source.AcceptedSnapshot
			|| State.PendingPullRequest || source.PendingPullRequest
			|| State.Repository != source.Repository || State.PullRequest != source.PullRequest
			|| State.ActiveLocator != source.ActiveLocator || State.ActiveNumber != source.ActiveNumber) {
			return false;
		}
		SharedSnapshotSequence = aSequence;
		State.AcceptedSnapshot = source.AcceptedSnapshot;
		return true;
	}
	auto github_window_read::accept_overview_observation(const bpr_window_context            aWindow,
														 const pull_request_request_context& aRequest) noexcept -> bool
	{
		if (aWindow != Window || !Session
			|| State.PendingPullRequest != aRequest || OverviewObserved) {
			return false;
		}
		OverviewObserved = true;
		return true;
	}
	auto github_window_read::adoption() const noexcept -> const std::optional<github_transition_output>& { return Adoption; }
	auto github_window_read::state() const noexcept -> const github_state& { return State; }
	auto github_window_read::window() const noexcept -> bpr_window_context { return Window; }
}
