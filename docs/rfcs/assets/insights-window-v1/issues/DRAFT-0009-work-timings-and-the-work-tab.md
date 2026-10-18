---
schema: "implement-rfc.card.v1"
id: "DRAFT-0009"
title: "Work timings and the Work tab"
status: "Backlog"
rfcs: ["RFC-DRAFT-0024"]
depends_on: ["DRAFT-0004"]
platforms: ["mac", "win", "linux"]
---

# Work timings and the Work tab

## Scope and acceptance

Implement RFC-DRAFT-0024 "Work" (D10) and AC10.

- Main-thread stall detection in the Qt adapter, with the 100 ms threshold,
  count, longest and p95.
- Per-file diff wall time in the application host, and work-limit counts.
- Five slowest files, syntax cache hit rate and evictions, and median source
  search time.
- Load `frontend-design:frontend-design` first.
