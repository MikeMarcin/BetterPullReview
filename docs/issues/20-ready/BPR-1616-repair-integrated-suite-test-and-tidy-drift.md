---
schema: "implement-rfc.card.v1"
id: "BPR-1616"
title: "Repair integrated suite test and tidy drift"
status: "Ready"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1594", "BPR-1603"]
metadata: {"criteria": ["AC1", "AC2"], "reservation": "docs/issues/_reservations/BPR-1616--BPR-1616--890f7349c660.json"}
---
# BPR-1616: Repair integrated suite test and tidy drift

Owner requested the eight deterministic failures and three tidy findings from
item 9 be fixed. Preserve the accepted Agent design; update stale tests.

## Acceptance

- AC1: All eight previously failing rows pass, with Agent controls and promotion
  checked through the accepted tab, review-thread wording still guarded, and
  profile recovery using the contained temporary helper.
- AC2: The three reported tidy findings are fixed; focused tidy, format 22 and
  the documentation gate pass. No full suite is required for this repair.

## Evidence

Baseline and causes: `docs/issues/_checkpoints/codex-handoff-2026-10-17.progress.md`.
Execution: direct, preserved from that handoff. Origin main merged before edits.
