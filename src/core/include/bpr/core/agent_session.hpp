#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_HPP

#include "bpr/core/agent_profile.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::core {
	struct agent_session_id_tag;

	using agent_session_id = strong_id<agent_session_id_tag>;

	/// \brief The exact revision one agent session was started against.
	///
	/// \details Immutable by construction: every member is part of the value,
	/// and `operator<=>` compares all of them. Two bindings differing in any
	/// field — including the epoch pair — are different bindings, and therefore
	/// different sessions.
	///
	/// The epoch pair is part of the identity rather than a detail of it,
	/// because an agent asked about "what changed since the last review" and
	/// one asked about "what changed in the whole pull request" were asked
	/// different questions even where the head is the same.
	struct agent_revision_binding
	{
		git_host_id     Host;
		repository_id   Repository;
		pull_request_id PullRequest;
		git_commit_oid  BaseOid;
		git_commit_oid  HeadOid;
		epoch_ordinal   LeftEpoch;
		epoch_ordinal   RightEpoch;

		auto operator<=>(const agent_revision_binding&) const = default;
	};

	/// \brief Why a session is no longer current.
	enum class agent_session_staleness : std::uint8_t {
		/// The binding still matches what the reviewer is looking at.
		current,
		/// The pull request's head has moved since the session started.
		head_moved,
		/// The reviewer selected a different epoch pair.
		epoch_pair_changed,
		/// The session is bound to a different review entirely.
		different_review,
	};

	[[nodiscard]] auto stable_name(agent_session_staleness aStaleness) noexcept
		-> std::string_view;

	/// \brief One explicit agent invocation, pinned to what it was asked about.
	///
	/// \details The binding is `const` in effect: nothing in this header
	/// mutates it, and `observe` returns a verdict rather than retargeting.
	///
	/// **Head movement makes a session stale; it never silently retargets it.**
	/// An agent asked about one revision and answering about another would be
	/// answering a question nobody put, and the reviewer would have no way to
	/// tell which revision the answer was about.
	struct agent_session
	{
		agent_session_id       Id;
		agent_profile_id       Profile;
		agent_revision_binding Binding;

		auto operator<=>(const agent_session&) const = default;
	};

	/// \brief Checks whether the supplied session satisfies its contract.
	///
	/// \details A usable session names itself, names the profile it was
	/// started with, and is pinned to a complete revision binding. Stated once
	/// here because two callers already needed it and a second copy is where
	/// the two answers start to differ.
	///
	/// Deliberately not another `valid` overload. "Is this session usable" is
	/// asked in more than one layer, about different things — this one is
	/// about the value being complete, not about what a store will accept —
	/// and one unqualified overload set spanning both is where the two
	/// questions get answered by the wrong function.
	[[nodiscard]] auto valid_agent_session(const agent_session& aSession) noexcept
		-> bool;

	/// \brief What one session may still be used for.
	struct agent_session_status
	{
		agent_session_staleness Staleness = agent_session_staleness::current;
		/// Whether the session may be treated as answering about what the
		/// reviewer is looking at now.
		bool Current = false;

		auto operator<=>(const agent_session_status&) const = default;
	};

	/// \brief Compares a session against the revision now under review.
	///
	/// \details Returns a verdict and changes nothing. The session keeps the
	/// identity it was started with, so a caller that wants an answer about the
	/// new revision has to start a new session — which is a distinct explicit
	/// choice, and visible as one.
	[[nodiscard]] auto observe_agent_session(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept
		-> agent_session_status;

	/// \brief Whether a session may be treated as current.
	///
	/// \details A stale session refuses. Continuing against the revision it was
	/// pinned to is legitimate and is a separate decision the caller makes with
	/// `continue_against_binding`, so that "use the old answer" is never
	/// something that happens by default.
	[[nodiscard]] auto may_treat_as_current(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept -> bool;

	/// \brief The binding a stale session may still be continued against.
	///
	/// \returns The session's own binding, always — a stale session answers
	/// about what it was asked about. Named as a function so that continuing
	/// is a call a reader can find, rather than a field access that looks like
	/// reading current state.
	[[nodiscard]] auto continue_against_binding(
		const agent_session& aSession) noexcept -> agent_revision_binding;

	/// \brief What a reviewer may do with a session that no longer matches.
	///
	/// \details Two for a chat session: ask again about what you are looking
	/// at now, which is a new session with a new binding, or keep talking to
	/// this one about the revision it was asked about. Silently pointing a
	/// session at the new head is still what `agent_session` exists to
	/// prevent, so it is not an enumerator here. The one explicit, announced
	/// way a native conversation follows the head is a separate decision,
	/// `decide_conversation_move` (RFC-BPR-0096, owner continue-at-latest
	/// amendment, 2026-11-24): the workspace moves between turns and the
	/// agent and the transcript are told.
	enum class agent_continuation : std::uint8_t {
		start_new_session,
		continue_against_old_revision,
	};

	/// \brief The number of declared continuations.
	inline constexpr std::size_t agent_continuation_count = 2U;

	static_assert(
		static_cast<std::size_t>(agent_continuation::continue_against_old_revision)
				+ 1U
			== agent_continuation_count,
		"agent_continuation_count must cover every declared continuation");

	[[nodiscard]] auto stable_name(agent_continuation aContinuation) noexcept
		-> std::string_view;

	/// \brief The choices one session offers against the revision now under review.
	///
	/// \details `Choices` is a fixed-size array, so "exactly two" is a
	/// property of the type rather than of a branch that could one day append
	/// to a vector. `Required` says whether the reviewer has to choose at all;
	/// the choices themselves do not change with the answer, because what can
	/// be done with a session does not depend on why it stopped matching.
	///
	/// `ContinueAgainst` is the session's own binding, unconditionally. This
	/// type has no field the observed binding could occupy, which is how
	/// "retargets neither" survives somebody deciding it would be convenient.
	struct agent_continuation_offer
	{
		/// The verdict `observe_agent_session` returned, carried whole. There
		/// is exactly one staleness vocabulary and this is it.
		agent_session_status                                     Status;
		bool                                                     Required = false;
		std::array<agent_continuation, agent_continuation_count> Choices  = {
			agent_continuation::start_new_session,
			agent_continuation::continue_against_old_revision,
		};
		agent_revision_binding ContinueAgainst;

		auto operator<=>(const agent_continuation_offer&) const = default;
	};

	/// \brief Offers the continuations one session has, and retargets nothing.
	///
	/// \details Decided from `observe_agent_session`, whose verdict it carries
	/// rather than recomputing. A current session still reports both choices
	/// and its own binding; `Required` is what says a stale one has to be
	/// answered before the reviewer keeps going.
	[[nodiscard]] auto offer_agent_continuations(
		const agent_session&          aSession,
		const agent_revision_binding& aObserved) noexcept
		-> agent_continuation_offer;

	/// \brief What one saved conversation holds when a turn would resume it.
	///
	/// \details The native session store keeps a conversation pinned to the
	/// revision it started at, apart from the chat session a turn is sent
	/// under, so the two can disagree: a conversation restored without a
	/// runtime handle, a transcript replay, or one chosen in History.
	struct pinned_conversation_facts
	{
		/// The revision the saved conversation is pinned to.
		agent_revision_binding Pinned;
		/// The revision the turn was asked about.
		agent_revision_binding Observed;
		/// The conversation holds something a turn would continue: an
		/// earlier turn, the agent's own session, a retained workspace or a
		/// transcript it replays. One that holds none of them was started
		/// and never used.
		bool Remembers = false;

		auto operator<=>(const pinned_conversation_facts&) const = default;
	};

	/// \brief The ways back one pinned conversation offers.
	///
	/// \details `Pinned` false means the turn goes ahead: the conversation
	/// answers about what it asked, or, with `StartFresh`, it was never used
	/// and a new conversation at the observed revision takes its place without
	/// asking (owner, 2026-11-24). Otherwise the turn is refused and the
	/// reviewer chooses: `ContinueOffered` continues the conversation at
	/// `PinnedHead`, its own revision, and `NewSessionOffered` starts a new
	/// conversation at `ObservedHead`. Neither retargets the conversation.
	struct pinned_conversation_offer
	{
		agent_session_staleness Staleness         = agent_session_staleness::current;
		bool                    Pinned            = false;
		bool                    StartFresh        = false;
		bool                    ContinueOffered   = false;
		bool                    NewSessionOffered = false;
		git_commit_oid          PinnedHead;
		git_commit_oid          ObservedHead;

		auto operator<=>(const pinned_conversation_offer&) const = default;
	};

	/// \brief Owner, 2026-11-24: a refusal that names a way back offers it.
	///
	/// \details Decided from `observe_agent_session`. A conversation at the
	/// observed revision is not pinned. One of this pull request that was
	/// never used holds nothing a turn could be about, so it starts fresh at
	/// the observed revision without asking. Any other pinned conversation
	/// refuses and always offers a new session; it offers to continue at its
	/// own revision only when it is about this pull request (one about another
	/// review cannot answer about this one).
	[[nodiscard]] auto offer_pinned_conversation(const pinned_conversation_facts& aFacts) noexcept
		-> pinned_conversation_offer;

	/// \brief What one finished load of a conversation's saved history
	/// leaves the chat holding.
	enum class conversation_reload : std::uint8_t {
		/// The chat's session is held at the revision under review.
		start_at_observed,
		/// The reviewer's Continue at <old> stands: the chat keeps the
		/// session at the revision it was continued at.
		keep_continuation,
	};

	[[nodiscard]] auto stable_name(conversation_reload aReload) noexcept -> std::string_view;

	/// \brief The facts one history reload is decided from.
	struct conversation_reload_facts
	{
		/// The reviewer chose `continue_against_old_revision`, and the
		/// revision under review has not moved since.
		bool Continued = false;
		/// The saved conversation shown when they chose it.
		std::string ContinuedConversation;
		/// The saved conversation the reload selected.
		std::string Selected;
		/// The reload answers the reviewer's own choice of a conversation:
		/// History's pick, or New session.
		bool ChosenAnew = false;

		auto operator<=>(const conversation_reload_facts&) const = default;
	};

	/// \brief Whether a reload of saved history keeps a Continue choice.
	///
	/// \details History is read again for reasons that are not the
	/// reviewer's choice (a file opened, a turn finished, the agent menu
	/// refreshed). Such a reload of the conversation the reviewer chose to
	/// continue keeps that choice, so the next turn still runs at the
	/// revision they chose. A reload the reviewer asked for, of another
	/// conversation or of a new one, starts at the observed revision, as
	/// does every reload when nothing was continued.
	[[nodiscard]] auto decide_conversation_reload(const conversation_reload_facts& aFacts) noexcept
		-> conversation_reload;
	/// \brief What the host observed of a conversation's workspace before a
	/// move (RFC-BPR-0096, owner continue-at-latest amendment, 2026-11-24).
	enum class conversation_workspace_state : std::uint8_t {
		/// HEAD is the old head and nothing is changed.
		clean,
		/// The working tree or index has changes.
		local_edits,
		/// HEAD has commits past the old head.
		agent_commits,
		/// Both of the above.
		edits_and_commits,
		/// A turn on this workspace is running.
		busy,
		/// The new head's objects could not be brought in.
		objects_unavailable,
	};

	[[nodiscard]] auto stable_name(conversation_workspace_state aState) noexcept -> std::string_view;

	/// \brief How the reviewer reconciles work that stops a move.
	enum class conversation_reconcile : std::uint8_t {
		/// Move only when nothing is in the way.
		none,
		/// Keep edits as a WIP commit, rebase the commits onto the new head;
		/// a conflict aborts back to the pre-move ref.
		rebase,
		/// Keep the work on a saved branch, then move.
		save_and_move,
		/// Keep the work on a saved branch, move, and ask the agent with the
		/// built-in reconcile prompt to bring it onto the new head.
		ask_agent,
	};

	[[nodiscard]] auto stable_name(conversation_reconcile aReconcile) noexcept -> std::string_view;

	/// \brief The facts one move decision is made from.
	struct conversation_move_facts
	{
		agent_revision_binding Pinned;
		agent_revision_binding Observed;
		/// The conversation holds an earlier turn, a runtime session, a
		/// retained workspace or a replay (as `pinned_conversation_facts`).
		bool Remembers = false;
		/// The backend resumes this conversation by its own session handle,
		/// or the conversation is a replay, which carries its own prose.
		bool Resumable = false;
		/// The host's observation of the workspace; `clean` when it has none
		/// (a conversation whose workspace is not retained has nothing to
		/// move).
		conversation_workspace_state Workspace = conversation_workspace_state::clean;
		/// The reviewer's choice on the card; `none` on a plain send.
		conversation_reconcile Reconcile = conversation_reconcile::none;
		/// A rebase onto the observed head stopped on conflicts and was put
		/// back; refused, the same rebase is not offered again.
		bool RebaseConflicted = false;

		auto operator<=>(const conversation_move_facts&) const = default;
	};

	/// \brief What happens to a conversation when a question is sent.
	enum class conversation_move_verdict : std::uint8_t {
		/// At the observed binding already: the turn runs as before.
		current,
		/// The workspace moves to the observed head, then the turn runs.
		move,
		/// Never used: a new conversation at the observed binding takes its
		/// place, then the turn runs.
		start_fresh,
		/// The turn is refused before anything is sent; `Offer` says the ways back.
		refuse,
	};

	[[nodiscard]] auto stable_name(conversation_move_verdict aVerdict) noexcept -> std::string_view;

	struct conversation_move_decision
	{
		conversation_move_verdict    Verdict   = conversation_move_verdict::current;
		conversation_reconcile       Reconcile = conversation_reconcile::none;
		conversation_workspace_state Blocked   = conversation_workspace_state::clean;
		/// The workspace's HEAD changes (the heads differ); false when only
		/// the comparison changed.
		bool MovesHead = false;
		/// For `refuse`: the reconcile actions that may be offered, and the
		/// card's other ways back.
		bool RebaseOffered      = false;
		bool SaveAndMoveOffered = false;
		bool AskAgentOffered    = false;
		bool ContinueOffered    = false;
		bool NewSessionOffered  = false;

		auto operator<=>(const conversation_move_decision&) const = default;
	};

	/// \brief RFC-BPR-0096, owner continue-at-latest amendment, 2026-11-24:
	/// "Yes just move it", "it must be recoverable".
	///
	/// \details In this order: a conversation at the observed binding is
	/// current. One about another review is refused with a new session. One
	/// never used starts fresh. One that cannot be resumed is refused with a
	/// new session and Continue at <old>. A busy workspace, or one whose new
	/// objects are missing, is refused with those two. Edits or agent commits
	/// stop the move unless the reviewer chose a reconcile; refused, they
	/// offer the three reconcile actions as well (no rebase after one
	/// conflicted). Otherwise the conversation
	/// moves, with the reconcile chosen (rebase only when there is work to
	/// carry; a clean workspace moves plainly).
	[[nodiscard]] auto decide_conversation_move(const conversation_move_facts& aFacts) noexcept
		-> conversation_move_decision;

	/// \brief The most commits and files one move note lists.
	inline constexpr std::size_t maximum_move_note_commits = 20U;
	inline constexpr std::size_t maximum_move_note_files   = 40U;

	/// \brief What the host found between the two heads, bounded by it.
	struct conversation_move_history
	{
		/// The new head descends from the old one.
		bool Linear = true;
		/// The objects were available to list; when false the note gives the
		/// commands instead.
		bool Listed = false;
		/// Oldest first: short head and subject.
		std::vector<std::pair<std::string, std::string>> Commits;
		std::size_t                                      CommitCount = 0U;
		std::vector<std::string>                         Files;
		std::size_t                                      FileCount = 0U;
		/// The branch the work was kept on, or the rebase that carried it.
		std::string              SavedBranch;
		std::string              PreMoveRef;
		bool                     Rebased = false;
		std::vector<std::string> Conflicts;

		auto operator<=>(const conversation_move_history&) const = default;
	};

	/// \brief The note the agent's first turn after a move begins with.
	///
	/// \details Names both heads, says earlier observations describe the old
	/// one, lists at most `maximum_move_note_commits` commits and
	/// `maximum_move_note_files` files with how many more, or the commands
	/// that show them when the history is rewritten or was not listed, and
	/// names any saved branch, rebase and pre-move ref. Deterministic in its
	/// inputs.
	[[nodiscard]] auto conversation_move_note(const git_commit_oid& aFrom, const git_commit_oid& aTo, const conversation_move_history& aHistory)
		-> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SESSION_HPP
