---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ4"
title: "Agent workspaces borrow the linked clone's history, and Check it works makes one"
status: "Done"
rfcs: ["RFC-BPR-0091"]
depends_on: ["BPR-PENDING-AQ3"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0091 adoption, 2026-10-22 (owner answers Q7 and Q8)","related_cards":["BPR-PENDING-AN","BPR-1738","BPR-0348"],"milestone":"RFC-BPR-0091 M4","deviation":"A private workspace that borrows the clone's objects through one alternates line, in place of a `git worktree` of the clone; owner confirmation asked in BPR-PENDING-AQ8's look","owner_confirmation":"pending"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "12c38719f", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(git_process_contract|agent_workspace_ownership|application_agent_workspace)$'", "detail": "git_process_contract run_borrowed_history_workspace_contract on fixture clones: one alternates line naming the clone's canonical objects folder; the head's files checked out; rev-list reaches the base through the clone; only 3 objects (head commit, tree, new blob) in the workspace's own pack; not shallow; no [remote in its config; an inventory of every clone file with size and mtime is equal before, after making and after removing; a clone holding the head gives 0 own objects; a folder that is not a clone is refused leaving the workspace parent empty; no clone gives the unchanged private copy. The sparse case: a clone holding the head's parent removes the shallow boundary and history reaches it. agent_workspace_ownership and application_agent_workspace unchanged and passing. Owner confirmation of the alternates form in place of git worktree is pending (BPR-PENDING-AQ8)."}}, {"criterion": "AC2", "outcome": "pass", "revision": "12c38719f", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(application_agent_invocation_host|application_agent_invocation_composition_contract|application_agent_finding_codex_found_contract)$'", "detail": "Each turn materializes at its bound head and cleans after its terminal, unchanged (test_revision_movement_discards_result_and_still_cleans and the composition courses); HistoryClone is carried into every materialization request the window dispatches."}}, {"criterion": "AC3", "outcome": "pass", "revision": "12c38719f", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^application_agent_invocation_host$'", "detail": "test_workspace_probe_makes_and_removes_a_workspace_without_a_session: the posted probe holds quiescence, answers on the decision lane only, makes and removes one workspace, starts no process, finishes no session, carries the linked clone and a reserved session identity; a failure answers with download_failed and leaves nothing to clean. The Qt rows that start Check it works are BPR-PENDING-AQ6's."}}]
---

# BPR-PENDING-AQ4: Agent workspaces borrow the linked clone's history, and Check it works makes one

RFC-BPR-0091 M4 ([RFC](../../rfcs/RFC-BPR-0091-agent-first-run-setup.md#milestones)), owner answers Q7 and Q8 A.

## What changed from the RFC, and why

The RFC's worktree section asks for `git worktree add --detach` in the
reviewer's clone. Building it showed two costs the owner did not weigh when
answering Q7:

- A worktree is a `.git` *file* pointing into the clone. The workspace
  ownership machinery that makes agent workspaces safe to remove after a
  crash (the owner-private marker inside `.git`, the journaled cleanup, the
  topology check that refuses any foreign object source) needs a `.git`
  directory the application owns. A worktree would need a second,
  weaker lifecycle written into the reviewer's clone.
- A worktree writes into the clone (`.git/worktrees/ENTRY`, and on a fetch
  the head's objects), and carries the clone's remotes, so the agent's tools
  could push with the reviewer's credentials.

This card instead gives the existing private disposable workspace the
clone's history: one `objects/info/alternates` line naming the clone's
objects folder. The agent gets the head's files, `git log` and `git blame`
through the clone's history, and Git reads the clone's objects without
writing anything to it. Only what the clone lacks is copied, from the
application's own cache (Q8 A): nothing when the clone holds the head, the
head's objects minus the base's when it holds the base. A sparse review's
workspace is not shallow when the clone holds every parent of the head; when
it does not, it is the head-only snapshot of [[BPR-1738]], as before. The
workspace has no remote, so there is nothing to push to. Lifetime, marker,
journal, crash reclamation and cleanup are the disposable workspace's own and
unchanged, so the RFC's per-repository limit of three and its `worktree
remove` rules have nothing to govern.

The owner is asked to confirm this in [[BPR-PENDING-AQ8]]'s look, as its own
Accept or Reject question. On Reject, a later card makes the `git worktree`
form the RFC describes.

## Scope

- `agent_workspace_materialization_request::HistoryClone`, from the
  repository's link ([[BPR-PENDING-AQ3]]), through every agent request the
  window dispatches; `prepared_workspace::BorrowsHistory` reports it.
- The Git adapter's workspace: the alternates line, `cat-file -e` to see what
  the clone holds, the pack only of what it lacks, the snapshot's shallow
  boundary only when needed, a topology check that admits exactly the line
  it wrote, and a refused clone (no objects folder) leaving nothing behind.
  The objects-folder resolution is the one publication already used, hoisted
  so both share it.
- `agent_invocation_host::probe_workspace`: makes and removes the workspace
  an Ask would make, through the same port, on the accepted binding, with a
  session identity from a reserved range; admits no session, runs no process,
  answers on the decision lane and counts toward quiescence. The window's
  Check it works runs the agent check and this probe and reports the
  workspace row; the Qt rows that start it are [[BPR-PENDING-AQ6]]'s.

## Acceptance criteria

- AC1: RFC AC06 as amended above, on fixture clones: the workspace's one
  alternates line names the clone's objects, only the head's new objects are
  copied, the clone's every file and timestamp is unchanged by making and
  removing the workspace, the workspace has no remote, a folder that is not
  a clone is refused leaving nothing, and without a clone the private copy
  is unchanged.
- AC2: RFC AC07: each turn's workspace is made at the turn's bound head
  (unchanged disposable-workspace behaviour), so a new head's session gets a
  new workspace and the old one keeps its head until its turn ends.
- AC3: RFC AC10's workspace row: the probe passes, and fails with the
  workspace's own reason, without a session or a process.
