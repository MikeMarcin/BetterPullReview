/// \file
/// \brief Proves a session stays pinned to what it was asked about.

#include "bpr/core/agent_session.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <tuple>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto binding() -> core::agent_revision_binding
	{
		return {
			.Host        = core::git_host_id{ 10U },
			.Repository  = core::repository_id{ 20U },
			.PullRequest = core::pull_request_id{ 30U },
			.BaseOid     = commit(1U),
			.HeadOid     = commit(2U),
			.LeftEpoch   = core::epoch_ordinal{ 1 },
			.RightEpoch  = core::epoch_ordinal{ 2 },
		};
	}

	[[nodiscard]] auto session() -> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 7U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = binding(),
		};
	}

	/// \brief A moved head makes a session stale, and never retargets it.
	void test_head_movement_makes_a_session_stale()
	{
		const auto pinned = session();
		auto       moved  = binding();
		moved.HeadOid     = commit(9U);

		const auto status = core::observe_agent_session(pinned, moved);
		expect(
			!status.Current
				&& status.Staleness == core::agent_session_staleness::head_moved,
			"a session pinned to one head is stale when the head moves");
		expect(
			pinned.Binding == binding(),
			"and the session still carries the identity it was started with, "
			"rather than adopting the one it was compared against");
		expect(
			core::continue_against_binding(pinned).HeadOid == commit(2U),
			"continuing answers about the revision the agent was asked about");
		expect(
			core::continue_against_binding(pinned).HeadOid != moved.HeadOid,
			"which is not the revision now under review, so the two are "
			"distinguishable rather than quietly the same");
	}

	/// \brief A stale session refuses to be treated as current.
	void test_a_stale_session_refuses_to_be_current()
	{
		const auto pinned = session();
		auto       moved  = binding();
		moved.HeadOid     = commit(9U);

		expect(
			core::may_treat_as_current(pinned, binding()),
			"an unmoved revision is current, so the refusal below is about the "
			"movement and not about the function always refusing");
		expect(
			!core::may_treat_as_current(pinned, moved),
			"a stale session refuses to be treated as current");
		// Continuing is still available, and is a separate call. That is the
		// point: "use the old answer" is never what happens by default.
		expect(
			core::continue_against_binding(pinned) == pinned.Binding,
			"continuing against the old revision remains possible, as a "
			"distinct explicit choice");
	}

	/// \brief Two sessions differing only in epoch pair are different sessions.
	void test_the_epoch_pair_is_part_of_the_identity()
	{
		const auto pinned     = session();
		auto       reselected = binding();
		reselected.RightEpoch = core::epoch_ordinal{ 5 };

		expect(
			reselected.HeadOid == pinned.Binding.HeadOid
				&& reselected.BaseOid == pinned.Binding.BaseOid,
			"the two bindings agree on every revision, so the difference below "
			"is the epoch pair alone");
		expect(
			reselected != pinned.Binding,
			"two bindings differing only in epoch pair are different bindings");
		const auto status = core::observe_agent_session(pinned, reselected);
		expect(
			!status.Current
				&& status.Staleness
					== core::agent_session_staleness::epoch_pair_changed,
			"and a session pinned to one epoch pair is stale against the "
			"other, because it was asked a different question");
	}

	/// \brief A stale session offers two continuations and retargets neither.
	void test_a_stale_session_offers_exactly_two_continuations()
	{
		static_assert(
			core::agent_continuation_count == 2U,
			"there are two things to do with a session that no longer matches. "
			"A third would be retargeting it, which is what the binding exists "
			"to prevent.");

		const auto pinned     = session();
		auto       moved      = binding();
		moved.HeadOid         = commit(9U);
		auto reselected       = binding();
		reselected.RightEpoch = core::epoch_ordinal{ 5 };
		auto elsewhere        = binding();
		elsewhere.PullRequest = core::pull_request_id{ 31U };

		struct staleness_row
		{
			core::agent_revision_binding  Observed;
			core::agent_session_staleness Staleness;
			bool                          Required;
		};
		const auto rows = std::array{
			staleness_row{
				moved, core::agent_session_staleness::head_moved, true },
			staleness_row{
				reselected,
				core::agent_session_staleness::epoch_pair_changed,
				true },
			staleness_row{
				elsewhere,
				core::agent_session_staleness::different_review,
				true },
			staleness_row{
				binding(), core::agent_session_staleness::current, false },
		};

		for (const auto& row : rows) {
			const auto offer = core::offer_agent_continuations(pinned, row.Observed);
			static_assert(
				std::tuple_size_v<decltype(offer.Choices)>
					== core::agent_continuation_count,
				"the offer names exactly two continuations, and the array is "
				"what says so rather than a branch that could append a third");
			expect(
				offer.Status
					== core::observe_agent_session(pinned, row.Observed),
				"the offer carries the verdict observe_agent_session returned, "
				"so there is one staleness vocabulary and not two");
			expect(
				offer.Status.Staleness == row.Staleness
					&& offer.Required == row.Required,
				"a session that no longer matches requires a choice, and a "
				"current one does not");
			expect(
				offer.Choices[0] == core::agent_continuation::start_new_session
					&& offer.Choices[1]
						== core::agent_continuation::continue_against_old_revision
					&& offer.Choices[0] != offer.Choices[1],
				"the two choices are asking again about what you are looking at, "
				"and keeping this answer about what it was asked");
			expect(
				offer.ContinueAgainst == pinned.Binding
					&& offer.ContinueAgainst
						== core::continue_against_binding(pinned),
				"and continuing answers about the session's own binding");
			expect(
				row.Observed == pinned.Binding
					|| offer.ContinueAgainst != row.Observed,
				"which is never the revision now under review, so offering the "
				"choice does not quietly take it");
			expect(
				pinned.Binding == binding(),
				"offering a continuation retargets nothing: the session still "
				"carries the identity it was started with");
			expect(
				!core::stable_name(offer.Choices[0]).empty()
					&& !core::stable_name(offer.Choices[1]).empty(),
				"both continuations have stable names a drawer can label");
		}
	}

	/// \brief A session about another review is not stale, it is elsewhere.
	void test_another_review_is_not_staleness()
	{
		const auto pinned = session();
		auto       other  = binding();
		other.PullRequest = core::pull_request_id{ 31U };
		const auto status = core::observe_agent_session(pinned, other);
		expect(
			status.Staleness == core::agent_session_staleness::different_review,
			"a session bound to another review reports that, rather than "
			"reporting a moved head and inviting a refresh that could not "
			"mean anything");
	}
	/// \brief Owner, 2026-11-24: a refusal that names a way back offers it.
	/// A pinned conversation offers a new session always, and continuing at
	/// its own revision when it is about this review. One of this review that
	/// was never used starts fresh at the observed revision without asking.
	void test_a_pinned_conversation_offers_what_it_names()
	{
		auto moved            = binding();
		moved.HeadOid         = commit(9U);
		auto reselected       = binding();
		reselected.LeftEpoch  = core::epoch_ordinal{ 2 };
		auto elsewhere        = binding();
		elsewhere.PullRequest = core::pull_request_id{ 31U };

		struct row
		{
			core::agent_revision_binding  Observed;
			bool                          Remembers;
			core::agent_session_staleness Staleness;
			bool                          Pinned;
			bool                          Continue;
			bool                          Fresh;
		};
		const auto rows = std::array{
			row{ binding(), true, core::agent_session_staleness::current, false, false, false },
			row{ binding(), false, core::agent_session_staleness::current, false, false, false },
			row{ moved, true, core::agent_session_staleness::head_moved, true, true, false },
			row{ moved, false, core::agent_session_staleness::head_moved, false, false, true },
			row{ reselected, true, core::agent_session_staleness::epoch_pair_changed, true, true, false },
			row{ reselected, false, core::agent_session_staleness::epoch_pair_changed, false, false, true },
			row{ elsewhere, true, core::agent_session_staleness::different_review, true, false, false },
			row{ elsewhere, false, core::agent_session_staleness::different_review, true, false, false },
		};
		for (const auto& expected : rows) {
			const auto facts = core::pinned_conversation_facts{ .Pinned = binding(), .Observed = expected.Observed, .Remembers = expected.Remembers };
			const auto offer = core::offer_pinned_conversation(facts);
			expect(offer == core::offer_pinned_conversation(facts), "equal facts give an equal offer");
			expect(offer.Staleness == expected.Staleness && offer.Staleness == core::observe_agent_session(session(), expected.Observed).Staleness,
				   "the offer carries observe_agent_session's verdict");
			expect(offer.Pinned == expected.Pinned, "only a conversation at another binding is pinned");
			expect(offer.NewSessionOffered == expected.Pinned, "a pinned conversation always offers a new session, and a current one offers nothing");
			expect(offer.StartFresh == expected.Fresh && !(offer.StartFresh && offer.Pinned),
				   "a conversation of this review never used starts fresh without asking, and only it");
			expect(offer.ContinueOffered == expected.Continue,
				   "continuing is offered only for a used conversation about this review");
			expect(offer.PinnedHead == binding().HeadOid && offer.ObservedHead == expected.Observed.HeadOid,
				   "the offer names the conversation's head and the observed one, so a button can say which");
		}
	}
	/// RFC-BPR-0096, owner continue-at-latest amendment, 2026-11-24: "Yes just
	/// move it", "it must be recoverable".
	void test_a_conversation_moves_or_says_why_not()
	{
		auto moved            = binding();
		moved.HeadOid         = commit(9U);
		auto reselected       = binding();
		reselected.LeftEpoch  = core::epoch_ordinal{ 2 };
		auto elsewhere        = binding();
		elsewhere.PullRequest = core::pull_request_id{ 31U };
		using state           = core::conversation_workspace_state;
		using reconcile       = core::conversation_reconcile;
		using verdict         = core::conversation_move_verdict;
		const auto decide     = [](const core::agent_revision_binding& aObserved, const bool aRemembers, const bool aResumable, const state aState, const reconcile aReconcile) {
			return core::decide_conversation_move({ .Pinned = binding(), .Observed = aObserved, .Remembers = aRemembers, .Resumable = aResumable, .Workspace = aState, .Reconcile = aReconcile });
		};

		expect(decide(binding(), true, true, state::clean, reconcile::none).Verdict == verdict::current, "a conversation at the observed binding is current");
		const auto plain = decide(moved, true, true, state::clean, reconcile::none);
		expect(plain.Verdict == verdict::move && plain.MovesHead && plain.Reconcile == reconcile::none, "a clean, resumable conversation moves on a plain send");
		expect(plain == decide(moved, true, true, state::clean, reconcile::none), "equal facts, equal decision");
		const auto epochs = decide(reselected, true, true, state::agent_commits, reconcile::none);
		expect(epochs.Verdict == verdict::move && !epochs.MovesHead && epochs.Reconcile == reconcile::none,
			   "only the comparison changed: nothing in the workspace moves, so its work is no obstacle");
		expect(decide(moved, false, false, state::clean, reconcile::none).Verdict == verdict::start_fresh, "one never used starts fresh");
		const auto other = decide(elsewhere, true, true, state::clean, reconcile::none);
		expect(other.Verdict == verdict::refuse && other.NewSessionOffered && !other.ContinueOffered && !other.RebaseOffered, "another review offers a new session only");
		const auto unresumable = decide(moved, true, false, state::clean, reconcile::none);
		expect(unresumable.Verdict == verdict::refuse && unresumable.NewSessionOffered && unresumable.ContinueOffered && !unresumable.AskAgentOffered,
			   "one the backend cannot resume offers a new session and continuing at its own head");
		for (const auto blocked : { state::busy, state::objects_unavailable }) {
			const auto refused = decide(moved, true, true, blocked, reconcile::rebase);
			expect(refused.Verdict == verdict::refuse && refused.Blocked == blocked && refused.ContinueOffered && !refused.RebaseOffered,
				   "a busy workspace, or missing objects, is never moved, whatever was chosen");
		}
		for (const auto work : { state::local_edits, state::agent_commits, state::edits_and_commits }) {
			const auto stopped = decide(moved, true, true, work, reconcile::none);
			expect(stopped.Verdict == verdict::refuse && stopped.Blocked == work && stopped.RebaseOffered && stopped.SaveAndMoveOffered && stopped.AskAgentOffered
					   && stopped.ContinueOffered && stopped.NewSessionOffered,
				   "work in the way stops the plain move and offers every way to reconcile it");
			for (const auto chosen : { reconcile::rebase, reconcile::save_and_move, reconcile::ask_agent }) {
				const auto reconciled = decide(moved, true, true, work, chosen);
				expect(reconciled.Verdict == verdict::move && reconciled.Reconcile == chosen, "a chosen reconcile moves with it");
			}
		}
		expect(decide(moved, true, true, state::clean, reconcile::rebase).Reconcile == reconcile::none, "a clean workspace has nothing to reconcile");
		const auto conflicted = core::decide_conversation_move({ .Pinned = binding(), .Observed = moved, .Remembers = true, .Resumable = true, .Workspace = state::agent_commits, .RebaseConflicted = true });
		expect(conflicted.Verdict == verdict::refuse && !conflicted.RebaseOffered && conflicted.SaveAndMoveOffered && conflicted.AskAgentOffered,
			   "after a rebase conflicted, the same rebase is not offered again");
		for (const auto name : { core::stable_name(verdict::refuse), core::stable_name(reconcile::ask_agent), core::stable_name(state::edits_and_commits) }) {
			expect(!name.empty(), "each value has a stable name");
		}
	}

	/// The agent is told where it is, what changed and how to get its work back.
	void test_the_move_note_says_what_happened()
	{
		const auto from    = core::canonical_hex(commit(2U));
		const auto to      = core::canonical_hex(commit(9U));
		auto       history = core::conversation_move_history{ .Linear = true, .Listed = true };
		for (auto index = 0U; index < 25U; ++index) {
			history.Commits.emplace_back("c" + std::to_string(index), "subject " + std::to_string(index));
		}
		history.CommitCount = 25U;
		history.Files       = { "src/a.cpp", "src/b.cpp" };
		history.FileCount   = 2U;
		history.PreMoveRef  = "refs/bpr/pre-move/x";
		const auto note     = core::conversation_move_note(commit(2U), commit(9U), history);
		expect(note == core::conversation_move_note(commit(2U), commit(9U), history), "the note is deterministic");
		expect(note.find(std::string{ from.view() }) != std::string::npos && note.find(std::string{ to.view() }) != std::string::npos
				   && note.find("earlier tool observations describe") != std::string::npos,
			   "it names both heads and what the earlier observations describe");
		expect(note.find("- c19 subject 19") != std::string::npos && note.find("- c20 ") == std::string::npos && note.find("and 5 more") != std::string::npos,
			   "it lists at most twenty commits and says how many more");
		expect(note.find("- src/b.cpp") != std::string::npos && note.find("git log") == std::string::npos && note.find("refs/bpr/pre-move/x") != std::string::npos,
			   "listed history needs no commands; the pre-move ref is named");

		auto       rewritten = core::conversation_move_history{ .Linear = false, .Listed = true, .SavedBranch = "bpr/saved/c/abc1234", .Conflicts = { "src/a.cpp" } };
		const auto forced    = core::conversation_move_note(commit(2U), commit(9U), rewritten);
		expect(forced.find("history was rewritten") != std::string::npos && forced.find("git log --oneline") != std::string::npos
				   && forced.find("bpr/saved/c/abc1234") != std::string::npos && forced.find("- src/a.cpp") != std::string::npos,
			   "a rewritten history gives the commands, the saved branch and the conflicting files");
		const auto unlisted = core::conversation_move_note(commit(2U), commit(9U), core::conversation_move_history{ .Linear = true, .Listed = false });
		expect(unlisted.find("git diff --stat") != std::string::npos && unlisted.find("Commits between") == std::string::npos, "unlisted history gives the commands");
	}
	/// A Continue at <old> survives the history reloads the reviewer did not
	/// ask for, and only those.
	void test_a_reload_keeps_a_continue_choice()
	{
		using reload     = core::conversation_reload;
		const auto keeps = core::conversation_reload_facts{ .Continued = true, .ContinuedConversation = "kept", .Selected = "kept", .ChosenAnew = false };
		expect(core::decide_conversation_reload(keeps) == reload::keep_continuation, "a reload of the continued conversation keeps the choice");
		expect(core::decide_conversation_reload(keeps) == core::decide_conversation_reload(keeps), "equal facts, equal decision");
		auto chosen       = keeps;
		chosen.ChosenAnew = true;
		expect(core::decide_conversation_reload(chosen) == reload::start_at_observed, "History's pick or New session starts at the observed revision");
		auto other     = keeps;
		other.Selected = "new";
		expect(core::decide_conversation_reload(other) == reload::start_at_observed, "another conversation does not inherit the choice");
		auto none      = keeps;
		none.Continued = false;
		expect(core::decide_conversation_reload(none) == reload::start_at_observed, "with nothing continued a reload starts at the observed revision");
		expect(core::decide_conversation_reload({ }) == reload::start_at_observed, "the empty facts continue nothing");
		expect(core::stable_name(reload::keep_continuation) == "keep_continuation" && core::stable_name(reload::start_at_observed) == "start_at_observed",
			   "each verdict has a stable name");
	}
} // namespace

auto main() -> int
{
	test_head_movement_makes_a_session_stale();
	test_a_stale_session_refuses_to_be_current();
	test_the_epoch_pair_is_part_of_the_identity();
	test_a_stale_session_offers_exactly_two_continuations();
	test_another_review_is_not_staleness();
	test_a_pinned_conversation_offers_what_it_names();
	test_a_conversation_moves_or_says_why_not();
	test_the_move_note_says_what_happened();
	test_a_reload_keeps_a_continue_choice();
	std::cout << "agent-session-tests:pass\n";
	return EXIT_SUCCESS;
}
