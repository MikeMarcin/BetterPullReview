---
schema: "implement-rfc.card.v1"
id: "BPR-1216"
title: "Bound session diagnostic files and retention"
status: "In Progress"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1215"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
---

# Bound session diagnostic files and retention

Replace the fixed-file ceiling with safely owned per-session files and coordinated rotation, aggregate and age retention. Preserve lifecycle and explicit failures.

## Acceptance

- AC1: Enforce file, aggregate, age and session bounds with safe ownership and concurrent-session coordination.
- AC2: Test disk/rotation/partial-tail failures and startup/cancel/shutdown availability through public composition.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.
