---
schema: "implement-rfc.card.v1"
id: "BPR-0823"
title: "Two appearance contracts never see the diff evidence comparison"
status: "In Progress"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux"]
metadata: {"found_by":"bpr-linux under BPR-0816 (pre-existing), confirmed on macOS by the dsp-002 integration pass","found_at":"99712ec3ada8e8482f3a855d3873d20690283f95","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0823: Two appearance contracts never see the diff evidence comparison

## Defect

Two appearance contracts fail on both platforms, in the same way:

- `application_appearance_theme_customization_contract`;
- `application_appearance_store_fallback_contract`.

- **macOS, at the `99712ec3ada8e8482f3a855d3873d20690283f95` integration pass**
  (`build/r2-logs/dsp2-int2-courses.log`). Theme customization passes
  every appearance stage, through `equal-stored-commit-acknowledged`, then
  prints `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` and times out at 80 s.
  Store fallback prints the same marker and times out
  (`appearance_store_fallback_contract.cmake:89`). In the same run,
  `application_appearance_gesture_contract` passes (73.29 s).
- **Linux, `bpr-linux`.** Both fail identically before and after
  [[BPR-0816]]'s changes, and there the gesture contract fails too, with
  `EPOCH_FIXTURE_SEED_FAILED` and `RUN_BUDGET_EXPIRED … stage=4`. See
  [the Linux stock build evidence](../../verification/BPR-0816-linux-gcc15-stock-build.md),
  "Focused Linux tests".

The appearance behaviour under test passes. What fails is the courses'
diff-evidence step, which never sees a visible comparison. When that
started is not established. Before this card existed, a red architecture
gate and a nested checkout hid several failures like this.

## Scope

- Find the first bad revision on macOS, and the mechanism. The candidates
  include the drawer, Home and window-composition changes of the last week:
  [[BPR-0751]]'s refresh hold, which [[BPR-0818]] has since ended; the
  RFC-BPR-0032 Home work; and [[BPR-0705]]'s per-profile Settings.
- Fix the product if a person no longer sees the comparison in that state.
  Change the courses only under a cited accepted decision.
- Then check the Linux gesture-contract seed failure: the same cause, or a
  separate Linux defect.

## Acceptance

- Both contracts pass on macOS, and the cause is named here.
- The Linux station reruns all three appearance contracts and records the
  result. A separate Linux cause gets its own card.
