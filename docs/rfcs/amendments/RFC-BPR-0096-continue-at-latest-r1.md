# RFC-BPR-0096 continue at the latest head, amendment r1

Status: adopted by the owner on 2026-11-24 through the coordinator; folded into
the live RFC as
[Owner continue-at-latest amendment, 2026-11-24](../RFC-BPR-0096-native-agent-sessions-and-tour-context.md#owner-continue-at-latest-amendment-2026-11-24).
This file keeps the decision record, the superseded text and the design notes.

## Owner input, 2026-11-24

Relayed by the coordinator, verbatim:

- "There doesn't appear to be an option anywhere to continue like it
  suggests" (the report that started this).
- "if we continue the chat can move the pin forward and read the latest
  commit's code right?"
- "A: amend the RFC".
- "just to be clear, i want it to work like a normal claude code session where
  if i am running and doing turns and then I git pull in another terminal in
  the same repo and continue chatting".
- Q1, default or a choice: "Yes just move it, maybe drop a notice in the
  transcript that it happened."
- Q2, local edits or agent commits in the workspace: the coordinator's default
  (the move is not made over them), then the owner: "That's fine if we have
  some reconcile options.. it must be recoverable.", and: "Yes a few
  non-agentic options and then a system prompt to just fix it".

The owner accepts the risk that an agent continuing after the move remembers
tool observations about the earlier head. The move is never silent: the
transcript and the agent are both told.

## Superseded text

From RFC-BPR-0096 (as of `733db5ccf`), "Owner shared-workspace amendment":

> A changed base/head uses another retained source binding for a new session;
> an explicit old-revision conversation keeps the old tree. Refresh never moves
> HEAD, resets edits, or silently changes an old native session's working
> directory.

and "Updated BPR state and revision changes":

> Preserve the established stale-session choice after head movement: start a
> new session at the observed revision, or explicitly continue against the old
> revision. Never move the old workspace underneath a native conversation or
> silently relabel its tool observations as current.

From RFC-BPR-0012, acceptance:

> The side chat shows stale state after head movement and offers new-session or
> explicit old-revision continuation.

`core::agent_continuation` stated that "keep this session, point it at the new
head" is the retargeting the type exists to prevent; its comment changes with
this amendment, and that choice is now a separate, explicit decision
(`core::decide_conversation_move`).

## Design notes

- **One move, between turns.** Sending a question in a conversation whose
  workspace is at an earlier head moves the conversation's own PR workspace to
  the head under review, then runs the turn there. Never while any turn on that
  workspace runs; refresh alone moves nothing.
- **What moves.** The PR's one workspace (owner shared-workspace amendment,
  2026-11-09). The new head's objects come from the review's own cache (the
  borrowed clone, the cache, or for a sparse review the pull request's files at
  the new head). HEAD is checked out at the new head, detached.
- **Recoverable by construction.** Before anything changes, the workspace
  records a pre-move ref, `refs/bpr/pre-move/<old head>`, at the current HEAD.
  The ownership journal records the move as its own step (moving, then
  retained at the new head), so recovery after a crash completes or rolls back
  the move and the marker and journal agree.
- **Reconcile, not discard.** If the working tree has edits or HEAD has commits
  past the old head, the move is not made by default. The turn is refused
  before anything is sent, names what is in the way, and offers:
  - **Rebase onto <new>** — uncommitted edits are first committed to a WIP
    commit on the pre-move ref's line; the commits past the old head are
    rebased onto the new head. On a conflict the rebase is aborted, the
    workspace is restored to the pre-move ref, and the conflicting files are
    named.
  - **Keep my changes on a branch and move** — the work (with uncommitted
    edits as a WIP commit) is kept on `bpr/saved/<conversation>/<old short
    head>`, then the workspace moves; the notice names the branch.
  - **Ask <agent> to reconcile** — the work is kept on the saved branch as
    above and the workspace moves; then a built-in prompt, kept with the other
    built-in prompts (RFC-BPR-0125), tells the agent both heads, the saved
    branch and pre-move ref and any files a rebase found conflicting, and asks
    it to bring its work onto the new head and report what it did. The saved
    branch and pre-move ref stay.
  - **Continue at <old>** and **Start a new session**, as before.
- **What the agent and reviewer are told.** The first turn after the move
  begins with a note from the core: the pull request moved from <old> to
  <new>; earlier tool observations describe <old>; the commits between (oldest
  first, bounded) and the changed files, or the git commands that show them
  when the history was rewritten or is missing; and any saved branch or
  rebase. The transcript shows a quiet line: "Updated to <new> — <n>
  commits", "— history rewritten", or with the reconcile that was done.
- **Who decides.** The core decides whether a conversation moves, starts
  fresh, or is refused with which ways back, from facts the host observes
  (memory, native resumability, the workspace's state). The host performs the
  move. Qt presents the notice and the actions.
