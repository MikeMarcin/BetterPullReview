---
schema: "implement-rfc.card.v1"
id: "BPR-1255"
title: "Linux: a review-bar control folds into the overflow menu in the title-row and workbench-window scale rows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-1251","BPR-1256","BPR-0905"]}
---

# BPR-1255: Linux: a review-bar control folds into the overflow menu in the title-row and workbench-window scale rows

Found by bpr-main-linux on 2026-10-01: WSL2 Linux 6.18, GCC 15.2, Qt offscreen,
`build/dev` Debug, at `db021262a`. bpr-main-win's BPR-0905 full suite in this
checkout (556 rows, -j10) reported them first. A serial rerun of each row
failed the same way.

## Observation

All six rows fail at every scale, in about 2 s each, at the same check
(`tests/adapters_qt/workbench_window_tests.cpp`, about line 5514):

```
FAILED: no review-bar control folds into an overflow menu at any width
```

- `qt_workbench_title_row_scale_{100,125,200}`
- `qt_workbench_window_scale_{100,125,200}`

[[BPR-1251]] records Windows failures in `qt_workbench_window_scale_*`, but at a
different check ("the title yields once and does not return at a narrower
width"). This card stays separate from it, at the integrator's direction.

The same Linux run also has a document tab strip narrower than one tab
([[BPR-1251]]'s Linux half) and a comparison splitter at zero ([[BPR-1256]]). All
three are width rules at Linux metrics. Measure them together once before
splitting fixes.

## Scope

- Measure the review bar at the failing widths under Linux metrics. Name the
  control that folds, and what takes its room.
- Fix the layout that owns the rule, not the check.
- Keep macOS and Windows unchanged.

## Acceptance

- The six rows pass on Linux.
- The card names the cause.
