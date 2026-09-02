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

	auto stable_name(const agent_continuation aContinuation) noexcept
		-> std::string_view
	{
		switch (aContinuation) {
		case agent_continuation::start_new_session:
			return "start_new_session";
		case agent_continuation::continue_against_old_revision:
			return "continue_against_old_revision";
		}
		return { };
	}

	auto valid_agent_session(const agent_session& aSession) noexcept -> bool
	{
		const auto& binding = aSession.Binding;
		return aSession.Id.value() != 0U && aSession.Profile.value() != 0U
			&& binding.Host.value() != 0U && binding.Repository.value() != 0U
			&& binding.PullRequest.value() != 0U && binding.BaseOid.valid()
			&& binding.HeadOid.valid() && binding.LeftEpoch.value() != 0U
			&& binding.RightEpoch.value() != 0U;
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

	auto offer_agent_continuations(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept
		-> agent_continuation_offer
	{
		const auto status = observe_agent_session(aSession, aObserved);
		return {
			.Status   = status,
			.Required = !status.Current,
			// `Choices` keeps its declared value: the two things that can be
			// done with a session do not depend on why it stopped matching.
			.ContinueAgainst = continue_against_binding(aSession),
		};
	}
} // namespace bpr::core
