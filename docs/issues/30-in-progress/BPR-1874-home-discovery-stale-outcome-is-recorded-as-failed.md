---
schema: "implement-rfc.card.v1"
id: "BPR-1874"
title: "Home's discovery refresh that ends stale is recorded as failed and names the rejected row"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05, split from BPR-1873"}
acceptance: []
---

# BPR-1874: Home's discovery refresh that ends stale is recorded as failed and names the rejected row

## Observed

Home's discovery refresh ends `stale` on most cycles with no failed read.
`personal_discovery_host` calls `fail(queue_discovery_problem::failed)` in
`apply_metadata` / `apply_metadata_row` when it rejects a retained row, yet
`finish_cycle` records the refresh as `succeeded`, because only `CycleStopped`
records `failed`.

## Change

- Find why retained rows are rejected on most cycles; fix the rejection if it
  is wrong.
- Record a stale outcome as failed in the diagnostic, naming which check
  rejected which retained row (no private content in the log).

## Acceptance

- AC1. A cycle that rejects a retained row records `failed` with the check and
  row identity; a clean cycle records `succeeded`.
- AC2. If the rejection was spurious, a normal cycle on fixtures ends fresh.
- AC3. `python3 scripts/verify_documentation.py --root .` passes.
