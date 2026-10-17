---
schema: "implement-rfc.card.v1"
id: "BPR-1592"
title: "Notes are margin notes, and agent tools leave Notes"
status: "Backlog"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1591"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "notes-margin", "criteria": ["AC05"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1592: Notes are margin notes, and agent tools leave Notes

RFC-BPR-0085 D3 in Qt: notes hang at their lines like threads, dashed with a
lock; edge chips for pull-request and other-file notes; N for a new note; the
promote action with the owner's label. The agent panels move unchanged into a
provisional Agent tab until [[BPR-1594]] rebuilds it.

## Acceptance

- RFC-BPR-0085 AC05.
- Owner, attended: "Private notes sit beside their lines like threads."

## Boundaries

Qt presentation over BPR-1591's anchors; promotion's semantics unchanged.
