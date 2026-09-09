---
schema: "implement-rfc.card.v1"
id: "BPR-0714"
title: "Refresh same-review conversation and defect projections"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0713"]
metadata: {"dispatch":"UX-2026-09-08-D6","mandate_revision":1}
---

# Refresh same-review conversation and defect projections

## Bounded scope and acceptance

Continue AC-03/09: accepted private conversation receipts and defect changes
reach independent sibling views of the exact same review without replacing
comparison, file, semantic selection or scroll. Reload accepted durable defects
without correlating or writing against the recipient's selected comparison.
Queued invalidation after review replacement must not expose the old review.
Exercise the rendered composed path; do not infer full AC-03 from cache tests.

## Inventory and ownership

BPR-0713 already carries accepted-only, coalesced, exact-authority ledger
invalidations on the profile decision lane. Conversation invalidation calls
`publish_read_state`, but private invalidation omits the per-window
`DurableDefects` cache. The ordinary defect loader correlates and persists,
so sibling reload needs a separate read-only projection operation.

Own narrow composition reload and its public multi-window regression, with
architecture and BPR-0595 continuation. Preserve RFC-0028 Home/layout authority,
unscheduled browser tabs, full two-account isolation, native geometry/focus,
Windows activation transport, anonymous selected-comparison limitations,
package/accessibility and attended evidence, and canonical dependencies.
