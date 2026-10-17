---
schema: "implement-rfc.card.v1"
id: "BPR-1588"
title: "The Overview's Status drawer: resizable, collapsible, remembered, and no Start review"
status: "In Progress"
rfcs: ["RFC-BPR-0085"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "drawer-foundation", "criteria": ["AC02"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1588: The Overview's Status drawer: resizable, collapsible, remembered, and no Start review

Make the Overview's side column a real drawer (RFC-BPR-0085 principle 7) and
apply the owner's Start review decision (D1). Its content is unchanged here;
[[BPR-1589]] rebuilds it.

## Outcome

- The column becomes **Status**: 340 pixels by default, drags from 260 to 560,
  collapses from its header toggle and ⌥⇧6, keeps its width per window across
  restart with the other drawer widths, and is one keyboard stop with its
  tools (Prepare review tour, Open on GitHub, Hide).
- No Start review or Continue review button. ⌘2 (and the Review segment) from
  the Overview of a pull request in which no file has been chosen in this
  window selects the first unseen change's file (`EntryPath`); Return on the
  Files list opens the selected file. Courses that pressed the button use these.
- The attended review also asks the promote-label question (RFC-BPR-0085,
  "Promoting private material"): To comment, Draft comment or Use as draft.

## Acceptance

- RFC-BPR-0085 AC02.
- Owner, attended (Accept or Reject): "The Overview's side panel now behaves
  like the other drawers, and Start review is gone."
- Owner, attended (option question): the promote label.

## Boundaries

Qt presentation and the window's layout store for one more drawer width; the
entry rule is the window's existing `enter_review_at`. Coordinate with
[[BPR-1576]], [[BPR-1583]], [[BPR-1585]] and [[BPR-1577]], which touch the same
page.
