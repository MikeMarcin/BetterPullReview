---
id: BPR-0520
title: "Qt: the defect action row clips in the conversations drawer at 100 %"
status: In Progress
rfc: RFC-BPR-0010
depends_on: [BPR-0513]
---

# Qt: the defect action row clips in the conversations drawer at 100 %

## Why

[[BPR-0513]] placed two more panels in the conversations drawer and its
capture showed action rows laid out horizontally clipping at 100 % scale:
the drawer never scrolls sideways, so a too-wide `QHBoxLayout` makes its
last actions absent rather than offscreen. [[BPR-0506]] stacked the finding
queue's six actions and the side chat's row was stacked with it; the defect
row (`defectActions` in `src/adapters/qt/src/workbench_window.cpp`, the
verify/reopen/withdraw/accept actions of RFC-BPR-0009/0010) still lays out
horizontally and still clips.

## Required final state

- Every action in the defect row is reachable at 100 %, 125 %, and 200 %
  with the drawer at its default width; the actions, their labels, object
  names, and intents are unchanged.
- The composed-window scale captures show the whole row.

## Verification

```text
ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|qt_review_queue\|qt_workbench_window_scale_100)$' --output-on-failure
```

Crop the drawer region of each capture and look at it; a whole-image diff
is not evidence.
