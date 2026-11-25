---
schema: "implement-rfc.card.v1"
id: "BPR-2396"
title: "Continue at the latest head: partial-history workspaces move, the reconcile prompt is editable, and the ownership record compacts"
status: "Done"
rfcs: ["RFC-BPR-0096"]
depends_on: ["BPR-2379", "BPR-2386", "BPR-2387"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"RFC-BPR-0096 continue-at-latest amendment r1, remaining slices (coordinator, 2026-11-25)","reservation":"docs/issues/_reservations/BPR-2396--BPR-2396--f9a0d849bf79.json","revision":"8e6fec863","related":["BPR-2379","BPR-2386","BPR-2387"],"acceptance":["BPR-2396:partial-move","BPR-2396:partial-recovery","BPR-2396:host-files","BPR-2396:store-holes","BPR-2396:editable-prompt","BPR-2396:prompt-migration","BPR-2396:compaction","BPR-2396:compaction-crash","BPR-2396:ac13-ledger"]}
---

# BPR-2396: Continue at the latest head: partial-history workspaces move, the reconcile prompt is editable, and the ownership record compacts

## Scope

The three pieces RFC-BPR-0096's continue-at-latest amendment
([amendment r1](../../rfcs/amendments/RFC-BPR-0096-continue-at-latest-r1.md))
left after [[BPR-2379]], [[BPR-2386]] and [[BPR-2387]]:

1. Slice 2c. A retained workspace whose history ends at its head (a sparse
   review's pull-request-files workspace, or a partial history on a complete
   review) refused the move as `object_unavailable`.
2. Ask <agent> to reconcile's built-in prompt
   (`resources/review-preparation/reconcile-move.md`) was not editable in
   Settings › Prompts.
3. The ownership journal's 64 KiB bound capped a workspace at about 50 moves.

## Change

- **Partial-history move** (`src/adapters/git/src/installed_git_process.cpp`).
  `move_agent_workspace` takes `agent_workspace_move_request::PullRequestFiles`
  for a pull-request-files workspace: the cache lends the new head's commit
  and trees, the files' blobs are written from the host's bytes and checked,
  the new head joins `.git/shallow`, and the sparse patterns become the new
  head's files, the old ones kept in `.git/bpr-sparse-checkout.pre-move` until
  the move ends. Without files the cache lends the head's whole snapshot.
  Git's checkout, commit and rebase verify blobs such a workspace lacks by
  design, so the move uses reads that do not (two- and three-way `read-tree
  -m -u`, `write-tree --missing-ok` + `commit-tree` for the WIP, `ls-files
  --unmerged`, `reset --mixed`, `read-tree --reset -u` to put it back), with
  Git's "invalid object" note on standard error allowed where the exit status
  decides (`process_request::AllowStandardError`). Rebase carries edits;
  commits in such a workspace are kept on a branch instead. The recovery in
  `inspect_agent_workspace` now runs for partial-history workspaces too and
  restores the patterns; a conversation one move behind resumes with the new
  head's holes. All Git stays in `workspace_git`.
- **Host** (`epoch_host_materialization.cpp`). A move of a shallow workspace
  that borrows no clone on a sparse review reads the new head's files through
  `read_pull_request_files`, as materialization does
  (`move_pull_request_files`). `agent_invocation_host` no longer excludes
  partial-history workspaces from the move.
- **Store** (`agent_setup_store.cpp`). `move_conversation` writes the new
  head's holes for every conversation on the checkout in the move's revision.
- **Editable prompt.** `core::writing_hook::reconcile_move` (purpose
  `agent.reconcile-move`, version `reconcile-move-v1`), Settings › Prompts'
  "Reconcile after an update" under the Agent tab; the composition sends
  `effective_reconcile_prompt`. Settings schema version 40 adds
  `writing_reconcile_move` and rebuilds version 39 keeping each window's side
  chat width (the copy now names `side_chat_width` from version 38); the
  prompt tables' check widens to 0..17 in place, keeping IDs.
- **Compaction.** A move whose line would pass half the bound first rewrites
  the record as its retained lines at the head it moved from last, plus its
  last line. The file keeps its identity, so it is rewritten in place, each
  step durable: copy appended between `%` marks, `%` first, body, `{` back,
  cut. `resolve_journal_compaction` reads any intermediate file as the old or
  the new lines; an exclusive load (move, startup, removal) makes it so.

## Acceptance

- `BPR-2396:partial-move`: a pull-request-files workspace moves holding only
  the new head's files, patterns and holes; edits and a new file are carried
  by a rebase and stay in view; a partial history on a complete review moves
  with the whole snapshot.
- `BPR-2396:partial-recovery`: a crash at `move-after-patterns`,
  `-checkout` (back) and `-journal`, `-marker` (forward) recovers with
  journal, marker, Git and patterns agreeing and the work kept.
- `BPR-2396:host-files`: the epoch host passes the new head's files only for a
  pull-request-files workspace on a sparse review.
- `BPR-2396:store-holes`: every conversation on a moved partial checkout reads
  the new head's holes.
- `BPR-2396:editable-prompt`: the prompt is listed, edited, saved, reset, not
  sample-tested, has a stable purpose ID, and the saved text is the one sent.
- `BPR-2396:prompt-migration`: settings version 39 migrates with prompts and
  side chat widths kept; the prompt tables widen keeping IDs.
- `BPR-2396:compaction`: a record filled to the old bound compacts to seven
  lines and the workspace moves; it resumes and startup reads it.
- `BPR-2396:compaction-crash`: a crash at `compact-after-copy`, `-mark`,
  `-body`, `-lead` reads at the head it had with Git unchanged; startup or the
  next move finishes or undoes it, then moves.
- `BPR-2396:ac13-ledger`: AC13's evidence is on the RFC's ledger.

## Evidence

At 8e6fec863 (lane `worktree-agent-a08ee2cc7a591cf46`, on main 5bc62c938;
ledger commit ffe9792fa), macOS arm64, Debug, Git 2.50.1 (Apple Git-155):

- `BPR-2396:partial-move`, `:partial-recovery`, `:compaction`,
  `:compaction-crash`: `better_pull_review_git_process_contract_tests
  --workspace-move` printed `GIT_WORKSPACE_MOVE_OK` (36 s);
  `ctest --preset dev -R '^git_process_contract$'` passed (143 s; the
  workspace-git-isolation course included). Under a parallel ctest beside
  other builds it timed out at the former 180 s bound, so the bound is now
  300 s (`tests/git_contract/CMakeLists.txt`).
- `BPR-2396:host-files`: `application_epoch_host`
  (`a_sparse_workspace_moves_with_the_pull_request_files`) passed;
  `application_agent_invocation_host` passed.
- `BPR-2396:store-holes`: `sqlite_agent_session_management`
  (`move_a_conversation`) passed.
- `BPR-2396:editable-prompt`: `qt_settings_window`, `application_writing_delivery`
  and `qt_agent_configuration_dialogs` passed.
- `BPR-2396:prompt-migration`: `sqlite_workbench_settings_storage`
  (`test_reconcile_hook_migrates_from_thirty_nine`) and
  `sqlite_agent_configuration_storage` passed.
- Also passed: `agent_workspace_ownership`, `retained_workspace_expiry`,
  `architecture_boundaries`, `architecture_boundary_contract`.
- `BPR-2396:ac13-ledger`: `python3 scripts/verify_documentation.py --root .`
  printed `DOCUMENTATION_OK` at ffe9792fa.
- The two changed composition sources (`window_agent_chat.cpp`,
  `tour_assist.cpp`) compile; clang-format 22 applied; clang-tidy 22 on the
  changed translation units found nothing new (two existing findings in
  `review_tour_editor.cpp` and `agent_setup_store.cpp`).

Not run: the composed course
`application_agent_invocation_native_pinned_contract` (the volume had under
1.5 GiB free while other agents built; the course's application binary was
not linked). Its paths for complete workspaces are unchanged here; BPR-2379's
passing evidence stands. `tests/end_to_end/profile_tour_handoff_contract.py`'s
hook list gained the update and reconcile prompts but was not run. Windows and
Linux were not run; the compaction's Windows writes use the same positional
`WriteFile` as the existing migration rewrite.

## Residual

- The composed pinned course has no sparse review fixture (a sparse review
  needs GitHub CLI's sparse reads, which the public-path fixture does not
  serve); the partial-history move is covered by the Git contract course and
  the epoch host's routing test.
- Rebase in a partial-history workspace carries edits by a three-way read;
  agent commits there are offered Keep on a branch and Ask the agent instead
  (Rebase refuses with `invalid_request`). Git's own commit in such a
  workspace fails for the same blob check, so commits there are rare.
- A journal compacted by this build is unreadable by an older build, as a
  moved journal already was.
