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

- Asked whether a session that shares its workspace with another session also
  moves: "a session in the same PR? yes single checkout." Then: "The other
  sessions, we dont wake them up and send them messages".

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

## Implementation design for the workspace move (checkpoint, 2026-11-24)

Grounded in `src/adapters/git/src/installed_git_process.cpp` at `faba46f37`.
Slice 1 (this amendment folded into the RFC; `core::decide_conversation_move`
and `core::conversation_move_note`) is done. What follows is the design for
the Git adapter move. Slice 2a, the adapter move and its recovery, is built
("Slice 2a as built" below). The store and plumbing are not built yet.

**What records the head today.**

- The ownership journal's `workspace_record.Head`.
- The `.git/bpr-owned-workspace` marker
  (`bpr-agent-workspace-v1:<scope0>:<session>:<head>`), checked by
  `inspect_agent_workspace`, `mark_ready` and `cleanup_locked`.
- The stored conversation (binding, `owned_agent_workspace` with
  `PinnedHead`, `RequiredBase` and `OwnershipMarker`).
- The shared-workspace map key `agent_workspace_key(authority, binding)`,
  which includes the head.

`record_transition` admits no retained→retained step. Startup recovery leaves
phase 7 records alone (`++result.Live`), so a move has to be safe at
`inspect_agent_workspace` and `cleanup_locked`.

**Journal.**

- Add `std::optional<std::string> PreviousHead` to `workspace_record`. Glaze
  does not require a nullable member, so Version 2 records stay readable.
  Journals of moved workspaces cannot be read by older app versions; that is a
  downgrade limit to state in the release notes.
- Admit a 7→7 transition that changes only `Head` and `PreviousHead`.
- Marker checks accept the marker of `Head` or of `PreviousHead`, which closes
  the window between writing the marker and appending the journal record.

**The move**, a new `git_process_port::move_agent_workspace(request, stop)`
taking the workspace, the cache repository, the new head and base, the
reconcile choice and the saved-branch name, under the process boundary lock
and the journal's exclusive lock (as `cleanup_agent_workspace` takes it):

1. Validate exactly as `inspect_agent_workspace` does. In the first slice a
   shallow (sparse) workspace returns `object_unavailable`; sparse follows.
2. Observe the workspace: `status --porcelain=v1 -z --untracked-files=all` for
   edits; `merge-base --is-ancestor <old> HEAD` and `rev-list --count
   <old>..HEAD` for agent commits. If there is work and the reconcile is none,
   return "not moved" with that state; nothing is changed.
3. Bring the new head's and base's commits and trees in from the review
   cache, the way `repair_borrowed_workspace` copies from it; refuse with
   `object_unavailable` when the cache lacks them.
4. For a reconcile with edits, commit them as a WIP commit with a fixed
   BetterPullReview identity and `--no-verify`. Write
   `refs/bpr/pre-move/<old>` at HEAD. For save and move, or ask the agent,
   write `refs/heads/bpr/saved/<conversation>/<old short>` at HEAD.
5. Write `refs/bpr/move-target` at the new head: the in-progress record.
6. Check out the new head detached, without `--force`. For rebase, run
   `rebase --onto <new> <old>`. On a conflict, collect `diff --name-only
   --diff-filter=U`, run `rebase --abort`, check out the pre-move ref, run
   `reset --mixed` to the pre-WIP head so the edits are uncommitted again, and
   return "not moved" with the conflicting files.
7. Write the marker for the new head (temporary file, then rename), append the
   7→7 record (`Head` new, `PreviousHead` old), and delete
   `refs/bpr/move-target`.
8. Return the reattached workspace at the new head and the history:
   linearity, commits oldest first, changed files, all bounded by
   `core::maximum_move_note_*`.

**Recovery in `inspect_agent_workspace`:**

- When `refs/bpr/move-target` exists and the journal `Head` is still the
  old head, abort any rebase, check out the pre-move ref and its marker, and
  delete the target ref (roll back).
- When the journal `Head` is the target, finish the checkout and marker, then
  delete the ref (roll forward).
- When the stored conversation's head is the journal's `PreviousHead` (a crash
  after the journal step and before the store), return the reattached
  workspace at the journal head, flagged as moved, so the host rebinds the
  conversation and still tells the agent.

**Store and plumbing.**

- `git_cache_port` and `agent_workspace_port` each gain the move, through the
  review actor, the way `inspect` runs.
- The native session store's update writes the conversation's base, head and
  epochs and the retained workspace's head. The shared-workspace map moves
  from the old key to the new one.
- The host calls the move on send when `decide_conversation_move` says move,
  holding the workspace lease, then runs the turn with the note.

**Contract tests** (`tests/git_contract/git_process_contract_tests.cpp`, beside
the inspect cases near its retained-workspace tests):

- A clean move, plain and borrowing a clone.
- Edits, and agent commits, observed and left untouched.
- Rebase that succeeds; rebase that conflicts, restored and naming the files.
- Save and move; ask the agent (saved branch, then move).
- Objects missing.
- A crash at each new `workspace_checkpoint` (after the WIP commit, after the
  target ref, after the checkout, after the marker, after the journal),
  recovered to the old or the new head with journal, marker and git agreeing.
- An old-version reader refusing a moved journal is stated, not tested.

### Slice 2a as built (2026-11-24)

`git_process_port::move_agent_workspace` and the recovery inside
`inspect_agent_workspace` are implemented in the installed Git adapter, with
`git_process_contract`'s workspace-move course (`--workspace-move`). Where the
build differs from the design above:

- **The journal line comes before the marker.** Step 7 appends the 7→7 line,
  then writes the marker, then deletes the refs. With the marker first, the
  window between the two holds a marker for the new head and a journal whose
  `Head` and `PreviousHead` both name older heads, which the "marker of `Head`
  or `PreviousHead`" rule does not cover. With the line first, the marker
  always names the journal's `Head` or `PreviousHead`, so `cleanup_locked`
  and retained-workspace removal accept it throughout.
- **`refs/bpr/move-restore`** records HEAD before any reconcile and is deleted
  before `refs/bpr/move-target`. Recovery runs when either exists. Without
  the target ref, a crash came before the checkout, and `reset --mixed` to the
  restore ref uncommits the WIP. With it, the journal decides as designed:
  roll back checks out the pre-move ref (or the old head for a plain move)
  with `--force`, since the WIP commit holds every edit, and then resets to the
  restore ref.
- **The WIP is made on a detached HEAD** (`update-ref --no-deref`), so a
  branch the agent made keeps pointing where it did. After an in-process
  rollback HEAD is reattached to that branch. After a crash rollback HEAD
  stays detached at the same commit.
- **A rebased WIP commit is uncommitted again** (`reset --mixed HEAD~1` when
  the tip is the WIP), so a rebase behaves like `pull --rebase --autostash`.
- **One move behind** is reported by the returned workspace's `PinnedHead`
  being the journal's head rather than the stored one. A move asked for from
  that stored workspace to the journal's head reports the finished move.
- **Bounded journal.** A move refuses (`invalid_repository`, "no room for
  another move") before changing anything when one more line would pass the
  record's 64 KiB bound. That bound allows about 50 moves. Compacting the
  record is left for later. (Built in slice 2c below: the bound is gone.)
- **Crash course.** The `git_process_contract` build now also defines
  `BPR_WORKSPACE_OWNERSHIP_FAULT_INJECTION`. Its checkpoint hook throws at the
  named step. That leaves every file as a kill would, and the test then drops
  the adapter and recovers with a fresh one. The checkpoints are
  `move-after-wip`, `-target`, `-checkout`, `-journal` and `-marker`.
- **Busy.** A turn holding the record's lock shared (an agent running)
  returns "not moved" with `busy`, and nothing changes.

### Slice 2b as built, and the plan for slice 3 (2026-11-24)

Built (slice 2b):

- **Shared types.** `agent_workspace_move_request` and `_result` live in
  `agent_workspace.hpp`; the Git process request wraps the move with the
  cache path. `saved_branch_name(session, old)` gives
  `bpr/saved/<session>/<old short>`.
- **Routing.** `agent_workspace_port::move_agent_workspace` goes through
  `epoch_application_host`, which runs it on the review's actor job as
  inspect does. `git_cache_port::move_agent_workspace` adds the installed
  cache's authority and object-format checks.
- **Store.** `native_agent_session_store_port::move_conversation` writes,
  in one revision, the conversation's base, head and epochs, its marker and
  identities, and the new pull request key. It drops the old key. It refuses
  a stale revision, and a tree another conversation or a kept workspace
  holds. `sqlite_agent_session_management` tests it.

Plan for slice 3. Grounded in `agent_invocation_host::run`, at the pinned
check after `select`:

1. **Which conversations move.** Only a conversation of the same review that
   is resumable (it has a runtime handle, or is a replay) and holds a
   retained, non-sparse workspace whose head differs from the observed one.
   Every other pinned conversation keeps today's refusal
   (`offer_pinned_conversation`).
2. **Leases and other holders.** Take the old binding's key lease and the
   workspace path lease, beside the observed key's lease.
   `NativeSessions->removal(scope, id).OtherHolders` must be 0; otherwise
   the move is refused and offers a new session or Continue at <old>.
3. **The move.** Call `Workspaces.move_agent_workspace` with the
   request's reconcile (a new `agent_invocation_request` field, `none` on a
   plain send) and `saved_branch_name`.
   - Not moved: refuse through `decide_conversation_move` with the observed
     state. The refusal carries the offered reconcile actions and any
     conflicting files in `agent_run_result`.
   - Moved: `move_conversation`, then run the turn. The supplement carries
     `conversation_move_note` (prepended to the question), and the result
     carries the move (from, to, commit count) for the transcript notice
     "Updated to <sha> — <n> commits".
4. **Inspect one move behind.** An inspected workspace at a head other than
   the stored one takes the same `move_conversation` and note path.
5. **Qt.** The refusal card offers Rebase onto <new>, Keep my changes on a
   branch and move, Ask <agent> to reconcile, Continue at <old> and Start a
   new session. Ask <agent> resends with the built-in reconcile prompt, kept
   with the other built-in prompts (RFC-BPR-0125).

### Slice 3 as built (2026-11-24)

- **Which conversations move.** In `agent_invocation_host::run`, after the
  pinned check, a conversation of the same review with a runtime handle or a
  replay, whose retained workspace is complete and at its stored head, moves
  when the heads differ and `decide_conversation_move` says move. A pinned
  conversation that differs only in the comparison (the same head), or holds
  no complete retained workspace, keeps the refusal of
  `offer_pinned_conversation`.
- **Leases.** The old binding's key lease and the workspace path lease are
  taken beside the observed key's; `removal().OtherHolders` must be 0. A lease
  held elsewhere is `busy`; another holder refuses with its own words.
- **Refusal.** The result carries `PinnedConversation` and `PinnedSession` as
  before, plus `MoveRefusal` (the core's decision, made with the observed
  state and no reconcile) and `MoveConflicts`. The reason names exactly the
  offered ways on. The core gained `conversation_move_facts::RebaseConflicted`:
  after a rebase conflicted, Rebase is not offered again.
- **Moved.** `move_conversation` records the binding, workspace and key; the
  question is sent after `conversation_move_note` and, for Ask the agent, the
  built-in prompt `default_reconcile_prompt()`
  (`resources/review-preparation/reconcile-move.md`, version
  `reconcile-move-v1`), shipped beside the other built-in prompts. It is not
  yet editable in Settings › Prompts: that needs its own settings column and
  migration (built in slice 2c below). The result's `ConversationMove` (from, to, commits, linearity,
  reconcile, saved branch) drives the transcript line "Updated to <sha> —
  <n> commits", "— history rewritten", followed by the reconcile done.
- **One move behind.** An inspected workspace at a head other than the stored
  one is moved to the conversation's head through the same path, and the
  agent is told.
- **History load.** A conversation a send would move is no longer pinned to
  the chat; a restored runtime session is held at the head under review, so a
  plain send reaches the move.
- **Qt.** The stopped card shows, on a wrapping row above Continue at <old>
  and Start a new session, the offered Rebase onto <new>, Keep my changes on a
  branch and move and Ask <agent> to reconcile. Each goes to the composition
  (`choose_move_reconcile`), which puts the reconcile on the resent turn once,
  and resends the kept message. A moved turn's reply card shows the notice.
- **Tests.** `application_agent_invocation_host`
  (`test_a_conversation_moves_to_the_latest_head`), `core_agent_session` and
  `qt_workbench_agent_send_refusal`
  (`test_a_pinned_conversation_card_offers_what_it_names`).

### Slice 4 as built: one checkout for the pull request (2026-11-24)

Owner, through the coordinator: "a session in the same PR? yes single
checkout", and "The other sessions, we dont wake them up and send them
messages". Slice 3 refused a move while another conversation held the tree
(`removal().OtherHolders`); that is withdrawn.

- **Store.** `move_conversation` refuses only when the pull request's kept
  workspace records the tree (`native_agent_session_removal::KeptHolders`).
  In the same transaction it rebinds every other conversation on the tree
  (binding, marker, identities) and records the head each was at in
  `native_agent_moved_conversations`; the keys that led to the tree go and the
  new key leads to it unless another tree is already there. A conversation
  whose record holds exactly its own checkout at its head keeps working in it
  when the key leads to another tree at that head (`shared_workspace`,
  `store`); nothing changes its working directory.
- **Nothing is sent to the others.** The move starts no turn for them. Their
  `native_agent_session::MovedFrom` is read on their own next turn, which
  begins with `conversation_move_note` (history not listed, so the note names
  the git commands) and whose transcript shows the notice; the store forgets
  it once that question was sent (`acknowledge_move`). History, opened, shows
  "Updated to <new> — this pull request's checkout moved from <old> while
  another session asked" (`native_agent_history_projection::SelectedMove`).
- **Idle agent processes.** A native connection kept between turns holds the
  checkout's lease, so the move found the record busy. The host now ends the
  idle connections working in the checkout first
  (`agent_process_port::release_native_connections`); a connection answering
  a turn keeps it busy. Their conversations resume their own sessions in a
  new process.
- **Found on the way.** A refused turn named the checkout's workspace session
  as its pinned session; a checkout Agent setup's check made carries one from
  the probes' reserved range, which no private run may name, so Continue at
  <old> from the refusal card failed to start. The refusal now names the
  turn's own session, and the window's chat keeps its own session when a
  restored one is in that range.
- **Transcript replays.** A replay holds no checkout until it runs a turn, so
  one made before the head moved is still offered Start a new session and
  Continue at <old> when opened. Once it has run at the old head it holds a
  checkout and follows the head like any other conversation.
- **Course.** `application_agent_invocation_native_pinned_contract`, with the
  fixture agent only: edits refuse with every way on and no process; Continue
  at <old> answers there and keeps the edit; Start a new session answers at
  the new head; a conflicting Rebase is put back, names the file and is not
  offered again; Keep my changes on a branch and move moves with the branch
  present and the note before the question, starting exactly one process
  while the second agent's conversation on the same checkout is only rebound;
  that conversation, opened, says the move, has no new turn, and its next
  question carries the note once; a never-used conversation moves silently;
  a replay continued at the old head then moves on a plain send; Rebase
  succeeds; Ask the agent puts the built-in prompt after the note.

### Slice 2c, the editable reconcile prompt and compaction, as built (2026-11-25)

- **Partial-history workspaces move.** A retained workspace whose history ends
  at its head (`Shallow`) moved to `object_unavailable` until now. It moves
  too:
  - A workspace of only the pull request's files (a sparse review with no
    clone) is given the new head's files. The epoch host reads them as
    materialization does (`read_pull_request_files`, through the review's
    lease, each checked against its blob) and passes them on the move
    request (`agent_workspace_move_request::PullRequestFiles`). The cache
    lends the new head's commit and trees; the files' blobs are written from
    the bytes; the new head joins `.git/shallow`; the sparse patterns become
    the new head's files, with the old patterns kept in
    `.git/bpr-sparse-checkout.pre-move` until the move ends, so a rollback
    or a crash before the journal line puts them back.
  - A partial history on a complete review (the cache holds whole
    snapshots) copies the new head's snapshot from the cache, its history
    ending there.
  - Git's checkout, commit and rebase verify every blob a tree names, which
    such a workspace lacks by design (they print "invalid object" and
    commit refuses). The move uses reads that do not: two-way
    `read-tree -m -u` for the checkout, `write-tree --missing-ok` and
    `commit-tree` for the WIP commit, a three-way `read-tree -m -u` for
    Rebase (edits only; commits in such a workspace are kept on a branch
    instead, and Rebase refuses with `invalid_request`), `ls-files
    --unmerged` for the conflicting files, `reset --mixed` to uncommit the
    carried edits, and `read-tree --reset -u` to put the workspace back.
    Each such read may write Git's note to standard error; its exit status
    decides (`process_request::AllowStandardError`). Edits a rebase carries
    stay in view: their paths join the new patterns.
  - The moved workspace keeps no base (as materialization keeps none for a
    partial history), and its holes are the new head's
    (`SparseMissingObjects`). The store writes them for every conversation on
    the checkout in the move's revision. A conversation one move behind
    resumes with the new head's holes listed again.
  - The note gives the git commands rather than a commit list: a partial
    history holds neither head's ancestry.
  - All Git stays in `workspace_git` (BPR-2387), and the new verbs are none
    the boundary rule (BPR-2386) forbids.
- **Ask <agent> to reconcile is editable.** Its prompt is the writing hook
  `reconcile_move` (purpose `agent.reconcile-move`, version
  `reconcile-move-v1`), listed in Settings › Prompts under the Agent tab as
  "Reconcile after an update", with the other built-in prompts
  (RFC-BPR-0125): edit, save, reset, export and import. The settings
  database gains its column at schema version 40; the prompt tables' check
  widens to 17 in place. The composition sends the profile's text
  (`effective_reconcile_prompt`).
- **Journal compaction.** The 50-move bound is gone. A move whose line would
  take the record past half its 64 KiB bound first rewrites it as its
  retained lines, at the head it moved from last, and its last line
  (`compact_workspace_lines`). The file keeps its identity, which the
  conversation stores and a turn locks, so it is rewritten in place
  (`journal_compact`), each step durable before the next: the compacted copy
  is appended between two `%` marks, `%` replaces the first byte, the
  compacted lines are written after it, `{` is written back, and the rest is
  cut off. A reader resolves every intermediate file to the old lines or the
  new (`resolve_journal_compaction`); an exclusive load (a move, startup
  recovery, retained-workspace removal) makes the file so.
- **Tests.** `git_process_contract --workspace-move`: a pull-request-files
  workspace moves holding only the new head's files; edits are carried by a
  rebase and stay in view; a crash at `move-after-patterns`,
  `-checkout`, `-journal` and `-marker` recovers with the patterns of the
  head it lands on; a partial history on a complete review moves; a record
  filled to the old bound compacts and moves; a crash at each compaction
  step (`compact-after-copy`, `-mark`, `-body`, `-lead`) reads as the head it
  had and the next move or startup finishes or undoes it.
  `epoch_application_host` (files passed only for a pull-request-files
  workspace), `sqlite_agent_session_management` (holes for every
  conversation), `sqlite_workbench_settings_storage` (version 39 migrates,
  keeping each window's side chat width), `sqlite_agent_configuration_storage`
  (the tables widen keeping IDs) and `application_writing_delivery`.
- **Pinned course.** The composed course
  (`application_agent_invocation_native_pinned_contract`) has no sparse
  review fixture: a sparse review needs GitHub CLI's sparse reads, which the
  public-path fixture does not serve. The partial-history move is covered by
  the Git contract course and the epoch host's routing test.
