---
schema: "implement-rfc.card.v1"
id: "BPR-0713"
title: "Share accepted private-ledger projections between review windows"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0712"]
metadata: {"dispatch":"UX-2026-09-08-D5","mandate_revision":1}
---

# Share accepted private-ledger projections between review windows

## Scope and acceptance

Bounded AC-03/09 slice: accepted receipt and private-note changes reach sibling
windows of the exact profile/account/host/repository/PR authority. Delivery must
preserve independent comparison, file, semantic selection, layout and scroll.
No cross-profile/review delivery, stale delivery after replacement or Close,
publication on failed writes, or recursive material carry-over writes.
Keep rejected note candidates separate from accepted projection refresh.

## Inventory and owned paths

The profile runtime already bounds window observers and owns decision delivery.
Each window composes its own ledger store/host; receipt retention and conversation
caches currently have no sibling invalidation. Agent finding delivery can store
notes through the store port directly. Observe successful writes at that common
boundary and refresh affected projections through application ownership.

Own the narrow application ledger observer/refresh contracts, composition wiring,
focused host and real multi-window tests, and architecture/closeout records.
Verify exact authority, failure and lifecycle rejection, selected-material safety,
and a rendered multi-window gesture course. Preserve all native/attended and
two-account obligations; RFC-0028 Home/layout and unscheduled tabs are unchanged.
