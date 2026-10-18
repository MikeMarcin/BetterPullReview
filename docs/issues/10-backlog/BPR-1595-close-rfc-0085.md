---
schema: "implement-rfc.card.v1"
id: "BPR-1595"
title: "Verify and close RFC-BPR-0085"
status: "Backlog"
rfcs: ["RFC-BPR-0085"]
closeout_for: ["RFC-BPR-0085"]
depends_on: ["BPR-1589", "BPR-1590", "BPR-1593", "BPR-1594", "BPR-1603"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "drawer-closeout", "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}}
---

# BPR-1595: Verify and close RFC-BPR-0085

Close [RFC-BPR-0085](../../rfcs/RFC-BPR-0085-drawer-design-system.md) once
every milestone has landed with its owner verdict.

RFC base: `4317330bad446c4c1577b6d6bf09d458cf8edb9a`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01 Shared parts:** the drawer header, card, ownership marks and empty state are one set of Qt components used by every surface below; Qt courses render each in the four themes at 100, 125 and 200 percent with negative controls (a nested tab, a stacked button column, a non-role colour). | Pending. |
| **AC02 Status is a drawer:** the Overview's side column resizes within its bounds, collapses by toggle and key, remembers its width per window across restart, and is reachable by keyboard; there is no Start review button; ⌘2 from a fresh Overview selects the first unseen change's file and Return on Files opens the selected file; the owner answers the promote-label question. | Pending. |
| **AC03 Status content:** the drawer leads with my role and what needs attention as specified, names each failed check in one place, draws no empty row, expands each section to its details, and Go to them opens Threads with Only new (or Discuss for pathless comments); core courses cover every role, including several at once and unobserved team membership. | Pending. |
| **AC04 Compose review:** Finish review opens the sheet over the current surface with no thread list; a pending row reveals its line behind the sheet; Expand and Restore keep text, caret and scroll; BPR-1541's panel and every submission outcome behave as before. | Pending. |
| **AC05 Notes:** notes hang at their lines and open in place; a note written on a line survives restart and a new head (or is shown as outdated); the promote action creates a local draft at the note's line and publishes nothing; Notes shows no agent tools. | Pending. |
| **AC06 Tours:** a tour is written, reordered by pointer and keyboard, expanded to the tour page and back, previewed and followed in the route, with code and explanation visible together. | Pending. |
| **AC07 Agent:** ⌥⇧A, ⌘I and the context menu reach the Agent tab; the four states render; a finding becomes a local draft only on a person's press; sessions, earlier heads and private runs are reachable from History. | Pending. |
| **AC08 Themes and sizes:** every rebuilt surface renders without clipping in the four themes at 100, 125 and 200 percent and at its narrowest and widest width. | Pending. |
| **AC09 Owner looks:** the owner accepts each milestone's frozen build. | Pending. |
| **AC10 Pull-request comments at the edge (amendment):** conversation comments and review summaries on no file wait as a chip at the Comments drawer's top edge, open in place with their unread marks, and offer Show in Discuss; Discuss is unchanged; the owner accepts the design note's mockup and the frozen build. | Pending. |
