---
id: BPR-0527
title: "Qt: the conversation panel's and agent launcher's rows still clip at the drawer's default width"
status: In Progress
rfc: RFC-BPR-0010
depends_on: [BPR-0520]
---

# Qt: the conversation panel's and agent launcher's rows still clip at the drawer's default width

## Why

[[BPR-0520]] stacked both defect rows, which let the conversations drawer's
default width fall from 540 px to 366 px because that row had been setting
the column's minimum. Measured at the new default, two controls owned by
other panels still overflow the visible width: `showReviewThreadsAction` in
the conversation panel (RFC-BPR-0009/0010) by 17 px and `cancelAgentAction`
in the agent launch panel (RFC-BPR-0011) by 8 px; the `defectCount` label
spans 17 px past the viewport with its left-aligned text fully visible.
Before BPR-0520 the overflows were 98 px and 89 px against a wider drawer,
so the change clipped strictly less, but "less" is not "none".

## Required final state

- Every control in the conversations drawer is inside the drawer's visible
  width at 100 %, 125 %, and 200 % at the default width, with actions,
  labels, object names, and intents unchanged.
- The composed-window case [[BPR-0520]] added covers every row, so a new
  overflow fails by name.
- The default width is stated once in `workbench_window.cpp` with the
  reason, rather than falling out of whichever row is widest.

## Verification

```text
ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|qt_workbench_window_scale_(100\|125\|200)\|qt_review_queue\|qt_agent_launch_panel)$' --output-on-failure
```

Crop the drawer at each scale and look; a whole-image diff is not evidence.
