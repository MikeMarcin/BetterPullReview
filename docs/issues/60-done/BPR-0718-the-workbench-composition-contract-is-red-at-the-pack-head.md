---
schema: "implement-rfc.card.v1"
id: "BPR-0718"
title: "The workbench composition contract is red at the pack head"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"red_at":"fd7388e8","found_by":"ux-redesign-0028-pack.d4"}
acceptance: [{"criterion":"application_workbench_composition_contract passes, and the selector is part of the pack's integration pass with the cause named on the card","outcome":"pass","revision":"working tree over d3abe68f","evidence":{"kind":"command","detail":"build/dev ctest -R '^application_workbench_composition_contract$' passes (22.09 s in scratch verify-2.log after the composition repair alone; 21.31 s in scratch verify-3.log with the desktop-layout gate and the BPR-0723 changes on the same binary). Before the repair it failed at PUBLIC_PATH_STAGE:2 with WORKBENCH_INTENT_WIRE_FAILED and every later flag (scratch repro-1.log); after the first repair it reached WORKBENCH_DRAWER_WIDTH_WIRE_FAILED (verify-1.log), which the configure_desktop_layouts gate closed. Cause: BPR-0708 (ad3abf50) installed lazy_workbench_defaults_port on the desktop path and marked persistence available without opening the store, so the contract's obstructed workbench-settings.sqlite3 never produced the session fallback; the desktop path now decides availability by one unkept open and installs the per-window layout service only when the store opened. The rfc-scout diagnosis (session ad6db331) named the failing check and commit; the drawer-width consequence was found by running the repaired probe. The obstruction is still a directory after the run (the contract's own check)."}}]
---

# The workbench composition contract is red at the pack head

`application_workbench_composition_contract` fails at `fd7388e8`, before
and after the [BPR-0715](BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md)
fix, with the same signature: the `--workbench-composition-probe` run
reaches stage 2 with the review history ready and an exact pair resolved
(`PUBLIC_PATH_EPOCH_PAIR:757e8f25…:f9108913…`, smart evidence `1:1:1:1`),
`WORKBENCH_AUTHORITY_WIRE_OK` and `WORKBENCH_BOUNDED_OBSERVATION_OK`, and
then every later probe fails: `WORKBENCH_INTENT_WIRE_FAILED`,
`WORKBENCH_PROJECTION_WIRE_FAILED`, `WORKBENCH_FILE_GESTURE_FAILED`,
`WORKBENCH_SELECTED_DIFF_REQUEST_FAILED`, `WORKBENCH_EXACT_COMPARISON_FAILED`,
`WORKBENCH_CHANGE_WIRE_FAILED`, `WORKBENCH_DRAWER_WIDTH_WIRE_FAILED`,
`WORKBENCH_SESSION_FALLBACK_FAILED`, `WORKBENCH_IDENTITY_MISMATCH_FAILED`,
`WORKBENCH_INVALIDATION_WIRE_FAILED`, `WORKBENCH_FAILURE_WIRE_FAILED`,
`WORKBENCH_COMPOSITION_TRACE_FAILED`. It was last recorded green on
[BPR-0651](BPR-0651-repositories-and-files-drawers.md); it is not
in the pass the pack head was accepted on, and the first bad commit is not
established. The probe's evidence chain starts at the intent wire, so one
early refusal plausibly explains the whole tail; the same window
composition change that broke the signed-out read (`826fb871`, BPR-0705)
is a candidate, not a finding.

## Scope

- Bisect the selector from BPR-0651's head, name the first bad commit and
  repair the probe or the composition it exercises. Owned paths:
  `src/app/main.cpp` (the probe), `src/app/src/github_application.cpp`
  (the workbench wiring it reads), `tests/end_to_end/application_workbench_composition_contract.cmake`.

## Acceptance

- `application_workbench_composition_contract` passes, and the selector is
  part of the pack's integration pass with the cause named here.

## Cause

Not a bisect: the failing stage names it. The probe fails at the
session-fallback check before its first gesture (`WORKBENCH_INTENT_WIRE_FAILED`
and the tail are the defaults of evidence never taken), because
`WorkbenchPersistenceAvailable` is true although the contract has obstructed
`state/v1/workbench-settings.sqlite3` with a directory. Since
[BPR-0708](BPR-0708-window-layout-restoration.md) (`ad3abf50`,
2026-09-08) the desktop path installs `lazy_workbench_defaults_port`, which
opens the store on every use so no adapter outlives an effect call, and
marks persistence available without ever opening it; the non-desktop
branch still opens the store at composition and falls back to
`session_workbench_settings` when it cannot. The probe moved onto the
desktop path in BPR-0696 (`519ddb2e`) and was not rerun after BPR-0708.

Two repairs, both in `github_application.cpp`:

- Composition decides availability by one open that is not kept: a store
  that opens gets the lazy port as before; one that cannot yields the
  session defaults and `WorkbenchPersistenceAvailable = false`, as the
  other branch does.
- `configure_desktop_layouts` installs the per-window
  `profile_window_layout_service` only when persistence is available, and
  treats `ReviewRestoration` as the "already configured" mark so a second
  window still opens. Without this the window's own lazy layout port fails
  every load and store against the obstructed file, the host never
  publishes a layout, `LayoutWidths` stays empty and the handle drag at the
  drawer-width stage reports nothing (`WORKBENCH_DRAWER_WIDTH_WIRE_FAILED`
  once the first repair let the probe reach it).
