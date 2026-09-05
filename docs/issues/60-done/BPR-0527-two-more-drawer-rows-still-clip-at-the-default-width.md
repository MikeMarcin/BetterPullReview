---
id: BPR-0527
title: "Qt: the conversation panel's and agent launcher's rows still clip at the drawer's default width"
status: Done
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

## What this slice delivered — 2026-09-02

The conversation panel's reveal row and the agent launcher's run controls are
`QVBoxLayout`s, like the finding queue's, the side chat's, and both defect
rows. Actions, labels, object names, accessible names, order, and the single
intent each emits are unchanged; only the arrangement moved.

The drawer's default width is now stated once, in `workbench_window.cpp`, as
`conversation_drawer_default_width = 375`: the stacked column asks for 357 px
at 100 % to draw every control at its natural width, and the vertical scroll
bar takes 18 px the column never gets. It is a floor on the column, not on the
drawer, so the panel keeps `minimum_workbench_drawer_width` and a reviewer can
still drag the drawer narrower and have the host persist it.

Measured root cause, which the card had attributed to the reveal row itself:
the drawer opened at exactly the width its widest row asked for and the scroll
bar then took 18 px back out of it, so the widest row was always the one that
lost its last control. The widest row was the agent launcher's, at 366 px. The
reveal button's 17 px and the `defectCount` label's 17 px were that same 366 px
column being wider than its own viewport, not a demand of their own rows: with
the run controls stacked, a horizontal reveal row asks for only 198 px. It is
stacked anyway, because the opening width is the only width this number
governs and the drawer drags down to 64 px.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Every control in the drawer is inside its visible width at 100/125/200 %, and the rows the card names still carry their actions | `ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|qt_workbench_window_scale_(100\|125\|200)\|qt_review_queue\|qt_agent_launch_panel\|qt_side_chat_panel\|qt_finding_queue_view\|qt_defect_card)$' --output-on-failure` | 11/11 passed, 9.7 s | this commit |
| Nothing else in the Qt presentation lane moved | `ctest --preset dev -R '^qt_'` | 43/43 passed, 124.2 s | this commit |
| A new overflow fails by name | agent run controls returned to `QHBoxLayout` with the stretch; `qt_composed_github_scale_{100,125,200}` all fail: `the drawer's column is no wider than the width a reviewer can reach; the widest thing in it is agentLaunchPanel, asking 366 px of a 357 px viewport` | reproduced, restored byte-exact (md5 `8ad8c5184bc074d8ac6fea51677bef0d`) | this commit |
| Either stacked row going back horizontal fails by name | reveal row returned to `QHBoxLayout`; all three scales fail: `the reveal action is stacked under the availability sentence rather than beside it: showReviewThreadsAction` | reproduced, restored byte-exact (md5 `f10d7adf921e3d51bb0570f5fd15683e`) | this commit |
| Every row is whole in the drawer at each scale | the case saves `composed-github-{100,125,200}-drawer-column.png`, the column rendered at exactly the reachable width | looked at, in four slices per scale; no control cut at the right edge | this commit |
| Qt presents, and nothing crossed inward | `cmake --build build/dev --target check_boundaries` | green | this commit |

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0), `check_boundaries` exit 0, and ran the
three composed-window scales, the three workbench scales, the review queue,
agent launch panel, side chat, finding queue, defect card, the agent launch
canary, the finding composition contract, the mounted panel's course, and
`composed_application_hosts`: 15/15. `clang-format` clean by exit code on
the four touched files. The measured root cause — the drawer opened at
exactly its widest row's width and the scroll bar took 18 px back — is
accepted over the card's original attribution, and the 375 px default is
stated once with that reason.
