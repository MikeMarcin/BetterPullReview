---
schema: "implement-rfc.card.v1"
id: "BPR-0718"
title: "The workbench composition contract is red at the pack head"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"red_at":"fd7388e8","found_by":"ux-redesign-0028-pack.d4"}
---

# The workbench composition contract is red at the pack head

`application_workbench_composition_contract` fails at `fd7388e8`, before
and after the [BPR-0715](../40-parked/BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md)
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
[BPR-0651](../60-done/BPR-0651-repositories-and-files-drawers.md); it is not
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
