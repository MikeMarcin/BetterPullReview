---
schema: "implement-rfc.card.v1"
id: "BPR-0984"
title: "Let the workbench composition probe's session fallback hold the window's observed geometry"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"integration pass after merging codex/rfc-0018-0026-closeout into main, 2026-09-22","found_at":"6a897af8","regressed_by":"9210330a (BPR-0977)","related_rfcs":["RFC-BPR-0018","RFC-BPR-0024"],"related_cards":["BPR-0593","BPR-0595","BPR-0977"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"827ca5f6","evidence":{"kind":"observation","detail":"Probe compares a copy of host->layout() with Geometry cleared against the defaults, still requiring unavailable persistence and a session store. Negative control (FilesWidth+1 in the copy) fails with WORKBENCH_SESSION_FALLBACK_FAILED; build/bpr0984-negative.log"}},{"criterion":"AC2","outcome":"pass","revision":"827ca5f6","evidence":{"kind":"command","detail":"ctest --preset dev -j 1 -R '^(application_workbench_composition_contract|application_review_round_trip_composition_contract)$': 2/2 pass (22.79 s, 11.70 s); build/bpr0984-focused.log"}},{"criterion":"AC3","outcome":"pass","revision":"827ca5f6","evidence":{"kind":"artifact","detail":"Full application 8ffa0e73..., manifest ac94ddb3... resealed with the review on this card; mutation-branch pin a7e2081e... unchanged","path":"docs/issues/60-done/BPR-0984-workbench-composition-probe-ignores-observed-window-geometry.md#evidence--2026-09-22"}}]
---

# BPR-0984: Let the workbench composition probe's session fallback hold the window's observed geometry

## Observation

`application_workbench_composition_contract` passes on main before the merge
(`5b14d7e8`, 23.79 s) and fails on the merged head `6a897af8` and on the
branch tip `317f0023` at `PUBLIC_PATH_STAGE:2`, `presentation=0`, with
`WORKBENCH_SESSION_FALLBACK_FAILED` the first failing evidence. A bisect over
the branch's product commits after [[BPR-0593]]'s recorded pass at `95e64b4c`
passes at `8d072d14` ([[BPR-0976]]) and fails at `9210330a` ([[BPR-0977]]).

The contract obstructs the workbench-settings database, so composition falls
back to the in-memory session store, and the probe asserts that the host's
layout equals the defaults. BPR-0977 added `Geometry` to the layout and lets
the host record each window's observed native placement once its layout has
loaded. The session store loads defaults, so the offscreen window's first
move/resize observation lands in the layout and the whole-record comparison
fails. The host is doing what BPR-0977 specified: observed geometry is window
presentation, not a layout supplied by storage.

## Acceptance

- AC1: The probe's session-fallback evidence still requires unavailable
  persistence, a session store and default values for every stored layout
  field; only the observed `Geometry` is excluded from the comparison.
- AC2: `application_workbench_composition_contract` passes on main.
- AC3: The pinned `github_application.cpp` digests are resealed in the same
  commit as the review of this change; the mutation-branch pin is unchanged.

## Boundaries

The change is confined to the `BPR_BUILD_TESTING` probe in
`src/app/src/github_application.cpp`. No host, storage or Qt behavior changes.

## Evidence — 2026-09-22

Change: the probe copies the host's layout, clears its `Geometry` and compares
the copy with the defaults. Persistence must still be unavailable and the
session store present. `clang-format-diff` reports nothing on the changed lines.

Pin review for the reseal: the only edit to `github_application.cpp` since the
merge reseal (`6a897af8`) is this one, inside
`github_application_fixture_access::exercise_workbench_composition` in the
`BPR_BUILD_TESTING` region. It reads host state and changes no product,
credential, provider or mutation path. Full application
`8ffa0e73517c48aafa985ff27f61bbd7484b19a7bef3dd3635485ff2501cb201`; production
manifest `ac94ddb31847481b5104404312faac7724051487b7eacbb03a0e7a96083b0c1c`;
mutation branch unchanged at
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.

- Dev build passes. `application_workbench_composition_contract` passes
  (22.79 s) and `application_review_round_trip_composition_contract` passes
  (11.70 s) with the new pins (`build/bpr0984-focused.log`).
- The same probe change applied to the branch tip `317f0023` in a task-owned
  worktree makes the contract pass there (22.71 s), confirming the diagnosis.
- Negative control in that worktree: also bumping one stored field
  (`FilesWidth + 1`) in the compared copy fails the contract with
  `WORKBENCH_SESSION_FALLBACK_FAILED` (`build/bpr0984-negative.log`). The
  source was restored afterwards.
