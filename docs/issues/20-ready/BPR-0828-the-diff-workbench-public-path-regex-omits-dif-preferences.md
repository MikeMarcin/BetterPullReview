---
schema: "implement-rfc.card.v1"
id: "BPR-0828"
title: "The diff workbench public path's pass expression omits the DIF_PREFERENCES line"
status: "Ready"
rfcs: ["RFC-BPR-0031"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux"]
metadata: {"found_by":"dsp-002 BPR-0823 slice, once the comparison-visibility failure was fixed; bpr-linux reported the earlier symptom","found_at":"fe629c90bf37bbeb6fa3e578cf76f81d45155d08","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0828: The diff workbench public path's pass expression omits the DIF_PREFERENCES line

## Defect

`application_diff_workbench_public_path` was red for two reasons stacked on
each other:

1. `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`, then a timeout. This is
   [[BPR-0823]]'s cause, fixed on main in `10f30f04`.
2. With that fixed, every evidence step passes, but the pass expression
   (`tests/end_to_end/CMakeLists.txt`, one contiguous block) still expects
   `APPEARANCE_SETTINGS:…` to be followed directly by
   `NOTIFICATION_DELIVERY_COMPOSED_OK`. Since `6f07c343` ("Add profile-owned
   Dif motion and visibility preferences", RFC-BPR-0031), the status dump
   prints `DIF_PREFERENCES:<visibility>:<motion>:<effective>` between them
   (`src/app/main.cpp`).

The block's own comment says every inserted line has to be named there; this
one never was.

## Fix

Name the line by shape, `DIF_PREFERENCES:[a-z]+:[a-z]+:[a-z]+`, between the two
markers. Match by shape, not by value: other courses own the values.

## Acceptance

- `application_diff_workbench_public_path` passes on macOS.
- The Linux station reruns it once `linux-lane` contains this fix and
  [[BPR-0823]]'s.
