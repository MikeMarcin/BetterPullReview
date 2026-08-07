#include "bpr/core/agent_session.hpp"

namespace bpr::core {
	auto stable_name(const agent_session_staleness aStaleness) noexcept
		-> std::string_view
	{
		switch (aStaleness) {
		case agent_session_staleness::current:
			return "current";
		case agent_session_staleness::head_moved:
			return "head_moved";
		case agent_session_staleness::epoch_pair_changed:
			return "epoch_pair_changed";
		case agent_session_staleness::different_review:
			return "different_review";
		}
		return { };
	}

	auto observe_agent_session(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept
		-> agent_session_status
	{
		const auto& pinned = aSession.Binding;
		// Which review comes first. A session bound to another pull request is
		// not stale about this one, it is about something else entirely, and
		// reporting "head moved" would invite a caller to offer the reviewer a
		// refresh that could not mean anything.
		if (
			pinned.Host != aObserved.Host
			|| pinned.Repository != aObserved.Repository
			|| pinned.PullRequest != aObserved.PullRequest) {
			return { .Staleness = agent_session_staleness::different_review };
		}
		if (pinned.HeadOid != aObserved.HeadOid
			|| pinned.BaseOid != aObserved.BaseOid) {
			return { .Staleness = agent_session_staleness::head_moved };
		}
		if (
			pinned.LeftEpoch != aObserved.LeftEpoch
			|| pinned.RightEpoch != aObserved.RightEpoch) {
			return { .Staleness = agent_session_staleness::epoch_pair_changed };
		}
		return {
			.Staleness = agent_session_staleness::current,
			.Current   = true,
		};
	}

	auto may_treat_as_current(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept -> bool
	{
		return observe_agent_session(aSession, aObserved).Current;
	}

	auto continue_against_binding(const agent_session& aSession) noexcept
		-> agent_revision_binding
	{
		// The session's own binding, unconditionally. There is deliberately no
		// path here that could return the observed one: retargeting is what
		// this whole slice exists to prevent, and a function that sometimes
		// retargeted would be the place it crept back in.
		return aSession.Binding;
	}
} // namespace bpr::core
