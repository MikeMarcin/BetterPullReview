---
schema: "implement-rfc.card.v1"
id: "BPR-2379"
title: "A native conversation pinned to an earlier revision offers Continue at <rev> and Start a new session on its refusal"
status: "Done"
rfcs: ["RFC-BPR-0012", "RFC-BPR-0096"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2379--BPR-2379--768bba33a21b.json","related":["BPR-2138","BPR-0532","BPR-2000","BPR-1876","BPR-2364"],"acceptance":["BPR-2379:core","BPR-2379:host","BPR-2379:qt","BPR-2379:course"]}
---

# BPR-2379: A pinned native conversation offers the ways back its refusal names

## Report

The owner, 2026-11-24, with a screenshot of the Agent tab (Native
continuation, agent declawd-cc, PR #197169 at 50e109d, after Review this
pull request): "There doesn't appear to be an option anywhere to continue
like it suggests". The stopped card said "This native session is pinned to
an earlier revision. Explicitly continue that revision or start a new
session. Your message is kept." and offered only Resend and Hide the log.
The turn showed the same words.

## Cause

Two records decide which revision a turn is about, and they can disagree.

- The chat session (`agent_chat_session_host`) decides whether the Agent tab
  shows a moved head. When that session is stale, the tab's head-moved card
  and the History menu offer Start a new session and Continue against the
  old revision.
- The native conversation the store selects for the agent
  (`native_agent_session_store_port::select`) is pinned to the revision it
  started at. `agent_invocation_host` refuses a turn whose revision is
  different.

Loading native history sets the chat session to the head under review
(`reset_conversation`). The pinned revision then reaches the chat only
through `restore_native_session`, and only for a conversation that has both
a retained workspace and a runtime handle. Three kinds of conversation leave
the chat current while the native conversation is pinned: a transcript
replay, a conversation whose turn ended before the agent named its session,
and one started with New session and never used. Ask then passes the chat
check and the host refuses the turn. Before this card:

- The refusal named two actions, and the stopped card offered neither. Its
  Resend ran into the same refusal. It is wired to the launch start gesture,
  not to the kept message. The turn's own Resend was refused the same way.
- History's Start a new session and Continue against the old revision were
  disabled, because the chat offered no choice.
- The New session icon beside History did start a conversation at the
  current head, but it did not send the kept message, and nothing pointed
  to it.
- No control could continue the pinned revision.

Also found: the chat's own start-a-new-session path set
`NativeStartAtCurrentRevision`. The host then refused the new conversation,
because it was not the one History last selected ("The selected
continuation changed").

## Fix

Owner decisions, 2026-11-24, taken through the coordinator: when the head has
moved, offer the choice before sending; a never-used conversation moves to
the latest commit without asking; the stopped card's Resend resends the
message.

- Core: `core::offer_pinned_conversation(pinned_conversation_facts)`
  decides from `observe_agent_session`.
  - A conversation at the observed binding is not pinned.
  - A conversation of this pull request that was never used (`StartFresh`)
    moves to the observed revision without asking.
  - Any other pinned conversation is refused and offers a new session. It
    also offers to continue at its own revision when it is about this pull
    request.

  Memory means an earlier turn, a runtime handle, a retained workspace or a
  replay. `agent_turn_facts::RevisionChoice` removes the refused turn's own
  Resend.
- Application:
  - When history loads, a used conversation pinned elsewhere that has no
    runtime session to restore (a replay, or a turn that ended before the
    agent named its session) is told to the chat
    (`pin_native_conversation`). The tab then shows its head-moved card
    before anything is sent.
  - A never-used conversation is replaced by a new one at the observed
    binding, and the turn runs.
  - A turn that still reaches a pinned conversation is refused with
    `agent_run_result::PinnedConversation` and `PinnedSession`. The reason
    names exactly the offered actions: "This conversation is pinned to an
    earlier revision, <pinned>. Continue at <pinned> to keep asking about
    that revision, or start a new session at <current>. No question was
    sent."
  - The host now accepts the new conversation that a start at the current
    revision makes; before this, the chat's own Start a new session was
    refused with "The selected continuation changed".
- Composition:
  - The chat takes the pinned revision. `choose_pinned_conversation` takes
    the refusal card's choice: Continue moves the chat to the pinned session
    and chooses `continue_against_old_revision` (BPR-0532), and Start a new
    session starts a conversation at the current revision.
  - A start gesture continues a chosen old revision too.
  - The window's selected conversation follows a new conversation a run
    started.
- Qt:
  - The refusal card shows "Continue at <short pinned head>" and "Start a
    new session" in place of Resend and the log toggle, and each sends the
    kept message.
  - Elsewhere the stopped card's Resend sends the kept message again, as the
    turn's Resend does, instead of firing the launch start gesture.
- Docs: `docs/ux/ai-experience.md` ("What the two continuations are").

Not done in the first slices, then adopted (see "Continue at the latest head" below): "Continue at <new head>". The owner
wants to keep the conversation's memory and move it to the latest commit.
RFC-BPR-0096 forbids moving a native conversation's workspace or working
directory ("Never move the old workspace underneath a native conversation
or silently relabel its tool observations as current"; "Refresh never moves
HEAD, resets edits, or silently changes an old native session's working
directory"). `core::agent_continuation` records that a third choice, "keep
this session, point it at the new head", is the retargeting it exists to
prevent. Doing it needs an RFC amendment, or a Transcript replay at the new
head, which keeps the prose memory without the agent's tool memory.

## Acceptance

- **BPR-2379:core** `core_agent_session`
  (`test_a_pinned_conversation_offers_what_it_names`) covers current, head
  moved, epoch pair changed and another review, each with and without
  memory. `core_agent_turn` (`test_error_actions`) checks that a turn
  waiting on a revision choice has no recovery of its own and keeps Copy and
  Delete. Passed at e92dfa069.
- **BPR-2379:host** `application_agent_invocation_host`
  (`test_a_pinned_conversation_offers_its_ways_back`):
  - A used conversation at an earlier head refuses the turn with both
    actions named, before any workspace or process starts.
  - Continuing with its `PinnedSession` resumes its own handle at its own
    revision.
  - A start at the current revision runs in a new conversation, which the
    result names.
  - A never-used conversation runs the turn in a new conversation at the
    current head.
  - Loading history says a used pinned conversation without a runtime
    session to the chat, and does not say a never-used one.

  Passed at e92dfa069.
- **BPR-2379:qt** `qt_workbench_agent_send_refusal`
  (`test_a_pinned_conversation_card_offers_what_it_names`):
  - The refusal card shows Continue at 3333333 and Start a new session, its
    text names both, and no Resend is shown.
  - Each button reaches the host and resends the kept message, and a choice
    the host declines sends nothing.
  - An ordinary failure's Resend sends the kept message.

  Passed at e92dfa069. `qt_workbench_window_scale_100` also passed at
  e92dfa069.
- **BPR-2379:course** `application_agent_invocation_native_pinned_contract`
  (fixture agents only; no installed agent starts). Before the head moves,
  the first agent's conversation and the second agent's conversation share
  one checkout, and three transcript replays are made. At e92dfa069, after the
  head moves:
  1. A replay with no checkout of its own is offered Start a new session and
     Continue at the earlier head when opened, before any question; no
     process starts.
  2. The first conversation follows the head (no choice first). An untracked
     file in its checkout refuses the send with Rebase onto <new>, Keep my
     changes on a branch and move, Ask <agent> to reconcile, Continue at
     <old> and Start a new session, and no process starts.
  3. Continue at <old> answers at the old head; nothing moves and the edit
     stays. Start a new session answers at the new head, twice.
  4. A line appended to the tracked file makes Rebase conflict: the refusal
     names the file, Rebase is not offered again, and both edits are still
     there.
  5. Keep my changes on a branch and move: the agent's input has the move
     note (both heads, "history was rewritten") before the question, its
     checkout is at the new head, the saved branch exists, and the reply
     card says "Updated to <new> — history rewritten. Your changes are kept
     on bpr/saved/…". Exactly one process starts.
  6. The second agent's never-used conversation moves silently and answers.
  7. The second agent's conversation on the moved checkout, opened, shows
     "Updated to <new> — this pull request's checkout moved …", has no turn
     it did not ask, and its next question carries the note once (owner:
     "The other sessions, we dont wake them up and send them messages").
  8. A replay continued at the old head makes its own checkout there; read
     again it follows the head, and a plain send moves it ("Updated to <new>
     — history rewritten", note before the question, no saved branch).
  9. Rebase onto <new> with an untracked file moves and keeps the file.
  10. Ask <agent> to reconcile moves with the saved branch present and the
      built-in reconcile prompt after the note and before the question.

  Passed at e92dfa069 (about 25 s).
- Also passing at e92dfa069: `application_native_agent_sessions` (rerun),
  `core_agent_turn`, `coding-format-check`, clang-tidy on the touched
  translation units (the only findings are two existing ones in
  `agent_tab_view.cpp`).

## Found on the way

- **Fixed here.** A continued turn (Continue against the old revision) never
  showed its answer. `agent_invocation_host::current_matches` dropped every
  stream frame and start projection whose binding differed from the host's,
  but a continuation's binding differs by design (BPR-0521; `supersede`
  already lets it run). It now admits a continued invocation's frames while
  the review is the same. `application_agent_invocation_host` checks that the
  continued answer streams.
- **Fixed elsewhere.** After a head move the agent runtime could hold a
  different epoch pair from the window's, so the next turn at the new head was
  refused or ended `stale_result`. BPR-2378 fixed it on main; this lane is
  rebased onto it and the course runs through.
- **Fixed here (slice 4).** A native connection kept between turns holds the
  checkout's lease, so every move found it busy. The host now ends the idle
  connections working in the checkout before moving it
  (`agent_process_port::release_native_connections`); one answering a turn
  keeps it busy.
- **Fixed here (slice 4).** Continue at <old> from the refusal card failed to
  start: the refusal named the checkout's workspace session, and a checkout
  Agent setup's check made carries one from the probes' reserved range, which
  `valid_run_provenance` refuses. The refusal names the turn's own session,
  and the window's chat keeps its own session when a restored one is in that
  range.
- **Proposed amendment.**
  `docs/rfcs/amendments/RFC-BPR-0096-continue-at-latest-r1.md`: "Continue at
  <new>" moves the conversation's own workspace forward between turns, like a
  `git pull` (owner, 2026-11-24). Two questions for the owner are open there.

## Continue at the latest head (RFC-BPR-0096 AC13, adopted 2026-11-24)

Adopted into RFC-BPR-0096 as the "Owner continue-at-latest amendment,
2026-11-24". The design and the as-built notes are in
`docs/rfcs/amendments/RFC-BPR-0096-continue-at-latest-r1.md`.

- **Slice 1, done.** Core: `decide_conversation_move` and
  `conversation_move_note` (`core_agent_session`).
- **Slice 2a, done.** Git adapter:
  `git_process_port::move_agent_workspace` and crash recovery inside
  `inspect_agent_workspace`. The journal gains `PreviousHead` and a 7→7
  head-only transition. The move's steps are: validate, observe, import the
  new head's and base's objects from the review cache, keep the work (WIP
  commit, `refs/bpr/pre-move/<old>`, saved branch), write
  `refs/bpr/move-target`, check out or rebase (a conflict aborts, restores
  the workspace and names the files), append the journal line, write the
  marker, delete the refs, and return the history. Sparse workspaces are
  refused as `object_unavailable`. A busy record changes nothing.
  `git_process_contract` (`run_workspace_move_contract`, `--workspace-move`)
  covers:
  - a clean move, plain and borrowing a clone;
  - edits and agent commits left untouched without a reconcile;
  - a rebase that succeeds;
  - a rebase that conflicts, put back with the file named;
  - save and move, and ask the agent;
  - missing objects, and a sparse workspace;
  - busy;
  - a crash at each of `move-after-wip`, `-target`, `-checkout`, `-journal`
    and `-marker`.

  Passed at e92dfa069. `agent_workspace_ownership` and `core_agent_session`
  also passed.
- **Slice 2b, done.**
  - The move is routed through `agent_workspace_port` and
    `epoch_application_host` on the review's actor, and through
    `git_cache_port` with the installed cache's authority checks.
  - `native_agent_session_store_port::move_conversation` writes the binding,
    marker and identities, and moves the shared-workspace key, in one
    revision. It refuses a stale revision and a shared tree.

  `sqlite_agent_session_management` (`move_a_conversation`) and the move
  course passed at e92dfa069.
- **Slice 3, done** (amendment, "Slice 3 as built"). The host moves a
  resumable conversation's complete retained workspace on send, under the
  old key's and path's leases with no other holder, records it with
  `move_conversation`, and runs the turn after the move note (and the
  built-in reconcile prompt for Ask the agent). Not moved: refused with the
  core's decision (reconcile actions), conflicting files, Continue and New
  session. One move behind is moved back through the same path. History
  load no longer pins a conversation a send would move. Qt: the stopped card
  offers Rebase onto <new>, Keep my changes on a branch and move, Ask <agent>
  to reconcile, Continue at <old>, Start a new session; a moved turn shows
  "Updated to <sha> — <n> commits". Tests: `application_agent_invocation_host`
  (`test_a_conversation_moves_to_the_latest_head`), `core_agent_session`,
  `qt_workbench_agent_send_refusal`.
- **Slice 4, done.** Rebased onto main at `75b0e02e4` (BPR-2378). Owner
  decisions through the coordinator: "a session in the same PR? yes single
  checkout" and "The other sessions, we dont wake them up and send them
  messages".
  - Store: `move_conversation` rebinds every other conversation on the
    checkout in the same transaction and records the head each was at
    (`native_agent_moved_conversations`, `MovedFrom`); only the pull
    request's kept workspace refuses (`KeptHolders`). A conversation holding
    its own checkout at its head keeps it when the key leads to another tree.
    `acknowledge_move` forgets it once told; History's projection carries
    `SelectedMove`.
  - Host: no `OtherHolders == 0` requirement; idle connections on the checkout
    are released first; a rebound conversation's next turn begins with the
    move note and its result carries the notice.
  - Qt: an opened conversation whose checkout moved shows a quiet line under
    the continuation mode (`agentConversationMoved`).
  - Fixture agent: keeps its state file out of `git status`
    (`.git/info/exclude`) and answers `verify-move-note` with the note's
    heads, the checkout's HEAD, the saved branch and where the reconcile
    prompt fell.
  - Transcript replays: one with no checkout is still offered the choice up
    front; once it has run at the old head it holds a checkout and follows the
    head. The course reflects both.
  - Docs: RFC-BPR-0096 (amendment section and AC13), the amendment's "Slice 4
    as built", `docs/ux/ai-experience.md`.
  - Production app manifest digest resealed (`9a40f5b8…`), citing
    RFC-BPR-0096 and the owner.

  Passed at e92dfa069: `application_agent_invocation_native_pinned_contract`,
  `application_native_agent_sessions`, `application_agent_invocation_host`
  (`test_a_conversation_moves_to_the_latest_head`, with the shared-checkout
  and kept-workspace cases), `sqlite_agent_session_management`
  (`move_a_conversation`), `qt_workbench_agent_send_refusal`,
  `application_review_round_trip_composition_contract`,
  `git_process_contract`, `application_agent_chat_head_move_send_contract`,
  the other `application_agent_invocation_*` cut, cancellation, staleness,
  cleanup and diagnostic courses, `application_agent_crash_recovery`,
  `application_agent_materialization_crash_recovery`, `qt_agent_launch_canary`,
  `core_agent_session`, `agent_workspace_ownership`, the documentation gate
  and its tests, `coding-format-check`, and clang-tidy on the touched
  translation units (remaining findings predate this card).
  `application_agent_invocation_composition_contract` fails at
  `wrapper-setup-durable-reset`: its course answers a Reset agents
  `QMessageBox`, which BPR-2377 replaced with the Clear local data route
  (`agent_tab_view::reset_agents`, `ResetRoute`). Nothing here touches that
  path; it needs its own card.
