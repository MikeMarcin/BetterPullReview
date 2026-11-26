#include "bpr/core/agent_session.hpp"

#include <algorithm>
#include <string>

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

	auto offer_pinned_conversation(const pinned_conversation_facts& aFacts) noexcept
		-> pinned_conversation_offer
	{
		const auto status = observe_agent_session(agent_session{ .Id = { }, .Profile = { }, .Binding = aFacts.Pinned }, aFacts.Observed);
		auto       offer  = pinned_conversation_offer{
			.Staleness    = status.Staleness,
			.PinnedHead   = aFacts.Pinned.HeadOid,
			.ObservedHead = aFacts.Observed.HeadOid,
		};
		if (status.Current) {
			return offer;
		}
		const auto thisReview = status.Staleness != agent_session_staleness::different_review;
		if (!aFacts.Remembers && thisReview) {
			offer.StartFresh = true;
			return offer;
		}
		offer.Pinned            = true;
		offer.NewSessionOffered = true;
		offer.ContinueOffered   = thisReview;
		return offer;
	}

	auto stable_name(const conversation_reload aReload) noexcept -> std::string_view
	{
		switch (aReload) {
		case conversation_reload::start_at_observed: return "start_at_observed";
		case conversation_reload::keep_continuation: return "keep_continuation";
		}
		return { };
	}

	auto decide_conversation_reload(const conversation_reload_facts& aFacts) noexcept -> conversation_reload
	{
		const auto same = aFacts.Selected == aFacts.ContinuedConversation;
		return aFacts.Continued && !aFacts.ChosenAnew && same ? conversation_reload::keep_continuation : conversation_reload::start_at_observed;
	}
	auto stable_name(const conversation_workspace_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case conversation_workspace_state::clean:               return "clean";
		case conversation_workspace_state::local_edits:         return "local_edits";
		case conversation_workspace_state::agent_commits:       return "agent_commits";
		case conversation_workspace_state::edits_and_commits:   return "edits_and_commits";
		case conversation_workspace_state::busy:                return "busy";
		case conversation_workspace_state::objects_unavailable: return "objects_unavailable";
		}
		return { };
	}

	auto stable_name(const conversation_reconcile aReconcile) noexcept -> std::string_view
	{
		switch (aReconcile) {
		case conversation_reconcile::none:          return "none";
		case conversation_reconcile::rebase:        return "rebase";
		case conversation_reconcile::save_and_move: return "save_and_move";
		case conversation_reconcile::ask_agent:     return "ask_agent";
		}
		return { };
	}

	auto stable_name(const conversation_move_verdict aVerdict) noexcept -> std::string_view
	{
		switch (aVerdict) {
		case conversation_move_verdict::current:     return "current";
		case conversation_move_verdict::move:        return "move";
		case conversation_move_verdict::start_fresh: return "start_fresh";
		case conversation_move_verdict::refuse:      return "refuse";
		}
		return { };
	}

	auto decide_conversation_move(const conversation_move_facts& aFacts) noexcept -> conversation_move_decision
	{
		const auto status   = observe_agent_session(agent_session{ .Id = { }, .Profile = { }, .Binding = aFacts.Pinned }, aFacts.Observed);
		auto       decision = conversation_move_decision{ .MovesHead = aFacts.Pinned.HeadOid != aFacts.Observed.HeadOid };
		const auto refuse   = [&decision](const conversation_workspace_state aBlocked, const bool aContinue) {
			decision.Verdict           = conversation_move_verdict::refuse;
			decision.Blocked           = aBlocked;
			decision.ContinueOffered   = aContinue;
			decision.NewSessionOffered = true;
			return decision;
		};
		if (status.Current) {
			decision.MovesHead = false;
			return decision;
		}
		if (status.Staleness == agent_session_staleness::different_review) {
			decision.MovesHead = false;
			return refuse(conversation_workspace_state::clean, false);
		}
		if (!aFacts.Remembers) {
			decision.Verdict = conversation_move_verdict::start_fresh;
			return decision;
		}
		if (!aFacts.Resumable) {
			return refuse(conversation_workspace_state::clean, true);
		}
		const auto state = aFacts.Workspace;
		if (state == conversation_workspace_state::busy || state == conversation_workspace_state::objects_unavailable) {
			return refuse(state, true);
		}
		const auto work = state != conversation_workspace_state::clean;
		if (work && decision.MovesHead && aFacts.Reconcile == conversation_reconcile::none) {
			refuse(state, true);
			decision.RebaseOffered      = !aFacts.RebaseConflicted;
			decision.SaveAndMoveOffered = true;
			decision.AskAgentOffered    = true;
			return decision;
		}
		decision.Verdict   = conversation_move_verdict::move;
		decision.Reconcile = work && decision.MovesHead ? aFacts.Reconcile : conversation_reconcile::none;
		return decision;
	}

	auto conversation_move_note(const git_commit_oid& aFrom, const git_commit_oid& aTo, const conversation_move_history& aHistory) -> std::string
	{
		const auto from = std::string{ canonical_hex(aFrom).view() };
		const auto to   = std::string{ canonical_hex(aTo).view() };
		auto       note = "Note from BetterPullReview: the pull request moved from " + from + " to " + to
			+ " since your last turn, and your working directory is now checked out at " + to
			+ ". Your earlier tool observations describe " + from + ".";
		if (!aHistory.Linear) {
			note += " The history was rewritten (" + to + " does not descend from " + from + ").";
		}
		if (aHistory.Listed && aHistory.Linear) {
			note += "\n\nCommits between, oldest first (" + std::to_string(aHistory.CommitCount) + "):";
			for (std::size_t index = 0U; index < aHistory.Commits.size() && index < maximum_move_note_commits; ++index) {
				note += "\n- " + aHistory.Commits[index].first + " " + aHistory.Commits[index].second;
			}
			const auto listedCommits = std::min(aHistory.Commits.size(), maximum_move_note_commits);
			if (aHistory.CommitCount > listedCommits) {
				note += "\n- and " + std::to_string(aHistory.CommitCount - listedCommits) + " more";
			}
		}
		if (aHistory.Listed) {
			note += "\n\nFiles changed between them (" + std::to_string(aHistory.FileCount) + "):";
			for (std::size_t index = 0U; index < aHistory.Files.size() && index < maximum_move_note_files; ++index) {
				note += "\n- " + aHistory.Files[index];
			}
			const auto listedFiles = std::min(aHistory.Files.size(), maximum_move_note_files);
			if (aHistory.FileCount > listedFiles) {
				note += "\n- and " + std::to_string(aHistory.FileCount - listedFiles) + " more";
			}
		}
		if (!aHistory.Listed || !aHistory.Linear) {
			note += "\n\nTo see what changed: git log --oneline " + from + ".." + to + " and git diff --stat " + from + " " + to + ".";
		}
		if (aHistory.Rebased) {
			note += "\n\nYour commits from before the move were rebased onto " + to + ".";
		}
		if (!aHistory.SavedBranch.empty()) {
			note += "\n\nYour work from before the move is kept on the branch " + aHistory.SavedBranch + ".";
		}
		if (!aHistory.Conflicts.empty()) {
			note += "\n\nThese files conflicted:";
			for (const auto& file : aHistory.Conflicts) {
				note += "\n- " + file;
			}
		}
		if (!aHistory.PreMoveRef.empty()) {
			note += "\n\nThe workspace as it was before the move is at " + aHistory.PreMoveRef + ".";
		}
		return note;
	}
} // namespace bpr::core
