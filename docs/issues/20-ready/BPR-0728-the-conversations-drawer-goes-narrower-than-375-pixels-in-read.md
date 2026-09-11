---
schema: "implement-rfc.card.v1"
id: "BPR-0728"
title: "The conversations drawer goes narrower than 375 pixels in Read"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"205ac8b2-30fd-4339-8d08-51c621ab7fc8"}
---

# The conversations drawer goes narrower than 375 pixels in Read

The owner chose, on the options candidate `BPR-0724-read-width` in the
v006 attended session (decision `205ac8b2`, 18:42:22Z), "Keep the
conversations drawer in Read but let it go narrower than 375 pixels"
(with the single-pane layout,
[BPR-0729](../10-backlog/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md),
as a separate slice). The context is
[BPR-0724](../40-parked/BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md):
"it was difficult to read the code because of a lack of horizontal area
for the text display".

## Scope

- The conversations drawer's minimum width
  (`conversation_drawer_default_width`, 375, in `src/adapters/qt`) is a
  default, not a floor: the splitter handle drags it down to the presented
  floor the other drawers use (160 pixels, BPR-0662), and the drawer's
  contents reflow or scroll at that width rather than clipping (the thread
  cards, the filter row, the review editor).
- The dragged width persists as any drawer width does (BPR-0662) and is
  not lifted back to 375 on the next launch.
- Owned paths: `src/adapters/qt` conversations drawer, drawer panel and
  workbench window sizing, their tests and the UIforETW probe if it pins
  the 375 figure.

## Acceptance

- `qt_workbench_window_scale_{100,125,200}` pin a handle drag of the
  conversations drawer to 200 pixels in Read, its persistence across Home
  and back, and no clipped control at that width.
- `qt_composed_github_scale_100`, `application_composition_contract` and
  the UIforETW contract pass with any digest repinned.
- The owner reads code in Read on the signed bundle with the drawer
  narrowed and accepts it in an attended session.
