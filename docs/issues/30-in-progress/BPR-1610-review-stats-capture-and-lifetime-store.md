---
schema: "implement-rfc.card.v1"
id: "BPR-1610"
title: "Review stats capture and lifetime store"
status: "In Progress"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1607"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# Review stats capture and lifetime store

## Scope and acceptance

Implement RFC-BPR-0086 "Reviews" counting (the count table) and AC06.

- Core counting rules and dedup keys. Observers on the submission, mutation,
  resolution, ledger, note, defect and Review Cycle terminals. Local-day and
  per-repository buckets, lifetime totals and records.
- Totals survive cache prune and delete, Clear local diagnostics and restart.
  Profile purge removes them.
- Count from the first launch with Insights. No backfill (Q2).
