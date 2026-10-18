---
schema: "implement-rfc.card.v1"
id: "BPR-1591"
title: "Private notes can be anchored to a line"
status: "Ready"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1587"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "notes-margin", "criteria": ["AC05"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1591: Private notes can be anchored to a line

The data half of RFC-BPR-0085 D3: a private note may record a side and line
against the comparison it was written on, in addition to today's file and
pull-request anchors.

## Acceptance

- Core decides placement (placed, outdated with its reason) from the note's
  anchor and the current comparison; the ledger migrates forward with old
  notes unchanged; restart and a new head keep or outdate the anchor as
  RFC-BPR-0085 AC05 says; architecture boundaries pass.

## Boundaries

Core, application host and SQLite ledger adapter; no Qt change.
