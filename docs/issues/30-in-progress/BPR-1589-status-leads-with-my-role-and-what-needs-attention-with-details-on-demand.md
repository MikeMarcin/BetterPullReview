---
schema: "implement-rfc.card.v1"
id: "BPR-1589"
title: "Status leads with my role and what needs attention, with details on demand"
status: "In Progress"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1588", "BPR-1576", "BPR-1583"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "overview-status", "criteria": ["AC03"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1589: Status leads with my role and what needs attention, with details on demand

RFC-BPR-0085's Status content (D7): my role, Needs attention, My review with
unread threads and Go to them (Threads with Only new; Discuss for pathless
comments), Checks as one bar, People as faces, every section a disclosure to
its details, no empty rows, each failed check named once.

## Acceptance

- RFC-BPR-0085 AC03, with core courses for every role, several at once, and
  unobserved team membership.
- Owner, attended: "Status leads with my role and what needs me, and I can dig
  into each part."

## Boundaries

Core decides role, attention and unread facts from accepted observations;
the host extends the overview projection; Qt presents. Reuses [[BPR-1576]]'s
loading pattern and [[BPR-1583]]'s avatars, so it starts after both land.
