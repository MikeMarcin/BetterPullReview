---
schema: "implement-rfc.card.v1"
id: "BPR-1286"
title: "Workbench window rows read a staged large publication before adoption; the installed package course calls tempfile.mkdtemp"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1083 implementer, macOS build/dev at 6e69e121d, 2026-10-05","reservation":"docs/issues/_reservations/BPR-1286--BPR-1286--6ee7aaaa9409.json","related_cards":["BPR-1264","BPR-1182","BPR-1083","BPR-0394"],"acceptance":["BPR-1286:window-rows","BPR-1286:contained-temporary","BPR-1286:large-review-kept"]}
---

# BPR-1286: Workbench window rows read a staged large publication before adoption; the installed package course calls tempfile.mkdtemp

## Observation

On main at `6e69e121d` (macOS arm64, `build/dev`) four rows fail without any
BPR-1083 change:

- `qt_workbench_window_scale_{100,125,200}` stop at "the composed surface
  accepts its exact publication".
- `contained_temporary_usage_contract` reports
  `tests/end_to_end/installed_cycle_package_course.py:133: tempfile.mkdtemp`.

## Cause

**Window rows: [[BPR-1264]], `ac0d9d1d4`.** A publication of more than 4,096
lines now fills two detached documents over bounded turns and is adopted
only once both are full. Until then `current_result()` is the previous
result and `geometry_pending()` is true. The test's `publish_fixture` read
`current_result()` after one drain of events, before `settle_geometry`, so
it assumed the old synchronous publish. The product is right; the test read
too early. Control: with the unchanged test and the staging threshold raised
out of reach, `qt_workbench_window_scale_100` passes.

**Contained temporary: [[BPR-1182]], `f57c828ac`.** The course made its root
with `tempfile.mkdtemp(dir=Path.home())`. [[BPR-0394]]'s gate forbids
`tempfile`'s creating calls anywhere under `tests/`.

## Outcome

- `publish_fixture` waits for `geometry_pending()` to clear, which covers the
  staged fill, and then requires the surface's result to be the published
  one by identity.
- The course makes its root with
  `make_contained_temporary_directory(prefix=..., parent=Path.home())`. On
  macOS, the only platform it runs on, that is `mkdtemp` in the same place.
- No product change. BPR-1264's staging and its gains stay.

## Acceptance

- **BPR-1286:window-rows.** `qt_workbench_window_scale_{100,125,200}` pass on
  macOS `build/dev`.
- **BPR-1286:contained-temporary.** `contained_temporary_usage_contract`
  passes.
- **BPR-1286:large-review-kept.** The 18 large-review rows pass with budgets
  enforced in an optimized tests-on macOS tree.
