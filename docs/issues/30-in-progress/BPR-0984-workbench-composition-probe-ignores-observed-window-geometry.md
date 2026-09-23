---
schema: "implement-rfc.card.v1"
id: "BPR-0984"
title: "Let the workbench composition probe's session fallback hold the window's observed geometry"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"integration pass after merging codex/rfc-0018-0026-closeout into main, 2026-09-22","found_at":"6a897af8","regressed_by":"9210330a (BPR-0977)","related_rfcs":["RFC-BPR-0018","RFC-BPR-0024"],"related_cards":["BPR-0593","BPR-0595","BPR-0977"]}
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
