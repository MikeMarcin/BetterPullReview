# RFC-BPR-0096 continue at the latest head, amendment r1

Status: proposed. The owner chose this direction on 2026-11-24 (option A,
"amend the RFC"); this text is not yet published into the live RFC. Two
choices below (Q1, Q2) still need the owner's answer before the affected
behavior is implemented.

## Owner input, 2026-11-24

Relayed by the coordinator, verbatim:

- "There doesn't appear to be an option anywhere to continue like it
  suggests" (the report that started this; fixed by the refusal card, see the
  card for BPR-NNNN).
- "if we continue the chat can move the pin forward and read the latest
  commit's code right?"
- "A: amend the RFC".
- "just to be clear, i want it to work like a normal claude code session where
  if i am running and doing turns and then I git pull in another terminal in
  the same repo and continue chatting".

The owner accepts the risk that an agent continuing after the move remembers
tool observations about the earlier head. The move is never silent: the
transcript and the agent are both told.

## What this changes

Live text in [RFC-BPR-0096](../RFC-BPR-0096-native-agent-sessions-and-tour-context.md),
as of `fc4a7f6cc`, "Owner shared-workspace amendment, 2026-11-09":

> A changed base/head uses another retained source binding for a new session;
> an explicit old-revision conversation keeps the old tree. Refresh never moves
> HEAD, resets edits, or silently changes an old native session's working
> directory.

and "Tour context" (the stale-session paragraph):

> Preserve the established stale-session choice after head movement: start a
> new session at the observed revision, or explicitly continue against the old
> revision. Never move the old workspace underneath a native conversation or
> silently relabel its tool observations as current.

[RFC-BPR-0012](../RFC-BPR-0012-ai-review-chat-findings-mcp.md), acceptance:

> The side chat shows stale state after head movement and offers new-session or
> explicit old-revision continuation.

`core::agent_continuation` (`src/core/include/bpr/core/agent_session.hpp`)
states that "keep this session, point it at the new head" is the retargeting
the type exists to prevent.

These become: after head movement a native conversation offers three ways,
in this order:

1. **Continue at <new short head>** — the conversation keeps its native
   memory and its working directory, and its PR workspace moves forward to the
   new head between turns, as a person's `git pull` does in a repository a
   Claude Code session is using.
2. **Start a new session** at the new head (unchanged).
3. **Continue at <old short head>** — keep asking about the old revision
   (unchanged behavior).

Refresh alone still moves nothing. A workspace never moves while any turn on
it is running. Silent retargeting stays forbidden: the move happens only as
the result of Continue at <new> (Q1 decides whether that is the default) and
is always announced.

## Behavior

### The move

Continue at <new> applies to a conversation of the same pull request that
the agent can resume by its own session (it has a runtime session handle).
The core decides that availability; the host performs the move.

- **When.** The move happens when the next question is sent, before the agent
  starts. The workspace lease that already serializes turns on a PR workspace
  is held for the move. If any turn on that workspace is running, the move
  waits for the reviewer to send again, as a busy workspace already does.
- **What moves.** The one PR workspace the conversation uses (owner
  shared-workspace amendment, 2026-11-09: "The PR only needs one workspace…
  the source of truth is the files and git which is shared"). The new head's
  commits are brought into the workspace from the review's own cache, the
  same sources materialization uses (the borrowed clone or the cache; for a
  sparse review, the pull request's files at the new head). HEAD is then
  checked out at the new head, detached, as materialization leaves it. The
  ownership record, the marker and the conversation's stored binding are
  updated in one recorded step, so recovery after a crash finds either the
  old head or the new one, never a mix.
- **Local changes.** If the working tree or index has changes, or HEAD is not
  the old pinned head (the agent committed there), nothing is moved or
  discarded. The turn is refused before anything is sent, saying which files
  or commits are in the way, and the card offers Start a new session and
  Continue at <old>.
- **Other conversations on the same workspace.** Because the workspace is the
  PR's, conversations still pinned to the old head no longer have their tree
  after the move. Their Continue at <old> materializes a fresh old-head
  workspace, as "A changed base/head uses another retained source binding"
  already allows; their Continue at <new> just rebinds.
- **Same head, other comparison.** When only the epoch pair changed, nothing
  in the workspace moves; the conversation is rebound and the note says the
  comparison changed.

### What the agent and the reviewer are told

The first turn after the move begins with a note to the agent, composed by
the core from the two heads and the host's observation of what lies between:
the pull request moved from <old> to <new>; earlier tool observations describe
<old>; the commits between (up to a bound, oldest first, with their subjects)
and the changed files, or, when the history is not linear (force-push) or the
objects are missing, the git commands that show them (`git log --oneline
<old>..<new>`, `git diff --stat <old> <new>`). The transcript shows one quiet
line where the move happened: "Updated to <new> — <n> commits" (or "— rewritten
history" for a force-push).

### Replay and other cases

- A transcript-replay conversation has no native memory to relabel; Continue
  at <new> rebinds it to the moved workspace.
- A conversation that cannot be resumed by its own session (no handle, or a
  backend without native resume) does not offer Continue at <new>.
- A conversation never used moves to the new head without asking (already
  implemented on the lane, BPR-NNNN).

## Open questions for the owner

- **Q1. Default or a choice?** Recommended: when the head has moved and the
  conversation can move, sending a question moves it (Continue at <new> is the
  default), with the transcript line above, and Start a new session /
  Continue at <old> stay reachable from the head chip's menu and History,
  instead of a card that blocks the composer. Alternative: keep the head-moved
  card before sending, with Continue at <new> first. The recommendation
  matches the owner's `git pull` model; the alternative keeps every move an
  explicit press.
- **Q2. Agent commits in the workspace.** Recommended as above: refuse the
  move and say so. Alternative: rebase or merge the agent's commits onto the
  new head. The recommendation never rewrites the agent's work.

## Acceptance (proposed IDs, to be bound by implement-rfc)

- **AMD-01** Continue at <new> on a resumable conversation: the next turn
  resumes the same native session handle in the same working directory, whose
  HEAD is the new head; the agent's first input starts with the move note
  naming both heads and the commits or files between; the transcript shows
  the update line.
- **AMD-02** No turn moves a workspace while a turn on that workspace runs;
  the refusal says it is busy and nothing is sent.
- **AMD-03** A workspace with local edits or agent commits is not moved: the
  turn is refused naming them, and no file or commit is lost.
- **AMD-04** A crash between the object import and the recorded update
  recovers to the old or the new head with its ownership record consistent.
- **AMD-05** Without a press (Q1 alternative) or a send (Q1 recommended) no
  workspace moves; refresh alone moves nothing.
- **AMD-06** Start a new session and Continue at <old> behave as before;
  Continue at <old> after another conversation moved the workspace
  materializes an old-head workspace.
- **AMD-07** A conversation without a native handle does not offer
  Continue at <new>; the core's decision is tested deterministically.

## Consequences for active work

- RFC-BPR-0096 AC07 ("both established stale continuations") gains the third
  continuation; its acceptance evidence is extended, not replaced.
- RFC-BPR-0012's stale-session criterion and `core::agent_continuation`'s
  "exactly two" comment and static assertion change with publication.
- The lane commits for BPR-NNNN (refusal card, choice before sending, unused
  conversations moving silently, Resend) stay valid and are the base for this
  work.

## Next action

After the owner answers Q1 and Q2: publish this amendment into RFC-BPR-0096
and RFC-BPR-0012 under implement-rfc, then implement in slices: core decision
and move note; the workspace move in the Git adapter (object import, checkout,
ownership record, crash recovery) with Git contract tests; host and
composition; Qt; the native pinned public-path course extended with the move.
