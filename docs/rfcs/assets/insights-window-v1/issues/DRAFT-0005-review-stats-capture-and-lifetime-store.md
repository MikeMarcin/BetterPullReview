---
schema: "implement-rfc.card.v1"
id: "DRAFT-0005"
title: "Review stats capture and lifetime store"
status: "Backlog"
rfcs: ["RFC-DRAFT-0024"]
depends_on: ["DRAFT-0002"]
platforms: ["mac", "win", "linux"]
---

# Review stats capture and lifetime store

## Scope and acceptance

Implement RFC-DRAFT-0024 "Reviews" counting (the count table) and AC06.

- Core counting rules and dedup keys. Observers on the submission, mutation,
  resolution, ledger, note, defect and Review Cycle terminals. Local-day and
  per-repository buckets, lifetime totals and records.
- Totals survive cache prune and delete, Clear local diagnostics and restart.
  Profile purge removes them.
- Count from the first launch with Insights. No backfill (Q2).
