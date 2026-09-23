---
schema: "implement-rfc.card.v1"
id: "BPR-0985"
title: "Repair deferred reading-position restoration so the window review restoration course passes"
status: "Ready"
rfcs: ["RFC-BPR-0025"]
depends_on: []
priority: "P1"
metadata: {"found_by":"BPR-0597 RFC-0025 closeout and the merge of codex/rfc-0018-0026-closeout into main, 2026-09-22","found_at":"6a897af8","last_pass":"ce63c67b (BPR-0709)","related_rfcs":["RFC-BPR-0024"],"related_cards":["BPR-0597","BPR-0595","BPR-0709","BPR-0710","BPR-0795"],"closeout_card":"BPR-0597"}
---

# BPR-0985: Repair deferred reading-position restoration so the window review restoration course passes

## Observation

`application_profile_window_review_restoration_public_path` (the
[[BPR-0709]]/[[BPR-0710]] course) last passed at `ce63c67b` on 2026-09-08. It
is red on every head examined since:

| Head | First failing phase | Message |
|---|---|---|
| main before the merge, `5b14d7e8` | `auth-home` | pending restoration leaves Home available for user navigation |
| branch tip `317f0023` and merge `6a897af8` | `auth-reading-gesture` | gesture precedes deferred reading restoration completion |

`2fbcc8cb` ([[BPR-0597]]) corrected the `auth-home` assertion to the
owner-accepted [[BPR-0795]] behavior (Overview's loading state shows and Home
stays one gesture away), which is why the merged head gets further. BPR-0597's
diagnostic driver ran every phase past failures at `2fbcc8cb` and found:

- `auth-reading-gesture`, `auth-reading-unchanged`, `auth-reading-home`:
  "gesture precedes deferred reading restoration completion". The course
  queues a gesture from the right pane's `contentsChanged` and expects the
  window's Home notice to still read `Resuming…`
  (`github_application.cpp` `restoration_notice`).
- `unavailable-semantic`: "unavailable reading reports explicit failure"; the
  destination and sibling reading are kept, the explicit report is missing.
- `auth-restore`: "restart preserves exact anchors, commit OIDs, file and
  complete semantic identity". Comparison, both anchors/OIDs and the file
  restore exactly; only the semantic selection differs. `auth-close` and
  `auth-verify` depend on it and were not reached.

These are RFC-BPR-0024 restoration behaviors. They are the blocker recorded on
BPR-0597 for RFC-BPR-0025 AC-05 (reading position across restart) and AC-06
(explicit unavailable-reading report), and they bear on RFC-BPR-0024 AC-07,
whose closeout on [[BPR-0595]] relied on retained BPR-0709/0710 evidence.

## Acceptance

- AC1: Each failing phase is traced to the change that altered it and
  classified as a product regression or an assertion superseded by an
  owner-accepted behavior. An assertion changes only in the second case, with
  the accepting decision named.
- AC2: A gesture made before deferred reading restoration completes wins over
  it; a restart restores the exact comparison, file and semantic selection; an
  unavailable reading keeps its destination and reports an explicit failure
  without retargeting.
- AC3: `application_profile_window_review_restoration_public_path` passes every
  phase on main, and a discriminating negative control fails the phase it
  names.
- AC4: `core_window_review_restoration`, `application_window_review_restoration`,
  `sqlite_window_review_restoration_storage`, and the restart, multi-window and
  layout public paths still pass.

## Boundaries

Fixture-only; no live provider, credential or GitHub mutation. RFC-BPR-0024's
status is not changed by this card; its closeout owner records the result on
BPR-0595.
