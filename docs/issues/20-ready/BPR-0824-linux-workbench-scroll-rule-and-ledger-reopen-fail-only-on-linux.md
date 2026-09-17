---
schema: "implement-rfc.card.v1"
id: "BPR-0824"
title: "Linux: the workbench position rule and the review ledger reopen fail only on Linux"
status: "Ready"
rfcs: ["RFC-BPR-0007", "RFC-BPR-0028"]
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux under BPR-0816 (pre-existing)","found_at":"4883dbd9","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0824: Linux: the workbench position rule and the review ledger reopen fail only on Linux

## Defect

`bpr-linux` reported four focused failures that were identical before and
after [[BPR-0816]] (see
[the Linux stock build evidence](../../verification/BPR-0816-linux-gcc15-stock-build.md),
"Focused Linux tests"). All four pass on macOS at the `99712ec3ada8e8482f3a855d3873d20690283f95` integration pass
(`build/r2-logs/dsp2-int2-courses.log`).

- `qt_workbench_window_scale_100`, `_125` and `_200`:
  `FAILED: the positions yield before the number does and keep their rule
  once it has gone` (`tests/adapters_qt/workbench_window_tests.cpp:1531`).
- `application_review_ledger_composition_contract`: the seed passes. The
  reopen fails with `REVIEW_LEDGER_COMPOSITION_REOPEN_FAILED`; the store
  loads but holds no receipt.

These are not platform-bound by design. A difference that exists only on
Linux (Qt platform plugin and font metrics under offscreen/WSLg, filesystem
or SQLite durability, locale) is the likely cause. The review ledger case
could be a durability defect, so it matters more than the layout one.

## Scope

- Run the courses at the Linux station's current head after it merges main,
  so [[BPR-0818]]'s drawer change is included, and name each mechanism.
- `REVIEW_LEDGER_COMPOSITION_REOPEN_FAILED`: establish whether the receipt
  never reached disk, reached a different root, or is filtered on load. A
  durability or root-selection defect is a product fix.
- The position rule: establish whether the course depends on font metrics or
  the platform plugin. Fix the course only if its claim is still asserted
  honestly on Linux.

## Acceptance

- All four pass on the Linux station, and the causes are named here.
- They still pass on macOS.
