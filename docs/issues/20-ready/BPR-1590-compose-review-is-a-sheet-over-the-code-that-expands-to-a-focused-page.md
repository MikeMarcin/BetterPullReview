---
schema: "implement-rfc.card.v1"
id: "BPR-1590"
title: "Compose review is a sheet over the code that expands to a focused page"
status: "Ready"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1587"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "compose-sheet", "criteria": ["AC04"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1590: Compose review is a sheet over the code that expands to a focused page

RFC-BPR-0085 D6: Finish your review as a 540-pixel sheet over the current
surface, Expand (⌘⇧M) to the focused single-column page in Discuss, Restore
back; no thread list; BPR-1541's panel unchanged. Supersedes [[BPR-1212]] Q3
for Compose review only.

## Acceptance

- RFC-BPR-0085 AC04.
- Owner, attended: "Finish review is a sheet beside the code, and expands to a
  focused page."

## Boundaries

Qt presentation and the composition page's placement; [[BPR-1345]] and
[[BPR-1541]] behaviour unchanged. Uses [[BPR-1584]]'s reading width if landed.
