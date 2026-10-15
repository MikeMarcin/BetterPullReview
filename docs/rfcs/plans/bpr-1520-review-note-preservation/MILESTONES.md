# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **bpr-1520-review-note-preservation** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## preserve-and-recover

Inline comments remain recoverable and eligible comment creation recovers after publication failure, refresh, navigation and restart.

**Work:** `BPR-1520` [AC1, AC2, AC3, AC4, AC5, AC6, AC7, AC8]

**Prerequisites:** None

**Completion:**

- Exact content and authority survive controlled failure/refresh/switch/return/restart.
- Unknown writes reconcile without duplicate POSTs; retained old-head drafts can be inspected safely.
- Existing owner recovery limits, command quiescence and next action are recorded.
- Line browsing allocates no durable drafts; active and historical limits retain authored material and publication lineage.

**Boundary:** Only BPR-1520; owner live profile stays untouched; no real review publication.
