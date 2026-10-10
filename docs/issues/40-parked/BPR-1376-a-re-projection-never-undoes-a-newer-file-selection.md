---
schema: "implement-rfc.card.v1"
id: "BPR-1376"
title: "A drawer re-projection never undoes the reviewer's newer file selection"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
parked_on: "human-review"
platforms: ["mac", "windows", "linux"]
metadata: {"found_by":"BPR-1345's Windows Debug diagnosis, bpr-main-win, 2026-10-09","filed_by":"bpr-main-win, at the coordinator's request","reservation":"docs/issues/_reservations/BPR-1376--BPR-1376--71cfe6a1a402.json","related_cards":["BPR-1345","BPR-1344"]}
acceptance: []
---

# BPR-1376: A drawer re-projection never undoes the reviewer's newer file selection

## Why

While diagnosing [[BPR-1345]]'s Windows Debug stall, one instrumented run of
`application_start_review_contract` did something its fix does not explain.
The fixed reveal helper picked `src/engine/renderer.h` in the Files drawer,
and the window dispatched the select intent: its file-selection count went
from 1 to 2. Then three things went wrong:

- the drawer's current row went back to `src/engine/renderer.cpp`;
- the selected diff never became `renderer.h`;
- the course waited until its budget ran out.

The selection was made just after phase 11's native publication, which
refreshes the pull request and re-projects the drawer. It did not happen
again in 31 more instrumented runs.

The coordinator's reading: an owner would see this as "I picked a file and it
jumped back". So it is hunted now rather than left to recur.

## Scope

A seeded stress test, at the application host and at the Qt window, of a
reviewer's file selection racing a drawer re-projection that a publication
or refresh caused. It covers many seeded interleavings and timings of three
events:

- the select intent;
- the re-projection and the refresh behind it;
- the diff completions.

The test asserts that the newest selection the reviewer made always wins,
in the drawer's current row and in the diff shown.

- If it reproduces, the core decides which selection is newest, by request
  identity, and the host and window follow that.
- If 500 or more seeded interleavings cannot reproduce it, that is recorded
  here and the card closes as not reproduced.

## Acceptance

- AC1: A host-level seeded stress test interleaves a file selection with a
  publication- or refresh-driven re-projection and diff completions, over at
  least 500 seeds. It asserts that the newest selection wins in the projected
  selection and in the diff requested and shown.
- AC2: A Qt-level course does the same through the window's Files drawer:
  the current row and the shown diff.
- AC3: If either reproduces the loss, the fix makes both pass, and a named
  control (the fix reverted) fails. If neither reproduces it, the seeds,
  the interleavings covered and the platforms run are recorded, and the
  card closes as not reproduced.
- AC4: `architecture_boundaries`, `test_network_isolation_manifest` and the
  round-trip contract pass. `DOCUMENTATION_OK`.

## Boundaries

No live GitHub, no model call, no new uncontained registration unless it is
measured and pinned.

## Parked (bpr-main-win, 2026-10-09)

The coordinator stopped the hunt for a pre-release test build. Seen once and
not reproduced in 31 runs; reopen if it recurs.

**Suspected cause (read from the code; not reproduced).** Three steps:

1. A file click reaches the host synchronously:
   `dispatch_workbench_intent` calls `WorkbenchHost->apply` with the select
   intent. The active document tab learns the new file only later, from the
   posted `observe_tab_changes`.
2. Suppose a same-head refresh's `accept` is already queued when the
   reviewer clicks. Then it runs first. `WorkbenchHost->observe` supersedes
   the new file's diff and clears the selection, and `RefreshKeptTabView` is
   set.
3. The posted task then finds no selection, so the tab keeps the old file.
   When the observation lands, `represent_refreshed_tab` ([[BPR-1303]])
   reloads the tab's old file and selects it. The drawer jumps back.

The narrowest fix seam is `dispatch_workbench_intent`. Once the host has
taken a file gesture, record it in the active tab synchronously and clear
`RefreshKeptTabView`.

**The stress course, unfinished.** Branch `bpr-1376-reselect-stress-wip`
(not main) has a seeded course through the published-file-switch probe:
`--published-file-switch-timing reselect`, with `BPR_RESELECT_STRESS_SEEDS`
and `BPR_RESELECT_STRESS_FIRST_SEED`. A tests-only hook at the start of
`accept()` clicks while the handoff is being delivered. Its first Windows
Debug run was inconclusive: the course's own settle condition never held on
seed 0. Its quiet test (refresh enabled, no tab work,
`RefreshKeptTabView` clear) needs checking before it can judge anything.
